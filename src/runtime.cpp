#include "pas/runtime.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include "pas/game.hpp"
#include "pas/game_dataset.hpp"
#include "pas/game_tracking_shadow.hpp"
#include "pas/journal.hpp"
#include "pas/preview.hpp"
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace pas {
GameRunBudget::GameRunBudget(Nanoseconds start,Nanoseconds duration,Nanoseconds wait_play)
    :start_(start),duration_(duration),wait_play_(wait_play),deadline_(0) {
    if(start<0||duration<=0||duration>3'600'000'000'000LL||wait_play<0||wait_play>60'000'000'000LL||
       start>std::numeric_limits<Nanoseconds>::max()-duration-wait_play)
        throw std::invalid_argument("invalid game run budget");
    deadline_=start+(wait_play?wait_play:duration);
    if(!wait_play)origin_=start;
}
bool GameRunBudget::arm_from_playing(Nanoseconds observed_ns) {
    if(!waiting()||observed_ns<start_||observed_ns>=start_+wait_play_)return false;
    origin_=observed_ns;deadline_=observed_ns+duration_;return true;
}
using nlohmann::json;
CaptureOptions runtime_capture_options(const RuntimeConfig& c) {
    CaptureOptions options;
    options.width=c.width; options.height=c.height; options.source_rotation=c.source_rotation;
    options.grpc_read_chunk_kib=c.grpc_read_chunk_kib;
    options.max_relative_lag_ns=static_cast<Nanoseconds>(c.max_relative_lag_ms)*1'000'000;
    return options;
}
nlohmann::json match_touch_capability(const RuntimeConfig& c,const json& report,
                                     const json& device,const std::string& installed_hash) {
    json mismatches=json::array();
    const auto test=[&](bool valid,const char* reason) {if(!valid) mismatches.push_back(reason);};
    test(report.value("schema_version",0)==2,"report_schema");
    test(report.value("fixture_schema","")=="native_touch_v2","fixture_schema");
    test(report.value("capability_verified",false),"historical_capability_not_verified");
    test(report.value("serial","")==c.serial,"serial");
    test(!installed_hash.empty()&&report.value("installed_apk_sha256","")==installed_hash&&
         report.value("fixture_apk_sha256","")==installed_hash,"fixture_apk_hash");
    const auto old=report.value("config",json::object());
    const auto capture=old.value("capture",json::object()),touch=old.value("touch",json::object());
    test(capture.value("kind","")=="emulator-grpc"&&capture.value("transport","")=="payload"&&
         capture.value("image_format","")=="rgb888"&&capture.value("row_order","")=="top-down","capture_format");
    test(capture.value("width",0)==c.width&&capture.value("height",0)==c.height&&
         capture.value("source_rotation",-1)==c.source_rotation,"capture_geometry");
    test(touch.value("kind","")=="emulator-grpc"&&touch.value("width",0)==c.touch_width&&
         touch.value("height",0)==c.touch_height&&touch.value("rotation_deg",-1)==c.touch_rotation&&
         touch.value("max_contacts",0)>=c.max_contacts&&
         report.value("max_contacts_verified",std::min(2,touch.value("max_contacts",0)))>=c.max_contacts,
         "touch_mapping_or_capacity");
    const auto historical=report.value("device_report",json::object());
    for(const auto* key:{"android_release","android_sdk","cpu_abi","model","wm_size","wm_density"}) {
        if(!historical.contains(key)||!device.contains(key)||historical.at(key)!=device.at(key))
            mismatches.push_back(std::string("device_")+key);
    }
    return {{"fingerprint_matches",mismatches.empty()},{"mismatches",mismatches},
            {"historical_capability_only",true},{"gameplay_enabled",false}};
}
json game_preflight(const std::string& config_path,const std::string& capability_path) {
    const auto c=load_config(config_path);
    if(c.capture_kind!="emulator-grpc") throw std::invalid_argument("preflight requires emulator-grpc");
    const auto adb=find_adb(); const auto device=probe_adb(adb,c.serial);
    std::ifstream file(capability_path); if(!file) throw std::runtime_error("cannot read capability report");
    const auto report=json::parse(file);
    const auto installed=installed_apk_sha256(adb,c.serial,"org.pas.touchfixture.cpp");
    const auto bytes=[&](const std::vector<std::string>& args) {
        const auto v=adb_call(adb,args); return std::string(v.begin(),v.end());
    };
    HostClock clock; GrpcCapture capture(clock,discover_endpoint(c.serial),runtime_capture_options(c));
    const auto frame=capture.snapshot(std::chrono::milliseconds(1500));
    PixelCoordinateMap map(c.width,c.height,c.touch_width,c.touch_height,c.touch_rotation);
    json points=json::array();
    for(const auto p:{std::array<double,2>{0,0},{static_cast<double>(c.width-1),0},
                     {0,static_cast<double>(c.height-1)},
                     {static_cast<double>(c.width-1),static_cast<double>(c.height-1)}}) {
        const auto q=map.map(p[0],p[1]); points.push_back({{"frame",p},{"touch",q}});
    }
    auto result=match_touch_capability(c,report,device,installed);
    result["config"]=c.public_json; result["device"]=device;
    result["capability_report_sha256"]=sha256_file(capability_path);
    result["current_fixture_apk_sha256"]=installed;
    result["guest_processors"]=bytes({"-s",c.serial,"shell","getconf","_NPROCESSORS_ONLN"});
    result["guest_memory"]=bytes({"-s",c.serial,"shell","cat","/proc/meminfo"});
    result["capture_geometry"]={{"width",frame.width},{"height",frame.height},{"source_rotation",frame.source_rotation}};
    result["corner_mapping"]=points; result["real_input_created"]=false;
    result["source_absolute_age"]=nullptr; result["clock_domain"]="host_qpc_ns";
    return result;
}
namespace {
std::atomic<bool> console_stop_requested=false;
BOOL WINAPI runtime_console_handler(DWORD event) {
    if(event==CTRL_C_EVENT||event==CTRL_BREAK_EVENT||event==CTRL_CLOSE_EVENT||event==CTRL_SHUTDOWN_EVENT) {
        console_stop_requested=true; return TRUE;
    }
    return FALSE;
}
class ConsoleStopGuard {
public:
    ConsoleStopGuard() {console_stop_requested=false;
        if(!SetConsoleCtrlHandler(runtime_console_handler,TRUE)) throw std::runtime_error("cannot register runtime stop handler");}
    ~ConsoleStopGuard() {SetConsoleCtrlHandler(runtime_console_handler,FALSE);}
};
// One action owner waits; latest-only producers coalesce wakeups into one event.
// Relative timer durations are derived from QPC deadlines, never UTC deadlines.
class ActionWake {
public:
    ActionWake() {
        event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!event_) throw std::runtime_error("cannot create action wake event");
        timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                     TIMER_MODIFY_STATE|SYNCHRONIZE);
        if(!timer_) {CloseHandle(event_); throw std::runtime_error("cannot create high resolution action timer");}
    }
    ~ActionWake() {CancelWaitableTimer(timer_); CloseHandle(timer_); CloseHandle(event_);}
    ActionWake(const ActionWake&)=delete;
    ActionWake& operator=(const ActionWake&)=delete;
    void notify_all() noexcept {SetEvent(event_);}
    void wait_for(Nanoseconds delay) {
        if(delay<=0) return;
        LARGE_INTEGER due{}; due.QuadPart=-((delay+99)/100);
        if(!SetWaitableTimerEx(timer_,&due,0,nullptr,nullptr,nullptr,0))
            throw std::runtime_error("cannot arm action timer");
        const HANDLE handles[]{event_,timer_};
        const auto result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
        if(result!=WAIT_OBJECT_0&&result!=WAIT_OBJECT_0+1)
            throw std::runtime_error("action wait failed");
    }
private:
    HANDLE event_{},timer_{};
};
// Dry execution holds no unbounded receipt history and has no transport.
class DryTouch final : public TouchBackend {
public:
    explicit DryTouch(const Clock& clock):clock_(clock) {}
    TouchReceipt inject(const TouchCommand& command) override {
        const auto start=clock_.now_ns();
        if(command.phase==Phase::up) contacts_.erase(command.contact_id);
        else contacts_[command.contact_id]={command.x,command.y};
        return {command,start,clock_.now_ns(),true,"dry_run_no_transport"};
    }
    ReleaseReport release_all() override {
        ReleaseReport report; report.start_ns=clock_.now_ns();
        for(const auto& [id,_]:contacts_) report.requested_ids.push_back(id);
        contacts_.clear(); report.return_ns=clock_.now_ns(); return report;
    }
private:
    const Clock& clock_;
    std::map<int,std::array<double,2>> contacts_;
};
}
static void run_runtime(const std::string& config_path,double duration_s,bool no_preview,
                 const std::string& launch_package,double stale_ms,bool auto_play,
                 const std::string& capability_path,bool assist=false,bool keep_diagnostic_anomalies=false,
                 bool keep_vision_dataset=false,const std::string& tracking_shadow="",double wait_play_s=0) {
    if(!std::isfinite(duration_s)||duration_s<=0||duration_s>3600 ||
       !std::isfinite(stale_ms)||stale_ms<5||stale_ms>5000)
        throw std::invalid_argument("invalid duration or stale-ms");
    if(!std::isfinite(wait_play_s)||wait_play_s<0||wait_play_s>60||
       (wait_play_s>0&&(!assist||auto_play)))throw std::invalid_argument("invalid manual PLAY wait");
    const auto config=load_config(config_path);
    if(!tracking_shadow.empty()&&tracking_shadow!="byte_association"&&tracking_shadow!="oc_observation")
        throw std::invalid_argument("tracking shadow requires byte_association or oc_observation");
    ConsoleStopGuard console_stop_guard;
    json capability=nullptr;
    if(auto_play||assist) {
        if(config.touch_kind!="emulator-grpc") throw std::invalid_argument("real input requires explicit touch profile");
        capability=game_preflight(config_path,capability_path);
        if(!capability.at("fingerprint_matches").get<bool>())
            throw std::runtime_error("real input touch fingerprint mismatch: "+capability.at("mismatches").dump());
    }
    HostClock clock;
    LatestFrame latest(config.width,config.height,3,&clock);
    const auto id=std::chrono::system_clock::now().time_since_epoch().count();
    const auto run_dir=std::filesystem::path(config.log_dir)/("cpp-observe-"+std::to_string(id));
    std::filesystem::create_directories(run_dir);
    const auto options=runtime_capture_options(config);
    wchar_t executable[32768]{};
    const auto executable_size=GetModuleFileNameW(nullptr,executable,32768);
    if(!executable_size||executable_size==32768) throw std::runtime_error("cannot identify runtime executable");
    const json provenance={{"binary_sha256",sha256_file(executable)},{"config_sha256",sha256_file(config_path)},
        {"capture_source","same consumed live frame"},{"clock_domain","host_qpc_ns"},{"qpc_frequency",clock.frequency()}};
    std::unique_ptr<GamePixelSampler> sampler;
    if(keep_diagnostic_anomalies||keep_vision_dataset)sampler=std::make_unique<GamePixelSampler>(config.width,config.height,DatasetSamplingOptions{keep_diagnostic_anomalies,keep_vision_dataset});
    std::unique_ptr<TrackingShadow> shadow;
    if(!tracking_shadow.empty())shadow=std::make_unique<TrackingShadow>(tracking_shadow);
    std::unique_ptr<TrackingBankRecorder> tracking_bank;
    if(keep_vision_dataset||shadow)tracking_bank=std::make_unique<TrackingBankRecorder>();
    const auto added_memory_upper=(sampler?sampler->allocated_raw_bytes():0)+(tracking_bank?16ULL*1024*1024:0)+(shadow?2ULL*1024*1024:0)+6ULL*1024*1024;
    if(added_memory_upper>128ULL*1024*1024)throw std::invalid_argument("combined sampling/shadow/bank 128MiB budget");
    {
        std::ofstream file(run_dir/"manifest.json");
        file<<json{{"schema_version",3},{"mode",assist?"assist":auto_play?"auto-start":"observe"},{"config",config.public_json},
            {"clock_domain","host_qpc_ns"},{"qpc_frequency",clock.frequency()},
            {"input_created",false},{"input_policy",assist?(auto_play?"pixels_PLAY_and_gated_gameplay":"manual_PLAY_and_gated_gameplay"):auto_play?"one_pixels_confirmed_PLAY_only":"none"},
            {"automatic_play_enabled",auto_play},
            {"duration_s",duration_s},{"wait_play_s",wait_play_s},
            {"duration_origin",wait_play_s>0?"first_pixels_confirmed_playing":"session_start"},
            {"dry_owner",!assist},{"game_observer_version",34},{"game_diagnostics_version",6},
            {"tracking_method","legacy"},{"tracking_shadow",tracking_shadow.empty()?json(nullptr):json(tracking_shadow)},
            {"vision_dataset_opt_in",keep_vision_dataset},{"added_memory_upper_bytes",added_memory_upper},
            {"executable_sha256",sha256_file(executable)},
            {"game_planner_version",16},{"hold_release_limit_ms",100},{"hold_missing_grace_ms",60},
            {"hold_normal_release_basis","consecutive_current_tail_passed_10ms_then_20ms_release"},
            {"drag_coverage_position_basis","current_region_deadzone_and_same_line_contact_move"},
            {"drag_planned_contact_ms",90},{"late_crossing_recovery_limit_ms",40},
            {"drag_shared_contact","fresh_colocated_drag_extends_existing_active_contact_only"},
            {"game_enabled_types_mask",config.game_type_mask},{"game_lead_ms",config.game_lead_ms},
            {"game_uncertainty_ms",config.game_uncertainty_ms},
            {"action_wait","win32_high_resolution_relative_timer_and_event"},
            {"capability_preflight",capability},
            {"source_absolute_age",nullptr},{"diagnostic_image_retention",keep_diagnostic_anomalies?
                "at_most_two_events_four_native_ROI_frames_each_encoded_after_input_stop":"none"},
            {"config_sha256",sha256_file(config_path)},
            {"grpc_transport",config.capture_kind=="emulator-grpc"?
                grpc_transport_manifest(options.grpc_read_chunk_kib):json(nullptr)},
            {"max_relative_lag_ns",*options.max_relative_lag_ns},
            {"target_evidence_ms",100},{"playing_confirmation_frames",3},
            {"prediction_horizon_ms",350},{"ui_classes_validated",json::array()},
            {"touch_mapping",{{"width",config.touch_width},{"height",config.touch_height},
                              {"rotation_deg",config.touch_rotation},{"capability_verified",auto_play||assist}}},
            {"preview_requested",!no_preview&&config.preview_hz>0},{"stale_ms",stale_ms}}.dump(2)<<'\n';
    }
    Journal log(run_dir/"events.jsonl");
    const auto record=[&](json value) {
        if(!log.push(std::move(value))) throw std::runtime_error("runtime journal fault or overrun");
    };
    std::unique_ptr<GrpcCapture> capture;
    std::optional<GrpcEndpoint> input_endpoint;
    if(config.capture_kind=="emulator-grpc") {
        GrpcEndpoint endpoint;
        if(config.endpoint.empty()) endpoint=discover_endpoint(config.serial);
        else {
            std::ifstream file(config.token_file); std::getline(file,endpoint.token);
            if(!endpoint.token.empty()&&endpoint.token.back()=='\r') endpoint.token.pop_back();
            if(endpoint.token.empty()) throw std::runtime_error("cannot read gRPC token");
            endpoint.target=config.endpoint; endpoint.instance="explicit";
        }
        if(auto_play||assist) input_endpoint=endpoint;
        capture=std::make_unique<GrpcCapture>(clock,std::move(endpoint),options);
    }
    std::unique_ptr<PreviewWindow> preview;
    if(!no_preview&&config.preview_hz>0) preview=std::make_unique<PreviewWindow>(config.width,config.height);
    std::atomic<bool> stopping=false, capture_done=false;
    std::atomic<std::uint64_t> epoch=1, revoke=0, consumed=0, dry_commands=0;
    std::atomic<std::uint64_t> gameplay_commands=0;
    std::atomic<Nanoseconds> last_capture=0;
    std::atomic<bool> real_input_created=false, play_requested=false, playing_seen=false;
    std::atomic<Nanoseconds> first_playing_ns=0;
    std::mutex fault_mutex, decision_mutex;
    ActionWake action_wakeup;
    std::exception_ptr worker_fault;
    std::shared_ptr<const DecisionSnapshot> decision;
    std::shared_ptr<const Frame> preview_frame;
    std::shared_ptr<const DecisionSnapshot> preview_scene;
    std::vector<double> intervals,residency,recognition,sampling_copy,shadow_publication,bank_copy;
    sampling_copy.reserve(100000);shadow_publication.reserve(100000);bank_copy.reserve(100000);
    std::string sampling_fault;
    const auto failure=[&] {
        {std::lock_guard lock(fault_mutex); if(!worker_fault) worker_fault=std::current_exception();}
        stopping=true; ++revoke; action_wakeup.notify_all(); latest.close();
        if(capture) capture->cancel();
    };
    const auto invalidate=[&](const std::string& reason) {
        ++epoch; ++revoke; action_wakeup.notify_all();
        record({{"event","runtime_revoke"},{"reason",reason},{"epoch",epoch.load()},
                {"requested_ns",clock.now_ns()}});
    };
    std::jthread capture_worker([&](std::stop_token stop) {
        try {
            if(capture) {
                capture->stream(stop,[&](Frame&& f) {
                f.epoch=epoch.load(); f.generation=1; f.geometry_version=1;
                last_capture=f.capture_complete_ns;
                latest.publish(f.rgb.data(),f.rgb.size(),f);
                });
                if(!stop.stop_requested()&&!stopping)
                    throw std::runtime_error("capture stream ended before runtime stop");
            }
            else {
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(config.width)*config.height*3);
                std::uint64_t sequence=0;
                while(!stop.stop_requested()&&!stopping) {
                    Frame f; f.sequence=++sequence; f.epoch=epoch.load(); f.generation=1; f.geometry_version=1;
                    f.width=config.width; f.height=config.height; f.stride=config.width*3;
                    f.source_rotation=config.source_rotation; f.capture_complete_ns=clock.now_ns();
                    f.pixels_ready_ns=f.capture_complete_ns; last_capture=f.capture_complete_ns;
                    pixels[sequence%pixels.size()]=static_cast<std::uint8_t>(sequence&255);
                    latest.publish(pixels.data(),pixels.size(),f);
                    std::this_thread::sleep_for(std::chrono::milliseconds(16));
                }
            }
        } catch(...) {failure();}
        capture_done=true; latest.close(); action_wakeup.notify_all();
    });
    std::jthread perception_worker([&](std::stop_token stop) {
        try {
            GameObserver observer(clock);
            ComboVisibilityDiagnostic combo_diagnostic;
            HoldVisibilityDiagnostic hold_diagnostic;
            std::uint64_t seq=0; Nanoseconds previous=0, preview_time=0; bool was_playing=false;
            while(!stop.stop_requested()&&!stopping) {
                auto f=latest.read_after(seq,10'000'000);
                if(!f) {if(capture_done) break; continue;}
                seq=f->sequence;
                if(f->epoch!=epoch.load() || clock.now_ns()-f->capture_complete_ns>=100'000'000) continue;
                auto s=std::make_shared<DecisionSnapshot>(observer.process(*f));
                const auto processed=clock.now_ns();
                if(previous&&intervals.size()<100'000) intervals.push_back((f->capture_complete_ns-previous)/1e6);
                previous=f->capture_complete_ns;
                if(residency.size()<100'000) residency.push_back((processed-f->capture_complete_ns)/1e6);
                if(recognition.size()<100'000) recognition.push_back((s->recognition_end_ns-s->recognition_start_ns)/1e6);
                ++consumed;
                record(decision_json(*s));
                if(was_playing&&!s->playing_gate) invalidate("UI_gate_lost");
                was_playing=s->playing_gate;
                record({{"event","frame_consumed"},{"frame_sequence",seq},
                    {"capture_complete_ns",f->capture_complete_ns},{"pixels_ready_ns",f->pixels_ready_ns},
                    {"published_ns",f->published_ns},{"consume_ns",processed},
                    {"width",f->width},{"height",f->height},{"source_rotation",f->source_rotation},
                    {"epoch",f->epoch},{"generation",f->generation},{"geometry_version",f->geometry_version},
                    {"source_timestamp_us",f->source_timestamp_us?json(*f->source_timestamp_us):json(nullptr)}});
                std::shared_ptr<Frame> display;
                if(!no_preview&&config.preview_hz>0&&processed-preview_time>=
                    static_cast<Nanoseconds>(std::llround(1e9/config.preview_hz))) {
                    display=std::make_shared<Frame>(*f);
                    draw_game_overlay(*display,*s); preview_time=processed;
                }
                if(display) {std::lock_guard lock(decision_mutex); preview_frame=std::move(display); preview_scene=s;}
                {std::lock_guard lock(decision_mutex); decision=s;}
                action_wakeup.notify_all();
                // Observation only, after decision publication. The first
                // visible combo disappearance is not attributed to a Note.
                const bool diagnostic_enabled=keep_diagnostic_anomalies||keep_vision_dataset;
                const bool combo_missing=diagnostic_enabled&&combo_diagnostic.observe(*s);
                const auto hold_missing=diagnostic_enabled?hold_diagnostic.observe(*s):std::optional<HoldDisappearance>{};
                if(combo_missing) {
                    record({{"event","diagnostic_combo_disappearance"},{"source_frame",s->context.frame},
                        {"capture_complete_ns",s->context.capture_ns},{"per_note_feedback","unknown"},
                        {"diagnostic_used_for_input",false}});
                }
                if(const auto missing=hold_missing) {
                    record({{"event","diagnostic_hold_disappearance"},{"source_frame",s->context.frame},
                        {"capture_complete_ns",s->context.capture_ns},{"prior_source_frame",missing->source_frame},
                        {"prior_note_id",missing->note_id},{"prior_capture_ns",missing->capture_ns},
                        {"prior_head_x",missing->head.x},{"prior_head_y",missing->head.y},
                        {"prior_width",missing->width},{"prior_height",missing->height},
                        {"per_note_feedback","unknown"},{"diagnostic_used_for_input",false}});
                }
                if(sampler){const auto start=clock.now_ns();try{sampler->consume(*f,*s,hold_missing,combo_missing);}catch(const std::exception& error){sampling_fault=error.what();sampler.reset();record({{"event","sampling_disabled"},{"reason",sampling_fault},{"diagnostic_raw_lost",true}});}
                    if(sampling_copy.size()<100000)sampling_copy.push_back((clock.now_ns()-start)/1e6);}
                if(shadow){const auto start=clock.now_ns();shadow->submit(observer.candidate_batch());if(shadow_publication.size()<100000)shadow_publication.push_back((clock.now_ns()-start)/1e6);}
                if(tracking_bank){const auto start=clock.now_ns();tracking_bank->append(observer.candidate_batch());if(bank_copy.size()<100000)bank_copy.push_back((clock.now_ns()-start)/1e6);}
                f.reset(); // Diagnostic copies never hold capture-pool leases.
            }
        } catch(...) {failure();}
    });
    std::jthread action_worker([&](std::stop_token stop) {
        std::unique_ptr<GrpcTouch> start_input;
        std::unique_ptr<ContactScheduler> start_scheduler;
        try {
            DryTouch backend(clock);
            if(assist) {
                start_input=std::make_unique<GrpcTouch>(clock,*input_endpoint,
                    PixelCoordinateMap(config.width,config.height,config.touch_width,config.touch_height,config.touch_rotation),
                    config.touch_width,config.touch_height,config.max_contacts,std::chrono::milliseconds(config.touch_timeout_ms));
                real_input_created=true;
            }
            TouchBackend& game_backend=assist?static_cast<TouchBackend&>(*start_input):static_cast<TouchBackend&>(backend);
            GamePlanOwner owner(clock,game_backend,config.max_contacts,
                {config.game_type_mask,config.game_lead_ms*Nanoseconds{1'000'000},config.game_uncertainty_ms*Nanoseconds{1'000'000}});
            PlayButtonPlanner play_planner;
            std::uint64_t seen=0, rev=0;
            std::uint64_t dispatch_epoch=0;
            owner.scheduler().set_dispatch_guard([&] {
                const auto last=last_capture.load();
                return !stopping && epoch.load()==dispatch_epoch && revoke.load()==rev &&
                    last && clock.now_ns()-last<100'000'000;
            });
            const auto receipts=[&](const std::vector<TouchReceipt>& events) {
                for(const auto& r:events) {
                    if(assist) ++gameplay_commands; else ++dry_commands;
                    record({{"event",assist?"game_touch_receipt":"dry_touch_receipt"},{"intent_id",r.command.intent_id},
                        {"contact_id",r.command.contact_id},{"phase",static_cast<int>(r.command.phase)},
                        {"scheduled_ns",r.command.scheduled_ns},{"injection_start_ns",r.injection_start_ns},
                        {"source_frame",r.command.source_frame_sequence},
                        {"injection_return_ns",r.injection_return_ns},{"real_input",assist},
                        {"success",r.success},{"reason",r.reason}});
                }
            };
            while(!stop.stop_requested()&&!stopping) {
                std::shared_ptr<const DecisionSnapshot> current;
                {std::lock_guard lock(decision_mutex); current=decision;}
                if(rev!=revoke.load()) {
                    rev=revoke.load(); owner.scheduler().cancel("supervisor_revoke");
                    if(start_scheduler) start_scheduler->cancel("supervisor_revoke");
                    record({{"event","owner_revoked"},{"revision",rev},{"ack_ns",clock.now_ns()},
                            {"real_input",assist}});
                }
                if(current&&current->sequence>seen) {
                    seen=current->sequence;
                    if(current->context.epoch==epoch.load()) {
                        dispatch_epoch=current->context.epoch;
                        if(!assist||!start_scheduler||start_scheduler->pending_count()==0)
                            receipts(owner.accept(*current));
                        for(auto coverage:owner.take_coverage_updates()) {coverage["real_input"]=assist;record(std::move(coverage));}
                        for(auto cancellation:owner.take_plan_cancellations()) {cancellation["real_input"]=assist;record(std::move(cancellation));}
                        for(const auto& plan:owner.take_accepted_plans()) {
                            json steps=json::array(); for(const auto& step:plan.steps)
                                steps.push_back({{"phase",static_cast<int>(step.phase)},{"x",step.x},{"y",step.y},{"due_ns",step.due_ns}});
                            json predicted_down=nullptr;
                            if(plan.predicted_down_ns) predicted_down=*plan.predicted_down_ns;
                            record({{"event","game_plan_accepted"},{"intent_id",plan.intent_id},{"note_id",plan.note_id},
                                {"epoch",plan.epoch},{"generation",plan.generation},{"geometry_version",plan.geometry_version},
                                {"revision",plan.revision},{"prefix_offset",plan.prefix_offset},{"evidence_ns",plan.evidence_ns},
                                {"accepted_ns",clock.now_ns()},{"predicted_down_ns",predicted_down},
                                {"valid_until_ns",plan.valid_until_ns},{"source_frame",plan.source_frame_sequence},
                                {"steps",steps},{"basis",plan.basis},{"real_input",assist}});
                        }
                        if(current->playing_gate) {
                            playing_seen=true;
                            Nanoseconds unarmed=0;
                            first_playing_ns.compare_exchange_strong(unarmed,clock.now_ns());
                        }
                        const auto play_plan=auto_play?play_planner.take(*current,clock.now_ns()):std::nullopt;
                        if(play_plan) {
                            play_requested=true; // One attempt. Unknown outcomes never retry down.
                            if(!start_input) start_input=std::make_unique<GrpcTouch>(clock,*input_endpoint,
                                PixelCoordinateMap(config.width,config.height,config.touch_width,
                                    config.touch_height,config.touch_rotation),
                                config.touch_width,config.touch_height,config.max_contacts,
                                std::chrono::milliseconds(config.touch_timeout_ms));
                            real_input_created=true;
                            start_scheduler=std::make_unique<ContactScheduler>(clock,*start_input,
                                config.max_contacts,1,2,100'000'000,100'000'000);
                            start_scheduler->set_dispatch_guard([&] {
                                return !stopping && epoch.load()==dispatch_epoch && revoke.load()==rev &&
                                    clock.now_ns()-last_capture.load()<100'000'000;
                            });
                            start_scheduler->set_context(current->context.epoch,current->context.generation,
                                current->context.geometry,true,current->context.capture_ns);
                            const auto p=*current->play_button; const auto due=play_plan->steps.front().due_ns;
                            if(!start_scheduler->submit(*play_plan))
                                throw std::runtime_error("pixels PLAY plan rejected");
                            record({{"event","auto_play_requested"},{"source_frame",current->context.frame},
                                {"epoch",current->context.epoch},{"x",p.x},{"y",p.y},{"monotonic_ns",due}});
                        }
                        if(start_scheduler && current->ui!=GameUi::menu) {
                            start_scheduler->cancel("PLAY_page_left");
                            const auto release=start_scheduler->last_release();
                            record({{"event","UI_play_release_report"},{"requested_ids",release.requested_ids},
                                {"failed_ids",release.failed_ids},{"unknown_ids",release.unknown_ids},
                                {"start_ns",release.start_ns},{"return_ns",release.return_ns},{"effect_verified",false}});
                            if(!release.failed_ids.empty()||!release.unknown_ids.empty())
                                throw std::runtime_error("UI PLAY release unknown");
                            start_scheduler.reset(); // Never let UI cancellation release gameplay contacts later.
                        } else if(start_scheduler)
                            start_scheduler->set_context(current->context.epoch,current->context.generation,
                                current->context.geometry,true,current->context.capture_ns);
                    }
                }
                receipts(owner.poll());
                if(assist&&!owner.scheduler().fault().empty())
                    throw std::runtime_error("gameplay input fault: "+owner.scheduler().fault());
                if(start_scheduler) {
                    for(const auto& r:start_scheduler->run_due())
                        record({{"event","UI_play_touch_receipt"},{"source_frame",r.command.source_frame_sequence},
                            {"phase",static_cast<int>(r.command.phase)},{"contact_id",r.command.contact_id},
                            {"scheduled_ns",r.command.scheduled_ns},{"injection_start_ns",r.injection_start_ns},
                            {"injection_return_ns",r.injection_return_ns},{"success",r.success},{"reason",r.reason}});
                    if(!start_scheduler->fault().empty())
                        throw std::runtime_error("auto-start input fault: "+start_scheduler->fault());
                    for(const auto& notice:start_scheduler->take_notices()) {
                        record({{"event","UI_play_rejection"},{"reason",notice.reason},
                                {"monotonic_ns",notice.monotonic_ns}});
                        throw std::runtime_error("auto-start intent rejected: "+notice.reason);
                    }
                }
                for(auto event:owner.take_coverage_updates()) {event["real_input"]=assist;record(std::move(event));}
                for(const auto& notice:owner.scheduler().take_notices())
                    record({{"event","scheduler_rejection"},{"intent_id",notice.intent_id},
                            {"reason",notice.reason},{"monotonic_ns",notice.monotonic_ns}});
                auto due=owner.scheduler().next_due_ns();
                if(start_scheduler) {
                    const auto ui_due=start_scheduler->next_due_ns();
                    if(ui_due&&(!due||*ui_due<*due)) due=ui_due;
                }
                const auto delay=due?std::clamp(*due-clock.now_ns(),Nanoseconds{0},Nanoseconds{10'000'000}):10'000'000;
                action_wakeup.wait_for(delay);
            }
            owner.stop();
            const auto game_release=owner.scheduler().last_release();
            record({{"event","game_release_report"},{"real_input",assist},{"requested_ids",game_release.requested_ids},
                {"failed_ids",game_release.failed_ids},{"unknown_ids",game_release.unknown_ids},
                {"start_ns",game_release.start_ns},{"return_ns",game_release.return_ns}});
            if(start_scheduler) {
                start_scheduler->cancel("runtime_stop");
                const auto r=start_scheduler->last_release();
                record({{"event","UI_play_release_report"},{"requested_ids",r.requested_ids},
                    {"failed_ids",r.failed_ids},{"unknown_ids",r.unknown_ids},
                    {"start_ns",r.start_ns},{"return_ns",r.return_ns},{"effect_verified",false}});
            }
            record({{"event","owner_stopped"},{"ack_ns",clock.now_ns()},
                    {"real_input_created",real_input_created.load()}});
        } catch(...) {
            failure();
            if(start_input) {
                try {
                    const auto r=start_input->emergency_release_all();
                    log.push({{"event","UI_play_emergency_release_report"},{"requested_ids",r.requested_ids},
                        {"failed_ids",r.failed_ids},{"unknown_ids",r.unknown_ids},
                        {"start_ns",r.start_ns},{"return_ns",r.return_ns},{"effect_verified",false}});
                } catch(...) { log.push({{"event","UI_play_release_unknown"}}); }
            }
        }
    });
    const auto start=clock.now_ns();
    GameRunBudget budget(start,static_cast<Nanoseconds>(std::llround(duration_s*1e9)),
                         static_cast<Nanoseconds>(std::llround(wait_play_s*1e9)));
    Nanoseconds preview_at=0; std::uint64_t draws=0, lag_drops=0;
    bool launched=false, stale_revoked=false;
    std::exception_ptr main_fault;
    try {
        record({{"event","session_state"},{"state","OBSERVING"},{"monotonic_ns",start}});
        while(!stopping&&!capture_done&&!console_stop_requested) {
            const auto first=first_playing_ns.load();
            if(first&&budget.arm_from_playing(std::max(start,first)))
                record({{"event","game_run_budget_armed"},{"origin_ns",*budget.origin()},
                        {"deadline_ns",budget.deadline()},{"basis","first_pixels_confirmed_playing"}});
            if(budget.expired(clock.now_ns()))break;
            if(preview&&!preview->pump()) {stopping=true; break;}
            if(capture) {
                const auto stats=capture->stats();
                if(stats.relative_stale_drops!=lag_drops) {
                    lag_drops=stats.relative_stale_drops; invalidate("relative_lag_drop");
                }
            }
            const auto last=last_capture.load();
            const auto now=clock.now_ns();
            if(last&&now-last>=static_cast<Nanoseconds>(std::min(100.0,stale_ms)*1e6)) {
                if(!stale_revoked) {invalidate("source_evidence_expired"); stale_revoked=true;}
            } else stale_revoked=false;
            if(!launch_package.empty()&&!launched&&last) {
                launch_android_package(find_adb(),config.serial,launch_package); launched=true;
            }
            if(preview&&now-preview_at>=static_cast<Nanoseconds>(std::llround(1e9/config.preview_hz))) {
                std::shared_ptr<const Frame> frame;
                std::shared_ptr<const DecisionSnapshot> current;
                {std::lock_guard lock(decision_mutex); current=preview_scene; frame=preview_frame;}
                if(frame) {
                    preview->draw(*frame);
                    if(current) preview->title(std::string(auto_play?"PAS auto-start / ":"PAS observe / ")+name(current->ui)+
                        " / notes="+std::to_string(current->targets.size())+" lines="+
                        std::to_string(current->lines.size())+(assist?" / REAL gameplay":auto_play?" / PLAY only; gameplay dry":" / no real input"));
                    ++draws; preview_at=now;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } catch(...) {main_fault=std::current_exception();}
    stopping=true; ++revoke; action_wakeup.notify_all();
    action_worker.request_stop(); perception_worker.request_stop(); capture_worker.request_stop();
    latest.close(); if(capture) capture->cancel();
    action_worker.join(); perception_worker.join(); capture_worker.join();
    if(shadow)shadow->stop();
    std::uint64_t diagnostic_saved=0;
    json sampled_manifest=nullptr;
    std::string sampled_path;
    if(sampler) {
        const auto path=std::filesystem::path(config.log_dir).parent_path()/"vision-dataset-20260927"/run_dir.filename();sampled_path=std::filesystem::absolute(path).string();
        try {
            sampled_manifest=sampler->flush(path,provenance,clock);diagnostic_saved=sampled_manifest.at("samples").size();
            record({{"event","pixel_samples_saved"},{"path",sampled_path},{"count",diagnostic_saved},{"manifest_sha256",sha256_file(path/"manifest.json")},{"input_already_stopped",true}});
        } catch(const std::exception& error) {
            sampling_fault=error.what();record({{"event","pixel_samples_save_failed"},{"reason",sampling_fault},{"input_already_stopped",true}});
        }
    }
    if(tracking_bank)try{tracking_bank->save(run_dir/"tracking-bank.json",provenance);}catch(const std::exception& error){record({{"event","tracking_bank_save_failed"},{"reason",error.what()}});}
    std::exception_ptr fault;
    {std::lock_guard lock(fault_mutex); fault=worker_fault;}
    const auto counts=latest.counters();
    json summary={{"mode",assist?"assist":auto_play?"auto-start":"observe"},{"state",fault||main_fault?"FAULT":"STOPPED"},
        {"input_created",real_input_created.load()},{"auto_play_requested",play_requested.load()},
        {"automatic_play_enabled",auto_play},
        {"duration_s",duration_s},{"wait_play_s",wait_play_s},
        {"duration_origin",wait_play_s>0?"first_pixels_confirmed_playing":"session_start"},
        {"budget_origin_ns",budget.origin()?json(*budget.origin()):json(nullptr)},
        {"budget_deadline_ns",budget.deadline()},
        {"playing_seen",playing_seen.load()},{"gameplay_input_enabled",assist},{"frames_consumed",consumed.load()},
        {"published",counts.published},{"overwritten",counts.overwritten},{"pool_drops",counts.pool_drops},
        {"consumer_skips",counts.consumer_skips},{"preview_draws",draws},{"dry_commands",dry_commands.load()},
        {"gameplay_commands",gameplay_commands.load()},
        {"diagnostic_images_saved",diagnostic_saved},
        {"vision_dataset_path",sampled_path},{"sampling_manifest",sampled_manifest},{"sampling_fault",sampling_fault},
        {"sampling_copy_ms",distribution(sampling_copy)},{"shadow_publication_ms",distribution(shadow_publication)},{"tracking_bank_copy_ms",distribution(bank_copy)},
        {"tracking_shadow",shadow?shadow->stats():json(nullptr)},{"tracking_bank",tracking_bank?tracking_bank->stats():json(nullptr)},
        {"stop_reason",console_stop_requested?"console_stop":budget.expired(clock.now_ns())?budget.expiry_reason():"preview_or_fault"},
        {"revocations",revoke.load()},{"relative_stale_drops",lag_drops},
        {"capture_interval_ms",distribution(intervals)},{"host_residency_ms",distribution(residency)},
        {"recognition_duration_ms",distribution(recognition)},{"source_absolute_age",nullptr},
        {"gameplay_validated",false},{"run_dir",std::filesystem::absolute(run_dir).string()}};
    if(fault||main_fault) {
        try {std::rethrow_exception(fault?fault:main_fault);} catch(const std::exception& e) {summary["fault"]=e.what();}
    }
    log.push({{"event","summary"},{"summary",summary}}); log.close();
    {std::ofstream file(run_dir/"summary.json"); file<<summary.dump(2)<<'\n';}
    if(fault) std::rethrow_exception(fault);
    if(main_fault) std::rethrow_exception(main_fault);
    if(log.faulted()) throw std::runtime_error("observe journal failed");
    if(!consumed) throw std::runtime_error("observe session had no valid frame");
    std::cout<<summary.dump(2)<<'\n';
}
void run_observe(const std::string& config_path,double duration_s,bool no_preview,
                 const std::string& launch_package,double stale_ms) {
    run_runtime(config_path,duration_s,no_preview,launch_package,stale_ms,false,"");
}
void run_auto_start(const std::string& config_path,const std::string& capability_path,
                    double duration_s,bool no_preview) {
    run_runtime(config_path,duration_s,no_preview,"",100,true,capability_path);
}
void run_assist(const std::string& config_path,const std::string& capability_path,double duration_s,bool no_preview,bool keep_diagnostic_anomalies,bool keep_vision_dataset,const std::string& tracking_shadow,bool manual_play,double wait_play_s) {
    if(manual_play&&(!std::isfinite(wait_play_s)||wait_play_s<=0||wait_play_s>60))
        throw std::invalid_argument("manual PLAY wait must be in (0,60] seconds");
    run_runtime(config_path,duration_s,no_preview,"",100,!manual_play,capability_path,true,keep_diagnostic_anomalies,keep_vision_dataset,tracking_shadow,manual_play?wait_play_s:0);
}
}
