#include "rhythm/screen_state_io.hpp"
#include "rhythm/chart.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace rhythm {
namespace {
using Json=nlohmann::json;
constexpr std::size_t max_json_bytes=1024*1024;
int pixel_integer(const Json& value) {
    if(!value.is_number_integer())throw std::invalid_argument("pixel geometry must contain integers");
    if(value.is_number_unsigned()) {
        const auto number=value.get<std::uint64_t>();
        if(number>static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            throw std::invalid_argument("pixel integer overflow");
        return static_cast<int>(number);
    }
    const auto number=value.get<std::int64_t>();
    if(number<std::numeric_limits<int>::min()||number>std::numeric_limits<int>::max())
        throw std::invalid_argument("pixel integer overflow");
    return static_cast<int>(number);
}
std::string bounded_read(const std::filesystem::path& path,std::size_t max_bytes) {
    if(std::filesystem::file_size(path)>max_bytes)throw std::invalid_argument("file exceeds calibration budget");
    std::ifstream stream(path,std::ios::binary);
    if(!stream)throw std::runtime_error("cannot open calibration input");
    std::string bytes;
    std::array<char,65536> buffer{};
    while(stream) {
        const auto wanted=std::min(buffer.size(),max_bytes-bytes.size()+1);
        stream.read(buffer.data(),static_cast<std::streamsize>(wanted));
        const auto count=static_cast<std::size_t>(stream.gcount());
        if(count>max_bytes-bytes.size())throw std::invalid_argument("input grew beyond calibration budget");
        bytes.append(buffer.data(),count);
        if(stream.bad()||(!stream.eof()&&stream.fail()))throw std::runtime_error("calibration input read failed");
    }
    return bytes;
}
ObservedGameState parse_template_state(const std::string& name) {
    if(name=="playing")return ObservedGameState::playing;
    if(name=="paused")return ObservedGameState::paused;
    if(name=="result")return ObservedGameState::result;
    throw std::invalid_argument("invalid calibrated template state");
}
Json bounded_json(std::string_view raw) {
    if(raw.size()>max_json_bytes)throw std::invalid_argument("dataset JSON exceeds budget");
    std::map<int,std::set<std::string>> keys;
    return Json::parse(raw.begin(),raw.end(),[&](int depth,Json::parse_event_t event,Json& parsed) {
        if(depth>32)throw std::invalid_argument("dataset JSON exceeds depth budget");
        if(event==Json::parse_event_t::object_start)keys[depth].clear();
        else if(event==Json::parse_event_t::object_end)keys.erase(depth);
        else if(event==Json::parse_event_t::key) {
            if(!keys[depth-1].insert(parsed.get<std::string>()).second)
                throw std::invalid_argument("duplicate dataset JSON key");
        }
        return true;
    });
}
} // namespace

ScreenStateCalibration parse_screen_state_calibration(std::string_view raw,const std::filesystem::path& base) {
    const auto dataset=bounded_json(raw);
    if(dataset.at("schema")!="pas.screen-state-dataset.v1")throw std::invalid_argument("dataset schema");
    ScreenStateCalibration result;
    result.width=pixel_integer(dataset.at("width"));result.height=pixel_integer(dataset.at("height"));
    if(result.width<2||result.height<2||result.width>4096||result.height>4096)
        throw std::invalid_argument("geometry budget");
    result.dataset_sha256=chart_text_sha256(raw);
    const auto& references=dataset.at("templates");
    if(!references.is_array()||references.empty()||references.size()>8)
        throw std::invalid_argument("calibration template count budget");
    const auto size=static_cast<std::size_t>(result.width)*result.height*3;
    std::set<std::string> calibration_ids;
    for(const auto& item:references) {
        const auto id=item.at("calibration_id").get<std::string>();
        if(id.empty()||id.size()>256||!calibration_ids.insert(id).second)
            throw std::invalid_argument("invalid/duplicate calibration id");
        const auto& rgb=item.at("rgb");
        const auto raw_path=rgb.at("path").get<std::string>();
        if(raw_path.empty()||raw_path.size()>32768||raw_path.find('\0')!=std::string::npos)
            throw std::invalid_argument("invalid calibration RGB path");
        auto path=std::filesystem::path(raw_path);
        if(path.is_relative())path=base/path;
        const auto expected=rgb.at("sha256").get<std::string>();
        if(!sha256_text(expected))throw std::invalid_argument("invalid calibration RGB hash");
        const auto bytes=bounded_read(path,size);
        if(bytes.size()!=size||chart_text_sha256(bytes)!=expected)
            throw std::invalid_argument("RGB size/hash mismatch");
        const std::vector<std::uint8_t> pixels(bytes.begin(),bytes.end());
        const auto& rows=item.at("regions");
        if(!rows.is_array()||rows.size()<2||rows.size()>8)throw std::invalid_argument("region count");
        std::vector<PixelRegion> regions;
        for(const auto& row:rows) {
            if(!row.is_array()||row.size()!=4)throw std::invalid_argument("region shape");
            regions.push_back({pixel_integer(row[0]),pixel_integer(row[1]),pixel_integer(row[2]),pixel_integer(row[3])});
        }
        result.templates.push_back(make_screen_state_template({result.width,result.height,result.width*3,pixels},
            parse_template_state(item.at("state").get<std::string>()),std::move(regions),
            item.at("source_image_sha256").get<std::string>(),id));
    }
    return result;
}

ScreenStateCalibration load_screen_state_calibration(const std::filesystem::path& path) {
    const auto source=std::filesystem::absolute(path);
    return parse_screen_state_calibration(bounded_read(source,max_json_bytes),source.parent_path());
}
} // namespace rhythm
