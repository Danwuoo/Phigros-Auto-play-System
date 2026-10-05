#include "pas/action_wake.hpp"
#include "pas/session_archive.hpp"
#include "pas/session_events.hpp"
#include "pas/game_session.hpp"
#include "meter_touch.hpp"
#include "archive_verify.hpp"
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
DecisionSnapshot owner_stimulus(std::uint64_t seq,Nanoseconds now,Nanoseconds start,std::uint64_t limit=128) {
 DecisionSnapshot s;s.sequence=seq;s.context={1,1,1,seq,now,W,H,1};s.playing_gate=true;s.ui=GameUi::playing;
 auto h=target(NoteKind::hold,now,start+2*ms,1);
 if(seq>1){h.crossing_ns.reset();h.reason="held_body_touch_only";h.note.head_on_line=false;h.note.held_body_evidence=true;}
 const double a=static_cast<double>(seq%16)*.015;
 h.note.tangent={std::cos(a),std::sin(a)};h.note.center=h.hit={500+a*20,500-a*8};
 h.revision=seq;s.targets.push_back(h);
 const auto phase=(seq-1)%16,cycle=(seq-1)/16;
 if(phase<3||phase>=9)for(std::uint64_t k=2;k<=limit;++k) {
  const auto kind=std::array<NoteKind,4>{NoteKind::tap,NoteKind::drag,NoteKind::flick,NoteKind::hold}[k%4];
  auto t=target(kind,now,now+(phase<3?55-static_cast<Nanoseconds>(phase)*8:20)*ms,cycle*128+k);
  t.revision=seq;s.targets.push_back(t);
 }
 return s;
}
json command(const TouchCommand& c) {return {{"intent",c.intent_id},{"contact",c.contact_id},{"phase",static_cast<int>(c.phase)},
 {"x",c.x},{"y",c.y},{"scheduled_ns",c.scheduled_ns},{"source_frame",c.source_frame_sequence}};}
using r1::MeterTouch;
json dist(std::vector<double> v) {
 std::sort(v.begin(),v.end());auto at=[&](double p){return v.empty()?json(nullptr):json(v[std::min(v.size()-1,static_cast<std::size_t>(std::ceil(p*v.size())-1))]);};
 return {{"n",v.size()},{"p50",at(.5)},{"p95",at(.95)},{"p99",at(.99)},{"max",v.empty()?json(nullptr):json(v.back())},
 {"jitter_p95_minus_p5",v.empty()?json(nullptr):json(at(.95).get<double>()-at(.05).get<double>())}};
}
std::uint64_t rss() {PROCESS_MEMORY_COUNTERS c{};c.cb=sizeof(c);if(!K32GetProcessMemoryInfo(GetCurrentProcess(),&c,sizeof(c)))throw std::runtime_error("rss");return c.WorkingSetSize;}
struct Sample {Nanoseconds capture=0,ready=0,published=0,recognition_start=0,recognition_end=0,owner_start=0,owner_end=0,publish_cost=0;
 bool published_ok=false,consumed=false,owned=false;std::size_t lines=0,targets=0;};
void save(const std::filesystem::path& p,const json& j){std::ofstream f(p);if(!f)throw std::runtime_error("output open");f<<j.dump(2)<<'\n';if(!f)throw std::runtime_error("output write");}

json run(const std::string& workload,const std::filesystem::path& out,bool stress) {
 if(workload!="rgb"&&workload!="owner")throw std::invalid_argument("workload");
 if(std::filesystem::exists(out))throw std::runtime_error("existing_output");std::filesystem::create_directories(out);
 const int frames=stress?64:workload=="rgb"?256:128,cadence=stress||workload=="owner"?8:16,slow=stress&&workload=="rgb"?24:0;
 const auto images=pixels();HostClock clock;LatestFrame latest(W,H,3,&clock);Wake wake;
 const auto begin=clock.now_ns();const auto rss_initial=rss();
 SessionArchive archive(out/"archive",{{"mode","offline-X11-P-R1"},{"lead_ms",35},{"real_input",false}},16*1024*1024,1024*1024,{&clock,stress?5:0});
 std::vector<Sample> samples(static_cast<std::size_t>(frames)+1);
 std::atomic<bool> capture_done=false,perception_done=false,failed=false;
 std::mutex mutex,error_mutex;std::shared_ptr<const DecisionSnapshot> decision;
 std::string error;std::uint64_t latest_decision=0;
 std::uint64_t journal_attempts=0,journal_bytes=0;std::map<std::string,std::uint64_t> event_counts;
 std::vector<double> enqueue_cost;enqueue_cost.reserve(16384);
 auto enqueue=[&](json j){const auto start=clock.now_ns();const auto bytes=j.dump().size()+96;
  if(bytes>256*1024||journal_bytes+bytes>static_cast<std::uint64_t>(stress?6:3)*1024*1024||enqueue_cost.size()>=16384)throw std::runtime_error("diagnostic_capacity");
  ++journal_attempts;journal_bytes+=bytes;++event_counts[j.at("event").get<std::string>()];archive.event(1,std::move(j));enqueue_cost.push_back((clock.now_ns()-start)/1e6);};
 auto fail=[&]{try{throw;}catch(const std::exception& e){std::lock_guard lock(error_mutex);if(error.empty())error=e.what();}catch(...){std::lock_guard lock(error_mutex);error="nonstandard_exception";}
  failed=true;capture_done=true;perception_done=true;latest.close();wake.notify();};
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
    auto s=std::make_shared<DecisionSnapshot>(workload=="rgb"?observer.process(*lease):owner_stimulus(seen,lease->capture_complete_ns,begin,stress?128:32));
    if(slow)std::this_thread::sleep_for(std::chrono::milliseconds(slow));
    x.recognition_end=clock.now_ns();s->recognition_start_ns=x.recognition_start;s->recognition_end_ns=x.recognition_end;
    x.lines=s->lines.size();x.targets=s->targets.size();x.consumed=true;lease.reset();
    {std::lock_guard lock(mutex);decision=std::move(s);latest_decision=decision->sequence;}wake.notify();
   }
  }catch(...){fail();}perception_done=true;wake.notify();});
  std::jthread action([&]{SessionGameOwner game(clock,touch,5,{15,35*ms,30*ms});game.start(1);
   std::size_t receipts_seen=0,releases_seen=0;
   auto drain=[&]{for(;receipts_seen<touch.receipts.size();++receipts_seen)enqueue(receipt_json(touch.receipts[receipts_seen],false));
    for(;releases_seen<touch.release_calls.size();++releases_seen){auto j=release_json(touch.release_calls[releases_seen],"offline_backend_call");j["call_index"]=releases_seen;enqueue(std::move(j));}
    if(!game.owner())return;
    auto& owner=*game.owner();auto plans=owner.take_accepted_plans();accepted+=plans.size();for(const auto& p:plans)enqueue(plan_json(p,clock.now_ns(),false));
    auto cov=owner.take_coverage_updates();coverage+=cov.size();for(auto& j:cov)enqueue(std::move(j));
    auto cancels=owner.take_plan_cancellations();canceled+=cancels.size();for(auto& j:cancels){auto k=j.at("reason").get<std::string>();cancel_reasons[k]=cancel_reasons.value(k,0ULL)+1;enqueue(std::move(j));}
    auto notices=owner.scheduler().take_notices();rejected+=notices.size();for(const auto& n:notices){reject_reasons[n.reason]=reject_reasons.value(n.reason,0ULL)+1;
     enqueue({{"event","scheduler_rejection"},{"intent_id",n.intent_id},{"reason",n.reason},{"monotonic_ns",n.monotonic_ns}});}
    pending_peak=std::max(pending_peak,owner.scheduler().pending_count());active_peak=std::max(active_peak,owner.scheduler().active_count());
    if(pending_peak>128||active_peak>5)throw std::runtime_error("scheduler_capacity");
   };
   try{std::uint64_t seen=0;Nanoseconds end_wait=0;
    while(!failed){std::shared_ptr<const DecisionSnapshot> s;{std::lock_guard lock(mutex);if(decision&&decision->sequence>seen)s=decision;}
     if(s){seen=s->sequence;auto& x=samples.at(s->context.frame);
      enqueue(decision_json(*s));enqueue({{"event","lifecycle_ui_evidence"},{"source_frame",seen},{"capture_complete_ns",s->context.capture_ns},
       {"hud",s->playing_gate},{"result_labels",0},{"result_similarity",0},{"source_timestamp_us",nullptr},{"source_sequence",nullptr},{"synthetic",true}});
      x.owner_start=clock.now_ns();game.accept(*s,s->playing_gate);x.owner_end=clock.now_ns();x.owned=true;drain();}
     const auto before=touch.receipts.size();game.poll();if(!s)between+=touch.receipts.size()-before;drain();
     if(!game.scheduler()->fault().empty())throw std::runtime_error(game.scheduler()->fault());
     if(archive.faulted())throw std::runtime_error(archive.error());
     {std::lock_guard lock(mutex);if(perception_done&&seen>=latest_decision){if(!end_wait)end_wait=clock.now_ns();if(clock.now_ns()-end_wait>=100*ms)break;}}
     wake.wait(action_wait_delay(game.scheduler()->next_due_ns(),clock.now_ns()));
    }
    game.finish();drain();
   }catch(...){fail();try{game.finish();drain();}catch(...){fail();}}
  });
 }
 latest.close();try{archive.complete(1,{{"status",error.empty()?"offline_complete":"aborted"}});}catch(...){fail();}archive.close();
 const auto stats=archive.meter_stats();const auto counter=latest.counters();
 std::vector<double> recognition,owner,c2o,publish,lateness,rpc,c2inject,release_latency;std::array<std::vector<double>,3> phase_lateness;
 std::size_t consumed=0,owned=0,target_peak=0,line_peak=0;bool time_order=true;
 std::ofstream raw(out/"frames.jsonl");
 for(int i=1;i<=frames;++i){const auto& x=samples[i];publish.push_back(x.publish_cost/1e6);
  if(x.consumed){++consumed;recognition.push_back((x.recognition_end-x.recognition_start)/1e6);target_peak=std::max(target_peak,x.targets);line_peak=std::max(line_peak,x.lines);
   time_order=time_order&&x.capture<=x.ready&&x.ready<=x.published&&x.published<=x.recognition_start&&x.recognition_start<=x.recognition_end;}
  if(x.owned){++owned;owner.push_back((x.owner_end-x.owner_start)/1e6);c2o.push_back((x.owner_end-x.capture)/1e6);time_order=time_order&&x.recognition_end<=x.owner_start&&x.owner_start<=x.owner_end;}
  auto row=json{{"attempt",i},{"published",x.published_ok},{"consumed",x.consumed},{"owned",x.owned},{"capture_complete_ns",x.capture},{"pixels_ready_ns",x.ready},{"published_ns",x.published},
   {"recognition_start_ns",x.recognition_start},{"recognition_complete_ns",x.recognition_end},{"owner_start_ns",x.owner_start},{"owner_end_ns",x.owner_end},{"publish_cost_ns",x.publish_cost},{"targets",x.targets},{"lines",x.lines}}.dump();
  if(row.size()>1022)throw std::runtime_error("frame row capacity");raw<<row<<'\n';
 }
 std::ofstream receipts(out/"receipts.jsonl");std::array<std::uint64_t,3> phases{};std::uint64_t failed_receipts=0,unknown=0,late=0;
 for(const auto& r:touch.receipts){const auto phase=static_cast<int>(r.command.phase);++phases[phase];if(!r.success)++failed_receipts;if(r.reason.find("unknown")!=std::string::npos)++unknown;
  const auto latency=(r.injection_start_ns-r.command.scheduled_ns)/1e6;lateness.push_back(latency);phase_lateness[phase].push_back(latency);
  rpc.push_back((r.injection_return_ns-r.injection_start_ns)/1e6);if(latency>15)++late;
  auto seq=r.command.source_frame_sequence;if(seq>0&&seq<samples.size())c2inject.push_back((r.injection_start_ns-samples[seq].capture)/1e6);
  auto row=receipt_json(r,false).dump();if(row.size()>510)throw std::runtime_error("receipt row capacity");receipts<<row<<'\n';time_order=time_order&&r.injection_start_ns>=r.command.scheduled_ns&&r.injection_return_ns>=r.injection_start_ns;
 }
 std::ofstream releases(out/"releases.jsonl");std::uint64_t release_requested=0,release_failed=0,release_unknown=0;
 for(std::size_t i=0;i<touch.release_calls.size();++i){const auto& r=touch.release_calls[i];auto j=release_json(r,"offline_backend_call");j["call_index"]=i;auto row=j.dump();if(row.size()>510)throw std::runtime_error("release row capacity");releases<<row<<'\n';
  release_requested+=r.requested_ids.size();release_failed+=r.failed_ids.size();release_unknown+=r.unknown_ids.size();release_latency.push_back((r.return_ns-r.start_ns)/1e6);}
 raw.flush();receipts.flush();releases.flush();bool raw_ok=static_cast<bool>(raw)&&static_cast<bool>(receipts)&&static_cast<bool>(releases);raw.close();receipts.close();releases.close();
 const bool complete=raw_ok&&r1::verify_archive(out/"archive",event_counts)&&stats.written==journal_attempts;
 const bool hard=error.empty()&&complete&&failed_receipts==0&&unknown==0&&release_failed==0&&release_unknown==0&&!touch.release_calls.empty()&&touch.contacts.empty()&&!archive.faulted()&&time_order&&counter.published+counter.pool_drops==static_cast<std::uint64_t>(frames)&&consumed<=counter.published&&owned<=consumed&&target_peak<=128&&line_peak<=16;
 json result={{"workload",workload},{"stress",stress},{"stimulus_sha256",stimulus_hash(images)},{"adapter_contract","same v1 stimulus; normal owner cap32/stress128; live lead35"},
  {"attempts",frames},{"cadence_ms",cadence},{"jitter_offsets_ms",{-2,1,2,-1}},{"consumer_delay_ms",slow},{"fake_rpc_delay_ms",stress?3:0},{"owner_options",{15,35*ms,30*ms}},
  {"published",counter.published},{"pool_drops",counter.pool_drops},{"overwritten",counter.overwritten},{"consumer_skips",counter.consumer_skips},
  {"consumed",consumed},{"owner_seen",owned},{"decision_skips",consumed-owned},{"down",phases[0]},{"move",phases[1]},{"up",phases[2]},
  {"receipt_n",touch.receipts.size()},{"failed_receipts",failed_receipts},{"unknown_receipts",unknown},{"late_over_15ms",late},{"between_frame_receipts",between},{"action_expectation","required"},
  {"release_n",touch.release_calls.size()},{"release_requested_ids",release_requested},{"release_failed_ids",release_failed},{"release_unknown_ids",release_unknown},
  {"accepted_plans",accepted},{"coverage",coverage},{"canceled",canceled},{"cancellation_reasons",cancel_reasons},{"rejected",rejected},{"rejection_reasons",reject_reasons},
  {"contacts_at_exit",touch.contacts.size()},{"contacts_peak",touch.peak},{"active_peak",active_peak},{"pending_peak",pending_peak},{"targets_peak",target_peak},{"lines_peak",line_peak},
  {"journal_attempts",journal_attempts},{"journal_admitted",stats.admitted},{"journal_written",stats.written},{"event_counts",event_counts},{"journal_debug_drops",0},{"journal_discarded_on_fault",stats.discarded_on_fault},{"journal_faulted",archive.faulted()},{"journal_error",archive.error()},
  {"journal_capacity",8192},{"journal_byte_capacity",16*1024*1024},{"journal_queue_peak",archive.peak_queue()},{"journal_queue_byte_peak",stats.queue_byte_peak},{"journal_row_byte_peak",stats.row_byte_peak},
  {"journal_bytes_reserved",journal_bytes},{"journal_serialized_bytes",stats.serialized_bytes},{"writer_adapter","shared production SessionArchive; optional offline aggregate instrumentation; full JSON"},
  {"physical_frame_slots",3},{"latest_capacity",1},{"decision_mailbox_capacity",1},{"sample_capacity",frames+1},{"receipt_capacity",512},{"release_capacity",512},{"archive_sample_capacity",16384},
  {"rss_initial_bytes",rss_initial},{"rss_final_bytes",rss()},{"elapsed_ms",(clock.now_ns()-begin)/1e6},{"qpc_frequency",clock.frequency()},
  {"time_order_valid",time_order},{"worker_error",error},{"raw_complete",complete},{"hard_gate",hard},{"source_render_age","unknown"},{"real_rpc","unknown"},{"game_adoption","unknown"},
  {"metrics_ms",{{"recognition",dist(recognition)},{"owner",dist(owner)},{"capture_to_owner",dist(c2o)},{"publish",dist(publish)},{"lateness",dist(lateness)},
   {"down_lateness",dist(phase_lateness[0])},{"move_lateness",dist(phase_lateness[1])},{"up_lateness",dist(phase_lateness[2])},{"injection_call",dist(rpc)},{"release_call",dist(release_latency)},
   {"event_enqueue",dist(enqueue_cost)},{"writer_serialize",dist(stats.serialize_ms)},{"writer_write",dist(stats.write_ms)},{"capture_to_injection_all_phases",dist(c2inject)}}}};
 save(out/"summary.json",result);return result;
}

// Public-observable bridge: identical source works with frozen research core
// and complete runtime core. No research getter, diagnostic hook or ABI mixing.
class CompareBuffer final:public std::streambuf {
    std::ifstream reference_;std::string row_;std::uint64_t rows_=0,bytes_=0;bool equal_=true;
protected:
    int overflow(int c) override {if(c==traits_type::eof())return traits_type::not_eof(c);char ch=static_cast<char>(c);row_+=ch;
        if(row_.size()>256*1024)throw std::runtime_error("bridge row capacity");
        if(ch=='\n'){std::string old;if(!std::getline(reference_,old)||old+"\n"!=row_)equal_=false;++rows_;bytes_+=row_.size();row_.clear();}return c;}
public:
    explicit CompareBuffer(const std::filesystem::path& p):reference_(p){if(!reference_)throw std::runtime_error("bridge reference missing");}
    json finish(){std::string extra;equal_=equal_&&row_.empty()&&!std::getline(reference_,extra)&&reference_.eof();return {{"equal",equal_},{"rows",rows_},{"bytes",bytes_}};}
};
void bridge(const std::filesystem::path& out,const std::filesystem::path& reference) {
 if(std::filesystem::exists(out))throw std::runtime_error("existing_output");std::filesystem::create_directories(out);
 auto imgs=pixels();FakeClock c;MeterTouch t(c,0);GameObserver observer(c);
 CompareBuffer buffer(reference);std::ostream f(&buffer);
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
 auto comparison=buffer.finish();if(!comparison.at("equal").get<bool>())throw std::runtime_error("bridge bytes differ");save(out/"summary.json",{{"frames",512},{"contacts_at_exit",t.contacts.size()},{"receipts",t.receipts.size()},{"comparison",comparison},{"reference_sha256",sha256_file(reference)},{"stimulus_sha256",stimulus_hash(imgs)}});
}
}
int main(int argc,char** argv) {
 try {
  if(argc==4&&std::string(argv[1])=="bridge"){bridge(argv[2],argv[3]);return 0;}
  if(argc!=5||std::string(argv[1])!="cost")throw std::invalid_argument("usage: x11_cost cost rgb|owner NEW_OUTPUT normal|stress; bridge NEW_OUTPUT");
  const std::string load=argv[4];if(load!="normal"&&load!="stress")throw std::invalid_argument("load");
  const auto r=run(argv[2],argv[3],load=="stress");std::cout<<r.dump()<<'\n';return r.at("hard_gate").get<bool>()?0:2;
 }catch(const std::exception& e){std::cerr<<json{{"error",e.what()}}.dump()<<'\n';return 1;}
}
