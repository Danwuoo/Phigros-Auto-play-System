#include "experimental_runtime.hpp"
#include "desktop_runtime_policy.hpp"
#include "flick_policy.hpp"
#include "catalog_source.hpp"
#include "../chart_calibrate/trial_io.hpp"
#include "../chart_calibrate/opening_detector.hpp"
#include "first_note_trigger.hpp"
#include "adaptive_first_note.hpp"
#include "adaptive_opening_detector.hpp"
#include "hud_glyph_evidence.hpp"
#include "result_label_evidence.hpp"
#include "learned_hud_redundancy.hpp"
#include "hud_timeline.hpp"
#include "image_retention.hpp"
#include "../chart_manual/foreground_watch.hpp"
#include "rhythm/manual_lifecycle.hpp"
#include "rhythm/screen_state_io.hpp"
#define NOMINMAX
#include <windows.h>
#include "owner_wait.hpp"
#include "streaming_touch.hpp"
#include "fresh_capture_recovery.hpp"
#include "numeric_hud_observer.hpp"
#include "native_head_snapshot.hpp"
#include "tap_dwell_policy.hpp"
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
constexpr Ns desktop_kernel_probe_interval=220*ms;
constexpr std::size_t dlyrotz_expected_steps=5669;
constexpr const char* dlyrotz_plan_sha="74e16603971e407627ea679a56f20300043a43c9fdf89008d9d6645d703e3ce5";
constexpr const char* dlyrotz_chart_sha="aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90";
constexpr const char* dlyrotz_audio_sha="aa9423b0f86ca4c1250f460bfbbc23d78a6405c2de739a2ba5ffef0b48261a44";
constexpr const char* source_id="live-grpc-phigros-whole-song";
constexpr const char* dlyrotz_hud_reference_sha="397431f2749bb38a845c6845977fc5c37cb2de51a79ff273dd3417ef2756e09a";
constexpr const char* usage=
    "chart_experiment run SERIAL AVD CHART.json STATE.json KERNEL.json NEW_DIR PHASE_MS --experimental-whole-song [--experimental-hud-reference CLEAN.rgb]\n"
    "chart_experiment prepare-adaptive CHART.json SOURCE-MANIFEST.json NEW_DIR [--normal-projection-pixels LIMIT] [--drag-duration-ms MS] [--input-inset-pixels PIXELS] [--contact-capacity 1..10] [--hud-policy POLICY_ID] [--import-policy POLICY_ID] [--tight-projection-speed-bounds 0|1] [--flick-lead-ms 1..100] [--flick-travel-ms 2..200] [--flick-distance-pixels 1..240] [--flick-release-ms 1..100] "
    "[--flick-path-policy dynamic-line-v1|judgment-anchored-straight-v1|judgment-anchored-screen-fit-v1|judgment-anchored-diagonal-snap-v1|judgment-anchored-diagonal-sampled-v2] [--hold-tail-policy full-pose-v1|phira-tail-grace-candidate-v1] "
    "[--offscreen-policy bounded-normal-intersection-candidate-v1|bounded-judgment-rectangle-candidate-v1] [--max-tangent-error-pixels 0..151.2] "
    "[--max-prejudge-ms 0..30] [--advance-above-drags 0|1] [--above-drag-source-ids JSON_ARRAY] [--advance-below-drags 0|1] [--below-drag-source-ids JSON_ARRAY] [--max-hold-gap-ms 0..20] [--coincident-flick-delay-ms 0|32] [--outlined-drag-heads 0|1] [--native-head-appearance-witness 0|1] [--tap-duration-ms 32|64|96]\n"
    "prepare-adaptive optionally pins the block area dispatch phase with --block-area-phase-ms -100..100 [--allow-block-area-temporal-fallback 0|1].\n"
    "chart_experiment run-adaptive SERIAL AVD CHART.json STATE.json KERNEL.json NEW_DIR PHASE_MS --experimental-whole-song --song-profile PROFILE.json --experimental-hud-reference CLEAN.rgb [--capacity-evidence EVIDENCE.json | --capacity-policy configured-ten-contact-v1]\n"
    "Use check or check-adaptive in place of run/run-adaptive for offline preflight only (no device discovery/input/output directory).\n"
    "Optional final suffix: --image-retention none|result|diagnostic (default none; result saves only a confirmed result PNG).\n"
    "Optional last suffix after image retention: --numeric-hud-observer v1 (bounded numeric metadata only; diagnostic, no input permission).\n"
    "Optional last suffix after numeric HUD: --native-head-snapshot-window-ms BEGIN:END (adaptive only; <=13000ms window inside0..600000ms; metadata shadow only).\n"
    "Optional final suffix after native head shadow: --experimental-clock-probe-ms CUT:DELTA (static empty-contact-gap suffix offset; unqualified controlled timing experiment).\n"
    "Optional final live adaptive suffix: --desktop-round-summary RUN_ID ATTEMPT_ID (compact terminal metadata; full manifest remains on disk).\n"
    "Dlyrotz IN13 complete 584-note / 5669-step candidate, capacity 4, D250 projection.\n"
    "Default experimental Tap/Drag dwell32ms; Flick lead30ms/travel60ms/distance60px/release24ms. Adaptive Flick lead must be less than travel; HUD timeline at50ms.\n"
    "Begin on its manually verified song-selection or result screen, then start/retry once. Phase -100..100 ms; -80 is the current hypothesis.\n"
    "Arrival-only rate=1 experiment; no qualified SyncLease/input latency/audio. Esc/Ctrl+C/NEW_DIR/STOP cancels.\n";
std::atomic<bool> interrupted{false};
struct DesktopRoundSummary {
    std::string run_id,attempt_id;
    std::filesystem::path output;
    bool owns_output=false,ready_seen=false,manifest_written=false;
    const char* stage="native_preflight";
    const char* failure_stage=nullptr;
    std::string failure_detail;
    Json cleanup_errors=Json::array();
    Json timing=Json::object();
    std::optional<AdaptiveFirstDispatchGate> first_note_gate;
    Ns ready_host_ns=-1,first_HUD_receipt_ns=-1;
    Json down_attempts=nullptr,up_attempts=nullptr,receipt_rows=nullptr;
    Json release_rpc_known=nullptr,kernel_final_zero=nullptr;
    const char* execution_state="not_started";
    const char* result_state="not_confirmed";
    const char* release_state="unknown";

    void cleanup_error(const char* cleanup_stage,std::string_view detail) {
        cleanup_errors.push_back({{"stage",cleanup_stage},{"code",std::string(cleanup_stage)+"_failed"},
            {"detail",std::string(detail.substr(0,384))}});
    }
};
bool desktop_id(std::string_view id) {
    return !id.empty()&&id.size()<=128&&std::all_of(id.begin(),id.end(),[](char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';
    });
}
std::optional<DesktopRoundSummary> desktop_round_summary_suffix(int& argc,char** argv) {
    if(argc<5||std::string_view(argv[argc-3])!="--desktop-round-summary")return std::nullopt;
    if(std::string_view(argv[1])!="run-adaptive"||!desktop_id(argv[argc-2])||!desktop_id(argv[argc-1]))
        throw std::invalid_argument("desktop round summary requires run-adaptive and two bounded IDs");
    DesktopRoundSummary summary;
    summary.run_id=argv[argc-2];summary.attempt_id=argv[argc-1];argc-=3;
    return summary;
}
std::string desktop_failure_code(std::string_view detail) {
    constexpr std::string_view trigger_prefix="first_note_trigger_cancelled: ";
    if(detail.starts_with(trigger_prefix))detail.remove_prefix(trigger_prefix.size());
    if(detail=="source timestamp jumped beyond 10 seconds")return "source_timestamp_jump";
    if(desktop_id(detail))return std::string(detail);
    return "native_error";
}
Json desktop_gate_json(const std::optional<AdaptiveFirstDispatchGate>& gate) {
    if(!gate)return nullptr;
    return {{"allowed",gate->allowed},{"gate_reason",gate->reason},{"origin_host_ns",gate->origin_host_ns},
        {"target_host_ns",gate->target_host_ns},{"received_host_ns",gate->received_host_ns},
        {"evaluated_host_ns",gate->evaluated_host_ns},{"minimum_lead_ns",gate->minimum_lead_ns},
        {"lead_ns",gate->target_host_ns-gate->evaluated_host_ns},{"receipt_age_ns",gate->evaluated_host_ns-gate->received_host_ns},
        {"song_interval_ns",{gate->song_interval.lower,gate->song_interval.upper}}};
}
void publish_desktop_summary(DesktopRoundSummary& desktop,int exit_code,std::string_view unhandled={}) noexcept {
    try {
        if(!unhandled.empty()){
            if(desktop.failure_stage)desktop.cleanup_error(desktop.stage,unhandled);
            else{desktop.failure_stage=desktop.stage;desktop.failure_detail=unhandled.substr(0,384);}
        }
        Json failure_stage=nullptr,failure_code=nullptr,failure_detail=nullptr;
        if(desktop.failure_stage){
            failure_stage=desktop.failure_stage;failure_code=desktop_failure_code(desktop.failure_detail);
            failure_detail=desktop.failure_detail;
        }else if(!desktop.cleanup_errors.empty()){
            failure_stage="cleanup";failure_code=desktop.cleanup_errors.front().at("code");
            failure_detail=desktop.cleanup_errors.front().at("detail");
        }
        desktop.timing["ready_host_ns"]=desktop.ready_host_ns<0?Json(nullptr):Json(desktop.ready_host_ns);
        desktop.timing["first_HUD_receipt_host_ns"]=desktop.first_HUD_receipt_ns<0?Json(nullptr):Json(desktop.first_HUD_receipt_ns);
        const Json summary={{"schema","pas.desktop-round-summary.v1"},{"run_id",desktop.run_id},{"attempt_id",desktop.attempt_id},
            {"stage",exit_code==0?Json("completed"):failure_stage.is_null()?Json(desktop.stage):failure_stage},{"exit_code",exit_code},
            {"manifest_written",desktop.manifest_written},{"ready_seen",desktop.ready_seen},
            {"execution_state",desktop.execution_state},{"result_state",desktop.result_state},
            {"release_state",desktop.release_state},{"judgment_state","unreviewed"},
            {"down_attempts",desktop.down_attempts},{"up_attempts",desktop.up_attempts},{"receipt_rows",desktop.receipt_rows},
            {"release_rpc_known",desktop.release_rpc_known},{"kernel_final_zero",desktop.kernel_final_zero},
            {"failure_stage",std::move(failure_stage)},{"failure_code",std::move(failure_code)},{"failure_detail",std::move(failure_detail)},
            {"cleanup_errors",desktop.cleanup_errors},{"first_note_gate",desktop_gate_json(desktop.first_note_gate)},
            {"timing",desktop.timing}};
        const auto line=summary.dump(-1,' ',false,Json::error_handler_t::replace);
        if(desktop.owns_output)try {
            const auto temporary=desktop.output/"desktop-summary.next.json",final=desktop.output/"desktop-summary.json";
            if(!std::filesystem::exists(temporary)&&!std::filesystem::exists(final)){
                std::ofstream stream(temporary,std::ios::binary);stream<<line<<'\n';stream.close();
                if(stream)std::filesystem::rename(temporary,final);
            }
        }catch(const std::exception&){} // The stdout record remains available.
        std::cout<<"DESKTOP ROUND SUMMARY: "<<line<<'\n'<<std::flush;
    }catch(const std::exception&){} // Optional recording cannot change the native outcome.
}
ClockProbe clock_probe_suffix(int& argc,char** argv) {
    if(argc<3||std::string_view(argv[argc-2])!="--experimental-clock-probe-ms")return {};
    const auto probe=parse_clock_probe_ms(argv[argc-1]);argc-=2;return probe;
}
Json clock_probe_proof_json(const ClockProbeProof& proof) {
    return {{"schema","pas.static-clock-probe-proof.v1"},{"enabled",proof.enabled},
        {"policy","experimental-static-piecewise-offset-probe-v1"},{"cut_song_ns",proof.cut_ns},{"suffix_delta_ns",proof.delta_ns},
        {"prefix_events",proof.prefix_events},{"suffix_events",proof.suffix_events},
        {"prefix_contacts",proof.prefix_contacts},{"suffix_contacts",proof.suffix_contacts},
        {"original_first_song_ns",proof.original_first_ns},{"original_last_song_ns",proof.original_last_ns},
        {"mapped_first_relative_ns",proof.mapped_first_ns},{"mapped_last_relative_ns",proof.mapped_last_ns},
        {"source_compiled_sha256",proof.source_cfp},{"map_identity",proof.map_identity},
        {"map_identity_sha256",chart_text_sha256(proof.map_identity)},
        {"source_chart_and_compiled_plan_unchanged",true},{"contact_internal_durations_unchanged",true},
        {"original_order_and_ties_preserved",true},{"mapping_fixed_before_READY",true},
        {"timing_qualified",false},{"source_pixel_age_qualified",false},{"global_rate_one_claim",false},
        {"scope","due=original_visual_origin+phase+original_song_ns+explicit_suffix_offset; no observation/clock updater; head-shadow prediction remains original received-origin; all input guards unchanged"}};
}
bool numeric_hud_suffix(int& argc,char** argv) {
    if(argc<3||std::string_view(argv[argc-2])!="--numeric-hud-observer")return false;
    if(std::string_view(argv[argc-1])!="v1")throw std::invalid_argument("numeric HUD observer requires explicit v1");
    argc-=2;return true;
}
std::optional<NativeHeadSnapshotWindow> native_head_snapshot_suffix(int& argc,char** argv) {
    if(argc<3||std::string_view(argv[argc-2])!="--native-head-snapshot-window-ms")return std::nullopt;
    const std::string_view text=argv[argc-1];const auto colon=text.find(':');
    if(colon==std::string_view::npos||colon==0||colon+1==text.size()||text.find(':',colon+1)!=std::string_view::npos)
        throw std::invalid_argument("native head snapshot requires explicit BEGIN:END milliseconds");
    const auto integer=[](std::string_view part){Ns value=0;const auto parsed=std::from_chars(part.data(),part.data()+part.size(),value);
        if(parsed.ec!=std::errc{}||parsed.ptr!=part.data()+part.size()||value<0||value>600000)
            throw std::invalid_argument("native head snapshot milliseconds must be0..600000");return value;};
    const auto begin=integer(text.substr(0,colon)),end=integer(text.substr(colon+1));
    if(end<=begin||end-begin>13000||(end-begin)%5!=0)
        throw std::invalid_argument("native head snapshot window must be positive, <=13000ms and a multiple of5ms");
    argc-=2;return NativeHeadSnapshotWindow{begin*ms,end*ms,5*ms};
}
struct NativeHeadShadowRow {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    std::optional<std::uint64_t> source_sequence;
    std::optional<Ns> source_timestamp_us;
    Ns received_ns=-1,processing_begin_ns=-1,processed_ns=-1,predicted_song_ns=-1,next_wake_at_admission_ns=-1;
    bool detector_valid=false;
    std::string detector_reason,error;
    std::size_t center_count=0;
    std::array<VisualCenter,8> centers{};
    NativeHeadSnapshotMatch matches;
};
struct NativeHeadShadowRows {
    std::vector<NativeHeadShadowRow> rows;
    std::size_t size=0,near_due_skips=0,cadence_skips=0,capacity_skips=0;
    Ns last_received_ns=-1,origin_host_ns=-1;
    bool disabled=false;
    NativeHeadShadowRows(){rows.resize(64);for(auto& row:rows){row.detector_reason.reserve(256);row.error.reserve(256);
        for(auto& center:row.centers)center.id.reserve(64);}}
};
const char* native_head_visibility_name(NativeHeadVisibility value) {
    switch(value){case NativeHeadVisibility::full_visible:return "full_visible";
    case NativeHeadVisibility::partial:return "partial";case NativeHeadVisibility::clipped:return "clipped";}
    return "invalid_visibility";
}
Json native_head_shadow_model_json(const NativeHeadSnapshotModel& model,std::size_t caller_pose_events) {
    return {{"model_id","native-head-grid-shadow-v1"},{"chart_sha256",model.chart_sha256()},
        {"window_ns",{model.window().begin_ns,model.window().end_ns}},{"grid_step_ns",model.window().step_ns},
        {"samples",model.samples().size()},{"sources",model.sources().size()},{"precompute_work",model.precompute_work()},
        {"owned_storage_bytes_estimate",model.storage_bytes_estimate()},{"owned_storage_limit_bytes",8*1024*1024},
        {"caller_pose_events",caller_pose_events},{"caller_pose_storage_scope","temporary evaluator copies original validated pose tracks before READY; excluded from model owned8MiB bound"},
        {"detector_roi_xywh",{0,30,1280,540}},{"full_visibility_center_roi_xyxy",{95,125,1184,474}},
        {"local_search_radius_ns",250*ms},{"match_radius_pixels",3},{"maximum_rows",64},{"minimum_receipt_spacing_ns",250*ms},
        {"maximum_match_work",262144},{"maximum_assignments_per_row",4096},{"pixels_retained",false},
        {"diagnostic_only",true},{"input_permission",false},{"source_pixel_age_qualified",false}};
}
Json native_head_shadow_row_json(const NativeHeadShadowRow& row,const NativeHeadSnapshotModel& model) {
    Json centers=Json::array(),grids=Json::array(),assignments=Json::array();
    for(std::size_t i=0;i<row.center_count;++i)centers.push_back({{"id",row.centers[i].id},{"x",row.centers[i].x},{"y",row.centers[i].y}});
    const auto& matched=row.matches;
    for(std::size_t i=0;i<matched.matched_grid_count;++i){const auto index=matched.matched_grid_indices[i];const auto& sample=model.samples().at(index);
        Json heads=Json::array();for(std::size_t h=0;h<sample.head_count;++h){const auto& head=sample.heads[h];
            heads.push_back({{"source_index",head.source_index},{"x",head.x},{"y",head.y},{"visibility",native_head_visibility_name(head.visibility)}});}
        grids.push_back({{"grid_index",index},{"song_ns",sample.song_ns},{"future_sources",sample.future_sources},
            {"outside_center_roi",sample.outside_center_roi},{"possible_heads",sample.possible_heads},
            {"full_visible_heads",sample.full_visible_heads},{"partial_heads",sample.partial_heads},{"clipped_heads",sample.clipped_heads},
            {"coincident_sources",sample.coincident_sources},{"heads",std::move(heads)}});}
    for(const auto& assignment:matched.assignments){Json sources=Json::array();
        for(std::size_t i=0;i<assignment.observed_count;++i)sources.push_back(assignment.source_indices[i]);
        assignments.push_back({{"grid_index",assignment.grid_index},{"source_indices_in_observation_order",std::move(sources)},
            {"partial_match",assignment.partial_match}});}
    return {{"sequence",row.sequence},{"epoch",row.epoch},{"generation",row.generation},{"geometry_version",row.geometry_version},
        {"source_sequence",row.source_sequence?Json(*row.source_sequence):Json(nullptr)},
        {"source_timestamp_us",row.source_timestamp_us?Json(*row.source_timestamp_us):Json(nullptr)},
        {"received_ns",row.received_ns},{"processing_begin_ns",row.processing_begin_ns},{"processed_ns",row.processed_ns},
        {"fixed_origin_prediction_song_ns",row.predicted_song_ns},{"next_wake_at_admission_ns",row.next_wake_at_admission_ns},
        {"detector_valid",row.detector_valid},{"detector_reason",row.detector_reason},{"detected_centers",std::move(centers)},
        {"valid",matched.valid},{"reason",matched.reason},{"error",row.error},{"work",matched.work},
        {"budget_exhausted",matched.budget_exhausted},{"source_ambiguity",matched.source_ambiguity},
        {"local_domain_clipped",matched.local_domain_clipped},{"local_sampled_domain_ns",{matched.local_begin_ns,matched.local_end_ns}},
        {"examined_samples",matched.examined_samples},{"incomplete_samples",matched.incomplete_samples},
        {"unobservable_samples",matched.unobservable_samples},{"matched_grids",std::move(grids)},{"assignments",std::move(assignments)}};
}
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
std::filesystem::path utf8_path(std::string_view value) {
    return std::filesystem::path(std::u8string(value.begin(),value.end()));
}
std::string read_utf8_file(const std::filesystem::path& path,std::uintmax_t limit) {
    return rhythm::calibration_trial::read_file(path,limit);
}
std::string read_utf8_file(const std::string& path,std::uintmax_t limit) {
    return read_utf8_file(utf8_path(path),limit);
}
std::string read_utf8_file(const char* path,std::uintmax_t limit) {
    return read_utf8_file(utf8_path(path),limit);
}
int phase_option(const char* input) {
    const std::string value=input;int phase=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),phase);
    if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||phase< -100||phase>100)
        throw std::invalid_argument("phase must be -100..100 ms");
    return phase;
}
std::string binary_sha() {
    wchar_t path[32768]{};const auto count=GetModuleFileNameW(nullptr,path,32768);
    if(!count||count==32768)throw std::runtime_error("binary identity unavailable");
    return chart_text_sha256(read_utf8_file(path,256*1024*1024));
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
    Json result={{"state",static_cast<int>(s.state)},{"reason",s.reason},{"selected_contacts",s.selected_contacts},
        {"selected_steps",s.selected_steps},{"completed_contacts",s.completed_contacts},{"pending",s.pending},
        {"active",s.active},{"peak_contacts",s.peak_contacts},{"cache_sha256",s.full_cache_sha256},
        {"plan_end_song_ns",s.plan_end_song_ns},{"projected_contacts",s.projected_contacts},
        {"UI_watchdog_ns",s.ui_receipt_max_age_ns},{"anchor_max_age_ns",s.anchor_max_age_ns},
        {"max_step_lateness_ns",s.max_step_lateness_ns},{"overdue_tail_revisions_deferred",s.overdue_tail_revisions_deferred},
        {"contact_capacity",s.contact_capacity},
        {"max_normal_projection_reference_pixels",s.max_normal_projection_reference_pixels},
        {"max_tangent_error_reference_pixels",s.max_tangent_error_reference_pixels},
        {"anchor_domain_ns",{s.anchor_domain_begin_ns,s.anchor_domain_end_ns}},
        {"selected_projection_free",s.selected_projection_free},{"requires_calibration",s.full_plan_requires_calibration},
        {"release_required",s.release_required},{"release_rpc_ok",s.release_rpc_ok},{"release",release_json(s.release)},
        {"timing_qualified",s.timing_qualified},{"source_pixel_age_qualified",s.source_pixel_age_qualified}};
    if(s.clock_probe.enabled) {
        result["rate_one_hypothesis"]=false;result["timing_model"]="static_piecewise_offset_probe_unqualified";
        result["experimental_clock_probe"]={{"cut_song_ns",s.clock_probe.cut_ns},{"suffix_delta_ns",s.clock_probe.delta_ns}};
    }
    return result;
}
Json owner_wait_json(const OwnerWaitDiagnostics& d) {
    const auto sample=[](const OwnerWaitSample& s){return Json{{"sequence",s.sequence},{"kind",owner_wait_kind_name(s.kind)},
        {"begin_ns",s.begin_ns},{"target_ns",s.target_ns},{"input_due_ns",s.input_due_ns},{"planned_return_ns",s.planned_return_ns},{"end_ns",s.end_ns},
        {"timer_begin_ns",s.timer_begin_ns},{"timer_end_ns",s.timer_end_ns},{"timer_request_ns",s.timer_request_ns},
        {"spin_begin_ns",s.spin_begin_ns},{"spin_end_ns",s.spin_end_ns},{"spin_iterations",s.spin_iterations},
        {"elapsed_ns",s.elapsed_ns},{"overshoot_ns",s.overshoot_ns},{"input_lateness_ns",s.input_lateness_ns},
        {"critical_deadline",s.critical_deadline},{"spin_iteration_limit_reached",s.spin_iteration_limit_reached},{"failed",s.failed}};};
    const auto& p=d.platform;Json recent=Json::array();
    for(std::size_t i=0;i<d.recent_size;++i)recent.push_back(sample(d.recent[(d.recent_next+d.recent.size()-d.recent_size+i)%d.recent.size()]));
    return {{"count",d.waits},{"calls",d.calls},{"timer_count",d.timer_count},{"spin_count",d.spin_count},{"no_wait_count",d.no_wait_count},
        {"failed_count",d.failed_count},{"spin_iterations",d.spin_iterations},{"spin_elapsed_ns",d.spin_elapsed_ns},
        {"spin_iteration_limit_reached",d.spin_iteration_limit_reached},{"maximum_elapsed_ns",d.maximum_elapsed_ns},
        {"maximum_overshoot_ns",d.maximum_overshoot_ns},{"last",sample(d.last)},{"last_timer",sample(d.last_timer)},
        {"last_spin",sample(d.last_spin)},{"maximum_elapsed",sample(d.maximum_elapsed)},{"maximum_overshoot",sample(d.maximum_overshoot)},
        {"recent",std::move(recent)},
        {"options",{{"poll_limit_ns",d.options.poll_limit_ns},{"critical_guard_ns",d.options.critical_guard_ns},
            {"spin_slice_ns",d.options.spin_slice_ns},{"spin_iteration_limit",d.options.spin_iteration_limit}}},
        {"platform",{{"owner_thread_id",p.owner_thread_id},{"original_thread_priority",p.original_thread_priority},
            {"applied_thread_priority",p.applied_thread_priority},{"priority_requested",p.priority_requested},{"priority_changed",p.priority_changed},
            {"priority_restore_attempted",p.priority_restore_attempted},{"priority_restored",p.priority_restored},
            {"priority_error",p.priority_error},{"priority_restore_error",p.priority_restore_error},{"high_resolution_timer",p.high_resolution_timer},
            {"timer_resolution_requested",p.timer_resolution_requested},{"timer_resolution_active",p.timer_resolution_active},
            {"timer_resolution_ms",p.timer_resolution_ms},{"timer_resolution_begin_result",p.timer_resolution_begin_result},
            {"timer_resolution_load_error",p.timer_resolution_load_error},{"timer_resolution_end_attempted",p.timer_resolution_end_attempted},
            {"timer_resolution_end_result",p.timer_resolution_end_result},{"finished",p.finished}}},
        {"scope","actual owner waits with bounded timing history; OS preemption remains possible; input lateness and actual UI guards are unchanged"}};
}
Json hud_glyph_json(const HudGlyphMetrics& m) {
    return {{"foreground_error",m.foreground_mean_error},{"white_fraction",m.foreground_white_fraction},
        {"negative_bright_fraction",m.negative_space_bright_fraction},{"contrast",m.local_contrast},
        {"glyph_present",m.glyph_present},{"negative_space_present",m.negative_space_present},
        {"subpixel_tolerant",m.subpixel_tolerant},{"reference_gray_error",m.reference_gray_error},
        {"unexpected_white_fraction",m.unexpected_white_fraction},{"observed_ink_ratio",m.observed_ink_ratio},
        {"literal_components",m.literal_components}};
}
Json pixel_region_json(const std::optional<PixelRegion>& region) {
    if(!region)return nullptr;
    return Json::array({region->x,region->y,region->width,region->height});
}
Json template_scores_json(const ScreenStateResult& classified) {
    Json out=Json::array();for(const auto& score:classified.scores)
        out.push_back({{"state",static_cast<int>(score.state)},{"calibration_id",score.calibration_id},
            {"worst_anchor_mean_error",score.worst_anchor_mean_error},{"all_anchors_match",score.all_anchors_match}});
    return out;
}
void add_block_area_evidence(Json& json,const ContactCompileMetadata& metadata) {
    if(metadata.block_area_policy_id.empty())return;
    json["block_area"]={{"policy_id",metadata.block_area_policy_id},{"active_regions",metadata.block_area_count},
        {"detoured_contacts",metadata.block_area_detoured_contacts},
        {"phase_ns",metadata.options.block_area_phase_ns?Json(*metadata.options.block_area_phase_ns):Json(nullptr)},
        {"temporal_fallback_enabled",metadata.options.allow_block_area_temporal_fallback},
        {"temporal_fallback_contacts",metadata.block_area_temporal_contacts}};
}
Json compiled_json(const CompiledContactPlan& plan) {
    Json result={{"schema","pas.experimental-whole-song-compiled.v1"},{"cache_sha256",plan.metadata.cache_sha256},
        {"compiler_version",plan.metadata.compiler_version},
        {"import_policy_id",plan.metadata.import_policy_id},{"import_interpretation_unqualified",plan.metadata.import_interpretation_unqualified},
        {"import_anomaly_source_ids",plan.metadata.import_anomaly_source_ids},
        {"tight_projection_speed_bounds",plan.metadata.options.tight_projection_speed_bounds},{"contacts",Json::array()},{"ownership",Json::array()},
        {"total_steps",plan.total_steps},{"peak_contacts",plan.peak_contacts},{"chart_sha256",plan.memory.identity.chart_sha256},
        {"requires_calibration",plan.metadata.requires_calibration},{"projection_policy",plan.metadata.projection_policy_version},
        {"max_normal_projection_reference_pixels",plan.metadata.options.max_normal_projection_reference_pixels},
        {"normal_projection_inset_reference_pixels",plan.metadata.options.normal_projection_inset_reference_pixels},
        {"tap_duration_ns",plan.metadata.options.tap_duration_ns},{"drag_duration_ns",plan.metadata.options.drag_duration_ns},
        {"flick_lead_ns",plan.metadata.options.flick_lead_ns},{"flick_travel_ns",plan.metadata.options.flick_travel_ns},
        {"flick_release_ns",plan.metadata.options.flick_release_ns},{"flick_distance_reference_pixels",plan.metadata.options.flick_distance_reference_pixels},
        {"flick_path_policy",flick_path_policy_name(plan.metadata.options.flick_path_policy)},
        {"calibration_id",plan.metadata.options.calibration_id},{"input_geometry",{1280,720,0}}};
    add_block_area_evidence(result,plan.metadata);
    if(plan.metadata.source_format_version!=3)result["source_format_version"]=plan.metadata.source_format_version;
    if(plan.metadata.options.hold_tail_policy!=HoldTailPolicy::full_pose_v1){
        result["hold_tail_policy_id"]=hold_tail_policy_name(plan.metadata.options.hold_tail_policy);
    }
    if(!plan.metadata.judgment_policy_provenance.empty())result["judgment_policy_provenance"]=plan.metadata.judgment_policy_provenance;
    if(plan.metadata.options.max_prejudge_ns>0){
        result["max_prejudge_ns"]=plan.metadata.options.max_prejudge_ns;
        result["advance_above_drags"]=plan.metadata.options.advance_above_drags;
    }
    if(plan.metadata.options.max_hold_reacquire_gap_ns>0)result["max_hold_reacquire_gap_ns"]=plan.metadata.options.max_hold_reacquire_gap_ns;
    if(plan.metadata.options.coincident_flick_delay_ns>0)result["coincident_flick_delay_ns"]=plan.metadata.options.coincident_flick_delay_ns;
    if(plan.metadata.options.advance_below_drags)result["advance_below_drags"]=true;
    if(!plan.metadata.options.below_drag_source_ids.empty())result["below_drag_source_ids"]=plan.metadata.options.below_drag_source_ids;
    if(!plan.metadata.options.above_drag_source_ids.empty())result["above_drag_source_ids"]=plan.metadata.options.above_drag_source_ids;
    const bool rectangle=plan.metadata.options.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
    if(rectangle){
        result["offscreen_policy_id"]=offscreen_policy_name(plan.metadata.options.offscreen_policy);
        result["max_tangent_error_reference_pixels"]=plan.metadata.options.max_tangent_error_reference_pixels;
    }
    for(const auto& contact:plan.memory.contacts) {
        Json steps=Json::array();
        for(const auto& step:contact.steps)steps.push_back({{"song_ns",step.song_ns},{"phase",static_cast<int>(step.phase)},
            {"point",{step.point.x,step.point.y}}});
        result["contacts"].push_back({{"id",contact.id},{"steps",std::move(steps)}});
    }
    for(const auto& owner:plan.ownership) {
        Json spans=Json::array();
        for(const auto& span:owner.normal_projection_spans){spans.push_back({{"begin_ns",span.begin_ns},{"end_ns",span.end_ns},
            {"correction_lower_reference_pixels",span.correction_lower_reference_pixels},
            {"correction_upper_reference_pixels",span.correction_upper_reference_pixels},
            {"possible_edge_mask",span.possible_edge_mask},{"speed_bound_input_pixels_per_ns",span.speed_bound_input_pixels_per_ns}});
            if(rectangle){
                spans.back()["tangent_correction_lower_reference_pixels"]=span.tangent_correction_lower_reference_pixels;
                spans.back()["tangent_correction_upper_reference_pixels"]=span.tangent_correction_upper_reference_pixels;
                if(span.judgment_domain_gap)spans.back()["judgment_domain_gap"]=true;
            }
        }
        result["ownership"].push_back({{"source_note_id",owner.source_note_id},{"json_pointer",owner.source_json_pointer},
            {"contact_id",owner.contact_id},{"judgment_ns",owner.judgment_ns},{"kind",static_cast<int>(owner.kind)},
            {"projection_min_reference_pixels",owner.projection_min_reference_pixels},
            {"projection_max_reference_pixels",owner.projection_max_reference_pixels},{"normal_projection_spans",std::move(spans)}});
        if(owner.hold_tracking_end_ns>=0)result["ownership"].back()["hold_tracking_end_ns"]=owner.hold_tracking_end_ns;
        if(owner.block_area_translation_reference)result["ownership"].back()["block_area_translation_reference"]={
            owner.block_area_translation_reference->x,owner.block_area_translation_reference->y};
        if(owner.block_area_temporal_fallback)result["ownership"].back()["block_area_temporal_fallback"]=true;
        if(owner.flick_anchor_reference)result["ownership"].back()["flick_anchor_reference"]={owner.flick_anchor_reference->x,owner.flick_anchor_reference->y};
        if(owner.flick_direction_reference)result["ownership"].back()["flick_direction_reference"]={owner.flick_direction_reference->x,owner.flick_direction_reference->y};
        if(owner.flick_discrete_path){
            const auto& path=*owner.flick_discrete_path;
            result["ownership"].back()["flick_discrete_path"]={{"model_id",path.model_id},
                {"bend_reference",{path.bend_reference.x,path.bend_reference.y}},{"end_reference",{path.end_reference.x,path.end_reference.y}},
                {"bend_ns",path.bend_ns},{"end_ns",path.end_ns},
                {"endpoint_distance_reference_pixels",path.endpoint_distance_reference_pixels},
                {"polyline_length_reference_pixels",path.polyline_length_reference_pixels},
                {"max_jump_input_pixels",path.max_jump_input_pixels},{"move_count",path.move_count}};
            if(path.middle){
                result["ownership"].back()["flick_discrete_path"]["middle_reference"]={path.middle->reference.x,path.middle->reference.y};
                result["ownership"].back()["flick_discrete_path"]["middle_ns"]=path.middle->song_ns;
            }
        }
        if(owner.dispatch_judgment_ns>=0)result["ownership"].back()["dispatch_judgment_ns"]=owner.dispatch_judgment_ns;
        if(rectangle){
            result["ownership"].back()["tangent_projection_min_reference_pixels"]=owner.tangent_projection_min_reference_pixels;
            result["ownership"].back()["tangent_projection_max_reference_pixels"]=owner.tangent_projection_max_reference_pixels;
            if(plan.metadata.options.max_hold_reacquire_gap_ns>0){
                result["ownership"].back()["gap_tangent_projection_min_reference_pixels"]=owner.gap_tangent_projection_min_reference_pixels;
                result["ownership"].back()["gap_tangent_projection_max_reference_pixels"]=owner.gap_tangent_projection_max_reference_pixels;
            }
        }
    }
    return result;
}
Json save_hud_timeline(const std::filesystem::path& output,const HudTimeline& timeline,const std::string& source_sha) {
    const auto pixels=timeline.bytes();
    const bool retain_pixels=timeline.pixels_retained();
    Json pixel_sha=nullptr;
    if(retain_pixels) {
        pixel_sha=chart_text_sha256({reinterpret_cast<const char*>(pixels.data()),pixels.size()});
        std::ofstream raw(output/"hud-timeline.gray",std::ios::binary);
        raw.write(reinterpret_cast<const char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
        raw.close();if(!raw)throw std::runtime_error("HUD timeline pixel write failed");
    }
    Json rows=Json::array();
    for(const auto& sample:timeline.samples()) {
        const auto& frame=sample.frame;
        rows.push_back({{"sequence",frame.sequence},{"epoch",frame.epoch},{"generation",frame.generation},
            {"geometry_version",frame.geometry_version},{"received_ns",frame.received_ns},{"processed_ns",frame.processed_ns},
            {"source_timestamp_us",frame.source_timestamp_us?Json(*frame.source_timestamp_us):Json(nullptr)},
            {"raw_class",static_cast<int>(frame.raw_class)},{"effective_HUD",frame.effective_HUD},
            {"source_valid",frame.source_valid},{"byte_offset",retain_pixels?Json(sample.byte_offset):Json(nullptr)}});
    }
    const auto& s=timeline.stats();
    Json stats={{"observe_calls",s.observe_calls},{"samples",s.samples},{"not_armed",s.not_armed},
        {"invalid_source",s.invalid_source},{"invalid_geometry",s.invalid_geometry},{"invalid_metadata",s.invalid_metadata},
        {"sequence_rejects",s.sequence_rejects},{"timestamp_rejects",s.timestamp_rejects},{"interval_skips",s.interval_skips},
        {"capacity_drops",s.capacity_drops},{"first_capacity_drop_received_ns",s.first_capacity_drop_received_ns},
        {"last_capacity_drop_received_ns",s.last_capacity_drop_received_ns},{"max_sample_gap_ns",s.max_sample_gap_ns}};
    Json index={{"schema",retain_pixels?"pas.experimental-hud-timeline.v1":"pas.experimental-hud-timeline-metadata.v1"},{"source_id",source_id},{"source_sha256",source_sha},
        {"clock_id","host-QPC-receipt-only"},{"source_timestamp_scope","raw emulator metadata; no qualified host mapping"},
        {"pixels_retained",retain_pixels},{"pixels_file",retain_pixels?Json("hud-timeline.gray"):Json(nullptr)},{"pixels_sha256",pixel_sha},{"pixels_bytes",pixels.size()},
        {"pixel_format","uint8 grayscale (77R+150G+29B+128)>>8; row-major score then combo per sample"},
        {"frame_geometry",{1280,720}},{"score_region_xywh",{1076,20,180,40}},{"combo_region_xywh",{560,8,160,72}},
        {"sample_bytes",HudTimeline::sample_bytes},{"minimum_receipt_interval_ns",HudTimeline::sample_interval_ns},
        {"capacity",timeline.capacity()},{"allocated_pixel_bytes",timeline.allocated_pixel_bytes()},
        {"scope",retain_pixels?"actual received pixels after arm; diagnostic only, no OCR or permission renewal":"actual receipt metadata only; images not retained; no OCR or permission renewal"},
        {"stats",stats},{"samples",std::move(rows)}};
    write_json(output/"hud-timeline.json",index);
    return {{"index_file","hud-timeline.json"},{"pixels_retained",retain_pixels},{"pixels_file",retain_pixels?Json("hud-timeline.gray"):Json(nullptr)},{"pixels_sha256",pixel_sha},
        {"pixels_bytes",pixels.size()},{"stats",std::move(stats)}};
}
// All actual transport receipts survive an exception in the scheduler tick.
// No JSON serialization or disk I/O happens while a command is being injected.
// UP and release-all remain available when the permission guard fails.
struct TouchMessageAdmission {
    pas::TouchCommand command{};Ns check_begin_ns=0,check_return_ns=0,received_ns=0;
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    bool allowed=false,playing=false,source_valid=false,foreground_healthy=false,source_matches=false;
    std::string reason;
};
class GuardedTouch final:public pas::TouchBackend {
    pas::TouchBackend& backend_;const pas::Clock& clock_;
    std::size_t receipt_budget_;
public:
    std::function<std::string(const pas::TouchCommand&)> guard;
    std::vector<pas::TouchReceipt> receipts;
    std::size_t down_attempts=0,up_attempts=0;
    GuardedTouch(pas::TouchBackend& backend,const pas::Clock& clock,std::size_t receipt_budget)
      :backend_(backend),clock_(clock),receipt_budget_(receipt_budget) {
        if(receipt_budget<1||receipt_budget>65536)throw std::invalid_argument("transport receipt budget 1..65536");
        receipts.reserve(receipt_budget);
    }
    pas::TouchReceipt inject(const pas::TouchCommand& command) override {
        if(receipts.size()>=receipt_budget_)throw std::runtime_error("actual transport receipt budget exceeded");
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
struct SongProfile {
    std::string chart_sha=dlyrotz_chart_sha,audio_sha=dlyrotz_audio_sha;
    std::string song="Dlyrotz.Likey.0",difficulty="IN13",revision="4.0.1/157";
    std::string calibration_id="dlyrotz-in13-D250-unqualified-whole-song-experiment";
    std::string plan_sha=dlyrotz_plan_sha,opening_sha,profile_sha;
    std::string hud_policy="calibrated-title-v1";
    std::string import_policy="phigros-v3-strict-v1";
    bool tight_projection_speed_bounds=false;
    FlickParameters flick;
    HoldTailPolicy hold_tail_policy=HoldTailPolicy::full_pose_v1;
    OffscreenPolicy offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
    double max_tangent_error_reference_pixels=0;
    std::optional<Ns> block_area_phase_ns;
    bool allow_block_area_temporal_fallback=false;
    Ns max_prejudge_ns=0,max_hold_reacquire_gap_ns=0;
    bool advance_above_drags=false;
    std::string installed_source_manifest;
    std::size_t notes=584,steps=dlyrotz_expected_steps;
    std::size_t runtime_step_budget=16000;
    std::size_t compiler_diagnostic_budget=2048;
    int peak=4,capacity=4,projection_pixels=250,input_inset_pixels=0;
    Ns first_down=12*second,drag_duration_ns=32*ms,tap_duration_ns=tap_dwell_default_ns;
    Ns runtime_duration_limit_ns=180*second;
    Ns coincident_flick_delay_ns=0;
    bool advance_below_drags=false;
    std::vector<std::string> below_drag_source_ids;
    std::vector<std::string> above_drag_source_ids;
    bool outlined_drag_heads=false;
    bool native_head_appearance_witness=false;
};
Json native_head_appearance_witness_json() {
    return {{"policy_id","native-certified-head-pop-in-v1"},
        {"source_rule","original chart, note identities and required native earliest group retained; constrain native head appearance without rewriting or deleting source notes"},
        {"scope","explicit arrival-only rendering hypothesis; not a qualified sample clock, render age or game touch callback"}};
}
OffscreenPolicy song_offscreen_policy(std::string_view name) {
    for(const auto policy:{OffscreenPolicy::reject,OffscreenPolicy::bounded_normal_candidate,
        OffscreenPolicy::bounded_normal_intersection_candidate_v1,OffscreenPolicy::bounded_judgment_rectangle_candidate_v1})
        if(name==offscreen_policy_name(policy))return policy;
    throw std::invalid_argument("unknown explicit offscreen judgment policy");
}
double tangent_ceiling(const Json& value) {
    if(!value.is_number())throw std::invalid_argument("tangent error ceiling must be a finite number");
    const auto bound=value.get<double>();
    if(!std::isfinite(bound)||bound<0||bound>151.2)throw std::invalid_argument("tangent error ceiling must be finite 0..151.2 reference pixels");
    return bound;
}
std::vector<std::string> below_drag_source_ids(const Json& value) {
    if(!value.is_array()||value.size()>2048)throw std::invalid_argument("below_drag_source_ids requires at most2048 exact source IDs");
    std::vector<std::string> result;result.reserve(value.size());
    for(const auto& item:value){
        if(!item.is_string())throw std::invalid_argument("below_drag_source_ids requires strings");
        auto id=item.get<std::string>();
        if(id.empty()||id.size()>512)throw std::invalid_argument("below Drag source ID length outside1..512");
        result.push_back(std::move(id));
    }
    std::sort(result.begin(),result.end());
    if(std::adjacent_find(result.begin(),result.end())!=result.end())throw std::invalid_argument("duplicate below Drag source ID");
    return result;
}
std::vector<std::string> above_drag_source_ids(const Json& value) {
    if(!value.is_array()||value.size()>2048)throw std::invalid_argument("above_drag_source_ids requires at most2048 exact source IDs");
    std::vector<std::string> result;result.reserve(value.size());
    for(const auto& item:value){
        if(!item.is_string())throw std::invalid_argument("above_drag_source_ids requires strings");
        auto id=item.get<std::string>();
        if(id.empty()||id.size()>512)throw std::invalid_argument("above Drag source ID length outside1..512");
        result.push_back(std::move(id));
    }
    std::sort(result.begin(),result.end());
    if(std::adjacent_find(result.begin(),result.end())!=result.end())throw std::invalid_argument("duplicate above Drag source ID");
    return result;
}
Ns coincident_flick_delay(const Json& value) {
    const auto delay=kernel_integer(value,0,32*ms);
    if(delay!=0&&delay!=32*ms)throw std::invalid_argument("coincident Flick delay must be exactly 0 or 32ms");
    return delay;
}
ContactCompileOptions song_compile_options(const SongProfile& song) {
    ContactCompileOptions options;options.max_contacts=song.capacity;
    options.max_diagnostics=song.compiler_diagnostic_budget;
    validate_tap_dwell_ns(song.tap_duration_ns);
    options.tap_duration_ns=song.tap_duration_ns;options.drag_duration_ns=song.drag_duration_ns;
    apply_flick_parameters(options,song.flick);
    options.hold_tail_policy=song.hold_tail_policy;
    options.offscreen_policy=song.offscreen_policy;
    options.max_tangent_error_reference_pixels=song.max_tangent_error_reference_pixels;
    options.block_area_phase_ns=song.block_area_phase_ns;
    options.allow_block_area_temporal_fallback=song.allow_block_area_temporal_fallback;
    options.max_prejudge_ns=song.max_prejudge_ns;
    options.advance_above_drags=song.advance_above_drags;
    options.max_hold_reacquire_gap_ns=song.max_hold_reacquire_gap_ns;
    options.coincident_flick_delay_ns=song.coincident_flick_delay_ns;
    options.advance_below_drags=song.advance_below_drags;
    options.below_drag_source_ids=song.below_drag_source_ids;
    options.above_drag_source_ids=song.above_drag_source_ids;
    options.max_normal_projection_reference_pixels=song.projection_pixels;options.calibration_id=song.calibration_id;
    options.normal_projection_inset_reference_pixels=song.input_inset_pixels;
    options.tight_projection_speed_bounds=song.tight_projection_speed_bounds;
    return options;
}
ImportOptions song_import_options(const SongProfile& song) {
    ImportOptions options;
    if(song.import_policy==phigros_import_policy_id(PhigrosImportPolicy::strict_v3))
        options.policy=PhigrosImportPolicy::strict_v3;
    else if(song.import_policy==phigros_import_policy_id(PhigrosImportPolicy::source_priority_constant_empty_candidate_v1))
        options.policy=PhigrosImportPolicy::source_priority_constant_empty_candidate_v1;
    else if(song.import_policy==phigros_import_policy_id(PhigrosImportPolicy::native_v1_source_priority_constant_empty_candidate_v1))
        options.policy=PhigrosImportPolicy::native_v1_source_priority_constant_empty_candidate_v1;
    else if(song.import_policy==phigros_import_policy_id(PhigrosImportPolicy::native_v1_source_priority_empty_interval_candidate_v1))
        options.policy=PhigrosImportPolicy::native_v1_source_priority_empty_interval_candidate_v1;
    else throw std::invalid_argument("unsupported explicit Phigros import policy");
    return options;
}
Json import_json(const SemanticChart& chart,bool include_anomalies=true) {
    if(!include_anomalies)return {{"policy_id",chart.import_policy_id},{"interpretation_unqualified",chart.import_interpretation_unqualified},
        {"source_chart_sha256",chart.source.chart_sha256},{"source_format_version",chart.source_format_version},
        {"normalized_format_version",chart.format_version},{"anomaly_count",chart.import_anomalies.size()},
        {"anomalies_included",false},{"detail_scope","Full anomaly inventory remains in the immutable preparation report.json"}};
    Json anomalies=Json::array();
    for(const auto& anomaly:chart.import_anomalies) {
        Json evaluated=Json::array();
        for(const auto& interval:anomaly.evaluated_segments)evaluated.push_back({
            {"native_ticks",{interval.start_tick,interval.end_tick}},{"song_ns",{interval.start_ns,interval.end_ns}}});
        anomalies.push_back({
            {"source_id",anomaly.source.id},{"json_pointer",anomaly.source.json_pointer},
            {"code",anomaly.code},{"reason",anomaly.reason},{"event_track",anomaly.event_track},
            {"line_index",anomaly.line_index},{"declaration_index",anomaly.declaration_index},
            {"declared_interval_ns",{anomaly.declared_time.start_ns,anomaly.declared_time.end_ns}},
            {"original_declaration_json",anomaly.original_declaration_json},{"declaration_evaluated",anomaly.declaration_evaluated},
            {"evaluated_segments",std::move(evaluated)}});
    }
    return {{"policy_id",chart.import_policy_id},{"interpretation_unqualified",chart.import_interpretation_unqualified},
        {"source_chart_sha256",chart.source.chart_sha256},{"source_format_version",chart.source_format_version},
        {"normalized_format_version",chart.format_version},{"anomalies",std::move(anomalies)}};
}
Identity song_identity(const SongProfile& song) {
    return {Game::phigros,song.song,song.difficulty,song.revision,song.chart_sha,song.audio_sha,
        "phigros-1280x720-r0-whole-song-experiment"};
}
SongProfile load_song_profile(const std::filesystem::path& path) {
    const auto bytes=read_utf8_file(path,32768);const auto value=Json::parse(bytes);
    if(value.at("schema")!="pas.adaptive-song-profile.v1")throw std::invalid_argument("song profile schema");
    SongProfile song;song.profile_sha=chart_text_sha256(bytes);
    song.chart_sha=value.at("chart_sha256");song.audio_sha=value.at("audio_sha256");
    song.song=value.at("song_id");song.difficulty=value.at("difficulty");song.revision=value.at("revision");
    song.calibration_id=value.at("calibration_id");song.plan_sha=value.at("expected_plan_sha256");
    song.opening_sha=value.at("opening_model_sha256");
    song.hud_policy=value.value("hud_policy",std::string("calibrated-title-v1"));
    song.import_policy=value.value("import_policy_id",std::string("phigros-v3-strict-v1"));
    song.tight_projection_speed_bounds=value.value("tight_projection_speed_bounds",false);
    song.flick=flick_parameters_from_profile(value);
    song.tap_duration_ns=tap_dwell_ns_from_profile(value);
    song.offscreen_policy=song_offscreen_policy(value.value("offscreen_policy_id",std::string("bounded-normal-intersection-candidate-v1")));
    song.max_tangent_error_reference_pixels=tangent_ceiling(value.value("max_tangent_error_reference_pixels",Json(0)));
    if(value.contains("block_area_phase_ms"))song.block_area_phase_ns=kernel_integer(value.at("block_area_phase_ms"),-100,100)*ms;
    if(value.contains("allow_block_area_temporal_fallback")) {
        const auto& fallback=value.at("allow_block_area_temporal_fallback");
        if(!fallback.is_boolean())throw std::invalid_argument("block area temporal fallback must be a boolean");
        song.allow_block_area_temporal_fallback=fallback.get<bool>();
    }
    song.max_prejudge_ns=kernel_integer(value.value("max_prejudge_ns",Json(0)),0,30*ms);
    song.max_hold_reacquire_gap_ns=kernel_integer(value.value("max_hold_reacquire_gap_ns",Json(0)),0,20*ms);
    song.coincident_flick_delay_ns=coincident_flick_delay(value.value("coincident_flick_delay_ns",Json(0)));
    const auto above_drags=value.value("advance_above_drags",Json(false));
    if(!above_drags.is_boolean())throw std::invalid_argument("advance_above_drags must be an explicit boolean");
    song.advance_above_drags=above_drags.get<bool>();
    const auto below_drags=value.value("advance_below_drags",Json(false));
    if(!below_drags.is_boolean())throw std::invalid_argument("advance_below_drags must be an explicit boolean");
    song.advance_below_drags=below_drags.get<bool>();
    song.below_drag_source_ids=below_drag_source_ids(value.value("below_drag_source_ids",Json::array()));
    song.above_drag_source_ids=above_drag_source_ids(value.value("above_drag_source_ids",Json::array()));
    const auto outlined=value.value("outlined_drag_heads",Json(false));
    if(!outlined.is_boolean())throw std::invalid_argument("outlined_drag_heads must be an explicit boolean");
    song.outlined_drag_heads=outlined.get<bool>();
    const auto appearance=value.value("native_head_appearance_witness",Json(false));
    if(!appearance.is_boolean())throw std::invalid_argument("native_head_appearance_witness must be an explicit boolean");
    song.native_head_appearance_witness=appearance.get<bool>();
    const auto hold_policy=value.value("hold_tail_policy_id",std::string("full-pose-v1"));
    if(hold_policy=="full-pose-v1")song.hold_tail_policy=HoldTailPolicy::full_pose_v1;
    else if(hold_policy=="phira-tail-grace-candidate-v1")song.hold_tail_policy=HoldTailPolicy::phira_tail_grace_candidate_v1;
    else throw std::invalid_argument("unknown Hold tail policy");
    (void)song_import_options(song);
    if(!valid_adaptive_hud_policy(song.hud_policy))
        throw std::invalid_argument("unsupported explicit adaptive HUD policy");
    if(!hud_policy_matches_difficulty(song.hud_policy,song.difficulty))
        throw std::invalid_argument("HUD policy does not match selected difficulty");
    song.installed_source_manifest=value.value("installed_source_manifest",std::string());
    if(!song.installed_source_manifest.empty()) {
        const auto raw=read_utf8_file(song.installed_source_manifest,1024*1024);const auto source=Json::parse(raw);
        if(chart_text_sha256(raw)!=value.at("source_manifest_sha256").get<std::string>()||source.at("track_id")!=song.song||
           source.at("difficulty")!=song.difficulty||source.at("chart").at("sha256")!=song.chart_sha||
           installed_catalog_audio_identity(source)!=song.audio_sha)
            throw std::invalid_argument("installed catalog source/profile identity differs");
    }
    for(const auto* hash:{&song.chart_sha,&song.audio_sha,&song.plan_sha,&song.opening_sha})
        if(!sha256_text(*hash))throw std::invalid_argument("invalid song profile hash");
    for(const auto* name:{&song.song,&song.difficulty,&song.calibration_id})
        if(name->empty()||name->size()>256||name->find_first_of("\r\n\0",0,3)!=std::string::npos)
            throw std::invalid_argument("invalid song profile label");
    if(song.revision!="4.0.1/157")throw std::invalid_argument("song revision unsupported");
    song.runtime_step_budget=static_cast<std::size_t>(kernel_integer(value.value("runtime_step_budget",Json(16000)),2,65536));
    song.compiler_diagnostic_budget=static_cast<std::size_t>(kernel_integer(value.value("compiler_diagnostic_budget",Json(2048)),1,65536));
    song.runtime_duration_limit_ns=kernel_integer(value.value("runtime_duration_limit_ns",Json(180*second)),second,600*second);
    song.notes=static_cast<std::size_t>(kernel_integer(value.at("expected_contacts"),1,song.runtime_step_budget/2));
    song.steps=static_cast<std::size_t>(kernel_integer(value.at("expected_steps"),1,song.runtime_step_budget));
    song.capacity=static_cast<int>(kernel_integer(value.value("contact_capacity",Json(4)),1,10));
    song.peak=static_cast<int>(kernel_integer(value.at("expected_peak"),1,song.capacity));
    song.first_down=kernel_integer(value.at("first_down_song_ns"),0,180*second-1);
    song.projection_pixels=static_cast<int>(kernel_integer(value.at("max_normal_projection_reference_pixels"),1,1000000));
    song.drag_duration_ns=kernel_integer(value.at("drag_duration_ns"),ms,32*ms);
    song.input_inset_pixels=static_cast<int>(kernel_integer(value.value("input_inset_pixels",Json(0)),0,100));
    return song;
}
int prepare_adaptive(int argc,char** argv) {
    // Bounded option/value pairs, including both source filters and BlockArea.
    if(argc<5||argc>59||(argc-5)%2)throw std::invalid_argument(usage);
    const std::filesystem::path output=std::filesystem::absolute(utf8_path(argv[4]));
    if(std::filesystem::exists(output))throw std::invalid_argument("output must not exist");
    const auto source_bytes=read_utf8_file(argv[3],1024*1024);const auto source=Json::parse(source_bytes);
    if(source.at("package")!="com.PigeonGames.Phigros"||source.at("version_name")!="4.0.1"||
       kernel_integer(source.at("version_code"),157,157)!=157)
        throw std::invalid_argument("asset source revision unsupported");
    SongProfile song;
    std::set<std::string> seen_options;
    for(int index=5;index<argc;index+=2) {
        const std::string option=argv[index];
        if(!seen_options.insert(option).second)throw std::invalid_argument("duplicate prepare option");
        if(option=="--import-policy") {
            song.import_policy=argv[index+1];(void)song_import_options(song);continue;
        }
        if(option=="--hud-policy") {
            song.hud_policy=argv[index+1];
            if(!valid_adaptive_hud_policy(song.hud_policy))
                throw std::invalid_argument("unsupported explicit adaptive HUD policy");
            continue;
        }
        if(option=="--flick-path-policy") {
            (void)set_flick_option(song.flick,option,Json(argv[index+1]));continue;
        }
        if(option=="--hold-tail-policy") {
            const std::string policy=argv[index+1];
            if(policy=="full-pose-v1")song.hold_tail_policy=HoldTailPolicy::full_pose_v1;
            else if(policy=="phira-tail-grace-candidate-v1")song.hold_tail_policy=HoldTailPolicy::phira_tail_grace_candidate_v1;
            else throw std::invalid_argument("unknown explicit Hold tail policy");
            continue;
        }
        if(option=="--offscreen-policy") {song.offscreen_policy=song_offscreen_policy(argv[index+1]);continue;}
        const auto value=Json::parse(argv[index+1]);
        if(set_flick_option(song.flick,option,value))continue;
        if(option=="--normal-projection-pixels")song.projection_pixels=static_cast<int>(kernel_integer(value,1,1000000));
        else if(option=="--tap-duration-ms")song.tap_duration_ns=tap_dwell_ns_from_ms_json(value);
        else if(option=="--drag-duration-ms")song.drag_duration_ns=kernel_integer(value,1,32)*ms;
        else if(option=="--input-inset-pixels")song.input_inset_pixels=static_cast<int>(kernel_integer(value,0,100));
        else if(option=="--contact-capacity")song.capacity=static_cast<int>(kernel_integer(value,1,10));
        else if(option=="--tight-projection-speed-bounds")song.tight_projection_speed_bounds=kernel_integer(value,0,1)!=0;
        else if(option=="--max-tangent-error-pixels")song.max_tangent_error_reference_pixels=tangent_ceiling(value);
        else if(option=="--block-area-phase-ms")song.block_area_phase_ns=kernel_integer(value,-100,100)*ms;
        else if(option=="--allow-block-area-temporal-fallback")song.allow_block_area_temporal_fallback=kernel_integer(value,0,1)!=0;
        else if(option=="--max-prejudge-ms")song.max_prejudge_ns=kernel_integer(value,0,30)*ms;
        else if(option=="--advance-above-drags")song.advance_above_drags=kernel_integer(value,0,1)!=0;
        else if(option=="--advance-below-drags")song.advance_below_drags=kernel_integer(value,0,1)!=0;
        else if(option=="--below-drag-source-ids")song.below_drag_source_ids=below_drag_source_ids(value);
        else if(option=="--above-drag-source-ids")song.above_drag_source_ids=above_drag_source_ids(value);
        else if(option=="--outlined-drag-heads")song.outlined_drag_heads=kernel_integer(value,0,1)!=0;
        else if(option=="--native-head-appearance-witness")song.native_head_appearance_witness=kernel_integer(value,0,1)!=0;
        else if(option=="--max-hold-gap-ms")song.max_hold_reacquire_gap_ns=kernel_integer(value,0,20)*ms;
        else if(option=="--coincident-flick-delay-ms")song.coincident_flick_delay_ns=coincident_flick_delay(Json(kernel_integer(value,0,32)*ms));
        else throw std::invalid_argument(usage);
    }
    validate_flick_parameters(song.flick);
    if(song.flick.path_policy==FlickPathPolicy::judgment_anchored_screen_fit_v1||
       song.hold_tail_policy==HoldTailPolicy::phira_tail_grace_candidate_v1||
       song.offscreen_policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1||song.max_prejudge_ns>0||song.max_hold_reacquire_gap_ns>0||song.coincident_flick_delay_ns>0)
        song.compiler_diagnostic_budget=65536;
    song.chart_sha=source.at("chart").at("sha256");
    const bool installed_catalog=source.value("schema",std::string())=="pas.installed-catalog-song-source.v1";
    song.audio_sha=installed_catalog ? installed_catalog_audio_identity(source) : source.at("decoded_audio").at("sha256").get<std::string>();
    song.song=source.at("track_id");song.difficulty=source.at("difficulty");
    if(!hud_policy_matches_difficulty(song.hud_policy,song.difficulty))
        throw std::invalid_argument("HUD policy does not match selected difficulty");
    song.calibration_id=song.song+'-'+song.difficulty+'-'+"D"+std::to_string(song.projection_pixels)+"-unqualified-whole-song-experiment";
    const auto audio_path=utf8_path(argv[3]).parent_path()/"music.wav";
    if(!installed_catalog&&chart_text_sha256(read_utf8_file(audio_path,64*1024*1024))!=song.audio_sha)
        throw std::invalid_argument("source WAV hash mismatch");
    const auto chart=parse_phigros(read_utf8_file(argv[2],32*1024*1024),
        {argv[2],song.chart_sha,song.song,song.difficulty,song.revision},{},song_import_options(song));
    PhigrosGeometry geometry;geometry.input_width=1280;geometry.input_height=720;
    const auto compiled=compile_phigros_contacts(chart,song_identity(song),geometry,song_compile_options(song));
    // The first dispatched contact can be a Flick whose configurable lead
    // precedes even an earlier native Tap. Bind the visual model to the exact
    // compiled first DOWN, not a second hard-coded 30ms assumption.
    const auto first_down=compiled.plan&&!compiled.plan->events.empty()?compiled.plan->events.front().song_ns:-1;
    AdaptiveFirstNoteTrigger opening(chart,-80*ms,first_down,song.outlined_drag_heads,song.native_head_appearance_witness);const auto& description=opening.description();
    if(description.native_head_appearance_witness!=song.native_head_appearance_witness)
        throw std::runtime_error("native head appearance witness differs from explicit preparation policy");
    Json report={{"schema","pas.adaptive-song-prepare.v1"},{"offline_only",true},{"device_discovery",false},
        {"chart_sha256",song.chart_sha},{"audio_sha256",song.audio_sha},{"source_manifest_sha256",chart_text_sha256(source_bytes)},
        {"import",import_json(chart)},{"source_notes",chart.note_count},{"compiled",compiled.plan.has_value()},{"diagnostics",Json::array()},
        {"flick_parameters",flick_parameters_json(song.flick)},
        {"opening",{{"note_time_ns",description.note_time_ns},{"first_down_ns",description.first_down_ns},{"note_kind",static_cast<int>(description.note_kind)},
            {"note_id",description.note_id},{"note_json_pointer",description.note_json_pointer},
            {"model_id",description.model_id},{"model_sha256",description.model_sha256},
            {"detector_policy_id",description.detector_policy_id},{"includes_hold_heads",description.includes_hold_heads},
            {"includes_classic_flick_heads",description.includes_classic_flick_heads},
            {"includes_extended_drag_heads",description.includes_extended_drag_heads},
            {"includes_mixed_earliest_heads",description.includes_mixed_earliest_heads},
            {"required_center_families",description.required_center_families},
            {"earliest_note_ids",description.earliest_note_ids},
            {"retained_equations",description.retained_equations},{"provably_invisible_equations",description.provably_invisible_equations}}}};
    if(song.coincident_flick_delay_ns>0)report["coincident_flick_delay_ns"]=song.coincident_flick_delay_ns;
    if(song.advance_below_drags)report["advance_below_drags"]=true;
    if(!song.below_drag_source_ids.empty())report["below_drag_source_ids"]=song.below_drag_source_ids;
    if(!song.above_drag_source_ids.empty())report["above_drag_source_ids"]=song.above_drag_source_ids;
    if(song.outlined_drag_heads){report["outlined_drag_heads"]=true;report["opening"]["includes_outlined_drag_heads"]=true;}
    if(song.native_head_appearance_witness) {
        report["native_head_appearance_witness"]=true;
        report["native_head_appearance_witness_policy"]=native_head_appearance_witness_json();
        report["opening"]["native_head_appearance_witness"]=true;
    }
    store_tap_dwell_profile_fields(report,song.tap_duration_ns);
    for(const auto& diagnostic:compiled.diagnostics)report["diagnostics"].push_back({
        {"severity",static_cast<int>(diagnostic.severity)},{"code",diagnostic.code},{"message",diagnostic.message},
        {"source_note_id",diagnostic.source_note_id},{"song_ns",diagnostic.song_ns}});
    if(!std::filesystem::create_directory(output))throw std::runtime_error("output creation failed");
    if(compiled.plan) {
        const auto& plan=*compiled.plan;
        add_block_area_evidence(report,plan.metadata);
        song.runtime_step_budget=65536;
        if(!plan.events.empty())song.runtime_duration_limit_ns=std::max(180*second,(plan.events.back().song_ns/second+31)*second);
        const bool bounded=plan.memory.contacts.size()==chart.note_count&&plan.total_steps<=song.runtime_step_budget&&
            plan.peak_contacts<=song.capacity&&!plan.events.empty()&&plan.events.front().phase==Phase::down&&
            plan.events.front().song_ns==description.first_down_ns&&song.runtime_duration_limit_ns<=600*second&&
            plan.events.back().song_ns<song.runtime_duration_limit_ns;
        report["within_live_experiment_bounds"]=bounded;
        report["runtime_step_budget"]=song.runtime_step_budget;report["runtime_duration_limit_ns"]=song.runtime_duration_limit_ns;
        report["hold_tail_policy_id"]=hold_tail_policy_name(song.hold_tail_policy);
        if(!plan.metadata.judgment_policy_provenance.empty())report["judgment_policy_provenance"]=plan.metadata.judgment_policy_provenance;
        if(!plan.events.empty())report["last_up_song_ns"]=plan.events.back().song_ns;
        write_json(output/"compiled-plan.json",compiled_json(plan));
        if(bounded) {
            Json profile={{"schema","pas.adaptive-song-profile.v1"},{"song_id",song.song},{"difficulty",song.difficulty},
                {"revision",song.revision},{"chart_sha256",song.chart_sha},{"audio_sha256",song.audio_sha},
                {"calibration_id",song.calibration_id},{"expected_contacts",plan.memory.contacts.size()},
                {"expected_steps",plan.total_steps},{"expected_peak",plan.peak_contacts},
                {"expected_plan_sha256",plan.metadata.cache_sha256},{"first_down_song_ns",plan.events.front().song_ns},
                {"drag_duration_ns",song.drag_duration_ns},{"max_normal_projection_reference_pixels",song.projection_pixels},
                {"input_inset_pixels",song.input_inset_pixels},
                {"contact_capacity",song.capacity},{"hud_policy",song.hud_policy},
                {"runtime_step_budget",song.runtime_step_budget},{"runtime_duration_limit_ns",song.runtime_duration_limit_ns},
                {"compiler_diagnostic_budget",song.compiler_diagnostic_budget},
                {"last_up_song_ns",plan.events.back().song_ns},
                {"hold_tail_policy_id",hold_tail_policy_name(song.hold_tail_policy)},
                {"offscreen_policy_id",offscreen_policy_name(song.offscreen_policy)},
                {"max_tangent_error_reference_pixels",song.max_tangent_error_reference_pixels},
                {"max_prejudge_ns",song.max_prejudge_ns},{"advance_above_drags",song.advance_above_drags},
                {"max_hold_reacquire_gap_ns",song.max_hold_reacquire_gap_ns},
                {"import_policy_id",song.import_policy},{"tight_projection_speed_bounds",song.tight_projection_speed_bounds},
                {"opening_model_sha256",description.model_sha256},{"source_manifest_sha256",chart_text_sha256(source_bytes)},
                {"gameplay_qualified",false},{"scope","per-song offline candidate; actual opening/HUD and full gameplay remain to be observed"}};
            profile.update(flick_parameters_json(song.flick));
            if(!plan.metadata.block_area_policy_id.empty()&&song.block_area_phase_ns)
                profile["block_area_phase_ms"]=*song.block_area_phase_ns/ms;
            if(plan.metadata.options.allow_block_area_temporal_fallback)
                profile["allow_block_area_temporal_fallback"]=true;
            store_tap_dwell_profile_fields(profile,song.tap_duration_ns);
            if(song.coincident_flick_delay_ns>0)profile["coincident_flick_delay_ns"]=song.coincident_flick_delay_ns;
            if(song.advance_below_drags)profile["advance_below_drags"]=true;
            if(!song.below_drag_source_ids.empty())profile["below_drag_source_ids"]=song.below_drag_source_ids;
            if(!song.above_drag_source_ids.empty())profile["above_drag_source_ids"]=song.above_drag_source_ids;
            if(song.outlined_drag_heads)profile["outlined_drag_heads"]=true;
            if(song.native_head_appearance_witness)profile["native_head_appearance_witness"]=true;
            if(!plan.metadata.judgment_policy_provenance.empty())profile["judgment_policy_provenance"]=plan.metadata.judgment_policy_provenance;
            if(installed_catalog) {
                const auto path=std::filesystem::absolute(utf8_path(argv[3])).u8string();
                profile["installed_source_manifest"]=std::string(path.begin(),path.end());
                profile["audio_identity_format"]="native-fsb5";
            }
            write_json(output/"song-profile.json",profile);
        }
    }
    write_json(output/"report.json",report);
    // A large source-anomaly inventory belongs in the report file. Printing it
    // again must not make a successful preparation exceed the caller's pipe cap.
    std::cout<<Json({{"schema","pas.adaptive-song-prepare-summary.v1"},{"report_file","report.json"},
        {"compiled",compiled.plan.has_value()},{"source_notes",chart.note_count},
        {"within_live_experiment_bounds",report.value("within_live_experiment_bounds",false)},
        {"diagnostic_count",compiled.diagnostics.size()}}).dump(2)<<'\n';
    return compiled.plan&&report.value("within_live_experiment_bounds",false)?0:2;
}
Json checked_capacity_evidence(const std::filesystem::path& path,const Json& kernel,int capacity) {
    const auto bytes=read_utf8_file(path,65536);auto proof=Json::parse(bytes);
    if(proof.at("schema")!="pas.fixture-capacity-evidence.v1"||proof.at("boot_id")!=kernel.at("boot_id")||
       kernel_integer(proof.at("capacity"),capacity,10)<capacity||proof.at("normal_path_valid")!=true||proof.at("final_zero")!=true||
       proof.at("fixture_apk_sha256")!="d4ae58c7f9561b4262791a1711e2688db233ec054b10400e88b5e60fccfc34a2")
        throw std::invalid_argument("capacity evidence does not qualify this boot/capacity");
    const auto artifact=[&](const Json& value) {
        const auto expected=value.at("sha256").get<std::string>();
        const auto content=read_utf8_file(value.at("file").get<std::string>(),32*1024*1024);
        if(!sha256_text(expected)||chart_text_sha256(content)!=expected)throw std::invalid_argument("capacity evidence artifact hash mismatch");
        return content;
    };
    const auto& artifacts=proof.at("artifacts");
    if(!artifacts.is_array()||artifacts.empty()||artifacts.size()>8)throw std::invalid_argument("capacity artifact budget");
    for(const auto& value:artifacts)(void)artifact(value);
    const auto analysis=Json::parse(artifact(proof.at("analysis")));
    const auto host=Json::parse(artifact(proof.at("host_manifest")));
    const auto retrieval=Json::parse(artifact(proof.at("retrieval_report")));
    const auto& native=analysis.at("android");const auto& geometry=host.at("geometry");
    if(analysis.at("status")!="passed_observed_fixture_path"||!analysis.at("failures").empty()||!analysis.at("incomplete_reasons").empty()||
       kernel_integer(native.at("peak_contacts"),capacity,10)<capacity||native.at("stream_complete")!=true||
       native.at("expected_path_verified")!=true||native.at("pointer_lifecycle_valid")!=true||native.at("final_zero_observed")!=true||
       host.at("serial")!="emulator-5554"||host.at("avd")!="phigros"||host.at("path_verified")!=true||host.at("zero_active_observed")!=true||
       kernel_integer(host.at("capacity_requested"),capacity,10)<capacity||
       geometry.at("touch_width")!=720||geometry.at("touch_height")!=1280||geometry.at("touch_rotation")!=90||
       retrieval.at("status")!="retrieved_closed_snapshot"||retrieval.at("association_stable")!=true||
       retrieval.at("identity_before").at("boot_id")!=kernel.at("boot_id")||retrieval.at("identity_after").at("boot_id")!=kernel.at("boot_id"))
        throw std::invalid_argument("capacity evidence native path/geometry/boot binding failed");
    proof["evidence_sha256"]=chart_text_sha256(bytes);return proof;
}
int run(int argc,char** argv,DesktopRoundSummary* desktop) {
    const auto clock_probe=clock_probe_suffix(argc,argv);
    const auto native_head_window=native_head_snapshot_suffix(argc,argv);
    const bool numeric_hud_enabled=numeric_hud_suffix(argc,argv);
    const auto image_retention=image_retention_suffix(argc,argv);
    const bool diagnostic_images=retain_diagnostic_images(image_retention);
    const std::string command=argc>1?argv[1]:"";
    const bool adaptive=command=="run-adaptive"||command=="check-adaptive";
    const bool offline=command=="check"||command=="check-adaptive";
    if(native_head_window&&(!adaptive||!numeric_hud_enabled))
        throw std::invalid_argument("native head snapshot requires run/check-adaptive and --numeric-hud-observer v1");
    if(native_head_window&&diagnostic_images)
        throw std::invalid_argument("native head snapshot shadow requires none or result image retention");
    if(clock_probe.enabled&&(!adaptive||!numeric_hud_enabled||!native_head_window))
        throw std::invalid_argument("clock probe requires adaptive mode, numeric HUD and native head snapshot diagnostics");
    if((!adaptive&&(argc!=10&&argc!=12))||(adaptive&&argc!=14&&argc!=16)||
       (!adaptive&&command!="run"&&command!="check")||std::string(argv[9])!="--experimental-whole-song"||
       (!adaptive&&argc==12&&std::string(argv[10])!="--experimental-hud-reference")||
       (adaptive&&(std::string(argv[10])!="--song-profile"||std::string(argv[12])!="--experimental-hud-reference"))||
       (argc==16&&std::string(argv[14])!="--capacity-evidence"&&std::string(argv[14])!="--capacity-policy"))
        throw std::invalid_argument(usage);
    const auto capacity_policy=argc==16&&std::string(argv[14])=="--capacity-policy"?
        parse_capacity_policy(argv[15]):CapacityPolicy::current_boot_fixture;
    const bool configured_capacity=capacity_policy==CapacityPolicy::configured_ten_contact;
    const auto song=adaptive?load_song_profile(utf8_path(argv[11])):SongProfile{};
    const auto& chart_sha=song.chart_sha;const auto& audio_sha=song.audio_sha;
    const auto& expected_plan_sha=song.plan_sha;const auto expected_steps=song.steps;
    const auto expected_notes=song.notes;const auto expected_peak=song.peak;const auto first_song_ns=song.first_down;
    const std::string serial=argv[2],avd=argv[3];const int phase=phase_option(argv[8]);
    if(serial!="emulator-5554"||avd!="phigros")throw std::invalid_argument("restricted to phigros / emulator-5554");
    const auto chart_bytes=read_utf8_file(argv[4],32*1024*1024);
    ChartSource chart_source;chart_source.label=argv[4];chart_source.chart_sha256=chart_sha;
    chart_source.song_id=song.song;chart_source.difficulty=song.difficulty;chart_source.game_revision=song.revision;
    const auto chart=parse_phigros(chart_bytes,chart_source,{},song_import_options(song));
    const auto active_block_areas=std::any_of(chart.block_areas.begin(),chart.block_areas.end(),
        [](const auto& area){return area.enable_ns<area.disable_ns;});
    if(active_block_areas&&song.block_area_phase_ns&&*song.block_area_phase_ns!=static_cast<Ns>(phase)*ms)
        throw std::invalid_argument("block_area_dispatch_phase_does_not_match_profile");
    if(active_block_areas&&clock_probe.enabled)
        throw std::invalid_argument("block_area_requires_unshifted_compiled_plan");
    if(song.allow_block_area_temporal_fallback&&(!active_block_areas||!song.block_area_phase_ns))
        throw std::invalid_argument("block_area_temporal_fallback_requires_active_areas_and_pinned_phase");
    if(chart.note_count!=expected_notes)throw std::runtime_error("whole original chart note count differs from song profile");
    const auto compile_options=song_compile_options(song);
    PhigrosGeometry geometry;geometry.input_width=1280;geometry.input_height=720;
    const auto identity=song_identity(song);
    auto compiled=compile_phigros_contacts(chart,identity,geometry,compile_options);
    if(!compiled.plan||compiled.plan->memory.contacts.size()!=expected_notes||compiled.plan->total_steps!=expected_steps||compiled.plan->peak_contacts!=expected_peak||
       compiled.plan->metadata.cache_sha256!=expected_plan_sha||
       compiled.plan->events.empty()||compiled.plan->events.front().song_ns!=first_song_ns||compiled.plan->events.front().phase!=Phase::down)
        throw std::runtime_error("fresh whole-chart compile differs from pinned song profile and fingerprint");
    const auto full_plan_json=compiled_json(*compiled.plan);
    std::optional<ClockProbeProof> clock_probe_proof;
    if(clock_probe.enabled) {
        clock_probe_proof=validate_clock_probe(*compiled.plan,clock_probe);
        if(!clock_probe_proof->prefix_events||!clock_probe_proof->suffix_events||
           clock_probe_proof->mapped_first_ns!=clock_probe_proof->original_first_ns||
           clock_probe_proof->mapped_last_ns>=song.runtime_duration_limit_ns)
            throw std::invalid_argument("clock probe must retain a first-input prefix and suffix inside the existing duration budget");
    }
    for(const auto& event:compiled.plan->events) {
        const auto point=pixel_point(event.point,1280,720);const auto inset=song.input_inset_pixels;
        if(point.first<inset||point.first>=1280-inset||point.second<inset||point.second>=720-inset)
            throw std::invalid_argument("compiled input enters profile's excluded gesture edge");
    }
    std::unique_ptr<AdaptiveFirstNoteTrigger> adaptive_trigger;
    if(adaptive) {
        adaptive_trigger=std::make_unique<AdaptiveFirstNoteTrigger>(
            chart,static_cast<Ns>(phase)*ms,first_song_ns,song.outlined_drag_heads,
            song.native_head_appearance_witness,desktop!=nullptr,
            desktop?desktop_first_note_dispatch_lead_ns:default_first_note_dispatch_lead_ns);
        if(adaptive_trigger->description().native_head_appearance_witness!=song.native_head_appearance_witness)
            throw std::invalid_argument("native head appearance witness differs from explicit song profile");
        if(adaptive_trigger->description().model_sha256!=song.opening_sha||
           adaptive_trigger->description().first_down_ns!=first_song_ns)
            throw std::invalid_argument("fresh adaptive opening differs from prepared song profile");
    }
    std::unique_ptr<NativeHeadSnapshotModel> native_head_model;
    std::size_t native_head_caller_pose_events=0;
    if(native_head_window) {
        for(const auto& line:chart.lines){const auto count=line.move_events.size()+line.rotate_events.size()+line.opacity_events.size();
            if(count>500000-native_head_caller_pose_events)throw std::invalid_argument("native head snapshot original pose event budget");
            native_head_caller_pose_events+=count;}
        // Original tracks are copied only during opt-in precomputation. This
        // transient evaluator is explicitly outside the model-owned8MiB cap.
        const PhigrosPoseEvaluator original_poses(chart);
        native_head_model=std::make_unique<NativeHeadSnapshotModel>(chart,original_poses,*native_head_window);
    }
    ExperimentalRuntimeOptions runtime_options;runtime_options.ui_receipt_max_age_ns=ui_watchdog;
    runtime_options.contact_capacity=song.capacity;
    runtime_options.max_song_steps=song.runtime_step_budget;
    runtime_options.song_duration_limit_ns=song.runtime_duration_limit_ns;
    runtime_options.max_tangent_error_reference_pixels=song.max_tangent_error_reference_pixels;
    runtime_options.max_prejudge_ns=song.max_prejudge_ns;
    runtime_options.max_hold_reacquire_gap_ns=song.max_hold_reacquire_gap_ns;
    runtime_options.max_coincident_flick_delay_ns=song.coincident_flick_delay_ns;
    runtime_options.allow_advance_below_drags=song.advance_below_drags;
    runtime_options.allow_block_area_temporal_fallback=song.allow_block_area_temporal_fallback;
    if(active_block_areas)runtime_options.block_area_phase_ns=song.block_area_phase_ns;
    runtime_options.clock_probe=clock_probe;
    if(adaptive) {
        runtime_options.max_normal_projection_reference_pixels=song.projection_pixels;
        runtime_options.anchor_domain_begin_ns=adaptive_trigger->description().begin_ns;
        runtime_options.anchor_domain_end_ns=adaptive_trigger->description().end_ns;
    }
    auto prepared_runtime=ExperimentalContactRuntime::prepare(*compiled.plan,runtime_options);
    const auto screen_calibration=load_screen_state_calibration(utf8_path(argv[5]));
    const auto state_dataset=Json::parse(read_utf8_file(argv[5],1024*1024));
    auto capture_profile=state_dataset.value("capture_geometry_profile",std::string("landscape-r1"));
    const auto calibration_capture_profile=capture_profile;
    if(capture_profile!="landscape-r1"&&capture_profile!="sensor-portrait-r0-ccw90")
        throw std::invalid_argument("unsupported explicit capture geometry profile");
    const bool shared_hud=adaptive&&song.hud_policy!="calibrated-title-v1";
    const auto shared_policy=song.hud_policy=="shared-difficulty-subpixel-prefix-score-v4"?HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4:
        song.hud_policy=="shared-in-subpixel-prefix-score-v3"?HudGlyphPolicy::shared_in_subpixel_prefix_score_v3:
        song.hud_policy=="shared-in-cell-score-v2"?HudGlyphPolicy::shared_in_cell_score_v2:HudGlyphPolicy::shared_in_prefix_score_v1;
    const auto expected_hud_policy=song.hud_policy=="shared-difficulty-subpixel-prefix-score-v4"?"shared_difficulty_subpixel_prefix_score_v4":
        song.hud_policy=="shared-in-subpixel-prefix-score-v3"?"shared_in_subpixel_prefix_score_v3":
        song.hud_policy=="shared-in-cell-score-v2"?"shared_in_cell_score_v2":"shared_in_prefix_score_v1";
    if(song.hud_policy=="shared-difficulty-subpixel-prefix-score-v4"&&state_dataset.value("hud_difficulty",std::string())!=song.difficulty)
        throw std::invalid_argument("actual HUD calibration difficulty differs from selected chart");
    if(state_dataset.value("experimental_shared_IN_HUD",false)!=shared_hud||
       (shared_hud&&state_dataset.value("hud_glyph_policy",std::string())!=expected_hud_policy))
        throw std::invalid_argument("HUD dataset/profile policy binding differs");
    if(screen_calibration.width!=1280||screen_calibration.height!=720)throw std::runtime_error("screen profile must be 1280x720");
    const auto result_policy=state_dataset.value("experimental_result_label_policy",std::string());
    std::unique_ptr<ResultLabelEvidenceProducer> result_labels;
    if(!result_policy.empty()) {
        if(!shared_hud||(result_policy!="shared-result-labels-v1"&&result_policy!="shared-result-local-contrast-v2"))
            throw std::invalid_argument("result label policy differs");
        result_labels=std::make_unique<ResultLabelEvidenceProducer>(screen_calibration.templates,
            result_policy=="shared-result-local-contrast-v2"?ResultLabelPolicy::local_contrast_v2:ResultLabelPolicy::absolute_v1);
    }
    const auto redundancy_policy=state_dataset.value("experimental_playing_redundancy_policy",std::string());
    std::unique_ptr<LearnedHudRedundancy> hud_redundancy;
    if(!redundancy_policy.empty()) {
    if(!shared_hud||(song.hud_policy!="shared-in-subpixel-prefix-score-v3"&&song.hud_policy!="shared-difficulty-subpixel-prefix-score-v4")||redundancy_policy!="learned-title-redundancy-v1")
            throw std::invalid_argument("playing redundancy policy differs");
        hud_redundancy=std::make_unique<LearnedHudRedundancy>();
    }
    std::unique_ptr<HudGlyphEvidenceProducer> hud_features;
    std::string hud_reference_sha;
    if(adaptive||argc==12) {
        const auto reference=read_utf8_file(argv[adaptive?13:11],1280*720*3);
        hud_reference_sha=chart_text_sha256(reference);
        if(reference.size()!=1280*720*3||(!adaptive&&hud_reference_sha!=dlyrotz_hud_reference_sha))
            throw std::invalid_argument("HUD experiment requires pinned clean 1280x720 RGB reference");
        if(adaptive) {
            const auto dataset=Json::parse(read_utf8_file(argv[5],1024*1024));bool matched=false;
            for(const auto& item:dataset.at("templates"))
                if(item.at("state")=="playing"&&item.at("rgb").at("sha256")==hud_reference_sha)matched=true;
            if(!matched)throw std::invalid_argument("adaptive HUD reference must equal the song's playing calibration RGB hash");
        }
        const RgbImageView view{1280,720,3840,{reinterpret_cast<const std::uint8_t*>(reference.data()),reference.size()}};
        hud_features=std::make_unique<HudGlyphEvidenceProducer>(view,hud_reference_sha,song.song+"-hud-experimental",
            PixelRegion{25,672,adaptive?160:105,29},
            shared_hud?shared_policy:HudGlyphPolicy::calibrated_title_v1);
    }
    const auto kernel_bytes=read_utf8_file(argv[6],8192);Json kernel_profile=Json::parse(kernel_bytes);
    const Json capacity_evidence=argc==16&&!configured_capacity&&std::string(argv[14])=="--capacity-evidence"?
        checked_capacity_evidence(utf8_path(argv[15]),kernel_profile,song.capacity):Json(nullptr);
    const bool proof_required=capacity_evidence_required(capacity_policy,song.capacity);
    if(!offline&&proof_required&&capacity_evidence.is_null())
        throw std::invalid_argument("live capacity above four contacts requires independently checked current-boot capacity evidence");
    const std::filesystem::path output=std::filesystem::absolute(utf8_path(argv[7]));
    if(std::filesystem::exists(output))throw std::runtime_error("output must not exist");
    if(offline) {
        Json preflight={{"status","offline_preflight_passed"},{"device_discovery",false},{"input_enabled",false},
            {"executable_sha256",binary_sha()},{"song_id",song.song},{"difficulty",song.difficulty},
            {"chart_sha256",chart_sha},{"import",import_json(chart,false)},{"compiled_sha256",compiled.plan->metadata.cache_sha256},
            {"contacts",expected_notes},{"events",expected_steps},{"peak",expected_peak},
            {"contact_capacity",song.capacity},{"capacity_evidence_required_for_live",proof_required},
            {"runtime_step_budget",song.runtime_step_budget},{"runtime_duration_limit_ns",song.runtime_duration_limit_ns},
            {"last_up_song_ns",compiled.plan->events.back().song_ns},
            {"tap_duration_ns",song.tap_duration_ns},{"tap_duration_is_default",song.tap_duration_ns==tap_dwell_default_ns},
            {"capacity_policy",capacity_policy_name(capacity_policy)},{"capacity_test_performed",false},
            {"capture_geometry_profile",capture_profile},
            {"flick_parameters",flick_parameters_json(song.flick)},
            {"hold_tail_policy_id",hold_tail_policy_name(song.hold_tail_policy)},
            {"judgment_policy_provenance",compiled.plan->metadata.judgment_policy_provenance},
            {"offscreen_policy_id",offscreen_policy_name(song.offscreen_policy)},
            {"max_tangent_error_reference_pixels",song.max_tangent_error_reference_pixels},
            {"max_prejudge_ns",song.max_prejudge_ns},{"advance_above_drags",song.advance_above_drags},
            {"coincident_flick_delay_ns",song.coincident_flick_delay_ns},
            {"advance_below_drags",song.advance_below_drags},
            {"below_drag_source_ids",song.below_drag_source_ids},
            {"outlined_drag_heads",song.outlined_drag_heads},
            {"max_hold_reacquire_gap_ns",song.max_hold_reacquire_gap_ns},
            {"image_retention",image_retention_name(image_retention)},
            {"screen_calibration_sha256",screen_calibration.dataset_sha256},{"HUD_reference_sha256",hud_reference_sha},{"HUD_policy",song.hud_policy},{"result_label_policy",result_policy},{"playing_redundancy_policy",redundancy_policy},
            {"kernel_profile_sha256",chart_text_sha256(kernel_bytes)},
            {"scope","offline artifact/config validation; current boot, foreground, contacts and game judgment NOT RUN"}};
        if(native_head_model){preflight["native_head_snapshot_shadow_requested"]=true;
            preflight["native_head_snapshot_shadow"]=native_head_shadow_model_json(*native_head_model,native_head_caller_pose_events);}
        if(song.tap_duration_ns!=tap_dwell_default_ns)preflight["tap_dwell_candidate"]=tap_dwell_candidate_json<Json>(song.tap_duration_ns);
        if(clock_probe_proof)preflight["experimental_clock_probe"]=clock_probe_proof_json(*clock_probe_proof);
        if(!song.above_drag_source_ids.empty())preflight["above_drag_source_ids"]=song.above_drag_source_ids;
        if(song.native_head_appearance_witness) {
            preflight["native_head_appearance_witness"]=true;
            preflight["native_head_appearance_witness_policy"]=native_head_appearance_witness_json();
        }
        add_block_area_evidence(preflight,compiled.plan->metadata);
        std::cout<<preflight.dump(2)<<'\n';
        return 0;
    }
    if(desktop)desktop->stage="adb_setup";
    const auto adb=pas::find_adb();const auto avd_text=adb_text(adb,serial,{"emu","avd","name"});
    if(avd_text.substr(0,avd_text.find_first_of("\r\n"))!=avd)throw std::runtime_error("AVD mismatch");
    const auto package_report=adb_text(adb,serial,{"shell","dumpsys","package","com.PigeonGames.Phigros"});
    if(field(package_report,"versionName=")!="4.0.1"||field(package_report,"versionCode=")!="157")throw std::runtime_error("game version mismatch");
    const auto kernel_template_boot_id=kernel_profile.at("boot_id");
    if(configured_capacity) {
        kernel_profile["boot_id"]=configured_runtime_boot_id(adb_text(adb,serial,{"shell","cat","/proc/sys/kernel/random/boot_id"}));
        kernel_profile["binding_scope"]="current session boot identity for release monitoring; no repeated capacity test";
    }
    std::unique_ptr<NumericHudObserver> numeric_hud;
    if(numeric_hud_enabled)numeric_hud=std::make_unique<NumericHudObserver>(expected_notes);
    std::unique_ptr<NativeHeadShadowRows> native_head_rows;
    if(native_head_model)native_head_rows=std::make_unique<NativeHeadShadowRows>();
    HudTimeline hud_timeline(HudTimeline::maximum_samples,diagnostic_images); // Metadata only by default; preallocate before READY.
    pas::HostClock clock;OwnerWait owner_wait(clock);
    const auto foreground_begin=clock.now_ns();foreground(adb,serial);const auto foreground_end=clock.now_ns();
    manual_ui::ForegroundWatch foreground_watch([adb,serial]{foreground(adb,serial);},[clock]{return clock.now_ns();},{foreground_begin,foreground_end});
    KernelSlots kernel(adb,serial,kernel_profile,clock);
    Json initial_kernel,initial_kernel_attempts=Json::array();
    // A slow read of zero contacts grants no permission. Before READY only,
    // allow two new read-only attempts; every accepted reply retains the same
    // 250ms causal bound. An active contact or query error fails immediately.
    for(unsigned attempt=0;attempt<3;++attempt){
        initial_kernel=kernel.query();initial_kernel_attempts.push_back(initial_kernel);
        if(initial_kernel.at("active_count")!=0)throw std::runtime_error("initial kernel state is not zero");
        if(initial_kernel.at("causal_host_interval_valid")==true)break;
    }
    if(initial_kernel.at("causal_host_interval_valid")!=true)throw std::runtime_error("initial kernel state not fresh zero after bounded read-only retries");
    if(desktop)desktop->stage="capture_setup";
    const auto endpoint=pas::discover_endpoint(serial);
    if(configured_capacity) {
        pas::CaptureOptions probe_options;
        pas::GrpcCapture probe(clock,endpoint,probe_options);
        const auto geometry_frame=probe.snapshot(std::chrono::milliseconds(1000));
        capture_profile=capture_profile_for_geometry(geometry_frame.width,geometry_frame.height,geometry_frame.source_rotation);
    }
    pas::CaptureOptions options;options.width=1280;options.height=720;options.source_rotation=1;options.max_relative_lag_ns=250*ms;
    if(capture_profile=="sensor-portrait-r0-ccw90") {
        options.width=720;options.height=1280;options.source_rotation=0;options.rotate_ccw=true;
    }
    pas::GrpcCapture capture(clock,endpoint,options);
    // A separate unary client can obtain an actual current screenshot when
    // streamScreenshot emits no framebuffer-change notification. No owner
    // thread waits for this RPC, and no old image is assigned a new receipt.
    pas::GrpcCapture recovery_capture(clock,endpoint,options);
    (void)recovery_capture.snapshot(std::chrono::milliseconds(1000));
    const auto initial_frame=capture.snapshot(std::chrono::milliseconds(1000));
    const auto initial_state=classify_screen_state({initial_frame.width,initial_frame.height,initial_frame.stride,initial_frame.rgb},screen_calibration.templates).state;
    if(!initial_frame.source_valid||initial_frame.width!=1280||initial_frame.height!=720||
       initial_state==ObservedGameState::playing||initial_state==ObservedGameState::paused)
        throw std::runtime_error("start from manually verified selected-song screen/result; already-playing or paused is rejected");
    if(desktop)desktop->output=output;
    if(!std::filesystem::create_directory(output))throw std::runtime_error("output creation failed");
    if(desktop){desktop->owns_output=true;desktop->stage="transport_setup";}
    Journal journal(output/"events.jsonl",song.runtime_step_budget>16000?65536:16000);
    // Allocate and touch the already bounded journal budget before READY/input,
    // rather than copying a growing multi-megabyte string on an owner deadline.
    journal.pending.resize(64u*1024u*1024u);journal.pending.clear();
    OpeningVisualModel model;model.chart_sha256=chart_sha;model.model_id="experimental-opening-static-v1";
    Json model_settings={{"id",model.model_id},{"domain_ns",{model.begin_ns,model.end_ns}},
        {"scale",{model.approach_scale_lower,model.approach_scale_upper}},{"pixel_error",model.pixel_error},
        {"pose_envelope_pixels",model.pose_tolerance_pixels},{"horizontal_visibility_margin_pixels",model.horizontal_visibility_margin_pixels},
        {"source_clock","arrival_only"},{"coverage_assumption","complete unoccluded cyan centers; conditional experiment, not a detection guarantee"}};
    model.model_sha256=chart_text_sha256(model_settings.dump());model.calibration_id="unqualified-whole-song-experiment";model.calibration_sha256=model.model_sha256;
    std::uint64_t acquisition_epoch=1;
    std::unique_ptr<VisualTimingCandidateProducer> candidates;std::unique_ptr<FirstNoteTrigger> trigger;
    if(adaptive) {
        const auto& description=adaptive_trigger->description();
        model_settings={{"id",description.model_id},{"detector_policy_id",description.detector_policy_id},
            {"includes_hold_heads",description.includes_hold_heads},
            {"includes_classic_flick_heads",description.includes_classic_flick_heads},
            {"includes_extended_drag_heads",description.includes_extended_drag_heads},
            {"includes_mixed_earliest_heads",description.includes_mixed_earliest_heads},
            {"required_center_families",description.required_center_families},
            {"domain_ns",{description.begin_ns,description.end_ns}},
            {"earliest_note_ids",description.earliest_note_ids},{"retained_equations",description.retained_equations},
            {"provably_invisible_equations",description.provably_invisible_equations},
            {"coverage_assumption",description.includes_hold_heads?
                "complete distinguishable typed heads is an explicit solver hypothesis; pixel detector does not establish coverage; direct same-kind/side whole-piece L1<=1/1024px source coincidence exempts absence only":
                "complete unoccluded typed head centers is an explicit solver hypothesis; pixel detector does not establish coverage"},
            {"first_note_id",description.note_id},{"first_note_json_pointer",description.note_json_pointer},
            {"first_note_kind",static_cast<int>(description.note_kind)},{"first_note_ns",description.note_time_ns},
            {"first_down_ns",description.first_down_ns},{"earliest_note_count",description.earliest_note_count},
            {"source_clock","arrival_only"},{"scope","native chart opening; conditional rendering hypothesis"}};
        if(description.certified_first_group_coverage)
            model_settings["opening_witness_policy"]="certified-first-group-coverage-v1";
        model_settings["minimum_dispatch_lead_ns"]=desktop?desktop_first_note_dispatch_lead_ns:default_first_note_dispatch_lead_ns;
        if(song.outlined_drag_heads)model_settings["includes_outlined_drag_heads"]=true;
        if(song.native_head_appearance_witness) {
            model_settings["native_head_appearance_witness"]=true;
            model_settings["native_head_appearance_witness_policy"]=native_head_appearance_witness_json();
        }
        model.model_sha256=description.model_sha256;
    } else {
        VisualCandidateLimits limits;limits.max_observations=256;
        candidates=std::make_unique<VisualTimingCandidateProducer>(chart,model,limits);candidates->begin_epoch(acquisition_epoch);
        trigger=std::make_unique<FirstNoteTrigger>(chart,model,static_cast<Ns>(phase)*ms);
    }
    const auto trigger_health=[&](Ns now,bool healthy) {
        if(adaptive)adaptive_trigger->status(now,healthy);else trigger->status(now,healthy);
    };
    const auto desktop_limits=desktop_runtime_limits(song.runtime_duration_limit_ns);
    const auto session_limit_ns=desktop_limits.session_limit_ns;
    ManualConfig manual_config;manual_config.round_limit=1;
    manual_config.round_watchdog_ns=desktop_limits.manual_settlement_watchdog_ns;
    manual_config.max_observation_age_ns=ui_watchdog;manual_config.max_continuity_gap_ns=ui_watchdog;
    ManualLifecycle lifecycle(manual_config);
    const auto source_sha=chart_text_sha256("emulator-5554/phigros/1280x720/"+capture_profile+"/"+kernel_profile.at("boot_id").get<std::string>());
    const Json touch_pressure_policy={{"id",StreamingTouchPressurePolicy::id},
        {"unit","raw ABS_MT_PRESSURE integer; not normalized Android MotionEvent pressure"},
        {"DOWN_MOVE",StreamingTouchPressurePolicy::active_pressure},{"UP_RELEASE",StreamingTouchPressurePolicy::up_pressure},
        {"project_validated_axis_min",0},{"project_validated_axis_max",1024},
        {"axis_scope","matches the project's validated event2 SDK device axis and standard local UI amplitude; not re-probed by this policy"},
        {"acceptance_scope","versioned transport amplitude candidate only; no game pressure threshold, kernel effect or judgment qualification"}};
    Json manifest={{"schema","pas.experimental-whole-song.v1"},{"serial",serial},{"avd",avd},{"executable_sha256",binary_sha()},
        {"touch_pressure_policy",touch_pressure_policy},
        {"chart_sha256",chart_sha},{"import",import_json(chart)},{"audio_sha256",audio_sha},{"audio_hash_scope","identity pinned from prior verified asset; no audio read/capture in this CLI"},
        {"screen_calibration_sha256",screen_calibration.dataset_sha256},{"result_label_policy",result_policy},{"playing_redundancy_policy",redundancy_policy},{"kernel_profile_sha256",chart_text_sha256(kernel_bytes)},
        {"kernel_initial",initial_kernel},{"kernel_initial_attempts",initial_kernel_attempts},
        {"model_settings",model_settings},{"model_sha256",model.model_sha256},
        {"source_id",source_id},{"source_sha256",source_sha},{"capture_geometry_profile",capture_profile},
        {"capture_normalization",{{"raw_width",options.width},{"raw_height",options.height},{"source_rotation",options.source_rotation},{"rotate_ccw",options.rotate_ccw}}},{"experimental_phase_ms",phase},
        {"calibration_capture_geometry_profile",calibration_capture_profile},
        {"song_identity_manual_selection",true},{"initial_screen_class",static_cast<int>(initial_state)},
        {"song_selection_scope","operator selects the profile's exact song/difficulty before launch; initial unknown is not automatic song identification"},
        {"song_id",song.song},{"difficulty",song.difficulty},{"adaptive_opening",adaptive},{"HUD_policy",song.hud_policy},
        {"input_inset_pixels",song.input_inset_pixels},
        {"capacity_evidence",capacity_evidence},
        {"capacity_policy",capacity_policy_name(capacity_policy)},{"capacity_test_performed",false},
        {"capacity_scope",configured_capacity?"user-configured capacity; previous ten-contact validation retained separately; no current-boot fixture qualification asserted":"explicit current-boot fixture evidence when required"},
        {"kernel_template_boot_id",kernel_template_boot_id},{"runtime_kernel_boot_id",kernel_profile.at("boot_id")},
        {"song_profile_sha256",adaptive?Json(song.profile_sha):Json(nullptr)},
        {"phase_semantics","added to first-note visual arrival-origin rate=1 prediction; combined phase, not separated input latency"},
        {"trigger",adaptive?"global earliest native group, all native opening aliases and two distinct actual frames":"first native Tap matched on its unique positive-speed approach with actual downward motion"},
        {"input_lead_ns",nullptr},{"input_enabled",true},{"timing_qualified",false},{"live_game_qualified",false},
        {"source_pixel_age_qualified",false},{"audio_enabled",false},{"automatic_play_enabled",false},
        {"full_chart_notes",expected_notes},{"full_compiled_steps",expected_steps},{"expected_plan_sha256",expected_plan_sha},{"contact_capacity",song.capacity},{"projection_candidate",true},
        {"experimental_tap_duration_ns",compile_options.tap_duration_ns},{"experimental_drag_duration_ns",compile_options.drag_duration_ns},
        {"experimental_flick_release_ns",compile_options.flick_release_ns},
        {"experimental_flick_lead_ns",compile_options.flick_lead_ns},{"experimental_flick_travel_ns",compile_options.flick_travel_ns},
        {"experimental_flick_distance_reference_pixels",compile_options.flick_distance_reference_pixels},
        {"experimental_flick_path_policy",flick_path_policy_name(compile_options.flick_path_policy)},
        {"hold_tail_policy_id",hold_tail_policy_name(song.hold_tail_policy)},
        {"judgment_policy_provenance",compiled.plan->metadata.judgment_policy_provenance},
        {"offscreen_policy_id",offscreen_policy_name(song.offscreen_policy)},
        {"max_tangent_error_reference_pixels",song.max_tangent_error_reference_pixels},
        {"max_prejudge_ns",song.max_prejudge_ns},{"advance_above_drags",song.advance_above_drags},
        {"coincident_flick_delay_ns",song.coincident_flick_delay_ns},
        {"advance_below_drags",song.advance_below_drags},
        {"below_drag_source_ids",song.below_drag_source_ids},
        {"outlined_drag_heads",song.outlined_drag_heads},
        {"max_hold_reacquire_gap_ns",song.max_hold_reacquire_gap_ns},
        {"flick_release_scope","hold final stroke position for experimental_flick_release_ns before UP"},
        {"flick_stroke_scope",compile_options.flick_path_policy==FlickPathPolicy::dynamic_line_v1 ?
            "lead/travel/distance recorded above;dynamic line pose retained;normal-offset zero at judgment;not a qualified game hit region" :
            compile_options.flick_path_policy==FlickPathPolicy::judgment_anchored_straight_v1 ?
            "fixed projected judgment anchor and side normal;constant straight stroke;full distance without clipping;unchanged lead/travel/release;not a qualified game hit region" :
            compile_options.flick_path_policy==FlickPathPolicy::judgment_anchored_screen_fit_v1 ?
            "bounded native judgment-domain anchor;explicit screen-fit direction;full straight stroke without clipping;unchanged lead/travel/release;not a qualified game hit region" :
            compile_options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_sampled_v2 ?
            "bounded native judgment anchor;three explicit post-judgment curved-diagonal MOVE samples with first/middle/end certificate;full distance without clipping;game sampling and judgment unqualified" :
            "bounded native judgment anchor held across judgment;two explicit post-judgment diagonal/bend MOVE samples;full distance without clipping;discrete path certificate, not a continuous held-error certificate or qualified game hit region"},
        {"dwell_scope","experimental per-song candidate; Tap/Drag durations are recorded above and bound by the compiled fingerprint; compiler defaults unchanged"},
        {"image_retention",image_retention_name(image_retention)},
        {"HUD_timeline_policy",diagnostic_images?"50ms actual receipt cadence; preallocated grayscale score/combo pixels; save after release; diagnostic only":"50ms actual receipt metadata only; no grayscale/pixel file; save after release"},
        {"max_normal_projection_reference_pixels",song.projection_pixels},{"requires_calibration",true},{"session_limit_seconds",session_limit_ns/second},
        {"runtime_step_budget",song.runtime_step_budget},{"runtime_duration_limit_ns",song.runtime_duration_limit_ns},
        {"input_duration_limit_ns",desktop_limits.input_duration_limit_ns},
        {"manual_settlement_watchdog_ns",desktop_limits.manual_settlement_watchdog_ns},
        {"manual_settlement_grace_ns",desktop_settlement_grace_ns},
        {"manual_settlement_policy",desktop_settlement_policy_name()},
        {"manual_settlement_watchdog_scope","fixed first-current-HUD round cap; independent existing anchor-relative session cap; no guaranteed60s after lastUP, no input-budget extension or observation freshness renewal"},
        {"round_watchdog_seconds",manual_config.round_watchdog_ns/second},{"UI_watchdog_ns",ui_watchdog},{"anchor_receipt_max_age_ns",100*ms},
        {"UI_watchdog_scope","actual recognized playing receipt; unknown does not refresh; pause/result/source loss stop immediately; separate from fixed song timing"},
        {"experimental_HUD_fallback",static_cast<bool>(hud_features)},{"HUD_reference_sha256",hud_features?Json(hud_reference_sha):Json(nullptr)},
        {"HUD_fallback_scope",shared_hud?"explicit shared IN pause/prefix/score glyph candidate; evaluated before and after first-note arm; same-frame denied-state priority; receipt-only":"only after first-note arm, same-frame template unknown; per-song pause/difficulty/title glyphs with negative space; denied-state priority; receipt-only"},
        {"source_sequence_semantics","stream event counter, not rendered-frame ID"},
        {"owner_wait_policy","armed: nonblocking frame read; actual input due within50ms forbids timer waits; <=250us bounded spin then owner UI/stop/tick; other polls<=1ms high-resolution timer with deadline guard; owner-thread ABOVE_NORMAL and balanced1ms resolution; real20ms lateness guard unchanged"},
        {"deadline_scope","cooperative deadlines; retained ADB termination/reader joins and OS calls have no hard wall-time guarantee"},
        {"session_timeout_policy","separate bounded manual-acquisition and anchor-relative playback deadlines; manual READY waiting does not consume playback time"},
        {"journal_policy","64MiB/explicit finite UI rows; pre-arm UI metadata sampled at100ms, every actual observation still processed; armed UI and transport receipts unchanged; flush after release"},
        {"preinput_UI_journal_cadence_ns",100*ms},
        {"journal_row_budget",journal.row_limit},
        {"transport",pas::grpc_transport_manifest(options.grpc_read_chunk_kib)}};
    add_block_area_evidence(manifest,compiled.plan->metadata);
    manifest["numeric_HUD_observer_requested"]=numeric_hud_enabled;
    if(!song.above_drag_source_ids.empty())manifest["above_drag_source_ids"]=song.above_drag_source_ids;
    if(song.native_head_appearance_witness) {
        manifest["native_head_appearance_witness"]=true;
        manifest["native_head_appearance_witness_policy"]=native_head_appearance_witness_json();
    }
    if(clock_probe_proof) {
        manifest["experimental_clock_probe"]=clock_probe_proof_json(*clock_probe_proof);
        manifest["phase_semantics"]="original first-note visual arrival-origin plus phase and static suffix offset; piecewise mapping is an unqualified probe, no global rate=1 qualification";
    }
    if(song.tap_duration_ns!=tap_dwell_default_ns)manifest["tap_dwell_candidate"]=tap_dwell_candidate_json<Json>(song.tap_duration_ns);
    if(native_head_model){manifest["native_head_snapshot_shadow_requested"]=true;
        manifest["native_head_snapshot_shadow"]=native_head_shadow_model_json(*native_head_model,native_head_caller_pose_events);}
    write_json(output/"compiled-plan.json",full_plan_json);write_json(output/"plan.json",manifest);
    if(diagnostic_images)pas::write_diagnostic_png(output/"initial.png",initial_frame);
    SignalGuard signal_guard;pas::PixelCoordinateMap mapping(1280,720,720,1280,90);
    StreamingTouch transport(clock,endpoint,mapping,720,1280,song.capacity,expected_steps,std::chrono::milliseconds(100));
    std::vector<StreamingTouchDispatch> dispatch_schedule;dispatch_schedule.reserve(expected_steps);
    for(const auto& event:compiled.plan->events)dispatch_schedule.push_back({event.song_ns,
        event.phase==rhythm::Phase::up?pas::Phase::up:event.phase==rhythm::Phase::move?pas::Phase::move:pas::Phase::down});
    transport.set_dispatch_schedule(std::move(dispatch_schedule),clock_probe);
    // Establish this exact channel and finish one empty input stream before
    // READY. The probe opens an RPC but sends zero input/touch events.
    const auto connection_begin=clock.now_ns();
    const bool connection_ready=transport.prepare_connection(std::chrono::milliseconds(1000));
    const auto connection_end=clock.now_ns();
    manifest["touch_connection_preparation"]={{"policy","same-channel-empty-input-stream-probe-v1"},
        {"begin_ns",connection_begin},{"return_ns",connection_end},{"timeout_ms",1000},
        {"connected",connection_ready},{"input_RPC_opened",transport.preparation().input_rpc_opened!=0},
        {"touch_events_sent",transport.preparation().touch_events},
        {"scope","connection plus zero-event stream completion; no latency or device-effect guarantee"}};
    manifest["touch_transport_policy"]="ordered-same-due-move-touch-event-batches-v1";
    write_json(output/"plan.json",manifest);
    if(!connection_ready)throw std::runtime_error("touch transport connection not ready before manual READY");
    GuardedTouch touch(transport,clock,expected_steps);
    std::vector<TouchMessageAdmission> write_admissions;write_admissions.reserve(expected_steps);
    write_admissions.resize(expected_steps);write_admissions.clear();
    // Two diagnostic leases replace two repeated RGB copies. Reserve two
    // corresponding pool slots so producer headroom stays bounded and available.
    pas::LatestFrame latest(1280,720,5,&clock);std::atomic<bool> capture_done{false},stop_file{false};
    std::mutex error_mutex;std::exception_ptr capture_error;
    std::unique_ptr<ExperimentalContactRuntime> runtime;
    const auto started=clock.now_ns();auto status=lifecycle.status(started);
    Ns session_deadline_host_ns=started+session_limit_ns;
    manifest["acquisition_deadline_host_ns"]=session_deadline_host_ns;
    Ns next_candidate=started,last_saved=-1,kernel_request=-1,first_due=-1,next_acquisition_report=started,last_acquisition_saved=-1;
    const bool sample_opening_by_receipt=adaptive&&desktop;
    Ns next_first_kernel_probe=started;
    Ns first_kernel_valid_after=started;
    std::size_t first_kernel_probes=0,first_kernel_rejected=0;
    std::uint64_t consumed=0,candidate_sequence=0;
    bool armed=false,kernel_started=false,kernel_ready=false,finished=false,release_known=false,final_zero=false,sequence_complete=false;
    std::string failure,opening_reason="waiting_for_native_note_centers";
    // Opening diagnostics must not allocate/fault a fresh multi-megabyte RGB
    // vector on each input deadline. All 48 bounded buffers are touched now.
    std::vector<pas::Frame> saved_frames(diagnostic_images?48:0);
    for(auto& diagnostic:saved_frames)diagnostic.rgb.resize(1280u*720u*3u);
    std::size_t saved_count=0;
    // Bounded pre-input diagnostics, saved only after cleanup. A failed
    // acquisition must retain actual pixels, not just center coordinates.
    std::vector<pas::Frame> acquisition_frames;if(diagnostic_images)acquisition_frames.reserve(24);
    std::optional<pas::Frame> anchor_frame,result_frame,first_unknown_frame;
    // At most two distinct pool leases survive an iteration. One-time anchor/
    // result/first-unknown diagnostics own copies, leaving producer headroom.
    std::shared_ptr<const pas::Frame> last_frame,last_unknown_frame;
    Ns last_ui_append_begin=-1,last_ui_append_end=-1,last_tick_begin=-1,last_tick_end=-1;
    Ns last_preinput_ui_journal_ns=-1;
    std::size_t preinput_ui_journal_skips=0;
    Ns last_opening_detector_journal_ns=-1;
    std::size_t opening_detector_journal_rows=0;
    Ns last_snapshot_begin=-1,last_snapshot_end=-1;
    Ns last_owner_frame_begin=-1,last_classify_end=-1,last_HUD_end=-1;
    std::uint64_t last_owner_sequence=0;std::size_t redundancy_frames=0;
    std::size_t hud_fallback_frames=0;Json kernel_before=nullptr;std::future<Json> kernel_pending;
    std::optional<ExperimentalUiObservation> last_playing_ui;
    const auto ui_failure=[&](Ns now)->const char* {
        if(!last_playing_ui||!last_playing_ui->playing||!last_playing_ui->source_valid||
           now<last_playing_ui->received_host_ns)return "actual_playing_UI_missing_or_invalid";
        if(now-last_playing_ui->received_host_ns>=ui_watchdog)return "actual_UI_receipt_expired";
        if(!foreground_watch.status().healthy)return "actual_foreground_invalid";
        return nullptr;
    };
    const auto ui_healthy=[&](Ns now){return ui_failure(now)==nullptr;};
    const auto runtime_ui=[&](Ns now) {
        auto observation=last_playing_ui.value_or(ExperimentalUiObservation{});
        observation.playing=ui_healthy(now);return observation;
    };
    std::optional<std::uint64_t> first_down_intent;
    std::string last_first_kernel_probe_error;
    const auto harvest_desktop_first_kernel=[&] {
        if(!kernel_pending.valid()||kernel_pending.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)
            return false;
        try {
            auto snapshot=kernel_pending.get();
            kernel_request=snapshot.at("host_request_begin_ns").get<Ns>();
            kernel_ready=kernel_request>=first_kernel_valid_after&&snapshot.at("active_count")==0&&
                snapshot.at("causal_host_interval_valid")==true;
            kernel_before=std::move(snapshot);
        } catch(const std::exception& error) {
            kernel_ready=false;
            last_first_kernel_probe_error=error.what();
        }
        if(!kernel_ready)++first_kernel_rejected;
        return true;
    };
    const auto service_desktop_first_kernel=[&](Ns now) {
        if(!sample_opening_by_receipt||first_down_intent)return;
        if(harvest_desktop_first_kernel()&&!kernel_ready&&armed)
            throw std::runtime_error("pre_down_kernel_not_fresh_zero");
        const bool needs_probe=!armed||
            (now>=first_due-350*ms&&(!kernel_ready||first_due-kernel_request>=500*ms));
        if(needs_probe&&!kernel_pending.valid()&&now>=next_first_kernel_probe&&ui_healthy(now)) {
            next_first_kernel_probe=now+desktop_kernel_probe_interval;
            ++first_kernel_probes;
            kernel_pending=std::async(std::launch::async,[&kernel]{return kernel.query();});
        }
    };
    touch.guard=[&](const pas::TouchCommand& command)->std::string {
        const auto now=clock.now_ns();
        if(interrupted.load()||stop_file.load()||(GetAsyncKeyState(VK_ESCAPE)&0x8000))return "user_stop";
        if(now>=session_deadline_host_ns)return armed?"playback_session_timeout":"manual_acquisition_timeout";
        if(const auto rejected=ui_failure(now))return rejected;
        if(command.phase==pas::Phase::down&&!first_down_intent)first_down_intent=command.intent_id;
        if(command.phase==pas::Phase::down&&command.intent_id==first_down_intent) {
            if(sample_opening_by_receipt)(void)harvest_desktop_first_kernel();
            const auto checked_at=clock.now_ns();
            if(!kernel_ready||checked_at<kernel_request||checked_at-kernel_request>=500*ms)
                return "first_DOWN_kernel_zero_not_fresh";
        }
        return {};
    };
    transport.before_write=touch.guard;
    transport.before_message_write=[&](const pas::TouchCommand& command)->std::string {
        if(write_admissions.size()>=expected_steps)return "message_write_admission_trace_budget";
        TouchMessageAdmission admission;admission.command=command;admission.check_begin_ns=clock.now_ns();
        admission.reason=touch.guard(command);
        if(last_playing_ui) {
            const auto& actual=*last_playing_ui;admission.sequence=actual.sequence;admission.epoch=actual.epoch;
            admission.generation=actual.generation;admission.geometry_version=actual.geometry_version;
            admission.received_ns=actual.received_host_ns;admission.playing=actual.playing;admission.source_valid=actual.source_valid;
            admission.source_matches=actual.source_id==source_id&&actual.width==1280&&actual.height==720;
        }
        admission.foreground_healthy=foreground_watch.status().healthy;
        if(admission.reason.empty()&&(!admission.source_matches||admission.epoch!=acquisition_epoch||
           admission.generation!=1||admission.geometry_version!=1||!admission.foreground_healthy))
            admission.reason="actual_UI_source_epoch_geometry_or_foreground_invalid_before_message_write";
        admission.allowed=admission.reason.empty();admission.check_return_ns=clock.now_ns();
        write_admissions.push_back(std::move(admission));return write_admissions.back().reason;
    };
    std::jthread stop_worker([&](std::stop_token stop){
        while(!stop.stop_requested()) {
            try{if(std::filesystem::exists(output/"STOP")){stop_file=true;break;}}
            catch(...){stop_file=true;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    std::atomic_bool recovery_enabled=false;
    std::mutex publication_mutex;
    FreshCaptureRecovery capture_recovery(options.source_rotation);
    struct RecoveryProbeTrace {
        Ns request_ns=0,return_ns=0,received_ns=-1,source_timestamp_us=-1;
        std::uint64_t sequence=0;bool accepted=false;std::string reason;
    };
    std::vector<RecoveryProbeTrace> recovery_probes;recovery_probes.reserve(4096);
    std::size_t recovery_stream_drops=0,recovery_pool_drops=0;
    const auto publish_current_frame=[&](pas::Frame& frame,RecoveryRoute route)->RecoveryAdmission {
        // Both routes have already normalized and validated their own complete
        // RPC payload. This lock orders actual receipts and source metadata;
        // it never changes capture_complete_ns to a publication timestamp.
        std::lock_guard lock(publication_mutex);
        RecoveryFrameMeta meta{frame.capture_complete_ns,frame.source_timestamp_us.value_or(-1),
            frame.width,frame.height,frame.stride,frame.source_rotation,frame.source_valid};
        auto proposed=capture_recovery;
        const auto admitted=proposed.admit(meta,route,clock.now_ns(),recovery_enabled.load());
        if(!admitted.accepted){if(route==RecoveryRoute::stream)++recovery_stream_drops;return admitted;}
        frame.sequence=admitted.sequence;frame.generation=1;frame.geometry_version=1;
        if(!latest.publish(frame.rgb.data(),frame.rgb.size(),frame)){
            ++recovery_pool_drops;return {false,0,"normalized_frame_pool_drop"};
        }
        capture_recovery=proposed;
        return admitted;
    };
    std::jthread recovery_worker([&](std::stop_token stop){
        while(!stop.stop_requested()) {
            bool probe=false;
            {std::lock_guard lock(publication_mutex);probe=capture_recovery.should_probe(clock.now_ns(),recovery_enabled.load());}
            if(probe) {
                RecoveryProbeTrace trace;trace.request_ns=clock.now_ns();
                try {
                    auto frame=recovery_capture.snapshot(std::chrono::milliseconds(100));
                    trace.received_ns=frame.capture_complete_ns;trace.source_timestamp_us=frame.source_timestamp_us.value_or(-1);
                    frame.receive_start_ns=trace.request_ns;frame.notification_received_ns=frame.capture_complete_ns;
                    frame.capture_backend="emulator-grpc-getScreenshot-recovery";
                    const auto admitted=publish_current_frame(frame,RecoveryRoute::snapshot);
                    trace.accepted=admitted.accepted;trace.sequence=admitted.sequence;trace.reason=admitted.reason;
                }catch(const std::exception& error){trace.reason=error.what();}
                trace.return_ns=clock.now_ns();recovery_probes.push_back(std::move(trace));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    std::jthread capture_worker([&](std::stop_token stop){
        try{capture.stream(stop,[&](pas::Frame&& frame){publish_current_frame(frame,RecoveryRoute::stream);});}
        catch(...){std::lock_guard lock(error_mutex);capture_error=std::current_exception();}
        capture_done=true;latest.close();
    });
    if(desktop){
        desktop->stage="waiting_for_play";desktop->ready_seen=true;desktop->ready_host_ns=clock.now_ns();
        desktop->execution_state="unknown";desktop->result_state="unknown";
    }
    std::cout<<"WHOLE SONG EXPERIMENT READY: manually start/retry "<<song.song<<' '<<song.difficulty<<" once; "<<expected_notes<<" notes / "<<expected_steps<<" steps / capacity "<<song.capacity<<".\n"<<std::flush;
    try {
        while(!finished) {
            auto now=clock.now_ns();
            if(interrupted.load()||stop_file.load()||(GetAsyncKeyState(VK_ESCAPE)&0x8000))throw std::runtime_error("user_stop");
            if(now>=session_deadline_host_ns)
                throw std::runtime_error(armed?"playback_session_timeout":"manual_acquisition_timeout");
            if(capture_done.load()){std::lock_guard lock(error_mutex);if(capture_error)std::rethrow_exception(capture_error);throw std::runtime_error("capture_ended");}
            if(!foreground_watch.status().healthy)throw std::runtime_error("foreground_unavailable");
            if(const auto stream_fault=transport.poll();!stream_fault.empty())throw std::runtime_error(stream_fault);
            status=lifecycle.poll(now);
            if(runtime&&!sequence_complete) {
                const auto current=runtime->status();
                if(current.state==ExperimentalRuntimeState::stopped)throw std::runtime_error(current.reason);
            }
            const auto frame=latest.read_after(consumed,armed&&!sequence_complete?0:10*ms);
            if(frame) {
                consumed=frame->sequence;
                last_owner_sequence=frame->sequence;last_owner_frame_begin=clock.now_ns();
                if(diagnostic_images)last_frame=frame;
                const auto classified=classify_screen_state({frame->width,frame->height,frame->stride,frame->rgb},screen_calibration.templates);
                last_classify_end=clock.now_ns();
                Json feature_diagnostic=nullptr;bool feature_playing=false;
                HudGlyphEvidence primary_features;
                bool pause_ok=false,prefix_ok=false,score_ok=false;
                std::string hud_feature_reason="waiting_for_playing_HUD";
                if((armed&&classified.state==ObservedGameState::unknown)||shared_hud) {
                    if(diagnostic_images&&classified.state==ObservedGameState::unknown) {
                        if(!first_unknown_frame)first_unknown_frame=*frame;
                        last_unknown_frame=frame;
                    }
                    if(hud_features&&hud_candidate_eligible(shared_hud,armed,frame->source_valid,classified)) {
                        // Feature-only path has no invented per-frame SHA. This
                        // row retains the actual source/sequence/receipt; saved
                        // diagnostic pixels are hashed after all input stops.
                        primary_features=hud_features->observe_features({frame->width,frame->height,frame->stride,frame->rgb},classified.state);
                        const auto& features=primary_features;
                        feature_playing=features.valid&&features.hud_candidate;
                        pause_ok=features.fixed_glyphs[0].glyph_present&&features.fixed_glyphs[0].negative_space_present;
                        prefix_ok=features.fixed_glyphs[1].glyph_present&&features.fixed_glyphs[1].negative_space_present&&features.difficulty_alignment_unique_family;
                        score_ok=features.score.glyph_structure_candidate;hud_feature_reason=features.reason;
                        feature_diagnostic={{"candidate",feature_playing},{"reason",features.reason},
                            {"pause",hud_glyph_json(features.fixed_glyphs[0])},{"difficulty",hud_glyph_json(features.fixed_glyphs[1])},
                            {"title",hud_glyph_json(features.title_glyph)},{"frame_hash_available",false},
                            {"title_required",features.title_required},{"difficulty_alignment_x",features.difficulty_alignment_x},
                            {"difficulty_alignment_candidates",features.difficulty_alignment_candidates},
                            {"difficulty_alignment_lower_x",features.difficulty_alignment_lower_x},
                            {"difficulty_alignment_upper_x",features.difficulty_alignment_upper_x},
                            {"difficulty_alignment_unique_family",features.difficulty_alignment_unique_family},
                            {"score",{{"components",features.score.components},{"boundary_components",features.score.boundary_components},
                                {"calibrated_cells",features.score.calibrated_cells},{"supported_cells",features.score.supported_cells},
                                {"trimmed_left_pixels",features.score.trimmed_left_pixels},
                                {"original_left_bounds",pixel_region_json(features.score.original_left_bounds)},
                                {"retained_left_bounds",pixel_region_json(features.score.retained_left_bounds)},
                                {"glyph_structure_candidate",features.score.glyph_structure_candidate}}}};
                        if(feature_playing)++hud_fallback_frames;
                    }
                }
                Json redundancy_diagnostic=nullptr;
                if(hud_redundancy) {
                    LearnedHudFrame meta;meta.sequence=frame->sequence;meta.epoch=acquisition_epoch;
                    meta.generation=frame->generation;meta.geometry_version=frame->geometry_version;meta.source_id=source_id;
                    meta.received_ns=frame->capture_complete_ns;meta.processed_ns=clock.now_ns();
                    meta.source_valid=frame->source_valid;meta.rotation=frame->source_rotation;
                    const auto& redundant=hud_redundancy->observe({frame->width,frame->height,frame->stride,frame->rgb},
                        primary_features,classified,meta,armed);
                    if(redundant.current_candidate&&!sequence_complete){feature_playing=true;++redundancy_frames;}
                    redundancy_diagnostic={{"policy",redundancy_policy},{"ready",redundant.ready},
                        {"current_candidate",redundant.current_candidate},{"reason",redundant.reason},
                        {"auxiliary_valid",redundant.auxiliary.valid},{"auxiliary_HUD_candidate",redundant.auxiliary.hud_candidate},
                        {"auxiliary_score_candidate",redundant.auxiliary.score.glyph_structure_candidate},
                        {"auxiliary_score_boundary_components",redundant.auxiliary.score.boundary_components},
                        {"reference_sequence",redundant.reference_frame.sequence},{"support_sequence",redundant.support_frame.sequence},
                        {"construction_attempts",redundant.construction_attempts},{"support_frames",redundant.support_frames},
                        {"merged_score",{{"candidate",redundant.merged_score.candidate},{"reason",redundant.merged_score.reason},
                            {"component_xywh",{redundant.merged_score.component.x,redundant.merged_score.component.y,
                                redundant.merged_score.component.width,redundant.merged_score.component.height}},
                            {"spanned_windows",redundant.merged_score.spanned_windows},{"textured_windows",redundant.merged_score.textured_windows},
                            {"boundary_fragments",redundant.merged_score.boundary_fragments},
                            {"clean_components",redundant.merged_score.clean_components},
                            {"boundary_textured_windows",redundant.merged_score.boundary_textured_windows},
                            {"white_pixels",redundant.merged_score.white_pixels},{"negative_pixels",redundant.merged_score.negative_pixels},
                            {"occupancy",redundant.merged_score.occupancy}}},
                        {"title",hud_glyph_json(redundant.auxiliary.title_glyph)},
                        {"difficulty",hud_glyph_json(redundant.auxiliary.fixed_glyphs[1])}};
                    Json fragment_bounds=Json::array();
                    for(std::size_t i=0;i<std::min<std::size_t>(8,redundant.merged_score.boundary_fragments);++i){
                        const auto& bounds=redundant.merged_score.boundary_regions[i];
                        fragment_bounds.push_back({bounds.x,bounds.y,bounds.width,bounds.height});
                    }
                    redundancy_diagnostic["merged_score"]["boundary_regions_xywh"]=std::move(fragment_bounds);
                    if(!redundant.auxiliary.score.glyph_structure_candidate){
                        Json components=Json::array();
                        for(std::size_t i=0;i<std::min<std::size_t>(64,redundant.auxiliary.score.glyphs.size());++i){
                            const auto& glyph=redundant.auxiliary.score.glyphs[i];
                            components.push_back({{"bounds_xywh",{glyph.bounds.x,glyph.bounds.y,glyph.bounds.width,glyph.bounds.height}},
                                {"white_pixels",glyph.white_pixels},{"negative_pixels",glyph.negative_pixels},
                                {"occupancy",glyph.occupancy},{"plausible_stroke",glyph.plausible_stroke}});
                        }
                        redundancy_diagnostic["auxiliary_score_components_bounded"]=std::move(components);
                    }
                }
                if(feature_diagnostic.is_object())feature_diagnostic["candidate_with_redundancy"]=feature_playing;
                last_HUD_end=clock.now_ns();
                ManualObservation observation;observation.sequence=frame->sequence;observation.generation=frame->generation;observation.geometry_version=frame->geometry_version;
                observation.width=frame->width;observation.height=frame->height;observation.rotation=frame->source_rotation;
                observation.timestamp_kind=ManualTimestampKind::arrival_only;observation.received_host_ns=frame->capture_complete_ns;
                observation.source_valid=frame->source_valid;observation.evidence_id="whole-song/frame-"+std::to_string(frame->sequence);
                observation.pause=hud_calibration_matches(classified,ObservedGameState::paused);
                observation.result=hud_calibration_matches(classified,ObservedGameState::result);
                Json result_diagnostic=nullptr;
                if(result_labels&&result_label_fallback_eligible(sequence_complete,frame->source_valid,classified)) {
                    const auto evidence=result_labels->observe({frame->width,frame->height,frame->stride,frame->rgb});
                    observation.result=evidence.candidate;
                    Json labels=Json::array();
                    for(const auto& label:evidence.labels)labels.push_back({{"supported_fraction",label.supported_fraction},
                        {"unexpected_fraction",label.unexpected_fraction},{"ink_ratio",label.ink_ratio},{"local_contrast",label.local_contrast}});
                    result_diagnostic={{"policy",result_policy},{"candidate",evidence.candidate},
                        {"reference",evidence.calibration_id},{"alignment_x",evidence.alignment_x},{"alignment_y",evidence.alignment_y},
                        {"labels",std::move(labels)},{"scope","terminal UI only after completed input; score/judgment unreviewed"}};
                }
                observation.hud=!observation.pause&&!observation.result&&(shared_hud?feature_playing:(classified.state==ObservedGameState::playing||feature_playing));
                now=clock.now_ns();
                HudTimelineMeta timeline_meta;timeline_meta.sequence=frame->sequence;timeline_meta.epoch=acquisition_epoch;
                timeline_meta.generation=frame->generation;timeline_meta.geometry_version=frame->geometry_version;
                timeline_meta.received_ns=frame->capture_complete_ns;timeline_meta.processed_ns=now;
                timeline_meta.source_timestamp_us=frame->source_timestamp_us;timeline_meta.raw_class=classified.state;
                timeline_meta.effective_HUD=observation.hud;timeline_meta.source_valid=frame->source_valid;
                hud_timeline.observe({frame->width,frame->height,frame->stride,frame->rgb},timeline_meta,armed);
                if(numeric_hud&&armed&&observation.hud&&observation.source_valid&&
                   now>=frame->capture_complete_ns&&now-frame->capture_complete_ns<ui_watchdog)
                    numeric_hud->submit(frame->sequence,frame->source_sequence,frame->source_timestamp_us,
                        frame->capture_complete_ns,now,acquisition_epoch,frame->generation,frame->geometry_version,
                        frame->width,frame->height,frame->stride,frame->rgb);
                status=lifecycle.observe(observation,now);
                if(observation.hud&&observation.source_valid&&now>=observation.received_host_ns&&
                   now-observation.received_host_ns<ui_watchdog) {
                    ExperimentalUiObservation actual;actual.sequence=observation.sequence;
                    actual.epoch=acquisition_epoch;actual.generation=actual.geometry_version=1;actual.width=1280;actual.height=720;
                    actual.received_host_ns=observation.received_host_ns;actual.source_id=source_id;
                    actual.evidence_id=observation.evidence_id;actual.playing=actual.source_valid=true;last_playing_ui=std::move(actual);
                    if(desktop&&desktop->first_HUD_receipt_ns<0){
                        desktop->first_HUD_receipt_ns=observation.received_host_ns;desktop->stage="first_note";
                    }
                    if(!armed)service_desktop_first_kernel(now);
                } else if(observation.pause||observation.result||!observation.source_valid) {
                    last_playing_ui.reset();
                    if(sample_opening_by_receipt&&!armed) {
                        kernel_ready=false;first_kernel_valid_after=next_first_kernel_probe=now;
                    }
                    if(armed&&!sequence_complete)throw std::runtime_error("explicit_pause_result_or_source_loss");
                }
                if(!armed&&now>=next_acquisition_report) {
                    next_acquisition_report=now+second;
                    // Diagnostic only: this never updates UI permission,
                    // evidence timestamps, the trigger, or any clock estimate.
                    const bool report_hud=observation.hud&&ui_healthy(now);
                    const bool partial_hud=(pause_ok&&prefix_ok)||(pause_ok&&score_ok)||(prefix_ok&&score_ok);
                    const char* stage=report_hud?"waiting_for_first_note":
                        partial_hud?"waiting_for_valid_HUD":"waiting_for_manual_play";
                    const Json progress={{"schema","pas.opening-acquisition-progress.v1"},{"stage",stage},
                        {"reason",report_hud?opening_reason:observation.hud?"actual_UI_source_or_foreground_not_ready":hud_feature_reason},
                        {"pause_ok",pause_ok},{"prefix_ok",prefix_ok},{"score_ok",score_ok},{"hud_ok",report_hud}};
                    std::cout<<"ACQUISITION STATUS: "<<progress.dump()<<'\n'<<std::flush;
                }
                // READY waiting has a separate bounded duration. Sampling only
                // its diagnostic rows prevents that wait consuming the active
                // song's journal budget; it never throttles real observations,
                // HUD learning, frame freshness, opening detection or input.
                if(armed||last_preinput_ui_journal_ns<0||now-last_preinput_ui_journal_ns>=100*ms) {
                if(!armed)last_preinput_ui_journal_ns=now;
                const auto previous_append_begin=last_ui_append_begin,previous_append_end=last_ui_append_end;
                last_ui_append_begin=clock.now_ns();
                journal.append({{"event","ui"},{"sequence",frame->sequence},{"received_ns",frame->capture_complete_ns},{"processed_ns",now},
                    {"capture_route",frame->capture_backend},{"source_sequence",frame->source_sequence?Json(*frame->source_sequence):Json(nullptr)},
                    {"pixels_ready_ns",frame->pixels_ready_ns},{"published_ns",frame->published_ns},
                    {"copy_start_ns",frame->copy_start_ns?Json(*frame->copy_start_ns):Json(nullptr)},
                    {"copy_end_ns",frame->copy_end_ns?Json(*frame->copy_end_ns):Json(nullptr)},
                    {"owner_frame_begin_ns",last_owner_frame_begin},{"classify_end_ns",last_classify_end},{"HUD_end_ns",last_HUD_end},
                    {"previous_ui_append_begin_ns",previous_append_begin},{"previous_ui_append_end_ns",previous_append_end},
                    {"previous_tick_begin_ns",last_tick_begin},{"previous_tick_end_ns",last_tick_end},
                    {"previous_snapshot_begin_ns",last_snapshot_begin},{"previous_snapshot_end_ns",last_snapshot_end},
                    {"source_timestamp_us",frame->source_timestamp_us?Json(*frame->source_timestamp_us):Json(nullptr)},
                    {"state",manual_state_name(status.state)},{"round",status.round},{"reason",status.reason},{"source_valid",frame->source_valid},
                    {"screen_class",static_cast<int>(classified.state)},{"template_scores",template_scores_json(classified)},
                    {"experimental_result_labels",std::move(result_diagnostic)},
                    {"playing_redundancy",std::move(redundancy_diagnostic)},
                    {"experimental_HUD_features",std::move(feature_diagnostic)},
                    {"actual_playing_receipt_ns",last_playing_ui?Json(last_playing_ui->received_host_ns):Json(nullptr)}});
                last_ui_append_end=clock.now_ns();
                } else ++preinput_ui_journal_skips;
                if(armed) {
                    if(!sequence_complete&&observation.hud) {
                        const auto current=runtime->observe_ui(runtime_ui(clock.now_ns()));
                        if(current.state==ExperimentalRuntimeState::stopped)throw std::runtime_error(current.reason);
                    }
                    if(native_head_model&&native_head_rows&&!native_head_rows->disabled&&!sequence_complete&&
                       observation.hud&&observation.source_valid&&native_head_rows->origin_host_ns>=0) {
                        auto& shadow=*native_head_rows;
                        const auto prediction=frame->capture_complete_ns-shadow.origin_host_ns;
                        if(prediction>=native_head_model->window().begin_ns&&prediction<=native_head_model->window().end_ns) {
                            const auto diagnostic_now=clock.now_ns();const auto next=runtime->next_wake_ns();
                            if(shadow.size>=shadow.rows.size())++shadow.capacity_skips;
                            else if(shadow.last_received_ns>=0&&frame->capture_complete_ns-shadow.last_received_ns<250*ms)++shadow.cadence_skips;
                            else if(!next||*next<diagnostic_now+10*ms)++shadow.near_due_skips;
                            else {
                                auto& row=shadow.rows[shadow.size++];shadow.last_received_ns=frame->capture_complete_ns;
                                row.sequence=frame->sequence;row.epoch=acquisition_epoch;row.generation=frame->generation;
                                row.geometry_version=frame->geometry_version;row.source_sequence=frame->source_sequence;
                                row.source_timestamp_us=frame->source_timestamp_us;row.received_ns=frame->capture_complete_ns;
                                row.processing_begin_ns=diagnostic_now;row.predicted_song_ns=prediction;row.next_wake_at_admission_ns=*next;
                                try {
                                    const auto detected=detect_adaptive_opening_centers(frame->width,frame->height,frame->stride,frame->rgb,NoteKind::tap);
                                    row.detector_valid=detected.valid;row.detector_reason.assign(detected.reason.data(),std::min<std::size_t>(detected.reason.size(),256));
                                    if(detected.centers.size()>row.centers.size())throw std::runtime_error("native head detector center budget");
                                    row.center_count=detected.centers.size();
                                    for(std::size_t i=0;i<row.center_count;++i)row.centers[i]=detected.centers[i];
                                    if(detected.valid&&!detected.centers.empty())
                                        row.matches=native_head_model->match(std::span<const VisualCenter>(row.centers.data(),row.center_count),prediction);
                                    else row.matches.reason="detector_invalid_or_empty_unknown";
                                }catch(const std::exception& error){const std::string_view message=error.what();
                                    row.error.assign(message.data(),std::min<std::size_t>(message.size(),256));shadow.disabled=true;}
                                row.processed_ns=clock.now_ns();
                            }
                        }
                    }
                    // Evidence is bounded to the opening judgment window. The
                    // full stream is classified but not copied into RAM images.
                    if(diagnostic_images&&now>=first_due-200*ms&&now<=first_due+1200*ms&&saved_count<saved_frames.size()&&
                       (last_saved<0||frame->capture_complete_ns-last_saved>=20*ms)) {
                        last_snapshot_begin=clock.now_ns();
                        // The published geometry is fixed/tightly packed, so
                        // vector copy-assignment reuses this allocated capacity.
                        saved_frames[saved_count++]=*frame;last_saved=frame->capture_complete_ns;
                        last_snapshot_end=clock.now_ns();
                    }
                    if(status.result_confirmed)result_frame=*frame;
                } else if(status.round==1&&ui_healthy(now)&&
                          (sample_opening_by_receipt?frame->capture_complete_ns:now)>=next_candidate) {
                    // Use actual receipt spacing. A scan that took a while must
                    // not delay the next eligible source frame by the same work.
                    next_candidate=sample_opening_by_receipt?frame->capture_complete_ns+16*ms:now+(adaptive?25:75)*ms;
                    ModelOpeningDetection detected;
                    if(adaptive)detected=detect_model_opening_centers(frame->width,frame->height,frame->stride,frame->rgb,adaptive_trigger->description());
                    else static_cast<calibration::OpeningDetection&>(detected)=
                        calibration::detect_opening_centers(frame->width,frame->height,frame->stride,frame->rgb);
                    // Bounded metadata also records rejected shapes. Missing
                    // detector candidates must be diagnosable with result-only
                    // image retention; this grants no input or freshness.
                    if(opening_detector_journal_rows<1024&&(last_opening_detector_journal_ns<0||
                       frame->capture_complete_ns-last_opening_detector_journal_ns>=250*ms)) {
                        last_opening_detector_journal_ns=frame->capture_complete_ns;++opening_detector_journal_rows;
                        Json metrics=Json::array();
                        for(const auto& metric:detected.metrics)metrics.push_back({{"x",metric.x},{"y",metric.y},
                            {"width",metric.width},{"rows",metric.rows}});
                        journal.append({{"event","opening_detector"},{"sequence",frame->sequence},
                            {"received_ns",frame->capture_complete_ns},{"processed_ns",clock.now_ns()},
                            {"valid",detected.valid},{"reason",detected.reason},{"groups",detected.groups},
                            {"qualifying_runs",detected.qualifying_runs},{"center_count",detected.centers.size()},
                            {"unavailable_note_kinds",detected.unavailable_kinds},
                            {"metrics",std::move(metrics)},{"pixels_retained",false},{"input_permission",false}});
                    }
                    if(!detected.valid||detected.centers.empty()) {
                        opening_reason=detected.reason;
                    } else {
                        if(diagnostic_images&&acquisition_frames.size()<24&&(last_acquisition_saved<0||
                           frame->capture_complete_ns-last_acquisition_saved>=100*ms)) {
                            acquisition_frames.push_back(*frame);last_acquisition_saved=frame->capture_complete_ns;
                        }
                        VisualCandidateFrame visual;visual.sequence=++candidate_sequence;visual.epoch=acquisition_epoch;visual.generation=visual.geometry_version=1;
                        visual.width=1280;visual.height=720;visual.source_id=source_id;visual.source_sha256=source_sha;
                        visual.frame_id=visual.evidence_id=observation.evidence_id;visual.frame_sha256=rgb_sha(*frame);visual.clock_id="host-QPC-receipt-only";
                        visual.opening_context_id="manual-round-1-user-selected-"+song.song;visual.clock_kind=VisualSourceClock::arrival_only;
                        visual.received_host_ns=frame->capture_complete_ns;visual.source_valid=frame->source_valid;
                        // The renderer assumes coverage only for detector kinds
                        // that were available. Actual completeness is unqualified.
                        visual.centers_complete_unoccluded=true;visual.centers=detected.centers;
                        const auto candidate_now=clock.now_ns();
                        if(candidate_now-visual.received_host_ns>=100*ms) {
                            opening_reason="receipt_expired_during_processing";
                            trigger_health(candidate_now,false);journal.append({{"event","candidate_dropped_before_solver"},{"frame",visual.evidence_id},
                                {"received_ns",visual.received_host_ns},{"processed_ns",candidate_now},{"reason","receipt_expired_during_processing"}});continue;
                        }
                        VisualCandidateResult result;FirstNoteTriggerStatus trigger_status;
                        if(adaptive) {
                            trigger_status=adaptive_trigger->observe(visual,detected.center_families,ui_healthy(candidate_now),candidate_now,
                                detected.unavailable_kinds);
                            result.candidates=std::move(trigger_status.raw_candidates);
                            result.song_intervals=trigger_status.selected_intervals;result.reason=trigger_status.reason;
                        } else {
                            result=candidates->observe(visual,candidate_now);
                            const auto observed_now=clock.now_ns();trigger_status=trigger->observe(result,ui_healthy(observed_now),observed_now);
                        }
                        opening_reason=trigger_status.reason;
                        const auto observed_now=clock.now_ns();
                        Json centers=Json::array();for(const auto& center:detected.centers)centers.push_back({{"id",center.id},{"x",center.x},{"y",center.y}});
                        journal.append({{"event","candidate"},{"frame",visual.evidence_id},
                            {"detector_reason",detected.reason},{"detector_coverage_verified",detected.centers_complete_unoccluded},
                            {"solver_complete_coverage_assumption",visual.centers_complete_unoccluded},{"rgb_sha256",visual.frame_sha256},{"centers",centers},
                            {"center_families",detected.center_families},{"unavailable_note_kinds",detected.unavailable_kinds},
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
                            candidates->begin_epoch(++acquisition_epoch);trigger=std::make_unique<FirstNoteTrigger>(chart,model,static_cast<Ns>(phase)*ms);
                            continue;
                        }
                        if(trigger_status.state==FirstNoteTriggerState::cancelled)
                            throw std::runtime_error("first_note_trigger_cancelled: "+trigger_status.reason);
                        const auto consume_now=clock.now_ns();
                        const bool before_runtime_window=trigger_status.state==FirstNoteTriggerState::ready&&trigger_status.anchor&&
                            trigger_status.anchor->target_host_ns-consume_now>3*second;
                        if(before_runtime_window) {
                            // An early visual solution is not consumed. Subsequent
                            // actual frames must produce a new fresh solution in
                            // the runtime's existing three-second arm window.
                            opening_reason="waiting_for_runtime_dispatch_window";
                            journal.append({{"event","first_note_ready_before_runtime_window"},
                                {"evidence_id",trigger_status.anchor->evidence_id},
                                {"received_ns",trigger_status.anchor->received_host_ns},{"processed_ns",consume_now},
                                {"target_host_ns",trigger_status.anchor->target_host_ns},{"consumed",false}});
                        }
                        const auto resolved=before_runtime_window?std::optional<FirstNoteTriggerAnchor>{}:
                            adaptive?adaptive_trigger->take_anchor(consume_now):trigger->take_anchor(consume_now);
                        if(resolved) {
                            ExperimentalAnchor anchor;anchor.origin_host_ns=resolved->origin_host_ns;
                            anchor.received_host_ns=resolved->received_host_ns;anchor.explicit_phase_ns=static_cast<Ns>(phase)*ms;
                            anchor.chosen_song_interval=resolved->chosen_song_interval;anchor.epoch=acquisition_epoch;anchor.generation=anchor.geometry_version=1;
                            anchor.source_id=visual.source_id;anchor.source_sha256=visual.source_sha256;anchor.clock_id=visual.clock_id;
                            anchor.evidence_id=resolved->evidence_id;anchor.supporting_evidence_ids=resolved->supporting_evidence_ids;
                            const auto construction_begin=clock.now_ns();
                            runtime=std::make_unique<ExperimentalContactRuntime>(std::move(prepared_runtime),clock,touch,anchor);
                            const auto construction_end=clock.now_ns();
                            // The original actual receipt is intentionally retained
                            // across binding; preparation cannot renew it.
                            const auto initial=runtime->observe_ui(runtime_ui(clock.now_ns()));
                            if(initial.selected_contacts!=expected_notes||initial.selected_steps!=expected_steps||initial.peak_contacts!=expected_peak)
                                throw std::runtime_error("runtime must retain every full-plan contact and step");
                            if(!runtime->arm())throw std::runtime_error(runtime->status().reason);
                            armed=true;recovery_enabled=true;first_due=anchor.origin_host_ns+anchor.explicit_phase_ns+clock_probe.mapped_song_ns(first_song_ns);
                            if(desktop)desktop->stage="pre_first_touch";
                            if(native_head_rows)native_head_rows->origin_host_ns=anchor.origin_host_ns;
                            // The chart's actual anchor fixes this deadline once.
                            // Neither fresh frames nor planner ticks can renew it.
                            session_deadline_host_ns=anchor.origin_host_ns+anchor.explicit_phase_ns+session_limit_ns;
                            manifest["playback_deadline_host_ns"]=session_deadline_host_ns;
                            manifest["manual_acquisition_elapsed_ns"]=construction_end-started;
                            if(diagnostic_images)anchor_frame=*frame;
                            manifest["anchor"]={{"origin_host_ns",anchor.origin_host_ns},{"received_host_ns",anchor.received_host_ns},
                                {"epoch",anchor.epoch},{"source_id",anchor.source_id},{"generation",anchor.generation},{"geometry_version",anchor.geometry_version},
                                {"song_interval",{anchor.chosen_song_interval.lower,anchor.chosen_song_interval.upper}},
                                {"evidence_id",anchor.evidence_id},{"supporting_evidence_ids",anchor.supporting_evidence_ids},
                                {"rgb_sha256",visual.frame_sha256},{"armed_host_ns",clock.now_ns()},
                                {"runtime_construction_begin_ns",construction_begin},{"runtime_construction_end_ns",construction_end}};
                            manifest["experimental_first_target_host_ns"]=first_due;
                            journal.append({{"event","armed_whole_song"},{"first_target_host_ns",first_due},{"runtime",runtime_json(runtime->status())}});
                            std::cout<<"FIRST NOTE ACQUIRED: chart-driven whole song armed; "<<expected_notes<<" contacts.\n"<<std::flush;
                        }
                    }
                } else if(!armed&&!ui_healthy(now))trigger_health(now,false);
            }
            now=clock.now_ns();
            service_desktop_first_kernel(now);
            if(armed&&!sequence_complete) {
                if(const auto rejected=ui_failure(now))throw std::runtime_error(rejected);
                if(!sample_opening_by_receipt&&!kernel_started&&now>=first_due-350*ms) {
                    kernel_started=true;kernel_pending=std::async(std::launch::async,[&kernel]{return kernel.query();});
                }
                if(!sample_opening_by_receipt&&kernel_started&&!kernel_ready&&
                   kernel_pending.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
                    kernel_before=kernel_pending.get();kernel_request=kernel_before.at("host_request_begin_ns").get<Ns>();
                    journal.append({{"event","kernel_pre_down"},{"snapshot",kernel_before}});
                    if(kernel_before.at("active_count")!=0||kernel_before.at("causal_host_interval_valid")!=true)
                        throw std::runtime_error("pre_down_kernel_not_fresh_zero");
                    kernel_ready=true;
                }
                last_tick_begin=clock.now_ns();const auto tick=runtime->tick();last_tick_end=clock.now_ns();
                for(const auto& notice:tick.notices)journal.append({{"event","scheduler_notice"},{"intent_id",notice.intent_id},
                    {"host_ns",notice.monotonic_ns},{"reason",notice.reason}});
                if(tick.status.state==ExperimentalRuntimeState::stopped)throw std::runtime_error(tick.status.reason);
                if(tick.status.state==ExperimentalRuntimeState::completed) {
                    sequence_complete=true;manifest["sequence_completed_host_ns"]=clock.now_ns();
                    if(desktop)desktop->stage="awaiting_result";
                    journal.append({{"event","whole_song_command_queue_sequence_completed"},{"runtime",runtime_json(tick.status)}});
                    std::cout<<"WHOLE SONG COMMAND QUEUE SEQUENCE COMPLETED; waiting for stream completion and manual-mode result evidence.\n"<<std::flush;
                }
            }
            if(status.session_done) {
                if(sequence_complete&&status.result_confirmed)finished=true;
                else throw std::runtime_error(sequence_complete?"manual_round_ended_without_result_confirmation":"manual_round_ended_before_whole_song_complete");
            }
            if(!finished&&armed&&!sequence_complete) {
                const auto now_before_wait=clock.now_ns();
                const auto next=runtime->next_wake_ns();
                owner_wait.until(next?std::min(*next,now_before_wait+ms):now_before_wait+ms,runtime->next_input_due_ns());
            }
        }
    }catch(const std::exception& error){failure=error.what();if(failure.empty())failure="trial_exception_without_message";}
    recovery_enabled=false;recovery_worker.request_stop();
    const auto primary_failure_size=failure.size();
    struct CleanupErrorRef {const char* stage;std::size_t offset,length;};
    std::array<CleanupErrorRef,12> cleanup_refs{};
    std::size_t cleanup_ref_count=0;
    const auto remember_cleanup=[&](const char* stage,std::size_t length) noexcept {
        if(desktop&&cleanup_ref_count<cleanup_refs.size())cleanup_refs[cleanup_ref_count++]={stage,failure.size()-length,length};
    };
    if(desktop){
        if(primary_failure_size)
            desktop->failure_stage=armed&&!sequence_complete&&touch.down_attempts?"playing":desktop->stage;
        desktop->stage="cleanup";
        desktop->first_note_gate=adaptive_trigger->dispatch_gate();
    }
    const auto cleanup_error=[&](const char* stage,const std::exception& error) {
        if(!failure.empty())failure+="; ";failure+=std::string(stage)+": "+error.what();
        if(desktop)remember_cleanup(stage,std::string_view(error.what()).size());
    };
    // Cleanup calls are independent: a failed runtime/release must not skip the
    // separate transport recovery or the read-only final slot query.
    if(runtime)try {runtime->stop(failure.empty()?"owner_cleanup":failure);manifest["runtime_final"]=runtime_json(runtime->status());}
        catch(const std::exception& error){cleanup_error("runtime_stop",error);}
    try {
        const auto release=transport.release_all();release_known=release.failed_ids.empty()&&release.unknown_ids.empty();
        manifest["release"]=release_json(release);
    }catch(const std::exception& error){cleanup_error("release",error);}
    if(!transport.fault().empty()) {
        if(!failure.empty())failure+="; ";failure+="touch_stream: "+transport.fault();
        if(desktop)remember_cleanup("touch_stream",transport.fault().size());
    }
    bool recovery_attempted=false;
    const auto recovery_release=[&] {
        recovery_attempted=true;
        try {
            pas::GrpcTouch recovery(clock,endpoint,mapping,720,1280,10,std::chrono::milliseconds(100));
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
    recovery_worker.join();
    if(desktop)try {
        desktop->execution_state=sequence_complete?"completed":touch.down_attempts?"incomplete":"not_started";
        desktop->result_state=status.result_confirmed?"confirmed_by_native":"not_confirmed";
        desktop->release_state=final_zero?"kernel_zero_confirmed":"kernel_zero_not_confirmed";
        desktop->down_attempts=touch.down_attempts;desktop->up_attempts=touch.up_attempts;
        desktop->release_rpc_known=release_known;desktop->kernel_final_zero=final_zero;
        desktop->timing={{"clock","host-QPC"},{"phase_ms",phase},{"start_host_ns",started},
            {"anchor_received_host_ns",manifest.contains("anchor")?manifest.at("anchor").at("received_host_ns"):Json(nullptr)},
            {"first_target_host_ns",manifest.value("experimental_first_target_host_ns",Json(nullptr))},
            {"sequence_completed_host_ns",manifest.value("sequence_completed_host_ns",Json(nullptr))},
            {"result_receipt_host_ns",result_frame?Json(result_frame->capture_complete_ns):Json(nullptr)},
            {"first_successful_write_host_ns",nullptr},{"last_successful_write_host_ns",nullptr},
            {"max_successful_write_lateness_ns",nullptr}};
        const auto& messages=transport.messages();
        for(const auto& message:messages)if(message.write_ok){
            if(desktop->timing.at("first_successful_write_host_ns").is_null())
                desktop->timing["first_successful_write_host_ns"]=message.host_write_start_ns;
            desktop->timing["last_successful_write_host_ns"]=message.host_write_start_ns;
        }
        std::optional<Ns> max_write_lateness;
        for(const auto& event:transport.events())if(event.message_write_ok){
            const auto& message=messages.at(static_cast<std::size_t>(event.message_id-1));
            const auto lateness=message.host_write_start_ns-event.command.scheduled_ns;
            if(!max_write_lateness||lateness>*max_write_lateness)max_write_lateness=lateness;
        }
        if(max_write_lateness)desktop->timing["max_successful_write_lateness_ns"]=*max_write_lateness;
    }catch(const std::exception&){} // Optional diagnostics cannot change the native outcome.
    if(numeric_hud) {
        numeric_hud->finish();
        try {
            Json numeric_report={{"schema","pas.numeric-hud-timeline.v1"},{"diagnostic_only",true},
                {"pixels_retained",false},{"input_permission",false},{"summary",numeric_hud->summary()},
                {"rows",numeric_hud->rows()},{"scope","OCR of original frame receipts; unknown readings retained; no judgment or clock qualification"}};
            const auto text=numeric_report.dump(2);
            std::ofstream numeric_file(output/"numeric-hud-timeline.json",std::ios::binary);
            numeric_file.write(text.data(),static_cast<std::streamsize>(text.size()));numeric_file.close();
            if(!numeric_file)throw std::runtime_error("numeric HUD metadata write failed");
            manifest["numeric_HUD_observer"]={{"enabled",true},{"diagnostic_only",true},{"pixels_retained",false},
                {"file","numeric-hud-timeline.json"},{"sha256",chart_text_sha256(text)},{"summary",numeric_hud->summary()}};
        }catch(const std::exception& error){manifest["numeric_HUD_observer_error"]=std::string(error.what()).substr(0,256);}
    }
    if(native_head_model&&native_head_rows) {
        try {
            constexpr std::size_t file_cap=2*1024*1024;
            const auto context=native_head_shadow_model_json(*native_head_model,native_head_caller_pose_events);
            const auto& shadow=*native_head_rows;Json sources=Json::array();
            for(std::size_t i=0;i<native_head_model->sources().size();++i){const auto& source=native_head_model->sources()[i];
                sources.push_back({{"source_index",i},{"source_note_id",source.source_note_id},{"line_index",source.line_index},
                    {"note_index_in_original_line",source.note_index},{"kind",static_cast<int>(source.kind)},
                    {"side",source.side==NoteSide::above?"above":"below"},{"judgment_ns",source.judgment_ns}});}
            Json report={{"schema","pas.native-head-snapshot-shadow.v1"},{"diagnostic_only",true},{"pixels_retained",false},
                {"input_permission",false},{"source_pixel_age_qualified",false},
                {"status",shadow.disabled?"diagnostic_disabled_after_error":shadow.size?"diagnostic_complete":"no_shadow_rows_recorded"},
                {"model",context},{"fixed_origin_host_ns",shadow.origin_host_ns>=0?Json(shadow.origin_host_ns):Json(nullptr)},
                {"rows_observed",shadow.size},{"near_due_skips",shadow.near_due_skips},{"cadence_skips",shadow.cadence_skips},
                {"capacity_skips",shadow.capacity_skips},{"source_catalog",std::move(sources)},{"rows",Json::array()},
                {"scope","Original actual frame centers and all complete local grid assignments; sampled points only, no nearest winner or continuous interval. Fixed-origin prediction is not actual game time; pixel/render age and input latency remain unknown. No permission renewal, retiming or gameplay qualification."}};
            std::size_t serialized_size=report.dump().size();bool exceeded=serialized_size>file_cap;
            for(std::size_t i=0;i<shadow.size&&!exceeded;++i){auto row=native_head_shadow_row_json(shadow.rows[i],*native_head_model);
                const auto row_size=row.dump().size()+(i?1u:0u);
                if(row_size>file_cap-serialized_size){exceeded=true;break;}
                serialized_size+=row_size;report["rows"].push_back(std::move(row));}
            if(exceeded)report={{"schema","pas.native-head-snapshot-shadow.v1"},{"diagnostic_only",true},{"pixels_retained",false},
                {"input_permission",false},{"source_pixel_age_qualified",false},{"status","diagnostic_file_budget_exceeded"},
                {"reason","Complete metadata exceeds2MiB; no truncated timing candidates are presented"},
                {"chart_sha256",native_head_model->chart_sha256()},{"window_ns",{native_head_model->window().begin_ns,native_head_model->window().end_ns}},
                {"rows_observed",shadow.size},{"rows",Json::array()}};
            const auto text=report.dump();if(text.size()>file_cap)throw std::runtime_error("native head snapshot failure metadata exceeds file cap");
            std::ofstream file(output/"native-head-snapshot-shadow.json",std::ios::binary);
            file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();
            if(!file)throw std::runtime_error("native head snapshot metadata write failed");
            auto evidence=context;evidence["enabled"]=true;evidence["file"]="native-head-snapshot-shadow.json";
            evidence["sha256"]=chart_text_sha256(text);evidence["bytes"]=text.size();evidence["status"]=report.at("status");
            evidence["rows_observed"]=shadow.size;evidence["maximum_file_bytes"]=file_cap;
            manifest["native_head_snapshot_shadow"]=std::move(evidence);
        }catch(const std::exception& error){manifest["native_head_snapshot_shadow_error"]=std::string(error.what()).substr(0,256);}
    }
    Json recovery_trace=Json::array();
    for(const auto& probe:recovery_probes)recovery_trace.push_back({{"request_ns",probe.request_ns},{"return_ns",probe.return_ns},
        {"received_ns",probe.received_ns},{"source_timestamp_us",probe.source_timestamp_us},{"accepted",probe.accepted},
        {"sequence",probe.sequence},{"reason",probe.reason}});
    manifest["capture_recovery"]={{"policy","actual-unary-screenshot-on-stream-idle-v1"},{"idle_ns",200*ms},
        {"cadence_ns",200*ms},{"rpc_timeout_ms",100},{"maximum_attempts",4096},{"maximum_inflight",1},
        {"stream_drops",recovery_stream_drops},{"pool_drops",recovery_pool_drops},{"probes",std::move(recovery_trace)},
        {"source_pixel_age_qualified",false},{"scope","fresh actual arrival-only RPC pixels; source timestamp is server request time, not GPU presentation time; unchanged 500ms current-HUD watchdog"}};
    const auto capture_stats=capture.stats();
    manifest["capture_stats"]={{"valid",capture_stats.valid},{"inactive",capture_stats.inactive},
        {"invalid",capture_stats.invalid},{"source_gaps",capture_stats.source_gaps},
        {"relative_stale_drops",capture_stats.relative_stale_drops},{"last_notification_ns",capture_stats.last_notification_ns}};
    owner_wait.finish();manifest["owner_wait"]=owner_wait_json(owner_wait.diagnostics());
    stop_worker.request_stop();stop_worker.join();
    if(!foreground_watch.stop()&&failure.empty()){
        failure="foreground_stop_timeout";
        remember_cleanup("foreground_stop",failure.size());
    }
    try {manifest["HUD_timeline"]=save_hud_timeline(output,hud_timeline,source_sha);}
        catch(const std::exception& error){cleanup_error("hud_timeline",error);}
    if(desktop){
        desktop->stage="recording";
        try {
            desktop->failure_detail=failure.substr(0,std::min<std::size_t>(primary_failure_size,384));
            for(std::size_t i=0;i<cleanup_ref_count;++i){
                const auto& entry=cleanup_refs[i];
                desktop->cleanup_error(entry.stage,std::string_view(failure).substr(entry.offset,entry.length));
            }
        }catch(const std::exception&){} // Preserve release and manifest behavior if optional logging fails.
    }
    journal.flush_after_cleanup();
    Journal receipts(output/"receipts.jsonl",expected_steps);for(const auto& receipt:touch.receipts) {
        auto row=receipt_json(receipt);row["intent_id"]=receipt.command.intent_id;
        row["event"]="transport_queue";
        row["source_frame_sequence"]=receipt.command.source_frame_sequence;receipts.append(row);
    }
    receipts.flush_after_cleanup();
    if(desktop)desktop->receipt_rows=receipts.rows;
    // Queue receipts and actual message-write traces retain separate clocks.
    // Neither local queue acceptance nor a Write is an RPC completion.
    Json stream_events=Json::array(),stream_sessions=Json::array(),stream_messages=Json::array(),stream_schedule=Json::array(),stream_admissions=Json::array();
    for(const auto& step:compiled.plan->events)stream_schedule.push_back({{"song_ns",step.song_ns},{"phase",static_cast<int>(step.phase)}});
    for(const auto& a:write_admissions)stream_admissions.push_back({
        {"command",{{"intent_id",a.command.intent_id},{"contact_id",a.command.contact_id},
            {"phase",static_cast<int>(a.command.phase)},{"x",a.command.x},{"y",a.command.y},
            {"scheduled_ns",a.command.scheduled_ns},{"source_frame_sequence",a.command.source_frame_sequence}}},
        {"check_begin_ns",a.check_begin_ns},{"check_return_ns",a.check_return_ns},{"allowed",a.allowed},{"reason",a.reason},
        {"actual_ui_sequence",a.sequence},{"received_ns",a.received_ns},{"epoch",a.epoch},{"generation",a.generation},
        {"geometry_version",a.geometry_version},{"source_id",a.source_matches?source_id:"<unexpected-source>"},
        {"playing",a.playing},{"source_valid",a.source_valid},{"foreground_healthy",a.foreground_healthy}});
    for(const auto& e:transport.events())stream_events.push_back({
        {"command",{{"intent_id",e.command.intent_id},{"contact_id",e.command.contact_id},
            {"phase",static_cast<int>(e.command.phase)},{"x",e.command.x},{"y",e.command.y},
            {"scheduled_ns",e.command.scheduled_ns},{"source_frame_sequence",e.command.source_frame_sequence}}},
        {"host_start_ns",e.host_start_ns},{"host_return_ns",e.host_return_ns},
        {"stream_id",e.stream_id},{"ordinal",e.ordinal},{"mapped_x",e.mapped_x},{"mapped_y",e.mapped_y},
        {"pressure",e.pressure},{"receipt_accepted",e.receipt_accepted},{"message_write_ok",e.message_write_ok},
        {"message_id",e.message_id},{"touch_index",e.touch_index}});
    for(const auto& message:transport.messages())stream_messages.push_back({
        {"id",message.id},{"stream_id",message.stream_id},{"first_event_index",message.first_event_index},
        {"event_count",message.event_count},{"host_write_start_ns",message.host_write_start_ns},
        {"host_write_return_ns",message.host_write_return_ns},{"write_ok",message.write_ok}});
    for(const auto& s:transport.sessions())stream_sessions.push_back({
        {"id",s.id},{"preparation_probe",s.preparation_probe},
        {"open_begin_ns",s.open_begin_ns},{"open_return_ns",s.open_return_ns},
        {"close_begin_ns",s.close_begin_ns},{"close_return_ns",s.close_return_ns},
        {"first_event_index",s.first_event_index},{"event_count",s.event_count},{"message_count",s.message_count},
        {"writes_done_ok",s.writes_done_ok},{"finish_called",s.finish_called},{"finish_ok",s.finish_ok},
        {"cancelled",s.cancelled},{"status_code",s.status_code}});
    const auto& prep=transport.preparation();
    manifest["touch_stream"]={{"policy","ordered-same-due-move-touch-event-batches-v1"},
        {"pressure_policy",touch_pressure_policy},
        {"coordinate_map",{{"frame_width",1280},{"frame_height",720},{"touch_width",720},{"touch_height",1280},{"rotation_deg",90}}},
        {"write_receipt_semantics","command_queue_accepted_effect_unverified"},
        {"receipt_clock_scope","inject queue acceptance and return; earlier receipts are never retimed to a later batched Write"},
        {"batching_policy","exact-same-due-distinct-contact-MOVE-only-v1"},{"max_touches_per_message",10},
        {"dispatch_schedule_source","complete sorted compiled plan events; fixed absolute offset and phase checked per command"},
        {"RPC_completion_scope","Finish OK confirms RPC completion only; device/game effect unverified"},
        {"max_stream_age_ns",50*ms},{"max_stream_events",256},{"stream_timeout_ms",100},
        {"events",std::move(stream_events)},{"messages",std::move(stream_messages)},{"sessions",std::move(stream_sessions)},
        {"dispatch_schedule",std::move(stream_schedule)},{"write_admissions",std::move(stream_admissions)},
        {"fault",transport.fault()},
        {"preparation",{{"begin_ns",prep.begin_ns},{"return_ns",prep.return_ns},{"timeout_ms",prep.timeout_ms},
            {"connected",prep.connected},{"probe_opened",prep.probe_opened},
            {"probe_writes_done_ok",prep.probe_writes_done_ok},{"probe_finish_ok",prep.probe_finish_ok},
            {"probe_status_code",prep.probe_status_code},{"input_rpc_opened",prep.input_rpc_opened!=0},
            {"touch_events",prep.touch_events}}},
        {"unary_release",release_json(transport.unary_release())}};
    if(diagnostic_images&&anchor_frame)pas::write_diagnostic_png(output/"anchor.png",*anchor_frame);
    if(diagnostic_images&&last_frame)pas::write_diagnostic_png(output/"last-observed.png",*last_frame);
    if(result_frame) {
        manifest["result_observation"]={{"sequence",result_frame->sequence},{"received_host_ns",result_frame->capture_complete_ns},
            {"rgb_sha256",rgb_sha(*result_frame)},{"policy",result_policy},
            {"image_saved",retain_result_image(image_retention)},
            {"scope","terminal UI confirmation only; score/judgments/AP unreviewed"}};
        if(retain_result_image(image_retention))pas::write_diagnostic_png(output/"result.png",*result_frame);
    }
    Json unknown_frames=Json::array();
    const auto save_unknown=[&](const char* name,const auto& frame) {
        if(!diagnostic_images||!frame)return;
        pas::write_diagnostic_png(output/name,*frame);
        unknown_frames.push_back({{"file",name},{"sequence",frame->sequence},{"received_host_ns",frame->capture_complete_ns},{"rgb_sha256",rgb_sha(*frame)}});
    };
    save_unknown("template-unknown-first.png",first_unknown_frame);save_unknown("template-unknown-last.png",last_unknown_frame);
    manifest["template_unknown_frames"]=unknown_frames;manifest["HUD_fallback_frames"]=hud_fallback_frames;
    manifest["owner_diagnostic_timing"]={{"scope","host stages only; never receipt retiming or input-latency qualification"},
        {"last_source_sequence",last_owner_sequence},{"frame_begin_ns",last_owner_frame_begin},{"classify_end_ns",last_classify_end},
        {"HUD_end_ns",last_HUD_end},{"ui_append_begin_ns",last_ui_append_begin},{"ui_append_end_ns",last_ui_append_end},
        {"tick_begin_ns",last_tick_begin},{"tick_end_ns",last_tick_end},
        {"snapshot_begin_ns",last_snapshot_begin},{"snapshot_end_ns",last_snapshot_end},
        {"opening_snapshot_RGB_buffers_preallocated",diagnostic_images?48:0},
        {"diagnostic_pool_leases_max",2},{"frame_pool_slots",5},{"journal_budget_preallocated_before_READY",true}};
    if(hud_redundancy) {
        const auto& learned=hud_redundancy->status();
        const auto save_learned=[&](const char* name,std::span<const std::uint8_t> pixels,int stride,const LearnedHudFrame& meta) {
            if(!diagnostic_images||pixels.empty())return;
            pas::Frame actual;actual.width=1280;actual.height=720;actual.stride=stride;
            actual.rgb.assign(pixels.begin(),pixels.end());actual.sequence=meta.sequence;actual.capture_complete_ns=meta.received_ns;
            pas::write_diagnostic_png(output/name,actual);
        };
        save_learned("learned-title-reference.png",hud_redundancy->reference_rgb(),hud_redundancy->reference_stride(),learned.reference_frame);
        save_learned("learned-title-support.png",hud_redundancy->support_rgb(),hud_redundancy->support_stride(),learned.support_frame);
        manifest["learned_HUD_redundancy"]={{"policy",redundancy_policy},{"ready",learned.ready},{"reason",learned.reason},
            {"accepted_actual_frames",redundancy_frames},{"construction_attempts",learned.construction_attempts},{"support_frames",learned.support_frames},
            {"reference_sequence",learned.reference_frame.sequence},{"reference_received_ns",learned.reference_frame.received_ns},
            {"reference_rgb_sha256",learned.reference_sha256},{"support_sequence",learned.support_frame.sequence},
            {"support_received_ns",learned.support_frame.received_ns},{"support_rgb_sha256",learned.support_sha256},
            {"title_region_xywh",pixel_region_json(learned.title_region)},
            {"scope","two pre-input clean actual HUD observations; only current matching pixels renew playing; not song identity/source-age qualification"}};
    }
    saved_frames.resize(saved_count); // Unused buffers are not observed frames.
    Json frame_index=Json::array();std::size_t index=0;
    for(const auto& frame:saved_frames) {
        const auto name="judgment-"+std::to_string(++index)+".png";pas::write_diagnostic_png(output/name,frame);
        frame_index.push_back({{"file",name},{"sequence",frame.sequence},{"received_host_ns",frame.capture_complete_ns},{"rgb_sha256",rgb_sha(frame)}});
    }
    Json acquisition_index=Json::array();index=0;
    for(const auto& frame:acquisition_frames) {
        const auto name="acquisition-"+std::to_string(++index)+".png";pas::write_diagnostic_png(output/name,frame);
        acquisition_index.push_back({{"file",name},{"sequence",frame.sequence},
            {"received_host_ns",frame.capture_complete_ns},{"rgb_sha256",rgb_sha(frame)}});
    }
    manifest["acquisition_frames"]=acquisition_index;
    manifest["acquisition_diagnostic_scope"]="at most24 actual pre-input center frames at100ms cadence; saved after release; no clock/coverage qualification";
    manifest["frames"]=frame_index;manifest["down_attempts"]=touch.down_attempts;manifest["up_attempts"]=touch.up_attempts;
    manifest["kernel_before_down"]=kernel_before;manifest["release_rpc_known"]=release_known;manifest["kernel_final_zero"]=final_zero;
    if(sample_opening_by_receipt)manifest["kernel_before_down_probe_policy"]={
        {"trigger","actual-playing-HUD-before-arm"},{"interval_ns",desktop_kernel_probe_interval},{"maximum_inflight",1},
        {"queries",first_kernel_probes},{"rejected",first_kernel_rejected},{"first_write_max_age_ns",500*ms},
        {"last_probe_error",last_first_kernel_probe_error.empty()?Json(nullptr):Json(last_first_kernel_probe_error)}};
    manifest["whole_song_sequence_complete"]=sequence_complete;manifest["manual_result_confirmed"]=status.result_confirmed;
    manifest["game_judgment"]="unreviewed";manifest["AP_verified"]=false;manifest["failure"]=failure.empty()?Json(nullptr):Json(failure);
    manifest["status"]=failure.empty()&&sequence_complete&&status.result_confirmed&&touch.down_attempts==expected_notes&&touch.up_attempts==expected_notes&&release_known&&final_zero?
        "experimental_whole_song_sent_judgment_unreviewed":"failed_or_stopped";
    manifest["start_host_ns"]=started;manifest["stop_host_ns"]=clock.now_ns();manifest["journal_rows"]=journal.rows;manifest["receipt_rows"]=receipts.rows;
    manifest["preinput_UI_journal_skips"]=preinput_ui_journal_skips;
    manifest["opening_detector_metadata"]={{"rows",opening_detector_journal_rows},{"maximum_rows",1024},
        {"minimum_receipt_spacing_ns",250*ms},{"pixels_retained",false},{"input_permission",false}};
    write_json(output/"manifest.json",manifest);
    const int exit_code=manifest.at("status")=="experimental_whole_song_sent_judgment_unreviewed"?0:2;
    if(desktop){
        desktop->manifest_written=true;
        try {
            desktop->timing["stop_host_ns"]=manifest.at("stop_host_ns");
            if(exit_code!=0&&!desktop->failure_stage&&desktop->cleanup_errors.empty()){
                desktop->failure_stage="cleanup";
                desktop->failure_detail=!release_known||!final_zero?"release_evidence_not_confirmed":"native_completion_not_confirmed";
            }
        }catch(const std::exception&){}
        publish_desktop_summary(*desktop,exit_code);
    }else std::cout<<manifest.dump(2)<<'\n';
    return exit_code;
}
} // namespace
int run_utf8(int argc,char** argv) {
    std::optional<DesktopRoundSummary> desktop;
    try {if(argc==2&&std::string(argv[1])=="--help"){std::cout<<usage;return 0;}
        if(argc>1&&std::string(argv[1])=="prepare-adaptive")return prepare_adaptive(argc,argv);
        desktop=desktop_round_summary_suffix(argc,argv);
        return run(argc,argv,desktop?&*desktop:nullptr);}
    catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';
        if(desktop)publish_desktop_summary(*desktop,1,error.what());
        return 1;
    }
}

// Windows command-line paths arrive as UTF-16. Keep native Unicode chart IDs
// intact through UTF-8 JSON/labels and explicit UTF-8 filesystem conversion.
int wmain(int argc,wchar_t** wide_argv) {
    try {
        std::vector<std::string> arguments;arguments.reserve(argc);
        for(int i=0;i<argc;++i) {
            const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,nullptr,0,nullptr,nullptr);
            if(size<=0)throw std::invalid_argument("invalid Unicode command-line argument");
            std::string value(static_cast<std::size_t>(size),'\0');
            if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,value.data(),size,nullptr,nullptr))
                throw std::invalid_argument("Unicode command-line conversion failed");
            value.pop_back();arguments.push_back(std::move(value));
        }
        std::vector<char*> pointers;pointers.reserve(arguments.size());
        for(auto& value:arguments)pointers.push_back(value.data());
        return run_utf8(argc,pointers.data());
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
