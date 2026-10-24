#pragma once
#include "rhythm/visual_candidates.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace rhythm::calibration {
enum class FirstTapState { acquiring, ready, armed, down_claimed, holding, up_claimed, completed, cancelled };
struct TrialStatus {
    FirstTapState state=FirstTapState::acquiring;
    std::string reason="waiting_for_provisional_origin";
    unsigned support_count=0;
    std::optional<SongNs> target_host_ns,up_due_host_ns,latest_origin_ns;
    bool requires_release=false,experiment_only=true,live_qualified=false;
    std::vector<VisualTimeInterval> provisional_origin_intervals;
    // The exact latest interval and three actual evidence IDs used by the
    // successful temporal-resolution step; downstream trials must not pick
    // their own alias from the raw candidate array.
    std::optional<VisualTimeInterval> resolved_song_interval;
    std::vector<std::string> supporting_evidence_ids;
};

// Pure, one-shot experimental command contract. Arrival-rate=1 and +/-30ms are
// explicit hypotheses, not capture-time bounds or a SyncLease. All aliases are
// retained until temporal intersection resolves them; the target gate never
// selects one alias. The owner supplies actual fresh UI health on every loop.
// After arm the fixed anchor lasts <=3s; the old candidate's 100ms age is NOT
// renewed/rechecked. UI freshness remains the owner's separate boolean contract.
class FirstTapTrial {
    using I=VisualTimeInterval;
    static constexpr SongNs ms=1'000'000,second=1'000'000'000;
    static constexpr SongNs safe=std::numeric_limits<SongNs>::max()/4;
    static constexpr SongNs allowance=30*ms,age_limit=100*ms,horizon=3*second;
    struct Support {
        SongNs received=0;
        std::string hash,frame_id,evidence_id;
        std::vector<I> intervals;
    };
    TrialStatus value_;
    SongNs phase_=0,last_now_=-1,last_received_=-1,armed_until_=-1,claimed_down_=-1;
    std::uint64_t sequence_=0;
    bool have_context_=false,last_ui_healthy_=false;
    std::string context_;
    std::vector<Support> history_;

    static bool valid(I a) {return a.lower<=a.upper;}
    static I overlap(I a,I b) {return {std::max(a.lower,b.lower),std::min(a.upper,b.upper)};}
    static SongNs mid(I a) {return a.lower+(a.upper-a.lower)/2;}
    static bool sha(const std::string& s) {
        return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
    }
    static bool text(const std::string& s) {return !s.empty()&&s.size()<=512;}
    static std::vector<I> merge(std::vector<I> in) {
        std::sort(in.begin(),in.end(),[](I a,I b){return a.lower<b.lower||(a.lower==b.lower&&a.upper<b.upper);});
        std::vector<I> out;
        for(auto i:in)if(valid(i)) {
            if(!out.empty()&&i.lower<=out.back().upper+1)out.back().upper=std::max(out.back().upper,i.upper);
            else out.push_back(i);
        }
        return out;
    }
    void reset(const char* reason) {
        history_.clear();value_.provisional_origin_intervals.clear();value_.support_count=0;
        value_.latest_origin_ns.reset();value_.state=FirstTapState::acquiring;value_.reason=reason;
        value_.resolved_song_interval.reset();value_.supporting_evidence_ids.clear();
    }
    bool terminal() const {return value_.state==FirstTapState::cancelled||value_.state==FirstTapState::completed;}
    bool anchored() const {
        return value_.state==FirstTapState::armed||value_.state==FirstTapState::down_claimed||
            value_.state==FirstTapState::holding||value_.state==FirstTapState::up_claimed;
    }
    bool health(SongNs now,bool healthy) {
        if(terminal())return false;
        if(now<0||now>safe||(last_now_>=0&&now<last_now_)) {cancel("host_clock_invalid_or_rollback");return false;}
        last_now_=now;last_ui_healthy_=healthy;
        if(!healthy) {
            if(anchored())cancel("UI_unknown_paused_stale_or_bad_geometry");
            else reset("waiting_for_healthy_UI");
            return false;
        }
        if(anchored()) {
            if(now>=armed_until_) {cancel("experimental_anchor_horizon_expired");return false;}
        }else if(last_received_>=0&&now-last_received_>=age_limit) {
            // Historical observations constrain a <=3s hypothesis, not a lease.
            // An old latest sample cannot arm, but a new actual sample may use
            // that history again. UI-unknown resets it separately above.
            value_.state=FirstTapState::acquiring;value_.reason="candidate_receipt_expired";
            value_.latest_origin_ns.reset();value_.support_count=0;
            value_.resolved_song_interval.reset();value_.supporting_evidence_ids.clear();
        }
        return true;
    }
    static std::string context(const VisualCandidateResult& r) {
        const auto& f=r.observation;
        // Lengths are bounded before this serialization; separators are not an
        // identity signature. Exact component lengths avoid delimiter aliases.
        std::string result;
        const auto append=[&](const std::string& s){result+=std::to_string(s.size())+":"+s;};
        for(const auto* s:{&f.source_id,&f.source_sha256,&f.clock_id,&f.opening_context_id,
                          &r.model_id,&r.model_sha256,&r.calibration_id,&r.calibration_sha256})append(*s);
        result+=","+std::to_string(f.epoch)+","+std::to_string(f.generation)+","+std::to_string(f.geometry_version);
        return result;
    }
    static bool provenance(const VisualCandidateResult& r) {
        const auto& f=r.observation;
        return f.sequence&&f.epoch&&f.generation&&f.geometry_version&&f.source_valid&&f.width==1280&&f.height==720&&
            f.clock_kind==VisualSourceClock::arrival_only&&!f.source_time_ns&&!f.capture_host_interval&&
            text(f.source_id)&&sha(f.source_sha256)&&text(f.clock_id)&&text(f.opening_context_id)&&
            text(f.frame_id)&&text(f.evidence_id)&&sha(f.frame_sha256)&&text(r.model_id)&&sha(r.model_sha256)&&
            text(r.calibration_id)&&sha(r.calibration_sha256);
    }
    static std::vector<I> projected(const Support& f,I origin) {
        const I possible{f.received-origin.upper-allowance,f.received-origin.lower+allowance};
        std::vector<I> out;
        for(auto i:f.intervals) {const auto a=overlap(i,possible);if(valid(a))out.push_back(a);}
        return merge(std::move(out));
    }
public:
    explicit FirstTapTrial(SongNs phase_ns):phase_(phase_ns) {
        if(phase_ns< -100*ms||phase_ns>100*ms)throw std::invalid_argument("experimental phase must be +/-100ms");
    }
    TrialStatus observe(const VisualCandidateResult& r,bool ui_healthy,SongNs now) {
        if(!health(now,ui_healthy))return value_;
        const auto& f=r.observation;
        if(!provenance(r)||f.received_host_ns<0||f.received_host_ns>now||now-f.received_host_ns>=age_limit) {
            if(anchored())cancel("invalid_or_stale_actual_observation");else reset("invalid_or_stale_actual_observation");
            return value_;
        }
        const auto identity=context(r);
        if(have_context_&&identity!=context_) {cancel("source_epoch_geometry_or_model_changed");return value_;}
        if(f.sequence<=sequence_||(last_received_>=0&&f.received_host_ns<=last_received_)) {
            if(anchored())cancel("non_distinct_actual_observation");else reset("non_distinct_actual_observation");
            return value_;
        }
        have_context_=true;context_=identity;sequence_=f.sequence;last_received_=f.received_host_ns;
        if(anchored())return value_; // Never move/renew an armed target from another anchor.
        if(r.requires_new_epoch||r.song_intervals.empty()||r.candidates.empty()||r.song_intervals.size()>64) {
            reset("no_usable_provisional_candidates");return value_;
        }
        for(auto i:r.song_intervals)if(!valid(i)||i.lower<0||i.upper>60*second) {
            reset("invalid_song_interval");return value_;
        }
        if(!history_.empty()&&f.received_host_ns-history_.front().received>horizon)reset("provisional_window_reset");
        if(history_.size()>=128) {cancel("provisional_observation_budget");return value_;}
        for(const auto& old:history_)if(old.frame_id==f.frame_id||old.evidence_id==f.evidence_id) {
            reset("replayed_actual_observation");return value_;
        }
        std::vector<I> proposed;
        for(auto i:r.song_intervals)proposed.push_back({f.received_host_ns-i.upper-allowance,f.received_host_ns-i.lower+allowance});
        proposed=merge(std::move(proposed));
        if(!history_.empty()) {
            std::vector<I> next;
            for(auto a:value_.provisional_origin_intervals)for(auto b:proposed) {
                auto i=overlap(a,b);if(valid(i))next.push_back(i);
                if(next.size()>4096) {cancel("provisional_branch_work_budget");return value_;}
            }
            proposed=merge(std::move(next));
        }
        if(proposed.empty()) {reset("inconsistent_provisional_origin");return value_;}
        if(proposed.size()>64) {cancel("provisional_branch_budget");return value_;}
        history_.push_back({f.received_host_ns,f.frame_sha256,f.frame_id,f.evidence_id,merge(r.song_intervals)});
        value_.provisional_origin_intervals=std::move(proposed);
        value_.state=FirstTapState::acquiring;value_.support_count=static_cast<unsigned>(std::min<std::size_t>(3,history_.size()));
        value_.reason="ambiguous_provisional_origin";value_.latest_origin_ns.reset();
        value_.resolved_song_interval.reset();value_.supporting_evidence_ids.clear();
        if(value_.provisional_origin_intervals.size()!=1||value_.provisional_origin_intervals[0].upper-
           value_.provisional_origin_intervals[0].lower>80*ms||history_.size()<3)return value_;
        const auto origin=value_.provisional_origin_intervals[0];
        const auto first=history_.size()-3;
        SongNs min_origin=safe,max_origin=-safe,latest_origin=0;
        std::vector<I> latest;
        for(std::size_t i=first;i<history_.size();++i) {
            for(std::size_t j=first;j<i;++j)if(history_[j].hash==history_[i].hash) {
                value_.reason="three_changing_pixel_frames_required";return value_;
            }
            const auto resolved=projected(history_[i],origin);
            if(resolved.size()!=1||resolved[0].upper-resolved[0].lower>80*ms) {
                value_.reason="resolved_support_still_ambiguous_or_wide";return value_;
            }
            const auto stamp=history_[i].received-mid(resolved[0]);
            min_origin=std::min(min_origin,stamp);max_origin=std::max(max_origin,stamp);latest_origin=stamp;
            if(i+1==history_.size())latest=resolved;
        }
        if(max_origin-min_origin>60*ms) {value_.reason="provisional_origin_spread_exceeds_60ms";return value_;}
        // This gate is applied ONLY after all aliases were resolved by temporal
        // evidence. It must never manufacture uniqueness by choosing this band.
        if(latest[0].lower<10*second+800*ms||latest[0].upper>11*second+500*ms) {
            value_.reason="resolved_anchor_not_in_arming_band";return value_;
        }
        value_.state=FirstTapState::ready;value_.reason="experimental_three_frame_anchor_ready";
        value_.latest_origin_ns=latest_origin;value_.resolved_song_interval=latest[0];
        for(std::size_t i=first;i<history_.size();++i)value_.supporting_evidence_ids.push_back(history_[i].evidence_id);
        return value_;
    }
    TrialStatus status(SongNs now,bool ui_healthy=true) {health(now,ui_healthy);return value_;}
    bool arm(SongNs now) {
        if(!health(now,last_ui_healthy_)||value_.state!=FirstTapState::ready||!value_.latest_origin_ns)return false;
        const auto target=*value_.latest_origin_ns+12*second+phase_;
        if(target<=now||target-now>horizon) {cancel("experimental_target_not_future_or_bounded");return false;}
        value_.target_host_ns=target;armed_until_=now+horizon;
        value_.state=FirstTapState::armed;value_.reason="one_tap_armed_experiment_only";return true;
    }
    bool claim_down(SongNs now) {
        if(!health(now,last_ui_healthy_)||value_.state!=FirstTapState::armed||!value_.target_host_ns||now<*value_.target_host_ns)return false;
        if(now-*value_.target_host_ns>20*ms) {cancel("DOWN_dispatch_late");return false;}
        value_.requires_release=true;claimed_down_=now;value_.state=FirstTapState::down_claimed;
        value_.reason="one_DOWN_claimed_effect_unknown";return true;
    }
    void note_down_return(SongNs returned,bool success=true) {
        if(terminal())return;
        if(value_.state!=FirstTapState::down_claimed||returned<claimed_down_||!health(returned,last_ui_healthy_)) {
            cancel("invalid_DOWN_return");return;
        }
        if(!success) {cancel("DOWN_RPC_effect_unknown");return;}
        value_.up_due_host_ns=returned+16*ms;value_.state=FirstTapState::holding;value_.reason="UP_due_after_actual_DOWN_return";
    }
    bool up_due(SongNs now) {
        if(!health(now,last_ui_healthy_)||value_.state!=FirstTapState::holding||!value_.up_due_host_ns||now<*value_.up_due_host_ns)return false;
        if(now-*value_.up_due_host_ns>20*ms) {cancel("UP_dispatch_late_cleanup_required");return false;}
        return true;
    }
    bool claim_up(SongNs now) {
        if(!up_due(now))return false;value_.state=FirstTapState::up_claimed;value_.reason="UP_claimed_effect_unknown";return true;
    }
    void note_up_return(SongNs returned,bool success=true) {
        if(terminal())return;
        if(value_.state!=FirstTapState::up_claimed||!health(returned,last_ui_healthy_)) {cancel("invalid_UP_return");return;}
        if(!success) {cancel("UP_RPC_effect_unknown");return;}
        value_.state=FirstTapState::completed;value_.reason="one_tap_RPC_sequence_completed_effect_unverified";
        // RPC success is not known-zero evidence. The owner must always perform
        // and verify cleanup; requires_release intentionally remains latched.
    }
    void cancel(std::string reason) {
        if(terminal())return;value_.state=FirstTapState::cancelled;value_.reason=reason.substr(0,512);
        if(value_.reason.empty())value_.reason="cancelled";
    }
};
} // namespace rhythm::calibration
