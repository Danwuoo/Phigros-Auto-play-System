#include "poll_gap_diagnostic.hpp"
#include "producer_wait.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#if defined(PAS_WAIT_DIAGNOSTIC_COMPONENT) && PAS_WAIT_DIAGNOSTIC_COMPONENT
#include "wait_diagnostic_component_identity.hpp"
#endif

using namespace pas;
using namespace pas::current_rails;
using J=nlohmann::json;
struct Tests {
 J checks=J::array();int failed=0;
 void check(const char* id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;if(!pass)std::cerr<<"poll gap control failed: "<<id<<'\n';}
};
struct TestClock final:Clock {
 mutable std::uint32_t reads=0;
 Nanoseconds value=0;std::uint32_t fail_read=0;
 Nanoseconds now_ns()const override{if(++reads==fail_read)throw std::runtime_error("original clock failure");return value;}
};
template<class Body> bool rejects(Body&& body,const char* message){
 try{body();}catch(const std::runtime_error& e){return std::string(e.what())==message;}catch(...){return false;}return false;
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("usage producer_poll_gap_controls fresh-report.json");
 const auto path=std::filesystem::absolute(argv[1]);if(std::filesystem::exists(path))throw std::runtime_error("fresh poll gap controls report");
 Tests t;
 {TestClock c;c.value=123;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);
  t.check("constructor_has_no_clock_reads_or_available_gap",c.reads==0&&s.sample_count==0&&s.interval_count==0&&!s.available&&!s.invalid);}
 {TestClock c;c.value=100;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);ProducerWaitTrace trace;int pauses=0;
  wait_producer_deadline_active(w,100,[&]{++pauses;},&trace);
  t.check("due_now_one_original_read_no_invented_gap",c.reads==1&&pauses==0&&trace.completed&&trace.qpc_reads==1&&s.sample_count==1&&s.interval_count==0&&!s.available&&!s.invalid);}
 {TestClock c;c.value=100;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);const auto a=w.now_ns(),b=w.now_ns();
  t.check("equal_reads_have_zero_interval_without_positive_gap",a==100&&b==100&&c.reads==2&&s.sample_count==2&&s.interval_count==1&&s.max_gap_ns==0&&!s.available&&!s.invalid);}
 {TestClock c;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);w.now_ns();c.value=3;w.now_ns();c.value=10;w.now_ns();c.value=12;w.now_ns();
  t.check("maximum_keeps_actual_endpoints_and_one_based_end_index",c.reads==4&&s.sample_count==4&&s.interval_count==3&&s.max_gap_ns==7&&s.gap_begin_ns==3&&s.gap_end_ns==10&&s.end_sample_index==3&&s.available&&!s.invalid);}
 {TestClock c;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);w.now_ns();c.value=5;w.now_ns();c.value=10;w.now_ns();
  t.check("equal_maximum_keeps_first_interval",s.max_gap_ns==5&&s.gap_begin_ns==0&&s.gap_end_ns==5&&s.end_sample_index==2&&s.sample_count==3&&!s.invalid);}
 {TestClock jumped,uniform;cold::poll_gap::Snapshot a,b;cold::poll_gap::Clock wa(jumped,a),wb(uniform,b);
  constexpr std::array<Nanoseconds,5> jump{0,1'000'000,150'000'000,151'000'000,152'000'000};
  constexpr std::array<Nanoseconds,5> even{0,38'000'000,76'000'000,114'000'000,152'000'000};
  for(std::size_t i=0;i<jump.size();++i){jumped.value=jump[i];uniform.value=even[i];wa.now_ns();wb.now_ns();}
  t.check("149ms_gap_is_distinct_from_same_elapsed_uniform_reads",jumped.reads==uniform.reads&&a.sample_count==b.sample_count&&a.interval_count==b.interval_count&&a.max_gap_ns==149'000'000&&a.gap_begin_ns==1'000'000&&a.gap_end_ns==150'000'000&&b.max_gap_ns==38'000'000&&a.available&&b.available&&!a.invalid&&!b.invalid);}
 {TestClock plain,wrapped;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(wrapped,s);ProducerWaitTrace p,q;int pp=0,wp=0;
  wait_producer_deadline_active(plain,20'000'000,[&]{++pp;plain.value+=1'000'000;},&p);
  wait_producer_deadline_active(w,20'000'000,[&]{++wp;wrapped.value+=1'000'000;},&q);
  t.check("wrapper_preserves_wait_due_pause_order_and_original_read_count",pp==20&&wp==pp&&wrapped.value==plain.value&&plain.reads==wrapped.reads&&wrapped.reads==21&&p.due_ns==q.due_ns&&p.last_qpc_ns==q.last_qpc_ns&&p.spin_poll_count==q.spin_poll_count&&q.completed&&s.sample_count==q.qpc_reads&&s.max_gap_ns==1'000'000);}
 {TestClock c;cold::poll_gap::Snapshot first,next;ProducerWaitTrace a,b;int pauses=0;
  {cold::poll_gap::Clock w(c,first);wait_producer_deadline_active(w,20'000'000,[&]{++pauses;c.value=169'000'000;},&a);}
  {cold::poll_gap::Clock w(c,next);wait_producer_deadline_active(w,40'000'000,[&]{++pauses;},&b);}
  t.check("terminal_overshoot_keeps_next_absolute_due_and_late_gap_unknown",pauses==1&&a.due_ns==20'000'000&&a.last_qpc_ns==169'000'000&&first.max_gap_ns==169'000'000&&first.end_sample_index==2&&b.due_ns==40'000'000&&b.first_qpc_ns==169'000'000&&b.spin_poll_count==0&&next.sample_count==1&&!next.available&&!next.invalid);}
 {TestClock c;c.value=-1;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);ProducerWaitTrace trace;
  const bool bad=rejects([&]{wait_producer_deadline_active(w,0,[]{},&trace);},"producer QPC domain");
  t.check("negative_first_read_retains_original_wait_error",bad&&c.reads==1&&s.sample_count==1&&s.invalid&&!s.available&&trace.first_qpc_ns==-1&&trace.failure==ProducerWaitFailure::clock_domain);}
 {TestClock c;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);ProducerWaitTrace trace;
  const bool bad=rejects([&]{wait_producer_deadline_active(w,1,[&]{c.value=-1;},&trace);},"producer QPC domain");
  t.check("negative_later_read_has_no_unsigned_gap",bad&&c.reads==2&&s.sample_count==2&&s.interval_count==0&&s.invalid&&!s.available&&s.max_gap_ns==0&&trace.failure==ProducerWaitFailure::clock_domain);}
 {TestClock c;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);ProducerWaitTrace trace;int pauses=0;
  const bool bad=rejects([&]{wait_producer_deadline_active(w,100,[&]{c.value=++pauses==1?10:9;},&trace);},"producer nonmonotonic QPC");
  t.check("backward_read_keeps_prior_valid_max_and_original_error",bad&&pauses==2&&c.reads==3&&s.sample_count==3&&s.interval_count==1&&s.invalid&&s.available&&s.max_gap_ns==10&&s.gap_begin_ns==0&&s.gap_end_ns==10&&trace.last_qpc_ns==9&&trace.failure==ProducerWaitFailure::nonmonotonic_clock);}
 {TestClock first,later;first.fail_read=1;later.fail_read=3;cold::poll_gap::Snapshot a,b;cold::poll_gap::Clock wa(first,a),wb(later,b);ProducerWaitTrace p,q;
  const bool one=rejects([&]{wait_producer_deadline_active(wa,10,[]{},&p);},"original clock failure");
  const bool partial=rejects([&]{wait_producer_deadline_active(wb,100,[&]{later.value+=10;},&q);},"original clock failure");
  t.check("original_clock_exceptions_preserve_unknown_and_partial_facts",one&&partial&&first.reads==1&&a.sample_count==0&&a.invalid&&!a.available&&later.reads==3&&b.sample_count==2&&b.interval_count==1&&b.max_gap_ns==10&&b.available&&b.invalid&&p.failure==ProducerWaitFailure::clock_read&&q.failure==ProducerWaitFailure::clock_read);}
 {TestClock c;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);ProducerWaitTrace trace;
  const bool bad=rejects([&]{wait_producer_deadline_active(w,10,[]{throw std::runtime_error("original pause failure");},&trace);},"original pause failure");
  t.check("pause_exception_never_adds_a_clock_read_or_gap",bad&&c.reads==1&&s.sample_count==1&&!s.available&&!s.invalid&&trace.spin_pause_calls==1&&trace.spin_poll_count==0&&trace.failure==ProducerWaitFailure::pause_callback);}
 {TestClock c;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);ProducerWaitTrace trace;int pauses=0;
  const bool bad=rejects([&]{wait_producer_deadline_active(w,100'000'001,[&]{++pauses;},&trace);},"producer wait horizon");
  t.check("original_horizon_rejection_does_not_invent_an_interval",bad&&pauses==0&&c.reads==1&&s.sample_count==1&&s.interval_count==0&&!s.available&&!s.invalid&&trace.failure==ProducerWaitFailure::horizon);}
 {TestClock c;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);w.now_ns();
  // Seed only fixed counters near their declared boundary; no 2^24-loop test.
  s.sample_count=cold::poll_gap::sample_cap-1;s.interval_count=cold::poll_gap::interval_cap-1;c.value=1;const auto at=w.now_ns();c.value=2;const auto past=w.now_ns();
  t.check("fixed_counters_saturate_without_changing_clock_or_wait_policy",at==1&&past==2&&c.reads==3&&s.sample_count==cold::poll_gap::sample_cap&&s.interval_count==cold::poll_gap::interval_cap&&s.max_gap_ns==1&&s.available&&s.invalid&&s.end_sample_index==cold::poll_gap::sample_cap);}
 {TestClock c;cold::poll_gap::Snapshot s;cold::poll_gap::Clock w(c,s);w.now_ns();c.value=std::numeric_limits<Nanoseconds>::max();const auto end=w.now_ns();
  t.check("ordered_nonnegative_int64_endpoints_do_not_overflow",end==std::numeric_limits<Nanoseconds>::max()&&s.max_gap_ns==end&&s.gap_begin_ns==0&&s.gap_end_ns==end&&s.sample_count==2&&s.interval_count==1&&s.available&&!s.invalid);}
 if(t.checks.size()!=16)throw std::runtime_error("fixed poll gap control denominator");
 J report={{"schema","pas.producer-poll-gap-controls.v1"},{"assertions",t.checks.size()},{"checks",t.checks},{"failed",t.failed},{"pass",t.failed==0},
  {"synthetic_clock_controls",true},{"sample_cap",cold::poll_gap::sample_cap},{"interval_cap",cold::poll_gap::interval_cap},{"extra_clock_reads",0},
  {"instrumentation_overhead_measured",false},{"diagnostic_only",true},{"device_endpoints",0},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
#if defined(PAS_WAIT_DIAGNOSTIC_COMPONENT) && PAS_WAIT_DIAGNOSTIC_COMPONENT
 report["diagnostic_component_identity"]={{"schema","pas.cold-wait-diagnostic-component-identity.v1"},
  {"source_commit",pas_wait_diagnostic_source_commit},{"source_dirty",pas_wait_diagnostic_source_dirty},
  {"base_source_commit",pas_wait_diagnostic_base_source_commit},{"main_sha256",pas_wait_diagnostic_main_sha256},
  {"poll_gap_header_sha256",pas_wait_diagnostic_header_sha256},{"diagnostic_only",true},{"qualification_issued",false}};
#endif
 std::ofstream f(path,std::ios::binary);f<<report.dump(2)<<'\n';f.flush();if(!f)throw std::runtime_error("poll gap report write");
 return t.failed?2:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
