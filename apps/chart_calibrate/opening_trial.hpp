#pragma once
#include "first_tap_trial.hpp"
#include <array>

namespace rhythm::calibration {
enum class OpeningTrialState { unarmed, waiting, down_claimed, holding, up_claimed, completed, cancelled };
enum class OpeningTrialPhase { down, up };
struct OpeningTap {
    SongNs song_ns=0;
    double x=0,y=0;
};
struct OpeningTrialAction {
    std::size_t tap_index=0;
    int contact_id=0;
    OpeningTrialPhase phase=OpeningTrialPhase::down;
    SongNs scheduled_host_ns=0;
    double x=0,y=0;
};
// The owner supplies an actual, already classified/debounced UI observation.
// This is arrival-only UI evidence, not a capture clock or a qualified lease.
struct OpeningUiEvidence {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    SongNs received_host_ns=-1;
    std::string source_id,evidence_id;
    bool playing=false,source_valid=false;
};
struct OpeningTrialStatus {
    OpeningTrialState state=OpeningTrialState::unarmed;
    std::string reason="waiting_for_actual_first_tap_anchor";
    std::size_t tap_index=0,down_attempts=0,completed_taps=0;
    std::optional<SongNs> down_due_host_ns,up_due_host_ns,anchor_expires_host_ns;
    std::uint64_t ui_sequence=0;
    bool requires_release=false,experiment_only=true,live_qualified=false;
};

// Three fixed Dlyrotz IN13 Taps. This contract never sends input, derives visual
// candidates, selects an alias, claims physical release, or changes production
// calibration gates. The caller must pass the real FirstTapTrial::arm output
// from the pinned chart/context, plus that original arm call's actual host time.
// TrialStatus is a value/provenance contract, not authenticated evidence.
class OpeningTrial {
    static constexpr SongNs ms=1'000'000,second=1'000'000'000;
    static constexpr SongNs safe=std::numeric_limits<SongNs>::max()/4;
    static constexpr SongNs age=100*ms,lateness=20*ms,hold=16*ms,horizon=3*second;
    static constexpr std::array<OpeningTap,3> notes_{{{12*second,352,576},
        {12*second+171'428'571,352,576},{12*second+514'285'714,928,576}}};
    OpeningTrialStatus value_;
    std::array<SongNs,3> due_{};
    OpeningUiEvidence ui_;
    bool have_ui_=false;
    SongNs last_now_=-1,down_claim_ns_=-1,up_claim_ns_=-1;
    std::uint64_t last_down_ui_sequence_=0;
    std::vector<std::string> seen_ui_ids_;

    bool terminal() const {return value_.state==OpeningTrialState::cancelled||value_.state==OpeningTrialState::completed;}
    bool clock(SongNs now) {
        if(terminal())return false;
        if(now<0||now>safe||(last_now_>=0&&now<last_now_)) {cancel("host_clock_invalid_or_rollback");return false;}
        last_now_=now;return true;
    }
    bool fresh(SongNs now) const {
        return have_ui_&&ui_.playing&&ui_.source_valid&&ui_.received_host_ns<=now&&now-ui_.received_host_ns<age;
    }
    bool health(SongNs now) {
        if(!clock(now))return false;
        if(value_.state==OpeningTrialState::unarmed)return true;
        if(!fresh(now)) {cancel("UI_unknown_paused_stale_or_invalid");return false;}
        if(!value_.anchor_expires_host_ns||now>=*value_.anchor_expires_host_ns) {cancel("original_anchor_horizon_expired");return false;}
        if(value_.state==OpeningTrialState::waiting&&now>due_[value_.tap_index]+lateness) {cancel("DOWN_dispatch_late");return false;}
        if(value_.state==OpeningTrialState::holding&&value_.up_due_host_ns&&now>*value_.up_due_host_ns+lateness) {
            cancel("UP_dispatch_late_cleanup_required");return false;
        }
        return true;
    }
    static bool bounded_text(const std::string& text,std::size_t maximum) {return !text.empty()&&text.size()<=maximum;}
    static bool same_ui(const OpeningUiEvidence& a,const OpeningUiEvidence& b) {
        return a.sequence==b.sequence&&a.received_host_ns==b.received_host_ns&&a.evidence_id==b.evidence_id&&
            a.source_id==b.source_id&&a.epoch==b.epoch&&a.generation==b.generation&&a.geometry_version==b.geometry_version&&
            a.playing==b.playing&&a.source_valid==b.source_valid;
    }
    OpeningTrialAction action(OpeningTrialPhase phase,SongNs scheduled) const {
        const auto& note=notes_[value_.tap_index];
        return {value_.tap_index,0,phase,scheduled,note.x,note.y};
    }
public:
    static constexpr std::array<OpeningTap,3> notes() {return notes_;}
    OpeningTrialStatus observe_ui(const OpeningUiEvidence& evidence,SongNs now) {
        if(!health(now))return value_;
        if(!evidence.sequence||!evidence.epoch||!evidence.generation||!evidence.geometry_version||
           evidence.received_host_ns<0||evidence.received_host_ns>now||now-evidence.received_host_ns>=age||
           !bounded_text(evidence.source_id,128)||!bounded_text(evidence.evidence_id,256)||!evidence.playing||!evidence.source_valid) {
            cancel("invalid_or_stale_actual_UI");return value_;
        }
        if(have_ui_) {
            if(evidence.source_id!=ui_.source_id||evidence.epoch!=ui_.epoch||evidence.generation!=ui_.generation||evidence.geometry_version!=ui_.geometry_version) {
                cancel("UI_source_epoch_generation_or_geometry_changed");return value_;
            }
            if(evidence.sequence==ui_.sequence&&same_ui(evidence,ui_)) {health(now);return value_;}
            if(evidence.sequence<=ui_.sequence||evidence.received_host_ns<=ui_.received_host_ns) {
                cancel("UI_sequence_or_receipt_not_increasing");return value_;
            }
        }
        if(seen_ui_ids_.size()>=1024||std::find(seen_ui_ids_.begin(),seen_ui_ids_.end(),evidence.evidence_id)!=seen_ui_ids_.end()) {
            cancel("UI_evidence_replay_or_budget");return value_;
        }
        seen_ui_ids_.push_back(evidence.evidence_id);ui_=evidence;have_ui_=true;value_.ui_sequence=evidence.sequence;
        health(now);return value_;
    }
    bool arm(const TrialStatus& first,SongNs original_armed_host_ns,SongNs now,const OpeningUiEvidence& evidence) {
        if(value_.state!=OpeningTrialState::unarmed) {cancel("duplicate_sequence_arm");return false;}
        observe_ui(evidence,now);if(terminal())return false;
        if(first.state!=FirstTapState::armed||!first.target_host_ns||!first.latest_origin_ns||first.support_count<3||
           first.requires_release||!first.experiment_only||first.live_qualified||first.provisional_origin_intervals.size()!=1||
           first.provisional_origin_intervals[0].lower< -safe||first.provisional_origin_intervals[0].upper>safe||
           first.provisional_origin_intervals[0].lower>first.provisional_origin_intervals[0].upper||
           first.provisional_origin_intervals[0].upper-first.provisional_origin_intervals[0].lower>80*ms||
           original_armed_host_ns<0||original_armed_host_ns>now||original_armed_host_ns>safe-horizon||
           *first.target_host_ns<0||*first.target_host_ns>safe-second||*first.latest_origin_ns< -safe||*first.latest_origin_ns>safe) {
            cancel("invalid_actual_first_tap_anchor");return false;
        }
        const auto phase=*first.target_host_ns-*first.latest_origin_ns-12*second;
        if(phase< -100*ms||phase>100*ms) {cancel("anchor_phase_out_of_bounds");return false;}
        for(std::size_t i=0;i<notes_.size();++i)due_[i]=*first.target_host_ns+(notes_[i].song_ns-notes_[0].song_ns);
        value_.anchor_expires_host_ns=original_armed_host_ns+horizon;
        if(due_[0]<=now||due_[2]+hold+lateness>=*value_.anchor_expires_host_ns) {
            cancel("sequence_not_future_or_outside_original_anchor_horizon");return false;
        }
        value_.state=OpeningTrialState::waiting;value_.reason="three_tap_experiment_armed";value_.down_due_host_ns=due_[0];
        return health(now);
    }
    OpeningTrialStatus status(SongNs now) {health(now);return value_;}
    std::optional<OpeningTrialAction> claim_down(SongNs now) {
        if(!health(now))return std::nullopt;
        if(value_.state!=OpeningTrialState::waiting) {cancel("DOWN_out_of_phase");return std::nullopt;}
        if(now<due_[value_.tap_index])return std::nullopt;
        if(value_.down_attempts>=3||ui_.sequence<=last_down_ui_sequence_) {cancel("DOWN_budget_or_fresh_UI_missing");return std::nullopt;}
        ++value_.down_attempts;value_.requires_release=true;last_down_ui_sequence_=ui_.sequence;down_claim_ns_=now;
        value_.state=OpeningTrialState::down_claimed;value_.reason="DOWN_claimed_effect_unknown";
        return action(OpeningTrialPhase::down,due_[value_.tap_index]);
    }
    void note_down_return(SongNs returned,bool success) {
        if(terminal())return;
        if(value_.state!=OpeningTrialState::down_claimed||returned<down_claim_ns_||!health(returned)) {cancel("invalid_DOWN_return");return;}
        if(!success) {cancel("DOWN_RPC_effect_unknown");return;}
        value_.up_due_host_ns=returned+hold;value_.state=OpeningTrialState::holding;value_.reason="UP_due_after_actual_DOWN_return";
    }
    std::optional<OpeningTrialAction> claim_up(SongNs now) {
        if(!health(now))return std::nullopt;
        if(value_.state!=OpeningTrialState::holding||!value_.up_due_host_ns) {cancel("UP_out_of_phase");return std::nullopt;}
        if(now<*value_.up_due_host_ns)return std::nullopt;
        up_claim_ns_=now;value_.state=OpeningTrialState::up_claimed;value_.reason="UP_claimed_effect_unknown";
        return action(OpeningTrialPhase::up,*value_.up_due_host_ns);
    }
    void note_up_return(SongNs returned,bool success) {
        if(terminal())return;
        if(value_.state!=OpeningTrialState::up_claimed||returned<up_claim_ns_||!health(returned)) {cancel("invalid_UP_return");return;}
        if(!success) {cancel("UP_RPC_effect_unknown");return;}
        ++value_.completed_taps;++value_.tap_index;value_.up_due_host_ns.reset();
        if(value_.tap_index==3) {value_.state=OpeningTrialState::completed;value_.reason="three_tap_RPC_sequence_completed_effect_unverified";value_.down_due_host_ns.reset();return;}
        value_.state=OpeningTrialState::waiting;value_.down_due_host_ns=due_[value_.tap_index];value_.reason="waiting_for_next_fixed_tap";
        health(returned); // A delayed UP never skips a missed next note.
    }
    void cancel(std::string reason) {
        if(terminal())return;
        value_.state=OpeningTrialState::cancelled;value_.requires_release=true;value_.reason=reason.substr(0,256);
        if(value_.reason.empty())value_.reason="cancelled";
    }
};
} // namespace rhythm::calibration
