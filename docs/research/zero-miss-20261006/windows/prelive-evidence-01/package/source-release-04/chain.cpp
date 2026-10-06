#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <psapi.h>
#include <wrl/client.h>
#include "runtime.hpp"
#include "pas/journal.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace pas;using namespace pas::current_rails;using J=nlohmann::json;using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT c){if(FAILED(c))throw std::runtime_error("WIC decode");}
J distribution(std::vector<double> v){if(v.empty())return {{"n",0}};std::sort(v.begin(),v.end());auto p=[&](double x){double a=(v.size()-1)*x;auto lo=std::size_t(a),hi=std::min(lo+1,v.size()-1);return v[lo]+(v[hi]-v[lo])*(a-lo);};return {{"n",v.size()},{"p50",p(.5)},{"p95",p(.95)},{"p99",p(.99)},{"max",v.back()},{"jitter_p95_minus_p5",p(.95)-p(.05)}};}
std::uint64_t rss(){PROCESS_MEMORY_COUNTERS p{};p.cb=sizeof(p);if(!K32GetProcessMemoryInfo(GetCurrentProcess(),&p,sizeof(p)))throw std::runtime_error("RSS");return p.WorkingSetSize;}
Frame load(IWICImagingFactory* factory,const J& input){
 const std::filesystem::path path=input.at("path").get<std::string>();ComPtr<IWICBitmapDecoder> decoder;checked(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder));ComPtr<IWICBitmapFrameDecode> bitmap;checked(decoder->GetFrame(0,&bitmap));UINT w,h;checked(bitmap->GetSize(&w,&h));if(w!=1280||h!=720)throw std::runtime_error("pixel profile");ComPtr<IWICFormatConverter> converter;checked(factory->CreateFormatConverter(&converter));checked(converter->Initialize(bitmap.Get(),GUID_WICPixelFormat24bppRGB,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
 const auto& i=input.at("index");Frame f;f.epoch=f.generation=f.geometry_version=1;f.sequence=i.at("source_frame");f.capture_complete_ns=i.at("capture_complete_ns");f.pixels_ready_ns=i.at("pixels_ready_ns");f.width=w;f.height=h;f.stride=3840;f.source_rotation=i.at("source_rotation");f.rgb.resize(1280*720*3);checked(converter->CopyPixels(nullptr,3840,static_cast<UINT>(f.rgb.size()),f.rgb.data()));return f;
}
J encode(const Evaluation& e){J rows=J::array();for(std::size_t k=0;k<e.count;++k){const auto& s=e.support[k];rows.push_back({{"note",s.note},{"candidate",s.candidate},{"line",s.line},{"front",{s.front.x,s.front.y}},{"hit",{s.hit.x,s.hit.y}},{"terminal",s.terminal},{"body",s.body},{"rows",s.rows},{"effect",s.effect},{"unique",s.unique},{"own_active",s.active},{"samples",s.samples},{"accepted",s.accepted},{"tail",s.tail},{"reason",s.reason}});}return rows;}
J pixels(const std::string& selection,const std::string& report){
 std::ifstream in(selection);const auto input=J::parse(in);const auto& frames=input.at("frames");if(frames.empty()||frames.size()>7722)throw std::runtime_error("selection bound");
 checked(CoInitializeEx(nullptr,COINIT_MULTITHREADED));ComPtr<IWICImagingFactory> factory;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
 FakeClock business;HostClock meter;std::unique_ptr<Runtime> rt;LatestFrame latest(1280,720,3,&meter);
 std::ofstream trace(report+".rows.jsonl");std::vector<double> times;J runs=J::array();std::uint64_t down=0,move=0,up=0,raw=0,accepted=0,unknown=0,terminal=0,body=0;std::size_t processed=0,bytes=0;int window=-1;bool released=true;std::string stop;
 auto close=[&](){if(!rt)return;released&=rt->finish();down+=rt->backend.downs;move+=rt->backend.moves;up+=rt->backend.ups;raw+=rt->raw;accepted+=rt->accepted;unknown+=rt->unknown;terminal+=rt->terminals;body+=rt->bodies;runs.push_back({{"window",window},{"down",rt->backend.downs},{"move",rt->backend.moves},{"up",rt->backend.ups},{"ledger_size",rt->ledger.size()},{"ledger_valid",rt->ledger.valid()},{"reason",rt->ledger.reason()}});};
 for(const auto& item:frames){const int next=item.at("window");if(window!=next){close();window=next;rt=std::make_unique<Runtime>(business);}
  auto f=load(factory.Get(),item);business.set(f.pixels_ready_ns);
  const auto start=meter.now_ns();if(!latest.publish(f.rgb.data(),f.rgb.size(),f))throw std::runtime_error("unexpected pixel pool drop");auto lease=latest.read_after(f.sequence-1,0);if(!lease||lease->sequence!=f.sequence)throw std::runtime_error("same lease");
  const bool ok=rt->dispatch(*lease);const auto complete=meter.now_ns();times.push_back((complete-start)/1e6);
  J row={{"ordinal",item.at("index").at("ordinal")},{"window",window},{"source_frame",f.sequence},{"capture",f.capture_complete_ns},{"pixels_ready",f.pixels_ready_ns},{"complete_ms",times.back()},{"valid",rt->last.valid},{"accepted",rt->last.accepted},{"current_support",encode(rt->last)},{"downs_total",rt->backend.downs},{"moves_total",rt->backend.moves},{"ups_total",rt->backend.ups}};
  const auto text=row.dump();bytes+=text.size()+1;if(bytes>16*1024*1024)throw std::runtime_error("pixel trace cap");trace<<text<<'\n';++processed;
  if(!ok){stop=rt->ledger.reason();break;}
 }
 close();latest.close();trace.flush();if(!trace)throw std::runtime_error("pixel trace write");
 return {{"schema","pas.prelive-current-pixels.v1"},{"frames",frames.size()},{"processed",processed},{"windows",runs},{"raw_proposals",raw},{"accepted",accepted},{"unknown",unknown},{"current_terminals",terminal},{"current_bodies",body},{"own_down",down},{"own_move",move},{"own_up",up},{"release_verified",released},{"stop",stop},{"compute_ms",distribution(times)},{"timing_domain","host_QPC; decode excluded; includes latest-frame copy/read+observer+hook+owner+receipt"},{"business_domain","archived QPC offline FakeClock"},{"trace_bytes",bytes},{"physical_gold",0},{"game_adoption","unknown"},{"device_endpoints",0}};
}
void box(std::vector<std::uint8_t>& p,int x0,int y0,int x1,int y1,std::array<std::uint8_t,3> c){for(int y=std::max(0,y0);y<std::min(720,y1);++y)for(int x=std::max(0,x0);x<std::min(1280,x1);++x)std::copy(c.begin(),c.end(),p.begin()+std::size_t(y)*3840+x*3);}
std::array<std::vector<std::uint8_t>,12> stimulus(std::string_view scene){std::array<std::vector<std::uint8_t>,12> images;for(int k=0;k<12;++k){auto& p=images[k];p.assign(1280*720*3,30);box(p,20,20,26,42,{255,255,255});box(p,34,20,40,42,{255,255,255});for(int d=0;d<6;++d)box(p,1020+d*24,20,1032+d*24,40,{255,255,255});
 if(scene=="dense"){for(int row=0;row<16;++row){const int y=150+row*31;box(p,40,y-1,1240,y+2,{255,255,255});for(int col=0;col<8;++col){const int x=190+col*130;box(p,x-39,y-12,x+39,y-3,{40,190,255});}}}
 else {box(p,115,499,1165,502,{255,255,255});const int y=440+k*8;
  if(scene=="hold"){box(p,350,y-100,451,y+1,{155,233,255});box(p,350,y-100,352,y+1,{255,255,255});box(p,449,y-100,451,y+1,{255,255,255});}
  else box(p,491,y-4,569,y+5,{40,190,255});}
 }return images;}
struct Sample {Nanoseconds capture=0,published=0,recognition=0,complete=0;bool published_ok=false,consumed=false,writer=false;};
J pipeline(std::string mode,std::string scene,int n,const std::string& report){
 if(n<1000||n>10000||(mode!="A"&&mode!="B")||(scene!="tap"&&scene!="hold"&&scene!="dense"&&scene!="slow"&&scene!="writer"&&scene!="rpc"&&scene!="fault"))throw std::runtime_error("pipeline bounds");
 const auto images=stimulus(scene);HostClock clock;LatestFrame latest(1280,720,3,&clock);Runtime rt(clock);
 const int writer_capacity=scene=="writer"?2:8192;Journal journal(report+".journal.jsonl",writer_capacity,[&]{if(scene=="writer")std::this_thread::sleep_for(std::chrono::milliseconds(30));});
 std::vector<Sample> samples(n+1);std::atomic<bool> done=false;std::atomic<int> publish_attempts=0;
 if(scene=="rpc")rt.backend.delay_ms=5;
 if(scene=="fault")rt.backend.failure=current_execution::ReplayBackend::Failure::unknown_down;
 std::vector<double> injection,late,evidence_to_injection;std::size_t receipt_count=0,release_calls=0;
 rt.receipt_output=[&](const auto& event){
  if(event.release){++release_calls;journal.push({{"event","release_receipt"},{"requested",event.report.requested_ids},{"failed",event.report.failed_ids},{"unknown",event.report.unknown_ids},{"start",event.report.start_ns},{"return",event.report.return_ns}});return;}
  const auto& r=event.receipt;const auto& c=event.issued;
  if(receipt_count>=20000)throw std::runtime_error("receipt sample capacity");++receipt_count;
  injection.push_back((r.injection_return_ns-r.injection_start_ns)/1e6);late.push_back((r.injection_start_ns-c.scheduled_ns)/1e6);
  if(c.source_frame_sequence<samples.size()&&samples[c.source_frame_sequence].capture)evidence_to_injection.push_back((r.injection_start_ns-samples[c.source_frame_sequence].capture)/1e6);
  journal.push({{"event","own_injection_receipt"},{"intent",c.intent_id},{"source_frame",c.source_frame_sequence},{"contact",c.contact_id},{"phase",static_cast<int>(c.phase)},{"scheduled",c.scheduled_ns},{"injection_start",r.injection_start_ns},{"injection_return",r.injection_return_ns},{"success",r.success},{"reason",r.reason}});
 };
 const auto begin=clock.now_ns();std::uint64_t initial_rss=rss(),peak_rss=initial_rss;std::size_t consumed=0,invalid=0;std::atomic<bool> safe=true;std::string stop;
 // Latest-only producer uses an 8ms cadence; no waiting for the consumer.
 std::jthread producer([&]{auto start=std::chrono::steady_clock::now();try{for(int i=1;i<=n;++i){std::this_thread::sleep_until(start+std::chrono::milliseconds((i-1)*8));Frame f;f.sequence=i;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.capture_backend="offline_memory_stub";f.capture_complete_ns=f.pixels_ready_ns=clock.now_ns();samples[i].capture=f.capture_complete_ns;samples[i].published_ok=latest.publish(images[(i-1)%12].data(),1280*720*3,f);++publish_attempts;}}catch(...){safe=false;}done=true;});
 std::uint64_t seen=0;while(true){auto f=latest.read_after(seen,10'000'000);if(!f){if(done)break;continue;}seen=f->sequence;auto& s=samples[seen];s.consumed=true;s.published=f->published_ns;
  if(scene=="slow")std::this_thread::sleep_for(std::chrono::milliseconds(12));
  if(!rt.dispatch(*f,mode=="B")){safe=false;stop=rt.ledger.reason();}
  s.recognition=clock.now_ns();++consumed;invalid+=mode=="B"&&!rt.last.valid;
  // Diagnostics carry joined frame/intent receipts and stay one-way.
  s.writer=journal.push({{"event","complete_current_chain"},{"frame",seen},{"capture",f->capture_complete_ns},{"pixels_ready",f->pixels_ready_ns},{"published",f->published_ns},{"owner_complete",s.recognition},{"downs",rt.backend.downs},{"moves",rt.backend.moves},{"ups",rt.backend.ups}},scene!="writer");
  s.complete=clock.now_ns();if(consumed%100==0)peak_rss=std::max(peak_rss,rss());
 }
 producer.join();const auto counters=latest.counters();latest.close();const auto release_start=clock.now_ns();const bool released=rt.finish();const auto release_end=clock.now_ns();journal.close();const auto end=clock.now_ns();
 std::vector<double> total,recognition,publish,all_attempt_latency;J raw=J::array();std::size_t writer_ok=0,not_consumed=0;
 for(int i=1;i<=n;++i){const auto& s=samples[i];if(s.consumed){total.push_back((s.complete-s.capture)/1e6);recognition.push_back((s.recognition-s.capture)/1e6);publish.push_back((s.published-s.capture)/1e6);writer_ok+=s.writer;}else ++not_consumed;
  raw.push_back({{"attempt",i},{"capture",s.capture},{"published_ok",s.published_ok},{"consumed",s.consumed},{"published",s.published},{"owner_complete",s.recognition},{"journal_enqueue_complete",s.complete},{"writer_ok",s.writer}});}
 std::ofstream rows(report+".attempts.json");rows<<raw.dump()<<'\n';if(!rows)throw std::runtime_error("attempt write");
 J result={{"schema","pas.prelive-complete-chain-cost.v1"},{"mode",mode},{"scene",scene},{"attempts",publish_attempts.load()},{"requested",n},{"consumed",consumed},{"owner_seen",consumed},{"owner_seen_fraction",double(consumed)/n},{"published",counters.published},{"pool_drops",counters.pool_drops},{"consumer_skips",counters.consumer_skips},{"unconsumed",not_consumed},{"writer_ok",writer_ok},{"writer_drops",journal.debug_drops()},{"writer_fault",journal.faulted()},{"raw_proposals",rt.raw},{"eligible",rt.accepted},{"unknown",rt.unknown},{"invalid",invalid},{"own_down",rt.backend.downs},{"own_move",rt.backend.moves},{"own_up",rt.backend.ups},{"injection_failed",rt.backend.failed},{"safe",safe.load()},{"stop",stop},{"release_verified",released},{"release_ms",(release_end-release_start)/1e6},{"capture_to_owner_journal_ms",distribution(total)},{"capture_to_owner_ms",distribution(recognition)},{"capture_to_publish_ms",distribution(publish)},{"wall_ms",(end-begin)/1e6},{"rss_initial",initial_rss},{"rss_peak_sampled",peak_rss},{"rss_sampling_frames",100},{"pool_slots",3},{"latest_capacity",1},{"writer_capacity",writer_capacity},{"sample_storage_bound",n+1},{"hook_storage_bytes",Hook::storage_bytes()},{"clock_domain","host_QPC"},{"source_absolute_age",nullptr},{"device_endpoints",0},{"transport","offline FakeTouch receipts; no real capture/RPC cost"},{"physical_opportunities",nullptr},{"runtime_cost_gate","NOT_READY until frozen AA/AB and complete safety review"}};
 result["injection_ms"]=distribution(injection);result["all_phase_lateness_ms"]=distribution(late);result["evidence_to_injection_ms"]=distribution(evidence_to_injection);result["receipt_count"]=receipt_count;result["release_calls"]=release_calls;return result;
}
}
int main(int argc,char**argv){try{if(argc<4)throw std::runtime_error("usage pixels selection report OR pipeline A/B scene n report");const std::string report=argv[argc-1];if(std::filesystem::exists(report)||std::filesystem::exists(report+".journal.jsonl"))throw std::runtime_error("fresh report");J result;
 if(std::string_view(argv[1])=="pixels"&&argc==4)result=pixels(argv[2],report);else if(std::string_view(argv[1])=="pipeline"&&argc==6)result=pipeline(argv[2],argv[3],std::stoi(argv[4]),report);else throw std::runtime_error("offline CLI only; no endpoint argument");
 std::ofstream out(report);out<<result.dump(2)<<'\n';if(!out)return 2;std::cout<<result.at("schema")<<'\n';return result.value("release_verified",false)?0:1;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
