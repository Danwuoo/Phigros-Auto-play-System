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
#include <tuple>

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
ContactCompileOptions coincident_options() {
    ContactCompileOptions options;options.max_contacts=10;
    options.flick_path_policy=FlickPathPolicy::judgment_anchored_diagonal_snap_v1;
    options.flick_distance_reference_pixels=48;options.flick_lead_ns=8'000'000;
    options.flick_travel_ns=20'000'000;options.flick_release_ns=24'000'000;
    return options;
}
ContactCompileOptions sampled_options() {
    auto options=coincident_options();options.flick_path_policy=FlickPathPolicy::judgment_anchored_diagonal_sampled_v2;
    options.flick_distance_reference_pixels=96;options.flick_travel_ns=36'000'000;
    options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
    options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;
    options.calibration_id="sampled-v2-unqualified";return options;
}
ChartBlockArea& block(SemanticChart& c,double left,double top,double right,double bottom,
                      SongNs enable=0,SongNs disable=3*second,bool subtract=false) {
    ChartBlockArea area;
    area.bottom_left={left/1280,1-bottom/720};area.top_right={right/1280,1-top/720};
    area.appear_ns=area.enable_ns=enable;area.disable_ns=area.disappear_ns=disable;
    area.subtract=subtract;
    area.source.json_pointer="/blockAreaList/"+std::to_string(c.block_areas.size());
    area.source.id=c.source.chart_sha256+'#'+area.source.json_pointer;
    return c.block_areas.emplace_back(std::move(area));
}
ContactCompileOptions block_options() {
    auto options=sampled_options();
    options.offscreen_policy=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
    options.max_tangent_error_reference_pixels=151.2;
    options.block_area_phase_ns=0;
    return options;
}
// Compare every canonical contact/owner field under the SAME metadata options,
// including source mapping, projection spans and the complete discrete proof.
std::string contact_owner_digest(const CompiledContactPlan& p,std::size_t index,const ContactCompileMetadata& metadata) {
    CompiledContactPlan single;single.metadata=metadata;single.memory.identity=p.memory.identity;
    single.memory.contacts.push_back(p.memory.contacts.at(index));single.ownership.push_back(p.ownership.at(index));
    single.total_steps=single.memory.contacts.front().steps.size();single.peak_contacts=1;
    return compiled_contact_fingerprint(single);
}
void check_coincident_shift(const CompiledContactPlan& before,const CompiledContactPlan& after,std::size_t index) {
    constexpr SongNs delay=32'000'000;
    const auto& old=before.memory.contacts.at(index);const auto& contact=after.memory.contacts.at(index);
    const auto& owner=after.ownership.at(index);
    require(old.steps.size()==4&&contact.steps.size()==4,"coincident Flick lost a complete four-step contact");
    for(std::size_t i=0;i<4;++i)
        require(contact.steps[i].song_ns==old.steps[i].song_ns+delay,"coincident Flick did not shift all four exact timestamps");
    require(owner.judgment_ns==before.ownership.at(index).judgment_ns&&owner.dispatch_judgment_ns==owner.judgment_ns+delay,
        "coincident Flick replaced its native judgment with the new dispatch anchor");
    auto restored=after;
    for(auto& step:restored.memory.contacts[index].steps)step.song_ns-=delay;
    restored.ownership[index].dispatch_judgment_ns=before.ownership[index].dispatch_judgment_ns;
    restored.ownership[index].flick_discrete_path->bend_ns-=delay;
    restored.ownership[index].flick_discrete_path->end_ns-=delay;
    require(contact_owner_digest(before,index,before.metadata)==contact_owner_digest(restored,index,before.metadata),
        "handoff changed a Flick coordinate, source, geometry, projection span, or another ownership field");
}
void check_complete_events(const CompiledContactPlan& p) {
    std::vector<CompiledSongEvent> expected;
    for(const auto& contact:p.memory.contacts)for(const auto& step:contact.steps)
        expected.push_back({step.song_ns,contact.id,step.phase,step.point});
    std::sort(expected.begin(),expected.end(),[](const auto& a,const auto& b){
        return std::tuple(a.song_ns,phase_order(a.phase),a.contact_id)<std::tuple(b.song_ns,phase_order(b.phase),b.contact_id);
    });
    require(expected.size()==p.events.size()&&expected.size()==p.total_steps,"handoff flattened event count is incomplete");
    for(std::size_t i=0;i<expected.size();++i){const auto& a=expected[i];const auto& b=p.events[i];
        require(a.song_ns==b.song_ns&&a.contact_id==b.contact_id&&a.phase==b.phase&&a.point.x==b.point.x&&a.point.y==b.point.y,
            "handoff events are stale, reordered, or differ from their complete contacts");}
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
        run("capacity2 and 5 through10 reject overcapacity rather than skipping notes",[] {
            for(int capacity:{2,5,6,7,8,9,10}) {
                auto c=chart();for(int i=0;i<capacity;++i) note(c,NoteKind::hold,second,2*second,i*.1);
                ContactCompileOptions options;options.max_contacts=capacity;
                const auto success=compile_phigros_contacts(c,identity(c),geometry(),options);
                require(plan(success).peak_contacts==capacity,"capacity parameter honored");
                note(c,NoteKind::hold,second,2*second);
                const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),options);
                require(!rejected.plan&&diagnostic(rejected,"capacity_exceeded"),"overcapacity rejects complete plan");
                require(rejected.required_peak_contacts==capacity+1,"overcapacity required count reported");
            }
            auto c=chart();note(c,NoteKind::hold,second,2*second);
            ContactCompileOptions invalid;invalid.max_contacts=11;
            const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),invalid);
            require(!rejected.plan&&diagnostic(rejected,"invalid_options"),"eleven-pointer declared capacity rejected");
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
        run("diagonal snap is a short curved two MOVE discrete path with held native anchor",[] {
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;options.calibration_id="diagonal-snap-unqualified";
            options.flick_path_policy=FlickPathPolicy::judgment_anchored_diagonal_snap_v1;
            options.flick_distance_reference_pixels=32;options.flick_lead_ns=8'000'000;options.flick_travel_ns=20'000'000;options.flick_release_ns=6'000'000;
            for(const double angle:{0.,30.,90.,135.,180.,-45.,-90.})for(const double x:{64.,640.,1215.})for(const double y:{64.,360.,655.})for(const auto side:{NoteSide::above,NoteSide::below}) {
                auto c=chart();auto& m=c.lines[0].move_events[0];m.start_x=m.end_x=x/1280;m.start_y=m.end_y=1-y/720;
                c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=angle;note(c,NoteKind::flick,second,second,0,side);
                const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
                const auto& contact=p.memory.contacts[0];const auto& owner=p.ownership[0];const auto& steps=contact.steps;
                require(steps.size()==4&&steps[0].phase==Phase::down&&steps[1].phase==Phase::move&&steps[2].phase==Phase::move&&steps[3].phase==Phase::up,"snap added dense MOVE events or removed a contact phase");
                require(steps[0].song_ns==second-8'000'000&&steps[1].song_ns==second+4'000'000&&steps[2].song_ns==second+12'000'000&&steps[3].song_ns==second+18'000'000,"snap changed explicit short timing contract");
                const auto held=std::upper_bound(steps.begin(),steps.end(),second,[](SongNs t,const auto& s){return t<s.song_ns;});
                require(std::prev(held)->phase==Phase::down&&owner.judgment_ns==second,"snap did not hold the native anchor across judgment");
                require(owner.flick_discrete_path.has_value()&&owner.flick_anchor_reference.has_value()&&owner.flick_direction_reference.has_value(),"snap discrete geometry evidence missing");
                const auto& proof=*owner.flick_discrete_path;const auto d=*owner.flick_direction_reference;
                require(proof.model_id=="judgment-anchored-diagonal-snap-v1"&&proof.move_count==2,"snap evidence model or MOVE count is ambiguous");
                require(std::abs(std::abs(d.x)-std::sqrt(.5))<1e-12&&std::abs(std::abs(d.y)-std::sqrt(.5))<1e-12,"snap endpoint is not a viewport diagonal");
                require(std::abs(proof.endpoint_distance_reference_pixels-32)<1e-9&&proof.polyline_length_reference_pixels>32,"snap shortened displacement or became a straight reference path");
                require(proof.max_jump_input_pixels>4&&p.metadata.strategy.find("flick-explicit-discrete-vertices-no-continuous-target-claim")!=std::string::npos,"snap falsely reused the old4px continuous target claim");
                const auto a=pixel_point(steps[0].point,1280,720),b=pixel_point(steps[1].point,1280,720),e=pixel_point(steps[2].point,1280,720);
                const double bx=double(b.first)-a.first,by=double(b.second)-a.second,dx=double(e.first)-a.first,dy=double(e.second)-a.second;
                require(dx!=0&&dy!=0&&bx*dy-by*dx!=0,"snap became straight or axis-aligned after rasterization");
                require(std::abs(std::hypot(dx,dy)-32)<=2*std::sqrt(2.),"snap lost complete measurable endpoint displacement");
                require(diagnostic(result,"unverified_discrete_diagonal_snap")&&owner.normal_projection_spans.size()==1&&
                    owner.normal_projection_spans[0].begin_ns==second&&owner.normal_projection_spans[0].end_ns==second,"snap lost honest candidate/anchor-only certificate scope");
            }
        });
        run("diagonal snap reduces RPC nodes and changes only Flick contacts with conditional evidence",[] {
            auto c=chart();note(c,NoteKind::flick);note(c,NoteKind::tap,2*second,2*second);note(c,NoteKind::drag,3*second,3*second);note(c,NoteKind::hold,4*second,5*second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;options.calibration_id="snap-comparison";
            options.flick_path_policy=FlickPathPolicy::judgment_anchored_screen_fit_v1;options.flick_distance_reference_pixels=60;
            options.flick_lead_ns=30'000'000;options.flick_travel_ns=60'000'000;options.flick_release_ns=24'000'000;
            const auto previous=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& old=plan(previous);
            options.flick_path_policy=FlickPathPolicy::judgment_anchored_diagonal_snap_v1;options.flick_distance_reference_pixels=32;
            options.flick_lead_ns=8'000'000;options.flick_travel_ns=20'000'000;options.flick_release_ns=6'000'000;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
            require(p.memory.contacts.size()==4&&p.ownership.size()==4&&p.memory.contacts[0].steps.size()==4&&old.memory.contacts[0].steps.size()>16,"snap did not materially reduce Flick RPC nodes");
            for(std::size_t i=1;i<4;++i) {
                const auto& a=old.memory.contacts[i];const auto& b=p.memory.contacts[i];require(a.steps.size()==b.steps.size(),"snap changed a non-Flick contact length");
                for(std::size_t j=0;j<a.steps.size();++j)require(a.steps[j].song_ns==b.steps[j].song_ns&&a.steps[j].phase==b.steps[j].phase&&
                    a.steps[j].point.x==b.steps[j].point.x&&a.steps[j].point.y==b.steps[j].point.y,"snap changed a Tap/Drag/Hold command");
                require(!p.ownership[i].flick_discrete_path.has_value(),"snap attached Flick evidence to another note kind");
            }
            require(p.metadata.cache_sha256!=old.metadata.cache_sha256&&p.ownership[0].source_note_id==old.ownership[0].source_note_id,"snap reused old fingerprint or replaced source identity");
            auto changed=p;changed.ownership[0].flick_discrete_path->bend_reference.x+=1;
            require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"snap bend certificate not fingerprinted");
            changed=p;changed.ownership[0].flick_discrete_path->max_jump_input_pixels+=1;
            require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"snap measurable input jump not fingerprinted");
        });
        run("diagonal snap respects rotated stretch geometry finite budgets and raster curvature",[] {
            auto c=chart();note(c,NoteKind::flick);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;options.calibration_id="snap-geometry";
            options.flick_path_policy=FlickPathPolicy::judgment_anchored_diagonal_snap_v1;options.flick_distance_reference_pixels=32;
            options.flick_lead_ns=8'000'000;options.flick_travel_ns=20'000'000;options.flick_release_ns=6'000'000;
            auto rotated=geometry();rotated.input_width=720;rotated.input_height=1280;rotated.rotation=InputRotation::clockwise_90;
            const auto result=compile_phigros_contacts(c,identity(c),rotated,options);require(plan(result).memory.contacts[0].steps.size()==4,"rotated snap added dense path samples");
            auto stretch=geometry();stretch.input_width=1600;stretch.input_height=600;stretch.fit=ViewportFit::stretch;
            require(compile_phigros_contacts(c,identity(c),stretch,options).plan.has_value(),"snap failed valid stretched viewport");
            auto tiny=geometry();tiny.input_width=tiny.input_height=10;
            require(diagnostic(compile_phigros_contacts(c,identity(c),tiny,options),"diagonal_snap_raster_resolution"),"subpixel snap falsely claimed a curved path");
            options.max_steps_per_contact=3;require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"trajectory_budget"),"snap bypassed finite per-contact step budget");
            options.max_steps_per_contact=20000;options.flick_distance_reference_pixels=2000;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"diagonal_snap_unavailable"),"oversized snap was clipped into the viewport");
        });
        run("diagonal snap retains early dispatch and native note source separately",[] {
            auto c=chart();auto m=c.lines[0].move_events[0];m.time.end_ns=second;auto hidden=m;hidden.time.start_ns=second;hidden.time.end_ns=10*second;hidden.start_x=hidden.end_x=3;
            c.lines[0].move_events={m,hidden};note(c,NoteKind::flick);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;options.calibration_id="snap-early";
            options.flick_path_policy=FlickPathPolicy::judgment_anchored_diagonal_snap_v1;options.flick_distance_reference_pixels=32;
            options.flick_lead_ns=8'000'000;options.flick_travel_ns=20'000'000;options.flick_release_ns=6'000'000;options.max_prejudge_ns=30'000'000;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);const auto& owner=p.ownership[0];
            require(owner.judgment_ns==second&&owner.dispatch_judgment_ns==second-30'000'000&&owner.flick_discrete_path.has_value(),"snap relabeled early dispatch as native source judgment");
            require(owner.flick_discrete_path->bend_ns==second-26'000'000&&owner.flick_discrete_path->end_ns==second-18'000'000&&
                p.memory.contacts[0].steps.front().song_ns==second-38'000'000,"early snap changed explicit per-note timing");
        });
        run("explicit Drag Flick pre-judge uses reachable early pose and retains native source time",[] {
            for(const auto kind:{NoteKind::drag,NoteKind::flick}) {
                auto c=chart();auto initial=c.lines[0].move_events[0];initial.time.end_ns=second;
                auto later=initial;later.time.start_ns=second;later.time.end_ns=10*second;later.start_x=later.end_x=3;
                c.lines[0].move_events={initial,later};note(c,kind);
                ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
                options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;
                options.calibration_id="prejudge-unqualified";options.flick_path_policy=FlickPathPolicy::judgment_anchored_screen_fit_v1;
                require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"native unreachable pose silently advanced without opt-in");
                options.max_prejudge_ns=30'000'000;
                const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
                require(c.lines[0].notes[0].start_ns==second&&c.lines[0].notes[0].end_ns==second,"prejudge mutated native input chart");
                require(p.ownership[0].judgment_ns==second&&p.ownership[0].dispatch_judgment_ns==second-30'000'000,"prejudge replaced native judgment with dispatch time");
                require(p.ownership[0].source_note_id==c.lines[0].notes[0].source.id&&p.memory.contacts.size()==1,"prejudge lost note identity or count");
                require(diagnostic(result,"unverified_early_judgment")&&p.metadata.options.max_prejudge_ns==30'000'000&&
                    p.metadata.judgment_policy_provenance.find("pre-judge")!=std::string::npos,"prejudge provenance missing");
                const auto down=kind==NoteKind::flick?second-30'000'000-options.flick_lead_ns:second-30'000'000;
                require(p.memory.contacts[0].steps.front().song_ns==down,"prejudge did not compile full contact at actual early anchor");
                auto changed=p;--changed.ownership[0].dispatch_judgment_ns;
                require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"dispatch anchor not fingerprinted");
            }
        });
        run("above Drag pre-judge staggers thirteen independent source contacts within ten slots",[] {
            auto c=chart();for(int i=0;i<6;++i){note(c,NoteKind::drag,second,second,0,NoteSide::above);note(c,NoteKind::drag,second,second,0,NoteSide::below);}
            note(c,NoteKind::flick);note(c,NoteKind::tap,3*second,3*second);note(c,NoteKind::hold,4*second,5*second);
            ContactCompileOptions options;options.max_contacts=10;options.drag_duration_ns=16'000'000;
            options.flick_lead_ns=30'000'000;options.flick_travel_ns=60'000'000;options.flick_release_ns=24'000'000;
            const auto native=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!native.plan&&native.required_peak_contacts==13,"thirteen-contact fixture no longer exercises true capacity limit");
            options.max_prejudge_ns=30'000'000;
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"fallback-only policy secretly staggered reachable notes");
            options.advance_above_drags=true;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
            require(p.memory.contacts.size()==15&&p.ownership.size()==15&&p.peak_contacts==7,"prejudge dropped sources or failed to stagger real contact lifetimes");
            for(const auto& owner:p.ownership) {
                if(owner.kind==NoteKind::tap||owner.kind==NoteKind::hold)require(owner.dispatch_judgment_ns==-1,"prejudge modified Tap/Hold timing policy");
                else require(owner.judgment_ns==second&&owner.dispatch_judgment_ns==second-(owner.kind==NoteKind::drag&&owner.side==NoteSide::above?30'000'000:0),"per-note advance changed nonselected source timing");
            }
            options.max_contacts=6;require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"prejudge bypassed actual seven-contact capacity");
            c.lines[0].notes[0].end_ns=second-1;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_note"),"advance repaired invalid original native note instead of rejecting it");
        });
        run("below Drag advance is independent and default false preserves archived fingerprints",[] {
            auto c=chart();note(c,NoteKind::drag);note(c,NoteKind::drag,second,second,0,NoteSide::below);
            note(c,NoteKind::tap,2*second,2*second);note(c,NoteKind::hold,3*second,3*second+500'000'000);
            note(c,NoteKind::flick,4*second,4*second);
            auto options=coincident_options();options.max_prejudge_ns=30'000'000;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& before=plan(original);
            auto explicit_false=options;explicit_false.advance_below_drags=false;
            const auto unchanged=compile_phigros_contacts(c,identity(c),geometry(),explicit_false);
            require(plan(unchanged).metadata.cache_sha256==before.metadata.cache_sha256,
                "explicit false below option changed the archived pre-judge transcript");
            require(before.metadata.strategy.find("advance-below-drags-candidate-v1")==std::string::npos&&
                before.metadata.judgment_policy_provenance.find("advance-below-drags-candidate-v1")==std::string::npos,
                "default false added below policy metadata");
            auto declared=before;declared.metadata.options.advance_below_drags=true;
            require(compiled_contact_fingerprint(declared)!=before.metadata.cache_sha256,
                "enabled below policy was omitted from the fingerprint");
            declared.metadata.options.advance_below_drags=false;
            require(compiled_contact_fingerprint(declared)==before.metadata.cache_sha256,
                "false below policy emitted a new canonical token");
            options.advance_below_drags=true;
            const auto compiled=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(compiled);
            require(after.memory.contacts.size()==5&&after.total_steps==before.total_steps,
                "below advance dropped a complete source contact or step");
            for(std::size_t i=0;i<5;++i){
                const auto& owner=after.ownership[i];
                require(owner.source_note_id==before.ownership[i].source_note_id&&owner.kind==before.ownership[i].kind&&
                    owner.judgment_ns==before.ownership[i].judgment_ns,"below advance changed immutable native source/kind/judgment");
                if(i!=1)require(contact_owner_digest(before,i,before.metadata)==contact_owner_digest(after,i,before.metadata),
                    "below-only policy changed an above Drag, Tap, Hold or Flick");
            }
            for(std::size_t i=0;i<2;++i)require(after.memory.contacts[1].steps[i].song_ns==before.memory.contacts[1].steps[i].song_ns-30'000'000,
                "below Drag did not retain its complete duration at the early endpoint");
            require(after.ownership[1].dispatch_judgment_ns==second-30'000'000&&
                after.metadata.strategy.find("advance-below-drags-candidate-v1")!=std::string::npos&&
                after.metadata.judgment_policy_provenance.find("advance-below-drags-candidate-v1")!=std::string::npos,
                "enabled below advance lacks its independent dispatch/provenance");check_complete_events(after);
        });
        run("above and below Drag options combine without dropping true peak contacts",[] {
            auto c=chart();for(int i=0;i<2;++i)note(c,NoteKind::drag);
            for(int i=0;i<2;++i)note(c,NoteKind::drag,second,second,0,NoteSide::below);
            for(int i=0;i<3;++i)note(c,NoteKind::tap);
            auto options=coincident_options();options.max_prejudge_ns=30'000'000;options.advance_above_drags=true;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& above=plan(original);
            require(above.peak_contacts==5,"above-only fixture lost its independent two below Drag contacts");
            options.max_contacts=4;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"capacity_exceeded"),
                "above-only candidate bypassed its actual five-contact peak");
            options.advance_below_drags=true;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& combined=plan(result);
            require(combined.peak_contacts==4&&combined.memory.contacts.size()==7&&combined.total_steps==14,
                "combined side policies dropped or shared a source to reduce peak");
            for(std::size_t i=0;i<7;++i){const auto& owner=combined.ownership[i];
                require(owner.judgment_ns==second&&owner.source_note_id==c.lines[0].notes[i].source.id,
                    "combined policies relabeled original native source times");
                if(i<4)require(owner.dispatch_judgment_ns==second-30'000'000,
                    "combined policy failed to advance one explicitly selected Drag side");
                if(i<2||i>=4)require(contact_owner_digest(above,i,above.metadata)==contact_owner_digest(combined,i,above.metadata),
                    "enabling below advance changed the existing above dispatch or Tap");
            }
            options.max_contacts=3;require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,
                "below policy bypassed the complete four-contact early peak");check_complete_events(combined);
        });
        run("below Drag early endpoint re-evaluates geometry and retains native fallback",[] {
            auto c=chart();auto& movement=c.lines[0].move_events[0];movement.time.end_ns=2*second;
            movement.start_x=.25;movement.end_x=.75;note(c,NoteKind::drag,second,second,0,NoteSide::below);
            auto options=coincident_options();options.max_prejudge_ns=30'000'000;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);options.advance_below_drags=true;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(result);
            const auto native=pixel_point(plan(original).memory.contacts[0].steps[0].point,1280,720);
            const auto early=pixel_point(after.memory.contacts[0].steps[0].point,1280,720);
            require(native.first==640&&early.first==630&&after.ownership[0].judgment_ns==second&&
                after.ownership[0].dispatch_judgment_ns==second-30'000'000,
                "below endpoint reused native coordinates instead of re-evaluating the earlier moving line");
            c=chart();auto hidden=c.lines[0].move_events[0];hidden.time.end_ns=second;hidden.start_x=hidden.end_x=3;
            auto visible=hidden;visible.time.start_ns=second;visible.time.end_ns=10*second;visible.start_x=visible.end_x=.5;
            c.lines[0].move_events={hidden,visible};note(c,NoteKind::drag,second,second,0,NoteSide::below);
            const auto fallback=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& restored=plan(fallback);
            require(restored.memory.contacts[0].steps[0].song_ns==second&&restored.ownership[0].dispatch_judgment_ns==second&&
                restored.ownership[0].judgment_ns==second&&restored.ownership[0].source_note_id==c.lines[0].notes[0].source.id,
                "unreachable below early geometry silently reused stale coordinates or lost native fallback");
        });
        run("below Drag advance rejects missing bounds and preserves time validation",[] {
            auto c=chart();note(c,NoteKind::drag,second,second,0,NoteSide::below);auto options=coincident_options();options.advance_below_drags=true;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),
                "below advance accepted an undeclared zero pre-judge window");
            options.max_prejudge_ns=30'000'001;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),
                "below advance exceeded the existing thirty-millisecond bound");
            options.max_prejudge_ns=30'000'000;c=chart();note(c,NoteKind::drag,20'000'000,20'000'000,0,NoteSide::below);
            const auto early=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(early).memory.contacts[0].steps[0].song_ns==20'000'000&&plan(early).ownership[0].judgment_ns==20'000'000,
                "below advance produced a negative dispatch before song zero");
            c=chart();const auto late=std::numeric_limits<SongNs>::max()-8'000'000;note(c,NoteKind::drag,late,late,0,NoteSide::below);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_time"),
                "below advance hid an invalid native duration overflow by moving it earlier");
        });
        run("closed below source filter advances only its named contact and retains empty legacy policies",[] {
            auto c=chart();note(c,NoteKind::drag);note(c,NoteKind::drag,2*second,2*second,0,NoteSide::below);
            note(c,NoteKind::drag,3*second,3*second,0,NoteSide::below);note(c,NoteKind::tap,4*second,4*second);
            note(c,NoteKind::hold,5*second,5*second+500'000'000);note(c,NoteKind::flick,6*second,6*second);
            auto options=sampled_options();options.flick_distance_reference_pixels=80;
            options.max_prejudge_ns=30'000'000;options.advance_above_drags=true;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& before=plan(original);
            options.advance_below_drags=true;
            const auto unfiltered=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& all=plan(unfiltered);
            options.below_drag_source_ids={c.lines[0].notes[1].source.id};
            const auto filtered=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(filtered);
            require(after.memory.contacts.size()==6&&after.total_steps==before.total_steps,"source filter dropped a contact or complete step");
            for(std::size_t i=0;i<6;++i){
                require(after.ownership[i].source_note_id==before.ownership[i].source_note_id&&
                    after.ownership[i].judgment_ns==before.ownership[i].judgment_ns,"filter changed original source or native judgment");
                if(i!=1)require(contact_owner_digest(before,i,before.metadata)==contact_owner_digest(after,i,before.metadata),
                    "source filter changed an unselected below Drag, above Drag, Tap, Hold or Flick");
            }
            for(std::size_t i=0;i<2;++i)require(after.memory.contacts[1].steps[i].song_ns==before.memory.contacts[1].steps[i].song_ns-30'000'000,
                "source filter did not advance both named Drag steps exactly30ms");
            require(all.memory.contacts[2].steps.front().song_ns==3*second-30'000'000&&
                after.memory.contacts[2].steps.front().song_ns==3*second,"nonempty filter still preferred an unnamed below Drag");
            require(after.metadata.judgment_policy_provenance.find("below-drag-source-filter-candidate-v1")!=std::string::npos&&
                all.metadata.judgment_policy_provenance.find("below-drag-source-filter-candidate-v1")==std::string::npos&&
                all.metadata.strategy.find("below-drag-source-filter-candidate-v1")==std::string::npos,
                "filtered provenance is absent or polluted the legacy all-below option");
            options.below_drag_source_ids.clear();
            const auto explicit_empty=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(explicit_empty).metadata.cache_sha256==all.metadata.cache_sha256,"empty filter changed existing all-below fingerprint");
            options.advance_below_drags=false;
            const auto old_false=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(old_false).metadata.cache_sha256==before.metadata.cache_sha256,"false plus empty changed original above-only fingerprint");
            require(compiled_contact_fingerprint(after)==after.metadata.cache_sha256&&after.metadata.cache_sha256!=all.metadata.cache_sha256,
                "explicit closed source set is missing from canonical identity");check_complete_events(after);
        });
        run("below source set is canonical but duplicate and unmatched declarations never disappear",[] {
            auto c=chart();note(c,NoteKind::drag,second,second,0,NoteSide::below);
            note(c,NoteKind::drag,2*second,2*second,0,NoteSide::below);
            auto options=sampled_options();options.max_prejudge_ns=30'000'000;options.advance_below_drags=true;
            const auto first=c.lines[0].notes[0].source.id,second_id=c.lines[0].notes[1].source.id;
            options.below_drag_source_ids={second_id,first};
            const auto reverse=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& a=plan(reverse);
            options.below_drag_source_ids={first,second_id};
            const auto forward=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& b=plan(forward);
            require(a.metadata.options.below_drag_source_ids==std::vector<std::string>{first,second_id}&&
                a.metadata.cache_sha256==b.metadata.cache_sha256,"source input ordering changed stored canonical set or fingerprint");
            auto permuted=b;std::reverse(permuted.metadata.options.below_drag_source_ids.begin(),permuted.metadata.options.below_drag_source_ids.end());
            require(compiled_contact_fingerprint(permuted)==b.metadata.cache_sha256,"fingerprint depends on set declaration order");
            for(const auto& ids:std::vector<std::vector<std::string>>{{first,first},{first,c.source.chart_sha256+"#/unknown"},
                    {std::string(64,'c')+"#/judgeLineList/0/notesAbove/0"},{""},{std::string(513,'x')}}){
                options.below_drag_source_ids=ids;
                require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_below_drag_source_filter"),
                    "duplicate, unknown, wrong-chart or unbounded filter declaration was silently ignored");
            }
            options.below_drag_source_ids={first};options.advance_below_drags=false;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),
                "filter enabled below advance without its explicit capability");
            options.advance_below_drags=true;c.lines[0].notes[0].source.json_pointer="/changed-source-pointer";
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_below_drag_source_filter"),
                "filter repaired a source identity which does not bind the original JSON pointer");
        });
        run("below source filter rejects wrong kind side repeated chart identity and finite budget overflow",[] {
            auto c=chart();note(c,NoteKind::drag);note(c,NoteKind::tap,2*second,2*second,0,NoteSide::below);
            note(c,NoteKind::hold,3*second,4*second,0,NoteSide::below);note(c,NoteKind::flick,5*second,5*second,0,NoteSide::below);
            note(c,NoteKind::drag,6*second,6*second,0,NoteSide::below);
            auto options=sampled_options();options.max_prejudge_ns=30'000'000;options.advance_below_drags=true;
            for(std::size_t i=0;i<4;++i){options.below_drag_source_ids={c.lines[0].notes[i].source.id};
                require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_below_drag_source_filter"),
                    "source filter accepted an above Drag or a non-Drag source");}
            options.below_drag_source_ids.assign(2049,c.lines[0].notes[4].source.id);
            const auto oversized=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(diagnostic(oversized,"invalid_below_drag_source_filter")&&std::any_of(oversized.diagnostics.begin(),oversized.diagnostics.end(),
                [](const auto& d){return d.message.find("2048")!=std::string::npos;}),"source filter has no explicit2048 budget");
            options.below_drag_source_ids={c.lines[0].notes[4].source.id};
            auto repeated=c.lines[0].notes[4];repeated.start_ns=repeated.end_ns=7*second;c.lines[0].notes.push_back(repeated);++c.note_count;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_below_drag_source_filter"),
                "closed source set matched multiple chart declarations without rejecting ambiguity");
        });
        run("named below early geometry is reevaluated and unreachable early geometry retains native fallback",[] {
            auto c=chart();auto& movement=c.lines[0].move_events[0];movement.time.end_ns=2*second;
            movement.start_x=.25;movement.end_x=.75;note(c,NoteKind::drag,second,second,0,NoteSide::below);
            auto options=sampled_options();options.max_prejudge_ns=30'000'000;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.advance_below_drags=true;options.below_drag_source_ids={c.lines[0].notes[0].source.id};
            const auto filtered=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& early=plan(filtered);
            require(pixel_point(plan(original).memory.contacts[0].steps[0].point,1280,720).first==640&&
                pixel_point(early.memory.contacts[0].steps[0].point,1280,720).first==630&&early.ownership[0].judgment_ns==second&&
                early.ownership[0].dispatch_judgment_ns==second-30'000'000,"named early Drag reused native rather than early pose");
            c=chart();auto hidden=c.lines[0].move_events[0];hidden.time.end_ns=second;hidden.start_x=hidden.end_x=3;
            auto visible=hidden;visible.time.start_ns=second;visible.time.end_ns=100*second;visible.start_x=visible.end_x=.5;
            c.lines[0].move_events={hidden,visible};note(c,NoteKind::drag,second,second,0,NoteSide::below);
            options.advance_below_drags=false;options.below_drag_source_ids.clear();
            const auto baseline=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.advance_below_drags=true;options.below_drag_source_ids={c.lines[0].notes[0].source.id};
            const auto fallback=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& restored=plan(fallback);
            require(restored.ownership[0].dispatch_judgment_ns==second&&
                contact_owner_digest(plan(baseline),0,plan(baseline).metadata)==contact_owner_digest(restored,0,plan(baseline).metadata),
                "selected unreachable early pose lost the existing native fallback or source proof");
            require(c.lines[0].notes[0].start_ns==second&&c.lines[0].notes[0].source.id==options.below_drag_source_ids[0],
                "source filter mutated the original chart");
        });
        run("unnamed below source preserves its existing native-geometry fallback",[] {
            auto c=chart();note(c,NoteKind::drag,second,second,0,NoteSide::below);
            auto other=c.lines[0];other.notes.clear();auto visible=other.move_events[0];visible.time.end_ns=2*second;
            auto hidden=visible;hidden.time.start_ns=2*second;hidden.time.end_ns=100*second;hidden.start_x=hidden.end_x=3;
            other.move_events={visible,hidden};auto fallback_note=c.lines[0].notes[0];fallback_note.start_ns=fallback_note.end_ns=2*second;
            fallback_note.source.json_pointer="/judgeLineList/1/notesBelow/0";
            fallback_note.source.id=c.source.chart_sha256+'#'+fallback_note.source.json_pointer;other.notes.push_back(fallback_note);
            c.lines.push_back(other);++c.note_count;auto options=sampled_options();options.max_prejudge_ns=30'000'000;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& before=plan(original);
            require(before.ownership[1].dispatch_judgment_ns==2*second-30'000'000,"fixture did not require old native-geometry fallback");
            options.advance_below_drags=true;options.below_drag_source_ids={c.lines[0].notes[0].source.id};
            const auto filtered=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(filtered);
            require(contact_owner_digest(before,1,before.metadata)==contact_owner_digest(after,1,before.metadata),
                "closed early-preference filter changed an unnamed source's preexisting geometry fallback");check_complete_events(after);
        });
        run("source filter accepts its closed512byte identity boundary",[] {
            auto c=chart();note(c,NoteKind::drag,second,second,0,NoteSide::below);
            auto& source=c.lines[0].notes[0].source;source.json_pointer='/'+std::string(446,'p');source.id=c.source.chart_sha256+'#'+source.json_pointer;
            require(source.id.size()==512,"source boundary fixture length changed");auto options=sampled_options();
            options.max_prejudge_ns=30'000'000;options.advance_below_drags=true;options.below_drag_source_ids={source.id};
            require(plan(compile_phigros_contacts(c,identity(c),geometry(),options)).ownership[0].source_note_id==source.id,
                "exact512byte original source identity was rejected");
        });
        run("closed above source filter changes only selected above Drag and keeps empty fingerprints",[] {
            auto c=chart();note(c,NoteKind::drag,second,second,0,NoteSide::below);
            note(c,NoteKind::drag,2*second,2*second);note(c,NoteKind::drag,3*second,3*second);
            note(c,NoteKind::tap,4*second,4*second);note(c,NoteKind::hold,5*second,5*second+500'000'000);
            note(c,NoteKind::flick,6*second,6*second);
            auto options=sampled_options();options.max_prejudge_ns=30'000'000;options.advance_below_drags=true;
            const auto baseline=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& before=plan(baseline);
            options.advance_above_drags=true;
            const auto all_result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& all=plan(all_result);
            options.above_drag_source_ids={c.lines[0].notes[1].source.id};
            const auto filtered=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(filtered);
            require(after.memory.contacts.size()==6&&after.total_steps==before.total_steps,"above filter dropped original notes or steps");
            for(std::size_t i=0;i<6;++i) {
                require(after.ownership[i].source_note_id==before.ownership[i].source_note_id&&
                    after.ownership[i].judgment_ns==before.ownership[i].judgment_ns,"above filter rewrote immutable source/judgment");
                if(i!=1)require(contact_owner_digest(before,i,before.metadata)==contact_owner_digest(after,i,before.metadata),
                    "above filter changed unselected above Drag, below Drag, Tap, Hold or Flick");
            }
            for(std::size_t i=0;i<2;++i)require(after.memory.contacts[1].steps[i].song_ns==before.memory.contacts[1].steps[i].song_ns-30'000'000,
                "selected above Drag did not retain both steps and its original dwell");
            require(all.memory.contacts[2].steps.front().song_ns==3*second-30'000'000&&
                after.memory.contacts[2].steps.front().song_ns==3*second,"above filter still advanced an unspecified reachable source");
            require(after.metadata.strategy.find("above-drag-source-filter-candidate-v1")!=std::string::npos&&
                after.metadata.judgment_policy_provenance.find("above-drag-source-filter-candidate-v1")!=std::string::npos&&
                all.metadata.strategy.find("above-drag-source-filter-candidate-v1")==std::string::npos&&
                all.metadata.judgment_policy_provenance.find("above-drag-source-filter-candidate-v1")==std::string::npos,
                "nonempty above-filter provenance missing or empty legacy policy polluted");
            options.above_drag_source_ids.clear();
            require(plan(compile_phigros_contacts(c,identity(c),geometry(),options)).metadata.cache_sha256==all.metadata.cache_sha256,
                "explicit empty filter changed archived all-above fingerprint");
            options.advance_above_drags=false;
            require(plan(compile_phigros_contacts(c,identity(c),geometry(),options)).metadata.cache_sha256==before.metadata.cache_sha256,
                "false plus empty above filter changed default fingerprint");
            auto declared=before;declared.metadata.options.above_drag_source_ids={c.lines[0].notes[1].source.id};
            require(compiled_contact_fingerprint(declared)!=before.metadata.cache_sha256,"nonempty above subset is absent from canonical identity");
            declared.metadata.options.above_drag_source_ids.clear();
            require(compiled_contact_fingerprint(declared)==before.metadata.cache_sha256,"empty above subset emitted a new fingerprint token");
            check_complete_events(after);
        });
        run("above source filter canonicalizes declarations and rejects malformed or nonabove Drag identity",[] {
            auto c=chart();note(c,NoteKind::drag);note(c,NoteKind::drag,2*second,2*second);
            note(c,NoteKind::drag,3*second,3*second,0,NoteSide::below);note(c,NoteKind::tap,4*second,4*second);
            note(c,NoteKind::hold,5*second,6*second);note(c,NoteKind::flick,7*second,7*second);
            auto options=sampled_options();options.max_prejudge_ns=30'000'000;options.advance_above_drags=true;
            const auto first=c.lines[0].notes[0].source.id,second_id=c.lines[0].notes[1].source.id;
            options.above_drag_source_ids={second_id,first};
            const auto reverse=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& a=plan(reverse);
            options.above_drag_source_ids={first,second_id};
            const auto forward=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& b=plan(forward);
            require(a.metadata.options.above_drag_source_ids==std::vector<std::string>{first,second_id}&&a.metadata.cache_sha256==b.metadata.cache_sha256,
                "above input set order changed canonical metadata or fingerprint");
            auto permuted=b;std::reverse(permuted.metadata.options.above_drag_source_ids.begin(),permuted.metadata.options.above_drag_source_ids.end());
            require(compiled_contact_fingerprint(permuted)==b.metadata.cache_sha256,"above fingerprint depends on input set order");
            for(const auto& ids:std::vector<std::vector<std::string>>{{first,first},{first,c.source.chart_sha256+"#/unknown"},
                    {std::string(64,'c')+"#/judgeLineList/0/notesAbove/0"},{""},{std::string(513,'x')}}) {
                options.above_drag_source_ids=ids;
                require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_above_drag_source_filter"),
                    "above duplicate/unknown/wrong-chart/empty/unbounded source was ignored");
            }
            for(std::size_t i=2;i<6;++i) {
                options.above_drag_source_ids={c.lines[0].notes[i].source.id};
                require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_above_drag_source_filter"),
                    "above filter accepted a below Drag or a Tap/Hold/Flick");
            }
            options.above_drag_source_ids={first};options.advance_above_drags=false;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"above filter secretly enabled its advance bool");
            options.advance_above_drags=true;options.above_drag_source_ids.assign(2049,first);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_above_drag_source_filter"),"above source filter exceeded2048");
            options.above_drag_source_ids={first};auto repeated=c.lines[0].notes[0];repeated.start_ns=repeated.end_ns=8*second;
            c.lines[0].notes.push_back(repeated);++c.note_count;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_above_drag_source_filter"),"above source ambiguity was silently repaired");
            c.lines[0].notes.pop_back();--c.note_count;c.lines[0].notes[0].source.json_pointer="/changed-source-pointer";
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_above_drag_source_filter"),"above filter failed to bind original JSON pointer");
        });
        run("above source subset reevaluates early geometry and retains native fallback",[] {
            auto c=chart();auto& movement=c.lines[0].move_events[0];movement.time.end_ns=2*second;
            movement.start_x=.25;movement.end_x=.75;note(c,NoteKind::drag);
            auto options=sampled_options();options.max_prejudge_ns=30'000'000;
            const auto native=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.advance_above_drags=true;options.above_drag_source_ids={c.lines[0].notes[0].source.id};
            const auto early=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(pixel_point(plan(native).memory.contacts[0].steps[0].point,1280,720).first==640&&
                pixel_point(plan(early).memory.contacts[0].steps[0].point,1280,720).first==630&&
                plan(early).ownership[0].judgment_ns==second&&plan(early).ownership[0].dispatch_judgment_ns==second-30'000'000,
                "above named early source reused native geometry or rewrote judgment");
            c=chart();auto hidden=c.lines[0].move_events[0];hidden.time.end_ns=second;hidden.start_x=hidden.end_x=3;
            auto visible=hidden;visible.time.start_ns=second;visible.time.end_ns=100*second;visible.start_x=visible.end_x=.5;
            c.lines[0].move_events={hidden,visible};note(c,NoteKind::drag);
            options.advance_above_drags=false;options.above_drag_source_ids.clear();
            const auto baseline=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.advance_above_drags=true;options.above_drag_source_ids={c.lines[0].notes[0].source.id};
            const auto fallback=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(fallback).ownership[0].dispatch_judgment_ns==second&&
                contact_owner_digest(plan(baseline),0,plan(baseline).metadata)==contact_owner_digest(plan(fallback),0,plan(baseline).metadata),
                "above source filter lost original native fallback");
            require(c.lines[0].notes[0].start_ns==second&&c.lines[0].notes[0].source.id==options.above_drag_source_ids[0],"above filter mutated chart source");
        });
        run("sampled diagonal v2 retains five exact commands and a complete three-MOVE certificate",[] {
            auto c=chart();note(c,NoteKind::flick);note(c,NoteKind::tap,2*second,2*second);
            note(c,NoteKind::drag,3*second,3*second);note(c,NoteKind::hold,4*second,5*second);
            auto options=sampled_options();auto old_options=options;old_options.flick_path_policy=FlickPathPolicy::judgment_anchored_diagonal_snap_v1;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),old_options);const auto& before=plan(original);
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(result);
            require(static_cast<int>(FlickPathPolicy::judgment_anchored_diagonal_snap_v1)==3&&
                static_cast<int>(FlickPathPolicy::judgment_anchored_diagonal_sampled_v2)==4,"new enum renumbered an archived Flick policy");
            const auto& steps=after.memory.contacts[0].steps;const auto& owner=after.ownership[0];
            require(steps.size()==5&&steps[0].phase==Phase::down&&steps[1].phase==Phase::move&&
                steps[2].phase==Phase::move&&steps[3].phase==Phase::move&&steps[4].phase==Phase::up,
                "sampled v2 dropped/aliased a discrete MOVE or phase");
            const SongNs expected[]={second-8'000'000,second+4'000'000,second+16'000'000,second+28'000'000,second+52'000'000};
            for(std::size_t i=0;i<5;++i)require(steps[i].song_ns==expected[i],"sampled v2 timing differs from8/36/24");
            require(owner.flick_discrete_path&&owner.flick_discrete_path->middle&&owner.flick_anchor_reference&&owner.flick_direction_reference,
                "sampled v2 lacks its explicit middle/anchor/direction certificate");
            const auto& proof=*owner.flick_discrete_path;const auto& middle=*proof.middle;const auto anchor=*owner.flick_anchor_reference;
            require(proof.model_id=="judgment-anchored-diagonal-sampled-v2"&&proof.move_count==3&&
                proof.bend_ns==expected[1]&&middle.song_ns==expected[2]&&proof.end_ns==expected[3],
                "sampled v2 certificate policy or all three vertex times differ");
            const auto length=[](PosePoint a,PosePoint b){return std::hypot(a.x-b.x,a.y-b.y);};
            require(std::abs(length(anchor,proof.end_reference)-96)<1e-9&&
                std::abs(proof.endpoint_distance_reference_pixels-96)<1e-9&&
                std::abs(proof.polyline_length_reference_pixels-(length(anchor,proof.bend_reference)+
                    length(proof.bend_reference,middle.reference)+length(middle.reference,proof.end_reference)))<1e-9,
                "sampled v2 reference distance or full four-vertex polyline is false");
            require(length(proof.bend_reference,middle.reference)/12>=2.8&&length(middle.reference,proof.end_reference)/12>=2.8,
                "sampled v2 middle legs lost their requested nominal reference-pixel speed");
            double maximum_jump=0;for(std::size_t i=1;i<4;++i){const auto a=pixel_point(steps[i-1].point,1280,720),b=pixel_point(steps[i].point,1280,720);
                maximum_jump=std::max(maximum_jump,std::hypot(double(b.first-a.first),double(b.second-a.second)));}
            require(proof.max_jump_input_pixels==maximum_jump&&steps[4].point.x==steps[3].point.x&&steps[4].point.y==steps[3].point.y,
                "sampled v2 maximum jump or held UP endpoint differs from commanded raster vertices");
            require(owner.judgment_ns==second&&owner.source_note_id==c.lines[0].notes[0].source.id&&
                owner.normal_projection_spans.size()==1&&owner.normal_projection_spans[0].begin_ns==second&&
                owner.normal_projection_spans[0].end_ns==second,"sampled v2 changed native anchor/source provenance");
            require(after.memory.contacts.size()==4&&after.ownership.size()==4&&after.total_steps==before.total_steps+1,
                "sampled v2 dropped a source or did not add exactly one Flick MOVE");
            for(std::size_t i=1;i<4;++i)require(contact_owner_digest(before,i,before.metadata)==contact_owner_digest(after,i,before.metadata),
                "sampled v2 changed a non-Flick contact or complete ownership");
            require(after.metadata.cache_sha256!=before.metadata.cache_sha256&&diagnostic(result,"unverified_discrete_diagonal_sampled")&&
                after.metadata.judgment_policy_provenance.find("three curved MOVE")!=std::string::npos,
                "sampled v2 omitted its distinct fingerprint or honest discrete candidate provenance");
            inside_inset(after);check_complete_events(after);
        });
        run("sampled middle evidence is conditional and old v1 absence preserves the archived transcript",[] {
            auto c=chart();note(c,NoteKind::flick);auto options=sampled_options();
            auto old_options=options;old_options.flick_path_policy=FlickPathPolicy::judgment_anchored_diagonal_snap_v1;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),old_options);const auto& before=plan(original);
            require(!before.ownership[0].flick_discrete_path->middle,"v1 unexpectedly gained middle-vertex evidence");
            auto copy=before;copy.ownership[0].flick_discrete_path->middle=std::nullopt;
            require(compiled_contact_fingerprint(copy)==before.metadata.cache_sha256,"explicitly absent middle changed v1 fingerprint bytes");
            copy.ownership[0].flick_discrete_path->middle=FlickDiscreteMiddleVertex{{0,0},0};
            require(compiled_contact_fingerprint(copy)!=before.metadata.cache_sha256,"present zero-valued middle evidence was not fingerprinted");
            const auto sampled=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(sampled);
            copy=after;copy.ownership[0].flick_discrete_path->middle->song_ns+=1;
            require(compiled_contact_fingerprint(copy)!=after.metadata.cache_sha256,"sampled middle time was not fingerprinted");
            copy=after;copy.ownership[0].flick_discrete_path->middle->reference.x+=1;
            require(compiled_contact_fingerprint(copy)!=after.metadata.cache_sha256,"sampled middle position was not fingerprinted");
            copy=after;copy.ownership[0].flick_discrete_path->middle.reset();
            require(compiled_contact_fingerprint(copy)!=after.metadata.cache_sha256,"sampled middle evidence could be removed without identity change");
        });
        run("sampled v2 fits complete curved paths within normal and rectangular domains",[] {
            auto options=sampled_options();
            for(const double angle:{0.,30.,90.,135.})for(const double x:{64.,1215.})for(const double y:{64.,655.}){
                auto c=chart();auto& m=c.lines[0].move_events[0];m.start_x=m.end_x=x/1280;m.start_y=m.end_y=1-y/720;
                c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=angle;note(c,NoteKind::flick);
                const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);inside_inset(plan(result));
                require(plan(result).memory.contacts[0].steps.size()==5,"sampled edge fit added dense moves or clipped a vertex");
            }
            auto c=chart();note(c,NoteKind::flick);c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=1.05;
            options.offscreen_policy=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;options.max_tangent_error_reference_pixels=151.2;
            const auto rectangle=compile_phigros_contacts(c,identity(c),geometry(),options);inside_inset(plan(rectangle));
            require(plan(rectangle).ownership[0].tangent_projection_min_reference_pixels<0,"sampled rectangular fit did not record its tangent correction");
            options.max_tangent_error_reference_pixels=50;
            // Native x=1344 already needs -129px to reach inset x<=1215.
            // The usable tangent budget is only50-4.000001px, so the common
            // judgment-target preflight must reject before the curve solver.
            const auto unreachable=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!unreachable.plan&&diagnostic(unreachable,"judgment_domain_unreachable"),
                "sampled path silently clipped an unreachable declared judgment domain");
        });
        run("sampled v2 uses exact integer timing and rejects insufficient post-time and overflow",[] {
            auto c=chart();note(c,NoteKind::flick);auto options=sampled_options();options.flick_travel_ns+=3;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& steps=plan(result).memory.contacts[0].steps;
            require(steps[1].song_ns==second+4'000'000&&steps[2].song_ns==second+16'000'001&&steps[3].song_ns==second+28'000'003,
                "sampled second time used(post/7)*4 instead of floor(post*4/7)");
            options.flick_travel_ns=options.flick_lead_ns+6;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"diagonal_sampled_resolution"),
                "sampled v2 accepted coincident or unrepresentable vertex times");
            options.flick_travel_ns=std::numeric_limits<SongNs>::max()/2;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_time"),"sampled post*4 overflow was accepted");
            c=chart();const auto t=std::numeric_limits<SongNs>::max()-40'000'000;note(c,NoteKind::flick,t,t);options=sampled_options();
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_time"),"sampled UP addition overflow was accepted");
        });
        run("sampled v2 keeps complete source capacity and finite five-step budgets",[] {
            auto c=chart();note(c,NoteKind::flick);note(c,NoteKind::flick);note(c,NoteKind::tap);auto options=sampled_options();options.max_contacts=2;
            const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!rejected.plan&&diagnostic(rejected,"capacity_exceeded")&&rejected.required_peak_contacts==3,
                "sampled v2 skipped a coincident source to fit pointer capacity");
            options.max_contacts=3;const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(result).memory.contacts.size()==3&&plan(result).total_steps==12&&plan(result).peak_contacts==3,
                "sampled v2 complete source/step peak differs");check_complete_events(plan(result));
            c=chart();note(c,NoteKind::flick);options=sampled_options();options.max_steps_per_contact=4;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"trajectory_budget"),"sampled v2 bypassed four-step contact budget");
            options=sampled_options();options.max_steps_in_plan=4;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"plan_step_budget"),"sampled v2 bypassed full-song step budget");
            options=sampled_options();auto tiny=geometry();tiny.input_width=tiny.input_height=10;
            require(diagnostic(compile_phigros_contacts(c,identity(c),tiny,options),"diagonal_sampled_raster_resolution"),
                "sampled v2 claimed three distinct curved MOVE vertices below raster resolution");
            options.coincident_flick_delay_ns=32'000'000;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"coincident_flick_requires_discrete_path"),
                "old coincident32 v1 policy silently shifted a sampled v2 contact");
        });
        run("sampled v2 pre-judge retains its middle dispatch time and immutable native source",[] {
            auto c=chart();auto initial=c.lines[0].move_events[0];initial.time.end_ns=second;
            auto later=initial;later.time.start_ns=second;later.time.end_ns=10*second;later.start_x=later.end_x=3;
            c.lines[0].move_events={initial,later};note(c,NoteKind::flick);auto options=sampled_options();options.max_prejudge_ns=30'000'000;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);const auto& o=p.ownership[0];
            require(o.judgment_ns==second&&o.dispatch_judgment_ns==second-30'000'000&&o.flick_discrete_path->middle&&
                o.flick_discrete_path->middle->song_ns==second-14'000'000&&p.memory.contacts[0].steps.size()==5,
                "sampled pre-judge lost middle certificate, native judgment, or the complete five-step path");
            inside_inset(p);check_complete_events(p);
        });
        run("coincident discrete Flick handoff preserves every source and all non-Flick contacts",[] {
            auto c=chart();note(c,NoteKind::tap);note(c,NoteKind::flick);note(c,NoteKind::flick,second,second,2);
            note(c,NoteKind::hold,2*second,2*second+500'000'000);note(c,NoteKind::flick,2*second,2*second,-2);
            note(c,NoteKind::drag,3*second,3*second);note(c,NoteKind::flick,3*second,3*second,3,NoteSide::below);
            auto options=coincident_options();
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& before=plan(original);
            options.coincident_flick_delay_ns=32'000'000;
            const auto compiled=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(compiled);
            require(before.memory.contacts.size()==7&&after.memory.contacts.size()==7&&after.ownership.size()==7&&
                before.total_steps==after.total_steps,"handoff dropped, shared, or duplicated source notes/steps");
            for(std::size_t i=0;i<after.ownership.size();++i){
                if(before.ownership[i].kind==NoteKind::flick)check_coincident_shift(before,after,i);
                else require(contact_owner_digest(before,i,before.metadata)==contact_owner_digest(after,i,before.metadata),
                    "coincident handoff changed a Tap/Hold/Drag contact or ownership field");
                require(after.ownership[i].source_note_id==c.lines[0].notes[i].source.id&&after.memory.contacts[i].id==before.memory.contacts[i].id,
                    "paired Flicks lost independent original source ownership");
            }
            require(before.peak_contacts==3&&after.peak_contacts==2&&compiled.required_peak_contacts==2,
                "handoff inherited pre-shift peak rather than recomputing real overlap");
            require(after.metadata.requires_calibration==before.metadata.requires_calibration&&
                after.metadata.cache_sha256!=before.metadata.cache_sha256&&diagnostic(compiled,"unverified_coincident_flick_delay")&&
                after.metadata.judgment_policy_provenance.find("coincident-flick-delay32-candidate-v1")!=std::string::npos,
                "handoff geometry qualification, explicit timing identity, or honest provenance changed");
            require(compiled_contact_fingerprint(after)==after.metadata.cache_sha256,"handoff stored cache differs from the canonical complete plan");
            check_complete_events(after);
            auto changed=after;changed.metadata.options.coincident_flick_delay_ns=0;
            require(compiled_contact_fingerprint(changed)!=after.metadata.cache_sha256,"handoff option not fingerprinted");
        });
        run("coincident handoff groups original Drag heads after pre-judge restoration",[] {
            auto c=chart();note(c,NoteKind::drag);note(c,NoteKind::flick);
            auto options=coincident_options();options.drag_duration_ns=16'000'000;
            options.max_prejudge_ns=30'000'000;options.advance_above_drags=true;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& before=plan(original);
            require(before.ownership[0].judgment_ns==second&&before.ownership[0].dispatch_judgment_ns==second-30'000'000,
                "fixture did not advance its Drag separately from native judgment");
            options.coincident_flick_delay_ns=32'000'000;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(result);
            check_coincident_shift(before,after,1);
            require(contact_owner_digest(before,0,before.metadata)==contact_owner_digest(after,0,before.metadata),
                "Flick handoff changed early Drag DOWN/UP, geometry, or source ownership");
            require(after.memory.contacts[1].steps.front().song_ns==second+24'000'000,
                "handoff used the advanced Drag dispatch time instead of the shared native head");
            check_complete_events(after);
        });
        run("solo Flicks Flick-only pairs and one-nanosecond near peers remain unchanged",[] {
            for(int shape:{0,1,2}){
                auto c=chart();note(c,NoteKind::flick);
                if(shape==1)note(c,NoteKind::flick);
                if(shape==2)note(c,NoteKind::tap,second+1,second+1);
                auto options=coincident_options();const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);
                const auto& before=plan(original);options.coincident_flick_delay_ns=32'000'000;
                const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& after=plan(result);
                for(std::size_t i=0;i<before.ownership.size();++i)
                    require(contact_owner_digest(before,i,before.metadata)==contact_owner_digest(after,i,before.metadata),
                        "nonmatching source group received an undeclared timing/ownership change");
                require(before.metadata.cache_sha256!=after.metadata.cache_sha256,"unused explicit handoff policy reused default fingerprint");
                check_complete_events(after);
            }
        });
        run("coincident handoff explicitly rejects already early Flick geometry only when it has a native peer",[] {
            auto c=chart();note(c,NoteKind::flick);note(c,NoteKind::tap);
            auto visible=c.lines[0];visible.notes.clear();c.lines.push_back(visible);
            auto tap=c.lines[0].notes.back();c.lines[0].notes.pop_back();
            tap.source.json_pointer="/judgeLineList/1/notesAbove/0";tap.source.id=c.source.chart_sha256+'#'+tap.source.json_pointer;
            c.lines[1].notes.push_back(tap);
            auto first=c.lines[0].move_events[0];first.time.end_ns=second;
            auto later=first;later.time.start_ns=second;later.time.end_ns=10*second;later.start_x=later.end_x=3;
            c.lines[0].move_events={first,later};
            auto options=coincident_options();options.max_prejudge_ns=30'000'000;
            options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;
            options.calibration_id="coincident-prejudge-conflict";
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(original).ownership[0].dispatch_judgment_ns==second-30'000'000,"fixture did not require early Flick geometry");
            options.coincident_flick_delay_ns=32'000'000;
            const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!rejected.plan&&diagnostic(rejected,"coincident_flick_prejudge_conflict"),
                "already early Flick was silently delayed using an unproved native anchor");
            c.lines[1].notes[0].start_ns+=1;c.lines[1].notes[0].end_ns+=1;
            options.coincident_flick_delay_ns=0;
            const auto unmatched=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.coincident_flick_delay_ns=32'000'000;
            const auto allowed=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(contact_owner_digest(plan(unmatched),0,plan(unmatched).metadata)==
                contact_owner_digest(plan(allowed),0,plan(unmatched).metadata),"solo pre-judged Flick was changed or globally rejected");
        });
        run("coincident handoff validates post-shift capacity including the ten-contact ceiling",[] {
            for(const int capacity:{2,10}){
                auto c=chart();note(c,NoteKind::tap);note(c,NoteKind::flick);
                for(int i=0;i<capacity;++i)note(c,NoteKind::hold,second+40'000'000,second+200'000'000,i*.1);
                auto options=coincident_options();options.max_contacts=capacity;
                const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);
                require(plan(original).peak_contacts==capacity,"capacity fixture did not fit before the Flick shift");
                options.coincident_flick_delay_ns=32'000'000;
                const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),options);
                require(!rejected.plan&&diagnostic(rejected,"capacity_exceeded")&&rejected.required_peak_contacts==capacity+1,
                    "shifted Flick bypassed its new overlap or the ten-pointer maximum");
            }
        });
        run("coincident handoff checked addition rejects a source-safe plan that would overflow only after delay",[] {
            auto c=chart();const auto native=std::numeric_limits<SongNs>::max()-50'000'000;
            note(c,NoteKind::tap,native,native);note(c,NoteKind::flick,native,native);
            auto options=coincident_options();
            require(compile_phigros_contacts(c,identity(c),geometry(),options).plan.has_value(),"overflow fixture already failed before handoff");
            options.coincident_flick_delay_ns=32'000'000;
            const auto rejected=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!rejected.plan&&diagnostic(rejected,"invalid_time"),"post-shift Flick UP overflow was accepted");
            require(c.lines[0].notes[1].start_ns==native,"failed delay mutated the native input chart");
        });
        run("coincident handoff option is discrete bounded opt-in and zero preserves archived fingerprints",[] {
            auto c=chart();note(c,NoteKind::tap);ContactCompileOptions options;
            const auto original=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.coincident_flick_delay_ns=0;
            const auto zero=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(zero).metadata.cache_sha256==plan(original).metadata.cache_sha256&&
                plan(zero).metadata.cache_sha256==v2_golden(
                    "2a9e0c0b1a909416bd2a440175bb5ccf96a9dae9a2a68aae3a449026df9305b7",
                    "b5e1a3a49232589c072a7fa5d433b523bc2112e5cec71b037be3347da451468b"),
                "default/explicit-zero handoff changed the archived v2 fingerprint");
            for(const SongNs invalid:{SongNs{-1},SongNs{1},SongNs{31'999'999},SongNs{32'000'001},std::numeric_limits<SongNs>::max()}){
                options.coincident_flick_delay_ns=invalid;
                require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"invalid handoff delay accepted");
            }
            options.coincident_flick_delay_ns=32'000'000;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"coincident_flick_requires_discrete_path"),
                "dynamic continuous Flick policy was silently reused for discrete handoff");
        });
        run("prejudge and normal domain limits remain explicit finite and bounded",[] {
            auto c=chart();note(c,NoteKind::drag);ContactCompileOptions options;
            options.max_prejudge_ns=30'000'001;require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"prejudge exceeded30ms");
            options.max_prejudge_ns=0;options.advance_above_drags=true;require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"above advance lacks explicit window");
            options.advance_above_drags=false;options.max_normal_projection_reference_pixels=1'000'001;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"normal resource domain exceeded1e6");
            options.max_normal_projection_reference_pixels=100000;options.offscreen_policy=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
            options.max_tangent_error_reference_pixels=151.2;options.calibration_id="large-normal-finite";
            c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=90;
            require(compile_phigros_contacts(c,identity(c),geometry(),options).plan.has_value(),"finite large normal offset incorrectly changes input reachability");
        });
        run("Hold reacquire candidate certifies short tangent gaps with continuous pointer and recovery",[] {
            auto c=chart();auto initial=c.lines[0].move_events[0];initial.time.end_ns=second;
            auto bad=initial;bad.time.start_ns=second;bad.time.end_ns=second+10'000'000;bad.start_x=bad.end_x=-.2;
            auto recovered=initial;recovered.time.start_ns=bad.time.end_ns;recovered.time.end_ns=10*second;
            c.lines[0].move_events={initial,bad,recovered};note(c,NoteKind::hold,second/2,2*second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
            options.max_tangent_error_reference_pixels=151.2;options.max_normal_projection_reference_pixels=7000;
            options.normal_projection_inset_reference_pixels=64;options.calibration_id="hold-reacquire-unqualified";
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"unreachable tangent gap accepted without opt-in");
            options.max_hold_reacquire_gap_ns=20'000'000;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"reacquire candidate omitted explicit tracking-end/tail policy");
            options.hold_tail_policy=HoldTailPolicy::phira_tail_grace_candidate_v1;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
            const auto& owner=p.ownership[0];SongNs total=0;
            for(const auto& span:owner.normal_projection_spans)if(span.judgment_domain_gap) {
                total+=span.end_ns-span.begin_ns;
                require(span.end_ns-span.begin_ns<=1'000'000&&span.begin_ns>=second&&span.end_ns<=second+10'000'000,"gap coverage not conservatively localized");
            }
            require(total==10'000'000&&owner.gap_tangent_projection_max_reference_pixels>=320&&
                owner.tangent_projection_max_reference_pixels<=151.2,"gap errors polluted valid-domain certificate or lost actual range");
            const auto& steps=p.memory.contacts[0].steps;
            require(steps.front().phase==Phase::down&&steps.front().song_ns==second/2&&steps.back().phase==Phase::up&&steps.back().song_ns==2*second,"reacquire released pointer or shortened native Hold");
            require(std::count_if(steps.begin(),steps.end(),[](const auto& s){return s.phase==Phase::down;})==1&&
                diagnostic(result,"unverified_hold_reacquire_gap"),"reacquire created another DOWN or lost unqualified warning");
            auto changed=p;changed.ownership[0].normal_projection_spans[0].judgment_domain_gap=true;
            require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"gap flags not fingerprinted");
            c.lines[0].move_events[1].time.end_ns=second+30'000'000;c.lines[0].move_events[2].time.start_ns=second+30'000'000;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"hold_reacquire_gap_exceeded"),"thirty-ms merged gap bypassed20ms bound");
            c.lines[0].move_events[1].time.end_ns=3*second;c.lines[0].move_events[2].time.start_ns=3*second;
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"unrecovered gap reached native UP");
            options.max_hold_reacquire_gap_ns=20'000'001;require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"gap limit exceeded20ms");
        });
        run("judgment rectangle is explicit bounded source traceable and fingerprinted",[] {
            auto c=chart();c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=-36./1280;note(c,NoteKind::tap);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;options.calibration_id="judgment-domain-unqualified";
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"normal policy silently accepted nonzero tangent error");
            options.offscreen_policy=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;options.max_tangent_error_reference_pixels=151.2;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
            require(p.ownership.size()==1&&p.ownership[0].source_note_id==c.lines[0].notes[0].source.id,"judgment projection dropped source note");
            require(std::abs(p.ownership[0].tangent_projection_min_reference_pixels-100)<1e-10&&
                std::abs(p.ownership[0].tangent_projection_max_reference_pixels-100)<1e-10,"actual tangent correction not recorded");
            require(p.ownership[0].normal_projection_spans[0].tangent_correction_lower_reference_pixels==100,"point tangent certificate missing");
            require(p.metadata.projection_policy_version=="bounded_judgment_rectangle_candidate_v1"&&
                diagnostic(result,"unverified_judgment_rectangle")&&!p.metadata.judgment_policy_provenance.empty(),"judgment projection missing explicit unqualified provenance");
            auto changed=p;changed.ownership[0].normal_projection_spans[0].tangent_correction_upper_reference_pixels+=1;
            require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"tangent continuous certificate not fingerprinted");
            changed=p;changed.metadata.options.max_tangent_error_reference_pixels=150;
            require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"tangent domain bound not fingerprinted");
            c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=-100./1280;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"judgment_domain_unreachable"),"judgment projection allowed tangent error above151.2");
            c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=-86./1280;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"judgment_domain_unreachable"),"rectangle silently added held/raster error beyond the declared151.2px domain");
            c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=12;note(c,NoteKind::tap);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"judgment_domain_unreachable"),"judgment projection exceeded finite normal bound");
        });
        run("judgment rectangle continuous moving rotating Hold has quantitative held error proof",[] {
            auto c=chart();auto& m=c.lines[0].move_events[0];m.time.end_ns=2*second;
            m.start_x=-.04;m.end_x=.15;m.start_y=.5;m.end_y=.8;
            c.lines[0].rotate_events[0].time.end_ns=2*second;c.lines[0].rotate_events[0].end=160;
            note(c,NoteKind::hold,0,2*second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
            options.max_tangent_error_reference_pixels=151.2;options.max_normal_projection_reference_pixels=7000;
            options.normal_projection_inset_reference_pixels=64;options.calibration_id="continuous-judgment-domain-unqualified";
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
            const auto& owner=p.ownership[0];
            require(!owner.normal_projection_spans.empty()&&owner.tangent_projection_max_reference_pixels>100,"continuous tangent certificate lost excursion");
            const PhigrosPoseEvaluator poses(c);const auto& steps=p.memory.contacts[0].steps;double worst=0;
            for(SongNs time=0;time<2*second;time+=500'003) {
                const auto native=poses.note_target(0,c.lines[0].notes[0],time).reference_pixels;
                const PosePoint wanted{std::clamp(native.x,64.,1215.),std::clamp(native.y,64.,655.)};
                const auto at=std::upper_bound(steps.begin(),steps.end(),time,[](SongNs t,const auto& s){return t<s.song_ns;});
                const auto actual=pixel_point(std::prev(at)->point,1280,720);
                worst=std::max(worst,std::hypot(actual.first-wanted.x,actual.second-wanted.y));
                const auto pose=poses.line_at(0,time);const PosePoint delta{wanted.x-native.x,wanted.y-native.y};
                const double tangent=delta.x*pose.tangent.x+delta.y*pose.tangent.y;
                require(std::abs(tangent)<=151.2,"test oracle left declared tangent domain");
                const auto certificate=std::find_if(owner.normal_projection_spans.begin(),owner.normal_projection_spans.end(),[&](const auto& s){return s.begin_ns<=time&&time<=s.end_ns;});
                require(certificate!=owner.normal_projection_spans.end()&&tangent>=certificate->tangent_correction_lower_reference_pixels-1e-8&&
                    tangent<=certificate->tangent_correction_upper_reference_pixels+1e-8,"continuous tangent bound failed independent dense oracle");
            }
            require(worst<=options.max_path_error_pixels+1e-7,"judgment rectangle violated discrete-held error budget");
            require(steps.front().song_ns==0&&steps.back().song_ns==2*second,"judgment rectangle changed full Hold lifetime");
        });
        run("judgment rectangle rejects hidden interior excursion and invalid domain options",[] {
            auto c=chart();c.lines[0].rotate_events[0].time.end_ns=2*second;c.lines[0].rotate_events[0].end=180;note(c,NoteKind::hold,0,2*second,10);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
            options.max_tangent_error_reference_pixels=151.2;options.max_normal_projection_reference_pixels=7000;
            options.normal_projection_inset_reference_pixels=64;options.calibration_id="negative-judgment-domain";
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!result.plan&&diagnostic(result,"judgment_domain_unreachable"),"valid rectangle endpoints concealed unreachable continuous interior");
            c=chart();note(c,NoteKind::tap);
            for(double bad:{-1.,0.,151.200001,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
                auto invalid=options;invalid.max_tangent_error_reference_pixels=bad;
                require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),invalid),"invalid_options"),"invalid tangent domain bound accepted");
            }
            options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"tangent tolerance silently ignored by normal-only policy");
        });
        run("screen fit rectangle declares nonzero tangent anchor and full stroke",[] {
            auto c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1-30./720;
            c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=90;note(c,NoteKind::flick);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_judgment_rectangle_candidate_v1;
            options.max_tangent_error_reference_pixels=151.2;options.max_normal_projection_reference_pixels=7000;
            options.normal_projection_inset_reference_pixels=64;options.calibration_id="screen-fit-rectangle-unqualified";
            options.flick_path_policy=FlickPathPolicy::judgment_anchored_screen_fit_v1;options.flick_distance_reference_pixels=72;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);inside_inset(p);
            require(std::abs(p.ownership[0].tangent_projection_min_reference_pixels+34)<1e-9,"screen fit concealed its nonzero tangent correction");
            const auto& steps=p.memory.contacts[0].steps;const auto first=pixel_point(steps.front().point,1280,720),last=pixel_point(steps[steps.size()-2].point,1280,720);
            require(std::abs(std::hypot(first.first-last.first,first.second-last.second)-72)<2,"rectangle screen fit clipped full Flick stroke");
        });
        run("screen fit Flick preserves full stroke timing and exact native judgment normal",[] {
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;
            options.calibration_id="screen-fit-unqualified";options.flick_path_policy=FlickPathPolicy::judgment_anchored_screen_fit_v1;
            options.flick_lead_ns=30'000'000;options.flick_travel_ns=60'000'000;options.flick_release_ns=24'000'000;options.flick_distance_reference_pixels=72;
            for(const double angle:{0.,30.,90.,135.,180.,-45.,-90.})for(const double native_y:{0.,360.,720.}) {
                auto c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1-native_y/720;
                c.lines[0].rotate_events[0].start=c.lines[0].rotate_events[0].end=angle;
                // A zero-opacity line still has a real judge pose.
                c.lines[0].opacity_events[0].start=c.lines[0].opacity_events[0].end=0;note(c,NoteKind::flick);
                const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
                if((angle==90||angle==-90)&&native_y!=360) {
                    require(!result.plan&&diagnostic(result,"projection_unavailable"),"screen fit fabricated tangent reachability for vertical line outside canvas");continue;
                }
                const auto& p=plan(result);inside_inset(p);
                const auto& owner=p.ownership[0];const auto& steps=p.memory.contacts[0].steps;
                require(owner.flick_anchor_reference.has_value()&&owner.flick_direction_reference.has_value(),"screen-fit anchor/direction provenance missing");
                const auto pose=PhigrosPoseEvaluator(c).line_at(0,second);const auto native=phigros_line_point(pose,0,NoteSide::above).reference_pixels;
                const auto anchor=*owner.flick_anchor_reference;const auto d=*owner.flick_direction_reference;
                require(std::abs((anchor.x-native.x)*pose.tangent.x+(anchor.y-native.y)*pose.tangent.y)<1e-9,"screen fit changed native tangent judgment coordinate");
                require(std::abs(std::hypot(d.x,d.y)-1)<1e-12,"screen fit shortened stroke by unnormalized direction");
                require(steps.front().song_ns==second-30'000'000&&steps.back().song_ns==second+54'000'000,"screen fit changed native stroke timing");
                const auto first=pixel_point(steps.front().point,1280,720),last=pixel_point(steps[steps.size()-2].point,1280,720);
                require(std::abs(std::hypot(first.first-last.first,first.second-last.second)-72)<2,"screen fit lost full raster stroke displacement");
                const auto judgment=std::find_if(steps.begin(),steps.end(),[](const auto& s){return s.song_ns==second;});
                require(judgment!=steps.end(),"screen fit omitted judgment key");
                require(diagnostic(result,"unverified_screen_fit_flick")&&!p.metadata.judgment_policy_provenance.empty(),"screen fit silently asserted qualification");
                auto changed=p;changed.ownership[0].flick_direction_reference->x+=.01;
                require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"screen fit provenance not fingerprinted");
            }
        });
        run("screen fit keeps old straight rejection and refuses unreachable judgment normal",[] {
            auto c=chart();c.lines[0].move_events[0].start_y=c.lines[0].move_events[0].end_y=1;note(c,NoteKind::flick);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.normal_projection_inset_reference_pixels=64;options.calibration_id="screen-fit-negative";
            options.flick_path_policy=FlickPathPolicy::judgment_anchored_straight_v1;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"straight_flick_outside_rectangle"),"old straight policy silently changed");
            options.flick_path_policy=FlickPathPolicy::judgment_anchored_screen_fit_v1;
            const auto success=compile_phigros_contacts(c,identity(c),geometry(),options);require(success.plan.has_value(),"full screen-fit stroke failed at top edge");
            c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=-1;
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"screen fit fabricated tangent tolerance for inaccessible line");
            c=chart();note(c,NoteKind::flick);options.flick_distance_reference_pixels=2000;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"screen_fit_flick_unavailable"),"screen fit clipped oversized stroke");
        });
        run("explicit Hold tail grace freezes only final220ms and retains full pointer lifetime",[] {
            auto c=chart();auto initial=c.lines[0].move_events[0];initial.time.end_ns=1'790'000'000;
            auto later=initial;later.time.start_ns=initial.time.end_ns;later.time.end_ns=10*second;later.start_x=later.end_x=-2;
            c.lines[0].move_events={initial,later};note(c,NoteKind::hold,second,2*second);
            ContactCompileOptions options;options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.calibration_id="tail-grace-unqualified";
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"full-pose policy stopped tracking without opt-in");
            options.hold_tail_policy=HoldTailPolicy::phira_tail_grace_candidate_v1;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
            require(p.memory.contacts.size()==1&&p.ownership[0].hold_tracking_end_ns==1'780'000'000,"tail grace lost note or tracking end");
            const auto& steps=p.memory.contacts[0].steps;
            require(steps.front().phase==Phase::down&&steps.front().song_ns==second&&steps.back().phase==Phase::up&&steps.back().song_ns==2*second,"tail grace shortened native contact lifetime");
            require(steps[steps.size()-2].song_ns==1'780'000'000&&steps[steps.size()-2].phase==Phase::move&&steps.back().point.x==steps[steps.size()-2].point.x,"tail grace did not freeze final contact");
            require(diagnostic(result,"unverified_hold_tail_grace")&&p.metadata.requires_calibration&&!p.metadata.judgment_policy_provenance.empty(),"tail grace missing unqualified provenance");
            auto changed=p;--changed.ownership[0].hold_tracking_end_ns;
            require(compiled_contact_fingerprint(changed)!=p.metadata.cache_sha256,"tail tracking end not fingerprinted");
            c.lines[0].move_events[0].time.end_ns=1'500'000'000;c.lines[0].move_events[1].time.start_ns=1'500'000'000;
            require(!compile_phigros_contacts(c,identity(c),geometry(),options).plan,"tail grace ignored an unreachable mid-Hold");
        });
        run("short Hold tail grace retains DOWN UP and source while invalid policy refuses",[] {
            auto c=chart();note(c,NoteKind::hold,second,second+100'000'000);
            ContactCompileOptions options;options.hold_tail_policy=HoldTailPolicy::phira_tail_grace_candidate_v1;
            options.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
            options.max_normal_projection_reference_pixels=7000;options.calibration_id="short-hold-tail-unqualified";
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);const auto& p=plan(result);
            require(p.memory.contacts[0].steps.size()==2&&p.memory.contacts[0].steps.back().song_ns==second+100'000'000,"short Hold tail grace dropped native duration");
            require(p.ownership[0].source_note_id==c.lines[0].notes[0].source.id&&p.ownership[0].hold_tracking_end_ns==second,"short Hold tail grace dropped source");
            require(p.ownership[0].normal_projection_spans.size()==1&&p.ownership[0].normal_projection_spans[0].begin_ns==second&&
                p.ownership[0].normal_projection_spans[0].end_ns==second,"short Hold omitted actual judgment geometry certificate required by runtime");
            options.hold_tail_policy=static_cast<HoldTailPolicy>(99);
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry(),options),"invalid_options"),"invalid tail policy accepted");
        });
        run("native source format metadata is conditional and bound to explicit interpretation",[] {
            auto c=chart();note(c,NoteKind::tap);const auto old=plan(compile_phigros_contacts(c,identity(c),geometry())).metadata.cache_sha256;
            c.source_format_version=1;
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry()),"invalid_import_policy"),"v1 source accepted under v3 policy");
            c.import_policy_id="phigros-v1-source-priority-constant-empty-candidate-v1";c.import_interpretation_unqualified=true;
            const auto result=compile_phigros_contacts(c,identity(c),geometry());const auto& p=plan(result);
            require(p.metadata.source_format_version==1&&p.metadata.cache_sha256!=old,"v1 source format omitted from metadata/fingerprint");
            auto copy=p;copy.metadata.source_format_version=3;
            require(compiled_contact_fingerprint(copy)!=p.metadata.cache_sha256,"source format not fingerprinted");
            c.import_policy_id="phigros-v1-source-priority-empty-interval-candidate-v1";
            const auto empty=compile_phigros_contacts(c,identity(c),geometry());
            require(empty.plan.has_value()&&empty.plan->metadata.import_policy_id==c.import_policy_id,"explicit native v1 empty-interval interpretation rejected or lost");
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
        run("BlockArea detours preserve complete contacts, projected judgment bounds and Flick geometry",[] {
            auto c=chart();
            note(c,NoteKind::tap,second,second);note(c,NoteKind::drag,second,second);
            note(c,NoteKind::hold,second,2*second);note(c,NoteKind::flick,second,second);
            const auto options=block_options();
            const auto baseline=compile_phigros_contacts(c,identity(c),geometry(),options);
            block(c,600,320,680,400);
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            const auto& before=plan(baseline);const auto& after=plan(result);
            require(after.metadata.block_area_policy_id=="phira-block-area-continuous-detour-v1"&&
                after.metadata.block_area_count==1&&after.metadata.block_area_detoured_contacts==4&&
                after.metadata.cache_sha256!=before.metadata.cache_sha256,"BlockArea detour metadata or fingerprint missing");
            require(before.peak_contacts==after.peak_contacts&&before.total_steps==after.total_steps,"BlockArea modified contact capacity or step count");
            for(std::size_t n=0;n<before.memory.contacts.size();++n) {
                const auto& a=before.memory.contacts[n];const auto& b=after.memory.contacts[n];
                require(a.id==b.id&&a.steps.size()==b.steps.size(),"BlockArea changed contact ownership");
                const auto displacement=*after.ownership[n].block_area_translation_reference;
                for(std::size_t i=0;i<a.steps.size();++i) {
                    const auto x=pixel_point(a.steps[i].point,1280,720),y=pixel_point(b.steps[i].point,1280,720);
                    require(a.steps[i].song_ns==b.steps[i].song_ns&&a.steps[i].phase==b.steps[i].phase&&
                        std::abs(y.first-x.first-displacement.x)<1e-6&&std::abs(y.second-x.second-displacement.y)<1e-6,
                        "BlockArea changed contact timing or deformed its path");
                }
            }
            const auto& old_flick=*before.ownership.back().flick_discrete_path;
            const auto& flick=*after.ownership.back().flick_discrete_path;
            require(flick.move_count==3&&flick.max_jump_input_pixels==old_flick.max_jump_input_pixels&&
                flick.endpoint_distance_reference_pixels==old_flick.endpoint_distance_reference_pixels,
                "BlockArea shortened or straightened sampled Flick");
            check_complete_events(after);
        });
        run("BlockArea handles moving coverage, subtraction parity, phase windows and impossible paths",[] {
            auto c=chart();note(c,NoteKind::hold,second,2*second);
            auto options=block_options();
            auto& moving=block(c,600,325,680,395);
            moving.moves={{second,{.25,.5},0,0},{2*second,{.75,.5},0,0}};
            auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(result).metadata.block_area_detoured_contacts==1,
                "BlockArea did not detect a moving region passing over a stationary Hold");
            c.block_areas.clear();block(c,0,0,1280,720);
            result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!result.plan&&diagnostic(result,"block_area_no_safe_path"),"Blocked screen accepted without a safe route");
            block(c,560,280,720,440,0,3*second,true);
            result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(result).metadata.block_area_detoured_contacts==0,"Subtractive hole did not preserve an allowed contact");
            block(c,560,280,720,440,0,3*second,true);
            result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!result.plan&&diagnostic(result,"block_area_no_safe_path"),"Overlapping subtractive regions did not use XOR parity");
            c.block_areas.clear();c.lines[0].notes.clear();note(c,NoteKind::tap);
            block(c,600,320,680,400,second+50'000'000,second+500'000'000);
            options.block_area_phase_ns=-70'000'000;
            result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(result).metadata.block_area_detoured_contacts==0,"Pinned phase did not exclude later region activation");
            const auto pinned=plan(result).metadata.cache_sha256;
            options.block_area_phase_ns.reset();
            result=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(result).metadata.block_area_detoured_contacts==1&&plan(result).metadata.cache_sha256!=pinned,
                "Unpinned phase window ignored later activation or reused the pinned fingerprint");
            require(diagnostic(compile_phigros_contacts(c,identity(c),geometry()),"block_area_detour_requires_judgment_rectangle"),
                "Blocked path was altered without bounded judgment geometry");
        });
        run("Active BlockArea temporal alternatives are explicit and retain exact contact timing",[] {
            auto c=chart();
            note(c,NoteKind::tap);note(c,NoteKind::drag);
            note(c,NoteKind::hold,second,2*second);note(c,NoteKind::flick);
            auto options=block_options();
            const auto baseline=compile_phigros_contacts(c,identity(c),geometry(),options);
            options.allow_block_area_temporal_fallback=true;
            const auto unrelated=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(plan(unrelated).metadata.cache_sha256==plan(baseline).metadata.cache_sha256,
                "Temporal opt-in affected a chart with no active regions");
            block(c,0,0,950,720);
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            const auto& p=plan(result);
            require(p.metadata.block_area_temporal_contacts==4&&p.metadata.block_area_detoured_contacts==4,
                "Temporal alternatives not counted separately from judgment-certified detours");
            require(p.total_steps==plan(baseline).total_steps&&p.peak_contacts==plan(baseline).peak_contacts,
                "Temporal alternatives did not preserve the contact budget or duration");
            for(std::size_t i=0;i<p.memory.contacts.size();++i) {
                const auto& owner=p.ownership[i];
                require(owner.block_area_temporal_fallback&&owner.normal_projection_spans.empty(),
                    "Temporal alternative made an unsupported geometric judgment claim");
                const auto& before=plan(baseline).memory.contacts[i];const auto& after=p.memory.contacts[i];
                for(std::size_t j=0;j<before.steps.size();++j) {
                    require(before.steps[j].song_ns==after.steps[j].song_ns&&before.steps[j].phase==after.steps[j].phase,
                        "Temporal route changed a contact's planned time or input phase");
                    require(pixel_point(after.steps[j].point,1280,720).first>=928,"Temporal route entered the red area");
                }
            }
            require(p.ownership.back().flick_discrete_path->move_count==3,"Temporal Flick lost sampled curved MOVE vertices");
            check_complete_events(p);
            c=chart();note(c,NoteKind::tap);c.lines[0].move_events[0].start_x=c.lines[0].move_events[0].end_x=1.5;
            block(c,0,0,950,720,2*second,3*second);
            const auto outside=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!outside.plan&&diagnostic(outside,"judgment_domain_unreachable"),
                "Temporal opt-in bypassed judgment geometry outside an active BlockArea time");
        });
        run("Moving BlockArea Hold alternative keeps one continuous pointer through a traveling hole",[] {
            auto c=chart();note(c,NoteKind::hold,second,2*second);
            block(c,-100,-100,1380,820);
            auto& hole=block(c,220,240,420,480,0,3*second,true);
            hole.moves={{second,{.25,.5},0,0},{2*second,{.75,.5},0,0}};
            auto options=block_options();options.allow_block_area_temporal_fallback=true;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            const auto& p=plan(result);const auto& steps=p.memory.contacts.front().steps;
            require(p.metadata.block_area_temporal_contacts==1&&p.peak_contacts==1&&steps.size()>2,
                "Moving hole did not compile to one continuously owned pointer with MOVE");
            require(steps.front().song_ns==second&&steps.front().phase==Phase::down&&
                steps.back().song_ns==2*second&&steps.back().phase==Phase::up,
                "Traveling Hold altered the original press or release");
            for(std::size_t i=1;i+1<steps.size();++i) {
                const auto [x,y]=pixel_point(steps[i].point,1280,720);
                const double center=320+640*static_cast<double>(steps[i].song_ns-second)/second;
                require(steps[i].phase==Phase::move&&std::abs(x-center)<100&&y>240&&y<480,
                    "Moving Hold left the subtractive corridor or started an extra pointer");
            }
            check_complete_events(p);
        });
        run("BlockArea Hold graph keeps a distant playable island through a later sweep",[] {
            auto c=chart();note(c,NoteKind::hold,second,2*second);
            block(c,-100,-100,1380,820);
            auto& hole=block(c,-70,-100,1350,820,0,3*second,true);
            hole.scales={{second,{140./1280,.5},{1,1},0,0},
                {second+200'000'000,{140./1280,.5},{.11,1},0,0}};
            hole.moves={{second+200'000'000,{.5,.5},0,0},{2*second,{.7,.5},0,0}};
            auto options=block_options();options.allow_block_area_temporal_fallback=true;
            options.hold_tail_policy=HoldTailPolicy::phira_tail_grace_candidate_v1;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            const auto& p=plan(result);const auto& contact=p.memory.contacts.front();
            require(p.metadata.block_area_temporal_contacts==1&&p.peak_contacts==1&&contact.steps.size()>2,
                "Distant playable island lost during moving Hold search");
            require(contact.steps.front().song_ns==second&&contact.steps.front().phase==Phase::down&&
                contact.steps.back().song_ns==2*second&&contact.steps.back().phase==Phase::up&&
                p.ownership.front().hold_tracking_end_ns==2*second,
                "Graph Hold modified lifetime or recorded a grace interval while moving");
            check_complete_events(p);
        });
        run("Opt-in BlockArea Hold handoff overlaps independently clear pointers across a blocked gap",[] {
            auto c=chart();note(c,NoteKind::hold,second,3*second);
            block(c,-100,-100,1380,820,0,4*second);
            block(c,100,200,340,520,0,2*second,true);
            block(c,900,200,1140,520,second+400'000'000,4*second,true);
            auto options=block_options();options.allow_block_area_temporal_fallback=true;
            auto strict=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!strict.plan&&diagnostic(strict,"block_area_no_safe_path"),"One pointer crossed the disconnected regions");
            options.allow_block_area_hold_handoffs=true;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            const auto& p=plan(result);const auto& owner=p.ownership.front();
            require(p.ownership.size()==1&&p.memory.contacts.size()==2&&p.peak_contacts==2&&p.total_steps==4&&
                p.metadata.block_area_hold_handoff_contacts==1&&owner.hold_handoff_contact_ids.size()==1,
                "Hold handoff changed source ownership or skipped physical contact accounting");
            const auto& a=p.memory.contacts[0];const auto& b=p.memory.contacts[1];
            require(a.id==owner.contact_id&&b.id==owner.hold_handoff_contact_ids.front()&&
                a.steps.front().song_ns==second&&b.steps.back().song_ns==3*second&&
                b.steps.front().song_ns>a.steps.front().song_ns&&
                a.steps.back().song_ns-b.steps.front().song_ns==block_area_hold_handoff_overlap_ns,
                "Hold handoff did not preserve original lifetime and exact pointer overlap");
            require(pixel_point(a.steps.front().point,1280,720).first<361&&pixel_point(b.steps.front().point,1280,720).first>878,
                "Pointer started outside its independently clear region");
            check_complete_events(p);
            options.max_contacts=1;
            const auto over=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!over.plan&&over.required_peak_contacts==2&&diagnostic(over,"capacity_exceeded"),
                "Handoff bypassed physical pointer capacity");
            options.max_contacts=10;c.block_areas.back().appear_ns=c.block_areas.back().enable_ns=2*second;
            const auto without_overlap=compile_phigros_contacts(c,identity(c),geometry(),options);
            require(!without_overlap.plan&&diagnostic(without_overlap,"block_area_no_safe_path"),
                "Handoff accepted two regions with no clear timing overlap");
        });
        run("Temporal Flick retains a curved full-length stroke in a narrow clear border",[] {
            auto c=chart();note(c,NoteKind::flick);
            block(c,0,0,1280,720);
            auto options=block_options();options.allow_block_area_temporal_fallback=true;
            options.normal_projection_inset_reference_pixels=12;
            const auto result=compile_phigros_contacts(c,identity(c),geometry(),options);
            const auto& p=plan(result);const auto& owner=p.ownership.front();
            require(owner.block_area_temporal_fallback&&owner.flick_discrete_path->move_count==3,
                "Narrow border did not keep the sampled temporal Flick");
            const auto& direction=*owner.flick_direction_reference;
            require(std::abs(std::min(std::abs(direction.x),std::abs(direction.y))-.075)<1e-6&&
                std::abs(owner.flick_discrete_path->endpoint_distance_reference_pixels-options.flick_distance_reference_pixels)<1e-6,
                "Narrow Flick lost its oblique angle or original distance");
            for(const auto& step:p.memory.contacts.front().steps) {
                const auto [x,y]=pixel_point(step.point,1280,720);
                require((y<=20||y>=700||x<=20||x>=1260)&&x>=12&&x<=1267&&y>=12&&y<=707,
                    "Narrow Flick entered a red area or crossed the playable inset");
            }
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
