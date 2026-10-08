#include "../windows_qualification/verifier.hpp"
#include <map>
#include <set>
#include <iostream>
namespace w=pas::windows_qualification;using J=nlohmann::json;using Ns=std::int64_t;
#include "cancellation_join.hpp"
#include "publication_denominator.hpp"
struct Packet{Ns start=200,end=260;};
struct Fixture {
 J active={{"intent",1},{"contact",0},{"x",400.0},{"y",500.0}};
 J receipt={{"issued",{{"intent",1},{"contact",0},{"phase",2},{"scheduled",230},{"source_frame",1},{"x",400.0},{"y",500.0}}},{"injection_start_ns",232},{"injection_return_ns",234}};
 std::vector<J> plans={{{"intent",1},{"note",42},{"source_frame",1},{"evidence",100},{"revision",3},{"prefix_offset",0},{"steps",J::array({{{"phase",0},{"due",110},{"x",400.0},{"y",500.0}},{{"phase",2},{"due",300},{"x",400.0},{"y",500.0}}})}}};
 std::vector<J> cancels={{{"intent_id",1},{"note_id",42},{"source_frame",2},{"cancel_ns",225},{"last_evidence_ns",100},{"executed_steps",1},{"contact_started",true},{"retry_without_prior_down",false},{"kind","hold"},{"reason","current_object_missing_or_region_lost"},{"game_effect","unknown"}}};
 std::map<std::uint64_t,Packet> packets={{2,{}}};std::set<std::size_t> used;
 void verify(){w::require(verify_cancelled_up(receipt,plans,cancels,packets,active,used)==0,"exact cancellation");}
};
int main(int argc,char** argv){try{w::require(argc==2&&!std::filesystem::exists(argv[1]),"fresh output");J checks=J::array();int failed=0;
 const auto test=[&](const char* name,bool accept,auto change){Fixture f;change(f);bool actual=true;try{f.verify();}catch(const std::exception&){actual=false;}checks.push_back({{"id",name},{"pass",actual==accept},{"expected_accept",accept}});failed+=actual!=accept;};
 test("explicit_own_active_up",true,[](auto&){});
 test("down_cannot_use_cancellation",false,[](auto& f){f.receipt["issued"]["phase"]=0;});
 test("move_cannot_use_cancellation",false,[](auto& f){f.receipt["issued"]["phase"]=1;});
 test("unknown_active_intent",false,[](auto& f){f.active["intent"]=2;});
 test("wrong_contact",false,[](auto& f){f.receipt["issued"]["contact"]=1;});
 test("position_jump",false,[](auto& f){f.receipt["issued"]["x"]=405;});
 test("no_accepted_body",false,[](auto& f){f.plans.clear();});
 test("wrong_accepted_source",false,[](auto& f){f.plans[0]["source_frame"]=2;});
 test("no_cancel_record",false,[](auto& f){f.cancels.clear();});
 test("duplicate_cancel",false,[](auto& f){f.cancels.push_back(f.cancels[0]);});
 test("reused_cancel",false,[](auto& f){f.used.insert(0);});
 test("wrong_cancel_identity",false,[](auto& f){f.cancels[0]["note_id"]=43;});
 test("wrong_body_evidence",false,[](auto& f){f.cancels[0]["last_evidence_ns"]=101;});
 test("no_current_owner_packet",false,[](auto& f){f.packets.clear();});
 test("outside_owner_invocation",false,[](auto& f){f.cancels[0]["cancel_ns"]=199;});
 test("scheduled_before_cancellation",false,[](auto& f){f.receipt["issued"]["scheduled"]=220;});
 test("return_after_owner_invocation",false,[](auto& f){f.receipt["injection_return_ns"]=261;});
 test("unexecuted_prefix",false,[](auto& f){f.cancels[0]["executed_steps"]=0;});
 test("already_executed_up",false,[](auto& f){f.cancels[0]["executed_steps"]=2;});
 test("contact_not_started",false,[](auto& f){f.cancels[0]["contact_started"]=false;});
 test("retry_cannot_be_release",false,[](auto& f){f.cancels[0]["retry_without_prior_down"]=true;});
 test("unsupported_reason",false,[](auto& f){f.cancels[0]["reason"]="forged_safe";});
 test("game_effect_not_proven",false,[](auto& f){f.cancels[0]["game_effect"]="verified";});
 test("expired_body_source",false,[](auto& f){f.plans[0]["evidence"]=-100'000'000;f.cancels[0]["last_evidence_ns"]=-100'000'000;});
 const auto publication_test=[&](const char* name,bool accept,auto change){
  J summary={{"published",std::uint64_t(10005)},{"pool_drops",std::uint64_t(7)},
             {"candidate_packet_counters",{{"physical_pool_drops",std::uint64_t(7)}}}};
  J attempts=J::array();for(std::size_t i=0;i<10064;++i)
   attempts.push_back({{"attempt",i+1},{"produced",i<10013},{"render_start_ns",Ns(200)}});
  std::size_t requested=10000,produced=10013;Ns stop=200;
  change(summary,attempts,requested,produced,stop);
  bool actual=true;try{verify_publication_denominator(summary,attempts,requested,produced,stop);}
  catch(const std::exception&){actual=false;}
  checks.push_back({{"id",name},{"pass",actual==accept},{"expected_accept",accept}});failed+=actual!=accept;
 };
 publication_test("stress_drops_and_post_stop_callback_accounted",true,[](auto&,auto&,auto&,auto&,auto&){});
 publication_test("normal_zero_drop_tail",true,[](auto& s,auto&,auto&,auto&,auto&){s["published"]=std::uint64_t(10012);s["pool_drops"]=s["candidate_packet_counters"]["physical_pool_drops"]=std::uint64_t(0);});
 publication_test("all_callbacks_accounted_no_tail",true,[](auto& s,auto&,auto&,auto&,auto&){s["published"]=std::uint64_t(10006);});
 publication_test("drop_counter_tampered",false,[](auto& s,auto&,auto&,auto&,auto&){s["pool_drops"]=std::uint64_t(0);});
 publication_test("pool_counter_join_tampered",false,[](auto& s,auto&,auto&,auto&,auto&){s["candidate_packet_counters"]["physical_pool_drops"]=std::uint64_t(6);});
 publication_test("two_unaccounted_callbacks",false,[](auto& s,auto&,auto&,auto&,auto&){s["published"]=std::uint64_t(10004);});
 publication_test("publication_and_drop_double_counted",false,[](auto& s,auto&,auto&,auto&,auto&){s["published"]=std::uint64_t(10007);});
 publication_test("requested_frames_not_generated",false,[](auto&,auto&,auto& requested,auto&,auto&){requested=10014;});
 publication_test("stop_tail_before_result_stop",false,[](auto&,auto&,auto&,auto&,auto& stop){stop=201;});
 publication_test("requested_frame_cannot_be_stop_tail",false,[](auto&,auto& a,auto&,auto& produced,auto&){a[produced-1]["attempt"]=10000;});
 publication_test("unproduced_frame_cannot_be_stop_tail",false,[](auto&,auto& a,auto&,auto& produced,auto&){a[produced-1]["produced"]=false;});
 publication_test("missing_attempt_slot",false,[](auto&,auto& a,auto&,auto&,auto&){a.erase(a.size()-1);});
 publication_test("negative_publication_counter",false,[](auto& s,auto&,auto&,auto&,auto&){s["published"]=-1;});
 publication_test("oversized_drop_counter",false,[](auto& s,auto&,auto&,auto&,auto&){s["pool_drops"]=std::numeric_limits<std::uint64_t>::max();});
 publication_test("no_successful_publication",false,[](auto& s,auto&,auto&,auto&,auto&){s["published"]=std::uint64_t(0);});
 J report={{"schema","pas.production-cancellation-join-controls.v1"},{"checks",checks},{"failed",failed},{"device_operations",0},{"live_ready",false}};std::ofstream out(argv[1]);out<<report.dump(2)<<'\n';w::require(bool(out),"write");std::cout<<"checks="<<checks.size()<<" failed="<<failed<<'\n';return failed?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
