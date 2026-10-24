#include "../apps/chart_experiment/adaptive_first_note.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace rhythm;
using namespace rhythm::experimental;
constexpr Ns ms=1'000'000,S=1'000'000'000,O=100*S;
unsigned checks=0,groups=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}check(rejected,why);}
SemanticChart chart() {
    SemanticChart c;c.source.chart_sha256=std::string(64,'a');c.note_count=1;c.lines.resize(1);
    auto& l=c.lines[0];l.source_index=0;l.bpm=120;
    SemanticNote n;n.kind=NoteKind::drag;n.side=NoteSide::above;n.source_index=0;
    n.start_ns=n.end_ns=2*S;n.position_x=0;n.speed=1;n.floor_position=3;
    n.source.json_pointer="/judgeLineList/0/notesAbove/0";n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;l.notes={n};
    SpeedLineEvent speed;speed.source.json_pointer="/judgeLineList/0/speedEvents/0";speed.time.start_ns=0;speed.time.end_ns=20*S;speed.value=1.5;l.speed_events={speed};
    MoveLineEvent move;move.time.start_ns=-S;move.time.end_ns=1250*ms;move.start_x=move.end_x=.5;move.start_y=move.end_y=.5;l.move_events={move};
    move.time.start_ns=1250*ms;move.time.end_ns=1450*ms;move.start_x=.5;move.end_x=.6;l.move_events.push_back(move);
    move.time.start_ns=1450*ms;move.time.end_ns=2*S;move.start_x=move.end_x=.6;l.move_events.push_back(move);
    NumericLineEvent numeric;numeric.time.start_ns=-S;numeric.time.end_ns=20*S;numeric.start=numeric.end=0;l.rotate_events={numeric};
    numeric.start=numeric.end=1;l.opacity_events={numeric};return c;
}
VisualCandidateFrame frame(unsigned seq,Ns song,double x=-1,double y=-1) {
    VisualCandidateFrame f;f.sequence=seq;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;
    f.source_id="synthetic-source";f.source_sha256=std::string(64,'d');f.frame_id="frame-"+std::to_string(seq);
    f.evidence_id="evidence-"+std::to_string(seq);f.frame_sha256=std::string(64,"0123456789abcdef"[seq%16]);
    f.clock_id="QPC-receipt-only";f.opening_context_id="manual-round-1";f.clock_kind=VisualSourceClock::arrival_only;
    f.received_host_ns=O+song;f.source_valid=f.centers_complete_unoccluded=true;
    const auto seconds=static_cast<double>(song)/S;
    const auto native_x=song<1250*ms?640:song<1450*ms?640+128*(static_cast<double>(song-1250*ms)/(200*ms)):768;
    f.centers={{"yellow-0",x<0?native_x:x,y<0?360+(1.5*seconds-3)*(720/(2*.83175)):y}};return f;
}
FirstNoteTriggerStatus feed(AdaptiveFirstNoteTrigger& t,VisualCandidateFrame f){return t.observe(f,true,f.received_host_ns+ms);}
FirstNoteTriggerStatus ready(AdaptiveFirstNoteTrigger& t) {
    check(feed(t,frame(1,1570*ms)).support_count==1,"single native image fired");
    return feed(t,frame(2,1670*ms));
}
std::vector<std::uint8_t> image(){return std::vector<std::uint8_t>(1280*720*3,0);}
void rectangle(std::vector<std::uint8_t>& pixels,int x,int y,int w,int h,std::array<std::uint8_t,3> color) {
    for(int row=y;row<y+h;++row)for(int col=x;col<x+w;++col)for(std::size_t c=0;c<3;++c)
        pixels[(static_cast<std::size_t>(row)*1280+static_cast<std::size_t>(col))*3+c]=color[c];
}
void oriented_bar(std::vector<std::uint8_t>& pixels,double x,double y,double length,double thickness,double degrees,std::array<std::uint8_t,3> color) {
    const auto radians=degrees*std::numbers::pi/180,c=std::cos(radians),s=std::sin(radians);
    for(int row=0;row<720;++row)for(int col=0;col<1280;++col){
        const auto dx=col-x,dy=row-y,u=c*dx+s*dy,v=-s*dx+c*dy;
        if(std::abs(u)<=length/2&&std::abs(v)<=thickness/2)for(std::size_t ch=0;ch<3;++ch)
            pixels[(static_cast<std::size_t>(row)*1280+static_cast<std::size_t>(col))*3+ch]=color[ch];
    }
}
}
int main(int argc,char** argv) {
try {
    {++groups;AdaptiveFirstNoteTrigger t(chart(),-80*ms);const auto s=ready(t);
        check(s.state==FirstNoteTriggerState::ready&&s.anchor&&s.support_count==2,"native Drag pair failed");
        check(s.anchor->target_host_ns==O+2*S-80*ms&&std::abs(s.anchor->origin_host_ns-O)<10,"native chart time or phase wrong");
        check(s.raw_candidate_count==1&&s.first_note_positive_candidates==1&&!s.timing_qualified,"native branch/qualification wrong");
        check(t.description().note_time_ns==2*S&&t.description().note_kind==NoteKind::drag&&t.description().modeled_notes==1,"description wrong");
        check(sha256_text(t.description().model_sha256)&&t.description().native_pieces==3,"native model identity/pieces wrong");
        AdaptiveFirstNoteTrigger different_phase(chart(),80*ms);
        check(different_phase.description().model_sha256==t.description().model_sha256,"execution phase polluted immutable opening identity");
        check(s.anchor->received_host_ns==O+1670*ms&&s.anchor->supporting_evidence_ids==std::vector<std::string>{"evidence-1","evidence-2"},"actual supports replaced");
        check(t.take_anchor(O+1672*ms).has_value()&&!t.take_anchor(O+1673*ms),"native trigger not one-shot");
        check(t.status(O+10*S,false).state==FirstNoteTriggerState::fired,"fired trigger expired as lease");}
    {++groups;auto c=chart();c.lines[0].notes[0].floor_position=2.5714285;c.lines[0].notes[0].start_ns=c.lines[0].notes[0].end_ns=1'714'285'714;
        AdaptiveFirstNoteTrigger t(c,-80*ms);const auto expected=[&](unsigned seq,Ns time) {
            auto f=frame(seq,time);f.centers[0].y=360+(1.5*static_cast<double>(time)/S-2.5714285)*(720/(2*.83175));return f;
        };
        feed(t,expected(1,1280*ms));auto s=feed(t,expected(2,1370*ms));
        check(s.state==FirstNoteTriggerState::ready&&s.anchor,"moving-x early Drag rejected");
        check(s.anchor->target_host_ns-(O+1371*ms)>200*ms,"native early lead missing");
        AdaptiveFirstNoteTrigger wrong(c,-80*ms);feed(wrong,expected(1,1280*ms));auto f=expected(2,1370*ms);f.centers[0].x=640;
        check(feed(wrong,f).state==FirstNoteTriggerState::acquiring,"fixed x accepted while line moves");}
    {++groups;AdaptiveFirstNoteTrigger t(chart(),0);auto f=frame(1,1570*ms);feed(t,f);auto newer=frame(2,1670*ms);
        newer.frame_sha256=f.frame_sha256;check(feed(t,newer).state==FirstNoteTriggerState::acquiring,"unchanged native pixels fired");
        AdaptiveFirstNoteTrigger replay(chart(),0);feed(replay,f);newer=frame(2,1670*ms);newer.evidence_id=f.evidence_id;
        check(feed(replay,newer).state==FirstNoteTriggerState::cancelled,"replayed native evidence accepted");
        AdaptiveFirstNoteTrigger stale(chart(),0);check(stale.observe(f,true,f.received_host_ns+100*ms).state==FirstNoteTriggerState::cancelled,"stale native observation accepted");
        AdaptiveFirstNoteTrigger changed(chart(),0);feed(changed,f);newer=frame(2,1670*ms);newer.geometry_version=2;
        check(feed(changed,newer).state==FirstNoteTriggerState::cancelled,"native geometry invalidation missing");}
    {++groups;AdaptiveFirstNoteTrigger t(chart(),0);ready(t);check(!t.take_anchor(O+1770*ms),"native exact100ms freshness accepted");
        AdaptiveFirstNoteTrigger ui(chart(),0);ready(ui);ui.status(O+1672*ms,false);check(!ui.take_anchor(O+1673*ms),"unknown UI reused native anchor");
        AdaptiveFirstNoteTrigger late(chart(),-80*ms);feed(late,frame(1,1700*ms));
        check(feed(late,frame(2,1800*ms)).state==FirstNoteTriggerState::cancelled,"late native origin accepted");
        AdaptiveFirstNoteTrigger clock(chart(),0);ready(clock);check(clock.status(O+1660*ms).state==FirstNoteTriggerState::cancelled,"native host rollback accepted");}
    {++groups;auto c=chart();auto alias=c.lines[0].notes[0];alias.start_ns=alias.end_ns=3*S;alias.source_index=1;
        alias.source.json_pointer="/judgeLineList/0/notesAbove/1";alias.source.id=c.source.chart_sha256+'#'+alias.source.json_pointer;
        alias.floor_position=3.15;c.lines[0].notes.push_back(alias);++c.note_count;
        AdaptiveFirstNoteTrigger t(c,0);auto f=frame(1,1750*ms);auto s=feed(t,f);
        check(s.raw_candidates.empty()&&s.state==FirstNoteTriggerState::acquiring,"unmatched definitely visible native alias ignored");
        f=frame(2,1770*ms);f.centers.push_back({"yellow-1",f.centers[0].x,f.centers[0].y-.15*(720/(2*.83175))});
        check(feed(t,f).first_note_positive_candidates==1,"two native visible notes not assigned");}
    {++groups;auto c=chart();auto alias=c.lines[0].notes[0];alias.start_ns=alias.end_ns=3*S;alias.source_index=1;
        alias.source.json_pointer="/judgeLineList/0/notesAbove/1";alias.source.id=c.source.chart_sha256+'#'+alias.source.json_pointer;
        c.lines[0].notes.push_back(alias);++c.note_count;
        AdaptiveFirstNoteTrigger t(c,0);auto f=frame(1,1570*ms);f.centers.push_back({"yellow-1",f.centers[0].x,f.centers[0].y});
        auto s=feed(t,f);check(s.raw_candidates.size()==2,"indistinguishable native assignments collapsed");
        check(s.state==FirstNoteTriggerState::acquiring&&!s.anchor,"ambiguous lowest native center fired");}
    {++groups;auto c=chart();c.lines[0].notes[0].source.id="wrong";rejects([&]{AdaptiveFirstNoteTrigger t(c,0);},"native wrong source accepted");
        c=chart();c.lines[0].notes[0].kind=NoteKind::hold;AdaptiveFirstNoteTrigger hold(c,0);
        check(hold.description().note_kind==NoteKind::hold&&ready(hold).state==FirstNoteTriggerState::ready,"native Hold head not retained");
        c=chart();c.lines[0].notes[0].side=NoteSide::below;
        c.lines[0].notes[0].source.json_pointer="/judgeLineList/0/notesBelow/0";
        c.lines[0].notes[0].source.id=c.source.chart_sha256+'#'+c.lines[0].notes[0].source.json_pointer;
        AdaptiveFirstNoteTrigger below(c,0);
        auto one=frame(1,1690*ms);one.centers[0].y=720-one.centers[0].y;feed(below,one);
        auto two=frame(2,1780*ms);two.centers[0].y=720-two.centers[0].y;
        check(feed(below,two).state==FirstNoteTriggerState::ready,"native upward below approach lost");
        c=chart();auto duplicate=c.lines[0].notes[0];duplicate.source_index=1;duplicate.position_x=1;
        duplicate.source.json_pointer="/judgeLineList/0/notesAbove/1";duplicate.source.id=c.source.chart_sha256+'#'+duplicate.source.json_pointer;
        c.lines[0].notes.push_back(duplicate);++c.note_count;AdaptiveFirstNoteTrigger tied(c,0);
        check(tied.description().earliest_note_count==2,"earliest native group collapsed");
        one=frame(1,1570*ms);one.centers.push_back({"yellow-1",one.centers[0].x+72,one.centers[0].y});feed(tied,one);
        two=frame(2,1670*ms);two.centers.push_back({"yellow-1",two.centers[0].x+72,two.centers[0].y});
        check(feed(tied,two).state==FirstNoteTriggerState::ready,"two-note earliest group did not match jointly");
        c=chart();c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=90;
        AdaptiveFirstNoteTrigger rotated(c,0);PhigrosPoseEvaluator pose(c);
        const auto rotated_frame=[&](unsigned seq,Ns t) {
            auto f=frame(seq,t);const auto p=pose.line_at(0,t);const auto z=(1.5*static_cast<double>(t)/S-3)*(720/(2*.83175));
            f.centers[0].x=p.origin.x*1280-z*p.above_normal.x;f.centers[0].y=p.origin.y*720-z*p.above_normal.y;return f;
        };
        feed(rotated,rotated_frame(1,1570*ms));check(feed(rotated,rotated_frame(2,1670*ms)).state==FirstNoteTriggerState::ready,"native sideways rotated approach failed");
        c=chart();c.lines[0].speed_events[0].time.start_ns=1;rejects([&]{AdaptiveFirstNoteTrigger t(c,0);},"native speed gap accepted");
        c=chart();c.offset_seconds=.01;
        for(auto& n:c.lines[0].notes){n.start_ns+=10*ms;n.end_ns+=10*ms;}
        for(auto& e:c.lines[0].move_events){e.time.start_ns+=10*ms;e.time.end_ns+=10*ms;}
        for(auto& e:c.lines[0].rotate_events){e.time.start_ns+=10*ms;e.time.end_ns+=10*ms;}
        for(auto& e:c.lines[0].opacity_events){e.time.start_ns+=10*ms;e.time.end_ns+=10*ms;}
        for(auto& e:c.lines[0].speed_events){e.time.start_ns+=10*ms;e.time.end_ns+=10*ms;}
        AdaptiveFirstNoteTrigger offset(c,0);one=frame(1,1570*ms);one.received_host_ns+=10*ms;feed(offset,one);
        two=frame(2,1670*ms);two.received_host_ns+=10*ms;
        check(feed(offset,two).state==FirstNoteTriggerState::ready&&offset.description().note_time_ns==2010*ms,"native offset lost or applied twice");
        c=chart();c.lines[0].notes[0].kind=NoteKind::flick;AdaptiveFirstNoteTrigger flick(c,0);
        check(flick.description().first_down_ns==1970*ms&&flick.description().note_time_ns==2*S,"Flick DOWN lead confused with native judgment");
        rejects([&]{AdaptiveFirstNoteTrigger t(chart(),101*ms);},"native phase overflow accepted");}
    {++groups;AdaptiveFirstNoteTrigger t(chart(),0);auto f=frame(1,1570*ms);f.centers_complete_unoccluded=false;
        check(feed(t,f).state==FirstNoteTriggerState::cancelled,"native coverage assumption inferred");
        AdaptiveFirstNoteTrigger pts(chart(),0);f=frame(1,1570*ms);f.clock_kind=VisualSourceClock::video_pts;
        check(feed(pts,f).state==FirstNoteTriggerState::cancelled,"native video clock relabeled receipt");
        AdaptiveFirstNoteTrigger upward(chart(),0);feed(upward,frame(1,1570*ms));
        check(feed(upward,frame(2,1670*ms,-1,50)).state==FirstNoteTriggerState::acquiring,"upward native center fired");}
    {++groups;auto c=chart();auto original=c.lines[0];c.lines[0].notes.clear();original.source_index=1;
        original.notes[0].source.json_pointer="/judgeLineList/1/notesAbove/0";original.notes[0].source.id=c.source.chart_sha256+'#'+original.notes[0].source.json_pointer;
        original.speed_events[0].source.json_pointer="/judgeLineList/1/speedEvents/0";c.lines.push_back(original);
        AdaptiveFirstNoteTrigger t(c,0);check(t.description().line_index==1,"line0 implicitly selected over actual first native line");
        check(ready(t).state==FirstNoteTriggerState::ready,"first note on nonzero native line not supported");
        c.note_count=2;rejects([&]{AdaptiveFirstNoteTrigger bad(c,0);},"mismatched semantic note count accepted");}
    {++groups;auto c=chart();c.lines[0].move_events.resize(1);auto& move=c.lines[0].move_events[0];move.time.end_ns=20*S;
        c.lines[0].notes[0].floor_position=-.8;c.lines[0].speed_events.clear();
        SpeedLineEvent negative;negative.time.start_ns=0;negative.time.end_ns=1400*ms;negative.value=-1;negative.source.json_pointer="/judgeLineList/0/speedEvents/negative";
        auto positive=negative;positive.time.start_ns=1400*ms;positive.time.end_ns=20*S;positive.value=1;positive.source.json_pointer="/judgeLineList/0/speedEvents/positive";
        c.lines[0].speed_events={negative,positive};AdaptiveFirstNoteTrigger t(c,0);
        auto f=frame(1,1600*ms,640,360+(-1.2+.8)*(720/(2*.83175)));auto s=feed(t,f);
        check(s.raw_candidates.size()==2,"negative native time alias discarded");
        f=frame(2,1700*ms,640,360+(-1.1+.8)*(720/(2*.83175)));s=feed(t,f);
        check(s.state==FirstNoteTriggerState::ready&&s.raw_candidates.size()==2,"negative alias blocks justified downward final branch");
        c.lines[0].notes[0].floor_position=.5;c.lines[0].speed_events.clear();
        const auto append=[&](Ns begin,Ns end,double speed) {
            SpeedLineEvent e;e.time.start_ns=begin;e.time.end_ns=end;e.value=speed;e.source.json_pointer="/judgeLineList/0/speedEvents/"+std::to_string(c.lines[0].speed_events.size());
            c.lines[0].speed_events.push_back(e);
        };
        append(0,500*ms,0);append(500*ms,S,1);append(S,1500*ms,-1);append(1500*ms,20*S,1);
        AdaptiveFirstNoteTrigger ambiguous(c,0);
        feed(ambiguous,frame(1,1600*ms,640,360+(.1-.5)*(720/(2*.83175))));
        const auto unresolved=feed(ambiguous,frame(2,1700*ms,640,360+(.2-.5)*(720/(2*.83175))));
        check(unresolved.state==FirstNoteTriggerState::acquiring&&unresolved.selected_intervals.size()>1,"earlier positive native branch silently dropped");}
    {++groups;auto pixels=image();rectangle(pixels,600,90,80,4,{250,235,60});
        auto detected=detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::drag);
        check(detected.valid&&detected.centers.size()==1&&detected.centers[0].x==639.5&&detected.centers[0].y==91.5,"yellow Drag detection wrong");
        check(!detected.centers_complete_unoccluded,"yellow detector inferred coverage");
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).valid,"yellow Drag treated as cyan Tap");
        pixels=image();rectangle(pixels,600,90,80,4,{255,255,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::drag).valid,"white line treated as yellow note");
        pixels=image();rectangle(pixels,600,90,80,4,{0,230,255});
        check(detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).valid,"archived cyan detector changed");
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::drag).valid,"cyan Tap treated as Drag");
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::flick).valid,"cyan Tap treated as Flick");
        check(detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::hold).valid,"cyan Hold head rejected");
        pixels=image();rectangle(pixels,600,90,80,4,{245,80,220});
        detected=detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::flick);
        check(detected.valid&&detected.centers.size()==1&&detected.centers[0].id=="pink-0","pink Flick center detection wrong");
        check(!detected.centers_complete_unoccluded,"pink detector inferred complete source coverage");
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::drag).valid,"pink Flick treated as yellow Drag");}
    {++groups;auto c=chart();AdaptiveFirstNoteTrigger baseline(c,0),explicit_down(c,0,1990*ms);
        const auto selected=ready(explicit_down);
        check(selected.anchor&&selected.anchor->target_host_ns==O+1990*ms,"compiled first DOWN not used for dispatch");
        check(explicit_down.description().note_time_ns==2*S,"compiled first DOWN replaced native judgment");
        rejects([&]{AdaptiveFirstNoteTrigger bad(c,0,2001*ms);},"post-judgment first DOWN accepted");
        c.import_policy_id="candidate-policy";c.import_interpretation_unqualified=true;
        AdaptiveFirstNoteTrigger candidate(c,0);
        check(candidate.description().model_sha256!=baseline.description().model_sha256,"import policy missing from opening hash");
        ChartImportAnomaly anomaly;anomaly.source.id="native-source";anomaly.code="empty-event";anomaly.original_declaration_json="{\"endTime\":0}";
        c.import_anomalies.push_back(anomaly);AdaptiveFirstNoteTrigger anomalous(c,0);
        check(candidate.description().model_sha256!=anomalous.description().model_sha256,"candidate anomaly missing from opening hash");
        c.import_anomalies[0].original_declaration_json="{\"endTime\":1}";AdaptiveFirstNoteTrigger changed(c,0);
        check(changed.description().model_sha256!=anomalous.description().model_sha256,"original anomaly semantics missing from opening hash");}
    {++groups;auto c=chart();auto& rotation=c.lines[0].rotate_events[0];
        rotation.time.start_ns=0;rotation.time.end_ns=2*S;rotation.start=-20;rotation.end=20;
        AdaptiveFirstNoteTrigger rotating(c,0);PhigrosPoseEvaluator pose(c);
        for(unsigned seq=1;seq<=2;++seq){
            const auto t=(seq==1?1570:1670)*ms;auto f=frame(seq,t);const auto p=pose.line_at(0,t);
            const auto z=(1.5*static_cast<double>(t)/S-3)*(720/(2*.83175));
            f.centers[0].x=p.origin.x*1280-z*p.above_normal.x;f.centers[0].y=p.origin.y*720-z*p.above_normal.y;
            const auto status=feed(rotating,f);
            check(std::any_of(status.raw_candidates.begin(),status.raw_candidates.end(),[&](const auto& v){return v.song_time.lower<=t&&t<=v.song_time.upper;}),"rotating interior truth pruned by affine chord");
        }
        c=chart();const auto append_alias=[&](double speed,double floor){
            auto n=c.lines[0].notes.front();n.source_index=c.lines[0].notes.size();n.start_ns=n.end_ns=3*S;n.speed=speed;n.floor_position=floor;
            n.source.json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(n.source_index);n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;
            c.lines[0].notes.push_back(n);++c.note_count;
        };
        append_alias(0,100);append_alias(-1,2.9);AdaptiveFirstNoteTrigger signed_aliases(c,0);
        check(signed_aliases.description().modeled_notes==3,"zero or negative note-speed alias omitted");
        auto f=frame(1,1670*ms);auto status=feed(signed_aliases,f);
        check(status.raw_candidates.empty()&&!status.anchor,"unmatched visible zero-speed alias ignored");
        f=frame(2,1670*ms+1);f.centers.push_back({"zero-alias",f.centers[0].x,360});
        f.centers.push_back({"negative-alias",f.centers[0].x,360-(1.5*static_cast<double>(1670*ms+1)/S-2.9)*(720/(2*.83175))});
        status=feed(signed_aliases,f);
        check(std::any_of(status.raw_candidates.begin(),status.raw_candidates.end(),[&](const auto& v){return v.song_time.lower<=1670*ms+1&&1670*ms+1<=v.song_time.upper;}),"source-complete signed aliases lost native truth");}
    {++groups;const auto verify=[&](NoteKind kind,double angle,std::array<std::uint8_t,3> color){
            auto pixels=image();oriented_bar(pixels,640,200,100,4,angle,color);
            const auto detected=detect_adaptive_opening_centers(1280,720,3840,pixels,kind);
            check(detected.valid&&detected.centers.size()==1,"oriented typed head missing or duplicated");
            check(std::hypot(detected.centers[0].x-640,detected.centers[0].y-200)<1,"oriented head center biased");
            check(!detected.centers_complete_unoccluded,"oriented detector inferred native coverage");
        };
        verify(NoteKind::tap,-32.7,{0,230,255});verify(NoteKind::hold,90,{0,230,255});
        verify(NoteKind::drag,73,{250,235,60});verify(NoteKind::flick,-47,{245,80,220});
        auto pixels=image();oriented_bar(pixels,640,200,100,4,-32.7,{255,255,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).valid,"rotated white line treated as cyan head");
        pixels=image();oriented_bar(pixels,640,200,100,4,-32.7,{0,230,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::flick).valid,"rotated cyan head treated as pink");
        rectangle(pixels,700,300,80,4,{0,230,255});
        check(detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).centers.size()==2,"rotated alias or horizontal head discarded");
        pixels=image();rectangle(pixels,600,90,80,4,{0,230,255});rectangle(pixels,638,94,4,626,{0,230,255});
        auto detected=detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::hold);
        check(detected.valid&&detected.centers.size()==1&&detected.centers[0].y==91.5,"Hold body shifted explicit horizontal head");
        pixels=image();rectangle(pixels,600,90,80,204,{0,230,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::hold).valid,"wide Hold body accepted as head centroid");
        pixels=image();rectangle(pixels,600,90,80,4,{0,230,255});rectangle(pixels,600,130,80,4,{0,230,255});rectangle(pixels,638,94,4,36,{0,230,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).valid,"merged head components accepted");
        pixels=image();oriented_bar(pixels,20,200,100,4,32.7,{0,230,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).valid,"edge-clipped oriented head accepted");
        pixels=image();oriented_bar(pixels,640,45,100,4,45,{0,230,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).valid,"ROI-clipped oriented head accepted");
        pixels=image();oriented_bar(pixels,640,32,100,4,2,{0,230,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).valid,"corner-clipped near-horizontal head accepted");
        pixels=image();oriented_bar(pixels,640,200,100,4,32,{0,230,255});
        oriented_bar(pixels,640-10*std::sin(32*std::numbers::pi/180),200+10*std::cos(32*std::numbers::pi/180),100,4,32,{0,230,255});
        oriented_bar(pixels,640-5*std::sin(32*std::numbers::pi/180),200+5*std::cos(32*std::numbers::pi/180),14,2,122,{0,230,255});
        check(!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap).valid,"merged oriented heads accepted");
        pixels=image();for(int y=30;y<570;y+=2)for(int x=0;x<1280;x+=2)rectangle(pixels,x,y,1,1,{0,230,255});
        detected=detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::tap);
        check(!detected.valid&&detected.reason=="oriented_head_run_budget_exhausted","oriented run bound not enforced");}
    {++groups;auto c=chart();auto edge=c.lines[0];edge.source_index=1;edge.notes[0].start_ns=edge.notes[0].end_ns=3*S;edge.notes[0].speed=0;
        edge.notes[0].source.json_pointer="/judgeLineList/1/notesAbove/0";edge.notes[0].source.id=c.source.chart_sha256+'#'+edge.notes[0].source.json_pointer;
        for(auto& e:edge.move_events)e.start_y=e.end_y=50.0/720;
        c.lines.push_back(edge);++c.note_count;AdaptiveFirstNoteTrigger t(c,0);
        const auto s=feed(t,frame(1,1570*ms));
        check(!s.raw_candidates.empty(),"ROI-clipped alias incorrectly treated as definitely full-head visible");
        check(!s.anchor,"single frame with clipped alias armed");
        check(t.description().detector_policy_id=="typed-rle-oriented-head-v1","detector policy missing from description");}
    if(argc==2) {
        ++groups;std::ifstream input(argv[1],std::ios::binary);check(static_cast<bool>(input),"private chart missing");
        const std::string raw((std::istreambuf_iterator<char>(input)),{});auto native=parse_phigros(raw,{"private-offline","","Glaciaxion","IN","4.0.1/157"});
        AdaptiveFirstNoteTrigger t(native,-80*ms);const auto& d=t.description();
        check(d.note_time_ns==1'714'285'714&&d.note_kind==NoteKind::drag&&d.line_index==0,"private first Drag identity changed");
        check(d.note_json_pointer=="/judgeLineList/0/notesAbove/0"&&d.modeled_notes==389,"private whole same-color alias count changed");
        PhigrosPoseEvaluator pose(native);const auto& n=native.lines[0].notes[0];
        for(unsigned seq=1;seq<=2;++seq) {
            const auto song=seq==1?1'250'000'000LL:1'350'000'000LL;auto f=frame(seq,song);
            const auto p=pose.note_target(0,n,song).reference_pixels;
            f.centers={{"yellow-0",p.x,p.y+(static_cast<double>(song)/S*1.5-n.floor_position)*(720/(2*.83175))}};
            const auto result=feed(t,f);if(seq==2)check(result.state==FirstNoteTriggerState::ready&&result.anchor,"private early dynamic native Drag pair failed");
        }
    }
    std::cout<<groups<<" adaptive-opening groups / "<<checks<<" checks passed; offline model only, no gameplay/timing qualification.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
