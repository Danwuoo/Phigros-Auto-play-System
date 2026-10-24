#pragma once
#include "rhythm/chart_memory.hpp"
#include <atomic>
#include <string>

namespace rhythm {
enum class ManualState { standby, starting, playing, suspended, result, aborted, fault, stopped };
enum class ManualTimestampKind { unknown, mapped_capture, arrival_only };
const char* manual_state_name(ManualState state);
const char* manual_timestamp_kind_name(ManualTimestampKind kind);
struct ManualConfig {
    unsigned round_limit=1; // Explicitly bounded, 1..20 confirmed rounds.
    Ns round_watchdog_ns=600'000'000'000LL; // Abort, never result; zero disables.
    Ns max_observation_age_ns=100'000'000;
    Ns max_continuity_gap_ns=250'000'000;
};
struct ManualObservation {
    std::uint64_t sequence=0,generation=0,geometry_version=0;
    int width=0,height=0,rotation=0; // Rotation is a quarter turn, 0..3.
    ManualTimestampKind timestamp_kind=ManualTimestampKind::unknown;
    Ns capture_host_ns=-1,received_host_ns=-1;
    std::string evidence_id; // Nonempty, <=512 bytes; caller's actual frame provenance.
    bool source_valid=false,hud=false,menu=false,result=false,pause=false;
};
struct ManualStatus {
    ManualState state=ManualState::standby;
    std::uint64_t round=0,observation_sequence=0;
    unsigned completed_rounds=0;
    bool active=false,new_round=false,ended=false,result_confirmed=false,session_done=false;
    bool ui_playing=false,capture_timestamp_known=false;
    ManualTimestampKind timestamp_kind=ManualTimestampKind::unknown;
    Ns observed_host_ns=-1,received_host_ns=-1;
    std::string evidence_id,reason="startup_standby";
};

// Single owner; only request_user_stop may run on another thread. UI lifecycle
// only: no classifier, song clock, SyncLease, arming, touch, or release I/O.
// Arrival-only observations time UI evidence at receipt, not at screen capture.
class ManualLifecycle {
public:
    explicit ManualLifecycle(ManualConfig config={});
    ManualStatus observe(const ManualObservation& observation,Ns now);
    ManualStatus poll(Ns now); // Expiry/watchdog only; never manufactures evidence.
    ManualStatus status(Ns now); // Same semantics as poll, including expiry.
    void request_user_stop() noexcept; // Sticky for this object's entire lifetime.
private:
    bool check_time(Ns now);
    void clear_debounce();
    void suspend(const char* reason);
    void terminal(ManualState state,const char* reason);
    ManualConfig config_;
    ManualStatus status_;
    ManualObservation previous_;
    std::atomic<bool> user_stop_requested_{false};
    bool have_source_=false,rearm_=true,left_result_=false;
    unsigned hud_count_=0,result_count_=0;
    std::vector<std::string> support_ids_; // <=64 IDs; no discarded support while debounce is pending.
    std::vector<std::string> recent_ids_; // <=64 actual frame IDs, including across UI class transitions.
    Ns last_now_=-1,round_started_ns_=-1,hud_since_ns_=-1,result_since_ns_=-1;
};
} // namespace rhythm
