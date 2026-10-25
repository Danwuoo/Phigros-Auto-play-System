#include "rhythm/contact_compiler.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
using namespace rhythm;
constexpr SongNs second=1000000000;
int checks=0;
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);++checks;}
void run(const char* name,const std::function<void()>& test) {test();std::cout<<"PASS "<<name<<'\n';}
const char* v2_golden(const char* padded_hex,const char* compact_hex) {
    // Archived v2 fingerprints include the standard library's hexfloat spelling.
    // MSVC emits 13 fractional digits; libstdc++ emits the shortest exact form.
    // Both goldens below were reproduced from the unchanged pre-inset baseline;
    // padding the GNU canonical transcript to 13 digits gives the MSVC hashes.
    // Keep production v2 unchanged so existing Windows archives stay valid.
    std::ostringstream probe;probe.imbue(std::locale::classic());probe<<std::hexfloat<<1.0;
    if(probe.str()=="0x1.0000000000000p+0")return padded_hex;
    if(probe.str()=="0x1p+0")return compact_hex;
    throw std::runtime_error("Unverified standard-library hexfloat spelling for archived v2 goldens");
}
SemanticChart chart() {
    SemanticChart chart;chart.source={"synthetic",std::string(64,'a'),"test-song","IN","synthetic-v3"};
    ChartLine line;line.bpm=120;
    MoveLineEvent move;move.time.end_ns=100*second;move.start_x=move.end_x=.5;move.start_y=move.end_y=.5;
    NumericLineEvent rotation;rotation.time.end_ns=100*second;
    NumericLineEvent opacity=rotation;opacity.start=opacity.end=1;
    line.move_events.push_back(move);line.rotate_events.push_back(rotation);line.opacity_events.push_back(opacity);
    chart.lines.push_back(line);return chart;
}
Identity identity(const SemanticChart& chart) {return {Game::phigros,chart.source.song_id,chart.source.difficulty,chart.source.game_revision,chart.source.chart_sha256,std::string(64,'b'),"test-geometry"};}
PhigrosGeometry geometry() {return {1280,720};}
void note(SemanticChart& chart,NoteKind kind,SongNs start=second,SongNs end=second,double x=0,NoteSide side=NoteSide::above) {
    SemanticNote note;note.kind=kind;note.start_ns=start;note.end_ns=end;note.position_x=x;note.side=side;
    note.source.json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(chart.lines[0].notes.size());
    note.source.id=chart.source.chart_sha256+'#'+note.source.json_pointer;
    chart.lines[0].notes.push_back(note);++chart.note_count;
}
bool diagnostic(const ContactCompileResult& result,const std::string& code) {
    return std::any_of(result.diagnostics.begin(),result.diagnostics.end(),[&](const auto& d){return d.code==code;});
}
const CompiledContactPlan& plan(const ContactCompileResult& result) {
    if(!result.plan) {
        for(const auto& d:result.diagnostics) std::cerr<<d.code<<": "<<d.message<<'\n';
        throw std::runtime_error("Expected compiler plan");
    }
    return *result.plan;
}
double held_error(const Contact& contact,const PhigrosPoseEvaluator& poses,std::size_t line,const SemanticNote& note,SongNs time,const PhigrosGeometryMap& map) {
    auto it=std::upper_bound(contact.steps.begin(),contact.steps.end(),time,[](SongNs t,const Step& step){return t<step.song_ns;});
    const auto& current=*std::prev(it);
    const auto wanted=map.map_normalized(poses.note_target(line,note,time).normalized).input_normalized;
    return std::hypot(current.point.x*1279-wanted.x*1280,current.point.y*719-wanted.y*720);
}
void inside_inset(const CompiledContactPlan& p) {
    for(const auto& contact:p.memory.contacts)for(const auto& step:contact.steps) {
        const auto [x,y]=pixel_point(step.point,1280,720);
        require(x>=64&&x<=1215&&y>=64&&y<=655,"generated input node entered 64px edge exclusion band");
    }
}
PosePoint projected_oracle(const PhigrosPoseEvaluator& poses,const SemanticNote& note,SongNs time,double offset,double inset) {
    const auto target=phigros_line_point(poses.line_at(0,time),note.position_x,note.side,offset);
    const double xlo=inset,xhi=inset>0?1279-inset:1280,ylo=inset,yhi=inset>0?719-inset:720;
    std::vector<double> candidates{0};
    if(target.side_normal.x!=0)for(const double edge:{xlo,xhi})candidates.push_back((edge-target.reference_pixels.x)/target.side_normal.x);
    if(target.side_normal.y!=0)for(const double edge:{ylo,yhi})candidates.push_back((edge-target.reference_pixels.y)/target.side_normal.y);
    bool found=false;double nearest=0;PosePoint best;
    for(const auto amount:candidates) {
        PosePoint point{target.reference_pixels.x+amount*target.side_normal.x,target.reference_pixels.y+amount*target.side_normal.y};
        if(point.x<xlo-1e-8||point.x>xhi+1e-8||point.y<ylo-1e-8||point.y>yhi+1e-8)continue;
        if(!found||std::abs(amount)<std::abs(nearest)) {
            found=true;nearest=amount;best={std::clamp(point.x,xlo,xhi),std::clamp(point.y,ylo,yhi)};
        }
    }
    if(!found)throw std::runtime_error("independent edge-enumeration oracle found no normal intersection");
    return best;
}
double projected_held_error(const CompiledContactPlan& plan_,const SemanticChart& chart_,const ContactCompileOptions& options_,
        SongNs begin,SongNs end,SongNs increment=337'009) {
    PhigrosPoseEvaluator poses(chart_);PhigrosGeometryMap map(plan_.metadata.geometry);
    const auto& note_=chart_.lines[0].notes[0];const auto& contact_=plan_.memory.contacts[0];double worst=0;
    for(SongNs time=begin;time<end;time+=increment) {
        const auto it=std::upper_bound(contact_.steps.begin(),contact_.steps.end(),time,[](SongNs t,const Step& s){return t<s.song_ns;});
        const auto& actual=std::prev(it)->point;
        const double offset=note_.kind==NoteKind::flick ? options_.flick_distance_reference_pixels*
            static_cast<double>(time-note_.start_ns)/static_cast<double>(options_.flick_travel_ns) : 0;
        const auto reference=projected_oracle(poses,note_,time,offset,options_.normal_projection_inset_reference_pixels);
        const auto expected=map.map_reference_pixels(reference).input_normalized;
        const auto& g=plan_.metadata.geometry;
        worst=std::max(worst,std::hypot(actual.x*(g.input_width-1)-expected.x*g.input_width,
            actual.y*(g.input_height-1)-expected.y*g.input_height));
    }
    return worst;
}
}
int main() {
    using namespace rhythm;
    try {
        run("closed source-traceable four-kind contacts",[] {
            auto c=chart();note(c,NoteKind::tap);note(c,NoteKind::drag,2*second,2*second);
            note(c,NoteKind::hold,3*second,5*second);note(c,NoteKind::flick,6*second,6*second);
            const auto result=compile_phigros_contacts(c,identity(c),geometry());const auto& p=plan(result);
            require(p.memory.contacts.size()==4,"all notes own a contact");require(p.ownership.size()==4,"ownership complete");
            require(p.peak_contacts==1,"sequential peak1");require(validate(p.memory,2)==1,"memory contract valid");
            for(std::size_t i=0;i<4;++i) {
                require(p.ownership[i].source_note_id==c.lines[0].notes[i].source.id,"source note retained");
                require(p.ownership[i].contact_id==p.memory.contacts[i].id,"pointer ownership matches");
                require(p.memory.contacts[i].steps.front().phase==Phase::down&&p.memory.contacts[i].steps.back().phase==Phase::up,"closed lifecycle");
            }
            const auto& flick=p.memory.contacts.back().steps;
            require(flick.front().song_ns<6*second&&flick.back().song_ns>6*second,"flick straddles judgment");
            require(std::any_of(flick.begin(),flick.end(),[](const auto& step){return step.song_ns==6*second&&step.phase==Phase::move;}),"flick center keytime retained");
            require(std::abs(flick.front().point.y-flick[flick.size()-2].point.y)*719>=47,"visible flick displacement");
            require(diagnostic(result,"fresh_down_candidate"),"fresh down unverified warning");
        });
        run("capacity2 5 10 and rejection rather than skipped notes",[] {
            for(int capacity:{2,5,10}) {
                auto c=chart();for(int i=0;i<capacity;++i) note(c,NoteKind::hold,second,2*second,i*.1);
                ContactCompileOptions options;options.max_contacts=capacity;
                const auto success=compile_phigros_contacts(c,identity(c),geometry(),options);
                require(plan(success).peak_contacts==capacity,"capacity parameter honored");
                note(c,NoteKind::hold,second,2*second);
                const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),options);
                require(!rejected.plan&&diagnostic(rejected,"capacity_exceeded"),"overcapacity rejects complete plan");
                require(rejected.required_peak_contacts==capacity+1,"overcapacity required count reported");
            }
        });
        run("simultaneous UP MOVE DOWN deterministic order",[] {
            auto c=chart();note(c,NoteKind::tap,second,second);
            note(c,NoteKind::flick,second+16'000'000,second+16'000'000);
            note(c,NoteKind::tap,second+16'000'000,second+16'000'000);
            ContactCompileOptions options;options.max_contacts=5;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
            int last=-1,count=0;
            for(const auto& event:p.events) if(event.song_ns==second+16'000'000) {
                require(phase_order(event.phase)>=last,"tie ordering stable");last=phase_order(event.phase);++count;
            }
            require(count==3,"one UP MOVE DOWN at boundary");
            const auto again=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(again).metadata.cache_sha256==p.metadata.cache_sha256,"deterministic cache identity");
        });
        run("moving rotating Hold bounded discrete error and same pointer",[] {
            auto c=chart();auto& move=c.lines[0].move_events[0];move.time.end_ns=10*second;move.start_x=.3;move.end_x=.7;
            auto& rotation=c.lines[0].rotate_events[0];rotation.time.end_ns=10*second;rotation.end=180;
            note(c,NoteKind::hold,second,9*second,1);
            ContactCompileOptions options;options.max_path_error_pixels=3;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
            require(p.memory.contacts.size()==1,"Hold one pointer for full lifetime");
            const auto& contact=p.memory.contacts[0];require(contact.steps.size()>10&&contact.steps.size()<2000,"motion-sensitive finite sampling");
            PhigrosPoseEvaluator poses(c);PhigrosGeometryMap map(geometry());double max_error=0;
            for(SongNs t=second;t<9*second;t+=1'000'003) max_error=std::max(max_error,held_error(contact,poses,0,c.lines[0].notes[0],t,map));
            require(max_error<=options.max_path_error_pixels+1e-8,"actual zero-order sampled error inside bound");
        });
        run("static long Hold compression critical times and motion boundaries",[] {
            auto c=chart();note(c,NoteKind::hold,second,99*second);
            const auto result=compile_phigros_contacts(c,identity(c),geometry());
            require(plan(result).memory.contacts[0].steps.size()==2,"98 second static hold only DOWN UP");
            auto& moves=c.lines[0].move_events;const auto first=moves[0];moves.clear();
            auto left=first;left.time.end_ns=2*second;moves.push_back(left);
            auto right=first;right.time.start_ns=2*second;right.start_x=right.end_x=.6;moves.push_back(right);
            const auto jump=compile_phigros_contacts(c,identity(c),geometry());const auto& steps=plan(jump).memory.contacts[0].steps;
            require(steps.size()==3&&steps[1].song_ns==2*second,"discrete jump preserves boundary exact MOVE");
        });
        run("offscreen strict rejection and explicit bounded unverified candidate",[] {
            auto c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1.3;
            note(c,NoteKind::hold,second,2*second);
            const auto reject=compile_phigros_contacts(c,identity(c),geometry());
            require(!reject.plan&&diagnostic(reject,"offscreen_target"),"native offscreen note rejects whole chart");
            require(reject.diagnostics[0].reference_point.has_value(),"offscreen coordinates reported");
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_candidate;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-normal-test";
            const auto candidate=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(candidate);
            require(p.metadata.requires_calibration,"candidate never implicitly verified");
            require(p.ownership[0].normal_projection_reference_pixels<-215,"bounded signed normal projection recorded");
            require(diagnostic(candidate,"unverified_normal_projection"),"candidate warning retained");
            options.max_normal_projection_reference_pixels=100;
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"projection over bound rejects");
            options.max_normal_projection_reference_pixels=250;options.calibration_id.clear();
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"projection requires calibration identity");
        });
        run("quarter turn stretch viewport Hold bound and exact raster roundtrip",[] {
            auto c=chart();auto& move=c.lines[0].move_events[0];move.time.end_ns=10*second;move.start_x=.3;move.end_x=.7;
            auto& rotation=c.lines[0].rotate_events[0];rotation.time.end_ns=10*second;rotation.end=90;
            note(c,NoteKind::hold,second,9*second,1);
            PhigrosGeometry g;g.input_width=900;g.input_height=1600;g.rotation=InputRotation::clockwise_90;
            g.viewport=PixelViewport{100,50,1400,800};g.fit=ViewportFit::stretch;
            ContactCompileOptions options;options.max_path_error_pixels=3;
            const auto result=compile_phigros_contacts(c,identity(c),g,options);const auto& contact=plan(result).memory.contacts[0];
            PhigrosPoseEvaluator poses(c);PhigrosGeometryMap mapping(g);double error=0;
            for(SongNs t=second;t<9*second;t+=1'000'003) {
                const auto it=std::upper_bound(contact.steps.begin(),contact.steps.end(),t,[](SongNs time,const Step& s){return time<s.song_ns;});
                const auto& step=*std::prev(it);const auto wanted=mapping.map_normalized(poses.note_target(0,c.lines[0].notes[0],t).normalized);
                error=std::max(error,std::hypot(step.point.x*899-wanted.input_normalized.x*900,step.point.y*1599-wanted.input_normalized.y*1600));
            }
            require(error<=3+1e-8,"rotated stretched zero order bound");
            for(const auto& step:contact.steps) {
                const auto pixels=pixel_point(step.point,900,1600);
                require(std::abs(step.point.x*899-pixels.first)<1e-9&&std::abs(step.point.y*1599-pixels.second)<1e-9,"normalized raster roundtrip");
            }
        });
        run("event gaps zero duration paired jump and below Hold",[] {
            auto c=chart();auto move=c.lines[0].move_events[0];
            move.time.end_ns=2*second;move.end_x=.6;c.lines[0].move_events={move};
            move.time.start_ns=3*second;move.time.end_ns=3*second;move.start_x=.6;move.end_x=.7;c.lines[0].move_events.push_back(move);
            auto rotation=c.lines[0].rotate_events[0];rotation.time.end_ns=2*second;rotation.end=45;c.lines[0].rotate_events={rotation};
            rotation.time.start_ns=3*second;rotation.time.end_ns=3*second;rotation.start=45;rotation.end=90;c.lines[0].rotate_events.push_back(rotation);
            note(c,NoteKind::hold,second,4*second,1,NoteSide::below);
            const auto result=compile_phigros_contacts(c,identity(c),geometry());const auto& p=plan(result);
            require(p.memory.contacts.size()==1,"same below Hold pointer through gap");
            const auto& steps=p.memory.contacts[0].steps;
            require(std::any_of(steps.begin(),steps.end(),[](const Step& s){return s.song_ns==3*second&&s.phase==Phase::move;}),"paired zero event jump retained");
            PhigrosPoseEvaluator poses(c);PhigrosGeometryMap mapping(geometry());double error=0;
            for(SongNs t=second;t<4*second;t+=1'000'003) error=std::max(error,held_error(p.memory.contacts[0],poses,0,c.lines[0].notes[0],t,mapping));
            require(error<=4+1e-8,"gaps and jump obey discrete bound");
        });
        run("continuous path excursion rejected between valid endpoints",[] {
            auto c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=.1;
            c.lines[0].rotate_events[0].time.end_ns=2*second;c.lines[0].rotate_events[0].end=360;
            note(c,NoteKind::hold,0,2*second,2);
            const auto result=compile_phigros_contacts(c,identity(c),geometry());
            require(!result.plan&&diagnostic(result,"offscreen_target"),"full rotation excursion offscreen rejected");
        });
        run("fresh drag flick neighboring Tap Hold conflict diagnostics",[] {
            auto c=chart();note(c,NoteKind::hold,second,3*second);note(c,NoteKind::tap,2*second,2*second);
            note(c,NoteKind::drag,2*second,2*second);note(c,NoteKind::flick,2*second+16'000'000,2*second+16'000'000);
            ContactCompileOptions options;options.max_contacts=5;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(result.plan.has_value(),"unverified interference is diagnostic not invented impossibility");
            require(diagnostic(result,"nearby_tap_hold_interference"),"neighbor conflict reported");
            const auto it=std::find_if(result.diagnostics.begin(),result.diagnostics.end(),[](const auto& d){return d.code=="nearby_tap_hold_interference";});
            require(!it->source_note_id.empty()&&!it->related_note_id.empty(),"both conflict sources retained");
        });
        run("cache invalidation geometry options source and complete output",[] {
            auto c=chart();note(c,NoteKind::tap);
            const auto original=compile_phigros_contacts(c,identity(c),geometry());const auto hash=plan(original).metadata.cache_sha256;
            auto alternate=geometry();alternate.input_width=1920;alternate.input_height=1080;
            require(plan(compile_phigros_contacts(c,identity(c),alternate)).metadata.cache_sha256!=hash,"geometry changes cache");
            auto options=ContactCompileOptions{};options.tap_duration_ns++;
            require(plan(compile_phigros_contacts(c,identity(c),geometry(),options)).metadata.cache_sha256!=hash,"timing changes cache");
            auto id=identity(c);id.audio_sha256=std::string(64,'c');
            require(plan(compile_phigros_contacts(c,id,geometry())).metadata.cache_sha256!=hash,"audio changes cache");
            c.lines[0].notes[0].position_x=1;
            require(plan(compile_phigros_contacts(c,identity(c),geometry())).metadata.cache_sha256!=hash,"claimed same source changed IR output changes cache");
        });
        run("dynamic normal intersection analytic cardinal diagonal and side goldens",[] {
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-dynamic-M7";
            for(auto side:{NoteSide::above,NoteSide::below}) {
                auto c=chart();auto& move=c.lines[0].move_events[0];move.start_y=move.end_y=1.3;
                note(c,NoteKind::tap,second,second,0,side);
                const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
                const auto pixel=pixel_point(p.memory.contacts[0].steps.front().point,1280,720);
                require(pixel.first==640&&pixel.second==0,"horizontal normal intersects top edge");
                const double expected=side==NoteSide::above ? -216 : 216;
                require(std::abs(p.ownership[0].normal_projection_reference_pixels-expected)<1e-9,"side-normal signed correction");
                require(p.metadata.requires_calibration&&p.ownership[0].time_varying_normal_projection,"all dynamic candidates explicitly unverified");
                require(p.metadata.projection_policy_version=="bounded_normal_intersection_candidate_v1","policy version explicit");
            }
            auto c=chart();auto& move=c.lines[0].move_events[0];move.start_x=move.end_x=-10.0/1280;move.start_y=move.end_y=1+10.0/720;
            c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=45;note(c,NoteKind::tap);
            const auto corner=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(corner);
            require(std::abs(p.ownership[0].normal_projection_reference_pixels+10*std::sqrt(2.0))<1e-9,"diagonal analytic corner distance");
            const auto pixel=pixel_point(p.memory.contacts[0].steps.front().point,1280,720);
            require(pixel.first==0&&pixel.second==0,"diagonal intersection lands at corner");
            c=chart();c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=1300.0/1280;
            c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=90;note(c,NoteKind::tap);
            const auto right=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& r=plan(right);
            require(std::abs(r.ownership[0].normal_projection_reference_pixels-20)<1e-9,"positive90 normal reaches right edge");
            require(pixel_point(r.memory.contacts[0].steps.front().point,1280,720).first==1279,"right edge rasterized inward");
        });
        run("dynamic projection follows interior Hold excursion fixed candidate cannot",[] {
            auto c=chart();auto first=c.lines[0].move_events[0];first.time.end_ns=2*second;
            auto off=first;off.time.start_ns=2*second;off.time.end_ns=3*second;off.start_y=off.end_y=1.3;
            auto last=first;last.time.start_ns=3*second;last.time.end_ns=10*second;
            c.lines[0].move_events={first,off,last};note(c,NoteKind::hold,second,4*second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_candidate;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-dynamic-M7";
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"old fixed policy retains original semantics");
            require(!compile_phigros_contacts(c,identity(c),geometry()).plan,"default remains strict rejection");
            options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
            require(p.memory.contacts.size()==1&&p.ownership[0].contact_id==p.memory.contacts[0].id,"Hold pointer retained through projection changes");
            const auto& steps=p.memory.contacts[0].steps;
            require(steps.size()==4&&steps[1].song_ns==2*second&&steps[2].song_ns==3*second,"both native jumps retained exactly");
            require(pixel_point(steps[0].point,1280,720).second==360&&pixel_point(steps[1].point,1280,720).second==0&&
                    pixel_point(steps[2].point,1280,720).second==360,"per-time projection only while source is offscreen");
            require(p.ownership[0].normal_projection_spans.size()==3,"all continuous native spans have provenance");
            require(p.ownership[0].projection_min_reference_pixels<-215&&p.ownership[0].projection_max_reference_pixels==0,"signed correction envelope retained");
            require(diagnostic(result,"unverified_normal_projection"),"mid-Hold projection warned even judgment is inside");
        });
        run("dynamic inside outside edge switches preserve zero order error",[] {
            auto c=chart();auto& move=c.lines[0].move_events[0];move.time.end_ns=second;
            move.start_y=1+20.0/720;move.end_y=-20.0/720;
            note(c,NoteKind::hold,0,second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=25;options.calibration_id="pending-switch";
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& contact=plan(result).memory.contacts[0];
            double error=0;
            for(SongNs t=0;t<second;t+=1'000'003) {
                const auto it=std::upper_bound(contact.steps.begin(),contact.steps.end(),t,[](SongNs time,const Step& step){return time<step.song_ns;});
                const auto& held=std::prev(it)->point;
                const double expected_y=std::clamp(-20+760*static_cast<double>(t)/second,0.0,720.0);
                error=std::max(error,std::hypot(held.x*1279-640,held.y*719-expected_y));
            }
            require(error<=options.max_path_error_pixels+1e-7,"inside/top/bottom branch path actual held error bound");
            require(plan(result).ownership[0].normal_projection_spans.size()>=2,"branch range split has bounded certificates");
        });
        run("dynamic corner branch switch and rotated stretch held bound",[] {
            auto c=chart();auto& move=c.lines[0].move_events[0];move.time.end_ns=second;
            move.start_x=-30.0/1280;move.end_x=-10.0/1280;move.start_y=1+10.0/720;move.end_y=1+30.0/720;
            c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=45;note(c,NoteKind::hold,0,second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=50;options.calibration_id="pending-corner";
            PhigrosGeometry g;g.input_width=900;g.input_height=1600;g.rotation=InputRotation::clockwise_90;
            g.fit=ViewportFit::stretch;g.viewport=PixelViewport{100,50,1400,800};
            const auto result=compile_phigros_contacts(c,identity(c),g,options);const auto& p=plan(result);
            PhigrosGeometryMap mapping(g);double error=0;
            for(SongNs t=0;t<second;t+=1'000'003) {
                const auto& steps=p.memory.contacts[0].steps;
                const auto it=std::upper_bound(steps.begin(),steps.end(),t,[](SongNs time,const Step& step){return time<step.song_ns;});
                const auto held=std::prev(it)->point;const double f=static_cast<double>(t)/second;
                const PosePoint expected=f<.5 ? PosePoint{0,20-40*f} : PosePoint{40*f-20,0};
                const auto target=mapping.map_reference_pixels(expected).input_normalized;
                error=std::max(error,std::hypot(held.x*899-target.x*900,held.y*1599-target.y*1600));
            }
            require(error<=options.max_path_error_pixels+1e-7,"corner-switch rotated stretched discrete bound");
            require(std::any_of(p.ownership[0].normal_projection_spans.begin(),p.ownership[0].normal_projection_spans.end(),[](const auto& span){return (span.possible_edge_mask&5)==5;}),"both corner branches included in speed proof");
        });
        run("dynamic empty sets grazing invisible flick and budgets reject",[] {
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-negative";
            auto c=chart();c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=1.1;note(c,NoteKind::tap);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"projection_unavailable"),"normal cannot fix along-line offscreen coordinate");
            c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1.3;note(c,NoteKind::tap);
            auto short_bound=options;short_bound.max_normal_projection_reference_pixels=200;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),short_bound),"projection_unavailable"),"distance over declared maximum rejected");
            c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1+10.0/720;
            c.lines[0].rotate_events[0].time.end_ns=second;c.lines[0].rotate_events[0].end=180;note(c,NoteKind::hold,0,second);
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"valid endpoint intersections cannot hide impossible middle normal");
            c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1+1e-7/720;
            c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=89.9999999;note(c,NoteKind::hold,0,second);
            const auto grazing=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(diagnostic(grazing,"uncertified_normal_projection"),"near tangent denominator cannot claim finite certified speed");
            c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1.3;note(c,NoteKind::flick);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invisible_flick"),"projection may not erase all visible Flick displacement");
            c=chart();note(c,NoteKind::hold,second,2*second);options.max_steps_per_contact=2;
            auto move=c.lines[0].move_events[0];move.time.start_ns=second+second/2;move.start_x=move.end_x=.6;
            c.lines[0].move_events[0].time.end_ns=move.time.start_ns;c.lines[0].move_events.push_back(move);
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"native keys cannot exceed configured step budget silently");
        });
        run("v2 canonical fingerprint copy roundtrip and semantic mutations",[] {
            auto c=chart();note(c,NoteKind::tap);
            const auto result=compile_phigros_contacts(c,identity(c),geometry());const auto original=plan(result);
            const auto hash=compiled_contact_fingerprint(original);
            require(hash==original.metadata.cache_sha256,"compiler uses public canonical fingerprint");
            std::vector<CompiledContactPlan> archive{original};auto roundtrip=archive.front();roundtrip.metadata.cache_sha256="ignored stored digest";
            require(compiled_contact_fingerprint(roundtrip)==hash,"semantic value copy roundtrip stable and stored cache excluded");
            require(plan(compile_phigros_contacts(c,identity(c),geometry())).metadata.cache_sha256==hash,"repeated compilation canonical digest stable");
            auto changed=original;changed.memory.contacts[0].steps[0].point.x+=.001;changed.events[0].point=changed.memory.contacts[0].steps[0].point;
            require(compiled_contact_fingerprint(changed)!=hash,"coordinated memory and events mutation changes digest");
            changed=original;changed.ownership[0].judgment_ns++;
            require(compiled_contact_fingerprint(changed)!=hash,"ownership judgment is fingerprinted");
            changed=original;changed.ownership[0].source_json_pointer+="changed";
            require(compiled_contact_fingerprint(changed)!=hash,"ownership JSON pointer fingerprinted");
            changed=original;changed.ownership[0].kind=NoteKind::hold;changed.ownership[0].side=NoteSide::below;
            require(compiled_contact_fingerprint(changed)!=hash,"ownership note kind and side fingerprinted");
            changed=original;changed.metadata.requires_calibration=true;
            require(compiled_contact_fingerprint(changed)!=hash,"calibration qualification fingerprinted");
            changed=original;changed.metadata.strategy+="other";
            require(compiled_contact_fingerprint(changed)!=hash,"strategy fingerprinted");
            changed=original;changed.peak_contacts++;changed.total_steps++;
            require(compiled_contact_fingerprint(changed)!=hash,"capacity and declared count fingerprinted");
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-digest";
            c=chart();note(c,NoteKind::hold,second,2*second);
            const auto dynamic=compile_phigros_contacts(c,identity(c),geometry(),options);auto certificate=plan(dynamic);
            const auto dynamic_hash=compiled_contact_fingerprint(certificate);certificate.ownership[0].normal_projection_spans[0].speed_bound_input_pixels_per_ns+=1;
            require(compiled_contact_fingerprint(certificate)!=dynamic_hash,"dynamic proof spans fingerprinted");
        });
        run("zero inset preserves pinned strict and dynamic fingerprints",[] {
            auto c=chart();note(c,NoteKind::tap);
            const auto strict=compile_phigros_contacts(c,identity(c),geometry());
            require(plan(strict).metadata.cache_sha256==v2_golden(
                "2a9e0c0b1a909416bd2a440175bb5ccf96a9dae9a2a68aae3a449026df9305b7",
                "b5e1a3a49232589c072a7fa5d433b523bc2112e5cec71b037be3347da451468b"),
                "pre-inset strict golden fingerprint changed");
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-inset-baseline";
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);
            const auto zero_hash=plan(original).metadata.cache_sha256;
            require(zero_hash==v2_golden(
                "e70738a35d657526185a1d10dc696e0b85e3e8c50f2e8d88dfccdc34f1eac62b",
                "42a06b7e0b5cb5480bd5c3a809f772092a4888d96da55596f4cd4815a1675e8f"),
                "pre-inset dynamic golden fingerprint changed");
            options.normal_projection_inset_reference_pixels=0;
            require(plan(compile_phigros_contacts(c,identity(c),geometry(),options)).metadata.cache_sha256==zero_hash,
                "explicit zero inset changed original candidate");
            options.normal_projection_inset_reference_pixels=64;
            const auto inset=compile_phigros_contacts(c,identity(c),geometry(),options);inside_inset(plan(inset));
            require(plan(inset).memory.contacts[0].steps[0].point.x==plan(original).memory.contacts[0].steps[0].point.x&&
                plan(inset).memory.contacts[0].steps[0].point.y==plan(original).memory.contacts[0].steps[0].point.y,
                "interior input point moved merely to change inset identity");
            require(plan(inset).metadata.cache_sha256!=zero_hash,"inset policy absent from fingerprint when coordinates happen to match");
            auto changed=plan(inset);changed.metadata.options.normal_projection_inset_reference_pixels=65;
            require(compiled_contact_fingerprint(changed)!=plan(inset).metadata.cache_sha256,"inset option mutation not fingerprinted");
        });
        run("inset projects static top bottom Tap Drag and below contacts inside edges",[] {
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-inset-static";
            options.normal_projection_inset_reference_pixels=64;
            for(const bool top:{true,false})for(const auto kind:{NoteKind::tap,NoteKind::drag})for(const auto side:{NoteSide::above,NoteSide::below}) {
                auto c=chart();auto& move=c.lines[0].move_events[0];move.start_y=move.end_y=top?1:0;
                note(c,kind,second,second,0,side);
                const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
                const auto expected=top?64:655;
                for(const auto& step:p.memory.contacts[0].steps)require(pixel_point(step.point,1280,720).second==expected,
                    "static projection did not use declared inset edge");
                const auto magnitude=top?64.:65.;
                require(std::abs(std::abs(p.ownership[0].normal_projection_reference_pixels)-magnitude)<1e-9,
                    "inset correction did not retain actual side-normal distance");
                require(p.metadata.requires_calibration&&diagnostic(result,"unverified_normal_projection"),
                    "inset candidate lost unverified projection warning");
            }
        });
        run("continuous Hold follows inset clipping with full pointer lifetime and error bound",[] {
            auto c=chart();auto prototype=c.lines[0].move_events[0];c.lines[0].move_events.clear();
            for(unsigned i=0;i<3;++i) {
                auto move=prototype;move.time.start_ns=i*second;move.time.end_ns=(i+1)*second;
                const double raw_y[]{.5,1.3,-.3,.5};move.start_y=raw_y[i];move.end_y=raw_y[i+1];
                c.lines[0].move_events.push_back(move);
            }
            note(c,NoteKind::hold,0,3*second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=350;options.calibration_id="pending-inset-continuous";
            options.normal_projection_inset_reference_pixels=64;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
            require(p.memory.contacts.size()==1&&p.memory.contacts[0].steps.front().song_ns==0&&
                p.memory.contacts[0].steps.back().song_ns==3*second,"inset Hold lifetime/source pointer shortened");
            const auto& owner=p.ownership[0];
            require(owner.projection_min_reference_pixels<-279&&owner.projection_max_reference_pixels>280,
                "continuous inset certificate omitted top/bottom excursion correction");
            PhigrosPoseEvaluator poses(c);const auto& contact=p.memory.contacts[0];double worst=0;
            for(SongNs t=0;t<3*second;t+=1'000'003) {
                const auto it=std::upper_bound(contact.steps.begin(),contact.steps.end(),t,[](SongNs at,const Step& step){return at<step.song_ns;});
                const auto actual=pixel_point(std::prev(it)->point,1280,720);
                const auto native=poses.note_target(0,c.lines[0].notes[0],t).reference_pixels;
                const auto expected_y=std::clamp(native.y,64.,655.);
                worst=std::max(worst,std::hypot(actual.first-native.x,actual.second-expected_y));
            }
            require(worst<=options.max_path_error_pixels+1e-8,"continuous inset Hold zero-order error exceeded declared bound");
            require(diagnostic(result,"unverified_normal_projection"),"continuous inset remains a game calibration candidate");
        });
        run("Flick stroke and final dwell stay inside top bottom inset",[] {
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-inset-flick";
            options.normal_projection_inset_reference_pixels=64;options.flick_lead_ns=30'000'000;
            options.flick_travel_ns=60'000'000;options.flick_distance_reference_pixels=60;options.flick_release_ns=24'000'000;
            for(const double native_y:{70.,649.}) {
                auto c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1-native_y/720;
                note(c,NoteKind::flick);
                const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
                const auto& steps=p.memory.contacts[0].steps;
                require(steps.front().song_ns==second-30'000'000&&steps.back().song_ns==second+54'000'000,
                    "inset changed native Flick lead/travel/release timing");
                require(std::any_of(steps.begin(),steps.end(),[](const auto& step){return step.song_ns==second&&step.phase==Phase::move;}),
                    "inset Flick lost native judgment keytime");
                const auto first=pixel_point(steps.front().point,1280,720),last=pixel_point(steps[steps.size()-2].point,1280,720);
                require(std::hypot(first.first-last.first,first.second-last.second)>=30,"inset erased useful Flick displacement");
                require(steps.back().point.x==steps[steps.size()-2].point.x&&steps.back().point.y==steps[steps.size()-2].point.y,
                    "Flick dwell UP moved after final inset stroke position");
            }
            auto c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1;
            note(c,NoteKind::flick);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invisible_flick"),
                "completely clipped Flick falsely retained observable displacement");
        });
        run("inset unreachable tangent continuous strip and invalid options reject whole chart",[] {
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=250;options.calibration_id="pending-inset-negative";
            options.normal_projection_inset_reference_pixels=64;
            auto c=chart();c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=.02;note(c,NoteKind::tap);
            const auto tangent=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!tangent.plan&&diagnostic(tangent,"projection_unavailable"),"normal projection changed along-line x to reach inset");
            c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1;note(c,NoteKind::tap);
            auto short_bound=options;short_bound.max_normal_projection_reference_pixels=63;
            const auto distance=compile_phigros_contacts(c,identity(c),geometry(),short_bound);
            require(!distance.plan&&diagnostic(distance,"projection_unavailable"),"inset exceeded explicit normal correction bound");
            c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1-30./720;
            c.lines[0].rotate_events[0].time.end_ns=2*second;c.lines[0].rotate_events[0].end=180;
            note(c,NoteKind::hold,0,2*second);
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,
                "valid endpoint intersections concealed impossible intermediate inset strip");
            c=chart();note(c,NoteKind::tap);
            for(const double invalid:{-1.,100.000001,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
                auto bad=options;bad.normal_projection_inset_reference_pixels=invalid;
                const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),bad);
                require(!rejected.plan&&diagnostic(rejected,"invalid_options"),"invalid/unbounded inset option accepted");
            }
            for(const auto unsupported:{OffscreenPolicy::reject,OffscreenPolicy::bounded_normal_candidate}) {
                auto bad=options;bad.offscreen_policy=unsupported;
                const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),bad);
                require(!rejected.plan&&diagnostic(rejected,"invalid_options"),"inset silently ignored by unsupported projection policy");
            }
        });
        run("analytic edge speed removes clipped normal motion and retains held path proof",[] {
            auto c=chart();auto& move=c.lines[0].move_events[0];move.time.end_ns=second;
            move.start_x=.3;move.end_x=.7;move.start_y=1.2;move.end_y=1.8;note(c,NoteKind::hold,0,second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;options.calibration_id="analytic-edge-test";
            const auto legacy=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.tight_projection_speed_bounds=true;const auto tight=compile_phigros_contacts(c,identity(c),geometry(),options);
            const auto& p=plan(tight);inside_inset(p);
            require(p.total_steps*3<plan(legacy).total_steps*2,"active-edge analytic bound failed to reduce over-sampling");
            require(projected_held_error(p,c,options,0,second)<=options.max_path_error_pixels+1e-7,"clipped path escaped original4px zero-order bound");
            require(p.memory.contacts[0].steps.front().song_ns==0&&p.memory.contacts[0].steps.back().song_ns==second,
                "analytic clipping changed complete Hold lifetime");
            require(p.metadata.cache_sha256!=plan(legacy).metadata.cache_sha256&&p.metadata.strategy.find("analytic-projected-derivative-v1")!=std::string::npos,
                "explicit analytic sampling policy absent from identity");
            c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=.5;
            const auto flat=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& flat_plan=plan(flat);
            require(flat_plan.total_steps==2,"constant clipped path retained invented interior moves");
            require(std::all_of(flat_plan.ownership[0].normal_projection_spans.begin(),flat_plan.ownership[0].normal_projection_spans.end(),
                [](const auto& span){return span.speed_bound_input_pixels_per_ns==0;}),"normal-only clipped motion retained nonzero tangent speed");
        });
        run("analytic velocity preserves affine Flick cancellation judgment and endpoint keys",[] {
            auto c=chart();const auto prototype=c.lines[0].move_events[0];c.lines[0].move_events.clear();
            auto before=prototype;before.time.end_ns=second-30'000'000;before.start_y=before.end_y=1-330./720;
            auto stroke=prototype;stroke.time.start_ns=before.time.end_ns;stroke.time.end_ns=second+30'000'000;
            stroke.start_y=before.end_y;stroke.end_y=1-384./720;
            auto after=prototype;after.time.start_ns=stroke.time.end_ns;after.start_y=after.end_y=stroke.end_y;
            c.lines[0].move_events={before,stroke,after};note(c,NoteKind::flick);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;options.calibration_id="analytic-flick-test";
            options.flick_lead_ns=30'000'000;options.flick_travel_ns=60'000'000;options.flick_distance_reference_pixels=60;options.flick_release_ns=24'000'000;
            const auto legacy=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.tight_projection_speed_bounds=true;const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
            require(p.total_steps<plan(legacy).total_steps,"affine motion cancellation did not tighten sampled Flick path");
            const auto& steps=p.memory.contacts[0].steps;
            for(const auto t:{second-30'000'000,second,second+30'000'000,second+54'000'000})
                require(std::any_of(steps.begin(),steps.end(),[&](const auto& s){return s.song_ns==t;}),"analytic Flick dropped critical lead/judgment/end/dwell key");
            require(projected_held_error(p,c,options,second-30'000'000,second+30'000'000)<=options.max_path_error_pixels+1e-7,
                "cancelled affine Flick violated unchanged zero-order error");
        });
        run("analytic rotating vertical horizontal and switching edges retain continuous error bound",[] {
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=250;options.normal_projection_inset_reference_pixels=64;
            options.calibration_id="analytic-rotating-test";options.tight_projection_speed_bounds=true;
            for(unsigned which=0;which<3;++which)for(const auto side:{NoteSide::above,NoteSide::below}) {
                auto c=chart();auto& move=c.lines[0].move_events[0];move.time.end_ns=second;
                auto& rotate=c.lines[0].rotate_events[0];rotate.time.end_ns=second;
                if(which==0){move.start_y=move.end_y=1+20./720;rotate.start=-20;rotate.end=20;}
                if(which==1){move.start_x=move.end_x=-20./1280;rotate.start=70;rotate.end=110;}
                if(which==2){move.start_x=-30./1280;move.end_x=-10./1280;move.start_y=1+10./720;move.end_y=1+30./720;rotate.start=rotate.end=45;}
                note(c,NoteKind::hold,0,second,which==2?0:1,side);
                auto g=geometry();if(which==1){g.input_width=900;g.input_height=1600;g.rotation=InputRotation::clockwise_90;g.fit=ViewportFit::stretch;g.viewport=PixelViewport{100,50,1400,800};}
                const auto result=compile_phigros_contacts(c,identity(c),g,options);const auto& p=plan(result);
                require(projected_held_error(p,c,options,0,second)<=options.max_path_error_pixels+1e-7,"rotating/corner analytic projected path escaped error certificate");
                require(p.memory.contacts[0].steps.back().song_ns==second,"rotating analytic path clipped lifetime");
            }
            auto c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1+1e-7/720;
            c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=89.9999999;note(c,NoteKind::hold,0,second);
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"analytic option weakened near-tangent rejection");
            options.offscreen_policy=OffscreenPolicy::reject;c=chart();note(c,NoteKind::tap);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"analytic policy silently ignored by strict projection mode");
        });
        run("native import interpretation provenance is automatic and conditionally fingerprinted",[] {
            auto c=chart();note(c,NoteKind::tap);const auto strict=compile_phigros_contacts(c,identity(c),geometry());
            const auto strict_hash=plan(strict).metadata.cache_sha256;
            require(strict_hash==v2_golden("2a9e0c0b1a909416bd2a440175bb5ccf96a9dae9a2a68aae3a449026df9305b7",
                "b5e1a3a49232589c072a7fa5d433b523bc2112e5cec71b037be3347da451468b"),"strict import default changed archived golden");
            c.import_policy_id="phigros-v3-source-priority-constant-empty-candidate-v1";c.import_interpretation_unqualified=true;
            ChartImportAnomaly anomaly;anomaly.source.json_pointer="/judgeLineList/0/judgeLineDisappearEvents/0";
            anomaly.source.id=c.source.chart_sha256+'#'+anomaly.source.json_pointer;c.import_anomalies.push_back(anomaly);
            const auto result=compile_phigros_contacts(c,identity(c),geometry());const auto& p=plan(result);
            require(p.metadata.import_policy_id==c.import_policy_id&&p.metadata.import_interpretation_unqualified&&
                p.metadata.import_anomaly_source_ids==std::vector<std::string>{anomaly.source.id},"compiler dropped native interpretation provenance");
            require(p.metadata.cache_sha256!=strict_hash,"different import interpretation reused strict fingerprint");
            auto changed=p;changed.metadata.import_anomaly_source_ids.push_back(anomaly.source.id);
            require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"native anomalous source interpretation not fingerprinted");
            c.import_anomalies[0].source.id="forged";
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry()),"invalid_import_policy"),"invented native anomaly ownership accepted");
        });
        run("invalid identity source time dimensions NaN and bounded budgets",[] {
            auto c=chart();note(c,NoteKind::tap);auto id=identity(c);id.revision="wrong";
            require(diagnostic(compile_phigros_contacts(c,id,geometry()),"identity_mismatch"),"revision mismatch rejected");
            c.lines[0].notes[0].source.id="untraceable";
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry()),"invalid_note_source"),"invented source id rejected");
            c=chart();note(c,NoteKind::hold,second,second);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry()),"zero_length_hold"),"zero duration Hold rejected");
            c=chart();note(c,NoteKind::tap,-1,-1);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry()),"invalid_note"),"negative song timestamps rejected");
            c=chart();note(c,NoteKind::flick,0,0);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry()),"negative_flick_lead"),"negative flick stroke start rejected");
            c=chart();note(c,NoteKind::tap);c.lines[0].notes[0].position_x=std::numeric_limits<double>::quiet_NaN();
            require(!compile_phigros_contacts(c,identity(c),geometry()).plan,"NaN rejected");
            c=chart();note(c,NoteKind::tap);auto g=geometry();g.input_width=1;
            require(!compile_phigros_contacts(c,identity(c),g).plan,"invalid pixel surface rejected");
            c=chart();note(c,NoteKind::tap,std::numeric_limits<SongNs>::max()-1,std::numeric_limits<SongNs>::max()-1);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry()),"invalid_time"),"release overflow rejected");
            c=chart();note(c,NoteKind::flick);ContactCompileOptions options;options.max_steps_per_contact=2;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"trajectory_budget"),"bounded percontact steps");
            options=ContactCompileOptions{};options.max_diagnostics=0;
            const auto invalid=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!invalid.plan&&!invalid.diagnostics.empty(),"invalid diagnostic budget still reports error");
            options=ContactCompileOptions{};options.max_steps_in_plan=2;
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"whole song steps bounded");
        });
        std::cout<<checks<<" checks passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
