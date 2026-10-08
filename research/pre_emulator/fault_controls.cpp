#include "frozen_manual_session.cpp"
#include "producer_wait.hpp"
#include "tap_reset_workload.hpp"
#include "streaming_workload.hpp"
#include "pas/result_ui_labels.hpp"
#include "pas/lossless_event_stream.hpp"
#include <psapi.h>
#include <set>

// These symbols replace only emulator boundaries in this test executable.
// No pas_emulator.lib, discovery, ADB, channel creation or sendTouch is linked.
namespace cold {
using J=nlohmann::json;
namespace fs=std::filesystem;
struct Attempt {pas::Nanoseconds due{},render_start{},render_end{},callback_return{};};
struct State {
 std::string scene,scenario;fs::path archive;std::uint64_t releases_before_rpc=0;bool rpc_injected=false;int requested{},cadence{};
 std::atomic<bool> cancelled=false;
 std::vector<Attempt> attempts;
 std::uint64_t downs{},moves{},ups{},releases{},preflights{},discoveries{},constructors{};
 std::optional<std::thread::id> touch_owner;
};
State state;
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
  if(cold::state.scenario=="writer"){
  std::vector<std::filesystem::path> sessions;for(const auto& e:std::filesystem::directory_iterator(cold::state.archive))if(e.is_directory())sessions.push_back(e.path());
  cold::require(sessions.size()==1,"one cold session before deliberate writer failure");
  std::filesystem::create_directory(sessions[0]/"round-1");
  std::filesystem::create_directory(sessions[0]/"round-1"/"events-0.jsonl.gz");
 }
 current_rails::ProducerDeadlineWait wait;const auto epoch=clock_.now_ns();
 std::vector<std::uint8_t> storage(1280*720*3);cold::state.attempts.resize(std::size_t(cold::state.requested)+64);
 for(int i=0;i<cold::state.requested+64&&!stop.stop_requested()&&!cold::state.cancelled;++i){
  if(cold::state.scenario=="capture"&&i==256)throw std::runtime_error("endpoint_free_capture_fault_at_attempt_257");
  auto& a=cold::state.attempts[std::size_t(i)];a.due=epoch+Nanoseconds(i)*cold::state.cadence*1'000'000;wait.wait_until(clock_,a.due);a.render_start=clock_.now_ns();
  if(i>=cold::state.requested)cold::result(storage);
  else if(cold::state.scene=="tap")current_rails::tap_reset_workload::render(storage,std::uint64_t(i+1));
  else if(cold::state.scene=="hold")current_rails::streaming_workload::render(storage,std::uint64_t(i+1));
  else cold::dense(storage);
  a.render_end=clock_.now_ns();Frame f;f.sequence=std::uint64_t(i+1);f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.capture_backend="cold_synthetic_capture_boundary";f.capture_complete_ns=f.pixels_ready_ns=a.render_end;f.rgb=std::move(storage);
  {std::lock_guard lock(mutex_);++stats_.valid;stats_.last_notification_ns=f.capture_complete_ns;}
  on_frame(std::move(f));a.callback_return=clock_.now_ns();storage=std::move(f.rgb);
 }
 while(!cold::state.cancelled&&!stop.stop_requested()){
  if(clock_.now_ns()>epoch+Nanoseconds(cold::state.requested)*cold::state.cadence*1'000'000+2'000'000'000)throw std::runtime_error("cold finite pipeline did not finish from result pixels");
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
}
GrpcTouch::GrpcTouch(const Clock& c,GrpcEndpoint endpoint,PixelCoordinateMap mapping,int width,int height,int max_contacts,std::chrono::milliseconds timeout)
 :clock_(c),endpoint_(std::move(endpoint)),mapping_(std::move(mapping)),width_(width),height_(height),max_contacts_(max_contacts),timeout_(timeout){cold::require(endpoint_.target.empty()&&endpoint_.token.empty()&&max_contacts==5,"no touch endpoint permitted");cold::owner();++cold::state.constructors;}
TouchReceipt GrpcTouch::inject(const TouchCommand& command){
 cold::owner();cold::require(command.contact_id>=0&&command.contact_id<max_contacts_,"contact pool");auto [x,y]=mapping_.map(command.x,command.y);cold::require(x>=0&&y>=0&&x<width_&&y<height_,"mapped geometry");
 const bool exists=positions_.contains(command.contact_id);cold::require(command.phase==Phase::down?!exists:exists,"contact phase ownership");const auto start=clock_.now_ns();
 if(command.phase==Phase::down){++cold::state.downs;positions_[command.contact_id]={x,y};}else if(command.phase==Phase::move){++cold::state.moves;positions_[command.contact_id]={x,y};}else {++cold::state.ups;positions_.erase(command.contact_id);}
  if(cold::state.scenario=="rpc"&&command.phase==Phase::down&&!cold::state.rpc_injected){cold::state.rpc_injected=true;cold::state.releases_before_rpc=cold::state.releases;return {command,start,clock_.now_ns(),false,"unknown_transport_outcome_no_retry"};}
 if(cold::state.scenario=="slow")std::this_thread::sleep_for(std::chrono::milliseconds(12));
 return {command,start,clock_.now_ns(),true,"endpoint_free_cold_effect_unverified"};
}
ReleaseReport GrpcTouch::release_all(){cold::owner();++cold::state.releases;ReleaseReport r;r.start_ns=clock_.now_ns();for(const auto& [id,_]:positions_)r.requested_ids.push_back(id);positions_.clear();r.return_ns=clock_.now_ns();return r;}
ReleaseReport GrpcTouch::emergency_release_all(){return release_all();}
}

int main(int argc,char** argv){try{
 using namespace cold;
 require(argc==3,"usage capture|writer|rpc|slow fresh-directory");state.scenario=argv[1];require(state.scenario=="capture"||state.scenario=="writer"||state.scenario=="rpc"||state.scenario=="slow","finite boundary fault scenario");
 state.scene="hold";state.requested=1000;state.cadence=20;
 const fs::path root=fs::absolute(argv[2]);require(!fs::exists(root),"fresh fault evidence root");fs::create_directory(root);state.archive=root/"archive";
 bool closed=false;try{pas::run_current_candidate_manual_session(pas::CandidateLiveAdmission{},"missing","missing",true);}catch(const std::runtime_error&e){closed=std::string_view(e.what()).starts_with("current candidate live admission UNQUALIFIED:");}
 require(closed&&state.preflights==0&&state.discoveries==0&&state.constructors==0,"real entry closed before boundaries");
 J config={{"schema",2},{"name","endpoint-free literal production fault fixture"},{"serial","emulator-offline-cold"},{"capture",{{"kind","emulator-grpc"},{"execution","thread"},{"transport","payload"},{"image_format","rgb888"},{"row_order","top-down"},{"width",1280},{"height",720},{"source_rotation",1},{"grpc_read_chunk_kib",256},{"max_relative_lag_ms",250}}},{"touch",{{"kind","emulator-grpc"},{"width",720},{"height",1280},{"rotation_deg",90},{"timeout_ms",100},{"max_contacts",5}}},{"scheduler",{{"max_plans",128},{"max_steps",16},{"horizon_ms",350},{"evidence_max_age_ms",100}}},{"game",{{"enabled_types",{"tap","hold","drag","flick"}},{"lead_ms",35},{"uncertainty_ms",30}}},{"preview",{{"hz",0}}},{"log_dir",state.archive.string()}};
 save(root/"profile.json",config);bool threw=false;std::string reason;
 try{pas::run_manual_session_impl((root/"profile.json").string(),"endpoint-free-test-double",true,0,false,false,true,pas::ManualVariant::current_candidate);}catch(const std::exception&e){threw=true;reason=e.what();}
 std::vector<fs::path> sessions;for(const auto&e:fs::directory_iterator(state.archive))if(e.is_directory())sessions.push_back(e.path());require(sessions.size()==1,"one finite fault session");const auto summary=read(sessions[0]/"summary.json");
 const bool expected_fault=state.scenario!="slow";require(threw==expected_fault,"expected fault status must agree with actual return");require(summary.at("state")== (expected_fault?"FAULT":"STOPPED"),"actual supervisor state");
 require(state.touch_owner.has_value(),"sole touch owner observed");
 if(state.scenario=="rpc")require(state.rpc_injected&&state.downs==1&&state.releases==state.releases_before_rpc+1,"unknown Down attempted once and release frozen once");
 if(state.scenario=="writer")require(summary.contains("archive_first_error")&&!summary.at("archive_first_error").get<std::string>().empty(),"sticky actual writer error");
 J attempts=J::array();std::size_t produced=0;
 for(int i=0;i<state.requested;++i){const auto&a=state.attempts.at(std::size_t(i));const bool present=a.render_end!=0;produced+=present;attempts.push_back({{"attempt",i+1},{"due_ns",a.due},{"render_start_ns",a.render_start},{"render_end_ns",a.render_end},{"callback_return_ns",a.callback_return},{"produced",present},{"measurement",i>=256}});}
 save(root/"attempts.json",attempts);
 save(root/"report.json",{{"schema","pas.literal-production-boundary-controls.v1"},{"scenario",state.scenario},{"requested",1000},{"produced",produced},{"not_produced",1000-produced},{"warmup",256},{"measurement_attempts",744},{"cadence_ms",20},{"public_entry_rejected",closed},{"expected_fault",expected_fault},{"actual_threw",threw},{"actual_error",reason},{"summary",summary},{"down_attempts",state.downs},{"move_attempts",state.moves},{"up_attempts",state.ups},{"release_calls",state.releases},{"releases_before_rpc",state.releases_before_rpc},{"single_touch_owner",true},{"source_build_commit",pas_session_build_commit},{"source_build_dirty",pas_session_build_dirty},{"actual_manual_translation_unit_sha256",PAS_COLD_MANUAL_SHA},{"endpoint_free",true},{"device_operations",0},{"physical_qualification",false},{"cost_qualification",false},{"live_ready",false},{"scope","literal manual_session, supervisor, owner, wake and SessionArchive; fault injection only in boundary test doubles; all 1000 planned attempts retained"}});
 std::cout<<state.scenario<<" literal boundary control PASS; live UNQUALIFIED\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}