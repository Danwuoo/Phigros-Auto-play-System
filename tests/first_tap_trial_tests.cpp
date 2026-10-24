#include "../apps/chart_calibrate/first_tap_trial.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace rhythm;
using namespace rhythm::calibration;
constexpr SongNs ms=1'000'000,S=1'000'000'000,O=100*S;
unsigned checks=0;
void check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
VisualCandidateResult observation(unsigned seq,SongNs song,SongNs receipt=-1) {
    VisualCandidateResult r;r.state=VisualCandidateState::conditional_candidate;
    r.model_id="synthetic-model";r.calibration_id="unqualified-test";
    r.model_sha256=std::string(64,'a');r.calibration_sha256=std::string(64,'b');
    r.song_intervals={{song-5*ms,song+5*ms}};r.candidates.resize(1);
    auto& f=r.observation;f.sequence=seq;f.epoch=f.generation=f.geometry_version=1;
    f.width=1280;f.height=720;f.source_valid=true;f.clock_kind=VisualSourceClock::arrival_only;
    f.received_host_ns=receipt<0?O+song:receipt;f.source_id="test-stream";f.clock_id="QPC-receipt-only";
    f.opening_context_id="actual-round-1";f.source_sha256=std::string(64,'c');
    f.frame_id="frame-"+std::to_string(seq);f.evidence_id="proof-"+std::to_string(seq);
    f.frame_sha256=std::string(64,"0123456789abcdef"[seq%16]);return r;
}
TrialStatus feed(FirstTapTrial& t,VisualCandidateResult r) {return t.observe(r,true,r.observation.received_host_ns+ms);}
SongNs ready(FirstTapTrial& t) {
    feed(t,observation(1,11*S));feed(t,observation(2,11*S+50*ms));
    const auto r=feed(t,observation(3,11*S+100*ms));
    check(r.state==FirstTapState::ready&&r.support_count==3,"three changing observations not ready");
    const auto now=O+11*S+101*ms;check(t.arm(now),"valid three-frame arm rejected");return now;
}
}
int main() {
try {
    {
        FirstTapTrial t(0);const auto armed=ready(t);
        auto s=t.status(armed,true);check(s.target_host_ns&&*s.target_host_ns==O+12*S,"wrong latest-midpoint target");
        check(s.resolved_song_interval&&s.resolved_song_interval->lower==11*S+95*ms&&s.resolved_song_interval->upper==11*S+105*ms,
              "resolved interval provenance changed");
        check(s.supporting_evidence_ids==std::vector<std::string>{"proof-1","proof-2","proof-3"},"support IDs not actual latest three");
        check(s.experiment_only&&!s.live_qualified&&!s.requires_release,"trial claimed qualification/release before DOWN");
        check(t.status(armed+500*ms,true).state==FirstTapState::armed,"old candidate100ms incorrectly expires armed target");
        check(!t.claim_down(O+12*S-1),"early DOWN allowed");
        check(t.claim_down(O+12*S),"due DOWN rejected");check(!t.claim_down(O+12*S),"second DOWN allowed");
        check(t.status(O+12*S,true).requires_release,"DOWN claim did not latch cleanup before RPC");
        t.note_down_return(O+12*S+10*ms,true);
        check(!t.up_due(O+12*S+16*ms),"UP measured from schedule rather than actual DOWN return");
        check(t.up_due(O+12*S+26*ms)&&t.claim_up(O+12*S+26*ms),"actual-return16ms UP rejected");
        check(!t.claim_up(O+12*S+26*ms),"second UP allowed");t.note_up_return(O+12*S+27*ms,true);
        s=t.status(O+12*S+28*ms,true);
        check(s.state==FirstTapState::completed&&s.requires_release&&!s.live_qualified,"RPC UP falsely verified physical release");
        t.cancel("cleanup stop");check(t.status(O+12*S+29*ms,true).state==FirstTapState::completed,"completed outcome replaced");
    }
    {
        FirstTapTrial t(100*ms);ready(t);check(*t.status(O+11*S+102*ms,true).target_host_ns==O+12*S+100*ms,"positive phase not delayed");
        FirstTapTrial early(-100*ms);ready(early);check(*early.status(O+11*S+102*ms,true).target_host_ns==O+12*S-100*ms,"negative phase not advanced");
        bool rejected=false;try {FirstTapTrial bad(100*ms+1);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"phase budget exceeded");
    }
    {
        FirstTapTrial t(0);
        for(unsigned i=1;i<=3;++i) {
            const auto song=10*S+800*ms+static_cast<SongNs>(i-1)*50*ms;
            auto r=observation(i,song);r.song_intervals.push_back({song-S-5*ms,song-S+5*ms});
            r.state=VisualCandidateState::ambiguous;feed(t,r);
        }
        const auto r=t.status(O+10*S+901*ms,true);
        check(r.provisional_origin_intervals.size()==2,"temporal alias silently discarded");
        check(!t.arm(O+10*S+901*ms),"arming band invented uniqueness from one alias");
    }
    {
        FirstTapTrial t(0);feed(t,observation(1,9*S+800*ms));
        for(unsigned i=2;i<=27;++i) {
            const auto song=9*S+800*ms+static_cast<SongNs>(i-1)*50*ms;
            auto r=observation(i,song);r.state=VisualCandidateState::ambiguous;
            r.song_intervals.push_back({song-S-5*ms,song-S+5*ms});feed(t,r);
        }
        const auto now=O+11*S+101*ms;const auto r=t.status(now,true);
        check(r.state==FirstTapState::ready&&r.provisional_origin_intervals.size()==1,"early unique seed failed to resolve later aliases");
        check(t.arm(now)&&*t.status(now,true).target_host_ns==O+12*S,"early evidence produced wrong target");
    }
    {
        FirstTapTrial t(0);
        for(unsigned i=1;i<=3;++i) {auto r=observation(i,11*S+static_cast<SongNs>(i-1)*50*ms);r.observation.frame_sha256=std::string(64,'d');feed(t,r);}
        check(!t.arm(O+11*S+101*ms),"unchanged pixels armed");
        FirstTapTrial aba(0);auto a=observation(1,11*S);feed(aba,a);feed(aba,observation(2,11*S+50*ms));
        auto b=observation(3,11*S+100*ms);b.observation.frame_sha256=a.observation.frame_sha256;feed(aba,b);
        check(!aba.arm(O+11*S+101*ms),"A/B/A pixels counted as three changing frames");
    }
    {
        FirstTapTrial t(0);feed(t,observation(1,11*S));feed(t,observation(2,11*S+50*ms));
        auto third=observation(3,11*S+100*ms);feed(t,third);
        check(!t.arm(third.observation.received_host_ns+100*ms),"exact100ms stale anchor armed");
        check(!t.status(third.observation.received_host_ns+100*ms).resolved_song_interval&&
              t.status(third.observation.received_host_ns+100*ms).supporting_evidence_ids.empty(),"expired readiness retained resolved proof");
        FirstTapTrial wide(0);
        for(unsigned i=1;i<=3;++i){auto r=observation(i,11*S+static_cast<SongNs>(i-1)*50*ms);r.song_intervals[0].lower-=40*ms;r.song_intervals[0].upper+=40*ms;feed(wide,r);}
        check(!wide.arm(O+11*S+101*ms),"wide candidate armed");
        FirstTapTrial reset(0);feed(reset,observation(1,11*S));reset.status(O+11*S+10*ms,false);
        feed(reset,observation(2,11*S+50*ms));feed(reset,observation(3,11*S+100*ms));
        check(!reset.arm(O+11*S+101*ms),"unknown before arm reused old support");
    }
    {
        FirstTapTrial t(0);ready(t);check(t.status(O+11*S+102*ms,false).state==FirstTapState::cancelled,"UI unknown did not cancel armed trial");
        check(!t.claim_down(O+12*S)&&!t.arm(O+12*S),"cancelled trial revived");
        FirstTapTrial geometry(0);ready(geometry);auto r=observation(4,11*S+150*ms);r.observation.width=1279;
        check(feed(geometry,r).state==FirstTapState::cancelled,"bad geometry did not cancel armed trial");
        FirstTapTrial epoch(0);ready(epoch);r=observation(4,11*S+150*ms);r.observation.epoch=2;
        check(feed(epoch,r).state==FirstTapState::cancelled,"epoch swap did not cancel armed trial");
        FirstTapTrial horizon(0);const auto arm=ready(horizon);
        check(horizon.status(arm+3*S,true).state==FirstTapState::cancelled,"exact3s arm horizon survived");
    }
    {
        FirstTapTrial late(0);ready(late);check(!late.claim_down(O+12*S+20*ms+1),"late DOWN allowed");
        check(late.status(O+12*S+21*ms,true).state==FirstTapState::cancelled,"late DOWN not sticky");
        FirstTapTrial edge(0);ready(edge);check(edge.claim_down(O+12*S+20*ms),"exact20ms lateness rejected");
        edge.note_down_return(O+12*S+21*ms,false);
        auto s=edge.status(O+12*S+22*ms,true);check(s.state==FirstTapState::cancelled&&s.requires_release,"failed DOWN lost unknown cleanup");
        FirstTapTrial stop(0);ready(stop);check(stop.claim_down(O+12*S),"stop test DOWN");stop.note_down_return(O+12*S+ms);
        s=stop.status(O+12*S+2*ms,false);check(s.state==FirstTapState::cancelled&&s.requires_release,"UI stop waited16ms instead of requiring immediate cleanup");
    }
    {
        FirstTapTrial budget(0);
        for(unsigned i=1;i<=129;++i)feed(budget,observation(i,11*S+static_cast<SongNs>(i-1)*ms));
        check(budget.status(O+11*S+129*ms,true).state==FirstTapState::cancelled,"128 observation budget not enforced");
        FirstTapTrial rollback(0);const auto arm=ready(rollback);
        check(rollback.status(arm-1,true).state==FirstTapState::cancelled,"clock rollback survived");
    }
    std::cout<<checks<<" first-Tap trial checks passed; pure experimental contract, no device or timing qualification.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
