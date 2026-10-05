#include "review_io.hpp"
#include "pas/analysis.hpp"
#include <iostream>
#include <map>
#include <set>
using namespace pas::review;
namespace {
struct Run {std::vector<json> events;std::map<std::uint64_t,json> plans;json counts=json::object();};
Run read(const fs::path& p){Run r;std::map<std::uint64_t,std::string> kinds;
 each_row(p/"lifecycle.jsonl",36000,[&](json row){for(const auto& t:row.at("targets")){if(kinds.size()>10000)throw std::runtime_error("note_kind_capacity");kinds[t.at("note_id").get<std::uint64_t>()]=t.at("kind").get<std::string>();}},32*1024*1024);
 each_row(p/"events.jsonl",100000,[&](json e){
 if(e.at("event")=="counterfactual_plan"){auto id=e.at("note_id").get<std::uint64_t>();e["kind"]=kinds.contains(id)?kinds.at(id):"unknown";r.plans[e.at("intent_id").get<std::uint64_t>()]=e;}
 r.events.push_back(std::move(e));});return r;}
std::string kind(const Run& r,const json& e){auto id=e.at("command").at("intent_id").get<std::uint64_t>();
 return r.plans.contains(id)?r.plans.at(id).value("kind",std::string("unknown")):"unknown";}
json denominators(const Run& r){json d=json::object();for(const auto* k:{"tap","hold","drag","flick","unknown"})d[k]={{"plans",0},{"down",0},{"move",0},{"up",0},{"failed",0},{"cancellations",0},{"pending_missing",0}};
 for(const auto& [_,p]:r.plans)++d[p.value("kind",std::string("unknown"))]["plans"].get_ref<json::number_integer_t&>();
 for(const auto& e:r.events){const auto type=e.at("event");
  if(type=="fake_receipt"){auto k=kind(r,e);const auto phase=e.at("command").at("phase").get<int>();auto key=phase==0?"down":phase==1?"move":"up";d[k][key]=d[k][key].get<int>()+1;if(!e.at("success").get<bool>())d[k]["failed"]=d[k]["failed"].get<int>()+1;}
  if(type=="game_contact_cancelled"){auto k=e.at("kind").get<std::string>();d[k]["cancellations"]=d[k]["cancellations"].get<int>()+1;if(e.at("reason")=="pending_down_current_object_missing")d[k]["pending_missing"]=d[k]["pending_missing"].get<int>()+1;}
 }return d;}
json find_id(const json& arr,std::uint64_t id){for(const auto& x:arr)if(x.at("note_id")==id)return x;return nullptr;}
json audit(const fs::path& ar,const fs::path& br,const fs::path& actionPath){const auto a=read(ar),b=read(br);const auto aa=load(actionPath);
 json cases=json::array();for(const auto& e:b.events)if(e.at("event")=="game_contact_cancelled"&&e.at("reason")=="pending_down_current_object_missing"){
  if(cases.size()>=1000)throw std::runtime_error("pending_case_capacity");
  if(e.at("executed_steps")!=0||e.at("contact_started")!=false||e.at("retry_without_prior_down")!=true)throw std::runtime_error("pending_cancel_guard");
  const auto intent=e.at("intent_id").get<std::uint64_t>();if(!b.plans.contains(intent))throw std::runtime_error("pending_plan_join");
  cases.push_back({{"cancel",e},{"last_revision_plan",b.plans.at(intent)},{"before",nullptr},{"at_accept",nullptr},{"first_return_target",nullptr},{"first_new_plan",nullptr},{"first_candidate_down_after_cancel",nullptr},{"baseline_down_after_cancel",json::array()},{"first_retired_ordinal",nullptr}});
 }
 for(auto& cs:cases){const auto id=cs.at("cancel").at("note_id").get<std::uint64_t>(),old=cs.at("cancel").at("intent_id").get<std::uint64_t>();const auto ord=cs.at("cancel").at("ordinal_at_delivery").get<std::size_t>();
  for(const auto& e:b.events)if(e.at("ordinal_at_delivery").get<std::size_t>()>=ord){
   if(e.at("event")=="counterfactual_plan"&&e.at("note_id")==id&&e.at("intent_id")!=old&&cs["first_new_plan"].is_null())cs["first_new_plan"]=e;
   if(e.at("event")=="fake_receipt"&&e.at("command").at("phase")==0&&e.value("note_id",json(nullptr))==id&&cs["first_candidate_down_after_cancel"].is_null())cs["first_candidate_down_after_cancel"]=e;
  }
  for(const auto& e:a.events)if(e.at("ordinal_at_delivery").get<std::size_t>()>=ord&&e.at("event")=="fake_receipt"&&e.at("command").at("phase")==0&&e.value("note_id",json(nullptr))==id)cs["baseline_down_after_cancel"].push_back(e);
 }
 std::size_t rows_n=0;each_row(br/"lifecycle.jsonl",36000,[&](json row){const auto ord=row.at("ordinal").get<std::size_t>();if(ord!=rows_n++)throw std::runtime_error("lifecycle_order");
  if(row.at("targets").size()>128||row.at("identities").size()>128)throw std::runtime_error("lifecycle_state_bound");
  for(auto& cs:cases){const auto c=cs.at("cancel").at("ordinal_at_delivery").get<std::size_t>();const auto id=cs.at("cancel").at("note_id").get<std::uint64_t>();
   if(ord+1==c)cs["before"]=row;
   if(ord==c){cs["at_accept"]=row;if(!row.at("consumed").get<bool>()||!find_id(row.at("targets"),id).is_null())throw std::runtime_error("missing_not_complete_absence");}
   if(ord>c){auto target=find_id(row.at("targets"),id);if(!target.is_null()&&cs["first_return_target"].is_null())cs["first_return_target"]={{"ordinal",ord},{"ready_ns",row.at("ready_ns")},{"target",target}};
    if(find_id(row.at("identities"),id).is_null()&&cs["first_retired_ordinal"].is_null())cs["first_retired_ordinal"]=ord;}
  }
 },32*1024*1024);if(rows_n!=7722)throw std::runtime_error("fullprefix_lifecycle_denominator");
 json first=nullptr;std::size_t scene_diffs=0,history_diffs=0,bank_diffs=0;auto da=rows(ar/"state-digests.jsonl",36000),db=rows(br/"state-digests.jsonl",36000);
 if(da.size()!=7722||db.size()!=7722)throw std::runtime_error("digest_denominator");
 for(std::size_t i=0;i<da.size();++i){for(const auto* k:{"scene_sha256","history_sha256","bank_sha256"})if(da[i].at("digests").at(k)!=db[i].at("digests").at(k)){if(std::string(k)=="scene_sha256")++scene_diffs;else if(std::string(k)=="history_sha256")++history_diffs;else ++bank_diffs;}
  if(first.is_null()&&da[i].at("digests").at("owner_sha256")!=db[i].at("digests").at("owner_sha256"))first=i;
 }
 json blocks=aa.at("alignment_blocks");std::set<std::uint64_t> affected;
 for(auto& block:blocks)for(const auto* role:{"reference","variant"})for(auto& x:block.at(role)){
  auto e=x.at("local_join_event");if(e.at("event")=="fake_receipt"){x["kind"]=kind(std::string(role)=="reference"?a:b,e);x["local_note_join"]=e.value("note_id",json(nullptr));if(e.contains("note_id")&&!e.at("note_id").is_null())affected.insert(e.at("note_id").get<std::uint64_t>());}
 }
 std::size_t returned=0,replanned=0,returned_without_down=0,baseline_opportunity=0,retired=0;
 for(auto& cs:cases){const bool rt=!cs.at("first_return_target").is_null(),np=!cs.at("first_new_plan").is_null(),cd=!cs.at("first_candidate_down_after_cancel").is_null(),bd=!cs.at("baseline_down_after_cancel").empty();returned+=rt;replanned+=np;returned_without_down+=rt&&!cd;baseline_opportunity+=bd&&!cd;retired+=!cs.at("first_retired_ordinal").is_null();
  cs["classification"]=np?"fresh_return_replanned":rt?"returned_without_new_intent":"no_return_before_EOF";cs["baseline_down_but_no_candidate_down"]=bd&&!cd;cs["gameplay_effect"]="unknown";
 }
 return {{"schema",1},{"reference_events_sha256",pas::sha256_file(ar/"events.jsonl")},{"candidate_events_sha256",pas::sha256_file(br/"events.jsonl")},{"action_audit_sha256",pas::sha256_file(actionPath)},
  {"reference_denominators",denominators(a)},{"candidate_denominators",denominators(b)},{"fullprefix_frames",rows_n},{"observer_scene_differences",scene_diffs},{"observer_history_differences",history_diffs},{"observer_bank_differences",bank_diffs},{"first_owner_divergence",first},
  {"pending_cancels",cases.size()},{"returned",returned},{"replanned",replanned},{"returned_without_candidate_down",returned_without_down},{"baseline_down_but_no_candidate_down_cases",baseline_opportunity},{"retired",retired},{"pending_lifecycles",cases},{"all_action_difference_blocks",blocks},{"affected_local_note_ids",affected},{"unexamined_actions",aa.at("unexamined_actions")},
  {"scope","Exact local joins supported by unchanged observer digests; local note IDs are not physical identity truth. Each repeated cancellation is a separate lifecycle case, not an independent gameplay sample. Active action changes require review of every alignment block."},{"physical_gold",0},{"gameplay_effect","unknown"}};
}
}
int main(int argc,char** argv){try{if(argc!=4)throw std::runtime_error("lifecycle-audit reference candidate action-audit");const auto r=audit(argv[1],argv[2],argv[3]);auto s=r.dump(2);if(s.size()>2*1024*1024)throw std::runtime_error("report_capacity");std::cout<<s<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
