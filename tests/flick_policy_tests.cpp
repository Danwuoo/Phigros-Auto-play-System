#include "../apps/chart_experiment/flick_policy.hpp"
#include "../apps/chart_experiment/adaptive_first_note.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace rhythm;
using namespace rhythm::experimental;
using Json=nlohmann::json;
constexpr Ns ms=1'000'000,second=1'000'000'000;
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool bad=false;try{f();}catch(const std::invalid_argument&){bad=true;}check(bad,why);}
SemanticChart chart() {
    SemanticChart c;c.source={"synthetic",std::string(64,'a'),"flick-controls","IN","synthetic-v3"};
    c.lines.resize(1);auto& l=c.lines[0];l.bpm=120;
    MoveLineEvent move;move.time.start_ns=-second;move.time.end_ns=20*second;move.start_x=move.end_x=.5;move.start_y=move.end_y=.5;l.move_events={move};
    NumericLineEvent numeric;numeric.time=move.time;l.rotate_events={numeric};numeric.start=numeric.end=1;l.opacity_events={numeric};
    SpeedLineEvent speed;speed.time.start_ns=0;speed.time.end_ns=20*second;speed.value=1.5;
    speed.source.json_pointer="/judgeLineList/0/speedEvents/0";l.speed_events={speed};
    return c;
}
void note(SemanticChart& c,NoteKind kind,Ns when) {
    SemanticNote n;n.kind=kind;n.side=NoteSide::above;n.start_ns=n.end_ns=when;n.speed=1;
    n.floor_position=1.5*static_cast<double>(when)/second;n.source_index=c.lines[0].notes.size();
    n.source.json_pointer="/judgeLineList/0/notesAbove/"+std::to_string(n.source_index);
    n.source.id=c.source.chart_sha256+'#'+n.source.json_pointer;c.lines[0].notes.push_back(n);++c.note_count;
}
ContactCompileResult raw_compile(const SemanticChart& c,const FlickParameters& p,
        ContactCompileOptions o={},PhigrosGeometry geometry={1280,720}) {
    o.max_contacts=5;o.tap_duration_ns=o.drag_duration_ns=32*ms;apply_flick_parameters(o,p);
    const Identity id{Game::phigros,c.source.song_id,c.source.difficulty,c.source.game_revision,c.source.chart_sha256,std::string(64,'b'),"synthetic-layout"};
    return compile_phigros_contacts(c,id,geometry,o);
}
CompiledContactPlan compile(const SemanticChart& c,const FlickParameters& p) {
    auto result=raw_compile(c,p);
    if(!result.plan)throw std::runtime_error("synthetic policy failed to compile");
    return *result.plan;
}
unsigned reversals(const Contact& contact) {
    double px=0,py=0;unsigned count=0;
    for(std::size_t i=1;i+1<contact.steps.size();++i) {
        const auto dx=contact.steps[i].point.x-contact.steps[i-1].point.x;
        const auto dy=contact.steps[i].point.y-contact.steps[i-1].point.y;
        if(dx==0&&dy==0)continue;
        if(dx*px+dy*py<0)++count;
        px=dx;py=dy;
    }
    return count;
}
void straight_contracts() {
    auto moving=chart();note(moving,NoteKind::flick,2*second);note(moving,NoteKind::tap,3*second);
    note(moving,NoteKind::drag,4*second);note(moving,NoteKind::hold,5*second);
    moving.lines[0].notes.back().end_ns=6*second;
    auto& line=moving.lines[0];const auto original_move=line.move_events[0];
    auto a=original_move,b=a,d=a;
    a.time.end_ns=1990*ms;b.time.start_ns=a.time.end_ns;b.time.end_ns=2010*ms;b.end_y=.35;
    d.time.start_ns=b.time.end_ns;d.start_y=d.end_y=b.end_y;line.move_events={a,b,d};
    auto r=line.rotate_events[0],r2=r,r3=r;
    r.time.end_ns=1995*ms;r2.time.start_ns=r.time.end_ns;r2.time.end_ns=2005*ms;r2.end=45;
    r3.time.start_ns=r2.time.end_ns;r3.start=r3.end=45;line.rotate_events={r,r2,r3};
    FlickParameters old;old.distance_reference_pixels=72;auto stable=old;
    stable.path_policy=FlickPathPolicy::judgment_anchored_straight_v1;
    check(flick_parameters_from_profile(flick_parameters_json(stable)).path_policy==stable.path_policy,"path policy did not roundtrip");
    FlickParameters cli;check(set_flick_option(cli,"--flick-path-policy","judgment-anchored-straight-v1")&&cli.path_policy==stable.path_policy,"path CLI not wired");
    for(const Json& invalid:{Json(true),Json(1),Json(nullptr),Json("unknown")})
        rejects([&]{(void)flick_parameters_from_profile(Json{{"flick_path_policy",invalid}});},"unknown or invalid path policy accepted");
    check(reversals(compile(moving,old).memory.contacts[0])>0,"moving-line regression no longer reproduces reversal");
    for(const auto side:{NoteSide::above,NoteSide::below})for(const auto rotation:{InputRotation::none,InputRotation::clockwise_90,InputRotation::clockwise_180,InputRotation::clockwise_270}) {
        moving.lines[0].notes[0].side=side;
        PhigrosGeometry g{1280,720};g.rotation=rotation;
        if(rotation==InputRotation::clockwise_90||rotation==InputRotation::clockwise_270){g.input_width=720;g.input_height=1280;}
        ContactCompileOptions o;o.offscreen_policy=OffscreenPolicy::bounded_normal_intersection_candidate_v1;
        o.normal_projection_inset_reference_pixels=64;o.max_normal_projection_reference_pixels=7000;
        o.calibration_id="synthetic-pending";o.tight_projection_speed_bounds=true;
        const auto prior=raw_compile(moving,old,o,g),updated=raw_compile(moving,stable,o,g);
        check(prior.plan.has_value()&&updated.plan.has_value(),"moving/rotating straight candidate rejected");
        const auto& p=*updated.plan;const auto& contact=p.memory.contacts[0];const auto& before=prior.plan->memory.contacts[0];
        check(reversals(contact)==0,"straight stroke reversed after raster/rotation");
        check(p.metadata.cache_sha256!=prior.plan->metadata.cache_sha256&&p.metadata.requires_calibration,"new path inherited archived identity or qualification");
        check(contact.steps.front().song_ns==before.steps.front().song_ns&&contact.steps[contact.steps.size()-2].song_ns==before.steps[before.steps.size()-2].song_ns&&contact.steps.back().song_ns==before.steps.back().song_ns,"path changed DOWN/end/UP timing");
        const auto judgment=std::find_if(contact.steps.begin(),contact.steps.end(),[](const Step& s){return s.song_ns==2*second;});
        const auto old_j=std::find_if(before.steps.begin(),before.steps.end(),[](const Step& s){return s.song_ns==2*second;});
        check(judgment!=contact.steps.end()&&old_j!=before.steps.end()&&judgment->point.x==old_j->point.x&&judgment->point.y==old_j->point.y,"judgment anchor moved");
        for(std::size_t i=1;i<p.memory.contacts.size();++i) {
            const auto& now=p.memory.contacts[i].steps;const auto& was=prior.plan->memory.contacts[i].steps;
            check(now.size()==was.size(),"non-Flick sampling changed");
            for(std::size_t j=0;j<now.size();++j)
                check(now[j].song_ns==was[j].song_ns&&now[j].phase==was[j].phase&&now[j].point.x==was[j].point.x&&now[j].point.y==was[j].point.y,"non-Flick command changed");
        }
        PhigrosPoseEvaluator poses(moving);PhigrosGeometryMap map(g);
        const auto target=poses.note_target(0,moving.lines[0].notes[0],2*second);
        double worst=0;
        for(Ns t=1970*ms;t<2030*ms;t+=157003) {
            const auto it=std::upper_bound(contact.steps.begin(),contact.steps.end(),t,[](Ns time,const Step& s){return time<s.song_ns;});
            const auto& held=std::prev(it)->point;
            const double offset=72*static_cast<double>(t-2*second)/(60*ms);
            const auto ideal=map.map_reference_pixels({target.reference_pixels.x+target.side_normal.x*offset,target.reference_pixels.y+target.side_normal.y*offset});
            worst=std::max(worst,std::hypot(held.x*(g.input_width-1)-ideal.input_normalized.x*g.input_width,
                held.y*(g.input_height-1)-ideal.input_normalized.y*g.input_height));
        }
        check(worst<=o.max_path_error_pixels,"straight path violated continuous held-error bound");
        check(p.ownership[0].normal_projection_spans.size()==1&&p.ownership[0].normal_projection_spans[0].begin_ns==2*second&&
            p.ownership[0].normal_projection_spans[0].end_ns==2*second&&!p.ownership[0].time_varying_normal_projection,"anchor projection mislabeled as moving-line proof");
    }
    auto edge=chart();note(edge,NoteKind::flick,2*second);edge.lines[0].move_events[0].start_y=edge.lines[0].move_events[0].end_y=.99;
    const auto rejected=raw_compile(edge,stable);
    check(!rejected.plan&&std::any_of(rejected.diagnostics.begin(),rejected.diagnostics.end(),[](const auto& d){return d.code=="straight_flick_outside_rectangle";}),"offscreen straight stroke was clipped or shortened");
    ContactCompileOptions tiny;tiny.max_steps_per_contact=3;
    check(!raw_compile(moving,stable,tiny).plan,"straight path exceeded finite step budget");
    auto static_chart=chart();note(static_chart,NoteKind::flick,2*second);
    tiny.max_steps_per_contact=100;tiny.max_path_error_pixels=1.4142246;
    const auto work=raw_compile(static_chart,stable,tiny);
    check(!work.plan&&std::any_of(work.diagnostics.begin(),work.diagnostics.end(),[](const auto& d){return d.code=="straight_flick_work_budget";}),"duplicate raster samples bypassed finite work budget");
}
}
int main() {
    try {
        const auto defaults=flick_parameters_from_profile(Json::object());
        auto c=chart();note(c,NoteKind::flick,2*second);note(c,NoteKind::tap,3*second);
        const auto old=compile(c,defaults);
        check(old.metadata.options.flick_lead_ns==30*ms&&old.metadata.options.flick_travel_ns==60*ms&&
              old.metadata.options.flick_release_ns==24*ms&&old.metadata.options.flick_distance_reference_pixels==60,"archived defaults changed");
        const Json explicit_defaults={{"flick_lead_ms",30},{"flick_travel_ms",60},{"flick_release_ms",24},{"flick_distance_reference_pixels",60}};
        check(compile(c,flick_parameters_from_profile(explicit_defaults)).metadata.cache_sha256==old.metadata.cache_sha256,"explicit old defaults changed plan fingerprint");
        check(flick_parameters_json(flick_parameters_from_profile(explicit_defaults))==explicit_defaults,"profile parameters did not roundtrip");
        for(const auto& [field,value]:std::vector<std::pair<std::string,Json>>{
                {"flick_lead_ms",40},{"flick_travel_ms",80},{"flick_release_ms",32},{"flick_distance_reference_pixels",72.5}}) {
            auto updated=explicit_defaults;updated[field]=value;
            const auto p=flick_parameters_from_profile(updated);const auto changed=compile(c,p);
            check(changed.metadata.cache_sha256!=old.metadata.cache_sha256,"parameter mutation reused stale compiled identity");
            const auto& steps=changed.memory.contacts[0].steps;
            check(steps.front().song_ns==2*second-p.lead_ms*ms,"profile lead did not reach compiler");
            check(steps[steps.size()-2].song_ns==steps.front().song_ns+p.travel_ms*ms,"profile travel did not reach compiler");
            check(steps.back().song_ns-steps[steps.size()-2].song_ns==p.release_ms*ms,"profile release did not reach compiler");
            check(changed.metadata.options.flick_distance_reference_pixels==p.distance_reference_pixels,"profile distance did not reach compiler");
            const auto& tap=changed.memory.contacts[1].steps;const auto& prior=old.memory.contacts[1].steps;
            check(tap.size()==prior.size()&&tap.front().song_ns==prior.front().song_ns&&tap.back().song_ns==prior.back().song_ns&&
                  tap.front().point.x==prior.front().point.x&&tap.front().point.y==prior.front().point.y,"Flick adjustment moved a Tap");
        }
        for(const auto& field:{"flick_lead_ms","flick_travel_ms","flick_release_ms"})for(const Json& bad:{Json(true),Json(3.5),Json("30"),Json(nullptr),Json(-1),Json(std::numeric_limits<std::uint64_t>::max())})
            rejects([&]{(void)flick_parameters_from_profile(Json{{field,bad}});},"invalid timing type/range accepted");
        for(const Json& bad:{Json(true),Json("60"),Json(nullptr),Json(0),Json(241),Json(std::numeric_limits<double>::infinity())})
            rejects([&]{(void)flick_parameters_from_profile(Json{{"flick_distance_reference_pixels",bad}});},"invalid distance accepted");
        rejects([&]{(void)flick_parameters_from_profile(Json{{"flick_lead_ms",60},{"flick_travel_ms",60}});},"stroke not spanning judgment accepted");
        rejects([&]{(void)flick_parameters_from_profile(Json{{"flick_lead_ms",101}});},"lead bound ignored");
        rejects([&]{(void)flick_parameters_from_profile(Json{{"flick_travel_ms",201}});},"travel bound ignored");
        rejects([&]{(void)flick_parameters_from_profile(Json{{"flick_release_ms",101}});},"release bound ignored");
        FlickParameters ordered;
        check(set_flick_option(ordered,"--flick-lead-ms",90)&&set_flick_option(ordered,"--flick-travel-ms",120),"CLI option missing");
        validate_flick_parameters(ordered);
        check(!set_flick_option(ordered,"--unknown",30),"unknown CLI option silently accepted");
        // A later native Flick may own the earliest DOWN. The opening model
        // must consume that compiled time, while observing the earliest note.
        auto mixed=chart();note(mixed,NoteKind::tap,2*second);note(mixed,NoteKind::flick,2020*ms);
        FlickParameters lead;lead.lead_ms=50;const auto plan=compile(mixed,lead);
        check(plan.events.front().song_ns==1970*ms,"later Flick lead not reflected in first contact");
        AdaptiveFirstNoteTrigger opening(mixed,-80*ms,plan.events.front().song_ns);
        check(opening.description().first_down_ns==1970*ms&&opening.description().note_time_ns==2*second,"native judgment and first dispatched DOWN conflated");
        AdaptiveFirstNoteTrigger archived(c,-80*ms),explicit_first(c,-80*ms,old.events.front().song_ns);
        check(archived.description().model_sha256==explicit_first.description().model_sha256,"explicit unchanged first DOWN broke archived model identity");
        const auto altered=compile(c,lead);AdaptiveFirstNoteTrigger altered_opening(c,-80*ms,altered.events.front().song_ns);
        check(altered_opening.description().model_sha256!=archived.description().model_sha256,"changed lead retained stale opening identity");
        straight_contracts();
        std::cout<<"PASS Flick parameter/profile/compiler/opening contracts: "<<checks<<" checks\n";
        return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
