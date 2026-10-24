#include "rhythm/audio_capture_win.hpp"
#include "rhythm/audio_features.hpp"
#include "rhythm/audio_file_win.hpp"
#include <nlohmann/json.hpp>
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace rhythm;
std::atomic<bool> interrupted{false};
BOOL WINAPI stop_signal(DWORD signal) {
    if(signal==CTRL_C_EVENT || signal==CTRL_BREAK_EVENT || signal==CTRL_CLOSE_EVENT) {
        interrupted.store(true);return TRUE;
    }
    return FALSE;
}
Ns integer(const char* text) {
    const std::string input=text;Ns result=0;
    const auto parsed=std::from_chars(input.data(),input.data()+input.size(),result);
    if(parsed.ec!=std::errc{} || parsed.ptr!=input.data()+input.size()) throw std::invalid_argument("invalid integer");
    return result;
}
nlohmann::json estimate_json(const AudioSyncEstimate& value,Ns now,Ns compute_ns) {
    return {{"type","estimate"},{"host_now_ns",now},{"compute_ns",compute_ns},{"state",audio_sync_state_name(value.state)},
        {"reason",audio_sync_reason_name(value.reason)},{"observation_id",value.observation_id},{"epoch",value.lease.epoch},
        {"observed_host_ns",value.lease.observed_host_ns},{"expires_host_ns",value.lease.expires_host_ns},
        {"uncertainty_ns",value.lease.uncertainty_ns},{"matched_song_ns",value.matched_song_ns},
        {"song_anchor_ns",value.clock.song_anchor_ns},{"host_anchor_ns",value.clock.host_anchor_ns},
        {"host_per_song",static_cast<double>(value.clock.host_per_song)},{"phase_residual_ns",value.phase_residual_ns},
        {"fit_residual_ns",value.fit_residual_ns},{"peak",value.peak},{"runner_up",value.runner_up},{"margin",value.margin},
        {"anchors",value.anchor_count},{"global_unique",value.global_unique},{"continuity_assumed",value.continuity_assumed},
        {"live_qualified",false}};
}
}
int main(int argc,char** argv) {
    using namespace rhythm;
    try {
        if(argc<2) throw std::invalid_argument("mode required: replay or capture");
        const std::string mode=argv[1];
        const bool capture=mode=="capture";
        if((capture && argc!=9) || (!capture && (mode!="replay" || argc!=8)))
            throw std::invalid_argument(
                "Usage:\n chart_shadow replay REFERENCE.wav SHA OBSERVATION.wav NEW_DIR rms|iir8|fft8 WINDOW_FRAMES\n"
                " chart_shadow capture REFERENCE.wav SHA NEW_DIR rms|iir8|fft8 WINDOW_FRAMES SECONDS TIMESTAMP_BUDGET_MS\n"
                "Shadow only, no device/touch. Capture observes default host render mix. Budget is an uncalibrated diagnostic assumption.");
        const std::filesystem::path output=argv[capture ? 4 : 5];
        if(std::filesystem::exists(output)) throw std::invalid_argument("output must not exist");
        const std::string kind_text=argv[capture ? 5 : 6];
        const auto kind=kind_text=="rms" ? AudioFeatureKind::log_rms_v1 :
                        kind_text=="iir8" ? AudioFeatureKind::iir8_log_energy_v1 :
                        kind_text=="fft8" ? AudioFeatureKind::fft8_log_energy_v1 :
                        throw std::invalid_argument("feature must be rms, iir8 or fft8");
        const auto window=integer(argv[capture ? 6 : 7]);
        if(window<200 || window>500) throw std::invalid_argument("window 200..500 frames");
        Ns seconds=0,budget=0;
        if(capture) {
            seconds=integer(argv[7]);const auto budget_ms=integer(argv[8]);
            if(seconds<1 || seconds>180 || budget_ms<1 || budget_ms>20)
                throw std::invalid_argument("capture 1..180s and timestamp budget 1..20ms");
            budget=budget_ms*1'000'000;
        }
        const auto reference_wav=load_audio_wav(argv[2],std::string(argv[3]));
        const auto reference_features=extract_audio_features(reference_wav.mono,reference_wav.sample_rate,kind);
        AudioFeatureReference reference{reference_features.feature_rate_hz,reference_features.bands,
            reference_features.values,reference_features.feature_origin_ns,reference_wav.sha256,reference_features.feature_id};
        AudioSyncConfig sync_config;sync_config.window_frames=static_cast<std::size_t>(window);
        AudioSync sync(std::move(reference),sync_config);
        AudioFeatureConfig feature_config;feature_config.kind=kind;feature_config.window_frames=static_cast<std::size_t>(window);
        AudioFeatureAssembler assembler(feature_config);
        std::optional<LoadedMonoWav> replay_wav;
        if(!capture) replay_wav=load_audio_wav(argv[4]);
        if(!std::filesystem::create_directory(output)) throw std::runtime_error("cannot create new output directory");
        std::ofstream timeline(output/"timeline.jsonl",std::ios::binary),packets(output/"packets.jsonl",std::ios::binary);
        if(!timeline || !packets) throw std::runtime_error("cannot open evidence files");
        std::ofstream pcm;
        if(capture) {
            static_assert(std::endian::native==std::endian::little && sizeof(float)==4);
            pcm.open(output/"mono.f32le",std::ios::binary);
            if(!pcm) throw std::runtime_error("cannot open bounded capture PCM evidence");
        }
        std::uint64_t pcm_frames=0;
        std::uint64_t observations=0,locked=0,resets=0,source_packets=0;
        Ns max_compute_ns=0,first_lock=-1;
        AudioSyncEstimate previous=sync.last_result();
        const auto process=[&](const MonoAudioPacket& packet,Ns now) {
            ++source_packets;
            const auto pcm_offset=pcm_frames;
            if(capture) {
                constexpr std::uint64_t max_pcm_frames=192000ULL*182;
                if(packet.samples.size()>max_pcm_frames-pcm_frames)
                    throw std::runtime_error("capture PCM evidence budget exceeded");
                pcm.write(reinterpret_cast<const char*>(packet.samples.data()),
                    static_cast<std::streamsize>(packet.samples.size_bytes()));
                if(!pcm) throw std::runtime_error("capture PCM write failure");
                pcm_frames+=packet.samples.size();
            }
            const auto batch=assembler.push(packet);
            packets<<nlohmann::json{{"sequence",source_packets},{"device_position",packet.device_position},
                {"frames",packet.samples.size()},{"sample_rate",packet.sample_rate},{"first_host_ns",packet.first_host_ns},
                {"received_host_ns",packet.received_host_ns},{"timestamp_uncertainty_budget_ns",packet.timestamp_uncertainty_ns},
                {"recorded_mono_frame_offset",capture ? nlohmann::json(pcm_offset) : nlohmann::json(nullptr)},
                {"discontinuity",packet.discontinuity},{"timestamp_error",packet.timestamp_error},
                {"accepted",batch.packet_accepted},{"segment_id",batch.segment_id},{"segment_reset",batch.segment_reset},
                {"reason",audio_feature_reason_name(batch.reason)}}.dump()<<'\n';
            if(batch.segment_reset) {
                ++resets;sync.reset(1,AudioSyncReason::reset);
                timeline<<nlohmann::json{{"type","capture_segment_reset"},{"host_now_ns",now},
                    {"feature_reason",audio_feature_reason_name(batch.reason)},{"segment_id",batch.segment_id}}.dump()<<'\n';
            }
            for(const auto& observation:batch.observations) {
                const auto started=audio_host_qpc_ns();
                auto estimate=sync.observe(observation,capture ? started : now);
                const auto ended=audio_host_qpc_ns(),compute=ended-started;
                // Correlation CPU time is real age in capture mode; never return
                // a lease that only looked fresh before the expensive query.
                if(capture) estimate=sync.status(ended);
                ++observations;max_compute_ns=std::max(max_compute_ns,compute);
                if(estimate.state==AudioSyncState::locked) {
                    ++locked;if(first_lock<0) first_lock=observation.window_end_host_ns;
                }
                timeline<<estimate_json(estimate,capture ? ended : now,compute).dump()<<'\n';
                previous=estimate;
            }
            if(!timeline || !packets) throw std::runtime_error("evidence write failure");
        };
        nlohmann::json manifest={{"schema","pas.audio-shadow.v1"},{"mode",mode},{"live_qualified",false},{"input_enabled",false},
            {"reference_sha256",reference_wav.sha256},{"reference_frames",reference_wav.frames},
            {"reference_sample_rate",reference_wav.sample_rate},{"feature_id",reference_features.feature_id},
            {"bands",reference_features.bands},{"feature_origin_ns",reference_features.feature_origin_ns},
            {"window_frames",window},{"hop_frames",feature_config.hop_frames},
            {"acquisition","three compatible globally unique anchors, each at least half-window new features"},
            {"true_phase_error","unknown"},{"playing_state","unknown; shadow cannot arm input"},
            {"sample_timestamp_budget_calibrated",false}};
        SetConsoleCtrlHandler(stop_signal,TRUE);
        bool user_stopped=false;
        if(capture) {
            AudioCaptureConfig config;config.max_duration_ns=seconds*1'000'000'000;
            WasapiLoopbackCapture source(config);
            source.start();
            for(;;) {
                if(interrupted.load() || std::filesystem::exists(output/"STOP")) {user_stopped=true;source.stop();break;}
                for(const auto& packet:source.drain()) {
                    process({packet.mono,packet.sample_rate,packet.device_position_frames,packet.first_host_ns,
                        packet.received_host_ns,budget,packet.discontinuity,packet.timestamp_error},audio_host_qpc_ns());
                }
                const auto now=audio_host_qpc_ns();const auto status=sync.status(now);
                if(status.state!=previous.state || status.reason!=previous.reason) {
                    timeline<<estimate_json(status,now,0).dump()<<'\n';previous=status;
                }
                const auto state=source.status();
                if(state.failed || state.finished) break;
                Sleep(2);
            }
            source.stop();
            if(!user_stopped) for(const auto& packet:source.drain())
                process({packet.mono,packet.sample_rate,packet.device_position_frames,packet.first_host_ns,
                    packet.received_host_ns,budget,packet.discontinuity,packet.timestamp_error},audio_host_qpc_ns());
            const auto state=source.status();
            manifest["capture"]={{"endpoint_id",state.endpoint_id},{"scope","default eRender/eMultimedia host mix; no microphone"},
                {"sample_rate",state.sample_rate},{"channels",state.channels},{"bits",state.bits_per_sample},
                {"packets",state.packets},{"frames",state.frames},{"dropped_packets",state.dropped_packets},
                {"timestamp_errors",state.timestamp_errors},{"discontinuities",state.discontinuities},
                {"failed",state.failed},{"error",state.error},{"duration_limit_seconds",seconds},
                {"timestamp_budget_ns",budget},{"timestamp_contract","GetBuffer first-sample QPC100ns times 100; absolute host QPC ns"}};
            manifest["status"]=state.failed ? "capture_failed" : user_stopped ? "user_stopped" : "completed_shadow";
        } else {
            const auto& wave=*replay_wav;
            if(wave.sample_rate%100) throw std::invalid_argument("replay rate must divide into 100Hz feature frames");
            const std::size_t chunk=wave.sample_rate/100; // 10ms packets, no wall-time clock claims.
            constexpr Ns origin=100'000'000'000;
            for(std::size_t position=0;position<wave.mono.size();position+=chunk) {
                if(interrupted.load() || std::filesystem::exists(output/"STOP")) {user_stopped=true;break;}
                const auto count=std::min(chunk,wave.mono.size()-position);
                const auto first=origin+static_cast<Ns>(static_cast<long double>(position)*1e9L/wave.sample_rate);
                const auto end=origin+static_cast<Ns>(static_cast<long double>(position+count)*1e9L/wave.sample_rate);
                process({std::span<const float>(wave.mono).subspan(position,count),wave.sample_rate,position,
                    first,end+10'000'000,1'000'000,false,false},end+10'000'000);
            }
            manifest["observation_sha256"]=wave.sha256;manifest["observation_frames"]=wave.frames;
            manifest["observation_sample_rate"]=wave.sample_rate;
            manifest["clock_scope"]="simulated sample clock, host origin100s,10ms delivery; does not recover recorded QPC/PTS";
            manifest["status"]=user_stopped ? "user_stopped" : "completed_replay";
        }
        SetConsoleCtrlHandler(stop_signal,FALSE);
        manifest["observations"]=observations;manifest["locked_observations"]=locked;manifest["segment_resets"]=resets;
        if(capture) {
            pcm.close();if(!pcm) throw std::runtime_error("capture PCM finalization failed");
            manifest["pcm_evidence"]={{"path","mono.f32le"},{"format","IEEE float32 little-endian arithmetic channel mean"},
                {"frames",pcm_frames},{"bytes",pcm_frames*4},{"index","packets.jsonl"},
                {"gaps","no padding; packet timestamps/device positions preserve discontinuities"},
                {"hash_status","compute SHA256 after closed manifest; not supplied by this CLI"}};
        }
        manifest["first_locked_window_end_host_ns"]=first_lock<0 ? nlohmann::json(nullptr) : nlohmann::json(first_lock);
        manifest["max_observe_compute_ns"]=max_compute_ns;
        timeline.flush();packets.flush();if(!timeline || !packets) throw std::runtime_error("evidence finalization failed");
        std::ofstream summary(output/"manifest.json",std::ios::binary);summary<<manifest.dump(2)<<'\n';summary.close();
        if(!summary) throw std::runtime_error("manifest write failed");
        std::cout<<manifest.dump(2)<<'\n';
        return capture && manifest["capture"]["failed"]==true ? 2 : 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
