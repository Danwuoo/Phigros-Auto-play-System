#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "cold_stop_boundary.hpp"
// Compile the exact manual-session body with endpoint-free user-input
// boundaries. No desktop key state or console registration enters this
// fixture. A real external Ctrl+C can still terminate its console process;
// such a native interruption remains a negative guard receipt.
#define GetAsyncKeyState ::cold::controlled_key_state
#define SetConsoleCtrlHandler ::cold::controlled_console_handler
#include "frozen_manual_session.cpp"
#undef SetConsoleCtrlHandler
#undef GetAsyncKeyState
#include "producer_wait.hpp"
#include "producer_placement.hpp"
#include "poll_gap_diagnostic.hpp"
#if defined(PAS_WAIT_DIAGNOSTIC_COMPONENT) && PAS_WAIT_DIAGNOSTIC_COMPONENT
#include "wait_diagnostic_component_identity.hpp"
#endif
#include "tap_reset_workload.hpp"
#include "streaming_workload.hpp"
#include "pas/result_ui_labels.hpp"
#include "pas/lossless_event_stream.hpp"
#include <psapi.h>
#include <set>

// These symbols replace emulator boundaries in this test executable.
// No pas_emulator.lib, discovery, ADB, channel creation or sendTouch is linked.
namespace cold {
using J=nlohmann::json;
namespace fs=std::filesystem;
struct Attempt {
 pas::Nanoseconds due{},render_start{},render_end{},callback_return{};
 pas::Nanoseconds previous_callback_return{},wait_enter{},wait_return{};
 pas::current_rails::ProducerWaitTrace wait_trace{};
 poll_gap::Snapshot poll_gap{};
 pas::current_rails::ProducerThreadCpuSample cpu_before{},cpu_after{};
 bool wait_entered=false,wait_return_available=false;
};
static_assert(std::is_trivially_copyable_v<Attempt> && sizeof(Attempt)<=488);
struct State {
 std::string scene;int requested{},cadence{};
 // Functional no-persistence tests may render slower than their scheduled
 // workload in Debug. Pace their result pixels from actual workload completion;
 // the original diagnostic/cost producer and all requested due times stay exact.
 bool score_only_result_tail=false;
 // Diagnostic facts flow out after the joined literal supervisor. Scores,
 // AA and formal runtime do not enable this optional trace.
 bool producer_wait_diagnostics=false;
 std::uint32_t producer_wait_thread_id=0;
 std::int64_t producer_wait_qpc_frequency=0;
 std::atomic<bool> cancelled=false;
 std::vector<Attempt> attempts;
 producer_placement::Snapshot producer_placement;
 // Boundary-only accounting; never fed to perception or action decisions.
 // Explicit Up and cancellation releases stay separate.
 std::uint64_t downs{},moves{},ups{},releases{},released_contacts{},contacts_final{},preflights{},discoveries{},constructors{};
 std::optional<std::thread::id> touch_owner;
};
State state;
constexpr pas::Nanoseconds score_result_epoch(pas::Nanoseconds planned_end,pas::Nanoseconds actual_end){return std::max(planned_end,actual_end);}
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void owner(){auto id=std::this_thread::get_id();if(!state.touch_owner)state.touch_owner=id;require(*state.touch_owner==id,"multiple touch owners");}
void box(std::vector<std::uint8_t>& rgb,int x0,int y0,int x1,int y1,std::array<std::uint8_t,3> color){
 for(int y=std::max(0,y0);y<std::min(720,y1);++y)for(int x=std::max(0,x0);x<std::min(1280,x1);++x)std::copy(color.begin(),color.end(),rgb.begin()+std::size_t(y)*3840+x*3);
}
void dense(std::vector<std::uint8_t>& rgb){
 std::fill(rgb.begin(),rgb.end(),30);box(rgb,20,20,26,42,{255,255,255});box(rgb,34,20,40,42,{255,255,255});
 for(int d=0;d<6;++d)box(rgb,1020+d*24,20,1032+d*24,40,{255,255,255});
 for(int row=0;row<16;++row){int y=150+row*31;box(rgb,40,y-1,1240,y+2,{255,255,255});for(int col=0;col<8;++col){int x=190+col*130;box(rgb,x-39,y-12,x+39,y-3,{40,190,255});}}
}
void result(std::vector<std::uint8_t>& rgb){
 std::fill(rgb.begin(),rgb.end(),30);
 for(const auto& l:pas::result_ui::labels)for(int y=0;y<l.height;++y)for(int x=0;x<l.width;++x)if(l.bits[std::size_t(y)*l.width+x]=='#')box(rgb,l.x+x,l.y+y,l.x+x+1,l.y+y+1,{255,255,255});
}
J read(const fs::path& path){require(fs::is_regular_file(path)&&fs::file_size(path)<=16*1024*1024,"bounded report file");std::ifstream f(path);return J::parse(f);}
void save(const fs::path& path,const J& value){require(!fs::exists(path),"fresh report required");std::ofstream f(path,std::ios::binary);f<<value.dump(2)<<'\n';require(bool(f),"report write");}
constexpr std::size_t wait_diagnostic_segment_rows=2048,wait_diagnostic_max_segments=5;
constexpr std::size_t wait_diagnostic_file_cap=4*1024*1024,wait_diagnostic_row_byte_bound=1536;
static_assert(wait_diagnostic_segment_rows*wait_diagnostic_row_byte_bound+4096<wait_diagnostic_file_cap);
J save_wait_diagnostic_file(const fs::path& path,const J& value){
 require(!fs::exists(path),"fresh producer wait diagnostic file");const auto encoded=value.dump();
 require(encoded.size()+1<=wait_diagnostic_file_cap,"bounded producer wait diagnostic file");
 {std::ofstream f(path,std::ios::binary);f<<encoded<<'\n';f.flush();require(bool(f),"producer wait diagnostic write");}
 return {{"path",fs::absolute(path).string()},{"bytes",fs::file_size(path)},{"sha256",pas::sha256_file(path)}};
}
J cpu_sample_json(const pas::current_rails::ProducerThreadCpuSample& s){return J::array({s.kernel_100ns,s.user_100ns,s.status,s.error,s.sampled});}
#if defined(PAS_WAIT_DIAGNOSTIC_COMPONENT) && PAS_WAIT_DIAGNOSTIC_COMPONENT
J wait_component_identity(){return {{"schema","pas.cold-wait-diagnostic-component-identity.v1"},
 {"source_commit",pas_wait_diagnostic_source_commit},{"source_dirty",pas_wait_diagnostic_source_dirty},
 {"base_source_commit",pas_wait_diagnostic_base_source_commit},{"main_sha256",pas_wait_diagnostic_main_sha256},
 {"poll_gap_header_sha256",pas_wait_diagnostic_header_sha256},{"diagnostic_only",true},{"qualification_issued",false}};}
#endif
J save_wait_diagnostics(const fs::path& root,const char* completion){
 require(state.producer_wait_diagnostics&&state.requested>=1000&&state.requested<=10000&&
     state.attempts.size()==std::size_t(state.requested)+64,"fixed producer wait diagnostic slots");
 J segments=J::array();std::size_t entered=0,completed=0,failed=0,cpu_available=0,cpu_unknown=0,kernel_calls=0,gap_available=0,gap_invalid=0,gap_valid=0;
 for(std::size_t begin=0;begin<state.attempts.size();begin+=wait_diagnostic_segment_rows){
  const auto end=std::min(state.attempts.size(),begin+wait_diagnostic_segment_rows);J rows=J::array(),gap_rows=J::array();
  for(std::size_t i=begin;i<end;++i){const auto& a=state.attempts[i];const auto& t=a.wait_trace;J kernels=J::array();
   require(t.kernel_calls<=pas::current_rails::producer_wait_kernel_cap&&t.spin_poll_count<=pas::current_rails::producer_active_tail_poll_cap,"fixed wait trace counters");
   for(std::size_t k=0;k<t.kernel_calls;++k){const auto& v=t.kernels[k];kernels.push_back(J::array({v.ticks_100ns,v.arm_begin_ns,v.arm_end_ns,v.kernel_begin_ns,v.kernel_end_ns,v.arm_status,v.arm_error,v.wait_result,v.wait_error,v.timestamp_flags,v.api_flags}));}
   auto row=J::array({i+1,i>=256&&i<std::size_t(state.requested),a.render_end!=0,
    a.due,a.render_start,a.render_end,a.callback_return,a.previous_callback_return,a.wait_enter,a.wait_return,a.wait_entered,a.wait_return_available,
    J::array({t.first_qpc_ns,t.last_qpc_ns,t.qpc_reads,t.kernel_calls,t.spin_enter_ns,t.spin_return_ns,t.spin_poll_count,t.spin_pause_calls,
      static_cast<unsigned>(t.phase),static_cast<unsigned>(t.failure_phase),static_cast<unsigned>(t.failure),t.entered,t.completed,t.spin_entered,t.spin_completed}),
    kernels,cpu_sample_json(a.cpu_before),cpu_sample_json(a.cpu_after)});
   const auto& gap=a.poll_gap;
   require(gap.sample_count<=poll_gap::sample_cap&&gap.interval_count<=poll_gap::interval_cap,"fixed poll gap diagnostic counters");
   auto gap_row=J::array({i+1,gap.max_gap_ns,gap.gap_begin_ns,gap.gap_end_ns,gap.end_sample_index,gap.sample_count,gap.interval_count,gap.available,gap.invalid});
   // Preserve all original sixteen row columns and their values. The optional
   // gap rows are an independent, attempt-keyed extension within the same cap.
   require(row.dump().size()+gap_row.dump().size()<=wait_diagnostic_row_byte_bound,"fixed numeric wait diagnostic row bound");
   rows.push_back(std::move(row));gap_rows.push_back(std::move(gap_row));gap_available+=gap.available;gap_invalid+=gap.invalid;gap_valid+=gap.available&&!gap.invalid;
   entered+=t.entered;completed+=t.completed;failed+=t.entered&&!t.completed;kernel_calls+=t.kernel_calls;
   if(a.wait_entered){if(pas::current_rails::producer_thread_cpu_delta_100ns(a.cpu_before,a.cpu_after))++cpu_available;else ++cpu_unknown;}
  }
  const auto number=segments.size();require(number<wait_diagnostic_max_segments,"producer wait diagnostic segment cap");
  const auto path=root/("producer-wait-diagnostic-segment-"+std::to_string(number)+".json");
  const auto pin=save_wait_diagnostic_file(path,{{"schema","pas.cold-producer-wait-diagnostic-segment.v1"},{"segment",number},{"first_attempt",begin+1},{"last_attempt",end},{"rows",rows},
   {"poll_gap_schema","pas.cold-producer-poll-gap-rows.v1"},{"poll_gap_rows",gap_rows}});
  segments.push_back({{"first_attempt",begin+1},{"last_attempt",end},{"rows",end-begin},{"file",pin}});
 }
 J summary={{"slots",state.attempts.size()},{"waits_entered",entered},{"waits_completed",completed},{"waits_failed",failed},
  {"slots_not_entered",state.attempts.size()-entered},{"kernel_calls",kernel_calls},{"cpu_pairs_available",cpu_available},{"cpu_pairs_unknown",cpu_unknown}};
 const auto path=root/"producer-wait-diagnostic.json";
 J diagnostic={{"schema","pas.cold-producer-wait-diagnostic.v1"},{"source_build_commit",::pas_session_build_commit},
  {"source_build_dirty",::pas_session_build_dirty},{"completion",completion},{"scene",state.scene},{"requested",state.requested},{"warmup",256},{"measurement_attempts",state.requested-256},
  {"cadence_ms",state.cadence},{"thread_id",state.producer_wait_thread_id},{"clock_domain",state.producer_wait_qpc_frequency>0?"host_qpc_ns":"provided_clock_ns"},
  {"host_clock_frequency_hz",state.producer_wait_qpc_frequency>0?J(state.producer_wait_qpc_frequency):J(nullptr)},
  {"qpc_value_kind","actual Clock::now_ns values; HostClock converts QPC ticks to monotonic ns; no raw QPC ticks are claimed"},
  {"cpu_api","GetThreadTimes(GetCurrentThread) before and after wait; outer QPC timestamps bracket these two samples"},
  {"cpu_units","raw kernel/user accounting 100ns units; granularity unknown; wall minus CPU is not exact off-CPU"},
  {"initial_native_status",std::numeric_limits<std::uint32_t>::max()},{"api_flags",{{"arm_returned",1},{"wait_returned",2}}},
  {"native_status_availability","api_flags distinguish unreached calls from raw WAIT_FAILED; timestamp_flags independently mark actual QPC values"},
  {"timestamp_flags",{{"arm_begin",1},{"arm_end",2},{"kernel_begin",4},{"kernel_end",8}}},
  {"phase_names",{"none","qpc","timer_arm","kernel_wait","spin","complete"}},
  {"failure_names",{"none","clock_read","clock_domain","nonmonotonic_clock","horizon","early_wake_bound","spin_bound","kernel_callback","pause_callback"}},
  {"attempt_columns",{"attempt","measurement","produced","producer_due_ns","render_start_ns","render_end_ns","capture_callback_return_ns","previous_callback_return_ns","wait_enter_ns","wait_return_ns","wait_entered","wait_return_available","wait","kernels","cpu_before","cpu_after"}},
  {"wait_columns",{"first_qpc_ns","last_qpc_ns","qpc_reads","kernel_calls","spin_enter_ns","spin_return_ns","spin_poll_count","spin_pause_calls","phase","failure_phase","failure","entered","completed","spin_entered","spin_completed"}},
  {"kernel_columns",{"ticks_100ns","arm_begin_ns","arm_end_ns","kernel_begin_ns","kernel_end_ns","arm_status","arm_error","wait_result","wait_error","timestamp_flags","api_flags"}},
  {"cpu_columns",{"kernel_100ns","user_100ns","status","error","sampled"}},
  {"segment_row_cap",wait_diagnostic_segment_rows},{"segment_cap",wait_diagnostic_max_segments},{"file_byte_cap",wait_diagnostic_file_cap},
  {"kernel_sample_cap",pas::current_rails::producer_wait_kernel_cap},{"spin_poll_cap",pas::current_rails::producer_active_tail_poll_cap},
  {"wait_policy","full_period_active_qpc"},{"active_tail_ns",0},{"kernel_timeout_ms",nullptr},{"no_kernel_wait",true},
  {"horizon_ns",pas::current_rails::producer_active_wait_horizon_ns},
  {"summary",summary},{"segments",segments},{"diagnostic_only",true},{"instrumentation_overhead_measured",false},
  {"cost_qualification",false},{"physical_qualification",false},{"live_ready",false},{"device_operations",0}};
 diagnostic["poll_gap_diagnostic"]={{"schema","pas.cold-producer-poll-gap-diagnostic.v1"},
  {"segment_field","poll_gap_rows"},{"segment_schema","pas.cold-producer-poll-gap-rows.v1"},
  {"columns",{"attempt","max_gap_ns","gap_begin_ns","gap_end_ns","end_sample_index","sample_count","interval_count","available","invalid"}},
  {"sample_cap",poll_gap::sample_cap},{"interval_cap",poll_gap::interval_cap},{"snapshot_byte_cap",40},{"attempt_slot_byte_cap",488},{"attempt_slot_bytes",sizeof(Attempt)},
  {"clock_reads","each wrapped now_ns calls the original Clock once; no extra QPC or OS calls"},
  {"scope","only original Clock reads inside this producer wait; epoch, outer wait, render and callback timestamps use the original Clock"},
  {"availability","a positive nonnegative monotonic adjacent interval was observed; valid timing evidence requires available && !invalid; single-sample and equal-only waits have no available positive gap"},
  {"invalid","sticky original Clock exception, negative or backward timestamp, or diagnostic counter cap; original return or exception is unchanged"},
  {"tie_rule","first maximum; end_sample_index is one-based"},
  {"interpretation","elapsed interval between adjacent converted QPC reads; not thread CPU or a scheduler, sleep or clock-cause verdict"},
  {"summary",{{"observed_positive_gap_slots",gap_available},{"valid_positive_gap_slots",gap_valid},{"partial_positive_gap_slots",gap_available-gap_valid},{"invalid_slots",gap_invalid}}},
  {"instrumentation_overhead_measured",false},{"diagnostic_only",true},{"qualification_issued",false}};
#if defined(PAS_WAIT_DIAGNOSTIC_COMPONENT) && PAS_WAIT_DIAGNOSTIC_COMPONENT
 diagnostic["diagnostic_component_identity"]=wait_component_identity();
#endif
 const auto pin=save_wait_diagnostic_file(path,diagnostic);
 return {{"index",pin},{"summary",summary},{"diagnostic_only",true},{"qualification_issued",false}};
}
J dist(std::vector<double> values){if(values.empty())return {{"n",0}};std::sort(values.begin(),values.end());auto q=[&](double p){double at=(values.size()-1)*p;auto lo=std::size_t(at),hi=std::min(lo+1,values.size()-1);return values[lo]+(values[hi]-values[lo])*(at-lo);};return {{"n",values.size()},{"p50",q(.5)},{"p95",q(.95)},{"p99",q(.99)},{"max",values.back()},{"jitter_p95_minus_p5",q(.95)-q(.05)}};}
}
namespace pas {
GrpcEndpoint discover_endpoint(const std::string& serial,const std::filesystem::path&){cold::require(serial=="emulator-offline-cold","cold serial required");++cold::state.discoveries;return {"", "", "endpoint-free-test-double"};}
nlohmann::json game_preflight(const std::string& config,const std::string& capability){
 ++cold::state.preflights;const auto c=load_config(config);cold::require(c.serial=="emulator-offline-cold"&&c.endpoint.empty()&&c.token_file.empty()&&capability=="endpoint-free-test-double","device-free cold preflight");
 return {{"fingerprint_matches",true},{"synthetic_fixture",true},{"physical_qualification",false}};
}
CaptureOptions runtime_capture_options(const RuntimeConfig& c){CaptureOptions o;o.width=c.width;o.height=c.height;o.source_rotation=c.source_rotation;o.grpc_read_chunk_kib=c.grpc_read_chunk_kib;return o;}
GrpcCapture::GrpcCapture(const Clock& c,GrpcEndpoint endpoint,CaptureOptions options):clock_(c),endpoint_(std::move(endpoint)),options_(options){
 cold::require(endpoint_.target.empty()&&endpoint_.token.empty()&&options_.width==1280&&options_.height==720,"no capture endpoint permitted");++cold::state.constructors;
}
GrpcCapture::~GrpcCapture()=default;
void GrpcCapture::cancel(){cold::state.cancelled=true;}
CaptureStats GrpcCapture::stats()const{std::lock_guard lock(mutex_);return stats_;}
void GrpcCapture::stream(std::stop_token stop,const std::function<void(Frame&&)>& on_frame){
 current_rails::WindowsWorkerAffinityApi affinity_api;
 current_rails::WindowsProducerPriorityApi priority_api;
 const auto& placement=current_rails::WindowsWorkerAffinityApi::plan();
 Nanoseconds result_epoch=0;
 cold::producer_placement::run(affinity_api,priority_api,placement,cold::state.producer_placement,[&]{
 if(cold::state.producer_wait_diagnostics){
  cold::state.producer_wait_thread_id=GetCurrentThreadId();
  if(const auto* host=dynamic_cast<const HostClock*>(&clock_))cold::state.producer_wait_qpc_frequency=host->frequency();
 }
 current_rails::ActiveProducerDeadlineWait wait;const auto epoch=clock_.now_ns();
 result_epoch=epoch+Nanoseconds(cold::state.requested)*cold::state.cadence*1'000'000;
 std::vector<std::uint8_t> storage(1280*720*3);cold::state.attempts.resize(std::size_t(cold::state.requested)+64);
 for(int i=0;i<cold::state.requested+64&&!stop.stop_requested()&&!cold::state.cancelled;++i){
  if(cold::state.score_only_result_tail&&i==cold::state.requested)
   result_epoch=cold::score_result_epoch(result_epoch,clock_.now_ns());
  auto& a=cold::state.attempts[std::size_t(i)];
  a.due=cold::state.score_only_result_tail&&i>=cold::state.requested
   ?result_epoch+Nanoseconds(i-cold::state.requested)*cold::state.cadence*1'000'000
   :epoch+Nanoseconds(i)*cold::state.cadence*1'000'000;
  if(cold::state.producer_wait_diagnostics){
   a.previous_callback_return=i?cold::state.attempts[std::size_t(i-1)].callback_return:0;
   a.wait_enter=clock_.now_ns();a.wait_entered=true;a.cpu_before=current_rails::sample_producer_thread_cpu();
   cold::poll_gap::Clock wait_clock(clock_,a.poll_gap);
   try{wait.wait_until(wait_clock,a.due,&a.wait_trace);}
   catch(...){
    a.cpu_after=current_rails::sample_producer_thread_cpu();
    try{a.wait_return=clock_.now_ns();a.wait_return_available=true;}catch(...){}
    throw;
   }
   a.cpu_after=current_rails::sample_producer_thread_cpu();a.wait_return=clock_.now_ns();a.wait_return_available=true;
  }else wait.wait_until(clock_,a.due);
  a.render_start=clock_.now_ns();
  if(i>=cold::state.requested)cold::result(storage);
  else if(cold::state.scene=="tap")current_rails::tap_reset_workload::render(storage,std::uint64_t(i+1));
  else if(cold::state.scene=="hold")current_rails::streaming_workload::render(storage,std::uint64_t(i+1));
  else cold::dense(storage);
  a.render_end=clock_.now_ns();Frame f;f.sequence=std::uint64_t(i+1);f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.capture_backend="cold_synthetic_capture_boundary";f.capture_complete_ns=f.pixels_ready_ns=a.render_end;f.rgb=std::move(storage);
  {std::lock_guard lock(mutex_);++stats_.valid;stats_.last_notification_ns=f.capture_complete_ns;}
  on_frame(std::move(f));a.callback_return=clock_.now_ns();storage=std::move(f.rgb);
 }
 });
 while(!cold::state.cancelled&&!stop.stop_requested()){
  if(clock_.now_ns()>result_epoch+2'000'000'000)throw std::runtime_error("cold finite pipeline did not finish from result pixels");
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
}
GrpcTouch::GrpcTouch(const Clock& c,GrpcEndpoint endpoint,PixelCoordinateMap mapping,int width,int height,int max_contacts,std::chrono::milliseconds timeout)
 :clock_(c),endpoint_(std::move(endpoint)),mapping_(std::move(mapping)),width_(width),height_(height),max_contacts_(max_contacts),timeout_(timeout){cold::require(endpoint_.target.empty()&&endpoint_.token.empty()&&max_contacts==5,"no touch endpoint permitted");cold::owner();++cold::state.constructors;}
TouchReceipt GrpcTouch::inject(const TouchCommand& command){
 cold::owner();cold::require(command.contact_id>=0&&command.contact_id<max_contacts_,"contact pool");auto [x,y]=mapping_.map(command.x,command.y);cold::require(x>=0&&y>=0&&x<width_&&y<height_,"mapped geometry");
 const bool exists=positions_.contains(command.contact_id);cold::require(command.phase==Phase::down?!exists:exists,"contact phase ownership");const auto start=clock_.now_ns();
 if(command.phase==Phase::down){++cold::state.downs;positions_[command.contact_id]={x,y};}else if(command.phase==Phase::move){++cold::state.moves;positions_[command.contact_id]={x,y};}else {++cold::state.ups;positions_.erase(command.contact_id);}
 cold::state.contacts_final=positions_.size();
 return {command,start,clock_.now_ns(),true,"endpoint_free_cold_effect_unverified"};
}
ReleaseReport GrpcTouch::release_all(){cold::owner();++cold::state.releases;ReleaseReport r;r.start_ns=clock_.now_ns();for(const auto& [id,_]:positions_)r.requested_ids.push_back(id);cold::state.released_contacts+=positions_.size();positions_.clear();cold::state.contacts_final=positions_.size();r.return_ns=clock_.now_ns();return r;}
ReleaseReport GrpcTouch::emergency_release_all(){return release_all();}
}
int main(int argc,char** argv){try{
 using namespace cold;require(argc==5,"usage tap|hold|dense requested cadence-ms fresh-directory");state.scene=argv[1];state.requested=std::stoi(argv[2]);state.cadence=std::stoi(argv[3]);require((state.scene=="tap"||state.scene=="hold"||state.scene=="dense")&&state.requested>=1000&&state.requested<=10000&&(state.cadence==8||state.cadence==20),"bounded cold workload");
 fs::path root=fs::absolute(argv[4]);require(!fs::exists(root),"fresh output root required");fs::create_directory(root);
 // Verify the actual public function still rejects before even test-double I/O.
 bool rejected=false;try{pas::run_current_candidate_manual_session(pas::CandidateLiveAdmission{},"missing","missing",true);}catch(const std::exception& e){rejected=std::string_view(e.what()).starts_with("current candidate live admission UNQUALIFIED:");}
 require(rejected&&state.preflights==0&&state.discoveries==0&&state.constructors==0,"public admission must remain closed");
 J config={{"schema",2},{"name","endpoint-free production pipeline cost fixture"},{"serial","emulator-offline-cold"},{"capture",{{"kind","emulator-grpc"},{"execution","thread"},{"transport","payload"},{"image_format","rgb888"},{"row_order","top-down"},{"width",1280},{"height",720},{"source_rotation",1},{"grpc_read_chunk_kib",256},{"max_relative_lag_ms",250}}},{"touch",{{"kind","emulator-grpc"},{"width",720},{"height",1280},{"rotation_deg",90},{"timeout_ms",100},{"max_contacts",5}}},{"scheduler",{{"max_plans",128},{"max_steps",16},{"horizon_ms",350},{"evidence_max_age_ms",100}}},{"game",{{"enabled_types",{"tap","hold","drag","flick"}},{"lead_ms",35},{"uncertainty_ms",30}}},{"preview",{{"hz",0}}},{"log_dir",(root/"archive").string()}};
 const auto config_path=root/"profile.json";save(config_path,config);
 // Same translation unit, exact production implementation. This test-only
 // wrapper and boundary symbols cannot grant a token to the production exe.
 state.producer_wait_diagnostics=true;state.attempts.resize(std::size_t(state.requested)+64);
 pas::current_rails::ColdProcessQosSnapshot process_qos;
 pas::current_rails::WindowsColdProcessQosApi process_qos_api;
 std::exception_ptr run_failure;
 try{pas::current_rails::run_cold_process_qos(process_qos_api,process_qos,[&]{
  pas::run_manual_session_impl(config_path.string(),"endpoint-free-test-double",true,0,false,false,true,pas::ManualVariant::current_candidate);
 });}
 catch(...){run_failure=std::current_exception();}
 J wait_diagnostic;
 try{wait_diagnostic=save_wait_diagnostics(root,run_failure?"joined_exception":"joined_completion");}
 catch(const std::exception& e){if(!run_failure)throw;std::cerr<<"partial producer wait diagnostic save failed: "<<e.what()<<'\n';}
 if(run_failure){
  if(!wait_diagnostic.is_null())try{J failure={{"schema","pas.cold-producer-wait-diagnostic-failure.v1"},{"diagnostic",wait_diagnostic},
   {"worker_placement",producer_placement::plan_json(state.producer_placement.placement)},
   {"producer_affinity",producer_placement::affinity_json(state.producer_placement.affinity)},
   {"producer_priority",producer_placement::priority_json(state.producer_placement)},
   {"process_qos",pas::current_rails::cold_process_qos_json(process_qos)},
   {"qualification_issued",false},{"cost_ready",false},{"live_ready",false},{"device_operations",0}};
#if defined(PAS_WAIT_DIAGNOSTIC_COMPONENT) && PAS_WAIT_DIAGNOSTIC_COMPONENT
   failure["diagnostic_component_identity"]=wait_component_identity();
#endif
   save(root/"producer-wait-diagnostic-failure.json",failure);}
  catch(const std::exception& e){std::cerr<<"partial producer wait failure marker save failed: "<<e.what()<<'\n';}
  std::rethrow_exception(run_failure);
 }
 require(stop_boundary.key_polls>0&&stop_boundary.handler_adds==1&&stop_boundary.handler_removes==1&&
     stop_boundary.handler==nullptr&&!stop_boundary.escape_held&&!stop_boundary.console_pending&&
     stop_boundary.console_deliveries==0,"controlled cold user-input boundary and handler cleanup");
 std::vector<fs::path> dirs;for(const auto& e:fs::directory_iterator(root/"archive"))if(e.is_directory())dirs.push_back(e.path());require(dirs.size()==1,"one finite session");const auto session=dirs[0],round=session/"round-1";
 const auto summary=read(session/"summary.json"),round_summary=read(round/"summary.json");require(summary.at("state")=="STOPPED"&&round_summary.at("status")=="result_confirmed","synthetic result lifecycle must complete");
 std::map<std::uint64_t,pas::Nanoseconds> owner_started;std::set<std::uint64_t> returned;std::vector<double> observer,hook,owner_ms,capture_to_owner,packet_wait,late;std::uint64_t rows=0,bytes=0,decoded=0;
 require(round_summary.at("event_encoding")=="gzip-jsonl-v1"&&round_summary.at("event_segments").size()<=256,"current bounded lossless journal");
 for(const auto& segment:round_summary.at("event_segments")){
  const auto path=round/segment.at("path").get<std::string>();bytes+=fs::file_size(path);require(bytes<=512ull*1024*1024,"bounded full journal");pas::LosslessEventReader f(path);std::string line;while(f.next(line)){require(line.size()<=1024*1024&&++rows<=1000000,"bounded journal rows");const auto v=J::parse(line);const auto event=v.at("event").get<std::string>();
   if(event=="candidate_packet_owner_qpc"&&v.at("eligible").get<bool>()){auto frame=v.at("source_frame").get<std::uint64_t>();owner_started[frame]=v.at("owner_start_ns");if(frame>256&&frame<=std::uint64_t(state.requested)){capture_to_owner.push_back((v.at("owner_start_ns").get<pas::Nanoseconds>()-v.at("capture_complete_ns").get<pas::Nanoseconds>())/1e6);packet_wait.push_back((v.at("owner_start_ns").get<pas::Nanoseconds>()-v.at("packet_published_ns").get<pas::Nanoseconds>())/1e6);}}
   if(event=="candidate_owner_return_qpc"){auto frame=v.at("source_frame").get<std::uint64_t>();require(returned.insert(frame).second,"owner frame must not repeat");if(owner_started.contains(frame)&&frame>256&&frame<=std::uint64_t(state.requested)){hook.push_back(v.at("hook_compute_ns").get<pas::Nanoseconds>()/1e6);owner_ms.push_back(v.at("owner_compute_ns").get<pas::Nanoseconds>()/1e6);}}
   if(event=="candidate_backend_receipt"){const auto& issued=v.at("issued");if(issued.at("source_frame").get<std::uint64_t>()>256)late.push_back((v.at("injection_start_ns").get<pas::Nanoseconds>()-issued.at("scheduled").get<pas::Nanoseconds>())/1e6);}
  }
  decoded+=f.decoded_bytes();require(decoded<=4ull*1024*1024*1024,"bounded full decoded journal");
 }
 J attempts=J::array();for(std::size_t i=0;i<state.attempts.size();++i){const auto& a=state.attempts[i];attempts.push_back({{"attempt",i+1},{"producer_due_ns",a.due},{"render_start_ns",a.render_start},{"render_end_ns",a.render_end},{"capture_callback_return_ns",a.callback_return},{"produced",a.render_end!=0},{"measurement",i>=256&&i<std::size_t(state.requested)}});}save(root/"attempts.json",attempts);
 PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);require(K32GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory))!=0,"OS peak RSS");
 J report={{"schema","pas.windows-production-pipeline-cold.v1"},{"scene",state.scene},{"requested",state.requested},{"warmup",256},{"measurement_attempts",state.requested-256},{"cadence_ms",state.cadence},{"actual_manual_translation_unit_sha256",PAS_COLD_MANUAL_SHA},{"source_build_commit",pas_session_build_commit},{"source_build_dirty",pas_session_build_dirty},{"public_entry_rejected",rejected},{"endpoint_free",true},{"preflight_is_test_double",true},{"capture_is_test_double",true},{"touch_is_test_double",true},{"production_packet_action_wake_archive_executed",true},{"real_capture_rpc_measured",false},{"device_operations",0},{"live_ready",false},{"archive_root",session.string()},{"summary",summary},{"round_summary",round_summary},{"owner_measurement_eligible",capture_to_owner.size()},{"owner_measurement_coverage",double(capture_to_owner.size())/(state.requested-256)},{"capture_to_owner_start_ms",dist(capture_to_owner)},{"packet_wait_ms",dist(packet_wait)},{"hook_ms",dist(hook)},{"owner_ms",dist(owner_ms)},{"measurement_phase_lateness_ms",dist(late)},{"down",state.downs},{"move",state.moves},{"up",state.ups},{"release_calls",state.releases},{"single_touch_owner",state.touch_owner.has_value()},{"full_journal_rows",rows},{"full_journal_bytes",bytes},{"full_journal_decoded_bytes",decoded},{"event_encoding","gzip-jsonl-v1"},{"rss_peak_os",memory.PeakWorkingSetSize},{"scope","literal production Packet/Wake/pool/perception/action/supervisor/SessionArchive; synthetic boundaries and result; no device or physical qualification"}};
 require(state.producer_placement.verified(),"cold producer placement/priority must explicitly restore after joined capture worker");
 require(process_qos.verified(),"cold own-process QoS must explicitly restore after all manual-session workers join");
 report["process_qos"]=pas::current_rails::cold_process_qos_json(process_qos);
 report["producer_priority_policy"]="synthetic capture thread only; NORMAL process class; scoped HIGHEST2; actual query/set/readback/explicit restore";
 report["producer_priority_requested"]=pas::current_rails::producer_priority_requested;
 report["producer_wait_policy"]="full_period_active_qpc";
 report["producer_active_tail_ns"]=0;
 report["producer_active_wait_horizon_ns"]=pas::current_rails::producer_active_wait_horizon_ns;
 report["producer_active_wait_poll_cap"]=pas::current_rails::producer_active_wait_poll_cap;
 report["producer_no_kernel_wait"]=true;
 report["worker_placement"]=producer_placement::plan_json(state.producer_placement.placement);
 report["producer_affinity"]=producer_placement::affinity_json(state.producer_placement.affinity);
 report["producer_priority"]=producer_placement::priority_json(state.producer_placement);
 report["producer_wait_diagnostic"]=wait_diagnostic;
#if defined(PAS_WAIT_DIAGNOSTIC_COMPONENT) && PAS_WAIT_DIAGNOSTIC_COMPONENT
 report["diagnostic_component_identity"]=wait_component_identity();
 report["diagnostic_only"]=true;report["qualification_issued"]=false;
#endif
 report["keyboard_is_test_double"]=true;report["console_registration_is_test_double"]=true;
 report["controlled_stop_key_polls"]=stop_boundary.key_polls;
 report["controlled_console_stop_deliveries"]=stop_boundary.console_deliveries;
 save(root/"report.json",report);std::cout<<report.dump(2)<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
