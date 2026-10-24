#include "experimental_runtime.hpp"
#include "../chart_calibrate/trial_io.hpp"
#include "../chart_calibrate/opening_detector.hpp"
#include "first_note_trigger.hpp"
#include "hud_glyph_evidence.hpp"
#include "hud_timeline.hpp"
#include "../chart_manual/foreground_watch.hpp"
#include "rhythm/manual_lifecycle.hpp"
#include "rhythm/screen_state_io.hpp"
#define NOMINMAX
#include <windows.h>
#include "owner_wait.hpp"
#include <atomic>
#include <charconv>
#include <future>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

namespace {
using namespace rhythm;
using namespace rhythm::calibration_trial;
using namespace rhythm::experimental;
constexpr Ns ms=1'000'000,second=1'000'000'000;
// UI is a bounded pause/exit watchdog, not the song clock. Only an actual
// recognized playing frame renews it; unknown pixels never renew permission.
constexpr Ns ui_watchdog=500*ms;
constexpr std::size_t expected_steps=5669;
constexpr const char* expected_plan_sha="74e16603971e407627ea679a56f20300043a43c9fdf89008d9d6645d703e3ce5";
constexpr const char* chart_sha="aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90";
constexpr const char* audio_sha="aa9423b0f86ca4c1250f460bfbbc23d78a6405c2de739a2ba5ffef0b48261a44";
constexpr const char* source_id="live-grpc-phigros-whole-song";
constexpr const char* hud_reference_sha="397431f2749bb38a845c6845977fc5c37cb2de51a79ff273dd3417ef2756e09a";
constexpr const char* usage=
    "chart_experiment run SERIAL AVD CHART.json STATE.json KERNEL.json NEW_DIR PHASE_MS --experimental-whole-song [--experimental-hud-reference CLEAN.rgb]\n"
    "Dlyrotz IN13 complete 584-note / 5669-step candidate, capacity 4, D250 projection.\n"
    "Experimental Tap/Drag dwell32ms; Flick lead30ms/travel60ms/distance60px/release24ms; HUD timeline at50ms.\n"
    "Begin on its manually verified song-selection or result screen, then start/retry once. Phase -100..100 ms; -80 is the current hypothesis.\n"
    "Arrival-only rate=1 experiment; no qualified SyncLease/input latency/audio. Esc/Ctrl+C/NEW_DIR/STOP cancels.\n";
std::atomic<bool> interrupted{false};
BOOL WINAPI on_stop(DWORD signal) {
    if(signal==CTRL_C_EVENT||signal==CTRL_BREAK_EVENT||signal==CTRL_CLOSE_EVENT||signal==CTRL_SHUTDOWN_EVENT) {
        interrupted=true;return TRUE;
    }
    return FALSE;
}
struct SignalGuard {
    SignalGuard(){interrupted=false;if(!SetConsoleCtrlHandler(on_stop,TRUE))throw std::runtime_error("signal handler failed");}
    ~SignalGuard(){SetConsoleCtrlHandler(on_stop,FALSE);}
};
int phase_option(const char* input) {
    const std::string value=input;int phase=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),phase);
    if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||phase< -100||phase>100)
        throw std::invalid_argument("phase must be -100..100 ms");
    return phase;
}
std::string binary_sha() {
    wchar_t path[32768]{};const auto count=GetModuleFileNameW(nullptr,path,32768);
    if(!count||count==32768)throw std::runtime_error("binary identity unavailable");
    return chart_text_sha256(read_file(path,256*1024*1024));
}
std::string rgb_sha(const pas::Frame& frame) {
    return chart_text_sha256({reinterpret_cast<const char*>(frame.rgb.data()),frame.rgb.size()});
}
Json intervals(const std::vector<VisualTimeInterval>& values) {
    Json result=Json::array();for(const auto value:values)result.push_back({value.lower,value.upper});return result;
}
Json candidate_assignments(const std::vector<VisualTimeCandidate>& values) {
    Json result=Json::array();
    for(const auto& candidate:values) {
        Json matches=Json::array();
        for(const auto& match:candidate.matches)matches.push_back({{"center_id",match.center_id},
            {"note_id",match.note_id},{"note_json_pointer",match.note_json_pointer}});
        result.push_back({{"song_interval",{candidate.song_time.lower,candidate.song_time.upper}},
            {"line_speed",candidate.line_speed},{"speed_event_json_pointer",candidate.speed_event_json_pointer},{"matches",std::move(matches)}});
    }
    return result;
}
Json runtime_json(const ExperimentalRuntimeStatus& s) {
    return {{"state",static_cast<int>(s.state)},{"reason",s.reason},{"selected_contacts",s.selected_contacts},
        {"selected_steps",s.selected_steps},{"completed_contacts",s.completed_contacts},{"pending",s.pending},
        {"active",s.active},{"peak_contacts",s.peak_contacts},{"cache_sha256",s.full_cache_sha256},
        {"plan_end_song_ns",s.plan_end_song_ns},{"projected_contacts",s.projected_contacts},
        {"UI_watchdog_ns",s.ui_receipt_max_age_ns},{"anchor_max_age_ns",s.anchor_max_age_ns},
        {"max_step_lateness_ns",s.max_step_lateness_ns},{"overdue_tail_revisions_deferred",s.overdue_tail_revisions_deferred},
        {"selected_projection_free",s.selected_projection_free},{"requires_calibration",s.full_plan_requires_calibration},
        {"release_required",s.release_required},{"release_rpc_ok",s.release_rpc_ok},{"release",release_json(s.release)},
        {"timing_qualified",s.timing_qualified},{"source_pixel_age_qualified",s.source_pixel_age_qualified}};
}
Json hud_glyph_json(const HudGlyphMetrics& m) {
    return {{"foreground_error",m.foreground_mean_error},{"white_fraction",m.foreground_white_fraction},
        {"negative_bright_fraction",m.negative_space_bright_fraction},{"contrast",m.local_contrast},
        {"glyph_present",m.glyph_present},{"negative_space_present",m.negative_space_present}};
}
Json template_scores_json(const ScreenStateResult& classified) {
    Json out=Json::array();for(const auto& score:classified.scores)
        out.push_back({{"state",static_cast<int>(score.state)},{"calibration_id",score.calibration_id},
            {"worst_anchor_mean_error",score.worst_anchor_mean_error},{"all_anchors_match",score.all_anchors_match}});
    return out;
}
Json compiled_json(const CompiledContactPlan& plan) {
    Json result={{"schema","pas.experimental-whole-song-compiled.v1"},{"cache_sha256",plan.metadata.cache_sha256},
        {"compiler_version",plan.metadata.compiler_version},{"contacts",Json::array()},{"ownership",Json::array()},
        {"total_steps",plan.total_steps},{"peak_contacts",plan.peak_contacts},{"chart_sha256",plan.memory.identity.chart_sha256},
        {"requires_calibration",plan.metadata.requires_calibration},{"projection_policy",plan.metadata.projection_policy_version},
        {"max_normal_projection_reference_pixels",plan.metadata.options.max_normal_projection_reference_pixels},
        {"tap_duration_ns",plan.metadata.options.tap_duration_ns},{"drag_duration_ns",plan.metadata.options.drag_duration_ns},
        {"flick_lead_ns",plan.metadata.options.flick_lead_ns},{"flick_travel_ns",plan.metadata.options.flick_travel_ns},
        {"flick_release_ns",plan.metadata.options.flick_release_ns},{"flick_distance_reference_pixels",plan.metadata.options.flick_distance_reference_pixels},
        {"calibration_id",plan.metadata.options.calibration_id},{"input_geometry",{1280,720,0}}};
    for(const auto& contact:plan.memory.contacts) {
        Json steps=Json::array();
        for(const auto& step:contact.steps)steps.push_back({{"song_ns",step.song_ns},{"phase",static_cast<int>(step.phase)},
            {"point",{step.point.x,step.point.y}}});
        result["contacts"].push_back({{"id",contact.id},{"steps",std::move(steps)}});
    }
    for(const auto& owner:plan.ownership) {
        Json spans=Json::array();
        for(const auto& span:owner.normal_projection_spans)spans.push_back({{"begin_ns",span.begin_ns},{"end_ns",span.end_ns},
            {"correction_lower_reference_pixels",span.correction_lower_reference_pixels},
            {"correction_upper_reference_pixels",span.correction_upper_reference_pixels},
            {"possible_edge_mask",span.possible_edge_mask},{"speed_bound_input_pixels_per_ns",span.speed_bound_input_pixels_per_ns}});
        result["ownership"].push_back({{"source_note_id",owner.source_note_id},{"json_pointer",owner.source_json_pointer},
            {"contact_id",owner.contact_id},{"judgment_ns",owner.judgment_ns},{"kind",static_cast<int>(owner.kind)},
            {"projection_min_reference_pixels",owner.projection_min_reference_pixels},
            {"projection_max_reference_pixels",owner.projection_max_reference_pixels},{"normal_projection_spans",std::move(spans)}});
    }
    return result;
}
Json save_hud_timeline(const std::filesystem::path& output,const HudTimeline& timeline,const std::string& source_sha) {
    const auto pixels=timeline.bytes();
    const auto pixel_sha=chart_text_sha256({reinterpret_cast<const char*>(pixels.data()),pixels.size()});
    std::ofstream raw(output/"hud-timeline.gray",std::ios::binary);
    raw.write(reinterpret_cast<const char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
    raw.close();if(!raw)throw std::runtime_error("HUD timeline pixel write failed");
    Json rows=Json::array();
    for(const auto& sample:timeline.samples()) {
        const auto& frame=sample.frame;
        rows.push_back({{"sequence",frame.sequence},{"epoch",frame.epoch},{"generation",frame.generation},
            {"geometry_version",frame.geometry_version},{"received_ns",frame.received_ns},{"processed_ns",frame.processed_ns},
            {"source_timestamp_us",frame.source_timestamp_us?Json(*frame.source_timestamp_us):Json(nullptr)},
            {"raw_class",static_cast<int>(frame.raw_class)},{"effective_HUD",frame.effective_HUD},
            {"source_valid",frame.source_valid},{"byte_offset",sample.byte_offset}});
    }
    const auto& s=timeline.stats();
    Json stats={{"observe_calls",s.observe_calls},{"samples",s.samples},{"not_armed",s.not_armed},
        {"invalid_source",s.invalid_source},{"invalid_geometry",s.invalid_geometry},{"invalid_metadata",s.invalid_metadata},
        {"sequence_rejects",s.sequence_rejects},{"timestamp_rejects",s.timestamp_rejects},{"interval_skips",s.interval_skips},
        {"capacity_drops",s.capacity_drops},{"first_capacity_drop_received_ns",s.first_capacity_drop_received_ns},
        {"last_capacity_drop_received_ns",s.last_capacity_drop_received_ns},{"max_sample_gap_ns",s.max_sample_gap_ns}};
    Json index={{"schema","pas.experimental-hud-timeline.v1"},{"source_id",source_id},{"source_sha256",source_sha},
        {"clock_id","host-QPC-receipt-only"},{"source_timestamp_scope","raw emulator metadata; no qualified host mapping"},
        {"pixels_file","hud-timeline.gray"},{"pixels_sha256",pixel_sha},{"pixels_bytes",pixels.size()},
        {"pixel_format","uint8 grayscale (77R+150G+29B+128)>>8; row-major score then combo per sample"},
        {"frame_geometry",{1280,720}},{"score_region_xywh",{1076,20,180,40}},{"combo_region_xywh",{560,8,160,72}},
        {"sample_bytes",HudTimeline::sample_bytes},{"minimum_receipt_interval_ns",HudTimeline::sample_interval_ns},
        {"capacity",timeline.capacity()},{"allocated_pixel_bytes",timeline.allocated_pixel_bytes()},
        {"scope","actual received pixels after arm; diagnostic only, no OCR or permission renewal"},
        {"stats",stats},{"samples",std::move(rows)}};
    write_json(output/"hud-timeline.json",index);
    return {{"index_file","hud-timeline.json"},{"pixels_file","hud-timeline.gray"},{"pixels_sha256",pixel_sha},
        {"pixels_bytes",pixels.size()},{"stats",std::move(stats)}};
}
// All actual transport receipts survive an exception in the scheduler tick.
// No JSON serialization or disk I/O happens while a command is being injected.
// UP and release-all remain available when the permission guard fails.
class GuardedTouch final:public pas::TouchBackend {
    pas::TouchBackend& backend_;const pas::Clock& clock_;
public:
    std::function<std::string(const pas::TouchCommand&)> guard;
    std::vector<pas::TouchReceipt> receipts;
    std::size_t down_attempts=0,up_attempts=0;
    GuardedTouch(pas::TouchBackend& backend,const pas::Clock& clock):backend_(backend),clock_(clock) {receipts.reserve(6000);}
    pas::TouchReceipt inject(const pas::TouchCommand& command) override {
        if(receipts.size()>=6000)throw std::runtime_error("actual transport receipt budget exceeded");
        if(command.phase!=pas::Phase::up) {
            const auto reason=guard?guard(command):"dispatch guard unavailable";
            if(!reason.empty()) {
                const auto now=clock_.now_ns();pas::TouchReceipt rejected{command,now,now,false,"before_transport: "+reason};
                receipts.push_back(rejected);return rejected;
            }
        }
        if(command.phase==pas::Phase::down)++down_attempts;
        if(command.phase==pas::Phase::up)++up_attempts;
        auto receipt=backend_.inject(command);receipts.push_back(receipt);return receipt;
    }
    pas::ReleaseReport release_all() override {return backend_.release_all();}
};
int run(int argc,char** argv) {
    if((argc!=10&&argc!=12)||std::string(argv[1])!="run"||std::string(argv[9])!="--experimental-whole-song"||
       (argc==12&&std::string(argv[10])!="--experimental-hud-reference"))throw std::invalid_argument(usage);
    const std::string serial=argv[2],avd=argv[3];const int phase=phase_option(argv[8]);
    if(serial!="emulator-5554"||avd!="phigros")throw std::invalid_argument("restricted to phigros / emulator-5554");
    const auto chart_bytes=read_file(argv[4],32*1024*1024);
    ChartSource chart_source;chart_source.label=argv[4];chart_source.chart_sha256=chart_sha;
    chart_source.song_id="Dlyrotz.Likey.0";chart_source.difficulty="IN13";chart_source.game_revision="4.0.1/157";
    const auto chart=parse_phigros(chart_bytes,chart_source);
    if(chart.note_count!=584)throw std::runtime_error("whole original chart must contain exactly 584 notes");
    ContactCompileOptions compile_options;compile_options.max_contacts=4;
    compile_options.tap_duration_ns=32*ms;compile_options.drag_duration_ns=32*ms;
    compile_options.flick_release_ns=24*ms;
    compile_options.flick_lead_ns=30*ms;compile_options.flick_travel_ns=60*ms;
    compile_options.flick_distance_reference_pixels=60;
    compile_options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
    compile_options.max_normal_projection_reference_pixels=250;
    compile_options.calibration_id="dlyrotz-in13-D250-unqualified-whole-song-experiment";
    PhigrosGeometry geometry;geometry.input_width=1280;geometry.input_height=720;
    const Identity identity{Game::phigros,chart_source.song_id,chart_source.difficulty,chart_source.game_revision,
        chart_sha,audio_sha,"phigros-1280x720-r0-whole-song-experiment"};
    auto compiled=compile_phigros_contacts(chart,identity,geometry,compile_options);
    if(!compiled.plan||compiled.plan->memory.contacts.size()!=584||compiled.plan->total_steps!=expected_steps||compiled.plan->peak_contacts!=4||
       compiled.plan->metadata.cache_sha256!=expected_plan_sha||
       compiled.plan->events.empty()||compiled.plan->events.front().song_ns!=12*second||compiled.plan->events.front().phase!=Phase::down)
        throw std::runtime_error("fresh whole-chart compile did not produce pinned 584/5669/4 candidate and fingerprint");
    const auto full_plan_json=compiled_json(*compiled.plan);
    const auto screen_calibration=load_screen_state_calibration(argv[5]);
    if(screen_calibration.width!=1280||screen_calibration.height!=720)throw std::runtime_error("screen profile must be 1280x720");
    std::unique_ptr<HudGlyphEvidenceProducer> hud_features;
    if(argc==12) {
        const auto reference=read_file(argv[11],1280*720*3);
        if(reference.size()!=1280*720*3||chart_text_sha256(reference)!=hud_reference_sha)
            throw std::invalid_argument("HUD experiment requires pinned clean 1280x720 RGB reference");
        const RgbImageView view{1280,720,3840,{reinterpret_cast<const std::uint8_t*>(reference.data()),reference.size()}};
        hud_features=std::make_unique<HudGlyphEvidenceProducer>(view,hud_reference_sha,"dlyrotz-hud-glyph-v2-experimental");
    }
    const auto kernel_bytes=read_file(argv[6],8192);const Json kernel_profile=Json::parse(kernel_bytes);
    const std::filesystem::path output=std::filesystem::absolute(argv[7]);
    if(std::filesystem::exists(output))throw std::runtime_error("output must not exist");
    const auto adb=pas::find_adb();const auto avd_text=adb_text(adb,serial,{"emu","avd","name"});
    if(avd_text.substr(0,avd_text.find_first_of("\r\n"))!=avd)throw std::runtime_error("AVD mismatch");
    const auto package_report=adb_text(adb,serial,{"shell","dumpsys","package","com.PigeonGames.Phigros"});
    if(field(package_report,"versionName=")!="4.0.1"||field(package_report,"versionCode=")!="157")throw std::runtime_error("game version mismatch");
    HudTimeline hud_timeline; // Preallocate before capture, foreground ownership and READY.
    pas::HostClock clock;OwnerWait owner_wait(clock);
    const auto foreground_begin=clock.now_ns();foreground(adb,serial);const auto foreground_end=clock.now_ns();
    manual_ui::ForegroundWatch foreground_watch([adb,serial]{foreground(adb,serial);},[clock]{return clock.now_ns();},{foreground_begin,foreground_end});
    KernelSlots kernel(adb,serial,kernel_profile,clock);const auto initial_kernel=kernel.query();
    if(initial_kernel.at("active_count")!=0||initial_kernel.at("causal_host_interval_valid")!=true)throw std::runtime_error("initial kernel state not fresh zero");
    const auto endpoint=pas::discover_endpoint(serial);
    pas::CaptureOptions options;options.width=1280;options.height=720;options.source_rotation=1;options.max_relative_lag_ns=250*ms;
    pas::GrpcCapture capture(clock,endpoint,options);
    const auto initial_frame=capture.snapshot(std::chrono::milliseconds(1000));
    const auto initial_state=classify_screen_state({initial_frame.width,initial_frame.height,initial_frame.stride,initial_frame.rgb},screen_calibration.templates).state;
    if(!initial_frame.source_valid||initial_frame.width!=1280||initial_frame.height!=720||
       initial_state==ObservedGameState::playing||initial_state==ObservedGameState::paused)
        throw std::runtime_error("start from manually verified Dlyrotz IN13 selection/result; already-playing or paused is rejected");
    if(!std::filesystem::create_directory(output))throw std::runtime_error("output creation failed");
    Journal journal(output/"events.jsonl");
    OpeningVisualModel model;model.chart_sha256=chart_sha;model.model_id="experimental-opening-static-v1";
    const Json model_settings={{"id",model.model_id},{"domain_ns",{model.begin_ns,model.end_ns}},
        {"scale",{model.approach_scale_lower,model.approach_scale_upper}},{"pixel_error",model.pixel_error},
        {"pose_envelope_pixels",model.pose_tolerance_pixels},{"horizontal_visibility_margin_pixels",model.horizontal_visibility_margin_pixels},
        {"source_clock","arrival_only"},{"coverage_assumption","complete unoccluded cyan centers; conditional experiment, not a detection guarantee"}};
    model.model_sha256=chart_text_sha256(model_settings.dump());model.calibration_id="unqualified-whole-song-experiment";model.calibration_sha256=model.model_sha256;
    VisualCandidateLimits limits;limits.max_observations=256;
    std::uint64_t acquisition_epoch=1;
    VisualTimingCandidateProducer candidates(chart,model,limits);candidates.begin_epoch(acquisition_epoch);
    FirstNoteTrigger trigger(chart,model,static_cast<Ns>(phase)*ms);
    ManualConfig manual_config;manual_config.round_limit=1;manual_config.round_watchdog_ns=180*second;
    manual_config.max_observation_age_ns=ui_watchdog;manual_config.max_continuity_gap_ns=ui_watchdog;
    ManualLifecycle lifecycle(manual_config);
    const auto source_sha=chart_text_sha256("emulator-5554/phigros/1280x720/r1/"+kernel_profile.at("boot_id").get<std::string>());
    Json manifest={{"schema","pas.experimental-whole-song.v1"},{"serial",serial},{"avd",avd},{"executable_sha256",binary_sha()},
        {"chart_sha256",chart_sha},{"audio_sha256",audio_sha},{"audio_hash_scope","identity pinned from prior verified asset; no audio read/capture in this CLI"},
        {"screen_calibration_sha256",screen_calibration.dataset_sha256},{"kernel_profile_sha256",chart_text_sha256(kernel_bytes)},
        {"kernel_initial",initial_kernel},{"model_settings",model_settings},{"model_sha256",model.model_sha256},
        {"source_id",source_id},{"source_sha256",source_sha},{"experimental_phase_ms",phase},
        {"song_identity_manual_selection",true},{"initial_screen_class",static_cast<int>(initial_state)},
        {"song_selection_scope","operator selects Dlyrotz IN13 before launch; initial unknown may be selection, not automatic song identification"},
        {"phase_semantics","added to first-note visual arrival-origin rate=1 prediction; combined phase, not separated input latency"},
        {"trigger","first native Tap matched on its unique positive-speed approach with actual downward motion"},
        {"input_lead_ns",nullptr},{"input_enabled",true},{"timing_qualified",false},{"live_game_qualified",false},
        {"source_pixel_age_qualified",false},{"audio_enabled",false},{"automatic_play_enabled",false},
        {"full_chart_notes",584},{"full_compiled_steps",expected_steps},{"expected_plan_sha256",expected_plan_sha},{"contact_capacity",4},{"projection_candidate",true},
        {"experimental_tap_duration_ns",compile_options.tap_duration_ns},{"experimental_drag_duration_ns",compile_options.drag_duration_ns},
        {"experimental_flick_release_ns",compile_options.flick_release_ns},
        {"experimental_flick_lead_ns",compile_options.flick_lead_ns},{"experimental_flick_travel_ns",compile_options.flick_travel_ns},
        {"experimental_flick_distance_reference_pixels",compile_options.flick_distance_reference_pixels},
        {"flick_release_scope","hold final stroke position 24ms before UP"},
        {"flick_stroke_scope","30ms before/after native judgment;60 reference pixels;dynamic line pose retained;normal-offset zero at judgment;not a qualified game hit region"},
        {"dwell_scope","experimental main only;non-Flick contacts and explicit phase unchanged from32ms Tap/Drag candidate;compiler defaults unchanged"},
        {"HUD_timeline_policy","50ms actual receipt cadence; preallocated grayscale score/combo pixels; save after release; diagnostic only"},
        {"max_normal_projection_reference_pixels",250},{"requires_calibration",true},{"session_limit_seconds",240},
        {"round_watchdog_seconds",180},{"UI_watchdog_ns",ui_watchdog},{"anchor_receipt_max_age_ns",100*ms},
        {"UI_watchdog_scope","actual recognized playing receipt; unknown does not refresh; pause/result/source loss stop immediately; separate from fixed song timing"},
        {"experimental_HUD_fallback",static_cast<bool>(hud_features)},{"HUD_reference_sha256",hud_features?Json(hud_reference_sha):Json(nullptr)},
        {"HUD_fallback_scope","only after first-note arm, same-frame template unknown; actual pause/IN13/title glyphs with negative space; denied-state priority; receipt-only, not production qualification"},
        {"source_sequence_semantics","stream event counter, not rendered-frame ID"},
        {"owner_wait_policy","armed: nonblocking frame read; high-resolution Windows timer until min(next runtime wake, now+1ms); real lateness guard unchanged"},
        {"deadline_scope","cooperative deadlines; retained ADB termination/reader joins and OS calls have no hard wall-time guarantee"},
        {"journal_policy","64MiB/16000 UI rows and 6000 transport receipts buffered; flush after release; crash may lose pending observations"},
        {"transport",pas::grpc_transport_manifest(options.grpc_read_chunk_kib)}};
    write_json(output/"compiled-plan.json",full_plan_json);write_json(output/"plan.json",manifest);
    pas::write_diagnostic_png(output/"initial.png",initial_frame);
    SignalGuard signal_guard;pas::PixelCoordinateMap mapping(1280,720,720,1280,90);
    pas::GrpcTouch transport(clock,endpoint,mapping,720,1280,4,std::chrono::milliseconds(100));
    GuardedTouch touch(transport,clock);
    pas::LatestFrame latest(1280,720,3,&clock);std::atomic<bool> capture_done{false},stop_file{false};
    std::mutex error_mutex;std::exception_ptr capture_error;
    std::unique_ptr<ExperimentalContactRuntime> runtime;
    const auto started=clock.now_ns();auto status=lifecycle.status(started);
    Ns next_candidate=started,last_saved=-1,kernel_request=-1,first_due=-1;
    std::uint64_t consumed=0,candidate_sequence=0;
    bool armed=false,kernel_started=false,kernel_ready=false,finished=false,release_known=false,final_zero=false,sequence_complete=false;
    std::string failure;std::vector<pas::Frame> saved_frames;saved_frames.reserve(48);
    std::optional<pas::Frame> anchor_frame,result_frame,last_frame,first_unknown_frame,last_unknown_frame;
    std::size_t hud_fallback_frames=0;Json kernel_before=nullptr;std::future<Json> kernel_pending;
    std::optional<ExperimentalUiObservation> last_playing_ui;
    const auto ui_healthy=[&](Ns now){return last_playing_ui&&last_playing_ui->playing&&last_playing_ui->source_valid&&
        now>=last_playing_ui->received_host_ns&&now-last_playing_ui->received_host_ns<ui_watchdog&&foreground_watch.status().healthy;};
    const auto runtime_ui=[&](Ns now) {
        auto observation=last_playing_ui.value_or(ExperimentalUiObservation{});
        observation.playing=ui_healthy(now);return observation;
    };
    touch.guard=[&](const pas::TouchCommand& command)->std::string {
        const auto now=clock.now_ns();
        if(interrupted.load()||stop_file.load()||(GetAsyncKeyState(VK_ESCAPE)&0x8000))return "user_stop";
        if(now-started>=240*second)return "session_timeout";
        if(!ui_healthy(now))return "actual_UI_or_foreground_invalid";
        if(command.phase==pas::Phase::down&&touch.down_attempts==0&&
           (!kernel_ready||now<kernel_request||now-kernel_request>=500*ms))return "first_DOWN_kernel_zero_not_fresh";
        return {};
    };
    std::jthread stop_worker([&](std::stop_token stop){
        while(!stop.stop_requested()) {
            try{if(std::filesystem::exists(output/"STOP")){stop_file=true;break;}}
            catch(...){stop_file=true;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    std::jthread capture_worker([&](std::stop_token stop){
        try{capture.stream(stop,[&](pas::Frame&& frame){frame.generation=1;frame.geometry_version=1;latest.publish(frame.rgb.data(),frame.rgb.size(),frame);});}
        catch(...){std::lock_guard lock(error_mutex);capture_error=std::current_exception();}
        capture_done=true;latest.close();
    });
    std::cout<<"WHOLE SONG EXPERIMENT READY: manually start/retry Dlyrotz IN13 once; 584 notes / "<<expected_steps<<" steps / capacity 4.\n"<<std::flush;
    try {
        while(!finished) {
            auto now=clock.now_ns();
            if(interrupted.load()||stop_file.load()||(GetAsyncKeyState(VK_ESCAPE)&0x8000))throw std::runtime_error("user_stop");
            if(now-started>=240*second)throw std::runtime_error("session_timeout");
            if(capture_done.load()){std::lock_guard lock(error_mutex);if(capture_error)std::rethrow_exception(capture_error);throw std::runtime_error("capture_ended");}
            if(!foreground_watch.status().healthy)throw std::runtime_error("foreground_unavailable");
            status=lifecycle.poll(now);
            if(runtime&&!sequence_complete) {
                const auto current=runtime->status();
                if(current.state==ExperimentalRuntimeState::stopped)throw std::runtime_error(current.reason);
            }
            const auto frame=latest.read_after(consumed,armed&&!sequence_complete?0:10*ms);
            if(frame) {
                consumed=frame->sequence;
                last_frame=*frame;
                const auto classified=classify_screen_state({frame->width,frame->height,frame->stride,frame->rgb},screen_calibration.templates);
                Json feature_diagnostic=nullptr;bool feature_playing=false;
                if(armed&&classified.state==ObservedGameState::unknown) {
                    if(!first_unknown_frame)first_unknown_frame=*frame;
                    last_unknown_frame=*frame;
                    if(hud_features&&hud_fallback_eligible(armed,frame->source_valid,classified)) {
                        // Feature-only path has no invented per-frame SHA. This
                        // row retains the actual source/sequence/receipt; saved
                        // diagnostic pixels are hashed after all input stops.
                        const auto features=hud_features->observe_features({frame->width,frame->height,frame->stride,frame->rgb},classified.state);
                        feature_playing=features.valid&&features.hud_candidate;
                        feature_diagnostic={{"candidate",feature_playing},{"reason",features.reason},
                            {"pause",hud_glyph_json(features.fixed_glyphs[0])},{"difficulty",hud_glyph_json(features.fixed_glyphs[1])},
                            {"title",hud_glyph_json(features.title_glyph)},{"frame_hash_available",false}};
                        if(feature_playing)++hud_fallback_frames;
                    }
                }
                ManualObservation observation;observation.sequence=frame->sequence;observation.generation=frame->generation;observation.geometry_version=frame->geometry_version;
                observation.width=frame->width;observation.height=frame->height;observation.rotation=frame->source_rotation;
                observation.timestamp_kind=ManualTimestampKind::arrival_only;observation.received_host_ns=frame->capture_complete_ns;
                observation.source_valid=frame->source_valid;observation.evidence_id="whole-song/frame-"+std::to_string(frame->sequence);
                observation.pause=hud_calibration_matches(classified,ObservedGameState::paused);
                observation.result=hud_calibration_matches(classified,ObservedGameState::result);
                observation.hud=!observation.pause&&!observation.result&&(classified.state==ObservedGameState::playing||feature_playing);
                now=clock.now_ns();
                HudTimelineMeta timeline_meta;timeline_meta.sequence=frame->sequence;timeline_meta.epoch=acquisition_epoch;
                timeline_meta.generation=frame->generation;timeline_meta.geometry_version=frame->geometry_version;
                timeline_meta.received_ns=frame->capture_complete_ns;timeline_meta.processed_ns=now;
                timeline_meta.source_timestamp_us=frame->source_timestamp_us;timeline_meta.raw_class=classified.state;
                timeline_meta.effective_HUD=observation.hud;timeline_meta.source_valid=frame->source_valid;
                hud_timeline.observe({frame->width,frame->height,frame->stride,frame->rgb},timeline_meta,armed);
                status=lifecycle.observe(observation,now);
                if(observation.hud&&observation.source_valid&&now>=observation.received_host_ns&&
                   now-observation.received_host_ns<ui_watchdog) {
                    ExperimentalUiObservation actual;actual.sequence=observation.sequence;
                    actual.epoch=acquisition_epoch;actual.generation=actual.geometry_version=1;actual.width=1280;actual.height=720;
                    actual.received_host_ns=observation.received_host_ns;actual.source_id=source_id;
                    actual.evidence_id=observation.evidence_id;actual.playing=actual.source_valid=true;last_playing_ui=std::move(actual);
                } else if(observation.pause||observation.result||!observation.source_valid) {
                    last_playing_ui.reset();
                    if(armed&&!sequence_complete)throw std::runtime_error("explicit_pause_result_or_source_loss");
                }
                journal.append({{"event","ui"},{"sequence",frame->sequence},{"received_ns",frame->capture_complete_ns},{"processed_ns",now},
                    {"source_timestamp_us",frame->source_timestamp_us?Json(*frame->source_timestamp_us):Json(nullptr)},
                    {"state",manual_state_name(status.state)},{"round",status.round},{"reason",status.reason},{"source_valid",frame->source_valid},
                    {"screen_class",static_cast<int>(classified.state)},{"template_scores",template_scores_json(classified)},
                    {"experimental_HUD_features",std::move(feature_diagnostic)},
                    {"actual_playing_receipt_ns",last_playing_ui?Json(last_playing_ui->received_host_ns):Json(nullptr)}});
                if(armed) {
                    if(!sequence_complete&&observation.hud) {
                        const auto current=runtime->observe_ui(runtime_ui(clock.now_ns()));
                        if(current.state==ExperimentalRuntimeState::stopped)throw std::runtime_error(current.reason);
                    }
                    // Evidence is bounded to the opening judgment window. The
                    // full stream is classified but not copied into RAM images.
                    if(now>=first_due-200*ms&&now<=first_due+1200*ms&&saved_frames.size()<48&&
                       (last_saved<0||frame->capture_complete_ns-last_saved>=20*ms)) {
                        saved_frames.push_back(*frame);last_saved=frame->capture_complete_ns;
                    }
                    if(status.result_confirmed)result_frame=*frame;
                } else if(status.round==1&&ui_healthy(now)&&now>=next_candidate) {
                    next_candidate=now+75*ms;
                    const auto detected=calibration::detect_opening_centers(frame->width,frame->height,frame->stride,frame->rgb);
                    if(detected.valid&&!detected.centers.empty()) {
                        VisualCandidateFrame visual;visual.sequence=++candidate_sequence;visual.epoch=acquisition_epoch;visual.generation=visual.geometry_version=1;
                        visual.width=1280;visual.height=720;visual.source_id=source_id;visual.source_sha256=source_sha;
                        visual.frame_id=visual.evidence_id=observation.evidence_id;visual.frame_sha256=rgb_sha(*frame);visual.clock_id="host-QPC-receipt-only";
                        visual.opening_context_id="manual-round-1-user-selected-dlyrotz";visual.clock_kind=VisualSourceClock::arrival_only;
                        visual.received_host_ns=frame->capture_complete_ns;visual.source_valid=frame->source_valid;
                        visual.centers_complete_unoccluded=true;visual.centers=detected.centers;
                        const auto candidate_now=clock.now_ns();
                        if(candidate_now-visual.received_host_ns>=100*ms) {
                            trigger.status(candidate_now,false);journal.append({{"event","candidate_dropped_before_solver"},{"frame",visual.evidence_id},
                                {"received_ns",visual.received_host_ns},{"processed_ns",candidate_now},{"reason","receipt_expired_during_processing"}});continue;
                        }
                        const auto result=candidates.observe(visual,candidate_now);
                        const auto observed_now=clock.now_ns();const auto trigger_status=trigger.observe(result,ui_healthy(observed_now),observed_now);
                        Json centers=Json::array();for(const auto& center:detected.centers)centers.push_back({{"id",center.id},{"x",center.x},{"y",center.y}});
                        journal.append({{"event","candidate"},{"frame",visual.evidence_id},{"rgb_sha256",visual.frame_sha256},{"centers",centers},
                            {"song_intervals",intervals(result.song_intervals)},{"assignments",result.candidates.size()},{"reason",result.reason},
                            {"raw_candidates",candidate_assignments(result.candidates)},
                            {"trigger_reason",trigger_status.reason},{"support_count",trigger_status.support_count},
                            {"first_note_positive_candidates",trigger_status.first_note_positive_candidates},
                            {"selected_intervals",intervals(trigger_status.selected_intervals)},{"received_ns",visual.received_host_ns},
                            {"processed_ns",observed_now},{"timing_qualified",false}});
                        if(result.requires_new_epoch) {
                            // Before any input, discard the entire provisional
                            // history and await new actual first-note evidence.
                            // An armed runtime is never recycled this way.
                            if(acquisition_epoch>=16)throw std::runtime_error("first_note_acquisition_epoch_budget");
                            journal.append({{"event","preinput_acquisition_reset"},{"old_epoch",acquisition_epoch},{"reason",result.reason}});
                            candidates.begin_epoch(++acquisition_epoch);trigger=FirstNoteTrigger(chart,model,static_cast<Ns>(phase)*ms);
                            continue;
                        }
                        if(trigger_status.state==FirstNoteTriggerState::cancelled)
                            throw std::runtime_error("first_note_trigger_cancelled: "+trigger_status.reason);
                        if(const auto resolved=trigger.take_anchor(clock.now_ns())) {
                            ExperimentalAnchor anchor;anchor.origin_host_ns=resolved->origin_host_ns;
                            anchor.received_host_ns=resolved->received_host_ns;anchor.explicit_phase_ns=static_cast<Ns>(phase)*ms;
                            anchor.chosen_song_interval=resolved->chosen_song_interval;anchor.epoch=acquisition_epoch;anchor.generation=anchor.geometry_version=1;
                            anchor.source_id=visual.source_id;anchor.source_sha256=visual.source_sha256;anchor.clock_id=visual.clock_id;
                            anchor.evidence_id=resolved->evidence_id;anchor.supporting_evidence_ids=resolved->supporting_evidence_ids;
                            const auto construction_begin=clock.now_ns();
                            ExperimentalRuntimeOptions runtime_options;runtime_options.ui_receipt_max_age_ns=ui_watchdog;
                            runtime=std::make_unique<ExperimentalContactRuntime>(std::move(*compiled.plan),clock,touch,anchor,runtime_options);
                            const auto construction_end=clock.now_ns();
                            // The original actual receipt is intentionally retained
                            // across validation; expensive validation cannot renew it.
                            const auto initial=runtime->observe_ui(runtime_ui(clock.now_ns()));
                            if(initial.selected_contacts!=584||initial.selected_steps!=expected_steps||initial.peak_contacts!=4)
                                throw std::runtime_error("runtime must retain every full-plan contact and step");
                            if(!runtime->arm())throw std::runtime_error(runtime->status().reason);
                            armed=true;first_due=anchor.origin_host_ns+anchor.explicit_phase_ns+12*second;anchor_frame=*frame;
                            manifest["anchor"]={{"origin_host_ns",anchor.origin_host_ns},{"received_host_ns",anchor.received_host_ns},
                                {"song_interval",{anchor.chosen_song_interval.lower,anchor.chosen_song_interval.upper}},
                                {"evidence_id",anchor.evidence_id},{"supporting_evidence_ids",anchor.supporting_evidence_ids},
                                {"rgb_sha256",visual.frame_sha256},{"armed_host_ns",clock.now_ns()},
                                {"runtime_construction_begin_ns",construction_begin},{"runtime_construction_end_ns",construction_end}};
                            manifest["experimental_first_target_host_ns"]=first_due;
                            journal.append({{"event","armed_whole_song"},{"first_target_host_ns",first_due},{"runtime",runtime_json(runtime->status())}});
                            std::cout<<"FIRST NOTE ACQUIRED: chart-driven whole song armed; 584 contacts.\n"<<std::flush;
                        }
                    }
                } else if(!armed&&!ui_healthy(now))trigger.status(now,false);
            }
            now=clock.now_ns();
            if(armed&&!sequence_complete) {
                if(!ui_healthy(now))throw std::runtime_error("actual_UI_or_foreground_invalid");
                if(!kernel_started&&now>=first_due-350*ms) {
                    kernel_started=true;kernel_pending=std::async(std::launch::async,[&kernel]{return kernel.query();});
                }
                if(kernel_started&&!kernel_ready&&kernel_pending.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
                    kernel_before=kernel_pending.get();kernel_request=kernel_before.at("host_request_begin_ns").get<Ns>();
                    journal.append({{"event","kernel_pre_down"},{"snapshot",kernel_before}});
                    if(kernel_before.at("active_count")!=0||kernel_before.at("causal_host_interval_valid")!=true)
                        throw std::runtime_error("pre_down_kernel_not_fresh_zero");
                    kernel_ready=true;
                }
                const auto tick=runtime->tick();
                for(const auto& notice:tick.notices)journal.append({{"event","scheduler_notice"},{"intent_id",notice.intent_id},
                    {"host_ns",notice.monotonic_ns},{"reason",notice.reason}});
                if(tick.status.state==ExperimentalRuntimeState::stopped)throw std::runtime_error(tick.status.reason);
                if(tick.status.state==ExperimentalRuntimeState::completed) {
                    sequence_complete=true;manifest["sequence_completed_host_ns"]=clock.now_ns();
                    journal.append({{"event","whole_song_RPC_sequence_completed"},{"runtime",runtime_json(tick.status)}});
                    std::cout<<"WHOLE SONG RPC SEQUENCE COMPLETED; waiting for manual-mode result evidence.\n"<<std::flush;
                }
            }
            if(status.session_done) {
                if(sequence_complete&&status.result_confirmed)finished=true;
                else throw std::runtime_error("manual_round_ended_before_whole_song_complete");
            }
            if(!finished&&armed&&!sequence_complete) {
                const auto now_before_wait=clock.now_ns();
                const auto next=runtime->next_wake_ns();
                owner_wait.until(next?std::min(*next,now_before_wait+ms):now_before_wait+ms);
            }
        }
    }catch(const std::exception& error){failure=error.what();if(failure.empty())failure="trial_exception_without_message";}
    const auto cleanup_error=[&](const char* stage,const std::exception& error) {
        if(!failure.empty())failure+="; ";failure+=std::string(stage)+": "+error.what();
    };
    // Cleanup calls are independent: a failed runtime/release must not skip the
    // separate transport recovery or the read-only final slot query.
    if(runtime)try {runtime->stop(failure.empty()?"owner_cleanup":failure);manifest["runtime_final"]=runtime_json(runtime->status());}
        catch(const std::exception& error){cleanup_error("runtime_stop",error);}
    try {
        const auto release=transport.release_all();release_known=release.failed_ids.empty()&&release.unknown_ids.empty();
        manifest["release"]=release_json(release);
    }catch(const std::exception& error){cleanup_error("release",error);}
    bool recovery_attempted=false;
    const auto recovery_release=[&] {
        recovery_attempted=true;
        try {
            pas::GrpcTouch recovery(clock,endpoint,mapping,720,1280,4,std::chrono::milliseconds(100));
            const auto release=recovery.emergency_release_all();release_known=release.failed_ids.empty()&&release.unknown_ids.empty();
            manifest["recovery_release"]=release_json(release);
        }catch(const std::exception& error){release_known=false;cleanup_error("recovery",error);}
    };
    if(!release_known)recovery_release();
    if(kernel_pending.valid())try {manifest["pending_kernel_on_exit"]=kernel_pending.get();}
        catch(const std::exception& error){cleanup_error("pending_kernel",error);}
    try {
        const auto final=kernel.query();manifest["kernel_final"]=final;
        final_zero=final.at("active_count")==0&&final.at("causal_host_interval_valid")==true;
    }catch(const std::exception& error){cleanup_error("kernel_final",error);}
    if(!final_zero&&touch.down_attempts&&!recovery_attempted) {
        recovery_release();
        try {
            const auto final=kernel.query();manifest["kernel_after_recovery"]=final;
            final_zero=final.at("active_count")==0&&final.at("causal_host_interval_valid")==true;
        }catch(const std::exception& error){cleanup_error("kernel_after_recovery",error);}
    }
    capture_worker.request_stop();capture.cancel();latest.close();capture_worker.join();
    const auto capture_stats=capture.stats();
    manifest["capture_stats"]={{"valid",capture_stats.valid},{"inactive",capture_stats.inactive},
        {"invalid",capture_stats.invalid},{"source_gaps",capture_stats.source_gaps},
        {"relative_stale_drops",capture_stats.relative_stale_drops},{"last_notification_ns",capture_stats.last_notification_ns}};
    manifest["owner_wait"]={{"count",owner_wait.waits},{"maximum_elapsed_ns",owner_wait.maximum_elapsed_ns},
        {"maximum_overshoot_ns",owner_wait.maximum_overshoot_ns},{"scope","owner waits only, not input latency or source age"}};
    stop_worker.request_stop();stop_worker.join();
    if(!foreground_watch.stop()&&failure.empty())failure="foreground_stop_timeout";
    try {manifest["HUD_timeline"]=save_hud_timeline(output,hud_timeline,source_sha);}
        catch(const std::exception& error){cleanup_error("hud_timeline",error);}
    journal.flush_after_cleanup();
    Journal receipts(output/"receipts.jsonl");for(const auto& receipt:touch.receipts) {
        auto row=receipt_json(receipt);row["intent_id"]=receipt.command.intent_id;
        row["source_frame_sequence"]=receipt.command.source_frame_sequence;receipts.append(row);
    }
    receipts.flush_after_cleanup();
    if(anchor_frame)pas::write_diagnostic_png(output/"anchor.png",*anchor_frame);
    if(last_frame)pas::write_diagnostic_png(output/"last-observed.png",*last_frame);
    if(result_frame)pas::write_diagnostic_png(output/"result.png",*result_frame);
    Json unknown_frames=Json::array();
    const auto save_unknown=[&](const char* name,const std::optional<pas::Frame>& frame) {
        if(!frame)return;
        pas::write_diagnostic_png(output/name,*frame);
        unknown_frames.push_back({{"file",name},{"sequence",frame->sequence},{"received_host_ns",frame->capture_complete_ns},{"rgb_sha256",rgb_sha(*frame)}});
    };
    save_unknown("template-unknown-first.png",first_unknown_frame);save_unknown("template-unknown-last.png",last_unknown_frame);
    manifest["template_unknown_frames"]=unknown_frames;manifest["HUD_fallback_frames"]=hud_fallback_frames;
    Json frame_index=Json::array();std::size_t index=0;
    for(const auto& frame:saved_frames) {
        const auto name="judgment-"+std::to_string(++index)+".png";pas::write_diagnostic_png(output/name,frame);
        frame_index.push_back({{"file",name},{"sequence",frame.sequence},{"received_host_ns",frame.capture_complete_ns},{"rgb_sha256",rgb_sha(frame)}});
    }
    manifest["frames"]=frame_index;manifest["down_attempts"]=touch.down_attempts;manifest["up_attempts"]=touch.up_attempts;
    manifest["kernel_before_down"]=kernel_before;manifest["release_rpc_known"]=release_known;manifest["kernel_final_zero"]=final_zero;
    manifest["whole_song_sequence_complete"]=sequence_complete;manifest["manual_result_confirmed"]=status.result_confirmed;
    manifest["game_judgment"]="unreviewed";manifest["AP_verified"]=false;manifest["failure"]=failure.empty()?Json(nullptr):Json(failure);
    manifest["status"]=failure.empty()&&sequence_complete&&status.result_confirmed&&touch.down_attempts==584&&touch.up_attempts==584&&release_known&&final_zero?
        "experimental_whole_song_sent_judgment_unreviewed":"failed_or_stopped";
    manifest["start_host_ns"]=started;manifest["stop_host_ns"]=clock.now_ns();manifest["journal_rows"]=journal.rows;manifest["receipt_rows"]=receipts.rows;
    write_json(output/"manifest.json",manifest);std::cout<<manifest.dump(2)<<'\n';
    return manifest.at("status")=="experimental_whole_song_sent_judgment_unreviewed"?0:2;
}
} // namespace
int main(int argc,char** argv) {
    try {if(argc==2&&std::string(argv[1])=="--help"){std::cout<<usage;return 0;}return run(argc,argv);}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
