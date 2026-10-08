#include "producer_wait.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace pas;using namespace pas::current_rails;using J=nlohmann::json;
struct Tests{J checks=J::array();int failed=0;void check(std::string name,bool pass){checks.push_back({{"id",name},{"pass",pass}});failed+=!pass;}};
struct PriorityApi {int value=-1,calls=0,fail_at=0;bool ignore=false;std::uint32_t id=1;int current()const{return value;}std::uint32_t thread()const{return id;}bool set(int p){if(++calls==fail_at)return false;if(!ignore)value=p;return true;}};
void priority_tests(Tests&t){
 PriorityApi api;{ProducerPriorityScope scope(api);t.check("producer_only_above_normal",api.value==1);scope.close();t.check("explicit_restore_prior_priority",api.value==-1&&api.calls==2);scope.close();t.check("close_idempotent",api.calls==2);}
 {ProducerPriorityScope scope(api);}t.check("exception_scope_restores_priority",api.value==-1&&api.calls==4);
 api.value=std::numeric_limits<int>::max();bool rejected=false;try{ProducerPriorityScope scope(api);}catch(...){rejected=true;}t.check("query_failure_before_priority_mutation",rejected&&api.calls==4);
 api.value=0;api.fail_at=5;rejected=false;try{ProducerPriorityScope scope(api);}catch(...){rejected=true;}t.check("set_failure_no_silent_fallback",rejected&&api.value==0&&api.calls==5);
 api.fail_at=0;api.ignore=true;rejected=false;try{ProducerPriorityScope scope(api);}catch(...){rejected=true;}t.check("set_success_readback_mismatch_rejected",rejected&&api.value==0);
 api.ignore=false;{ProducerPriorityScope scope(api);api.fail_at=api.calls+1;rejected=false;try{scope.close();}catch(...){rejected=true;}t.check("restore_failure_disqualifies",rejected);api.fail_at=0;}t.check("failed_restore_unwind_cleanup",api.value==0);
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
 {WindowsProducerPriorityApi api;const auto before=api.current();{ProducerPriorityScope scope(api);t.check("actual_Windows_producer_priority_readback",api.current()==THREAD_PRIORITY_ABOVE_NORMAL);scope.close();}t.check("actual_Windows_priority_restored",api.current()==before);}
 {HostClock real;ProducerDeadlineWait timer;const auto due=real.now_ns()+2'000'000;timer.wait_until(real,due);t.check("real_Windows_timer_not_before_QPC_due",real.now_ns()>=due);}
#endif
 J r={{"schema","pas.producer-deadline-wait-tests.v1"},{"assertions",t.checks.size()},{"checks",t.checks},{"failed",t.failed},{"device_endpoints",0},{"qualification_scope","functional timer/fake-clock contract only; actual cadence cost separately gated"}};std::ofstream f(argv[1]);f<<r.dump(2)<<'\n';if(!f)throw std::runtime_error("write");std::cout<<"assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
