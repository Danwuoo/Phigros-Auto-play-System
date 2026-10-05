#include "review_io.hpp"
#include <iostream>
#include <iomanip>
#include <set>
using namespace pas::review;
int main(int argc,char** argv){try{
    if(argc!=4)throw std::runtime_error("x10c_inspect run-root first last");
    const fs::path root=argv[1];const int first=std::stoi(argv[2]),last=std::stoi(argv[3]);
    if(first<0||last<first||last-first>=120)throw std::runtime_error("window_capacity");
    std::cout<<std::fixed<<std::setprecision(3);std::size_t frames=0,targets=0;
    std::cout<<"Current targets: ordinal id kind center distance samples span_ms crossing reason basis body/head\n";
    each_row(root/"trace.jsonl",960,[&](const json& row){const int n=row.at("ordinal");if(n<first||n>last)return;++frames;
        if(row.at("scene").is_null())return;
        for(const auto& t:row.at("scene").at("targets")){++targets;
            std::cout<<n<<' '<<t.at("note_id")<<' '<<t.at("kind")<<' '<<t.at("x")<<','<<t.at("y")<<' '<<t.at("relative_distance_px")<<' '<<t.at("samples")<<' '<<t.at("history_span_ns").get<double>()/1e6<<' '<<t.at("crossing_ns")<<' '<<t.at("reason")<<' '<<t.at("observation_basis")<<' '<<t.at("held_body_evidence")<<'/'<<t.at("head_on_line")<<'\n';
        }
    });std::cout<<"All selected frame/target denominator "<<frames<<'/'<<targets<<"\nFallback predicates: ordinal prior_ID candidate_center history samples positions would_skip applied current_claims\n";
    each_row(root/"fallback-witness.jsonl",100000,[&](const json& row){const int n=row.at("ordinal");if(n<first||n>last)return;
        std::cout<<n<<' '<<row.at("history").at("id")<<' '<<row.at("candidate").at("center")<<' '<<row.at("history").at("points").size()<<' ';
        for(const auto& p:row.at("history").at("points"))std::cout<<p.at("position")<<'@'<<p.at("time_ns")<<';';
        std::cout<<' '<<row.at("would_suppress")<<' '<<row.at("applied")<<' '<<row.at("current_claims")<<'\n';
    });
    std::cout<<"Window receipts/cancellations/plans; local IDs are within-run joins only\n";
    each_row(root/"events.jsonl",100000,[&](const json& e){const int n=e.at("ordinal_at_delivery");if(n>=first&&n<=last)std::cout<<e.dump()<<'\n';});
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
