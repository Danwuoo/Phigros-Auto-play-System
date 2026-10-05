#include "pas/game.hpp"
#include "pas/journal.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <bcrypt.h>

using namespace pas;
using json=nlohmann::json;
namespace {
constexpr int W=1280,H=720,S=W*3,N=16;
constexpr Nanoseconds ms=1'000'000;
using Pixels=std::array<std::vector<std::uint8_t>,N>;
void rect(std::vector<std::uint8_t>& p,int x,int y,int w,int h,std::array<std::uint8_t,3> c) {
 for(int iy=std::max(0,y);iy<std::min(H,y+h);++iy)for(int ix=std::max(0,x);ix<std::min(W,x+w);++ix)
  std::copy(c.begin(),c.end(),p.data()+static_cast<std::size_t>(iy)*S+ix*3);
}
Pixels pixels() {
 Pixels result;
 for(int i=0;i<N;++i) {
  auto& p=result[i];p.assign(S*H,0);
  rect(p,20,20,6,22,{255,255,255});rect(p,34,20,6,22,{255,255,255});
  for(int d=0;d<6;++d)rect(p,1020+d*24,20,12,20,{255,255,255});
  rect(p,100,498,1080,4,{255,255,255});
  const int y=375+i*10;
  if(i!=7&&i!=8)rect(p,170,y,78,9,{40,190,255});
  rect(p,340,y-40,78,9,{255,220,50});
  rect(p,950,y-20,78,9,{255,70,100});rect(p,977,y-23,24,4,{255,255,255});
  rect(p,560,y-180,4,184,{210,197,146});rect(p,690,y-180,4,184,{210,197,146});
  rect(p,564,y-180,126,4,{210,197,146});rect(p,564,y,126,4,{210,197,146});
 }
 return result;
}
std::string stimulus_hash(const Pixels& p) {
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("hash provider");
 if(BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(alg,0);throw std::runtime_error("hash create");}
 std::array<unsigned char,32> d{};bool ok=true;
 for(const auto& v:p)ok=ok&&BCryptHashData(h,const_cast<PUCHAR>(v.data()),static_cast<ULONG>(v.size()),0)>=0;
 ok=ok&&BCryptFinishHash(h,d.data(),32,0)>=0;BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(alg,0);
 if(!ok)throw std::runtime_error("hash finish");
 std::ostringstream s;for(auto b:d)s<<std::hex<<std::setfill('0')<<std::setw(2)<<static_cast<int>(b);return s.str();
}
GameTarget target(NoteKind kind,Nanoseconds now,Nanoseconds due,std::uint64_t id) {
 GameTarget t;t.note_id=id;t.revision=1;t.evidence_ns=now;t.expires_ns=now+100*ms;
 t.note.kind=kind;t.note.center=t.hit={100.0+static_cast<double>((id%8)*135),500};
 t.note.width=120;t.note.height=kind==NoteKind::hold?300:8;
 t.crossing_ns=due;t.reason="prediction_observe_only";t.uncertainty_ns=2*ms;t.samples=4;t.line_id=7;
 if(kind==NoteKind::hold)t.note.rails_geometry=t.note.head_on_line=true;return t;
}
DecisionSnapshot owner_stimulus(std::uint64_t seq,Nanoseconds now,Nanoseconds start) {
 DecisionSnapshot s;s.sequence=seq;s.context={1,1,1,seq,now,W,H,1};s.playing_gate=true;s.ui=GameUi::playing;
 auto h=target(NoteKind::hold,now,start+2*ms,1);
 if(seq>1){h.crossing_ns.reset();h.reason="held_body_touch_only";h.note.head_on_line=false;h.note.held_body_evidence=true;}
 const double a=static_cast<double>(seq%16)*.015;
 h.note.tangent={std::cos(a),std::sin(a)};h.note.center=h.hit={500+a*20,500-a*8};
 h.revision=seq;s.targets.push_back(h);
 const auto phase=(seq-1)%16,cycle=(seq-1)/16;
 if(phase<3||phase>=9)for(std::uint64_t k=2;k<=128;++k) {
  const auto kind=std::array<NoteKind,4>{NoteKind::tap,NoteKind::drag,NoteKind::flick,NoteKind::hold}[k%4];
  auto t=target(kind,now,now+(phase<3?55-static_cast<Nanoseconds>(phase)*8:20)*ms,cycle*128+k);
  t.revision=seq;s.targets.push_back(t);
 }
 return s;
}
json command(const TouchCommand& c) {return {{"intent",c.intent_id},{"contact",c.contact_id},{"phase",static_cast<int>(c.phase)},
 {"x",c.x},{"y",c.y},{"scheduled_ns",c.scheduled_ns},{"source_frame",c.source_frame_sequence}};}
class MeterTouch final:public TouchBackend {
public:
 const Clock& clock;int delay;std::map<int,std::array<double,2>> contacts;std::vector<TouchReceipt> receipts;
 std::size_t peak=0;std::uint64_t releases=0;ReleaseReport last;
 MeterTouch(const Clock& c,int d):clock(c),delay(d){receipts.reserve(32768);}
 TouchReceipt inject(const TouchCommand& c) override {
  if(receipts.size()>=32768)throw std::runtime_error("receipt_capacity");
  if(c.contact_id<0||c.contact_id>=5)throw std::runtime_error("contact_capacity");
  if((c.phase==Phase::down)==contacts.contains(c.contact_id))throw std::runtime_error("contact_lifecycle");
  const auto begin=clock.now_ns();
  if(delay)std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  if(c.phase==Phase::up)contacts.erase(c.contact_id);else contacts[c.contact_id]={c.x,c.y};
  peak=std::max(peak,contacts.size());TouchReceipt r{c,begin,clock.now_ns(),true,"fake_success"};receipts.push_back(r);return r;
 }
 ReleaseReport release_all() override {
  last={};last.start_ns=clock.now_ns();for(const auto& [id,p]:contacts){(void)p;last.requested_ids.push_back(id);}
  contacts.clear();last.return_ns=clock.now_ns();++releases;return last;
 }
};
json dist(std::vector<double> v) {
 std::sort(v.begin(),v.end());auto at=[&](double p){return v.empty()?json(nullptr):json(v[std::min(v.size()-1,static_cast<std::size_t>(std::ceil(p*v.size())-1))]);};
 return {{"n",v.size()},{"p50",at(.5)},{"p95",at(.95)},{"p99",at(.99)},{"max",v.empty()?json(nullptr):json(v.back())},
 {"jitter_p95_minus_p5",v.empty()?json(nullptr):json(at(.95).get<double>()-at(.05).get<double>())}};
}
std::uint64_t rss() {PROCESS_MEMORY_COUNTERS c{};c.cb=sizeof(c);if(!K32GetProcessMemoryInfo(GetCurrentProcess(),&c,sizeof(c)))throw std::runtime_error("rss");return c.WorkingSetSize;}
struct Sample {Nanoseconds capture=0,ready=0,published=0,recognition_start=0,recognition_end=0,owner_start=0,owner_end=0,publish_cost=0;
 bool published_ok=false,consumed=false,owned=false;std::size_t lines=0,targets=0;};
// Harness-only writer load: C36h's production Journal has no delay hook.
// The queue has both row and byte bounds; it never feeds the core policy.
class BoundedWriter {
 std::size_t cap_;int delay_;std::mutex mutex_;std::condition_variable cv_;std::deque<std::string> queue_;
 std::size_t bytes_=0;std::atomic<std::size_t> peak_=0,byte_peak_=0;std::atomic<std::uint64_t> drops_=0;
 std::atomic<bool> fault_=false;bool closed_=false;std::jthread worker_;
public:
 BoundedWriter(const std::filesystem::path& path,std::size_t cap,int delay):cap_(cap),delay_(delay) {
  worker_=std::jthread([this,path]{std::ofstream f(path);if(!f){fault_=true;return;}
   while(true){std::string row;{std::unique_lock lock(mutex_);cv_.wait(lock,[&]{return closed_||!queue_.empty();});
    if(queue_.empty()&&closed_)break;row=std::move(queue_.front());queue_.pop_front();bytes_-=row.size();}
    if(delay_)std::this_thread::sleep_for(std::chrono::milliseconds(delay_));f<<row<<'\n';if(!f){fault_=true;return;}
   }f.flush();if(!f)fault_=true;
  });
 }
 bool push(json j,bool) {auto row=j.dump();std::lock_guard lock(mutex_);
  if(closed_||fault_)return false;if(queue_.size()>=cap_||bytes_+row.size()>16*1024*1024){++drops_;return false;}
  bytes_+=row.size();queue_.push_back(std::move(row));peak_=std::max(peak_.load(),queue_.size());byte_peak_=std::max(byte_peak_.load(),bytes_);cv_.notify_one();return true;
 }
 void close(){ {std::lock_guard lock(mutex_);closed_=true;}cv_.notify_one();if(worker_.joinable())worker_.join();}
 ~BoundedWriter(){close();}
 bool faulted() const {return fault_;}std::uint64_t debug_drops() const {return drops_;}
 std::size_t peak() const {return peak_;}std::size_t byte_peak() const {return byte_peak_;}
};
void save(const std::filesystem::path& p,const json& j){std::ofstream f(p);if(!f)throw std::runtime_error("output open");f<<j.dump(2)<<'\n';if(!f)throw std::runtime_error("output write");}

json run(const std::string& workload,const std::filesystem::path& out,bool stress) {
 if(workload!="rgb"&&workload!="owner")throw std::invalid_argument("workload");
 if(std::filesystem::exists(out))throw std::runtime_error("existing_output");std::filesystem::create_directories(out);
 const int frames=stress?256:512,cadence=stress||workload=="owner"?8:16,slow=stress&&workload=="rgb"?24:0;
 const auto images=pixels();HostClock clock;LatestFrame latest(W,H,3,&clock);
 const auto begin=clock.now_ns();const auto rss_initial=rss();
 BoundedWriter journal(out/"journal.jsonl",stress?8:256,stress?5:0);
 std::vector<Sample> samples(static_cast<std::size_t>(frames)+1);
 std::atomic<bool> capture_done=false,perception_done=false,failed=false;
 std::mutex mutex,error_mutex;std::condition_variable cv;std::shared_ptr<const DecisionSnapshot> decision;
 std::string error;std::uint64_t latest_decision=0;
 std::atomic<std::uint64_t> journal_attempts=0,journal_rejected=0,journal_bytes=0;
 auto enqueue=[&](json j){const auto bytes=j.dump().size()+128;if(bytes>65536||journal_bytes.fetch_add(bytes)+bytes>16*1024*1024)throw std::runtime_error("diagnostic_capacity");
  ++journal_attempts;if(!journal.push(std::move(j),false))++journal_rejected;};
 auto fail=[&]{try{throw;}catch(const std::exception& e){std::lock_guard lock(error_mutex);if(error.empty())error=e.what();}catch(...){std::lock_guard lock(error_mutex);error="nonstandard_exception";}
  failed=true;capture_done=true;perception_done=true;latest.close();cv.notify_all();};
 MeterTouch touch(clock,stress?3:0);std::size_t pending_peak=0,active_peak=0;std::uint64_t accepted=0,canceled=0,rejected=0,coverage=0,between=0;
 json cancel_reasons=json::object(),reject_reasons=json::object();
 {
  std::jthread capture([&]{try {
   const auto wall=std::chrono::steady_clock::now();
   for(int i=1;i<=frames&&!failed;++i){const int jitter=std::array<int,4>{-2,1,2,-1}[(i-1)%4];
    std::this_thread::sleep_until(wall+std::chrono::milliseconds((i-1)*cadence+jitter+2));
    Frame m;m.sequence=i;m.epoch=m.generation=m.geometry_version=1;m.width=W;m.height=H;m.stride=S;m.source_rotation=1;
    m.source_pixel_format="RGB888_top_down";m.capture_backend="synthetic_memory";m.capture_complete_ns=clock.now_ns();m.pixels_ready_ns=m.capture_complete_ns;
    auto& x=samples[i];x.capture=m.capture_complete_ns;x.ready=m.pixels_ready_ns;const auto p=clock.now_ns();
    x.published_ok=latest.publish(images[(i-1)%16].data(),S*H,m);x.publish_cost=clock.now_ns()-p;
   }
  }catch(...){fail();}capture_done=true;});
  std::jthread perception([&]{try {
   GameObserver observer(clock);std::uint64_t seen=0;
   while(!failed){auto lease=latest.read_after(seen,10*ms);if(!lease){if(capture_done)break;continue;}
    seen=lease->sequence;auto& x=samples.at(seen);x.published=lease->published_ns;x.recognition_start=clock.now_ns();
    auto s=std::make_shared<DecisionSnapshot>(workload=="rgb"?observer.process(*lease):owner_stimulus(seen,lease->capture_complete_ns,begin));
    if(slow)std::this_thread::sleep_for(std::chrono::milliseconds(slow));
    x.recognition_end=clock.now_ns();s->recognition_start_ns=x.recognition_start;s->recognition_end_ns=x.recognition_end;
    x.lines=s->lines.size();x.targets=s->targets.size();x.consumed=true;lease.reset();
    enqueue({{"event","consumed"},{"frame",seen},{"capture_complete_ns",x.capture},{"pixels_ready_ns",x.ready},{"published_ns",x.published},{"recognition_complete_ns",x.recognition_end}});
    {std::lock_guard lock(mutex);decision=std::move(s);latest_decision=decision->sequence;}cv.notify_one();
   }
  }catch(...){fail();}perception_done=true;cv.notify_one();});
  std::jthread action([&]{GamePlanOwner owner(clock,touch,5,{15,0,30*ms});
   auto drain=[&]{auto plans=owner.take_accepted_plans();accepted+=plans.size();auto cov=owner.take_coverage_updates();coverage+=cov.size();
    auto cancels=owner.take_plan_cancellations();canceled+=cancels.size();for(const auto& j:cancels){auto k=j.at("reason").get<std::string>();cancel_reasons[k]=cancel_reasons.value(k,0ULL)+1;}
    auto notices=owner.scheduler().take_notices();rejected+=notices.size();for(const auto& n:notices)reject_reasons[n.reason]=reject_reasons.value(n.reason,0ULL)+1;
    pending_peak=std::max(pending_peak,owner.scheduler().pending_count());active_peak=std::max(active_peak,owner.scheduler().active_count());
    if(pending_peak>128||active_peak>5)throw std::runtime_error("scheduler_capacity");
    if(!plans.empty()||!cov.empty()||!cancels.empty()||!notices.empty())enqueue({{"event","diagnostic_drain"},{"accepted",plans.size()},{"coverage",cov.size()},{"canceled",cancels.size()},{"rejected",notices.size()}});
   };
   try{std::uint64_t seen=0;Nanoseconds end_wait=0;
    while(!failed){std::shared_ptr<const DecisionSnapshot> s;
     {std::unique_lock lock(mutex);cv.wait_for(lock,std::chrono::milliseconds(1),[&]{return failed||perception_done||(decision&&decision->sequence>seen);});
      if(decision&&decision->sequence>seen)s=decision;}
     if(s){seen=s->sequence;auto& x=samples.at(s->context.frame);x.owner_start=clock.now_ns();owner.accept(*s);x.owner_end=clock.now_ns();x.owned=true;
      enqueue({{"event","owner"},{"frame",s->context.frame},{"owner_start_ns",x.owner_start},{"owner_end_ns",x.owner_end}});drain();}
     const auto before=touch.receipts.size();owner.poll();if(!s)between+=touch.receipts.size()-before;drain();
     {std::lock_guard lock(mutex);if(perception_done&&seen>=latest_decision){if(!end_wait)end_wait=clock.now_ns();if(clock.now_ns()-end_wait>=100*ms)break;}}
     // Avoid the done predicate spinning while draining between-frame work.
     if(perception_done)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    owner.stop();drain();
   }catch(...){fail();owner.stop();}
  });
 }
 latest.close();journal.close();const auto counter=latest.counters();
 std::vector<double> recognition,owner,c2o,publish,lateness,rpc,c2inject;
 std::size_t consumed=0,owned=0,target_peak=0,line_peak=0;bool time_order=true;
 std::ofstream raw(out/"frames.jsonl");
 for(int i=1;i<=frames;++i){const auto& x=samples[i];publish.push_back(x.publish_cost/1e6);
  if(x.consumed){++consumed;recognition.push_back((x.recognition_end-x.recognition_start)/1e6);target_peak=std::max(target_peak,x.targets);line_peak=std::max(line_peak,x.lines);
   time_order=time_order&&x.capture<=x.ready&&x.ready<=x.published&&x.published<=x.recognition_start&&x.recognition_start<=x.recognition_end;}
  if(x.owned){++owned;owner.push_back((x.owner_end-x.owner_start)/1e6);c2o.push_back((x.owner_end-x.capture)/1e6);time_order=time_order&&x.recognition_end<=x.owner_start&&x.owner_start<=x.owner_end;}
  raw<<json{{"attempt",i},{"published",x.published_ok},{"consumed",x.consumed},{"owned",x.owned},{"capture_complete_ns",x.capture},{"pixels_ready_ns",x.ready},{"published_ns",x.published},
   {"recognition_start_ns",x.recognition_start},{"recognition_complete_ns",x.recognition_end},{"owner_start_ns",x.owner_start},{"owner_end_ns",x.owner_end},{"publish_cost_ns",x.publish_cost},{"targets",x.targets},{"lines",x.lines}}.dump()<<'\n';
 }
 std::ofstream receipts(out/"receipts.jsonl");std::array<std::uint64_t,3> phases{};std::uint64_t failed_receipts=0,unknown=0,late=0;
 for(const auto& r:touch.receipts){++phases[static_cast<int>(r.command.phase)];if(!r.success)++failed_receipts;if(r.reason.find("unknown")!=std::string::npos)++unknown;
  lateness.push_back((r.injection_start_ns-r.command.scheduled_ns)/1e6);rpc.push_back((r.injection_return_ns-r.injection_start_ns)/1e6);if(r.injection_start_ns-r.command.scheduled_ns>15*ms)++late;
  auto seq=r.command.source_frame_sequence;if(seq>0&&seq<samples.size())c2inject.push_back((r.injection_start_ns-samples[seq].capture)/1e6);
  receipts<<json{{"command",command(r.command)},{"injection_start_ns",r.injection_start_ns},{"injection_return_ns",r.injection_return_ns},{"success",r.success},{"reason",r.reason}}.dump()<<'\n';
 }
 raw.close();receipts.close();
 const bool hard=error.empty()&&failed_receipts==0&&unknown==0&&touch.contacts.empty()&&!journal.faulted()&&time_order&&counter.published+counter.pool_drops==static_cast<std::uint64_t>(frames)&&consumed<=counter.published&&owned<=consumed&&target_peak<=128&&line_peak<=16;
 json result={{"workload",workload},{"stress",stress},{"stimulus_sha256",stimulus_hash(images)},{"adapter_contract","owner_stimulus v1; source hash in source binding"},
  {"attempts",frames},{"cadence_ms",cadence},{"jitter_offsets_ms",{-2,1,2,-1}},{"consumer_delay_ms",slow},{"fake_rpc_delay_ms",stress?3:0},
  {"published",counter.published},{"pool_drops",counter.pool_drops},{"overwritten",counter.overwritten},{"consumer_skips",counter.consumer_skips},
  {"consumed",consumed},{"owner_seen",owned},{"decision_skips",consumed-owned},{"down",phases[0]},{"move",phases[1]},{"up",phases[2]},
  {"receipt_n",touch.receipts.size()},{"failed_receipts",failed_receipts},{"unknown_receipts",unknown},{"late_over_15ms",late},{"between_frame_receipts",between},
  {"accepted_plans",accepted},{"coverage",coverage},{"canceled",canceled},{"cancellation_reasons",cancel_reasons},{"rejected",rejected},{"rejection_reasons",reject_reasons},
  {"contacts_at_exit",touch.contacts.size()},{"contacts_peak",touch.peak},{"active_peak",active_peak},{"pending_peak",pending_peak},{"targets_peak",target_peak},{"lines_peak",line_peak},
  {"journal_attempts",journal_attempts.load()},{"journal_rejected",journal_rejected.load()},{"journal_debug_drops",journal.debug_drops()},{"journal_faulted",journal.faulted()},
  {"journal_capacity",stress?8:256},{"journal_queue_peak",journal.peak()},{"journal_queue_byte_peak",journal.byte_peak()},
  {"writer_adapter","harness BoundedWriter; production SessionArchive/Journal not benchmarked"},{"journal_bytes_reserved",journal_bytes.load()},{"journal_file_bytes",std::filesystem::file_size(out/"journal.jsonl")},
  {"physical_frame_slots",3},{"latest_capacity",1},{"decision_mailbox_capacity",1},{"sample_capacity",frames+1},{"receipt_capacity",32768},
  {"rss_initial_bytes",rss_initial},{"rss_final_bytes",rss()},{"elapsed_ms",(clock.now_ns()-begin)/1e6},{"qpc_frequency",clock.frequency()},
  {"time_order_valid",time_order},{"worker_error",error},{"hard_gate",hard},{"source_render_age","unknown"},{"real_rpc","unknown"},{"game_adoption","unknown"},
  {"metrics_ms",{{"recognition",dist(recognition)},{"owner",dist(owner)},{"capture_to_owner",dist(c2o)},{"publish",dist(publish)},{"lateness",dist(lateness)},{"injection_call",dist(rpc)},{"capture_to_injection_all_phases",dist(c2inject)}}}};
 save(out/"summary.json",result);return result;
}

// Public-observable bridge: identical source works with frozen research core
// and complete runtime core. No research getter, diagnostic hook or ABI mixing.
void bridge(const std::filesystem::path& out) {
 if(std::filesystem::exists(out))throw std::runtime_error("existing_output");std::filesystem::create_directories(out);
 auto imgs=pixels();FakeClock c;MeterTouch t(c,0);GameObserver observer(c);
 std::ofstream f(out/"public-events.jsonl");
 for(int mode=0;mode<2;++mode){GamePlanOwner o(c,t,5,{15,0,30*ms});observer.reset();
  for(std::uint64_t i=1;i<=256;++i){const auto now=static_cast<Nanoseconds>(mode*10000+i*16)*ms;c.set(now);
   DecisionSnapshot s;if(mode==0){Frame p;p.sequence=i;p.epoch=p.generation=p.geometry_version=1;p.width=W;p.height=H;p.stride=S;p.source_rotation=1;
    p.capture_complete_ns=p.pixels_ready_ns=p.published_ns=now;p.rgb=imgs[(i-1)%16];s=observer.process(p);}
   else s=owner_stimulus(i,now,10016*ms);
   f<<json{{"event","scene"},{"mode",mode},{"frame",i},{"value",decision_json(s)}}.dump()<<'\n';
   o.accept(s);o.poll();
   for(auto p:o.take_accepted_plans())f<<json{{"event","plan"},{"intent",p.intent_id},{"revision",p.revision},{"prefix_offset",p.prefix_offset},{"steps",p.steps.size()},{"source_frame",p.source_frame_sequence}}.dump()<<'\n';
   for(auto j:o.take_coverage_updates())f<<j.dump()<<'\n';for(auto j:o.take_plan_cancellations())f<<j.dump()<<'\n';
   for(const auto& n:o.scheduler().take_notices())f<<json{{"event","notice"},{"intent",n.intent_id},{"reason",n.reason},{"at",n.monotonic_ns}}.dump()<<'\n';
  }o.stop();
 }
 for(const auto& r:t.receipts)f<<json{{"event","receipt"},{"command",command(r.command)},{"start",r.injection_start_ns},{"return",r.injection_return_ns},{"success",r.success}}.dump()<<'\n';
 f.close();save(out/"summary.json",{{"frames",512},{"contacts_at_exit",t.contacts.size()},{"receipts",t.receipts.size()},{"events_sha256",sha256_file(out/"public-events.jsonl")},{"stimulus_sha256",stimulus_hash(imgs)}});
}
}
int main(int argc,char** argv) {
 try {
  if(argc==3&&std::string(argv[1])=="bridge"){bridge(argv[2]);return 0;}
  if(argc!=5||std::string(argv[1])!="cost")throw std::invalid_argument("usage: x11_cost cost rgb|owner NEW_OUTPUT normal|stress; bridge NEW_OUTPUT");
  const std::string load=argv[4];if(load!="normal"&&load!="stress")throw std::invalid_argument("load");
  const auto r=run(argv[2],argv[3],load=="stress");std::cout<<r.dump()<<'\n';return r.at("hard_gate").get<bool>()?0:2;
 }catch(const std::exception& e){std::cerr<<json{{"error",e.what()}}.dump()<<'\n';return 1;}
}
