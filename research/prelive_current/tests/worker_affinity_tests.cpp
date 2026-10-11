#include "worker_affinity.hpp"
#include "../../windows_production_pipeline_v3/producer_placement.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <type_traits>
#include <utility>
using namespace pas::current_rails;using J=nlohmann::json;using Failure=WorkerAffinityFailure;
struct Tests {J checks=J::array();int failed=0;void check(const char* id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}};
struct Api {
 std::uint32_t process=0x20,groups=1,id=11;std::uint64_t process_mask=0xff,system_mask=0xff;
 bool masks_ok=true;WorkerGroupAffinity value{0,0xff};
 int reads=0,sets=0,threads=0,topology_reads=0,fail_read=0,fail_set=0,ignore_set=0,change_thread_at=0,foreign_group_read=0;
 WorkerCoreTopology core_data;
 Api(){core_data.failure=Failure::none;core_data.count=4;core_data.returned_bytes=192;
  core_data.cores[0]={0,1,0x03};core_data.cores[1]={0,0,0x0c};core_data.cores[2]={0,1,0x30};core_data.cores[3]={0,1,0xc0};}
 std::uint32_t process_class() const noexcept{return process;}
 std::uint32_t active_groups() const noexcept{return groups;}
 bool process_masks(std::uint64_t& p,std::uint64_t& s) const noexcept{if(!masks_ok)return false;p=process_mask;s=system_mask;return true;}
 WorkerCoreTopology topology() noexcept{++topology_reads;return core_data;}
 std::uint32_t thread() noexcept{return ++threads==change_thread_at?id+1:id;}
 std::optional<WorkerGroupAffinity> current() noexcept{++reads;if(reads==fail_read)return {};auto result=value;if(reads==foreign_group_read)result.group=1;return result;}
 bool set(WorkerGroupAffinity requested) noexcept{++sets;if(sets==fail_set)return false;if(sets!=ignore_set)value=requested;return true;}
};
template<class F> bool rejects(F&& work,Failure reason,Failure rollback=Failure::none){try{work();}catch(const WorkerAffinityError& error){return error.reason()==reason&&error.rollback_reason()==rollback;}catch(...){return false;}return false;}
void placement_controls(Tests& t){
 {Api api;const auto p=make_worker_placement(api);t.check("highest_class_last_two_role_anchors_complete_eligible_union_v2",p.valid()&&p.efficiency_class==1&&p.consumer_core==2&&p.producer_core==3&&p.consumer_number==4&&p.producer_number==6&&p.consumer_mask==0x10&&p.producer_anchor_mask==0x40&&p.producer_mask==0x41&&api.topology_reads==1);}
 {Api api;api.core_data.cores[0].efficiency_class=255;api.core_data.cores[2].efficiency_class=255;const auto p=make_worker_placement(api);t.check("raw_class_two_cores_keep_lp_zero_no_number_sentinel",p.valid()&&p.efficiency_class==255&&p.consumer_core==0&&p.producer_core==2&&p.consumer_number==0&&p.producer_number==4&&p.consumer_mask==1&&p.producer_mask==0x10);}
 {Api api;for(auto& core:api.core_data.cores)core.efficiency_class=0;const auto p=make_worker_placement(api);t.check("homogeneous_zero_class_all_eligible_cores_except_consumer_v2",p.valid()&&p.efficiency_class==0&&p.consumer_core==2&&p.producer_core==3&&p.consumer_mask==0x10&&p.producer_mask==0x45);}
 {Api api;std::swap(api.core_data.cores[0],api.core_data.cores[3]);const auto p=make_worker_placement(api);t.check("record_order_does_not_change_role_anchors_or_eligible_union_v2",p.valid()&&p.consumer_mask==0x10&&p.producer_mask==0x41&&p.consumer_core==2&&p.producer_core==0);}
 {Api api;api.process=0;t.check("plan_process_query_failure_before_topology",make_worker_placement(api).failure==Failure::process_query&&api.topology_reads==0);}
 {Api api;api.process=0x80;t.check("plan_normal_process_required_no_mutation",make_worker_placement(api).failure==Failure::normal_process_required&&api.sets==0);}
 {Api api;api.groups=0;t.check("plan_group_query_failure",make_worker_placement(api).failure==Failure::group_query&&api.topology_reads==0);}
 {Api api;api.groups=2;t.check("plan_multiple_groups_no_guessed_mask",make_worker_placement(api).failure==Failure::single_active_group_required&&api.topology_reads==0);}
 {Api api;api.masks_ok=false;t.check("plan_process_mask_query_failure",make_worker_placement(api).failure==Failure::process_mask_query);}
 {Api api;api.process_mask=0;t.check("plan_empty_process_mask_rejected",make_worker_placement(api).failure==Failure::process_mask_invalid);}
 {Api api;api.system_mask=0;t.check("plan_empty_system_mask_rejected",make_worker_placement(api).failure==Failure::process_mask_invalid);}
 {Api api;api.process_mask=0x1ff;t.check("plan_process_mask_outside_system_rejected",make_worker_placement(api).failure==Failure::process_mask_invalid);}
 for(auto failure:{Failure::topology_query,Failure::topology_read,Failure::topology_byte_bound}){Api api;api.core_data.failure=failure;const auto p=make_worker_placement(api);t.check(failure==Failure::topology_query?"topology_query_negative_retained":failure==Failure::topology_read?"topology_read_negative_retained":"topology_byte_bound_negative_retained",p.failure==failure&&!p.valid());}
 {Api api;api.core_data.count=257;t.check("topology_core_count_checked_before_indexing",make_worker_placement(api).failure==Failure::topology_core_bound);}
 {Api api;api.core_data.returned_bytes=65537;t.check("topology_actual_bytes_bound_checked",make_worker_placement(api).failure==Failure::topology_byte_bound);}
 {Api api;api.core_data.cores[2].mask=3;t.check("overlapping_core_masks_rejected",make_worker_placement(api).failure==Failure::topology_malformed);}
 {Api api;api.core_data.cores[2].mask=0;t.check("empty_physical_core_rejected",make_worker_placement(api).failure==Failure::topology_malformed);}
 {Api api;api.core_data.count=3;t.check("incomplete_topology_system_mask_join_rejected",make_worker_placement(api).failure==Failure::topology_incomplete);}
 {Api api;api.core_data.cores[2].group=1;t.check("foreign_topology_group_rejected",make_worker_placement(api).failure==Failure::topology_group);}
 {Api api;api.core_data.cores[2].efficiency_class=0;api.core_data.cores[3].efficiency_class=0;t.check("one_highest_core_cannot_use_sibling_or_lower_class",make_worker_placement(api).failure==Failure::two_highest_class_cores_required);}
 {Api api;api.process_mask=0xc3;t.check("selected_lp_outside_process_no_other_core_fallback",make_worker_placement(api).failure==Failure::requested_mask_not_in_process);}
 {Api api;api.process_mask=0xef;t.check("excluded_selected_lowest_lp_no_sibling_fallback",make_worker_placement(api).failure==Failure::requested_mask_not_in_process);}
}
void scope_controls(Tests& t){
 static_assert(std::is_trivially_copyable_v<WorkerAffinitySnapshot>);static_assert(sizeof(WorkerAffinitySnapshot)<=192);
 static_assert(!std::is_copy_constructible_v<WorkerAffinityScope<Api>>);static_assert(!std::is_move_constructible_v<WorkerAffinityScope<Api>>);
 {Api api;WorkerAffinitySnapshot r;{WorkerAffinityScope scope(api,r,1);t.check("only_current_thread_requested_group_mask_applied",api.value==WorkerGroupAffinity{0,1}&&r.previous==WorkerGroupAffinity{0,0xff}&&r.applied&&!r.verified());scope.close();t.check("explicit_restore_exact_original_group_mask",api.value==WorkerGroupAffinity{0,0xff}&&r.verified());const auto calls=api.sets;scope.close();t.check("explicit_close_idempotent_no_mutation",api.sets==calls);}t.check("successful_close_no_destructor_fallback",!r.fallback_attempted);}
 {Api api;api.value={0,1};WorkerAffinitySnapshot r;{WorkerAffinityScope scope(api,r,1);scope.close();}t.check("already_pinned_same_mask_restored",r.verified()&&api.value==WorkerGroupAffinity{0,1});}
 for(std::uint64_t mask:{0ull,3ull,256ull}){Api api;WorkerAffinitySnapshot r;
  if(mask==3){WorkerAffinityScope scope(api,r,mask);scope.close();t.check("scope_nonzero_multiple_lps_exact_restore_v2",r.verified()&&r.requested.mask==3&&api.value.mask==0xff);}
  else{const auto reason=mask==256?Failure::requested_mask_not_in_process:Failure::requested_mask_invalid;t.check(mask==0?"scope_zero_mask_rejected":"scope_mask_outside_process_rejected",rejects([&]{WorkerAffinityScope scope(api,r,mask);},reason)&&api.sets==0);}}
 {Api api;api.groups=2;WorkerAffinitySnapshot r;t.check("scope_rechecks_single_group_before_mutation",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::single_active_group_required)&&api.sets==0);}
 {Api api;api.process_mask=0xfe;WorkerAffinitySnapshot r;t.check("scope_rechecks_process_mask_before_mutation",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::requested_mask_not_in_process)&&api.sets==0);}
 {Api api;api.fail_read=1;WorkerAffinitySnapshot r;t.check("original_affinity_query_failure_no_mutation",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_query)&&api.sets==0);}
 {Api api;api.value.group=1;WorkerAffinitySnapshot r;t.check("original_cross_group_not_guessed_or_replaced",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::original_group)&&api.sets==0);}
 {Api api;api.value.mask=0x100;WorkerAffinitySnapshot r;t.check("original_mask_not_in_process_rejected",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::original_mask)&&api.sets==0);}
 {Api api;api.change_thread_at=2;WorkerAffinitySnapshot r;t.check("constructor_thread_change_before_set_rejected",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::wrong_thread)&&api.sets==0);}
 {Api api;api.fail_set=1;WorkerAffinitySnapshot r;t.check("apply_set_failure_typed_no_fallback",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_set)&&api.value.mask==0xff&&!r.verified());}
 {Api api;api.fail_read=2;WorkerAffinitySnapshot r;t.check("apply_query_failure_rolls_back_and_reads_original",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_readback_query)&&r.rollback_restored&&r.rollback_readback==r.previous&&!r.restored&&!r.verified());}
 {Api api;api.ignore_set=1;WorkerAffinitySnapshot r;t.check("apply_mask_mismatch_verified_rollback",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_readback_mismatch)&&r.rollback_restored&&!r.verified());}
 {Api api;api.foreign_group_read=2;WorkerAffinitySnapshot r;t.check("apply_group_mismatch_not_just_mask_checked",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_readback_mismatch)&&r.rollback_restored);}
 {Api api;api.fail_read=2;api.fail_set=2;WorkerAffinitySnapshot r;t.check("constructor_and_rollback_set_failure_both_retained",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_readback_query,Failure::rollback_set)&&r.failure==Failure::affinity_readback_query&&r.rollback_failure==Failure::rollback_set&&!r.verified());}
 {Api api;api.ignore_set=1;api.fail_read=3;WorkerAffinitySnapshot r;t.check("rollback_readback_query_failure_rejected",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_readback_mismatch,Failure::rollback_readback_query)&&!r.rollback_restored);}
 {Api api;api.fail_read=2;api.ignore_set=2;WorkerAffinitySnapshot r;t.check("rollback_write_success_mismatch_rejected",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_readback_query,Failure::rollback_readback_mismatch)&&!r.rollback_restored);}
 {Api api;api.fail_read=2;api.change_thread_at=3;WorkerAffinitySnapshot r;t.check("rollback_wrong_thread_does_not_mutate_other_thread",rejects([&]{WorkerAffinityScope scope(api,r,1);},Failure::affinity_readback_query,Failure::wrong_thread)&&api.sets==1);}
 {Api api;WorkerAffinitySnapshot r;{WorkerAffinityScope scope(api,r,1);api.id=12;const auto sets=api.sets;t.check("wrong_thread_close_no_other_thread_mutation",rejects([&]{scope.close();},Failure::wrong_thread)&&api.sets==sets);api.id=11;scope.close();t.check("later_explicit_cleanup_keeps_sticky_failure",r.restored&&r.failure==Failure::wrong_thread&&!r.verified());}}
 for(int variant=0;variant<3;++variant){Api api;WorkerAffinitySnapshot r;{WorkerAffinityScope scope(api,r,1);if(variant==0)api.fail_set=2;else if(variant==1)api.fail_read=3;else api.ignore_set=2;const auto reason=variant==0?Failure::restore_set:variant==1?Failure::restore_readback_query:Failure::restore_readback_mismatch;t.check(variant==0?"restore_set_failure_rejected":variant==1?"restore_readback_query_failure_rejected":"restore_readback_mismatch_rejected",rejects([&]{scope.close();},reason)&&!r.restored);}t.check(variant==0?"fallback_after_set_failure_not_verified":variant==1?"fallback_after_query_failure_not_verified":"fallback_after_mismatch_not_verified",api.value.mask==0xff&&r.fallback_attempted&&!r.restored&&!r.verified());}
 {Api api;WorkerAffinitySnapshot r;try{WorkerAffinityScope scope(api,r,1);throw std::runtime_error("work fault");}catch(const std::runtime_error&){}t.check("work_exception_fallback_restores_without_verification",api.value.mask==0xff&&r.fallback_attempted&&!r.close_attempted&&!r.restored&&!r.verified());}
 {Api api;WorkerAffinitySnapshot r;{WorkerAffinityScope scope(api,r,1);api.id=12;}t.check("destructor_wrong_thread_no_other_thread_mutation",api.sets==1&&r.fallback_failure==Failure::wrong_thread&&!r.verified());}
 for(int variant=0;variant<3;++variant){Api api;WorkerAffinitySnapshot r;{WorkerAffinityScope scope(api,r,1);if(variant==0)api.fail_set=2;else if(variant==1)api.fail_read=3;else api.ignore_set=2;}const auto reason=variant==0?Failure::restore_set:variant==1?Failure::restore_readback_query:Failure::restore_readback_mismatch;t.check(variant==0?"destructor_set_failure_retained":variant==1?"destructor_query_failure_retained":"destructor_mismatch_retained",r.fallback_failure==reason&&!r.restored&&!r.verified());}
}
Tests eligibility_controls(){
 Tests t;
 {Api api;const auto p=make_worker_placement(api);const auto j=cold::producer_placement::plan_json(p);
  t.check("all_highest_class_available_cores_in_exact_union",p.valid()&&p.producer_mask==0x41&&p.producer_eligible_core_count==2&&j.at("producer_eligible_cores").size()==2&&j.at("topology_available")==true&&j.at("topology_core_records").size()==4&&j.at("schema")=="pas.cold-worker-placement.v2");}
 {Api api;const auto p=make_worker_placement(api);t.check("consumer_entire_physical_core_and_all_siblings_excluded",p.valid()&&p.consumer_core_mask==0x30&&(p.producer_mask&p.consumer_core_mask)==0);}
 {Api api;const auto p=make_worker_placement(api);t.check("lower_class_and_nonlowest_sibling_lps_never_eligible",p.valid()&&(p.producer_mask&0x0c)==0&&(p.producer_mask&0xaa)==0);}
 {Api api;const auto p=make_worker_placement(api);t.check("last_core_lp_remains_identity_anchor_not_singleton_request",p.valid()&&p.producer_core==3&&p.producer_number==6&&p.producer_anchor_mask==0x40&&p.producer_mask!=p.producer_anchor_mask&&(p.producer_mask&p.producer_anchor_mask)==p.producer_anchor_mask);}
 {Api api;api.process_mask=0xfe;const auto p=make_worker_placement(api);t.check("unavailable_core_lowest_lp_skipped_without_sibling_fallback",p.valid()&&p.producer_mask==0x40&&p.producer_eligible_core_count==1&&(p.producer_mask&2)==0);}
 {Api api;for(auto& core:api.core_data.cores)core.efficiency_class=0;const auto p=make_worker_placement(api);t.check("homogeneous_class_retains_every_nonconsumer_core",p.valid()&&p.producer_mask==0x45&&p.producer_eligible_core_count==3);}
 {Api api;api.core_data.cores[0].efficiency_class=255;api.core_data.cores[2].efficiency_class=255;const auto p=make_worker_placement(api);t.check("raw_unsigned_class255_and_lp_zero_are_exact_facts",p.valid()&&p.efficiency_class==255&&p.consumer_number==0&&p.producer_mask==0x10&&p.producer_eligible_core_count==1);}
 {Api api;api.core_data.count=2;api.core_data.cores[0]={0,255,1};api.core_data.cores[1]={0,255,std::uint64_t{1}<<63};api.process_mask=api.system_mask=1|(std::uint64_t{1}<<63);const auto p=make_worker_placement(api);t.check("lowest_and_highest_group_bit_no_signed_or_shift_sentinel",p.valid()&&p.consumer_number==0&&p.producer_number==63&&p.producer_mask==(std::uint64_t{1}<<63));}
 {Api api;WorkerAffinitySnapshot s;{WorkerAffinityScope scope(api,s,0x41);const bool applied=api.value.mask==0x41&&s.applied_readback==s.requested;scope.close();t.check("multibit_request_apply_and_restore_are_exact_full_masks",applied&&s.verified()&&s.restored_readback.mask==0xff&&api.value.mask==0xff);}}
 {Api api;api.value.mask=0x40;api.ignore_set=1;WorkerAffinitySnapshot s;t.check("partial_multibit_apply_rejected_even_when_anchor_present",rejects([&]{WorkerAffinityScope scope(api,s,0x41);},Failure::affinity_readback_mismatch)&&s.rollback_restored&&api.value.mask==0x40&&!s.verified());}
 {Api api;WorkerAffinitySnapshot s;bool rejected=false;{WorkerAffinityScope scope(api,s,0x41);api.ignore_set=2;rejected=rejects([&]{scope.close();},Failure::restore_readback_mismatch);}t.check("restore_wrong_full_mask_sticky_after_destructor_fallback",rejected&&s.fallback_attempted&&api.value.mask==0xff&&!s.restored&&!s.verified());}
 {Api consumer;Api writer=consumer;const auto p=make_worker_placement(consumer);WorkerAffinitySnapshot c,s;
  {WorkerAffinityScope consumer_scope(consumer,c,p.consumer_mask);Api producer=consumer;producer.id=12;
   {WorkerAffinityScope producer_scope(producer,s,p.producer_mask);producer_scope.close();}
   consumer_scope.close();}
  t.check("writer_created_before_affinity_keeps_prior_while_workers_restore",p.valid()&&writer.value.mask==0xff&&writer.sets==0&&c.verified()&&s.verified()&&s.previous.mask==p.consumer_mask&&consumer.value.mask==0xff);}
 if(t.checks.size()!=12)throw std::runtime_error("fixed independent producer eligibility denominator");
 return t;
}
int main(int argc,char** argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh worker affinity output");Tests t;placement_controls(t);scope_controls(t);
#ifdef _WIN32
 WindowsWorkerAffinityApi api;const auto& plan=WindowsWorkerAffinityApi::plan();const auto& again=WindowsWorkerAffinityApi::plan();
 t.check("actual_windows_bounded_plan_cached_and_normal",plan.valid()&&(&plan==&again)&&api.process_class()==NORMAL_PRIORITY_CLASS);
 if(!plan.valid())throw WorkerAffinityError(plan.failure);
 const auto previous=api.current();const auto priority=GetThreadPriority(GetCurrentThread());std::uint64_t process=0,system=0;const auto masks=api.process_masks(process,system);
 t.check("actual_windows_reserved_core_and_full_eligible_process_mask_v2",masks&&plan.consumer_core!=plan.producer_core&&std::has_single_bit(plan.consumer_mask)&&plan.producer_mask&&(plan.producer_mask&plan.producer_anchor_mask)==plan.producer_anchor_mask&&!(plan.producer_mask&plan.consumer_core_mask)&&(plan.consumer_mask&process)==plan.consumer_mask&&(plan.producer_mask&process)==plan.producer_mask&&plan.producer_eligible_core_count==std::size_t(std::popcount(plan.producer_mask)));
 WorkerAffinitySnapshot consumer,producer;
 {WorkerAffinityScope scope(api,consumer,plan.consumer_mask);t.check("actual_windows_consumer_set_readback",api.current()==std::optional<WorkerGroupAffinity>(consumer.requested)&&!consumer.verified());scope.close();}
 t.check("actual_windows_consumer_original_restored",previous&&api.current()==previous&&consumer.verified()&&!consumer.fallback_attempted);
 {WorkerAffinityScope scope(api,producer,plan.producer_mask);t.check("actual_windows_producer_set_readback",api.current()==std::optional<WorkerGroupAffinity>(producer.requested)&&!producer.verified());scope.close();}
 t.check("actual_windows_producer_original_restored_priority_unchanged",previous&&api.current()==previous&&producer.verified()&&GetThreadPriority(GetCurrentThread())==priority&&api.process_class()==NORMAL_PRIORITY_CLASS);
#endif
 J result={{"schema","pas.worker-affinity-tests.v1"},{"assertions",t.checks.size()},{"checks",t.checks},{"failed",t.failed},{"pass",t.failed==0},
  {"device_endpoints",0},{"device_operations",0},{"cost_qualification",false},{"live_ready",false},{"qualification_scope","cold OS placement and current-thread restore controls; noise, physical and live separate"}};
 const auto e=eligibility_controls();
 result["eligibility_controls"]={{"schema","pas.cold-producer-eligibility-controls.v1"},{"assertions",e.checks.size()},{"checks",e.checks},{"failed",e.failed},{"pass",e.failed==0},
  {"synthetic_api_controls",true},{"device_operations",0},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false}};
 result["producer_eligibility_policy_id"]=producer_eligibility_policy_id;
 result["producer_eligibility_policy_version"]=producer_eligibility_policy_version;
 result["fixture_contract"]="historical 65 denominator; affected singleton expected masks explicitly updated to v2 eligible union; new eligibility12 separate";
#ifdef _WIN32
 result["worker_placement"]=cold::producer_placement::plan_json(plan,true);
 result["consumer_affinity"]=cold::producer_placement::affinity_json(consumer);
 result["producer_affinity"]=cold::producer_placement::affinity_json(producer);
#endif
#if defined(PAS_PRODUCER_ELIGIBILITY_DELTA) && PAS_PRODUCER_ELIGIBILITY_DELTA
 result["producer_eligibility_delta_identity"]=cold::producer_placement::eligibility_delta_identity();
#endif
 std::ofstream out(argv[1],std::ios::binary);out<<result.dump(2)<<'\n';if(!out)throw std::runtime_error("worker affinity output write");std::cout<<"assertions="<<t.checks.size()<<" failed="<<t.failed<<" eligibility_assertions="<<e.checks.size()<<" eligibility_failed="<<e.failed<<'\n';return (t.failed||e.failed)?1:0;
}catch(const WorkerAffinityError& error){std::cerr<<error.what()<<"; rollback="<<worker_affinity_failure_name(error.rollback_reason())<<'\n';return 2;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}}
