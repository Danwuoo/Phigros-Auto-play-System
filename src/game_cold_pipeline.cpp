#include "pas/game.hpp"
#include "pas/analysis.hpp"
#include "pas/journal.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <bcrypt.h>

namespace pas {
namespace {

constexpr int width=1280,height=720,stride=width*3,poses=12;
constexpr std::size_t rgb_bytes=static_cast<std::size_t>(stride)*height;

// This is a fixed, independent RGB stimulus. It contains a visible playing HUD,
// one long line and a Tap approaching it. Only pixels enter GameObserver.
void box(std::vector<std::uint8_t>& rgb,int x0,int y0,int x1,int y1,
         std::array<std::uint8_t,3> color) {
    for(int y=std::max(0,y0);y<std::min(height,y1);++y)
        for(int x=std::max(0,x0);x<std::min(width,x1);++x) {
            auto* pixel=rgb.data()+static_cast<std::size_t>(y)*stride+x*3;
            std::copy(color.begin(),color.end(),pixel);
        }
}

std::array<std::vector<std::uint8_t>,poses> make_stimulus(std::string_view scene) {
    std::array<std::vector<std::uint8_t>,poses> images;
    for(int i=0;i<poses;++i) {
        auto& rgb=images[i];rgb.assign(rgb_bytes,0);
        box(rgb,20,20,26,42,{255,255,255});
        box(rgb,34,20,40,42,{255,255,255});
        for(int digit=0;digit<6;++digit)
            box(rgb,1020+digit*24,20,1032+digit*24,40,{255,255,255});
        if(scene=="tap") {
            box(rgb,115,479,1165,482,{255,255,255});
            const int note_y=385+i*14;
            box(rgb,491,note_y-4,569,note_y+5,{40,190,255});
        } else if(scene=="dense") {
            for(int row=0;row<16;++row) {
                const int y=150+row*31;
                box(rgb,40,y-1,1240,y+2,{255,255,255});
                for(int col=0;col<8;++col) {
                    const int x=190+col*130;
                    box(rgb,x-39,y-12,x+39,y-3,{40,190,255});
                }
            }
        } else throw std::invalid_argument("unknown cold pipeline scene");
    }
    return images;
}

std::string stimulus_sha256(const std::array<std::vector<std::uint8_t>,poses>& images) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)
        throw std::runtime_error("cold stimulus SHA256 provider");
    const auto cleanup=[&] {
        if(hash)BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm,0);
    };
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0) {
        cleanup();throw std::runtime_error("cold stimulus SHA256 create");
    }
    for(const auto& image:images)
        if(BCryptHashData(hash,const_cast<PUCHAR>(image.data()),
            static_cast<ULONG>(image.size()),0)<0) {
            cleanup();throw std::runtime_error("cold stimulus SHA256 update");
        }
    std::array<unsigned char,32> digest{};
    if(BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0) {
        cleanup();throw std::runtime_error("cold stimulus SHA256 finish");
    }
    cleanup();
    std::ostringstream value;
    for(const auto byte:digest)value<<std::hex<<std::setfill('0')<<std::setw(2)<<static_cast<int>(byte);
    return value.str();
}

std::uint64_t working_set_bytes() {
    PROCESS_MEMORY_COUNTERS counters{};counters.cb=sizeof(counters);
    if(!K32GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters)))
        throw std::runtime_error("cold pipeline RSS measurement");
    return counters.WorkingSetSize;
}

struct Sample {
    Nanoseconds capture=0,published=0,recognition=0,owner=0;
    Nanoseconds publish_cost=0,writer_enqueue_cost=0,owner_cost=0;
    Nanoseconds components_cost=0,base_scene_cost=0,line_scan_cost=0;
    Nanoseconds note_decode_cost=0,held_recovery_cost=0,tracking_cost=0;
    bool published_ok=false,consumed=false,owner_seen=false,writer_ok=false;
};

class LoadedFakeTouch final:public TouchBackend {
public:
    LoadedFakeTouch(const Clock& clock,int delay_ms)
        :clock_(clock),inner_(clock),delay_ms_(delay_ms) {}
    TouchReceipt inject(const TouchCommand& command) override {
        const auto start=clock_.now_ns();
        auto receipt=inner_.inject(command);
        if(delay_ms_>0)std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
        receipt.injection_start_ns=start;
        receipt.injection_return_ns=clock_.now_ns();
        receipts_.push_back(receipt);
        return receipt;
    }
    ReleaseReport release_all() override {return inner_.release_all();}
    const auto& contacts() const {return inner_.contacts();}
    const auto& receipts() const {return receipts_;}
private:
    const Clock& clock_;
    FakeTouchBackend inner_;
    int delay_ms_;
    std::vector<TouchReceipt> receipts_;
};

} // namespace

nlohmann::json benchmark_game_cold_pipeline(const std::filesystem::path& journal_path,
    int frames,int cadence_ms,bool jitter,bool row_prescreen,
    const std::string& scene,int writer_capacity,int writer_delay_us,int rpc_delay_ms) {
    using nlohmann::json;
    if(frames<12||frames>10000||cadence_ms<8||cadence_ms>50||journal_path.empty()||
       (scene!="tap"&&scene!="dense")||writer_capacity<2||writer_capacity>8192||
       writer_delay_us<0||writer_delay_us>50000||rpc_delay_ms<0||rpc_delay_ms>50)
        throw std::invalid_argument("cold pipeline benchmark capacity/config");
    const auto images=make_stimulus(scene);
    const auto input_sha256=stimulus_sha256(images);
    const auto rss_initial=working_set_bytes();
    std::vector<std::uint64_t> rss_samples(static_cast<std::size_t>((frames+99)/100));
    HostClock clock;
    LatestFrame latest(width,height,3,&clock);
    std::function<void()> writer_hook;
    if(writer_delay_us>0)writer_hook=[writer_delay_us] {
        std::this_thread::sleep_for(std::chrono::microseconds(writer_delay_us));
    };
    Journal journal(journal_path,static_cast<std::size_t>(writer_capacity),
        std::move(writer_hook));
    std::vector<Sample> samples(static_cast<std::size_t>(frames)+1);
    std::atomic<bool> capture_done=false,perception_done=false,failed=false;
    std::atomic<std::uint64_t> last_decision=0;
    std::mutex decision_mutex,error_mutex;
    std::condition_variable decision_ready;
    std::shared_ptr<const DecisionSnapshot> decision;
    std::exception_ptr worker_error;
    const auto fail=[&] {
        {std::lock_guard lock(error_mutex);if(!worker_error)worker_error=std::current_exception();}
        failed=true;capture_done=true;perception_done=true;
        latest.close();decision_ready.notify_all();
    };
    bool contacts_released=false;
    std::vector<TouchReceipt> receipts;
    std::uint64_t accepted_plans=0,coverage_updates=0,plan_cancellations=0;
    std::uint64_t scheduler_rejections=0;
    const Nanoseconds started=clock.now_ns();
    {
        std::jthread capture([&] {
            try {
                const auto wall_start=std::chrono::steady_clock::now();
                for(int i=1;i<=frames&&!failed;++i) {
                    // Deterministic periodic jitter, zero mean over four frames.
                    const int offset=jitter?std::array<int,4>{-2,1,2,-1}[(i-1)%4]:0;
                    std::this_thread::sleep_until(wall_start+
                        std::chrono::milliseconds((i-1)*cadence_ms+offset+2));
                    Frame metadata;metadata.sequence=static_cast<std::uint64_t>(i);
                    metadata.epoch=1;metadata.generation=1;metadata.geometry_version=1;
                    metadata.width=width;metadata.height=height;metadata.stride=stride;
                    metadata.source_rotation=1;
                    metadata.capture_backend="fake_memory_rgb";
                    metadata.source_pixel_format="RGB888_top_down";
                    metadata.capture_complete_ns=clock.now_ns();
                    metadata.pixels_ready_ns=metadata.capture_complete_ns;
                    auto& sample=samples[i];sample.capture=metadata.capture_complete_ns;
                    const auto begin=clock.now_ns();
                    sample.published_ok=latest.publish(images[(i-1)%poses].data(),
                        rgb_bytes,metadata);
                    // publish() can already have woken the consumer. Only
                    // this producer writes publish_cost, which is read after
                    // joining; publication itself belongs to the leased Frame.
                    sample.publish_cost=clock.now_ns()-begin;
                    if(i%100==0)rss_samples[(i-1)/100]=working_set_bytes();
                }
            } catch(...) {fail();}
            capture_done=true;
        });
        std::jthread perception([&] {
            try {
                GameObserver observer(clock,false,row_prescreen);
                std::uint64_t seen=0;
                while(!failed) {
                    auto frame=latest.read_after(seen,10'000'000);
                    if(!frame) {if(capture_done)break;continue;}
                    seen=frame->sequence;
                    auto scene=std::make_shared<DecisionSnapshot>(observer.process(*frame));
                    auto& sample=samples[seen];
                    sample.published=frame->published_ns;
                    sample.recognition=scene->recognition_end_ns;
                    sample.components_cost=scene->components_compute_ns;
                    sample.base_scene_cost=scene->base_scene_compute_ns;
                    sample.line_scan_cost=scene->line_scan_compute_ns;
                    sample.note_decode_cost=scene->note_decode_compute_ns;
                    sample.held_recovery_cost=scene->held_recovery_compute_ns;
                    sample.tracking_cost=scene->tracking_compute_ns;
                    sample.consumed=true;
                    frame.reset();
                    const auto enqueue_start=clock.now_ns();
                    sample.writer_ok=journal.push(decision_json(*scene),false);
                    sample.writer_enqueue_cost=clock.now_ns()-enqueue_start;
                    journal.push({{"event","cold_frame_consumed"},
                        {"source_frame",seen},{"capture_complete_ns",sample.capture},
                        {"published_ns",sample.published},
                        {"recognition_end_ns",sample.recognition}},false);
                    {
                        std::lock_guard lock(decision_mutex);
                        decision=std::move(scene);
                        last_decision=decision->sequence;
                    }
                    decision_ready.notify_one();
                }
            } catch(...) {fail();}
            perception_done=true;decision_ready.notify_all();latest.close();
        });
        std::jthread action([&] {
            try {
                LoadedFakeTouch touch(clock,rpc_delay_ms);
                GamePlanOwner owner(clock,touch,5,{15,0,30'000'000});
                const auto drain_diagnostics=[&] {
                    for(const auto& plan:owner.take_accepted_plans()) {
                        ++accepted_plans;
                        journal.push({{"event","cold_plan_accepted"},
                            {"source_frame",plan.source_frame_sequence},
                            {"intent_id",plan.intent_id},{"revision",plan.revision},
                            {"evidence_ns",plan.evidence_ns},
                            {"valid_until_ns",plan.valid_until_ns}},false);
                    }
                    for(auto event:owner.take_coverage_updates()) {
                        ++coverage_updates;journal.push(std::move(event),false);
                    }
                    for(auto event:owner.take_plan_cancellations()) {
                        ++plan_cancellations;journal.push(std::move(event),false);
                    }
                    for(const auto& notice:owner.scheduler().take_notices()) {
                        ++scheduler_rejections;
                        journal.push({{"event","cold_scheduler_rejection"},
                            {"intent_id",notice.intent_id},{"reason",notice.reason},
                            {"monotonic_ns",notice.monotonic_ns}},false);
                    }
                };
                std::uint64_t seen=0;
                Nanoseconds drain_start=0;
                while(!failed) {
                    std::shared_ptr<const DecisionSnapshot> current;
                    {
                        std::unique_lock lock(decision_mutex);
                        decision_ready.wait_for(lock,std::chrono::milliseconds(1),[&] {
                            return failed||perception_done||(decision&&decision->sequence>seen);
                        });
                        if(decision&&decision->sequence>seen) current=decision;
                    }
                    if(current) {
                        seen=current->sequence;
                        const auto source=current->context.frame;
                        const auto begin=clock.now_ns();
                        const auto immediate=owner.accept(*current);
                        auto& sample=samples.at(source);sample.owner=clock.now_ns();
                        sample.owner_cost=sample.owner-begin;sample.owner_seen=true;
                        journal.push({{"event","cold_owner_accept"},
                            {"source_frame",source},{"decision_sequence",seen},
                            {"owner_accept_end_ns",sample.owner},
                            {"owner_accept_cost_ns",sample.owner_cost}},false);
                        drain_diagnostics();
                        for(const auto& receipt:immediate)
                            journal.push({{"event","cold_touch_receipt"},
                                {"source_frame",receipt.command.source_frame_sequence},
                                {"intent_id",receipt.command.intent_id},
                                {"phase",static_cast<int>(receipt.command.phase)},
                                {"scheduled_ns",receipt.command.scheduled_ns},
                                {"injection_start_ns",receipt.injection_start_ns},
                                {"injection_return_ns",receipt.injection_return_ns},
                                {"success",receipt.success}},false);
                    }
                    for(const auto& receipt:owner.poll())
                        journal.push({{"event","cold_touch_receipt"},
                            {"source_frame",receipt.command.source_frame_sequence},
                            {"intent_id",receipt.command.intent_id},
                            {"phase",static_cast<int>(receipt.command.phase)},
                            {"scheduled_ns",receipt.command.scheduled_ns},
                            {"injection_start_ns",receipt.injection_start_ns},
                            {"injection_return_ns",receipt.injection_return_ns},
                            {"success",receipt.success}},false);
                    drain_diagnostics();
                    if(perception_done&&seen>=last_decision) {
                        if(!drain_start)drain_start=clock.now_ns();
                        if(clock.now_ns()-drain_start>=100'000'000)break;
                    }
                }
                owner.stop();contacts_released=touch.contacts().empty();
                drain_diagnostics();
                receipts=touch.receipts();
            } catch(...) {fail();}
        });
    }
    const Nanoseconds finished=clock.now_ns();
    const auto rss_final=working_set_bytes();
    journal.close();
    if(worker_error)std::rethrow_exception(worker_error);
    const auto counters=latest.counters();
    std::vector<double> publish_ms,capture_to_recognition_ms,capture_to_owner_ms;
    std::vector<double> writer_enqueue_ms,owner_cost_ms,capture_to_down_ms,lateness_ms;
    std::vector<double> components_ms,base_scene_ms,line_scan_ms,note_decode_ms;
    std::vector<double> held_recovery_ms,tracking_ms;
    std::vector<double> rss_mib;
    std::uint64_t published=0,consumed=0,owned=0,writer_rejected=0;
    publish_ms.reserve(frames);capture_to_recognition_ms.reserve(frames);
    capture_to_owner_ms.reserve(frames);writer_enqueue_ms.reserve(frames);
    for(const auto bytes:rss_samples)if(bytes)rss_mib.push_back(bytes/1048576.0);
    for(int i=1;i<=frames;++i) {
        const auto& sample=samples[i];
        publish_ms.push_back(sample.publish_cost/1e6);
        if(sample.published_ok)++published;
        if(sample.consumed) {
            ++consumed;
            capture_to_recognition_ms.push_back((sample.recognition-sample.capture)/1e6);
            writer_enqueue_ms.push_back(sample.writer_enqueue_cost/1e6);
            components_ms.push_back(sample.components_cost/1e6);
            base_scene_ms.push_back(sample.base_scene_cost/1e6);
            line_scan_ms.push_back(sample.line_scan_cost/1e6);
            note_decode_ms.push_back(sample.note_decode_cost/1e6);
            held_recovery_ms.push_back(sample.held_recovery_cost/1e6);
            tracking_ms.push_back(sample.tracking_cost/1e6);
            if(!sample.writer_ok)++writer_rejected;
        }
        if(sample.owner_seen) {
            ++owned;capture_to_owner_ms.push_back((sample.owner-sample.capture)/1e6);
            owner_cost_ms.push_back(sample.owner_cost/1e6);
        }
    }
    std::uint64_t down=0,move=0,up=0,failed_receipts=0,unknown_receipts=0;
    for(const auto& receipt:receipts) {
        if(receipt.command.phase==Phase::down) {
            ++down;
            const auto source=receipt.command.source_frame_sequence;
            if(source>0&&source<samples.size()&&samples[source].capture)
                capture_to_down_ms.push_back((receipt.injection_start_ns-samples[source].capture)/1e6);
        } else if(receipt.command.phase==Phase::move)++move;
        else if(receipt.command.phase==Phase::up)++up;
        if(!receipt.success)++failed_receipts;
        if(receipt.reason.find("unknown")!=std::string::npos)++unknown_receipts;
        lateness_ms.push_back((receipt.injection_start_ns-receipt.command.scheduled_ns)/1e6);
    }
    return {{"schema_version",1},{"benchmark","memory_fake_capture_game_pipeline"},
        {"clock_domain","host_qpc_ns"},{"business_clock","same_host_qpc"},
        {"publication_time_basis","latest_frame_lease_under_publish_lock"},
        {"source_render_age","unknown"},{"grpc_transport","not_measured"},
        {"game_effect","unknown"},{"no_device_access",true},
        {"stimulus_version",scene=="tap"?1:2},{"scene",scene},
        {"stimulus_sha256",input_sha256},
        {"rss_initial_bytes",rss_initial},{"rss_final_bytes",rss_final},
        {"rss_sampling_every_frames",100},{"rss_sample_mib",distribution(rss_mib)},
        {"row_prescreen",row_prescreen},{"requested_frames",frames},
        {"unique_rgb_frames",scene=="tap"?poses:1},
        {"loops",(frames+poses-1)/poses},
        {"writer_capacity",writer_capacity},{"writer_delay_us",writer_delay_us},
        {"rpc_delay_ms",rpc_delay_ms},
        {"cadence_ms",cadence_ms},{"deterministic_jitter",jitter},
        {"wall_time_ms",(finished-started)/1e6},
        {"published",published},{"consumed",consumed},{"owner_seen",owned},
        {"mailbox",{{"published",counters.published},{"overwritten",counters.overwritten},
            {"pool_drops",counters.pool_drops},{"consumer_skips",counters.consumer_skips}}},
        {"writer_rejected",writer_rejected},{"writer_debug_drops",journal.debug_drops()},
        {"writer_faulted",journal.faulted()},{"journal_path",journal_path.string()},
        {"touch_receipts",receipts.size()},{"down",down},{"move",move},{"up",up},
        {"accepted_plans",accepted_plans},{"coverage_updates",coverage_updates},
        {"plan_cancellations",plan_cancellations},
        {"scheduler_rejections",scheduler_rejections},
        {"failed_receipts",failed_receipts},{"unknown_receipts",unknown_receipts},
        {"contacts_released",contacts_released},
        {"publish_copy_ms",distribution(publish_ms)},
        {"capture_complete_to_recognition_ms",distribution(capture_to_recognition_ms)},
        {"capture_complete_to_owner_accept_ms",distribution(capture_to_owner_ms)},
        {"writer_enqueue_ms",distribution(writer_enqueue_ms)},
        {"components_compute_ms",distribution(components_ms)},
        {"base_scene_compute_ms",distribution(base_scene_ms)},
        {"line_scan_compute_ms",distribution(line_scan_ms)},
        {"note_decode_compute_ms",distribution(note_decode_ms)},
        {"held_recovery_compute_ms",distribution(held_recovery_ms)},
        {"tracking_compute_ms",distribution(tracking_ms)},
        {"owner_accept_ms",distribution(owner_cost_ms)},
        {"capture_complete_to_down_start_ms",distribution(capture_to_down_ms)},
        {"touch_lateness_ms",distribution(lateness_ms)}};
}

nlohmann::json analyze_game_cold_pipeline_ab(const std::filesystem::path& directory) {
    using nlohmann::json;
    const auto read_json=[](const std::filesystem::path& path) {
        if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>1'048'576)
            throw std::invalid_argument("cold pipeline A/B missing or oversized input: "+path.string());
        std::ifstream stream(path);
        if(!stream)throw std::runtime_error("cold pipeline A/B open failed: "+path.string());
        return json::parse(stream);
    };
    const auto tolerance_path=directory/"pipeline-aa-tolerance.json";
    const auto freeze=read_json(tolerance_path);
    if(freeze.at("status")!="frozen_before_formal_ab"||
       freeze.at("stimulus_sha256").get<std::string>().empty())
        throw std::invalid_argument("cold pipeline A/B tolerance is not frozen");
    const auto expected_meter=freeze.at("meter_binary_sha256").get<std::string>();
    const auto actual_meter=sha256_file(freeze.at("meter_binary").get<std::string>());
    if(actual_meter!=expected_meter)
        throw std::invalid_argument("cold pipeline A/B meter binary changed");
    const auto& limits=freeze.at("frozen_tolerance");
    json batches=json::array(),failures=json::array();
    const auto score=[&](json& checks,const std::string& name,double baseline,
                         double candidate,double permitted,bool improvement) {
        const double gain=baseline-candidate;
        const bool pass=improvement?gain>permitted:candidate-baseline<=permitted;
        checks.push_back({{"metric",name},{"baseline_mean",baseline},
            {"candidate_mean",candidate},{"baseline_minus_candidate",gain},
            {"frozen_tolerance",permitted},{"pass",pass}});
        return pass;
    };
    bool all_pass=true;
    for(int batch=1;batch<=3;++batch) {
        std::array<json,4> runs;
        const std::array<bool,4> candidate{false,true,true,false};
        for(int slot=0;slot<4;++slot) {
            const auto name="pipeline-ab-"+std::to_string(batch)+"-"+
                std::to_string(slot)+(candidate[slot]?"-b.json":"-a.json");
            runs[slot]=read_json(directory/name);
            const auto& r=runs[slot];
            const bool valid=r.at("stimulus_sha256")==freeze.at("stimulus_sha256")&&
                r.at("requested_frames")==freeze.at("input").at("frames_per_run")&&
                r.at("cadence_ms")==freeze.at("input").at("cadence_ms")&&
                r.at("unique_rgb_frames")==freeze.at("input").at("unique_rgb_frames")&&
                r.at("deterministic_jitter")==freeze.at("input").at("jitter")&&
                r.value("scene",std::string("tap"))==
                    freeze.at("input").value("scene",std::string("tap"))&&
                r.value("writer_capacity",8192)==
                    freeze.at("input").value("writer_capacity",8192)&&
                r.value("writer_delay_us",0)==
                    freeze.at("input").value("writer_delay_us",0)&&
                r.value("rpc_delay_ms",0)==
                    freeze.at("input").value("rpc_delay_ms",0)&&
                r.at("row_prescreen")==candidate[slot]&&
                r.at("published")==r.at("requested_frames")&&
                r.at("no_device_access")==true&&r.at("contacts_released")==true&&
                r.at("mailbox").at("pool_drops")==0&&r.at("writer_rejected")==0&&
                r.at("writer_debug_drops")==0&&r.at("writer_faulted")==false&&
                r.at("failed_receipts")==0&&r.at("unknown_receipts")==0;
            if(!valid) {
                all_pass=false;failures.push_back(name+": input, denominator, or safety invariant");
            }
        }
        const auto pair_mean=[&](const std::string& key,const std::string& field,
                                 bool screened) {
            const int first=screened?1:0,second=screened?2:3;
            const auto value=[&](int slot) {
                const auto& r=runs[slot];
                return field.empty()?r.at(key).get<double>():
                    r.at(key).at(field).get<double>();
            };
            return (value(first)+value(second))/2;
        };
        json checks=json::array();bool passed=true;
        for(const auto& [key,prefix]:std::array<std::pair<std::string,std::string>,2>{
            {{"capture_complete_to_recognition_ms","capture_to_recognition"},
             {"capture_complete_to_owner_accept_ms","capture_to_owner"}}})
            for(const auto* percentile:{"p95","p99"}) {
                const std::string metric=prefix+"_"+percentile+"_ms";
                passed&=score(checks,metric,pair_mean(key,percentile,false),
                    pair_mean(key,percentile,true),limits.at(metric).get<double>(),true);
            }
        for(const auto* percentile:{"p95","p99"}) {
            const std::string metric=std::string("touch_lateness_")+percentile+"_ms";
            const bool no_touch=std::all_of(runs.begin(),runs.end(),[](const auto& r) {
                return r.at("touch_lateness_ms").at("n")==0;
            });
            const bool mixed_touch=std::any_of(runs.begin(),runs.end(),[](const auto& r) {
                return r.at("touch_lateness_ms").at("n")==0;
            });
            if(no_touch)checks.push_back({{"metric",metric},{"pass",true},
                {"not_applicable","no touch intents in dense scene"}});
            else if(mixed_touch) {
                checks.push_back({{"metric",metric},{"pass",false},
                    {"reason","touch sample denominator differs across modes"}});
                passed=false;
            } else passed&=score(checks,metric,
                pair_mean("touch_lateness_ms",percentile,false),
                pair_mean("touch_lateness_ms",percentile,true),
                limits.at(metric).get<double>(),false);
        }
        passed&=score(checks,"consumer_skips_frames",
            pair_mean("mailbox","consumer_skips",false),
            pair_mean("mailbox","consumer_skips",true),
            limits.at("consumer_skips_frames").get<double>(),false);
        const double baseline_down=pair_mean("down","",false);
        const double candidate_down=pair_mean("down","",true);
        const double down_limit=limits.at("down_count").get<double>();
        const bool down_pass=std::abs(candidate_down-baseline_down)<=down_limit;
        checks.push_back({{"metric","down_count"},{"baseline_mean",baseline_down},
            {"candidate_mean",candidate_down},
            {"baseline_minus_candidate",baseline_down-candidate_down},
            {"frozen_tolerance",down_limit},{"pass",down_pass}});
        passed&=down_pass;
        passed&=score(checks,"rss_peak_sample_mib",
            pair_mean("rss_sample_mib","max",false),
            pair_mean("rss_sample_mib","max",true),
            limits.at("rss_peak_sample_mib").get<double>(),false);
        if(!passed)all_pass=false;
        batches.push_back({{"batch",batch},{"order","ABBA"},
            {"attempts_per_mode",2000},{"checks",checks},{"pass",passed}});
    }
    return {{"schema_version",1},{"analysis","frozen_pipeline_interleaved_ab"},
        {"status",all_pass?"passed":"failed"},
        {"frozen_tolerance_sha256",sha256_file(tolerance_path)},
        {"meter_binary_sha256",actual_meter},
        {"stimulus_sha256",freeze.at("stimulus_sha256")},
        {"batches",batches},{"failures",failures},
        {"limits","single synthetic repeated RGB scene, fake-memory source, no game-effect claim"}};
}

nlohmann::json analyze_game_cold_pipeline_aa(const std::filesystem::path& directory,
    const std::filesystem::path& frozen_meter) {
    using nlohmann::json;
    if(!std::filesystem::is_regular_file(frozen_meter))
        throw std::invalid_argument("cold pipeline A/A meter missing");
    const std::array<std::pair<std::string,std::string>,9> metrics{{
        {"capture_to_recognition_p95_ms","capture_complete_to_recognition_ms.p95"},
        {"capture_to_recognition_p99_ms","capture_complete_to_recognition_ms.p99"},
        {"capture_to_owner_p95_ms","capture_complete_to_owner_accept_ms.p95"},
        {"capture_to_owner_p99_ms","capture_complete_to_owner_accept_ms.p99"},
        {"touch_lateness_p95_ms","touch_lateness_ms.p95"},
        {"touch_lateness_p99_ms","touch_lateness_ms.p99"},
        {"consumer_skips_frames","mailbox.consumer_skips"},
        {"down_count","down"},
        {"rss_peak_sample_mib","rss_sample_mib.max"}}};
    const std::array<double,9> minimum_tolerance{.75,1.0,.8,3.5,1.2,5.0,12.0,1.0,.5};
    json maxima=json::object(),pairs=json::array(),failures=json::array();
    for(const auto& [name,path]:metrics)maxima[name]=0.0;
    std::string stimulus,scene;int frames=0,cadence=0;
    int writer_capacity=8192,writer_delay_us=0,rpc_delay_ms=0;
    for(int batch=1;batch<=3;++batch) {
        std::array<json,2> pair;
        for(int side=0;side<2;++side) {
            const auto name="pipeline-aa-"+std::to_string(batch)+
                (side==0?"a.json":"b.json");
            const auto path=directory/name;
            if(!std::filesystem::is_regular_file(path)||
               std::filesystem::file_size(path)>1'048'576)
                throw std::invalid_argument("cold pipeline A/A missing or oversized run: "+name);
            std::ifstream stream(path);
            pair[side]=json::parse(stream);
            const auto& r=pair[side];
            if(stimulus.empty()) {
                stimulus=r.at("stimulus_sha256").get<std::string>();
                frames=r.at("requested_frames").get<int>();
                cadence=r.at("cadence_ms").get<int>();
                scene=r.value("scene",std::string("tap"));
                writer_capacity=r.value("writer_capacity",8192);
                writer_delay_us=r.value("writer_delay_us",0);
                rpc_delay_ms=r.value("rpc_delay_ms",0);
            }
            const bool valid=r.at("stimulus_sha256")==stimulus&&
                r.at("requested_frames")==frames&&r.at("published")==frames&&
                r.at("cadence_ms")==cadence&&r.at("row_prescreen")==false&&
                r.at("deterministic_jitter")==false&&
                r.value("scene",std::string("tap"))==scene&&
                r.value("writer_capacity",8192)==writer_capacity&&
                r.value("writer_delay_us",0)==writer_delay_us&&
                r.value("rpc_delay_ms",0)==rpc_delay_ms&&
                r.at("no_device_access")==true&&r.at("contacts_released")==true&&
                r.at("mailbox").at("pool_drops")==0&&
                r.at("writer_rejected")==0&&r.at("writer_debug_drops")==0&&
                r.at("writer_faulted")==false&&r.at("failed_receipts")==0&&
                r.at("unknown_receipts")==0;
            if(!valid)failures.push_back(name+": input, denominator, or safety invariant");
        }
        json differences=json::object();
        for(const auto& [name,path]:metrics) {
            const auto dot=path.find('.');
            const auto value=[&](const json& r) {
                if(name.starts_with("touch_lateness_")&&
                   r.at("touch_lateness_ms").at("n")==0)return 0.0;
                return dot==std::string::npos?r.at(path).get<double>():
                    r.at(path.substr(0,dot)).at(path.substr(dot+1)).get<double>();
            };
            if(name.starts_with("touch_lateness_")&&
               (pair[0].at("touch_lateness_ms").at("n")==0)!=
               (pair[1].at("touch_lateness_ms").at("n")==0))
                failures.push_back("A/A touch sample denominator differs within pair "+
                    std::to_string(batch));
            const double difference=std::abs(value(pair[0])-value(pair[1]));
            differences[name]=difference;
            maxima[name]=std::max(maxima.at(name).get<double>(),difference);
        }
        pairs.push_back({{"batch",batch},{"attempts_each",frames},
            {"differences",differences},{"a_consumed",pair[0].at("consumed")},
            {"b_consumed",pair[1].at("consumed")}});
    }
    json suggested=json::object();
    for(std::size_t i=0;i<metrics.size();++i) {
        const auto& name=metrics[i].first;
        suggested[name]=std::max(minimum_tolerance[i],
            maxima.at(name).get<double>()*1.5+.1);
    }
    return {{"schema_version",1},{"analysis","pipeline_full_scan_aa_noise"},
        {"status",failures.empty()?"measured":"failed"},
        {"meter_binary_sha256",sha256_file(frozen_meter)},
        {"stimulus_sha256",stimulus},{"scene",scene},
        {"writer_capacity",writer_capacity},
        {"writer_delay_us",writer_delay_us},{"rpc_delay_ms",rpc_delay_ms},
        {"frames_per_run",frames},
        {"cadence_ms",cadence},{"pairs",pairs},
        {"noise_observed_max_pair_difference",maxima},
        {"suggested_tolerance",suggested},
        {"tolerance_formula","max(prior frozen floor, 1.5 times largest A/A pair difference plus 0.1)"},
        {"failures",failures}};
}

nlohmann::json analyze_game_line_gap_sweep() {
    using nlohmann::json;
    json candidates=json::array();
    for(const int gap_limit:{3,4,5}) {
        int true_positive=0,false_negative=0,false_positive=0,true_negative=0;
        for(const int phase:{3,17,31})
            for(const int gap_width:{0,3,4,5,-1}) {
                Frame frame;frame.width=width;frame.height=height;frame.stride=stride;
                frame.epoch=1;frame.generation=1;frame.geometry_version=1;
                frame.source_rotation=1;frame.sequence=1;
                frame.capture_complete_ns=1'000'000'000LL+phase*1'000'000;
                frame.rgb.assign(rgb_bytes,0);
                box(frame.rgb,20,20,26,42,{255,255,255});
                box(frame.rgb,34,20,40,42,{255,255,255});
                for(int digit=0;digit<6;++digit)
                    box(frame.rgb,1020+digit*24,20,1032+digit*24,40,
                        {255,255,255});
                const bool truth_line=gap_width>=0&&gap_width<=4;
                if(gap_width>=0) {
                    box(frame.rgb,115,479,1165,482,{255,255,255});
                    for(int x=128+phase;x<1140;x+=64)
                        box(frame.rgb,x,479,x+gap_width,482,{0,0,0});
                }
                FakeClock clock;clock.set(frame.capture_complete_ns);
                GameObserver observer(clock,false,false,gap_limit);
                const auto scene=observer.process(frame);
                const bool detected=std::any_of(scene.lines.begin(),scene.lines.end(),
                    [](const LineCandidate& line) {
                        return std::abs(line.center.y-480)<=3&&line.length>=800;
                    });
                if(truth_line&&detected)++true_positive;
                else if(truth_line)++false_negative;
                else if(detected)++false_positive;
                else ++true_negative;
            }
        candidates.push_back({{"horizontal_line_gap_limit",gap_limit},
            {"true_positive",true_positive},{"false_negative",false_negative},
            {"false_positive",false_positive},{"true_negative",true_negative},
            {"passes_frozen_safety",false_negative==0&&false_positive==0}});
    }
    return {{"schema_version",1},{"analysis","c1_finite_line_gap_synthetic_truth"},
        {"plan","c1-line-gap-sweep-plan.json"},
        {"truth_basis","independent_synthetic_rgb"},
        {"candidate_count",3},{"scenes_per_candidate",15},
        {"candidates",candidates},{"runtime_value",4},
        {"real_rgb_truth","unknown; replay separately by source family"},
        {"no_device_access",true}};
}

nlohmann::json analyze_game_cold_c5_gate(const std::filesystem::path& directory) {
    using nlohmann::json;
    const auto read=[&](const std::filesystem::path& path) {
        if(!std::filesystem::is_regular_file(path)||
           std::filesystem::file_size(path)>1'048'576)
            throw std::invalid_argument("C5 evidence missing or oversized: "+path.string());
        std::ifstream file(path);return json::parse(file);
    };
    const auto tap_path=directory/"pipeline47"/"pipeline-ab-analysis.json";
    const auto dense_path=directory/"pipeline47-dense"/"pipeline-ab-analysis.json";
    const auto tap=read(tap_path),dense=read(dense_path);
    const bool tap_improvement=tap.at("status")=="passed"&&
        tap.at("failures").empty()&&tap.at("batches").size()==3;
    bool dense_nonregression=dense.at("failures").empty()&&
        dense.at("batches").size()==3;
    json dense_checks=json::array();
    for(const auto& batch:dense.at("batches")) {
        if(batch.at("attempts_per_mode")!=2000)dense_nonregression=false;
        for(const auto& check:batch.at("checks")) {
            const auto metric=check.at("metric").get<std::string>();
            const bool latency=metric=="capture_to_recognition_p95_ms"||
                metric=="capture_to_recognition_p99_ms"||
                metric=="capture_to_owner_p95_ms"||
                metric=="capture_to_owner_p99_ms";
            const bool pass=latency?
                check.at("candidate_mean").get<double>()-
                    check.at("baseline_mean").get<double>()<=
                    check.at("frozen_tolerance").get<double>():
                check.at("pass").get<bool>();
            if(!pass)dense_nonregression=false;
            dense_checks.push_back({{"batch",batch.at("batch")},
                {"metric",metric},{"nonregression_pass",pass}});
        }
    }
    const auto load_path=directory/"pipeline47-writer-rpc-1000.json";
    const auto load=read(load_path);
    const bool load_safe=load.at("requested_frames")==1000&&
        load.at("published")==1000&&load.at("writer_rejected").get<int>()>0&&
        load.at("writer_debug_drops").get<int>()>0&&
        load.at("writer_faulted")==false&&load.at("failed_receipts")==0&&
        load.at("unknown_receipts")==0&&load.at("contacts_released")==true&&
        load.at("down")==load.at("up");
    json long_runs=json::array();bool long_safe=true;
    for(const auto& scene:{"dense","tap"}) {
        const auto path=directory/(std::string("pipeline47-")+scene+"-long-10000.json");
        const auto r=read(path);
        const bool pass=r.at("scene")==scene&&r.at("requested_frames")==10000&&
            r.at("published")==10000&&r.at("contacts_released")==true&&
            r.at("mailbox").at("pool_drops")==0&&
            r.at("writer_debug_drops")==0&&r.at("writer_faulted")==false&&
            r.at("failed_receipts")==0&&r.at("unknown_receipts")==0&&
            r.at("down")==r.at("up")&&
            (std::string_view(scene)=="tap"?r.at("down").get<int>()>0:r.at("down")==0);
        long_safe&=pass;
        long_runs.push_back({{"scene",scene},{"pass",pass},
            {"requested_frames",r.at("requested_frames")},
            {"consumed",r.at("consumed")},
            {"skips",r.at("mailbox").at("consumer_skips")},
            {"wall_time_ms",r.at("wall_time_ms")},
            {"rss_peak_sample_mib",r.at("rss_sample_mib").at("max")},
            {"down",r.at("down")},{"up",r.at("up")},
            {"evidence_sha256",sha256_file(path)}});
    }
    const bool complete=tap_improvement&&dense_nonregression&&load_safe&&long_safe;
    return {{"schema_version",1},{"analysis","original_c5_global_cold_gate"},
        {"status",complete?"passed":"failed"},
        {"tap_improvement_all_three_batches",tap_improvement},
        {"dense_nonregression_all_three_batches",dense_nonregression},
        {"dense_stricter_every_batch_improvement",dense.at("status")},
        {"dense_checks",dense_checks},
        {"writer_and_fake_rpc_load_safe",load_safe},
        {"writer_debug_drops_under_intentional_load",load.at("writer_debug_drops")},
        {"long_runs",long_runs},{"tap_ab_sha256",sha256_file(tap_path)},
        {"dense_ab_sha256",sha256_file(dense_path)},
        {"limitations","Synthetic fake-memory RGB; dense scene has no executable touch; no measured render age, true RPC transport or game effect"}};
}

} // namespace pas
