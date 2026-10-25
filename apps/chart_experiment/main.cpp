#include "experimental_runtime.hpp"
#include "flick_policy.hpp"
#include "catalog_source.hpp"
#include "../chart_calibrate/trial_io.hpp"
#include "../chart_calibrate/opening_detector.hpp"
#include "first_note_trigger.hpp"
#include "adaptive_first_note.hpp"
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
constexpr std::size_t dlyrotz_expected_steps=5669;
constexpr const char* dlyrotz_plan_sha="74e16603971e407627ea679a56f20300043a43c9fdf89008d9d6645d703e3ce5";
constexpr const char* dlyrotz_chart_sha="aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90";
constexpr const char* dlyrotz_audio_sha="aa9423b0f86ca4c1250f460bfbbc23d78a6405c2de739a2ba5ffef0b48261a44";
constexpr const char* source_id="live-grpc-phigros-whole-song";
constexpr const char* dlyrotz_hud_reference_sha="397431f2749bb38a845c6845977fc5c37cb2de51a79ff273dd3417ef2756e09a";
constexpr const char* usage=
    "chart_experiment run SERIAL AVD CHART.json STATE.json KERNEL.json NEW_DIR PHASE_MS --experimental-whole-song [--experimental-hud-reference CLEAN.rgb]\n"
    "chart_experiment prepare-adaptive CHART.json SOURCE-MANIFEST.json NEW_DIR [--normal-projection-pixels LIMIT] [--drag-duration-ms MS] [--input-inset-pixels PIXELS] [--contact-capacity N] [--hud-policy calibrated-title-v1|shared-in-prefix-score-v1|shared-in-cell-score-v2|shared-in-subpixel-prefix-score-v3] [--import-policy POLICY_ID] [--tight-projection-speed-bounds 0|1] [--flick-lead-ms 1..100] [--flick-travel-ms 2..200] [--flick-distance-pixels 1..240] [--flick-release-ms 1..100] [--flick-path-policy dynamic-line-v1|judgment-anchored-straight-v1]\n"
    "chart_experiment run-adaptive SERIAL AVD CHART.json STATE.json KERNEL.json NEW_DIR PHASE_MS --experimental-whole-song --song-profile PROFILE.json --experimental-hud-reference CLEAN.rgb [--capacity-evidence EVIDENCE.json]\n"
    "Use check or check-adaptive in place of run/run-adaptive for offline preflight only (no device discovery/input/output directory).\n"
    "Optional final suffix: --image-retention none|result|diagnostic (default none; result saves only a confirmed result PNG).\n"
    "Dlyrotz IN13 complete 584-note / 5669-step candidate, capacity 4, D250 projection.\n"
    "Default experimental Tap/Drag dwell32ms; Flick lead30ms/travel60ms/distance60px/release24ms. Adaptive Flick lead must be less than travel; HUD timeline at50ms.\n"
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
    return {{"state",static_cast<int>(s.state)},{"reason",s.reason},{"selected_contacts",s.selected_contacts},
        {"selected_steps",s.selected_steps},{"completed_contacts",s.completed_contacts},{"pending",s.pending},
        {"active",s.active},{"peak_contacts",s.peak_contacts},{"cache_sha256",s.full_cache_sha256},
        {"plan_end_song_ns",s.plan_end_song_ns},{"projected_contacts",s.projected_contacts},
        {"UI_watchdog_ns",s.ui_receipt_max_age_ns},{"anchor_max_age_ns",s.anchor_max_age_ns},
        {"max_step_lateness_ns",s.max_step_lateness_ns},{"overdue_tail_revisions_deferred",s.overdue_tail_revisions_deferred},
        {"contact_capacity",s.contact_capacity},
        {"max_normal_projection_reference_pixels",s.max_normal_projection_reference_pixels},
        {"anchor_domain_ns",{s.anchor_domain_begin_ns,s.anchor_domain_end_ns}},
        {"selected_projection_free",s.selected_projection_free},{"requires_calibration",s.full_plan_requires_calibration},
        {"release_required",s.release_required},{"release_rpc_ok",s.release_rpc_ok},{"release",release_json(s.release)},
        {"timing_qualified",s.timing_qualified},{"source_pixel_age_qualified",s.source_pixel_age_qualified}};
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
class GuardedTouch final:public pas::TouchBackend {
    pas::TouchBackend& backend_;const pas::Clock& clock_;
    std::size_t receipt_budget_;
public:
    std::function<std::string(const pas::TouchCommand&)> guard;
    std::vector<pas::TouchReceipt> receipts;
    std::size_t down_attempts=0,up_attempts=0;
    GuardedTouch(pas::TouchBackend& backend,const pas::Clock& clock,std::size_t receipt_budget)
      :backend_(backend),clock_(clock),receipt_budget_(receipt_budget) {
        if(receipt_budget<1||receipt_budget>16000)throw std::invalid_argument("transport receipt budget 1..16000");
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
    std::string installed_source_manifest;
    std::size_t notes=584,steps=dlyrotz_expected_steps;
    int peak=4,capacity=4,projection_pixels=250,input_inset_pixels=0;
    Ns first_down=12*second,drag_duration_ns=32*ms;
};
ContactCompileOptions song_compile_options(const SongProfile& song) {
    ContactCompileOptions options;options.max_contacts=song.capacity;
    options.tap_duration_ns=32*ms;options.drag_duration_ns=song.drag_duration_ns;
    apply_flick_parameters(options,song.flick);
    options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
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
    else throw std::invalid_argument("unsupported explicit Phigros import policy");
    return options;
}
Json import_json(const SemanticChart& chart) {
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
        {"source_chart_sha256",chart.source.chart_sha256},{"anomalies",std::move(anomalies)}};
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
    song.notes=static_cast<std::size_t>(kernel_integer(value.at("expected_contacts"),1,16000/2));
    song.steps=static_cast<std::size_t>(kernel_integer(value.at("expected_steps"),1,16000));
    song.capacity=static_cast<int>(kernel_integer(value.value("contact_capacity",Json(4)),1,5));
    song.peak=static_cast<int>(kernel_integer(value.at("expected_peak"),1,song.capacity));
    song.first_down=kernel_integer(value.at("first_down_song_ns"),0,180*second-1);
    song.projection_pixels=static_cast<int>(kernel_integer(value.at("max_normal_projection_reference_pixels"),1,10000));
    song.drag_duration_ns=kernel_integer(value.at("drag_duration_ns"),ms,32*ms);
    song.input_inset_pixels=static_cast<int>(kernel_integer(value.value("input_inset_pixels",Json(0)),0,100));
    return song;
}
int prepare_adaptive(int argc,char** argv) {
    if(argc<5||argc>29||(argc-5)%2)throw std::invalid_argument(usage);
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
        const auto value=Json::parse(argv[index+1]);
        if(set_flick_option(song.flick,option,value))continue;
        if(option=="--normal-projection-pixels")song.projection_pixels=static_cast<int>(kernel_integer(value,1,10000));
        else if(option=="--drag-duration-ms")song.drag_duration_ns=kernel_integer(value,1,32)*ms;
        else if(option=="--input-inset-pixels")song.input_inset_pixels=static_cast<int>(kernel_integer(value,0,100));
        else if(option=="--contact-capacity")song.capacity=static_cast<int>(kernel_integer(value,1,5));
        else if(option=="--tight-projection-speed-bounds")song.tight_projection_speed_bounds=kernel_integer(value,0,1)!=0;
        else throw std::invalid_argument(usage);
    }
    validate_flick_parameters(song.flick);
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
    AdaptiveFirstNoteTrigger opening(chart,-80*ms,first_down);const auto& description=opening.description();
    Json report={{"schema","pas.adaptive-song-prepare.v1"},{"offline_only",true},{"device_discovery",false},
        {"chart_sha256",song.chart_sha},{"audio_sha256",song.audio_sha},{"source_manifest_sha256",chart_text_sha256(source_bytes)},
        {"import",import_json(chart)},{"source_notes",chart.note_count},{"compiled",compiled.plan.has_value()},{"diagnostics",Json::array()},
        {"flick_parameters",flick_parameters_json(song.flick)},
        {"opening",{{"note_time_ns",description.note_time_ns},{"first_down_ns",description.first_down_ns},{"note_kind",static_cast<int>(description.note_kind)},
            {"note_id",description.note_id},{"note_json_pointer",description.note_json_pointer},
            {"model_id",description.model_id},{"model_sha256",description.model_sha256},
            {"detector_policy_id",description.detector_policy_id},{"earliest_note_ids",description.earliest_note_ids},
            {"retained_equations",description.retained_equations},{"provably_invisible_equations",description.provably_invisible_equations}}}};
    for(const auto& diagnostic:compiled.diagnostics)report["diagnostics"].push_back({
        {"severity",static_cast<int>(diagnostic.severity)},{"code",diagnostic.code},{"message",diagnostic.message},
        {"source_note_id",diagnostic.source_note_id},{"song_ns",diagnostic.song_ns}});
    if(!std::filesystem::create_directory(output))throw std::runtime_error("output creation failed");
    if(compiled.plan) {
        const auto& plan=*compiled.plan;
        const bool bounded=plan.memory.contacts.size()==chart.note_count&&plan.total_steps<=16000&&
            plan.peak_contacts<=song.capacity&&!plan.events.empty()&&plan.events.front().phase==Phase::down&&
            plan.events.front().song_ns==description.first_down_ns&&plan.events.back().song_ns<180*second;
        report["within_live_experiment_bounds"]=bounded;
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
                {"import_policy_id",song.import_policy},{"tight_projection_speed_bounds",song.tight_projection_speed_bounds},
                {"opening_model_sha256",description.model_sha256},{"source_manifest_sha256",chart_text_sha256(source_bytes)},
                {"gameplay_qualified",false},{"scope","per-song offline candidate; actual opening/HUD and full gameplay remain to be observed"}};
            profile.update(flick_parameters_json(song.flick));
            if(installed_catalog) {
                const auto path=std::filesystem::absolute(utf8_path(argv[3])).u8string();
                profile["installed_source_manifest"]=std::string(path.begin(),path.end());
                profile["audio_identity_format"]="native-fsb5";
            }
            write_json(output/"song-profile.json",profile);
        }
    }
    write_json(output/"report.json",report);std::cout<<report.dump(2)<<'\n';
    return compiled.plan&&report.value("within_live_experiment_bounds",false)?0:2;
}
Json checked_capacity_evidence(const std::filesystem::path& path,const Json& kernel,int capacity) {
    const auto bytes=read_utf8_file(path,65536);auto proof=Json::parse(bytes);
    if(proof.at("schema")!="pas.fixture-capacity-evidence.v1"||proof.at("boot_id")!=kernel.at("boot_id")||
       kernel_integer(proof.at("capacity"),capacity,5)<capacity||proof.at("normal_path_valid")!=true||proof.at("final_zero")!=true||
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
       kernel_integer(native.at("peak_contacts"),capacity,5)<capacity||native.at("stream_complete")!=true||
       native.at("expected_path_verified")!=true||native.at("pointer_lifecycle_valid")!=true||native.at("final_zero_observed")!=true||
       host.at("serial")!="emulator-5554"||host.at("avd")!="phigros"||host.at("path_verified")!=true||host.at("zero_active_observed")!=true||
       kernel_integer(host.at("capacity_requested"),capacity,5)<capacity||
       geometry.at("touch_width")!=720||geometry.at("touch_height")!=1280||geometry.at("touch_rotation")!=90||
       retrieval.at("status")!="retrieved_closed_snapshot"||retrieval.at("association_stable")!=true||
       retrieval.at("identity_before").at("boot_id")!=kernel.at("boot_id")||retrieval.at("identity_after").at("boot_id")!=kernel.at("boot_id"))
        throw std::invalid_argument("capacity evidence native path/geometry/boot binding failed");
    proof["evidence_sha256"]=chart_text_sha256(bytes);return proof;
}
int run(int argc,char** argv) {
    const auto image_retention=image_retention_suffix(argc,argv);
    const bool diagnostic_images=retain_diagnostic_images(image_retention);
    const std::string command=argc>1?argv[1]:"";
    const bool adaptive=command=="run-adaptive"||command=="check-adaptive";
    const bool offline=command=="check"||command=="check-adaptive";
    if((!adaptive&&(argc!=10&&argc!=12))||(adaptive&&argc!=14&&argc!=16)||
       (!adaptive&&command!="run"&&command!="check")||std::string(argv[9])!="--experimental-whole-song"||
       (!adaptive&&argc==12&&std::string(argv[10])!="--experimental-hud-reference")||
       (adaptive&&(std::string(argv[10])!="--song-profile"||std::string(argv[12])!="--experimental-hud-reference"))||
       (argc==16&&std::string(argv[14])!="--capacity-evidence"))
        throw std::invalid_argument(usage);
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
    for(const auto& event:compiled.plan->events) {
        const auto point=pixel_point(event.point,1280,720);const auto inset=song.input_inset_pixels;
        if(point.first<inset||point.first>=1280-inset||point.second<inset||point.second>=720-inset)
            throw std::invalid_argument("compiled input enters profile's excluded gesture edge");
    }
    std::unique_ptr<AdaptiveFirstNoteTrigger> adaptive_trigger;
    if(adaptive) {
        adaptive_trigger=std::make_unique<AdaptiveFirstNoteTrigger>(chart,static_cast<Ns>(phase)*ms,first_song_ns);
        if(adaptive_trigger->description().model_sha256!=song.opening_sha||
           adaptive_trigger->description().first_down_ns!=first_song_ns)
            throw std::invalid_argument("fresh adaptive opening differs from prepared song profile");
    }
    ExperimentalRuntimeOptions runtime_options;runtime_options.ui_receipt_max_age_ns=ui_watchdog;
    runtime_options.contact_capacity=song.capacity;
    if(adaptive) {
        runtime_options.max_normal_projection_reference_pixels=song.projection_pixels;
        runtime_options.anchor_domain_begin_ns=adaptive_trigger->description().begin_ns;
        runtime_options.anchor_domain_end_ns=adaptive_trigger->description().end_ns;
    }
    auto prepared_runtime=ExperimentalContactRuntime::prepare(*compiled.plan,runtime_options);
    const auto screen_calibration=load_screen_state_calibration(utf8_path(argv[5]));
    const auto state_dataset=Json::parse(read_utf8_file(argv[5],1024*1024));
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
    const auto kernel_bytes=read_utf8_file(argv[6],8192);const Json kernel_profile=Json::parse(kernel_bytes);
    const Json capacity_evidence=argc==16?checked_capacity_evidence(utf8_path(argv[15]),kernel_profile,song.capacity):Json(nullptr);
    if(!offline&&song.capacity>4&&capacity_evidence.is_null())
        throw std::invalid_argument("five-contact live run requires independently checked current-boot capacity evidence");
    const std::filesystem::path output=std::filesystem::absolute(utf8_path(argv[7]));
    if(std::filesystem::exists(output))throw std::runtime_error("output must not exist");
    if(offline) {
        std::cout<<Json({{"status","offline_preflight_passed"},{"device_discovery",false},{"input_enabled",false},
            {"executable_sha256",binary_sha()},{"song_id",song.song},{"difficulty",song.difficulty},
            {"chart_sha256",chart_sha},{"import",import_json(chart)},{"compiled_sha256",compiled.plan->metadata.cache_sha256},
            {"contacts",expected_notes},{"events",expected_steps},{"peak",expected_peak},
            {"flick_parameters",flick_parameters_json(song.flick)},
            {"image_retention",image_retention_name(image_retention)},
            {"screen_calibration_sha256",screen_calibration.dataset_sha256},{"HUD_reference_sha256",hud_reference_sha},{"HUD_policy",song.hud_policy},{"result_label_policy",result_policy},{"playing_redundancy_policy",redundancy_policy},
            {"kernel_profile_sha256",chart_text_sha256(kernel_bytes)},
            {"scope","offline artifact/config validation; current boot, foreground, contacts and game judgment NOT RUN"}}).dump(2)<<'\n';
        return 0;
    }
    const auto adb=pas::find_adb();const auto avd_text=adb_text(adb,serial,{"emu","avd","name"});
    if(avd_text.substr(0,avd_text.find_first_of("\r\n"))!=avd)throw std::runtime_error("AVD mismatch");
    const auto package_report=adb_text(adb,serial,{"shell","dumpsys","package","com.PigeonGames.Phigros"});
    if(field(package_report,"versionName=")!="4.0.1"||field(package_report,"versionCode=")!="157")throw std::runtime_error("game version mismatch");
    HudTimeline hud_timeline(HudTimeline::maximum_samples,diagnostic_images); // Metadata only by default; preallocate before READY.
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
        throw std::runtime_error("start from manually verified selected-song screen/result; already-playing or paused is rejected");
    if(!std::filesystem::create_directory(output))throw std::runtime_error("output creation failed");
    Journal journal(output/"events.jsonl");
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
            {"domain_ns",{description.begin_ns,description.end_ns}},
            {"earliest_note_ids",description.earliest_note_ids},{"retained_equations",description.retained_equations},
            {"provably_invisible_equations",description.provably_invisible_equations},
            {"coverage_assumption","complete unoccluded typed head centers is an explicit solver hypothesis; pixel detector does not establish coverage"},
            {"first_note_id",description.note_id},{"first_note_json_pointer",description.note_json_pointer},
            {"first_note_kind",static_cast<int>(description.note_kind)},{"first_note_ns",description.note_time_ns},
            {"first_down_ns",description.first_down_ns},{"earliest_note_count",description.earliest_note_count},
            {"source_clock","arrival_only"},{"scope","native chart opening; conditional rendering hypothesis"}};
        model.model_sha256=description.model_sha256;
    } else {
        VisualCandidateLimits limits;limits.max_observations=256;
        candidates=std::make_unique<VisualTimingCandidateProducer>(chart,model,limits);candidates->begin_epoch(acquisition_epoch);
        trigger=std::make_unique<FirstNoteTrigger>(chart,model,static_cast<Ns>(phase)*ms);
    }
    const auto trigger_health=[&](Ns now,bool healthy) {
        if(adaptive)adaptive_trigger->status(now,healthy);else trigger->status(now,healthy);
    };
    ManualConfig manual_config;manual_config.round_limit=1;manual_config.round_watchdog_ns=180*second;
    manual_config.max_observation_age_ns=ui_watchdog;manual_config.max_continuity_gap_ns=ui_watchdog;
    ManualLifecycle lifecycle(manual_config);
    const auto source_sha=chart_text_sha256("emulator-5554/phigros/1280x720/r1/"+kernel_profile.at("boot_id").get<std::string>());
    Json manifest={{"schema","pas.experimental-whole-song.v1"},{"serial",serial},{"avd",avd},{"executable_sha256",binary_sha()},
        {"chart_sha256",chart_sha},{"import",import_json(chart)},{"audio_sha256",audio_sha},{"audio_hash_scope","identity pinned from prior verified asset; no audio read/capture in this CLI"},
        {"screen_calibration_sha256",screen_calibration.dataset_sha256},{"result_label_policy",result_policy},{"playing_redundancy_policy",redundancy_policy},{"kernel_profile_sha256",chart_text_sha256(kernel_bytes)},
        {"kernel_initial",initial_kernel},{"model_settings",model_settings},{"model_sha256",model.model_sha256},
        {"source_id",source_id},{"source_sha256",source_sha},{"experimental_phase_ms",phase},
        {"song_identity_manual_selection",true},{"initial_screen_class",static_cast<int>(initial_state)},
        {"song_selection_scope","operator selects the profile's exact song/difficulty before launch; initial unknown is not automatic song identification"},
        {"song_id",song.song},{"difficulty",song.difficulty},{"adaptive_opening",adaptive},{"HUD_policy",song.hud_policy},
        {"input_inset_pixels",song.input_inset_pixels},
        {"capacity_evidence",capacity_evidence},
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
        {"flick_release_scope","hold final stroke position for experimental_flick_release_ns before UP"},
        {"flick_stroke_scope",compile_options.flick_path_policy==FlickPathPolicy::judgment_anchored_straight_v1 ?
            "fixed projected judgment anchor and side normal;constant straight stroke;full distance without clipping;unchanged lead/travel/release;not a qualified game hit region" :
            "lead/travel/distance recorded above;dynamic line pose retained;normal-offset zero at judgment;not a qualified game hit region"},
        {"dwell_scope","experimental per-song candidate; Tap/Drag durations are recorded above and bound by the compiled fingerprint; compiler defaults unchanged"},
        {"image_retention",image_retention_name(image_retention)},
        {"HUD_timeline_policy",diagnostic_images?"50ms actual receipt cadence; preallocated grayscale score/combo pixels; save after release; diagnostic only":"50ms actual receipt metadata only; no grayscale/pixel file; save after release"},
        {"max_normal_projection_reference_pixels",song.projection_pixels},{"requires_calibration",true},{"session_limit_seconds",240},
        {"round_watchdog_seconds",180},{"UI_watchdog_ns",ui_watchdog},{"anchor_receipt_max_age_ns",100*ms},
        {"UI_watchdog_scope","actual recognized playing receipt; unknown does not refresh; pause/result/source loss stop immediately; separate from fixed song timing"},
        {"experimental_HUD_fallback",static_cast<bool>(hud_features)},{"HUD_reference_sha256",hud_features?Json(hud_reference_sha):Json(nullptr)},
        {"HUD_fallback_scope",shared_hud?"explicit shared IN pause/prefix/score glyph candidate; evaluated before and after first-note arm; same-frame denied-state priority; receipt-only":"only after first-note arm, same-frame template unknown; per-song pause/difficulty/title glyphs with negative space; denied-state priority; receipt-only"},
        {"source_sequence_semantics","stream event counter, not rendered-frame ID"},
        {"owner_wait_policy","armed: nonblocking frame read; high-resolution Windows timer until min(next runtime wake, now+1ms); real lateness guard unchanged"},
        {"deadline_scope","cooperative deadlines; retained ADB termination/reader joins and OS calls have no hard wall-time guarantee"},
        {"journal_policy","64MiB/16000 UI rows; transport receipts preallocated to whole plan step count; flush after release"},
        {"transport",pas::grpc_transport_manifest(options.grpc_read_chunk_kib)}};
    write_json(output/"compiled-plan.json",full_plan_json);write_json(output/"plan.json",manifest);
    if(diagnostic_images)pas::write_diagnostic_png(output/"initial.png",initial_frame);
    SignalGuard signal_guard;pas::PixelCoordinateMap mapping(1280,720,720,1280,90);
    pas::GrpcTouch transport(clock,endpoint,mapping,720,1280,song.capacity,std::chrono::milliseconds(100));
    GuardedTouch touch(transport,clock,expected_steps);
    // Two diagnostic leases replace two repeated RGB copies. Reserve two
    // corresponding pool slots so producer headroom stays bounded and available.
    pas::LatestFrame latest(1280,720,5,&clock);std::atomic<bool> capture_done{false},stop_file{false};
    std::mutex error_mutex;std::exception_ptr capture_error;
    std::unique_ptr<ExperimentalContactRuntime> runtime;
    const auto started=clock.now_ns();auto status=lifecycle.status(started);
    Ns next_candidate=started,last_saved=-1,kernel_request=-1,first_due=-1,next_acquisition_report=started,last_acquisition_saved=-1;
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
    Ns last_snapshot_begin=-1,last_snapshot_end=-1;
    Ns last_owner_frame_begin=-1,last_classify_end=-1,last_HUD_end=-1;
    std::uint64_t last_owner_sequence=0;std::size_t redundancy_frames=0;
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
    std::cout<<"WHOLE SONG EXPERIMENT READY: manually start/retry "<<song.song<<' '<<song.difficulty<<" once; "<<expected_notes<<" notes / "<<expected_steps<<" steps / capacity "<<song.capacity<<".\n"<<std::flush;
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
                        {"title",hud_glyph_json(redundant.auxiliary.title_glyph)},
                        {"difficulty",hud_glyph_json(redundant.auxiliary.fixed_glyphs[1])}};
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
                const auto previous_append_begin=last_ui_append_begin,previous_append_end=last_ui_append_end;
                last_ui_append_begin=clock.now_ns();
                journal.append({{"event","ui"},{"sequence",frame->sequence},{"received_ns",frame->capture_complete_ns},{"processed_ns",now},
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
                if(armed) {
                    if(!sequence_complete&&observation.hud) {
                        const auto current=runtime->observe_ui(runtime_ui(clock.now_ns()));
                        if(current.state==ExperimentalRuntimeState::stopped)throw std::runtime_error(current.reason);
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
                } else if(status.round==1&&ui_healthy(now)&&now>=next_candidate) {
                    next_candidate=now+(adaptive?25:75)*ms;
                    const auto detected=adaptive?
                        detect_adaptive_opening_centers(frame->width,frame->height,frame->stride,frame->rgb,adaptive_trigger->description().note_kind):
                        calibration::detect_opening_centers(frame->width,frame->height,frame->stride,frame->rgb);
                    if(detected.valid&&!detected.centers.empty()) {
                        if(diagnostic_images&&acquisition_frames.size()<24&&(last_acquisition_saved<0||
                           frame->capture_complete_ns-last_acquisition_saved>=100*ms)) {
                            acquisition_frames.push_back(*frame);last_acquisition_saved=frame->capture_complete_ns;
                        }
                        VisualCandidateFrame visual;visual.sequence=++candidate_sequence;visual.epoch=acquisition_epoch;visual.generation=visual.geometry_version=1;
                        visual.width=1280;visual.height=720;visual.source_id=source_id;visual.source_sha256=source_sha;
                        visual.frame_id=visual.evidence_id=observation.evidence_id;visual.frame_sha256=rgb_sha(*frame);visual.clock_id="host-QPC-receipt-only";
                        visual.opening_context_id="manual-round-1-user-selected-"+song.song;visual.clock_kind=VisualSourceClock::arrival_only;
                        visual.received_host_ns=frame->capture_complete_ns;visual.source_valid=frame->source_valid;
                        // Admission assumption of this explicit rendering experiment;
                        // the detector cannot establish complete/unoccluded coverage.
                        visual.centers_complete_unoccluded=true;visual.centers=detected.centers;
                        const auto candidate_now=clock.now_ns();
                        if(candidate_now-visual.received_host_ns>=100*ms) {
                            trigger_health(candidate_now,false);journal.append({{"event","candidate_dropped_before_solver"},{"frame",visual.evidence_id},
                                {"received_ns",visual.received_host_ns},{"processed_ns",candidate_now},{"reason","receipt_expired_during_processing"}});continue;
                        }
                        VisualCandidateResult result;FirstNoteTriggerStatus trigger_status;
                        if(adaptive) {
                            trigger_status=adaptive_trigger->observe(visual,ui_healthy(candidate_now),candidate_now);
                            result.observation=visual;result.candidates=trigger_status.raw_candidates;
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
                        const auto resolved=adaptive?adaptive_trigger->take_anchor(clock.now_ns()):trigger->take_anchor(clock.now_ns());
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
                            armed=true;first_due=anchor.origin_host_ns+anchor.explicit_phase_ns+first_song_ns;
                            if(diagnostic_images)anchor_frame=*frame;
                            manifest["anchor"]={{"origin_host_ns",anchor.origin_host_ns},{"received_host_ns",anchor.received_host_ns},
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
                last_tick_begin=clock.now_ns();const auto tick=runtime->tick();last_tick_end=clock.now_ns();
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
                else throw std::runtime_error(sequence_complete?"manual_round_ended_without_result_confirmation":"manual_round_ended_before_whole_song_complete");
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
            pas::GrpcTouch recovery(clock,endpoint,mapping,720,1280,song.capacity,std::chrono::milliseconds(100));
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
    manifest["whole_song_sequence_complete"]=sequence_complete;manifest["manual_result_confirmed"]=status.result_confirmed;
    manifest["game_judgment"]="unreviewed";manifest["AP_verified"]=false;manifest["failure"]=failure.empty()?Json(nullptr):Json(failure);
    manifest["status"]=failure.empty()&&sequence_complete&&status.result_confirmed&&touch.down_attempts==expected_notes&&touch.up_attempts==expected_notes&&release_known&&final_zero?
        "experimental_whole_song_sent_judgment_unreviewed":"failed_or_stopped";
    manifest["start_host_ns"]=started;manifest["stop_host_ns"]=clock.now_ns();manifest["journal_rows"]=journal.rows;manifest["receipt_rows"]=receipts.rows;
    write_json(output/"manifest.json",manifest);std::cout<<manifest.dump(2)<<'\n';
    return manifest.at("status")=="experimental_whole_song_sent_judgment_unreviewed"?0:2;
}
} // namespace
int run_utf8(int argc,char** argv) {
    try {if(argc==2&&std::string(argv[1])=="--help"){std::cout<<usage;return 0;}
        if(argc>1&&std::string(argv[1])=="prepare-adaptive")return prepare_adaptive(argc,argv);
        return run(argc,argv);}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
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
