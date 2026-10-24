#include "rhythm/session_runner.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>

namespace rhythm {
namespace {
bool same_identity(const Identity& a,const Identity& b) {
    return a.game==b.game&&a.song_id==b.song_id&&a.difficulty==b.difficulty&&a.revision==b.revision&&
        a.chart_sha256==b.chart_sha256&&a.audio_sha256==b.audio_sha256&&a.layout_id==b.layout_id;
}
bool same_event(const CompiledSongEvent& a,const CompiledSongEvent& b) {
    return a.song_ns==b.song_ns&&a.contact_id==b.contact_id&&a.phase==b.phase&&
        a.point.x==b.point.x&&a.point.y==b.point.y;
}
PasBridgeConfig make_bridge_config(const CompiledContactPlan& plan) {
    PasBridgeConfig config;
    config.max_contacts=plan.metadata.options.max_contacts;
    config.input_width=plan.metadata.geometry.input_width;
    config.input_height=plan.metadata.geometry.input_height;
    return config;
}
CompiledContactPlan checked(CompiledContactPlan plan,const PlanQualification& qualification,SupervisorMode mode) {
    const auto report=preflight_session(plan,qualification,mode);
    if(!report.accepted) {
        std::string message="Session preflight rejected";
        for(const auto& reason:report.reasons) message+="; "+reason;
        throw std::invalid_argument(message);
    }
    return plan;
}
} // namespace

SupervisorConfig fake_session_config() {SupervisorConfig config;config.mode=SupervisorMode::fake_touch;return config;}
SessionPreflight preflight_session(const CompiledContactPlan& plan,const PlanQualification& q,SupervisorMode mode) {
    SessionPreflight result;
    const auto reject=[&](const char* reason){if(result.reasons.size()<32)result.reasons.emplace_back(reason);};
    if(mode!=SupervisorMode::fake_touch) reject("only_fake_touch_runner_available");
    if(!same_identity(plan.memory.identity,q.identity)) reject("plan_identity_mismatch");
    if(!sha256_text(plan.metadata.cache_sha256)||q.compiled_cache_sha256!=plan.metadata.cache_sha256)
        reject("compiled_cache_mismatch");
    if(plan.metadata.compiler_version.empty()) reject("missing_compiler_version");
    if(!q.geometry_version||!q.all_notes_in_scope_compiled) reject("incomplete_scope_qualification");
    if(plan.metadata.requires_calibration&&!q.requires_calibration) reject("calibration_metadata_downgrade");
    const auto policy=plan.metadata.options.offscreen_policy;
    const bool projected=policy==OffscreenPolicy::bounded_normal_candidate||
        policy==OffscreenPolicy::bounded_normal_intersection_candidate_v1;
    if(policy!=OffscreenPolicy::reject&&!projected) reject("unknown_projection_policy");
    if(plan.metadata.requires_calibration!=projected) reject("calibration_metadata_inconsistent");
    if(mode==SupervisorMode::live&&(q.requires_calibration||plan.metadata.requires_calibration||
       !q.timing_qualified||!q.geometry_qualified||!q.backend_qualified||q.timing_evidence_id.empty()||
       q.geometry_evidence_id.empty()||q.backend_evidence_id.empty())) reject("unqualified_live_plan");
    try {
        auto config=fake_session_config();config.mode=mode;
        RunSupervisor validation(q,config);
        (void)validation;
        PhigrosGeometryMap geometry(plan.metadata.geometry);
        (void)geometry;
        if(plan.metadata.options.max_contacts<1||plan.metadata.options.max_contacts>10||
           plan.memory.contacts.empty()||plan.memory.contacts.size()>200000||plan.ownership.size()>200000||
           plan.events.size()>2000000)
            throw std::invalid_argument("invalid bounded plan");
        if(validate(plan.memory,plan.metadata.options.max_contacts)!=plan.peak_contacts)
            reject("peak_contacts_mismatch");
        if(plan.ownership.size()!=plan.memory.contacts.size()) reject("ownership_incomplete");
        std::set<std::uint64_t> owners;std::set<std::string> sources;std::size_t projection_spans=0;
        for(const auto& owned:plan.ownership) {
            if(owned.normal_projection_spans.size()>2000000-projection_spans)
                throw std::invalid_argument("projection provenance budget");
            projection_spans+=owned.normal_projection_spans.size();
            if(owned.source_json_pointer.empty()||
               owned.source_note_id!=q.identity.chart_sha256+'#'+owned.source_json_pointer||
               !sources.insert(owned.source_note_id).second||!owners.insert(owned.contact_id).second)
                reject("invalid_note_ownership");
            if(q.coverage==PlanCoverage::explicit_segment&&q.segment_begin_ns&&q.segment_end_ns&&
               (owned.judgment_ns<*q.segment_begin_ns||owned.judgment_ns>=*q.segment_end_ns))
                reject("note_outside_segment");
        }
        std::vector<CompiledSongEvent> expected;
        for(const auto& contact:plan.memory.contacts) {
            if(!owners.contains(contact.id)) reject("contact_without_owner");
            if(contact.steps.size()>2000000-expected.size()) throw std::invalid_argument("step budget");
            for(const auto& step:contact.steps) {
                expected.push_back({step.song_ns,contact.id,step.phase,step.point});
                if(q.coverage==PlanCoverage::explicit_segment&&q.segment_begin_ns&&q.segment_end_ns&&
                   (step.song_ns<*q.segment_begin_ns||step.song_ns>=*q.segment_end_ns))
                    reject("contact_step_outside_segment");
            }
        }
        if(expected.size()!=plan.total_steps) reject("total_steps_mismatch");
        std::sort(expected.begin(),expected.end(),[](const auto& a,const auto& b){
            return std::tuple(a.song_ns,phase_order(a.phase),a.contact_id)<std::tuple(b.song_ns,phase_order(b.phase),b.contact_id);
        });
        if(expected.size()!=plan.events.size()||!std::equal(expected.begin(),expected.end(),plan.events.begin(),same_event))
            reject("events_memory_mismatch");
        if(compiled_contact_fingerprint(plan)!=plan.metadata.cache_sha256)
            reject("compiled_fingerprint_mismatch");
    } catch(const std::exception&) {reject("invalid_plan_or_qualification");}
    result.accepted=result.reasons.empty();return result;
}

SessionRunner::SessionRunner(CompiledContactPlan plan,PlanQualification qualification,const pas::Clock& clock,
        pas::FakeTouchBackend& backend,SupervisorConfig config)
    :plan_(checked(std::move(plan),qualification,config.mode)),qualification_(std::move(qualification)),
     clock_(clock),backend_(backend),bridge_config_(make_bridge_config(plan_)),supervisor_(qualification_,config),
     bridge_(plan_.memory,clock,backend,bridge_config_) {}

SessionStatus SessionRunner::observe_state(StateObservation observation) {
    (void)supervisor_.observe_state(std::move(observation),clock_.now_ns());return pump(false);
}
SessionStatus SessionRunner::observe_sync(SupervisorSyncObservation observation) {
    (void)supervisor_.observe_sync(std::move(observation),clock_.now_ns());return pump(false);
}
SessionStatus SessionRunner::arm(std::uint64_t epoch) {(void)supervisor_.arm(epoch,clock_.now_ns());return pump(false);}
SessionStatus SessionRunner::status() {return pump(false);}
SessionStatus SessionRunner::tick() {return pump(true);}
SessionStatus SessionRunner::invalidate() {(void)supervisor_.invalidate(clock_.now_ns());return pump(false);}
void SessionRunner::request_user_stop() {supervisor_.request_user_stop();bridge_.request_stop();}
std::optional<Ns> SessionRunner::next_wake_ns() const {return bridge_.next_wake_ns();}

SessionStatus SessionRunner::acknowledge_release(ReleaseAcknowledgement acknowledgement) {
    // Even the concrete FakeTouch simulation must actually be empty before
    // promoting caller-supplied knowledge. Real Android release has no adapter here.
    const auto release=bridge_.scheduler().last_release();
    if(acknowledgement.knowledge==ReleaseKnowledge::known_released&&
       (!backend_.contacts().empty()||!release.failed_ids.empty()||!release.unknown_ids.empty()))
        acknowledgement.knowledge=ReleaseKnowledge::unknown;
    (void)supervisor_.acknowledge_release(acknowledgement,clock_.now_ns());return pump(false);
}

SessionStatus SessionRunner::pump(bool dispatch) {
    SessionStatus result;auto decision=supervisor_.status(clock_.now_ns());
    const auto retire=[&](const std::string& reason) {
        if(active_||bridge_.running()) bridge_.stop(reason);
        active_=false;terminal_reason_=reason;
    };
    if(!decision.dispatch_permitted||!decision.grant) {
        if(active_||bridge_.running()) retire(supervisor_reason_name(decision.reason));
    } else {
        const auto& grant=*decision.grant;
        const ExecutionObservation observed{grant.identity,grant.clock,grant.lease,grant.sequence};
        if(grant.mode!=SupervisorMode::fake_touch||grant.compiled_cache_sha256!=plan_.metadata.cache_sha256) {
            retire("grant_mode_or_cache_mismatch");decision=supervisor_.invalidate(clock_.now_ns());
        } else if(!active_) {
            if(grant.lease.epoch<=bridge_epoch_||!bridge_.begin(observed)) {
                retire("bridge_begin_rejected");decision=supervisor_.invalidate(clock_.now_ns());
            } else {
                active_=true;finished_=false;terminal_reason_.clear();
                bridge_epoch_=grant.lease.epoch;forwarded_sequence_=grant.sequence;
            }
        } else if(grant.sequence!=forwarded_sequence_) {
            if(!bridge_.update(observed)) {
                retire(bridge_.reason());decision=supervisor_.invalidate(clock_.now_ns());
            } else forwarded_sequence_=grant.sequence;
        }
        if(dispatch&&active_) {
            result.dispatched=bridge_.tick();
            if(!result.dispatched->running) {
                active_=false;finished_=result.dispatched->finished;terminal_reason_=result.dispatched->reason;
                decision=supervisor_.invalidate(clock_.now_ns());
            }
        }
    }
    result.supervision=std::move(decision);result.running=bridge_.running();result.finished=finished_;
    result.forwarded_sequence=forwarded_sequence_;result.terminal_reason=terminal_reason_;
    result.release=bridge_.scheduler().last_release();return result;
}
} // namespace rhythm
