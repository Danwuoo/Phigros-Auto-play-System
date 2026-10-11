#include "producer_wait.hpp"
#include <array>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace pas;using namespace pas::current_rails;using J=nlohmann::json;
struct Tests{J checks=J::array();int failed=0;void check(std::string name,bool pass){checks.push_back({{"id",name},{"pass",pass}});failed+=!pass;}};
struct PriorityApi {int value=-1,calls=0,fail_at=0;bool ignore=false;std::uint32_t id=1;int current()const{return value;}std::uint32_t thread()const{return id;}bool set(int p){if(++calls==fail_at)return false;if(!ignore)value=p;return true;}};
void priority_tests(Tests&t){
 PriorityApi api;{ProducerPriorityScope scope(api);t.check("producer_only_highest",api.value==2&&producer_priority_requested==2);scope.close();t.check("explicit_restore_prior_priority",api.value==-1&&api.calls==2);scope.close();t.check("close_idempotent",api.calls==2);}
 {ProducerPriorityScope scope(api);}t.check("exception_scope_restores_priority",api.value==-1&&api.calls==4);
 api.value=std::numeric_limits<int>::max();bool rejected=false;try{ProducerPriorityScope scope(api);}catch(...){rejected=true;}t.check("query_failure_before_priority_mutation",rejected&&api.calls==4);
 api.value=0;api.fail_at=5;rejected=false;try{ProducerPriorityScope scope(api);}catch(...){rejected=true;}t.check("set_failure_no_silent_fallback",rejected&&api.value==0&&api.calls==5);
 api.fail_at=0;api.ignore=true;rejected=false;try{ProducerPriorityScope scope(api);}catch(...){rejected=true;}t.check("set_success_readback_mismatch_rejected",rejected&&api.value==0);
 api.ignore=false;{ProducerPriorityScope scope(api);api.fail_at=api.calls+1;rejected=false;try{scope.close();}catch(...){rejected=true;}t.check("restore_failure_disqualifies",rejected);api.fail_at=0;}t.check("failed_restore_unwind_cleanup",api.value==0);
}
void active_tail_tests(Tests& t){
 const auto rejects=[](auto&& work,const char* reason){try{work();}catch(const std::runtime_error& error){return std::string(error.what())==reason;}catch(...){return false;}return false;};
 {FakeClock c;c.set(1000);int waits=0,pauses=0;wait_producer_deadline_with_active_tail(c,1000,[&](Nanoseconds){++waits;},[&]{++pauses;});t.check("active_tail_due_now_no_callbacks",waits==0&&pauses==0&&c.now_ns()==1000);}
 {FakeClock c;c.set(1000);int waits=0,pauses=0;wait_producer_deadline_with_active_tail(c,999,[&](Nanoseconds){++waits;},[&]{++pauses;});t.check("active_tail_already_late_no_reset_or_callbacks",waits==0&&pauses==0&&c.now_ns()==1000);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;Nanoseconds ticks=0,tail_begin=-1;wait_producer_deadline_with_active_tail(c,30'000'000,[&](Nanoseconds n){++waits;ticks=n;c.set(c.now_ns()+100*n);},[&]{if(!pauses)tail_begin=c.now_ns();++pauses;c.set(c.now_ns()+1'000'000);});t.check("active_tail_kernel_only_to_due_minus_2ms",waits==1&&ticks==280'000&&tail_begin==28'000'000&&pauses==2&&c.now_ns()==30'000'000);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;Nanoseconds ticks=0,tail_begin=-1;constexpr Nanoseconds due=2'000'101;wait_producer_deadline_with_active_tail(c,due,[&](Nanoseconds n){++waits;ticks=n;c.set(100*n);},[&]{tail_begin=c.now_ns();++pauses;c.set(due);});t.check("active_tail_kernel_ceil100ns_then_QPC_due",waits==1&&ticks==2&&tail_begin==200&&pauses==1&&c.now_ns()==due);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;wait_producer_deadline_with_active_tail(c,2'000'000,[&](Nanoseconds){++waits;},[&]{++pauses;c.set(c.now_ns()+1'000'000);});t.check("active_tail_full_2ms_QPC_no_kernel_sleep",producer_active_tail_ns==2'000'000&&waits==0&&pauses==2&&c.now_ns()==2'000'000);}
 {FakeClock c;c.set(1234);int waits=0,pauses=0;wait_producer_deadline_with_active_tail(c,1235,[&](Nanoseconds){++waits;},[&]{++pauses;c.set(1235);});t.check("active_tail_last_ns_not_early",waits==0&&pauses==1&&c.now_ns()==1235);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;Nanoseconds first=0,second=0;wait_producer_deadline_with_active_tail(c,50'000'000,[&](Nanoseconds n){++waits;if(waits==1){first=n;c.set(47'000'000);}else{second=n;c.set(c.now_ns()+100*n);}},[&]{++pauses;c.set(c.now_ns()+1'000'000);});t.check("active_tail_early_kernel_wake_rechecks_remaining",waits==2&&first==480'000&&second==10'000&&pauses==2&&c.now_ns()==50'000'000);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,50'000'000,[&](Nanoseconds){++waits;},[&]{++pauses;});},"producer repeated early wake");t.check("active_tail_repeated_kernel_early_wake_four_bound",bad&&waits==4&&pauses==0&&c.now_ns()==0);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;wait_producer_deadline_with_active_tail(c,50'000'000,[&](Nanoseconds){if(++waits==4)c.set(48'000'000);},[&]{++pauses;c.set(50'000'000);});t.check("active_tail_fourth_kernel_wake_can_enter_tail",waits==4&&pauses==1&&c.now_ns()==50'000'000);}
 {FakeClock c;c.set(0);int waits=0;std::uint32_t pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,1,[&](Nanoseconds){++waits;},[&]{++pauses;});},"producer active tail poll bound");t.check("active_tail_stalled_QPC_hard_poll_cap",bad&&waits==0&&pauses==(1u<<24)&&c.now_ns()==0);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,30'000'000,[&](Nanoseconds){++waits;throw std::runtime_error("kernel failure");},[&]{++pauses;});},"kernel failure");t.check("active_tail_kernel_failure_no_spin_fallback",bad&&waits==1&&pauses==0);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,2'000'000,[&](Nanoseconds){++waits;},[&]{++pauses;throw std::runtime_error("pause failure");});},"pause failure");t.check("active_tail_pause_failure_not_hidden",bad&&waits==0&&pauses==1);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;std::array<Nanoseconds,2> ticks{};auto wait=[&](Nanoseconds n){if(waits<2)ticks[waits]=n;++waits;c.set(waits==1?67'000'000:c.now_ns()+100*n);};auto pause=[&]{++pauses;c.set(c.now_ns()+1'000'000);};wait_producer_deadline_with_active_tail(c,40'000'000,wait,pause);const auto late=c.now_ns();wait_producer_deadline_with_active_tail(c,60'000'000,wait,pause);const auto next=c.now_ns();wait_producer_deadline_with_active_tail(c,80'000'000,wait,pause);t.check("active_tail_kernel_27ms_overshoot_keeps_next_absolute_due",late==67'000'000&&next==67'000'000&&waits==2&&ticks[0]==380'000&&ticks[1]==110'000&&pauses==2&&c.now_ns()==80'000'000);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;Nanoseconds first_pause=-1;std::array<Nanoseconds,2> ticks{};auto wait=[&](Nanoseconds n){if(waits<2)ticks[waits]=n;++waits;c.set(c.now_ns()+100*n);};auto pause=[&]{if(!pauses)first_pause=c.now_ns();++pauses;c.set(pauses==1?47'000'000:c.now_ns()+1'000'000);};wait_producer_deadline_with_active_tail(c,20'000'000,wait,pause);const auto late=c.now_ns();wait_producer_deadline_with_active_tail(c,40'000'000,wait,pause);const auto next=c.now_ns();wait_producer_deadline_with_active_tail(c,60'000'000,wait,pause);t.check("active_tail_spin_27ms_overshoot_keeps_next_absolute_due",first_pause==18'000'000&&late==47'000'000&&next==47'000'000&&waits==2&&ticks[0]==180'000&&ticks[1]==110'000&&pauses==3&&c.now_ns()==60'000'000);}
 {FakeClock c;constexpr Nanoseconds epoch=1'234'567;c.set(epoch);int waits=0,pauses=0,attempts=0;Nanoseconds slept=0;bool exact=true,exact_ticks=true;for(int i=0;i<2560;++i){const auto due=epoch+Nanoseconds(i)*20'000'000;wait_producer_deadline_with_active_tail(c,due,[&](Nanoseconds n){++waits;exact_ticks&=n==180'000;slept+=100*n;c.set(c.now_ns()+100*n);},[&]{++pauses;c.set(c.now_ns()+1'000'000);});++attempts;exact&=c.now_ns()==due;}t.check("active_tail_all_2560_original_absolute_20ms_attempts",exact&&exact_ticks&&attempts==2560&&waits==2559&&slept==Nanoseconds(2559)*18'000'000&&pauses==2559*2&&c.now_ns()==epoch+Nanoseconds(2559)*20'000'000);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;Nanoseconds ticks=0;wait_producer_deadline_with_active_tail(c,100'000'000,[&](Nanoseconds n){++waits;ticks=n;c.set(100*n);},[&]{++pauses;c.set(c.now_ns()+1'000'000);});t.check("active_tail_original_100ms_horizon_inclusive",waits==1&&ticks==980'000&&pauses==2&&c.now_ns()==100'000'000);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,100'000'001,[&](Nanoseconds){++waits;},[&]{++pauses;});},"producer wait horizon");t.check("active_tail_distant_horizon_before_callbacks",bad&&waits==0&&pauses==0);}
 {FakeClock c;c.set(-1);int waits=0,pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,0,[&](Nanoseconds){++waits;},[&]{++pauses;});},"producer QPC domain");t.check("active_tail_negative_QPC_before_callbacks",bad&&waits==0&&pauses==0);}
 {FakeClock c;c.set(0);int waits=0,pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,-1,[&](Nanoseconds){++waits;},[&]{++pauses;});},"producer QPC domain");t.check("active_tail_negative_due_before_callbacks",bad&&waits==0&&pauses==0);}
 {FakeClock c;c.set(1000);int waits=0,pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,50'000'000,[&](Nanoseconds){++waits;c.set(999);},[&]{++pauses;});},"producer nonmonotonic QPC");t.check("active_tail_backward_QPC_after_kernel_rejected",bad&&waits==1&&pauses==0);}
 {FakeClock c;c.set(1000);int waits=0,pauses=0;const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,1001,[&](Nanoseconds){++waits;},[&]{++pauses;c.set(999);});},"producer nonmonotonic QPC");t.check("active_tail_backward_QPC_after_pause_rejected",bad&&waits==0&&pauses==1);}
}
// These checks exercise the optional trace through the same wait/arm helpers,
// without native calls. They are a separate denominator from the original 43.
struct TraceClock final:Clock {
 mutable std::uint32_t reads=0;std::uint32_t fail_read=0;Nanoseconds value=0;
 Nanoseconds now_ns()const override{if(++reads==fail_read)throw std::runtime_error("fake QPC failure");return value;}
};
struct TraceTimerApi {
 TraceClock& clock;ProducerTimerResult arm_result{1,0},wait_result{0,0};
 Nanoseconds arm_cost=0,wake=-1;int arms=0,waits=0;Nanoseconds ticks=0;std::uint32_t timeout=0;
 ProducerTimerResult arm(Nanoseconds n){++arms;ticks=n;clock.value+=arm_cost;return arm_result;}
 ProducerTimerResult wait(std::uint32_t ms){++waits;timeout=ms;if(wake>=0)clock.value=wake;return wait_result;}
};
J diagnostic_trace_tests(){
 Tests d;const auto rejects=[](auto&& body,const char* reason){try{body();}catch(const std::runtime_error& e){return std::string(e.what())==reason;}catch(...){return false;}return false;};
 {TraceClock c;TraceTimerApi api{c};producer_kernel_wait_phase(c,180000,api);
  d.check("null_kernel_trace_has_zero_extra_QPC_and_original_two_API_calls",c.reads==0&&api.arms==1&&api.waits==1&&api.ticks==180000&&api.timeout==250);}
 {TraceClock c;c.value=1000;TraceTimerApi api{c};api.arm_cost=100;api.wake=1900;ProducerKernelTrace k;ProducerWaitPhase p=ProducerWaitPhase::none;
  producer_kernel_wait_phase(c,7,api,&k,&p);
  d.check("kernel_trace_keeps_distinct_arm_wait_boundaries_and_raw_success",c.reads==4&&k.ticks_100ns==7&&k.arm_begin_ns==1000&&k.arm_end_ns==1100&&k.kernel_begin_ns==1100&&k.kernel_end_ns==1900&&k.arm_status==1&&k.wait_result==0&&k.timestamp_flags==15&&k.api_flags==3&&p==ProducerWaitPhase::kernel_wait);}
 {TraceClock plain,traced;ProducerWaitTrace t;std::array<Nanoseconds,3> plain_order{},trace_order{};int pn=0,tn=0;
  wait_producer_deadline_with_active_tail(plain,20'000'000,[&](Nanoseconds n){plain_order[std::size_t(pn++)]=n;plain.value+=100*n;},[&]{plain_order[std::size_t(pn++)]=-1;plain.value+=1'000'000;});
  wait_producer_deadline_with_active_tail(traced,20'000'000,[&](Nanoseconds n){trace_order[std::size_t(tn++)]=n;traced.value+=100*n;},[&]{trace_order[std::size_t(tn++)]=-1;traced.value+=1'000'000;},&t);
  d.check("trace_and_null_preserve_due_callback_order_and_algorithm_QPC_reads",plain_order==trace_order&&pn==3&&tn==3&&plain.reads==traced.reads&&traced.reads==4&&plain.value==traced.value&&t.completed&&t.kernel_calls==1&&t.spin_poll_count==2);}
 {TraceClock c;c.value=67'000'000;ProducerWaitTrace t;t.kernel_calls=4;t.spin_poll_count=99;t.kernels[0].arm_status=1;int callbacks=0;
  wait_producer_deadline_with_active_tail(c,60'000'000,[&](Nanoseconds){++callbacks;},[&]{++callbacks;},&t);
  d.check("already_late_trace_clears_old_facts_without_reset_or_callbacks",callbacks==0&&c.reads==1&&t.due_ns==60'000'000&&t.first_qpc_ns==67'000'000&&t.last_qpc_ns==67'000'000&&t.completed&&!t.spin_entered&&t.kernel_calls==0&&t.kernels[0].arm_status==std::numeric_limits<std::uint32_t>::max());}
 {TraceClock c;TraceTimerApi api{c};ProducerWaitTrace t;
  const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,50'000'000,[&](Nanoseconds n){producer_kernel_wait_phase(c,n,api,&t.kernels[t.kernel_calls-1],&t.phase);},[]{},&t);},"producer repeated early wake");
  bool all=true;for(const auto& k:t.kernels)all&=k.ticks_100ns==480'000&&k.arm_status==1&&k.wait_result==0&&k.timestamp_flags==15;
  d.check("early_wake_failure_retains_exactly_four_complete_kernel_samples",bad&&api.arms==4&&api.waits==4&&t.kernel_calls==4&&all&&!t.completed&&!t.spin_entered&&t.failure==ProducerWaitFailure::early_wake_bound);}
 {TraceClock c;ProducerWaitTrace t;int waits=0;
  wait_producer_deadline_with_active_tail(c,50'000'000,[&](Nanoseconds){if(++waits==4)c.value=48'000'000;},[&]{c.value=50'000'000;},&t);
  d.check("fourth_early_wake_can_complete_tail_without_fifth_kernel_sample",t.completed&&t.kernel_calls==4&&t.spin_enter_ns==48'000'000&&t.spin_return_ns==50'000'000&&t.spin_poll_count==1&&t.spin_pause_calls==1&&t.spin_completed);}
 {TraceClock c;TraceTimerApi api{c};api.arm_result={0,5};ProducerWaitTrace t;
  const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,20'000'000,[&](Nanoseconds n){producer_kernel_wait_phase(c,n,api,&t.kernels[t.kernel_calls-1],&t.phase);},[]{},&t);},"producer timer arm");
  const auto& k=t.kernels[0];d.check("arm_false_keeps_GetLastError_and_unreached_kernel_unknown",bad&&api.arms==1&&api.waits==0&&t.kernel_calls==1&&k.arm_status==0&&k.arm_error==5&&k.wait_result==std::numeric_limits<std::uint32_t>::max()&&k.timestamp_flags==3&&k.api_flags==1&&t.failure_phase==ProducerWaitPhase::timer_arm&&!t.completed);}
 {TraceClock c;TraceTimerApi api{c};api.wait_result={std::numeric_limits<std::uint32_t>::max(),6};ProducerWaitTrace t;
  const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,20'000'000,[&](Nanoseconds n){producer_kernel_wait_phase(c,n,api,&t.kernels[t.kernel_calls-1],&t.phase);},[]{},&t);},"producer bounded timer wait");
  const auto& k=t.kernels[0];d.check("WAIT_FAILED_keeps_raw_status_error_and_complete_boundary_flags",bad&&api.arms==1&&api.waits==1&&k.wait_result==std::numeric_limits<std::uint32_t>::max()&&k.wait_error==6&&k.timestamp_flags==15&&k.api_flags==3&&t.failure_phase==ProducerWaitPhase::kernel_wait&&!t.completed);}
 {TraceClock c;TraceTimerApi api{c};api.wait_result={258,0};ProducerWaitTrace t;
  const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,20'000'000,[&](Nanoseconds n){producer_kernel_wait_phase(c,n,api,&t.kernels[t.kernel_calls-1],&t.phase);},[]{},&t);},"producer bounded timer wait");
  d.check("timeout_raw258_is_not_success_or_spin_fallback",bad&&t.kernels[0].wait_result==258&&t.kernels[0].wait_error==0&&!t.spin_entered&&!t.completed);}
 {TraceClock c;ProducerWaitTrace t;
  const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,1,[](Nanoseconds){},[]{throw std::runtime_error("pause fault");},&t);},"pause fault");
  d.check("pause_exception_keeps_attempted_vs_completed_poll_facts",bad&&t.spin_entered&&!t.spin_completed&&t.spin_pause_calls==1&&t.spin_poll_count==0&&t.failure==ProducerWaitFailure::pause_callback&&t.failure_phase==ProducerWaitPhase::spin&&!t.completed);}
 {TraceClock c;c.value=1000;ProducerWaitTrace t;
  const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,50'000'000,[&](Nanoseconds){c.value=999;},[]{},&t);},"producer nonmonotonic QPC");
  d.check("nonmonotonic_raw_QPC_retained_without_making_valid_duration",bad&&t.first_qpc_ns==1000&&t.last_qpc_ns==999&&t.qpc_reads==2&&t.failure==ProducerWaitFailure::nonmonotonic_clock&&!t.completed);}
 {TraceClock c;c.fail_read=1;ProducerWaitTrace t;
  const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,0,[](Nanoseconds){},[]{},&t);},"fake QPC failure");
  TraceClock arm_clock;arm_clock.fail_read=3;TraceTimerApi api{arm_clock};ProducerWaitTrace partial;
  const bool arm_bad=rejects([&]{wait_producer_deadline_with_active_tail(arm_clock,20'000'000,[&](Nanoseconds n){producer_kernel_wait_phase(arm_clock,n,api,&partial.kernels[partial.kernel_calls-1],&partial.phase);},[]{},&partial);},"fake QPC failure");
  d.check("QPC_failures_keep_unknown_read_and_partial_actual_arm_facts",bad&&t.entered&&t.qpc_reads==0&&!t.completed&&t.failure==ProducerWaitFailure::clock_read&&t.failure_phase==ProducerWaitPhase::qpc&&arm_bad&&api.arms==1&&api.waits==0&&partial.kernels[0].api_flags==1&&partial.kernels[0].timestamp_flags==1&&partial.kernels[0].arm_status==1&&!partial.completed);}
 {ProducerThreadCpuSample a{3,4,1,0,true},b{8,11,1,0,true};const auto delta=producer_thread_cpu_delta_100ns(a,b);
  d.check("CPU_delta_uses_raw_kernel_plus_user_100ns_accounting",delta&&*delta==12);}
 {ProducerThreadCpuSample a{3,4,1,0,true},failed{0,0,0,5,true};
  d.check("failed_CPU_sample_is_unknown_and_retains_actual_error",!producer_thread_cpu_delta_100ns(a,failed)&&failed.status==0&&failed.error==5);}
 {ProducerThreadCpuSample a{3,4,1,0,true},backward{2,100,1,0,true};
  d.check("backward_CPU_counter_does_not_cancel_against_other_counter",!producer_thread_cpu_delta_100ns(a,backward));}
 {ProducerThreadCpuSample a{0,0,1,0,true},b{std::numeric_limits<std::uint64_t>::max(),1,1,0,true};
  d.check("CPU_delta_overflow_remains_unknown",!producer_thread_cpu_delta_100ns(a,b));}
 {ProducerThreadCpuSample zero{3,4,1,0,true},unknown;
  const auto known=producer_thread_cpu_delta_100ns(zero,zero);
  d.check("genuine_zero_CPU_is_available_but_unreached_sample_is_unknown",known&&*known==0&&!producer_thread_cpu_delta_100ns(zero,unknown));}
 {TraceClock c;c.value=1'234'567;bool exact=true;std::uint32_t kernels=0,polls=0;
  for(int i=0;i<2560;++i){ProducerWaitTrace t;const auto due=1'234'567+Nanoseconds(i)*20'000'000;
   wait_producer_deadline_with_active_tail(c,due,[&](Nanoseconds n){c.value+=100*n;},[&]{c.value+=1'000'000;},&t);
   exact&=t.completed&&t.due_ns==due&&t.last_qpc_ns==due&&t.kernel_calls==(i?1u:0u)&&t.spin_poll_count==(i?2u:0u);kernels+=t.kernel_calls;polls+=t.spin_poll_count;}
  d.check("all_2560_trace_rows_keep_original_absolute_due_and_full_counts",exact&&kernels==2559&&polls==5118);}
 {TraceClock c;ProducerWaitTrace first,next;int waits=0;
  wait_producer_deadline_with_active_tail(c,40'000'000,[&](Nanoseconds){++waits;c.value=67'000'000;},[]{},&first);
  wait_producer_deadline_with_active_tail(c,60'000'000,[&](Nanoseconds){++waits;},[]{},&next);
  d.check("27ms_timer_overshoot_is_retained_and_next_due_is_not_reset",waits==1&&first.completed&&first.last_qpc_ns==67'000'000&&first.kernel_calls==1&&!first.spin_entered&&next.completed&&next.due_ns==60'000'000&&next.first_qpc_ns==67'000'000&&next.kernel_calls==0);}
 {TraceClock c;ProducerWaitTrace t;
  const bool bad=rejects([&]{wait_producer_deadline_with_active_tail(c,1,[](Nanoseconds){},[]{},&t);},"producer active tail poll bound");
  d.check("stalled_spin_trace_counters_stop_at_original_hard_cap",bad&&t.spin_poll_count==producer_active_tail_poll_cap&&t.spin_pause_calls==producer_active_tail_poll_cap&&t.qpc_reads==producer_active_tail_poll_cap+1&&!t.completed&&!t.spin_completed&&t.failure==ProducerWaitFailure::spin_bound);}
 if(d.checks.size()!=20)throw std::runtime_error("fixed wait diagnostic control denominator");
 return {{"schema","pas.producer-wait-diagnostic-controls.v1"},{"assertions",d.checks.size()},{"checks",d.checks},{"failed",d.failed},{"pass",d.failed==0},
  {"synthetic_api_controls",true},{"kernel_sample_cap",producer_wait_kernel_cap},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
}
// Independent full-period candidate controls; the original 43 and trace 20
// keep their existing cases and denominators.
J active_wait_tests(){
 Tests d;const auto rejects=[](auto&& body,const char* reason){try{body();}catch(const std::runtime_error& e){return std::string(e.what())==reason;}catch(...){return false;}return false;};
 {TraceClock c;c.value=1000;ProducerWaitTrace trace;int pauses=0;
  wait_producer_deadline_active(c,1000,[&]{++pauses;},&trace);const bool due=trace.completed&&!trace.spin_entered&&c.reads==1;
  wait_producer_deadline_active(c,999,[&]{++pauses;},&trace);const bool late=trace.completed&&!trace.spin_entered&&trace.due_ns==999&&trace.last_qpc_ns==1000&&c.reads==2;
  wait_producer_deadline_active(c,2000,[&]{++pauses;c.value=2000;},&trace);
  d.check("due_and_already_late_keep_next_absolute_deadline",due&&late&&pauses==1&&c.reads==4&&c.value==2000&&trace.due_ns==2000&&trace.completed&&trace.kernel_calls==0);}
 {TraceClock plain,traced;ProducerWaitTrace trace;std::array<Nanoseconds,20> plain_order{},trace_order{};int pn=0,tn=0;
  wait_producer_deadline_active(plain,20'000'000,[&]{if(pn<20)plain_order[std::size_t(pn)]=plain.value;++pn;plain.value+=1'000'000;});
  wait_producer_deadline_active(traced,20'000'000,[&]{if(tn<20)trace_order[std::size_t(tn)]=traced.value;++tn;traced.value+=1'000'000;},&trace);
  bool unknown=true;for(const auto& k:trace.kernels)unknown&=k.api_flags==0&&k.timestamp_flags==0&&k.arm_status==std::numeric_limits<std::uint32_t>::max()&&k.wait_result==std::numeric_limits<std::uint32_t>::max();
  d.check("whole_20ms_active_and_null_trace_have_identical_reads_and_pause_order",plain_order==trace_order&&pn==20&&tn==20&&plain.reads==21&&traced.reads==plain.reads&&plain.value==traced.value&&trace.completed&&trace.kernel_calls==0&&unknown&&trace.spin_enter_ns==0&&trace.spin_return_ns==20'000'000&&trace.spin_poll_count==20&&trace.spin_pause_calls==20);}
 {TraceClock inclusive,rejected_clock;ProducerWaitTrace ok,bad;int pauses=0,rejected_pauses=0;
  wait_producer_deadline_active(inclusive,100'000'000,[&]{++pauses;inclusive.value+=50'000'000;},&ok);
  const bool rejected=rejects([&]{wait_producer_deadline_active(rejected_clock,100'000'001,[&]{++rejected_pauses;},&bad);},"producer wait horizon");
  d.check("100ms_initial_horizon_is_inclusive_and_excess_fails_before_pause",ok.completed&&pauses==2&&inclusive.value==100'000'000&&rejected&&rejected_clock.reads==1&&rejected_pauses==0&&!bad.completed&&!bad.spin_entered&&bad.failure==ProducerWaitFailure::horizon);}
 {TraceClock negative_clock,negative_due;negative_clock.value=-1;ProducerWaitTrace a,b;int pauses=0;
  const bool clock_bad=rejects([&]{wait_producer_deadline_active(negative_clock,0,[&]{++pauses;},&a);},"producer QPC domain");
  const bool due_bad=rejects([&]{wait_producer_deadline_active(negative_due,-1,[&]{++pauses;},&b);},"producer QPC domain");
  d.check("negative_clock_or_deadline_fails_before_any_pause",clock_bad&&due_bad&&pauses==0&&a.failure==ProducerWaitFailure::clock_domain&&b.failure==ProducerWaitFailure::clock_domain&&!a.completed&&!b.completed);}
 {TraceClock c;c.value=1000;ProducerWaitTrace trace;int pauses=0;
  const bool bad=rejects([&]{wait_producer_deadline_active(c,1001,[&]{++pauses;c.value=999;},&trace);},"producer nonmonotonic QPC");
  d.check("backward_QPC_is_retained_and_rejected",bad&&pauses==1&&trace.first_qpc_ns==1000&&trace.last_qpc_ns==999&&trace.qpc_reads==2&&trace.spin_poll_count==1&&trace.failure==ProducerWaitFailure::nonmonotonic_clock&&!trace.completed&&!trace.spin_completed);}
 {TraceClock c;ProducerWaitTrace trace;std::uint32_t pauses=0;
  const bool bad=rejects([&]{wait_producer_deadline_active(c,1,[&]{++pauses;},&trace);},"producer active wait poll bound");
  d.check("stalled_clock_stops_at_total_full_period_poll_cap",bad&&pauses==producer_active_wait_poll_cap&&trace.spin_pause_calls==producer_active_wait_poll_cap&&trace.spin_poll_count==producer_active_wait_poll_cap&&trace.qpc_reads==producer_active_wait_poll_cap+1&&trace.kernel_calls==0&&trace.failure==ProducerWaitFailure::spin_bound&&trace.failure_phase==ProducerWaitPhase::spin&&!trace.completed&&!trace.spin_completed);}
 {TraceClock c;ProducerWaitTrace trace;int pauses=0;
  const bool bad=rejects([&]{wait_producer_deadline_active(c,1,[&]{++pauses;throw std::runtime_error("active pause fault");},&trace);},"active pause fault");
  d.check("pause_exception_keeps_attempted_and_completed_poll_counts_distinct",bad&&pauses==1&&trace.qpc_reads==1&&trace.spin_pause_calls==1&&trace.spin_poll_count==0&&trace.failure==ProducerWaitFailure::pause_callback&&trace.failure_phase==ProducerWaitPhase::spin&&!trace.completed);}
 {TraceClock first,after_pause;first.fail_read=1;after_pause.fail_read=2;ProducerWaitTrace a,b;int initial_pauses=0,later_pauses=0;
  const bool first_bad=rejects([&]{wait_producer_deadline_active(first,1,[&]{++initial_pauses;},&a);},"fake QPC failure");
  const bool later_bad=rejects([&]{wait_producer_deadline_active(after_pause,1,[&]{++later_pauses;},&b);},"fake QPC failure");
  d.check("QPC_exceptions_preserve_unreached_and_partial_trace_facts",first_bad&&later_bad&&initial_pauses==0&&later_pauses==1&&a.entered&&a.qpc_reads==0&&!a.completed&&b.qpc_reads==1&&b.spin_poll_count==1&&b.spin_pause_calls==1&&!b.completed&&a.failure==ProducerWaitFailure::clock_read&&b.failure==ProducerWaitFailure::clock_read&&a.failure_phase==ProducerWaitPhase::qpc&&b.failure_phase==ProducerWaitPhase::qpc);}
 {TraceClock c;ProducerWaitTrace first,next,following;int pauses=0;auto pause=[&]{if(++pauses==1)c.value=67'000'000;else c.value+=1'000'000;};
  wait_producer_deadline_active(c,40'000'000,pause,&first);wait_producer_deadline_active(c,60'000'000,pause,&next);wait_producer_deadline_active(c,80'000'000,pause,&following);
  d.check("27ms_pause_overshoot_is_retained_without_resetting_or_skipping_next_due",pauses==14&&first.completed&&first.due_ns==40'000'000&&first.last_qpc_ns==67'000'000&&next.completed&&next.due_ns==60'000'000&&next.first_qpc_ns==67'000'000&&!next.spin_entered&&following.completed&&following.due_ns==80'000'000&&c.value==80'000'000&&first.kernel_calls==0&&next.kernel_calls==0&&following.kernel_calls==0);}
 {TraceClock c;constexpr Nanoseconds epoch=1'234'567;c.value=epoch;bool exact=true;std::uint32_t pauses=0;
  for(int i=0;i<2560;++i){ProducerWaitTrace trace;const auto due=epoch+Nanoseconds(i)*20'000'000;wait_producer_deadline_active(c,due,[&]{++pauses;c.value+=1'000'000;},&trace);
   exact&=trace.completed&&trace.due_ns==due&&trace.last_qpc_ns==due&&trace.kernel_calls==0&&trace.spin_poll_count==(i?20u:0u)&&trace.qpc_reads==(i?21u:1u);}
  d.check("all_2560_absolute_20ms_attempts_remain_present_with_no_kernel_calls",exact&&pauses==2559u*20u&&c.reads==2560u+pauses&&c.value==epoch+Nanoseconds(2559)*20'000'000);}
 {TraceClock c;const auto due=std::numeric_limits<Nanoseconds>::max();c.value=due-1;ProducerWaitTrace trace;int pauses=0;
  wait_producer_deadline_active(c,due,[&]{++pauses;c.value=due;},&trace);
  d.check("last_ns_near_signed_limit_does_not_overflow_or_return_early",pauses==1&&c.reads==2&&trace.completed&&trace.first_qpc_ns==due-1&&trace.last_qpc_ns==due&&trace.spin_return_ns==due&&trace.kernel_calls==0);}
#ifdef _WIN32
 {HostClock real;ActiveProducerDeadlineWait waiter;ProducerWaitTrace trace;const auto due=real.now_ns()+20'000'000;waiter.wait_until(real,due,&trace);
  d.check("actual_Windows_active_QPC_class_is_not_before_due_and_has_no_kernel_samples",real.now_ns()>=due&&trace.completed&&trace.due_ns==due&&trace.last_qpc_ns>=due&&trace.kernel_calls==0&&trace.kernels[0].api_flags==0&&trace.kernels[0].arm_status==std::numeric_limits<std::uint32_t>::max()&&trace.kernels[0].wait_result==std::numeric_limits<std::uint32_t>::max());}
#else
 {TraceClock c;ProducerWaitTrace trace;trace.kernel_calls=4;trace.kernels[0].api_flags=3;trace.kernels[0].arm_status=1;trace.kernels[0].wait_result=0;int pauses=0;
  wait_producer_deadline_active(c,0,[&]{++pauses;},&trace);
  d.check("fresh_active_trace_clears_prior_timer_success_without_inventing_API_calls",trace.completed&&pauses==0&&trace.kernel_calls==0&&trace.kernels[0].api_flags==0&&trace.kernels[0].arm_status==std::numeric_limits<std::uint32_t>::max()&&trace.kernels[0].wait_result==std::numeric_limits<std::uint32_t>::max());}
#endif
 if(d.checks.size()!=12)throw std::runtime_error("fixed active QPC control denominator");
 return {{"schema","pas.producer-active-qpc-controls.v1"},{"assertions",d.checks.size()},{"checks",d.checks},{"failed",d.failed},{"pass",d.failed==0},
  {"synthetic_api_controls",true},{"device_operations",0},{"cost_qualification",false},{"live_ready",false},{"policy",producer_active_wait_policy},
  {"horizon_ns",producer_active_wait_horizon_ns},{"poll_cap",producer_active_wait_poll_cap},{"no_kernel_wait",true},{"cpu_cost","unknown"}};
}
// Separate API-state/cleanup controls, with no timing or cost assertion.
struct QosApiDouble {
 ColdProcessQosState value{1,0,0};std::uint32_t pid=41,tid=7,priority_class=0x20;
 int reads=0,sets=0,fail_read=0,fail_set=0,mismatch_read=0,ignore_set=0;
 bool failed_set_mutates=false;
 std::uint32_t process()const noexcept{return pid;}
 std::uint32_t thread()const noexcept{return tid;}
 std::uint32_t process_class()const noexcept{return priority_class;}
 ColdProcessQosApiResult get(ColdProcessQosState& s)noexcept{
  ++reads;s=value;if(reads==fail_read)return {0,87,true};
  if(reads==mismatch_read)s.state_mask^=4;return {1,0,true};
 }
 ColdProcessQosApiResult set(const ColdProcessQosState& s)noexcept{
  ++sets;if(sets==fail_set){if(failed_set_mutates)value=s;return {0,5,true};}
  if(sets!=ignore_set)value=s;return {1,0,true};
 }
};
template<class Api,class Body> std::string qos_error(Api& a,ColdProcessQosSnapshot& s,Body&& body){
 try{run_cold_process_qos(a,s,std::forward<Body>(body));}catch(const std::exception& e){return e.what();}return {};
}
J process_qos_tests(){
 Tests q;using F=ColdProcessQosFailure;using S=ColdProcessQosSnapshot;
 {QosApiDouble a;S s;bool body=false;int closed_reads=0,closed_sets=0;
  {ColdProcessQosScope scope(a,s);body=a.value==ColdProcessQosState{1,1,0};scope.close();closed_reads=a.reads;closed_sets=a.sets;scope.close();}
  q.check("system_managed_prior_exact_restore_and_idempotent_close",body&&s.verified()&&a.value==ColdProcessQosState{1,0,0}&&a.reads==closed_reads&&a.sets==closed_sets&&a.reads==3&&a.sets==2);}
 {QosApiDouble a;a.value={1,1,1};S s;bool high=false;const auto error=qos_error(a,s,[&]{high=a.value==ColdProcessQosState{1,1,0};});
  q.check("Eco_prior_execution_bit_disabled_then_full_prior_restored",error.empty()&&high&&s.verified()&&a.value==ColdProcessQosState{1,1,1});}
 {QosApiDouble a;a.value={1,1,0};S s;const auto error=qos_error(a,s,[]{});
  q.check("already_High_prior_is_queried_set_readback_and_restored",error.empty()&&s.verified()&&s.previous==s.requested&&a.sets==2);}
 {QosApiDouble a;a.value={1,4,4};S s;bool kept=false;const auto error=qos_error(a,s,[&]{kept=a.value==ColdProcessQosState{1,5,4};});
  q.check("known_timer_resolution_control_and_state_bits_preserved",error.empty()&&kept&&s.verified()&&a.value==ColdProcessQosState{1,4,4});}
 {QosApiDouble a;a.value={1,4,0};S s;bool kept=false;{ColdProcessQosScope scope(a,s);kept=a.value==ColdProcessQosState{1,5,0};}
  q.check("destructor_only_restore_cannot_issue_verified_and_keeps_known_off_bit",kept&&!s.verified()&&!s.close_attempted&&s.fallback_attempted&&s.fallback_restored&&a.value==ColdProcessQosState{1,4,0});}
 {QosApiDouble a;a.fail_read=1;S s;bool body=false;const auto error=qos_error(a,s,[&]{body=true;});
  q.check("prior_query_error_preserves_raw_status_and_never_sets",!error.empty()&&s.failure==F::prior_query&&s.query_previous.sampled&&s.query_previous.status==0&&s.query_previous.error==87&&!body&&a.sets==0&&!s.verified());}
 {QosApiDouble a;a.value.version=2;S s;const auto error=qos_error(a,s,[]{});
  q.check("unknown_version_rejected_before_state_mutation",!error.empty()&&s.failure==F::prior_version&&a.sets==0&&!s.requested_available);}
 {QosApiDouble a;a.value.control_mask=2;S s;const auto error=qos_error(a,s,[]{});
  q.check("unknown_control_bit_rejected_without_guessing_mask",!error.empty()&&s.failure==F::prior_bits&&a.sets==0);}
 {QosApiDouble a;a.value.state_mask=8;S s;const auto error=qos_error(a,s,[]{});
  q.check("unknown_state_bit_rejected_without_guessing_mask",!error.empty()&&s.failure==F::prior_bits&&a.sets==0);}
 {QosApiDouble a;a.priority_class=0x80;S s;const auto error=qos_error(a,s,[]{});
  q.check("non_NORMAL_process_rejected_before_query_or_set",!error.empty()&&s.failure==F::process_class&&a.reads==0&&a.sets==0);}
 {bool rejected=true;for(const bool process:{false,true}){QosApiDouble a;if(process)a.pid=0;else a.tid=0;S s;const auto error=qos_error(a,s,[]{});rejected&=!error.empty()&&s.failure==F::identity&&a.reads==0&&a.sets==0;}
  q.check("zero_process_or_owner_thread_identity_rejected_before_API",rejected);}
 {QosApiDouble a;a.fail_set=1;a.failed_set_mutates=true;S s;bool body=false;const auto error=qos_error(a,s,[&]{body=true;});
  q.check("failed_apply_with_side_effect_is_rolled_back_but_stays_failed",!error.empty()&&s.failure==F::requested_set&&s.set_requested.error==5&&s.rollback_restored&&s.rollback_available&&a.value==s.previous&&!body&&!s.verified());}
 {QosApiDouble a;a.fail_read=2;S s;const auto error=qos_error(a,s,[]{});
  q.check("applied_query_failure_has_verified_rollback_and_sticky_reason",!error.empty()&&s.failure==F::applied_query&&s.rollback_restored&&!s.applied_available&&!s.verified());}
 {QosApiDouble a;a.mismatch_read=2;S s;const auto error=qos_error(a,s,[]{});
  q.check("full_applied_readback_including_other_bit_must_match",!error.empty()&&s.failure==F::applied_mismatch&&s.applied_available&&s.rollback_restored&&!s.verified());}
 {QosApiDouble a;a.mismatch_read=2;a.fail_set=2;S s;const auto error=qos_error(a,s,[]{});
  q.check("constructor_rollback_set_error_records_bounded_fallback_not_PASS",!error.empty()&&s.failure==F::applied_mismatch&&s.rollback_failure==F::restore_set&&s.fallback_attempted&&s.fallback_restored&&a.sets==3&&a.value==s.previous&&!s.verified());}
 {QosApiDouble a;a.ignore_set=1;a.fail_read=3;S s;const auto error=qos_error(a,s,[]{});
  q.check("constructor_rollback_query_error_retained_after_fallback_restore",!error.empty()&&s.failure==F::applied_mismatch&&s.rollback_failure==F::restored_query&&s.query_rollback.error==87&&s.fallback_restored&&a.reads==4&&!s.verified());}
 {QosApiDouble a;a.ignore_set=1;a.mismatch_read=3;S s;const auto error=qos_error(a,s,[]{});
  q.check("constructor_rollback_fullstate_mismatch_never_becomes_verified",!error.empty()&&s.rollback_available&&!s.rollback_restored&&s.rollback_failure==F::restored_mismatch&&s.fallback_restored&&!s.verified());}
 {QosApiDouble a;S s;bool rejected=false;{ColdProcessQosScope scope(a,s);a.tid=8;try{scope.close();}catch(...){rejected=true;}const bool untouched=a.sets==1;a.tid=7;rejected&=untouched;}
  q.check("wrong_thread_close_never_mutates_foreign_thread_and_failure_sticky",rejected&&s.failure==F::wrong_thread&&s.fallback_restored&&a.value==s.previous&&!s.verified());}
 {QosApiDouble a;S s;bool rejected=false;{ColdProcessQosScope scope(a,s);a.pid=42;try{scope.close();}catch(...){rejected=true;}const bool untouched=a.sets==1;a.pid=41;rejected&=untouched;}
  q.check("wrong_process_close_never_mutates_foreign_process",rejected&&s.failure==F::wrong_process&&s.fallback_restored&&!s.verified());}
 {QosApiDouble a;S s;const auto error=qos_error(a,s,[&]{a.fail_set=2;throw std::runtime_error("original body exception");});
  q.check("body_exception_survives_restore_set_error_and_fallback_is_unqualified",error=="original body exception"&&s.failure==F::restore_set&&s.fallback_restored&&a.value==s.previous&&!s.verified());}
 {QosApiDouble a;S s;const auto error=qos_error(a,s,[&]{a.fail_read=3;});
  q.check("explicit_restore_query_failure_cannot_be_repaired_into_PASS",!error.empty()&&s.failure==F::restored_query&&!s.restored_available&&s.fallback_restored&&!s.verified());}
 {bool retained=true;for(const bool fallback_fail:{false,true}){QosApiDouble a;S s;const auto error=qos_error(a,s,[&]{a.mismatch_read=3;if(fallback_fail)a.fail_read=4;});
  retained&=!error.empty()&&s.failure==F::restored_mismatch&&s.restored_available&&!s.restored&&s.fallback_attempted&&!s.verified()&&
   (fallback_fail?s.fallback_failure==F::restored_query&&!s.fallback_available&&!s.fallback_restored:s.fallback_restored&&s.fallback_failure==F::none);}
  q.check("explicit_restore_fullstate_mismatch_and_failed_fallback_are_retained",retained);}
 {QosApiDouble a;S s;const auto error=qos_error(a,s,[&]{a.priority_class=0x80;});
  q.check("process_class_after_body_requires_NORMAL_even_after_exact_restore",!error.empty()&&s.failure==F::process_class_after_close&&s.process_class_after_close==0x80&&s.restored&&a.value==s.previous&&!s.verified());}
 J actual=nullptr;bool actual_windows=false;
#ifdef _WIN32
 {WindowsColdProcessQosApi a;S s;const auto error=qos_error(a,s,[]{});actual=cold_process_qos_json(s);actual_windows=true;
  q.check("actual_Windows_own_process_HighQoS_full_readback_and_exact_restore",error.empty()&&s.verified()&&s.process_id==GetCurrentProcessId()&&s.owner_thread_id==GetCurrentThreadId()&&a.process_class()==NORMAL_PRIORITY_CLASS);}
#else
 {QosApiDouble a;S s;bool rejected=false;{ColdProcessQosScope scope(a,s);a.fail_set=2;try{scope.close();}catch(...){rejected=true;}a.fail_read=4;}
  q.check("nonWindows_fallback_query_failure_remains_unknown_and_unqualified",rejected&&s.fallback_failure==F::restored_query&&!s.fallback_available&&!s.verified());}
#endif
 if(q.checks.size()!=24)throw std::runtime_error("fixed process QoS control denominator");
 return {{"schema","pas.cold-process-qos-controls.v1"},{"assertions",q.checks.size()},{"checks",q.checks},{"failed",q.failed},{"pass",q.failed==0},
  {"policy",cold_process_qos_policy},{"version",cold_process_qos_version},{"execution_speed_mask",cold_process_qos_execution_speed_mask},
  {"supported_mask",cold_process_qos_supported_mask},{"synthetic_api_controls",true},{"actual_windows_restore_control",actual_windows},
  {"actual_windows_snapshot",actual},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh test output");Tests t;FakeClock c;int calls=0;Nanoseconds ticks=0;
 priority_tests(t);auto wait=[&](Nanoseconds n){++calls;ticks=n;c.set(c.now_ns()+100*n);};
 c.set(1000);wait_producer_deadline(c,1000,wait);t.check("due_now_does_not_wait",calls==0);
 wait_producer_deadline(c,999,wait);t.check("already_late_not_reset_or_slept",calls==0&&c.now_ns()==1000);
 wait_producer_deadline(c,1101,wait);t.check("ceil100ns_never_early",calls==1&&ticks==2&&c.now_ns()==1200);
 c.set(0);calls=0;for(int i=0;i<4;++i)wait_producer_deadline(c,20'000'000*i,wait);t.check("unchanged_absolute_epoch_and_20ms_cadence",calls==3&&c.now_ns()==60'000'000);
 c.set(43'000'000);calls=0;wait_producer_deadline(c,40'000'000,wait);wait_producer_deadline(c,60'000'000,wait);t.check("late_attempt_kept_next_due_not_shifted",calls==1&&c.now_ns()==60'000'000);
 c.set(0);calls=0;wait_producer_deadline(c,1000,[&](Nanoseconds){++calls;c.set(calls==1?900:1000);});t.check("one_early_wake_rechecks_QPC",calls==2&&c.now_ns()==1000);
 c.set(0);calls=0;bool rejected=false;try{wait_producer_deadline(c,1000,[&](Nanoseconds){++calls;});}catch(...){rejected=true;}t.check("repeated_early_wake_bounded_four",rejected&&calls==4);
 calls=0;rejected=false;try{wait_producer_deadline(c,100'000'001,wait);}catch(...){rejected=true;}t.check("distant_wait_rejected_before_kernel",rejected&&calls==0);
 c.set(-1);rejected=false;try{wait_producer_deadline(c,0,wait);}catch(...){rejected=true;}t.check("negative_clock_domain_rejected",rejected);
 c.set(0);rejected=false;try{wait_producer_deadline(c,1000,[](Nanoseconds){throw std::runtime_error("kernel failure");});}catch(...){rejected=true;}t.check("timer_failure_no_fallback",rejected);
#ifdef _WIN32
 {WindowsProducerPriorityApi api;const auto before=api.current();{ProducerPriorityScope scope(api);t.check("actual_Windows_producer_highest_priority_readback",api.current()==THREAD_PRIORITY_HIGHEST);scope.close();}t.check("actual_Windows_priority_restored",api.current()==before);}
 {HostClock real;ProducerDeadlineWait timer;const auto due=real.now_ns()+20'000'000;timer.wait_until(real,due);t.check("real_Windows_timer_not_before_QPC_due",real.now_ns()>=due);}
#endif
 active_tail_tests(t);
 const auto diagnostic_controls=diagnostic_trace_tests();const auto active_wait_controls=active_wait_tests();const auto process_qos_controls=process_qos_tests();const bool pass=t.failed==0&&diagnostic_controls.at("pass").get<bool>()&&active_wait_controls.at("pass").get<bool>()&&process_qos_controls.at("pass").get<bool>();
 J r={{"schema","pas.producer-deadline-wait-tests.v1"},{"assertions",t.checks.size()},{"checks",t.checks},{"failed",t.failed},{"pass",pass},{"diagnostic_controls",diagnostic_controls},{"active_wait_controls",active_wait_controls},{"process_qos_controls",process_qos_controls},{"device_endpoints",0},{"device_operations",0},{"cost_qualification",false},{"live_ready",false},{"active_tail_ns",producer_active_tail_ns},{"active_tail_poll_cap",producer_active_tail_poll_cap},{"qualification_scope","functional timer/fake-clock contract only; active-tail CPU and actual cadence cost separately gated"}};std::ofstream f(argv[1]);f<<r.dump(2)<<'\n';if(!f)throw std::runtime_error("write");std::cout<<"assertions="<<t.checks.size()<<" failed="<<t.failed<<" diagnostic_assertions="<<diagnostic_controls.at("assertions")<<" diagnostic_failed="<<diagnostic_controls.at("failed")<<" active_wait_assertions="<<active_wait_controls.at("assertions")<<" active_wait_failed="<<active_wait_controls.at("failed")<<" process_qos_assertions="<<process_qos_controls.at("assertions")<<" process_qos_failed="<<process_qos_controls.at("failed")<<'\n';return pass?0:1;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
