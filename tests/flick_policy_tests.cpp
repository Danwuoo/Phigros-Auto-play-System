#include "../apps/chart_experiment/flick_policy.hpp"
#include "../apps/chart_experiment/adaptive_first_note.hpp"
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
CompiledContactPlan compile(const SemanticChart& c,const FlickParameters& p) {
    ContactCompileOptions o;o.max_contacts=5;o.tap_duration_ns=o.drag_duration_ns=32*ms;apply_flick_parameters(o,p);
    const Identity id{Game::phigros,c.source.song_id,c.source.difficulty,c.source.game_revision,c.source.chart_sha256,std::string(64,'b'),"synthetic-layout"};
    PhigrosGeometry geometry;geometry.input_width=1280;geometry.input_height=720;
    auto result=compile_phigros_contacts(c,id,geometry,o);
    if(!result.plan)throw std::runtime_error("synthetic policy failed to compile");
    return *result.plan;
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
        std::cout<<"PASS Flick parameter/profile/compiler/opening contracts: "<<checks<<" checks\n";
        return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
