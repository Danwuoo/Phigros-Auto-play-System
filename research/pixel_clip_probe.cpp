// Replay only a bounded saved three-frame RGB clip through current observer.
// Resetting before the clip cannot reconstruct private pre-clip history.
#include "pas/game.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace pas;
using nlohmann::json;
int main(int argc,char** argv) {
    try {
        if(argc!=5)throw std::invalid_argument("usage: pixel_clip_probe index.jsonl round clip output.json");
        const std::filesystem::path index_path=argv[1],output_path=argv[4];
        if(std::filesystem::exists(output_path))throw std::runtime_error("output already exists");
        const int round=std::stoi(argv[2]),clip=std::stoi(argv[3]);
        std::ifstream index(index_path);if(!index)throw std::runtime_error("index missing");
        std::vector<json> records;std::string row;
        while(std::getline(index,row)) {
            const auto j=json::parse(row);
            if(j.at("round_id")==round&&j.at("clip_id")==clip)records.push_back(j);
        }
        if(records.size()!=3)throw std::runtime_error("expected exactly three indexed frames");
        std::sort(records.begin(),records.end(),[](const auto& a,const auto& b){
            return a.at("frame_in_clip").template get<int>()<b.at("frame_in_clip").template get<int>();});
        FakeClock clock;GameObserver observer(clock);json frames=json::array();
        for(const auto& record:records) {
            Frame frame;frame.width=record.at("width");frame.height=record.at("height");
            frame.stride=record.at("stride");
            if(frame.width!=1280||frame.height!=720||frame.stride!=3840||
               record.at("layout")!="top_down_rgb888")throw std::runtime_error("unsupported frame layout");
            frame.epoch=1;frame.generation=1;frame.geometry_version=1;
            frame.sequence=record.at("source_frame");frame.capture_complete_ns=record.at("capture_complete_ns");
            frame.pixels_ready_ns=frame.capture_complete_ns;
            frame.rgb.resize(static_cast<std::size_t>(frame.stride)*frame.height);
            const auto path=index_path.parent_path()/record.at("path").get<std::string>();
            std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("RGB missing");
            input.read(reinterpret_cast<char*>(frame.rgb.data()),static_cast<std::streamsize>(frame.rgb.size()));
            if(input.gcount()!=static_cast<std::streamsize>(frame.rgb.size())||input.peek()!=EOF)
                throw std::runtime_error("RGB length mismatch");
            clock.set(frame.capture_complete_ns);const auto scene=observer.process(frame);
            json lines=json::array(),targets=json::array();
            for(const auto& line:scene.lines)lines.push_back({{"id",line.track_id},
                {"association_valid",line.association_valid},{"center",{line.center.x,line.center.y}},
                {"tangent",{line.tangent.x,line.tangent.y}}});
            for(const auto& target:scene.targets)targets.push_back({{"note_id",target.note_id},
                {"line_id",target.line_id},{"reason",target.reason},{"kind",name(target.note.kind)},
                {"crossing_ns",target.crossing_ns?json(*target.crossing_ns):json(nullptr)}});
            frames.push_back({{"source_frame",frame.sequence},{"capture_ns",frame.capture_complete_ns},
                {"input_sha256",record.at("sha256")},{"historical_detected_lines",record.at("detected_lines")},
                {"historical_detected_targets",record.at("detected_targets")},
                {"playing_gate",scene.playing_gate},{"lines",lines},{"targets",targets}});
        }
        const json result={{"kind","local_three_frame_pixel_replay"},
            {"reset_before_clip",true},{"private_prior_state_available",false},
            {"game_judgment_truth_available",false},{"round_id",round},{"clip_id",clip},{"frames",frames}};
        std::ofstream output(output_path);if(!output)throw std::runtime_error("output unavailable");
        output<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
