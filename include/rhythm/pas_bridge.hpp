#pragma once
#include "rhythm/chart_memory.hpp"
#include "pas/core.hpp"
#include <map>
#include <memory>
#include <optional>

namespace rhythm {
struct ExecutionObservation {
    Identity identity;
    ClockMap clock;
    SyncLease lease;
    std::uint64_t sequence = 0; // Actual audio/geometry observation, not planner tick.
};
struct PasBridgeConfig {
    int max_contacts = 2, max_plans = 64, max_steps = 16;
    int input_width = 1280, input_height = 720;
    Ns horizon_ns = 2'000'000'000, lookahead_ns = 100'000'000;
    Ns evidence_max_age_ns = 150'000'000, max_late_ns = 30'000'000;
    Ns max_uncertainty_ns = 20'000'000, input_lead_ns = 0;
    Ns commit_window_ns = 10'000'000; // Preserve already admitted near-term deadlines.
    std::size_t max_contacts_in_song = 200000, max_steps_in_song = 2000000;
};
struct BridgeTick {
    std::vector<pas::TouchReceipt> receipts;
    std::vector<pas::SchedulerNotice> notices;
    bool running = false, finished = false;
    std::string reason;
    std::size_t pending = 0, active = 0, completed_contacts = 0;
    pas::ReleaseReport release;
};

// Single owner thread: begin/update/tick/stop. request_stop alone is safe from a
// supervisor thread. The scheduler is owned here so its limits cannot silently
// differ from the bridge's bounded submissions. There is no automatic re-arm.
class PasBridge {
public:
    PasBridge(Memory memory, const pas::Clock& clock, pas::TouchBackend& backend,
              PasBridgeConfig config = {});
    ~PasBridge();
    PasBridge(const PasBridge&) = delete;
    PasBridge& operator=(const PasBridge&) = delete;
    bool begin(const ExecutionObservation& observation); // Strictly newer epoch after a stop.
    bool update(const ExecutionObservation& observation);
    BridgeTick tick();
    void stop(const std::string& reason);
    void request_stop();
    // Includes expiry and trajectory replenishment, not only scheduler due time.
    std::optional<Ns> next_wake_ns() const;
    bool running() const { return running_; }
    const std::string& reason() const { return reason_; }
    const pas::ContactScheduler& scheduler() const { return *scheduler_; }
private:
    struct Pending {
        std::size_t contact_index = 0;
        pas::ContactPlan submitted;
        std::optional<Ns> coverage_until;
    };
    bool observation_valid(const ExecutionObservation&, Ns now) const;
    bool dispatch_valid();
    bool revise(std::uint64_t intent, Pending& pending, std::uint64_t executed, Ns now);
    pas::ContactStep mapped(const Step& step) const;
    void refresh_plans(Ns now);
    bool release_ok() const;
    Memory memory_;
    const pas::Clock& clock_;
    pas::TouchBackend& backend_;
    PasBridgeConfig config_;
    std::unique_ptr<pas::ContactScheduler> scheduler_;
    std::vector<std::size_t> order_;
    std::map<std::uint64_t, Pending> pending_;
    ExecutionObservation observation_;
    std::uint64_t epoch_ = 0, next_intent_ = 1;
    std::size_t cursor_ = 0, completed_ = 0;
    bool running_ = false, finished_ = false;
    std::atomic<bool> user_stop_requested_{false};
    Ns last_tick_ns_ = -1;
    std::string reason_, guard_reason_;
};
} // namespace rhythm
