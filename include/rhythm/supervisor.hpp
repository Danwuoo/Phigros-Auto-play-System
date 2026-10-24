#pragma once
#include "rhythm/chart_memory.hpp"
#include <atomic>
#include <optional>
#include <string>

namespace rhythm {
enum class ObservedGameState { playing, paused, result, unknown };
enum class StateObservationSource { visual, manual_confirmation };
enum class StateTimestampKind { mapped_capture, host_confirmation, arrival_only, unknown };
enum class SupervisorMode { shadow, fake_touch, live };
enum class PlanCoverage { full_chart, explicit_segment };
struct PlanQualification {
    Identity identity;
    std::uint64_t geometry_version=0;
    std::string compiled_cache_sha256;
    PlanCoverage coverage=PlanCoverage::full_chart;
    bool all_notes_in_scope_compiled=false,requires_calibration=true;
    std::optional<Ns> segment_begin_ns,segment_end_ns;
    bool timing_qualified=false,geometry_qualified=false,backend_qualified=false;
    std::string timing_evidence_id,geometry_evidence_id,backend_evidence_id;
};
struct StateObservation {
    ObservedGameState state=ObservedGameState::unknown;
    StateObservationSource source=StateObservationSource::visual;
    StateTimestampKind timestamp_kind=StateTimestampKind::unknown;
    Identity identity;
    std::uint64_t sequence=0,epoch=0,geometry_version=0;
    std::string evidence_id; // Actual frame/confirmation provenance, not a polling tick.
    Ns observed_host_ns=-1,received_host_ns=-1,expires_host_ns=-1;
    Ns timestamp_uncertainty_ns=-1; // Unknown by default; never silently zero.
};
struct SupervisorSyncObservation {
    Identity identity;
    ClockMap clock;
    SyncLease lease;
    std::uint64_t sequence=0;
    Ns received_host_ns=-1;
    std::string evidence_id;
};
struct SupervisorConfig {
    SupervisorMode mode=SupervisorMode::shadow;
    Ns max_state_age_ns=150'000'000,max_manual_age_ns=1'000'000'000;
    Ns max_sync_age_ns=150'000'000,max_sync_lease_ns=250'000'000;
    Ns max_release_ack_age_ns=150'000'000;
    Ns max_state_uncertainty_ns=20'000'000,max_sync_uncertainty_ns=20'000'000;
    Ns max_run_duration_ns=30'000'000'000;
    double max_clock_drift_ppm=3000;
    bool allow_manual_confirmation=true;
    // Engineering budgets; live qualification must cite measurements separately.
};
enum class SupervisorState { waiting, ready, armed, revoked, user_stopped };
enum class SupervisorReason {
    none, missing_state, missing_sync, invalid_observation, stale_state, stale_sync,
    not_playing, identity_mismatch, geometry_mismatch, epoch_mismatch, retired_epoch,
    unqualified_plan, unqualified_timestamp, uncertainty, clock_fault, run_limit,
    explicit_invalidation, user_stop, release_pending, release_unknown, release_failed,
    invalid_release_ack, sequence_exhausted
};
const char* supervisor_reason_name(SupervisorReason reason);
struct SupervisorGrant {
    SupervisorMode mode=SupervisorMode::shadow;
    Identity identity;
    ClockMap clock;
    SyncLease lease; // Conservatively bounded by BOTH actual observation lifetimes.
    std::uint64_t sequence=0,state_sequence=0,sync_sequence=0;
    std::string compiled_cache_sha256;
};
struct SupervisorDecision {
    SupervisorState state=SupervisorState::waiting;
    SupervisorReason reason=SupervisorReason::missing_state;
    bool dispatch_permitted=false,live_permitted=false,release_required=false;
    bool manual_state=false,capture_timestamp_known=false;
    std::uint64_t revocation_id=0,revoked_epoch=0;
    SupervisorReason release_reason=SupervisorReason::none;
    SupervisorReason revocation_reason=SupervisorReason::none;
    std::optional<SupervisorGrant> grant;
};
enum class ReleaseKnowledge { unknown, failed, known_released };
struct ReleaseAcknowledgement {
    std::uint64_t revocation_id=0,epoch=0;
    ReleaseKnowledge knowledge=ReleaseKnowledge::unknown;
    Ns observed_host_ns=-1,received_host_ns=-1;
    std::string evidence_id;
};

// Pure single-owner contract. Only request_user_stop() may be called from a
// different thread. No classifier, device, automatic arm/re-arm, or release I/O.
class RunSupervisor {
public:
    RunSupervisor(PlanQualification qualification,SupervisorConfig config={});
    SupervisorDecision observe_state(StateObservation observation,Ns now);
    SupervisorDecision observe_sync(SupervisorSyncObservation observation,Ns now);
    SupervisorDecision arm(std::uint64_t epoch,Ns now);
    SupervisorDecision status(Ns now); // Does not renew evidence or increment sequence.
    SupervisorDecision invalidate(Ns now);
    SupervisorDecision acknowledge_release(const ReleaseAcknowledgement& acknowledgement,Ns now);
    void request_user_stop() noexcept; // Sticky for this object's entire lifetime.
private:
    bool check_now(Ns now);
    void revoke(SupervisorReason reason,Ns now,std::uint64_t observed_epoch=0);
    SupervisorReason health(Ns now) const;
    SupervisorDecision decision(Ns now) const;
    bool next_sequence(Ns now);
    PlanQualification qualification_;
    SupervisorConfig config_;
    std::optional<StateObservation> state_;
    std::optional<SupervisorSyncObservation> sync_;
    std::atomic<bool> user_stop_requested_{false};
    bool armed_=false,revoked_=false,release_pending_=false;
    SupervisorReason reason_=SupervisorReason::none,release_reason_=SupervisorReason::none;
    std::uint64_t epoch_=0,retired_epoch_=0,sequence_=0,revocation_id_=0,revoked_epoch_=0;
    std::uint64_t last_state_sequence_=0,last_sync_sequence_=0;
    Ns last_now_=-1,last_state_observed_=-1,last_sync_observed_=-1;
    Ns run_deadline_=-1,revoked_host_ns_=-1;
};
} // namespace rhythm
