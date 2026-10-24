#include "trial_io.hpp"
#include "opening_detector.hpp"
#include "first_tap_trial.hpp"
#include "opening_trial.hpp"
#include "../chart_manual/foreground_watch.hpp"
#include "rhythm/manual_lifecycle.hpp"
#include "rhythm/phigros_pose.hpp"
#include "rhythm/screen_state_io.hpp"
#include "rhythm/visual_candidates.hpp"
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <charconv>
#include <cmath>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
using namespace rhythm;
using namespace rhythm::calibration_trial;
constexpr SongNs ms=1'000'000,second=1'000'000'000;
constexpr const char* chart_sha="aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90";
constexpr const char* usage="chart_calibrate run SERIAL AVD CHART.json STATE.json KERNEL.json NEW_DIR PHASE_MS --experimental-first-tap|--experimental-three-tap\n"
    "One or three Dlyrotz IN13 opening Taps. Begin on its result screen, then manually retry.\n"
    "Phase -100..100 ms is an experimental combined phase; not input lead. One DOWN, 16 ms UP.\n"
    "Arrival-only visual estimate, no qualified SyncLease. Esc/Ctrl+C/NEW_DIR/STOP cancels.\n";
std::atomic<bool> interrupted{false};
BOOL WINAPI on_stop(DWORD signal) {
    if(signal==CTRL_C_EVENT||signal==CTRL_BREAK_EVENT||signal==CTRL_CLOSE_EVENT||signal==CTRL_SHUTDOWN_EVENT){interrupted=true;return TRUE;}
    return FALSE;
}
struct SignalGuard {
    SignalGuard(){interrupted=false;if(!SetConsoleCtrlHandler(on_stop,TRUE))throw std::runtime_error("signal handler failed");}
    ~SignalGuard(){SetConsoleCtrlHandler(on_stop,FALSE);}
};
int phase_option(const char* input) {
    const std::string text=input;int value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||value< -100||value>100)throw std::invalid_argument("phase must be -100..100 ms");
    return value;
}
std::string binary_sha() {
    wchar_t path[32768]{};const auto length=GetModuleFileNameW(nullptr,path,32768);
    if(!length||length==32768)throw std::runtime_error("binary identity unavailable");
    return chart_text_sha256(read_file(path,256*1024*1024));
}
std::string rgb_sha(const pas::Frame& frame) {
    return chart_text_sha256({reinterpret_cast<const char*>(frame.rgb.data()),frame.rgb.size()});
}
Json intervals(const std::vector<VisualTimeInterval>& values) {
    Json out=Json::array();for(auto value:values)out.push_back({value.lower,value.upper});return out;
}
int run(int argc,char** argv) {
    if(argc!=10||std::string(argv[1])!="run")throw std::invalid_argument(usage);
    const bool three_tap=std::string(argv[9])=="--experimental-three-tap";
    if(!three_tap&&std::string(argv[9])!="--experimental-first-tap")throw std::invalid_argument(usage);
    const std::string serial=argv[2],avd=argv[3];const int phase=phase_option(argv[8]);
    if(serial!="emulator-5554"||avd!="phigros")throw std::invalid_argument("this experiment is restricted to phigros / emulator-5554");
    const auto chart_bytes=read_file(argv[4],32*1024*1024);
    ChartSource chart_source;chart_source.label=argv[4];chart_source.chart_sha256=chart_sha;chart_source.song_id="Dlyrotz.Likey.0";
    chart_source.difficulty="IN13";chart_source.game_revision="4.0.1/157";
    const auto chart=parse_phigros(chart_bytes,chart_source);
    if(chart.lines.empty()||chart.lines[0].notes.empty())throw std::runtime_error("missing target note");
    const auto& note=chart.lines[0].notes[0];PhigrosPoseEvaluator pose(chart);
    const auto target=pose.note_target(0,note,note.start_ns);
    if(note.source.json_pointer!="/judgeLineList/0/notesAbove/0"||note.kind!=NoteKind::tap||
       note.start_ns!=12*second||std::abs(target.reference_pixels.x-352)>.001||std::abs(target.reference_pixels.y-576)>.001)
        throw std::runtime_error("pinned first-note identity/geometry mismatch");
    if(three_tap) {
        const auto expected=calibration::OpeningTrial::notes();
        if(chart.lines[0].notes.size()<expected.size())throw std::runtime_error("missing opening notes");
        for(std::size_t i=0;i<expected.size();++i) {
            const auto& current=chart.lines[0].notes[i];const auto point=pose.note_target(0,current,current.start_ns);
            if(current.kind!=NoteKind::tap||current.start_ns!=expected[i].song_ns||
               current.source.json_pointer!="/judgeLineList/0/notesAbove/"+std::to_string(i)||
               std::abs(point.reference_pixels.x-expected[i].x)>.001||std::abs(point.reference_pixels.y-expected[i].y)>.001)
                throw std::runtime_error("pinned three-note identity/geometry mismatch");
        }
    }
    const auto screen_calibration=load_screen_state_calibration(argv[5]);
    if(screen_calibration.width!=1280||screen_calibration.height!=720)throw std::runtime_error("screen profile must be 1280x720");
    const auto kernel_bytes=read_file(argv[6],8192);const Json kernel_profile=Json::parse(kernel_bytes);
    const std::filesystem::path output=std::filesystem::absolute(argv[7]);
    if(std::filesystem::exists(output))throw std::runtime_error("output must not exist");
    const auto adb=pas::find_adb();const auto avd_text=adb_text(adb,serial,{"emu","avd","name"});
    if(avd_text.substr(0,avd_text.find_first_of("\r\n"))!=avd)throw std::runtime_error("AVD mismatch");
    const auto package_report=adb_text(adb,serial,{"shell","dumpsys","package","com.PigeonGames.Phigros"});
    if(field(package_report,"versionName=")!="4.0.1"||field(package_report,"versionCode=")!="157")throw std::runtime_error("game version mismatch");
    pas::HostClock clock;const auto foreground_begin=clock.now_ns();foreground(adb,serial);const auto foreground_end=clock.now_ns();
    manual_ui::ForegroundWatch foreground_watch([adb,serial]{foreground(adb,serial);},[clock]{return clock.now_ns();},{foreground_begin,foreground_end});
    KernelSlots kernel(adb,serial,kernel_profile,clock);const auto initial_kernel=kernel.query();
    if(initial_kernel.at("active_count")!=0||initial_kernel.at("causal_host_interval_valid")!=true)throw std::runtime_error("initial kernel state not fresh zero");
    const auto endpoint=pas::discover_endpoint(serial);
    pas::CaptureOptions options;options.width=1280;options.height=720;options.source_rotation=1;options.max_relative_lag_ns=250*ms;
    pas::GrpcCapture capture(clock,endpoint,options);
    const auto initial_frame=capture.snapshot(std::chrono::milliseconds(1000));
    if(classify_screen_state({initial_frame.width,initial_frame.height,initial_frame.stride,initial_frame.rgb},screen_calibration.templates).state!=ObservedGameState::result)
        throw std::runtime_error("start on manually verified Dlyrotz IN13 result screen");
    if(!std::filesystem::create_directory(output))throw std::runtime_error("output creation failed");
    Journal journal(output/"events.jsonl");
    OpeningVisualModel model;model.chart_sha256=chart_sha;model.model_id="experimental-opening-static-v1";
    const Json model_settings={{"id",model.model_id},{"domain_ns",{model.begin_ns,model.end_ns}},
        {"scale",{model.approach_scale_lower,model.approach_scale_upper}},{"pixel_error",model.pixel_error},
        {"pose_envelope_pixels",model.pose_tolerance_pixels},{"horizontal_visibility_margin_pixels",model.horizontal_visibility_margin_pixels},
        {"source_clock","arrival_only"},{"coverage_assumption","complete unoccluded cyan centers; conditional trial, not established detection guarantee"}};
    model.model_sha256=chart_text_sha256(model_settings.dump());model.calibration_id="unqualified-first-tap-experiment";model.calibration_sha256=model.model_sha256;
    VisualCandidateLimits limits;limits.max_observations=256;
    VisualTimingCandidateProducer candidates(chart,model,limits);candidates.begin_epoch(1);
    calibration::FirstTapTrial trial(static_cast<SongNs>(phase)*ms);
    calibration::OpeningTrial opening;
    ManualConfig manual_config;manual_config.round_limit=1;manual_config.round_watchdog_ns=25*second;
    ManualLifecycle lifecycle(manual_config);
    Json manifest={{"schema",three_tap?"pas.experimental-three-tap.v1":"pas.experimental-first-tap.v1"},{"serial",serial},{"avd",avd},
        {"executable_sha256",binary_sha()},{"chart_sha256",chart_sha},{"screen_calibration_sha256",screen_calibration.dataset_sha256},
        {"kernel_profile_sha256",chart_text_sha256(kernel_bytes)},{"kernel_initial",initial_kernel},
        {"model_settings",model_settings},{"model_sha256",model.model_sha256},{"note_json_pointer",note.source.json_pointer},
        {"note_song_ns",note.start_ns},{"target_reference_pixels",{target.reference_pixels.x,target.reference_pixels.y}},
        {"experimental_phase_ms",phase},{"phase_semantics","added to arrival-anchored prediction; combined capture/render/dispatch phase, not separated input latency"},
        {"input_lead_ns",nullptr},{"input_enabled",true},{"timing_qualified",false},{"live_game_qualified",false},
        {"source_pixel_age_qualified",false},{"audio_enabled",false},{"automatic_play_enabled",false},
        {"max_notes",three_tap?3:1},{"max_down_attempts",three_tap?3:1},{"up_after_down_return_ms",16},{"arm_horizon_ms",3000},
        {"session_limit_seconds",90},{"source_sequence_semantics","stream event counter, not rendered-frame ID"},
        {"deadline_scope","cooperative deadlines; retained ADB termination/reader joins and OS calls have no hard wall-time guarantee"},
        {"journal_policy","bounded 64MiB/16000 rows in memory during trial; persisted after release; process crash may lose unflushed observations"},
        {"transport",pas::grpc_transport_manifest(options.grpc_read_chunk_kib)}};
    manifest["planned_taps"]=Json::array();
    for(std::size_t i=0;i<(three_tap?3u:1u);++i) {
        const auto expected=calibration::OpeningTrial::notes()[i];
        manifest["planned_taps"].push_back({{"song_ns",expected.song_ns},{"reference_pixels",{expected.x,expected.y}},
            {"json_pointer","/judgeLineList/0/notesAbove/"+std::to_string(i)}});
    }
    write_json(output/"plan.json",manifest);pas::write_diagnostic_png(output/"initial-result.png",initial_frame);
    SignalGuard signal_guard;
    pas::PixelCoordinateMap mapping(1280,720,720,1280,90);
    pas::GrpcTouch touch(clock,endpoint,mapping,720,1280,1,std::chrono::milliseconds(100));
    pas::LatestFrame latest(1280,720,3,&clock);std::atomic<bool> capture_done{false};std::mutex error_mutex;std::exception_ptr capture_error;
    std::jthread capture_worker([&](std::stop_token stop){
        try{capture.stream(stop,[&](pas::Frame&& frame){frame.generation=1;frame.geometry_version=1;latest.publish(frame.rgb.data(),frame.rgb.size(),frame);});}
        catch(...){std::lock_guard lock(error_mutex);capture_error=std::current_exception();}capture_done=true;latest.close();
    });
    const auto started=clock.now_ns();auto status=lifecycle.status(started);
    SongNs next_candidate=started,last_saved=-1,down_return=-1,up_return=-1,kernel_request=-1;
    std::uint64_t consumed=0,candidate_sequence=0;bool saw_initial_result=false,armed=false,down_attempted=false,up_attempted=false;
    bool kernel_ready=false,finished=false,release_known=false,final_zero=false;
    std::string failure;std::vector<pas::Frame> saved_frames;std::optional<pas::Frame> anchor_frame;
    std::vector<pas::TouchReceipt> pending_receipts;pending_receipts.reserve(2);
    Json kernel_before=nullptr;
    std::future<Json> kernel_pending;bool kernel_started=false;
    const auto ui_healthy=[&](SongNs now){return status.state==ManualState::playing&&status.ui_playing&&
        status.received_host_ns>=0&&now>=status.received_host_ns&&now-status.received_host_ns<100*ms&&foreground_watch.status().healthy;};
    const auto opening_ui=[&](SongNs now) {
        calibration::OpeningUiEvidence ui;ui.sequence=status.observation_sequence;ui.epoch=ui.generation=ui.geometry_version=1;
        ui.received_host_ns=status.received_host_ns;ui.source_id="trial-grpc-stream-1";ui.evidence_id=status.evidence_id;
        ui.playing=ui_healthy(now);ui.source_valid=ui.playing;return ui;
    };
    std::cout<<(three_tap?"THREE TAP":"FIRST TAP")<<" EXPERIMENT READY: manually retry Dlyrotz IN13 once.\n"<<std::flush;
    try {
        while(!finished) {
            auto now=clock.now_ns();
            if(interrupted.load()||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||std::filesystem::exists(output/"STOP"))throw std::runtime_error("user_stop");
            if(now-started>=90*second)throw std::runtime_error("session_timeout");
            if(capture_done.load()){std::lock_guard lock(error_mutex);if(capture_error)std::rethrow_exception(capture_error);throw std::runtime_error("capture_ended");}
            if(!foreground_watch.status().healthy)throw std::runtime_error("foreground_unavailable");
            status=lifecycle.poll(now);
            const auto frame=latest.read_after(consumed,armed?1*ms:10*ms);
            if(frame) {
                consumed=frame->sequence;const auto classified=classify_screen_state({frame->width,frame->height,frame->stride,frame->rgb},screen_calibration.templates);
                ManualObservation observation;observation.sequence=frame->sequence;observation.generation=frame->generation;observation.geometry_version=frame->geometry_version;
                observation.width=frame->width;observation.height=frame->height;observation.rotation=frame->source_rotation;
                observation.timestamp_kind=ManualTimestampKind::arrival_only;observation.received_host_ns=frame->capture_complete_ns;
                observation.source_valid=frame->source_valid;observation.evidence_id="trial-stream/frame-"+std::to_string(frame->sequence);
                observation.hud=classified.state==ObservedGameState::playing;observation.pause=classified.state==ObservedGameState::paused;
                observation.result=classified.state==ObservedGameState::result;
                if(observation.result&&status.round==0)saw_initial_result=true;
                now=clock.now_ns();status=lifecycle.observe(observation,now);
                if(!armed&&!ui_healthy(now))trial.status(now,false);
                journal.append({{"event","ui"},{"sequence",frame->sequence},{"received_ns",frame->capture_complete_ns},{"processed_ns",now},
                    {"source_timestamp_us",frame->source_timestamp_us?Json(*frame->source_timestamp_us):Json(nullptr)},
                    {"state",manual_state_name(status.state)},{"round",status.round},{"reason",status.reason},{"source_valid",frame->source_valid}});
                if(armed) {
                    if(three_tap) {
                        const auto current=opening.observe_ui(opening_ui(now),now);
                        if(current.state==calibration::OpeningTrialState::cancelled)throw std::runtime_error(current.reason);
                    } else {
                        const auto current=trial.status(now,ui_healthy(now));
                        if(current.state==calibration::FirstTapState::cancelled)throw std::runtime_error(current.reason);
                    }
                    const auto first_due=manifest.at("experimental_target_host_ns").get<SongNs>();
                    if(now>=first_due-200*ms&&saved_frames.size()<(three_tap?48u:24u)&&
                       (last_saved<0||frame->capture_complete_ns-last_saved>=20*ms)) {
                        saved_frames.push_back(*frame);last_saved=frame->capture_complete_ns;
                    }
                } else if(saw_initial_result&&status.round==1&&ui_healthy(now)&&now>=next_candidate) {
                    next_candidate=now+75*ms;
                    // The detector cannot establish coverage. The complete/unoccluded condition
                    // below is explicitly an experimental model assumption recorded in plan.json.
                    const auto detected=calibration::detect_opening_centers(frame->width,frame->height,frame->stride,frame->rgb);
                    if(detected.valid&&!detected.centers.empty()) {
                        VisualCandidateFrame observation_frame;observation_frame.sequence=++candidate_sequence;
                        observation_frame.epoch=observation_frame.generation=observation_frame.geometry_version=1;
                        observation_frame.width=1280;observation_frame.height=720;observation_frame.source_id="live-grpc-phigros-trial";
                        observation_frame.source_sha256=chart_text_sha256("emulator-5554/phigros/1280x720/r1/"+kernel_profile.at("boot_id").get<std::string>());
                        observation_frame.frame_id=observation.evidence_id;observation_frame.evidence_id=observation.evidence_id;
                        observation_frame.frame_sha256=rgb_sha(*frame);observation_frame.clock_id="host-QPC-receipt-only";
                        observation_frame.opening_context_id="manual-round-1-user-selected-dlyrotz";observation_frame.clock_kind=VisualSourceClock::arrival_only;
                        observation_frame.received_host_ns=frame->capture_complete_ns;observation_frame.source_valid=frame->source_valid;
                        observation_frame.centers_complete_unoccluded=true;observation_frame.centers=detected.centers;
                        const auto candidate_now=clock.now_ns();
                        if(candidate_now-observation_frame.received_host_ns>=100*ms) {
                            trial.status(candidate_now,false);
                            journal.append({{"event","candidate_dropped_before_solver"},{"frame",observation.evidence_id},
                                {"received_ns",observation_frame.received_host_ns},{"processed_ns",candidate_now},{"reason","receipt_expired_during_processing"}});
                            continue;
                        }
                        const auto result=candidates.observe(observation_frame,candidate_now);
                        const auto trial_status=trial.observe(result,ui_healthy(clock.now_ns()),clock.now_ns());
                        Json centers=Json::array();for(const auto& center:detected.centers)centers.push_back({{"id",center.id},{"x",center.x},{"y",center.y}});
                        journal.append({{"event","candidate"},{"frame",observation.evidence_id},{"rgb_sha256",observation_frame.frame_sha256},
                            {"centers",centers},{"song_intervals",intervals(result.song_intervals)},{"assignments",result.candidates.size()},
                            {"reason",result.reason},{"trial_reason",trial_status.reason},{"support_count",trial_status.support_count},
                            {"received_ns",observation_frame.received_host_ns},{"processed_ns",candidate_now},{"timing_qualified",false}});
                        const auto armed_time=clock.now_ns();
                        if(trial.arm(armed_time)) {armed=true;anchor_frame=*frame;const auto s=trial.status(clock.now_ns(),true);
                            if(three_tap&&!opening.arm(s,armed_time,clock.now_ns(),opening_ui(clock.now_ns())))
                                throw std::runtime_error(opening.status(clock.now_ns()).reason);
                            manifest["experimental_target_host_ns"]=s.target_host_ns?Json(*s.target_host_ns):Json(nullptr);
                            manifest["anchor_rgb_sha256"]=observation_frame.frame_sha256;manifest["anchor_received_ns"]=frame->capture_complete_ns;
                            manifest["anchor_song_intervals"]=intervals(result.song_intervals);
                            manifest["anchor_origin_host_ns"]=s.latest_origin_ns?Json(*s.latest_origin_ns):Json(nullptr);
                            manifest["anchor_origin_intervals"]=intervals(s.provisional_origin_intervals);
                            manifest["anchor_armed_host_ns"]=armed_time;
                            journal.append({{"event",three_tap?"armed_three_taps":"armed_one_tap"},{"target_host_ns",manifest["experimental_target_host_ns"]},{"host_ns",clock.now_ns()}});
                            std::cout<<(three_tap?"ARMED THREE TAPS\n":"ARMED ONE TAP\n")<<std::flush;
                        }
                        if(result.requires_new_epoch)throw std::runtime_error("visual_epoch_invalidated: "+result.reason);
                    }
                }
            }
            now=clock.now_ns();
            if(status.session_done)throw std::runtime_error("manual_round_ended_before_trial_complete");
            if(armed&&three_tap) {
                const auto state=opening.status(now);
                if(state.state==calibration::OpeningTrialState::cancelled)throw std::runtime_error(state.reason);
                const auto first_due=manifest.at("experimental_target_host_ns").get<SongNs>();
                if(!kernel_started&&now>=first_due-350*ms) {
                    kernel_started=true;kernel_pending=std::async(std::launch::async,[&kernel]{return kernel.query();});
                }
                if(kernel_started&&!kernel_ready&&kernel_pending.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
                    kernel_before=kernel_pending.get();kernel_request=kernel_before.at("host_request_begin_ns").get<SongNs>();
                    journal.append({{"event","kernel_pre_down"},{"snapshot",kernel_before}});
                    if(kernel_before.at("active_count")!=0||kernel_before.at("causal_host_interval_valid")!=true)
                        throw std::runtime_error("pre_down_kernel_not_fresh_zero");
                    kernel_ready=true;
                }
                now=clock.now_ns();
                if(state.state==calibration::OpeningTrialState::waiting&&state.down_due_host_ns&&now>=*state.down_due_host_ns) {
                    if(!ui_healthy(now))throw std::runtime_error("UI_unhealthy_before_sequence_DOWN");
                    if(state.down_attempts==0&&(!kernel_ready||now-kernel_request>=500*ms))throw std::runtime_error("kernel_zero_interval_expired");
                    const auto command=opening.claim_down(now);if(!command)throw std::runtime_error("sequence_down_claim_failed");
                    down_attempted=true;
                    const auto receipt=touch.inject({command->tap_index+1,command->contact_id,pas::Phase::down,
                        command->x,command->y,command->scheduled_host_ns,consumed});
                    down_return=receipt.injection_return_ns;opening.note_down_return(down_return,receipt.success);pending_receipts.push_back(receipt);
                    if(!receipt.success)throw std::runtime_error("sequence_DOWN_RPC_unknown");
                    while(true) {
                        now=clock.now_ns();
                        if(interrupted.load()||(GetAsyncKeyState(VK_ESCAPE)&0x8000))throw std::runtime_error("user_stop_while_held");
                        if(!ui_healthy(now))throw std::runtime_error("UI_unhealthy_while_held");
                        const auto held=opening.status(now);
                        if(held.state==calibration::OpeningTrialState::cancelled)throw std::runtime_error(held.reason);
                        if(held.up_due_host_ns&&now>=*held.up_due_host_ns)break;
                        std::this_thread::yield();
                    }
                    const auto command_up=opening.claim_up(clock.now_ns());if(!command_up)throw std::runtime_error("sequence_up_claim_failed");
                    up_attempted=true;
                    const auto up=touch.inject({command_up->tap_index+1,command_up->contact_id,pas::Phase::up,
                        command_up->x,command_up->y,command_up->scheduled_host_ns,consumed});
                    up_return=up.injection_return_ns;opening.note_up_return(up_return,up.success);pending_receipts.push_back(up);
                    if(!up.success)throw std::runtime_error("sequence_UP_RPC_unknown");
                    const auto returned=opening.status(up_return);
                    if(returned.state==calibration::OpeningTrialState::cancelled)throw std::runtime_error(returned.reason);
                    for(const auto& value:pending_receipts)journal.append(receipt_json(value));pending_receipts.clear();
                    journal.append({{"event","opening_tap_RPC_completed"},{"tap_index",command->tap_index},
                        {"song_ns",calibration::OpeningTrial::notes()[command->tap_index].song_ns},{"host_ns",up_return}});
                }
            }
            if(armed&&!three_tap&&!up_attempted) {
                const auto state=trial.status(now,ui_healthy(now));
                if(state.state==calibration::FirstTapState::cancelled)throw std::runtime_error(state.reason);
                if(!down_attempted&&state.target_host_ns) {
                    if(!kernel_started&&now>=*state.target_host_ns-350*ms) {
                        kernel_started=true;
                        // ADB process latency must not block delivery/expiry of UI evidence.
                        kernel_pending=std::async(std::launch::async,[&kernel]{return kernel.query();});
                    }
                    if(kernel_started&&!kernel_ready&&kernel_pending.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
                        kernel_before=kernel_pending.get();kernel_request=kernel_before.at("host_request_begin_ns").get<SongNs>();
                        journal.append({{"event","kernel_pre_down"},{"snapshot",kernel_before}});
                        if(kernel_before.at("active_count")!=0||kernel_before.at("causal_host_interval_valid")!=true)throw std::runtime_error("pre_down_kernel_not_fresh_zero");
                        kernel_ready=true;
                    }
                    now=clock.now_ns();
                    if(now>=*state.target_host_ns) {
                        const auto dispatch_state=trial.status(now,ui_healthy(now));
                        if(dispatch_state.state!=calibration::FirstTapState::armed)throw std::runtime_error(dispatch_state.reason);
                        if(!kernel_ready||now-kernel_request>=500*ms)throw std::runtime_error("kernel_zero_interval_expired");
                        if(!trial.claim_down(now))throw std::runtime_error("down_claim_failed");
                        down_attempted=true;const auto receipt=touch.inject({1,0,pas::Phase::down,target.reference_pixels.x,target.reference_pixels.y,*state.target_host_ns,consumed});
                        down_return=receipt.injection_return_ns;trial.note_down_return(down_return,receipt.success);pending_receipts.push_back(receipt);
                        if(!receipt.success)throw std::runtime_error("DOWN_RPC_unknown");
                        // No disk flush, classification or large frame copy while held.
                        // Existing UI evidence has its own 100ms expiry. These are bounded
                        // engineering deadlines, not a hard realtime OS guarantee.
                        while(true) {
                            now=clock.now_ns();
                            if(interrupted.load()||(GetAsyncKeyState(VK_ESCAPE)&0x8000))throw std::runtime_error("user_stop_while_held");
                            const auto held=trial.status(now,ui_healthy(now));
                            if(held.state==calibration::FirstTapState::cancelled)throw std::runtime_error(held.reason);
                            if(trial.up_due(now))break;
                            std::this_thread::yield();
                        }
                        if(!trial.claim_up(clock.now_ns()))throw std::runtime_error("up_claim_failed");
                        up_attempted=true;const auto up=touch.inject({1,0,pas::Phase::up,target.reference_pixels.x,target.reference_pixels.y,clock.now_ns(),consumed});
                        up_return=up.injection_return_ns;trial.note_up_return(up_return,up.success);pending_receipts.push_back(up);
                        if(!up.success)throw std::runtime_error("UP_RPC_unknown");
                        const auto completed=trial.status(up_return,ui_healthy(up_return));
                        if(completed.state!=calibration::FirstTapState::completed)throw std::runtime_error(completed.reason);
                        for(const auto& value:pending_receipts)journal.append(receipt_json(value));
                        pending_receipts.clear();
                    }
                }
            }
            if(up_attempted&&now-up_return>=500*ms&&(!three_tap||opening.status(clock.now_ns()).state==calibration::OpeningTrialState::completed))finished=true;
        }
    }catch(const std::exception& error){failure=error.what();if(failure.empty())failure="trial_exception_without_message";trial.cancel(failure);opening.cancel(failure);}
    // Independent best-effort cleanup steps: failure in release/logging must not
    // skip recovery or the read-only final observation. Errors never become zero.
    const auto cleanup_error=[&](const char* stage,const std::exception& error){
        if(!failure.empty())failure+="; ";failure+=std::string(stage)+": "+error.what();
    };
    try {
        const auto release=touch.release_all();
        release_known=release.failed_ids.empty()&&release.unknown_ids.empty();
        manifest["release"]=release_json(release);
    }catch(const std::exception& error){cleanup_error("release",error);}
    bool recovery_attempted=false;
    const auto recovery_release=[&] {
        recovery_attempted=true;
        try {
            pas::GrpcTouch recovery(clock,endpoint,mapping,720,1280,1,std::chrono::milliseconds(100));
            const auto release=recovery.emergency_release_all();
            release_known=release.failed_ids.empty()&&release.unknown_ids.empty();
            manifest["recovery_release"]=release_json(release);
        }catch(const std::exception& error){release_known=false;cleanup_error("recovery",error);}
    };
    if(!release_known)recovery_release();
    // Reap the read-only worker after input cleanup. Its inherited ADB join is
    // cooperative, not a hard wall bound; it never owns or sends touch commands.
    if(kernel_pending.valid()) {
        try{manifest["pending_kernel_on_exit"]=kernel_pending.get();}
        catch(const std::exception& error){cleanup_error("pending_kernel",error);}
    }
    try {
        const auto final=kernel.query();manifest["kernel_final"]=final;
        final_zero=final.at("active_count")==0&&final.at("causal_host_interval_valid")==true;
    }catch(const std::exception& error){cleanup_error("kernel_final",error);}
    if(!final_zero&&down_attempted&&!recovery_attempted) {
        recovery_release();
        try {
            const auto final=kernel.query();manifest["kernel_after_recovery"]=final;
            final_zero=final.at("active_count")==0&&final.at("causal_host_interval_valid")==true;
        }catch(const std::exception& error){cleanup_error("kernel_after_recovery",error);}
    }
    capture_worker.request_stop();capture.cancel();latest.close();capture_worker.join();
    const bool foreground_joined=foreground_watch.stop();
    if(!foreground_joined&&failure.empty())failure="foreground_stop_timeout";
    // A failed DOWN/UP path persists receipts only after its release attempt.
    for(const auto& value:pending_receipts)journal.append(receipt_json(value));
    journal.flush_after_cleanup();
    if(anchor_frame)pas::write_diagnostic_png(output/"anchor.png",*anchor_frame);
    Json frame_index=Json::array();std::size_t index=0;
    for(const auto& frame:saved_frames) {
        const auto name="judgment-"+std::to_string(++index)+".png";pas::write_diagnostic_png(output/name,frame);
        frame_index.push_back({{"file",name},{"sequence",frame.sequence},{"received_host_ns",frame.capture_complete_ns},{"rgb_sha256",rgb_sha(frame)}});
    }
    manifest["frames"]=frame_index;manifest["down_attempted"]=down_attempted;manifest["up_attempted"]=up_attempted;
    manifest["down_return_ns"]=down_return<0?Json(nullptr):Json(down_return);manifest["up_return_ns"]=up_return<0?Json(nullptr):Json(up_return);
    manifest["kernel_before_down"]=kernel_before;manifest["release_rpc_known"]=release_known;manifest["kernel_final_zero"]=final_zero;
    manifest["game_judgment"]="unreviewed";manifest["failure"]=failure.empty()?Json(nullptr):Json(failure);
    bool sequence_complete=!three_tap;
    if(three_tap) {const auto sequence_status=opening.status(clock.now_ns());
        manifest["down_attempts"]=sequence_status.down_attempts;manifest["completed_taps"]=sequence_status.completed_taps;
        manifest["sequence_reason"]=sequence_status.reason;
        sequence_complete=sequence_status.state==calibration::OpeningTrialState::completed&&sequence_status.down_attempts==3&&sequence_status.completed_taps==3;}
    manifest["status"]=failure.empty()&&down_attempted&&up_attempted&&sequence_complete&&release_known&&final_zero?"experimental_tap_sent_judgment_unreviewed":"failed_or_stopped";
    manifest["start_host_ns"]=started;manifest["stop_host_ns"]=clock.now_ns();manifest["journal_rows"]=journal.rows;
    write_json(output/"manifest.json",manifest);std::cout<<manifest.dump(2)<<'\n';
    return manifest.at("status")=="experimental_tap_sent_judgment_unreviewed"?0:2;
}
} // namespace
int main(int argc,char** argv) {
    try{if(argc==2&&std::string(argv[1])=="--help"){std::cout<<usage;return 0;}return run(argc,argv);}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
