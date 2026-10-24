#include "rhythm/session_runner.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

static_assert(!std::is_constructible_v<rhythm::SessionRunner,rhythm::CompiledContactPlan,
    rhythm::PlanQualification,const pas::Clock&,pas::TouchBackend&>);

namespace {
using namespace rhythm;
constexpr Ns ms=1000000;
int checks=0;
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);++checks;}
template<class Function> void rejects(Function run,const char* message) {
    bool failed=false;try{run();}catch(const std::exception&){failed=true;}require(failed,message);
}
CompiledContactPlan candidate() {
    SemanticChart chart;chart.source={"synthetic-session",std::string(64,'a'),"test-song","IN","synthetic-v3"};
    ChartLine line;line.bpm=120;
    MoveLineEvent move;move.time.end_ns=10'000*ms;move.start_x=move.end_x=.5;move.start_y=move.end_y=.5;
    NumericLineEvent rotation;rotation.time.end_ns=move.time.end_ns;
    NumericLineEvent opacity=rotation;opacity.start=opacity.end=1;
    line.move_events={move};line.rotate_events={rotation};line.opacity_events={opacity};
    for(auto kind:{NoteKind::hold,NoteKind::tap,NoteKind::flick}) {
        SemanticNote note;note.kind=kind;
        note.start_ns=kind==NoteKind::hold ? 10*ms : (kind==NoteKind::tap ? 300*ms : 400*ms);
        note.end_ns=kind==NoteKind::hold ? 250*ms : note.start_ns;
        note.source.json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(line.notes.size());
        note.source.id=chart.source.chart_sha256+'#'+note.source.json_pointer;
        line.notes.push_back(note);
    }
    chart.lines.push_back(line);chart.note_count=3;
    Identity id{Game::phigros,chart.source.song_id,chart.source.difficulty,chart.source.game_revision,
        chart.source.chart_sha256,std::string(64,'b'),"synthetic-1280x720"};
    auto result=compile_phigros_contacts(chart,id,{1280,720});
    if(!result.plan)throw std::runtime_error("synthetic contact compilation failed");
    return std::move(*result.plan);
}
PlanQualification qualification(const CompiledContactPlan& plan) {
    PlanQualification q;q.identity=plan.memory.identity;q.compiled_cache_sha256=plan.metadata.cache_sha256;
    q.geometry_version=7;q.all_notes_in_scope_compiled=true;q.requires_calibration=true;return q;
}
StateObservation state(const Identity& id,Ns observed,std::uint64_t sequence=1,std::uint64_t epoch=1) {
    StateObservation result;result.identity=id;result.state=ObservedGameState::playing;
    result.source=StateObservationSource::visual;result.timestamp_kind=StateTimestampKind::mapped_capture;
    result.sequence=sequence;result.epoch=epoch;result.geometry_version=7;
    result.evidence_id="synthetic actual-frame-"+std::to_string(sequence);
    result.observed_host_ns=observed;result.received_host_ns=observed+2*ms;
    result.expires_host_ns=observed+100*ms;result.timestamp_uncertainty_ns=ms;return result;
}
SupervisorSyncObservation sync(const Identity& id,Ns observed,std::uint64_t sequence=1,
        std::uint64_t epoch=1,Ns origin=0) {
    SupervisorSyncObservation result;result.identity=id;result.sequence=sequence;result.received_host_ns=observed+2*ms;
    result.clock={0,origin,1};result.lease={true,epoch,7,observed,observed+120*ms,ms};
    result.evidence_id="synthetic actual-sync-"+std::to_string(sequence);return result;
}
SessionStatus feed(SessionRunner& runner,const Identity& id,Ns observed,std::uint64_t sequence=1,
        std::uint64_t epoch=1,Ns origin=0) {
    runner.observe_state(state(id,observed,sequence,epoch));
    return runner.observe_sync(sync(id,observed,sequence,epoch,origin));
}
ReleaseAcknowledgement ack(const SessionStatus& status,Ns now,ReleaseKnowledge knowledge) {
    return {status.supervision.revocation_id,status.supervision.revoked_epoch,knowledge,now,now,
        "synthetic FakeTouch observed zero-active map"};
}
bool reason(const SessionPreflight& report,const std::string& value) {
    return std::find(report.reasons.begin(),report.reasons.end(),value)!=report.reasons.end();
}
void run(const char* name,const std::function<void()>& test) {test();std::cout<<"PASS "<<name<<'\n';}
}

int main() {
    using namespace rhythm;
    try {
        run("compiler supervisor bridge FakeTouch preserve complete plan and dispatch",[] {
            auto plan=candidate();const auto q=qualification(plan);const auto expected_steps=plan.total_steps;
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);SessionRunner runner(plan,q,clock,touch);
            require(runner.plan().metadata.cache_sha256==plan.metadata.cache_sha256,"metadata retained");
            require(runner.qualification().requires_calibration,"candidate qualification preserved");
            require(runner.bridge_config().max_contacts==plan.metadata.options.max_contacts,"capacity derives from plan");
            clock.set(2*ms);require(!feed(runner,q.identity,0,1,1,100*ms).running,"observations do not arm");
            auto result=runner.arm(1);require(result.running&&!result.supervision.live_permitted,"explicit fake-only arm");
            require(touch.receipts().empty(),"arm does not dispatch before tick");
            std::uint64_t sequence=1;
            for(Ns now=3*ms;now<600*ms&&result.running;now+=ms) {
                clock.set(now);
                if(now%(40*ms)==2*ms) feed(runner,q.identity,now-2*ms,++sequence,1,100*ms);
                result=runner.tick();
            }
            require(result.finished&&!result.running,"complete finite plan finishes");
            require(touch.receipts().size()==expected_steps&&touch.contacts().empty(),"all steps and final release observed");
            require(result.supervision.release_required,"completion awaits explicit known-release ack");
            clock.set(clock.now_ns()+ms);
            const auto released=runner.acknowledge_release(ack(result,clock.now_ns(),ReleaseKnowledge::known_released));
            require(!released.supervision.release_required&&released.finished,"actual fake zero clears release latch");
        });
        run("timer status never changes observation sequence or dispatches",[] {
            auto plan=candidate();const auto q=qualification(plan);pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            SessionRunner runner(plan,q,clock,touch);clock.set(2*ms);feed(runner,q.identity,0);const auto first=runner.arm(1);
            const auto observed=first.supervision.grant->lease.observed_host_ns;
            for(Ns now=3*ms;now<90*ms;now+=ms) {
                clock.set(now);const auto result=runner.status();
                require(result.forwarded_sequence==first.forwarded_sequence,"status does not forward aged snapshot");
                require(result.supervision.grant->lease.observed_host_ns==observed,"actual timestamp retained");
                require(result.running&&!result.dispatched,"status neither fails same-sequence update nor dispatches");
            }
            require(touch.receipts().empty(),"status cannot inject even overdue fake work");
            clock.set(90*ms);const auto late=runner.tick();
            require(!late.running&&!late.finished&&late.supervision.release_required,"late owner tick stops rather than inventing on-time work");
        });
        run("state expiry revokes active Hold and releases before any later dispatch",[] {
            auto plan=candidate();const auto q=qualification(plan);pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            SessionRunner runner(plan,q,clock,touch);clock.set(2*ms);feed(runner,q.identity,0);runner.arm(1);
            clock.set(10*ms);require(runner.tick().running&&touch.contacts().size()==1,"Hold active");
            clock.set(100*ms);const auto stopped=runner.tick();
            require(!stopped.running&&stopped.supervision.release_required,"expired state stops before bridge tick");
            require(stopped.supervision.revocation_reason==SupervisorReason::stale_state,"expiry reason retained");
            require(touch.contacts().empty()&&!stopped.release.requested_ids.empty(),"active Hold released");
        });
        run("pause unknown release and fresh epoch require explicit acknowledgement and arm",[] {
            auto plan=candidate();const auto q=qualification(plan);pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            SessionRunner runner(plan,q,clock,touch);clock.set(2*ms);feed(runner,q.identity,0);runner.arm(1);
            clock.set(10*ms);runner.tick();clock.set(20*ms);
            auto paused=state(q.identity,18*ms,2);paused.state=ObservedGameState::paused;
            auto stopped=runner.observe_state(paused);require(!stopped.running&&touch.contacts().empty(),"pause releases immediately on owner");
            clock.set(21*ms);stopped=runner.acknowledge_release(ack(stopped,21*ms,ReleaseKnowledge::unknown));
            require(stopped.supervision.release_required,"unknown ack remains pending");
            clock.set(24*ms);feed(runner,q.identity,22*ms,3,2,100*ms);
            require(!runner.arm(2).running,"new epoch cannot bypass pending release");
            clock.set(25*ms);const auto ready=runner.acknowledge_release(ack(stopped,25*ms,ReleaseKnowledge::known_released));
            require(!ready.supervision.release_required&&!ready.running,"known release does not auto-arm");
            clock.set(26*ms);require(runner.arm(2).running,"fresh epoch explicitly starts after known release");
        });
        run("user stop is sticky across release acknowledgement and new evidence",[] {
            auto plan=candidate();const auto q=qualification(plan);pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            SessionRunner runner(plan,q,clock,touch);clock.set(2*ms);feed(runner,q.identity,0);runner.arm(1);
            clock.set(10*ms);runner.tick();std::thread stop([&]{runner.request_user_stop();});stop.join();
            clock.set(11*ms);auto stopped=runner.tick();
            require(!stopped.running&&touch.contacts().empty(),"owner handles asynchronous stop release");
            clock.set(12*ms);runner.acknowledge_release(ack(stopped,12*ms,ReleaseKnowledge::known_released));
            clock.set(22*ms);feed(runner,q.identity,20*ms,3,2,100*ms);
            require(!runner.arm(2).running&&runner.status().supervision.state==SupervisorState::user_stopped,"stop never re-arms object");
        });
        run("claimed known release cannot conceal an observed FakeTouch contact",[] {
            auto plan=candidate();const auto q=qualification(plan);pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            SessionRunner runner(plan,q,clock,touch);clock.set(2*ms);feed(runner,q.identity,0);runner.arm(1);
            clock.set(10*ms);runner.tick();clock.set(20*ms);auto stopped=runner.invalidate();
            require(stopped.supervision.release_required&&touch.contacts().empty(),"revocation attempted release");
            // Model a caller sharing the borrowed backend incorrectly: zero-active
            // must still be observed, not inferred from the preceding RPC report.
            touch.inject({999,1,pas::Phase::down,100,100,20*ms,99});
            clock.set(21*ms);const auto refused=runner.acknowledge_release(ack(stopped,21*ms,ReleaseKnowledge::known_released));
            require(refused.supervision.release_required&&refused.supervision.release_reason==SupervisorReason::release_unknown,"actual nonzero map downgrades claimed knowledge");
            touch.release_all();clock.set(22*ms);
            require(!runner.acknowledge_release(ack(stopped,22*ms,ReleaseKnowledge::known_released)).supervision.release_required,"fresh actual zero permits explicit ack");
        });
        run("preflight refuses calibration downgrade live mode and damaged plan",[] {
            auto plan=candidate();auto q=qualification(plan);pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            const auto live=preflight_session(plan,q,SupervisorMode::live);
            require(!live.accepted&&!live.live_permitted&&reason(live,"unqualified_live_plan"),"pending calibration blocks live");
            auto config=fake_session_config();config.mode=SupervisorMode::live;
            rejects([&]{SessionRunner bad(plan,q,clock,touch,config);},"concrete FakeTouch cannot be live backend");
            q.requires_calibration=false;q.timing_qualified=q.geometry_qualified=q.backend_qualified=true;
            q.timing_evidence_id=q.geometry_evidence_id=q.backend_evidence_id="synthetic only";
            require(!preflight_session(plan,q,SupervisorMode::live).accepted,"flags cannot enable unavailable live runner");
            plan.metadata.requires_calibration=true;
            require(reason(preflight_session(plan,q,SupervisorMode::fake_touch),"calibration_metadata_downgrade"),"projection status cannot disappear");
            plan=candidate();q=qualification(plan);plan.events.front().point.x+=.1;
            require(reason(preflight_session(plan,q,SupervisorMode::fake_touch),"events_memory_mismatch"),"inconsistent plan rejected");
            plan=candidate();q=qualification(plan);
            plan.memory.contacts.front().steps.front().point.x+=.1;
            plan.events.front().point.x+=.1;
            require(reason(preflight_session(plan,q,SupervisorMode::fake_touch),"compiled_fingerprint_mismatch"),"consistent coordinate edits cannot retain original fingerprint");
            plan=candidate();q=qualification(plan);plan.ownership.front().judgment_ns+=ms;
            require(reason(preflight_session(plan,q,SupervisorMode::fake_touch),"compiled_fingerprint_mismatch"),"semantic ownership edit cannot retain fingerprint");
            plan=candidate();q=qualification(plan);plan.metadata.options.offscreen_policy=OffscreenPolicy::bounded_normal_candidate;
            require(reason(preflight_session(plan,q,SupervisorMode::fake_touch),"calibration_metadata_inconsistent"),"projection cannot hide calibration via metadata flag");
            require(touch.receipts().empty(),"preflight never injects");
        });
        run("explicit segment includes full contact lead and release envelope",[] {
            const auto plan=candidate();auto q=qualification(plan);q.coverage=PlanCoverage::explicit_segment;
            q.segment_begin_ns=0;q.segment_end_ns=410*ms;
            require(reason(preflight_session(plan,q,SupervisorMode::fake_touch),"contact_step_outside_segment"),"Flick UP outside declared envelope rejects");
            q.segment_end_ns=500*ms;
            require(preflight_session(plan,q,SupervisorMode::fake_touch).accepted,"complete bounded segment accepted");
        });
        std::cout<<checks<<" session runner checks passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
