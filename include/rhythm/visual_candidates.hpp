#pragma once
#include "rhythm/chart.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rhythm {
// Closed integer-nanosecond interval, in the explicitly named domain.
struct VisualTimeInterval { SongNs lower=0,upper=0; };
enum class VisualSourceClock { unknown, video_pts, arrival_only, mapped_capture };
struct OpeningVisualModel {
    std::string chart_sha256,model_id,model_sha256,calibration_id,calibration_sha256;
    // Only static line 0, horizontal tangent, above Tap/Drag, 1280x720.
    SongNs begin_ns=1'028'571'429,end_ns=12'000'000'000; // Search [begin,end), before first judgment.
    SongNs note_horizon_ns=45'000'000'000;
    double origin_x=640,origin_y=576,x_pixels_per_unit=72;
    double pose_tolerance_pixels=.00001; // Explicit near-static native float envelope, included in predictions.
    double approach_scale_lower=720/(2*.83175),approach_scale_upper=720/(2*.83175);
    double pixel_error=3,visible_top=30,visible_bottom=570;
    double horizontal_visibility_margin_pixels=90; // Full <=180px run fits; edge notes are not mandatory observations.
    // Conditional source-domain rate=1 model allowance, NOT measured accuracy.
    SongNs per_frame_time_allowance_ns=30'000'000;
};
struct VisualCandidateLimits {
    std::size_t max_notes=256,max_speed_pieces=256,max_centers=8;
    std::size_t max_candidates=1024,max_work=200000,max_observations=128;
    SongNs max_observation_age_ns=100'000'000,max_gap_ns=1'000'000'000;
};
struct VisualCenter { std::string id;double x=0,y=0; };
struct VisualCandidateFrame {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    int width=0,height=0;
    std::string source_id,source_sha256,frame_id,evidence_id,frame_sha256;
    std::string clock_id,opening_context_id;
    VisualSourceClock clock_kind=VisualSourceClock::unknown;
    SongNs received_host_ns=-1;
    std::optional<SongNs> source_time_ns; // video PTS only; never host time.
    std::optional<VisualTimeInterval> capture_host_interval; // caller-declared mapping only.
    bool source_valid=false;
    bool centers_complete_unoccluded=false; // Explicit caller/model assumption; never inferred from raw pixels.
    std::vector<VisualCenter> centers;
};
struct VisualNoteMatch { std::string center_id,note_id,note_json_pointer; };
struct VisualTimeCandidate {
    VisualTimeInterval song_time;
    std::string speed_event_json_pointer;
    double line_speed=0;
    std::vector<VisualNoteMatch> matches;
};
enum class VisualCandidateState { unknown, conditional_candidate, ambiguous, rejected };
struct VisualCandidateResult {
    VisualCandidateState state=VisualCandidateState::unknown;
    std::string reason="no_observation";
    bool timing_qualified=false,live_permitted=false,requires_new_epoch=false,unchanged_pixels=false;
    std::string model_id,model_sha256,calibration_id,calibration_sha256;
    VisualCandidateFrame observation;
    std::vector<VisualTimeCandidate> candidates; // all assignments/branches, never best-score-only.
    std::vector<VisualTimeInterval> song_intervals;
    std::vector<VisualTimeInterval> conditional_source_minus_song_intervals;
    std::vector<std::string> supporting_evidence_ids;
    std::size_t work=0;
};

// Pure candidate solver. No file/device/audio I/O, clock fit, SyncLease, arm or
// input. Construction validates supported model scope. begin_epoch is explicit;
// invalid provenance, gaps, resource exhaustion and lost temporal consistency
// require a newer epoch. Timer polling is deliberately absent.
class VisualTimingCandidateProducer {
public:
    VisualTimingCandidateProducer(const SemanticChart& chart,OpeningVisualModel model,
                                  VisualCandidateLimits limits={});
    ~VisualTimingCandidateProducer();
    VisualTimingCandidateProducer(VisualTimingCandidateProducer&&) noexcept;
    VisualTimingCandidateProducer& operator=(VisualTimingCandidateProducer&&) noexcept;
    VisualTimingCandidateProducer(const VisualTimingCandidateProducer&)=delete;
    VisualTimingCandidateProducer& operator=(const VisualTimingCandidateProducer&)=delete;
    void begin_epoch(std::uint64_t epoch);
    VisualCandidateResult observe(const VisualCandidateFrame& observation,SongNs now_host_ns);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rhythm
