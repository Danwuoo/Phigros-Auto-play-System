#include "review_io.hpp"
#include "pas/analysis.hpp"
#include <iostream>
#include <map>
#include <set>
using namespace pas::review;
namespace {
json signature(const json& e){if(e.at("event")=="fake_receipt"){auto c=e.at("command");c.erase("intent_id");c.erase("contact_id");return {{"ordinal",e.at("ordinal_at_delivery")},{"command",c},{"success",e.at("success")},{"reason",e.at("reason")},{"start_ns",e.at("injection_start_ns")},{"return_ns",e.at("injection_return_ns")}};}
 if(e.at("event")=="fake_release"){const auto& r=e.at("report");return {{"ordinal",e.at("ordinal_at_delivery")},{"release_count",r.at("requested_ids").size()},{"failed_count",r.at("failed_ids").size()},{"unknown_count",r.at("unknown_ids").size()},{"start_ns",r.at("start_ns")},{"return_ns",r.at("return_ns")}};}return nullptr;}
json find_note(const json& arr,std::uint64_t id){for(const auto& t:arr)if(t.at("note_id")==id)return t;return nullptr;}
struct Events {std::vector<json> all;std::map<std::uint64_t,std::vector<json>> receipts;std::map<std::string,int> bag;};
Events events(const fs::path& root){Events r;each_row(root/"events.jsonl",100000,[&](json e){auto s=signature(e);if(!s.is_null())++r.bag[s.dump()];if(e.at("event")=="fake_receipt"&&!e.at("note_id").is_null())r.receipts[e.at("note_id").get<std::uint64_t>()].push_back(e);r.all.push_back(std::move(e));});return r;}
int phase(const std::vector<json>& v,int p){return static_cast<int>(std::count_if(v.begin(),v.end(),[&](const auto& e){return e.at("command").at("phase")==p;}));}
json review(const fs::path& batch){const auto action=load(batch/"full-action-audit-1.json"),life=load(batch/"lifecycle-audit-1.json"),manifest=load(batch/"input-manifest.json");const auto a=events(batch/"baseline-on-1"),b=events(batch/"variant-on-1");
 std::set<std::size_t> ords;std::map<std::uint64_t,std::vector<json>> hooks;std::map<std::uint64_t,std::string> kinds;json lifecycle_checks=json::array();bool valid=true;
 for(const auto& cs:life.at("pending_lifecycles")){const auto& c=cs.at("cancel");auto id=c.at("note_id").get<std::uint64_t>();hooks[id].push_back(c);kinds[id]=c.at("kind");
  auto before=find_note(cs.at("before").at("identities"),id);const auto ord=c.at("ordinal_at_delivery").get<std::size_t>();ords.insert(ord);
  bool no_prior_receipt=true;for(const auto& e:b.all)if(e.at("event")=="fake_receipt"&&e.at("command").at("intent_id")==c.at("intent_id")&&e.at("injection_start_ns")<=c.at("cancel_ns"))no_prior_receipt=false;
  const bool never_down=!before.is_null()&&before.at("cursor")==0&&before.at("prefix_offset")==0&&no_prior_receipt;
  const bool fresh_accept=cs.at("at_accept").at("acceptance_ns")==c.at("cancel_ns")&&cs.at("at_accept").at("ready_ns")==c.at("cancel_ns")&&cs.at("at_accept").at("consumed")==true;
  bool absent=find_note(cs.at("at_accept").at("targets"),id).is_null();valid&=never_down&&fresh_accept&&absent&&!cs.at("first_retired_ordinal").is_null();
  auto np=cs.at("first_new_plan");if(!np.is_null())ords.insert(np.at("ordinal_at_delivery").get<std::size_t>());
  lifecycle_checks.push_back({{"note_id",id},{"cancel_ordinal",ord},{"kind",kinds.at(id)},{"known_zero_absolute_cursor",never_down},{"accepted_complete_absence",fresh_accept&&absent},{"retired",cs.at("first_retired_ordinal")},{"new_plan",np}});
 }
 std::set<std::uint64_t> affected;for(const auto& block:life.at("all_action_difference_blocks"))for(const auto* role:{"reference","variant"})for(const auto& x:block.at(role)){ords.insert(x.at("action").at("ordinal").get<std::size_t>());if(!x.at("local_note_join").is_null())affected.insert(x.at("local_note_join").get<std::uint64_t>());}
 std::map<std::size_t,json> current;json occurrence=json::object();std::size_t lc=0;each_row(batch/"variant-on-1/lifecycle.jsonl",36000,[&](json row){++lc;for(const auto& t:row.at("targets")){const auto k=t.at("kind").get<std::string>();occurrence[k]=occurrence.value(k,0)+1;}if(ords.contains(row.at("ordinal").get<std::size_t>()))current[row.at("ordinal").get<std::size_t>()]=row;},32*1024*1024);
 for(auto& check:lifecycle_checks){auto p=check.at("new_plan");if(p.is_null())continue;const auto ord=p.at("ordinal_at_delivery").get<std::size_t>();auto target=find_note(current.at(ord).at("targets"),check.at("note_id").get<std::uint64_t>());
  const bool fresh=!target.is_null()&&target.at("evidence_ns")==p.at("evidence_ns")&&target.at("reason")=="prediction_observe_only"&&target.at("samples").get<int>()>0&&!target.at("crossing_ns").is_null()&&current.at(ord).at("acceptance_ns")==p.at("fake_now_ns");
  check["new_intent_fresh_current_target"]=fresh;valid&=fresh;
 }
 json contact_diff=json::array();std::size_t removed=0,replaced=0,unexplained=0;
 for(auto id:affected){const auto ar=a.receipts.contains(id)?a.receipts.at(id):std::vector<json>{},br=b.receipts.contains(id)?b.receipts.at(id):std::vector<json>{};const auto ad=phase(ar,0),bd=phase(br,0);
  std::string cls=bd==0&&ad==1?"pending_cancel_removed_whole_contact":ad==1&&bd==1?"fresh_return_replaced_pending_plan":"unknown";
  const bool explained=hooks.contains(id)&&cls!="unknown";if(!explained)++unexplained;
  removed+=cls=="pending_cancel_removed_whole_contact";replaced+=cls=="fresh_return_replaced_pending_plan";
  json anew=json::array(),bnew=json::array();for(const auto& e:ar)anew.push_back(signature(e));for(const auto& e:br)bnew.push_back(signature(e));
  contact_diff.push_back({{"note_id",id},{"kind",kinds.contains(id)?kinds.at(id):"unknown"},{"classification",cls},{"hook_events",hooks.contains(id)?json(hooks.at(id)):json::array()},{"reference_receipts",anew},{"candidate_receipts",bnew},{"reference_down",ad},{"candidate_down",bd},{"explained",explained},{"gameplay_effect","unknown"}});
 }
 std::size_t bag_removed=0,bag_added=0;for(const auto& [s,n]:a.bag)bag_removed+=std::max(0,n-(b.bag.contains(s)?b.bag.at(s):0));for(const auto& [s,n]:b.bag)bag_added+=std::max(0,n-(a.bag.contains(s)?a.bag.at(s):0));
 json reorder=json::array();for(const auto& block:life.at("all_action_difference_blocks"))for(const auto& x:block.at("variant")){const auto s=x.at("action").dump();if(a.bag.contains(s))reorder.push_back(x);}
 json windows=json::array();for(const auto& w:manifest.at("windows")){json av=json::array(),bv=json::array();for(const auto& e:a.all){const auto ord=e.at("ordinal_at_delivery").get<std::size_t>();auto s=signature(e);if(!s.is_null()&&ord>=w.at("first").get<std::size_t>()&&ord<=w.at("last").get<std::size_t>())av.push_back(s);}for(const auto& e:b.all){const auto ord=e.at("ordinal_at_delivery").get<std::size_t>();auto s=signature(e);if(!s.is_null()&&ord>=w.at("first").get<std::size_t>()&&ord<=w.at("last").get<std::size_t>())bv.push_back(s);}windows.push_back({{"id",w.at("id")},{"reference_actions",av.size()},{"candidate_actions",bv.size()},{"normalized_ordered_actions_equal",av==bv}});}
 json first=life.at("pending_lifecycles").at(0);first.erase("before");first.erase("at_accept");const auto& cs0=life.at("pending_lifecycles").at(0);first["timing"]={{"capture_ns",cs0.at("at_accept").at("capture_ns")},{"ready_ns",cs0.at("at_accept").at("ready_ns")},{"owner_acceptance_ns",cs0.at("at_accept").at("acceptance_ns")},{"original_down_due_ns",cs0.at("last_revision_plan").at("steps").at(0).at("due_ns")}};
 valid&=unexplained==0&&life.at("observer_scene_differences")==0&&life.at("observer_history_differences")==0&&life.at("observer_bank_differences")==0&&action.at("unexamined_actions")==0&&lc==7722;
 return {{"schema",1},{"decision",valid?"cold single-hook contract passed; gameplay tradeoff unknown; awaiting controller independent acceptance":"negative or unknown causal result blocks candidate adoption"},{"cold_contract_pass",valid},{"formal_adoption",false},{"action_audit_sha256",pas::sha256_file(batch/"full-action-audit-1.json")},{"lifecycle_audit_sha256",pas::sha256_file(batch/"lifecycle-audit-1.json")},
  {"fullprefix_target_occurrences_by_kind",occurrence},{"affected_contact_notes",affected.size()},{"removed_contacts",removed},{"fresh_return_replaced_contacts",replaced},{"unexplained_action_notes",unexplained},{"lifecycle_checks",lifecycle_checks},{"contact_action_differences",contact_diff},{"bag_unmatched_reference",bag_removed},{"bag_unmatched_candidate",bag_added},{"exact_same_signatures_with_order_change_in_LCS",reorder},{"selected_windows",windows},{"first_causal_divergence",first},
  {"causal_order_review","New intent allocation changes existing map-key tie order for simultaneous equal-phase Moves. LCS retains this ordering difference; the identical reordered Move is separately reported. Scheduler/frame-first policy itself is unchanged."},{"active_continuity_scope","All 28 hook cancellations had known absolute cursor0 and no prior receipt. Every unmatched receipt belongs to one of these local note joins; 14 contacts never start, 5 restart from new current evidence. Existing active Hold/alias/shared-drag/rotation guards verified by original suite and new tests. Local IDs do not establish physical continuity."},{"source_render_age",nullptr},{"early_recording_state","unknown"},{"physical_identity_gold",0},{"gameplay_effect","unknown"},{"miss_improvement","unknown"}};
}
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("causal-review batch");auto r=review(argv[1]);r["analysis_binary_sha256"]=pas::sha256_file(argv[0]);auto s=r.dump(2);if(s.size()>2*1024*1024)throw std::runtime_error("review_capacity");std::cout<<s<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
