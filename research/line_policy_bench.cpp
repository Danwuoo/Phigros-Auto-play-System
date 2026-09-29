// Bounded production line-assignment timing, with no capture or touch access.
#include "pas/game.hpp"
#include "provenance.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

using namespace pas;
using nlohmann::json;
namespace {
json measure(int count,bool twin) {
    GameLineTracker tracker;std::vector<double> durations;durations.reserve(1000);
    std::size_t invalid=0,zero_ids=0;
    for(int frame=0;frame<1050;++frame) {
        SceneContext context{1,1,1,static_cast<std::uint64_t>(frame+1),
            1'000'000'000LL+static_cast<Nanoseconds>(frame)*16'000'000,1280,720,1};
        std::vector<LineCandidate> lines;lines.reserve(count);
        for(int i=0;i<count;++i) {
            const double y=twin?570.0+i*2:120.0+i*30;
            lines.push_back({{640,y},{1,0},1100,2,.85});
        }
        if(frame%2)std::reverse(lines.begin(),lines.end());
        const auto begin=std::chrono::steady_clock::now();
        tracker.update(lines,context);
        const auto end=std::chrono::steady_clock::now();
        if(frame>=50) {
            durations.push_back(std::chrono::duration<double,std::micro>(end-begin).count());
            for(const auto& line:lines) {invalid+=!line.association_valid;zero_ids+=line.track_id==0;}
        }
    }
    std::sort(durations.begin(),durations.end());
    const auto quantile=[&](double q){return durations[static_cast<std::size_t>(q*(durations.size()-1))];};
    return {{"lines",count},{"near_twin",twin},{"n",durations.size()},
        {"p50_us",quantile(.5)},{"p95_us",quantile(.95)},{"p99_us",quantile(.99)},
        {"max_us",durations.back()},{"execution_spread_p99_minus_p50_us",quantile(.99)-quantile(.5)},
        {"invalid_observations",invalid},{"zero_ids",zero_ids},{"failures",0}};
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::invalid_argument("output JSON path required");
        if(std::filesystem::exists(argv[1]))throw std::runtime_error("output already exists");
        const json result={{"kind","synthetic_line_assignment_timing"},
            {"clock","std::chrono::steady_clock / Windows QPC"},
            {"build","MSVC Release /O2 C++20"},{"game_motion_sha256",motion_sha256},
            {"game_header_sha256",game_header_sha256},
            {"scenarios",json::array({measure(1,false),measure(2,true),measure(16,false)})}};
        std::ofstream out(argv[1]);if(!out)throw std::runtime_error("cannot open output");
        out<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
