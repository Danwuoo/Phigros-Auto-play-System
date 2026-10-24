#include "../apps/chart_calibrate/anchor_monitor.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using namespace rhythm;
using namespace rhythm::calibration;
constexpr SongNs ms=1'000'000,S=1'000'000'000,O=100*S,T=11*S,R=O+T,A=R+1;
unsigned checks=0,groups=0;
void check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
void aliases(VisualCandidateResult& r,std::vector<VisualTimeInterval> intervals) {
    r.song_intervals=std::move(intervals);r.candidates.clear();
    for(auto i:r.song_intervals) {VisualTimeCandidate c;c.song_time=i;r.candidates.push_back(c);}
    r.state=r.song_intervals.size()==1?VisualCandidateState::conditional_candidate:VisualCandidateState::ambiguous;
}
VisualCandidateResult observation(unsigned seq,SongNs receipt,SongNs residual=0,SongNs half_width=4*ms) {
    VisualCandidateResult r;
    r.model_id="synthetic-opening-model";r.calibration_id="unqualified-test";
    r.model_sha256=std::string(64,'a');r.calibration_sha256=std::string(64,'b');
    const auto song=receipt-O+residual;aliases(r,{{song-half_width,song+half_width}});
    auto& f=r.observation;f.sequence=seq;f.epoch=f.generation=f.geometry_version=1;
    f.width=1280;f.height=720;f.source_valid=true;f.clock_kind=VisualSourceClock::arrival_only;
    f.received_host_ns=receipt;f.source_id="test-stream";f.clock_id="QPC-receipt-only";
    f.opening_context_id="actual-round-1";f.source_sha256=std::string(64,'c');
    f.frame_id="frame-"+std::to_string(seq);f.evidence_id="proof-"+std::to_string(seq);
    f.frame_sha256=std::string(64,"0123456789abcdef"[seq%16]);
    f.centers_complete_unoccluded=true;f.centers={{"center-1",352,400}};
    r.supporting_evidence_ids={f.evidence_id};return r;
}
AnchorMonitor monitor() {return AnchorMonitor(observation(1,R),O,A);}
bool cancelled(const AnchorMonitorStatus& s) {return s.state==AnchorMonitorState::cancelled;}
bool active(const AnchorMonitorStatus& s) {return s.state==AnchorMonitorState::monitoring;}
}
int main() {
try {
    ++groups;{
        auto m=monitor();auto s=m.status(A);
        check(active(s)&&s.matching_alias_count==1&&s.observation_count==1,"actual anchor not accepted");
        check(s.fixed_origin_ns==O&&s.original_arm_ns==A&&s.predicted_song_ns==T,"fixed origin/anchor receipt changed");
        check(s.aliases.size()==1&&s.aliases[0].residual_interval.lower==-4*ms&&
              s.aliases[0].residual_interval.upper==4*ms&&s.aliases[0].signed_nearest_residual_ns==0,"signed residual mismatch");
        check(s.matched_song_interval&&s.last_actual_receipt_ns==R&&s.last_evidence_id=="proof-1","matched evidence missing");
        check(s.experiment_only&&!s.live_qualified&&!s.timing_qualified,"experimental consistency promoted to qualification");
        s=m.status(A+50*ms);
        check(s.predicted_song_ns==T&&s.observation_count==1&&s.last_actual_receipt_ns==R,"poll forged a fresh observation");
    }
    ++groups;{
        // Rounded diagnostic residual pattern of live005, not an original-frame
        // replay or game outcome assertion. All values use fresh receipt time.
        const SongNs residuals[]={4,-5,-13,-28,-22,-10,12,8};
        auto m=monitor();unsigned seq=2;
        for(auto residual:residuals) {
            auto r=observation(seq,R+static_cast<SongNs>(seq-1)*30*ms,residual*ms);
            const auto s=m.observe(r,r.observation.received_host_ns+ms);
            check(active(s)&&s.matching_alias_count==1,"005-type bounded residual cancelled");
            check(s.fixed_origin_ns==O&&s.original_arm_ns==A,"accepted frame moved origin or arm");++seq;
        }
    }
    ++groups;{
        // Rounded growing residual from live006: +29,+45,+65,...,+120ms.
        // At +65ms, nearest point of a +/-4ms interval is +61ms: reject.
        auto m=monitor();unsigned seq=2;
        for(const auto residual:{29,45}) {
            const auto receipt=R+static_cast<SongNs>(seq-1)*30*ms;
            check(active(m.observe(observation(seq,receipt,residual*ms),receipt+ms)),"006-type early residual prematurely rejected");++seq;
        }
        auto s=m.observe(observation(4,R+90*ms,65*ms),R+91*ms);
        check(cancelled(s)&&s.reason=="no_alias_consistent_with_fixed_origin"&&s.matching_alias_count==0,"growing006 residual did not cancel");
        check(s.aliases[0].signed_nearest_residual_ns==61*ms&&s.predicted_song_ns==T+90*ms,"failed residual diagnostics lost");
        s=m.observe(observation(5,R+120*ms),R+121*ms);
        check(cancelled(s)&&s.observation_count==4&&s.fixed_origin_ns==O&&s.original_arm_ns==A,"later convenient frame revived or retimed monitor");
    }
    ++groups;{
        for(const auto sign:{-1,1}) {
            auto exact=observation(1,R,sign*60*ms,0);AnchorMonitor good(exact,O,A);
            check(active(good.status(A)),"exact60ms residual rejected");
            auto outside=observation(1,R,sign*(60*ms+1),0);AnchorMonitor bad(outside,O,A);
            check(cancelled(bad.status(A)),"outside60ms residual accepted");
        }
        auto exact=observation(1,R,0,40*ms);AnchorMonitor good(exact,O,A);
        check(active(good.status(A)),"exact80ms interval rejected");
        exact.song_intervals[0].upper+=1;exact.candidates[0].song_time=exact.song_intervals[0];
        AnchorMonitor bad(exact,O,A);check(bad.status(A).reason=="consistent_alias_too_wide","wide interval accepted");
    }
    ++groups;{
        auto raw=observation(1,R);aliases(raw,{{T-500*ms,T-490*ms},{T-4*ms,T+4*ms},{T+500*ms,T+510*ms}});
        AnchorMonitor m(raw,O,A);auto s=m.status(A);
        check(active(s)&&s.aliases.size()==3&&s.matching_alias_count==1,"distant aliases discarded or falsely ambiguous");
        check(s.aliases[0].signed_nearest_residual_ns==-490*ms&&s.aliases[2].signed_nearest_residual_ns==500*ms,"all alias diagnostics missing");
        aliases(raw,{{T-50*ms,T-40*ms},{T+40*ms,T+50*ms}});AnchorMonitor ambiguous(raw,O,A);
        s=ambiguous.status(A);check(cancelled(s)&&s.matching_alias_count==2&&s.reason=="multiple_aliases_consistent_with_fixed_origin","two near aliases accepted");
        aliases(raw,{{T-100*ms,T-10*ms},{T+10*ms,T+20*ms}});AnchorMonitor wide_and_narrow(raw,O,A);
        s=wide_and_narrow.status(A);
        check(cancelled(s)&&s.matching_alias_count==2,"wide competing alias filtered to create uniqueness");
        aliases(raw,{{T-4*ms,T+4*ms}});raw.candidates.push_back(raw.candidates[0]);
        AnchorMonitor duplicate_assignment(raw,O,A);
        check(active(duplicate_assignment.status(A)),"same time union from multiple assignments falsely ambiguous");
        // Match the producer's exact union: adjacent integer intervals remain
        // distinct. The monitor must not merge away a one-nanosecond boundary.
        aliases(raw,{{T-4*ms,T},{T+1,T+4*ms}});AnchorMonitor adjacent(raw,O,A);
        s=adjacent.status(A);
        check(cancelled(s)&&s.matching_alias_count==2&&s.reason=="multiple_aliases_consistent_with_fixed_origin","adjacent aliases merged to create uniqueness");
    }
    ++groups;{
        auto m=monitor();check(active(m.status(R+100*ms-1)),"receipt expired before100ms");
        check(m.status(R+100*ms).reason=="previous_actual_receipt_expired","exact100ms old receipt survived");
        auto gap=monitor();auto next=observation(2,R+100*ms);
        check(gap.observe(next,R+100*ms).reason=="previous_actual_receipt_expired","fresh frame revived unpolled old expiry gap");
        auto edge=monitor();next=observation(2,R+99*ms);
        check(active(edge.observe(next,R+99*ms)),"fresh frame just before expiry refused");
        check(active(edge.status(R+198*ms)),"accepted actual observation did not update freshness");
        check(cancelled(edge.status(R+199*ms)),"fresh receipt extended to exact100ms");
        AnchorMonitor stale(observation(1,R),O,R+100*ms);
        check(stale.status(R+100*ms).reason=="invalid_or_stale_actual_receipt","stale constructor anchor accepted");
        AnchorMonitor future(observation(1,R+2),O,A);
        check(cancelled(future.status(A)),"future constructor anchor accepted");
    }
    ++groups;{
        const auto begin=O+S,arm=begin+1;
        AnchorMonitor m(observation(1,begin),O,arm);
        for(unsigned seq=2;seq<=60;++seq) {
            const auto receipt=begin+static_cast<SongNs>(seq-1)*50*ms;
            check(active(m.observe(observation(seq,receipt),receipt+1)),"fresh samples failed before3s horizon");
        }
        check(m.status(arm+3*S).reason=="original_arm_horizon_expired","new samples renewed original3s horizon");
        AnchorMonitor end_scope(observation(1,O+11*S+950*ms),O,O+11*S+950*ms+1);
        check(active(end_scope.status(O+12*S-1)),"pre12s scope boundary rejected");
        check(end_scope.status(O+12*S).reason=="outside_pre_first_judgment_scope","12s scope silently extended");
        auto raw=observation(1,R);aliases(raw,{{12*S-1,12*S}});AnchorMonitor outside(raw,O,A);
        check(cancelled(outside.status(A)),"candidate at12s accepted");
    }
    ++groups;{
        const std::vector<std::function<void(VisualCandidateResult&)>> changes={
            [](auto& r){++r.observation.epoch;},[](auto& r){++r.observation.generation;},
            [](auto& r){++r.observation.geometry_version;},[](auto& r){r.observation.source_id="other";},
            [](auto& r){r.observation.source_sha256=std::string(64,'d');},[](auto& r){r.observation.clock_id="other";},
            [](auto& r){r.observation.opening_context_id="other";},[](auto& r){r.model_id="other";},
            [](auto& r){r.model_sha256=std::string(64,'d');},[](auto& r){r.calibration_id="other";},
            [](auto& r){r.calibration_sha256=std::string(64,'d');}
        };
        for(const auto& mutate:changes) {
            auto m=monitor();auto raw=observation(2,R+50*ms);mutate(raw);
            auto s=m.observe(raw,R+51*ms);
            check(cancelled(s)&&s.reason=="source_epoch_geometry_or_model_changed","identity change accepted");
            check(cancelled(m.observe(observation(3,R+60*ms),R+61*ms)),"identity restored cancelled monitor");
        }
        auto m=monitor();auto same_pixels=observation(2,R+50*ms);
        same_pixels.observation.frame_sha256=observation(1,R).observation.frame_sha256;same_pixels.unchanged_pixels=true;
        check(active(m.observe(same_pixels,R+51*ms)),"receipt-only model silently demanded changed render identity");
    }
    ++groups;{
        const std::vector<std::function<void(VisualCandidateResult&)>> invalid={
            [](auto& r){r.state=VisualCandidateState::unknown;},[](auto& r){r.state=VisualCandidateState::rejected;},
            [](auto& r){r.requires_new_epoch=true;},[](auto& r){r.timing_qualified=true;},[](auto& r){r.live_permitted=true;},
            [](auto& r){r.candidates.clear();},[](auto& r){r.song_intervals.clear();},
            [](auto& r){r.song_intervals[0].lower=-1;},[](auto& r){r.song_intervals[0].upper=r.song_intervals[0].lower-1;},
            [](auto& r){r.candidates[0].song_time.upper=12*S;},[](auto& r){r.observation.source_valid=false;},
            [](auto& r){r.observation.width=1279;},[](auto& r){r.observation.height=719;},
            [](auto& r){r.observation.clock_kind=VisualSourceClock::video_pts;},[](auto& r){r.observation.source_time_ns=0;},
            [](auto& r){r.observation.capture_host_interval=VisualTimeInterval{1,2};},
            [](auto& r){r.observation.source_sha256="bad";},[](auto& r){r.observation.frame_sha256=std::string(64,'X');},
            [](auto& r){r.observation.frame_id.clear();},[](auto& r){r.observation.evidence_id=std::string(513,'x');},
            [](auto& r){r.observation.sequence=0;},[](auto& r){r.observation.epoch=0;},
            [](auto& r){r.observation.generation=0;},[](auto& r){r.observation.geometry_version=0;},
            [](auto& r){r.observation.centers_complete_unoccluded=false;},[](auto& r){r.observation.centers.clear();},
            [](auto& r){r.observation.centers[0].x=std::numeric_limits<double>::quiet_NaN();},
            [](auto& r){r.observation.centers[0].y=570;},[](auto& r){r.observation.centers.push_back(r.observation.centers[0]);},
            [](auto& r){r.supporting_evidence_ids.clear();},[](auto& r){r.supporting_evidence_ids={"other"};},
            [](auto& r){r.conditional_source_minus_song_intervals={{1,2}};}
        };
        for(const auto& mutate:invalid) {
            auto raw=observation(1,R);mutate(raw);AnchorMonitor m(raw,O,A);
            check(cancelled(m.status(A)),"invalid actual result accepted");
        }
    }
    ++groups;{
        auto raw=observation(1,R);VisualTimeCandidate other;other.song_time={T+100*ms,T+110*ms};raw.candidates.push_back(other);
        AnchorMonitor missing(raw,O,A);
        check(missing.status(A).reason=="candidate_alias_union_mismatch","unlisted candidate alias ignored");
        raw=observation(1,R);raw.song_intervals.push_back(raw.song_intervals[0]);raw.state=VisualCandidateState::ambiguous;
        AnchorMonitor duplicate(raw,O,A);check(cancelled(duplicate.status(A)),"duplicate alias accepted");
        raw=observation(1,R);aliases(raw,{{T+100*ms,T+110*ms},{T-4*ms,T+4*ms}});
        AnchorMonitor unsorted(raw,O,A);check(cancelled(unsorted.status(A)),"unsorted alias list silently canonicalized");
        raw=observation(1,R);raw.state=VisualCandidateState::ambiguous;
        AnchorMonitor state_mismatch(raw,O,A);check(cancelled(state_mismatch.status(A)),"wrong candidate state accepted");
        raw=observation(1,R);raw.candidates.resize(1025,raw.candidates[0]);
        AnchorMonitor work_budget(raw,O,A);check(cancelled(work_budget.status(A)),"candidate work budget ignored");
        raw=observation(1,R);raw.song_intervals.resize(65,raw.song_intervals[0]);
        AnchorMonitor alias_budget(raw,O,A);check(cancelled(alias_budget.status(A)),"alias budget ignored");
    }
    ++groups;{
        auto m=monitor();check(active(m.observe(observation(2,R+50*ms),R+51*ms)),"replay setup failed");
        auto replay=observation(3,R+60*ms);replay.observation.frame_id="frame-1";
        check(m.observe(replay,R+61*ms).reason=="replayed_actual_observation","nonlatest frame replay accepted");
        auto proof=monitor();replay=observation(2,R+50*ms);replay.observation.evidence_id="proof-1";replay.supporting_evidence_ids={"proof-1"};
        check(proof.observe(replay,R+51*ms).reason=="replayed_actual_observation","evidence ID replay accepted");
        auto sequence=monitor();check(cancelled(sequence.observe(observation(1,R+50*ms),R+51*ms)),"nonincreasing sequence accepted");
        auto receipt=monitor();check(cancelled(receipt.observe(observation(2,R),R+ms)),"nonincreasing receipt accepted");
        auto clock=monitor();check(clock.status(A-1).reason=="host_clock_invalid_or_rollback","host rollback accepted");
        auto future=monitor();check(cancelled(future.observe(observation(2,R+60*ms),R+50*ms)),"future new receipt accepted");
        auto negative=monitor();check(cancelled(negative.status(-1)),"negative host time accepted");
        const auto huge=std::numeric_limits<SongNs>::max();
        AnchorMonitor bad_origin(observation(1,R),huge,A),bad_arm(observation(1,R),O,huge);
        check(cancelled(bad_origin.status(A))&&cancelled(bad_arm.status(A)),"extreme constructor times accepted");
        auto huge_now=monitor();check(cancelled(huge_now.status(huge)),"overflowing host time accepted");
    }
    ++groups;{
        auto m=monitor();
        for(unsigned seq=2;seq<=256;++seq) {
            const auto receipt=R+static_cast<SongNs>(seq-1)*ms;
            check(active(m.observe(observation(seq,receipt),receipt+1)),"bounded observation budget ended early");
        }
        const auto s=m.observe(observation(257,R+256*ms),R+256*ms+1);
        check(cancelled(s)&&s.observation_count==256&&s.reason=="actual_observation_budget","observation budget exceeded");
    }
    std::cout<<groups<<" groups / "<<checks<<" anchor monitor checks passed; pure experimental consistency, no device or timing qualification.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
