// Static horizontal-line ablation: B = original pair gate and greedy order,
// but contested observations cannot birth tracks. C = production tracker.
// The original A output remains the frozen M0 synthetic.json, not rerun here.
#include "pas/game.hpp"
#include "provenance.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace pas;
using nlohmann::json;
namespace {
LineCandidate line(double offset) {return {{640,570+offset},{1,0},1200,2,.85};}
struct Metrics {std::set<std::uint64_t> ids;int valid=0,unknown=0,zero=0;
    void add(const LineCandidate& l) {if(l.track_id)ids.insert(l.track_id);
        valid+=l.association_valid;unknown+=!l.association_valid;zero+=l.track_id==0;}
    json result() const {return {{"distinct_ids",ids.size()},{"valid",valid},{"unknown",unknown},{"zero_id",zero}};}
};
using Sequence=std::vector<std::vector<double>>;
json compare(const Sequence& sequence) {
    struct Prior {double offset;Nanoseconds time;std::uint64_t id;};
    std::vector<Prior> prior;std::uint64_t next=0;Metrics minimal,global;
    GameLineTracker tracker;
    for(std::size_t frame=0;frame<sequence.size();++frame) {
        const Nanoseconds now=1'000'000'000+static_cast<Nanoseconds>(frame)*16'000'000;
        std::erase_if(prior,[&](const auto& t){return now-t.time>=90'000'000;});
        struct Pair {std::size_t obs,track;double cost;};std::vector<Pair> pairs;
        for(std::size_t i=0;i<sequence[frame].size();++i)
            for(std::size_t j=0;j<prior.size();++j) {
                const double cost=std::abs(sequence[frame][i]-prior[j].offset);
                if(cost<=64)pairs.push_back({i,j,cost});
            }
        std::sort(pairs.begin(),pairs.end(),[](const auto& a,const auto& b){return a.cost<b.cost;});
        std::vector<int> assigned(sequence[frame].size(),-1);
        std::vector<bool> used(prior.size()),contested(sequence[frame].size());
        for(const auto& p:pairs) {
            if(assigned[p.obs]>=0||used[p.track])continue;
            const bool ambiguous=std::any_of(pairs.begin(),pairs.end(),[&](const auto& q){
                return ((q.obs==p.obs&&q.track!=p.track)||(q.track==p.track&&q.obs!=p.obs))&&
                    q.cost<=p.cost+3;
            });
            if(ambiguous){contested[p.obs]=true;continue;}
            assigned[p.obs]=static_cast<int>(p.track);used[p.track]=true;
        }
        for(std::size_t i=0;i<sequence[frame].size();++i) {
            LineCandidate l=line(sequence[frame][i]);l.association_valid=!contested[i];
            if(assigned[i]>=0) {
                auto& t=prior[assigned[i]];l.track_id=t.id;t.offset=sequence[frame][i];t.time=now;
            } else if(!contested[i]) {
                l.track_id=++next;prior.push_back({sequence[frame][i],now,next});
            } else l.track_id=0;
            minimal.add(l);
        }
        std::vector<LineCandidate> current;
        for(const auto offset:sequence[frame])current.push_back(line(offset));
        SceneContext context{1,1,1,frame+1,now,1280,720,1};
        tracker.update(current,context);for(const auto& l:current)global.add(l);
    }
    return {{"minimal_birth_guard",minimal.result()},{"bounded_global_assignment",global.result()},
            {"frames",sequence.size()}};
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::invalid_argument("output JSON path required");
        if(std::filesystem::exists(argv[1]))throw std::runtime_error("output already exists");
        Sequence single(32,{{0}});single[1]={0,2};
        Sequence twins(32,{{0,2}});
        Sequence late(5,{{0}});for(std::size_t i=1;i<late.size();++i)late[i]={0,2};
        const json result={{"kind","static_horizontal_line_ablation"},
            {"production_motion_sha256",motion_sha256},{"game_header_sha256",game_header_sha256},
            {"minimal_scope","original greedy pair order and cost for stationary horizontal lines; contested observations do not birth"},
            {"historical_A","measurements/line-identity-research-20260928/synthetic.json"},
            {"single_one_frame_twin",compare(single)},{"two_close_real_lines",compare(twins)},
            {"late_new_close_line",compare(late)}};
        std::ofstream output(argv[1]);if(!output)throw std::runtime_error("cannot open output");
        output<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
