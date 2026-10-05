#include "review_io.hpp"
#include <iostream>
#include <map>
using namespace pas::review;
namespace {
json get_target(const json& row,std::uint64_t id){if(!row.at("scene").is_null())for(const auto& t:row.at("scene").at("targets"))if(t.at("note_id")==id)return t;return nullptr;}
json get_history(const json& row,std::uint64_t id){for(const auto& h:row.at("observer_history"))if(h.at("id")==id)return h;return nullptr;}
std::map<int,json> trace(const fs::path& root){std::map<int,json> result;for(const auto& r:rows(root/"trace.jsonl",960))result[r.at("ordinal").get<int>()]=r;return result;}
std::map<int,json> witness(const fs::path& root,std::uint64_t id){std::map<int,json> result;each_row(root/"fallback-witness.jsonl",100000,[&](const json& j){if(j.at("history").at("id")==id)result[j.at("ordinal").get<int>()]=j;});return result;}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("x10c_causal_report baseline-run variant-run > report");
    const fs::path ar=argv[1],br=argv[2];const auto a=trace(ar),b=trace(br),aw=witness(ar,684),bw=witness(br,684);
    json frames=json::array();json first_target=nullptr,first_history=nullptr;
    for(int n=2865;n<=2895;++n){if(!a.contains(n)||!b.contains(n))throw std::runtime_error("K_window_missing");
        const auto& x=a.at(n);const auto& y=b.at(n);if(x.at("source_frame")!=y.at("source_frame")||x.at("png_sha256")!=y.at("png_sha256")||x.at("timing")!=y.at("timing"))throw std::runtime_error("pixel_clock_join");
        const auto at=get_target(x,684),bt=get_target(y,684),ah=get_history(x,684),bh=get_history(y,684);
        if(at!=bt&&first_target.is_null())first_target=n;if(ah!=bh&&first_history.is_null())first_history=n;
        const auto a682=get_target(x,682),b682=get_target(y,682);
        frames.push_back({{"ordinal",n},{"source_frame",x.at("source_frame")},{"png_sha256",x.at("png_sha256")},{"timing",x.at("timing")},
            {"684_target_equal",at==bt},{"684_history_equal",ah==bh},{"682_target_equal",a682==b682},
            {"baseline684",at},{"variant684",bt}});
    }
    if(!aw.contains(2881)||!bw.contains(2881))throw std::runtime_error("first_boundary_witness_missing");
    auto x=aw.at(2881),y=bw.at(2881);x.erase("applied");y.erase("applied");
    json event_rows=json::array();for(const auto& role:{std::pair{"baseline",ar},std::pair{"variant",br}})each_row(role.second/"events.jsonl",100000,[&](const json& j){
        const int n=j.at("ordinal_at_delivery");if(n>=2879&&n<=2887&&j.value("note_id",std::uint64_t{0})==684)event_rows.push_back({{"role",role.first},{"event",j}});
    });
    json result={{"schema",1},{"case","K2865-2895"},{"frame_denominator",31},{"first_684_target_divergence",first_target},{"first_684_history_divergence",first_history},
        {"2881_pre_suppression_witness_equal_except_applied",x==y},{"2881_baseline_witness",aw.at(2881)},{"2881_variant_witness",bw.at(2881)},
        {"frames",frames},{"684_event_rows",event_rows},{"physical_identity","unknown; local684 is a within-run join, comparisons below prove field equality rather than physical identity"},
        {"gameplay_effect","unknown"}};
    const auto text=result.dump(2);if(text.size()>2*1024*1024)throw std::runtime_error("report_capacity");std::cout<<text<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
