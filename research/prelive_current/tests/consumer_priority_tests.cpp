#include "consumer_priority.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <type_traits>
using namespace pas::current_rails;
using J=nlohmann::json;using Failure=ConsumerPriorityFailure;
struct Tests {
 J checks=J::array();int failed=0;
 void check(const char* id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}
};
// Scripted OS boundary: failed queries, ignored writes and wrong-thread calls
// are independently injected, rather than copying the scope's state machine.
struct PriorityApi {
 int value=-1,reads=0,sets=0,fail_read=0,fail_set=0,ignore_set=0,threads=0,change_thread_at=0;
 std::uint32_t id=7,process=consumer_priority_normal_process_class;
 std::array<int,8> writes{};
 std::uint32_t process_class() const noexcept{return process;}
 std::uint32_t thread() noexcept{return ++threads==change_thread_at?id+1:id;}
 int current() noexcept{return ++reads==fail_read?consumer_priority_query_error:value;}
 bool set(int priority) noexcept {
  ++sets;if(sets<=int(writes.size()))writes[sets-1]=priority;
  if(sets==fail_set)return false;if(sets!=ignore_set)value=priority;return true;
 }
};
template<class F> bool rejects(F&& work,Failure reason,Failure rollback=Failure::none) {
 try{work();}catch(const ConsumerPriorityError& e){return e.reason()==reason&&e.rollback_reason()==rollback;}
 catch(...){return false;}return false;
}
void controls(Tests& t) {
 static_assert(std::is_trivially_copyable_v<ConsumerPrioritySnapshot>);
 static_assert(sizeof(ConsumerPrioritySnapshot)<=64);
 static_assert(!std::is_copy_constructible_v<ConsumerPriorityScope<PriorityApi>>);
 static_assert(!std::is_move_constructible_v<ConsumerPriorityScope<PriorityApi>>);
 {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);
   t.check("current_consumer_above_normal_normal_process",api.value==1&&api.process==0x20&&api.writes[0]==1&&receipt.thread_id==7&&receipt.previous==-1&&receipt.applied&&!receipt.verified());
   scope.close();t.check("explicit_close_restores_exact_previous",api.value==-1&&receipt.restored_readback==-1&&receipt.restored&&receipt.verified());
   const auto calls=api.sets;scope.close();t.check("close_idempotent_no_second_write",api.sets==calls);
  }
  t.check("verified_close_needs_no_destructor_fallback",api.sets==2&&!receipt.fallback_attempted);
 }
 {
  PriorityApi api;api.value=1;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);scope.close();}
  t.check("already_above_normal_restored_without_demotion",api.value==1&&receipt.previous==1&&receipt.verified());
 }
 for(const bool failed_query:{false,true}) {
  PriorityApi api;api.process=failed_query?0:0x80;ConsumerPrioritySnapshot receipt;
  const auto reason=failed_query?Failure::process_query:Failure::normal_process_required;
  const auto rejected=rejects([&]{ConsumerPriorityScope scope(api,receipt);},reason);
  t.check(failed_query?"process_query_failure_no_mutation":"non_normal_process_rejected_without_mutation",rejected&&api.sets==0&&receipt.failure==reason&&!receipt.verified());
 }
 {
  PriorityApi api;api.id=0;ConsumerPrioritySnapshot receipt;
  t.check("invalid_thread_identity_no_mutation",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::thread_query)&&api.sets==0);
 }
 {
  PriorityApi api;api.fail_read=1;ConsumerPrioritySnapshot receipt;
  t.check("initial_priority_query_failure_no_mutation",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::priority_query)&&api.sets==0&&receipt.failure==Failure::priority_query);
 }
 {
  PriorityApi api;api.change_thread_at=2;ConsumerPrioritySnapshot receipt;
  t.check("constructor_wrong_thread_before_set_rejected",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::wrong_thread)&&api.sets==0&&receipt.failure==Failure::wrong_thread);
 }
 {
  PriorityApi api;api.fail_set=1;ConsumerPrioritySnapshot receipt;
  t.check("failed_apply_is_typed_no_silent_fallback",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::priority_set)&&api.value==-1&&api.sets==1&&!receipt.applied&&!receipt.verified());
 }
 {
  PriorityApi api;api.ignore_set=1;ConsumerPrioritySnapshot receipt;
  t.check("ignored_apply_rolls_back_and_readbacks_prior",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::priority_readback_mismatch)&&api.sets==2&&api.reads==3&&api.value==-1&&receipt.rollback_attempted&&receipt.rollback_restored&&!receipt.restored&&!receipt.verified());
 }
 {
  PriorityApi api;api.fail_read=2;ConsumerPrioritySnapshot receipt;
  t.check("apply_readback_query_failure_verified_rollback",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::priority_readback_query)&&api.value==-1&&receipt.rollback_restored&&receipt.failure==Failure::priority_readback_query);
 }
 {
  PriorityApi api;api.fail_read=2;api.fail_set=2;ConsumerPrioritySnapshot receipt;
  t.check("failed_constructor_retains_apply_and_rollback_set_failure",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::priority_readback_query,Failure::rollback_set)&&api.value==1&&receipt.failure==Failure::priority_readback_query&&receipt.rollback_failure==Failure::rollback_set&&!receipt.rollback_restored&&!receipt.verified());
 }
 {
  PriorityApi api;api.fail_read=2;api.change_thread_at=3;ConsumerPrioritySnapshot receipt;
  t.check("constructor_rollback_wrong_thread_no_other_thread_mutation",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::priority_readback_query,Failure::wrong_thread)&&api.sets==1&&receipt.rollback_failure==Failure::wrong_thread&&!receipt.rollback_restored);
 }
 {
  PriorityApi api;api.ignore_set=1;api.fail_read=3;ConsumerPrioritySnapshot receipt;
  t.check("rollback_readback_query_failure_not_verified",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::priority_readback_mismatch,Failure::rollback_readback_query)&&receipt.rollback_readback==consumer_priority_query_error&&!receipt.rollback_restored);
 }
 {
  PriorityApi api;api.fail_read=2;api.ignore_set=2;ConsumerPrioritySnapshot receipt;
  t.check("rollback_write_success_but_mismatch_is_typed",rejects([&]{ConsumerPriorityScope scope(api,receipt);},Failure::priority_readback_query,Failure::rollback_readback_mismatch)&&api.value==1&&!receipt.rollback_restored);
 }
 {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);api.id=8;const auto sets=api.sets;
   t.check("wrong_thread_close_cannot_mutate_other_thread",rejects([&]{scope.close();},Failure::wrong_thread)&&api.sets==sets&&!receipt.restored);
   api.id=7;scope.close();
   t.check("later_cleanup_cannot_erase_wrong_thread_failure",api.value==-1&&receipt.restored&&receipt.failure==Failure::wrong_thread&&!receipt.verified());
  }
 }
 {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);api.fail_set=2;
   t.check("explicit_restore_set_failure_rejected",rejects([&]{scope.close();},Failure::restore_set)&&!receipt.restored&&receipt.failure==Failure::restore_set);
  }
  t.check("fallback_cleanup_cannot_upgrade_failed_close",api.value==-1&&receipt.fallback_attempted&&receipt.fallback_readback==-1&&!receipt.restored&&!receipt.verified());
 }
 {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);api.fail_read=3;
   t.check("explicit_restore_readback_query_failure_rejected",rejects([&]{scope.close();},Failure::restore_readback_query)&&!receipt.restored);
  }
  t.check("restore_query_failure_retained_after_fallback",api.value==-1&&receipt.failure==Failure::restore_readback_query&&!receipt.verified());
 }
 {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);api.ignore_set=2;
   t.check("restore_write_success_mismatch_rejected",rejects([&]{scope.close();},Failure::restore_readback_mismatch)&&api.value==1&&!receipt.restored);
  }
  t.check("restore_mismatch_cleanup_not_admission",api.value==-1&&receipt.failure==Failure::restore_readback_mismatch&&!receipt.verified());
 }
 {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  try{ConsumerPriorityScope scope(api,receipt);throw std::runtime_error("work failed");}catch(const std::runtime_error&){}
  t.check("work_exception_fallback_restores_but_never_verified",api.value==-1&&receipt.fallback_attempted&&!receipt.close_attempted&&!receipt.restored&&!receipt.verified());
 }
 {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);api.id=8;}
  t.check("wrong_thread_destructor_does_not_mutate",api.sets==1&&api.value==1&&receipt.fallback_failure==Failure::wrong_thread&&!receipt.verified());
 }
 for(int variant=0;variant<3;++variant) {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);if(variant==0)api.fail_set=2;else if(variant==1)api.fail_read=3;else api.ignore_set=2;}
  const auto reason=variant==0?Failure::restore_set:variant==1?Failure::restore_readback_query:Failure::restore_readback_mismatch;
  t.check(variant==0?"fallback_set_failure_retained":variant==1?"fallback_query_failure_retained":"fallback_mismatch_retained",receipt.fallback_attempted&&receipt.fallback_failure==reason&&!receipt.restored&&!receipt.verified());
 }
 for(const bool failed_query:{false,true}) {
  PriorityApi api;ConsumerPrioritySnapshot receipt;
  {ConsumerPriorityScope scope(api,receipt);api.process=failed_query?0:0x80;
   const auto reason=failed_query?Failure::process_query:Failure::normal_process_required;
   t.check(failed_query?"process_query_failure_after_close_not_verified":"changed_process_class_after_close_not_verified",rejects([&]{scope.close();},reason)&&api.value==-1&&receipt.restored&&!receipt.verified());
   const auto sets=api.sets;scope.close();t.check(failed_query?"failed_process_query_close_no_extra_mutation":"changed_process_close_no_extra_mutation",api.sets==sets&&!receipt.verified());
  }
 }
}
int main(int argc,char** argv) {
 try {
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh consumer priority test output");
  Tests t;controls(t);
#ifdef _WIN32
  WindowsConsumerPriorityApi api;ConsumerPrioritySnapshot receipt;
  const auto process=api.process_class(),thread=api.thread();const auto before=api.current();
  {ConsumerPriorityScope scope(api,receipt);
   t.check("actual_windows_current_thread_readback",api.current()==THREAD_PRIORITY_ABOVE_NORMAL&&api.thread()==thread&&api.process_class()==process);
   scope.close();
  }
  t.check("actual_windows_exact_prior_restored_normal_process",api.current()==before&&api.process_class()==process&&receipt.verified()&&!receipt.fallback_attempted);
#endif
  J result={{"schema","pas.consumer-priority-tests.v1"},{"assertions",t.checks.size()},{"checks",t.checks},{"failed",t.failed},{"pass",t.failed==0},
   {"cost_qualification",false},{"live_ready",false},{"device_endpoints",0},{"device_operations",0},{"qualification_scope","current-thread scheduling contract only; noise and physical remain separate"}};
  std::ofstream out(argv[1],std::ios::binary);out<<result.dump(2)<<'\n';if(!out)throw std::runtime_error("consumer priority test output write");
  std::cout<<"assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
