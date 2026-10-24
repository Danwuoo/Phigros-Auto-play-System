#include "rhythm/supervisor.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rhythm {
namespace {
bool same_identity(const Identity& a,const Identity& b) {
    return a.game==b.game&&a.song_id==b.song_id&&a.difficulty==b.difficulty&&a.revision==b.revision&&
        a.chart_sha256==b.chart_sha256&&a.audio_sha256==b.audio_sha256&&a.layout_id==b.layout_id;
}
bool complete(const Identity& i) {
    return (i.game==Game::phigros||i.game==Game::project_sekai)&&!i.song_id.empty()&&!i.difficulty.empty()&&
        !i.revision.empty()&&!i.layout_id.empty()&&sha256_text(i.chart_sha256)&&sha256_text(i.audio_sha256);
}
bool valid_interval(Ns observed,Ns received,Ns expires,Ns now,Ns max_age) {
    return observed>=0&&received>=observed&&received<=now&&expires>observed&&
        static_cast<long double>(expires)-observed<=max_age;
}
Ns cap_add(Ns first,Ns delta) {
    return first>std::numeric_limits<Ns>::max()-delta ? std::numeric_limits<Ns>::max() : first+delta;
}
Ns uncertainty_at(const SupervisorSyncObservation& sync,Ns now,const SupervisorConfig& config) {
    const long double age=static_cast<long double>(now)-sync.lease.observed_host_ns;
    const long double value=std::ceil(sync.lease.uncertainty_ns+std::max(0.L,age)*config.max_clock_drift_ppm/1000000.L);
    if(!std::isfinite(value)||value>=static_cast<long double>(std::numeric_limits<Ns>::max()))
        return std::numeric_limits<Ns>::max();
    return static_cast<Ns>(value);
}
Ns uncertainty_expiry(const SupervisorSyncObservation& sync,const SupervisorConfig& config) {
    if(config.max_clock_drift_ppm==0)return std::numeric_limits<Ns>::max();
    const long double allowance=std::floor((static_cast<long double>(config.max_sync_uncertainty_ns)-sync.lease.uncertainty_ns)*1000000.L/config.max_clock_drift_ppm);
    if(allowance<=0)return sync.lease.observed_host_ns;
    if(allowance>=static_cast<long double>(std::numeric_limits<Ns>::max()))return std::numeric_limits<Ns>::max();
    return cap_add(sync.lease.observed_host_ns,static_cast<Ns>(allowance));
}
} // namespace

const char* supervisor_reason_name(SupervisorReason reason) {
    switch(reason) {
#define SUPERVISOR_REASON(name) case SupervisorReason::name:return #name
    SUPERVISOR_REASON(none);SUPERVISOR_REASON(missing_state);SUPERVISOR_REASON(missing_sync);
    SUPERVISOR_REASON(invalid_observation);SUPERVISOR_REASON(stale_state);SUPERVISOR_REASON(stale_sync);
    SUPERVISOR_REASON(not_playing);SUPERVISOR_REASON(identity_mismatch);SUPERVISOR_REASON(geometry_mismatch);
    SUPERVISOR_REASON(epoch_mismatch);SUPERVISOR_REASON(retired_epoch);SUPERVISOR_REASON(unqualified_plan);
    SUPERVISOR_REASON(unqualified_timestamp);SUPERVISOR_REASON(uncertainty);SUPERVISOR_REASON(clock_fault);
    SUPERVISOR_REASON(run_limit);SUPERVISOR_REASON(explicit_invalidation);SUPERVISOR_REASON(user_stop);
    SUPERVISOR_REASON(release_pending);SUPERVISOR_REASON(release_unknown);SUPERVISOR_REASON(release_failed);
    SUPERVISOR_REASON(invalid_release_ack);SUPERVISOR_REASON(sequence_exhausted);
#undef SUPERVISOR_REASON
    }
    return "invalid_reason";
}

RunSupervisor::RunSupervisor(PlanQualification qualification,SupervisorConfig config)
    :qualification_(std::move(qualification)),config_(config) {
    if(!complete(qualification_.identity)||!qualification_.geometry_version||!sha256_text(qualification_.compiled_cache_sha256))
        throw std::invalid_argument("Supervisor requires exact chart/audio/layout, geometry version, and compiled cache identity");
    if(qualification_.coverage==PlanCoverage::full_chart) {
        if(qualification_.segment_begin_ns||qualification_.segment_end_ns)
            throw std::invalid_argument("Full-chart qualification cannot conceal a segment selection");
    } else if(qualification_.coverage==PlanCoverage::explicit_segment) {
        if(!qualification_.segment_begin_ns||!qualification_.segment_end_ns||*qualification_.segment_begin_ns<0||
           *qualification_.segment_end_ns<=*qualification_.segment_begin_ns)
            throw std::invalid_argument("Segment qualification requires explicit nonnegative begin/end");
    } else throw std::invalid_argument("Unknown plan coverage");
    if(config_.mode!=SupervisorMode::shadow&&config_.mode!=SupervisorMode::fake_touch&&config_.mode!=SupervisorMode::live)
        throw std::invalid_argument("Unknown supervisor mode");
    for(auto budget:{config_.max_state_age_ns,config_.max_manual_age_ns,config_.max_sync_age_ns,
                     config_.max_sync_lease_ns,config_.max_run_duration_ns,config_.max_release_ack_age_ns})
        if(budget<=0||budget>600'000'000'000LL)throw std::invalid_argument("Supervisor positive time budgets must be bounded by 600 seconds");
    if(config_.max_state_uncertainty_ns<0||config_.max_sync_uncertainty_ns<0||
       config_.max_state_uncertainty_ns>600'000'000'000LL||config_.max_sync_uncertainty_ns>600'000'000'000LL||
       !std::isfinite(config_.max_clock_drift_ppm)||config_.max_clock_drift_ppm<0||config_.max_clock_drift_ppm>1000000)
        throw std::invalid_argument("Supervisor uncertainty/drift budget is invalid");
}

void RunSupervisor::request_user_stop() noexcept {user_stop_requested_.store(true,std::memory_order_release);}

void RunSupervisor::revoke(SupervisorReason reason,Ns now,std::uint64_t observed_epoch) {
    retired_epoch_=std::max({retired_epoch_,observed_epoch,epoch_});
    revoked_=true;reason_=reason;
    if(armed_) {
        armed_=false;
        if(config_.mode!=SupervisorMode::shadow&&!release_pending_) {
            release_pending_=true;
            if(revocation_id_==std::numeric_limits<std::uint64_t>::max()) {
                reason_=SupervisorReason::sequence_exhausted;retired_epoch_=std::numeric_limits<std::uint64_t>::max();
            } else ++revocation_id_;
            revoked_epoch_=epoch_;
            revoked_host_ns_=std::max({Ns{0},now,last_now_});
            release_reason_=SupervisorReason::release_pending;
        }
    }
}

bool RunSupervisor::check_now(Ns now) {
    if(now<0||(last_now_>=0&&now<last_now_)) {
        revoke(SupervisorReason::clock_fault,last_now_);return false;
    }
    last_now_=now;
    if(user_stop_requested_.load(std::memory_order_acquire)) {
        revoke(SupervisorReason::user_stop,now);return false;
    }
    return true;
}

bool RunSupervisor::next_sequence(Ns now) {
    if(sequence_==std::numeric_limits<std::uint64_t>::max()) {
        revoke(SupervisorReason::sequence_exhausted,now,std::numeric_limits<std::uint64_t>::max());return false;
    }
    ++sequence_;return true;
}

SupervisorReason RunSupervisor::health(Ns now) const {
    if(!qualification_.all_notes_in_scope_compiled)return SupervisorReason::unqualified_plan;
    if(config_.mode==SupervisorMode::live&&(qualification_.requires_calibration||!qualification_.timing_qualified||
        !qualification_.geometry_qualified||!qualification_.backend_qualified||qualification_.timing_evidence_id.empty()||
        qualification_.geometry_evidence_id.empty()||qualification_.backend_evidence_id.empty()))
        return SupervisorReason::unqualified_plan;
    if(!state_)return SupervisorReason::missing_state;
    if(!sync_)return SupervisorReason::missing_sync;
    const auto& s=*state_;const auto& a=*sync_;
    if(!same_identity(s.identity,qualification_.identity)||!same_identity(a.identity,qualification_.identity))return SupervisorReason::identity_mismatch;
    if(s.geometry_version!=qualification_.geometry_version||a.lease.geometry_version!=qualification_.geometry_version)return SupervisorReason::geometry_mismatch;
    if(s.epoch!=a.lease.epoch||!s.epoch)return SupervisorReason::epoch_mismatch;
    if(s.epoch<=retired_epoch_&&!armed_)return SupervisorReason::retired_epoch;
    if(armed_&&(s.epoch!=epoch_||a.lease.epoch!=epoch_))return SupervisorReason::epoch_mismatch;
    if(s.state!=ObservedGameState::playing)return SupervisorReason::not_playing;
    const Ns state_age=s.source==StateObservationSource::manual_confirmation ? config_.max_manual_age_ns : config_.max_state_age_ns;
    if(now<s.observed_host_ns||now>=s.expires_host_ns||static_cast<long double>(now)-s.observed_host_ns>=state_age)return SupervisorReason::stale_state;
    if(config_.mode==SupervisorMode::live&&s.timestamp_kind==StateTimestampKind::arrival_only)return SupervisorReason::unqualified_timestamp;
    if(s.timestamp_kind!=StateTimestampKind::arrival_only&&
       (s.timestamp_uncertainty_ns<0||s.timestamp_uncertainty_ns>config_.max_state_uncertainty_ns))return SupervisorReason::uncertainty;
    if(!a.lease.locked||now<a.lease.observed_host_ns||now>=a.lease.expires_host_ns||
       static_cast<long double>(now)-a.lease.observed_host_ns>=config_.max_sync_age_ns)return SupervisorReason::stale_sync;
    if(a.lease.uncertainty_ns<0||std::max(uncertainty_at(a,now,config_),s.timestamp_uncertainty_ns)>config_.max_sync_uncertainty_ns||
       now>=uncertainty_expiry(a,config_))return SupervisorReason::uncertainty;
    if(armed_&&now>=run_deadline_)return SupervisorReason::run_limit;
    return SupervisorReason::none;
}

SupervisorDecision RunSupervisor::decision(Ns now) const {
    SupervisorDecision result;result.release_required=release_pending_;result.revocation_id=revocation_id_;
    result.revoked_epoch=revoked_epoch_;result.release_reason=release_reason_;result.revocation_reason=reason_;
    if(state_) {
        result.manual_state=state_->source==StateObservationSource::manual_confirmation;
        result.capture_timestamp_known=state_->timestamp_kind==StateTimestampKind::mapped_capture;
    }
    if(user_stop_requested_.load(std::memory_order_acquire)) {
        result.state=SupervisorState::user_stopped;result.reason=SupervisorReason::user_stop;return result;
    }
    const auto why=health(now);
    if(release_pending_) {
        result.state=SupervisorState::revoked;result.reason=release_reason_;return result;
    }
    if(why!=SupervisorReason::none) {
        result.state=revoked_ ? SupervisorState::revoked : SupervisorState::waiting;
        result.reason=why;
        if(reason_==SupervisorReason::clock_fault||reason_==SupervisorReason::sequence_exhausted)result.reason=reason_;
        return result;
    }
    result.state=armed_ ? SupervisorState::armed : SupervisorState::ready;
    result.reason=SupervisorReason::none;
    if(!armed_)return result;
    result.dispatch_permitted=config_.mode!=SupervisorMode::shadow;
    result.live_permitted=config_.mode==SupervisorMode::live;
    SupervisorGrant grant;grant.mode=config_.mode;grant.identity=qualification_.identity;grant.clock=sync_->clock;
    grant.lease=sync_->lease;
    grant.lease.observed_host_ns=std::min(state_->observed_host_ns,sync_->lease.observed_host_ns);
    const auto state_age=result.manual_state ? config_.max_manual_age_ns : config_.max_state_age_ns;
    grant.lease.expires_host_ns=std::min({state_->expires_host_ns,sync_->lease.expires_host_ns,
        cap_add(state_->observed_host_ns,state_age),cap_add(sync_->lease.observed_host_ns,config_.max_sync_age_ns),
        uncertainty_expiry(*sync_,config_),run_deadline_});
    grant.lease.uncertainty_ns=std::max(uncertainty_at(*sync_,now,config_),state_->timestamp_uncertainty_ns);
    grant.sequence=sequence_;grant.state_sequence=state_->sequence;grant.sync_sequence=sync_->sequence;
    grant.compiled_cache_sha256=qualification_.compiled_cache_sha256;result.grant=std::move(grant);
    return result;
}

SupervisorDecision RunSupervisor::status(Ns now) {
    if(!check_now(now))return decision(std::max(last_now_,Ns{0}));
    const auto why=health(now);
    if(armed_&&why!=SupervisorReason::none)revoke(why,now);
    return decision(now);
}

SupervisorDecision RunSupervisor::observe_state(StateObservation observation,Ns now) {
    if(!check_now(now))return decision(std::max(last_now_,Ns{0}));
    const auto previous_health=health(now);
    if(armed_&&previous_health!=SupervisorReason::none)revoke(previous_health,now);
    const auto source=observation.source;
    const bool manual=source==StateObservationSource::manual_confirmation;
    const auto max_age=manual ? config_.max_manual_age_ns : config_.max_state_age_ns;
    const bool known_state=observation.state==ObservedGameState::playing||observation.state==ObservedGameState::paused||
        observation.state==ObservedGameState::result||observation.state==ObservedGameState::unknown;
    const bool kind_ok=manual ? observation.timestamp_kind==StateTimestampKind::host_confirmation :
        (observation.timestamp_kind==StateTimestampKind::mapped_capture||observation.timestamp_kind==StateTimestampKind::arrival_only);
    if(!known_state||(source!=StateObservationSource::visual&&!manual)||!kind_ok||
       (manual&&!config_.allow_manual_confirmation)||!observation.sequence||observation.sequence<=last_state_sequence_||
       observation.observed_host_ns<=last_state_observed_||!observation.epoch||!observation.geometry_version||observation.evidence_id.empty()||
       !valid_interval(observation.observed_host_ns,observation.received_host_ns,observation.expires_host_ns,now,max_age)||
       (observation.timestamp_kind==StateTimestampKind::arrival_only&&observation.observed_host_ns!=observation.received_host_ns)||
       (observation.timestamp_kind!=StateTimestampKind::arrival_only&&observation.timestamp_uncertainty_ns<0)) {
        const auto epoch=observation.epoch;state_.reset();revoke(SupervisorReason::invalid_observation,now,epoch);return decision(now);
    }
    last_state_sequence_=observation.sequence;last_state_observed_=observation.observed_host_ns;
    const auto epoch=observation.epoch;state_=std::move(observation);
    if(!next_sequence(now))return decision(now);
    SupervisorReason why=SupervisorReason::none;
    if(!same_identity(state_->identity,qualification_.identity))why=SupervisorReason::identity_mismatch;
    else if(state_->geometry_version!=qualification_.geometry_version)why=SupervisorReason::geometry_mismatch;
    else if(state_->state!=ObservedGameState::playing)why=SupervisorReason::not_playing;
    else if(armed_&&epoch!=epoch_)why=SupervisorReason::epoch_mismatch;
    else if(now>=state_->expires_host_ns||static_cast<long double>(now)-state_->observed_host_ns>=max_age)why=SupervisorReason::stale_state;
    if(why!=SupervisorReason::none)revoke(why,now,epoch);
    return status(now);
}

SupervisorDecision RunSupervisor::observe_sync(SupervisorSyncObservation observation,Ns now) {
    if(!check_now(now))return decision(std::max(last_now_,Ns{0}));
    const auto previous_health=health(now);
    if(armed_&&previous_health!=SupervisorReason::none)revoke(previous_health,now);
    const auto& lease=observation.lease;
    if(!observation.sequence||observation.sequence<=last_sync_sequence_||lease.observed_host_ns<=last_sync_observed_||
       !lease.epoch||!lease.geometry_version||observation.evidence_id.empty()||lease.uncertainty_ns<0||
       !valid_interval(lease.observed_host_ns,observation.received_host_ns,lease.expires_host_ns,now,config_.max_sync_lease_ns)||
       observation.clock.host_anchor_ns<0||!std::isfinite(observation.clock.host_per_song)||observation.clock.host_per_song<=0) {
        const auto epoch=lease.epoch;sync_.reset();revoke(SupervisorReason::invalid_observation,now,epoch);return decision(now);
    }
    last_sync_sequence_=observation.sequence;last_sync_observed_=lease.observed_host_ns;
    const auto epoch=lease.epoch;sync_=std::move(observation);
    if(!next_sequence(now))return decision(now);
    SupervisorReason why=SupervisorReason::none;
    if(!same_identity(sync_->identity,qualification_.identity))why=SupervisorReason::identity_mismatch;
    else if(sync_->lease.geometry_version!=qualification_.geometry_version)why=SupervisorReason::geometry_mismatch;
    else if(armed_&&epoch!=epoch_)why=SupervisorReason::epoch_mismatch;
    else if(!sync_->lease.locked||now>=sync_->lease.expires_host_ns||
        static_cast<long double>(now)-sync_->lease.observed_host_ns>=config_.max_sync_age_ns)why=SupervisorReason::stale_sync;
    if(why!=SupervisorReason::none)revoke(why,now,epoch);
    return status(now);
}

SupervisorDecision RunSupervisor::arm(std::uint64_t epoch,Ns now) {
    const auto current=status(now);
    if(user_stop_requested_.load(std::memory_order_acquire)||release_pending_||current.reason==SupervisorReason::clock_fault||
       current.reason==SupervisorReason::sequence_exhausted)return current;
    if(armed_) {
        if(epoch!=epoch_) {revoke(SupervisorReason::epoch_mismatch,now);return decision(now);}
        return current; // Repeated matching arm does not renew any deadline.
    }
    if(!epoch||epoch<=retired_epoch_||!state_||!sync_||epoch!=state_->epoch||epoch!=sync_->lease.epoch) {
        auto result=current;result.reason=epoch<=retired_epoch_ ? SupervisorReason::retired_epoch : SupervisorReason::epoch_mismatch;return result;
    }
    if(current.reason!=SupervisorReason::none)return current;
    epoch_=epoch;armed_=true;revoked_=false;reason_=SupervisorReason::none;
    run_deadline_=cap_add(now,config_.max_run_duration_ns);
    return decision(now);
}

SupervisorDecision RunSupervisor::invalidate(Ns now) {
    (void)check_now(now);
    const auto observed=std::max(state_ ? state_->epoch : 0,sync_ ? sync_->lease.epoch : 0);
    revoke(SupervisorReason::explicit_invalidation,std::max(last_now_,Ns{0}),observed);
    state_.reset();sync_.reset();return decision(std::max(last_now_,Ns{0}));
}

SupervisorDecision RunSupervisor::acknowledge_release(const ReleaseAcknowledgement& acknowledgement,Ns now) {
    // Stop is sticky, but its release acknowledgement must remain possible.
    if(now<0||(last_now_>=0&&now<last_now_)) {revoke(SupervisorReason::clock_fault,last_now_);return decision(std::max(last_now_,Ns{0}));}
    last_now_=now;
    if(user_stop_requested_.load(std::memory_order_acquire))revoke(SupervisorReason::user_stop,now);
    const auto previous_health=health(now);
    if(armed_&&previous_health!=SupervisorReason::none)revoke(previous_health,now);
    if(!release_pending_||!acknowledgement.revocation_id||acknowledgement.revocation_id!=revocation_id_||
       acknowledgement.epoch!=revoked_epoch_||acknowledgement.evidence_id.empty()||acknowledgement.observed_host_ns<revoked_host_ns_||
       acknowledgement.received_host_ns<acknowledgement.observed_host_ns||acknowledgement.received_host_ns>now||
       static_cast<long double>(now)-acknowledgement.observed_host_ns>=config_.max_release_ack_age_ns) {
        auto result=decision(now);result.release_reason=SupervisorReason::invalid_release_ack;return result;
    }
    if(acknowledgement.knowledge==ReleaseKnowledge::known_released) {release_pending_=false;release_reason_=SupervisorReason::none;}
    else if(acknowledgement.knowledge==ReleaseKnowledge::failed)release_reason_=SupervisorReason::release_failed;
    else if(acknowledgement.knowledge==ReleaseKnowledge::unknown)release_reason_=SupervisorReason::release_unknown;
    else {auto result=decision(now);result.release_reason=SupervisorReason::invalid_release_ack;return result;}
    return decision(now);
}
} // namespace rhythm
