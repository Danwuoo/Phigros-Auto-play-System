#include "pas/game.hpp"
#include "pas/analysis.hpp"
#include "pas/adb.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>

namespace {
// Derived line observations only: the owner journal can skip recognition
// frames and does not include the pre-round tracker state. This is never a
// full observer replay, a pixel label, or input to the running application.
int line_history(const std::filesystem::path& root,const std::filesystem::path& output) {
    using namespace pas;
    using json=nlohmann::json;
    if(std::filesystem::exists(output))throw std::invalid_argument("line-history output exists");
    const auto load=[](const std::filesystem::path& path) {
        if(std::filesystem::file_size(path)>2*1024*1024)throw std::invalid_argument("summary capacity");
        std::ifstream input(path);return json::parse(input);
    };
    const auto original=load(root/"summary.json");
    const auto& segments=original.at("event_segments");
    if(!segments.is_array()||segments.empty()||segments.size()>32)throw std::invalid_argument("segment capacity");
    for(const auto& segment:segments) {
        const std::filesystem::path relative=segment.at("path").get<std::string>();
        if(relative.is_absolute()||relative.has_parent_path())throw std::invalid_argument("segment path");
        if(std::filesystem::file_size(root/relative)>32*1024*1024||
           sha256_file(root/relative)!=segment.at("sha256").get<std::string>())
            throw std::invalid_argument("segment bytes/SHA mismatch");
    }
    std::filesystem::create_directories(output);
    std::ofstream frames_file(output/"frames.jsonl");
    GameLineTracker tracker;
    std::uint64_t rows=0,frames=0,playing=0,line_frames=0,invalid_frames=0,all_invalid=0,
        recorded_lines=0,recorded_invalid=0,new_samples=0,replay_invalid=0,replay_diff_frames=0,
        invisible_target_frames=0,invalid_target_frames=0,valid_line_unobservable_targets=0;
    std::vector<double> run_ms;
    json longest=json::array();
    std::optional<Nanoseconds> run_start;
    Nanoseconds last_time=0;std::uint64_t run_frame=0,run_frames=0,last_source_frame=0;
    const auto finish_run=[&] {
        if(!run_start)return;
        const double duration=(last_time-*run_start)/1e6;run_ms.push_back(duration);
        longest.push_back({{"first_frame",run_frame},{"last_frame",last_source_frame},
            {"capture_start_ns",*run_start},{"capture_end_ns",last_time},
            {"duration_ms",duration},{"decision_frames",run_frames}});
        std::sort(longest.begin(),longest.end(),[](const json& a,const json& b){
            return a.at("duration_ms").get<double>()>b.at("duration_ms").get<double>();});
        if(longest.size()>12)longest.erase(longest.end()-1);
        run_start.reset();run_frames=0;
    };
    std::uint64_t prior_frame=0;Nanoseconds prior_time=0;
    for(const auto& segment:segments) {
        std::ifstream input(root/segment.at("path").get<std::string>());
        std::string row;
        while(std::getline(input,row)) {
            if(row.size()>2*1024*1024||++rows>1'000'000)throw std::invalid_argument("journal row capacity");
            const auto entry=json::parse(row);
            if(entry.value("event","")!="game_decision")continue;
            if(++frames>100'000||entry.at("lines").size()>16||entry.at("targets").size()>128)
                throw std::invalid_argument("decision capacity");
            const auto time=entry.at("capture_complete_ns").get<Nanoseconds>();
            const auto frame=entry.at("frame_sequence").get<std::uint64_t>();
            if(prior_frame&&(frame<=prior_frame||time<=prior_time))throw std::invalid_argument("nonincreasing context");
            prior_frame=frame;prior_time=time;
            SceneContext context{entry.at("epoch"),entry.at("generation"),entry.at("geometry_version"),
                frame,time,1280,720,1};
            std::vector<LineCandidate> lines;std::size_t invalid=0;
            for(const auto& source:entry.at("lines")) {
                LineCandidate line;line.center={source.at("x"),source.at("y")};
                line.tangent={source.at("ux"),source.at("uy")};line.length=source.at("length");
                if(!std::isfinite(line.center.x)||!std::isfinite(line.center.y)||!std::isfinite(line.length)||
                   !std::isfinite(line.tangent.x)||!std::isfinite(line.tangent.y)||
                   std::abs(std::hypot(line.tangent.x,line.tangent.y)-1)>.01)
                    throw std::invalid_argument("nonfinite/nonunit line observation");
                lines.push_back(line);
                invalid+=!source.at("association_valid").get<bool>();
            }
            tracker.update(lines,context);std::size_t replay_bad=0;
            for(const auto& line:lines)replay_bad+=!line.association_valid;
            const bool gate=entry.at("playing_gate");
            if(gate) {
                ++playing;recorded_lines+=lines.size();recorded_invalid+=invalid;replay_invalid+=replay_bad;
                if(!lines.empty())++line_frames;
                if(invalid)++invalid_frames;
                bool changed=false;
                for(std::size_t i=0;i<lines.size();++i) {
                    const auto& source=entry.at("lines")[i];
                    new_samples+=source.at("motion_samples")==1;
                    changed|=source.at("association_valid").get<bool>()!=lines[i].association_valid;
                }
                replay_diff_frames+=changed;
                for(const auto& target:entry.at("targets"))if(target.at("reason")=="line_unobservable") {
                    if(lines.empty())++invisible_target_frames;
                    else if(invalid==lines.size())++invalid_target_frames;
                    else ++valid_line_unobservable_targets;
                }
            }
            const bool bad=gate&&!lines.empty()&&invalid==lines.size();
            if(bad&&run_start&&time-last_time>=100'000'000)finish_run();
            if(bad) {
                ++all_invalid;
                if(!run_start){run_start=time;run_frame=frame;}
                ++run_frames;last_time=time;last_source_frame=frame;
            } else finish_run();
            frames_file<<json{{"frame",frame},{"capture_complete_ns",time},{"playing_gate",gate},
                {"line_count",lines.size()},{"invalid_line_count",invalid},{"replay_invalid_line_count",replay_bad},
                {"recorded_lines",entry.at("lines")}}.dump()<<'\n';
        }
    }
    finish_run();frames_file.close();
    const json report={{"schema",1},{"segment_integrity_verified",true},{"decision_frames",frames},
        {"playing_frames",playing},{"playing_line_frames",line_frames},{"playing_any_invalid_line_frames",invalid_frames},
        {"playing_all_lines_invalid_frames",all_invalid},{"playing_line_observations",recorded_lines},
        {"playing_invalid_line_observations",recorded_invalid},{"playing_motion_samples_one_observations",new_samples},
        {"replay_invalid_line_observations",replay_invalid},{"replay_association_different_frames",replay_diff_frames},
        {"line_unobservable_target_frames_no_line",invisible_target_frames},
        {"line_unobservable_target_frames_all_lines_invalid",invalid_target_frames},
        {"line_unobservable_target_frames_some_valid_line",valid_line_unobservable_targets},
        {"all_lines_invalid_run_ms",distribution(run_ms)},{"longest_all_invalid_runs",longest},
        {"round_summary_sha256",sha256_file(root/"summary.json")},{"frames_sha256",sha256_file(output/"frames.jsonl")},
        {"source_absolute_age",nullptr},{"human_gold",0},{"real_input_backend_constructed",false},
        {"replay_scope","journal-derived current line geometry; no pixels, no note/owner replay"},
        {"replay_missing","pre-round tracker state and skipped recognition decisions; tangent signs already aligned"},
        {"replay_context_assumption","verified campaign profile: 1280x720 rotation1"},
        {"capacity",{{"segments",32},{"segment_bytes",32*1024*1024},{"rows",1'000'000},{"decisions",100'000},{"line_candidates",16}}}};
    std::ofstream summary(output/"summary.json");summary<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';
    return 0;
}
}

// Read-only, bounded replay. No emulator or real input backend is constructed.
// Clip resets make the missing warm state explicit; predictions are not labels.
int main(int argc,char** argv) {
    using namespace pas;
    using json=nlohmann::json;
    try {
        if(argc==4&&std::string(argv[1])=="line-history")return line_history(argv[2],argv[3]);
        if(argc==5&&std::string(argv[1])=="compare") {
            std::ifstream a(argv[2]),b(argv[3]);
            const std::filesystem::path output=argv[4];
            if(!a||!b||std::filesystem::exists(output))throw std::invalid_argument("compare inputs/output");
            std::filesystem::create_directories(output);
            std::ofstream diffs(output/"differences.jsonl");
            std::string ar,br;int frames=0,changed=0,line_changed=0,target_changed=0,geometry_changed=0,without_ids_changed=0;
            const auto strip_ids=[](json scene) {
                for(auto& line:scene.at("lines"))line.erase("line_id");
                for(auto& target:scene.at("targets")) {target.erase("line_id");target.erase("note_id");}
                return scene;
            };
            while(std::getline(a,ar)) {
                if(!std::getline(b,br)||ar.size()>2*1024*1024||br.size()>2*1024*1024||++frames>2048)
                    throw std::invalid_argument("compare capacity/length");
                const auto aj=json::parse(ar),bj=json::parse(br);
                for(const auto* key:{"round","clip","frame","rgb_sha256","capture_complete_ns"})
                    if(aj.at(key)!=bj.at(key))throw std::invalid_argument("compare frame provenance mismatch");
                const auto& as=aj.at("scene");const auto& bs=bj.at("scene");
                if(as.at("lines")!=bs.at("lines"))++line_changed;
                if(as.at("targets")!=bs.at("targets"))++target_changed;
                const auto ag=strip_ids(as),bg=strip_ids(bs);
                if(ag.at("lines")!=bg.at("lines"))++geometry_changed;
                if(ag.at("targets")!=bg.at("targets"))++without_ids_changed;
                if(as!=bs) {++changed;diffs<<json{{"round",aj.at("round")},{"clip",aj.at("clip")},
                    {"frame",aj.at("frame")},{"rgb_sha256",aj.at("rgb_sha256")},{"a",as},{"b",bs}}.dump()<<'\n';}
            }
            if(std::getline(b,br)||!frames)throw std::invalid_argument("compare unequal/empty inputs");
            const json summary={{"frames",frames},{"decision_different_frames",changed},
                {"line_different_frames",line_changed},{"target_different_frames",target_changed},
                {"line_different_frames_without_numeric_ids",geometry_changed},
                {"target_different_frames_without_numeric_ids",without_ids_changed},
                {"numeric_id_renumbering_is_not_identity_gold",true},
                {"a_sha256",sha256_file(argv[2])},{"b_sha256",sha256_file(argv[3])},
                {"human_gold",0},{"correctness_from_difference",false}};
            std::ofstream report(output/"summary.json");report<<summary.dump(2)<<'\n';std::cout<<summary.dump(2)<<'\n';
            return 0;
        }
        if(argc!=3&&argc!=5)throw std::invalid_argument("usage: pas_pixel_audit clip-root new-output-dir [png-round png-clip]");
        const std::filesystem::path root=argv[1],output=argv[2];
        if(std::filesystem::exists(output))throw std::invalid_argument("output already exists");
        std::ifstream index(root/"index.jsonl",std::ios::binary);
        if(!index)throw std::invalid_argument("missing clip index");
        std::filesystem::create_directories(output);
        std::ofstream decisions(output/"decisions.jsonl",std::ios::binary);
        if(!decisions)throw std::runtime_error("output open failed");
        FakeClock clock;
        GameObserver observer(clock);
        std::unique_ptr<FakeTouchBackend> touch;
        std::unique_ptr<GamePlanOwner> owner;
        int prior_round=-1,prior_clip=-1,frames=0,clips=0;
        Nanoseconds prior_time=0;
        std::uint64_t prior_sequence=0;
        std::vector<double> compute_ms;
        std::map<std::string,std::uint64_t> reasons;
        std::uint64_t targets=0,no_root=0,down=0,up=0,unreleased=0;
        const auto finish=[&] {
            if(!owner)return;
            owner->stop();
            unreleased+=touch->contacts().size();
            for(const auto& receipt:touch->receipts()) {
                if(receipt.command.phase==Phase::down)++down;
                if(receipt.command.phase==Phase::up)++up;
            }
            owner.reset();touch.reset();
        };
        std::string row;
        while(std::getline(index,row)) {
            if(row.size()>64*1024)throw std::invalid_argument("index row capacity");
            if(row.empty())continue;
            if(++frames>2048)throw std::invalid_argument("frame capacity");
            const auto entry=json::parse(row);
            const int round=entry.at("round_id"),clip=entry.at("clip_id");
            const auto relative=std::filesystem::path(entry.at("path").get<std::string>());
            if(relative.is_absolute()||relative.has_root_name())throw std::invalid_argument("absolute RGB path");
            for(const auto& part:relative)if(part=="..")throw std::invalid_argument("escaping RGB path");
            if(entry.at("width")!=1280||entry.at("height")!=720||entry.at("stride")!=3840||
               entry.at("layout")!="top_down_rgb888")throw std::invalid_argument("RGB geometry/layout");
            Frame frame;frame.width=1280;frame.height=720;frame.stride=3840;frame.source_rotation=1;
            frame.epoch=frame.generation=frame.geometry_version=1;
            frame.sequence=entry.at("source_frame");frame.capture_complete_ns=entry.at("capture_complete_ns");
            if(round!=prior_round||clip!=prior_clip) {
                finish();observer.reset();++clips;
                touch=std::make_unique<FakeTouchBackend>(clock);
                owner=std::make_unique<GamePlanOwner>(clock,*touch,5,GameActionOptions{15,35'000'000,30'000'000});
            } else if(frame.capture_complete_ns<=prior_time||frame.sequence<=prior_sequence)
                throw std::invalid_argument("nonincreasing clip context");
            prior_round=round;prior_clip=clip;prior_time=frame.capture_complete_ns;prior_sequence=frame.sequence;
            const auto path=root/relative;
            if(std::filesystem::file_size(path)!=3840*720||sha256_file(path)!=entry.at("sha256").get<std::string>())
                throw std::invalid_argument("RGB bytes/SHA mismatch");
            frame.rgb.resize(3840*720);
            std::ifstream pixels(path,std::ios::binary);
            pixels.read(reinterpret_cast<char*>(frame.rgb.data()),static_cast<std::streamsize>(frame.rgb.size()));
            if(pixels.gcount()!=static_cast<std::streamsize>(frame.rgb.size()))throw std::invalid_argument("RGB truncated");
            clock.set(frame.capture_complete_ns);
            HostClock meter;const auto begin=meter.now_ns();
            const auto scene=observer.process(frame);
            const auto elapsed=meter.now_ns()-begin;compute_ms.push_back(elapsed/1e6);
            owner->accept(scene);owner->poll();
            for(const auto& target:scene.targets) {++targets;++reasons[target.reason];if(!target.crossing_ns)++no_root;}
            decisions<<json{{"round",round},{"clip",clip},{"frame",frame.sequence},
                {"rgb_sha256",entry.at("sha256")},{"capture_complete_ns",frame.capture_complete_ns},
                {"cold_clip_reset",entry.at("frame_in_clip")==0},{"scene",decision_json(scene)},
                {"observer_compute_ns",elapsed},{"fake_receipts",touch->receipts().size()}}.dump()<<'\n';
            if(argc==5&&round==std::stoi(argv[3])&&clip==std::stoi(argv[4])) {
                const auto name=std::to_string(frame.sequence);
                write_diagnostic_png(output/(name+"-raw.png"),frame);
                auto annotated=frame;draw_game_overlay(annotated,scene);
                write_diagnostic_png(output/(name+"-overlay.png"),annotated);
            }
        }
        finish();decisions.close();
        if(!frames)throw std::invalid_argument("empty index");
        const json summary={{"schema",1},{"frames",frames},{"clips",clips},{"target_frames",targets},
            {"target_frames_without_root",no_root},{"reasons",reasons},
            {"fake_down",down},{"fake_scheduled_up",up},{"fake_contacts_after_stop",unreleased},
            {"stop_releases_are_separate_from_scheduled_up",true},{"observer_compute_ms",distribution(compute_ms)},
            {"index_sha256",sha256_file(root/"index.jsonl")},{"decisions_sha256",sha256_file(output/"decisions.jsonl")},
            {"warm_state_available",false},{"source_absolute_age",nullptr},{"labels","proposed_or_unknown; human_gold=0"},
            {"runtime_feedback",false},{"frame_limit",2048}};
        std::ofstream report(output/"summary.json");report<<summary.dump(2)<<'\n';
        std::cout<<summary.dump(2)<<'\n';
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
