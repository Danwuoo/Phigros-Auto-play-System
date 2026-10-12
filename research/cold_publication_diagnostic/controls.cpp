// Link the diagnostic core object. Original wait controls remain separate.
#include "publication_diagnostic_output.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <limits>
#include <mutex>
#include <thread>

#ifndef PAS_PUBLICATION_DIAGNOSTIC_SOURCE_COMMIT
#error "Explicit clean diagnostic compiled source identity required"
#endif
#ifndef PAS_PUBLICATION_DIAGNOSTIC_SOURCE_DIRTY
#error "Explicit diagnostic compiled dirty state required"
#endif
namespace {
namespace pd=cold::publication_diag;
namespace pg=cold::poll_gap;
namespace rails=pas::current_rails;
namespace fs=std::filesystem;
using J=nlohmann::json;
using Ns=pas::Nanoseconds;
constexpr std::size_t case_count=16;
std::size_t expectations=0;
void need(bool ok,const char* message){++expectations;if(!ok)throw std::runtime_error(message);}
template<class Error,class Body> void throws(Body&& body,const char* text){
 bool caught=false;try{body();}catch(const Error& e){caught=true;if(text)need(std::string(e.what()).find(text)!=std::string::npos,"original error text");}
 need(caught,"expected original exception type");
}
struct FixedClock final:pas::Clock {
 Ns value;mutable std::uint32_t reads=0;
 mutable std::function<void(std::uint32_t)> on_read;
 explicit FixedClock(Ns v=100):value(v){}
 Ns now_ns()const override{const auto number=++reads;if(on_read)on_read(number);return value;}
};
struct ScriptClock final:pas::Clock {
 std::vector<Ns> values;mutable std::size_t reads=0;
 std::size_t throw_on=std::numeric_limits<std::size_t>::max();
 explicit ScriptClock(std::initializer_list<Ns> v):values(v){}
 Ns now_ns()const override{
  const auto i=reads++;if(i==throw_on)throw std::logic_error("original_script_clock_fault");
  if(i>=values.size())throw std::logic_error("script_exhausted");return values[i];
 }
};
struct ThrowClock final:pas::Clock {
 mutable unsigned reads=0;
 Ns now_ns()const override{++reads;throw std::logic_error("diagnostic_clock_fault");}
};
bool point(const pd::Attempt& a,pd::Point p){return (a.present_mask&(std::uint64_t(1)<<static_cast<unsigned>(p)))!=0;}
pas::Frame metadata(std::uint64_t sequence){
 pas::Frame m;m.sequence=sequence;m.epoch=7;m.generation=8;m.geometry_version=9;
 m.width=2;m.height=2;m.stride=6;m.source_rotation=3;m.normalization_rotation_degrees=180;
 m.capture_backend="offline_memory_stub";m.source_pixel_format="RGB24";m.source_stride=19;
 m.crop_x=1;m.crop_y=2;m.crop_width=3;m.crop_height=4;m.source_valid=false;
 m.capture_complete_ns=101;m.pixels_ready_ns=102;m.published_ns=777;
 m.receive_start_ns=103;m.parse_start_ns=104;m.parse_end_ns=105;m.notification_received_ns=106;
 m.copy_start_ns=107;m.copy_end_ns=108;m.readback_start_ns=109;m.readback_end_ns=110;
 m.decode_start_ns=111;m.decode_end_ns=112;m.source_qpc_ticks=113;m.source_system_relative_100ns=114;
 m.codec_pts=-115;m.source_timestamp_us=116;m.source_sequence=117;return m;
}
bool same_metadata(const pas::Frame& a,const pas::Frame& b,Ns expected_published){
 return a.sequence==b.sequence&&a.epoch==b.epoch&&a.generation==b.generation&&a.geometry_version==b.geometry_version&&
  a.width==b.width&&a.height==b.height&&a.stride==b.stride&&a.source_rotation==b.source_rotation&&
  a.normalization_rotation_degrees==b.normalization_rotation_degrees&&a.capture_backend==b.capture_backend&&
  a.source_pixel_format==b.source_pixel_format&&a.source_stride==b.source_stride&&a.crop_x==b.crop_x&&a.crop_y==b.crop_y&&
  a.crop_width==b.crop_width&&a.crop_height==b.crop_height&&a.source_valid==b.source_valid&&
  a.capture_complete_ns==b.capture_complete_ns&&a.pixels_ready_ns==b.pixels_ready_ns&&a.published_ns==expected_published&&
  a.receive_start_ns==b.receive_start_ns&&a.parse_start_ns==b.parse_start_ns&&a.parse_end_ns==b.parse_end_ns&&
  a.notification_received_ns==b.notification_received_ns&&a.copy_start_ns==b.copy_start_ns&&a.copy_end_ns==b.copy_end_ns&&
  a.readback_start_ns==b.readback_start_ns&&a.readback_end_ns==b.readback_end_ns&&a.decode_start_ns==b.decode_start_ns&&
  a.decode_end_ns==b.decode_end_ns&&a.source_qpc_ticks==b.source_qpc_ticks&&a.source_system_relative_100ns==b.source_system_relative_100ns&&
  a.codec_pts==b.codec_pts&&a.source_timestamp_us==b.source_timestamp_us&&a.source_sequence==b.source_sequence;
}
const std::array<std::uint8_t,12> pixels{0,1,2,3,4,5,6,7,8,9,10,255};
J read_json(const fs::path& path){need(fs::is_regular_file(path)&&fs::file_size(path)>0&&fs::file_size(path)<=pd::file_bytes,"nonempty bounded metadata file");std::ifstream in(path);need(bool(in),"metadata open");return J::parse(in);}
struct Context {fs::path root;};

void inactive(Context&){
 FixedClock clock;pd::Attempt a;auto* old=pd::active_attempt;auto* old_clock=pd::active_clock;
 need(old==nullptr&&old_clock==nullptr,"initial inactive producer TLS");
 pd::stamp(pd::Point::copy_enter);pd::record(pd::Point::render_start,42);
 need(clock.reads==0&&a.present_mask==0&&a.read_failure_mask==0,"inactive markers have no Clock reads or row mutation");
 need(pd::active_attempt==old&&pd::active_clock==old_clock,"inactive TLS unchanged");
}
void alias(Context&){
 FixedClock clock(1234);pd::Attempt a;
 {pd::AttemptBinding binding(a,clock,1,2000);const auto before=clock.reads;pd::record(pd::Point::render_start,1234);
  need(clock.reads==before&&a.stamps[static_cast<unsigned>(pd::Point::render_start)]==1234,"existing timestamp exact, zero extra read");
  pd::stamp(pd::Point::copy_enter);need(clock.reads==before+1,"one additional read per enabled point");binding.complete();}
 need(a.completed&&!a.invalid_clock&&point(a,pd::Point::iteration_return),"complete enabled marker snapshot");
 need(pd::active_attempt==nullptr&&pd::active_clock==nullptr,"binding restored inactive TLS");
}
void bounds(Context&){
 need(std::is_trivially_copyable_v<pd::Attempt>&&sizeof(pd::Attempt)<=pd::slot_byte_bound,"fixed POD slot bound");
 need(pd::point_count==36&&pd::segment_rows==2048&&pd::max_segments==5&&pd::row_bytes==1536&&pd::file_bytes==4194304,"original diagnostic output bounds");
 need(((10000+pd::slot_extra)+pd::segment_rows-1)/pd::segment_rows==pd::max_segments,"all maximum requested slots fit without truncation");
 pd::Attempt a;a.attempt=10000;a.due_ns=std::numeric_limits<Ns>::max();a.entered=a.completed=a.published_ok=true;
 a.present_mask=(std::uint64_t(1)<<pd::point_count)-1;a.read_failure_mask=std::numeric_limits<std::uint64_t>::max();
 a.stamps.fill(std::numeric_limits<Ns>::min());a.invalid_clock=a.exception_seen=true;
 a.wait.first_qpc_ns=a.wait.last_qpc_ns=a.wait.spin_enter_ns=a.wait.spin_return_ns=std::numeric_limits<Ns>::max();
 a.wait.qpc_reads=rails::producer_active_wait_poll_cap+1;a.wait.spin_poll_count=a.wait.spin_pause_calls=rails::producer_active_wait_poll_cap;
 a.wait.phase=a.wait.failure_phase=rails::ProducerWaitPhase::complete;a.wait.failure=rails::ProducerWaitFailure::pause_callback;
 a.wait.entered=a.wait.completed=a.wait.spin_entered=a.wait.spin_completed=true;
 a.gap.max_gap_ns=a.gap.gap_begin_ns=a.gap.gap_end_ns=std::numeric_limits<Ns>::max();
 a.gap.sample_count=pg::sample_cap;a.gap.interval_count=pg::interval_cap;a.gap.end_sample_index=pg::sample_cap;a.gap.available=a.gap.invalid=true;
 for(auto* s:{&a.frame_backend,&a.slot_backend,&a.slot_format}){s->before_size=s->before_capacity=s->after_size=s->after_capacity=std::numeric_limits<std::uint64_t>::max();s->before_available=s->assignment_returned=s->data_pointer_changed=true;}
 const auto row=pd::row_json(a);need(row.is_array()&&row.size()==17&&row.dump().size()<=pd::row_bytes,"worst fixed scalar serialization remains within combined row bound");
 need(pd::segment_rows*pd::row_bytes+4096<pd::file_bytes,"maximum segment numeric payload bound");
}
void extra_clock_fault(Context&){
 ThrowClock clock;pd::Attempt a;pas::LatestFrame frames(2,2);auto m=metadata(1);
 {pd::AttemptBinding binding(a,clock,1,20);need(frames.publish(pixels.data(),pixels.size(),m),"diagnostic fault must not change successful business publish");binding.complete();}
 need(a.invalid_clock&&a.read_failure_mask!=0&&a.present_mask==0,"failed diagnostic reads never fabricate timestamps");
 auto lease=frames.read_after(0,0);need(lease&&same_metadata(*lease,m,m.published_ns),"business metadata survives diagnostic fault");
 pd::Attempt b;throws<std::invalid_argument>([&]{pd::AttemptBinding binding(b,clock,2,20);frames.publish(pixels.data(),pixels.size()-1,m);},"invalid RGB frame");
 need(b.exception_seen&&b.invalid_clock&&!b.completed,"business exception retained with partial diagnostic facts");
 need(pd::active_attempt==nullptr&&pd::active_clock==nullptr,"clock-error unwind restores TLS");
}
void invalid_stamp(Context&){
 FixedClock clock(100);pd::Attempt a;
 {pd::AttemptBinding binding(a,clock,1,200);pd::record(pd::Point::render_start,-1);need(a.invalid_clock,"negative stamp invalid");
  pd::record(pd::Point::render_end,200);pd::record(pd::Point::publication_return,199);need(a.invalid_clock,"backward stamp remains invalid");}
 need(point(a,pd::Point::render_start)&&a.stamps[static_cast<unsigned>(pd::Point::render_start)]==-1,"invalid actual value remains partial evidence");
}
void wait_forwarding(Context&){
 ScriptClock direct{0,5'000'000,20'000'000},source{0,5'000'000,20'000'000};unsigned pauses_direct=0,pauses_wrapped=0;pg::Snapshot gap;pg::Clock wrapped(source,gap);rails::ProducerWaitTrace trace;
 rails::wait_producer_deadline_active(direct,20'000'000,[&]{++pauses_direct;});
 rails::wait_producer_deadline_active(wrapped,20'000'000,[&]{++pauses_wrapped;},&trace);
 need(direct.reads==source.reads&&source.reads==3&&pauses_direct==pauses_wrapped&&pauses_wrapped==2,"original reads forwarded once and pause order exact");
 need(trace.due_ns==20'000'000&&trace.last_qpc_ns==20'000'000&&trace.completed&&trace.kernel_calls==0,"same due/full-active/no-kernel policy");
 ScriptClock over{0,70'000'000};pg::Snapshot over_gap;pg::Clock over_clock(over,over_gap);rails::ProducerWaitTrace late;
 rails::wait_producer_deadline_active(over_clock,20'000'000,[]{},&late);
 need(late.due_ns==20'000'000&&late.last_qpc_ns==70'000'000,"lateness preserved without resetting due");
 ScriptClock next{70'000'000};rails::ProducerWaitTrace next_trace;rails::wait_producer_deadline_active(next,40'000'000,[]{},&next_trace);
 need(next_trace.due_ns==40'000'000&&next_trace.qpc_reads==1,"next absolute due preserved without skip");
 for(auto domain:{-1,0}){ScriptClock s{domain};pg::Snapshot g;pg::Clock c(s,g);throws<std::runtime_error>([&]{rails::wait_producer_deadline_active(c,domain<0?20:100'000'001,[]{});},domain<0?"domain":"horizon");need(s.reads==1,"failure has one original read");}
 ScriptClock backward{100,150,149};pg::Snapshot bg;pg::Clock bc(backward,bg);throws<std::runtime_error>([&]{rails::wait_producer_deadline_active(bc,200,[]{});},"nonmonotonic");need(bg.invalid,"wait backward reading is invalid");
 ScriptClock fault{0,1};fault.throw_on=1;pg::Snapshot fg;pg::Clock fc(fault,fg);throws<std::logic_error>([&]{rails::wait_producer_deadline_active(fc,20,[]{});},"original_script_clock_fault");need(fault.reads==2&&fg.invalid,"original QPC exception unchanged");
 ScriptClock pause{0};pg::Snapshot pgap;pg::Clock pc(pause,pgap);throws<std::logic_error>([&]{rails::wait_producer_deadline_active(pc,20,[]{throw std::logic_error("original_pause_fault");});},"original_pause_fault");need(pause.reads==1,"pause exception adds no Clock read");
 // Seed diagnostic counters at their boundary; no 2^24 busy loop is needed
 // to retest the unchanged shared wait policy's existing active12 controls.
 ScriptClock cap{0,1,2,3};pg::Snapshot cg;pg::Clock cc(cap,cg);need(cc.now_ns()==0,"initial cap fixture read");cg.interval_count=pg::interval_cap;
 need(cc.now_ns()==1&&cg.invalid&&cg.interval_count==pg::interval_cap,"interval cap fails diagnostic closed while forwarding original value");
 const auto retained=cg.max_gap_ns;need(cc.now_ns()==2&&cg.invalid&&cg.max_gap_ns==retained,"cap invalidity sticky");cg.sample_count=pg::sample_cap;
 need(cc.now_ns()==3&&cg.sample_count==pg::sample_cap&&cg.invalid&&cap.reads==4,"sample cap remains bounded with exact forwarding");
}
void gap_partition(Context&){
 ScriptClock source{0,149'000'000,150'000'000};pg::Snapshot single;pg::Clock c(source,single);for(int i=0;i<3;++i)c.now_ns();
 ScriptClock uniform{0,50'000'000,100'000'000,150'000'000};pg::Snapshot spread;pg::Clock u(uniform,spread);for(int i=0;i<4;++i)u.now_ns();
 need(single.available&&!single.invalid&&single.max_gap_ns==149'000'000&&single.gap_begin_ns==0&&single.gap_end_ns==149'000'000&&single.end_sample_index==2,"single149ms adjacent gap located");
 need(spread.available&&!spread.invalid&&spread.max_gap_ns==50'000'000&&spread.end_sample_index==2&&spread.gap_begin_ns==0,"uniform reads differ and first maximum tie retained");
 need(source.reads==3&&uniform.reads==4,"no extra source reads in gap diagnostic");
}
void no_gap(Context&){
 ScriptClock late{30};pg::Snapshot gap;pg::Clock c(late,gap);rails::ProducerWaitTrace trace;rails::wait_producer_deadline_active(c,20,[]{},&trace);
 need(trace.completed&&trace.spin_poll_count==0&&late.reads==1&&!gap.available&&!gap.invalid&&gap.interval_count==0,"one already-late read cannot claim a gap");
 ScriptClock equal{7,7,7};pg::Snapshot e;pg::Clock ec(equal,e);for(int i=0;i<3;++i)ec.now_ns();need(!e.available&&!e.invalid&&e.max_gap_ns==0&&e.interval_count==2,"equal-only reads have no positive gap");
}
void complete_publish(Context&){
 FixedClock clock(123456789);pas::LatestFrame frames(2,2,2,&clock);auto m=metadata(1);m.rgb.assign(12,17);pd::Attempt a;
 {pd::AttemptBinding binding(a,clock,1,100);a.published_ok=frames.publish(pixels.data(),pixels.size(),m);binding.complete();}
 auto lease=frames.read_after(0,0);need(a.published_ok&&lease&&same_metadata(*lease,m,clock.value),"all public metadata including optionals exact");
 need(std::equal(lease->rgb.begin(),lease->rgb.end(),pixels.begin(),pixels.end()),"every RGB byte preserved");
 for(auto p:{pd::Point::publish_enter,pd::Point::lock1_request,pd::Point::lock1_acquired,pd::Point::lock1_released,pd::Point::copy_enter,pd::Point::copy_return,
  pd::Point::target_backend_enter,pd::Point::target_backend_return,pd::Point::target_format_enter,pd::Point::target_format_return,pd::Point::lock2_request,
  pd::Point::lock2_acquired,pd::Point::published_clock_return,pd::Point::lock2_body_return,pd::Point::lock2_released,pd::Point::notify_enter,pd::Point::notify_return,pd::Point::publish_exit})need(point(a,p),"complete publish stage present");
 need(a.publish_outcome==pd::PublishOutcome::returned_true&&!a.exception_seen&&!a.invalid_clock&&frames.counters().published==1,"success/counters exact");
 need(a.slot_backend.assignment_returned&&a.slot_backend.after_size==19&&a.slot_format.after_size==5,"actual assignment facts recorded");
}
void closed_publish(Context&){
 FixedClock clock;pas::LatestFrame frames(2,2);frames.close();pd::Attempt a;auto m=metadata(1);
 {pd::AttemptBinding binding(a,clock,1,20);need(!frames.publish(pixels.data(),pixels.size(),m),"closed original false return");binding.complete();}
 need(a.publish_outcome==pd::PublishOutcome::closed&&point(a,pd::Point::lock1_released)&&point(a,pd::Point::publish_exit),"closed path unlock/exit present");
 need(!point(a,pd::Point::copy_enter)&&!point(a,pd::Point::lock2_request)&&!point(a,pd::Point::notify_enter),"closed path does not fabricate later stages");
 need(frames.counters().published==0&&!frames.peek(),"closed state unchanged");
 // Close during the unlocked metadata stage to cover the second-lock early
 // return too, with no thread scheduling or timer assumption.
 FixedClock later_clock;pas::LatestFrame later(2,2);pd::Attempt late;bool closed_between_locks=false;
 later_clock.on_read=[&](std::uint32_t){if(point(late,pd::Point::copy_return)&&!point(late,pd::Point::lock2_request)&&!closed_between_locks){later.close();closed_between_locks=true;}};
 {pd::AttemptBinding binding(late,later_clock,1,20);need(!later.publish(pixels.data(),pixels.size(),m),"closed after copy original false return");binding.complete();}
 need(closed_between_locks&&late.publish_outcome==pd::PublishOutcome::closed&&point(late,pd::Point::lock2_released)&&!point(late,pd::Point::notify_enter),"second-lock close releases without publish notify");
 need(later.counters().published==0&&!later.peek(),"second-lock close preserves closed state");
}
void leases_reuse(Context&){
 FixedClock clock;pas::LatestFrame frames(2,2,2);need(frames.publish(pixels.data(),pixels.size(),metadata(1)),"lease first publish");auto first=frames.read_after(0,0);
 std::array<std::uint8_t,12> other{};other.fill(23);need(frames.publish(other.data(),other.size(),metadata(2)),"lease second publish");auto second=frames.read_after(1,0);pd::Attempt drop;
 {pd::AttemptBinding binding(drop,clock,3,20);need(!frames.publish(pixels.data(),pixels.size(),metadata(3)),"all leased pool rejects new frame");binding.complete();}
 need(drop.publish_outcome==pd::PublishOutcome::pool_drop&&point(drop,pd::Point::lock1_released)&&!point(drop,pd::Point::copy_enter)&&frames.counters().pool_drops==1,"drop counters/stages exact");
 need(first->rgb==std::vector<std::uint8_t>(pixels.begin(),pixels.end())&&second->rgb==std::vector<std::uint8_t>(other.begin(),other.end()),"all leased bytes unchanged");
 first.reset();need(frames.publish(pixels.data(),pixels.size(),metadata(4)),"released physical slot reused");need(second->sequence==2&&second->rgb==std::vector<std::uint8_t>(other.begin(),other.end()),"remaining lease unchanged after reuse");
 pas::LatestFrame reuse(2,2,2);need(reuse.publish(pixels.data(),pixels.size(),metadata(1)),"latest reuse first");auto held=reuse.read_after(0,0);
 need(reuse.publish(other.data(),other.size(),metadata(2))&&reuse.publish(pixels.data(),pixels.size(),metadata(3)),"unconsumed latest storage is reused while other slot leased");
 need(reuse.counters().published==3&&reuse.counters().overwritten==1&&held->sequence==1&&held->rgb==std::vector<std::uint8_t>(pixels.begin(),pixels.end()),"latest reuse counters and held bytes preserved");
}
void publish_errors(Context&){
 FixedClock clock;pas::LatestFrame frames(2,2);auto m=metadata(1);need(frames.publish(pixels.data(),pixels.size(),m),"error fixture initial publish");auto held=frames.read_after(0,0);
 for(int variant=0;variant<3;++variant){pd::Attempt a;auto bad=m;if(variant==2)bad.stride=7;
  throws<std::invalid_argument>([&]{pd::AttemptBinding binding(a,clock,2,20);frames.publish(pixels.data(),variant==1?pixels.size()-1:pixels.size(),bad);},variant==0?"sequence":"invalid RGB frame");
  need(a.exception_seen&&!a.completed&&point(a,pd::Point::publish_exit)&&!point(a,pd::Point::copy_enter),"original invalid input exception and partial exit");
  if(variant==0)need(point(a,pd::Point::lock1_released),"sequence error unlock recorded");
  need(pd::active_attempt==nullptr&&pd::active_clock==nullptr,"publish error restores TLS");
 }
 need(frames.counters().published==1&&frames.counters().pool_drops==0&&same_metadata(*held,m,m.published_ns)&&held->rgb==std::vector<std::uint8_t>(pixels.begin(),pixels.end()),"invalid inputs cannot mutate leased publication");
}
void unlock_observation(Context&){
 // The clock hook executes at release before its timestamp is stored. Ask a
 // separate worker to peek; bound the deadlock probe, not producer performance.
 FixedClock clock;pas::LatestFrame frames(2,2);std::mutex mutex;std::condition_variable_any cv;bool requested=false,finished=false,quit=false;
 std::jthread observer([&](std::stop_token stop){std::unique_lock lock(mutex);if(!cv.wait(lock,stop,[&]{return requested||quit;})||quit)return;lock.unlock();frames.peek();lock.lock();finished=true;cv.notify_all();});
 pd::Attempt a;bool unlocked=false;
 clock.on_read=[&](std::uint32_t){if(point(a,pd::Point::lock1_acquired)&&!point(a,pd::Point::lock1_released)){
  std::unique_lock lock(mutex);requested=true;cv.notify_all();unlocked=cv.wait_for(lock,std::chrono::seconds(2),[&]{return finished;});
 }};
 try{pd::AttemptBinding binding(a,clock,1,20);need(frames.publish(pixels.data(),pixels.size(),metadata(1)),"unlock fixture publication");binding.complete();}
 catch(...){std::lock_guard lock(mutex);quit=true;cv.notify_all();throw;}
 {std::lock_guard lock(mutex);quit=true;cv.notify_all();}observer.join();
 need(unlocked&&finished&&point(a,pd::Point::lock1_released),"release marker is after actual mutex unlock");
 // Early-return paths were exercised separately, using the same StampOnExit
 // after lock_guard destructor in the exact linked core object.
}
void string_facts(Context&){
 FixedClock clock;pas::LatestFrame frames(2,2,2);auto m=metadata(1);m.capture_backend=std::string(257,'x');m.source_pixel_format.clear();pd::Attempt a;
 {pd::AttemptBinding binding(a,clock,1,20);auto address=pd::before_string(a.frame_backend,m.capture_backend);m.capture_backend="offline_memory_stub";pd::after_string(a.frame_backend,m.capture_backend,address);
  need(frames.publish(pixels.data(),pixels.size(),m),"string first publish");binding.complete();}
 auto lease=frames.read_after(0,0);need(lease&&same_metadata(*lease,m,m.published_ns)&&lease->source_pixel_format.empty(),"long-to-short and empty values exact");
 need(a.frame_backend.before_size==257&&a.frame_backend.after_size==19&&a.frame_backend.assignment_returned,"frame string size/capacity facts from actual assignment");
 need(a.slot_backend.after_size==19&&a.slot_format.after_size==0&&a.slot_format.assignment_returned,"target string facts do not invent an allocation count");lease.reset();
 m.sequence=2;m.capture_backend=std::string(513,'y');m.source_pixel_format="RGB24_LONG_FORMAT";pd::Attempt b;
 {pd::AttemptBinding binding(b,clock,2,20);need(frames.publish(pixels.data(),pixels.size(),m),"string second publish");binding.complete();}
 auto second=frames.read_after(1,0);need(second&&same_metadata(*second,m,m.published_ns)&&b.slot_backend.after_size==513&&b.slot_format.after_size==17,"growth/nonempty format exact");
 need(b.slot_backend.after_capacity>=b.slot_backend.after_size&&b.slot_format.after_capacity>=b.slot_format.after_size,"observed capacity facts consistent");
}
void destruction_tls(Context&){
 FixedClock outer_clock;pd::Attempt outer,inner;FixedClock inner_clock(200);
 {pd::AttemptBinding outer_binding(outer,outer_clock,1,20);const auto outer_reads=outer_clock.reads;
  {pd::AttemptBinding inner_binding(inner,inner_clock,2,20);pd::stamp(pd::Point::copy_enter);inner_binding.complete();}
  need(pd::active_attempt==&outer&&pd::active_clock==&outer_clock&&outer_clock.reads==outer_reads,"nested TLS restored without reading outer Clock");
  std::atomic<bool> isolated=false;
  std::jthread worker([&]{FixedClock worker_clock(300);pd::Attempt worker_row;const bool initially_empty=pd::active_attempt==nullptr&&pd::active_clock==nullptr;
   {pd::AttemptBinding worker_binding(worker_row,worker_clock,3,20);pd::stamp(pd::Point::notify_enter);worker_binding.complete();}
   isolated=initially_empty&&pd::active_attempt==nullptr&&pd::active_clock==nullptr&&point(worker_row,pd::Point::notify_enter);
  });worker.join();need(isolated&&pd::active_attempt==&outer&&!point(outer,pd::Point::notify_enter),"producer TLS is not inherited by another thread");outer_binding.complete();}
 bool destructor_body=false,begin_before_body=false,end_after_body=false;pd::Attempt a;FixedClock clock;
 struct TrackedFrame {pas::Frame value;bool& body;explicit TrackedFrame(bool& b):body(b){value.capture_backend=std::string(257,'z');}~TrackedFrame(){body=true;}};
 clock.on_read=[&](std::uint32_t){if(point(a,pd::Point::publication_return)&&!point(a,pd::Point::frame_destroy_enter))begin_before_body=!destructor_body;
  else if(point(a,pd::Point::frame_destroy_enter)&&!point(a,pd::Point::frame_destroy_return))end_after_body=destructor_body;};
 throws<std::logic_error>([&]{pd::AttemptBinding binding(a,clock,4,20);pd::StampOnExit destroyed(pd::Point::frame_destroy_return);TrackedFrame frame(destructor_body);
  pd::StampOnExit destroying(pd::Point::frame_destroy_enter);pd::record(pd::Point::publication_return,100);throw std::logic_error("original_business_fault");},"original_business_fault");
 need(begin_before_body&&end_after_body&&destructor_body&&point(a,pd::Point::frame_destroy_return)&&a.exception_seen&&!a.completed,"partial exception destructor brackets and original error preserved");
 need(pd::active_attempt==nullptr&&pd::active_clock==nullptr,"destructor unwind restored TLS");
}
void output_control(Context& context){
 const auto count=std::size_t(1000)+pd::slot_extra;std::vector<pd::Attempt> rows(count);rows[0].attempt=1;rows[0].entered=true;
 const auto report=(context.root/"normal").string();pd::Output output(report,rows,"A","tap",1000,20);const auto result=output.save("synthetic_control");
 const auto index=read_json(result.at("index").at("path").get<std::string>());need(index.at("summary").at("slots")==count&&index.at("summary").at("partial")==1,"all requested+64 slots and partial row retained");
 std::size_t emitted=0;for(const auto& segment:index.at("segments")){const auto& pin=segment.at("file");const auto p=fs::path(pin.at("path").get<std::string>());auto value=read_json(p);
  need(fs::file_size(p)==pin.at("bytes").get<std::uint64_t>()&&pas::evidence::sha256(p,pd::file_bytes)==pin.at("sha256").get<std::string>(),"actual output pin exact");
  need(value.at("rows").size()<=pd::segment_rows,"segment row bound");for(const auto& row:value.at("rows"))need(row.dump().size()<=pd::row_bytes,"actual combined numeric row bound");emitted+=value.at("rows").size();}
 need(emitted==count&&index.at("file_byte_cap")==4194304,"no empty or truncated segments");
 need(output.save("second_save")==result,"repeated save returns same immutable output");
 throws<std::runtime_error>([&]{pd::Output duplicate(report,rows,"A","tap",1000,20);},"fresh publication diagnostic directory");
 const auto big_report=(context.root/"oversize-index").string();{
  pd::Output oversized(big_report,rows,std::string(pd::file_bytes,'X'),"tap",1000,20);
  throws<std::runtime_error>([&]{oversized.save("synthetic_control");},"4MiB cap");
  need(!fs::exists(fs::path(big_report+".publication-diagnostic")/"index.json"),"file cap rejects before writing an empty/oversize index");
 }
 const auto partial_report=(context.root/"partial").string();std::vector<pd::Attempt> partial_rows(count);std::atomic<bool> producer_returned=false;bool writer_finished=false;
 struct Finish {bool& value;~Finish(){value=true;}};
 throws<std::logic_error>([&]{pd::Output partial(partial_report,partial_rows,"B","hold",1000,20);Finish writer{writer_finished};
  std::jthread producer([&]{partial_rows[0].attempt=1;partial_rows[0].entered=true;partial_rows[0].exception_seen=true;producer_returned=true;});
  throw std::logic_error("original_outer_business_fault");},"original_outer_business_fault");
 const auto partial_index=read_json(fs::path(partial_report+".publication-diagnostic")/"index.json");
 need(producer_returned&&writer_finished&&partial_index.at("summary").at("partial")==1&&partial_index.at("summary").at("entered")==1,"exception output is after jthread join and writer destruction");
 for(const auto* key:{"qualification_issued","cost_qualification","normal_qualification","AA_qualification","noise_qualification","ABBA_qualification","live_ready"})need(partial_index.at(key)==false,"partial diagnostics never promote qualification");
}
struct Case {const char* id;void(*body)(Context&);};
const std::array<Case,case_count> cases{{
 {"unbound_stamp_zero_clock_reads",inactive},{"bound_existing_timestamp_alias",alias},{"bounded_slot_and_output_shapes",bounds},
 {"extra_clock_throw_is_partial_not_business_throw",extra_clock_fault},{"negative_backward_stamp_sticky_invalid",invalid_stamp},
 {"wait_clock_forwarding_and_absolute_due",wait_forwarding},{"single_large_gap_vs_uniform_reads",gap_partition},
 {"already_late_wait_no_positive_gap_claim",no_gap},{"publish_complete_points_and_metadata_pixels",complete_publish},
 {"publish_closed_first_lock",closed_publish},{"publish_pool_drop_with_live_leases",leases_reuse},{"publish_geometry_sequence_exception",publish_errors},
 {"unlock_brackets_include_early_return",unlock_observation},{"string_facts_preserve_long_and_empty_values",string_facts},
 {"frame_destruction_and_nested_tls_unwind",destruction_tls},{"fresh_partial_postjoin_output",output_control}
}};
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("controls FRESH_REPORT_JSON only");
 const std::string source=PAS_PUBLICATION_DIAGNOSTIC_SOURCE_COMMIT;const bool dirty=PAS_PUBLICATION_DIAGNOSTIC_SOURCE_DIRTY;
 if(source.size()!=40||source.find_first_not_of("0123456789abcdef")!=std::string::npos||dirty)throw std::runtime_error("clean compiled diagnostic source required");
 const auto report=fs::absolute(argv[1]);if(!fs::is_directory(report.parent_path())||fs::exists(report)||fs::is_symlink(fs::symlink_status(report)))throw std::runtime_error("fresh bounded controls report path");
 Context context{fs::path(report.string()+".artifacts")};if(!fs::create_directory(context.root))throw std::runtime_error("fresh controls artifact directory");
 J checks=J::array();std::size_t failed=0;
 for(const auto& test:cases){const auto before=expectations;bool pass=true;std::string error;
  auto* previous=pd::active_attempt;auto* previous_clock=pd::active_clock;
  try{test.body(context);need(pd::active_attempt==previous&&pd::active_clock==previous_clock,"case restored original TLS");}
  catch(const std::exception& e){pass=false;error=e.what();if(error.size()>512)error.resize(512);}
  catch(...){pass=false;error="unknown exception";}
  // Restore only after the leak check/error has made this case fail. Never
  // dereference a possibly dangling leaked diagnostic pointer in a later case.
  pd::active_attempt=previous;pd::active_clock=previous_clock;
  failed+=!pass;checks.push_back({{"id",test.id},{"pass",pass},{"expectations",expectations-before},{"error",error}});
  if(!pass)std::cerr<<test.id<<": "<<error<<'\n';
 }
 const J value={{"schema","pas.cold-publication-diagnostic-controls.v1"},{"assertions",case_count},{"checks",checks},{"failed",failed},{"passed",case_count-failed},{"pass",failed==0},
  {"expectation_count",expectations},{"assertion_unit","one aggregate case; individual expectations separately counted"},
  {"source_build_commit",source},{"source_build_dirty",dirty},{"source_commit",source},{"source_dirty",dirty},
  {"synthetic_api_controls",true},{"component_memory_controls",true},{"diagnostic_only",true},{"device_endpoints",0},{"device_operations",0},
  {"etw_start_operations",0},{"etw_stop_operations",0},{"actual_recording_tested",false},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},
  {"original_wait_controls_unchanged",{{"original",43},{"diagnostic",20},{"active_wait",12},{"process_qos",24}}},
  {"point_count",pd::point_count},{"slot_bytes",sizeof(pd::Attempt)},{"slot_byte_cap",pd::slot_byte_bound},
  {"row_byte_cap",pd::row_bytes},{"segment_rows",pd::segment_rows},{"segment_cap",pd::max_segments},{"file_byte_cap",pd::file_bytes},
  {"controls_artifact_root",context.root.string()},{"controls_artifacts_preserved",true},{"performance_measurement",false},{"root_cause_claim",false}};
 const auto bytes=value.dump(2);if(bytes.size()+1>1024*1024)throw std::runtime_error("controls JSON1MiB cap");
 std::ofstream out(report,std::ios::binary);out<<bytes<<'\n';out.flush();if(!out)throw std::runtime_error("controls report write");
 return failed?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
