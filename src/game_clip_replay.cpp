#include "pas/game.hpp"
#include "pas/analysis.hpp"
#include "pas/adb.hpp"

#include <fstream>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace pas {

nlohmann::json replay_game_pixel_clips(const std::filesystem::path& clips_directory,
    const std::filesystem::path& overlay_directory,int overlay_round,int overlay_clip,
    bool reuse_component_scratch,bool row_prescreen,int horizontal_line_gap_limit) {
    using nlohmann::json;
    if(overlay_directory.empty()!=(overlay_round==0&&overlay_clip==0)||
       (!overlay_directory.empty()&&(overlay_round<1||overlay_round>20||overlay_clip<1||overlay_clip>10)))
        throw std::invalid_argument("pixel clip overlay selection");
    if(!overlay_directory.empty()) std::filesystem::create_directories(overlay_directory);
    const auto index_path=clips_directory/"index.jsonl";
    if(std::filesystem::file_size(index_path)>16*1024*1024)
        throw std::invalid_argument("pixel clip index capacity");
    std::ifstream index(index_path,std::ios::binary);
    if(!index) throw std::runtime_error("cannot open pixel clip index");
    FakeClock clock;
    HostClock compute_clock;
    std::unique_ptr<GameObserver> observer;
    json frames=json::array();
    std::vector<double> compute_ms;
    std::vector<double> components_ms,base_scene_ms,held_recovery_ms,tracking_ms;
    std::vector<double> combo_glyph_ms,line_scan_ms,component_line_decode_ms;
    std::vector<double> note_decode_ms,line_tracking_ms;
    std::unordered_set<std::string> distinct_hashes;
    std::string line,prior_key;
    std::size_t clips=0,expected_frame=0;
    std::uint64_t prior_source=0;
    while(std::getline(index,line)) {
        if(line.empty()) continue;
        if(frames.size()>=600) throw std::invalid_argument("pixel clip frame capacity");
        const auto item=json::parse(line);
        const int round=item.at("round_id").get<int>(),clip=item.at("clip_id").get<int>();
        const auto frame_in_clip=item.at("frame_in_clip").get<int>();
        if(round<1||round>20||clip<1||clip>10||frame_in_clip<0||frame_in_clip>2)
            throw std::invalid_argument("pixel clip identity range");
        const auto key=std::to_string(round)+":"+std::to_string(clip);
        if(key!=prior_key) {
            if(!prior_key.empty()&&expected_frame!=3) throw std::invalid_argument("incomplete pixel clip");
            if(frame_in_clip!=0||clips>=200) throw std::invalid_argument("pixel clip order/capacity");
            observer=std::make_unique<GameObserver>(clock,reuse_component_scratch,
                row_prescreen,horizontal_line_gap_limit);
            prior_key=key;expected_frame=0;++clips;
        }
        if(frame_in_clip!=static_cast<int>(expected_frame++))
            throw std::invalid_argument("pixel clip frame order");
        const auto relative=item.at("path").get<std::string>();
        const auto relative_path=std::filesystem::path(relative);
        if(relative.empty()||relative_path.is_absolute()||relative.find("..")!=std::string::npos||
           relative.rfind("round-",0)!=0||relative_path.extension()!=".rgb")
            throw std::invalid_argument("pixel clip path");
        if(item.at("layout")!="top_down_rgb888"||item.at("width")!=1280||
           item.at("height")!=720||item.at("stride")!=3840)
            throw std::invalid_argument("pixel clip geometry/layout");
        const auto path=clips_directory/relative_path;
        const auto hash=sha256_file(path);
        if(hash!=item.at("sha256").get<std::string>())
            throw std::runtime_error("pixel clip SHA256 mismatch: "+relative);
        distinct_hashes.insert(hash);
        constexpr std::size_t bytes=1280ULL*720*3;
        if(std::filesystem::file_size(path)!=bytes) throw std::invalid_argument("pixel clip RGB length");
        Frame frame;frame.width=1280;frame.height=720;frame.stride=3840;
        frame.source_rotation=1;
        frame.epoch=1;frame.generation=1;frame.geometry_version=1;
        frame.sequence=item.at("source_frame").get<std::uint64_t>();
        frame.capture_complete_ns=item.at("capture_complete_ns").get<Nanoseconds>();
        if(frame_in_clip&&frame.sequence<=prior_source) throw std::invalid_argument("pixel clip source order");
        prior_source=frame.sequence;
        frame.rgb.resize(bytes);
        std::ifstream raw(path,std::ios::binary);
        if(!raw||!raw.read(reinterpret_cast<char*>(frame.rgb.data()),bytes))
            throw std::runtime_error("cannot read pixel clip RGB");
        clock.set(frame.capture_complete_ns);
        const auto start=compute_clock.now_ns();
        const auto scene=observer->process(frame);
        const auto elapsed=(compute_clock.now_ns()-start)/1e6;
        compute_ms.push_back(elapsed);
        components_ms.push_back(scene.components_compute_ns/1e6);
        base_scene_ms.push_back(scene.base_scene_compute_ns/1e6);
        combo_glyph_ms.push_back(scene.combo_glyph_compute_ns/1e6);
        line_scan_ms.push_back(scene.line_scan_compute_ns/1e6);
        component_line_decode_ms.push_back(scene.component_line_decode_compute_ns/1e6);
        note_decode_ms.push_back(scene.note_decode_compute_ns/1e6);
        line_tracking_ms.push_back(scene.line_tracking_compute_ns/1e6);
        held_recovery_ms.push_back(scene.held_recovery_compute_ns/1e6);
        tracking_ms.push_back(scene.tracking_compute_ns/1e6);
        auto semantic_decision=decision_json(scene);
        semantic_decision.erase("compute_clock_domain");
        for(const auto* field:{"components_compute_ns","base_scene_compute_ns",
                "combo_glyph_compute_ns","line_scan_compute_ns",
                "component_line_decode_compute_ns","note_decode_compute_ns",
                "line_tracking_compute_ns","held_recovery_compute_ns","tracking_compute_ns"})
            semantic_decision.erase(field);
        if(!overlay_directory.empty()&&round==overlay_round&&clip==overlay_clip) {
            const auto suffix=std::to_string(frame_in_clip)+".png";
            write_diagnostic_png(overlay_directory/("source-"+suffix),frame);
            std::ofstream decision(overlay_directory/(
                "decision-"+std::to_string(frame_in_clip)+".json"),std::ios::binary);
            if(!decision||!(decision<<decision_json(scene).dump(2)<<'\n'))
                throw std::runtime_error("cannot write selected pixel clip decision");
            draw_game_overlay(frame,scene);
            write_diagnostic_png(overlay_directory/("proposed-overlay-"+suffix),frame);
        }
        frames.push_back({{"round_id",round},{"clip_id",clip},{"frame_in_clip",frame_in_clip},
            {"source_frame",frame.sequence},{"capture_complete_ns",frame.capture_complete_ns},
            {"source_rotation",frame.source_rotation},
            {"relative_path",relative},{"clip_trigger",item.value("clip_trigger","unknown")},
            {"sha256",hash},{"playing_gate",scene.playing_gate},
            {"ui",name(scene.ui)},{"cold_lines",scene.lines.size()},
            {"cold_targets",scene.targets.size()},
            {"recorded_warm_lines",item.at("detected_lines")},
            {"recorded_warm_targets",item.at("detected_targets")},
            {"semantic_decision",std::move(semantic_decision)},
            {"compute_ms",elapsed},{"components_compute_ms",scene.components_compute_ns/1e6},
            {"base_scene_compute_ms",scene.base_scene_compute_ns/1e6},
            {"combo_glyph_compute_ms",scene.combo_glyph_compute_ns/1e6},
            {"line_scan_compute_ms",scene.line_scan_compute_ns/1e6},
            {"component_line_decode_compute_ms",scene.component_line_decode_compute_ns/1e6},
            {"note_decode_compute_ms",scene.note_decode_compute_ns/1e6},
            {"line_tracking_compute_ms",scene.line_tracking_compute_ns/1e6},
            {"held_recovery_compute_ms",scene.held_recovery_compute_ns/1e6},
            {"tracking_compute_ms",scene.tracking_compute_ns/1e6}});
    }
    if(!index.eof()||(!prior_key.empty()&&expected_frame!=3))
        throw std::runtime_error("pixel clip index read/incomplete");
    return {{"schema_version",1},{"replay_mode","cold_start_per_three_frame_clip"},
        {"reuse_component_scratch",reuse_component_scratch},
        {"row_prescreen",row_prescreen},
        {"horizontal_line_gap_limit",horizontal_line_gap_limit},
        {"frames",frames},{"indexed_rgb_frames",frames.size()},
        {"unique_rgb_frames",distinct_hashes.size()},{"clips",clips},
        {"observer_compute_ms",distribution(compute_ms)},
        {"components_compute_ms",distribution(components_ms)},
        {"base_scene_compute_ms",distribution(base_scene_ms)},
        {"combo_glyph_compute_ms",distribution(combo_glyph_ms)},
        {"line_scan_compute_ms",distribution(line_scan_ms)},
        {"component_line_decode_compute_ms",distribution(component_line_decode_ms)},
        {"note_decode_compute_ms",distribution(note_decode_ms)},
        {"line_tracking_compute_ms",distribution(line_tracking_ms)},
        {"held_recovery_compute_ms",distribution(held_recovery_ms)},
        {"tracking_compute_ms",distribution(tracking_ms)},
        {"input_integrity_verified",true},{"input_created",false},
        {"recorded_count_semantics","warm runtime context; count differences are not validated errors"},
        {"human_reviewed_truth",false},{"game_effect","unknown"}};
}

nlohmann::json benchmark_game_pixel_clips(const std::filesystem::path& clips_directory,
    int batches,int replays_per_mode,bool benchmark_row_prescreen,bool row_prescreen_aa) {
    using nlohmann::json;
    if(batches<1||batches>5||replays_per_mode<1||replays_per_mode>10)
        throw std::invalid_argument("pixel clip benchmark batch/replay capacity");
    if(row_prescreen_aa&&!benchmark_row_prescreen)
        throw std::invalid_argument("row prescreen A/A requires row prescreen benchmark mode");
    json reference;
    json batch_results=json::array();
    std::vector<double> all_local,all_reused;
    std::size_t indexed_frames=0,unique_frames=0;
    for(int batch=0;batch<batches;++batch) {
        std::vector<double> local,reused;
        for(int run=0;run<replays_per_mode*2;++run) {
            // ABBA order balances startup and thermal drift within a batch.
            const bool second_mode=run%4==1||run%4==2;
            const auto output=replay_game_pixel_clips(clips_directory,{},0,0,
                benchmark_row_prescreen?false:second_mode,
                benchmark_row_prescreen?(second_mode&&!row_prescreen_aa):true);
            auto signature=output.at("frames");
            for(auto& frame:signature) {
                frame.erase("compute_ms");frame.erase("components_compute_ms");
                frame.erase("base_scene_compute_ms");frame.erase("held_recovery_compute_ms");
                frame.erase("tracking_compute_ms");
                frame.erase("combo_glyph_compute_ms");frame.erase("line_scan_compute_ms");
                frame.erase("component_line_decode_compute_ms");frame.erase("note_decode_compute_ms");
                frame.erase("line_tracking_compute_ms");
            }
            if(reference.is_null()) {
                reference=std::move(signature);
                indexed_frames=reference.size();
                unique_frames=output.at("unique_rgb_frames").get<std::size_t>();
            }
            else if(signature!=reference) throw std::runtime_error("pixel clip A/B output or coverage differs");
            auto& samples=second_mode?reused:local;
            for(const auto& frame:output.at("frames")) samples.push_back(frame.at("compute_ms").get<double>());
        }
        if(local.size()!=indexed_frames*static_cast<std::size_t>(replays_per_mode)||
           reused.size()!=local.size()) throw std::runtime_error("pixel clip A/B sample coverage");
        all_local.insert(all_local.end(),local.begin(),local.end());
        all_reused.insert(all_reused.end(),reused.begin(),reused.end());
        batch_results.push_back({{"batch",batch+1},
            {benchmark_row_prescreen?"full_row_scan_first_ms":"local_allocation_ms",distribution(local)},
            {benchmark_row_prescreen?(row_prescreen_aa?"full_row_scan_second_ms":"prescreen_ms"):
                "reused_scratch_ms",distribution(reused)},
            {"coverage_equal",true}});
    }
    return {{"schema_version",1},{"benchmark",benchmark_row_prescreen?
                (row_prescreen_aa?"cold_start_three_frame_clip_full_scan_aa":
                "cold_start_three_frame_clip_row_prescreen_ab"):
                "cold_start_three_frame_clip_observer_compute_only"},
        {"indexed_rgb_frames",indexed_frames},{"unique_rgb_frames",unique_frames},{"batches",batches},
        {"replays_per_mode_per_batch",replays_per_mode},{"per_batch",batch_results},
        {benchmark_row_prescreen?"full_row_scan_first_ms":"local_allocation_ms",distribution(all_local)},
        {benchmark_row_prescreen?(row_prescreen_aa?"full_row_scan_second_ms":"prescreen_ms"):
            "reused_scratch_ms",distribution(all_reused)},
        {"coverage_equal",true},{"input_integrity_verified_each_replay",true},
        {"captures_or_touch_created",false},{"game_effect","unknown"}};
}

} // namespace pas
