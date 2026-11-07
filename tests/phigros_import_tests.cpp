#include "rhythm/chart.hpp"
#include "rhythm/phigros_pose.hpp"
#include "rhythm/chart_memory.hpp" // Public semantic IR can coexist with resolved memory.
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace rhythm;
namespace {
void check(bool result) { if (!result) throw std::runtime_error("check failed"); }
void rejects(const std::function<void()>& run) {
    try { run(); } catch (const std::exception&) { return; }
    throw std::runtime_error("expected rejection");
}
std::string note(std::string tick = "32", std::string kind = "1", std::string hold = "0") {
    return "{\"type\":" + kind + ",\"time\":" + tick + ",\"holdTime\":" + hold +
           ",\"positionX\":-4,\"speed\":1.25,\"floorPosition\":42}";
}
std::string line(std::string above = "[]", std::string below = "[]", std::string bpm = "120",
                 std::string events = "") {
    return "{\"bpm\":" + bpm + ",\"notesAbove\":" + above + ",\"notesBelow\":" + below +
        ",\"speedEvents\":[],\"judgeLineMoveEvents\":[],\"judgeLineRotateEvents\":[],\"judgeLineDisappearEvents\":[]" + events + "}";
}
std::string chart(std::string lines, std::string offset = "0") {
    return "{\"formatVersion\":3,\"offset\":" + offset + ",\"judgeLineList\":[" + lines + "]}";
}
SemanticChart parse(const std::string& text, const ChartImportLimits& limits = {}) {
    ChartSource source; source.label = "synthetic fixture";
    return parse_phigros(text, source, limits);
}
std::string replace(std::string input, const std::string& before, const std::string& after) {
    const auto at = input.find(before);
    if (at == std::string::npos) throw std::runtime_error("fixture field missing");
    input.replace(at, before.size(), after);
    return input;
}
ImportOptions candidate_options() {
    ImportOptions options;options.policy=PhigrosImportPolicy::source_priority_constant_empty_candidate_v1;return options;
}
SemanticChart candidate(const std::string& text,const ImportOptions& options=candidate_options()) {
    ChartSource source;source.label="synthetic candidate";return parse_phigros(text,source,{},options);
}
std::string pose_fixture() {
    auto text=chart(line("["+note()+"]"));
    text=replace(text,"\"judgeLineMoveEvents\":[]","\"judgeLineMoveEvents\":[{\"startTime\":-999999,\"endTime\":1000000000,\"start\":0.5,\"end\":0.5,\"start2\":0.5,\"end2\":0.5}]");
    text=replace(text,"\"judgeLineRotateEvents\":[]","\"judgeLineRotateEvents\":[{\"startTime\":-999999,\"endTime\":1000000000,\"start\":0,\"end\":0}]");
    return text;
}
ImportOptions v1_options() {
    ImportOptions options;options.policy=PhigrosImportPolicy::native_v1_source_priority_constant_empty_candidate_v1;return options;
}
std::string v1_fixture(const std::string& moves="{\"startTime\":0,\"endTime\":64,\"start\":0,\"end\":880520}") {
    auto text=replace(pose_fixture(),"\"formatVersion\":3","\"formatVersion\":1");
    text=replace(text,"\"judgeLineDisappearEvents\":[]","\"judgeLineDisappearEvents\":[{\"startTime\":-999999,\"endTime\":1000000000,\"start\":1,\"end\":1}]");
    return replace(text,"{\"startTime\":-999999,\"endTime\":1000000000,\"start\":0.5,\"end\":0.5,\"start2\":0.5,\"end2\":0.5}",moves);
}
} // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"separate native v1 empty-interval policy retains nonconstant reversed declarations without changing notes", [] {
            const auto text=v1_fixture("{\"startTime\":0,\"endTime\":64,\"start\":440260,\"end\":440260},"
                "{\"startTime\":7416,\"endTime\":7408,\"start\":0,\"end\":880520}");
            rejects([&]{candidate(text,v1_options());});
            auto options=v1_options();options.policy=PhigrosImportPolicy::native_v1_source_priority_empty_interval_candidate_v1;
            const auto c=candidate(text,options);
            check(c.note_count==1&&c.event_count==4&&c.source_format_version==1&&c.format_version==3);
            check(c.import_policy_id=="phigros-v1-source-priority-empty-interval-candidate-v1"&&c.source.chart_sha256==chart_text_sha256(text));
            check(c.lines[0].native_event_declarations->move_events.size()==2&&c.lines[0].move_events.size()==1);
            const auto& raw=c.lines[0].native_event_declarations->move_events[1];
            check(raw.time.start_tick==7416&&raw.time.end_tick==7408&&raw.packed_start==0&&raw.packed_end==880520);
            check(c.import_anomalies.size()==2&&c.import_anomalies[1].code=="empty_reversed_pose_event"&&
                !c.import_anomalies[1].declaration_evaluated&&c.import_anomalies[1].evaluated_segments.empty());
            check(c.import_anomalies[1].source.id==c.source.chart_sha256+"#/judgeLineList/0/judgeLineMoveEvents/1");
            const auto pose=PhigrosPoseEvaluator(c).line_at(0,phigros_time_ns(7412,120,0));check(pose.origin.x==.5&&pose.origin.y==.5);
            rejects([&]{candidate(replace(text,"\"formatVersion\":1","\"formatVersion\":3"),options);});
        }},
        {"native v1 empty interval interpretation is explicit for rotate and alpha and remains bounded", [] {
            auto text=v1_fixture();text=replace(text,"\"start\":0,\"end\":0}","\"start\":0,\"end\":0},{\"startTime\":7416,\"endTime\":7408,\"start\":0,\"end\":222}");
            text=replace(text,"\"start\":1,\"end\":1}","\"start\":1,\"end\":1},{\"startTime\":50,\"endTime\":45,\"start\":1,\"end\":0}");
            auto options=v1_options();options.policy=PhigrosImportPolicy::native_v1_source_priority_empty_interval_candidate_v1;
            rejects([&]{candidate(text,v1_options());});
            const auto c=candidate(text,options);check(c.note_count==1&&c.lines[0].native_event_declarations->rotate_events.size()==2);
            check(c.lines[0].rotate_events.size()==1&&c.import_anomalies[1].original_declaration_json.find("222")!=std::string::npos);
            check(c.lines[0].native_event_declarations->opacity_events.size()==2&&c.lines[0].opacity_events.size()==1&&
                c.import_anomalies.back().code=="empty_reversed_pose_event");
            options.max_anomalies=1;rejects([&]{candidate(text,options);});
            options.max_anomalies=4096;
            const auto only_empty=v1_fixture("{\"startTime\":2,\"endTime\":1,\"start\":0,\"end\":880520}");
            rejects([&]{candidate(only_empty,options);});
            rejects([&]{candidate(replace(text,"\"offset\":0","\"offset\":0,\"blockAreaList\":[]"),options);});
        }},
        {"explicit native v1 decodes endpoints before interpolation and preserves source identity", [] {
            const auto text=replace(v1_fixture(),"\"offset\":0","\"offset\":0.125");
            ChartSource source;source.label="native v1 fixture";source.chart_sha256=chart_text_sha256(text);
            const auto c=parse_phigros(text,source,{},v1_options());
            check(c.format_version==3&&c.source_format_version==1&&c.import_interpretation_unqualified);
            check(c.import_policy_id=="phigros-v1-source-priority-constant-empty-candidate-v1");
            check(c.source.chart_sha256==chart_text_sha256(text)&&c.note_count==1&&c.event_count==3);
            const auto& declaration=c.lines[0].native_event_declarations->move_events[0];
            check(declaration.packed_start==0&&declaration.packed_end==880520);
            check(declaration.start_x==0&&declaration.start_y==0&&declaration.end_x==1&&declaration.end_y==1);
            check(declaration.source.id==source.chart_sha256+"#/judgeLineList/0/judgeLineMoveEvents/0");
            const auto pose=PhigrosPoseEvaluator(c).line_at(0,625000000);
            check(std::abs(pose.origin.x-.5)<1e-12&&std::abs(pose.origin.y-.5)<1e-12);
            check(c.lines[0].notes[0].start_ns==625000000&&c.lines[0].notes[0].position_x==-4);
            rejects([&]{parse(text);});rejects([&]{candidate(text);});
            rejects([&]{candidate(pose_fixture(),v1_options());});
        }},
        {"native v1 signed packed remainder and fractional native y remain unmodified", [] {
            const auto c=candidate(v1_fixture("{\"startTime\":0,\"endTime\":64,\"start\":-1001,\"end\":440260.5}"),v1_options());
            const auto& e=c.lines[0].move_events[0];
            check(e.packed_start==-1001&&e.packed_end==440260.5);
            check(std::abs(e.start_x+1.0/880)<1e-15&&std::abs(e.start_y+1.0/520)<1e-15);
            check(e.end_x==.5&&std::abs(e.end_y-260.5/520)<1e-15);
        }},
        {"native v1 candidate keeps original packed declarations for source-priority slices", [] {
            const auto text=v1_fixture("{\"startTime\":0,\"endTime\":32,\"start\":440260,\"end\":440260},"
                "{\"startTime\":0,\"endTime\":64,\"start\":0,\"end\":880520},"
                "{\"startTime\":70,\"endTime\":65,\"start\":440260,\"end\":440260}");
            const auto c=candidate(text,v1_options());
            check(c.lines[0].native_event_declarations->move_events.size()==3&&c.lines[0].move_events.size()==2);
            check(c.lines[0].move_events[1].start_x==.5&&c.lines[0].move_events[1].end_x==1);
            check(c.lines[0].move_events[1].packed_start==0&&c.lines[0].move_events[1].packed_end==880520);
            check(c.import_anomalies.size()==3&&c.import_anomalies[1].original_declaration_json.find("880520")!=std::string::npos);
            auto options=v1_options();options.max_anomalies=2;rejects([&]{candidate(text,options);});
        }},
        {"native v1 refuses mixed dialect unknown fields block areas and nonconstant reversed events", [] {
            rejects([&]{candidate(replace(v1_fixture(),"\"start\":0,\"end\":880520","\"start\":0,\"end\":880520,\"start2\":0,\"end2\":1"),v1_options());});
            rejects([&]{candidate(v1_fixture("{\"startTime\":64,\"endTime\":0,\"start\":0,\"end\":880520}"),v1_options());});
            rejects([&]{candidate(replace(v1_fixture(),"\"offset\":0","\"offset\":0,\"blockAreaList\":[]"),v1_options());});
            ChartSource source;source.label="mismatched bytes";source.chart_sha256=std::string(64,'a');
            rejects([&]{parse_phigros(v1_fixture(),source,{},v1_options());});
        }},
        {"SHA256 original bytes and padding vectors", [] {
            check(chart_text_sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
            check(chart_text_sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
            check(chart_text_sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
                  "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
            check(chart_text_sha256(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
        }},
        {"first note and raw semantics", [] {
            const auto c = parse(chart(line("[" + note() + "]")));
            const auto& n = c.lines[0].notes[0];
            check(c.note_count == 1 && c.event_count == 0 && n.start_ns == 500000000 && n.end_ns == n.start_ns);
            check(n.time_tick == 32 && n.hold_ticks == 0 && n.position_x == -4 && n.speed == 1.25 && n.floor_position == 42);
            check(n.source.json_pointer == "/judgeLineList/0/notesAbove/0");
            check(n.source.id == c.source.chart_sha256 + "#/judgeLineList/0/notesAbove/0");
        }},
        {"simultaneous notes on different BPM lines", [] {
            const auto c = parse(chart(line("[" + note("32") + "]") + "," + line("[]", "[" + note("64") + "]", "240")));
            check(c.lines[0].notes[0].start_ns == c.lines[1].notes[0].start_ns);
            check(c.lines[1].notes[0].side == NoteSide::below && c.lines[1].notes[0].source_index == 0);
            check(c.lines[0].notes[0].source.id != c.lines[1].notes[0].source.id);
        }},
        {"positive offset delays exactly once", [] {
            const auto c = parse(chart(line("[" + note() + "]"), "0.125"));
            check(c.offset_seconds == 0.125 && c.lines[0].notes[0].start_ns == 625000000);
        }},
        {"negative offset and negative song time retained", [] {
            const auto c = parse(chart(line("[" + note("0") + "," + note() + "]"), "-0.125"));
            check(c.lines[0].notes[0].start_ns == -125000000 && c.lines[0].notes[1].start_ns == 375000000);
        }},
        {"fractional tick BPM independent rational expectations", [] {
            // Expected integers computed with exact rational arithmetic, not this converter.
            check(phigros_time_ns(1.25, 137.5, .001) == 18045455);
            const auto fractional = parse(chart(line("[" + note("1.25", "3", "0.75") + "]", "[]", "137.5"), "0.001"));
            const auto& imported = fractional.lines[0].notes[0];
            check(imported.time_tick == 1.25 && imported.hold_ticks == .75 &&
                  imported.start_ns == 18045455 && imported.end_ns == 28272727);
            check(phigros_time_ns(4.25, 123.45, -.0023) == 62250425);
            check(phigros_time_ns(-.00000001, 137.5, 0) == 0);
            check(phigros_time_ns(0, 120, .0000000005) == 1);
            check(phigros_time_ns(0, 120, -.0000000005) == -1);
        }},
        {"hold end converts absolute ticks once", [] {
            const auto c = parse(chart(line("[" + note("32", "3", "16") + "]"), "0.125"));
            const auto& n = c.lines[0].notes[0];
            check(n.kind == NoteKind::hold && n.start_ns == 625000000 && n.end_ns == 875000000);
        }},
        {"all four types preserved", [] {
            const auto c = parse(chart(line("[" + note("0", "1") + "," + note("0", "2") + "," + note("0", "3") + "," + note("0", "4") + "]")));
            for (std::size_t i = 0; i < 4; ++i) check(static_cast<int>(c.lines[0].notes[i].kind) == static_cast<int>(i) + 1);
        }},
        {"empty chart and line are semantic input", [] {
            check(parse(chart("")).lines.empty());
            const auto c = parse(chart(line()));
            check(c.lines.size() == 1 && c.note_count == 0 && c.event_count == 0);
        }},
        {"raw sentinels events speed floor and opacity preserved", [] {
            auto text = chart(line());
            text = replace(text, "\"speedEvents\":[]", "\"speedEvents\":[{\"startTime\":0,\"endTime\":1000000000,\"value\":-4},{\"startTime\":0,\"endTime\":0,\"value\":9999,\"floorPosition\":17}]");
            text = replace(text, "\"judgeLineMoveEvents\":[]", "\"judgeLineMoveEvents\":[{\"startTime\":-999999,\"endTime\":0,\"start\":10,\"end\":-0.1,\"start2\":1.1,\"end2\":0.5}]");
            text = replace(text, "\"judgeLineRotateEvents\":[]", "\"judgeLineRotateEvents\":[{\"startTime\":0,\"endTime\":32,\"start\":-90,\"end\":360}]");
            text = replace(text, "\"judgeLineDisappearEvents\":[]", "\"judgeLineDisappearEvents\":[{\"startTime\":0,\"endTime\":32,\"start\":1.33,\"end\":0}]");
            const auto c = parse(text);
            const auto& l = c.lines[0];
            check(c.event_count == 5 && l.move_events[0].time.start_tick == -999999 && l.move_events[0].start_x == 10);
            check(l.speed_events[0].time.end_tick == 1000000000 && !l.speed_events[0].floor_position);
            check(l.speed_events[0].value == -4 && l.speed_events[1].floor_position == 17);
            check(l.rotate_events[0].end == 360 && l.opacity_events[0].start == 1.33);
        }},
        {"actual identity verified and stable", [] {
            const auto text = chart(line("[" + note() + "]"));
            const auto first = parse(text), second = parse(text);
            check(first.source.chart_sha256 == second.source.chart_sha256 && first.lines[0].notes[0].source.id == second.lines[0].notes[0].source.id);
            ChartSource good; good.label = "fixture"; good.chart_sha256 = chart_text_sha256(text);
            check(parse_phigros(text, good).source.chart_sha256 == good.chart_sha256);
            good.chart_sha256 = std::string(64, '0');
            rejects([&] { parse_phigros(text, good); });
            check(parse(text + " ").source.chart_sha256 != first.source.chart_sha256);
        }},
        {"unknown version type fields duplicate keys and missing fields rejected", [] {
            const auto text = chart(line("[" + note() + "]"));
            for (const auto& bad : {replace(text, "\"formatVersion\":3", "\"formatVersion\":1"),
                                    replace(text, "\"type\":1", "\"type\":5"),
                                    replace(text, "\"type\":1", "\"type\":1.0"),
                                    replace(text, "\"speed\":1.25", "\"unknown\":1.25"),
                                    replace(text, "\"offset\":0", "\"offset\":0,\"offset\":1")})
                rejects([&] { parse(bad); });
        }},
        {"v3 block areas preserve note declarations and normalize their seconds and keyframes", [] {
            const auto text = replace(chart(line("[" + note() + "]"), "0.125"), "\"offset\":0.125",
                R"("offset":0.125,"blockAreaList":[{
                    "topRightPercentage":{"x":0.7,"y":0.2},"bottomLeftPercentage":{"x":0.3,"y":0.8},
                    "appearTime":1,"enableTime":0.9,"disableTime":2,"disappearTime":0,
                    "isSubtract":true,
                    "moveEvents":[{"time":1.5,"endPosition":{"x":0.9,"y":0.6},"easeTypeX":13},
                                  {"time":0.5,"endPosition":{"x":0.5,"y":0.5}}],
                    "rotateEvents":[{"time":1,"anchor":{"x":0.5,"y":0.5},"rotation":0},
                                    {"time":1,"anchor":{"x":0.5,"y":0.5},"rotation":180,"easeType":14}],
                    "scaleEvents":[{"time":1.25,"anchor":{"x":0.5,"y":0.5},"scale":{"x":0,"y":-1},"easeTypeY":6}]
                }])");
            const auto c = parse(text);
            check(c.note_count == 1 && c.event_count == 0 && c.lines[0].notes[0].start_ns == 625000000);
            check(c.block_areas.size() == 1 && c.import_interpretation_unqualified);
            const auto& area = c.block_areas.front();
            check(area.source.id == chart_text_sha256(text) + "#/blockAreaList/0" &&
                  area.source.json_pointer == "/blockAreaList/0");
            check(area.top_right.y == 0.2 && area.bottom_left.y == 0.8 && area.subtract);
            check(area.appear_ns == 1125000000 && area.enable_ns == 1125000000 &&
                  area.disable_ns == 2125000000 && area.disappear_ns == 2125000000);
            check(area.moves[0].time_ns == 625000000 && area.moves[0].ease_x == 0 &&
                  area.moves[1].time_ns == 1625000000 && area.moves[1].ease_x == 13);
            check(area.rotations[0].time_ns == area.rotations[1].time_ns &&
                  area.rotations[0].degrees == 0 && area.rotations[1].degrees == 180 && area.rotations[1].ease == 14);
            check(area.scales.front().time_ns == 1375000000 && area.scales.front().scale.y == -1 &&
                  area.scales.front().ease_y == 6);
            rejects([&] { parse(replace(text, "\"easeType\":14", "\"easeType\":15")); });
            rejects([&] { parse(replace(text, "\"isSubtract\":true", "\"isSubtract\":1")); });
            ChartImportLimits limits;
            limits.max_events = 5;
            rejects([&] { parse(text, limits); });
        }},
        {"only the exact top-level v3 block area field is accepted", [] {
            for (const auto& key : {"blockAreaList", "block area list", "block_area_list"}) {
                for (const auto& value : {"[]", "[{}]", "null", "{}"}) {
                    const auto root = replace(chart(line()), "\"offset\":0", "\"offset\":0,\"" + std::string(key) + "\":" + value);
                    const auto nested = chart(line("[]", "[]", "120", ",\"" + std::string(key) + "\":" + value));
                    rejects([&] { parse(nested); });
                    rejects([&] { candidate(nested); });
                    if (std::string_view(key) == "blockAreaList" &&
                        (std::string_view(value) == "[]" || std::string_view(value) == "null")) {
                        const auto c = parse(root);
                        check(c.block_areas.empty() && !c.import_interpretation_unqualified);
                        check(candidate(root).block_areas.empty());
                    } else {
                        rejects([&] { parse(root); });
                        rejects([&] { candidate(root); });
                    }
                }
            }
        }},
        {"nonfinite and invalid numeric forms rejected", [] {
            const auto text = chart(line("[" + note() + "]"));
            for (const std::string value : {"NaN", "Infinity", "1e9999", "null", "true", "\"32\""})
                rejects([&] { parse(replace(text, "\"time\":32", "\"time\":" + value)); });
            rejects([] { phigros_time_ns(1, 0, 0); });
            rejects([] { phigros_time_ns(std::numeric_limits<double>::quiet_NaN(), 120, 0); });
            rejects([] { phigros_time_ns(1e300, 1, 0); });
        }},
        {"negative note and invalid hold rejected", [] {
            rejects([] { parse(chart(line("[" + note("-1") + "]"))); });
            rejects([] { parse(chart(line("[" + note("0", "3", "-1") + "]"))); });
            rejects([] { parse(chart(line("[" + note("0", "1", "1") + "]"))); });
            rejects([] { parse(chart(line("[]", "[]", "0"))); });
        }},
        {"byte line note event depth and numeric limits enforced", [] {
            const auto text = chart(line("[" + note() + "," + note() + "]"));
            ChartImportLimits limits; limits.max_json_bytes = 8;
            rejects([&] { parse(text, limits); });
            limits = {}; limits.max_lines = 1;
            rejects([&] { parse(chart(line() + "," + line()), limits); });
            limits = {}; limits.max_notes = 1;
            rejects([&] { parse(text, limits); });
            limits = {}; limits.max_json_depth = 1;
            rejects([&] { parse(text, limits); });
            limits = {}; limits.max_abs_offset_seconds = 1;
            rejects([&] { parse(chart("", "2"), limits); });
            limits = {}; limits.max_events = 1;
            auto events = replace(chart(line()), "\"speedEvents\":[]", "\"speedEvents\":[{\"startTime\":0,\"endTime\":1,\"value\":1},{\"startTime\":1,\"endTime\":2,\"value\":1}]");
            rejects([&] { parse(events, limits); });
        }},
        {"reversed events rejected without sorting or dropping", [] {
            const auto text = replace(chart(line()), "\"speedEvents\":[]", "\"speedEvents\":[{\"startTime\":2,\"endTime\":1,\"value\":1}]");
            rejects([&] { parse(text); });
        }},
        {"candidate empty declarations preserved and source priority is explicit", [] {
            auto text=pose_fixture();
            text=replace(text,"\"judgeLineDisappearEvents\":[]",R"("judgeLineDisappearEvents":[
                {"startTime":-999999,"endTime":128,"start":0,"end":0},
                {"startTime":128,"endTime":96,"start":0,"end":0},
                {"startTime":96,"endTime":1000000000,"start":1,"end":1}])");
            rejects([&]{parse(text);});const auto c=candidate(text);const auto& l=c.lines[0];
            check(c.source.chart_sha256==chart_text_sha256(text)&&c.note_count==1&&c.event_count==5);
            check(c.import_policy_id=="phigros-v3-source-priority-constant-empty-candidate-v1"&&c.import_interpretation_unqualified);
            check(l.native_event_declarations&&l.native_event_declarations->opacity_events.size()==3&&l.opacity_events.size()==2);
            const auto& original=l.native_event_declarations->opacity_events[1];
            check(original.time.start_tick==128&&original.time.end_tick==96&&original.start==0&&original.end==0);
            check(original.source.json_pointer=="/judgeLineList/0/judgeLineDisappearEvents/1"&&original.source.id==c.source.chart_sha256+'#'+original.source.json_pointer);
            check(c.import_anomalies.size()==3&&c.import_anomalies[1].code=="empty_reversed_constant_pose_event"&&!c.import_anomalies[1].declaration_evaluated);
            check(c.import_anomalies[1].original_declaration_json.find("\"startTime\":128")!=std::string::npos&&c.import_anomalies[1].evaluated_segments.empty());
            check(c.import_anomalies[2].evaluated_segments.size()==1&&c.import_anomalies[2].evaluated_segments[0].start_tick==128);
            PhigrosPoseEvaluator pose(c);
            check(pose.line_at(0,1'750'000'000).opacity==0&&pose.line_at(0,2'000'000'000).opacity==1);
        }},
        {"constant move reversal does not hide a nonconstant winning declaration", [] {
            auto text=chart(line("["+note()+"]"));
            text=replace(text,"\"judgeLineMoveEvents\":[]",R"("judgeLineMoveEvents":[
                {"startTime":-999999,"endTime":32,"start":0.5,"end":0.5,"start2":0.5,"end2":0.5},
                {"startTime":32,"endTime":64,"start":0.5,"end":0.8,"start2":0.5,"end2":0.5},
                {"startTime":64,"endTime":32,"start":0.5,"end":0.5,"start2":0.5,"end2":0.5},
                {"startTime":32,"endTime":128,"start":0.5,"end":0.5,"start2":0.5,"end2":0.5},
                {"startTime":128,"endTime":1000000000,"start":0.5,"end":0.5,"start2":0.5,"end2":0.5}])");
            text=replace(text,"\"judgeLineRotateEvents\":[]",R"("judgeLineRotateEvents":[{"startTime":-999999,"endTime":1000000000,"start":0,"end":0}])");
            text=replace(text,"\"judgeLineDisappearEvents\":[]",R"("judgeLineDisappearEvents":[{"startTime":-999999,"endTime":1000000000,"start":1,"end":1}])");
            const auto c=candidate(text);const auto& l=c.lines[0];PhigrosPoseEvaluator pose(c);
            check(l.native_event_declarations->move_events.size()==5&&l.move_events.size()==4);
            check(std::abs(pose.line_at(0,750'000'000).origin.x-.65)<1e-12);
            check(pose.line_at(0,1'000'000'000).origin.x==.5&&pose.line_at(0,999'999'999).origin.x>.799999);
            check(l.native_event_declarations->move_events[3].time.start_tick==32&&l.move_events[2].time.start_tick==64);
            check(l.move_events[2].source.id==l.native_event_declarations->move_events[3].source.id);
            check(c.import_anomalies[2].code=="empty_reversed_constant_pose_event");
        }},
        {"out of order opacity source priority preserves fade and later uncovered suffix", [] {
            auto text=pose_fixture();
            text=replace(text,"\"judgeLineDisappearEvents\":[]",R"("judgeLineDisappearEvents":[
                {"startTime":-999999,"endTime":32,"start":1,"end":1},
                {"startTime":32,"endTime":64,"start":1,"end":0},
                {"startTime":64,"endTime":128,"start":0,"end":0},
                {"startTime":128,"endTime":32,"start":0,"end":0},
                {"startTime":32,"endTime":160,"start":1,"end":1},
                {"startTime":160,"endTime":1000000000,"start":0,"end":0}])");
            const auto c=candidate(text);PhigrosPoseEvaluator pose(c);
            check(pose.line_at(0,750'000'000).opacity==.5&&pose.line_at(0,1'500'000'000).opacity==0&&pose.line_at(0,2'250'000'000).opacity==1);
            check(c.lines[0].native_event_declarations->opacity_events[4].time.start_tick==32);
            check(c.lines[0].opacity_events[3].time.start_tick==128&&c.import_anomalies[4].evaluated_segments[0].start_tick==128);
        }},
        {"duplicate constant overlap is shadowed with declarations retained", [] {
            auto text=pose_fixture();
            text=replace(text,R"("judgeLineRotateEvents":[{"startTime":-999999,"endTime":1000000000,"start":0,"end":0}])",R"("judgeLineRotateEvents":[
                {"startTime":-999999,"endTime":128,"start":0,"end":0},
                {"startTime":128,"endTime":96,"start":0,"end":0},
                {"startTime":96,"endTime":128,"start":0,"end":0},
                {"startTime":128,"endTime":160,"start":90,"end":180},
                {"startTime":160,"endTime":1000000000,"start":180,"end":180}])");
            text=replace(text,"\"judgeLineDisappearEvents\":[]",R"("judgeLineDisappearEvents":[{"startTime":-999999,"endTime":1000000000,"start":1,"end":1}])");
            const auto c=candidate(text);PhigrosPoseEvaluator pose(c);
            check(c.lines[0].rotate_events.size()==3&&c.lines[0].native_event_declarations->rotate_events.size()==5);
            check(!c.import_anomalies[2].declaration_evaluated&&c.import_anomalies[2].code=="source_priority_pose_track");
            check(pose.line_at(0,1'999'999'999).rotation_degrees==0&&pose.line_at(0,2'000'000'000).rotation_degrees==90);
        }},
        {"candidate never accepts reversed nonconstant speed or unknown fields", [] {
            const auto base=pose_fixture();
            auto reversed=replace(base,"\"judgeLineDisappearEvents\":[]",R"("judgeLineDisappearEvents":[{"startTime":2,"endTime":1,"start":0,"end":1}])");
            rejects([&]{candidate(reversed);});
            reversed=replace(base,"\"speedEvents\":[]",R"("speedEvents":[{"startTime":2,"endTime":1,"value":1}])");
            rejects([&]{candidate(reversed);});
            reversed=replace(base,R"("startTime":-999999,"endTime":1000000000,"start":0.5,"end":0.5)",R"("startTime":2,"endTime":1,"start":0.5,"end":0.6)");
            rejects([&]{candidate(reversed);});
            rejects([&]{candidate(replace(base,"\"formatVersion\":3","\"formatVersion\":1"));});
            rejects([&]{candidate(replace(base,"\"offset\":0","\"offset\":0,\"unknown\":1"));});
        }},
        {"ordinary zero steps gap hold offset and strict metadata unchanged", [] {
            auto text=chart(line("["+note()+"]"),"0.125");
            text=replace(text,"\"judgeLineMoveEvents\":[]",R"("judgeLineMoveEvents":[{"startTime":-999999,"endTime":1000000000,"start":0.5,"end":0.5,"start2":0.5,"end2":0.5}])");
            text=replace(text,"\"judgeLineRotateEvents\":[]",R"("judgeLineRotateEvents":[
                {"startTime":-999999,"endTime":0,"start":0,"end":0},
                {"startTime":32,"endTime":32,"start":0,"end":90},
                {"startTime":64,"endTime":96,"start":90,"end":180}])");
            text=replace(text,"\"judgeLineDisappearEvents\":[]",R"("judgeLineDisappearEvents":[{"startTime":-999999,"endTime":1000000000,"start":1,"end":1}])");
            const auto strict=parse(text),c=candidate(text);PhigrosPoseEvaluator a(strict),b(c);
            check(strict.import_policy_id=="phigros-v3-strict-v1"&&!strict.import_interpretation_unqualified&&strict.import_anomalies.empty());
            check(!strict.lines[0].native_event_declarations&&c.lines[0].native_event_declarations&&c.import_anomalies.empty());
            check(c.lines[0].notes[0].start_ns==625'000'000&&c.lines[0].notes[0].source.id==strict.lines[0].notes[0].source.id);
            for(const SongNs t:{-2'000'000'000LL,125'000'000LL,375'000'000LL,625'000'000LL,875'000'000LL,2'000'000'000LL})
                check(a.line_at(0,t).rotation_degrees==b.line_at(0,t).rotation_degrees);
            check(b.line_at(0,375'000'000).rotation_degrees==0&&b.line_at(0,625'000'000).rotation_degrees==90&&b.line_at(0,2'000'000'000).rotation_degrees==180);
        }},
        {"ambiguous mixed zero-time malformed tracks and provenance budgets reject", [] {
            auto text=pose_fixture();
            text=replace(text,"\"judgeLineDisappearEvents\":[]",R"("judgeLineDisappearEvents":[
                {"startTime":-999999,"endTime":128,"start":0,"end":0},
                {"startTime":128,"endTime":96,"start":0,"end":0},
                {"startTime":96,"endTime":1000000000,"start":1,"end":1}])");
            auto options=candidate_options();options.max_anomalies=1;rejects([&]{candidate(text,options);});
            options=candidate_options();options.max_evaluated_pose_events=2;rejects([&]{candidate(text,options);});
            options=candidate_options();options.policy=static_cast<PhigrosImportPolicy>(99);rejects([&]{candidate(text,options);});
            text=replace(text,R"({"startTime":128,"endTime":96,"start":0,"end":0})",R"({"startTime":96,"endTime":96,"start":0,"end":1})");
            rejects([&]{candidate(text);});
        }}
    };
    for (const auto& [name, run] : tests) {
        try { run(); }
        catch (const std::exception& error) { std::cerr << "FAIL " << name << ": " << error.what() << '\n'; return 1; }
    }
    std::cout << tests.size() << " synthetic Phigros importer checks passed\n";
    return 0;
}
