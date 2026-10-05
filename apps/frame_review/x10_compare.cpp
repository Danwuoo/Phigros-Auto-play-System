#include "review_io.hpp"
#include "pas/analysis.hpp"
#include <iostream>
#include <map>
#include <set>
using namespace pas::review;
namespace {
json action(const json& e){
    if(e.at("event")=="fake_receipt"){
        auto c=e.at("command");c.erase("intent_id");c.erase("contact_id");
        return {{"ordinal",e.at("ordinal_at_delivery")},{"command",c},{"success",e.at("success")},{"reason",e.at("reason")},
                {"injection_start_ns",e.at("injection_start_ns")},{"injection_return_ns",e.at("injection_return_ns")}};
    }
    if(e.at("event")=="fake_release"){
        const auto& r=e.at("report");return {{"ordinal",e.at("ordinal_at_delivery")},{"release_count",r.at("requested_ids").size()},
          {"failed_count",r.at("failed_ids").size()},{"unknown_count",r.at("unknown_ids").size()},{"start_ns",r.at("start_ns")},{"return_ns",r.at("return_ns")}};
    }
    return nullptr;
}
json normalize_target(json t){t.erase("note_id");t.erase("revision");return t;}
}
int main(int argc,char** argv){try{
    if(argc!=4)throw std::runtime_error("x10_compare original_X1_run new_X9_run variant_run > report");
    const fs::path reference=argv[1],window=argv[2],variant=argv[3];
    const auto rs=load(reference/"summary.json"),ws=load(window/"summary.json"),vs=load(variant/"summary.json");
    for(const auto* s:{&rs,&ws,&vs})if(!s->at("success").get<bool>()||s->at("consumed_frames")!=7715||s->at("verified_pngs")!=7722||s->at("contacts_at_exit")!=0)throw std::runtime_error("incomplete source");
    for(const auto* s:{&ws,&vs})for(const auto* k:{"cadence","tie","receipt_policy","recognition","time_origin_ns"})if(s->at(k)!=rs.at(k))throw std::runtime_error("clock_policy_mismatch");
    if(rs.at("semantic_sha256")!=ws.at("semantic_sha256")||pas::sha256_file(reference/"events.jsonl")!=pas::sha256_file(window/"events.jsonl"))throw std::runtime_error("reference_bridge");
    const auto re=rows(reference/"events.jsonl",100000),ve=rows(variant/"events.jsonl",100000);
    std::vector<json> ra,va;for(const auto& e:re){auto a=action(e);if(!a.is_null())ra.push_back(std::move(a));}for(const auto& e:ve){auto a=action(e);if(!a.is_null())va.push_back(std::move(a));}
    json first=nullptr;for(std::size_t i=0;i<std::max(ra.size(),va.size());++i){const auto a=i<ra.size()?ra[i]:json(nullptr),b=i<va.size()?va[i]:json(nullptr);if(a!=b){first={{"ordered_index",i},{"reference",a},{"variant",b}};break;}}
    std::map<std::size_t,json> original;for(const auto& row:rows(reference/"trace.jsonl",600))original.emplace(row.at("ordinal").get<std::size_t>(),row);
    for(const auto& row:rows(window/"trace.jsonl",600)){const auto n=row.at("ordinal").get<std::size_t>();if(original.contains(n)&&original.at(n)!=row)throw std::runtime_error("trace_bridge");original[n]=row;}
    json windows=json::array();const std::array<std::pair<int,int>,5> ranges{{{2460,2495},{3493,3504},{4979,4990},{5281,5293},{6158,6220}}};
    const auto vt=rows(variant/"trace.jsonl",600);
    for(const auto [begin,end]:ranges){json first_target=nullptr;int n=0;std::vector<json> aa,bb;
        for(const auto& row:vt){const int ord=row.at("ordinal");if(ord<begin||ord>end)continue;++n;const auto& old=original.at(ord);if(old.at("png_sha256")!=row.at("png_sha256"))throw std::runtime_error("pixel_join");
            std::vector<std::string> a,b;for(const auto& t:old.at("scene").at("targets"))a.push_back(normalize_target(t).dump());for(const auto& t:row.at("scene").at("targets"))b.push_back(normalize_target(t).dump());std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
            if(first_target.is_null()&&a!=b)first_target=ord;
        }
        for(const auto& a:ra){const int ord=a.at("ordinal");if(ord>=begin&&ord<=end)aa.push_back(a);}for(const auto& a:va){const int ord=a.at("ordinal");if(ord>=begin&&ord<=end)bb.push_back(a);}
        json fa=nullptr;for(std::size_t i=0;i<std::max(aa.size(),bb.size());++i){auto a=i<aa.size()?aa[i]:json(nullptr),b=i<bb.size()?bb[i]:json(nullptr);if(a!=b){fa={{"reference",a},{"variant",b}};break;}}
        windows.push_back({{"first",begin},{"last",end},{"frames",n},{"first_target_difference_excluding_note_id_revision",first_target},{"reference_actions",aa.size()},{"variant_actions",bb.size()},{"first_action_difference",fa},{"all_objects_scope",true}});
    }
    json out={{"schema",1},{"reference_summary_sha256",pas::sha256_file(reference/"summary.json")},{"window_summary_sha256",pas::sha256_file(window/"summary.json")},{"variant_summary_sha256",pas::sha256_file(variant/"summary.json")},
       {"reference_actions",ra.size()},{"variant_actions",va.size()},{"first_full_prefix_action_difference",first},{"windows",windows},{"normalization","exclude local note/revision for target equality; exclude local intent/contact IDs for receipt equality; release count and failure/unknown counts retained; no physical-identity correspondence claim"},
       {"first_full_state_difference","unknown: generic X1 output has whole-prefix digest but no per-frame full-prefix digest"},{"gameplay_effect","unknown"},{"physical_gold",false}};
    const auto text=out.dump(2);if(text.size()>2*1024*1024)throw std::runtime_error("output_capacity");std::cout<<text<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
