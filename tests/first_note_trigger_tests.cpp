#include "../apps/chart_experiment/first_note_trigger.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace rhythm;
using namespace rhythm::experimental;
constexpr Ns ms=1'000'000,S=1'000'000'000,O=100*S;
unsigned checks=0,groups=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F> void rejects(F f,const char* why){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught,why);}
OpeningVisualModel model() {
    OpeningVisualModel m;m.chart_sha256=std::string(64,'a');m.model_id="synthetic-first-note-model";
    m.model_sha256=std::string(64,'b');m.calibration_id="unqualified-test";m.calibration_sha256=std::string(64,'c');return m;
}
SemanticChart chart() {
    SemanticChart c;c.source.chart_sha256=model().chart_sha256;c.lines.resize(1);c.note_count=2;
    auto& line=c.lines[0];line.source_index=0;
    for(int i=0;i<2;++i) {
        SemanticNote n;n.source_index=static_cast<std::size_t>(i);n.source.json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(i);
        n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;n.start_ns=n.end_ns=12*S+i*200*ms;
        n.kind=NoteKind::tap;n.side=NoteSide::above;n.position_x=-4;n.speed=1;n.floor_position=-10.8+i*.2;line.notes.push_back(n);
    }
    SpeedLineEvent negative;negative.time={0,0,0,11*S+400*ms};negative.value=-1;
    negative.source.json_pointer="/judgeLineList/0/speedEvents/0";
    SpeedLineEvent positive;positive.time={0,0,11*S+400*ms,12*S};positive.value=1;
    positive.source.json_pointer="/judgeLineList/0/speedEvents/1";line.speed_events={negative,positive};return c;
}
VisualCandidateResult observation(unsigned seq,Ns song,double y,Ns received=-1) {
    VisualCandidateResult r;const auto m=model();r.state=VisualCandidateState::ambiguous;
    r.model_id=m.model_id;r.model_sha256=m.model_sha256;r.calibration_id=m.calibration_id;r.calibration_sha256=m.calibration_sha256;
    auto& f=r.observation;f.sequence=seq;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;
    f.source_id="synthetic-actual-source";f.source_sha256=std::string(64,'d');f.clock_id="QPC-receipt-only";
    f.opening_context_id="manual-start-1";f.frame_id="frame-"+std::to_string(seq);f.evidence_id="evidence-"+std::to_string(seq);
    f.frame_sha256=std::string(64,"0123456789abcdef"[seq%16]);f.clock_kind=VisualSourceClock::arrival_only;
    f.received_host_ns=received<0?O+song:received;f.source_valid=f.centers_complete_unoccluded=true;
    f.centers={{"lowest",352,y},{"higher",352,y-155}};
    VisualTimeCandidate positive;positive.song_time={song-5*ms,song+5*ms};positive.line_speed=1;
    positive.speed_event_json_pointer="/judgeLineList/0/speedEvents/1";
    positive.matches={{"lowest",m.chart_sha256+"#/judgeLineList/0/notesAbove/0","/judgeLineList/0/notesAbove/0"},
        {"higher",m.chart_sha256+"#/judgeLineList/0/notesAbove/1","/judgeLineList/0/notesAbove/1"}};
    auto negative=positive;negative.line_speed=-1;negative.speed_event_json_pointer="/judgeLineList/0/speedEvents/0";
    negative.song_time={10*S+795*ms,10*S+805*ms};r.candidates={negative,positive};
    r.song_intervals={negative.song_time,positive.song_time};return r;
}
FirstNoteTriggerStatus feed(FirstNoteTrigger& t,VisualCandidateResult r) {return t.observe(r,true,r.observation.received_host_ns+ms);}
FirstNoteTriggerStatus ready(FirstNoteTrigger& t) {
    const auto first=feed(t,observation(1,11*S+550*ms,310));
    check(first.state==FirstNoteTriggerState::acquiring&&first.support_count==1,"single first-note image fired");
    return feed(t,observation(2,11*S+650*ms,350));
}
}
int main() {
try {
    {++groups;FirstNoteTrigger t(chart(),model(),-80*ms);const auto s=ready(t);
        check(s.state==FirstNoteTriggerState::ready&&s.support_count==2,"downward first-note pair not ready");
        check(s.raw_candidate_count==2&&s.raw_candidates.size()==2,"raw negative alias discarded");
        check(s.raw_candidates[0].line_speed==-1&&s.first_note_positive_candidates==1,"native branch audit wrong");
        check(s.selected_intervals.size()==1&&s.selected_intervals[0].lower==11*S+645*ms,"wrong positive branch selected");
        check(s.anchor&&s.anchor->origin_host_ns==O&&s.anchor->target_host_ns==O+12*S-80*ms,"first-note ETA/phase wrong");
        check(s.anchor->received_host_ns==O+11*S+650*ms&&!s.timing_qualified,"receipt replaced or qualified");
        check(s.anchor->supporting_evidence_ids==std::vector<std::string>{"evidence-1","evidence-2"},"support not actual pair");
        const auto a=t.take_anchor(O+11*S+652*ms);check(a.has_value(),"ready take failed");
        check(!t.take_anchor(O+11*S+653*ms),"trigger fired twice");
        check(t.status(O+50*S,false).state==FirstNoteTriggerState::fired,"fired immutable trigger expired like lease");
        check(feed(t,observation(3,11*S+700*ms,400)).state==FirstNoteTriggerState::fired,"fired trigger rearmed");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);ready(t);const auto a=t.take_anchor(O+11*S+652*ms);
        check(a&&a->target_host_ns==O+12*S,"zero phase wrong");FirstNoteTrigger delayed(chart(),model(),100*ms);ready(delayed);
        check(delayed.take_anchor(O+11*S+652*ms)->target_host_ns==O+12*S+100*ms,"positive phase not delayed");
        rejects([]{FirstNoteTrigger bad(chart(),model(),101*ms);},"phase overflow accepted");}
    {++groups;FirstNoteTrigger t(chart(),model(),-80*ms);feed(t,observation(1,11*S+550*ms,350));
        const auto s=feed(t,observation(2,11*S+650*ms,310));check(s.state==FirstNoteTriggerState::acquiring&&!s.anchor,"upward motion selected positive alias");
        FirstNoteTrigger flat(chart(),model(),0);feed(flat,observation(1,11*S+550*ms,330));
        check(feed(flat,observation(2,11*S+650*ms,336)).support_count==1,"exact combined6px error counted as downward");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);feed(t,observation(1,11*S+500*ms,290));
        const auto s=feed(t,observation(2,11*S+600*ms,299));check(s.support_count==2&&s.state==FirstNoteTriggerState::acquiring,"trigger below y300 fired");
        check(feed(t,observation(3,11*S+680*ms,310)).state==FirstNoteTriggerState::ready,"pair after threshold did not trigger");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);auto a=observation(1,11*S+550*ms,310);feed(t,a);
        auto b=observation(2,11*S+650*ms,350);b.observation.frame_sha256=a.observation.frame_sha256;
        check(feed(t,b).state==FirstNoteTriggerState::acquiring,"unchanged pixels triggered");
        FirstNoteTrigger replay(chart(),model(),0);feed(replay,a);b=observation(2,11*S+650*ms,350);b.observation.evidence_id=a.observation.evidence_id;
        check(feed(replay,b).state==FirstNoteTriggerState::cancelled,"replayed evidence accepted");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);feed(t,observation(1,11*S+550*ms,310));
        auto b=observation(2,11*S+650*ms,350);std::swap(b.candidates[1].matches[0].note_id,b.candidates[1].matches[1].note_id);
        std::swap(b.candidates[1].matches[0].note_json_pointer,b.candidates[1].matches[1].note_json_pointer);
        const auto s=feed(t,b);check(s.state==FirstNoteTriggerState::acquiring&&s.first_note_positive_candidates==0,"other note chosen as first");
        FirstNoteTrigger wrong_x(chart(),model(),0);feed(wrong_x,observation(1,11*S+550*ms,310));b=observation(2,11*S+650*ms,350);
        b.observation.centers[0].x=360;check(feed(wrong_x,b).state==FirstNoteTriggerState::acquiring,"wrong column triggered");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);feed(t,observation(1,11*S+550*ms,310));
        auto b=observation(2,11*S+650*ms,350);auto alias=b.candidates[1];alias.song_time={11*S+700*ms,11*S+710*ms};b.candidates.push_back(alias);
        const auto s=feed(t,b);check(s.state==FirstNoteTriggerState::acquiring&&s.selected_intervals.size()==2&&s.raw_candidates.size()==3,
            "multiple first-note positive times greedily resolved");}
    {++groups;FirstNoteTrigger t(chart(),model(),-80*ms);feed(t,observation(1,11*S+650*ms,350));
        const auto s=feed(t,observation(2,11*S+750*ms,390));check(s.state==FirstNoteTriggerState::cancelled,"late first-note trigger allowed");
        check(!t.take_anchor(O+11*S+752*ms),"late trigger rearmed");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);ready(t);check(!t.take_anchor(O+11*S+750*ms),"exact100ms stale receipt fired");
        check(t.status(O+11*S+751*ms).state==FirstNoteTriggerState::acquiring,"expired readiness stayed armed");
        FirstNoteTrigger unknown(chart(),model(),0);ready(unknown);unknown.status(O+11*S+652*ms,false);
        check(!unknown.take_anchor(O+11*S+653*ms),"UI unknown reused ready anchor");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);auto a=observation(1,11*S+550*ms,310);feed(t,a);
        auto b=observation(2,11*S+650*ms,350);b.observation.epoch=2;
        check(feed(t,b).state==FirstNoteTriggerState::cancelled,"epoch changed without cancellation");
        FirstNoteTrigger stale(chart(),model(),0);check(stale.observe(a,true,a.observation.received_host_ns+100*ms).state==FirstNoteTriggerState::cancelled,"stale actual observation accepted");
        FirstNoteTrigger rollback(chart(),model(),0);ready(rollback);check(rollback.status(O+11*S+640*ms).state==FirstNoteTriggerState::cancelled,"clock rollback survived");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);feed(t,observation(1,11*S+550*ms,310));
        const auto s=feed(t,observation(2,11*S+650*ms,350,O+11*S+800*ms));
        check(s.state==FirstNoteTriggerState::acquiring,"over200ms pair gap triggered");
        FirstNoteTrigger inconsistent(chart(),model(),0);feed(inconsistent,observation(1,11*S+450*ms,300));
        check(feed(inconsistent,observation(2,11*S+650*ms,350,O+11*S+500*ms)).state==FirstNoteTriggerState::acquiring,"unphysical rate1 pair triggered");}
    {++groups;auto c=chart();c.lines[0].speed_events.clear();c.lines[0].notes[0].floor_position=.6;
        const auto append=[&](Ns begin,Ns end,double speed) {
            SpeedLineEvent e;e.time.start_ns=begin;e.time.end_ns=end;e.value=speed;
            e.source.json_pointer="/judgeLineList/0/speedEvents/"+std::to_string(c.lines[0].speed_events.size());
            c.lines[0].speed_events.push_back(e);
        };
        append(0,1100*ms,0);append(1100*ms,1700*ms,1);append(1700*ms,2300*ms,-1);
        append(2300*ms,11400*ms,0);append(11400*ms,12*S,1);
        rejects([&]{FirstNoteTrigger t(c,model(),0);},"earlier visible separated positive approach silently excluded");
        c=chart();auto extra=c.lines[0].speed_events.front();extra.time.end_ns=5*S;extra.value=1;
        extra.source.json_pointer="/judgeLineList/0/speedEvents/early-offscreen";
        c.lines[0].speed_events.front().time.start_ns=5*S;c.lines[0].speed_events.insert(c.lines[0].speed_events.begin(),extra);
        c.lines[0].notes[0].floor_position=-.8;
        FirstNoteTrigger offscreen(c,model(),0);check(offscreen.status(0).state==FirstNoteTriggerState::acquiring,"offscreen positive run wrongly rejected");
        c=chart();c.lines[0].speed_events.back().value=0;rejects([&]{FirstNoteTrigger t(c,model(),0);},"no final positive approach accepted");
        c=chart();c.lines[0].notes[0].source.json_pointer="wrong";rejects([&]{FirstNoteTrigger t(c,model(),0);},"wrong source firstnote accepted");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);auto a=observation(1,11*S+550*ms,310);a.candidates[1].line_speed=2;
        check(feed(t,a).state==FirstNoteTriggerState::cancelled,"fake speed disagreed with native piece");
        FirstNoteTrigger out(chart(),model(),0);a=observation(1,11*S+550*ms,310);a.candidates[1].song_time={11*S,11*S+ms};
        check(feed(out,a).state==FirstNoteTriggerState::cancelled,"candidate outside native piece accepted");
        FirstNoteTrigger fakeid(chart(),model(),0);a=observation(1,11*S+550*ms,310);a.candidates[0].matches[0].note_id="wrong";
        check(feed(fakeid,a).state==FirstNoteTriggerState::cancelled,"malformed excluded alias hidden");}
    {++groups;FirstNoteTrigger t(chart(),model(),0);auto a=observation(1,11*S+550*ms,310);a.observation.centers_complete_unoccluded=false;
        check(feed(t,a).state==FirstNoteTriggerState::cancelled,"coverage assumption not explicit");
        FirstNoteTrigger clockkind(chart(),model(),0);a=observation(1,11*S+550*ms,310);a.observation.clock_kind=VisualSourceClock::video_pts;
        check(feed(clockkind,a).state==FirstNoteTriggerState::cancelled,"PTS relabeled receipt");
        FirstNoteTrigger center(chart(),model(),0);a=observation(1,11*S+550*ms,310);a.observation.centers[1].y=306;
        check(feed(center,a).state==FirstNoteTriggerState::acquiring,"ambiguous lowest center accepted");}
    std::cout<<groups<<" first-note-trigger groups / "<<checks<<" checks passed; pure contract, no device or timing qualification.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
