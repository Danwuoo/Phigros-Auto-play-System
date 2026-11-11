#pragma once
#include "pas/core.hpp"
#include "rhythm/contact_compiler.hpp"
#include "rhythm/visual_candidates.hpp"
#include "clock_probe.hpp"
#include <atomic>
#include <array>
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
    // Explicit experiment limit. Actual backend capacity must be verified
    // independently before live use; FakeTouch does not establish it.
    int contact_capacity=4;
    // Prepared library profiles opt into larger finite songs. Legacy callers
    // keep their original limits; freshness and transport lateness do not change.
    std::size_t max_song_steps=20000;
    Ns song_duration_limit_ns=180'000'000'000;
    // Explicit per-song geometry ceiling; changing it is not judgment
    // calibration. All dispatched pixels must still lie inside the surface.
    double max_normal_projection_reference_pixels=250;
    double max_tangent_error_reference_pixels=0;
    Ns max_prejudge_ns=0,max_hold_reacquire_gap_ns=0;
    // Inclusive native song domain for the actual visual anchor. Defaults
    // preserve the frozen Dlyrotz opening; other charts must opt in explicitly.
    Ns anchor_domain_begin_ns=10'800'000'000,anchor_domain_end_ns=11'950'000'000;
    Ns max_coincident_flick_delay_ns=0; // Explicit shared-head candidate: 0 or 32ms.
    bool allow_advance_below_drags=false; // Explicit below-side early Drag candidate.
    bool allow_block_area_temporal_fallback=false;
    bool allow_block_area_hold_handoffs=false;
    std::optional<Ns> block_area_phase_ns;
    ClockProbe clock_probe; // Static opt-in whole-suffix mapping, fixed before READY.
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
    double max_normal_projection_reference_pixels=250;
    double max_tangent_error_reference_pixels=0;
    Ns anchor_domain_begin_ns=10'800'000'000,anchor_domain_end_ns=11'950'000'000;
    int peak_contacts=0,contact_capacity=4;
    ClockProbe clock_probe;
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
// Default deadlines remain the immutable arrival-origin + phase + song_ns
// hypothesis. An explicit static clock probe adds its prevalidated whole-suffix
// offset; no observation or tick can revise that mapping after preparation.
//
// Whole-song scope: every compiled contact, <=20000 steps by default; closed contacts
// derive their finite count bound from that step budget, not a 2000-note cap.
// duration <180s by default, explicitly bounded library profiles <=65536 steps
// and <600s. Peak<=explicit capacity (default4, at most10), 1280x720
// unrotated compiler surface. Projection
// candidates default to D<=250; a per-song explicit ceiling may be at most
// 1000000 reference pixels. This is a source-geometry bound, not an input-surface
// expansion or qualified hit region. No note is filtered.
// Admit each contact's DOWN within 100ms, retaining its complete unexecuted
// trajectory plus the last executed step when compacting on actual UI renewal.
// No MOVE is omitted behind a temporary prefix boundary: synchronous RPCs may
// advance time inside run_due without returning control to the owner. The
// total immutable song already has a finite step bound, also used as the per-plan
// scheduler limit. The original 2s admission horizon limits the first step,
// not the true release deadline of an already-owned long hold.
//
// Single owner except request_stop(). No I/O/classification/disk work in guard.
// status() checks expiry but never advances dispatch or renews an observation.
// Stop/expiry/fault is terminal: construct a new runtime after independently
// establishing release; changing epoch cannot re-arm this object.
class ExperimentalContactRuntime {
    static constexpr Ns ms=1'000'000,second=1'000'000'000;
    static constexpr Ns anchor_age=100*ms,lookahead=100*ms;
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
        validate_clock_probe_options(value.clock_probe);
        if(value.contact_capacity<1||value.contact_capacity>10)
            throw std::invalid_argument("explicit experimental contact capacity must be 1..10");
        if(value.max_song_steps<2||value.max_song_steps>65536||
           value.song_duration_limit_ns<second||value.song_duration_limit_ns>600*second)
            throw std::invalid_argument("explicit whole-song budget must be 2..65536 steps and 1..600 seconds");
        if(value.ui_receipt_max_age_ns<100*ms||value.ui_receipt_max_age_ns>500*ms||
           value.max_step_lateness_ns<0||value.max_step_lateness_ns>20*ms)
            throw std::invalid_argument("experimental UI age must be 100..500ms and step lateness 0..20ms");
        if(!std::isfinite(value.max_normal_projection_reference_pixels)||
           value.max_normal_projection_reference_pixels<0||value.max_normal_projection_reference_pixels>1000000)
            throw std::invalid_argument("explicit experimental normal projection ceiling must be finite 0..1000000 reference pixels");
        if(value.max_prejudge_ns<0||value.max_prejudge_ns>30*ms||
           value.max_hold_reacquire_gap_ns<0||value.max_hold_reacquire_gap_ns>20*ms)
            throw std::invalid_argument("explicit pre-judge/gap candidate duration exceeds its finite domain");
        if(value.max_coincident_flick_delay_ns!=0&&value.max_coincident_flick_delay_ns!=32*ms)
            throw std::invalid_argument("explicit coincident Flick delay must be 0 or 32ms");
        if(!std::isfinite(value.max_tangent_error_reference_pixels)||value.max_tangent_error_reference_pixels<0||
           value.max_tangent_error_reference_pixels>151.2)
            throw std::invalid_argument("explicit tangent judgment ceiling must be finite 0..151.2 reference pixels");
        if(value.anchor_domain_begin_ns<0||value.anchor_domain_begin_ns>=value.anchor_domain_end_ns||
           value.anchor_domain_end_ns>=180*second)
            throw std::invalid_argument("explicit experimental anchor domain must satisfy 0<=begin<end<180s");
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
        return !owner.block_area_temporal_fallback&&owner.normal_projection_reference_pixels==0&&owner.projection_min_reference_pixels==0&&
            owner.projection_max_reference_pixels==0&&
            owner.tangent_projection_min_reference_pixels==0&&owner.tangent_projection_max_reference_pixels==0&&
            owner.gap_tangent_projection_min_reference_pixels==0&&owner.gap_tangent_projection_max_reference_pixels==0&&
            std::all_of(owner.normal_projection_spans.begin(),owner.normal_projection_spans.end(),[](const auto& s){
                return s.correction_lower_reference_pixels==0&&s.correction_upper_reference_pixels==0&&s.possible_edge_mask==16&&
                    s.tangent_correction_lower_reference_pixels==0&&s.tangent_correction_upper_reference_pixels==0&&!s.judgment_domain_gap;
            });
    }
    static bool discrete_flick_valid(const Contact& contact,const NoteContactOwnership& owner,const ContactCompileOptions& options) {
        const bool sampled=options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_sampled_v2;
        const bool enabled=sampled||options.flick_path_policy==FlickPathPolicy::judgment_anchored_diagonal_snap_v1;
        if(!enabled||owner.kind!=NoteKind::flick)return !owner.flick_discrete_path;
        if(!owner.flick_discrete_path||!owner.flick_anchor_reference||!owner.flick_direction_reference||contact.steps.size()!=(sampled?5:4))return false;
        const auto& cert=*owner.flick_discrete_path;
        const auto anchor=*owner.flick_anchor_reference,direction=*owner.flick_direction_reference;
        const auto judgment=owner.dispatch_judgment_ns>=0?owner.dispatch_judgment_ns:owner.judgment_ns;
        const auto finite=[](PosePoint p){return std::isfinite(p.x)&&std::isfinite(p.y);};
        const auto distance=[](PosePoint a,PosePoint b){return std::hypot(a.x-b.x,a.y-b.y);};
        if(cert.model_id!=(sampled?"judgment-anchored-diagonal-sampled-v2":"judgment-anchored-diagonal-snap-v1")||
           cert.move_count!=(sampled?3:2)||cert.middle.has_value()!=sampled||!finite(anchor)||!finite(direction)||
           !finite(cert.bend_reference)||!finite(cert.end_reference)||
           options.flick_lead_ns<ms||options.flick_lead_ns>100*ms||options.flick_travel_ns<=options.flick_lead_ns||
           options.flick_travel_ns>200*ms||options.flick_release_ns<ms||options.flick_release_ns>100*ms||
           !std::isfinite(options.flick_distance_reference_pixels)||options.flick_distance_reference_pixels<1||
           options.flick_distance_reference_pixels>240)return false;
        const auto post=options.flick_travel_ns-options.flick_lead_ns;
        if(judgment<options.flick_lead_ns||judgment>std::numeric_limits<Ns>::max()-post-options.flick_release_ns||
           (sampled&&post<7))return false;
        const std::size_t endpoint=sampled?3:2,up=endpoint+1;
        if(
           contact.steps[0].phase!=Phase::down||contact.steps[1].phase!=Phase::move||
           contact.steps[2].phase!=Phase::move||contact.steps[endpoint].phase!=Phase::move||contact.steps[up].phase!=Phase::up||
           contact.steps[0].song_ns!=judgment-options.flick_lead_ns||
           cert.bend_ns<=judgment||cert.bend_ns>=cert.end_ns||
           cert.end_ns!=judgment+post||contact.steps[1].song_ns!=cert.bend_ns||contact.steps[endpoint].song_ns!=cert.end_ns||
           contact.steps[up].song_ns!=cert.end_ns+options.flick_release_ns)return false;
        if(sampled&&(!finite(cert.middle->reference)||cert.bend_ns!=judgment+post/7||
           cert.middle->song_ns!=judgment+(post*4)/7||cert.middle->song_ns<=cert.bend_ns||
           cert.middle->song_ns>=cert.end_ns||contact.steps[2].song_ns!=cert.middle->song_ns))return false;
        const auto dx=cert.end_reference.x-anchor.x,dy=cert.end_reference.y-anchor.y;
        const auto bx=cert.bend_reference.x-anchor.x,by=cert.bend_reference.y-anchor.y;
        const auto length=distance(anchor,cert.end_reference);
        const auto polyline=distance(anchor,cert.bend_reference)+(sampled?
            distance(cert.bend_reference,cert.middle->reference)+distance(cert.middle->reference,cert.end_reference):
            distance(cert.bend_reference,cert.end_reference));
        const auto approx=[](double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-6*std::max({1.0,std::abs(a),std::abs(b)});};
        const bool narrow_oblique=owner.block_area_temporal_fallback&&
            approx(std::min(std::abs(direction.x),std::abs(direction.y)),.075);
        if(!approx(length,options.flick_distance_reference_pixels)||!approx(cert.endpoint_distance_reference_pixels,length)||
           !approx(cert.polyline_length_reference_pixels,polyline)||!approx(std::hypot(direction.x,direction.y),1)||
           !approx(dx,length*direction.x)||!approx(dy,length*direction.y)||(!narrow_oblique&&!approx(std::abs(dx),std::abs(dy)))||
           std::abs(dx*by-dy*bx)<.01)return false;
        if(sampled){
            const auto sign=(direction.x*by-direction.y*bx)>0?1.:-1.;
            const PosePoint perpendicular{-direction.y,direction.x};
            const double bend_curve=narrow_oblique?.018:.10,middle_curve=narrow_oblique?.030:.12;
            const PosePoint first{anchor.x+length*(.20*direction.x+sign*bend_curve*perpendicular.x),
                anchor.y+length*(.20*direction.y+sign*bend_curve*perpendicular.y)};
            const PosePoint middle{anchor.x+length*(.55*direction.x+sign*middle_curve*perpendicular.x),
                anchor.y+length*(.55*direction.y+sign*middle_curve*perpendicular.y)};
            if(distance(first,cert.bend_reference)>1e-6||distance(middle,cert.middle->reference)>1e-6)return false;
        }
        const std::array<PosePoint,5> vertices{anchor,cert.bend_reference,
            sampled?cert.middle->reference:cert.end_reference,cert.end_reference,cert.end_reference};
        double max_jump=0;
        const auto inset=options.normal_projection_inset_reference_pixels;
        const auto xmax=inset>0?1279-inset:1280,ymax=inset>0?719-inset:720;
        for(std::size_t i=0;i<contact.steps.size();++i){
            const auto& p=vertices[i];
            if(p.x<inset-1e-6||p.x>xmax+1e-6||p.y<inset-1e-6||p.y>ymax+1e-6)return false;
            const PosePoint actual{contact.steps[i].point.x*1279,contact.steps[i].point.y*719};
            if(distance(actual,p)>1.415)return false;
            if(i)max_jump=std::max(max_jump,std::hypot((contact.steps[i].point.x-contact.steps[i-1].point.x)*1279,
                (contact.steps[i].point.y-contact.steps[i-1].point.y)*719));
        }
        const auto a=pixel_point(contact.steps[0].point,1280,720),b=pixel_point(contact.steps[1].point,1280,720),e=pixel_point(contact.steps[endpoint].point,1280,720);
        const auto actual_dx=static_cast<double>(e.first)-a.first,actual_dy=static_cast<double>(e.second)-a.second;
        const auto actual_bx=static_cast<double>(b.first)-a.first,actual_by=static_cast<double>(b.second)-a.second;
        return actual_dx!=0&&actual_dy!=0&&actual_bx*actual_dy-actual_by*actual_dx!=0&&
            std::hypot(actual_dx,actual_dy)>=std::max(1.,length-2*std::sqrt(2.))&&
            approx(cert.max_jump_input_pixels,max_jump)&&contact.steps.back().point.x==contact.steps[endpoint].point.x&&
            contact.steps.back().point.y==contact.steps[endpoint].point.y;
    }
    static bool projection_bounded(const NoteContactOwnership& owner,double bound,Ns end_song,double tangent_bound,Ns gap_limit) {
        const auto within=[bound](double x){return std::isfinite(x)&&std::abs(x)<=bound;};
        const auto tangent_within=[tangent_bound](double x){return std::isfinite(x)&&std::abs(x)<=tangent_bound;};
        if(!within(owner.normal_projection_reference_pixels)||!within(owner.projection_min_reference_pixels)||
           !within(owner.projection_max_reference_pixels)||owner.projection_min_reference_pixels>owner.projection_max_reference_pixels)return false;
        if(!tangent_within(owner.tangent_projection_min_reference_pixels)||!tangent_within(owner.tangent_projection_max_reference_pixels)||
           owner.tangent_projection_min_reference_pixels>owner.tangent_projection_max_reference_pixels)return false;
        const auto gap_within=[](double v){return std::isfinite(v)&&std::abs(v)<=1000000;};
        if(!gap_within(owner.gap_tangent_projection_min_reference_pixels)||!gap_within(owner.gap_tangent_projection_max_reference_pixels)||
           owner.gap_tangent_projection_min_reference_pixels>owner.gap_tangent_projection_max_reference_pixels)return false;
        Ns gap_begin=-1,gap_end=-1,previous_begin=-1;bool valid_before=false,saw_gap=false;
        for(const auto& span:owner.normal_projection_spans){
            if(span.begin_ns<0||span.end_ns<span.begin_ns||span.end_ns>=end_song||
               span.begin_ns<previous_begin||
               !within(span.correction_lower_reference_pixels)||!within(span.correction_upper_reference_pixels)||
               span.correction_lower_reference_pixels>span.correction_upper_reference_pixels||
               span.tangent_correction_lower_reference_pixels>span.tangent_correction_upper_reference_pixels||
               !span.possible_edge_mask||span.possible_edge_mask>31||!std::isfinite(span.speed_bound_input_pixels_per_ns)||
               span.speed_bound_input_pixels_per_ns<0)return false;
            previous_begin=span.begin_ns;
            if(span.judgment_domain_gap){
                if(gap_limit<=0||owner.kind!=NoteKind::hold||!valid_before||span.begin_ns<=owner.judgment_ns||
                   owner.hold_tracking_end_ns<0||span.end_ns>=owner.hold_tracking_end_ns||
                   !gap_within(span.tangent_correction_lower_reference_pixels)||!gap_within(span.tangent_correction_upper_reference_pixels)||
                   span.tangent_correction_lower_reference_pixels<owner.gap_tangent_projection_min_reference_pixels||
                   span.tangent_correction_upper_reference_pixels>owner.gap_tangent_projection_max_reference_pixels)return false;
                if(gap_begin<0)gap_begin=span.begin_ns;
                gap_end=std::max(gap_end,span.end_ns);saw_gap=true;
                if(gap_end-gap_begin>gap_limit)return false;
            }else{
                if(!tangent_within(span.tangent_correction_lower_reference_pixels)||!tangent_within(span.tangent_correction_upper_reference_pixels))return false;
                if(span.end_ns>span.begin_ns){valid_before=true;gap_begin=gap_end=-1;}
            }
        }
        return gap_begin<0&&(saw_gap||(owner.gap_tangent_projection_min_reference_pixels==0&&owner.gap_tangent_projection_max_reference_pixels==0));
    }
public:
    // Immutable validation is performed before collecting a fresh visual anchor.
    // Only this class can create a Prepared value; the move transfers all notes
    // and validation results without rehashing on the timing-critical arm path.
    class Prepared {
        friend class ExperimentalContactRuntime;
        Memory selected_;std::vector<ExperimentalContactSource> sources_;
        ExperimentalRuntimeStatus value_;ExperimentalRuntimeOptions options_;
        explicit Prepared(ExperimentalRuntimeOptions options):options_(options) {}
    public:
        Prepared(Prepared&&)=default;
        Prepared& operator=(Prepared&&)=default;
        Prepared(const Prepared&)=delete;
        Prepared& operator=(const Prepared&)=delete;
    };
    static Prepared prepare(const CompiledContactPlan& full,ExperimentalRuntimeOptions options={}) {
        Prepared prepared(checked_options(options));
        const auto& options_=prepared.options_;
        auto& selected_=prepared.selected_;auto& sources_=prepared.sources_;auto& value_=prepared.value_;
        const auto& meta=full.metadata;
        if(full.memory.identity.game!=Game::phigros||meta.compiler_version!="phigros-contact-v2"||
           meta.geometry.input_width!=1280||meta.geometry.input_height!=720||
           meta.geometry.rotation!=InputRotation::none||meta.geometry.viewport||meta.geometry.fit!=ViewportFit::contain||
           full.memory.contacts.empty()||full.memory.contacts.size()>options_.max_song_steps/2||
           full.ownership.size()>full.memory.contacts.size()||
           full.memory.contacts.size()-full.ownership.size()!=meta.block_area_hold_handoff_contacts||
           full.events.size()>options_.max_song_steps||!sha256_text(meta.cache_sha256))
            throw std::invalid_argument("unsupported or unbounded full compiled plan");
        if(options_.clock_probe.enabled) {
            const auto proof=validate_clock_probe(full,options_.clock_probe);
            if(!proof.prefix_events||!proof.suffix_events||proof.mapped_first_ns!=proof.original_first_ns||
               proof.mapped_last_ns>=options_.song_duration_limit_ns)
                throw std::invalid_argument("clock probe must preserve the first input and remain inside the original finite duration budget");
            value_.clock_probe=options_.clock_probe;value_.rate_one_hypothesis=false;
        }
        const auto policy=meta.options.offscreen_policy;
        const bool rectangle=policy==OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
        const bool candidate=policy==OffscreenPolicy::bounded_normal_candidate||
            policy==OffscreenPolicy::bounded_normal_intersection_candidate_v1||rectangle;
        const bool block_areas=meta.block_area_count!=0;
        if((block_areas?meta.block_area_policy_id!="phira-block-area-continuous-detour-v1":!meta.block_area_policy_id.empty())||
           (block_areas&&(options_.clock_probe.enabled||meta.options.block_area_phase_ns!=options_.block_area_phase_ns))||
           meta.options.allow_block_area_temporal_fallback!=options_.allow_block_area_temporal_fallback||
           meta.options.allow_block_area_hold_handoffs!=options_.allow_block_area_hold_handoffs||
           (options_.allow_block_area_hold_handoffs&&!options_.allow_block_area_temporal_fallback)||
           (meta.block_area_hold_handoff_contacts&&!options_.allow_block_area_hold_handoffs)||
           (meta.options.allow_block_area_temporal_fallback&&(!block_areas||!rectangle||!options_.block_area_phase_ns))||
           meta.block_area_temporal_contacts>meta.block_area_detoured_contacts||
           meta.block_area_detoured_contacts>full.ownership.size())
            throw std::invalid_argument("whole-song BlockArea metadata is inconsistent");
        if((policy!=OffscreenPolicy::reject&&!candidate)||meta.requires_calibration!=candidate||
           (candidate&&(!std::isfinite(meta.options.max_normal_projection_reference_pixels)||
             meta.options.max_normal_projection_reference_pixels<0||
             meta.options.max_normal_projection_reference_pixels>options_.max_normal_projection_reference_pixels))||
           meta.options.max_contacts<1||meta.options.max_contacts>options_.contact_capacity||
           validate(full.memory,meta.options.max_contacts)!=full.peak_contacts)
            throw std::invalid_argument("full plan capacity/calibration metadata mismatch");
        if(!std::isfinite(meta.options.max_tangent_error_reference_pixels)||
           (rectangle?(meta.options.max_tangent_error_reference_pixels<=0||
             meta.options.max_tangent_error_reference_pixels>options_.max_tangent_error_reference_pixels||
             meta.judgment_policy_provenance.empty()):meta.options.max_tangent_error_reference_pixels!=0))
            throw std::invalid_argument("unrecorded or unbounded tangent judgment candidate");
        const bool hold_grace=meta.options.hold_tail_policy==HoldTailPolicy::phira_tail_grace_candidate_v1;
        if((meta.options.hold_tail_policy!=HoldTailPolicy::full_pose_v1&&!hold_grace)||
           (hold_grace&&(!candidate||meta.judgment_policy_provenance.empty()||meta.judgment_policy_provenance.size()>2048)))
            throw std::invalid_argument("unsupported or unrecorded Hold continuation candidate");
        if(meta.options.max_prejudge_ns<0||meta.options.max_prejudge_ns>options_.max_prejudge_ns||
           (meta.options.advance_above_drags&&meta.options.max_prejudge_ns==0)||
           (meta.options.advance_below_drags&&(!options_.allow_advance_below_drags||meta.options.max_prejudge_ns==0||
             meta.judgment_policy_provenance.find("advance-below-drags-candidate-v1")==std::string::npos))||
           meta.options.max_hold_reacquire_gap_ns<0||meta.options.max_hold_reacquire_gap_ns>options_.max_hold_reacquire_gap_ns||
           (meta.options.max_hold_reacquire_gap_ns>0&&(!rectangle||!hold_grace))||
           ((meta.options.max_prejudge_ns>0||meta.options.max_hold_reacquire_gap_ns>0)&&meta.judgment_policy_provenance.empty()))
            throw std::invalid_argument("unrecorded or unbounded pre-judge/reacquisition candidate");
        if(meta.options.below_drag_source_ids.size()>2048||(!meta.options.below_drag_source_ids.empty()&&
           (!meta.options.advance_below_drags||meta.judgment_policy_provenance.find("below-drag-source-filter-candidate-v1")==std::string::npos)))
            throw std::invalid_argument("unrecorded or unbounded below Drag source filter");
        if(meta.options.above_drag_source_ids.size()>2048||(!meta.options.above_drag_source_ids.empty()&&
           (!meta.options.advance_above_drags||!std::is_sorted(meta.options.above_drag_source_ids.begin(),meta.options.above_drag_source_ids.end())||
            meta.judgment_policy_provenance.find("above-drag-source-filter-candidate-v1")==std::string::npos)))
            throw std::invalid_argument("unrecorded, noncanonical or unbounded above Drag source filter");
        if((meta.options.coincident_flick_delay_ns!=0&&meta.options.coincident_flick_delay_ns!=32*ms)||
           meta.options.coincident_flick_delay_ns>options_.max_coincident_flick_delay_ns||
           (meta.options.coincident_flick_delay_ns>0&&
            (meta.options.flick_path_policy!=FlickPathPolicy::judgment_anchored_diagonal_snap_v1||
             meta.judgment_policy_provenance.size()>2048||
             meta.judgment_policy_provenance.find("coincident-flick-delay32-candidate-v1")==std::string::npos)))
            throw std::invalid_argument("unrecorded or unbounded coincident Flick handoff candidate");
        std::map<std::uint64_t,const NoteContactOwnership*> owners;
        std::map<std::uint64_t,const Contact*> physical_contacts;
        for(const auto& contact:full.memory.contacts)physical_contacts.emplace(contact.id,&contact);
        std::map<std::string,const NoteContactOwnership*> source_notes;
        std::set<Ns> non_flick_heads;
        std::vector<std::string> offscreen_hold_ids;
        std::size_t certificates=0,block_area_temporal_contacts=0,block_area_detoured_contacts=0;
        for(const auto& owner:full.ownership) {
            if(owner.source_note_id!=full.memory.identity.chart_sha256+'#'+owner.source_json_pointer||
               !text(owner.source_json_pointer)||!source_notes.emplace(owner.source_note_id,&owner).second||
               !owners.emplace(owner.contact_id,&owner).second||owner.normal_projection_spans.size()>options_.max_song_steps-certificates)
                throw std::invalid_argument("invalid full note ownership");
            certificates+=owner.normal_projection_spans.size();
            if(owner.block_area_translation_reference||owner.block_area_temporal_fallback) {
                if(!block_areas||!rectangle)throw std::invalid_argument("unrecorded BlockArea contact detour");
                ++block_area_detoured_contacts;
            }
            if(owner.block_area_temporal_fallback) {
                if(!options_.allow_block_area_temporal_fallback||!owner.normal_projection_spans.empty()||
                   !projection_bounded(owner,0,options_.song_duration_limit_ns,0,0))
                    throw std::invalid_argument("invalid or unexpected BlockArea timing-only contact");
                ++block_area_temporal_contacts;
            }
            if(owner.block_area_offscreen_hold_fallback) {
                if(owner.kind!=NoteKind::hold||!owner.block_area_temporal_fallback||offscreen_hold_ids.size()>=32)
                    throw std::invalid_argument("Invalid selected offscreen Hold continuation");
                offscreen_hold_ids.push_back(owner.source_note_id);
            }
            if(!owner.hold_handoff_contact_ids.empty()) {
                if(!options_.allow_block_area_hold_handoffs||owner.kind!=NoteKind::hold||!owner.block_area_temporal_fallback||
                   owner.hold_handoff_contact_ids.size()>16||owner.hold_handoff_contact_ids.size()>meta.block_area_hold_handoff_contacts)
                    throw std::invalid_argument("Unrecorded BlockArea Hold handoff");
                auto preceding=physical_contacts.at(owner.contact_id);
                if(preceding->steps.front().song_ns!=owner.judgment_ns)
                    throw std::invalid_argument("BlockArea Hold handoff changed the original DOWN");
                Ns prior_end=-1;
                for(const auto id:owner.hold_handoff_contact_ids) {
                    const auto following=physical_contacts.at(id);
                    const auto down=following->steps.front().song_ns;
                    const auto up=preceding->steps.back().song_ns;
                    if(!owners.emplace(id,&owner).second||down<=preceding->steps.front().song_ns||down<prior_end||
                       up-down!=block_area_hold_handoff_overlap_ns||following->steps.back().song_ns<=up)
                        throw std::invalid_argument("BlockArea Hold handoff has invalid or overlapping pointer lifetimes");
                    prior_end=up;
                    preceding=following;
                }
                if(hold_grace&&owner.hold_tracking_end_ns!=preceding->steps.back().song_ns)
                    throw std::invalid_argument("BlockArea Hold handoff must retain tracking through the original UP");
            }
            if(owner.kind==NoteKind::tap||owner.kind==NoteKind::hold||owner.kind==NoteKind::drag)
                non_flick_heads.insert(owner.judgment_ns);
        }
        if(owners.size()!=physical_contacts.size())throw std::invalid_argument("Incomplete physical contact ownership");
        if(block_area_temporal_contacts!=meta.block_area_temporal_contacts||block_area_detoured_contacts!=meta.block_area_detoured_contacts)
            throw std::invalid_argument("BlockArea contact counts differ from compiled ownership");
        std::sort(offscreen_hold_ids.begin(),offscreen_hold_ids.end());
        if(offscreen_hold_ids!=meta.options.block_area_offscreen_hold_source_ids||
           offscreen_hold_ids.size()!=meta.block_area_offscreen_hold_contacts)
            throw std::invalid_argument("Offscreen Hold continuation differs from the selected source IDs");
        std::set<std::string> filtered_sources;
        for(const auto& id:meta.options.below_drag_source_ids){
            const auto found=source_notes.find(id);
            if(id.empty()||id.size()>512||!filtered_sources.insert(id).second||found==source_notes.end()||
               found->second->kind!=NoteKind::drag||found->second->side!=NoteSide::below)
                throw std::invalid_argument("below Drag filter must name unique original below-side Drag source IDs");
        }
        std::set<std::string> filtered_above_sources;
        for(const auto& id:meta.options.above_drag_source_ids){
            const auto found=source_notes.find(id);
            if(id.empty()||id.size()>512||!filtered_above_sources.insert(id).second||found==source_notes.end()||
               found->second->kind!=NoteKind::drag||found->second->side!=NoteSide::above)
                throw std::invalid_argument("above Drag filter must name unique original above-side Drag source IDs");
        }
        std::vector<CompiledSongEvent> events;
        for(const auto& contact:full.memory.contacts) {
            if(!owners.contains(contact.id)||contact.steps.size()>options_.max_song_steps-events.size())
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
            if(!discrete_flick_valid(contact,owner,meta.options))
                throw std::invalid_argument("invalid or unexpected discrete Flick path certificate: "+owner.source_json_pointer);
            if(meta.options.coincident_flick_delay_ns>0&&owner.kind==NoteKind::flick&&
               non_flick_heads.contains(owner.judgment_ns)&&owner.dispatch_judgment_ns<=owner.judgment_ns)
                throw std::invalid_argument("explicit shared-head Flick policy requires its complete delayed contact");
            if(owner.dispatch_judgment_ns>owner.judgment_ns) {
                if(owner.kind!=NoteKind::flick||meta.options.coincident_flick_delay_ns!=32*ms||
                   owner.judgment_ns<0||owner.judgment_ns>std::numeric_limits<Ns>::max()-32*ms||
                   owner.dispatch_judgment_ns-owner.judgment_ns!=meta.options.coincident_flick_delay_ns||
                   !non_flick_heads.contains(owner.judgment_ns)||
                   contact.steps.front().song_ns!=owner.dispatch_judgment_ns-meta.options.flick_lead_ns)
                    throw std::invalid_argument("late dispatch requires an exact bounded Flick and original simultaneous non-Flick head");
            }else if(meta.options.max_prejudge_ns>0&&(owner.kind==NoteKind::drag||owner.kind==NoteKind::flick)){
                if(owner.dispatch_judgment_ns<0||
                   owner.dispatch_judgment_ns>owner.judgment_ns||owner.judgment_ns-owner.dispatch_judgment_ns>meta.options.max_prejudge_ns||
                   (owner.kind==NoteKind::flick&&(meta.options.flick_lead_ns<0||meta.options.flick_lead_ns>100*ms))||
                   contact.steps.front().song_ns!=owner.dispatch_judgment_ns-(owner.kind==NoteKind::flick?meta.options.flick_lead_ns:0))
                    throw std::invalid_argument("candidate dispatch anchor differs from original note or bounded pre-judge policy");
            }else if(owner.dispatch_judgment_ns!=-1)throw std::invalid_argument("unexpected dispatch-anchor metadata outside explicit Drag/Flick candidate");
            if(hold_grace&&owner.kind==NoteKind::hold&&owner.hold_handoff_contact_ids.empty()){
                if(owner.hold_tracking_end_ns<contact.steps.front().song_ns||
                   owner.hold_tracking_end_ns>contact.steps.back().song_ns||
                   contact.steps.back().song_ns-owner.hold_tracking_end_ns>220*ms)
                    throw std::invalid_argument("Hold continuation candidate exceeds recorded tail domain");
                Point frozen=contact.steps.front().point;
                for(const auto& step:contact.steps){
                    if(step.song_ns<=owner.hold_tracking_end_ns)frozen=step.point;
                    else if(step.point.x!=frozen.x||step.point.y!=frozen.y)
                        throw std::invalid_argument("Hold tail candidate must retain the last tracked point through original UP");
                }
            }else if((!hold_grace||owner.kind!=NoteKind::hold)&&owner.hold_tracking_end_ns>=0){
                throw std::invalid_argument("unexpected Hold tracking-domain metadata");
            }
            if(candidate&&owner.kind==NoteKind::hold&&!owner.block_area_temporal_fallback){
                auto cursor=contact.steps.front().song_ns;
                for(const auto& span:owner.normal_projection_spans){
                    if(span.begin_ns!=cursor||span.end_ns<span.begin_ns)
                        throw std::invalid_argument("Hold projection certificates overlap or leave an unrecorded interval");
                    cursor=span.end_ns;
                }
                const auto tracking_end=hold_grace?owner.hold_tracking_end_ns:contact.steps.back().song_ns;
                if(cursor!=tracking_end)throw std::invalid_argument("Hold projection certificates do not cover its complete tracked domain");
            }
            if(contact.steps.back().song_ns>=options_.song_duration_limit_ns||owner.judgment_ns<0||owner.judgment_ns>=options_.song_duration_limit_ns)
                throw std::invalid_argument("whole-song contact exceeds explicit duration: "+owner.source_json_pointer);
            if(!projection_bounded(owner,candidate?meta.options.max_normal_projection_reference_pixels:0,options_.song_duration_limit_ns,
                 rectangle?meta.options.max_tangent_error_reference_pixels:0,meta.options.max_hold_reacquire_gap_ns))
                throw std::invalid_argument("whole-song contact exceeds recorded projection bounds: "+owner.source_json_pointer);
            if(candidate&&owner.normal_projection_spans.empty()&&!owner.block_area_temporal_fallback)
                throw std::invalid_argument("whole-song candidate lacks a projection certificate: "+owner.source_json_pointer);
            if(contact.steps.size()>options_.max_song_steps-value_.selected_steps)
                throw std::invalid_argument("whole-song contact exceeds explicit event budget: "+owner.source_json_pointer);
            selected_.contacts.push_back(contact);value_.selected_steps+=contact.steps.size();
            value_.plan_end_song_ns=std::max(value_.plan_end_song_ns,contact.steps.back().song_ns);
            if(!projection_free(owner))++value_.projected_contacts;
        }
        value_.selected_projection_free=value_.projected_contacts==0;
        value_.peak_contacts=validate(selected_,options_.contact_capacity);
        std::sort(selected_.contacts.begin(),selected_.contacts.end(),[](const auto& a,const auto& b){
            return std::tuple(a.steps.front().song_ns,a.id)<std::tuple(b.steps.front().song_ns,b.id);
        });
        for(std::size_t i=0;i<selected_.contacts.size();++i) {
            const auto& contact=selected_.contacts[i];const auto& owner=*owners.at(contact.id);
            sources_.push_back({static_cast<std::uint64_t>(i+1),contact.id,owner.source_note_id,owner.source_json_pointer});
        }
        value_.selected_contacts=selected_.contacts.size();value_.full_cache_sha256=meta.cache_sha256;
        value_.full_plan_requires_calibration=meta.requires_calibration;
        return prepared;
    }
private:
    void validate_anchor() const {
        const auto& a=anchor_;
        if(a.origin_host_ns<0||a.origin_host_ns>safe||a.received_host_ns<0||a.received_host_ns>safe||
           a.explicit_phase_ns< -100*ms||a.explicit_phase_ns>100*ms||
           (options_.block_area_phase_ns&&a.explicit_phase_ns!=*options_.block_area_phase_ns)||
           a.chosen_song_interval.lower<options_.anchor_domain_begin_ns||a.chosen_song_interval.upper>options_.anchor_domain_end_ns||
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
    Ns due(Ns song) const {return anchor_.origin_host_ns+anchor_.explicit_phase_ns+options_.clock_probe.mapped_song_ns(song);}
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
        result.source_frame_sequence=ui_->sequence;result.basis=options_.clock_probe.enabled?
            "EXPERIMENTAL/UI-receipt-permission;static-piecewise-offset-probe;unqualified;not-SyncLease":
            "EXPERIMENTAL/UI-receipt-permission;fixed-rate1-arrival-origin;not-SyncLease";
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
      :ExperimentalContactRuntime(prepare(full,options),clock,backend,std::move(anchor)) {}
    ExperimentalContactRuntime(Prepared prepared,const pas::Clock& clock,pas::TouchBackend& backend,ExperimentalAnchor anchor)
      :clock_(clock),anchor_(std::move(anchor)),options_(prepared.options_),
       selected_(std::move(prepared.selected_)),sources_(std::move(prepared.sources_)),
       scheduler_(clock,backend,options_.contact_capacity,64,static_cast<int>(options_.max_song_steps),2*second,options_.ui_receipt_max_age_ns,options_.max_step_lateness_ns),value_(std::move(prepared.value_)) {
        if(selected_.contacts.empty())throw std::invalid_argument("prepared plan was already moved");
        value_.ui_receipt_max_age_ns=options_.ui_receipt_max_age_ns;value_.max_step_lateness_ns=options_.max_step_lateness_ns;
        value_.anchor_max_age_ns=anchor_age;
        value_.contact_capacity=options_.contact_capacity;
        value_.max_normal_projection_reference_pixels=options_.max_normal_projection_reference_pixels;
        value_.max_tangent_error_reference_pixels=options_.max_tangent_error_reference_pixels;
        value_.anchor_domain_begin_ns=options_.anchor_domain_begin_ns;value_.anchor_domain_end_ns=options_.anchor_domain_end_ns;
        validate_anchor();scheduler_.set_dispatch_guard([this]{return dispatch_valid();});
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
           due(selected_.contacts.front().steps.front().song_ns)-now>options_.song_duration_limit_ns) {stop("anchor_not_fresh_or_first_contact_not_future");return false;}
        value_.state=ExperimentalRuntimeState::armed;value_.reason=options_.clock_probe.enabled?
            "static_piecewise_offset_probe_armed_unqualified":"fixed_rate1_arrival_hypothesis_armed";
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
    // Actual immutable input deadline, distinct from the earlier admission wake.
    // Retain overdue values so the wait policy cannot hide scheduler lateness.
    std::optional<Ns> next_input_due_ns() const {
        if(value_.state!=ExperimentalRuntimeState::armed)return std::nullopt;
        // The retained scheduler's next_due_ns also includes UI/evidence
        // expiry. Read its actual executed ordinal and the immutable source
        // steps instead; permission expiry remains in next_wake_ns above.
        std::optional<Ns> next;
        for(const auto& [intent,pending]:pending_) {
            const auto executed=scheduler_.executed_steps(intent);
            const auto& contact=selected_.contacts[pending.index];
            if(executed&&*executed<contact.steps.size()) {
                const auto step=due(contact.steps[static_cast<std::size_t>(*executed)].song_ns);
                next=next?std::min(*next,step):step;
            }
        }
        if(cursor_<selected_.contacts.size()) {
            const auto first=due(selected_.contacts[cursor_].steps.front().song_ns);
            next=next?std::min(*next,first):first;
        }
        return next;
    }
    const Memory& selected_memory() const noexcept {return selected_;}
    const std::vector<ExperimentalContactSource>& sources() const noexcept {return sources_;}
    const ExperimentalAnchor& anchor() const noexcept {return anchor_;}
};
} // namespace rhythm::experimental
