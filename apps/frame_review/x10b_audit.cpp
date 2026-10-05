#include "review_io.hpp"
#include "pas/analysis.hpp"
#include <iostream>
#include <vector>
using namespace pas::review;
namespace {
std::vector<json> actions(const fs::path& p){std::vector<json> result;
    for(const auto& e:rows(p,100000)){
        json a=nullptr;
        if(e.at("event")=="fake_receipt"){
            auto c=e.at("command");c.erase("intent_id");c.erase("contact_id");
            a={{"ordinal",e.at("ordinal_at_delivery")},{"command",c},{"success",e.at("success")},{"reason",e.at("reason")},{"start_ns",e.at("injection_start_ns")},{"return_ns",e.at("injection_return_ns")}};
        }else if(e.at("event")=="fake_release"){
            auto r=e.at("report");a={{"ordinal",e.at("ordinal_at_delivery")},{"requested",r.at("requested_ids").size()},{"failed",r.at("failed_ids").size()},{"unknown",r.at("unknown_ids").size()},{"start_ns",r.at("start_ns")},{"return_ns",r.at("return_ns")}};
        }
        if(!a.is_null()){if(result.size()>=10000)throw std::runtime_error("action_capacity");result.push_back(std::move(a));}
    }return result;
}
std::vector<std::string> targets(const json& row){std::vector<std::string> out;for(auto t:row.at("scene").at("targets")){t.erase("note_id");t.erase("revision");out.push_back(t.dump());}std::sort(out.begin(),out.end());return out;}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("x10b_audit baseline-run variant-run > new-report");
    const fs::path aroot=argv[1],broot=argv[2];const auto sa=load(aroot/"summary.json"),sb=load(broot/"summary.json");
    for(const auto* s:{&sa,&sb})if(s->at("success")!=true||s->at("window_frames")!=156||s->at("verified_pngs")!=7722||s->at("consumed_frames")!=7715||s->at("contacts_at_exit")!=0)throw std::runtime_error("source_denominator");
    for(const auto* key:{"input_manifest_sha256","cadence","tie","receipt_policy","recognition","time_origin_ns"})if(sa.at(key)!=sb.at(key))throw std::runtime_error("source_policy");
    const auto a=actions(aroot/"events.jsonl"),b=actions(broot/"events.jsonl");std::size_t ai=0,bi=0;json removed=json::array(),unmatched=nullptr;
    while(ai<a.size()&&bi<b.size()){
        if(a[ai]==b[bi]){++ai;++bi;continue;}
        std::size_t found=ai+1;for(;found<a.size()&&found<=ai+8;++found)if(a[found]==b[bi])break;
        if(found>=a.size()||found>ai+8){unmatched={{"reference_index",ai},{"variant_index",bi},{"reference",a[ai]},{"variant",b[bi]}};break;}
        for(;ai<found;++ai)removed.push_back({{"index",ai},{"action",a[ai]}});
    }
    if(unmatched.is_null()&&bi==b.size())for(;ai<a.size();++ai)removed.push_back({{"index",ai},{"action",a[ai]}});
    const bool subsequence=unmatched.is_null()&&bi==b.size();
    const auto at=rows(aroot/"trace.jsonl",600),bt=rows(broot/"trace.jsonl",600);if(at.size()!=156||bt.size()!=156)throw std::runtime_error("trace_denominator");
    json first_target=nullptr;std::size_t early=0,early_equal=0;
    for(std::size_t i=0;i<at.size();++i){if(at[i].at("ordinal")!=bt[i].at("ordinal")||at[i].at("png_sha256")!=bt[i].at("png_sha256"))throw std::runtime_error("trace_join");
        const int ordinal=at[i].at("ordinal");const bool equal=targets(at[i])==targets(bt[i]);
        if(ordinal>=2440&&ordinal<=2459){++early;if(equal)++early_equal;}
        if(first_target.is_null()&&!equal)first_target=ordinal;
    }
    json out={{"schema",1},{"baseline_summary_sha256",pas::sha256_file(aroot/"summary.json")},{"variant_summary_sha256",pas::sha256_file(broot/"summary.json")},
        {"reference_full_actions",a.size()},{"variant_full_actions",b.size()},{"variant_is_exact_normalized_subsequence",subsequence},{"removed_reference_actions",removed},{"first_unmatched_non_deletion",unmatched},
        {"algorithm","bounded deletion alignment,lookahead8; failure explicit; does not claim general edit distance"},{"remaining_variant_actions",b.size()-bi},
        {"early_2440_2459_frames",early},{"early_targets_equal_excluding_note_id_revision",early_equal},{"first_target_difference_in_available_156_frame_union",first_target},
        {"limits","whole-prefix first state unknown; local IDs omitted for equivalence, physical truth/gameplay unknown; release ID sets compared only by counts"}};
    const auto encoded=out.dump(2);if(encoded.size()>2*1024*1024)throw std::runtime_error("output_capacity");std::cout<<encoded<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
