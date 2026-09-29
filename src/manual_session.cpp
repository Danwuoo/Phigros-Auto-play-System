#include "pas/runtime.hpp"
#include "pas/game_session.hpp"
#include "pas/session_archive.hpp"
#include "pas/session_pixel_clips.hpp"
#include "pas/analysis.hpp"
#include "pas/preview.hpp"
#include "pas/strategy_version.hpp"
#include "session_build_provenance.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace pas {
namespace {
std::atomic<bool> manual_stop=false;
BOOL WINAPI stop_handler(DWORD e) {
    if(e==CTRL_C_EVENT||e==CTRL_BREAK_EVENT||e==CTRL_CLOSE_EVENT||e==CTRL_SHUTDOWN_EVENT) {
        manual_stop=true;return TRUE;
    }return FALSE;
}
class StopGuard {
public:
    StopGuard(){manual_stop=false;if(!SetConsoleCtrlHandler(stop_handler,TRUE))throw std::runtime_error("stop handler failed");}
    ~StopGuard(){SetConsoleCtrlHandler(stop_handler,FALSE);}
};
class Wake {
public:
    Wake() {
        event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);
        if(!event_||!timer_){if(event_)CloseHandle(event_);if(timer_)CloseHandle(timer_);throw std::runtime_error("action wake creation failed");}
    }
    ~Wake(){CancelWaitableTimer(timer_);CloseHandle(timer_);CloseHandle(event_);}
    void notify(){SetEvent(event_);}
    void wait(Nanoseconds delay) {
        if(delay<=0)return;
        LARGE_INTEGER due;due.QuadPart=-((delay+99)/100);
        if(!SetWaitableTimerEx(timer_,&due,0,nullptr,nullptr,nullptr,0))throw std::runtime_error("action timer failed");
        HANDLE handles[]{event_,timer_};const auto result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
        if(result!=WAIT_OBJECT_0&&result!=WAIT_OBJECT_0+1)throw std::runtime_error("action wait failed");
    }
private:HANDLE event_{},timer_{};
};
struct Packet : SessionObservation {
    std::shared_ptr<const Frame> result;
    std::optional<std::int64_t> source_time;
    std::optional<std::uint64_t> source_sequence;
};
using nlohmann::json;
json release_json(const ReleaseReport& r,const std::string& reason) {
    return {{"event","game_release_report"},{"reason",reason},{"requested_ids",r.requested_ids},
        {"failed_ids",r.failed_ids},{"unknown_ids",r.unknown_ids},{"start_ns",r.start_ns},{"return_ns",r.return_ns},
        {"effect_verified",false}};
}
}
void run_manual_session(const std::string& config_path,const std::string& capability_path,bool no_preview,Nanoseconds watchdog,bool pixel_clips) {
    const auto config=load_config(config_path);
    if(config.capture_kind!="emulator-grpc"||config.touch_kind!="emulator-grpc"||
       config.width!=1280||config.height!=720||config.source_rotation!=1||config.grpc_read_chunk_kib!=256||
       config.max_relative_lag_ms!=250||config.touch_width!=720||config.touch_height!=1280||config.touch_rotation!=90||
       config.touch_timeout_ms!=100||config.max_plans!=128||config.max_steps!=16||config.horizon_ms!=350||
       config.evidence_max_age_ms!=100||config.max_contacts!=5||config.game_type_mask!=15||
       config.game_lead_ms<30||config.game_lead_ms>45||config.game_uncertainty_ms!=30)
        throw std::invalid_argument("manual-session requires 1280x720 rotation1 / 256KiB / five contacts / all types / lead30..45 / uncertainty30 profile");
    if(watchdog<0||watchdog>3'600'000'000'000LL)throw std::invalid_argument("invalid round watchdog");
    StopGuard stop_guard;
    const auto preflight=game_preflight(config_path,capability_path);
    if(!preflight.at("fingerprint_matches").get<bool>())throw std::runtime_error("manual-session capability fingerprint mismatch");
    HostClock clock;
    wchar_t module[32768]{};const auto size=GetModuleFileNameW(nullptr,module,32768);
    if(!size||size==32768)throw std::runtime_error("runtime executable unavailable");
    const auto root=std::filesystem::path(config.log_dir)/("manual-session-"+std::to_string(clock.now_ns()));
    auto manifest=json{{"schema_version",1},{"mode","manual-session"},{"strategy",game_strategy_name()+" plus manual standby lifecycle"},
        {"baseline_source_commit","5ad759ef5004ab89be8f1a326e75fb96e7cb9a0e"},
        {"baseline_historical_binary_sha256","d7ce474576a0283711b046b82720f9c10e8de0bb92203eb999a6c8decc157162"},
        {"executable_sha256",sha256_file(module)},{"config_sha256",sha256_file(config_path)},
        {"source_build_commit",pas_session_build_commit},{"source_build_dirty",pas_session_build_dirty},
        {"compiled_source_sha256",json::parse(pas_session_source_hashes)},
        {"config",config.public_json},{"capability_preflight",preflight},{"clock_domain","host_qpc_ns"},{"qpc_frequency",clock.frequency()},
        {"game_observer_version",game_observer_version},{"game_planner_version",game_planner_version},
        {"effective_game_lead_ns",static_cast<Nanoseconds>(config.game_lead_ms)*1'000'000},
        {"effective_game_uncertainty_ns",static_cast<Nanoseconds>(config.game_uncertainty_ms)*1'000'000},
        {"game_diagnostics_version",game_diagnostics_version},{"lifecycle_version",1},
        {"automatic_play_enabled",false},{"round_watchdog_ns",watchdog},{"watchdog_is_result",false},
        {"standby_timeout",nullptr},{"source_absolute_age",nullptr},{"input_policy","manual_PLAY_only_gated_gameplay"},
        {"capture",grpc_transport_manifest(256)},{"capture_pool_slots",3},{"decision_slots",1},
        {"journal_queue_capacity",8192},{"journal_segment_bytes",16*1024*1024},{"round_max_segments",32},
        {"standby_segment_bytes",1024*1024},{"standby_segments",4},{"stats_samples_per_round_max",100000},
        {"dataset_sampling",false},{"pixel_clip_sampling",pixel_clips},
        {"pixel_clip_policy",pixel_clips?"at most 20 rounds x (8 uniform + 2 complex-line) x 3 full 1280x720 RGB888 frames; 4-frame writer mailbox; no runtime feedback":"disabled"},
        {"result_image_policy","one_same_capture_frame_per_confirmed_round_after_owner_release"},
        {"result_ui_profile","English six static labels / 1280x720 / translation <=3px"}};
    SessionArchive archive(root,manifest);
    std::unique_ptr<SessionPixelClips> clips;
    if(pixel_clips)clips=std::make_unique<SessionPixelClips>(root/"pixel-clips",clock,config.width,config.height);
    auto endpoint=discover_endpoint(config.serial);
    if(!config.endpoint.empty()) {
        std::ifstream f(config.token_file);std::getline(f,endpoint.token);
        if(!endpoint.token.empty()&&endpoint.token.back()=='\r')endpoint.token.pop_back();
        if(endpoint.token.empty())throw std::runtime_error("gRPC token unavailable");
        endpoint.target=config.endpoint;endpoint.instance="explicit";
    }
    GrpcCapture capture(clock,endpoint,runtime_capture_options(config));
    LatestFrame latest(config.width,config.height,3,&clock);
    Wake wake;
    std::atomic<bool> stopping=false,live_gate=false;
    std::atomic<std::uint64_t> round_epoch=1,revocation=0;
    std::atomic<Nanoseconds> last_capture=0;
    std::atomic<Nanoseconds> invalid_through=0;
    std::atomic<Nanoseconds> active_round_start=0;
    std::mutex packet_mutex,fault_mutex;
    std::shared_ptr<const Packet> packet;
    std::shared_ptr<const Frame> display;
    std::exception_ptr fault;
    const auto fail=[&] {
        {std::lock_guard lock(fault_mutex);if(!fault)fault=std::current_exception();}
        live_gate=false;stopping=true;++revocation;wake.notify();latest.close();capture.cancel();
    };
    std::jthread capture_worker([&](std::stop_token stop) {
        try {
            capture.stream(stop,[&](Frame&& f) {
                f.epoch=round_epoch.load();f.generation=1;f.geometry_version=1;
                if(f.width!=config.width||f.height!=config.height||f.source_rotation!=config.source_rotation)
                    throw std::runtime_error("capture geometry changed");
                last_capture=f.capture_complete_ns;latest.publish(f.rgb.data(),f.rgb.size(),f);
            });
            if(!stopping&&!stop.stop_requested())throw std::runtime_error("capture stream ended");
        } catch(...) {fail();}
    });
    std::jthread perception_worker([&](std::stop_token stop) {
        try {
            SessionPerception perception(clock,watchdog);
            std::uint64_t sequence=0;Nanoseconds preview_at=0;
            std::shared_ptr<const Frame> result_frame;
            while(!stopping&&!stop.stop_requested()) {
                auto f=latest.read_after(sequence,10'000'000);if(!f)continue;sequence=f->sequence;
                if(clock.now_ns()-f->capture_complete_ns>=100'000'000){live_gate=false;continue;}
                auto p=std::make_shared<Packet>();static_cast<SessionObservation&>(*p)=perception.process(*f);
                if(clips)clips->observe(*f,*p);
                if(p->status.new_round) {
                    round_epoch=p->status.round;result_frame.reset();
                }
                // This independent latch closes dispatch before the action owner consumes the packet.
                live_gate=p->allow_down&&f->capture_complete_ns>invalid_through.load();
                if(p->status.ended&&p->status.state==PlaySessionState::result&&!result_frame)
                    result_frame=std::make_shared<Frame>(*f); // One bounded copy; never a pool lease.
                p->result=result_frame;p->source_time=f->source_timestamp_us;p->source_sequence=f->source_sequence;
                const auto now=clock.now_ns();
                if(!no_preview&&config.preview_hz>0&&now-preview_at>=static_cast<Nanoseconds>(1e9/config.preview_hz)) {
                    auto view=std::make_shared<Frame>(*f);draw_game_overlay(*view,p->scene);
                    std::lock_guard lock(packet_mutex);display=std::move(view);preview_at=now;
                }
                {std::lock_guard lock(packet_mutex);packet=std::move(p);}
                wake.notify();
            }
        } catch(...) {fail();}
    });
    std::jthread action_worker([&](std::stop_token stop) {
        std::unique_ptr<GrpcTouch> backend;
        std::uint64_t active=0,completed=0,seen=0,rev=0,commands=0,frames=0;
        Nanoseconds round_start=0,previous=0,previous_playing=0;
        std::vector<double> intervals,playing_intervals,recognition,residency;
        PlaySessionState last_state=PlaySessionState::standby;
        try {
            backend=std::make_unique<GrpcTouch>(clock,endpoint,PixelCoordinateMap(config.width,config.height,config.touch_width,config.touch_height,config.touch_rotation),
                config.touch_width,config.touch_height,config.max_contacts,std::chrono::milliseconds(config.touch_timeout_ms));
            SessionGameOwner game(clock,*backend,config.max_contacts,
                {config.game_type_mask,static_cast<Nanoseconds>(config.game_lead_ms)*1'000'000,
                 static_cast<Nanoseconds>(config.game_uncertainty_ms)*1'000'000});
            const auto record=[&](json value){archive.event(active,std::move(value));};
            const auto receipts=[&](const std::vector<TouchReceipt>& rs) {
                for(const auto& r:rs) {
                    ++commands;record({{"event","game_touch_receipt"},{"intent_id",r.command.intent_id},{"contact_id",r.command.contact_id},
                        {"phase",static_cast<int>(r.command.phase)},{"scheduled_ns",r.command.scheduled_ns},
                        {"source_frame",r.command.source_frame_sequence},{"injection_start_ns",r.injection_start_ns},
                        {"injection_return_ns",r.injection_return_ns},{"success",r.success},{"reason",r.reason},{"real_input",true}});
                }
            };
            const auto finish=[&](const std::string& reason,bool result,std::shared_ptr<const Frame> image) {
                live_gate=false;const auto release=game.finish();record(release_json(release,reason));
                record({{"event","round_finished"},{"state",result?"RESULT":"ABORTED"},{"reason",reason},{"monotonic_ns",clock.now_ns()}});
                archive.complete(active,{{"status",result?"result_confirmed":"aborted"},{"stop_reason",reason},
                    {"round_start_ns",round_start},{"round_stop_ns",clock.now_ns()},{"commands",commands},{"decisions",frames},
                    {"capture_interval_ms",distribution(intervals)},{"playing_capture_interval_ms",distribution(playing_intervals)},
                    {"recognition_ms",distribution(recognition)},{"host_residency_ms",distribution(residency)},
                    {"interval_scope","latest decisions consumed by action owner; includes skipped frames; first 100000 samples"},
                    {"source_absolute_age",nullptr},{"release_requested_ids",release.requested_ids},
                    {"release_failed_ids",release.failed_ids},{"release_unknown_ids",release.unknown_ids}},std::move(image));
                completed=active;active=0;active_round_start=0;
            };
            archive.event(0,{{"event","session_state"},{"state","STANDBY"},{"reason","capability_preflight_passed"},{"monotonic_ns",clock.now_ns()}});
            std::cout<<"STANDBY: manually select any HD and press Play. Escape / Ctrl+C stops.\n"<<std::flush;
            while(!stopping&&!stop.stop_requested()) {
                std::shared_ptr<const Packet> p;{std::lock_guard lock(packet_mutex);p=packet;}
                if(rev!=revocation.load()) {
                    rev=revocation.load();if(active)record(release_json(game.suspend("source_or_supervisor_revoke"),"source_or_supervisor_revoke"));
                }
                if(p&&p->scene.context.frame>seen) {
                    seen=p->scene.context.frame;
                    if(p->status.state==PlaySessionState::fault)throw std::runtime_error(p->status.reason);
                    if(active&&p->status.round!=active)throw std::runtime_error("round transition missed by action owner");
                    if(!active&&p->status.active&&p->status.round>completed) {
                        active=p->status.round;game.start(active);round_start=p->scene.context.capture_ns;
                        active_round_start=round_start;
                        intervals.clear();playing_intervals.clear();recognition.clear();residency.clear();
                        previous=previous_playing=0;commands=frames=0;
                        game.scheduler()->set_dispatch_guard([&]{return !stopping&&!manual_stop&&live_gate&&round_epoch.load()==active&&
                            revocation.load()==rev&&last_capture.load()&&clock.now_ns()-last_capture.load()<100'000'000;});
                    }
                    if(p->status.state!=last_state) {
                        last_state=p->status.state;
                        record({{"event","session_state"},{"state",name(last_state)},{"reason",p->status.reason},
                            {"source_frame",seen},{"capture_complete_ns",p->scene.context.capture_ns},{"monotonic_ns",clock.now_ns()}});
                        std::cout<<name(last_state)<<" round="<<p->status.round<<" "<<p->status.reason<<'\n'<<std::flush;
                    }
                    if(active) {
                        const auto& s=p->scene;const auto t=s.context.capture_ns;++frames;
                        if(previous&&intervals.size()<100000)intervals.push_back((t-previous)/1e6);previous=t;
                        if(s.playing_gate&&p->status.state==PlaySessionState::playing) {
                            if(previous_playing&&playing_intervals.size()<100000)playing_intervals.push_back((t-previous_playing)/1e6);
                            previous_playing=t;
                        }
                        if(recognition.size()<100000)recognition.push_back((s.recognition_end_ns-s.recognition_start_ns)/1e6);
                        if(residency.size()<100000)residency.push_back((clock.now_ns()-t)/1e6);
                        record(decision_json(s));
                        record({{"event","lifecycle_ui_evidence"},{"source_frame",seen},{"capture_complete_ns",t},
                            {"hud",p->evidence.hud},{"result_labels",p->evidence.result_labels},{"result_similarity",p->evidence.result_similarity},
                            {"source_timestamp_us",p->source_time?json(*p->source_time):json(nullptr)},
                            {"source_sequence",p->source_sequence?json(*p->source_sequence):json(nullptr)}});
                        if(!p->status.active) {
                            finish(p->status.reason,static_cast<bool>(p->result),p->result);
                            // Result state is already released. A later fresh packet controls new Down.
                        } else {
                            const bool allow=p->allow_down&&s.context.capture_ns>invalid_through.load();
                            const bool was_armed=game.scheduler()->armed();
                            receipts(game.accept(s,allow));
                            if(was_armed&&!allow)record(release_json(game.scheduler()->last_release(),"current_UI_gate_lost"));
                            for(const auto& plan:game.owner()->take_accepted_plans()) {
                                json steps=json::array();for(const auto& st:plan.steps)steps.push_back({{"phase",static_cast<int>(st.phase)},{"x",st.x},{"y",st.y},{"due_ns",st.due_ns}});
                                record({{"event","game_plan_accepted"},{"epoch",plan.epoch},{"intent_id",plan.intent_id},{"note_id",plan.note_id},
                                    {"revision",plan.revision},{"prefix_offset",plan.prefix_offset},{"evidence_ns",plan.evidence_ns},
                                    {"valid_until_ns",plan.valid_until_ns},{"source_frame",plan.source_frame_sequence},{"accepted_ns",clock.now_ns()},
                                    {"predicted_down_ns",plan.predicted_down_ns?json(*plan.predicted_down_ns):json(nullptr)},
                                    {"steps",steps},{"basis",plan.basis},{"real_input",true}});
                            }
                            for(auto e:game.owner()->take_plan_cancellations())record(std::move(e));
                        }
                    }
                }
                receipts(game.poll());
                if(game.scheduler()) {
                    if(!game.scheduler()->fault().empty())throw std::runtime_error("gameplay input fault: "+game.scheduler()->fault());
                    for(auto e:game.owner()->take_coverage_updates())record(std::move(e));
                    for(const auto& n:game.scheduler()->take_notices())record({{"event","scheduler_rejection"},{"intent_id",n.intent_id},{"reason",n.reason},{"monotonic_ns",n.monotonic_ns}});
                }
                const auto due=game.scheduler()?game.scheduler()->next_due_ns():std::optional<Nanoseconds>{};
                wake.wait(due?std::clamp(*due-clock.now_ns(),Nanoseconds{0},Nanoseconds{10'000'000}):10'000'000);
            }
            if(active)finish(manual_stop?"user_stop_aborted":"runtime_stop_aborted",false,{});
            archive.event(0,{{"event","session_state"},{"state","STOPPED"},{"monotonic_ns",clock.now_ns()}});
        } catch(...) {
            fail();
            if(backend)try {
                const auto r=backend->emergency_release_all();archive.event(active,release_json(r,"fault_emergency_release"));
                if(active)archive.complete(active,{{"status","aborted"},{"stop_reason","fault"},{"round_start_ns",round_start},
                    {"round_stop_ns",clock.now_ns()},{"release_unknown_ids",r.unknown_ids},{"release_failed_ids",r.failed_ids}});
            }catch(...){}
        }
    });
    std::unique_ptr<PreviewWindow> preview;
    try {
        if(!no_preview&&config.preview_hz>0)preview=std::make_unique<PreviewWindow>(config.width,config.height);
        bool stale=false;std::uint64_t lag=0,inactive=0;Nanoseconds heartbeat=clock.now_ns();
        while(!stopping&&!manual_stop) {
            if(GetAsyncKeyState(VK_ESCAPE)&0x8000){manual_stop=true;break;}
            if(archive.faulted())throw std::runtime_error(archive.error());
            const auto now=clock.now_ns(),last=last_capture.load();const auto stats=capture.stats();
            const bool expired=last&&now-last>=100'000'000;
            if((expired&&!stale)||stats.relative_stale_drops!=lag||stats.inactive!=inactive) {
                invalid_through=last;live_gate=false;++revocation;wake.notify();lag=stats.relative_stale_drops;inactive=stats.inactive;
            }
            stale=expired;
            const auto active_start=active_round_start.load();
            if(watchdog&&active_start&&now-active_start>=watchdog)
                throw std::runtime_error("round_watchdog_aborted");
            if(now-heartbeat>=60'000'000'000LL) {
                archive.event(0,{{"event","session_health"},{"monotonic_ns",now},{"published",latest.counters().published},{"capture_stale",stale}});heartbeat=now;
            }
            if(preview) {
                if(!preview->pump()){manual_stop=true;break;}
                std::shared_ptr<const Frame> view;std::shared_ptr<const Packet> p;
                {std::lock_guard lock(packet_mutex);view=display;p=packet;}
                if(view){preview->draw(*view);preview->title(std::string("PAS manual / ")+(p?name(p->status.state):"STANDBY")+
                    (p&&p->status.active&&!live_gate?" / touch paused":""));}
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }catch(...){fail();}
    live_gate=false;stopping=true;++revocation;wake.notify();latest.close();capture.cancel();
    action_worker.request_stop();perception_worker.request_stop();capture_worker.request_stop();
    action_worker.join();perception_worker.join();capture_worker.join();archive.close();
    if(clips)clips->close();
    std::exception_ptr error;{std::lock_guard lock(fault_mutex);error=fault;}
    json summary={{"state",error||archive.faulted()?"FAULT":"STOPPED"},{"run_dir",std::filesystem::absolute(root).string()},
        {"published",latest.counters().published},{"pool_drops",latest.counters().pool_drops},
        {"consumer_skips",latest.counters().consumer_skips},{"journal_peak_queue",archive.peak_queue()},
        {"automatic_play_enabled",false},{"pixel_clips",clips?clips->summary():json{{"enabled",false}}}};
    if(error)try{std::rethrow_exception(error);}catch(const std::exception& e){summary["fault"]=e.what();}
    std::ofstream file(root/"summary.json");file<<summary.dump(2)<<'\n';file.close();
    if(!file)throw std::runtime_error("session summary write failed");
    std::cout<<summary.dump(2)<<'\n';if(error)std::rethrow_exception(error);
    if(archive.faulted())throw std::runtime_error(archive.error());
}
}
