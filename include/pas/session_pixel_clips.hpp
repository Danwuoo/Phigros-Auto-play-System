#pragma once
#include "pas/game_session.hpp"
#include <filesystem>
#include <deque>
#include <thread>

namespace pas {

struct PixelClipOptions {
    int max_rounds=20, uniform_clips=8, event_clips=2, frames_per_clip=3;
    Nanoseconds uniform_interval_ns=20'000'000'000LL;
    Nanoseconds event_cooldown_ns=10'000'000'000LL;
    std::size_t queue_capacity=4;
};

// Diagnostic copies of current, full-field pixels only. One producer, one bounded
// writer; no saved frame or metadata is ever read by gameplay decisions.
class SessionPixelClips final {
public:
    SessionPixelClips(std::filesystem::path root,const Clock& clock,int width,int height,
                      PixelClipOptions options={});
    ~SessionPixelClips();
    void observe(const Frame&,const SessionObservation&);
    void close();
    nlohmann::json summary() const;
private:
    struct Sample {
        std::uint64_t round=0,clip=0;
        int index=0;
        std::string trigger;
        std::uint64_t source_frame=0;
        Nanoseconds capture_ns=0,recognition_start_ns=0,recognition_end_ns=0,copy_start_ns=0,copy_end_ns=0;
        std::optional<std::int64_t> source_timestamp_us;
        std::optional<std::uint64_t> source_sequence;
        int line_count=0,target_count=0;
        std::vector<std::uint8_t> rgb;
    };
    void writer();
    void finish_clip();
    std::filesystem::path root_;
    const Clock& clock_;
    int width_,height_;
    PixelClipOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Sample> queue_;
    std::jthread worker_;
    bool closed_=false;
    std::string error_;
    std::uint64_t round_=0,clip_=0;
    int uniform_=0,event_=0,clip_frame_=0,enqueued_round_=0;
    Nanoseconds round_start_=0,last_event_ns_=0;
    std::string trigger_;
    bool previous_complex_=false;
    std::size_t peak_queue_=0,accepted_=0,written_=0,dropped_=0,copy_samples_=0;
    double copy_sum_ms_=0,copy_max_ms_=0;
    std::vector<double> copy_latencies_ms_;
};
}
