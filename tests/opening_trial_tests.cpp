#include "../apps/chart_calibrate/opening_trial.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace rhythm;
using namespace rhythm::calibration;
constexpr SongNs ms=1'000'000,S=1'000'000'000,O=100*S,ARM=O+11*S+101*ms;
constexpr std::array<SongNs,3> native_times{12'000'000'000,12'171'428'571,12'514'285'714};
unsigned checks=0,groups=0;
void check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
VisualCandidateResult candidate(unsigned seq,SongNs song) {
    VisualCandidateResult r;r.state=VisualCandidateState::conditional_candidate;
    r.model_id="synthetic-opening";r.calibration_id="unqualified-test";
    r.model_sha256=std::string(64,'a');r.calibration_sha256=std::string(64,'b');
    r.song_intervals={{song-5*ms,song+5*ms}};r.candidates.resize(1);
    auto& f=r.observation;f.sequence=seq;f.epoch=f.generation=f.geometry_version=1;
    f.width=1280;f.height=720;f.source_valid=true;f.clock_kind=VisualSourceClock::arrival_only;
    f.received_host_ns=O+song;f.source_id="synthetic-stream";f.clock_id="QPC-arrival-only";
    f.opening_context_id="actual-round-1";f.source_sha256=std::string(64,'c');
    f.frame_id="frame-"+std::to_string(seq);f.evidence_id="proof-"+std::to_string(seq);
    f.frame_sha256=std::string(64,"0123456789abcdef"[seq]);return r;
}
TrialStatus anchor(SongNs phase=0) {
    FirstTapTrial first(phase);
    for(unsigned i=1;i<=3;++i) {
        auto r=candidate(i,11*S+static_cast<SongNs>(i-1)*50*ms);
        first.observe(r,true,r.observation.received_host_ns+ms);
    }
    check(first.arm(ARM),"synthetic FirstTap anchor failed");return first.status(ARM,true);
}
OpeningUiEvidence ui(std::uint64_t seq,SongNs receipt) {
    return {seq,1,1,1,receipt,"synthetic-stream","ui-"+std::to_string(seq),true,true};
}
struct Harness {
    OpeningTrial trial;
    SongNs now=ARM,phase=0;
    std::uint64_t seq=1;
    explicit Harness(SongNs p=0):phase(p) {
        check(trial.arm(anchor(p),ARM,ARM,ui(seq,ARM)),"three-Tap arm failed");
    }
    void advance(SongNs target) {
        check(target>=now,"test clock rollback");
        while(now+50*ms<target) {now+=50*ms;trial.observe_ui(ui(++seq,now),now);}
        if(target>now) {now=target;trial.observe_ui(ui(++seq,now),now);}
    }
    SongNs due(std::size_t index) const {return O+native_times[index]+phase;}
    OpeningTrialAction down(std::size_t index) {
        advance(due(index));const auto command=trial.claim_down(now);
        check(command.has_value(),"due DOWN rejected");return *command;
    }
    void finish_tap(std::size_t index) {
        const auto command=down(index);
        check(command.tap_index==index&&command.contact_id==0&&command.phase==OpeningTrialPhase::down,"DOWN identity/phase mismatch");
        check(command.scheduled_host_ns==due(index),"phase/time applied incorrectly");
        check(command.x==(index==2?928:352)&&command.y==576,"opening target mismatch");
        now+=ms;trial.note_down_return(now,true);
        check(!trial.claim_up(now+15*ms),"UP before actual return+16ms");
        now+=16*ms;const auto up=trial.claim_up(now);
        check(up&&up->tap_index==index&&up->phase==OpeningTrialPhase::up&&up->contact_id==0,"UP identity/phase mismatch");
        check(up->x==command.x&&up->y==command.y&&up->scheduled_host_ns==now,"UP coordinate/due mismatch");
        now+=ms;trial.note_up_return(now,true);
    }
};
template<class F> void group(F body) {++groups;body();}
}
int main() {
try {
    group([] {
        Harness h;
        for(std::size_t i=0;i<3;++i)h.finish_tap(i);
        auto s=h.trial.status(h.now);
        check(s.state==OpeningTrialState::completed&&s.completed_taps==3&&s.down_attempts==3,"three-Tap completion/count wrong");
        check(s.requires_release&&s.experiment_only&&!s.live_qualified,"RPC completion falsely cleared release/qualified game");
        check(!h.trial.claim_down(h.now)&&!h.trial.claim_up(h.now),"fourth DOWN/UP allowed");
        h.trial.cancel("cleanup stop");check(h.trial.status(h.now).state==OpeningTrialState::completed,"completed first reason changed");
    });
    group([] {
        for(const auto phase:{-100*ms,100*ms}) {
            Harness h(phase);for(std::size_t i=0;i<3;++i)h.finish_tap(i);
            check(h.trial.status(h.now).completed_taps==3,"phase endpoint failed");
        }
        const auto notes=OpeningTrial::notes();
        for(std::size_t i=0;i<3;++i)check(notes[i].song_ns==native_times[i],"native ns rounding changed");
    });
    group([] {
        Harness h;h.advance(h.due(0)-1);check(!h.trial.claim_down(h.now),"early DOWN allowed");
        h.advance(h.due(0)+20*ms);check(h.trial.claim_down(h.now).has_value(),"exact20ms DOWN rejected");
        Harness late;late.advance(late.due(0)+20*ms+1);
        check(!late.trial.claim_down(late.now)&&late.trial.status(late.now).requires_release,"late DOWN allowed or release obligation lost");
    });
    group([] {
        Harness h;h.down(0);h.now+=ms;h.trial.note_down_return(h.now,true);
        check(h.trial.claim_up(h.now+36*ms).has_value(),"exact20ms UP lateness rejected");
        Harness late;late.down(0);late.now+=ms;late.trial.note_down_return(late.now,true);
        check(!late.trial.claim_up(late.now+36*ms+1),"late UP allowed");
        check(late.trial.status(late.now+37*ms).state==OpeningTrialState::cancelled,"late UP not cancelled");
    });
    group([] {
        Harness h;h.down(0);
        check(!h.trial.claim_down(h.now),"duplicate DOWN allowed");
        auto s=h.trial.status(h.now);check(s.state==OpeningTrialState::cancelled&&s.down_attempts==1&&s.requires_release,"duplicate DOWN not fail-closed");
        Harness up;up.down(0);up.now+=ms;up.trial.note_down_return(up.now,true);up.now+=16*ms;
        check(up.trial.claim_up(up.now).has_value()&&!up.trial.claim_up(up.now),"duplicate UP allowed");
        check(up.trial.status(up.now).state==OpeningTrialState::cancelled,"duplicate UP did not cancel");
    });
    group([] {
        Harness missing;missing.down(0);missing.now+=ms;missing.trial.note_down_return(missing.now,true);
        check(!missing.trial.claim_down(missing.now+20*ms),"next DOWN skipped UP");
        check(missing.trial.status(missing.now+21*ms).down_attempts==1,"missing UP consumed another DOWN");
        Harness wrong;wrong.trial.note_up_return(wrong.now,true);
        check(wrong.trial.status(wrong.now).state==OpeningTrialState::cancelled,"unsolicited UP receipt accepted");
    });
    group([] {
        Harness down;down.down(0);down.now+=ms;down.trial.note_down_return(down.now,false);
        auto s=down.trial.status(down.now);check(s.state==OpeningTrialState::cancelled&&s.requires_release,"unknown DOWN did not require release");
        check(!down.trial.claim_down(down.now+ms),"failed DOWN retried");
        Harness up;up.down(0);up.now+=ms;up.trial.note_down_return(up.now,true);up.now+=16*ms;up.trial.claim_up(up.now);up.now+=ms;up.trial.note_up_return(up.now,false);
        s=up.trial.status(up.now);check(s.state==OpeningTrialState::cancelled&&s.completed_taps==0&&s.requires_release,"unknown UP advanced sequence");
    });
    group([] {
        Harness h;const auto s=h.trial.status(ARM+50*ms);
        check(s.ui_sequence==1&&s.down_attempts==0,"poll created evidence/action");
        check(h.trial.status(ARM+100*ms).state==OpeningTrialState::cancelled,"exact100ms stale UI survived");
        Harness gap;gap.trial.observe_ui(ui(2,ARM+100*ms),ARM+100*ms);
        check(gap.trial.status(ARM+100*ms).state==OpeningTrialState::cancelled,"fresh UI revived an unpolled expired lease");
    });
    group([] {
        Harness h;h.trial.observe_ui(ui(1,ARM),ARM+50*ms);
        check(h.trial.status(ARM+99*ms).ui_sequence==1,"same sequence became new UI evidence");
        check(h.trial.status(ARM+100*ms).state==OpeningTrialState::cancelled,"same sequence refreshed receipt");
        Harness replay;auto reused=ui(2,ARM+ms);reused.evidence_id="ui-1";
        replay.trial.observe_ui(reused,ARM+ms);
        check(replay.trial.status(ARM+ms).state==OpeningTrialState::cancelled,"source evidence replay accepted");
    });
    group([] {
        for(unsigned field=0;field<6;++field) {
            Harness h;auto changed=ui(2,ARM+ms);
            if(field==0)++changed.epoch;if(field==1)++changed.generation;if(field==2)++changed.geometry_version;
            if(field==3)changed.source_id="other";if(field==4)changed.playing=false;if(field==5)changed.source_valid=false;
            h.trial.observe_ui(changed,ARM+ms);
            check(h.trial.status(ARM+ms).state==OpeningTrialState::cancelled,"UI identity/loss did not cancel");
        }
    });
    group([] {
        Harness h;h.down(0);h.trial.observe_ui(ui(++h.seq,h.now+ms),h.now+ms);
        auto invalid=ui(++h.seq,h.now+2*ms);invalid.playing=false;h.trial.observe_ui(invalid,h.now+2*ms);
        const auto reason=h.trial.status(h.now+2*ms).reason;h.trial.cancel("later stop");
        auto s=h.trial.status(h.now+3*ms);
        check(s.requires_release&&s.state==OpeningTrialState::cancelled&&s.reason==reason,"held UI loss lost sticky release/reason");
        Harness rollback;check(rollback.trial.status(ARM-1).state==OpeningTrialState::cancelled,"host rollback survived");
    });
    group([] {
        OpeningTrial future;auto bad=ui(1,ARM+1);
        check(!future.arm(anchor(),ARM,ARM,bad),"future UI accepted");
        OpeningTrial old;check(!old.arm(anchor(),ARM,ARM+100*ms,ui(1,ARM)),"stale UI armed");
        OpeningTrial horizon;check(!horizon.arm(anchor(),ARM-2*S,ARM,ui(1,ARM)),"original3s horizon was restarted by sequence");
        OpeningTrial completed;auto first=anchor();first.state=FirstTapState::completed;first.requires_release=true;
        check(!completed.arm(first,ARM,ARM,ui(1,ARM)),"already played first Tap could be played again");
    });
    group([] {
        for(unsigned field=0;field<5;++field) {
            auto first=anchor();
            if(field==0)first.live_qualified=true;if(field==1)first.latest_origin_ns.reset();
            if(field==2)first.provisional_origin_intervals.push_back(first.provisional_origin_intervals[0]);
            if(field==3)first.provisional_origin_intervals[0]={std::numeric_limits<SongNs>::min(),std::numeric_limits<SongNs>::max()};
            if(field==4)first.target_host_ns=*first.target_host_ns+101*ms;
            OpeningTrial t;check(!t.arm(first,ARM,ARM,ui(1,ARM)),"invalid/ambiguous/overflow anchor accepted");
        }
    });
    group([] {
        Harness h;h.down(0);h.trial.note_down_return(h.now-1,true);
        check(h.trial.status(h.now).state==OpeningTrialState::cancelled,"DOWN receipt before claim accepted");
        Harness up;up.down(0);up.now+=ms;up.trial.note_down_return(up.now,true);up.now+=16*ms;up.trial.claim_up(up.now);
        up.trial.note_up_return(up.now-1,true);check(up.trial.status(up.now).state==OpeningTrialState::cancelled,"UP receipt before claim accepted");
    });
    group([] {
        Harness h;
        for(unsigned i=2;i<=1025;++i) {const auto now=ARM+static_cast<SongNs>(i)*1000;h.trial.observe_ui(ui(i,now),now);}
        check(h.trial.status(ARM+1'026'000).state==OpeningTrialState::cancelled,"UI evidence budget unbounded");
    });
    std::cout<<groups<<" groups / "<<checks<<" opening three-Tap checks passed; offline synthetic contract only.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
