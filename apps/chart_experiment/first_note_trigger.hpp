#pragma once
#include "rhythm/chart_memory.hpp"
#include "rhythm/visual_candidates.hpp"
#include <map>
#include <limits>
#include <optional>
#include <set>

namespace rhythm::experimental {
enum class FirstNoteTriggerState { acquiring, ready, fired, cancelled };
struct FirstNoteTriggerAnchor {
    Ns origin_host_ns=-1,received_host_ns=-1,target_host_ns=-1;
    VisualTimeInterval chosen_song_interval;
    std::string evidence_id;
    std::vector<std::string> supporting_evidence_ids;
};
struct FirstNoteTriggerStatus {
    FirstNoteTriggerState state=FirstNoteTriggerState::acquiring;
    std::string reason="waiting_for_first_note_downward_approach";
    unsigned support_count=0;
    std::size_t raw_candidate_count=0,first_note_positive_candidates=0;
    std::vector<VisualTimeCandidate> raw_candidates;
    std::vector<VisualTimeInterval> selected_intervals;
    std::optional<FirstNoteTriggerAnchor> anchor;
    bool timing_qualified=false;
};

// One-shot first-note detector/ETA hypothesis, not a SyncLease or full-song
// clock qualifier. It does NOT select the candidate nearest 12s. Constructor
// proves exactly one native positive-speed run can put the first note in the
// observed approach band, ending at its judgment. Earlier positive runs are
// excluded only by integrated native floor geometry, never by time proximity.
// Each of two actual frames must match its
// lowest cyan center to that exact first-note source ID on that native run.
// The second center must move down by > the combined 3+3px measurement error.
// Negative-speed and different-note aliases remain in raw_candidates; their
// exclusion is justified by identity + native speed + observed motion.
//
// Receipt-minus-song midpoint and rate=1 remain explicit hypotheses. UI source
// age and independent input latency remain unqualified. Fire with >=200ms
// dispatch lead; preparing a runtime afterwards may consume that lead and must
// never renew this anchor's actual receipt. No re-arm after take_anchor().
class FirstNoteTrigger {
    static constexpr Ns ms=1'000'000,second=1'000'000'000,safe=(Ns{1}<<53)-1;
    struct Piece {VisualTimeInterval interval;double speed=0;bool first_note_visible=false;};
    struct Range {long double lo=0,hi=0;};
    static long double down(long double v) {return std::nextafter(v,-std::numeric_limits<long double>::infinity());}
    static long double up(long double v) {return std::nextafter(v,std::numeric_limits<long double>::infinity());}
    static Range add(Range a,Range b) {return {down(a.lo+b.lo),up(a.hi+b.hi)};}
    static Range multiply(Range a,Range b) {
        const long double values[]{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
        const auto [lo,hi]=std::minmax_element(std::begin(values),std::end(values));
        return {down(*lo),up(*hi)};
    }
    static Range increment(double speed,Ns duration) {
        const auto seconds=static_cast<long double>(duration)/second;
        return multiply({speed,speed},{down(seconds),up(seconds)});
    }
    struct Support {VisualTimeInterval song;VisualCandidateFrame frame;double y=0;};
    FirstNoteTriggerStatus value_;
    OpeningVisualModel model_;
    Ns phase_=0,note_time_=12*second,last_now_=-1,last_received_=-1;
    std::uint64_t last_sequence_=0;
    std::string first_note_id_,first_note_pointer_="/judgeLineList/0/notesAbove/0",context_;
    std::map<std::string,Piece> pieces_;
    std::map<std::string,std::string> note_pointers_;
    std::set<std::string> evidence_ids_,frame_ids_;
    std::optional<Support> previous_;
    bool consumed_=false;

    static bool text(const std::string& s,std::size_t limit=512) {return !s.empty()&&s.size()<=limit;}
    static Ns midpoint(VisualTimeInterval i) {return i.lower+(i.upper-i.lower)/2;}
    static void require(bool valid,const char* reason) {if(!valid)throw std::invalid_argument(reason);}
    static std::vector<VisualTimeInterval> merge(std::vector<VisualTimeInterval> in) {
        std::sort(in.begin(),in.end(),[](auto a,auto b){return a.lower<b.lower||(a.lower==b.lower&&a.upper<b.upper);});
        std::vector<VisualTimeInterval> out;
        for(const auto i:in) {
            if(!out.empty()&&i.lower<=out.back().upper+1)out.back().upper=std::max(out.back().upper,i.upper);
            else out.push_back(i);
        }
        return out;
    }
    static std::string context(const VisualCandidateFrame& f) {
        std::string out;
        for(const auto* part:{&f.source_id,&f.source_sha256,&f.clock_id,&f.opening_context_id})
            out+=std::to_string(part->size())+":"+*part;
        return out+":"+std::to_string(f.epoch)+":"+std::to_string(f.generation)+":"+std::to_string(f.geometry_version);
    }
    void reset(const char* reason) {
        previous_.reset();value_.anchor.reset();value_.support_count=0;
        value_.state=FirstNoteTriggerState::acquiring;value_.reason=reason;
    }
    bool health(Ns now,bool ui_healthy) {
        if(value_.state==FirstNoteTriggerState::cancelled||value_.state==FirstNoteTriggerState::fired)return false;
        if(now<0||now>safe||(last_now_>=0&&now<last_now_)){cancel("host_clock_invalid_or_rollback");return false;}
        last_now_=now;
        if(!ui_healthy){reset("waiting_for_healthy_actual_UI");return false;}
        if(value_.state==FirstNoteTriggerState::ready&&value_.anchor&&
           (now-value_.anchor->received_host_ns>=100*ms||value_.anchor->target_host_ns-now<200*ms))
            reset("first_note_readiness_expired");
        return true;
    }
    bool provenance(const VisualCandidateResult& r,Ns now) const {
        const auto& f=r.observation;
        return !r.requires_new_epoch&&r.model_id==model_.model_id&&r.model_sha256==model_.model_sha256&&
            r.calibration_id==model_.calibration_id&&r.calibration_sha256==model_.calibration_sha256&&
            f.sequence&&f.epoch&&f.generation&&f.geometry_version&&f.source_valid&&f.centers_complete_unoccluded&&
            f.width==1280&&f.height==720&&f.clock_kind==VisualSourceClock::arrival_only&&!f.source_time_ns&&!f.capture_host_interval&&
            text(f.source_id)&&sha256_text(f.source_sha256)&&text(f.clock_id)&&text(f.opening_context_id)&&
            text(f.frame_id)&&text(f.evidence_id)&&sha256_text(f.frame_sha256)&&
            f.received_host_ns>=0&&f.received_host_ns<=now&&now-f.received_host_ns<100*ms&&
            f.centers.size()<=8&&r.candidates.size()<=1024;
    }
public:
    FirstNoteTrigger(const SemanticChart& chart,const OpeningVisualModel& model,Ns phase_ns):model_(model),phase_(phase_ns) {
        require(phase_ns>=-100*ms&&phase_ns<=100*ms,"first-note phase must be +/-100ms");
        require(chart.format_version==3&&chart.offset_seconds==0&&!chart.lines.empty()&&
            sha256_text(chart.source.chart_sha256)&&chart.source.chart_sha256==model.chart_sha256,
            "first-note native chart/model identity mismatch");
        require(model.begin_ns>=0&&model.begin_ns<model.end_ns&&model.end_ns==note_time_&&
            model.origin_x==640&&model.origin_y==576&&model.x_pixels_per_unit==72&&model.pixel_error==3&&
            std::isfinite(model.approach_scale_lower)&&std::isfinite(model.approach_scale_upper)&&
            model.approach_scale_lower>0&&model.approach_scale_lower<=model.approach_scale_upper&&model.approach_scale_upper<=100000&&
            std::isfinite(model.pose_tolerance_pixels)&&model.pose_tolerance_pixels>=0&&model.pose_tolerance_pixels<=1&&
            text(model.model_id)&&text(model.calibration_id)&&sha256_text(model.model_sha256)&&sha256_text(model.calibration_sha256),
            "unsupported first-note model");
        const auto& line=chart.lines[0];require(line.source_index==0&&!line.notes.empty()&&line.notes.size()<=1024&&
            line.speed_events.size()<=256,"first-note native line budget");
        const auto& note=line.notes[0];first_note_id_=chart.source.chart_sha256+'#'+first_note_pointer_;
        require(note.source.id==first_note_id_&&note.source.json_pointer==first_note_pointer_&&note.kind==NoteKind::tap&&
            note.side==NoteSide::above&&note.start_ns==note_time_&&note.position_x==-4&&note.speed==1&&
            std::isfinite(note.floor_position)&&std::abs(note.floor_position)<=1e9,
            "first-note identity/geometry changed");
        for(const auto& current:line.notes) {
            require(current.start_ns>=note_time_&&text(current.source.json_pointer)&&
                current.source.id==chart.source.chart_sha256+'#'+current.source.json_pointer&&
                note_pointers_.emplace(current.source.id,current.source.json_pointer).second,"invalid native note ownership");
        }
        Ns cursor=0;bool last_positive=false;unsigned positive_run=0,visible_run=0;Range floor;
        for(const auto& e:line.speed_events) {
            require(e.time.start_ns==cursor&&e.time.end_ns>cursor&&e.time.end_ns<=safe&&std::isfinite(e.value)&&std::abs(e.value)<=1e6&&!e.floor_position&&
                text(e.source.json_pointer),"native first-note speed coverage invalid");
            const auto begin=std::max(model.begin_ns,e.time.start_ns),end=std::min(model.end_ns,e.time.end_ns);
            if(begin<end) {
                // The first-note y is affine within a speed piece. Enclose both
                // endpoints (including the right boundary conservatively), all
                // declared scale/pose uncertainty and detector pixel error.
                const auto start_floor=add(floor,increment(e.value,begin-e.time.start_ns));
                const auto end_floor=add(floor,increment(e.value,end-e.time.start_ns));
                const Range delta{down(std::min(start_floor.lo,end_floor.lo)-note.floor_position),
                    up(std::max(start_floor.hi,end_floor.hi)-note.floor_position)};
                const auto distance=multiply({model.approach_scale_lower,model.approach_scale_upper},delta);
                const auto error=model.pose_tolerance_pixels+model.pixel_error;
                const auto y=add({model.origin_y-error,model.origin_y+error},distance);
                const bool visible=y.hi>=290&&y.lo<=565;
                require(pieces_.emplace(e.source.json_pointer,Piece{{begin,end-1},e.value,visible}).second,"duplicate native speed pointer");
                if(e.value>0&&!last_positive)++positive_run;
                if(e.value>0&&visible) {
                    require(!visible_run||visible_run==positive_run,"earlier visible positive native approach remains ambiguous");
                    visible_run=positive_run;
                }
                last_positive=e.value>0;
            }
            cursor=e.time.end_ns;if(cursor>=model.end_ns)break;
            floor=add(floor,increment(e.value,e.time.end_ns-e.time.start_ns));
        }
        require(cursor>=model.end_ns&&visible_run&&visible_run==positive_run&&last_positive,"native domain must end in one unique visible positive approach");
    }
    FirstNoteTriggerStatus observe(const VisualCandidateResult& r,bool ui_healthy,Ns now) {
        if(!health(now,ui_healthy))return value_;
        if(!provenance(r,now)){cancel("invalid_or_stale_first_note_provenance");return value_;}
        const auto& f=r.observation;const auto current_context=context(f);
        if(!context_.empty()&&context_!=current_context){cancel("first_note_source_epoch_or_geometry_changed");return value_;}
        if(f.sequence<=last_sequence_||f.received_host_ns<=last_received_||evidence_ids_.contains(f.evidence_id)||frame_ids_.contains(f.frame_id)) {
            cancel("replayed_or_nonmonotonic_first_note_frame");return value_;
        }
        if(evidence_ids_.size()>=512){cancel("first_note_observation_budget");return value_;}
        context_=current_context;last_sequence_=f.sequence;last_received_=f.received_host_ns;
        evidence_ids_.insert(f.evidence_id);frame_ids_.insert(f.frame_id);
        std::map<std::string,const VisualCenter*> centers;
        const VisualCenter* lowest=nullptr;
        for(const auto& center:f.centers) {
            if(!text(center.id,128)||!std::isfinite(center.x)||!std::isfinite(center.y)||center.x<0||center.x>=1280||center.y<0||center.y>=720||
               !centers.emplace(center.id,&center).second){cancel("invalid_first_note_centers");return value_;}
            if(!lowest||center.y>lowest->y)lowest=&center;
        }
        value_.raw_candidates.clear();value_.raw_candidate_count=r.candidates.size();
        value_.first_note_positive_candidates=0;value_.selected_intervals.clear();value_.anchor.reset();
        std::vector<VisualTimeInterval> selected;
        for(const auto& candidate:r.candidates) {
            const auto piece=pieces_.find(candidate.speed_event_json_pointer);
            if(piece==pieces_.end()||candidate.line_speed!=piece->second.speed||
               candidate.song_time.lower>candidate.song_time.upper||
               candidate.song_time.lower<piece->second.interval.lower||candidate.song_time.upper>piece->second.interval.upper||
               candidate.matches.size()!=f.centers.size()) {cancel("candidate_does_not_match_native_speed_piece");return value_;}
            std::set<std::string> matched_centers,matched_notes;bool first_matches_lowest=false;
            for(const auto& match:candidate.matches) {
                const auto native=note_pointers_.find(match.note_id);
                if(!centers.contains(match.center_id)||native==note_pointers_.end()||native->second!=match.note_json_pointer||
                   !matched_centers.insert(match.center_id).second||!matched_notes.insert(match.note_id).second) {
                    cancel("candidate_note_identity_or_assignment_invalid");return value_;
                }
                if(lowest&&match.center_id==lowest->id&&match.note_id==first_note_id_)first_matches_lowest=true;
            }
            value_.raw_candidates.push_back(candidate);
            if(first_matches_lowest&&candidate.line_speed>0&&piece->second.first_note_visible) {
                ++value_.first_note_positive_candidates;selected.push_back(candidate.song_time);
            }
        }
        value_.selected_intervals=merge(std::move(selected));
        if(!lowest||std::abs(lowest->x-352)>4||lowest->y<290||lowest->y>565) {
            reset("waiting_for_lowest_first_note_in_approach_band");return value_;
        }
        for(const auto& center:f.centers)if(center.id!=lowest->id&&lowest->y-center.y<=6) {
            reset("lowest_note_center_ambiguous");return value_;
        }
        if(value_.selected_intervals.size()!=1||value_.selected_intervals[0].upper-value_.selected_intervals[0].lower>40*ms) {
            reset("first_note_positive_branch_missing_or_ambiguous");return value_;
        }
        Support current{value_.selected_intervals[0],f,lowest->y};
        const auto previous=std::move(previous_);previous_=current;value_.support_count=1;
        value_.state=FirstNoteTriggerState::acquiring;value_.reason="waiting_for_second_downward_first_note_frame";
        if(!previous)return value_;
        const auto dt=f.received_host_ns-previous->frame.received_host_ns;
        if(dt<20*ms||dt>200*ms||f.frame_sha256==previous->frame.frame_sha256||
           current.y-previous->y<=6||current.song.lower<=previous->song.upper) {
            value_.reason="first_note_motion_not_distinct_downward";return value_;
        }
        // Two receipt samples can each have the explicitly assumed +/-30ms
        // arrival phase spread. This consistency check is not a clock fit.
        const auto delta_lower=current.song.lower-previous->song.upper;
        const auto delta_upper=current.song.upper-previous->song.lower;
        if(dt<delta_lower-60*ms||dt>delta_upper+60*ms) {
            value_.reason="first_note_motion_inconsistent_with_rate_one_hypothesis";return value_;
        }
        value_.support_count=2;
        if(current.y<300){value_.reason="downward_first_note_seen_waiting_for_y300";return value_;}
        FirstNoteTriggerAnchor anchor;anchor.received_host_ns=f.received_host_ns;
        anchor.chosen_song_interval=current.song;anchor.origin_host_ns=f.received_host_ns-midpoint(current.song);
        anchor.target_host_ns=anchor.origin_host_ns+note_time_+phase_;anchor.evidence_id=f.evidence_id;
        anchor.supporting_evidence_ids={previous->frame.evidence_id,f.evidence_id};
        if(anchor.origin_host_ns<0||anchor.target_host_ns>safe||anchor.target_host_ns-now<200*ms) {
            cancel("first_note_dispatch_lead_below_200ms");return value_;
        }
        value_.anchor=std::move(anchor);value_.state=FirstNoteTriggerState::ready;
        value_.reason="two_downward_first_note_matches_on_unique_native_positive_approach";return value_;
    }
    FirstNoteTriggerStatus status(Ns now,bool ui_healthy=true) {health(now,ui_healthy);return value_;}
    std::optional<FirstNoteTriggerAnchor> take_anchor(Ns now) {
        if(consumed_||!health(now,true)||value_.state!=FirstNoteTriggerState::ready||!value_.anchor)return std::nullopt;
        consumed_=true;value_.state=FirstNoteTriggerState::fired;value_.reason="first_note_trigger_fired_once";return value_.anchor;
    }
    void cancel(std::string reason) {
        if(value_.state==FirstNoteTriggerState::cancelled)return;
        value_.state=FirstNoteTriggerState::cancelled;value_.reason=reason.substr(0,512);previous_.reset();
    }
};
} // namespace rhythm::experimental
