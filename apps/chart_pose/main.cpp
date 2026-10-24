#include "rhythm/phigros_pose.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument("Usage: chart_pose CHART_JSON EXPECTED_SHA256 SONG_NS");
        const std::string text_time=argv[3];
        rhythm::SongNs song=0;
        const auto parsed=std::from_chars(text_time.data(),text_time.data()+text_time.size(),song);
        if (parsed.ec!=std::errc{} || parsed.ptr!=text_time.data()+text_time.size())
            throw std::invalid_argument("SONG_NS must be an integer");
        if (std::filesystem::file_size(argv[1])>32*1024*1024)
            throw std::invalid_argument("chart exceeds 32 MiB");
        std::ifstream file(argv[1],std::ios::binary);
        if(!file) throw std::runtime_error("cannot read chart");
        const std::string raw((std::istreambuf_iterator<char>(file)),{});
        const auto chart=rhythm::parse_phigros(raw,{"local-pose-query",argv[2],"local","local","local"});
        const rhythm::PhigrosPoseEvaluator evaluator(chart);
        nlohmann::json result={{"schema","pas.chart-pose-query.v1"},{"chart_sha256",chart.source.chart_sha256},
                              {"song_ns",song},{"lines",nlohmann::json::array()},
                              {"nearby_notes",nlohmann::json::array()}};
        for(std::size_t l=0;l<chart.lines.size();++l) {
            const auto pose=evaluator.line_at(l,song);
            result["lines"].push_back({{"line",l},{"origin",{pose.origin.x,pose.origin.y}},
                {"tangent",{pose.tangent.x,pose.tangent.y}},{"rotation_degrees",pose.rotation_degrees},{"opacity",pose.opacity}});
            for(const auto& note:chart.lines[l].notes) {
                if(static_cast<long double>(song)<static_cast<long double>(note.start_ns)-100000000 ||
                   static_cast<long double>(song)>static_cast<long double>(note.end_ns)+100000000) continue;
                const auto target=evaluator.note_target(l,note,song);
                result["nearby_notes"].push_back({{"line",l},{"source",note.source.json_pointer},
                    {"kind",static_cast<int>(note.kind)},{"start_ns",note.start_ns},{"end_ns",note.end_ns},
                    {"target",{target.normalized.x,target.normalized.y}},{"inside_reference",target.inside_reference}});
            }
        }
        std::cout<<result.dump()<<'\n'; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
