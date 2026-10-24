#include "rhythm/manual_lifecycle.hpp"
#include <limits>
#include <stdexcept>

namespace rhythm {
namespace {
constexpr Ns ms=1'000'000;
bool known_kind(ManualTimestampKind kind) {
    return kind==ManualTimestampKind::mapped_capture||kind==ManualTimestampKind::arrival_only;
}
Ns observation_time(const ManualObservation& observation) {
    return observation.timestamp_kind==ManualTimestampKind::mapped_capture?
        observation.capture_host_ns:observation.received_host_ns;
}
}
const char* manual_state_name(ManualState state) {
    switch(state) {
    case ManualState::standby:return "STANDBY";
    case ManualState::starting:return "STARTING";
    case ManualState::playing:return "PLAYING";
    case ManualState::suspended:return "SUSPENDED";
    case ManualState::result:return "RESULT";
    case ManualState::aborted:return "ABORTED";
    case ManualState::fault:return "FAULT";
    case ManualState::stopped:return "STOPPED";
    }
    return "FAULT";
}
const char* manual_timestamp_kind_name(ManualTimestampKind kind) {
    switch(kind) {
    case ManualTimestampKind::unknown:return "unknown";
    case ManualTimestampKind::mapped_capture:return "mapped_capture";
    case ManualTimestampKind::arrival_only:return "arrival_only";
    }
    return "unknown";
}
ManualLifecycle::ManualLifecycle(ManualConfig config):config_(config) {
    if(config.round_limit<1||config.round_limit>20||config.round_watchdog_ns<0||
        config.round_watchdog_ns>3'600'000*ms||config.max_observation_age_ns<=0||
        config.max_observation_age_ns>1'000*ms||config.max_continuity_gap_ns<=0||
        config.max_continuity_gap_ns>10'000*ms)
        throw std::invalid_argument("invalid manual lifecycle configuration");
}
void ManualLifecycle::clear_debounce() {
    hud_count_=result_count_=0;hud_since_ns_=result_since_ns_=-1;support_ids_.clear();
}
void ManualLifecycle::suspend(const char* reason) {
    clear_debounce();status_.ui_playing=false;status_.reason=reason;
    if(status_.active)status_.state=ManualState::suspended;
}
void ManualLifecycle::terminal(ManualState state,const char* reason) {
    status_.ended=status_.active;status_.new_round=false;status_.active=false;
    status_.ui_playing=false;status_.result_confirmed=false;status_.session_done=true;
    status_.state=state;status_.reason=reason;clear_debounce();
}
bool ManualLifecycle::check_time(Ns now) {
    status_.new_round=false;status_.ended=false;
    if(status_.session_done)return false; // Preserve the first terminal outcome, including confirmed result.
    if(user_stop_requested_.load(std::memory_order_acquire)) {
        terminal(ManualState::stopped,"user_stop");return false;
    }
    if(now<0||(last_now_>=0&&now<last_now_)) {
        terminal(ManualState::fault,"host_clock_rollback_or_invalid");return false;
    }
    last_now_=now;
    if(status_.active&&config_.round_watchdog_ns&&
        now-round_started_ns_>=config_.round_watchdog_ns) {
        terminal(ManualState::aborted,"round_watchdog_aborted");return false;
    }
    if(status_.observed_host_ns>=0&&now-status_.observed_host_ns>=config_.max_observation_age_ns)
        suspend("observation_expired");
    return true;
}
ManualStatus ManualLifecycle::poll(Ns now) {check_time(now);return status_;}
ManualStatus ManualLifecycle::status(Ns now) {return poll(now);}
void ManualLifecycle::request_user_stop() noexcept {
    user_stop_requested_.store(true,std::memory_order_release);
}
ManualStatus ManualLifecycle::observe(const ManualObservation& observation,Ns now) {
    if(!check_time(now))return status_;
    const int evidence_count=static_cast<int>(observation.hud)+static_cast<int>(observation.menu)+
        static_cast<int>(observation.result)+static_cast<int>(observation.pause);
    if(!observation.sequence||!observation.generation||!observation.geometry_version||
        observation.width<2||observation.width>16384||observation.height<2||observation.height>16384||
        observation.rotation<0||observation.rotation>3||!known_kind(observation.timestamp_kind)||
        observation.evidence_id.empty()||observation.evidence_id.size()>512||evidence_count>1||
        observation.received_host_ns<0||observation.received_host_ns>now||
        (observation.timestamp_kind==ManualTimestampKind::mapped_capture&&
            (observation.capture_host_ns<0||observation.capture_host_ns>observation.received_host_ns))||
        (observation.timestamp_kind==ManualTimestampKind::arrival_only&&observation.capture_host_ns!=-1)) {
        terminal(ManualState::fault,"invalid_observation");return status_;
    }
    if(have_source_) {
        if(observation.generation!=previous_.generation||observation.geometry_version!=previous_.geometry_version||
            observation.width!=previous_.width||observation.height!=previous_.height||
            observation.rotation!=previous_.rotation||observation.timestamp_kind!=previous_.timestamp_kind) {
            terminal(ManualState::fault,"source_geometry_or_timestamp_kind_changed");return status_;
        }
        if(observation.sequence<previous_.sequence||observation.received_host_ns<previous_.received_host_ns||
            observation_time(observation)<observation_time(previous_)) {
            terminal(ManualState::fault,"observation_sequence_or_clock_rollback");return status_;
        }
        if(observation.sequence==previous_.sequence) {
            suspend("duplicate_observation_sequence");return status_;
        }
        if(observation.evidence_id==previous_.evidence_id||observation_time(observation)==observation_time(previous_)) {
            previous_=observation;suspend("non_distinct_frame_or_timestamp");return status_;
        }
        const bool same_support=(observation.hud&&hud_count_)||(observation.result&&result_count_);
        if(std::find(recent_ids_.begin(),recent_ids_.end(),observation.evidence_id)!=recent_ids_.end()||
            (same_support&&std::find(support_ids_.begin(),support_ids_.end(),observation.evidence_id)!=support_ids_.end())) {
            previous_=observation;suspend("replayed_frame_provenance");return status_;
        }
        if(same_support&&support_ids_.size()>=64&&!status_.ui_playing) {
            previous_=observation;suspend("debounce_support_budget");return status_;
        }
        if(observation_time(observation)-observation_time(previous_)>config_.max_continuity_gap_ns)
            suspend("observation_continuity_gap");
    }
    previous_=observation;have_source_=true;
    if(recent_ids_.size()==64)recent_ids_.erase(recent_ids_.begin());
    recent_ids_.push_back(observation.evidence_id);
    status_.observation_sequence=observation.sequence;status_.timestamp_kind=observation.timestamp_kind;
    status_.capture_timestamp_known=observation.timestamp_kind==ManualTimestampKind::mapped_capture;
    status_.observed_host_ns=observation_time(observation);status_.received_host_ns=observation.received_host_ns;
    status_.evidence_id=observation.evidence_id;
    if(!observation.source_valid) {suspend("invalid_source_frame");return status_;}
    if(now-status_.observed_host_ns>=config_.max_observation_age_ns) {
        suspend("stale_observation");return status_;
    }
    if(!status_.active) {
        if(observation.result) {
            rearm_=false;left_result_=false;status_.ui_playing=false;
            status_.reason="result_latched_waiting_manual_transition";return status_;
        }
        if(observation.menu) {rearm_=true;left_result_=false;}
        if(!observation.hud&&!observation.menu&&!observation.pause)left_result_=true;
        if(left_result_&&observation.hud)rearm_=true;
        if(!rearm_||!observation.hud) {
            clear_debounce();status_.ui_playing=false;
            if(observation.menu||left_result_) {status_.state=ManualState::standby;status_.result_confirmed=false;}
            status_.reason=rearm_?"waiting_for_manual_play_HUD":"result_latched_waiting_manual_transition";
            return status_;
        }
        if(status_.round==std::numeric_limits<std::uint64_t>::max()||
            status_.completed_rounds>=config_.round_limit) {
            terminal(ManualState::fault,"round_limit_or_id_exhausted");return status_;
        }
        ++status_.round;status_.active=true;status_.new_round=true;status_.result_confirmed=false;
        status_.state=ManualState::starting;status_.reason="fresh_HUD_after_manual_transition";
        round_started_ns_=status_.observed_host_ns;clear_debounce();rearm_=false;left_result_=false;
        if(config_.round_watchdog_ns&&now-round_started_ns_>=config_.round_watchdog_ns) {
            terminal(ManualState::aborted,"round_watchdog_aborted");return status_;
        }
    }
    if(observation.result) {
        hud_count_=0;hud_since_ns_=-1;status_.ui_playing=false;status_.state=ManualState::suspended;
        status_.reason="result_confirmation_pending";
        if(!result_count_) {result_since_ns_=status_.observed_host_ns;support_ids_.clear();}
        support_ids_.push_back(observation.evidence_id);
        result_count_=std::min(3u,result_count_+1);
        if(result_count_>=3&&status_.observed_host_ns-result_since_ns_>=60*ms) {
            status_.state=ManualState::result;status_.active=false;status_.ended=true;
            status_.result_confirmed=true;++status_.completed_rounds;
            status_.session_done=status_.completed_rounds>=config_.round_limit;
            status_.reason=status_.session_done?"confirmed_result_round_limit_completed":"confirmed_result_waiting_manual_transition";
            rearm_=false;left_result_=false;clear_debounce();
        }
        return status_;
    }
    result_count_=0;result_since_ns_=-1;
    if(!observation.hud) {
        suspend(observation.pause?"paused_UI":"unknown_or_menu_UI");return status_;
    }
    if(!hud_count_) {hud_since_ns_=status_.observed_host_ns;support_ids_.clear();}
    // After confirmation, a bounded recent-ID ring still rejects replay. Pending
    // debounce cannot discard support to hide reuse of its first anchor.
    if(support_ids_.size()==64)support_ids_.erase(support_ids_.begin());
    support_ids_.push_back(observation.evidence_id);
    hud_count_=std::min(3u,hud_count_+1);
    if(hud_count_>=3&&status_.observed_host_ns-hud_since_ns_>=20*ms) {
        status_.state=ManualState::playing;status_.ui_playing=true;status_.reason="fresh_HUD_confirmed";
    } else {
        status_.state=ManualState::starting;status_.ui_playing=false;status_.reason="HUD_confirmation_pending";
    }
    return status_;
}
} // namespace rhythm
