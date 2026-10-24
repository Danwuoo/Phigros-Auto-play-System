#include "rhythm/screen_state_io.hpp"
#include "rhythm/chart.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <array>
#include <set>
#include <stdexcept>

namespace {
using Json=nlohmann::json;
using State=rhythm::ObservedGameState;
std::string read(const std::filesystem::path& path,std::uintmax_t max) {
    if(std::filesystem::file_size(path)>max)throw std::invalid_argument("file exceeds budget");
    std::ifstream stream(path,std::ios::binary);
    if(!stream)throw std::runtime_error("cannot open input");
    std::string bytes;std::array<char,65536> buffer{};
    while(stream) {
        const auto wanted=std::min<std::uintmax_t>(buffer.size(),max-bytes.size()+1);
        stream.read(buffer.data(),static_cast<std::streamsize>(wanted));
        const auto count=static_cast<std::size_t>(stream.gcount());
        if(count>max-bytes.size())throw std::runtime_error("input grew beyond budget");
        bytes.append(buffer.data(),count);
        if(stream.bad()||(!stream.eof()&&stream.fail()))throw std::runtime_error("input read failed");
    }
    return bytes;
}
State parse_state(const std::string& name) {
    if(name=="playing")return State::playing;
    if(name=="paused")return State::paused;
    if(name=="result")return State::result;
    if(name=="unknown")return State::unknown;
    throw std::invalid_argument("invalid state name");
}
const char* name(State state) {
    switch(state) {
    case State::playing:return "playing";
    case State::paused:return "paused";
    case State::result:return "result";
    default:return "unknown";
    }
}
std::vector<std::uint8_t> rgb(const Json& item,const std::filesystem::path& base,std::size_t count) {
    auto path=std::filesystem::path(item.at("path").get<std::string>());
    if(path.is_relative())path=base/path;
    const auto bytes=read(path,count);
    if(bytes.size()!=count||rhythm::chart_text_sha256(bytes)!=item.at("sha256").get<std::string>())
        throw std::invalid_argument("RGB size/hash mismatch");
    return {bytes.begin(),bytes.end()};
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::invalid_argument("Usage: screen_state_probe DATASET.json (offline RGB fixtures only)");
        const auto source=std::filesystem::absolute(argv[1]);const auto raw=read(source,1024*1024);
        const auto calibration=rhythm::parse_screen_state_calibration(raw,source.parent_path());
        const auto dataset=Json::parse(raw);const auto& samples=dataset.at("samples");
        const int width=calibration.width,height=calibration.height;
        if(!samples.is_array()||samples.empty()||samples.size()>256)throw std::invalid_argument("dataset count budget");
        const auto size=static_cast<std::size_t>(width)*height*3;
        std::set<std::string> sample_ids;
        Json output={{"schema","pas.screen-state-probe.v1"},{"dataset_sha256",calibration.dataset_sha256},
            {"live_qualified",false},{"scope","offline pixel anchors; RGB hashes verified; source PNG hashes and labels supplied by dataset producer; no timestamps or game identity inferred"},
            {"samples",Json::array()}};
        bool all=true;
        for(const auto& item:samples) {
            const auto id=item.at("id").get<std::string>();
            if(id.empty()||id.size()>256||!sample_ids.insert(id).second)throw std::invalid_argument("invalid/duplicate sample id");
            const auto pixels=rgb(item.at("rgb"),source.parent_path(),size);
            const auto result=rhythm::classify_screen_state({width,height,width*3,pixels},calibration.templates);
            const auto expected=parse_state(item.at("expected_state"));const bool match=expected==result.state;
            all=all&&match;Json scores=Json::array();
            for(const auto& score:result.scores)scores.push_back({{"state",name(score.state)},
                {"calibration_id",score.calibration_id},{"worst_anchor_mean_error",score.worst_anchor_mean_error},
                {"all_anchors_match",score.all_anchors_match}});
            output["samples"].push_back({{"id",id},{"expected_state",name(expected)},{"state",name(result.state)},
                {"reason",result.reason},{"matched_expected",match},{"scores",std::move(scores)}});
        }
        output["all_expected_labels_match"]=all;
        std::cout<<output.dump(2)<<'\n';return all?0:2;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
