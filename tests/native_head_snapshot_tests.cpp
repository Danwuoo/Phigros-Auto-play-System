#include "../apps/chart_experiment/native_head_snapshot.hpp"
#include <iostream>
#include <stdexcept>
using namespace rhythm;
using namespace rhythm::experimental;
namespace {
constexpr SongNs S=1'000'000'000,ms=1'000'000;
std::size_t checks=0;
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}check(rejected,why);}
SemanticChart chart() {
    SemanticChart c;c.source.chart_sha256=std::string(64,'a');c.lines.resize(1);auto& l=c.lines[0];l.source_index=0;l.bpm=120;
    MoveLineEvent m;m.time.start_ns=0;m.time.end_ns=10*S;m.start_x=m.end_x=m.start_y=m.end_y=.5;l.move_events={m};
    NumericLineEvent e;e.time=m.time;e.start=e.end=0;l.rotate_events={e};e.start=e.end=1;l.opacity_events={e};
    SpeedLineEvent sp;sp.time=m.time;sp.value=1.5;l.speed_events={sp};
    for(std::size_t i=0;i<2;++i){SemanticNote n;n.kind=NoteKind::tap;n.side=NoteSide::above;n.source_index=i;n.start_ns=n.end_ns=2*S;
        n.position_x=i?3:-3;n.speed=1;n.floor_position=3;n.source.json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(i);
        n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;l.notes.push_back(n);}c.note_count=2;return c;
}
std::vector<VisualCenter> centers(const NativeHeadSnapshotSample& s) {
    std::vector<VisualCenter> out;for(std::size_t i=0;i<s.head_count;++i)out.push_back({"observed-"+std::to_string(i),s.heads[i].x,s.heads[i].y});return out;
}
}
int main() {
try {
    auto c=chart();PhigrosPoseEvaluator poses(c);NativeHeadSnapshotModel model(c,poses,{1500*ms,1900*ms});
    check(model.sources().size()==2&&model.samples().size()==81,"original source or grid count lost");
    const auto& sample=model.samples()[40];auto observed=centers(sample);auto matched=model.match(observed,1700*ms);
    check(sample.song_ns==1700*ms&&sample.full_visible_heads==2&&sample.head_count==2,"wrong source-time projection");
    check(matched.valid&&matched.matched_grid_count==1&&matched.assignments.size()==1&&matched.assignments[0].grid_index==40,"two-head unique grid not retained");
    check(matched.assignments[0].source_indices[0]==0&&matched.assignments[0].source_indices[1]==1,"original source mapping reordered");
    check(!matched.input_permission&&!matched.pixels_retained&&matched.diagnostic_only,"diagnostic gained authority");
    const auto positive_partial=model.match(centers(model.samples()[0]),1500*ms);
    check(positive_partial.valid&&positive_partial.assignments[0].partial_match&&model.samples()[0].partial_heads==2,
        "positive edge observations were silently promoted to full-visibility proof");
    observed.pop_back();check(!model.match(observed,1700*ms).valid,"missing full-visible head accepted");
    observed=centers(sample);observed[0].x+=20;check(!model.match(observed,1700*ms).valid,"unowned detected center accepted");
    check(!model.match({},1700*ms).valid,"empty frame produced a song clock");
    observed=centers(sample);observed[1].id=observed[0].id;check(!model.match(observed,1700*ms).valid,"duplicate observation identity accepted");
    observed=centers(sample);observed[0].x=std::numeric_limits<double>::quiet_NaN();check(!model.match(observed,1700*ms).valid,"nonfinite observation accepted");
    observed=centers(sample);check(!model.match(observed,3*S).valid,"searched outside declared original-time domain");
    c=chart();auto alias=c.lines[0].notes[0];alias.source_index=2;alias.source.json_pointer="/judgeLineList/0/notesAbove/2";
    alias.source.id=c.source.chart_sha256+'#'+alias.source.json_pointer;c.lines[0].notes.push_back(alias);++c.note_count;
    PhigrosPoseEvaluator alias_poses(c);NativeHeadSnapshotModel aliases(c,alias_poses,{1500*ms,1900*ms});
    check(aliases.sources().size()==3&&aliases.samples()[40].head_count==3&&aliases.samples()[40].coincident_sources,"coincident original alias deleted");
    check(!aliases.match(centers(sample),1700*ms).valid,"unproved alias exemption hid a missing full-visible source");
    c=chart();c.lines[0].notes[1].position_x=c.lines[0].notes[0].position_x+2./72;
    PhigrosPoseEvaluator close_poses(c);NativeHeadSnapshotModel close(c,close_poses,{1500*ms,1900*ms});
    auto two=centers(close.samples()[40]);two[0].x+=.5;two[1].x-=.5;auto alternatives=close.match(two,1700*ms);
    check(alternatives.valid&&alternatives.source_ambiguity&&alternatives.assignments.size()==2,"one-to-one alternative source assignments discarded");
    check(alternatives.assignments[0].source_indices!=alternatives.assignments[1].source_indices,"source alternatives averaged");
    auto exhausted=close.match(two,1700*ms,{1,4096});check(!exhausted.valid&&exhausted.budget_exhausted&&exhausted.assignments.empty(),"work limit returned partial successful matches");
    exhausted=close.match(two,1700*ms,{262144,1});check(!exhausted.valid&&exhausted.budget_exhausted&&exhausted.assignments.empty(),"assignment limit picked a first winner");
    c=chart();c.lines[0].notes[0].kind=NoteKind::hold;c.lines[0].notes[0].end_ns=3*S;c.lines[0].notes[0].speed=99;
    PhigrosPoseEvaluator hold_poses(c);NativeHeadSnapshotModel hold(c,hold_poses,{1500*ms,1900*ms});
    check(std::abs(hold.samples()[40].heads[0].y-sample.heads[0].y)<1e-9,"Hold head used native speed instead of effective1");
    c=chart();c.lines[0].speed_events[0].floor_position=3;for(auto& n:c.lines[0].notes)n.floor_position+=3;
    PhigrosPoseEvaluator anchored_poses(c);NativeHeadSnapshotModel anchored(c,anchored_poses,{1500*ms,1900*ms});
    check(std::abs(anchored.samples()[40].heads[0].y-sample.heads[0].y)<1e-9,"provided native floor anchor ignored");
    c=chart();c.source_format_version=1;auto sentinel=c.lines[0].speed_events[0];sentinel.time.start_ns=-S;sentinel.time.end_ns=0;sentinel.value=100;
    c.lines[0].speed_events.insert(c.lines[0].speed_events.begin(),sentinel);PhigrosPoseEvaluator v1_poses(c);NativeHeadSnapshotModel v1(c,v1_poses,{1500*ms,1900*ms});
    check(std::abs(v1.samples()[40].heads[0].y-sample.heads[0].y)<1e-9,"v1 negative sentinel integrated before native zero");
    c=chart();c.lines[0].notes[0].side=NoteSide::below;c.lines[0].notes[0].source_index=0;
    c.lines[0].notes[0].source.json_pointer="/judgeLineList/0/notesBelow/0";c.lines[0].notes[0].source.id=c.source.chart_sha256+'#'+c.lines[0].notes[0].source.json_pointer;
    PhigrosPoseEvaluator below_poses(c);NativeHeadSnapshotModel below(c,below_poses,{1500*ms,1900*ms});
    check(below.sources()[0].side==NoteSide::below&&below.samples()[40].heads[0].y>360,"below-side approach not reflected");
    c=chart();c.lines[0].notes[0].position_x=100;PhigrosPoseEvaluator outside_poses(c);NativeHeadSnapshotModel outside(c,outside_poses,{1500*ms,1900*ms});
    check(outside.samples()[40].future_sources==2&&outside.samples()[40].outside_center_roi==1&&outside.sources().size()==2,"outside source silently removed from provenance");
    c=chart();for(std::size_t i=2;i<9;++i){auto n=c.lines[0].notes[0];n.source_index=i;n.position_x=-3+static_cast<double>(i)/10;
        n.source.json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(i);n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;c.lines[0].notes.push_back(n);}c.note_count=9;
    PhigrosPoseEvaluator many_poses(c);NativeHeadSnapshotModel many(c,many_poses,{1500*ms,1900*ms});
    check(!many.samples()[40].complete&&many.samples()[40].possible_heads==9&&many.samples()[40].head_count==0,"head budget retained a cherry-picked subset");
    c=chart();PhigrosPoseEvaluator final_poses(c);NativeHeadSnapshotModel ended(c,final_poses,{1900*ms,2100*ms});
    check(ended.samples()[20].future_sources==0&&ended.samples()[20].unobservable,"already judged source remained a future head");
    rejects([&]{NativeHeadSnapshotModel bad(c,final_poses,{1500*ms,1900*ms},{10000,2601,1,8*1024*1024});},"precompute work cap ignored");
    rejects([&]{NativeHeadSnapshotModel bad(c,final_poses,{1500*ms,1900*ms},{10000,2601,6'000'000,1});},"storage cap ignored");
    rejects([&]{NativeHeadSnapshotModel bad(c,final_poses,{1500*ms,1900*ms},{1,2601,6'000'000,8*1024*1024});},"source budget silently truncated");
    rejects([&]{NativeHeadSnapshotModel bad(c,final_poses,{0,14*S});},"unbounded grid domain accepted");
    rejects([&]{NativeHeadSnapshotModel bad(c,final_poses,{1500*ms,1900*ms,ms});},"unversioned grid step accepted");
    c.lines[0].notes[0].source.id=std::string(64,'b')+'#'+c.lines[0].notes[0].source.json_pointer;
    rejects([&]{NativeHeadSnapshotModel bad(c,final_poses,{1500*ms,1900*ms});},"wrong chart source identity accepted");
    std::cout<<checks<<" native head snapshot checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
