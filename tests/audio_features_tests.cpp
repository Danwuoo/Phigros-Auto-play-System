#include "rhythm/audio_features.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

using namespace rhythm;
namespace {
void check(bool value) { if (!value) throw std::runtime_error("check failed"); }
void rejects(const std::function<void()>& run) {
    try { run(); } catch (const std::exception&) { return; }
    throw std::runtime_error("expected rejection");
}
Ns at(std::uint64_t position, unsigned rate) {
    return 10000000000LL + static_cast<Ns>(std::llround(position * 1000000000.L / rate));
}
MonoAudioPacket packet(std::span<const float> samples, std::uint64_t position, unsigned rate = 48000) {
    return {samples, rate, position, at(position,rate), at(position+samples.size(),rate)+1000000, 1000, false, false};
}
std::vector<float> signal(unsigned rate, double seconds) {
    std::vector<float> result(static_cast<std::size_t>(rate * seconds));
    for (std::size_t i=0;i<result.size();++i)
        result[i] = static_cast<float>(.3*std::sin(2*std::numbers::pi*1000*i/rate) + .1*std::sin(2*std::numbers::pi*250*i/rate));
    return result;
}
std::vector<float> direct_dft_bands(std::span<const float> samples) {
    // Independent quadratic real DFT oracle: no production FFT/chirp helper.
    std::array<double,8> power{};
    constexpr std::array<unsigned,7> boundaries{100,250,500,1000,2000,4000,8000};
    const auto size=samples.size(); double normalizer=0;
    std::vector<double> window(size);
    for(std::size_t i=0;i<size;++i) {
        window[i]=.5-.5*std::cos(2*std::numbers::pi*i/(size-1));
        normalizer+=size*window[i]*window[i];
    }
    for(std::size_t k=0;k<=size/2;++k) {
        double real=0,imag=0;
        for(std::size_t i=0;i<size;++i) {
            const double phase=-2*std::numbers::pi*k*i/size;
            real+=samples[i]*window[i]*std::cos(phase);
            imag+=samples[i]*window[i]*std::sin(phase);
        }
        unsigned band=0;while(band<7 && k*50>=boundaries[band]) ++band;
        power[band]+=(real*real+imag*imag)*(k==0||k==size/2 ? 1 : 2)/normalizer;
    }
    std::vector<float> result;
    for(double value:power) result.push_back(static_cast<float>(std::log(std::max(std::sqrt(value),1e-5))));
    return result;
}
AudioFeatureBatch feed(AudioFeatureAssembler& assembler, std::span<const float> samples, unsigned rate = 48000,
                       std::uint64_t first = 0) {
    AudioFeatureBatch last;
    for (std::size_t start=0;start<samples.size();) {
        const auto count=std::min<std::size_t>(rate, samples.size()-start);
        last=assembler.push(packet(samples.subspan(start,count), first+start,rate));
        check(last.packet_accepted); start+=count;
    }
    return last;
}
} // namespace

int main() {
    const std::vector<std::pair<std::string,std::function<void()>>> tests{
        {"constant PCM has independent physical RMS and log floor", [] {
            const std::vector<float> pcm(4800,.25f);
            const auto frames=extract_audio_features(pcm,48000,AudioFeatureKind::log_rms_v1);
            check(frames.bands==1 && frames.values.size()==10 && frames.dropped_tail_samples==0);
            for(std::size_t i=0;i<10;++i) check(std::abs(frames.rms[i]-.25)<1e-12 && std::abs(frames.values[i]-std::log(.25))<1e-6);
            const auto silence=extract_audio_features(std::vector<float>(480,0),48000,AudioFeatureKind::log_rms_v1);
            check(silence.rms[0]==0 && std::abs(silence.values[0]-std::log(1e-5))<1e-6);
        }},
        {"IIR chunking is identical across partial-bin packet boundaries", [] {
            const auto pcm=signal(44100,.23);
            const auto full=extract_audio_features(pcm,44100,AudioFeatureKind::iir8_log_energy_v1);
            MonoFeatureExtractor streaming(44100,AudioFeatureKind::iir8_log_energy_v1);
            std::vector<float> values; std::vector<double> rms;
            for(std::size_t start=0;start<pcm.size();) {
                const auto count=std::min<std::size_t>(137,pcm.size()-start);
                const auto frames=streaming.push(std::span<const float>(pcm).subspan(start,count));
                values.insert(values.end(),frames.values.begin(),frames.values.end());
                rms.insert(rms.end(),frames.rms.begin(),frames.rms.end()); start+=count;
            }
            check(values==full.values && rms==full.rms && full.bands==8);
        }},
        {"sample-rate-normalized IIR has comparable steady sine band energies", [] {
            const auto a=extract_audio_features(signal(44100,1),44100,AudioFeatureKind::iir8_log_energy_v1);
            const auto b=extract_audio_features(signal(48000,1),48000,AudioFeatureKind::iir8_log_energy_v1);
            double maximum=0;
            for(std::size_t i=50*8;i<a.values.size();++i) maximum=std::max(maximum,std::abs(static_cast<double>(a.values[i])-b.values[i]));
            check(maximum<.08); // Numerical filter comparison, not an acoustic identity guarantee.
        }},
        {"Bluestein FFT matches an independent unpadded direct DFT at 44.1k and 48k", [] {
            for(unsigned rate:{44100u,48000u}) {
                auto pcm=signal(rate,.02);
                for(std::size_t i=0;i<pcm.size();++i)
                    pcm[i]+=static_cast<float>(.11+.08*std::sin(2*std::numbers::pi*7403*i/rate));
                const auto actual=extract_audio_features(pcm,rate,AudioFeatureKind::fft8_log_energy_v1);
                const auto expected=direct_dft_bands(pcm);
                check(actual.bands==8 && actual.values.size()==8 && actual.rms.size()==1);
                check(actual.feature_origin_ns==10000000 && actual.dropped_tail_samples==0);
                for(std::size_t i=0;i<8;++i) check(std::abs(actual.values[i]-expected[i])<2e-6);
                double energy=0;for(float value:pcm) energy+=static_cast<double>(value)*value;
                check(std::abs(actual.rms[0]-std::sqrt(energy/pcm.size()))<1e-12);
            }
        }},
        {"FFT 20ms warmup and 10ms overlap survive arbitrary packet splits exactly", [] {
            for(unsigned rate:{44100u,48000u}) {
                const auto pcm=signal(rate,.23);
                const auto full=extract_audio_features(pcm,rate,AudioFeatureKind::fft8_log_energy_v1);
                MonoFeatureExtractor streaming(rate,AudioFeatureKind::fft8_log_energy_v1);
                std::vector<float> values;std::vector<double> rms;
                for(std::size_t start=0;start<pcm.size();) {
                    const auto count=std::min<std::size_t>(137,pcm.size()-start);
                    const auto frames=streaming.push(std::span<const float>(pcm).subspan(start,count));
                    values.insert(values.end(),frames.values.begin(),frames.values.end());
                    rms.insert(rms.end(),frames.rms.begin(),frames.rms.end());start+=count;
                }
                check(values==full.values && rms==full.rms && rms.size()==22);
                streaming.reset();
                const auto hop=std::span<const float>(pcm).first(rate/100);
                check(streaming.push(hop).values.empty());
                check(streaming.push(hop).rms.size()==1);
                check(streaming.push(hop).rms.size()==1);
                check(extract_audio_features(hop,rate,AudioFeatureKind::fft8_log_energy_v1).dropped_tail_samples==hop.size());
            }
        }},
        {"FFT assembler timestamps actual exclusive end and discards overlap at gaps", [] {
            AudioFeatureConfig config;config.kind=AudioFeatureKind::fft8_log_energy_v1;config.window_frames=20;
            const auto pcm=signal(44100,.23);AudioFeatureAssembler whole(config),fragmented(config);
            const auto one=whole.push(packet(pcm,0,44100));std::vector<AudioObservation> many;
            for(std::size_t start=0;start<pcm.size();) {
                const auto count=std::min<std::size_t>(137,pcm.size()-start);
                const auto batch=fragmented.push(packet(std::span<const float>(pcm).subspan(start,count),start,44100));
                check(batch.packet_accepted && !batch.segment_reset);
                many.insert(many.end(),batch.observations.begin(),batch.observations.end());start+=count;
            }
            check(one.observations.size()==1 && many.size()==1 && one.observations[0].values==many[0].values);
            check(one.observations[0].window_end_host_ns==10210000000LL);
            check(std::abs(one.observations[0].window_end_host_ns-many[0].window_end_host_ns)<=1);
            AudioFeatureAssembler gap(config);const std::vector<float> before(44100/100,.1f);
            check(gap.push(packet(before,0,44100)).observations.empty());
            const std::vector<float> after(44100/5,.2f);
            const auto reset=gap.push(packet(after,100000,44100));
            check(reset.packet_accepted && reset.segment_reset && reset.observations.empty());
            const auto next=gap.push(packet(before,100000+after.size(),44100));
            check(next.observations.size()==1 && next.observations[0].observation_id==1);
        }},
        {"FFT overlap retains prior-hop uncertainty only while it is in the window", [] {
            AudioFeatureConfig config;config.kind=AudioFeatureKind::fft8_log_energy_v1;config.window_frames=16;config.hop_frames=1;
            AudioFeatureAssembler assembler(config);const std::vector<float> first(480,.1f),rest(17*480,.2f);
            auto initial=packet(first,0);initial.timestamp_uncertainty_ns=7000000;
            check(assembler.push(initial).observations.empty());
            const auto batch=assembler.push(packet(rest,480));check(batch.observations.size()==2);
            check(batch.observations[0].timestamp_uncertainty_ns==9000000);
            check(batch.observations[1].timestamp_uncertainty_ns==2001000);
        }},
        {"44.1k partial packet boundaries preserve observation values and sample time", [] {
            AudioFeatureConfig config;config.window_frames=20;
            AudioFeatureAssembler whole(config), fragmented(config);
            const auto pcm=signal(44100,.23);
            const auto one=whole.push(packet(pcm,0,44100));
            std::vector<AudioObservation> many;
            for(std::size_t start=0;start<pcm.size();) {
                const auto count=std::min<std::size_t>(137,pcm.size()-start);
                const auto batch=fragmented.push(packet(std::span<const float>(pcm).subspan(start,count),start,44100));
                check(batch.packet_accepted && !batch.segment_reset);
                many.insert(many.end(),batch.observations.begin(),batch.observations.end());start+=count;
            }
            check(one.observations.size()==1 && many.size()==1 && one.observations[0].values==many[0].values);
            check(std::abs(one.observations[0].window_end_host_ns-many[0].window_end_host_ns)<=1 && many[0].observation_id==1);
            check(one.observations[0].rms==many[0].rms && many[0].timestamp_uncertainty_ns==2001000);
        }},
        {"ring emits 500-frame window then only after five new feature frames", [] {
            AudioFeatureAssembler assembler;
            const std::vector<float> pcm(48000*5,.25f);
            const auto first=feed(assembler,pcm);
            check(first.observations.size()==1);
            const auto& observation=first.observations[0];
            check(observation.values.size()==500*8 && observation.observation_id==1);
            check(observation.window_end_host_ns==15000000000LL && observation.received_host_ns==15001000000LL);
            check(observation.timestamp_uncertainty_ns==2001000 && std::abs(observation.rms-.25)<1e-12);
            const std::vector<float> hop(2400,.25f);
            const auto next=assembler.push(packet(hop,240000));
            check(next.observations.size()==1 && next.observations[0].observation_id==2);
            check(next.observations[0].window_end_host_ns==15050000000LL);
        }},
        {"device gap clears partial filters and ring without resetting observation IDs", [] {
            AudioFeatureAssembler assembler; const std::vector<float> pcm(48000*5,.1f);
            check(feed(assembler,pcm).observations.back().observation_id==1);
            const std::vector<float> one(480,.1f);
            const auto gap=assembler.push(packet(one,240001));
            check(gap.packet_accepted && gap.segment_reset && gap.reason==AudioFeatureReason::device_gap && gap.observations.empty());
            check(assembler.last_observation_id()==1);
            const auto after=feed(assembler,std::vector<float>(48000*5-480,.1f),48000,240481);
            check(after.observations.size()==1 && after.observations[0].observation_id==2);
        }},
        {"timestamp gap and explicit discontinuity start independent windows", [] {
            for(unsigned mode=0;mode<2;++mode) {
                AudioFeatureAssembler assembler; const std::vector<float> one(480,.1f);
                check(assembler.push(packet(one,0)).packet_accepted);
                auto changed=packet(one,480);
                if(mode==0) {changed.first_host_ns+=3000000;changed.received_host_ns+=3000000;}
                else changed.discontinuity=true;
                const auto result=assembler.push(changed);
                check(result.packet_accepted && result.segment_reset && result.observations.empty());
                check(result.reason==(mode==0?AudioFeatureReason::time_gap:AudioFeatureReason::discontinuity));
            }
        }},
        {"unknown uncertainty timestamp-error and future samples are refused", [] {
            for(unsigned mode=0;mode<4;++mode) {
                AudioFeatureAssembler assembler; const std::vector<float> one(480,.1f);
                auto bad=packet(one,0);
                if(mode==0) bad.timestamp_uncertainty_ns=-1;
                if(mode==1) bad.timestamp_error=true;
                if(mode==2) bad.received_host_ns=bad.first_host_ns;
                if(mode==3) bad.timestamp_uncertainty_ns=18000001;
                const auto result=assembler.push(bad);
                check(!result.packet_accepted && result.segment_reset && result.observations.empty());
            }
        }},
        {"invalid PCM and packet limits fail atomically", [] {
            const std::vector<float> overlong(96001,.1f);
            AudioFeatureAssembler assembler;
            check(!assembler.push(packet(overlong,0)).packet_accepted);
            auto nan=std::vector<float>(480,.1f); nan.back()=std::numeric_limits<float>::quiet_NaN();
            check(assembler.push(packet(nan,0)).reason==AudioFeatureReason::nonfinite);
            auto invalid=packet(std::span<const float>(nan).first(1),0); invalid.sample_rate=44101;
            check(assembler.push(invalid).reason==AudioFeatureReason::unsupported_sample_rate);
            rejects([&] { extract_audio_features(overlong,48000,AudioFeatureKind::log_rms_v1,1); });
        }},
        {"offline extraction bounds and partial tail are explicit", [] {
            const auto frames=extract_audio_features(std::vector<float>(442,.1f),44100,AudioFeatureKind::log_rms_v1);
            check(frames.values.size()==1 && frames.source_sample_frames==442 && frames.dropped_tail_samples==1);
            rejects([] { MonoFeatureExtractor invalid(44101,AudioFeatureKind::log_rms_v1); });
            rejects([] { AudioFeatureConfig config;config.window_frames=501;AudioFeatureAssembler invalid(config); });
            check(std::string(audio_feature_id(AudioFeatureKind::log_rms_v1))!=audio_feature_id(AudioFeatureKind::iir8_log_energy_v1));
        }},
        {"reset preserves monotonically increasing real IDs and removes partial bin", [] {
            AudioFeatureConfig config;config.window_frames=20;AudioFeatureAssembler assembler(config);
            const auto before=feed(assembler,std::vector<float>(9600,.1f));
            check(before.observations.back().observation_id==1);
            const std::vector<float> partial(13,.1f);check(assembler.push(packet(partial,9600)).packet_accepted);
            assembler.reset(); const auto after=feed(assembler,std::vector<float>(9600,.2f),48000,20000);
            check(after.observations.size()==1 && after.observations[0].observation_id==2 && std::abs(after.observations[0].rms-.2)<1e-7);
        }}
    };
    for(const auto& [name,run]:tests) {
        try {run();} catch(const std::exception& error) {std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';return 1;}
    }
    std::cout<<tests.size()<<" portable audio feature checks passed\n";
    return 0;
}
