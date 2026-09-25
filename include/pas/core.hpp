#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace pas {

using Nanoseconds = std::int64_t;

class Clock {
public:
    virtual ~Clock() = default;
    virtual Nanoseconds now_ns() const = 0;
};

class HostClock final : public Clock {
public:
    HostClock();
    Nanoseconds now_ns() const override;
    std::int64_t frequency() const { return frequency_; }
private:
    std::int64_t frequency_;
};

class FakeClock final : public Clock {
public:
    Nanoseconds now_ns() const override { return now_; }
    void set(Nanoseconds value) { now_ = value; }
private:
    Nanoseconds now_ = 0;
};

struct Frame {
    std::uint64_t sequence = 0;
    std::uint64_t epoch = 0;
    std::uint64_t generation = 0;
    std::uint64_t geometry_version = 0;
    int width = 0;
    int height = 0;
    int stride = 0;
    int source_rotation = 0;
    Nanoseconds capture_complete_ns = 0;
    Nanoseconds pixels_ready_ns = 0;
    Nanoseconds published_ns = 0;
    std::optional<std::int64_t> source_timestamp_us;
    std::optional<std::uint64_t> source_sequence;
    std::vector<std::uint8_t> rgb;
};

struct FrameCounters {
    std::uint64_t published = 0;
    std::uint64_t overwritten = 0;
    std::uint64_t pool_drops = 0;
    std::uint64_t consumer_skips = 0;
};

// Exactly three preallocated physical buffers and one logical latest slot.
// A leased buffer cannot be modified. Exhaustion drops the incoming frame.
class LatestFrame final {
public:
    LatestFrame(int width, int height, std::size_t slots = 3, const Clock* clock = nullptr);
    bool publish(const std::uint8_t* pixels, std::size_t bytes, const Frame& metadata);
    std::shared_ptr<const Frame> read_after(std::uint64_t sequence, Nanoseconds timeout_ns);
    std::shared_ptr<const Frame> peek() const;
    FrameCounters counters() const;
    void close();
private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::vector<std::shared_ptr<Frame>> slots_;
    std::shared_ptr<Frame> latest_;
    FrameCounters counters_;
    std::uint64_t last_consumed_ = 0;
    bool consumed_any_ = false;
    bool closed_ = false;
    int width_;
    int height_;
    std::size_t bytes_;
    const Clock* clock_;
};

struct Observation {
    std::uint64_t frame_sequence = 0;
    Nanoseconds capture_complete_ns = 0;
    Nanoseconds recognition_complete_ns = 0;
    double x = 0;
    double y = 0;
};

struct Track {
    std::uint64_t track_id = 0;
    std::uint64_t frame_sequence = 0;
    Nanoseconds capture_complete_ns = 0;
    Nanoseconds recognition_complete_ns = 0;
    double x = 0;
    double y = 0;
    double vx_px_s = 0;
    double vy_px_s = 0;
    double residual_px = 0;
};

struct HitIntent {
    std::uint64_t intent_id = 0;
    std::uint64_t track_id = 0;
    std::uint64_t frame_sequence = 0;
    double x = 0;
    double y = 0;
    Nanoseconds predicted_hit_ns = 0;
    std::string basis;
    double confidence = 0;
    Nanoseconds uncertainty_ns = 0;
};

class GreenTargetDetector final {
public:
    explicit GreenTargetDetector(const Clock& clock) : clock_(clock) {}
    std::optional<Observation> detect(const Frame& frame) const;
private:
    const Clock& clock_;
};

class VelocityTracker final {
public:
    void clear();
    std::optional<Track> update(std::optional<Observation> observation);
private:
    std::vector<Observation> history_;
    std::uint64_t track_id_ = 0;
};

class LineCrossingPredictor final {
public:
    explicit LineCrossingPredictor(double line_y) : line_y_(line_y) {}
    std::optional<HitIntent> predict(const Track& track) const;
private:
    double line_y_;
};

enum class Phase { down, move, up };

struct ContactStep {
    Phase phase;
    double x;
    double y;
    Nanoseconds due_ns;
};

struct ContactPlan {
    std::uint64_t epoch = 0;
    // The producer assigns increasing birth IDs within an epoch. A revision
    // retains the same ID; a truly new target gets a larger one.
    std::uint64_t intent_id = 0;
    std::uint64_t revision = 0;
    Nanoseconds evidence_ns = 0;
    Nanoseconds valid_until_ns = 0;
    std::uint64_t source_frame_sequence = 0;
    std::string basis;
    std::vector<ContactStep> steps;
};

struct TouchCommand {
    std::uint64_t intent_id;
    int contact_id;
    Phase phase;
    double x;
    double y;
    Nanoseconds scheduled_ns;
    std::uint64_t source_frame_sequence;
};

struct TouchReceipt {
    TouchCommand command;
    Nanoseconds injection_start_ns;
    Nanoseconds injection_return_ns;
    bool success;
    std::string reason;
};

struct ReleaseReport {
    std::vector<int> requested_ids;
    std::vector<int> failed_ids;
    std::vector<int> unknown_ids;
    Nanoseconds start_ns = 0;
    Nanoseconds return_ns = 0;
};

class TouchBackend {
public:
    virtual ~TouchBackend() = default;
    virtual TouchReceipt inject(const TouchCommand& command) = 0;
    virtual ReleaseReport release_all() = 0;
};

class FakeTouchBackend final : public TouchBackend {
public:
    explicit FakeTouchBackend(const Clock& clock) : clock_(clock) {}
    TouchReceipt inject(const TouchCommand& command) override;
    ReleaseReport release_all() override;
    const std::map<int, std::array<double, 2>>& contacts() const { return contacts_; }
    const std::vector<TouchReceipt>& receipts() const { return receipts_; }
    std::function<void(const TouchCommand&, Nanoseconds)> on_touch;
private:
    const Clock& clock_;
    std::map<int, std::array<double, 2>> contacts_;
    std::vector<TouchReceipt> receipts_;
};

class PixelCoordinateMap final {
public:
    PixelCoordinateMap(int frame_width, int frame_height, int touch_width,
                       int touch_height, int rotation_deg);
    std::array<int, 2> map(double x, double y) const;
private:
    int fw_, fh_, tw_, th_, rotation_;
};

class ContactScheduler final {
public:
    ContactScheduler(const Clock& clock, TouchBackend& backend, int max_contacts = 2,
                     int max_plans = 64, int max_steps = 16,
                     Nanoseconds horizon_ns = 2'000'000'000,
                     Nanoseconds evidence_max_age_ns = 150'000'000,
                     Nanoseconds max_late_ns = 30'000'000);
    bool set_gate(std::uint64_t epoch, bool armed, Nanoseconds evidence_ns = 0);
    bool submit(ContactPlan plan);
    std::optional<Nanoseconds> next_due_ns() const;
    std::vector<TouchReceipt> run_due();
    // May be called by the supervisor. Linearizes against each injection;
    // the owner thread performs the actual release in run_due/cancel.
    void request_stop();
    void cancel(const std::string& reason);
    bool armed() const { return armed_; }
    const std::string& fault() const { return fault_; }
    std::size_t pending_count() const { return pending_.size(); }
    std::size_t active_count() const;
    std::uint64_t accepted_high_watermark() const { return accepted_high_watermark_; }
    ReleaseReport last_release() const { return last_release_; }
private:
    struct Pending { ContactPlan plan; int contact_id; std::size_t next_step = 0; bool active = false; };
    bool evidence_expired(Nanoseconds now) const;
    void fail(const std::string& reason);
    const Clock& clock_;
    TouchBackend& backend_;
    const int max_contacts_, max_plans_, max_steps_;
    const Nanoseconds horizon_ns_, evidence_max_age_ns_, max_late_ns_;
    std::map<std::uint64_t, Pending> pending_;
    std::uint64_t epoch_ = 0;
    std::uint64_t accepted_high_watermark_ = 0;
    bool armed_ = false;
    Nanoseconds gate_evidence_ns_ = 0;
    std::string fault_;
    ReleaseReport last_release_;
    std::mutex dispatch_mutex_;
    bool stop_requested_ = false;
};

} // namespace pas
