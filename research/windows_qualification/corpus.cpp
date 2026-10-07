#include "../prelive_current/cost_workload.hpp"
#include "../prelive_current/cost_workload_family_v2.hpp"
#include "../prelive_current/evidence_hash.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

int main(int argc,char**) {
    if(argc!=1)return 2;
    using namespace pas::current_rails;
    using J=nlohmann::json;
    J rows=J::array();
    auto add=[&](const char* scene,int phase,int offset,const auto& pixels) {
        pas::evidence::Sha256 sha;
        sha.update(reinterpret_cast<const char*>(pixels.data()),pixels.size());
        rows.push_back({{"scene",scene},{"phase",phase},{"offset",offset},
                        {"bytes",pixels.size()},{"sha256",sha.finish()}});
    };
    for(const auto* scene:{"tap","dense"}) {
        const auto images=cost_workload::stimulus(scene);
        for(int phase=0;phase<12;++phase)add(scene,phase,0,images[phase]);
    }
    for(int offset:{-2,0,2})for(int phase=0;phase<81;++phase)
        add("hold-closed-reset-v2",phase,offset,
            cost_family_v2::image(phase,offset,cost_family_v2::Control::positive));
    std::cout<<J{{"schema","pas.windows-cost-corpus.v1"},{"rows",rows},
        {"legacy_loop_frames",12},{"hold_cycle_frames",81},{"hold_offsets",{-2,0,2}},
        {"requested",2560},{"warmup",256},{"measurement",2304},{"cadence_ms",20},
        {"device_endpoints",0},{"physical_gold",0}}.dump(2)<<'\n';
    return std::cout?0:1;
}
