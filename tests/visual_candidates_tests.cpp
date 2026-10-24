#include "rhythm/visual_candidates.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace rhythm;
constexpr SongNs S=1'000'000'000;
int checks=0,groups=0;
void check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
template<class F> void reject(F fn,const char* reason) {
    bool rejected=false;try {fn();}catch(const std::invalid_argument&) {rejected=true;}check(rejected,reason);
}
template<class F> void group(const char* name,F fn) {fn();++groups;std::cout<<name<<" passed\n";}
SemanticChart chart(std::vector<double> speeds={1},std::vector<SongNs> ends={10*S}) {
    SemanticChart c;c.source.chart_sha256=std::string(64,'a');c.lines.resize(1);
    auto& l=c.lines[0];l.bpm=175;l.source_index=0;
    MoveLineEvent m;m.time={0,0,0,100*S};m.start_x=m.end_x=.5;m.start_y=m.end_y=.2;l.move_events={m};
    NumericLineEvent r;r.time={0,0,0,100*S};r.start=r.end=0;l.rotate_events={r};r.start=r.end=1;l.opacity_events={r};
    SongNs start=0;
    for(std::size_t i=0;i<speeds.size();++i) {
        SpeedLineEvent e;e.time={0,0,start,ends[i]};e.value=speeds[i];
        e.source.json_pointer="/judgeLineList/0/speedEvents/"+std::to_string(i);l.speed_events.push_back(e);start=ends[i];
    }
    SemanticNote n;n.source.json_pointer="/judgeLineList/0/notesAbove/0";
    n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;n.start_ns=n.end_ns=20*S;
    n.kind=NoteKind::tap;n.side=NoteSide::above;n.floor_position=5;n.speed=1;n.position_x=0;l.notes={n};
    return c;
}
OpeningVisualModel model(SongNs end=10*S) {
    OpeningVisualModel m;m.chart_sha256=std::string(64,'a');m.model_id="analytic-horizontal-v1";
    m.model_sha256=std::string(64,'b');m.calibration_id="unqualified-synthetic";m.calibration_sha256=std::string(64,'c');
    m.begin_ns=0;m.end_ns=end;m.pose_tolerance_pixels=0;m.approach_scale_lower=m.approach_scale_upper=100;m.pixel_error=1;m.per_frame_time_allowance_ns=0;return m;
}
VisualCandidateFrame frame(std::uint64_t n=1,SongNs pts=100*S,double y=276) {
    VisualCandidateFrame f;f.sequence=n;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;
    f.source_id="synthetic-video";f.source_sha256=std::string(64,'d');f.frame_id="ordinal-"+std::to_string(n);
    f.evidence_id="frame-proof-"+std::to_string(n);f.frame_sha256=std::string(64,'e');f.clock_id="video-PTS-domain";
    f.opening_context_id="explicit-opening-test";f.clock_kind=VisualSourceClock::video_pts;f.source_time_ns=pts;
    f.received_host_ns=S+static_cast<SongNs>(n)*10'000'000;f.source_valid=true;f.centers_complete_unoccluded=true;
    f.centers={{"cyan-0",640,y}};return f;
}
VisualCandidateResult observe(VisualTimingCandidateProducer& p,VisualCandidateFrame f) {return p.observe(f,f.received_host_ns);}
bool covers(VisualTimeInterval i,SongNs t) {return i.lower<=t&&t<=i.upper;}
}
int main() {
try {
    group("independently derived positive affine inverse",[] {
        VisualTimingCandidateProducer p(chart(),model());p.begin_epoch(1);const auto r=observe(p,frame());
        check(r.candidates.size()==1,"one analytic candidate");check(r.song_intervals.size()==1,"one time interval");
        const auto i=r.song_intervals[0]; // y=76+100*t, y=276+-1 -> t=2+-.01 seconds.
        check(std::abs(i.lower-1'990'000'000LL)<=5&&std::abs(i.upper-2'010'000'000LL)<=5,"independent interval endpoints");
        check(!r.timing_qualified&&!r.live_permitted,"candidate never qualifies input");
        check(r.candidates[0].matches[0].note_json_pointer=="/judgeLineList/0/notesAbove/0","source pointer preserved");
        check(r.model_id=="analytic-horizontal-v1"&&r.observation.frame_id=="ordinal-1","model/frame identity preserved");
    });
    group("negative speed aliases and zero-speed full interval",[] {
        VisualTimingCandidateProducer p(chart({1,-1,0},{3*S,6*S,8*S}),model(8*S));p.begin_epoch(1);
        auto r=observe(p,frame(1,100*S,176)); // F=1 -> t=1 or t=5.
        check(r.song_intervals.size()==2,"both negative-speed aliases retained");
        check(covers(r.song_intervals[0],S)&&covers(r.song_intervals[1],5*S),"both analytic roots covered");
        VisualTimingCandidateProducer z(chart({0},{10*S}),model());z.begin_epoch(1);
        auto a=observe(z,frame(1,100*S,76));
        check(a.song_intervals.size()==1&&a.song_intervals[0].lower==0&&a.song_intervals[0].upper==10*S-1,"whole zero-speed domain retained");
        auto b=observe(z,frame(2,100*S+200'000'000,76));
        check(b.unchanged_pixels&&b.song_intervals[0].upper-b.song_intervals[0].lower>9*S,"frozen zero-speed pixels cannot imply precision");
        check(!b.timing_qualified,"frozen pixels never grant timing");
    });
    group("complete non-greedy assignments and unmatched visibility",[] {
        auto c=chart();auto n=c.lines[0].notes[0];n.floor_position=6;n.source.json_pointer="/judgeLineList/0/notesAbove/1";
        n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;c.lines[0].notes.push_back(n);
        VisualTimingCandidateProducer p(c,model());p.begin_epoch(1);auto f=frame();f.centers={{"first",640,176},{"second",640,276}};
        auto r=observe(p,f);check(r.candidates.size()==1&&r.candidates[0].matches.size()==2,"all centers assigned uniquely");
        check(r.candidates[0].matches[0].note_json_pointer==n.source.json_pointer,"order does not force greedy identity");
        VisualTimingCandidateProducer missing(c,model());missing.begin_epoch(1);auto rejected=observe(missing,frame());
        check(rejected.candidates.empty()&&rejected.state==VisualCandidateState::rejected,"unmatched visible note cannot be silently dropped");
        c.lines[0].notes[1].floor_position=5;
        VisualTimingCandidateProducer same(c,model());same.begin_epoch(1);f=frame();f.centers.push_back({"second",640,276});
        auto aliases=observe(same,f);check(aliases.candidates.size()==2,"both indistinguishable note assignments preserved");
        for(const double offscreen_x:{-20.,20.}) {
            auto edge=chart();auto outside=edge.lines[0].notes[0];outside.position_x=offscreen_x;
            outside.source.json_pointer="/judgeLineList/0/notesAbove/1";outside.source.id=edge.source.chart_sha256+'#'+outside.source.json_pointer;
            edge.lines[0].notes.push_back(outside);VisualTimingCandidateProducer safe(edge,model());safe.begin_epoch(1);
            auto kept=observe(safe,frame());check(!kept.candidates.empty()&&covers(kept.song_intervals[0],2*S),
                "offscreen horizontal note is not a required visible center");
        }
    });
    group("conditional sequence intersection and frozen moving rejection",[] {
        VisualTimingCandidateProducer p(chart(),model());p.begin_epoch(1);auto a=observe(p,frame());
        auto b=observe(p,frame(2,100*S+10'000'000,277));
        check(!b.candidates.empty()&&b.supporting_evidence_ids.size()==2,"actual new frame supports same offset set");
        check(b.conditional_source_minus_song_intervals[0].lower<=98*S&&b.conditional_source_minus_song_intervals[0].upper>=98*S,"analytic source offset covered");
        auto frozen=observe(p,frame(3,100*S+300'000'000,277));
        check(frozen.state==VisualCandidateState::rejected&&frozen.requires_new_epoch&&frozen.candidates.empty(),"frozen nonzero motion rejected without best-score recovery");
        check(observe(p,frame(4,101*S,276)).reason=="new_epoch_required","failure cannot be laundered by another frame");
    });
    group("arrival-only and mapped interval provenance",[] {
        VisualTimingCandidateProducer p(chart(),model());p.begin_epoch(1);auto f=frame();
        f.clock_kind=VisualSourceClock::arrival_only;f.source_time_ns.reset();
        auto r=observe(p,f);check(!r.candidates.empty()&&r.conditional_source_minus_song_intervals.empty(),"arrival never becomes a capture timestamp");
        check(!r.timing_qualified&&!r.live_permitted,"arrival cannot grant input");
        VisualTimingCandidateProducer q(chart(),model());q.begin_epoch(1);f=frame();f.clock_kind=VisualSourceClock::mapped_capture;
        f.source_time_ns.reset();f.capture_host_interval=VisualTimeInterval{f.received_host_ns-2'000'000,f.received_host_ns-1'000'000};
        auto mapped=observe(q,f);check(!mapped.candidates.empty()&&!mapped.timing_qualified,"caller mapping remains conditional");
        check(mapped.conditional_source_minus_song_intervals[0].lower<0,"signed offset interval not clamped");
    });
    group("missing provenance duplicate ABA stale future and gap",[] {
        for(int kind=0;kind<8;++kind) {
            VisualTimingCandidateProducer p(chart(),model());p.begin_epoch(1);auto f=frame();
            if(kind==0)f.evidence_id.clear();if(kind==1)f.frame_sha256.clear();if(kind==2)f.source_valid=false;
            if(kind==3)f.clock_kind=VisualSourceClock::unknown;if(kind==4)f.opening_context_id.clear();
            if(kind==5)f.centers[0].x=std::numeric_limits<double>::quiet_NaN();if(kind==6)f.centers[0].id.clear();
            auto r=p.observe(f,kind==7?f.received_host_ns+100'000'000:f.received_host_ns);
            check(r.candidates.empty()&&r.requires_new_epoch,"invalid or stale evidence rejects");
        }
        VisualTimingCandidateProducer p(chart(),model());p.begin_epoch(1);observe(p,frame());observe(p,frame(2,100*S+10'000'000,277));
        auto aba=frame(3,100*S+20'000'000,278);aba.frame_id="ordinal-1";
        check(observe(p,aba).requires_new_epoch,"nonadjacent frame replay rejected");
        VisualTimingCandidateProducer q(chart(),model());q.begin_epoch(1);observe(q,frame());auto gap=frame(2,101*S,276);gap.received_host_ns=3*S;
        check(observe(q,gap).requires_new_epoch,"new observation cannot hide gap");
        VisualTimingCandidateProducer future(chart(),model());future.begin_epoch(1);auto f=frame();
        check(future.observe(f,f.received_host_ns-1).requires_new_epoch,"future receipt rejected");
        VisualTimingCandidateProducer rollback(chart(),model());rollback.begin_epoch(1);
        rollback.observe(frame(),S+50'000'000);auto newer=frame(2,100*S+10'000'000,277);
        check(rollback.observe(newer,S+40'000'000).requires_new_epoch,"host now rollback cannot be hidden by fresh frame");
    });
    group("bounded resource failure returns no partial aliases",[] {
        auto limits=VisualCandidateLimits{};limits.max_candidates=1;
        VisualTimingCandidateProducer p(chart({1,-1},{3*S,6*S}),model(6*S),limits);p.begin_epoch(1);
        auto r=observe(p,frame(1,100*S,176));check(r.candidates.empty()&&r.requires_new_epoch&&r.reason=="resource_budget_exhausted","no truncation into a best branch");
        limits=VisualCandidateLimits{};limits.max_work=1;
        VisualTimingCandidateProducer w(chart(),model(),limits);w.begin_epoch(1);check(observe(w,frame()).requires_new_epoch,"work budget enforced");
        limits=VisualCandidateLimits{};limits.max_observations=1;
        VisualTimingCandidateProducer n(chart(),model(),limits);n.begin_epoch(1);observe(n,frame());
        check(observe(n,frame(2,100*S+10'000'000,277)).requires_new_epoch,"bounded replay ledger cannot silently discard IDs");
    });
    group("explicit scope and model validation plus no centers",[] {
        auto c=chart();auto m=model();m.chart_sha256=std::string(64,'f');reject([&]{VisualTimingCandidateProducer p(c,m);},"wrong chart rejected");
        m=model();m.calibration_id.clear();reject([&]{VisualTimingCandidateProducer p(c,m);},"missing model calibration identity");
        c=chart();c.lines[0].rotate_events[0].end=10;reject([&]{VisualTimingCandidateProducer p(c,model());},"moving scope rejected");
        c=chart();auto reset=c.lines[0].move_events[0];c.lines[0].move_events[0].time.end_ns=5*S;
        c.lines[0].move_events[0].end_x=1;reset.time.start_ns=5*S;c.lines[0].move_events.push_back(reset);
        reject([&]{VisualTimingCandidateProducer p(c,model());},"left-limit excursion before reset key rejected");
        c=chart({1,1},{3*S,10*S});c.lines[0].speed_events[1].time.start_ns+=1;
        reject([&]{VisualTimingCandidateProducer p(c,model());},"unexplained speed gap rejected");
        c=chart();c.lines[0].notes[0].start_ns=5*S;reject([&]{VisualTimingCandidateProducer p(c,model());},"post-judgment model scope rejected");
        c=chart();c.lines[0].speed_events[0].floor_position=0;
        reject([&]{VisualTimingCandidateProducer p(c,model());},"explicit floor anchors cannot be silently ignored");
        VisualTimingCandidateProducer occluded(chart(),model());occluded.begin_epoch(1);auto overlap=frame();overlap.centers_complete_unoccluded=false;
        check(observe(occluded,overlap).candidates.empty(),"occluded/incomplete centers cannot support exact-count model");
        VisualTimingCandidateProducer overflow(chart(),model());overflow.begin_epoch(1);auto huge=frame();
        huge.source_time_ns=std::numeric_limits<SongNs>::max()-10000;
        check(observe(overflow,huge).requires_new_epoch,"unrepresentable clock integers reject instead of rounding to fake precision");
        VisualTimingCandidateProducer p(chart(),model());p.begin_epoch(1);auto f=frame();f.centers.clear();
        auto r=observe(p,f);check(r.state==VisualCandidateState::unknown&&r.candidates.empty()&&r.reason=="no_centers","empty observation stays unknown");
        reject([&]{p.begin_epoch(1);},"epoch cannot be reused");p.begin_epoch(2);f=frame();f.epoch=2;
        check(!observe(p,f).candidates.empty(),"explicit newer epoch permits new candidate analysis");
    });
    std::cout<<groups<<" groups / "<<checks<<" visual candidate checks passed; no live timing qualification.\n";
    return 0;
}catch(const std::exception& e) {std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
