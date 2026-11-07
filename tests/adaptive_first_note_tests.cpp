#include "../apps/chart_experiment/adaptive_opening_detector.hpp"
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
SemanticChart native_v1_chart() {
    constexpr std::string_view raw=R"({"formatVersion":1,"offset":0,"judgeLineList":[{"bpm":200,
        "notesAbove":[{"type":1,"time":256,"positionX":-0.4385958,"holdTime":0,"speed":1,"floorPosition":2.88}],"notesBelow":[],
        "speedEvents":[{"startTime":-10666,"endTime":0,"value":1},{"startTime":0,"endTime":2752,"value":1.2}],
        "judgeLineMoveEvents":[{"startTime":-10666,"endTime":2752,"start":440260,"end":440260}],
        "judgeLineRotateEvents":[{"startTime":-10666,"endTime":2752,"start":0,"end":0}],
        "judgeLineDisappearEvents":[{"startTime":-10666,"endTime":2752,"start":1,"end":1}]}]})";
    ImportOptions options;options.policy=PhigrosImportPolicy::native_v1_source_priority_constant_empty_candidate_v1;
    return parse_phigros(raw,{"synthetic-native-v1","","fixture","IN","offline"},{},options);
}
SemanticChart occlusion_chart() {
    auto c=chart();auto& notes=c.lines[0].notes;notes[0].kind=NoteKind::hold;notes[0].position_x=-4;
    auto tap=notes[0];tap.kind=NoteKind::tap;tap.position_x=4;notes.push_back(tap);
    for(unsigned i=0;i<3;++i){auto alias=tap;alias.start_ns=alias.end_ns=3*S;
        alias.position_x=0;alias.speed=0;alias.floor_position=0;notes.push_back(alias);}
    for(std::size_t i=0;i<notes.size();++i){auto& n=notes[i];n.source_index=i;
        n.source.json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(i);n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;}
    c.note_count=notes.size();return c;
}
SemanticChart rotating_flick_alias_chart(double along=63) {
    auto c=occlusion_chart();auto aliases=c.lines[0].notes;c.lines[0].notes.resize(2);
    for(auto& note:c.lines[0].notes)note.kind=NoteKind::flick;
    for(std::size_t i=0;i<3;++i){auto line=c.lines[0];line.source_index=i+1;auto note=aliases[i+2];
        note.kind=NoteKind::flick;note.position_x=along/72;note.source_index=0;
        note.source.json_pointer="/judgeLineList/"+std::to_string(i+1)+"/notesAbove/0";
        note.source.id=c.source.chart_sha256+'#'+note.source.json_pointer;line.notes={note};
        line.move_events.resize(1);auto& move=line.move_events[0];move.time.start_ns=0;move.time.end_ns=20*S;
        move.start_x=move.end_x=move.start_y=move.end_y=.5;
        auto& rotate=line.rotate_events[0];rotate.time.start_ns=0;rotate.time.end_ns=2*S;
        rotate.start=-371+360*static_cast<double>(i)+.000002*static_cast<double>(i);
        rotate.end=89+360*static_cast<double>(i)+.000002*static_cast<double>(i);
        line.speed_events[0].source.json_pointer="/judgeLineList/"+std::to_string(i+1)+"/speedEvents/0";
        c.lines.push_back(std::move(line));
    }
    return c;
}
SemanticChart mixed_earliest_chart() {
    auto c=chart();c.lines[0].notes[0].kind=NoteKind::flick;c.lines[0].notes[0].position_x=-4;
    auto second=c.lines[0];second.source_index=1;auto& tap=second.notes[0];tap.kind=NoteKind::tap;tap.position_x=4;
    tap.source.json_pointer="/judgeLineList/1/notesAbove/0";tap.source.id=c.source.chart_sha256+'#'+tap.source.json_pointer;
    second.speed_events[0].source.json_pointer="/judgeLineList/1/speedEvents/0";c.lines.push_back(second);++c.note_count;return c;
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
VisualCandidateFrame occlusion_frame(unsigned seq,Ns song) {
    auto f=frame(seq,song);const auto y=f.centers[0].y;
    f.centers={{"earliest-hold",480,y},{"earliest-tap",1056,y},{"one-ornament",768,360}};return f;
}
VisualCandidateFrame rotating_flick_frame(const SemanticChart& c,unsigned seq,Ns song) {
    auto f=occlusion_frame(seq,song);const auto p=PhigrosPoseEvaluator(c).note_target(1,c.lines[1].notes[0],song).reference_pixels;
    f.centers.back().x=p.x;f.centers.back().y=p.y;return f;
}
FirstNoteTriggerStatus ready(AdaptiveFirstNoteTrigger& t) {
    check(feed(t,frame(1,1570*ms)).support_count==1,"single native image fired");
    return feed(t,frame(2,1670*ms));
}
SemanticChart appearance_chart(bool viewport_offscreen=true,bool clipped_destination=false,Ns transition=10*ms) {
    auto c=chart();auto& first=c.lines[0];first.notes[0].start_ns=first.notes[0].end_ns=5*S;
    first.notes[0].floor_position=0;first.notes[0].position_x=0;
    first.speed_events.resize(1);first.speed_events[0].value=0;
    first.move_events.resize(1);auto& move=first.move_events[0];move.time.start_ns=-S;move.time.end_ns=20*S;
    move.start_x=move.end_x=.3;move.start_y=move.end_y=.2;
    auto next=first;next.source_index=1;auto& note=next.notes[0];note.start_ns=note.end_ns=6*S;
    note.source.json_pointer="/judgeLineList/1/notesAbove/0";note.source.id=c.source.chart_sha256+'#'+note.source.json_pointer;
    note.floor_position=viewport_offscreen?3:.8;
    next.move_events[0].start_x=next.move_events[0].end_x=.7;
    next.move_events[0].start_y=next.move_events[0].end_y=clipped_destination?.15:.5;
    auto speed=next.speed_events[0];speed.time.start_ns=0;speed.time.end_ns=2500*ms;speed.value=0;
    speed.source.json_pointer="/judgeLineList/1/speedEvents/0";next.speed_events={speed};
    speed.time.start_ns=2500*ms;speed.time.end_ns=2500*ms+transition;speed.value=note.floor_position/(static_cast<double>(transition)/S);
    speed.source.json_pointer="/judgeLineList/1/speedEvents/1";next.speed_events.push_back(speed);
    speed.time.start_ns=2500*ms+transition;speed.time.end_ns=20*S;speed.value=0;
    speed.source.json_pointer="/judgeLineList/1/speedEvents/2";next.speed_events.push_back(speed);
    c.lines.push_back(next);c.note_count=2;return c;
}
VisualCandidateFrame appearance_frame(unsigned seq,Ns song,bool popped,bool clipped=false) {
    auto f=frame(seq,song);f.centers={{"static-earliest",384,576}};
    if(popped)f.centers.push_back({"new-native-head",896,clipped?612.0:360.0});return f;
}
std::vector<std::uint8_t> image(){return std::vector<std::uint8_t>(1280*720*3,0);}
void rectangle(std::vector<std::uint8_t>& pixels,int x,int y,int w,int h,std::array<std::uint8_t,3> color) {
    for(int row=y;row<y+h;++row)for(int col=x;col<x+w;++col)for(std::size_t c=0;c<3;++c)
        pixels[(static_cast<std::size_t>(row)*1280+static_cast<std::size_t>(col))*3+c]=color[c];
}
void yellow_outline(std::vector<std::uint8_t>& pixels,int x,int y,int w,int h) {
    rectangle(pixels,x,y,w,h,{250,235,60});rectangle(pixels,x+2,y+2,w-4,h-4,{0,0,0});
}
void yellow_halo(std::vector<std::uint8_t>& pixels,int x,int y) {
    // One closed original beveled contour: long rails and 4px cap paths.
    for(int row=0;row<14;++row){const int depth=std::min(row,13-row),left=17-17*depth/6,right=161-left;
        rectangle(pixels,x+left,y+row,right-left+1,1,{250,235,60});
        if(row>=2&&row<12)rectangle(pixels,x+left+4,y+row,right-left-7,1,{0,0,0});}
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
    {++groups;auto c=appearance_chart();AdaptiveFirstNoteTrigger legacy(c,0),explicit_off(c,0,-1,false,false),candidate(c,0,-1,false,true);
        check(legacy.description().model_sha256==explicit_off.description().model_sha256&&
              !legacy.description().native_head_appearance_witness,"default false changed native opening identity");
        check(candidate.description().native_head_appearance_witness&&candidate.description().model_id=="native-piecewise-2d-opening-v14"&&
              candidate.description().model_sha256!=legacy.description().model_sha256&&
              candidate.description().earliest_note_ids==legacy.description().earliest_note_ids&&
              candidate.description().first_down_ns==legacy.description().first_down_ns,"appearance opt-in changed native first source/deadline or lacks identity");
        for(const auto f:{appearance_frame(1,2350*ms,false),appearance_frame(2,2490*ms,false)}) {
            check(feed(candidate,f).state!=FirstNoteTriggerState::ready,"absence/count alone armed appearance model");feed(legacy,f);
        }
        const auto final=appearance_frame(3,2530*ms,true);const auto status=feed(candidate,final);
        check(status.state==FirstNoteTriggerState::ready&&status.anchor&&status.support_count==3&&status.anchor->supporting_evidence_ids.size()==3,
              "native true-viewport-offscreen to fully-visible continuous jump failed with static earliest group");
        check(status.anchor->chosen_song_interval.upper-status.anchor->chosen_song_interval.lower<=40*ms&&
              status.anchor->target_host_ns-final.received_host_ns-ms>=200*ms&&
              status.anchor->received_host_ns==final.received_host_ns,"appearance fabricated freshness/lead or kept broad origin");
        check(feed(legacy,final).state!=FirstNoteTriggerState::ready,"archived motion-only model silently acquired new appearance behavior");
        rejects([&]{AdaptiveFirstNoteTrigger invalid(c,0,-1,true,true);},"outlined+appearance policy silently combined");
        c.lines[0].notes[0].kind=NoteKind::tap;rejects([&]{AdaptiveFirstNoteTrigger invalid(c,0,-1,false,true);},"non-Drag appearance opt-in accepted");
    }
    {++groups;for(const int mode:{0,1,2}) {
        auto c=appearance_chart(mode!=0,mode==1,mode==2?50*ms:10*ms);AdaptiveFirstNoteTrigger candidate(c,0,-1,false,true);
        feed(candidate,appearance_frame(1,2350*ms,false,mode==1));feed(candidate,appearance_frame(2,2490*ms,false,mode==1));
        const auto status=feed(candidate,appearance_frame(3,mode==2?2570*ms:2530*ms,true,mode==1));
        check(status.state!=FirstNoteTriggerState::ready,"ROI clipping, not-viewport-offscreen, or slow continuous scroll became native jump witness");
    }}
    {++groups;auto c=appearance_chart();auto alias=c.lines[0].notes[0];alias.start_ns=alias.end_ns=7*S;alias.source_index=1;
        alias.source.json_pointer="/judgeLineList/0/notesAbove/1";alias.source.id=c.source.chart_sha256+'#'+alias.source.json_pointer;
        c.lines[0].notes.push_back(alias);++c.note_count;AdaptiveFirstNoteTrigger candidate(c,0,-1,false,true);
        feed(candidate,appearance_frame(1,2350*ms,false));feed(candidate,appearance_frame(2,2490*ms,false));
        check(feed(candidate,appearance_frame(3,2530*ms,true)).state!=FirstNoteTriggerState::ready,
              "favorable earliest-source alias was selected over a retained nonfirst static-source alias");
    }
    {++groups;auto c=appearance_chart();c.lines[0].notes[0].start_ns=c.lines[0].notes[0].end_ns=2650*ms;
        AdaptiveFirstNoteTrigger late(c,0,-1,false,true);feed(late,appearance_frame(1,2350*ms,false));feed(late,appearance_frame(2,2490*ms,false));
        const auto final=appearance_frame(3,2530*ms,true);const auto status=feed(late,final);
        check(status.state==FirstNoteTriggerState::cancelled&&status.reason=="first_note_dispatch_lead_below_200ms",
              "appearance path bypassed actual200ms first-input lead");
        const auto& gate=late.dispatch_gate();
        check(gate&&!gate->allowed&&std::string_view(gate->reason)=="insufficient_lead"&&
              gate->received_host_ns==final.received_host_ns&&gate->evaluated_host_ns==final.received_host_ns+ms&&
              gate->minimum_lead_ns==200*ms&&gate->target_host_ns-gate->evaluated_host_ns<gate->minimum_lead_ns,
              "cancelled dispatch gate lost its actual evaluation, receipt, or lead");
        c=appearance_chart();AdaptiveFirstNoteTrigger stale(c,0,-1,false,true);auto f=appearance_frame(1,2350*ms,false);
        check(stale.observe(f,true,f.received_host_ns+100*ms).state==FirstNoteTriggerState::cancelled,"appearance renewed a100ms stale receipt");
        AdaptiveFirstNoteTrigger epoch(c,0,-1,false,true);feed(epoch,appearance_frame(1,2350*ms,false));f=appearance_frame(2,2490*ms,false);++f.epoch;
        check(feed(epoch,f).state==FirstNoteTriggerState::cancelled,"appearance crossed source epoch");
        AdaptiveFirstNoteTrigger stopped(c,0,-1,false,true);feed(stopped,appearance_frame(1,2350*ms,false));stopped.cancel("operator_stop");
        check(feed(stopped,appearance_frame(2,2490*ms,false)).state==FirstNoteTriggerState::cancelled,"appearance revived stopped trigger");
    }
    {++groups;auto c=appearance_chart();auto extra=c.lines[1];extra.source_index=2;extra.notes[0].source_index=0;
        extra.notes[0].start_ns=extra.notes[0].end_ns=7*S;extra.notes[0].source.json_pointer="/judgeLineList/2/notesAbove/0";
        extra.notes[0].source.id=c.source.chart_sha256+'#'+extra.notes[0].source.json_pointer;
        extra.move_events[0].start_x=extra.move_events[0].end_x=.85;
        for(std::size_t i=0;i<extra.speed_events.size();++i)extra.speed_events[i].source.json_pointer="/judgeLineList/2/speedEvents/"+std::to_string(i);
        c.lines.push_back(extra);++c.note_count;AdaptiveFirstNoteTrigger candidate(c,0,-1,false,true);
        feed(candidate,appearance_frame(1,2350*ms,false));feed(candidate,appearance_frame(2,2490*ms,false));
        auto f=appearance_frame(3,2530*ms,true);f.centers.push_back({"another-new-head",1088,360});
        check(feed(candidate,f).state!=FirstNoteTriggerState::ready,"one favorable jump was chosen from two distinct new native sources");
    }
    {++groups;auto c=appearance_chart();
        for(const Ns dt:{19*ms,201*ms}) {
            AdaptiveFirstNoteTrigger candidate(c,0,-1,false,true);
            feed(candidate,appearance_frame(1,2514*ms-dt,false));
            check(feed(candidate,appearance_frame(2,2514*ms,true)).state!=FirstNoteTriggerState::ready,"appearance bypassed20..200ms actual support spacing");
        }
        AdaptiveFirstNoteTrigger repeated(c,0,-1,false,true);auto first=appearance_frame(1,2350*ms,false);
        feed(repeated,first);feed(repeated,appearance_frame(2,2490*ms,false));auto current=appearance_frame(3,2530*ms,true);
        current.frame_sha256=first.frame_sha256;
        check(feed(repeated,current).state!=FirstNoteTriggerState::ready,"appearance authorized with repeated witness image SHA");
    }
    {++groups;auto pixels=image();yellow_outline(pixels,300,626,120,16);yellow_outline(pixels,850,626,120,16);
        const auto old=detect_extended_drag_opening_centers(1280,720,3840,pixels);
        check(!old.valid,"outlined fixture no longer exercises preserved legacy rejection");
        const auto detected=detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels);
        check(detected.valid&&detected.centers.size()==2&&detected.centers[0].x==359.5&&detected.centers[1].x==909.5&&
            detected.centers[0].y==633.5&&detected.centers[1].y==633.5,"closed original contours did not yield their own exact bbox centers");
        check(!detected.centers_complete_unoccluded,"outlined detector fabricated coverage");
        pixels=image();rectangle(pixels,400,200,100,6,{250,235,60});
        check(detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels).valid,"solid legacy yellow head regressed under opt-in");
        pixels=image();rectangle(pixels,400,200,162,13,{250,235,60});rectangle(pixels,404,204,154,5,{0,0,0});
        const auto thick=detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels);
        check(thick.valid&&thick.centers.size()==1&&thick.centers[0].x==480.5&&thick.centers[0].y==206,
            "thirteen-row closed outline with two four-row rails failed original row-band proof");
    }
    {++groups;auto c=chart();AdaptiveFirstNoteTrigger before(c,0),explicit_false(c,0,-1,false),outlined(c,0,-1,true);
        check(before.description().model_sha256==explicit_false.description().model_sha256&&
            before.description().model_sha256=="baa3fc12f2c4572135a186f27ce5b3688540ddcc6ccf693c8c2e88c6db3da741"&&
            !before.description().includes_outlined_drag_heads,"false outlined flag changed archived default geometry/hash");
        check(outlined.description().includes_outlined_drag_heads&&outlined.description().model_id=="native-piecewise-2d-opening-v13"&&
            outlined.description().detector_policy_id=="typed-rle-closed-outline-horizontal-drag-roi650-v3"&&
            outlined.description().model_sha256!=before.description().model_sha256&&
            outlined.description().first_down_ns==before.description().first_down_ns&&
            outlined.description().earliest_note_ids==before.description().earliest_note_ids,"opt-in did not bind a separate detector identity with immutable native sources");
        auto pixels=image();yellow_outline(pixels,600,200,120,16);
        check(detect_model_opening_centers(1280,720,3840,pixels,outlined.description()).valid,"opt-in model did not route the outline helper");
        c.lines[0].notes[0].kind=NoteKind::tap;rejects([&]{AdaptiveFirstNoteTrigger invalid(c,0,-1,true);},"outline enabled for non-Drag earliest source");
        c=mixed_earliest_chart();rejects([&]{AdaptiveFirstNoteTrigger invalid(c,0,-1,true);},"outline enabled for mixed earliest families");
    }
    {++groups;auto pixels=image();yellow_halo(pixels,300,626);
        check(!detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels).valid,
            "wide-cap halo without an original solid core silently used a larger standalone cap tube");
        rectangle(pixels,317,630,128,6,{250,235,60});yellow_halo(pixels,850,626);rectangle(pixels,867,630,128,6,{250,235,60});
        const auto composite=detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels);
        check(composite.valid&&composite.centers.size()==2&&composite.groups==4&&
            composite.centers[0].x==380.5&&composite.centers[1].x==930.5&&composite.centers[0].y==632.5&&
            composite.metrics[0].width==128&&composite.metrics[0].rows==6,
            "two disjoint original halo/core pairs were duplicated, averaged or lost their solid-core center");
        rectangle(pixels,1050,400,100,6,{250,235,60});
        check(detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels).centers.size()==3,
            "an independent qualified third head was silently ignored as an outlier");
    }
    {++groups;const auto rejected=[](const auto& pixels){const auto d=detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels);
            check(!d.valid&&d.centers.empty()&&d.metrics.size()<=8,"unowned or invalid composite retained solver centers");};
        auto pixels=image();yellow_halo(pixels,400,200);rectangle(pixels,420,204,128,6,{250,235,60});rejected(pixels);
        pixels=image();yellow_halo(pixels,400,200);rectangle(pixels,402,204,158,6,{250,235,60});rejected(pixels);
        pixels=image();yellow_halo(pixels,400,200);rectangle(pixels,417,201,128,6,{250,235,60});rejected(pixels);
        pixels=image();yellow_halo(pixels,400,200);rectangle(pixels,417,204,128,6,{250,235,60});
        rectangle(pixels,400,206,19,1,{0,0,0});rejected(pixels);
        pixels=image();yellow_halo(pixels,400,200);rectangle(pixels,417,204,128,6,{250,235,60});
        rectangle(pixels,480,202,1,10,{250,235,60});rejected(pixels);
        pixels=image();yellow_halo(pixels,400,200);rectangle(pixels,417,204,128,6,{250,235,60});
        rectangle(pixels,800,300,80,30,{250,235,60});rejected(pixels);
        pixels=image();rectangle(pixels,400,200,170,18,{250,235,60});rectangle(pixels,401,201,168,16,{0,0,0});
        rectangle(pixels,404,202,162,14,{250,235,60});rectangle(pixels,405,203,160,12,{0,0,0});
        rectangle(pixels,421,206,128,6,{250,235,60});rejected(pixels);
        check(detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels).reason=="outlined_drag_ambiguous_nested_ownership",
            "two qualified halos sharing an original core did not reject closed ownership ambiguity");
    }
    {++groups;const auto rejected=[](const auto& pixels){const auto r=detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels);
            check(!r.valid&&r.centers.empty(),"incomplete/merged outline was accepted or partial centers leaked");};
        auto pixels=image();yellow_outline(pixels,400,635,120,16);rejected(pixels);
        pixels=image();yellow_outline(pixels,400,200,120,16);rectangle(pixels,400,205,2,4,{0,0,0});rejected(pixels);
        pixels=image();yellow_outline(pixels,400,200,120,16);rectangle(pixels,459,202,2,12,{250,235,60});rejected(pixels);
        pixels=image();yellow_outline(pixels,400,200,60,16);yellow_outline(pixels,459,200,60,16);rejected(pixels);
        pixels=image();yellow_outline(pixels,400,200,120,7);yellow_outline(pixels,400,206,120,7);rejected(pixels);
        const auto stacked=detect_outlined_horizontal_drag_opening_centers(1280,720,3840,pixels);
        check(stacked.reason.find("outlined_drag_central_band_count=3;bands=0..1,5..7,11..12")==0&&
            stacked.reason.size()<256&&stacked.metrics.size()<=8,"rejected stacked contour lacks bounded original band diagnostics");
        pixels=image();rectangle(pixels,400,200,162,13,{250,235,60});rectangle(pixels,404,204,154,5,{0,0,0});
        rectangle(pixels,480,204,1,5,{250,235,60});rejected(pixels);
        pixels=image();for(int i=0;i<9;++i)yellow_outline(pixels,100+i*125,200,80,16);rejected(pixels);
        pixels=image();yellow_outline(pixels,400,200,120,16);rectangle(pixels,800,300,80,30,{250,235,60});rejected(pixels);
        pixels=image();rectangle(pixels,600,200,120,16,{255,255,255});rejected(pixels);
    }
    {++groups;auto c=chart();auto& notes=c.lines[0].notes;notes[0].position_x=-2;auto second=notes[0];second.position_x=2;second.source_index=1;
        second.source.json_pointer="/judgeLineList/0/notesAbove/1";second.source.id=c.source.chart_sha256+'#'+second.source.json_pointer;
        notes.push_back(second);c.note_count=2;AdaptiveFirstNoteTrigger trigger(c,-40*ms,-1,true);
        const auto observe=[&](unsigned seq,Ns song){auto f=frame(seq,song);auto pixels=image();
            const int y=static_cast<int>(std::lround(f.centers[0].y));yellow_outline(pixels,564,y-7,121,15);yellow_outline(pixels,852,y-7,121,15);
            const auto d=detect_model_opening_centers(1280,720,3840,pixels,trigger.description());check(d.valid&&d.centers.size()==2,"synthetic complete outline pair not detected");
            f.centers=d.centers;return trigger.observe(f,d.center_families,true,f.received_host_ns+ms);};
        check(!observe(1,1570*ms).anchor,"one outline frame armed input");const auto acquired=observe(2,1670*ms);
        check(acquired.state==FirstNoteTriggerState::ready&&acquired.anchor&&acquired.anchor->supporting_evidence_ids.size()==2&&
            acquired.anchor->target_host_ns-(O+1670*ms+ms)>=200*ms,"outline path bypassed two actual frames/motion/lead or failed valid synthetic pair");
    }
    {++groups;auto c=mixed_earliest_chart();AdaptiveFirstNoteTrigger mixed(c,-40*ms);const auto& d=mixed.description();
        check(d.includes_mixed_earliest_heads&&d.model_id=="native-piecewise-2d-opening-v9"&&d.detector_policy_id=="typed-family-mixed-earliest-heads-v1"&&
            d.required_center_families==std::vector<int>{1,4}&&d.modeled_notes==2&&d.earliest_note_count==2&&c.note_count==2,
            "mixed earliest sources/families were omitted or not explicitly versioned");
        const auto pair=[&](unsigned seq,Ns song){auto f=frame(seq,song);const auto y=f.centers[0].y;
            f.centers={{"opaque-center-A",480,y},{"opaque-center-B",1056,y}};return f;};
        const auto a=pair(1,1570*ms),b=pair(2,1670*ms);const std::vector<int> families{4,1};
        check(mixed.observe(a,families,true,a.received_host_ns+ms).support_count==1,"single mixed frame armed");
        const auto acquired=mixed.observe(b,families,true,b.received_host_ns+ms);
        check(acquired.state==FirstNoteTriggerState::ready&&acquired.anchor&&acquired.raw_candidates.size()==1&&acquired.first_note_positive_candidates==1&&
            acquired.raw_candidates[0].matches.size()==2&&acquired.anchor->target_host_ns-b.received_host_ns-ms>=200*ms,
            "typed Flick+Tap complete earliest pair failed original temporal/motion/lead guards");
        for(int missing=0;missing<2;++missing){AdaptiveFirstNoteTrigger incomplete(c,-40*ms);auto one=a,two=b;
            one.centers.erase(one.centers.begin()+missing);two.centers.erase(two.centers.begin()+missing);const std::vector<int> only{missing==0?1:4};
            incomplete.observe(one,only,true,one.received_host_ns+ms);const auto status=incomplete.observe(two,only,true,two.received_host_ns+ms);
            check(!status.anchor&&status.state!=FirstNoteTriggerState::ready&&status.first_note_positive_candidates==0,
                "missing earliest color was silently dropped or counted as observed");}
        AdaptiveFirstNoteTrigger swapped(c,0);swapped.observe(a,std::vector<int>{1,4},true,a.received_host_ns+ms);
        check(!swapped.observe(b,std::vector<int>{1,4},true,b.received_host_ns+ms).anchor,"wrong typed family matched another color's source");
        for(const auto bad:{std::vector<int>{4},std::vector<int>{4,2},std::vector<int>{4,0},std::vector<int>{4,1,1}}){
            AdaptiveFirstNoteTrigger invalid(c,0);check(invalid.observe(a,bad,true,a.received_host_ns+ms).state==FirstNoteTriggerState::cancelled,
                "mixed typed-family length or unsupported family accepted");}
        AdaptiveFirstNoteTrigger untyped(c,0);check(feed(untyped,a).state==FirstNoteTriggerState::cancelled,"mixed model guessed center types from arbitrary IDs");}
    {++groups;auto c=mixed_earliest_chart();for(auto& line:c.lines)line.notes[0].position_x=0;
        AdaptiveFirstNoteTrigger coincident(c,0);auto f=frame(1,1570*ms);f.centers.push_back({"another-typed-center",f.centers[0].x,f.centers[0].y});
        const auto rejected=coincident.observe(f,std::vector<int>{4,1},true,f.received_host_ns+ms);
        check(rejected.state==FirstNoteTriggerState::cancelled&&!rejected.anchor&&rejected.reason=="mixed_cross_family_head_ownership_ambiguous",
            "direct mixed API bypassed the canonical cross-family ownership separation");}
    {++groups;auto c=mixed_earliest_chart();c.lines[1].notes[0].kind=NoteKind::hold;c.lines[1].notes[0].speed=16.8;
        AdaptiveFirstNoteTrigger mixed_hold(c,-40*ms);const auto& d=mixed_hold.description();
        check(d.model_id=="native-piecewise-2d-opening-v10"&&d.detector_policy_id=="typed-family-mixed-earliest-oriented-hold-heads-v2"&&
            d.includes_hold_heads&&d.includes_mixed_earliest_heads&&d.required_center_families==std::vector<int>{1,4}&&d.earliest_note_count==2,
            "mixed Hold detector upgrade dropped a required family/earliest source or retained v9 cache identity");
        const auto pair=[&](unsigned seq,Ns song){auto f=frame(seq,song);const auto y=f.centers[0].y;
            f.centers={{"true-red",480,y},{"true-oriented-hold",1056,y}};return f;};
        auto a=pair(1,1570*ms),b=pair(2,1670*ms);const std::vector<int> families{4,1};
        mixed_hold.observe(a,families,true,a.received_host_ns+ms);
        const auto ready=mixed_hold.observe(b,families,true,b.received_host_ns+ms);
        check(ready.state==FirstNoteTriggerState::ready&&ready.anchor&&ready.raw_candidates[0].matches.size()==2,
            "mixed v10 no longer requires/retains the complete typed earliest group");
        AdaptiveFirstNoteTrigger missing(c,0);a.centers.pop_back();b.centers.pop_back();
        missing.observe(a,std::vector<int>{4},true,a.received_host_ns+ms);
        check(!missing.observe(b,std::vector<int>{4},true,b.received_host_ns+ms).anchor,
            "oriented Hold version upgrade invented an unobserved mixed-family head");}
    {++groups;auto c=mixed_earliest_chart();
        for(auto& line:c.lines){auto& note=line.notes[0];note.start_ns=note.end_ns=12*S;note.position_x=0;note.floor_position=0;
            line.speed_events[0].value=0;line.move_events.resize(1);auto& move=line.move_events[0];move.time.start_ns=0;move.time.end_ns=20*S;
            move.start_x=move.end_x=.8;move.start_y=move.end_y=.8;}
        auto& moving=c.lines[0].move_events;moving[0].time.end_ns=6*S;moving[0].start_x=moving[0].end_x=.25;
        moving[0].start_y=moving[0].end_y=.5;auto motion=moving[0];motion.time.start_ns=6*S;motion.time.end_ns=9500*ms;motion.end_x=.75;
        moving.push_back(motion);motion.time.start_ns=9500*ms;motion.time.end_ns=20*S;motion.start_x=motion.end_x=.75;moving.push_back(motion);
        AdaptiveFirstNoteTrigger trigger(c,-40*ms,12*S-8*ms);PhigrosPoseEvaluator pose(c);
        const auto actual=[&](unsigned seq,Ns song){auto f=frame(seq,song);f.centers.clear();
            for(std::size_t line=0;line<2;++line){const auto target=pose.note_target(line,c.lines[line].notes[0],song).reference_pixels;
                f.centers.push_back({line==0?"source-flick":"source-tap",target.x,target.y});}return f;};
        const std::vector<int> families{4,1};const auto early_a=actual(1,6*S),early_b=actual(2,6040*ms);
        trigger.observe(early_a,families,true,early_a.received_host_ns+ms);
        const auto early=trigger.observe(early_b,families,true,early_b.received_host_ns+ms);
        check(early.state==FirstNoteTriggerState::ready&&early.anchor&&early.anchor->target_host_ns-early_b.received_host_ns-ms>3*S,
            "early mixed proof did not exercise the consumer's three-second defer window");
        // The consumer deliberately does NOT take this early proof. A later
        // arm must not revive or retimestamp it merely because the due is near.
        check(!trigger.take_anchor(early_b.received_host_ns+100*ms)&&trigger.status(early_b.received_host_ns+100*ms).state==FirstNoteTriggerState::acquiring,
            "deferred early anchor survived its original100ms freshness limit");
        const auto fresh_a=actual(3,9*S),fresh_b=actual(4,9040*ms);
        check(trigger.observe(fresh_a,families,true,fresh_a.received_host_ns+ms).support_count==1,"expired early support was reused as the first new frame");
        const auto current=trigger.observe(fresh_b,families,true,fresh_b.received_host_ns+ms);
        check(current.state==FirstNoteTriggerState::ready&&current.anchor&&current.anchor->target_host_ns-fresh_b.received_host_ns-ms<=3*S&&
            current.anchor->target_host_ns-fresh_b.received_host_ns-ms>=200*ms,"two fresh mixed frames could not reacquire within the unchanged dispatch window");
        const auto taken=trigger.take_anchor(fresh_b.received_host_ns+ms);
        check(taken&&taken->received_host_ns==fresh_b.received_host_ns&&taken->evidence_id=="evidence-4"&&
            taken->supporting_evidence_ids==std::vector<std::string>{"evidence-3","evidence-4"}&&taken->received_host_ns!=early.anchor->received_host_ns&&
            !trigger.take_anchor(fresh_b.received_host_ns+2*ms),"reacquisition consumed an old timestamp/ID or fired more than once");}
    {++groups;AdaptiveFirstNoteTrigger mixed(mixed_earliest_chart(),0);auto pixels=image();
        rectangle(pixels,984,142,80,4,{0,230,255});rectangle(pixels,264,358,80,4,{245,80,220});
        const auto detected=detect_model_opening_centers(1280,720,3840,pixels,mixed.description());
        check(detected.valid&&detected.centers.size()==2&&detected.center_families==std::vector<int>{1,4}&&!detected.centers_complete_unoccluded,
            "same actual frame did not retain both independently detected typed families");
        pixels=image();rectangle(pixels,264,358,80,4,{245,80,220});const auto only_red=detect_model_opening_centers(1280,720,3840,pixels,mixed.description());
        check(only_red.valid&&only_red.centers.size()==1&&only_red.center_families==std::vector<int>{4},"missing cyan family was fabricated or existing red center dropped");
        pixels=image();rectangle(pixels,600,200,80,2,{0,230,255});rectangle(pixels,600,202,80,2,{245,80,220});
        const auto ambiguous=detect_model_opening_centers(1280,720,3840,pixels,mixed.description());
        check(!ambiguous.valid&&ambiguous.centers.empty()&&ambiguous.center_families.empty()&&
            ambiguous.reason=="mixed_cross_family_head_ownership_ambiguous","near-overlapping typed detectors had ambiguous center ownership");
        auto c=mixed_earliest_chart();c.lines[0].notes[0].kind=NoteKind::drag;AdaptiveFirstNoteTrigger cyan_yellow(c,0);
        pixels=image();rectangle(pixels,300,600,80,4,{0,230,255});rectangle(pixels,600,600,80,4,{250,235,60});
        const auto roi=detect_model_opening_centers(1280,720,3840,pixels,cyan_yellow.description());
        check(roi.valid&&roi.centers.size()==1&&roi.center_families==std::vector<int>{2},"Drag650 ROI leaked into mixed cyan570 detector");}
    {++groups;for(const auto kind:{NoteKind::tap,NoteKind::drag,NoteKind::hold,NoteKind::flick}){
        auto c=chart();c.lines[0].notes[0].kind=kind;AdaptiveFirstNoteTrigger unchanged(c,0);const int family=kind==NoteKind::drag?2:kind==NoteKind::flick?4:1;
        check(!unchanged.description().includes_mixed_earliest_heads&&unchanged.description().required_center_families==std::vector<int>{family},
            "single-color family unnecessarily adopted mixed policy");
        auto one=frame(1,1570*ms),two=frame(2,1670*ms);unchanged.observe(one,std::vector<int>{family},true,one.received_host_ns+ms);
        check(unchanged.observe(two,std::vector<int>{family},true,two.received_host_ns+ms).state==FirstNoteTriggerState::ready,
            "explicit family array changed legacy single-family solver behavior");}}
    {++groups;auto pixels=image();rectangle(pixels,600,600,80,4,{250,235,60});
        const auto extended=detect_extended_drag_opening_centers(1280,720,3840,pixels);
        check(extended.valid&&extended.centers.size()==1&&extended.centers[0].x==639.5&&extended.centers[0].y==601.5,
            "complete Drag head in extended ROI was not recovered");
        check(!extended.centers_complete_unoccluded&&!detect_adaptive_opening_centers(1280,720,3840,pixels,NoteKind::drag).valid,
            "extended Drag ROI changed original detector or inferred coverage");
        for(const auto angle:{0.0,73.0,170.0}){
            pixels=image();oriented_bar(pixels,320,590,100,4,angle,{250,235,60});const auto found=detect_extended_drag_opening_centers(1280,720,3840,pixels);
            check(found.valid&&found.centers.size()==1&&std::hypot(found.centers[0].x-320,found.centers[0].y-590)<1,
                "extended rotated Drag lost original PCA shape/center rules");
        }
        pixels=image();rectangle(pixels,600,646,80,4,{250,235,60});
        check(!detect_extended_drag_opening_centers(1280,720,3840,pixels).valid,"ROI650-clipped horizontal head accepted");
        pixels=image();oriented_bar(pixels,640,645,100,4,73,{250,235,60});
        check(!detect_extended_drag_opening_centers(1280,720,3840,pixels).valid,"ROI650-clipped rotated head accepted");
        pixels=image();rectangle(pixels,600,680,80,4,{250,235,60});
        check(!detect_extended_drag_opening_centers(1280,720,3840,pixels).valid,"bottom title region leaked into the bounded Drag ROI");
        for(const auto color:{std::array<std::uint8_t,3>{255,255,255},std::array<std::uint8_t,3>{0,230,255},std::array<std::uint8_t,3>{254,67,101}}){
            pixels=image();rectangle(pixels,600,620,80,4,color);
            check(!detect_extended_drag_opening_centers(1280,720,3840,pixels).valid,"white HUD/cyan/red glyph accepted as Drag");
        }
        pixels=image();for(int n=0;n<7;++n)rectangle(pixels,580+n*18,620,8,12,{250,235,60});
        check(!detect_extended_drag_opening_centers(1280,720,3840,pixels).valid,"small yellow text glyphs accepted as a complete Drag bar");
        check(!detect_extended_drag_opening_centers(640,720,3840,pixels).valid&&
            !detect_extended_drag_opening_centers(1280,720,3839,pixels).valid&&
            !detect_adaptive_opening_centers_roi(1280,720,3840,pixels,NoteKind::drag,651).valid&&
            !detect_adaptive_opening_centers_roi(1280,720,3840,pixels,NoteKind::tap,650).valid,
            "extended ROI accepted unbounded geometry or another typed detector policy");}
    {++groups;auto c=chart();auto& note=c.lines[0].notes[0];note.side=NoteSide::below;
        note.source.json_pointer="/judgeLineList/0/notesBelow/0";note.source.id=c.source.chart_sha256+'#'+note.source.json_pointer;
        AdaptiveFirstNoteTrigger extended(c,-40*ms);
        const auto below=[&](unsigned sequence,Ns song){auto f=frame(sequence,song);f.centers[0].y=360-(1.5*static_cast<double>(song)/S-3)*(720/(2*.83175));return f;};
        const auto first=below(1,1600*ms),second=below(2,1640*ms);
        check(first.centers[0].y>570&&first.centers[0].y<650&&second.centers[0].y>570,
            "positive-domain regression no longer exercises the added ROI");
        feed(extended,first);const auto acquired=feed(extended,second);
        check(acquired.state==FirstNoteTriggerState::ready&&acquired.anchor&&acquired.anchor->target_host_ns-second.received_host_ns-ms>=200*ms&&
            acquired.raw_candidate_count==1&&acquired.first_note_positive_candidates==1,"extended Drag positive domain could not retain the unchanged200ms lead");
        c.lines[0].notes[0].kind=NoteKind::tap;AdaptiveFirstNoteTrigger legacy(c,0);
        check(!legacy.description().includes_extended_drag_heads&&legacy.description().model_id=="native-piecewise-2d-opening-v3"&&
            feed(legacy,first).state==FirstNoteTriggerState::cancelled,"Tap v3 inherited first-Drag extended positive coordinates");
        c.lines[0].notes[0].kind=NoteKind::drag;AdaptiveFirstNoteTrigger clipped(c,0);auto invalid=first;invalid.centers[0].y=650;
        check(feed(clipped,invalid).state==FirstNoteTriggerState::cancelled,"out-of-ROI Drag center was admitted");}
    {++groups;AdaptiveFirstNoteTrigger t(chart(),-80*ms);const auto s=ready(t);
        check(s.state==FirstNoteTriggerState::ready&&s.anchor&&s.support_count==2,"native Drag pair failed");
        const auto& gate=t.dispatch_gate();
        check(gate&&gate->allowed&&gate->target_host_ns==s.anchor->target_host_ns&&
              gate->received_host_ns==s.anchor->received_host_ns&&gate->evaluated_host_ns==O+1671*ms&&
              gate->target_host_ns-gate->evaluated_host_ns>=gate->minimum_lead_ns,
              "accepted dispatch gate did not preserve the actual checked lead");
        check(s.anchor->target_host_ns==O+2*S-80*ms&&std::abs(s.anchor->origin_host_ns-O)<10,"native chart time or phase wrong");
        check(s.raw_candidate_count==1&&s.first_note_positive_candidates==1&&!s.timing_qualified,"native branch/qualification wrong");
        check(t.description().note_time_ns==2*S&&t.description().note_kind==NoteKind::drag&&t.description().modeled_notes==1,"description wrong");
        check(sha256_text(t.description().model_sha256)&&t.description().native_pieces==3,"native model identity/pieces wrong");
        check(!t.description().includes_hold_heads&&!t.description().includes_classic_flick_heads&&t.description().includes_extended_drag_heads&&t.description().model_id=="native-piecewise-2d-opening-v8"&&
            t.description().model_sha256=="baa3fc12f2c4572135a186f27ce5b3688540ddcc6ccf693c8c2e88c6db3da741"&&t.description().model_sha256!="73e97922c7fa7d5e999fec1f5f9b670dd2df9065f7488fc2ff0a00efb308b428",
            "first Drag v8 ROI canonical fingerprint missing or stale v3 accepted");
        AdaptiveFirstNoteTrigger different_phase(chart(),80*ms);
        check(different_phase.description().model_sha256==t.description().model_sha256,"execution phase polluted immutable opening identity");
        check(s.anchor->received_host_ns==O+1670*ms&&s.anchor->supporting_evidence_ids==std::vector<std::string>{"evidence-1","evidence-2"},"actual supports replaced");
        check(t.take_anchor(O+1672*ms).has_value()&&!t.take_anchor(O+1673*ms),"native trigger not one-shot");
        check(t.status(O+10*S,false).state==FirstNoteTriggerState::fired,"fired trigger expired as lease");}
    {++groups;auto c=native_v1_chart();const auto& line=c.lines[0];
        check(c.source_format_version==1&&line.notes[0].start_ns==2400*ms&&line.notes[0].floor_position==2.88,"native v1 source/time changed");
        check(line.speed_events[0].time.start_tick==-10666&&line.speed_events[0].time.end_ns==0,"native v1 sentinel changed");
        check(!line.speed_events[0].floor_position&&!line.speed_events[1].floor_position,"native v1 importer invented speed floors");
        const auto expected=[&](unsigned seq,Ns song){
            auto f=frame(seq,song,640+line.notes[0].position_x*72,360+(1.2*static_cast<double>(song)/S-2.88)*(720/(2*.83175)));
            f.centers[0].id="cyan-0";return f;
        };
        AdaptiveFirstNoteTrigger t(c,0);feed(t,expected(1,1950*ms));const auto status=feed(t,expected(2,2050*ms));
        check(status.state==FirstNoteTriggerState::ready&&status.anchor,"native v1 sentinel/missing-floor opening failed");
        check(status.anchor->target_host_ns==O+2400*ms&&std::abs(status.anchor->origin_host_ns-O)<10,"native v1 integrated floor used sentinel duration");
        check(!line.speed_events[0].floor_position&&!line.speed_events[1].floor_position&&line.native_event_declarations&&
            !line.native_event_declarations->speed_events[0].floor_position&&!line.native_event_declarations->speed_events[1].floor_position,
            "opening model altered missing raw speed floors");
        AdaptiveFirstNoteTrigger stale(c,0);const auto old=expected(1,1950*ms);
        check(stale.observe(old,true,old.received_host_ns+100*ms).state==FirstNoteTriggerState::cancelled,"native v1 weakened observation freshness");
        auto bad=c;bad.source_format_version=3;rejects([&]{AdaptiveFirstNoteTrigger rejected(bad,0);},"negative sentinel allowed without explicit native v1 source");
        bad=c;bad.lines[0].speed_events[1].time.start_ns=1;rejects([&]{AdaptiveFirstNoteTrigger rejected(bad,0);},"native v1 zero coverage gap accepted");
        bad=c;bad.lines[0].speed_events[0].time.end_ns=-1;rejects([&]{AdaptiveFirstNoteTrigger rejected(bad,0);},"native v1 sentinel gap accepted");
        bad=c;bad.lines[0].speed_events[0].time.end_ns=1;rejects([&]{AdaptiveFirstNoteTrigger rejected(bad,0);},"native v1 sentinel overlap accepted");
        bad=c;bad.lines[0].speed_events[0].value=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{AdaptiveFirstNoteTrigger rejected(bad,0);},"native v1 nonfinite sentinel ignored");
        bad=c;bad.lines[0].speed_events[0].time.start_ns=1;rejects([&]{AdaptiveFirstNoteTrigger rejected(bad,0);},"native v1 reversed sentinel accepted");
        bad=c;bad.lines[0].speed_events[0].floor_position=0;
        rejects([&]{AdaptiveFirstNoteTrigger rejected(bad,0);},"missing-floor v1 rule applied to declared sentinel floor");
        bad=c;bad.lines[0].speed_events.erase(bad.lines[0].speed_events.begin());AdaptiveFirstNoteTrigger without_sentinel(bad,0);
        bad.source_format_version=3;AdaptiveFirstNoteTrigger v3(bad,0);
        check(v3.description().model_sha256!=without_sentinel.description().model_sha256,"native v1 floor policy omitted from opening identity");
        bad=c;auto& events=bad.lines[0].speed_events;events[0].time.end_ns=200*ms;events[1].time.start_ns=200*ms;
        bad.lines[0].notes[0].floor_position=2.84;AdaptiveFirstNoteTrigger crossing_zero(bad,0);
        const auto crossing_frame=[&](unsigned seq,Ns song){auto f=expected(seq,song);f.centers[0].y=360+(.2+1.2*(static_cast<double>(song)/S-.2)-2.84)*(720/(2*.83175));return f;};
        feed(crossing_zero,crossing_frame(1,1950*ms));
        check(feed(crossing_zero,crossing_frame(2,2050*ms)).state==FirstNoteTriggerState::ready,"native v1 crossing-zero speed not clipped at tick zero");
        bad=c;bad.offset_seconds=.01;for(auto& n:bad.lines[0].notes){n.start_ns+=10*ms;n.end_ns+=10*ms;}
        const auto shift=[](auto& events){for(auto& e:events){e.time.start_ns+=10*ms;e.time.end_ns+=10*ms;}};
        shift(bad.lines[0].speed_events);shift(bad.lines[0].move_events);shift(bad.lines[0].rotate_events);shift(bad.lines[0].opacity_events);
        AdaptiveFirstNoteTrigger offset(bad,0);auto one=expected(1,1950*ms);one.received_host_ns+=10*ms;feed(offset,one);
        auto two=expected(2,2050*ms);two.received_host_ns+=10*ms;const auto shifted=feed(offset,two);
        check(shifted.state==FirstNoteTriggerState::ready&&shifted.anchor&&shifted.anchor->target_host_ns==O+2410*ms,
            "native v1 floor origin omitted or doubled chart offset");}
    {++groups;auto c=chart();c.lines[0].notes[0].floor_position=2.5714285;c.lines[0].notes[0].start_ns=c.lines[0].notes[0].end_ns=1'714'285'714;
        AdaptiveFirstNoteTrigger t(c,-80*ms);const auto expected=[&](unsigned seq,Ns time) {
            auto f=frame(seq,time);f.centers[0].y=360+(1.5*static_cast<double>(time)/S-2.5714285)*(720/(2*.83175));return f;
        };
        feed(t,expected(1,1280*ms));auto s=feed(t,expected(2,1370*ms));
        check(s.state==FirstNoteTriggerState::ready&&s.anchor,"moving-x early Drag rejected");
        check(s.anchor->target_host_ns-(O+1371*ms)>200*ms,"native early lead missing");
        AdaptiveFirstNoteTrigger wrong(c,-80*ms);feed(wrong,expected(1,1280*ms));auto f=expected(2,1370*ms);f.centers[0].x=640;
        check(feed(wrong,f).state==FirstNoteTriggerState::acquiring,"fixed x accepted while line moves");}
    {++groups;AdaptiveFirstNoteTrigger t(chart(),0);const auto model=t.description().model_sha256;
        constexpr Ns native_advance=16'727'457,host_advance=31'198'700;
        auto one=frame(1,1570*ms),two=frame(2,1570*ms+native_advance);
        two.received_host_ns=one.received_host_ns+host_advance;
        check(std::hypot(two.centers[0].x-one.centers[0].x,two.centers[0].y-one.centers[0].y)>6,
            "inconsistent-origin fixture does not meet the unchanged motion guard");
        check(feed(t,one).support_count==1,"single inconsistent-origin fixture frame fired");
        const auto rejected=feed(t,two);
        check(rejected.state==FirstNoteTriggerState::acquiring&&!rejected.anchor&&rejected.support_count==1&&
            rejected.raw_candidates.size()==1&&rejected.first_note_positive_candidates==1&&rejected.selected_intervals.empty(),
            "native16.727ms advance with host31.1987ms invented a shared origin or cancelled acquisition");
        auto three=frame(3,1570*ms+native_advance+100*ms);three.received_host_ns=two.received_host_ns+100*ms;
        const auto accepted=feed(t,three);
        check(accepted.state==FirstNoteTriggerState::ready&&accepted.anchor&&accepted.support_count==2&&
            accepted.anchor->supporting_evidence_ids==std::vector<std::string>{two.evidence_id,three.evidence_id},
            "next consistent actual pair did not recover using the newest two distinct frames");
        check(t.description().model_sha256==model,"origin gate changed immutable native model identity");}
    {++groups;AdaptiveFirstNoteTrigger t(chart(),0);auto one=frame(1,1570*ms),noisy=frame(2,1580*ms),
            another=frame(3,1590*ms),current=frame(4,1700*ms);
        noisy.received_host_ns=one.received_host_ns+50*ms;another.received_host_ns=one.received_host_ns+80*ms;
        feed(t,one);check(!feed(t,noisy).anchor&&!feed(t,another).anchor,"inconsistent intermediate origin authorized input");
        const auto accepted=feed(t,current);
        check(accepted.state==FirstNoteTriggerState::ready&&accepted.anchor&&
            accepted.anchor->supporting_evidence_ids==std::vector<std::string>{one.evidence_id,current.evidence_id}&&
            accepted.anchor->received_host_ns==current.received_host_ns&&accepted.anchor->target_host_ns-current.received_host_ns-ms>=200*ms,
            "eligible older actual support was overwritten by incompatible intermediate receipts");}
    {++groups;AdaptiveFirstNoteTrigger fast(chart(),0);feed(fast,frame(1,1570*ms));
        check(!feed(fast,frame(2,1580*ms)).anchor,"sub20ms pair bypassed the unchanged distinct-frame spacing");
        const auto accepted=feed(fast,frame(3,1590*ms));
        check(accepted.state==FirstNoteTriggerState::ready&&accepted.anchor&&
            accepted.anchor->supporting_evidence_ids==std::vector<std::string>{"evidence-1","evidence-3"},
            "high-rate intermediate receipt erased the eligible20ms actual pair");
        AdaptiveFirstNoteTrigger expired(chart(),0);feed(expired,frame(1,1570*ms));
        const auto alone=feed(expired,frame(2,1771*ms));
        check(alone.state==FirstNoteTriggerState::acquiring&&!alone.anchor&&alone.support_count==1,
            "historical support older than200ms authorized input");}
    {++groups;AdaptiveFirstNoteTrigger chain(chart(),0);auto one=frame(1,1570*ms),two=frame(2,1670*ms),three=frame(3,1730*ms);
        one.received_host_ns+=4*ms;two.received_host_ns-=4*ms;feed(chain,one);feed(chain,two);
        const auto rejected=feed(chain,three);
        check(rejected.state==FirstNoteTriggerState::acquiring&&!rejected.anchor&&rejected.selected_intervals.size()==1&&
            rejected.reason=="native_group_interval_union_not_supported_by_one_actual_pair",
            "overlapping historical interval chain was selected as though one real pair supported its full union");}
    {++groups;auto slow=chart();slow.lines[0].speed_events[0].value=.5;slow.lines[0].notes[0].floor_position=1;
        AdaptiveFirstNoteTrigger t(slow,0);const auto actual=[](unsigned seq,Ns song){auto f=frame(seq,song);
            f.centers[0].y=360+(.5*static_cast<double>(song)/S-1)*(720/(2*.83175));return f;};
        const auto one=actual(1,1570*ms),two=actual(2,1650*ms),three=actual(3,1670*ms);
        check(std::hypot(three.centers[0].x-two.centers[0].x,three.centers[0].y-two.centers[0].y)<6,
            "slow fixture lacks a compatible but nonmoving recent pair");
        feed(t,one);feed(t,two);const auto accepted=feed(t,three);
        check(accepted.state==FirstNoteTriggerState::ready&&accepted.anchor&&
            accepted.anchor->supporting_evidence_ids==std::vector<std::string>{one.evidence_id,three.evidence_id},
            "nonmoving recent pair suppressed the unchanged-motion older actual pair");
        AdaptiveFirstNoteTrigger bounded(chart(),0);for(unsigned i=0;i<16;++i)feed(bounded,frame(i+1,1570*ms+i*ms));
        const auto full=feed(bounded,frame(17,1586*ms));
        check(full.state==FirstNoteTriggerState::cancelled&&full.reason=="adaptive_opening_support_history_budget"&&!full.anchor,
            "history silently evicted an unexpired potentially conflicting support");}
    {++groups;AdaptiveFirstNoteTrigger t(chart(),0);auto one=frame(1,1570*ms),two=frame(2,1670*ms);
        two.received_host_ns+=200'000;const auto first=feed(t,one);const auto second=feed(t,two);
        check(first.raw_candidates.size()==1&&second.raw_candidates.size()==1&&second.anchor&&
            second.state==FirstNoteTriggerState::ready,"consistent small origin uncertainty was rejected");
        const auto dt=two.received_host_ns-one.received_host_ns;
        const auto p=first.raw_candidates[0].song_time,c=second.raw_candidates[0].song_time;
        check(second.anchor->chosen_song_interval.lower==std::max(c.lower,p.lower+dt)&&
            second.anchor->chosen_song_interval.upper==std::min(c.upper,p.upper+dt),
            "anchor chose a single-frame interval instead of the joint current rate-one interval");
        check(second.anchor->received_host_ns==two.received_host_ns&&second.anchor->target_host_ns-two.received_host_ns-ms>=200*ms,
            "joint origin gate retimed receipt or bypassed the original dispatch lead");}
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
    {++groups;auto c=chart();c.lines[0].notes[0].kind=NoteKind::hold;
        AdaptiveFirstNoteTrigger unit_speed(c,0);
        check(unit_speed.description().includes_hold_heads&&unit_speed.description().model_id=="native-piecewise-2d-opening-v10"&&
            unit_speed.description().detector_policy_id=="typed-rle-oriented-classic-hold-head-v3"&&
            sha256_text(unit_speed.description().model_sha256)&&
            unit_speed.description().model_sha256!="3c02e347c5530e3f0a21fc1fabb45c343ce39f4ec06fbdb1a5e2bbb9ad057e7a",
            "unit-speed Hold retained the old horizontal-only detector/cache identity");
        for(const auto raw_speed:{2.2,0.0,-2.2}){
            c.lines[0].notes[0].speed=raw_speed;AdaptiveFirstNoteTrigger corrected(c,0);
            check(corrected.description().model_id=="native-piecewise-2d-opening-v10"&&
                corrected.description().model_sha256==unit_speed.description().model_sha256,
                "nonunit Hold head did not get explicit corrected canonical identity");
            check(corrected.description().model_sha256!="73e97922c7fa7d5e999fec1f5f9b670dd2df9065f7488fc2ff0a00efb308b428"&&
                corrected.description().model_sha256==unit_speed.description().model_sha256,
                "old incorrect Hold opening hash was accepted");
            check(ready(corrected).state==FirstNoteTriggerState::ready,
                "Hold head reused positive/zero/negative raw note multiplier");
            check(c.lines[0].notes[0].speed==raw_speed&&c.lines[0].notes[0].floor_position==3&&
                c.lines[0].notes[0].start_ns==2*S&&c.lines[0].speed_events[0].value==1.5,
                "opening correction changed immutable native note/line values");
        }
        for(const auto kind:{NoteKind::tap,NoteKind::drag,NoteKind::flick}){
            c=chart();c.lines[0].notes[0].kind=kind;c.lines[0].notes[0].speed=2.2;
            AdaptiveFirstNoteTrigger raw(c,0);
            for(unsigned seq=1;seq<=2;++seq){
                const auto t=(seq==1?1770:1790)*ms;auto f=frame(seq,t);
                f.centers[0].y=360+(1.5*static_cast<double>(t)/S-3)*2.2*(720/(2*.83175));
                const auto s=feed(raw,f);
                check(std::any_of(s.raw_candidates.begin(),s.raw_candidates.end(),[&](const auto& candidate){
                    return candidate.song_time.lower<=t&&t<=candidate.song_time.upper;
                }),"non-Hold raw-speed geometry changed");
            }
            check(!raw.description().includes_hold_heads&&raw.description().includes_classic_flick_heads==(kind==NoteKind::flick)&&
                raw.description().model_id==(kind==NoteKind::flick?"native-piecewise-2d-opening-v7":kind==NoteKind::drag?"native-piecewise-2d-opening-v8":"native-piecewise-2d-opening-v3"),
                "non-Hold geometry selected an incorrect conditional detector policy");
        }
        c=chart();c.lines[0].notes[0].kind=NoteKind::tap;c.lines[0].notes[0].speed=-1;
        AdaptiveFirstNoteTrigger negative_tap(c,0);
        auto one=frame(1,1700*ms),two=frame(2,1750*ms);
        one.centers[0].y=360-(1.5*1.7-3)*(720/(2*.83175));
        two.centers[0].y=360-(1.5*1.75-3)*(720/(2*.83175));
        feed(negative_tap,one);check(feed(negative_tap,two).state==FirstNoteTriggerState::ready,
            "negative non-Hold speed was normalized with Hold heads");}
    {++groups;auto c=chart();c.lines[0].notes[0].kind=NoteKind::hold;
        auto& rotate=c.lines[0].rotate_events[0];rotate.time.start_ns=0;rotate.time.end_ns=2*S;rotate.start=-20;rotate.end=20;
        PhigrosPoseEvaluator pose(c);
        for(const auto raw_speed:{2.2,0.0,-2.2}){
            c.lines[0].notes[0].speed=raw_speed;AdaptiveFirstNoteTrigger corrected(c,0);
            auto reference_chart=c;reference_chart.lines[0].notes[0].speed=1;AdaptiveFirstNoteTrigger reference(reference_chart,0);
            for(unsigned seq=1;seq<=2;++seq){
                const auto t=(seq==1?1570:1670)*ms;auto f=frame(seq,t);const auto p=pose.line_at(0,t);
                const auto z=(1.5*static_cast<double>(t)/S-3)*(720/(2*.83175));
                f.centers[0].x=p.origin.x*1280-z*p.above_normal.x;f.centers[0].y=p.origin.y*720-z*p.above_normal.y;
                const auto actual=feed(corrected,f),expected=feed(reference,f);
                check(actual.raw_candidates.size()==expected.raw_candidates.size(),"Hold curvature changed with raw speed");
                for(std::size_t i=0;i<actual.raw_candidates.size();++i)check(
                    actual.raw_candidates[i].song_time.lower==expected.raw_candidates[i].song_time.lower&&
                    actual.raw_candidates[i].song_time.upper==expected.raw_candidates[i].song_time.upper,
                    "Hold interval endpoints/derivatives retained raw multiplier");
                check(std::any_of(actual.raw_candidates.begin(),actual.raw_candidates.end(),[&](const auto& candidate){
                    return candidate.song_time.lower<=t&&t<=candidate.song_time.upper;
                }),"corrected rotating Hold truth pruned by curvature enclosure");
            }
        }}
    {++groups;auto c=chart();c.lines[0].notes[0].kind=NoteKind::tap;
        auto alias=c.lines[0].notes[0];alias.kind=NoteKind::hold;alias.speed=2.2;alias.source_index=1;
        alias.start_ns=3*S;alias.end_ns=4*S;alias.position_x=1;alias.floor_position=3.15;
        alias.source.json_pointer="/judgeLineList/0/notesAbove/1";alias.source.id=c.source.chart_sha256+'#'+alias.source.json_pointer;
        c.lines[0].notes.push_back(alias);++c.note_count;AdaptiveFirstNoteTrigger missing(c,0),complete(c,0);
        check(complete.description().includes_hold_heads&&complete.description().model_id=="native-piecewise-2d-opening-v10"&&complete.description().modeled_notes==2&&
            complete.description().note_kind==NoteKind::tap,"same-color Hold alias was not covered by v4 correction");
        auto one=frame(1,1750*ms),two=frame(2,1790*ms);
        check(feed(missing,one).raw_candidates.empty(),"corrected definitely visible Hold alias was silently omitted");
        one.centers.push_back({"cyan-hold",840,360+(1.5*1.75-3.15)*(720/(2*.83175))});
        two.centers.push_back({"cyan-hold",840,360+(1.5*1.79-3.15)*(720/(2*.83175))});
        feed(complete,one);check(feed(complete,two).state==FirstNoteTriggerState::ready,"source-complete Tap/Hold pair failed after effective-speed correction");
        c.lines[0].notes[0].kind=NoteKind::drag;AdaptiveFirstNoteTrigger other_color(c,0);
        check(!other_color.description().includes_hold_heads&&other_color.description().modeled_notes==1&&other_color.description().model_id=="native-piecewise-2d-opening-v8"&&
            other_color.description().model_sha256=="baa3fc12f2c4572135a186f27ce5b3688540ddcc6ccf693c8c2e88c6db3da741",
            "unmodeled different-color Hold changed legacy opening identity");}
    {++groups;auto c=chart();auto& line=c.lines[0];auto& hold=line.notes[0];
        // Synthetic two-line regression of the 12.324s, line-speed 2.2 case.
        // The line floor already includes 2.2; the head must not apply it twice.
        constexpr Ns judgment=12'324'324'324,cutoff=12'044'324'324;
        hold.kind=NoteKind::hold;hold.start_ns=judgment;hold.end_ns=judgment+324*ms;hold.speed=2.2;
        hold.floor_position=27.113514;hold.position_x=5.25;line.speed_events[0].value=2.2;
        line.move_events.resize(1);line.move_events[0].time.end_ns=20*S;
        line.move_events[0].start_x=line.move_events[0].end_x=.5;
        line.move_events[0].start_y=line.move_events[0].end_y=1-578.9126032409218/720;
        auto tap_line=line;tap_line.source_index=1;auto& tap=tap_line.notes[0];tap.kind=NoteKind::tap;tap.end_ns=judgment;
        tap.position_x=-5.25;tap.speed=1;tap.source.json_pointer="/judgeLineList/1/notesAbove/0";
        tap.source.id=c.source.chart_sha256+'#'+tap.source.json_pointer;
        tap_line.move_events[0].start_y=tap_line.move_events[0].end_y=1-576.0000072/720;
        tap_line.speed_events[0].source.json_pointer="/judgeLineList/1/speedEvents/0";
        c.lines.push_back(tap_line);++c.note_count;AdaptiveFirstNoteTrigger corrected(c,-80*ms);
        const auto z=(2.2*static_cast<double>(cutoff)/S-27.113514)*(720/(2*.83175));
        check(std::abs(578.9126032409218+z-312.29381707681273)<1e-6&&
            578.9126032409218+2.2*z<0,"12.324s Hold unit-speed geometry regression lost its double-multiplier distinction");
        const auto pair=[&](unsigned seq,Ns song){auto f=frame(seq,song);const auto displacement=(2.2*static_cast<double>(song)/S-27.113514)*(720/(2*.83175));
            f.centers={{"cyan-hold",1018,578.9126032409218+displacement},{"cyan-tap",262,576.0000072+displacement}};return f;};
        feed(corrected,pair(1,11'940*ms));const auto acquired=feed(corrected,pair(2,12*S));
        check(acquired.state==FirstNoteTriggerState::ready&&acquired.anchor&&
            std::abs(acquired.anchor->origin_host_ns-O)<10&&acquired.anchor->target_host_ns==acquired.anchor->origin_host_ns+judgment-80*ms,
            "12.324s two-head synthetic geometry could not acquire before unchanged dispatch lead");
        check(c.lines[0].notes[0].speed==2.2&&c.lines[1].notes[0].speed==1&&c.note_count==2,
            "Frozen-like geometry regression rewrote raw notes");}
    {++groups;auto c=chart();auto original=c.lines[0];c.lines[0].notes.clear();original.source_index=1;
        original.notes[0].source.json_pointer="/judgeLineList/1/notesAbove/0";original.notes[0].source.id=c.source.chart_sha256+'#'+original.notes[0].source.json_pointer;
        original.speed_events[0].source.json_pointer="/judgeLineList/1/speedEvents/0";c.lines.push_back(original);
        AdaptiveFirstNoteTrigger t(c,0);check(t.description().line_index==1,"line0 implicitly selected over actual first native line");
        check(ready(t).state==FirstNoteTriggerState::ready,"first note on nonzero native line not supported");
        c.note_count=2;rejects([&]{AdaptiveFirstNoteTrigger bad(c,0);},"mismatched semantic note count accepted");}
    for(const auto version:{3,1}){++groups;auto c=chart();c.source_format_version=version;c.lines[0].move_events.resize(1);auto& move=c.lines[0].move_events[0];move.time.end_ns=20*S;
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
        check(t.description().detector_policy_id=="typed-rle-oriented-drag-head-roi650-v2","detector policy missing from description");}
    {++groups;auto c=chart();c.lines[0].notes[0].kind=NoteKind::flick;AdaptiveFirstNoteTrigger flick(c,0);
        const auto& d=flick.description();
        check(d.includes_classic_flick_heads&&!d.includes_hold_heads&&d.model_id=="native-piecewise-2d-opening-v7"&&
            d.detector_policy_id=="typed-rle-oriented-classic-red-flick-head-v3",
            "earliest Flick did not bind the explicit classic red detector policy");
        check(d.model_sha256=="877e08b6e934548f98335b149976ded8a89cc91653bc1eedcd4c9e9dc41582e7"&&
            d.model_sha256!="449ccf65c602a3df3ec5bc4eb1fa79ce55ced98ccdae3a35d7e625929d417998"&&
            d.model_sha256!="5f81309f72eac357dd15f64dfb7ae00b478fab641be8eafca2033137351b232d",
            "classic Flick relative-pose policy accepted an old v6/pink model fingerprint");
        check(d.note_time_ns==2*S&&d.first_down_ns==1970*ms&&ready(flick).state==FirstNoteTriggerState::ready,
            "Flick detector version changed native geometry or first-DOWN timing");
        AdaptiveFirstNoteTrigger other_phase(c,80*ms);
        check(other_phase.description().model_sha256==d.model_sha256,"Flick immutable detector hash included execution phase");}
    {++groups;for(const auto kind:{NoteKind::tap,NoteKind::drag,NoteKind::hold}){
            auto c=chart();c.lines[0].notes[0].kind=kind;auto later=c.lines[0].notes[0];later.kind=NoteKind::flick;
            later.start_ns=later.end_ns=3*S;later.source_index=1;later.source.json_pointer="/judgeLineList/0/notesAbove/1";
            later.source.id=c.source.chart_sha256+'#'+later.source.json_pointer;c.lines[0].notes.push_back(later);++c.note_count;
            AdaptiveFirstNoteTrigger unchanged(c,0);const auto& d=unchanged.description();
            check(!d.includes_classic_flick_heads&&d.modeled_notes==1,
                "unmodeled later Flick changed a non-Flick opening policy");
            if(kind==NoteKind::hold){auto reference=chart();reference.lines[0].notes[0].kind=NoteKind::hold;AdaptiveFirstNoteTrigger hold(reference,0);
                check(d.model_sha256==hold.description().model_sha256&&d.model_id=="native-piecewise-2d-opening-v10",
                    "unmodeled Flick altered the conditional oriented Hold model");
            }else check(d.model_sha256==(kind==NoteKind::drag?"baa3fc12f2c4572135a186f27ce5b3688540ddcc6ccf693c8c2e88c6db3da741":"73e97922c7fa7d5e999fec1f5f9b670dd2df9065f7488fc2ff0a00efb308b428"),
                "conditional Drag v8 or unchanged Tap v3 canonical fingerprint changed");
        }}
    {++groups;auto c=rotating_flick_alias_chart();AdaptiveFirstNoteTrigger t(c,0);
        feed(t,rotating_flick_frame(c,1,1700*ms));const auto s=feed(t,rotating_flick_frame(c,2,1750*ms));
        check(s.state==FirstNoteTriggerState::ready&&s.anchor&&s.raw_candidates.size()==3&&s.first_note_positive_candidates==3,
            "common rotating Flick aliases with360-degree float differences blocked the distinct earliest pair");
        std::set<std::string> aliases;
        for(const auto& candidate:s.raw_candidates){check(candidate.matches.size()==3,"relative pose proof marked an occluded alias observed");
            for(const auto& match:candidate.matches)if(match.center_id=="one-ornament")aliases.insert(match.note_id);}
        check(aliases.size()==3&&c.note_count==5,"relative pose proof collapsed alternative Flick source IDs");
    }
    {++groups;auto c=rotating_flick_alias_chart(1);auto original=c.lines[2].rotate_events[0];auto first=original,second=original;
        first.time.end_ns=1450*ms;first.end=-371+230*1.45+360+.000002;
        second.time.start_ns=1450*ms;second.start=first.end;second.end=89+720+.000002;c.lines[2].rotate_events={first,second};
        AdaptiveFirstNoteTrigger extra_turn(c,0);
        check(feed(extra_turn,rotating_flick_frame(c,1,1700*ms)).raw_candidates.empty(),
            "independently wrapping two endpoints hid an extra complete turn inside the native piece");
        c=rotating_flick_alias_chart(1);c.lines[2].notes[0].position_x+=.01/72;AdaptiveFirstNoteTrigger local(c,0);
        check(feed(local,rotating_flick_frame(c,1,1700*ms)).raw_candidates.empty(),"separated local vectors received correlated occlusion");
        c=rotating_flick_alias_chart(1);c.lines[2].move_events[0].start_x+=.01/1280;c.lines[2].move_events[0].end_x+=.01/1280;
        AdaptiveFirstNoteTrigger origin(c,0);
        check(feed(origin,rotating_flick_frame(c,1,1700*ms)).raw_candidates.empty(),"separated native origins received correlated occlusion");
    }
    {++groups;auto c=chart();c.lines[0].notes[0].kind=NoteKind::flick;auto duplicate=c.lines[0].notes[0];duplicate.source_index=1;
        duplicate.source.json_pointer="/judgeLineList/0/notesAbove/1";duplicate.source.id=c.source.chart_sha256+'#'+duplicate.source.json_pointer;
        c.lines[0].notes.push_back(duplicate);++c.note_count;AdaptiveFirstNoteTrigger missing(c,0);
        feed(missing,frame(1,1570*ms));const auto s=feed(missing,frame(2,1670*ms));
        check(s.raw_candidates.size()==2&&s.first_note_positive_candidates==0&&!s.anchor&&s.state!=FirstNoteTriggerState::ready,
            "Flick occlusion fabricated the missing distinct earliest source");
        for(const auto& candidate:s.raw_candidates)check(candidate.matches.size()==1,"Flick occlusion added an unobserved earliest match");
    }
    {++groups;auto c=occlusion_chart();AdaptiveFirstNoteTrigger t(c,0);
        feed(t,occlusion_frame(1,1700*ms));const auto s=feed(t,occlusion_frame(2,1750*ms));
        check(s.state==FirstNoteTriggerState::ready&&s.anchor&&s.raw_candidates.size()==3&&s.first_note_positive_candidates==3,
            "three coincident decorative aliases blocked the distinct earliest pair");
        std::set<std::string> observed_aliases;
        for(const auto& candidate:s.raw_candidates){
            check(candidate.matches.size()==3,"occluded source was falsely marked observed");
            for(const auto& match:candidate.matches)if(match.center_id=="one-ornament")observed_aliases.insert(match.note_id);
        }
        check(observed_aliases.size()==3&&c.note_count==5&&c.lines[0].notes.size()==5,
            "coincident alternative source assignments or native notes were collapsed");}
    {++groups;auto c=occlusion_chart();c.lines[0].notes[4].position_x=2.0/72;
        AdaptiveFirstNoteTrigger separated(c,0);
        check(feed(separated,occlusion_frame(1,1700*ms)).raw_candidates.empty(),
            "two-pixel separated source incorrectly used the three-pixel visual gate as coincidence");
        c=occlusion_chart();auto& opposite=c.lines[0].notes[3];opposite.side=NoteSide::below;opposite.source_index=0;
        opposite.source.json_pointer="/judgeLineList/0/notesBelow/0";opposite.source.id=c.source.chart_sha256+'#'+opposite.source.json_pointer;
        AdaptiveFirstNoteTrigger other_side(c,0);
        check(feed(other_side,occlusion_frame(1,1700*ms)).raw_candidates.empty(),"opposite-side head was granted occlusion equivalence");
        c=occlusion_chart();c.lines[0].speed_events[0].value=0;for(auto& note:c.lines[0].notes)note.floor_position=0;
        c.lines[0].notes[3].kind=NoteKind::hold;c.lines[0].move_events.resize(1);auto& move=c.lines[0].move_events[0];
        move.time.start_ns=0;move.time.end_ns=2*S;move.start_x=move.end_x=.6;move.start_y=.4;move.end_y=.6;
        AdaptiveFirstNoteTrigger other_kind(c,0);auto f=occlusion_frame(1,1700*ms);for(auto& center:f.centers)center.y=432-72*1.7;
        check(feed(other_kind,f).raw_candidates.empty(),"different-kind coincident head was granted occlusion equivalence");}
    {++groups;const auto crossing=[](double slope){auto c=occlusion_chart();auto moving=c.lines[0];moving.source_index=1;
            auto note=c.lines[0].notes.back();c.lines[0].notes.pop_back();note.source_index=0;
            note.source.json_pointer="/judgeLineList/1/notesAbove/0";note.source.id=c.source.chart_sha256+'#'+note.source.json_pointer;moving.notes={note};
            moving.move_events.resize(1);auto& move=moving.move_events[0];move.time.start_ns=0;move.time.end_ns=2*S;
            move.start_x=(768-1.7*slope)/1280;move.end_x=(768+.3*slope)/1280;move.start_y=move.end_y=.5;
            moving.speed_events[0].source.json_pointer="/judgeLineList/1/speedEvents/0";c.lines.push_back(moving);return c;};
        auto c=crossing(80);AdaptiveFirstNoteTrigger fast(c,0);
        check(feed(fast,occlusion_frame(1,1700*ms)).raw_candidates.empty(),"single-point crossing received an all-time occlusion exemption");
        c=crossing(.005);AdaptiveFirstNoteTrigger partial(c,0);
        check(feed(partial,occlusion_frame(1,1700*ms)).raw_candidates.empty(),
            "small matching subdomain hid separation elsewhere in the complete native piece");}
    {++groups;auto c=chart();c.lines[0].notes[0].kind=NoteKind::hold;auto duplicate=c.lines[0].notes[0];
        duplicate.source_index=1;duplicate.source.json_pointer="/judgeLineList/0/notesAbove/1";duplicate.source.id=c.source.chart_sha256+'#'+duplicate.source.json_pointer;
        c.lines[0].notes.push_back(duplicate);++c.note_count;AdaptiveFirstNoteTrigger missing_earliest(c,0);
        feed(missing_earliest,frame(1,1570*ms));const auto absent=feed(missing_earliest,frame(2,1670*ms));
        check(absent.raw_candidates.size()==2&&absent.first_note_positive_candidates==0&&!absent.anchor&&absent.state!=FirstNoteTriggerState::ready,
            "occluded-only earliest source fabricated distinct earliest-group evidence");
        for(const auto& candidate:absent.raw_candidates)check(candidate.matches.size()==1,"unobserved earliest source was inserted into matches");
        c=occlusion_chart();c.lines[0].notes[3].position_x=(.75/1024)/72;c.lines[0].notes[4].position_x=(1.5/1024)/72;
        AdaptiveFirstNoteTrigger chain(c,0);const auto direct=feed(chain,occlusion_frame(1,1700*ms));
        check(direct.raw_candidates.size()==1,"coincidence was incorrectly propagated transitively through covered sources");
        check(std::any_of(direct.raw_candidates[0].matches.begin(),direct.raw_candidates[0].matches.end(),[&](const auto& match){
            return match.center_id=="one-ornament"&&match.note_id==c.lines[0].notes[3].source.id;}),
            "direct observed-middle source did not retain both independently proved occlusions");}
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
