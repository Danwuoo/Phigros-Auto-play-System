#pragma once
#include "pas/core.hpp"
#include <deque>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>

namespace pas {
struct FullRecordingOptions {
    std::size_t pre_roll_frames=32,queue_capacity=64;
    std::uint64_t frame_limit=36000,byte_limit=5ULL*1024*1024*1024;
    Nanoseconds duration_limit=600'000'000'000LL;
};
// Same-stream, lossless diagnostic copies only. Does not hold LatestFrame
// leases, feed recognition, or silently discard an active-round frame.
class SessionRecording final {
public:
    SessionRecording(std::filesystem::path root,const Clock&,int width,int height,FullRecordingOptions={});
    ~SessionRecording();
    void observe(const Frame&); // capture producer
    void start(std::uint64_t round); // perception lifecycle; flush bounded pre-roll
    void finish(std::uint64_t round);
    void close();
    bool faulted() const;
    std::string error() const;
    nlohmann::json summary() const;
private:
    struct Sample {Frame frame;Nanoseconds copy_start=0,copy_end=0;bool pre_roll=false;};
    void writer();
    std::filesystem::path root_;
    const Clock& clock_;
    int width_,height_;
    FullRecordingOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::vector<std::unique_ptr<Sample>> slots_;
    std::vector<Sample*> free_;
    std::deque<Sample*> pre_,queue_;
    std::vector<std::jthread> workers_;
    std::ofstream index_;
    bool closed_=false,complete_=false;
    std::string error_;
    std::uint64_t round_=0,seen_=0,accepted_=0,written_=0,bytes_=0,peak_queue_=0;
    Nanoseconds first_ns_=0,last_ns_=0;
    std::uint64_t next_ordinal_=0,previous_frame_=0;
    std::optional<std::uint64_t> previous_source_;
    Nanoseconds previous_time_=0;
    std::vector<double> copy_ms_,encode_ms_,write_ms_,interval_ms_,commit_wait_ms_;
};
// Native PNG RGB24 chunks and existing zlib, level1/sub filter. Offline callers use
// this same function to check exact round trips and representative CPU cost.
std::vector<std::uint8_t> encode_recording_png(const Frame&);
}
