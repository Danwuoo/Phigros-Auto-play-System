#pragma once
#include "rhythm/contact_compiler.hpp"
#include "rhythm/pas_bridge.hpp"
#include "rhythm/supervisor.hpp"

namespace rhythm {
struct SessionPreflight {
    bool accepted=false,live_permitted=false;
    std::vector<std::string> reasons;
};
// Pure offline check. Live is deliberately unavailable in this runner version.
SessionPreflight preflight_session(const CompiledContactPlan& plan,
    const PlanQualification& qualification,SupervisorMode mode);
SupervisorConfig fake_session_config();

struct SessionStatus {
    SupervisorDecision supervision;
    std::optional<BridgeTick> dispatched; // Only this call's scheduler work; never replay old receipts.
    bool running=false,finished=false;
    std::uint64_t forwarded_sequence=0;
    std::string terminal_reason;
    pas::ReleaseReport release; // RPC/FakeTouch report, not an independent Android observation.
};

// Single owner, concrete FakeTouch backend only. No device, file loader, timing
// producer, classifier, implicit arm, or live backend factory is provided.
class SessionRunner {
public:
    SessionRunner(CompiledContactPlan plan,PlanQualification qualification,
        const pas::Clock& clock,pas::FakeTouchBackend& backend,
        SupervisorConfig config=fake_session_config());
    SessionStatus observe_state(StateObservation observation);
    SessionStatus observe_sync(SupervisorSyncObservation observation);
    SessionStatus arm(std::uint64_t epoch);
    SessionStatus tick(); // status/expiry first, then at most one bridge tick.
    SessionStatus status(); // No new observation, grant sequence, or scheduler dispatch.
    SessionStatus invalidate();
    SessionStatus acknowledge_release(ReleaseAcknowledgement acknowledgement);
    void request_user_stop(); // Thread-safe, sticky; release I/O remains on the owner.
    std::optional<Ns> next_wake_ns() const;
    const CompiledContactPlan& plan() const { return plan_; }
    const PlanQualification& qualification() const { return qualification_; }
    const PasBridgeConfig& bridge_config() const { return bridge_config_; }
private:
    SessionStatus pump(bool dispatch);
    const CompiledContactPlan plan_;
    const PlanQualification qualification_;
    const pas::Clock& clock_;
    pas::FakeTouchBackend& backend_;
    PasBridgeConfig bridge_config_;
    RunSupervisor supervisor_;
    PasBridge bridge_;
    std::uint64_t forwarded_sequence_=0,bridge_epoch_=0;
    bool active_=false,finished_=false;
    std::string terminal_reason_;
};
} // namespace rhythm
