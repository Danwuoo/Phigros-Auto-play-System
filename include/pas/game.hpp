#pragma once
#include "pas/core.hpp"
#include <array>
#include <deque>
#include <filesystem>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace pas {

enum class GameUi { unknown, menu, loading, playing, paused, result };
enum class NoteKind { tap, hold, drag, flick, ambiguous };
const char* name(GameUi value);
const char* name(NoteKind value);
struct Vec2 { double x = 0, y = 0; };
struct SceneContext {
    std::uint64_t epoch = 0, generation = 0, geometry = 0, frame = 0;
    Nanoseconds capture_ns = 0;
    int width = 0, height = 0, rotation = 0;
    bool operator==(const SceneContext&) const = default;
};
struct LineCandidate {
    Vec2 center, tangent;
    double length = 0, thickness = 0, confidence = 0;
    std::uint64_t track_id = 0;
    Nanoseconds observed_ns = 0;
    Vec2 velocity{};
    double angular_velocity = 0;
    bool association_valid = true;
    bool motion_valid = false; // Bounded pose fit for association only; never current pixel evidence.
    int motion_samples = 0;
    Nanoseconds motion_span_ns = 0;
    double motion_residual = 0, angular_residual = 0;
};
struct NoteCandidate {
    Vec2 center;
    NoteKind kind = NoteKind::ambiguous;
    double width = 0, height = 0, confidence = 0;
    std::optional<Vec2> tail;
    Vec2 tangent{1,0};
    bool outline_evidence = false;
    std::uint64_t recent_identity = 0; // Current rails validated against a bounded recent anchor.
    bool rails_geometry = false;
    bool head_on_line = false;
    bool direct_rails_evidence = false;
    bool held_body_evidence = false; // Current paired body/front from a recently on-line Hold.
    bool held_body_patch = false; // Current interior touch region; the front is occluded/unknown.
};
struct GameTarget {
    std::uint64_t note_id = 0, revision = 0;
    NoteCandidate note;
    Nanoseconds evidence_ns = 0, expires_ns = 0;
    std::optional<Nanoseconds> crossing_ns;
    Nanoseconds uncertainty_ns = 0;
    Vec2 hit;
    double distance = 0, velocity = 0, residual = 0;
    double prediction_error_px = 0, fit_residual_limit_px = 0;
    std::string reason;
    int samples = 0;
    std::optional<Nanoseconds> tail_crossing_ns;
    Nanoseconds history_span_ns = 0;
    std::uint64_t line_id = 0;
    Vec2 hit_velocity{};
    // A fresh Note paired with a bounded projection of its last confirmed
    // line. This is prediction, never a currently observed line pixel.
    bool line_projection_only = false;
    Nanoseconds last_line_observed_ns = 0;
    double projected_line_along_px = 0, projected_line_half_length_px = 0;
};
struct DecisionSnapshot {
    std::uint64_t sequence = 0;
    SceneContext context;
    GameUi ui = GameUi::unknown;
    bool playing_gate = false, capacity_valid = true;
    Nanoseconds recognition_start_ns = 0, recognition_end_ns = 0;
    // Host QPC compute durations, distinct from the injected fake-clock
    // business timestamps used by deterministic action tests.
    Nanoseconds components_compute_ns = 0,base_scene_compute_ns = 0;
    Nanoseconds combo_glyph_compute_ns = 0,line_scan_compute_ns = 0;
    Nanoseconds component_line_decode_compute_ns = 0,note_decode_compute_ns = 0;
    Nanoseconds line_tracking_compute_ns = 0;
    Nanoseconds held_recovery_compute_ns = 0,tracking_compute_ns = 0;
    bool compute_timing_host_qpc = false;
    std::string ui_basis;
    std::optional<Vec2> play_button;
    int combo_digit_glyphs = 0; // Diagnostic shape count only; no OCR/judgment.
    std::vector<LineCandidate> lines;
    std::vector<GameTarget> targets;
};

// Shared bounded baseline state. The detector may read current rail anchors;
// comparison batches explicitly identify that source of candidate bias.
struct GameTrackPoint { Nanoseconds t; Vec2 p; LineCandidate line; std::optional<Vec2> tail; bool rails=false; bool front_is_touch=false; };
struct GameTrackHistory {
    std::uint64_t id=0,revision=0;
    NoteKind kind=NoteKind::ambiguous;
    Vec2 last;
    Nanoseconds observed=0;
    std::deque<GameTrackPoint> points;
    NoteCandidate appearance;
    std::optional<NoteCandidate> rail_anchor;
    Nanoseconds rail_observed=0,point_bucket_ns=0;
    std::uint64_t rail_frame=0;
    std::uint64_t confirmed_line_id=0;
    std::uint64_t replacement_line_id=0;
    Nanoseconds replacement_first_ns=0;
    int replacement_observations=0;
};
enum class ObservationQuality { strong_current, weak_current, rejected };
struct TrackingCandidate {
    std::uint64_t candidate_id=0;
    NoteCandidate note;
    std::optional<NoteCandidate> shortened_hold;
    ObservationQuality quality=ObservationQuality::rejected;
    bool head_visible=true,body_visible=false,left_rail=false,right_rail=false;
    bool action_support=false;
    std::uint64_t hint_source_frame=0;
    Nanoseconds hint_age_ns=0;
    std::string origin="color_core";
};
struct CandidateBatch {
    int schema=1,extractor_version=29,quality_version=1;
    SceneContext context;
    Nanoseconds extraction_start_ns=0,extraction_end_ns=0;
    GameUi ui=GameUi::unknown;
    bool playing_gate=false,capacity_valid=true,source_valid=true;
    std::string history_source="baseline_guided";
    std::vector<LineCandidate> lines;
    std::vector<TrackingCandidate> candidates;
};

// Read-only diagnostic trigger. Never supplied to the planner or scheduler.
// Two current positive frames arm; two missing frames over >=12 ms trigger.
class ComboVisibilityDiagnostic final {
public:
    bool observe(const DecisionSnapshot& scene);
private:
    SceneContext previous_;
    int present_ = 0, absent_ = 0;
    bool armed_ = false;
    Nanoseconds absent_since_ = 0;
};

struct HoldDisappearance {
    std::uint64_t source_frame=0,note_id=0;
    Nanoseconds capture_ns=0;
    Vec2 head;
    double width=0,height=0;
};
// At most 16 recent coordinate summaries; no frame or input state retained.
class HoldVisibilityDiagnostic final {
public:
    std::optional<HoldDisappearance> observe(const DecisionSnapshot& scene);
private:
    SceneContext previous_;
    std::array<HoldDisappearance,16> previous_holds_{};
    std::size_t previous_count_=0;
};

// All thresholds are development hypotheses. The observer supplies pixel
// evidence without injecting input. The runtime chooses dry or real transport.
// History has <=128 tracks, <=6 points each, no retained frames.
class GameLineTracker final {
public:
    void reset();
    void update(std::vector<LineCandidate>& lines,const SceneContext& context);
private:
    struct Pose {Vec2 center,tangent;Nanoseconds time=0;};
    struct Track {
        LineCandidate line;Nanoseconds time=0,bucket_start=0;
        std::deque<Pose> poses; // <=6 measured poses, <=90 ms, >=10 ms buckets.
    };
    static void fit_motion(Track& track,LineCandidate& current,Nanoseconds time);
    struct PendingBirth {LineCandidate line;Nanoseconds time=0;int consecutive=0;};
    std::vector<Track> tracks_;
    std::vector<PendingBirth> pending_births_; // <=16, current-only consecutive observations.
    SceneContext context_;
    std::uint64_t next_id_=0;
};
class GameObserver final {
public:
    explicit GameObserver(const Clock& clock,bool reuse_component_scratch=false,
        bool row_prescreen=true,int horizontal_line_gap_limit=4)
        : clock_(clock),reuse_component_scratch_(reuse_component_scratch),
          row_prescreen_(row_prescreen),
          horizontal_line_gap_limit_(horizontal_line_gap_limit) {
        if(horizontal_line_gap_limit<2||horizontal_line_gap_limit>6)
            throw std::invalid_argument("horizontal line gap research bound");
    }
    DecisionSnapshot process(const Frame& frame);
    const CandidateBatch& candidate_batch() const { return candidate_batch_; }
    void reset();
private:
    using History=GameTrackHistory;
    const Clock& clock_;
    SceneContext previous_;
    std::vector<History> tracks_;
    std::uint64_t next_id_ = 0, sequence_ = 0;
    int playing_confirmations_ = 0;
    int menu_confirmations_ = 0;
    CandidateBatch candidate_batch_;
    GameLineTracker line_tracker_;
    // Offline A/B scratch option. Runtime keeps local allocation after the
    // interleaved RGB ablation found no compute-tail improvement. At 1280x720
    // the optional storage is <=230400 mask bytes plus <=921600 queue bytes.
    std::vector<std::uint8_t> component_mask_;
    std::vector<int> component_queue_;
    bool reuse_component_scratch_=false;
    bool row_prescreen_=true;
    int horizontal_line_gap_limit_=4;
};

// Single-thread owner. Note identity is distinct from monotonically assigned
// submission identity; snapshots are complete sets so skipped snapshots do
// not lose cancellations. No real backend is constructed by observe runtime.
struct GameActionOptions {
    int enabled_types=1;
    Nanoseconds lead_ns=8'000'000, uncertainty_ns=30'000'000;
};
class GamePlanOwner final {
public:
    GamePlanOwner(const Clock& clock, TouchBackend& backend, int contacts = 2,
                  GameActionOptions options = {});
    std::vector<TouchReceipt> accept(const DecisionSnapshot& snapshot);
    std::vector<TouchReceipt> poll();
    void stop();
    ContactScheduler& scheduler() { return scheduler_; }
    const std::string& last_rejection() const { return last_rejection_; }
    std::vector<ContactPlan> take_accepted_plans();
    std::vector<nlohmann::json> take_coverage_updates();
    std::vector<nlohmann::json> take_plan_cancellations();
private:
    struct Identity { std::uint64_t intent, revision; Nanoseconds expires; bool submitted = false;
        NoteKind kind=NoteKind::ambiguous; ContactPlan plan;
        std::uint64_t drag_leader=0;bool shared_drag=false;
        std::optional<Nanoseconds> hold_tail_release_ns;
        NoteCandidate last_note; std::uint64_t line_id=0;
        std::optional<Nanoseconds> tail_first_pass_ns; };
    struct ContactAlias {std::uint64_t owner=0;Nanoseconds last_seen_ns=0;};
    std::map<std::uint64_t,ContactAlias> contact_aliases_;
    const Clock& clock_;
    ContactScheduler scheduler_;
    std::map<std::uint64_t, Identity> identities_;
    std::uint64_t next_intent_ = 0, last_snapshot_ = 0, epoch_ = 0;
    SceneContext context_;
    std::string last_rejection_;
    GameActionOptions options_;
    std::vector<ContactPlan> accepted_plans_;
    std::vector<nlohmann::json> coverage_updates_;
    std::vector<nlohmann::json> plan_cancellations_;
};

class PlayButtonPlanner final {
public:
    std::optional<ContactPlan> take(const DecisionSnapshot& scene, Nanoseconds now);
private:
    bool attempted_ = false;
};

nlohmann::json decision_json(const DecisionSnapshot& scene);
nlohmann::json analyze_game_jsonl(const std::filesystem::path& path);
nlohmann::json analyze_game_round(const std::filesystem::path& round_directory);
nlohmann::json replay_game_pixel_clips(const std::filesystem::path& clips_directory,
    const std::filesystem::path& overlay_directory = {},int overlay_round = 0,int overlay_clip = 0,
    bool reuse_component_scratch = false,bool row_prescreen = true,
    int horizontal_line_gap_limit = 4);
nlohmann::json benchmark_game_pixel_clips(const std::filesystem::path& clips_directory,
    int batches,int replays_per_mode,bool benchmark_row_prescreen = false,
    bool row_prescreen_aa = false);
nlohmann::json benchmark_game_cold_pipeline(const std::filesystem::path& journal_path,
    int frames,int cadence_ms,bool jitter,bool row_prescreen,
    const std::string& scene,int writer_capacity,int writer_delay_us,int rpc_delay_ms);
nlohmann::json analyze_game_cold_pipeline_ab(const std::filesystem::path& directory);
nlohmann::json analyze_game_cold_pipeline_aa(const std::filesystem::path& directory,
    const std::filesystem::path& frozen_meter);
nlohmann::json analyze_game_line_gap_sweep();
nlohmann::json analyze_game_cold_c5_gate(const std::filesystem::path& directory);
nlohmann::json index_game_pixel_corpus(const std::filesystem::path& old_clips,
    const std::filesystem::path& old_results,const std::filesystem::path& new_clips,
    const std::filesystem::path& new_results);
nlohmann::json write_game_corpus_proposals(const std::filesystem::path& corpus_index,
    const std::filesystem::path& output_directory);
nlohmann::json validate_game_corpus_proposals(const std::filesystem::path& corpus_index,
    const std::filesystem::path& proposals_manifest);
nlohmann::json validate_game_cold_coverage_manifest(const std::filesystem::path& manifest_path);
nlohmann::json compare_game_pixel_replays(const std::filesystem::path& corpus_index,
    const std::filesystem::path& before_old,const std::filesystem::path& after_old,
    const std::filesystem::path& before_new,const std::filesystem::path& after_new);
void draw_game_overlay(Frame& frame, const DecisionSnapshot& scene);
} // namespace pas
