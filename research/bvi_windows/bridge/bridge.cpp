#include "bridge.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace pas::bvi_offline {
namespace {
bool same(const NoteCandidate& a,const NoteCandidate& b) {
    return a.kind==b.kind&&a.center.x==b.center.x&&a.center.y==b.center.y&&
        a.width==b.width&&a.height==b.height&&a.tangent.x==b.tangent.x&&a.tangent.y==b.tangent.y&&
        bool(a.tail)==bool(b.tail)&&(!a.tail||(a.tail->x==b.tail->x&&a.tail->y==b.tail->y))&&
        a.held_body_patch==b.held_body_patch;
}
bool same_command(const TouchCommand& a,const TouchCommand& b) {
    return a.intent_id==b.intent_id&&a.contact_id==b.contact_id&&a.phase==b.phase&&
        a.x==b.x&&a.y==b.y&&a.scheduled_ns==b.scheduled_ns&&
        a.source_frame_sequence==b.source_frame_sequence;
}
}
Prepared prepare(const Frame& f,const CandidateBatch& batch,const DecisionSnapshot& scene,Nanoseconds now) {
    Prepared p;
    auto reject=[&](std::string_view reason){p.reason=reason;return p;};
    const SceneContext current{f.epoch,f.generation,f.geometry_version,f.sequence,
        f.capture_complete_ns,f.width,f.height,f.source_rotation};
    if(!f.source_valid||!batch.source_valid||!batch.capacity_valid||!scene.capacity_valid||
       !f.epoch||!f.generation||!f.geometry_version||!f.sequence||
       f.width!=1280||f.height!=720||f.stride!=3840||f.rgb.size()!=1280*720*3)
        return reject("frame_source_or_capacity");
    if(batch.context!=current||scene.context!=current||f.capture_complete_ns<0||
       f.capture_complete_ns>std::numeric_limits<Nanoseconds>::max()-100'000'000||
       f.capture_complete_ns>f.pixels_ready_ns||
       f.pixels_ready_ns>now||now-f.capture_complete_ns>=100'000'000)
        return reject("same_current_frame_required");
    if(batch.candidates.size()>128||batch.lines.size()>16||scene.targets.size()>128)
        return reject("complete_batch_capacity");
    // No selected-line subset: association-invalid measured lines still mask
    // and compete. A stale/nonfinite entry invalidates the complete input.
    for(const auto& l:batch.lines) {
        const auto norm=std::hypot(l.tangent.x,l.tangent.y);
        if(l.observed_ns!=f.capture_complete_ns||!std::isfinite(norm)||norm<.999||norm>1.001||
           !std::isfinite(l.center.x)||!std::isfinite(l.center.y)||!std::isfinite(l.length)||l.length<=0)
            return reject("all_lines_must_be_current_measured_geometry");
        p.lines[p.line_count++]={{l.center.x,l.center.y},std::atan2(l.tangent.y,l.tangent.x),l.length,l.track_id};
    }
    for(const auto& c:batch.candidates) {
        const auto& n=c.note;
        if(n.held_body_patch){++p.body_patch_denied;continue;}
        if(!c.head_visible||c.quality==ObservationQuality::rejected||
           (n.kind!=NoteKind::tap&&n.kind!=NoteKind::hold)) {++p.unsupported;continue;}
        const auto norm=std::hypot(n.tangent.x,n.tangent.y);
        if(!std::isfinite(norm)||norm<.999||norm>1.001||!std::isfinite(n.width)||
           n.width<4||n.width>4096||!std::isfinite(n.center.x)||!std::isfinite(n.center.y)||
           n.center.x<0||n.center.x>=f.width||n.center.y<0||n.center.y>=f.height) {
            ++p.unsupported;continue;
        }
        double angle=std::atan2(n.tangent.y,n.tangent.x),depth=1;
        if(n.kind==NoteKind::hold) {
            // Screen-Y height is never substituted for rotated normal depth.
            if(!n.tail){++p.unsupported;continue;}
            const double dx=n.center.x-n.tail->x,dy=n.center.y-n.tail->y;
            depth=-std::sin(angle)*dx+std::cos(angle)*dy;
            if(depth<0){angle+=3.14159265358979323846;depth=-depth;}
            if(!std::isfinite(depth)||depth<=0||depth>4095||
               std::abs(std::cos(angle)*dx+std::sin(angle)*dy)>2) {++p.unsupported;continue;}
        }
        const auto i=p.count++;
        p.queries[i]={{n.center.x,n.center.y},n.width,depth,angle,n.kind==NoteKind::tap};
        p.candidate_ids[i]=c.candidate_id;
        p.target_indices[i]=scene.targets.size();
        std::size_t matches=0;
        for(std::size_t t=0;t<scene.targets.size();++t)if(same(n,scene.targets[t].note)) {
            p.target_indices[i]=t;++matches;
        }
        if(matches==1&&scene.targets[p.target_indices[i]].note_id)
            p.note_ids[i]=scene.targets[p.target_indices[i]].note_id;
        else {p.target_indices[i]=scene.targets.size();++p.unmapped;}
    }
    p.valid=true;p.reason="same_frame_complete_lines";return p;
}
std::size_t ReplayBackend::active_count() const {return std::count(active_.begin(),active_.end(),true);}
TouchReceipt ReplayBackend::inject(const TouchCommand& command) {
    TouchReceipt r{command,clock_.now_ns(),clock_.now_ns(),true,"offline_fake_success"};
    if(event_count_==events_.size()) {overflow_=true;r.success=false;r.reason="receipt_capacity";return r;}
    if(command.contact_id<0||command.contact_id>=5) {r.success=false;r.reason="invalid_contact";}
    else {
        const auto c=static_cast<std::size_t>(command.contact_id);
        if(command.phase==Phase::down) {++downs;active_[c]=true;}
        if(command.phase==Phase::move) {++moves;if(!active_[c])r.success=false;}
        if(command.phase==Phase::up) {
            ++ups;
            // An uncertain Up may have left the contact down. Only the later
            // verified release may clear that worst-case physical slot.
            if(failure!=Failure::unknown_up)active_[c]=false;
        }
        if((failure==Failure::unknown_down&&command.phase==Phase::down)||
           (failure==Failure::unknown_move&&command.phase==Phase::move)||
           (failure==Failure::unknown_up&&command.phase==Phase::up)) {
            r.success=false;r.reason="offline_unknown_injection";
        }
        if(failure==Failure::wrong_contact) r.command.contact_id=(command.contact_id+1)%5;
    }
    if(!r.success)++failed;
    events_[event_count_++]={false,command,r,{}};return r;
}
ReleaseReport ReplayBackend::release_all() {
    ReleaseReport r;r.start_ns=r.return_ns=clock_.now_ns();
    for(int c=0;c<5;++c)if(active_[c]) {
        r.requested_ids.push_back(c);
        if(failure==Failure::release_unknown)r.unknown_ids.push_back(c);
        else active_[c]=false;
    }
    if(event_count_==events_.size())overflow_=true;
    else events_[event_count_++]={true,{},{},r};
    return r;
}
std::vector<BackendEvent> ReplayBackend::take_events() {
    std::vector<BackendEvent> result(events_.begin(),events_.begin()+event_count_);
    event_count_=0;return result;
}
int phase_prefix(Execution state,std::optional<std::uint64_t> cursor) {
    if(state==Execution::completed)return 2; // explicit successful Up receipt
    if(state==Execution::unknown)return 1; // attempted injection receipt
    if(state==Execution::active&&cursor&&*cursor>0)return 1;
    return 0;
}
const Attachment* ExecutionLedger::find(std::uint64_t note) const {
    for(std::size_t i=0;i<count_;++i)if(attachments_[i].note==note)return &attachments_[i];
    return nullptr;
}
std::size_t ExecutionLedger::reservations() const {
    return std::count_if(attachments_.begin(),attachments_.begin()+count_,[](const auto& a){
        return a.state==Execution::active||a.state==Execution::pending||a.state==Execution::unknown;});
}
bool ExecutionLedger::observe(std::span<const ContactPlan> plans,std::span<const BackendEvent> events,
                               const ContactScheduler& scheduler) {
    auto invalid=[&](std::string_view reason){valid_=false;reason_=reason;return false;};
    if(!valid_)return false;
    for(const auto& plan:plans) {
        if(!plan.note_id||!plan.intent_id)return invalid("missing_plan_identity");
        if(!plan.epoch||!plan.generation||!plan.geometry_version)return invalid("missing_plan_context");
        const SceneContext plan_context{plan.epoch,plan.generation,plan.geometry_version};
        if(!context_matches(plan_context))return invalid("attachment_context_changed");
        if(!context_)context_=plan_context;
        Attachment* a=nullptr;
        for(std::size_t i=0;i<count_;++i)if(attachments_[i].intent==plan.intent_id)a=&attachments_[i];
        if(!a) {
            if(count_==attachments_.size())return invalid("attachment_capacity");
            if(find(plan.note_id))return invalid("note_identity_rebirth_unproved");
            a=&attachments_[count_++];a->note=plan.note_id;a->intent=plan.intent_id;
        } else if(a->note!=plan.note_id||plan.revision<=a->revision||
                  a->state==Execution::completed||a->state==Execution::unknown||a->state==Execution::cancelled)
            return invalid("revision_or_completion_identity");
        a->revision=plan.revision;a->prefix_offset=plan.prefix_offset;
        a->deadline=plan.valid_until_ns;
    }
    for(const auto& event:events) {
        if(event.release) {
            if(!event.report.failed_ids.empty()||!event.report.unknown_ids.empty())
                return invalid("release_not_verified");
            for(std::size_t i=0;i<count_;++i) {
                auto& a=attachments_[i];
                if(a.state==Execution::active&&std::find(event.report.requested_ids.begin(),
                   event.report.requested_ids.end(),a.contact)!=event.report.requested_ids.end())
                    a.state=Execution::cancelled;
            }
            continue;
        }
        if(!same_command(event.issued,event.receipt.command))return invalid("receipt_command_mismatch");
        Attachment* a=nullptr;
        for(std::size_t i=0;i<count_;++i)if(attachments_[i].intent==event.issued.intent_id)a=&attachments_[i];
        if(!a)return invalid("receipt_without_accepted_plan");
        if(!event.receipt.success) {a->state=Execution::unknown;a->contact=event.issued.contact_id;continue;}
        if(event.issued.phase==Phase::down) {
            if(a->state!=Execution::pending||a->contact!=-1)return invalid("repeated_down");
            for(std::size_t i=0;i<count_;++i)if(attachments_[i].state==Execution::active&&
                attachments_[i].contact==event.issued.contact_id)return invalid("contact_stolen");
            a->state=Execution::active;a->contact=event.issued.contact_id;
        } else {
            if(a->state!=Execution::active||a->contact!=event.issued.contact_id)
                return invalid("move_up_without_own_down");
            if(event.issued.phase==Phase::up)a->state=Execution::completed;
        }
        a->last_contact=event.receipt.injection_return_ns;
    }
    for(std::size_t i=0;i<count_;++i) {
        auto& a=attachments_[i];a.cursor=scheduler.executed_steps(a.intent);
        if((a.state==Execution::active||a.state==Execution::pending)&&!a.cursor)a.state=Execution::cancelled;
        if(a.state==Execution::active&&*a.cursor==0)return invalid("active_without_execution_cursor");
    }
    return true;
}
bvi::Guard ExecutionLedger::guard(std::uint64_t note,int query,Nanoseconds now,Nanoseconds gate_deadline,
                                  Nanoseconds plan_deadline,int free_contacts) const {
    bvi::Guard g{now,gate_deadline,plan_deadline,0,"never_executed",0,0,query,free_contacts,false};
    const auto* a=find(note);
    if(!valid_){g.execution="invalid_ledger";return g;}
    if(!a)return g;
    g.last_contact=a->last_contact;g.prefix=phase_prefix(a->state,a->cursor);g.contact_id=a->contact;
    g.execution=a->state==Execution::active?"known_down":a->state==Execution::completed?"completed_up":
        a->state==Execution::unknown?"unknown_down":a->state==Execution::pending?"never_executed":"cancelled";
    g.receipt_unknown=a->state==Execution::unknown;
    return g;
}
Evaluation evaluate(bvi::Candidate& candidate,const Frame& f,const CandidateBatch& batch,
                    const DecisionSnapshot& scene,const ExecutionLedger& ledger,Nanoseconds now) {
    Evaluation e;e.input=prepare(f,batch,scene,now);e.filtered=scene;e.filtered.targets.clear();
    if(!e.input.valid||!ledger.valid()) {e.filtered.playing_gate=false;e.filtered.capacity_valid=false;return e;}
    if(!ledger.context_matches(scene.context)) {
        e.input.reason="execution_context_mismatch";
        e.filtered.playing_gate=false;e.filtered.capacity_valid=false;return e;
    }
    bvi::Key key{{f.epoch,f.generation,f.geometry_version,f.source_rotation},f.capture_complete_ns,
        f.pixels_ready_ns,f.sequence,f.source_valid};
    e.observation=candidate.extract({f.rgb,f.width,f.height,f.stride,key},
        {e.input.queries.data(),e.input.count},{e.input.lines.data(),e.input.line_count});
    candidate.relate(e.observation,key,now);
    if(e.observation.invalid||e.observation.context_invalid){e.filtered.playing_gate=false;return e;}
    int remaining=std::max(0,5-static_cast<int>(ledger.reservations()));
    for(std::size_t i=0;i<e.input.count;++i) {
        const auto note=e.input.note_ids[i];if(!note)continue;
        const auto& target=scene.targets[e.input.target_indices[i]];
        const auto& descriptor=e.observation.parts[i];
        const auto line=std::find_if(batch.lines.begin(),batch.lines.end(),[&](const auto& l){
            return l.track_id==descriptor.line_id;});
        // An invalid measured line remains a competitor, never an action source.
        if(line==batch.lines.end()||!line->association_valid||!line->track_id||
           target.line_projection_only||target.line_id!=line->track_id||target.evidence_ns!=f.capture_complete_ns)
            continue;
        const auto g=ledger.guard(note,static_cast<int>(i),now,f.capture_complete_ns+100'000'000,
                                  target.expires_ns,remaining);
        const auto constraints=bvi::constrain(e.observation,g);
        const auto* a=ledger.find(note);
        const bool active=a&&a->state==Execution::active;
        if(constraints.invalid||constraints.release||constraints.completed||
           !(active?constraints.move&&constraints.refresh:constraints.down[i]))continue;
        auto accepted=target;accepted.hit={descriptor.hit.x,descriptor.hit.y};
        e.filtered.targets.push_back(std::move(accepted));++e.allowed;
        if(!a)--remaining;
    }
    return e;
}
} // namespace pas::bvi_offline
