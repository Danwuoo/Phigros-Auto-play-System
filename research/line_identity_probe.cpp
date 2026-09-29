// Offline characterization of the unchanged production tracker. No capture,
// touch backend, model, wall-time pacing, or private-state access is involved.
#include "pas/game.hpp"
#include "provenance.hpp"
#include <fstream>
#include <deque>
#include <iostream>
#include <set>
#include <stdexcept>

using nlohmann::json;
using pas::LineCandidate;
using pas::Nanoseconds;

LineCandidate line(double offset) {
    LineCandidate result;
    result.center={640,570+offset};result.tangent={1,0};
    result.length=1200;result.thickness=2;result.confidence=.85;
    return result;
}

struct Probe {
    pas::GameLineTracker tracker;
    json frames=json::array();
    std::set<std::uint64_t> ids;
    std::size_t valid=0,invalid=0;
    std::uint64_t seq=0;
    std::vector<LineCandidate> step(Nanoseconds elapsed,std::vector<double> offsets) {
        std::vector<LineCandidate> lines;
        for(auto value:offsets)lines.push_back(line(value));
        pas::SceneContext context{1,1,1,++seq,1'000'000'000+elapsed,1280,720,1};
        tracker.update(lines,context);
        json outputs=json::array();
        for(const auto& current:lines) {
            ids.insert(current.track_id);
            current.association_valid?++valid:++invalid;
            outputs.push_back({{"id",current.track_id},{"valid",current.association_valid},
                {"y",current.center.y},{"motion_samples",current.motion_samples}});
        }
        frames.push_back({{"elapsed_ns",elapsed},{"input_offsets_px",offsets},{"outputs",outputs}});
        return lines;
    }
    json result(const char* label) const {
        return {{"case",label},{"frames",frames},{"unique_output_ids",ids.size()},
            {"valid_observations",valid},{"invalid_observations",invalid}};
    }
};

void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}

int main(int argc,char** argv) {
    try {
        if(argc<2)throw std::runtime_error("Usage: pas_line_identity_probe <output.json> [round15 event segments in summary order]");
        json cases=json::array();
        Probe single;
        for(int i=0;i<32;++i)single.step(i*16'000'000LL,{0});
        require(single.ids.size()==1&&single.invalid==0,"Single-line control changed");
        cases.push_back(single.result("single_stable_control"));

        Probe separated;
        for(int i=0;i<32;++i)separated.step(i*16'000'000LL,{-10,10});
        require(separated.ids.size()==2&&separated.invalid==0,"Two-line control changed");
        cases.push_back(separated.result("two_separated_lines_control"));

        Probe near;
        for(int i=0;i<32;++i)near.step(i*16'000'000LL,{0,2});
        cases.push_back(near.result("two_distinct_close_lines_no_ground_truth_merge"));

        Probe birth;
        birth.step(0,{0});birth.step(16'000'000,{0,2});
        for(int i=2;i<32;++i)birth.step(i*16'000'000LL,{0});
        cases.push_back(birth.result("one_duplicate_observation_then_single_for_480ms"));

        // No update gap reaches 100ms: isolate expiry of the track history.
        birth.step(520'000'000,{});birth.step(560'000'000,{});
        const auto expiry=birth.step(600'000'000,{0});
        require(expiry.size()==1&&expiry.front().association_valid,"90ms absence did not recover");
        cases.push_back(birth.result("same_case_then_empty_frames_until_history_expires"));

        Probe gap;
        gap.step(0,{0,2});gap.step(16'000'000,{0});
        const auto reset=gap.step(116'000'000,{0});
        require(reset.size()==1&&reset.front().association_valid,"100ms context reset did not recover");
        cases.push_back(gap.result("context_gap_control"));

        Probe fast;
        fast.step(0,{0,2});
        for(int i=1;i<=120;++i)fast.step(i*1'000'000LL,{0});
        cases.push_back(fast.result("one_ms_updates_capacity_characterization"));

        json report={{"schema",1},{"kind","offline_synthetic_characterization"},
            {"production_strategy_modified",false},{"touch_backend_created",false},
            {"clock","synthetic integer nanoseconds; not latency measurements"},
            {"motion_source_sha256",motion_sha256},{"game_header_sha256",game_header_sha256},
            {"probe_sha256",probe_sha256},{"cases",cases},
            {"interpretation_limit","Sufficient synthetic mechanism only; real prehistory and pixel truth are not established"}};
        // The optional log scan is a distinct observation of historical output,
        // never a replay of pixels or proof of private tracker prehistory.
        if(argc>2) {
            std::deque<json> recent;
            json window=json::array(),sources=json::array();
            json run_start=nullptr,run_prefix=json::array(),case_run=nullptr;
            std::size_t invalid_single_run=0;
            std::size_t decisions=0;
            bool found=false;
            for(int index=2;index<argc;++index) {
                sources.push_back(argv[index]);std::ifstream input(argv[index]);
                if(!input)throw std::runtime_error("Missing event segment");
                std::string text;
                while(std::getline(input,text)) {
                    if(text.find("game_decision")==std::string::npos)continue;
                    const auto event=json::parse(text);
                    if(event.value("event","")!="game_decision")continue;
                    ++decisions;
                    const auto frame=event.at("frame_sequence").get<std::uint64_t>();
                    json entry={{"frame",frame},{"capture_ns",event.at("capture_complete_ns")},
                        {"epoch",event.at("epoch")},{"ui",event.at("ui")},{"lines",event.at("lines")}};
                    const bool single_invalid=event.at("lines").size()==1&&
                        !event.at("lines").at(0).at("association_valid").get<bool>();
                    if(single_invalid) {
                        if(invalid_single_run==0) {
                            run_start=entry;run_prefix=json::array();
                            for(const auto& previous:recent)run_prefix.push_back(previous);
                        }
                        ++invalid_single_run;
                    } else {invalid_single_run=0;run_start=nullptr;run_prefix=json::array();}
                    recent.push_back(entry);if(recent.size()>32)recent.pop_front();
                    if(frame==238007) {
                        found=true;for(const auto& prior:recent)window.push_back(prior);
                        case_run={{"invalid_single_decisions_through_case",invalid_single_run},
                            {"first",run_start},{"preceding_published_decisions",run_prefix}};
                    }
                    else if(found&&frame>238007&&frame<=238009)window.push_back(entry);
                }
                if(input.bad())throw std::runtime_error("Event read failure");
            }
            require(found,"Expected Pixel Rebelz case frame was not found");
            report["historical_output_window"]={{"source_segments",sources},{"decisions_scanned",decisions},
                {"entries",window},{"contiguous_invalid_single_output_run",case_run},
                {"limit","Published line geometry and IDs only, not pre-assignment candidates or pixels"}};
        }
        std::ofstream output(argv[1],std::ios::binary);
        output<<report.dump(2)<<'\n';output.close();
        if(!output)throw std::runtime_error("Failed to save report");
        for(const auto& item:cases)std::cout<<item.at("case").get<std::string>()
            <<": ids="<<item.at("unique_output_ids")<<" valid="<<item.at("valid_observations")
            <<" invalid="<<item.at("invalid_observations")<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
