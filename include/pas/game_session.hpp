#pragma once
#include "pas/game.hpp"

namespace pas {
enum class PlaySessionState { standby, starting, playing, result, fault, stopped };
const char* name(PlaySessionState state);
struct SessionUiEvidence {
    bool hud=false, menu=false, result=false;
    int result_labels=0;
    double result_similarity=0;
};
// Independent result/UI lifecycle. It never changes note candidates or fits.
SessionUiEvidence session_ui_pixels(const Frame&,const DecisionSnapshot&);
struct SessionStatus {
    PlaySessionState state=PlaySessionState::standby;
    std::uint64_t round=0;
    bool active=false, new_round=false, ended=false;
    std::string reason="startup_standby";
};
class PlaySessionLifecycle final {
public:
    explicit PlaySessionLifecycle(Nanoseconds watchdog=0);
    SessionStatus observe(const SceneContext&,const SessionUiEvidence&,bool playing_gate,Nanoseconds now);
    SessionStatus status() const { return status_; }
    SessionStatus stop(const std::string& reason,bool fault=false);
private:
    SessionStatus status_;
    SceneContext previous_;
    Nanoseconds watchdog_,start_=0,result_since_=0,hud_since_=0;
    int result_count_=0,hud_count_=0;
    bool rearm_=true, left_result_=false;
};
struct SessionObservation {
    DecisionSnapshot scene;
    SessionStatus status;
    SessionUiEvidence evidence;
    bool allow_down=false;
};
class SessionPerception final {
public:
    SessionPerception(const Clock& clock,Nanoseconds watchdog=0):clock_(clock),observer_(clock),lifecycle_(watchdog) {}
    SessionObservation process(const Frame&);
private:
    const Clock& clock_;GameObserver observer_;PlaySessionLifecycle lifecycle_;
};
// Wraps the linked gameplay strategy; a new owner is created only for a new round.
// Gate/source revocations cancel contacts while retaining completion identity.
class SessionGameOwner final {
public:
    SessionGameOwner(const Clock&,TouchBackend&,int contacts,GameActionOptions);
    void start(std::uint64_t round);
    std::vector<TouchReceipt> accept(const DecisionSnapshot&,bool allow);
    std::vector<TouchReceipt> poll();
    ReleaseReport suspend(const std::string& reason);
    ReleaseReport finish();
    ContactScheduler* scheduler() { return owner_?&owner_->scheduler():nullptr; }
    GamePlanOwner* owner() { return owner_.get(); }
private:
    const Clock& clock_; TouchBackend& backend_; int contacts_; GameActionOptions options_;
    std::uint64_t round_=0;
    std::unique_ptr<GamePlanOwner> owner_;
};
}
