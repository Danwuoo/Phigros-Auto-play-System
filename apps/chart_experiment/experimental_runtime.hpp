#pragma once
#include "pas/core.hpp"
#include "rhythm/contact_compiler.hpp"
#include "rhythm/visual_candidates.hpp"
#include <atomic>
#include <map>
#include <set>
#include <tuple>

namespace rhythm::experimental {
// Declared arrival-rate=1 hypothesis, not a capture clock or SyncLease. The
// supplied point must be receipt - midpoint(chosen_song_interval). Positive
// phase delays every compiled event. Supporting IDs are provenance, not proof
// of the hypothesis: the owner must obtain them from the experimental solver.
struct ExperimentalAnchor {
    Ns origin_host_ns=-1,received_host_ns=-1,explicit_phase_ns=0;
    VisualTimeInterval chosen_song_interval;
    std::uint64_t epoch=0,generation=0,geometry_version=0;
    std::string source_id,source_sha256,clock_id,evidence_id;
    std::vector<std::string> supporting_evidence_ids;
};
struct ExperimentalUiObservation {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    Ns received_host_ns=-1;
    int width=0,height=0;
    bool source_valid=false,playing=false;
    std::string source_id,evidence_id;
};
// Permission watchdog only. This does not qualify song timing or the age of
// captured pixels. The owner may retain the last actual playing observation
// through unknown frames, but must not refresh it with an unknown frame or a
// poll timestamp. Explicit pause/result observations revoke permission at once.
struct ExperimentalRuntimeOptions {
    Ns ui_receipt_max_age_ns=100'000'000;
    Ns max_step_lateness_ns=20'000'000;
};
enum class ExperimentalRuntimeState { waiting,ready,armed,completed,stopped };
struct ExperimentalContactSource {
    std::uint64_t intent_id=0,source_contact_id=0;
    std::string source_note_id,source_json_pointer;
};
struct ExperimentalRuntimeStatus {
    ExperimentalRuntimeState state=ExperimentalRuntimeState::waiting;
    std::string reason="waiting_for_actual_UI",full_cache_sha256;
    bool experimental=true,timing_qualified=false,source_pixel_age_qualified=false;
    bool rate_one_hypothesis=true,full_plan_requires_calibration=false;
    bool selected_projection_free=true,release_required=false,release_rpc_ok=false;
    // release_rpc_ok does NOT establish kernel zero or game callback delivery.
    std::size_t selected_contacts=0,selected_steps=0,completed_contacts=0,pending=0,active=0;
    std::size_t projected_contacts=0;
    std::size_t overdue_tail_revisions_deferred=0;
    Ns plan_end_song_ns=0;
    Ns ui_receipt_max_age_ns=100'000'000,max_step_lateness_ns=20'000'000,anchor_max_age_ns=100'000'000;
    int peak_contacts=0;
    pas::ReleaseReport release;
};
struct ExperimentalRuntimeTick {
    ExperimentalRuntimeStatus status;
    std::vector<pas::TouchReceipt> receipts;
    std::vector<pas::SchedulerNotice> notices;
};

// App-local, one-shot trial. No production bridge/supervisor qualification is
// weakened. Reuses the retained ContactScheduler directly, with its gate and
// plan evidence explicitly meaning UI receipt permission ONLY. All song
// deadlines remain the immutable arrival-origin + phase + song_ns hypothesis.
//
// Whole-song scope: every compiled contact, <=2000 contacts/20000 steps,
// duration <180s, peak<=4, 1280x720 unrotated compiler surface. Bounded D<=250
// projection candidates remain explicitly unqualified. No note is filtered.
// Admit each contact's DOWN within 100ms, retaining its complete unexecuted
// trajectory plus the last executed step when compacting on actual UI renewal.
// No MOVE is omitted behind a temporary prefix boundary: synchronous RPCs may
// advance time inside run_due without returning control to the owner. The
// total immutable song already has a 20000-step bound, also used as the per-plan
// scheduler limit. The original 2s admission horizon limits the first step,
// not the true release deadline of an already-owned long hold.
//
// Single owner except request_stop(). No I/O/classification/disk work in guard.
// status() checks expiry but never advances dispatch or renews an observation.
// Stop/expiry/fault is terminal: construct a new runtime after independently
// establishing release; changing epoch cannot re-arm this object.
class ExperimentalContactRuntime {
    static constexpr Ns ms=1'000'000,second=1'000'000'000;
    static constexpr Ns end_song=180*second,anchor_age=100*ms,lookahead=100*ms;
    static constexpr std::size_t max_song_contacts=2000,max_song_steps=20000;
    static constexpr Ns safe=std::numeric_limits<Ns>::max()/4;
    struct Pending {std::size_t index=0;pas::ContactPlan plan;};
    const pas::Clock& clock_;
    const ExperimentalAnchor anchor_;
    const ExperimentalRuntimeOptions options_;
    Memory selected_;
    std::vector<ExperimentalContactSource> sources_;
    pas::ContactScheduler scheduler_;
    std::map<std::uint64_t,Pending> pending_;
    std::optional<ExperimentalUiObservation> ui_;
    std::set<std::string> ui_ids_;
    std::atomic<bool> stop_requested_{false};
    ExperimentalRuntimeStatus value_;
    Ns last_now_=-1;
    std::uint64_t applied_ui_sequence_=0;
    std::size_t cursor_=0;
    bool release_exception_=false,release_attempted_=false;
    std::string guard_reason_;

    static bool text(const std::string& value) {return !value.empty()&&value.size()<=512;}
    static ExperimentalRuntimeOptions checked_options(ExperimentalRuntimeOptions value) {
        if(value.ui_receipt_max_age_ns<100*ms||value.ui_receipt_max_age_ns>500*ms||
           value.max_step_lateness_ns<0||value.max_step_lateness_ns>20*ms)
            throw std::invalid_argument("experimental UI age must be 100..500ms and step lateness 0..20ms");
        return value;
    }
    static bool terminal(ExperimentalRuntimeState state) {
        return state==ExperimentalRuntimeState::completed||state==ExperimentalRuntimeState::stopped;
    }
    static bool same_event(const CompiledSongEvent& a,const CompiledSongEvent& b) {
        return a.song_ns==b.song_ns&&a.contact_id==b.contact_id&&a.phase==b.phase&&
            a.point.x==b.point.x&&a.point.y==b.point.y;
    }
    static bool projection_free(const NoteContactOwnership& owner) {
        return owner.normal_projection_reference_pixels==0&&owner.projection_min_reference_pixels==0&&
            owner.projection_max_reference_pixels==0&&
            std::all_of(owner.normal_projection_spans.begin(),owner.normal_projection_spans.end(),[](const auto& s){
                return s.correction_lower_reference_pixels==0&&s.correction_upper_reference_pixels==0&&s.possible_edge_mask==16;
            });
    }
    static bool projection_bounded(const NoteContactOwnership& owner,double bound) {
        const auto within=[bound](double x){return std::isfinite(x)&&std::abs(x)<=bound;};
        if(!within(owner.normal_projection_reference_pixels)||!within(owner.projection_min_reference_pixels)||
           !within(owner.projection_max_reference_pixels)||owner.projection_min_reference_pixels>owner.projection_max_reference_pixels)return false;
        for(const auto& span:owner.normal_projection_spans)
            if(span.begin_ns<0||span.end_ns<span.begin_ns||span.end_ns>=end_song||
               !within(span.correction_lower_reference_pixels)||!within(span.correction_upper_reference_pixels)||
               span.correction_lower_reference_pixels>span.correction_upper_reference_pixels||
               !span.possible_edge_mask||span.possible_edge_mask>31||!std::isfinite(span.speed_bound_input_pixels_per_ns)||
               span.speed_bound_input_pixels_per_ns<0)return false;
        return true;
    }
    void select(const CompiledContactPlan& full) {
        const auto& meta=full.metadata;
        if(full.memory.identity.game!=Game::phigros||meta.compiler_version!="phigros-contact-v2"||
           meta.geometry.input_width!=1280||meta.geometry.input_height!=720||
           meta.geometry.rotation!=InputRotation::none||meta.geometry.viewport||meta.geometry.fit!=ViewportFit::contain||
           full.memory.contacts.empty()||full.memory.contacts.size()>max_song_contacts||full.ownership.size()!=full.memory.contacts.size()||
           full.events.size()>max_song_steps||!sha256_text(meta.cache_sha256))
            throw std::invalid_argument("unsupported or unbounded full compiled plan");
        const auto policy=meta.options.offscreen_policy;
        const bool candidate=policy==OffscreenPolicy::bounded_normal_candidate||
            policy==OffscreenPolicy::bounded_normal_intersection_candidate_v1;
        if((policy!=OffscreenPolicy::reject&&!candidate)||meta.requires_calibration!=candidate||
           (candidate&&(!std::isfinite(meta.options.max_normal_projection_reference_pixels)||
             meta.options.max_normal_projection_reference_pixels<0||meta.options.max_normal_projection_reference_pixels>250))||
           meta.options.max_contacts<1||meta.options.max_contacts>4||
           validate(full.memory,meta.options.max_contacts)!=full.peak_contacts)
            throw std::invalid_argument("full plan capacity/calibration metadata mismatch");
        std::map<std::uint64_t,const NoteContactOwnership*> owners;
        std::set<std::string> source_notes;
        std::size_t certificates=0;
        for(const auto& owner:full.ownership) {
            if(owner.source_note_id!=full.memory.identity.chart_sha256+'#'+owner.source_json_pointer||
               !text(owner.source_json_pointer)||!source_notes.insert(owner.source_note_id).second||
               !owners.emplace(owner.contact_id,&owner).second||owner.normal_projection_spans.size()>max_song_steps-certificates)
                throw std::invalid_argument("invalid full note ownership");
            certificates+=owner.normal_projection_spans.size();
        }
        std::vector<CompiledSongEvent> events;
        for(const auto& contact:full.memory.contacts) {
            if(!owners.contains(contact.id)||contact.steps.size()>max_song_steps-events.size())
                throw std::invalid_argument("invalid full contacts/step budget");
            for(const auto& step:contact.steps)events.push_back({step.song_ns,contact.id,step.phase,step.point});
        }
        std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){
            return std::tuple(a.song_ns,phase_order(a.phase),a.contact_id)<std::tuple(b.song_ns,phase_order(b.phase),b.contact_id);
        });
        if(events.size()!=full.total_steps||events.size()!=full.events.size()||
           !std::equal(events.begin(),events.end(),full.events.begin(),same_event)||
           compiled_contact_fingerprint(full)!=meta.cache_sha256)
            throw std::invalid_argument("full compiled events/fingerprint mismatch");
        selected_.identity=full.memory.identity;
        for(const auto& contact:full.memory.contacts) {
            const auto& owner=*owners.at(contact.id);
            if(contact.steps.back().song_ns>=end_song||owner.judgment_ns<0||owner.judgment_ns>=end_song||
               !projection_bounded(owner,candidate?meta.options.max_normal_projection_reference_pixels:0)||
               (candidate&&owner.normal_projection_spans.empty())||
               contact.steps.size()>max_song_steps-value_.selected_steps)
                throw std::invalid_argument("whole-song contact exceeds duration/projection/step budget");
            selected_.contacts.push_back(contact);value_.selected_steps+=contact.steps.size();
            value_.plan_end_song_ns=std::max(value_.plan_end_song_ns,contact.steps.back().song_ns);
            if(!projection_free(owner))++value_.projected_contacts;
        }
        value_.selected_projection_free=value_.projected_contacts==0;
        value_.peak_contacts=validate(selected_,4);
        std::sort(selected_.contacts.begin(),selected_.contacts.end(),[](const auto& a,const auto& b){
            return std::tuple(a.steps.front().song_ns,a.id)<std::tuple(b.steps.front().song_ns,b.id);
        });
        for(std::size_t i=0;i<selected_.contacts.size();++i) {
            const auto& contact=selected_.contacts[i];const auto& owner=*owners.at(contact.id);
            sources_.push_back({static_cast<std::uint64_t>(i+1),contact.id,owner.source_note_id,owner.source_json_pointer});
        }
        value_.selected_contacts=selected_.contacts.size();value_.full_cache_sha256=meta.cache_sha256;
        value_.full_plan_requires_calibration=meta.requires_calibration;
    }
    void validate_anchor() const {
        const auto& a=anchor_;
        if(a.origin_host_ns<0||a.origin_host_ns>safe||a.received_host_ns<0||a.received_host_ns>safe||
           a.explicit_phase_ns< -100*ms||a.explicit_phase_ns>100*ms||
           a.chosen_song_interval.lower<10*second+800*ms||a.chosen_song_interval.upper>11*second+950*ms||
           a.chosen_song_interval.upper<a.chosen_song_interval.lower||
           a.chosen_song_interval.upper-a.chosen_song_interval.lower>80*ms||
           a.origin_host_ns!=a.received_host_ns-(a.chosen_song_interval.lower+
               (a.chosen_song_interval.upper-a.chosen_song_interval.lower)/2)||
           !a.epoch||!a.generation||!a.geometry_version||!text(a.source_id)||!sha256_text(a.source_sha256)||
           !text(a.clock_id)||!text(a.evidence_id)||a.supporting_evidence_ids.size()<2||a.supporting_evidence_ids.size()>3)
            throw std::invalid_argument("invalid explicit experimental arrival anchor");
        std::set<std::string> ids;
        for(const auto& id:a.supporting_evidence_ids)if(!text(id)||!ids.insert(id).second)
            throw std::invalid_argument("anchor requires two or three distinct actual evidence IDs");
        if(!ids.contains(a.evidence_id))throw std::invalid_argument("latest anchor evidence missing from support");
    }
    Ns due(Ns song) const {return anchor_.origin_host_ns+anchor_.explicit_phase_ns+song;}
    bool healthy(Ns now) {
        if(terminal(value_.state))return false;
        if(now<0||now>safe||(last_now_>=0&&now<last_now_)) {stop("host_clock_invalid_or_regressed");return false;}
        last_now_=now;
        if(stop_requested_.load()) {stop("user_stop");return false;}
        if(ui_&&(now<ui_->received_host_ns||now-ui_->received_host_ns>=options_.ui_receipt_max_age_ns)) {stop("actual_UI_receipt_expired");return false;}
        if(value_.state==ExperimentalRuntimeState::armed&&now>due(value_.plan_end_song_ns)+options_.max_step_lateness_ns) {stop("whole_song_plan_deadline");return false;}
        return true;
    }
    bool dispatch_valid() {
        const auto now=clock_.now_ns();
        // Nonblocking check, called inside retained scheduler's dispatch mutex.
        // Do not call stop()/request_stop()/scheduler from this callback.
        if(stop_requested_.load()) {guard_reason_="user_stop";return false;}
        if(value_.state!=ExperimentalRuntimeState::armed||!ui_||!ui_->playing||now<last_now_||
           now<ui_->received_host_ns||now-ui_->received_host_ns>=options_.ui_receipt_max_age_ns||
           now>due(value_.plan_end_song_ns)+options_.max_step_lateness_ns) {
            guard_reason_="actual_UI_or_trial_deadline_invalid";return false;
        }
        for(const auto& [intent,pending]:pending_) {
            const auto count=scheduler_.executed_steps(intent);
            if(!count)continue; // A successful UP can erase it during run_due.
            if(*count<pending.plan.prefix_offset||*count-pending.plan.prefix_offset>=pending.plan.steps.size()) {guard_reason_="invalid_executed_prefix";return false;}
            const auto next=pending.plan.steps[static_cast<std::size_t>(*count-pending.plan.prefix_offset)].due_ns;
            if(now>next&&now-next>options_.max_step_lateness_ns) {guard_reason_="compiled_step_lateness_limit_exceeded";return false;}
        }
        return true;
    }
    bool revise(Pending& pending,std::uint64_t executed) {
        const auto& contact=selected_.contacts[pending.index];pas::ContactPlan result;
        if(executed>=contact.steps.size()) {stop("invalid_scheduler_progress");return false;}
        result.epoch=anchor_.epoch;result.generation=anchor_.generation;result.geometry_version=anchor_.geometry_version;
        result.intent_id=static_cast<std::uint64_t>(pending.index+1);result.note_id=contact.id;result.revision=pending.plan.revision+1;
        result.evidence_ns=ui_->received_host_ns;result.valid_until_ns=ui_->received_host_ns+options_.ui_receipt_max_age_ns;
        result.source_frame_sequence=ui_->sequence;result.basis="EXPERIMENTAL/UI-receipt-permission;fixed-rate1-arrival-origin;not-SyncLease";
        result.predicted_down_ns=due(contact.steps.front().song_ns);
        const std::size_t begin=executed?static_cast<std::size_t>(executed-1):0;
        const auto last=contact.steps.size()-1;
        result.prefix_offset=begin;
        const auto append=[&](std::size_t ordinal) {
            // Compaction drops only steps with successful real receipts.
            // The last executed step remains immutable; every later source
            // ordinal, including the true UP, is present without a gap.
            if(ordinal<executed) {
                const auto first=pending.plan.prefix_offset;
                if(ordinal<first||pending.plan.steps.size()<2||ordinal-first>=pending.plan.steps.size()-1)
                    throw std::logic_error("executed immutable prefix missing");
                result.steps.push_back(pending.plan.steps[static_cast<std::size_t>(ordinal-first)]);return;
            }
            const auto& step=contact.steps[ordinal];
            const auto pixel=pixel_point(step.point,1280,720);
            const auto phase=step.phase==Phase::down?pas::Phase::down:step.phase==Phase::move?pas::Phase::move:pas::Phase::up;
            result.steps.push_back({phase,static_cast<double>(pixel.first),static_cast<double>(pixel.second),due(step.song_ns)});
        };
        for(std::size_t i=begin;i<=last;++i)append(i);
        if(!scheduler_.submit(result)) {
            const auto rejected_now=clock_.now_ns();
            const auto progress=scheduler_.executed_steps(result.intent_id);
            // submit samples its own clock. The owner can be preempted after
            // refresh's earlier now but before submit, crossing a real UP;
            // the retained scheduler then correctly refuses a past-UP active
            // revision. If the old plan already covers every remaining step,
            // keep it unchanged and let run_due dispatch its still-bounded
            // tail. Do not shift UP, renew old plan evidence, or ACK any input.
            const bool existing_complete_tail=executed&&progress&&*progress==executed&&
                !pending.plan.steps.empty()&&scheduler_.armed()&&scheduler_.fault().empty()&&
                result.steps.back().phase==pending.plan.steps.back().phase&&
                result.steps.back().x==pending.plan.steps.back().x&&result.steps.back().y==pending.plan.steps.back().y&&
                result.steps.back().due_ns==pending.plan.steps.back().due_ns&&
                rejected_now>pending.plan.steps.back().due_ns&&
                rejected_now-pending.plan.steps.back().due_ns<=options_.max_step_lateness_ns&&
                rejected_now>=pending.plan.evidence_ns&&
                rejected_now-pending.plan.evidence_ns<options_.ui_receipt_max_age_ns;
            if(existing_complete_tail) {
                if(!dispatch_valid()) {stop(guard_reason_);return false;}
                ++value_.overdue_tail_revisions_deferred;
                return true;
            }
            stop("immutable_prefix_revision_rejected");return false;
        }
        pending.plan=std::move(result);return true;
    }
    void refresh(Ns now) {
        const bool new_ui=applied_ui_sequence_!=ui_->sequence;
        if(new_ui) {
            if(!scheduler_.set_context(anchor_.epoch,anchor_.generation,anchor_.geometry_version,true,ui_->received_host_ns)) {
                stop("scheduler_UI_gate_rejected");return;
            }
            applied_ui_sequence_=ui_->sequence;
        }
        // A complete tail does not need per-tick window renewal. Compact and
        // update permission only when a new actual UI receipt is available.
        if(new_ui)for(auto& [intent,pending]:pending_) {
            const auto count=scheduler_.executed_steps(intent);
            if(!count) {stop("compiled_contact_disappeared");return;}
            if(*count&&pending.plan.steps.back().due_ns<now)continue;
            if(!revise(pending,*count))return;
        }
        while(cursor_<selected_.contacts.size()&&due(selected_.contacts[cursor_].steps.front().song_ns)<=now+lookahead) {
            const auto down=due(selected_.contacts[cursor_].steps.front().song_ns);
            if(now>down&&now-down>options_.max_step_lateness_ns) {stop("new_contact_lateness_limit_exceeded");return;}
            if(pending_.size()>=64) {stop("experimental_plan_capacity");return;}
            Pending pending;pending.index=cursor_;
            if(!revise(pending,0))return;
            const auto intent=pending.plan.intent_id;
            pending_.emplace(intent,std::move(pending));++cursor_;
        }
    }
    void snapshot() {
        value_.pending=scheduler_.pending_count();value_.active=scheduler_.active_count();
        value_.release=scheduler_.last_release();
        value_.release_rpc_ok=release_attempted_&&!release_exception_&&value_.release.failed_ids.empty()&&value_.release.unknown_ids.empty();
    }
public:
    ExperimentalContactRuntime(CompiledContactPlan full,const pas::Clock& clock,pas::TouchBackend& backend,ExperimentalAnchor anchor,
                               ExperimentalRuntimeOptions options={})
      :clock_(clock),anchor_(std::move(anchor)),options_(checked_options(options)),
       scheduler_(clock,backend,4,64,static_cast<int>(max_song_steps),2*second,options_.ui_receipt_max_age_ns,options_.max_step_lateness_ns) {
        value_.ui_receipt_max_age_ns=options_.ui_receipt_max_age_ns;value_.max_step_lateness_ns=options_.max_step_lateness_ns;
        value_.anchor_max_age_ns=anchor_age;
        validate_anchor();select(full);scheduler_.set_dispatch_guard([this]{return dispatch_valid();});
    }
    ~ExperimentalContactRuntime() {try {if(!terminal(value_.state))stop("runtime_destroyed");}catch(...) {}}
    ExperimentalContactRuntime(const ExperimentalContactRuntime&)=delete;
    ExperimentalContactRuntime& operator=(const ExperimentalContactRuntime&)=delete;
    ExperimentalRuntimeStatus observe_ui(ExperimentalUiObservation observation) {
        const auto now=clock_.now_ns();if(!healthy(now))return status(); // check OLD permission before overwriting
        if(!observation.playing||!observation.source_valid||observation.width!=1280||observation.height!=720||
           observation.epoch!=anchor_.epoch||observation.generation!=anchor_.generation||
           observation.geometry_version!=anchor_.geometry_version||observation.source_id!=anchor_.source_id||
           !observation.sequence||!text(observation.evidence_id)||observation.received_host_ns<0||
           observation.received_host_ns>now||now-observation.received_host_ns>=options_.ui_receipt_max_age_ns||
           (ui_&&(observation.sequence<=ui_->sequence||observation.received_host_ns<=ui_->received_host_ns))||
           ui_ids_.contains(observation.evidence_id)||ui_ids_.size()>=64000) {
            stop("invalid_unknown_repeated_or_stale_UI_observation");return status();
        }
        ui_ids_.insert(observation.evidence_id);ui_=std::move(observation);
        if(value_.state==ExperimentalRuntimeState::waiting)value_.state=ExperimentalRuntimeState::ready;
        return status();
    }
    bool arm() {
        const auto now=clock_.now_ns();if(!healthy(now)||value_.state!=ExperimentalRuntimeState::ready||!ui_)return false;
        if(now<anchor_.received_host_ns||now-anchor_.received_host_ns>=anchor_age||
           due(selected_.contacts.front().steps.front().song_ns)<=now||
           due(selected_.contacts.front().steps.front().song_ns)-now>3*second) {stop("anchor_not_fresh_or_first_contact_not_future");return false;}
        value_.state=ExperimentalRuntimeState::armed;value_.reason="fixed_rate1_arrival_hypothesis_armed";
        value_.release_required=true; // any armed path must independently establish cleanup
        try {
            if(!scheduler_.set_context(anchor_.epoch,anchor_.generation,anchor_.geometry_version,true,ui_->received_host_ns)) {stop("scheduler_arm_rejected");return false;}
        }catch(const std::exception&) {stop("scheduler_arm_exception");return false;}
        applied_ui_sequence_=ui_->sequence;return true;
    }
    ExperimentalRuntimeTick tick() {
        ExperimentalRuntimeTick out;const auto now=clock_.now_ns();
        try {
        if(healthy(now)&&value_.state==ExperimentalRuntimeState::armed) {
            if(!dispatch_valid())stop(guard_reason_);
            if(value_.state==ExperimentalRuntimeState::armed)refresh(now);
            if(value_.state==ExperimentalRuntimeState::armed) {
                out.receipts=scheduler_.run_due();out.notices=scheduler_.take_notices();
                for(const auto& receipt:out.receipts) {
                    value_.release_required=true; // effect may be unknown even on failed DOWN
                    if(receipt.success&&receipt.command.phase==pas::Phase::up) {pending_.erase(receipt.command.intent_id);++value_.completed_contacts;}
                }
                if(!scheduler_.armed()||!scheduler_.fault().empty()||!out.notices.empty()) {
                    const auto reason=!scheduler_.fault().empty()?scheduler_.fault():!guard_reason_.empty()?guard_reason_:
                        !out.notices.empty()?out.notices.front().reason:"scheduler_stopped";
                    stop(reason);
                } else if(cursor_==selected_.contacts.size()&&pending_.empty()) {
                    release_attempted_=true;scheduler_.cancel("experimental_whole_song_finished");value_.state=ExperimentalRuntimeState::completed;
                    value_.reason="compiled_whole_song_RPC_sequence_completed_unqualified";snapshot();
                    if(!value_.release_rpc_ok) {value_.state=ExperimentalRuntimeState::stopped;value_.reason="release_failed_or_unknown";}
                }
            }
        }
        }catch(const std::exception&) {stop("scheduler_or_backend_exception_effect_unknown");}
        out.status=status();return out;
    }
    ExperimentalRuntimeStatus status() {if(!terminal(value_.state))healthy(clock_.now_ns());snapshot();return value_;}
    void stop(std::string reason) {
        if(terminal(value_.state))return;
        value_.state=ExperimentalRuntimeState::stopped;value_.reason=reason.substr(0,512);
        release_attempted_=true;try {scheduler_.cancel(value_.reason);}catch(...) {release_exception_=true;}
        pending_.clear();snapshot();
        if(!value_.release_rpc_ok)value_.reason="release_failed_or_unknown";
    }
    void request_stop() noexcept {stop_requested_.store(true);} // nonblocking; guard sees it before each inject
    std::optional<Ns> next_wake_ns() const {
        if(value_.state!=ExperimentalRuntimeState::armed)return std::nullopt;
        auto next=scheduler_.next_due_ns();
        if(cursor_<selected_.contacts.size()) {
            const auto admit=due(selected_.contacts[cursor_].steps.front().song_ns)-lookahead;
            next=next?std::min(*next,admit):admit;
        }
        return next?std::optional<Ns>(std::max(clock_.now_ns(),*next)):std::nullopt;
    }
    const Memory& selected_memory() const noexcept {return selected_;}
    const std::vector<ExperimentalContactSource>& sources() const noexcept {return sources_;}
    const ExperimentalAnchor& anchor() const noexcept {return anchor_;}
};
} // namespace rhythm::experimental
