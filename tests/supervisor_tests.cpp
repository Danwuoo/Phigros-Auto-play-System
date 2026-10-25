#include "rhythm/supervisor.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
using namespace rhythm;
constexpr Ns ms=1000000;
int checks=0;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);++checks;}
template<class Function> void rejects(Function fn,const char* message) {bool failed=false;try{fn();}catch(const std::exception&){failed=true;}require(failed,message);}
void run(const char* name,const std::function<void()>& fn) {fn();std::cout<<"PASS "<<name<<'\n';}
Identity identity() {return {Game::phigros,"song","IN","v3",std::string(64,'a'),std::string(64,'b'),"1280x720"};}
PlanQualification qualification() {
    PlanQualification q;q.identity=identity();q.geometry_version=7;q.compiled_cache_sha256=std::string(64,'c');
    q.all_notes_in_scope_compiled=true;q.requires_calibration=false;
    q.timing_qualified=q.geometry_qualified=q.backend_qualified=true;
    q.timing_evidence_id="synthetic timing";q.geometry_evidence_id="synthetic geometry";q.backend_evidence_id="FakeTouch evidence";return q;
}
SupervisorConfig config(SupervisorMode mode=SupervisorMode::fake_touch) {SupervisorConfig c;c.mode=mode;return c;}
StateObservation state(Ns observed=0,std::uint64_t epoch=1,std::uint64_t sequence=1) {
    StateObservation s;s.state=ObservedGameState::playing;s.source=StateObservationSource::visual;s.timestamp_kind=StateTimestampKind::mapped_capture;
    s.identity=identity();s.sequence=sequence;s.epoch=epoch;s.geometry_version=7;s.evidence_id="actual-frame-"+std::to_string(sequence);
    s.observed_host_ns=observed;s.received_host_ns=observed+2*ms;s.expires_host_ns=observed+100*ms;s.timestamp_uncertainty_ns=ms;return s;
}
SupervisorSyncObservation sync_observation(Ns observed=0,std::uint64_t epoch=1,std::uint64_t sequence=1) {
    SupervisorSyncObservation a;a.identity=identity();a.clock={0,observed,1};a.lease={true,epoch,7,observed,observed+120*ms,ms};
    a.sequence=sequence;a.received_host_ns=observed+2*ms;a.evidence_id="actual-audio-"+std::to_string(sequence);return a;
}
SupervisorDecision feed(RunSupervisor& supervisor,Ns observed=0,std::uint64_t epoch=1,std::uint64_t sequence=1) {
    supervisor.observe_state(state(observed,epoch,sequence),observed+2*ms);
    return supervisor.observe_sync(sync_observation(observed,epoch,sequence),observed+2*ms);
}
ReleaseAcknowledgement ack(const SupervisorDecision& d,Ns now,ReleaseKnowledge knowledge=ReleaseKnowledge::known_released) {
    return {d.revocation_id,d.revoked_epoch,knowledge,now,now,"actual FakeTouch zero-active receipt"};
}
}
int main() {
    using namespace rhythm;
    try {
        run("explicit arm pure shadow and FakeTouch modes",[] {
            RunSupervisor supervisor(qualification(),config());
            require(!supervisor.status(0).dispatch_permitted,"missing evidence fails closed");
            const auto ready=feed(supervisor);
            require(ready.state==SupervisorState::ready&&!ready.dispatch_permitted&&!ready.grant,"evidence does not auto-arm");
            const auto armed=supervisor.arm(1,2*ms);
            require(armed.state==SupervisorState::armed&&armed.dispatch_permitted&&!armed.live_permitted,"FakeTouch permit distinguished from live");
            require(armed.grant&&armed.grant->mode==SupervisorMode::fake_touch&&armed.grant->lease.epoch==1,"explicit grant epoch/mode");
            require(armed.grant->identity.chart_sha256==identity().chart_sha256&&armed.grant->compiled_cache_sha256==std::string(64,'c'),"exact plan/source identity in grant");
            RunSupervisor shadow(qualification(),config(SupervisorMode::shadow));feed(shadow);const auto passive=shadow.arm(1,2*ms);
            require(passive.state==SupervisorState::armed&&!passive.dispatch_permitted&&!passive.live_permitted,"shadow never authorizes touch");
            const auto wrong_epoch=supervisor.arm(2,3*ms);
            require(wrong_epoch.release_required&&!wrong_epoch.dispatch_permitted,"arm of another epoch cannot silently return old permission");
        });
        run("status and repeated arm do not manufacture freshness or extend deadline",[] {
            auto c=config();c.max_run_duration_ns=50*ms;RunSupervisor supervisor(qualification(),c);feed(supervisor);
            const auto first=supervisor.arm(1,2*ms);const auto original=*first.grant;
            const auto status=supervisor.status(20*ms);const auto repeated=supervisor.arm(1,30*ms);
            require(status.grant->sequence==original.sequence&&repeated.grant->sequence==original.sequence,"status/arm create no observation sequence");
            require(status.grant->lease.observed_host_ns==0&&repeated.grant->lease.expires_host_ns==52*ms,"actual observed and fixed run cutoff retained");
            require(status.grant->lease.uncertainty_ns>original.lease.uncertainty_ns,"status may only age uncertainty conservatively");
            feed(supervisor,40*ms,1,2);
            const auto expired=supervisor.status(52*ms);
            require(!expired.dispatch_permitted&&expired.release_required&&expired.revocation_reason==SupervisorReason::run_limit,"new evidence does not extend bounded run");
        });
        run("playing paused result unknown revoke and retire old epoch",[] {
            for(auto phase:{ObservedGameState::paused,ObservedGameState::result,ObservedGameState::unknown}) {
                RunSupervisor supervisor(qualification(),config());feed(supervisor);supervisor.arm(1,2*ms);
                auto changed=state(10*ms,1,2);changed.state=phase;
                const auto stopped=supervisor.observe_state(changed,12*ms);
                require(stopped.release_required&&!stopped.dispatch_permitted&&stopped.revoked_epoch==1,"nonplaying state requests release");
                require(stopped.revocation_reason==SupervisorReason::not_playing,"nonplaying cause explicit");
                const auto released=supervisor.acknowledge_release(ack(stopped,13*ms),13*ms);
                require(!released.release_required,"matching known release clears latch");
                feed(supervisor,20*ms,1,3);
                require(!supervisor.arm(1,22*ms).dispatch_permitted,"same epoch cannot resume after pause/result/unknown");
                const auto ready=feed(supervisor,30*ms,2,4);
                require(ready.state==SupervisorState::ready&&!ready.dispatch_permitted,"new epoch still requires explicit arm");
                require(supervisor.arm(2,32*ms).dispatch_permitted,"fresh new epoch explicitly armed");
            }
        });
        run("release unknown failure wrong ids and epochs cannot be bypassed",[] {
            RunSupervisor supervisor(qualification(),config());feed(supervisor);supervisor.arm(1,2*ms);
            auto stopped=supervisor.invalidate(10*ms);
            require(stopped.release_required&&stopped.revocation_id!=0&&stopped.revoked_epoch==1,"revocation token emitted");
            const auto token=stopped.revocation_id;
            require(supervisor.acknowledge_release(ack(stopped,11*ms,ReleaseKnowledge::unknown),11*ms).release_required,"unknown release remains pending");
            const auto failed=supervisor.acknowledge_release(ack(stopped,12*ms,ReleaseKnowledge::failed),12*ms);
            require(failed.release_required&&failed.release_reason==SupervisorReason::release_failed,"failed release explicit");
            auto wrong=ack(stopped,13*ms);wrong.revocation_id++;
            require(supervisor.acknowledge_release(wrong,13*ms).release_required,"wrong acknowledgement id rejected");
            wrong=ack(stopped,14*ms);wrong.epoch++;
            require(supervisor.acknowledge_release(wrong,14*ms).release_required,"wrong acknowledgement epoch rejected");
            wrong=ack(stopped,15*ms);wrong.observed_host_ns=9*ms;
            require(supervisor.acknowledge_release(wrong,15*ms).release_required,"pre-revocation observation cannot prove release");
            wrong=ack(stopped,16*ms);wrong.received_host_ns=17*ms;
            require(supervisor.acknowledge_release(wrong,16*ms).release_required,"future release receipt rejected");
            feed(supervisor,20*ms,2,2);
            require(!supervisor.arm(2,22*ms).dispatch_permitted,"new epoch cannot bypass pending release");
            const auto done=supervisor.acknowledge_release(ack(stopped,23*ms),23*ms);
            require(!done.release_required&&done.revocation_id==token,"matching ack acknowledges original token");
            require(supervisor.arm(2,24*ms).dispatch_permitted,"known release and fresh epoch allow explicit arm");
            RunSupervisor stale(qualification(),config());feed(stale);stale.arm(1,2*ms);
            const auto pending=stale.invalidate(10*ms);
            require(stale.acknowledge_release(ack(pending,11*ms),170*ms).release_required,"old release evidence cannot acknowledge a current revocation indefinitely");
            RunSupervisor expired(qualification(),config());feed(expired);expired.arm(1,2*ms);
            ReleaseAcknowledgement unsolicited;
            require(expired.acknowledge_release(unsolicited,101*ms).release_required,"every owner entrypoint detects expired armed evidence");
        });
        run("state expiry boundary cannot be overwritten by fresh state",[] {
            RunSupervisor supervisor(qualification(),config());feed(supervisor);supervisor.arm(1,2*ms);
            const auto updated=supervisor.observe_state(state(101*ms,1,2),103*ms);
            require(updated.release_required&&!updated.dispatch_permitted&&updated.revocation_reason==SupervisorReason::stale_state,"old state gap revoked before replacement");
            require(!supervisor.arm(1,103*ms).dispatch_permitted,"new state cannot hide expired old state");
            RunSupervisor boundary(qualification(),config());feed(boundary);boundary.arm(1,2*ms);
            require(boundary.status(100*ms).release_required,"exact state expiry is already invalid");
        });
        run("sync expiry and uncertainty cannot be overwritten by new sync",[] {
            auto c=config();c.max_state_age_ns=200*ms;
            RunSupervisor supervisor(qualification(),c);auto s=state();s.expires_host_ns=180*ms;
            supervisor.observe_state(s,2*ms);supervisor.observe_sync(sync_observation(),2*ms);supervisor.arm(1,2*ms);
            const auto updated=supervisor.observe_sync(sync_observation(121*ms,1,2),123*ms);
            require(updated.release_required&&!updated.dispatch_permitted&&updated.revocation_reason==SupervisorReason::stale_sync,"old sync lease gap revoked before replacement");
            c=config();c.max_sync_uncertainty_ns=1'050'000;c.max_clock_drift_ppm=3000;
            RunSupervisor uncertainty(qualification(),c);feed(uncertainty);require(uncertainty.arm(1,2*ms).dispatch_permitted,"uncertainty initially acceptable");
            const auto grant=uncertainty.status(3*ms);
            require(grant.grant->lease.expires_host_ns<17*ms,"grant self-expires before drift exceeds budget without synthetic renewal");
            const auto renewed=uncertainty.observe_sync(sync_observation(20*ms,1,2),22*ms);
            require(renewed.release_required&&renewed.revocation_reason==SupervisorReason::uncertainty,"uncertainty gap revoked before replacing evidence");
        });
        run("actual sequences future samples arrivals and manual expiry are enforced",[] {
            for(int fault=0;fault<7;++fault) {
                RunSupervisor supervisor(qualification(),config());feed(supervisor);supervisor.arm(1,2*ms);
                auto s=state(10*ms,1,2);
                if(fault==0)s.sequence=1;
                if(fault==1)s.observed_host_ns=0;
                if(fault==2)s.received_host_ns=20*ms;
                if(fault==3)s.observed_host_ns=20*ms;
                if(fault==4)s.timestamp_kind=StateTimestampKind::unknown;
                if(fault==5)s.expires_host_ns=std::numeric_limits<Ns>::max();
                if(fault==6)s.evidence_id.clear();
                const auto invalid=supervisor.observe_state(s,12*ms);
                require(invalid.release_required&&!invalid.dispatch_permitted&&invalid.revocation_reason==SupervisorReason::invalid_observation,"invalid actual-state evidence fails closed");
            }
            RunSupervisor manual(qualification(),config());auto s=state();s.source=StateObservationSource::manual_confirmation;
            s.timestamp_kind=StateTimestampKind::host_confirmation;s.expires_host_ns=500*ms;
            manual.observe_state(s,2*ms);manual.observe_sync(sync_observation(),2*ms);
            const auto armed=manual.arm(1,2*ms);
            require(armed.dispatch_permitted&&armed.manual_state&&!armed.capture_timestamp_known,"manual evidence explicitly labeled");
            const auto old_sequence=armed.grant->sequence;
            manual.observe_sync(sync_observation(80*ms,1,2),82*ms);
            const auto aged=manual.status(90*ms);
            require(aged.grant->state_sequence==1&&aged.grant->lease.observed_host_ns==0&&aged.grant->sequence==old_sequence+1,"audio evidence does not renew manual timestamp");
            auto disabled=config();disabled.allow_manual_confirmation=false;
            RunSupervisor deny(qualification(),disabled);
            require(!deny.observe_state(s,2*ms).dispatch_permitted,"manual can be explicitly disabled");
            s.expires_host_ns=std::numeric_limits<Ns>::max();RunSupervisor infinite(qualification(),config());
            require(infinite.observe_state(s,2*ms).revocation_reason==SupervisorReason::invalid_observation,"manual infinite expiry rejected");
        });
        run("live requires full qualification and known timestamp kind",[] {
            for(int missing=0;missing<8;++missing) {
                auto q=qualification();
                if(missing==0)q.all_notes_in_scope_compiled=false;
                if(missing==1)q.requires_calibration=true;
                if(missing==2)q.timing_qualified=false;
                if(missing==3)q.geometry_qualified=false;
                if(missing==4)q.backend_qualified=false;
                if(missing==5)q.timing_evidence_id.clear();
                if(missing==6)q.geometry_evidence_id.clear();
                if(missing==7)q.backend_evidence_id.clear();
                RunSupervisor supervisor(q,config(SupervisorMode::live));feed(supervisor);
                require(!supervisor.arm(1,2*ms).live_permitted,"unqualified live run refused");
            }
            RunSupervisor live(qualification(),config(SupervisorMode::live));feed(live);
            require(live.arm(1,2*ms).live_permitted,"fully asserted synthetic live qualification passes contract only");
            RunSupervisor arrival(qualification(),config(SupervisorMode::live));auto s=state();s.timestamp_kind=StateTimestampKind::arrival_only;
            s.observed_host_ns=s.received_host_ns;s.timestamp_uncertainty_ns=-1;
            arrival.observe_state(s,2*ms);arrival.observe_sync(sync_observation(),2*ms);
            require(!arrival.arm(1,2*ms).live_permitted,"arrival-only visual timestamp cannot qualify live");
            RunSupervisor fake(qualification(),config());fake.observe_state(s,2*ms);fake.observe_sync(sync_observation(),2*ms);
            const auto allowed=fake.arm(1,2*ms);require(allowed.dispatch_permitted&&!allowed.capture_timestamp_known&&!allowed.live_permitted,"arrival-only precision remains explicit in FakeTouch");
        });
        run("identity geometry epochs and lease lock mismatches revoke",[] {
            for(int mismatch=0;mismatch<7;++mismatch) {
                RunSupervisor supervisor(qualification(),config());feed(supervisor);supervisor.arm(1,2*ms);
                auto s=state(10*ms,1,2);
                if(mismatch==0)s.identity.chart_sha256=std::string(64,'d');
                if(mismatch==1)s.identity.audio_sha256=std::string(64,'d');
                if(mismatch==2)s.identity.layout_id="other";
                if(mismatch==3)s.geometry_version=8;
                if(mismatch==4)s.epoch=2;
                SupervisorDecision stopped;
                if(mismatch<5)stopped=supervisor.observe_state(s,12*ms);
                else {auto a=sync_observation(10*ms,1,2);if(mismatch==5)a.lease.locked=false;else a.lease.uncertainty_ns=-1;stopped=supervisor.observe_sync(a,12*ms);}
                require(stopped.release_required&&!stopped.dispatch_permitted,"identity/geometry/epoch/unlocked/unknown uncertainty fail closed");
            }
        });
        run("sticky cross-thread user stop and clock regression",[] {
            RunSupervisor supervisor(qualification(),config());feed(supervisor);supervisor.arm(1,2*ms);
            std::thread stop([&]{supervisor.request_user_stop();});stop.join();
            const auto stopped=supervisor.status(10*ms);
            require(stopped.state==SupervisorState::user_stopped&&stopped.release_required&&!stopped.dispatch_permitted,"sticky atomic stop consumed by owner");
            const auto released=supervisor.acknowledge_release(ack(stopped,11*ms),11*ms);
            require(!released.release_required&&released.state==SupervisorState::user_stopped,"user stop permits release acknowledgement");
            feed(supervisor,20*ms,2,2);
            require(!supervisor.arm(2,22*ms).dispatch_permitted&&supervisor.status(23*ms).reason==SupervisorReason::user_stop,"new evidence and epoch cannot clear user stop");
            RunSupervisor clock(qualification(),config());feed(clock);clock.arm(1,2*ms);clock.status(10*ms);
            const auto reversed=clock.status(9*ms);
            require(reversed.release_required&&!reversed.dispatch_permitted&&reversed.revocation_reason==SupervisorReason::clock_fault,"host clock regression revokes");
        });
        run("segment scope and malformed configuration are explicit",[] {
            auto q=qualification();q.coverage=PlanCoverage::explicit_segment;q.segment_begin_ns=0;q.segment_end_ns=60'000*ms;
            RunSupervisor segment(q,config());feed(segment);require(segment.arm(1,2*ms).dispatch_permitted,"explicit segment accepted as a segment");
            q=qualification();q.segment_begin_ns=0;q.segment_end_ns=60'000*ms;
            rejects([&]{RunSupervisor bad(q,config());},"partial selection cannot be labeled full-chart");
            q=qualification();q.coverage=PlanCoverage::explicit_segment;
            rejects([&]{RunSupervisor bad(q,config());},"segment requires endpoints");
            q=qualification();q.compiled_cache_sha256.clear();rejects([&]{RunSupervisor bad(q,config());},"missing exact compiled identity rejected");
            auto c=config();c.max_manual_age_ns=0;rejects([&]{RunSupervisor bad(qualification(),c);},"zero max manual age rejected");
            c=config();c.max_release_ack_age_ns=0;rejects([&]{RunSupervisor bad(qualification(),c);},"zero release freshness budget rejected");
            c=config();c.max_clock_drift_ppm=std::numeric_limits<double>::quiet_NaN();rejects([&]{RunSupervisor bad(qualification(),c);},"NaN drift budget rejected");
            require(std::string(supervisor_reason_name(SupervisorReason::release_failed))=="release_failed","reason names are structured");
        });
        std::cout<<checks<<" supervisor checks passed; no classifier, device, or UI invoked\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
