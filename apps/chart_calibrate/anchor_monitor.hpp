#pragma once
#include "rhythm/visual_candidates.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rhythm::calibration {
enum class AnchorMonitorState { monitoring, cancelled };
struct AnchorAliasResidual {
    VisualTimeInterval song_interval;
    // Native candidate minus receipt-based prediction. Positive means the
    // pixels imply later song time; this does not identify capture/input delay.
    VisualTimeInterval residual_interval;
    SongNs signed_nearest_residual_ns=0;
};
struct AnchorMonitorStatus {
    AnchorMonitorState state=AnchorMonitorState::monitoring;
    std::string reason="awaiting_actual_anchor";
    SongNs fixed_origin_ns=0,original_arm_ns=0;
    std::optional<SongNs> predicted_song_ns,last_actual_receipt_ns;
    std::optional<VisualTimeInterval> matched_song_interval;
    std::vector<AnchorAliasResidual> aliases;
    std::size_t matching_alias_count=0,observation_count=0;
    std::string last_evidence_id;
    bool experiment_only=true,live_qualified=false,timing_qualified=false;
};

// Read-only, pure consistency gate for the first-Tap opening experiment. The
// fixed origin/rate=1 and 60ms cutoff are hypotheses, never a SyncLease or a
// measured timing bound. All aliases are checked; no target/phase is adjusted.
// Arrival freshness is not pixel age. Same pixels may be received again, but
// their candidates must still agree with the unchanged origin. The owner must
// separately enforce UI/foreground/STOP health and use this only before DOWN.
class AnchorMonitor {
    using I=VisualTimeInterval;
    static constexpr SongNs ms=1'000'000,second=1'000'000'000;
    static constexpr SongNs age_limit=100*ms,horizon=3*second;
    static constexpr SongNs residual_limit=60*ms,width_limit=80*ms,end=12*second;
    static constexpr SongNs safe=std::numeric_limits<SongNs>::max()/4;
    static constexpr std::size_t max_aliases=64,max_candidates=1024,max_observations=256;
    AnchorMonitorStatus value_;
    SongNs last_now_=-1;
    std::uint64_t sequence_=0;
    std::string context_;
    std::vector<std::pair<std::string,std::string>> evidence_;

    void cancel(const char* reason) {
        if(value_.state==AnchorMonitorState::cancelled)return;
        value_.state=AnchorMonitorState::cancelled;value_.reason=reason;
        value_.matched_song_interval.reset();
    }
    static bool text(const std::string& s,std::size_t max=512) {return !s.empty()&&s.size()<=max;}
    static bool sha(const std::string& s) {
        return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
    }
    static bool valid(I i) {return i.lower>=0&&i.lower<=i.upper&&i.upper<end;}
    static std::vector<I> merged(std::vector<I> in) {
        std::sort(in.begin(),in.end(),[](I a,I b){return a.lower<b.lower||(a.lower==b.lower&&a.upper<b.upper);});
        std::vector<I> out;
        for(auto i:in) {
            if(!out.empty()&&i.lower<=out.back().upper)out.back().upper=std::max(out.back().upper,i.upper);
            else out.push_back(i);
        }
        return out;
    }
    static bool same(const std::vector<I>& a,const std::vector<I>& b) {
        return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](I x,I y){return x.lower==y.lower&&x.upper==y.upper;});
    }
    static std::string context(const VisualCandidateResult& r) {
        const auto& f=r.observation;
        std::string result;
        const auto append=[&](const std::string& s){result+=std::to_string(s.size())+":"+s;};
        for(const auto* s:{&f.source_id,&f.source_sha256,&f.clock_id,&f.opening_context_id,
                          &r.model_id,&r.model_sha256,&r.calibration_id,&r.calibration_sha256})append(*s);
        result+=","+std::to_string(f.epoch)+","+std::to_string(f.generation)+","+std::to_string(f.geometry_version);
        return result;
    }
    static bool provenance(const VisualCandidateResult& r) {
        const auto& f=r.observation;
        if(!f.sequence||!f.epoch||!f.generation||!f.geometry_version||!f.source_valid||
           f.width!=1280||f.height!=720||f.clock_kind!=VisualSourceClock::arrival_only||
           f.source_time_ns||f.capture_host_interval||!text(f.source_id)||!sha(f.source_sha256)||
           !text(f.clock_id)||!text(f.opening_context_id)||!text(f.frame_id)||!text(f.evidence_id)||
           !sha(f.frame_sha256)||!text(r.model_id)||!sha(r.model_sha256)||
           !text(r.calibration_id)||!sha(r.calibration_sha256)||!f.centers_complete_unoccluded||
           f.centers.empty()||f.centers.size()>8)return false;
        for(std::size_t i=0;i<f.centers.size();++i) {
            const auto& c=f.centers[i];
            if(!text(c.id,128)||!std::isfinite(c.x)||!std::isfinite(c.y)||c.x<0||c.x>=1280||c.y<30||c.y>=570)return false;
            for(std::size_t j=0;j<i;++j)if(c.id==f.centers[j].id)return false;
        }
        return true;
    }
    bool health(SongNs now) {
        if(value_.state==AnchorMonitorState::cancelled)return false;
        if(now<0||now>safe||(last_now_>=0&&now<last_now_)) {cancel("host_clock_invalid_or_rollback");return false;}
        last_now_=now;
        if(now-value_.original_arm_ns>=horizon) {cancel("original_arm_horizon_expired");return false;}
        // This opening model cannot monitor through or beyond first judgment.
        const auto predicted_now=now-value_.fixed_origin_ns;
        if(predicted_now<0||predicted_now>=end) {cancel("outside_pre_first_judgment_scope");return false;}
        // Check the old observation before accepting a new one. A fresh reply
        // after an unobserved expiry gap cannot revive this one-shot gate.
        if(value_.last_actual_receipt_ns&&now-*value_.last_actual_receipt_ns>=age_limit) {
            cancel("previous_actual_receipt_expired");return false;
        }
        return true;
    }
    void consume(const VisualCandidateResult& r,SongNs now) {
        const auto& f=r.observation;
        if(!provenance(r)) {cancel("invalid_actual_observation_provenance");return;}
        if(f.received_host_ns<0||f.received_host_ns>now||now-f.received_host_ns>=age_limit) {
            cancel("invalid_or_stale_actual_receipt");return;
        }
        const auto identity=context(r);
        if(!evidence_.empty()&&identity!=context_) {cancel("source_epoch_geometry_or_model_changed");return;}
        if(f.sequence<=sequence_||(value_.last_actual_receipt_ns&&f.received_host_ns<=*value_.last_actual_receipt_ns)) {
            cancel("non_increasing_actual_observation");return;
        }
        for(const auto& old:evidence_)if(old.first==f.frame_id||old.second==f.evidence_id) {
            cancel("replayed_actual_observation");return;
        }
        if(evidence_.size()>=max_observations) {cancel("actual_observation_budget");return;}
        if(r.requires_new_epoch||r.timing_qualified||r.live_permitted||
           (r.state!=VisualCandidateState::conditional_candidate&&r.state!=VisualCandidateState::ambiguous)||
           r.song_intervals.empty()||r.song_intervals.size()>max_aliases||r.candidates.empty()||
           r.candidates.size()>max_candidates||!r.conditional_source_minus_song_intervals.empty()||
           r.supporting_evidence_ids!=std::vector<std::string>{f.evidence_id}) {
            cancel("invalid_or_incomplete_actual_candidates");return;
        }
        for(auto i:r.song_intervals)if(!valid(i)) {cancel("invalid_or_out_of_scope_song_interval");return;}
        std::vector<I> all;
        all.reserve(r.candidates.size());
        for(const auto& c:r.candidates) {
            if(!valid(c.song_time)) {cancel("invalid_or_out_of_scope_candidate_interval");return;}
            all.push_back(c.song_time);
        }
        // Producer song_intervals is the canonical union of every assignment.
        // Reject a missing/duplicated/unsorted alias list rather than sanitizing
        // it here and accidentally hiding ambiguity supplied by the producer.
        if(!same(merged(std::move(all)),r.song_intervals)||!same(merged(r.song_intervals),r.song_intervals)||
           (r.state==VisualCandidateState::conditional_candidate)!=(r.song_intervals.size()==1)) {
            cancel("candidate_alias_union_mismatch");return;
        }
        const auto predicted=f.received_host_ns-value_.fixed_origin_ns;
        if(predicted<0||predicted>=end) {cancel("outside_pre_first_judgment_scope");return;}
        context_=identity;sequence_=f.sequence;evidence_.emplace_back(f.frame_id,f.evidence_id);
        value_.observation_count=evidence_.size();value_.last_actual_receipt_ns=f.received_host_ns;
        value_.last_evidence_id=f.evidence_id;value_.predicted_song_ns=predicted;
        value_.matching_alias_count=0;value_.aliases.clear();value_.matched_song_interval.reset();
        std::optional<I> match;
        for(auto i:r.song_intervals) {
            const I residual{i.lower-predicted,i.upper-predicted};
            const auto nearest=residual.lower>0?residual.lower:residual.upper<0?residual.upper:0;
            value_.aliases.push_back({i,residual,nearest});
            if(nearest>=-residual_limit&&nearest<=residual_limit) {++value_.matching_alias_count;match=i;}
        }
        if(!value_.matching_alias_count) {cancel("no_alias_consistent_with_fixed_origin");return;}
        if(value_.matching_alias_count!=1) {cancel("multiple_aliases_consistent_with_fixed_origin");return;}
        if(match->upper-match->lower>width_limit) {cancel("consistent_alias_too_wide");return;}
        value_.matched_song_interval=match;value_.reason="experimental_fixed_origin_consistent";
    }
public:
    AnchorMonitor(const VisualCandidateResult& actual_anchor,SongNs fixed_origin_ns,SongNs original_arm_ns) {
        value_.fixed_origin_ns=fixed_origin_ns;value_.original_arm_ns=original_arm_ns;
        if(fixed_origin_ns< -safe||fixed_origin_ns>safe||original_arm_ns<0||original_arm_ns>safe) {
            cancel("invalid_fixed_origin_or_original_arm");return;
        }
        if(health(original_arm_ns))consume(actual_anchor,original_arm_ns);
    }
    AnchorMonitorStatus observe(const VisualCandidateResult& result,SongNs now) {
        if(health(now))consume(result,now);
        return value_;
    }
    AnchorMonitorStatus status(SongNs now) {health(now);return value_;}
};
} // namespace rhythm::calibration
