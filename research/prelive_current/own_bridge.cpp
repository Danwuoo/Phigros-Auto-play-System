#include "own_bridge.hpp"
#include "../bvi_owner_cooperative/extract.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <chrono>

namespace pas::current_execution {
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
namespace {
ReleaseReport bounded_report(const ReleaseReport& input,bool& malformed) {
    ReleaseReport r;r.start_ns=input.start_ns;r.return_ns=input.return_ns;
    const auto copy=[&](const auto& source,auto& destination) {
        malformed|=source.size()>5;
        destination.assign(source.begin(),source.begin()+std::min<std::size_t>(source.size(),5));
        for(const auto id:destination)if(id<0||id>=5)malformed=true;
    };
    copy(input.requested_ids,r.requested_ids);copy(input.failed_ids,r.failed_ids);copy(input.unknown_ids,r.unknown_ids);
    return r;
}
}
bool ReceiptStream::can_inject() {
    if(overflow_||uncertain_release_||command_uncertain_||count_==events_.size()) {overflow_|=count_==events_.size();return false;}
    return true;
}
std::size_t ReceiptStream::active_count() const {return std::count(active_.begin(),active_.end(),true);}
void ReceiptStream::record_injection(const TouchCommand& command,const TouchReceipt& receipt) {
    command_uncertain_|=!receipt.success||!same_command(command,receipt.command)||command.contact_id<0||command.contact_id>=5;
    if(command.contact_id>=0&&command.contact_id<5) {
        auto& active=active_[static_cast<std::size_t>(command.contact_id)];
        if(command.phase==Phase::down)active=true;
        if(command.phase==Phase::up&&receipt.success&&same_command(command,receipt.command))active=false;
    }
    if(count_==events_.size()) {overflow_=true;return;}
    TouchReceipt r{receipt.command,receipt.injection_start_ns,receipt.injection_return_ns,receipt.success,receipt.reason.substr(0,512)};
    overflow_|=receipt.reason.size()>512;
    events_[count_++]={false,command,std::move(r),{},true};
}
void ReceiptStream::record_release(const ReleaseReport& report,bool dispatched) {
    bool malformed=false;auto r=bounded_report(report,malformed);
    overflow_|=malformed;
    const bool uncertain=malformed||!r.failed_ids.empty()||!r.unknown_ids.empty();
    if(uncertain&&!uncertain_release_)uncertain_release_=r;
    for(const auto id:r.requested_ids)if(id>=0&&id<5&&
       std::find(r.failed_ids.begin(),r.failed_ids.end(),id)==r.failed_ids.end()&&
       std::find(r.unknown_ids.begin(),r.unknown_ids.end(),id)==r.unknown_ids.end()&&!malformed)
        active_[static_cast<std::size_t>(id)]=false;
    if(count_==events_.size()) {overflow_=true;return;}
    events_[count_++]={true,{},{},std::move(r),dispatched};
}
std::vector<BackendEvent> ReceiptStream::take_events() {
    std::vector<BackendEvent> events;events.reserve(count_);
    for(std::size_t i=0;i<count_;++i) {events.push_back(std::move(events_[i]));events_[i]={};}
    count_=0;return events;
}
bool ReceiptStream::reset_after_verified_round() {
    if(!healthy()||active_count()!=0||count_!=0)return false;
    return true;
}
TouchReceipt RecordingBackend::inject(const TouchCommand& c) {
    if(!stream_.can_inject())return {c,clock_.now_ns(),clock_.now_ns(),false,"receipt_capacity_or_uncertain_release"};
    TouchReceipt r;
    try {r=backend_.inject(c);}
    catch(...) {r={c,clock_.now_ns(),clock_.now_ns(),false,"backend_injection_exception_unknown"};stream_.record_injection(c,r);throw;}
    stream_.record_injection(c,r);return r;
}
ReleaseReport RecordingBackend::release_all() {
    if(stream_.first_uncertain_release()) {
        const auto r=*stream_.first_uncertain_release();stream_.record_release(r,false);return r;
    }
    ReleaseReport r;
    try {r=backend_.release_all();}
    catch(...) {r.start_ns=r.return_ns=clock_.now_ns();for(int i=0;i<5;++i){r.requested_ids.push_back(i);r.unknown_ids.push_back(i);}stream_.record_release(r);throw;}
    stream_.record_release(r);return r;
}
nlohmann::json backend_event_json(const BackendEvent& e) {
    if(e.release)return {{"event","candidate_backend_release"},{"dispatched",e.dispatched},
        {"requested_ids",e.report.requested_ids},{"failed_ids",e.report.failed_ids},{"unknown_ids",e.report.unknown_ids},
        {"start_ns",e.report.start_ns},{"return_ns",e.report.return_ns},{"effect_verified",false}};
    const auto command=[](const TouchCommand& c){return nlohmann::json{{"intent",c.intent_id},{"contact",c.contact_id},
        {"phase",int(c.phase)},{"x",c.x},{"y",c.y},{"scheduled",c.scheduled_ns},{"source_frame",c.source_frame_sequence}};};
    return {{"event","candidate_backend_receipt"},{"issued",command(e.issued)},{"returned",command(e.receipt.command)},
        {"injection_start_ns",e.receipt.injection_start_ns},{"injection_return_ns",e.receipt.injection_return_ns},
        {"success",e.receipt.success},{"reason",e.receipt.reason},{"dispatched",e.dispatched},{"effect_verified",false}};
}
Prepared prepare(const Frame& f,const CandidateBatch& batch,const DecisionSnapshot& scene,Nanoseconds now) {
    Prepared p;
    auto reject=[&](std::string_view reason){p.reason=reason;return p;};
    const SceneContext current{f.epoch,f.generation,f.geometry_version,f.sequence,
        f.capture_complete_ns,f.width,f.height,f.source_rotation};
    if(!f.source_valid||!batch.source_valid||!batch.capacity_valid||!scene.capacity_valid||
       !f.epoch||!f.generation||!f.geometry_version||!f.sequence||
       f.width!=1280||f.height!=720||f.stride!=3840||f.source_rotation!=1||f.rgb.size()!=1280*720*3)
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
    if(!receipts_.can_inject()) {r.success=false;r.reason="receipt_capacity";return r;}
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
    if(delay_ms<0||delay_ms>50)throw std::invalid_argument("offline delay bound");
    if(delay_ms)std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    r.injection_return_ns=clock_.now_ns();
    receipts_.record_injection(command,r);return r;
}
ReleaseReport ReplayBackend::release_all() {
    if(receipts_.first_uncertain_release()) {
        const auto r=*receipts_.first_uncertain_release();receipts_.record_release(r,false);return r;
    }
    ReleaseReport r;r.start_ns=r.return_ns=clock_.now_ns();
    for(int c=0;c<5;++c)if(active_[c]) {
        r.requested_ids.push_back(c);
        if(failure==Failure::release_unknown)r.unknown_ids.push_back(c);
        else active_[c]=false;
    }
    receipts_.record_release(r);
    return r;
}
std::vector<BackendEvent> ReplayBackend::take_events() {
    return receipts_.take_events();
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
            if(plan.note_id<=retired_note_floor_)return invalid("retired_note_does_not_revive");
            if(count_==attachments_.size()) {
                auto old=std::find_if(attachments_.begin(),attachments_.end(),[&](const auto& row){
                    return (row.state==Execution::completed||row.state==Execution::cancelled)&&
                        !row.cursor&&row.deadline<plan.valid_until_ns-100'000'000;
                });
                if(old==attachments_.end())return invalid("attachment_capacity");
                retired_note_floor_=std::max(retired_note_floor_,old->note);
                std::move(old+1,attachments_.end(),old);--count_;
            }
            if(find(plan.note_id))return invalid("note_identity_rebirth_unproved");
            a=&attachments_[count_++];*a=Attachment{};a->note=plan.note_id;a->intent=plan.intent_id;
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
bool ExecutionLedger::finalize(const ReleaseReport& r){
 if(!r.failed_ids.empty()||!r.unknown_ids.empty()){valid_=false;reason_="final_release_not_verified";return false;}
 for(std::size_t i=0;i<count_;++i){auto&a=attachments_[i];if(a.state==Execution::active&&std::find(r.requested_ids.begin(),r.requested_ids.end(),a.contact)==r.requested_ids.end()){valid_=false;reason_="final_release_attachment_missing";return false;}if(a.state==Execution::active||a.state==Execution::pending)a.state=Execution::cancelled;a.cursor.reset();}
 return valid_;
}
bvi::Guard ExecutionLedger::guard(std::uint64_t note,int query,Nanoseconds now,Nanoseconds gate_deadline,
                                  Nanoseconds plan_deadline,int free_contacts) const {
    bvi::Guard g{now,gate_deadline,plan_deadline,0,"never_executed",0,0,query,free_contacts,false};
    const auto* a=find(note);
    if(!valid_){g.execution="invalid_ledger";return g;}
    if(!a){if(retired(note)){g.execution="retired_identity_no_rebirth";g.prefix=2;}return g;}
    g.last_contact=a->last_contact;g.prefix=phase_prefix(a->state,a->cursor);g.contact_id=a->contact;
    g.execution=a->state==Execution::active?"known_down":a->state==Execution::completed?"completed_up":
        a->state==Execution::unknown?"unknown_down":a->state==Execution::pending?"never_executed":"cancelled";
    g.receipt_unknown=a->state==Execution::unknown;
    return g;
}
Evaluation evaluate(bvi::Candidate& candidate,const Frame& f,const CandidateBatch& batch,
                    const DecisionSnapshot& scene,const ExecutionLedger& ledger,Nanoseconds now,
                    const OwnerCheckpoint* checkpoint) {
    Evaluation e;e.input=prepare(f,batch,scene,now);e.filtered=scene;e.filtered.targets.clear();
    if(!e.input.valid||!ledger.valid()) {e.filtered.playing_gate=false;e.filtered.capacity_valid=false;return e;}
    if(!ledger.context_matches(scene.context)) {
        e.input.reason="execution_context_mismatch";
        e.filtered.playing_gate=false;e.filtered.capacity_valid=false;return e;
    }
    bvi::Key key{{f.epoch,f.generation,f.geometry_version,f.source_rotation},f.capture_complete_ns,
        f.pixels_ready_ns,f.sequence,f.source_valid};
    const bvi::View view{f.rgb,f.width,f.height,f.stride,key};
    const std::span<const bvi::Query> queries{e.input.queries.data(),e.input.count};
    const std::span<const bvi::Line> lines{e.input.lines.data(),e.input.line_count};
    e.observation=checkpoint?bvi::owner_cooperative::extract(candidate,view,queries,lines,checkpoint->poll):
        candidate.extract(view,queries,lines);
    if(checkpoint) {
        if(e.observation.reason=="owner checkpoint stopped"||!checkpoint->check()) {
            e.interrupted=true;e.outcome="owner_checkpoint_failed";e.filtered.playing_gate=false;e.filtered.capacity_valid=false;return e;
        }
        const auto resumed=checkpoint->now();
        // Poll may complete/revoke an attachment. Re-read both time and ledger;
        // never commit with the pre-extraction lease or completed identity.
        if(resumed<now||!ledger.valid()) {
            e.interrupted=true;e.outcome=resumed<now?"checkpoint_clock_reversed":"checkpoint_ledger_invalid";
            e.filtered.playing_gate=false;e.filtered.capacity_valid=false;return e;
        }
        if(resumed-f.capture_complete_ns>=100'000'000) {
            // Expiry revokes this observation. A healthy owner remains able to
            // process a later fresh frame; it does not retry an expired intent.
            e.source_expired=true;e.outcome="source_expired_after_extract";
            e.filtered.playing_gate=false;e.filtered.capacity_valid=false;return e;
        }
        now=resumed;
    }
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
} // namespace pas::current_execution
