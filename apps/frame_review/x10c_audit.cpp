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
        return {{"ordinal",e.at("ordinal_at_delivery")},{"command",c},{"success",e.at("success")},
            {"reason",e.at("reason")},{"start_ns",e.at("injection_start_ns")},{"return_ns",e.at("injection_return_ns")}};
    }
    if(e.at("event")=="fake_release"){
        const auto& r=e.at("report");return {{"ordinal",e.at("ordinal_at_delivery")},{"release_count",r.at("requested_ids").size()},
            {"failed_count",r.at("failed_ids").size()},{"unknown_count",r.at("unknown_ids").size()},
            {"start_ns",r.at("start_ns")},{"return_ns",r.at("return_ns")}};
    }return nullptr;
}
struct Actions {std::vector<json> normalized,raw;std::vector<std::string> canonical;json phase_counts=json::object();};
Actions actions(const fs::path& root){Actions out;each_row(root/"events.jsonl",100000,[&](json e){auto a=action(e);if(a.is_null())return;
    if(out.raw.size()>=10000)throw std::runtime_error("action_capacity");
    const auto key=e.at("event")=="fake_receipt"?"phase_"+std::to_string(e.at("command").at("phase").get<int>()):"release";
    out.phase_counts[key]=out.phase_counts.value(key,0)+1;out.canonical.push_back(a.dump());out.normalized.push_back(std::move(a));out.raw.push_back(std::move(e));});return out;}
json target_set(const json& row){std::vector<std::string> ts;for(auto t:row.at("scene").at("targets")){t.erase("note_id");t.erase("revision");ts.push_back(t.dump());}std::sort(ts.begin(),ts.end());return ts;}
void denominator(const json& s){if(s.at("success")!=true||s.at("verified_pngs")!=7722||s.at("consumed_frames")!=7715||s.at("contacts_at_exit")!=0||s.at("input_truncated")!=false)throw std::runtime_error("denominator");}
json audit(const fs::path& ar,const fs::path& br){const auto sa=load(ar/"summary.json"),sb=load(br/"summary.json");denominator(sa);denominator(sb);
    for(const auto* key:{"cadence","tie","receipt_policy","recognition","time_origin_ns","input_manifest_sha256"})if(sa.at(key)!=sb.at(key))throw std::runtime_error("policy");
    const auto a=actions(ar),b=actions(br);const auto n=a.raw.size(),m=b.raw.size();
    if((n+1)*(m+1)>25000000)throw std::runtime_error("alignment_cell_capacity");
    std::vector<std::uint16_t> lcs((n+1)*(m+1));auto cell=[&](std::size_t i,std::size_t j)->std::uint16_t&{return lcs[i*(m+1)+j];};
    for(std::size_t i=n;i-->0;)for(std::size_t j=m;j-->0;)cell(i,j)=a.canonical[i]==b.canonical[j]?cell(i+1,j+1)+1:std::max(cell(i+1,j),cell(i,j+1));
    json blocks=json::array(),block=nullptr;std::size_t i=0,j=0,matched=0,removed=0,added=0;
    auto flush=[&]{if(!block.is_null()){blocks.push_back(block);block=nullptr;}};
    while(i<n||j<m){if(i<n&&j<m&&a.canonical[i]==b.canonical[j]){flush();++i;++j;++matched;continue;}
        if(block.is_null())block={{"reference_begin_index",i},{"variant_begin_index",j},{"reference",json::array()},{"variant",json::array()}};
        if(i<n&&(j==m||cell(i+1,j)>=cell(i,j+1))){block["reference"].push_back({{"index",i},{"action",a.normalized[i]},{"local_join_event",a.raw[i]}});++i;++removed;}
        else {block["variant"].push_back({{"index",j},{"action",b.normalized[j]},{"local_join_event",b.raw[j]}});++j;++added;}
    }flush();
    json first=nullptr;for(std::size_t k=0;k<std::max(n,m);++k){const auto av=k<n?a.normalized[k]:json(nullptr),bv=k<m?b.normalized[k]:json(nullptr);if(av!=bv){first={{"index",k},{"reference",av},{"variant",bv}};break;}}
    json windows=json::array();if(fs::exists(ar/"trace.jsonl")&&fs::exists(br/"trace.jsonl")){
        std::map<std::size_t,json> old;for(const auto& row:rows(ar/"trace.jsonl",960))old[row.at("ordinal").get<std::size_t>()]=row;
        std::size_t target_diff=0;json ordinals=json::array();for(const auto& row:rows(br/"trace.jsonl",960)){
            const auto ord=row.at("ordinal").get<std::size_t>();if(!old.contains(ord))throw std::runtime_error("trace_frame_missing");
            if(row.at("png_sha256")!=old.at(ord).at("png_sha256")||row.at("source_frame")!=old.at(ord).at("source_frame"))throw std::runtime_error("trace_pixel_join");
            if(!row.at("scene").is_null()&&!old.at(ord).at("scene").is_null()&&target_set(row)!=target_set(old.at(ord))){++target_diff;ordinals.push_back(ord);}
        }
        windows={{"frames",old.size()},{"normalized_target_different_frames",target_diff},{"ordinals",ordinals}};
    }
    json digests=nullptr;if(fs::exists(ar/"state-digests.jsonl")&&fs::exists(br/"state-digests.jsonl")){
        auto da=rows(ar/"state-digests.jsonl",36000),db=rows(br/"state-digests.jsonl",36000);if(da.size()!=7722||db.size()!=7722)throw std::runtime_error("state_digest_denominator");
        digests=json::object();for(const auto* key:{"semantic_sha256","scene_sha256","bank_sha256","history_sha256","owner_sha256","contacts_sha256"}){
            json ranges=json::array(),start=nullptr;std::size_t count=0;json first_ord=nullptr;
            for(std::size_t k=0;k<da.size();++k){if(da[k].at("ordinal")!=k||db[k].at("ordinal")!=k||da[k].at("png_sha256")!=db[k].at("png_sha256"))throw std::runtime_error("digest_join");
                const bool different=da[k].at("digests").at(key)!=db[k].at("digests").at(key);
                if(different){++count;if(first_ord.is_null())first_ord=k;if(start.is_null())start=k;}
                if(!start.is_null()&&(!different||k+1==da.size())){ranges.push_back({{"first",start},{"last",different?k:k-1}});start=nullptr;}
            }digests[key]={{"different_frames",count},{"first",first_ord},{"ranges",ranges}};
        }
    }
    return {{"schema",1},{"reference_root",fs::absolute(ar).generic_string()},{"variant_root",fs::absolute(br).generic_string()},
        {"reference_summary_sha256",pas::sha256_file(ar/"summary.json")},{"variant_summary_sha256",pas::sha256_file(br/"summary.json")},
        {"reference_events_sha256",pas::sha256_file(ar/"events.jsonl")},{"variant_events_sha256",pas::sha256_file(br/"events.jsonl")},
        {"reference_actions",n},{"variant_actions",m},{"reference_phase_counts",a.phase_counts},{"variant_phase_counts",b.phase_counts},
        {"matched_normalized_actions",matched},{"unmatched_reference_actions",removed},{"unmatched_variant_actions",added},
        {"unexamined_actions",0},{"first_ordered_difference",first},{"alignment_blocks",blocks},{"trace_target_comparison",windows},{"frame_digests",digests},
        {"alignment","exact LCS; fixed <=10000 actions and <=25 million cells; deterministic reference deletion on ties; signatures retain time/position/source_frame/receipt status; local IDs removed, release counts retained"},
        {"physical_identity_gold",false},{"gameplay_effect","unknown"}};
}
}
int main(int argc,char** argv){try{if(argc!=3)throw std::runtime_error("x10c_audit reference-run variant-run > new-report");const auto report=audit(argv[1],argv[2]);const auto text=report.dump(2);if(text.size()>2*1024*1024)throw std::runtime_error("report_capacity");std::cout<<text<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
