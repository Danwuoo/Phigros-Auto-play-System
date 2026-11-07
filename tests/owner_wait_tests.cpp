#include "../apps/chart_experiment/owner_wait.hpp"
#include "../apps/chart_experiment/fresh_capture_recovery.hpp"
#include <iostream>
#include <utility>

namespace {
using namespace rhythm::experimental;
constexpr OwnerWaitNs ms=1'000'000,O=100'000'000'000;
unsigned checks=0;
void check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
template<class F>void rejects(F f,const char* reason){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,reason);}
struct Clock final:pas::Clock {
    mutable OwnerWaitNs value=O;
    mutable unsigned reads=0;
    unsigned jump_read=0;OwnerWaitNs jump=0;
    OwnerWaitNs now_ns()const override {if(++reads==jump_read)value+=jump;return value;}
};
struct FakeState {unsigned timers=0,pauses=0,finishes=0;bool finished=false;};
class Backend final:public OwnerWaitBackend {
    Clock& clock_;std::shared_ptr<FakeState> state_;
public:
    OwnerWaitNs timer_extra=0,pause_advance=50'000;bool fail_timer=false;
    Backend(Clock& clock,std::shared_ptr<FakeState> state):clock_(clock),state_(std::move(state)){}
    ~Backend()override{finish();}
    void wait_timer(OwnerWaitNs duration)override {++state_->timers;clock_.value+=duration+timer_extra;if(fail_timer)throw std::runtime_error("fake timer failure");}
    void spin_pause()override {++state_->pauses;clock_.value+=pause_advance;}
    OwnerWaitPlatformStatus platform_status()const noexcept override {
        OwnerWaitPlatformStatus s;s.high_resolution_timer=true;s.finished=state_->finished;return s;
    }
    void finish()noexcept override {if(!state_->finished){state_->finished=true;++state_->finishes;}}
};
}
int main() {
try {
    // Independent pure capture policy: no gRPC/device/UI or input operations.
    {constexpr std::int64_t U=1'700'000'000'000'000;
        const auto meta=[&](std::int64_t received,std::int64_t source,int rotation=1){return RecoveryFrameMeta{received,source,1280,720,3840,rotation,true};};
        FreshCaptureRecovery policy(1);const auto first=meta(O,U);
        check(!policy.admit(first,RecoveryRoute::snapshot,O,false).accepted&&!policy.admit(first,RecoveryRoute::snapshot,O,true).accepted,
            "snapshot established a stream anchor or ran before arm");
        check(!policy.should_probe(O+200*ms,true)&&policy.last_received_ns()==-1,"probe ran without an accepted stream anchor");
        const auto initial=policy.admit(first,RecoveryRoute::stream,O,false);
        check(initial.accepted&&initial.sequence==1&&policy.last_received_ns()==O,"initial real stream frame was not admitted");
        check(!policy.should_probe(O+200*ms-1,true)&&!policy.should_probe(O+200*ms,false),"snapshot cadence or after-arm gate shortened");
        check(policy.should_probe(O+200*ms,true)&&!policy.should_probe(O+200*ms+1,true)&&policy.probe_attempts()==1&&policy.last_received_ns()==O,
            "probe reservation repeated or renewed the accepted receipt");
        const auto snapshot=policy.admit(meta(O+210*ms,U+210'000),RecoveryRoute::snapshot,O+211*ms,true);
        check(snapshot.accepted&&snapshot.sequence==2&&policy.last_received_ns()==O+210*ms,"fresh actual snapshot was not admitted with original receipt");
        check(!policy.admit(meta(O+205*ms,U+205'000),RecoveryRoute::stream,O+220*ms,true).accepted,
            "delayed stream rolled the accepted receipt backward");
        check(!policy.admit(meta(O+220*ms,U+205'000),RecoveryRoute::stream,O+220*ms,true).accepted,
            "late arriving old stream PTS replaced a newer snapshot");
        check(policy.sequence()==2&&policy.last_source_timestamp_us()==U+210'000,"rejected old source manufactured a publication");
        check(policy.admit(meta(O+230*ms,U+230'000),RecoveryRoute::stream,O+231*ms,true).sequence==3,"fresh stream did not resume after snapshot");
        check(!policy.admit(meta(O+240*ms,U+230'000),RecoveryRoute::snapshot,O+241*ms,true).accepted,"equal source timestamp renewed proof");
        check(!policy.should_probe(O+430*ms-1,true)&&policy.should_probe(O+430*ms,true),"fresh publication did not postpone the next probe");
        check(!policy.admit(meta(O+440*ms,U+440'000),RecoveryRoute::snapshot,O+690*ms,true).accepted,"exact250ms-old snapshot receipt accepted");
        check(policy.should_probe(O+730*ms,true)&&policy.last_received_ns()==O+230*ms&&O+730*ms-policy.last_received_ns()==500*ms,
            "failed/repeated snapshots refreshed the external unchanged500ms runtime watchdog");
        FreshCaptureRecovery portrait(0);check(portrait.admit(meta(O,U,0),RecoveryRoute::stream,O,false).accepted,"normalized portrait rotation0 rejected");
        rejects([]{FreshCaptureRecovery bad(-1);},"unfixed capture rotation accepted");
        rejects([]{FreshCaptureRecovery bad(2);},"unsupported source rotation accepted");}
    {constexpr std::int64_t U=1'700'000'000'000'000;
        const auto meta=[&](std::int64_t received,std::int64_t source){return RecoveryFrameMeta{received,source,1280,720,3840,1,true};};
        const auto first=meta(O,U);
        for(int bad=0;bad<10;++bad){FreshCaptureRecovery policy(1);check(policy.admit(first,RecoveryRoute::stream,O,false).accepted,"invalid-case anchor failed");
            auto frame=meta(O+ms,U+1000);auto route=RecoveryRoute::snapshot;auto now=O+2*ms;
            switch(bad){case 0:frame.width=1279;break;case 1:frame.height=719;break;case 2:frame.stride=3839;break;
                case 3:frame.rotation=0;break;case 4:frame.source_valid=false;break;case 5:frame.source_timestamp_us=0;break;
                case 6:frame.received_ns=O;break;case 7:frame.source_timestamp_us=U;break;case 8:now=O;break;
                case 9:route=static_cast<RecoveryRoute>(99);break;}
            const auto rejected=policy.admit(frame,route,now,true);
            check(!rejected.accepted&&rejected.sequence==0&&policy.sequence()==1&&policy.last_received_ns()==O,
                "malformed or old capture changed accepted proof/sequence");}
        for(const auto age:{250*ms-1,250*ms}){FreshCaptureRecovery policy(1);policy.admit(first,RecoveryRoute::stream,O,false);
            check(policy.admit(meta(O+100*ms,U+100'000),RecoveryRoute::snapshot,O+100*ms+age,true).accepted==(age<250*ms),
                "capture receipt250ms boundary changed");}
        for(const auto extra:{std::int64_t{0},std::int64_t{1}}){FreshCaptureRecovery policy(1);policy.admit(first,RecoveryRoute::stream,O,false);
            check(policy.admit(meta(O+300*ms+extra,U+50'000),RecoveryRoute::snapshot,O+300*ms+extra,true).accepted==(extra==0),
                "relative source lag250ms boundary changed");}
        for(const auto source:{U+1'000'001,U+10'000'001,std::numeric_limits<std::int64_t>::max()}){
            FreshCaptureRecovery policy(1);policy.admit(first,RecoveryRoute::stream,O,false);
            check(!policy.admit(meta(O+10*ms,source),RecoveryRoute::snapshot,O+10*ms,true).accepted,
                "source clock jump/overflow bypassed relative timestamp guard");}
        FreshCaptureRecovery budget(1);budget.admit(first,RecoveryRoute::stream,O,false);
        for(std::size_t n=1;n<=FreshCaptureRecovery::max_probe_attempts;++n)
            check(budget.should_probe(O+static_cast<std::int64_t>(n)*200*ms,true),"finite recovery probe budget ended early");
        check(!budget.should_probe(O+static_cast<std::int64_t>(FreshCaptureRecovery::max_probe_attempts+1)*200*ms,true)&&
            budget.probe_attempts()==4096&&budget.sequence()==1&&budget.last_received_ns()==O,"probe attempts were unbounded or invented observations");}
    {const auto at_edge=plan_owner_wait(O,O+ms,O+50*ms);
        check(at_edge.kind==OwnerWaitKind::spin&&at_edge.critical_deadline,"50ms edge entered timer");
        check(at_edge.planned_return_ns==O+250'000&&at_edge.timer_request_ns==0,"critical slice exceeded250us");
        const auto just_outside=plan_owner_wait(O,O+ms,O+50*ms+1);
        check(just_outside.kind==OwnerWaitKind::spin&&!just_outside.critical_deadline&&just_outside.planned_return_ns==O+1,"guard crossing overshot boundary");
        const auto distant_poll=plan_owner_wait(O,O+10*ms,O+52*ms);
        check(distant_poll.kind==OwnerWaitKind::timer&&distant_poll.timer_request_ns==ms,"far polling exceeded1ms");
        const auto approaching=plan_owner_wait(O,O+ms,O+50*ms+600'000);
        check(approaching.kind==OwnerWaitKind::timer&&approaching.planned_return_ns==O+600'000,"timer ignored50ms advance guard");
        check(plan_owner_wait(O,O+ms,O-1).kind==OwnerWaitKind::input_overdue,"overdue input was clamped or waited");
        check(plan_owner_wait(O,O,O+ms).kind==OwnerWaitKind::target_due,"due polling target waited");
        check(plan_owner_wait(O,O+ms,std::nullopt).kind==OwnerWaitKind::timer,"ordinary polling changed without actual deadline");
        check(plan_owner_wait(O,O+1,O+10*ms).planned_return_ns==O+1,"spin delayed a nearer target");
        const OwnerWaitNs long_uptime=OwnerWaitNs{1}<<54;
        check(plan_owner_wait(long_uptime,long_uptime+ms,long_uptime+9*ms).kind==OwnerWaitKind::spin,"valid long uptime rejected");
        for(const auto bad:{OwnerWaitNs{-1},std::numeric_limits<OwnerWaitNs>::max()})
            rejects([&]{(void)plan_owner_wait(bad,O,std::nullopt);},"invalid host clock accepted");
        auto options=OwnerWaitOptions{};options.spin_slice_ns=250'001;
        rejects([&]{(void)plan_owner_wait(O,O+ms,O+ms,options);},"unbounded spin slice accepted");
        options={};options.critical_guard_ns=100'000'001;
        rejects([&]{(void)plan_owner_wait(O,O+ms,O+ms,options);},"unbounded critical guard accepted");}
    {Clock clock;auto state=std::make_shared<FakeState>();auto backend=std::make_unique<Backend>(clock,state);
        backend->timer_extra=37*ms;OwnerWait wait(clock,std::move(backend));
        wait.until(O+ms,O+9*ms);const auto d=wait.diagnostics();
        check(state->timers==0&&state->pauses==5,"9ms actual failure window still entered kernel timer");
        check(clock.value==O+250'000&&d.spin_count==1&&d.timer_count==0,"critical spin did not return to owner");
        check(d.last.begin_ns==O&&d.last.end_ns==clock.value&&d.last.input_due_ns==O+9*ms,"critical timing identity lost");
        check(d.spin_elapsed_ns==250'000&&d.spin_iterations==5,"spin accounting wrong");
        wait.until(clock.value+ms,clock.value-1);const auto overdue=wait.diagnostics();
        check(overdue.no_wait_count==1&&state->pauses==5&&state->timers==0,"overdue call waited");
        check(overdue.last.input_due_ns==clock.value-1&&overdue.last.input_lateness_ns==1,"overdue value was retimed");
        wait.finish();wait.finish();check(state->finishes==1,"backend rollback not idempotent");
        rejects([&]{wait.until(clock.value+ms);},"finished owner wait reused");}
    {Clock clock;auto state=std::make_shared<FakeState>();auto backend=std::make_unique<Backend>(clock,state);
        backend->timer_extra=37*ms;OwnerWait wait(clock,std::move(backend));
        wait.until(O+ms,O+100*ms);auto d=wait.diagnostics();
        check(d.last.timer_begin_ns==O&&d.last.timer_end_ns==O+38*ms,"actual timer begin/end missing");
        check(d.maximum_overshoot_ns==37*ms&&d.maximum_overshoot.begin_ns==O,"timer oversleep hidden or unattributed");
        check(d.maximum_elapsed_ns==38*ms&&wait.waits==1&&wait.maximum_overshoot_ns==37*ms,"compatibility timer metrics differ");
        check(d.last.planned_return_ns==O+ms&&d.last.target_ns==O+ms,"timer target rewritten after oversleep");}
    {Clock clock;auto state=std::make_shared<FakeState>();auto backend=std::make_unique<Backend>(clock,state);
        backend->pause_advance=40*ms;OwnerWait wait(clock,std::move(backend));wait.until(O+ms,O+9*ms);
        const auto d=wait.diagnostics();
        check(d.last.end_ns==O+40*ms&&d.last.input_lateness_ns==31*ms,"spin preemption hid real late clock");
        check(d.last.input_due_ns==O+9*ms&&d.last.overshoot_ns==39'750'000,"spin preemption changed deadline");}
    {Clock clock;auto state=std::make_shared<FakeState>();auto backend=std::make_unique<Backend>(clock,state);
        backend->pause_advance=0;auto options=OwnerWaitOptions{};options.spin_iteration_limit=7;
        OwnerWait wait(clock,std::move(backend),options);wait.until(O+ms,O+9*ms);const auto d=wait.diagnostics();
        check(state->pauses==7&&d.spin_iteration_limit_reached==1,"frozen clock spin was not bounded");
        check(d.last.spin_iteration_limit_reached&&d.last.end_ns==O,"spin bound invented elapsed time");}
    {Clock clock;auto state=std::make_shared<FakeState>();auto backend=std::make_unique<Backend>(clock,state);
        backend->fail_timer=true;backend->timer_extra=37*ms;OwnerWait wait(clock,std::move(backend));
        rejects([&]{wait.until(O+ms,O+100*ms);},"failed timer hidden");const auto d=wait.diagnostics();
        check(d.failed_count==1&&d.last.failed&&d.last.timer_end_ns==O+38*ms,"failed wait timing not retained");}
    {Clock clock;auto state=std::make_shared<FakeState>();OwnerWait wait(clock,std::make_unique<Backend>(clock,state));
        clock.jump_read=3;clock.jump=60*ms;wait.until(O+ms,O+60*ms);const auto d=wait.diagnostics();
        check(state->timers==0&&state->pauses==0&&d.last.kind==OwnerWaitKind::input_overdue,"stale planning decision entered timer");}
    {Clock clock;auto state=std::make_shared<FakeState>();OwnerWait wait(clock,std::make_unique<Backend>(clock,state));
        for(unsigned i=0;i<100;++i)wait.until(clock.value+ms,clock.value+9*ms);
        const auto d=wait.diagnostics();check(d.calls==100&&d.recent_size==16&&d.recent_next==4,"wait history grew or wrapped incorrectly");
        check(d.last.sequence==100&&d.recent[3].sequence==100,"bounded wait history lost sequence");
        clock.value-=ms;rejects([&]{wait.until(clock.value+ms);},"cross-call clock rollback accepted");}
    {Clock clock;clock.value=-1;auto state=std::make_shared<FakeState>();
        rejects([&]{OwnerWait wait(clock,std::make_unique<Backend>(clock,state));},"invalid initial clock accepted");
        check(state->finishes==1,"constructor failure skipped backend rollback");}
    // Real OS checks assert ownership/rollback, never a latency guarantee.
    {const auto process_priority=GetPriorityClass(GetCurrentProcess());const auto original=GetThreadPriority(GetCurrentThread());
        Clock clock;{
            OwnerWait wait(clock);const auto active=wait.diagnostics().platform;
            check(active.owner_thread_id==GetCurrentThreadId()&&active.high_resolution_timer,"real owner policy identity wrong");
            check(active.applied_thread_priority>=THREAD_PRIORITY_ABOVE_NORMAL&&GetThreadPriority(GetCurrentThread())==active.applied_thread_priority,"owner priority not actually applied");
            check(GetPriorityClass(GetCurrentProcess())==process_priority,"process priority changed");
            wait.finish();const auto done=wait.diagnostics().platform;
            check(done.finished&&GetThreadPriority(GetCurrentThread())==original,"owner priority not restored");
            check(!active.priority_changed||(done.priority_restore_attempted&&done.priority_restored),"priority rollback unreported");
            check(!active.timer_resolution_active||(done.timer_resolution_end_attempted&&done.timer_resolution_end_result==0&&!done.timer_resolution_active),"timer period request unbalanced");
        }
        check(GetThreadPriority(GetCurrentThread())==original&&GetPriorityClass(GetCurrentProcess())==process_priority,"RAII changed process/thread policy");
        clock.value=-1;rejects([&]{OwnerWait wait(clock);},"real backend constructor failure missing");
        check(GetThreadPriority(GetCurrentThread())==original,"constructor failure leaked thread priority");}
    std::cout<<checks<<" owner-wait checks passed; offline policy/mock/OS ownership only, no latency or gameplay qualification.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}
}
