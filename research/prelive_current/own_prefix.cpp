#include "own_bridge.hpp"
#include "pas/game_session.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>

using namespace pas;
using namespace pas::current_execution;
using J=nlohmann::json;
constexpr Nanoseconds ms=1'000'000;
namespace {
struct Recorder {
    J checks=J::array();int failed=0;
    void check(std::string id,bool pass) {checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}
};
DecisionSnapshot scene(std::uint64_t frame,Nanoseconds time) {
    DecisionSnapshot s;s.sequence=frame;s.context={1,1,1,frame,time,1280,720,1};
    s.ui=GameUi::playing;s.playing_gate=true;
    LineCandidate l;l.center={640,500};l.tangent={1,0};l.length=1280;
    l.confidence=1;l.track_id=7;l.observed_ns=time;s.lines={l};return s;
}
GameTarget hold(std::uint64_t note,Nanoseconds time,double x=400) {
    GameTarget t;t.note_id=note;t.revision=1;t.evidence_ns=time;t.expires_ns=time+100*ms;
    t.crossing_ns=time+10*ms;t.uncertainty_ns=2*ms;t.samples=4;t.reason="prediction_observe_only";
    t.line_id=7;t.hit={x,500};t.note.center=t.hit;t.note.kind=NoteKind::hold;
    t.note.width=100;t.note.height=300;t.note.tail=Vec2{x,200};t.note.rails_geometry=true;
    return t;
}
struct Owner {
    FakeClock clock;ReplayBackend backend{clock};SessionGameOwner owner{clock,backend,5,{3,0,30*ms}};
    ExecutionLedger ledger;
    Owner(){owner.start(1);}
    bool sync() {
        const auto plans=owner.owner()->take_accepted_plans();const auto events=backend.take_events();
        owner.owner()->take_coverage_updates();owner.owner()->take_plan_cancellations();
        return ledger.observe(plans,events,*owner.scheduler());
    }
    void accept(const DecisionSnapshot& s){owner.accept(s,true);}
    void poll(){owner.poll();}
};
Frame pixels(const DecisionSnapshot& s) {
    Frame f;f.epoch=1;f.generation=1;f.geometry_version=1;f.sequence=s.context.frame;
    f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;
    f.capture_complete_ns=f.pixels_ready_ns=s.context.capture_ns;f.rgb.resize(1280*720*3);
    return f;
}
CandidateBatch batch(const DecisionSnapshot& s) {
    CandidateBatch b;b.context=s.context;b.playing_gate=true;b.lines=s.lines;
    for(const auto& t:s.targets){TrackingCandidate c;c.candidate_id=b.candidates.size()+1;
        c.note=t.note;c.head_visible=!t.note.held_body_patch;c.quality=ObservationQuality::strong_current;
        b.candidates.push_back(c);}
    return b;
}
void rect(Frame& f,int x,int y,int w,int h,std::array<std::uint8_t,3> color) {
    for(int py=y;py<y+h;++py)for(int px=x;px<x+w;++px)
        std::copy(color.begin(),color.end(),f.rgb.begin()+std::size_t(py)*f.stride+px*3);
}
void pixel_entry(Recorder& r,bool unknown) {
    Owner o;auto candidate=std::make_unique<bvi::Candidate>();
    if(unknown)o.backend.failure=ReplayBackend::Failure::unknown_down;
    for(int i=0;i<3;++i) {
        o.clock.set((10+i*15)*ms);auto s=scene(i+1,o.clock.now_ns());
        auto t=hold(42,o.clock.now_ns(),398+i);t.note.kind=NoteKind::tap;t.note.height=8;t.note.tail.reset();
        t.crossing_ns=o.clock.now_ns();s.targets={t};auto f=pixels(s);auto b=batch(s);
        rect(f,0,499,1280,3,{255,255,255});rect(f,348+i,497,101,7,{40,190,255});
        const auto evaluated=evaluate(*candidate,f,b,s,o.ledger,o.clock.now_ns());
        if(i<2)r.check(std::string(unknown?"unknown":"success")+"_no_early_down_"+std::to_string(i),evaluated.allowed==0);
        if(i==2)r.check(std::string(unknown?"unknown":"success")+"_own_pixel_entry_opportunity",evaluated.allowed==1);
        o.accept(evaluated.filtered);o.poll();r.check("pixel_entry_receipts_verified_"+std::to_string(i)+std::to_string(unknown),o.sync());
    }
    const auto* a=o.ledger.find(42);
    r.check(unknown?"unknown_from_own_failed_down":"known_down_from_own_successful_receipt",
        a&&a->state==(unknown?Execution::unknown:Execution::active)&&a->contact==0&&o.backend.downs==1);
    if(unknown) {
        const auto g=o.ledger.guard(42,0,o.clock.now_ns(),200*ms,200*ms,4);
        r.check("unknown_down_null_cursor_is_not_zero",a&&!a->cursor&&g.prefix==1&&g.receipt_unknown);
        auto s=scene(9,50*ms);s.targets={hold(42,50*ms)};o.clock.set(50*ms);o.accept(s);o.poll();o.sync();
        r.check("unknown_down_never_retried",o.backend.downs==1&&o.backend.active_count()==0&&o.owner.scheduler()->fault()=="input_result_unknown");
    } else {
        o.clock.set(58*ms);o.poll();r.check("up_receipt_verified",o.sync());a=o.ledger.find(42);
        r.check("completed_up_null_cursor_retained",a&&a->state==Execution::completed&&!a->cursor&&o.backend.ups==1);
        o.clock.set(60*ms);auto s=scene(9,60*ms);auto t=hold(42,60*ms);t.note.kind=NoteKind::tap;
        t.note.tail.reset();t.note.height=8;t.crossing_ns=60*ms;s.targets={t};o.accept(s);o.poll();o.sync();
        r.check("completed_does_not_resurrect",o.backend.downs==1);
    }
}
void prefix_tests(Recorder& r) {
    Owner o;auto s=scene(1,0);
    for(int i=0;i<5;++i){auto h=hold(i+1,0,100+i*220);h.note.width=120;h.note.head_on_line=true;s.targets.push_back(h);}
    o.accept(s);r.check("pending_is_verified_zero",o.sync()&&o.ledger.find(1)->cursor==0);
    o.clock.set(10*ms);o.poll();r.check("five_actual_downs",o.sync()&&o.backend.downs==5&&o.backend.active_count()==5);
    const int finger=o.ledger.find(1)->contact;
    std::uint64_t prior=1;bool monotonic=true,compacted=false,bounded=true;
    std::optional<ContactPlan> current_plan;
    for(int k=1;k<=20;++k) {
        o.clock.set((10+k*10)*ms);
        // Preserve the formal dispatch-before-revision ordering for a due
        // command before replacing the remaining plan suffix.
        o.poll();r.check("prior_move_dispatch_verified_"+std::to_string(k),o.sync());
        s=scene(k+1,o.clock.now_ns());
        const double angle=k*.02;
        s.lines[0].tangent={std::cos(angle),std::sin(angle)};
        for(int i=0;i<5;++i) {
            auto t=hold(i+1,o.clock.now_ns(),100+i*220+k);
            t.revision=k+1;t.note.width=120;t.note.head_on_line=true;t.note.tangent=s.lines[0].tangent;
            const double along=100+i*220+k-640;
            t.hit={640+along*t.note.tangent.x,500+along*t.note.tangent.y};t.note.center=t.hit;
            const Vec2 n{-t.note.tangent.y,t.note.tangent.x};
            t.note.tail=Vec2{t.hit.x-n.x*300,t.hit.y-n.y*300};s.targets.push_back(t);
        }
        o.accept(s);o.poll();const auto plans=o.owner.owner()->take_accepted_plans();
        for(const auto& p:plans){compacted|=p.prefix_offset>0;bounded&=p.steps.size()<=4;if(p.note_id==1)current_plan=p;}
        const auto events=o.backend.take_events();o.owner.owner()->take_coverage_updates();o.owner.owner()->take_plan_cancellations();
        r.check("rotating_hold_prefix_receipts_"+std::to_string(k),o.ledger.observe(plans,events,*o.owner.scheduler()));
        const auto* a=o.ledger.find(1);monotonic&=a&&a->cursor&&*a->cursor>=prior&&a->contact==finger;
        if(a&&a->cursor)prior=*a->cursor;
    }
    r.check("rotating_hold_same_contact_no_down_replay",monotonic&&o.backend.downs==5&&o.backend.moves>0);
    r.check("full_prefix_compacts_without_narrowing",compacted&&bounded&&prior>10);
    r.check("truncated_prefix_has_actual_plan",current_plan.has_value());
    if(current_plan) {
        auto bad=*current_plan;++bad.revision;bad.prefix_offset=prior;
        r.check("truncated_prefix_revision_rejected",!o.owner.scheduler()->submit(bad));
        r.check("truncated_prefix_preserves_contact_and_cursor",o.owner.scheduler()->executed_steps(bad.intent_id)==prior&&
            o.backend.active_count()==5&&o.ledger.find(1)->contact==finger);
    }
    const auto actual_phase=o.ledger.find(1)->state;
    r.check("uint64_cursor_projection_above_int_max",phase_prefix(actual_phase,std::uint64_t(std::numeric_limits<int>::max())+100)==1);
    r.check("uint64_cursor_projection_at_max",phase_prefix(actual_phase,std::numeric_limits<std::uint64_t>::max())==1);
    r.check("null_cursor_not_zero_or_known_down",phase_prefix(actual_phase,std::nullopt)==0);
    o.clock.set(o.clock.now_ns()+100*ms);o.poll();r.check("evidence_deadline_releases_five",o.sync()&&o.backend.active_count()==0);
    r.check("expiry_retains_terminal_identity",o.ledger.find(1)->state==Execution::completed||o.ledger.find(1)->state==Execution::cancelled);
}
void failure_tests(Recorder& r) {
    for(const auto failure:{ReplayBackend::Failure::unknown_move,ReplayBackend::Failure::unknown_up}) {
        const bool move=failure==ReplayBackend::Failure::unknown_move;
        const std::string id=move?"unknown_move":"unknown_up";
        Owner o;auto s=scene(1,0);auto t=hold(71,0);
        t.note.width=120;t.note.head_on_line=true;
        if(!move){t.note.kind=NoteKind::tap;t.note.height=8;t.note.tail.reset();}
        s.targets={t};o.accept(s);o.sync();o.clock.set(10*ms);o.poll();
        r.check(id+"_starts_with_own_down",o.sync()&&o.ledger.find(71)&&
            o.ledger.find(71)->state==Execution::active&&o.backend.downs==1);
        o.backend.failure=failure;
        if(move) {
            o.clock.set(20*ms);s=scene(2,20*ms);t=hold(71,20*ms,420);
            t.revision=2;t.note.width=120;t.note.head_on_line=true;s.targets={t};o.accept(s);
        }
        const auto due=o.owner.scheduler()->next_due_ns();
        r.check(id+"_uses_actual_scheduler_due",due.has_value());
        if(due)o.clock.set(*due);
        o.poll();r.check(id+"_receipt_prefix_verified",o.sync());
        const auto* a=o.ledger.find(71);
        const auto g=o.ledger.guard(71,0,o.clock.now_ns(),200*ms,200*ms,5);
        r.check(id+"_unknown_is_not_completed_or_zero",a&&a->state==Execution::unknown&&
            !a->cursor&&g.receipt_unknown&&g.prefix==1);
        const auto release=o.owner.scheduler()->last_release();
        r.check(id+"_stops_and_verifies_release",o.owner.scheduler()->fault()=="input_result_unknown"&&
            release.requested_ids.size()==1&&release.unknown_ids.empty()&&release.failed_ids.empty()&&
            o.backend.active_count()==0&&o.backend.failed==1);
        const auto moves=o.backend.moves,ups=o.backend.ups;
        o.clock.set(o.clock.now_ns()+ms);s=scene(3,o.clock.now_ns());
        t=hold(71,o.clock.now_ns());s.targets={t};o.accept(s);o.poll();o.sync();
        r.check(id+"_never_retries_or_rebirths",o.backend.downs==1&&o.backend.moves==moves&&o.backend.ups==ups);
    }
    {
        Owner o;o.backend.failure=ReplayBackend::Failure::wrong_contact;auto s=scene(1,0);s.targets={hold(1,0)};
        o.accept(s);o.sync();o.clock.set(10*ms);o.poll();r.check("wrong_contact_receipt_rejected",!o.sync()&&o.ledger.reason()=="receipt_command_mismatch");
        o.owner.scheduler()->cancel("bridge_receipt_mismatch");o.backend.take_events();
        r.check("wrong_contact_stops_and_releases",o.backend.active_count()==0&&o.backend.downs==1);
    }
    {
        Owner o;auto s=scene(1,0);s.targets={hold(1,0)};o.accept(s);o.sync();o.clock.set(10*ms);o.poll();o.sync();
        o.backend.failure=ReplayBackend::Failure::release_unknown;bool threw=false;
        try{o.owner.finish();}catch(const std::exception&){threw=true;}
        r.check("unknown_release_remains_failure",threw&&o.owner.scheduler()->fault()=="release_failed"&&!o.sync()&&o.backend.active_count()==1);
    }
    {
        Owner o;auto s=scene(1,0);s.targets={hold(1,0)};o.accept(s);o.sync();
        o.owner.scheduler()->cancel_intent(o.ledger.find(1)->intent);o.sync();
        r.check("cancelled_pending_missing_cursor_not_zero",!o.ledger.find(1)->cursor&&o.ledger.find(1)->state==Execution::cancelled);
        const auto g=o.ledger.guard(1,0,0,100*ms,100*ms,5);
        r.check("cancelled_prefix_cannot_seed_never_executed",g.execution=="cancelled");
    }
}
void geometry_tests(Recorder& r) {
    auto s=scene(1,10*ms);auto t=hold(123,10*ms);t.note.tangent={0,1};t.note.tail=Vec2{700,500};
    t.note.height=17;s.targets={t};auto f=pixels(s);auto b=batch(s);
    const auto p=prepare(f,b,s,10*ms);
    r.check("rotated_depth_comes_from_current_tail",p.valid&&p.count==1&&std::abs(p.queries[0].depth-300)<1e-9);
    r.check("roi_id_is_not_contact_or_note_id",p.candidate_ids[0]==1&&p.note_ids[0]==123);
    b.lines[0].association_valid=false;
    r.check("invalid_association_line_still_masks",prepare(f,b,s,10*ms).line_count==1);
    b.lines.resize(17,b.lines.front());r.check("all_lines_overflow_rejected_without_truncation",!prepare(f,b,s,10*ms).valid);
    b=batch(s);b.context.frame=2;r.check("different_frame_batch_rejected",!prepare(f,b,s,10*ms).valid);
    b=batch(s);b.candidates[0].note.held_body_patch=true;b.candidates[0].head_visible=true;
    const auto body=prepare(f,b,s,10*ms);r.check("body_patch_cannot_be_promoted_to_seen_front",body.valid&&body.count==0&&body.body_patch_denied==1);
    b=batch(s);s.targets.push_back(t);const auto duplicate=prepare(f,b,s,10*ms);
    r.check("ambiguous_target_binding_abstains",duplicate.valid&&duplicate.note_ids[0]==0&&duplicate.unmapped==1);
    f.source_rotation=2;s.context.rotation=2;b.context=s.context;
    r.check("matching_rotation_outside_frozen_profile_rejected",!prepare(f,b,s,10*ms).valid);
    f.source_rotation=1;s.context.rotation=1;b.context=s.context;
    f.epoch=0;r.check("zero_context_rejected",!prepare(f,b,s,10*ms).valid);
}
void context_tests(Recorder& r) {
    for(int field=0;field<4;++field) {
        const auto id=std::string(field==0?"epoch":field==1?"generation":field==2?"geometry":"rotation");
        Owner o;auto s=scene(1,0);s.targets={hold(71,0)};o.accept(s);o.sync();
        o.clock.set(10*ms);o.poll();
        r.check(id+"_change_starts_with_own_down",o.sync()&&o.backend.active_count()==1);
        o.clock.set(20*ms);s=scene(2,20*ms);s.targets={hold(71,20*ms)};
        if(field==0)++s.context.epoch;else if(field==1)++s.context.generation;
        else if(field==2)++s.context.geometry;else ++s.context.rotation;
        auto f=pixels(s);f.epoch=s.context.epoch;f.generation=s.context.generation;f.geometry_version=s.context.geometry;
        f.source_rotation=s.context.rotation;
        auto candidate=std::make_unique<bvi::Candidate>();
        auto evaluated=evaluate(*candidate,f,batch(s),s,o.ledger,o.clock.now_ns());
        r.check(id+"_change_cannot_reuse_known_down",
            (field==3?(!evaluated.input.valid&&evaluated.input.reason=="frame_source_or_capacity"):
             (evaluated.input.valid&&evaluated.input.reason=="execution_context_mismatch"))&&
            !evaluated.filtered.playing_gate&&evaluated.allowed==0);
        o.accept(evaluated.filtered);o.poll();
        r.check(id+"_change_releases_actual_contact",o.sync()&&o.backend.active_count()==0&&
            o.backend.downs==1&&o.ledger.find(71)->state==Execution::cancelled);
        evaluated=evaluate(*candidate,f,batch(s),s,o.ledger,o.clock.now_ns());
        r.check(id+(field==3?"_change_requires_frozen_profile":"_change_requires_new_verified_owner_context"),
            !evaluated.filtered.playing_gate&&evaluated.allowed==0);
    }
}
}
int main(int argc,char** argv) {try {
    if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh report required");
    Recorder r;geometry_tests(r);pixel_entry(r,false);pixel_entry(r,true);prefix_tests(r);failure_tests(r);context_tests(r);
    J report={{"schema","pas.windows-bvi-current-bridge-prefix.v1"},{"assertions",r.checks.size()},
        {"failed_assertions",r.failed},{"checks",r.checks},{"backend","bounded_offline_fake"},
        {"official_owner_and_scheduler",true},{"known_down_source","own issued commands and returned receipts"},
        {"high_uint64_cursor_tests","conversion controls; not a claim of billions of executed moves"},
        {"device_commands",0},{"physical_human_gold",0}};
    std::ofstream out(argv[1]);out<<report.dump(2)<<'\n';if(!out)return 2;
    std::cout<<"prefix assertions="<<r.checks.size()<<" failed="<<r.failed<<'\n';return r.failed?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
