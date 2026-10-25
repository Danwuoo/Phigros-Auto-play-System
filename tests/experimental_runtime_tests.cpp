#include "../apps/chart_experiment/experimental_runtime.hpp"
#include <functional>
#include <iostream>

namespace {
using namespace rhythm;
using namespace rhythm::experimental;
constexpr Ns ms=1'000'000,S=1'000'000'000,O=100*S;
unsigned checks=0;
void check(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message) {bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,message);}
Contact contact(std::uint64_t id,Ns begin=12*S,Ns end=12*S+16*ms,unsigned moves=0) {
    Contact c{id,{{begin,Phase::down,{.25,.75}}}};
    for(unsigned i=1;i<=moves;++i)c.steps.push_back({begin+(end-begin)*i/(moves+1),Phase::move,{.25+.1*i/(moves+1),.75}});
    c.steps.push_back({end,Phase::up,{moves?.35:.25,.75}});return c;
}
CompiledContactPlan compiled(std::vector<Contact> contacts,bool candidate=false,int capacity=4) {
    CompiledContactPlan p;
    p.memory.identity={Game::phigros,"synthetic","IN","v3",std::string(64,'a'),std::string(64,'b'),"1280x720"};
    p.memory.contacts=std::move(contacts);p.metadata.geometry.input_width=1280;p.metadata.geometry.input_height=720;
    p.metadata.options.max_contacts=capacity;p.metadata.requires_calibration=candidate;
    if(candidate) {p.metadata.options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
        p.metadata.options.max_normal_projection_reference_pixels=250;p.metadata.options.calibration_id="pending-test";
        p.metadata.projection_policy_version="bounded-normal-intersection-candidate-v1";}
    for(std::size_t i=0;i<p.memory.contacts.size();++i) {
        const auto& c=p.memory.contacts[i];NoteContactOwnership owner;
        owner.contact_id=c.id;owner.note_index=i;owner.judgment_ns=c.steps.front().song_ns;
        owner.kind=c.steps.size()>2?NoteKind::hold:NoteKind::tap;
        owner.source_json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(i);
        owner.source_note_id=p.memory.identity.chart_sha256+'#'+owner.source_json_pointer;
        if(candidate)owner.normal_projection_spans.push_back({c.steps.front().song_ns,c.steps.back().song_ns,0,0,16,0});
        p.ownership.push_back(owner);
        for(const auto& s:c.steps)p.events.push_back({s.song_ns,c.id,s.phase,s.point});
        p.total_steps+=c.steps.size();
    }
    std::sort(p.events.begin(),p.events.end(),[](const auto& a,const auto& b){return std::tuple(a.song_ns,phase_order(a.phase),a.contact_id)<std::tuple(b.song_ns,phase_order(b.phase),b.contact_id);});
    p.peak_contacts=validate(p.memory,capacity);p.metadata.cache_sha256=compiled_contact_fingerprint(p);return p;
}
ExperimentalAnchor anchor(Ns phase=0) {
    ExperimentalAnchor a;a.origin_host_ns=O;a.received_host_ns=O+11*S+100*ms;a.explicit_phase_ns=phase;
    a.chosen_song_interval={11*S+95*ms,11*S+105*ms};a.epoch=a.generation=a.geometry_version=1;
    a.source_id="synthetic-actual-receipt-stream";a.source_sha256=std::string(64,'c');a.clock_id="QPC-receipt-only";
    a.evidence_id="anchor-3";a.supporting_evidence_ids={"anchor-1","anchor-2","anchor-3"};return a;
}
ExperimentalUiObservation ui(Ns now,std::uint64_t seq=1) {
    ExperimentalUiObservation u;u.sequence=seq;u.epoch=u.generation=u.geometry_version=1;u.received_host_ns=now;
    u.width=1280;u.height=720;u.source_valid=u.playing=true;u.source_id=anchor().source_id;u.evidence_id="UI-"+std::to_string(seq);return u;
}
void arm(ExperimentalContactRuntime& runtime,pas::FakeClock& clock) {
    clock.set(anchor().received_host_ns);runtime.observe_ui(ui(clock.now_ns()));check(runtime.arm(),"valid explicit trial arm rejected");
}
ExperimentalRuntimeTick advance(ExperimentalContactRuntime& runtime,pas::FakeClock& clock,Ns target,std::uint64_t& sequence) {
    auto now=clock.now_ns();ExperimentalRuntimeTick result;
    while(now<target) {now=std::min(now+20*ms,target);clock.set(now);runtime.observe_ui(ui(now,++sequence));result=runtime.tick();}
    return result;
}
class FaultTouch final:public pas::TouchBackend {
public:
    pas::FakeTouchBackend actual;bool fail_inject=false,throw_inject=false,fail_release=false,throw_release=false;
    unsigned releases=0;
    explicit FaultTouch(const pas::Clock& clock):actual(clock){}
    pas::TouchReceipt inject(const pas::TouchCommand& c) override {
        auto receipt=actual.inject(c);
        if(throw_inject)throw std::runtime_error("injected effect then exception");
        if(fail_inject) {receipt.success=false;receipt.reason="unknown injected effect";}
        return receipt;
    }
    pas::ReleaseReport release_all() override {
        ++releases;if(throw_release)throw std::runtime_error("release unknown");
        if(fail_release)return {{0},{},{0},0,0};
        return actual.release_all();
    }
};
// Deterministic owner/preemption clock: an actual clock read, including the
// scheduler's own submit read, can cross a real UP deadline after tick sampled
// its earlier time. No wall sleep or device operation is involved.
class ReadJumpClock final:public pas::Clock {
    mutable Ns now_=0;
    mutable unsigned reads_until_jump_=0;
    Ns jump_to_=0;
public:
    Ns now_ns() const override {
        if(reads_until_jump_&&!--reads_until_jump_)now_=jump_to_;
        return now_;
    }
    void set(Ns value){now_=value;}
    void jump_on_read(unsigned reads,Ns target){reads_until_jump_=reads;jump_to_=target;}
};
class MeasuredMoveTouch final:public pas::TouchBackend {
    ReadJumpClock& clock_;
public:
    pas::FakeTouchBackend actual;
    explicit MeasuredMoveTouch(ReadJumpClock& clock):clock_(clock),actual(clock){}
    pas::TouchReceipt inject(const pas::TouchCommand& command) override {
        auto receipt=actual.inject(command);
        // live005's last successful MOVE RPC took 360900 ns. Keep that
        // measured host interval in this synthetic minimal boundary fixture.
        if(command.phase==pas::Phase::move&&command.scheduled_ns==O+12*S+24'223'717) {
            clock_.set(receipt.injection_start_ns+360'900);receipt.injection_return_ns=clock_.now_ns();
        }
        return receipt;
    }
    pas::ReleaseReport release_all() override {return actual.release_all();}
};
class TimedTouch final:public pas::TouchBackend {
    ReadJumpClock& clock_;
public:
    pas::FakeTouchBackend actual;
    std::vector<pas::TouchReceipt> receipts;
    std::function<void(const pas::TouchCommand&)> before;
    std::function<Ns(const pas::TouchCommand&)> duration;
    std::function<void(const pas::TouchCommand&)> after;
    explicit TimedTouch(ReadJumpClock& clock):clock_(clock),actual(clock){}
    pas::TouchReceipt inject(const pas::TouchCommand& command) override {
        if(before)before(command);
        auto receipt=actual.inject(command);
        const auto elapsed=duration?duration(command):665'600;
        check(elapsed>0,"timed transport must consume nonzero time");
        clock_.set(receipt.injection_start_ns+elapsed);
        receipt.injection_return_ns=clock_.now_ns();receipts.push_back(receipt);
        if(after)after(command);
        return receipt;
    }
    pas::ReleaseReport release_all() override {return actual.release_all();}
};
void group(const char* name,const std::function<void()>& f){f();std::cout<<"PASS "<<name<<'\n';}
}
int main(int argc,char** argv) {
    try {
        if(argc>1)throw std::invalid_argument("offline synthetic tests take no arguments");
        (void)argv;
        group("whole-song over2000 notes retains every closed contact within step budget",[]{
            std::vector<Contact> contacts;
            for(unsigned n=0;n<2001;++n)contacts.push_back(contact(n+1,12*S+static_cast<Ns>(n)*50*ms,12*S+static_cast<Ns>(n)*50*ms+16*ms));
            const auto full=compiled(std::move(contacts));pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            ExperimentalContactRuntime run(full,clock,touch,anchor());const auto status=run.status();
            check(status.selected_contacts==2001&&status.selected_steps==4002,"runtime rejected/truncated more than2000 notes");
            check(touch.receipts().empty(),"prepare/construct emitted input");
        });
        group("whole-song integrity keeps every projected contact and calibration flag",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            auto full=compiled({contact(40),contact(5,31*S,31*S+16*ms)},true);
            full.ownership[1].normal_projection_reference_pixels=10;full.ownership[1].projection_min_reference_pixels=10;
            full.ownership[1].projection_max_reference_pixels=10;full.ownership[1].normal_projection_spans[0].correction_lower_reference_pixels=10;
            full.ownership[1].normal_projection_spans[0].correction_upper_reference_pixels=10;
            full.metadata.cache_sha256=compiled_contact_fingerprint(full);
            ExperimentalContactRuntime run(full,clock,touch,anchor());auto s=run.status();
            check(s.selected_contacts==2&&s.selected_steps==4&&s.peak_contacts==1,"whole-song contact silently filtered");
            check(s.experimental&&!s.timing_qualified&&!s.source_pixel_age_qualified&&s.full_plan_requires_calibration&&!s.selected_projection_free&&s.projected_contacts==1,"qualification downgrade/claim");
            check(s.plan_end_song_ns==31*S+16*ms,"whole-song end missing");
            check(run.sources()[0].intent_id==1&&run.sources()[0].source_contact_id==40,"source ownership missing");
            check(run.selected_memory().contacts[0].id==40,"source contact ID changed");
            auto bad=full;bad.memory.contacts[0].steps[0].point.x=.3;bad.events[0].point.x=.3;
            rejects([&]{ExperimentalContactRuntime r(bad,clock,touch,anchor());},"coherent data mutation bypassed full fingerprint");
            bad=full;bad.ownership[0].judgment_ns++;rejects([&]{ExperimentalContactRuntime r(bad,clock,touch,anchor());},"ownership mutation bypassed fingerprint");
            bad=full;bad.ownership[0].normal_projection_spans[0].correction_upper_reference_pixels=251;
            bad.metadata.cache_sha256=compiled_contact_fingerprint(bad);
            rejects([&]{ExperimentalContactRuntime r(bad,clock,touch,anchor());},"projection above D250 accepted");
        });
        group("fixed budgets and boundary never drop or clip",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            rejects([&]{ExperimentalContactRuntime r(compiled({contact(1,179*S,180*S)}),clock,touch,anchor());},"180s boundary clipped");
            rejects([&]{ExperimentalContactRuntime r(compiled({}),clock,touch,anchor());},"empty song accepted");
            rejects([&]{ExperimentalContactRuntime r(compiled({contact(1,12*S,13*S,19999)}),clock,touch,anchor());},"20001 steps accepted");
            rejects([&]{ExperimentalContactRuntime r(compiled({contact(1),contact(2),contact(3),contact(4),contact(5)}),clock,touch,anchor());},"peak5 accepted");
            std::vector<Contact> many;for(unsigned i=1;i<=10001;++i)many.push_back(contact(i,12*S+i*ms,12*S+i*ms+ms/2));
            rejects([&]{ExperimentalContactRuntime r(compiled(many),clock,touch,anchor());},"closed contacts exceeded20000-step resource budget");
        });
        group("fifth pointer requires explicit capacity and dispatches all five ownerships",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            std::vector<Contact> contacts;for(std::uint64_t id=1;id<=5;++id)contacts.push_back(contact(id,12*S,13*S));
            const auto full=compiled(contacts,false,5);check(full.peak_contacts==5,"five-contact fixture did not overlap");
            rejects([&]{ExperimentalContactRuntime run(full,clock,touch,anchor());},"default four-contact runtime accepted fifth pointer");
            ExperimentalRuntimeOptions options;options.contact_capacity=5;
            ExperimentalContactRuntime run(full,clock,touch,anchor(),options);arm(run,clock);std::uint64_t seq=1;
            advance(run,clock,O+12*S,seq);
            check(touch.contacts().size()==5&&touch.receipts().size()==5,"explicit capacity silently omitted fifth DOWN");
            std::set<int> pointers;
            for(const auto& receipt:touch.receipts()) {
                check(receipt.success&&receipt.command.phase==pas::Phase::down,"five-pointer setup included missing/failed DOWN");
                pointers.insert(receipt.command.contact_id);
            }
            check(pointers==std::set<int>{0,1,2,3,4},"fifth pointer was not independently owned");
            advance(run,clock,O+13*S,seq);const auto status=run.status();
            check(status.state==ExperimentalRuntimeState::completed&&status.completed_contacts==5&&touch.receipts().size()==10&&touch.contacts().empty(),
                "five-pointer completion omitted UP or retained contact");
            check(status.contact_capacity==5&&status.peak_contacts==5&&status.release_rpc_ok&&!status.timing_qualified&&!status.source_pixel_age_qualified,
                "five-pointer option lost capacity evidence or claimed device/timing qualification");
            options.contact_capacity=4;
            rejects([&]{ExperimentalContactRuntime limited(full,clock,touch,anchor(),options);},"explicit four-contact runtime accepted metadata capacity5");
            for(const int invalid:{-1,0,6}) {
                options.contact_capacity=invalid;
                rejects([&]{ExperimentalContactRuntime invalid_run(full,clock,touch,anchor(),options);},"invalid/unbounded contact capacity accepted");
            }
            options.contact_capacity=1;
            ExperimentalContactRuntime single(compiled({contact(1)},false,1),clock,touch,anchor(),options);
            check(single.status().contact_capacity==1&&single.status().selected_contacts==1,"capacity1 lower boundary rejected");
            rejects([&]{ExperimentalContactRuntime wrong_meta(compiled({contact(1)},false,2),clock,touch,anchor(),options);},
                "compiler metadata capacity exceeded explicit runtime capacity even with one selected note");
        });
        group("fifth held pointer is released on stop expiry and epoch invalidation",[]{
            for(unsigned which=0;which<3;++which) {
                pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
                std::vector<Contact> contacts;for(std::uint64_t id=1;id<=5;++id)contacts.push_back(contact(id,12*S,13*S));
                ExperimentalRuntimeOptions options;options.contact_capacity=5;
                ExperimentalContactRuntime run(compiled(contacts,false,5),clock,touch,anchor(),options);
                arm(run,clock);std::uint64_t seq=1;advance(run,clock,O+12*S,seq);
                check(touch.contacts().size()==5,"stop/expiry fixture did not hold five pointers");
                if(which==0){run.request_stop();run.tick();}
                if(which==1){clock.set(clock.now_ns()+100*ms);run.tick();}
                if(which==2){clock.set(clock.now_ns()+ms);auto observation=ui(clock.now_ns(),++seq);observation.epoch=2;run.observe_ui(observation);}
                const auto status=run.status();
                check(status.state==ExperimentalRuntimeState::stopped&&touch.contacts().empty()&&status.release_rpc_ok,
                    "fifth pointer survived stop/expiry/epoch invalidation");
                check(status.release.requested_ids==std::vector<int>{0,1,2,3,4}&&!run.arm(),
                    "five-pointer cleanup omitted an owner or allowed rearming");
            }
        });
        group("per-song projection ceiling is explicit and keeps input geometry bounded",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            auto full=compiled({contact(1)},true);auto& owner=full.ownership.front();
            full.metadata.options.max_normal_projection_reference_pixels=7000;
            full.metadata.options.calibration_id="synthetic-D7000-unqualified-candidate";
            owner.normal_projection_reference_pixels=-6480;owner.projection_min_reference_pixels=-6480;
            owner.projection_max_reference_pixels=0;
            owner.normal_projection_spans[0].correction_lower_reference_pixels=-6480;
            full.metadata.cache_sha256=compiled_contact_fingerprint(full);
            rejects([&]{ExperimentalContactRuntime r(full,clock,touch,anchor());},"default D250 accepted D7000 metadata");
            ExperimentalRuntimeOptions options;options.max_normal_projection_reference_pixels=7000;
            ExperimentalContactRuntime run(full,clock,touch,anchor(),options);
            const auto status=run.status();
            check(status.selected_contacts==1&&status.selected_steps==2&&status.projected_contacts==1,
                "explicit projection candidate omitted ownership/events");
            check(status.max_normal_projection_reference_pixels==7000&&status.full_plan_requires_calibration&&
                !status.timing_qualified&&!status.source_pixel_age_qualified,"explicit projection candidate became qualified");
            options.max_normal_projection_reference_pixels=6999;
            rejects([&]{ExperimentalContactRuntime r(full,clock,touch,anchor(),options);},"plan bound exceeded explicit per-song ceiling");
            options.max_normal_projection_reference_pixels=7000;
            auto outside=full;outside.memory.contacts.front().steps.front().point.x=1.01;
            outside.events.front().point.x=1.01;outside.metadata.cache_sha256=compiled_contact_fingerprint(outside);
            rejects([&]{ExperimentalContactRuntime r(outside,clock,touch,anchor(),options);},"larger native projection allowed offscreen input pixel");
            for(const double invalid:{-1.,10001.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
                options.max_normal_projection_reference_pixels=invalid;
                rejects([&]{ExperimentalContactRuntime r(full,clock,touch,anchor(),options);},"unbounded/invalid explicit projection ceiling accepted");
            }
        });
        group("per-song opening anchor domain preserves provenance freshness and all first notes",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            auto early=anchor();early.received_host_ns=O+S+200*ms;
            early.chosen_song_interval={S+195*ms,S+205*ms};
            const auto full=compiled({contact(1,S+700*ms,S+716*ms)});
            rejects([&]{ExperimentalContactRuntime r(full,clock,touch,early);},"early anchor silently changed default Dlyrotz domain");
            ExperimentalRuntimeOptions options;options.anchor_domain_begin_ns=0;options.anchor_domain_end_ns=S+700*ms;
            ExperimentalContactRuntime run(full,clock,touch,early,options);
            clock.set(early.received_host_ns);run.observe_ui(ui(clock.now_ns()));check(run.arm(),"explicit early-song anchor rejected");
            std::uint64_t sequence=1;advance(run,clock,O+S+700*ms,sequence);advance(run,clock,O+S+716*ms,sequence);
            check(run.status().state==ExperimentalRuntimeState::completed&&touch.receipts().size()==2&&touch.contacts().empty(),
                "early-song first note or release omitted");
            check(run.status().anchor_domain_begin_ns==0&&run.status().anchor_domain_end_ns==S+700*ms&&
                !run.status().timing_qualified,"explicit native domain changed timing qualification");
            auto bad=early;bad.chosen_song_interval.lower=S+701*ms;bad.chosen_song_interval.upper=S+711*ms;
            bad.received_host_ns=O+S+706*ms;
            rejects([&]{ExperimentalContactRuntime r(full,clock,touch,bad,options);},"anchor outside explicit domain accepted");
            for(const auto domain:std::vector<std::pair<Ns,Ns>>{{-1,S},{S,S},{2*S,S},{0,180*S}}) {
                options.anchor_domain_begin_ns=domain.first;options.anchor_domain_end_ns=domain.second;
                rejects([&]{ExperimentalContactRuntime r(full,clock,touch,early,options);},"invalid/overlong anchor domain accepted");
            }
        });
        group("whole synthetic584 contacts4911 steps peak4 across full duration",[]{
            std::vector<Contact> contacts;
            for(unsigned i=0;i<584;++i) {
                const auto begin=12*S+(i/4)*700*ms;
                contacts.push_back(contact(i+1,begin,begin+200*ms,6+(i<239?1:0)));
            }
            const auto full=compiled(std::move(contacts),true);
            check(full.total_steps==4911&&full.peak_contacts==4,"synthetic full-song shape differs");
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);ExperimentalContactRuntime run(full,clock,touch,anchor());
            arm(run,clock);std::uint64_t seq=1;const auto end=full.events.back().song_ns;
            const auto result=advance(run,clock,O+end,seq);
            check(result.status.state==ExperimentalRuntimeState::completed,"whole-song fake run did not finish");
            check(result.status.completed_contacts==584&&result.status.selected_steps==4911&&touch.contacts().empty(),"whole-song dropped contact/release");
            check(touch.receipts().size()==4911,"whole-song step count mismatch");
            for(std::size_t i=0;i<full.events.size();++i) {
                const auto& got=touch.receipts()[i].command;const auto& expected=full.events[i];
                check(got.scheduled_ns==O+expected.song_ns,"whole-song deadline changed");
                check(run.sources()[static_cast<std::size_t>(got.intent_id-1)].source_contact_id==expected.contact_id,"whole-song ownership/order changed");
            }
            clock.set(O+180*S);check(run.status().state==ExperimentalRuntimeState::completed,"completed state changed while waiting for result UI");
        });
        group("long hold compacts complete tail without shortening true release",[]{
            const auto full=compiled({contact(8,12*S,17*S,1000)});
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);ExperimentalContactRuntime run(full,clock,touch,anchor());
            arm(run,clock);std::uint64_t seq=1;advance(run,clock,O+17*S,seq);
            check(run.status().state==ExperimentalRuntimeState::completed,"long hold rolling window failed");
            check(touch.receipts().size()==1002&&touch.contacts().empty(),"long hold duplicated/omitted steps");
            const auto pointer=touch.receipts().front().command.contact_id;
            for(std::size_t i=0;i<full.events.size();++i) {
                const auto& got=touch.receipts()[i].command;
                check(got.scheduled_ns==O+full.events[i].song_ns&&got.contact_id==pointer,"long hold prefix/deadline/pointer changed");
                check(got.phase==(i==0?pas::Phase::down:i+1==full.events.size()?pas::Phase::up:pas::Phase::move),"long hold shortened or replayed DOWN");
            }
        });
        group("live007 nonzero RPCs cross former32step window without losing source steps",[]{
            // The live007 intent196 timing deltas are relative to its DOWN
            // due=140505819032943. Coordinates/IDs are synthetic. All 40
            // native event offsets, the first12 RPC intervals and concurrent
            // intent195 UP are retained. A4us next-guard advance is an explicit
            // simulation, not an assertion of the unlogged live guard time.
            constexpr Ns offsets[]{0,1'723'835,3'447'670,5'171'505,6'895'340,8'619'175,
                10'343'010,12'066'845,13'790'680,15'514'515,16'000'000,16'568'107,
                17'136'214,17'704'321,18'272'428,18'840'535,19'408'642,19'976'749,
                20'544'856,21'112'963,21'681'070,22'249'177,22'817'284,23'385'391,
                23'953'498,24'521'605,25'089'712,25'657'819,26'225'926,26'714'286,
                27'307'190,27'900'094,28'492'998,29'085'902,29'678'806,30'271'710,
                30'864'614,31'457'518,32'000'000,36'000'000};
            constexpr Ns starts[]{2'982'157,3'673'157,4'189'457,8'012'557,8'696'257,9'265'357,
                23'116'857,24'032'457,24'662'457,25'444'557,28'578'057,29'638'457};
            constexpr Ns durations[]{684'000,491'400,478'900,676'300,563'200,402'700,
                904'800,621'900,773'900,1'763'400,1'053'400,1'223'000};
            Contact flick{2,{}};
            for(std::size_t i=0;i<std::size(offsets);++i)
                flick.steps.push_back({12*S+offsets[i],i==0?Phase::down:i+1==std::size(offsets)?Phase::up:Phase::move,
                    {.7,.8-.1*static_cast<double>(i)/39}});
            const auto full=compiled({contact(1,12*S-155'428'571,12*S+16*ms),flick});
            ReadJumpClock clock;TimedTouch touch(clock);const auto down=O+12*S;
            touch.before=[&](const pas::TouchCommand& c){
                if(c.intent_id==1&&c.phase==pas::Phase::up)clock.set(down+27'215'457);
                if(c.intent_id==2)for(std::size_t i=0;i<std::size(starts);++i)
                    if(c.scheduled_ns==down+offsets[i])clock.set(down+starts[i]);
            };
            touch.duration=[&](const pas::TouchCommand& c)->Ns {
                if(c.intent_id==1&&c.phase==pas::Phase::up)return 1'351'900;
                if(c.intent_id==2)for(std::size_t i=0;i<std::size(starts);++i)
                    if(c.scheduled_ns==down+offsets[i])return durations[i];
                return 665'600; // observed live007 median for the unobserved tail
            };
            touch.after=[&](const pas::TouchCommand& c){
                if(c.intent_id==2&&c.scheduled_ns==down+offsets[11])
                    clock.jump_on_read(1,down+30'865'457);
            };
            ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;
            ExperimentalContactRuntime run(full,clock,touch,anchor(),options);
            clock.set(anchor().received_host_ns);std::uint64_t seq=1;run.observe_ui(ui(clock.now_ns(),seq));check(run.arm(),"live007 fixture arm");
            while(clock.now_ns()<down-30*ms) {
                clock.set(std::min(clock.now_ns()+20*ms,down-30*ms));run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
            }
            clock.set(down+starts[0]);run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
            check(touch.receipts.size()==4,"live007 first three flick steps missing");
            clock.set(down+starts[3]);run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
            check(touch.receipts.size()==7,"live007 next three flick steps missing");
            check(clock.now_ns()==down+9'668'057,"live007 actual preceding RPC end differs");
            clock.set(down+starts[6]);run.observe_ui(ui(down+20'784'457,++seq));const auto caught_up=run.tick();
            if(caught_up.status.state!=ExperimentalRuntimeState::completed)
                throw std::runtime_error("live007 dense prefix: "+caught_up.status.reason);
            check(touch.receipts.size()==42&&touch.actual.contacts().empty(),"live007 tail omitted or duplicate");
            for(std::size_t i=0;i<full.events.size();++i) {
                const auto& got=touch.receipts[i];const auto& event=full.events[i];const auto px=pixel_point(event.point,1280,720);
                check(got.command.scheduled_ns==O+event.song_ns&&got.command.x==px.first&&got.command.y==px.second,
                    "live007 original deadline or coordinate changed");
                check(got.command.intent_id==event.contact_id&&got.command.phase==
                    (event.phase==Phase::down?pas::Phase::down:event.phase==Phase::up?pas::Phase::up:pas::Phase::move),"live007 phase/source order differs");
                check(got.injection_return_ns>got.injection_start_ns&&got.injection_start_ns-got.command.scheduled_ns<=20*ms,
                    "live007 fake ACK or lateness relaxed");
            }
        });
        group("owner stall beyond20ms stops before any late MOVE even with complete tail",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            ExperimentalContactRuntime run(compiled({contact(1,12*S,12*S+100*ms,99)}),clock,touch,anchor());
            arm(run,clock);std::uint64_t seq=1;advance(run,clock,O+12*S,seq);
            check(touch.contacts().size()==1,"dense trajectory setup failed");
            clock.set(O+12*S+32*ms);run.observe_ui(ui(clock.now_ns(),++seq));
            const auto result=run.tick();
            check(result.status.state==ExperimentalRuntimeState::stopped&&result.status.reason=="compiled_step_lateness_limit_exceeded","complete tail erased actual step lateness");
            check(touch.receipts().size()==1&&touch.contacts().empty(),"missing dense MOVEs skipped or held contact not released");
        });
        group("owner crosses former omitted-step deadline while every remaining step stays within20ms",[]{
            ReadJumpClock clock;TimedTouch touch(clock);touch.duration=[](const pas::TouchCommand&)->Ns{return 300'000;};
            const auto full=compiled({contact(1,12*S,12*S+100*ms,99)});
            ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;
            ExperimentalContactRuntime run(full,clock,touch,anchor(),options);
            clock.set(anchor().received_host_ns);std::uint64_t seq=1;run.observe_ui(ui(clock.now_ns(),seq));check(run.arm(),"owner-stall fixture arm");
            const auto down=O+12*S;
            while(clock.now_ns()<down) {
                clock.set(std::min(clock.now_ns()+20*ms,down));run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
            }
            clock.set(down+15*ms);run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
            check(touch.receipts.size()==22&&clock.now_ns()==down+21'300'000,"nonzero owner-stall setup differs");
            // Old32-step plan omitted MOVE31 at31ms, but the actual next
            // source step is MOVE22 and is only10ms late after this owner gap.
            clock.set(down+32*ms);run.observe_ui(ui(clock.now_ns(),++seq));const auto crossed=run.tick();
            check(crossed.status.state==ExperimentalRuntimeState::armed,"owner boundary mistaken for actual lateness");
            while(run.status().state==ExperimentalRuntimeState::armed) {
                clock.set(clock.now_ns()+10*ms);run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
            }
            check(run.status().state==ExperimentalRuntimeState::completed&&touch.receipts.size()==101&&touch.actual.contacts().empty(),
                "owner boundary lost complete tail");
            for(std::size_t i=0;i<full.events.size();++i) {
                const auto& r=touch.receipts[i];const auto px=pixel_point(full.events[i].point,1280,720);
                check(r.command.scheduled_ns==O+full.events[i].song_ns&&r.command.x==px.first&&r.command.y==px.second,
                    "owner boundary skipped or retimed MOVE");
                check(r.injection_return_ns-r.injection_start_ns==300'000&&r.injection_start_ns-r.command.scheduled_ns<=20*ms,
                    "owner boundary fake ACK or late dispatch");
            }
        });
        group("nonzero dense RPC backlog still stops at actual20ms step lateness",[]{
            ReadJumpClock clock;TimedTouch touch(clock);touch.duration=[](const pas::TouchCommand&)->Ns{return 1'763'400;};
            const auto full=compiled({contact(1,12*S,12*S+36*ms,38)});
            ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;
            ExperimentalContactRuntime run(full,clock,touch,anchor(),options);
            clock.set(anchor().received_host_ns);std::uint64_t seq=1;run.observe_ui(ui(clock.now_ns(),seq));check(run.arm(),"backlog fixture arm");
            const auto down=O+12*S;
            while(clock.now_ns()<down-20*ms) {
                clock.set(std::min(clock.now_ns()+20*ms,down-20*ms));run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
            }
            clock.set(down+15*ms);run.observe_ui(ui(clock.now_ns(),++seq));const auto result=run.tick();
            check(result.status.state==ExperimentalRuntimeState::stopped&&result.status.reason=="compiled_step_lateness_limit_exceeded",
                "complete tail relaxed actual RPC backlog limit");
            check(touch.receipts.size()>1&&touch.receipts.size()<full.events.size()&&touch.actual.contacts().empty()&&result.status.release_rpc_ok,
                "RPC backlog omitted cleanup or falsely completed");
            for(const auto& r:touch.receipts)
                check(r.injection_return_ns-r.injection_start_ns==1'763'400&&r.injection_start_ns-r.command.scheduled_ns<=20*ms,
                    "backlog dispatched an over20ms step or false ACK");
        });
        group("live005 owner preemption across real UP preserves existing immutable tail",[]{
            // Minimal synthetic contacts, with only timing deltas taken from
            // private live005 receipts/UI: last MOVE start=UP-7.711596ms,
            // return=UP-7.350696ms; next UI process=UP-7.091996ms;
            // failed-revision cleanup began UP+0.027004ms. This does not
            // reproduce game pixels or claim the unlogged submit time is known.
            const Ns end=O+12*S+32*ms;
            Contact moving{1,{{12*S,Phase::down,{.25,.75}},
                {12*S+24'223'717,Phase::move,{.26,.75}},
                {12*S+25'733'149,Phase::move,{.27,.75}},
                {12*S+27'242'581,Phase::move,{.28,.75}},
                {12*S+28*ms,Phase::move,{.29,.75}},
                {12*S+32*ms,Phase::up,{.29,.75}}}};
            std::size_t deferred=0;
            for(unsigned jump_read=2;jump_read<=7;++jump_read) {
                ReadJumpClock clock;MeasuredMoveTouch touch(clock);ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;
                const auto full=compiled({moving,contact(2,12*S+12*ms,12*S+28*ms)});
                ExperimentalContactRuntime run(full,clock,touch,anchor(),options);
                clock.set(anchor().received_host_ns);std::uint64_t seq=1;run.observe_ui(ui(clock.now_ns(),seq));check(run.arm(),"preemption fixture arm");
                auto advance_to=[&](Ns target) {
                    while(clock.now_ns()<target) {
                        clock.set(std::min(clock.now_ns()+ms,target));run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
                    }
                };
                advance_to(O+12*S+12*ms);
                clock.set(end-7'711'596);run.observe_ui(ui(clock.now_ns(),++seq));const auto prior=run.tick();
                check(prior.status.state==ExperimentalRuntimeState::armed&&touch.actual.contacts().size()==2,"measured MOVE setup not active");
                check(clock.now_ns()==end-7'350'696,"measured360900ns MOVE duration not applied");
                clock.set(end-7'091'996);run.observe_ui(ui(clock.now_ns(),++seq));
                clock.jump_on_read(jump_read,end+27'004);const auto result=run.tick();
                if(result.status.state!=ExperimentalRuntimeState::completed)
                    throw std::runtime_error("UP crossed during revision: read="+std::to_string(jump_read)+
                        " state="+std::to_string(static_cast<int>(result.status.state))+" reason="+result.status.reason+
                        " deferred="+std::to_string(result.status.overdue_tail_revisions_deferred));
                check(result.status.state==ExperimentalRuntimeState::completed,"UP crossed during revision incorrectly cancelled whole song");
                check(result.status.completed_contacts==2&&touch.actual.contacts().empty(),"crossed-UP tail not released");
                deferred+=result.status.overdue_tail_revisions_deferred;
                check(touch.actual.receipts().size()==full.events.size(),"crossed-UP duplicated or omitted source step");
                for(std::size_t i=0;i<full.events.size();++i)
                    check(touch.actual.receipts()[i].command.scheduled_ns==O+full.events[i].song_ns,"crossed-UP timestamp changed to now");
            }
            check(deferred>0,"boundary fixture did not exercise rejected-revision deferral");
        });
        group("past UP revision deferral never extends20ms lateness",[]{
            ReadJumpClock clock;pas::FakeTouchBackend touch(clock);ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;
            ExperimentalContactRuntime run(compiled({contact(1)}),clock,touch,anchor(),options);
            clock.set(anchor().received_host_ns);std::uint64_t seq=1;run.observe_ui(ui(clock.now_ns(),seq));check(run.arm(),"late-tail arm");
            while(clock.now_ns()<O+12*S) {
                clock.set(std::min(clock.now_ns()+ms,O+12*S));run.observe_ui(ui(clock.now_ns(),++seq));run.tick();
            }
            check(touch.contacts().size()==1,"late-tail setup missing DOWN");
            const auto up=O+12*S+16*ms;clock.set(up-ms);run.observe_ui(ui(clock.now_ns(),++seq));
            clock.jump_on_read(4,up+20*ms+1);const auto result=run.tick();
            check(result.status.state==ExperimentalRuntimeState::stopped&&touch.contacts().empty(),"over20ms rejected revision retained contact");
            check(result.status.overdue_tail_revisions_deferred==0&&touch.receipts().size()==1,"late tail was deferred or dispatched past20ms");
        });
        group("immutable prefix revisions and exact phase under actual UI updates",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            const auto full=compiled({contact(17,12*S,12*S+400*ms,8),contact(4,12*S+400*ms,12*S+416*ms)});
            ExperimentalContactRuntime run(full,clock,touch,anchor(-80*ms));arm(run,clock);std::uint64_t seq=1;
            const auto result=advance(run,clock,O+12*S+416*ms-80*ms,seq);
            check(result.status.state==ExperimentalRuntimeState::completed,"long contact did not complete across revisions");
            check(result.status.completed_contacts==2&&touch.contacts().empty(),"lost completion/release");
            check(touch.receipts().size()==full.events.size(),"step duplicated or omitted");
            for(std::size_t i=0;i<full.events.size();++i) {
                const auto& receipt=touch.receipts()[i];const auto& expected=full.events[i];
                check(receipt.command.scheduled_ns==O-80*ms+expected.song_ns,"arrival origin/phase deadline changed");
                const auto pixel=pixel_point(expected.point,1280,720);
                check(receipt.command.x==pixel.first&&receipt.command.y==pixel.second,"normalized point not raster mapped");
                check(receipt.command.phase==(expected.phase==Phase::down?pas::Phase::down:expected.phase==Phase::move?pas::Phase::move:pas::Phase::up),"tie UP-before-DOWN lost");
            }
            const auto pointer=touch.receipts()[0].command.contact_id;
            for(std::size_t i=0;i<10;++i)check(touch.receipts()[i].command.contact_id==pointer,"held pointer reassigned during revision");
            check(run.status().release_required&&run.status().release_rpc_ok,"completion cleanup evidence not distinguished");
            check(!run.arm()&&!run.next_wake_ns(),"completed runtime rearmed");
        });
        group("same timestamp UP before DOWN and two-contact capacity",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            const auto p=compiled({contact(1,12*S,12*S+40*ms),contact(2,12*S,12*S+20*ms),contact(3,12*S+20*ms,12*S+60*ms)});
            ExperimentalContactRuntime run(p,clock,touch,anchor());arm(run,clock);std::uint64_t seq=1;
            advance(run,clock,O+12*S+60*ms,seq);
            check(run.status().state==ExperimentalRuntimeState::completed,"peak2 trial failed");
            const auto& receipts=touch.receipts();check(receipts.size()==6,"peak2 action count");
            check(receipts[2].command.phase==pas::Phase::up&&receipts[3].command.phase==pas::Phase::down,"simultaneous release ordering changed");
            check(touch.contacts().empty(),"peak2 not released");
        });
        group("old permission expiry precedes replacement and status never renews",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);ExperimentalContactRuntime run(compiled({contact(1)}),clock,touch,anchor());
            arm(run,clock);const auto first=clock.now_ns();clock.set(first+100*ms);
            const auto s=run.observe_ui(ui(clock.now_ns(),2));check(s.state==ExperimentalRuntimeState::stopped,"new UI erased old expiry gap");
            check(!run.arm(),"expired trial rearmed");
            pas::FakeClock c2;pas::FakeTouchBackend t2(c2);ExperimentalContactRuntime poll(compiled({contact(1)}),c2,t2,anchor());arm(poll,c2);
            c2.set(first+99*ms);check(poll.status().state==ExperimentalRuntimeState::armed,"99ms receipt rejected");
            c2.set(first+100*ms);check(poll.status().state==ExperimentalRuntimeState::stopped,"status manufactured new receipt");
        });
        group("explicit500ms UI watchdog retains actual receipt and exact boundary",[]{
            ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            ExperimentalContactRuntime run(compiled({contact(1,12*S,14*S)}),clock,touch,anchor(),options);arm(run,clock);
            const auto first=clock.now_ns();
            clock.set(first+499*ms);const auto before=run.status();
            check(before.state==ExperimentalRuntimeState::armed&&before.ui_receipt_max_age_ns==500*ms&&
                before.anchor_max_age_ns==100*ms&&before.max_step_lateness_ns==20*ms,"watchdog option changed timing acquisition contract");
            // Owner has seen only unknown frames: it does not deliver forged
            // playing observations. Polling leaves the original receipt intact.
            clock.set(first+500*ms);const auto expired=run.status();
            check(expired.state==ExperimentalRuntimeState::stopped&&expired.reason=="actual_UI_receipt_expired","500ms exact UI boundary did not expire");
            run.observe_ui(ui(clock.now_ns(),2));check(!run.arm()&&touch.receipts().empty(),"expired UI replaced or rearmed");

            pas::FakeClock c2;pas::FakeTouchBackend t2(c2);
            ExperimentalContactRuntime renewed(compiled({contact(1,12*S,14*S)}),c2,t2,anchor(),options);arm(renewed,c2);
            c2.set(first+499*ms);const auto actual=ui(c2.now_ns(),2);
            check(renewed.observe_ui(actual).state==ExperimentalRuntimeState::armed,"distinct actual playing receipt not accepted");
            c2.set(actual.received_host_ns+499*ms);check(renewed.status().state==ExperimentalRuntimeState::armed,"new actual receipt not used");
            c2.set(actual.received_host_ns+500*ms);
            check(renewed.observe_ui(ui(c2.now_ns(),3)).state==ExperimentalRuntimeState::stopped,"new receipt erased previous500ms expiry gap");
        });
        group("first-note11point8 anchor permits two actual supports and retains full plan",[]{
            ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;
            auto a=anchor();a.received_host_ns=O+11*S+800*ms;a.chosen_song_interval={11*S+795*ms,11*S+805*ms};
            a.supporting_evidence_ids={"anchor-2","anchor-3"};
            const auto full=compiled({contact(1),contact(2,31*S,31*S+16*ms)},true);
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);ExperimentalContactRuntime run(full,clock,touch,a,options);
            clock.set(a.received_host_ns+99*ms);run.observe_ui(ui(clock.now_ns()));
            check(run.arm(),"fresh11.8s two-support first-note anchor refused");
            check(run.status().selected_contacts==2&&run.status().selected_steps==4&&run.status().full_plan_requires_calibration,
                "first-note watchdog mode filtered full plan or dropped calibration flag");
            check(run.anchor().received_host_ns==a.received_host_ns&&run.anchor().origin_host_ns==O&&
                run.anchor().supporting_evidence_ids==a.supporting_evidence_ids,"first-note provenance or origin rewritten");
            pas::FakeClock c2;pas::FakeTouchBackend t2(c2);ExperimentalContactRuntime old(full,c2,t2,a,options);
            c2.set(a.received_host_ns+100*ms);old.observe_ui(ui(c2.now_ns()));
            check(!old.arm()&&old.status().state==ExperimentalRuntimeState::stopped,"500ms UI watchdog extended100ms anchor acquisition age");
            for(unsigned which=0;which<3;++which) {
                auto bad=a;
                if(which==0)bad.supporting_evidence_ids={"anchor-3"};
                if(which==1)bad.supporting_evidence_ids={"anchor-0","anchor-1","anchor-2","anchor-3"};
                if(which==2){bad.chosen_song_interval={11*S+950*ms,11*S+950*ms+1};bad.received_host_ns=O+11*S+950*ms;}
                rejects([&]{ExperimentalContactRuntime invalid(full,c2,t2,bad,options);},"first-note anchor support/window bound bypassed");
            }
        });
        group("500ms watchdog explicit pause revokes held contact immediately",[]{
            ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            ExperimentalContactRuntime run(compiled({contact(1,12*S,14*S)}),clock,touch,anchor(),options);arm(run,clock);
            std::uint64_t seq=1;advance(run,clock,O+12*S,seq);check(touch.contacts().size()==1,"500ms pause setup not held");
            clock.set(clock.now_ns()+ms);auto paused=ui(clock.now_ns(),++seq);paused.playing=false;
            check(run.observe_ui(paused).state==ExperimentalRuntimeState::stopped&&touch.contacts().empty(),"500ms watchdog delayed explicit pause release");
            check(!run.arm(),"paused500ms experiment rearmed");
        });
        group("UI watchdog and step lateness options are independently bounded",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            for(unsigned which=0;which<4;++which) {
                ExperimentalRuntimeOptions options;
                if(which==0)options.ui_receipt_max_age_ns=100*ms-1;
                if(which==1)options.ui_receipt_max_age_ns=500*ms+1;
                if(which==2)options.max_step_lateness_ns=-1;
                if(which==3)options.max_step_lateness_ns=20*ms+1;
                rejects([&]{ExperimentalContactRuntime invalid(compiled({contact(1)}),clock,touch,anchor(),options);},"unbounded UI/lateness option accepted");
            }
            ExperimentalRuntimeOptions options;options.ui_receipt_max_age_ns=500*ms;options.max_step_lateness_ns=5*ms;
            ExperimentalContactRuntime run(compiled({contact(1,12*S,13*S)}),clock,touch,anchor(),options);arm(run,clock);
            std::uint64_t seq=1;advance(run,clock,O+12*S-20*ms,seq);
            clock.set(O+12*S+6*ms);run.observe_ui(ui(clock.now_ns(),++seq));const auto tick=run.tick();
            check(tick.status.state==ExperimentalRuntimeState::stopped&&touch.receipts().empty(),"500ms permission relaxed explicit5ms step lateness");
        });
        group("receipt sequence epoch geometry and identity cannot be reused",[]{
            for(unsigned which=0;which<7;++which) {
                pas::FakeClock clock;pas::FakeTouchBackend touch(clock);ExperimentalContactRuntime run(compiled({contact(1)}),clock,touch,anchor());arm(run,clock);
                const auto prior=clock.now_ns();clock.set(prior+ms);auto u=ui(clock.now_ns(),2);
                if(which==0)u.sequence=1;if(which==1)u.received_host_ns=prior;if(which==2)u.epoch=2;
                if(which==3)u.generation=2;if(which==4)u.geometry_version=2;if(which==5)u.source_id="another";if(which==6)u.evidence_id="UI-1";
                check(run.observe_ui(u).state==ExperimentalRuntimeState::stopped,"invalid actual UI renewed permission");
                check(touch.receipts().empty()&&!run.arm(),"bad UI dispatched/rearmed");
            }
        });
        group("unknown pause stop and clock rollback release a held contact",[]{
            for(unsigned which=0;which<4;++which) {
                pas::FakeClock clock;pas::FakeTouchBackend touch(clock);ExperimentalContactRuntime run(compiled({contact(1,12*S,13*S)}),clock,touch,anchor());arm(run,clock);
                std::uint64_t seq=1;advance(run,clock,O+12*S,seq);check(touch.contacts().size()==1,"setup did not hold");
                if(which==0){clock.set(clock.now_ns()+ms);auto u=ui(clock.now_ns(),++seq);u.playing=false;run.observe_ui(u);}
                if(which==1){clock.set(clock.now_ns()+100*ms);run.tick();}
                if(which==2){run.request_stop();run.tick();}
                if(which==3){clock.set(clock.now_ns()-1);run.tick();}
                check(run.status().state==ExperimentalRuntimeState::stopped&&touch.contacts().empty(),"invalid live state kept held contact");
                check(!run.arm(),"cancelled runtime resumed");
            }
        });
        group("late steps fail rather than catch-up replay",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);ExperimentalContactRuntime run(compiled({contact(1,12*S,12*S+200*ms,1)}),clock,touch,anchor());arm(run,clock);
            std::uint64_t seq=1;advance(run,clock,O+12*S,seq);advance(run,clock,O+12*S+80*ms,seq);
            clock.set(O+12*S+121*ms);run.observe_ui(ui(clock.now_ns(),++seq));auto result=run.tick();
            check(result.status.state==ExperimentalRuntimeState::stopped&&touch.contacts().empty(),"late MOVE did not abort/release");
            check(touch.receipts().size()==1&&touch.receipts().front().command.phase==pas::Phase::down&&
                result.status.release.requested_ids==std::vector<int>{0},"late MOVE caught up or cleanup was not requested");
        });
        group("failed and throwing injection retain unknown cleanup duty",[]{
            for(bool throwing:{false,true}) {
                pas::FakeClock clock;FaultTouch touch(clock);ExperimentalContactRuntime run(compiled({contact(1)}),clock,touch,anchor());arm(run,clock);
                touch.fail_inject=!throwing;touch.throw_inject=throwing;std::uint64_t seq=1;
                const auto result=advance(run,clock,O+12*S,seq);
                check(result.status.state==ExperimentalRuntimeState::stopped&&result.status.release_required,"unknown input treated as complete");
                check(touch.actual.contacts().empty()&&touch.releases>0,"unknown injection skipped cleanup");
                check(!run.arm(),"input fault rearmed");
            }
        });
        group("failed or throwing release never becomes known",[]{
            for(bool throwing:{false,true}) {
                pas::FakeClock clock;FaultTouch touch(clock);ExperimentalContactRuntime run(compiled({contact(1,12*S,13*S)}),clock,touch,anchor());arm(run,clock);
                std::uint64_t seq=1;advance(run,clock,O+12*S,seq);touch.fail_release=!throwing;touch.throw_release=throwing;
                run.request_stop();const auto result=run.tick();
                check(result.status.state==ExperimentalRuntimeState::stopped&&!result.status.release_rpc_ok,"release failure became zero");
                check(touch.actual.contacts().size()==1&&!run.arm(),"unknown release resumed/forgot effect");
            }
        });
        group("prepared immutable plans preserve provenance freshness and one-shot transfer",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
            auto plan=compiled({contact(1)});auto bad=plan;bad.metadata.cache_sha256[0]='0';
            rejects([&]{(void)ExperimentalContactRuntime::prepare(bad);},"bad fingerprint prepared");
            auto prepared=ExperimentalContactRuntime::prepare(plan);
            check(touch.receipts().empty(),"offline preparation performed input");
            clock.set(anchor().received_host_ns);
            ExperimentalContactRuntime run(std::move(prepared),clock,touch,anchor());
            run.observe_ui(ui(clock.now_ns()));check(run.arm(),"prepared plan did not arm");
            check(run.status().selected_contacts==1&&run.status().selected_steps==2,"prepared plan lost source steps");
            rejects([&]{ExperimentalContactRuntime reused(std::move(prepared),clock,touch,anchor());},"moved prepared plan reused");
            auto stale=ExperimentalContactRuntime::prepare(plan);clock.set(anchor().received_host_ns+100*ms);
            ExperimentalContactRuntime expired(std::move(stale),clock,touch,anchor());
            expired.observe_ui(ui(clock.now_ns()));check(!expired.arm(),"prepared plan renewed stale anchor");
        });
        group("explicit anchor provenance and arm freshness",[]{
            pas::FakeClock clock;pas::FakeTouchBackend touch(clock);auto a=anchor();a.origin_host_ns++;
            rejects([&]{ExperimentalContactRuntime run(compiled({contact(1)}),clock,touch,a);},"anchor origin not linked to midpoint");
            a=anchor();a.supporting_evidence_ids[0]=a.supporting_evidence_ids[1];
            rejects([&]{ExperimentalContactRuntime run(compiled({contact(1)}),clock,touch,a);},"repeated support IDs accepted");
            a=anchor(100*ms+1);rejects([&]{ExperimentalContactRuntime run(compiled({contact(1)}),clock,touch,a);},"phase exceeded explicit budget");
            ExperimentalContactRuntime stale(compiled({contact(1)}),clock,touch,anchor());clock.set(anchor().received_host_ns+100*ms);
            stale.observe_ui(ui(clock.now_ns()));check(!stale.arm(),"old anchor renewed by new UI");
        });
        std::cout<<checks<<" experimental runtime checks passed; no device, SyncLease or timing qualification.\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
