#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
using J=nlohmann::json;
namespace fs=std::filesystem;
using Ns=std::int64_t;
constexpr Ns million=1000000;
constexpr std::size_t frame_limit=2560,event_limit=65536,output_limit=16*1024*1024;
void need(bool p,const std::string& m){if(!p)throw std::runtime_error(m);}
struct Hasher{
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
 Hasher(){need(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA provider");need(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)>=0,"SHA create");}
 ~Hasher(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}
 void add(const char* p,std::size_t n){need(BCryptHashData(h,reinterpret_cast<PUCHAR>(const_cast<char*>(p)),static_cast<ULONG>(n),0)>=0,"SHA update");}
 std::string finish(){std::array<unsigned char,32>d{};need(BCryptFinishHash(h,d.data(),32,0)>=0,"SHA finish");std::ostringstream s;for(auto b:d)s<<std::hex<<std::setw(2)<<std::setfill('0')<<int(b);return s.str();}
};
std::string hash(const fs::path& p){std::ifstream f(p,std::ios::binary);need(bool(f),"hash open "+p.string());Hasher h;std::array<char,65536>b{};while(f){f.read(b.data(),b.size());h.add(b.data(),static_cast<std::size_t>(f.gcount()));}need(f.eof()&&!f.bad(),"hash read");return h.finish();}
J read(const fs::path& p){need(fs::file_size(p)<=2*1024*1024,"metadata bound");std::ifstream f(p,std::ios::binary);need(bool(f),"metadata open");return J::parse(f);}
template<class F>void rows(std::istream& f,std::size_t count,std::size_t bytes,F emit){std::string s;s.reserve(std::min<std::size_t>(bytes,4096));char c;std::size_t n=0;while(f.get(c)){if(c=='\n'){need(++n<=count,"row count");if(!s.empty()&&s.back()=='\r')s.pop_back();need(!s.empty(),"empty row");emit(J::parse(s));s.clear();}else{need(s.size()<bytes,"row bytes");s+=c;}}need(f.eof()&&!f.bad(),"row read");need(s.empty(),"truncated row");}
template<class F>void rows(const fs::path& p,std::size_t count,std::size_t bytes,F emit){std::ifstream f(p,std::ios::binary);need(bool(f),"rows open "+p.string());rows(f,count,bytes,emit);}
Ns integer(const J& j,const char* k){const auto& v=j.at(k);need(v.is_number_integer(),std::string("integer ")+k);need(!v.is_number_unsigned()||v.get<std::uint64_t>()<=std::uint64_t(INT64_MAX),"integer overflow");const auto x=v.get<Ns>();need(x>=0,"negative time/count");return x;}
struct Frame{
 std::size_t a=0,targets=0,lines=0;bool published=false,consumed=false,owned=false;
 Ns capture=0,ready=0,pub=0,rs=0,re=0,os=0,oe=0,pc=0;
};
Frame frame(const J& j,std::size_t expected){Frame x;x.a=static_cast<std::size_t>(integer(j,"attempt"));need(x.a==expected,"frame sequence");x.targets=integer(j,"targets");x.lines=integer(j,"lines");need(x.targets<=128&&x.lines<=16,"objects bound");
 x.published=j.at("published").get<bool>();x.consumed=j.at("consumed").get<bool>();x.owned=j.at("owned").get<bool>();need(x.owned<=x.consumed&&x.consumed<=x.published,"frame state");
 x.capture=integer(j,"capture_complete_ns");x.ready=integer(j,"pixels_ready_ns");x.pub=integer(j,"published_ns");x.rs=integer(j,"recognition_start_ns");x.re=integer(j,"recognition_complete_ns");x.os=integer(j,"owner_start_ns");x.oe=integer(j,"owner_end_ns");x.pc=integer(j,"publish_cost_ns");
 need(x.capture>0&&x.ready>=x.capture,"capture time");
 if(x.consumed)need(x.pub>=x.ready&&x.rs>=x.pub&&x.re>=x.rs,"recognition time");else need(x.pub==0&&x.rs==0&&x.re==0&&x.targets==0&&x.lines==0,"unconsumed fields not unknown");
 if(x.owned)need(x.os>=x.re&&x.oe>=x.os,"owner time");else need(x.os==0&&x.oe==0,"unowned fields not unknown");return x;
}
int window(std::size_t a){return a<=256?0:1+int((a-257)/576);}
J wname(int w){return w==0?J("warmup"):J("block"+std::to_string(w));}
J nullable(Ns x){return x?J(x):J(nullptr);}
J brief(const Frame& x){return {{"attempt",x.a},{"published",x.published},{"consumed",x.consumed},{"owned",x.owned},{"capture_complete_ns",x.capture},{"pixels_ready_ns",x.ready},{"published_ns",nullable(x.pub)},{"recognition_start_ns",nullable(x.rs)},{"recognition_complete_ns",nullable(x.re)},{"owner_start_ns",nullable(x.os)},{"owner_end_ns",nullable(x.oe)},{"publish_cost_ns",x.pc},{"targets",x.consumed?J(x.targets):J(nullptr)},{"lines",x.consumed?J(x.lines):J(nullptr)}};}
J dist(std::vector<double> v){for(auto x:v)need(std::isfinite(x)&&x>=0,"metric sample");std::sort(v.begin(),v.end());auto at=[&](double p)->J{return v.empty()?J(nullptr):J(v.at(static_cast<std::size_t>(std::ceil(p*v.size()))-1));};return {{"n",v.size()},{"p50",at(.5)},{"p95",at(.95)},{"p99",at(.99)},{"max",at(1)},{"jitter_p95_minus_p5",v.empty()?J(nullptr):J(at(.95).get<double>()-at(.05).get<double>())}};}
struct Bounds{Ns lo=0,hi=0;};
Bounds publication(const Frame& x,const Frame* next){need(x.consumed,"publication for unconsumed");if(next)need(next->rs>=x.re,"sequential perception");return {x.re,next?next->rs:0};}
J bounds(Bounds b){return {{"lower_ns",b.lo},{"upper_ns",nullable(b.hi)},{"measured_point",false}};}
J relation(const Frame& s,const Frame* p,const Frame* next,const Frame* next2){
 J j={{"causal_attribution","Unknown"},{"completion_relation_to_previous_accept","no_previous_owned"},{"publication_definitely_after_previous_accept",false},{"publication_definitely_before_previous_accept_start",false},{"entire_opportunity_enclosed_by_previous_accept",false}};
 if(!p)return j;
 j["completion_relation_to_previous_accept"]=s.re<p->os?"before_start":s.re>p->oe?"after_end":"inside_closed_interval";
 j["publication_definitely_after_previous_accept"]=s.re>p->oe;
 j["publication_definitely_before_previous_accept_start"]=next&&next->rs<p->os;
 // The next consumed publication replaces s; upper bound of that publication
 // requires the following consumed recognition_start, not its own completion.
 j["entire_opportunity_enclosed_by_previous_accept"]=next&&next2&&s.re>=p->os&&next2->rs<=p->oe;
 j["exploratory_replacement_definitely_before_previous_accept_start"]=next&&next2&&next2->rs<p->os;
 return j;
}
struct Timing{std::size_t anchor=0,raw_anchor=0;double enqueue=0,serial=0,write=0;std::string kind;std::size_t bytes=0;};
struct Edge{std::size_t p=0,q=0,skips=0;Ns gap=0;double enqueue=0,calls=0,residual=0;std::size_t en=0;};
struct Run{
 std::string name,load;fs::path root;J summary;std::vector<Frame> f;std::vector<std::size_t> consumed,owned;
 std::vector<J> receipts,releases;std::vector<Timing> timings;std::vector<std::vector<std::size_t>> anchored;std::vector<Edge> edges;
 std::vector<std::size_t> ci,prev_c,next_c,prev_o,next_o;double cadence=0;
};
void anchor_check(std::size_t seq,std::size_t a,std::size_t raw,std::size_t previous_seq,std::size_t previous_frame,const std::vector<Frame>& f){need(a>previous_frame&&a<f.size()&&seq>previous_seq&&seq==raw&&f[a].owned,"decision anchor/order");}
std::pair<double,std::size_t> contained_enqueue(const std::vector<Timing>& t,const std::vector<std::size_t>& p,const std::vector<std::size_t>& q){need(p.size()>=2&&q.size()>=2,"enqueue containment count");double sum=0;std::size_t n=0;for(std::size_t i=2;i<p.size();++i){sum+=t.at(p[i]).enqueue;++n;}sum+=t.at(q[0]).enqueue+t.at(q[1]).enqueue;n+=2;return {sum,n};}
J check_manifest(const fs::path& m){auto j=read(m);need(j.at("schema")==1,"manifest schema");need(j.at("files").size()<=512,"input file count");std::set<std::string> seen;std::uint64_t total=0;for(const auto& x:j.at("files")){fs::path p=x.at("path").get<std::string>();need(p.is_absolute()&&seen.insert(p.lexically_normal().generic_string()).second,"manifest path/duplicate");auto n=integer(x,"bytes");need(n<=1024LL*1024*1024,"input file bound");need(fs::file_size(p)==std::uint64_t(n)&&hash(p)==x.at("sha256").get<std::string>(),"input SHA/length "+p.string());total+=n;}need(total<=2ULL*1024*1024*1024,"total input bound");return j;}
Run load_run(const fs::path& root,const std::string& name){Run r;r.name=name;r.root=root/name;r.summary=read(r.root/"summary.json");auto& s=r.summary;r.load=s.at("workload");r.cadence=s.at("cadence_ms");need(!s.at("stress").get<bool>()&&s.at("attempts")==2560&&s.at("worker_error")==""&&s.at("raw_complete")==true,"normal complete input");
 std::uint64_t bytes=0;for(const auto& x:fs::recursive_directory_iterator(r.root))if(x.is_regular_file())bytes+=x.file_size();need(bytes<=80ULL*1024*1024,"run bytes");r.f.push_back({});
 rows(r.root/"frames.jsonl",frame_limit,1023,[&](const J& j){auto x=frame(j,r.f.size());if(r.f.size()>1)need(x.capture>r.f.back().capture,"capture increasing");r.f.push_back(x);if(x.consumed)r.consumed.push_back(x.a);if(x.owned)r.owned.push_back(x.a);});
 need(r.f.size()==2561&&r.consumed.size()==s.at("consumed")&&r.owned.size()==s.at("owner_seen"),"frame denominators");need(s.at("published")==2560&&s.at("pool_drops")==0,"published denominator");for(std::size_t i=1;i<r.f.size();++i)need(r.f[i].published,"all published");need(s.at("decision_skips")==r.consumed.size()-r.owned.size()&&s.at("consumer_skips")==2560-r.consumed.size(),"skip denominator");
 r.ci.assign(2561,0);r.prev_c.assign(2561,0);r.next_c.assign(2561,0);r.prev_o.assign(2561,0);r.next_o.assign(2561,0);
 std::size_t pc=0,po=0;for(std::size_t i=1;i<=2560;++i){r.prev_c[i]=pc;r.prev_o[i]=po;if(r.f[i].consumed)pc=i;if(r.f[i].owned)po=i;}
 pc=po=0;for(std::size_t i=2560;i>0;--i){r.next_c[i]=pc;r.next_o[i]=po;if(r.f[i].consumed)pc=i;if(r.f[i].owned)po=i;}
 for(std::size_t i=0;i<r.consumed.size();++i){r.ci[r.consumed[i]]=i;if(i)need(r.f[r.consumed[i]].rs>=r.f[r.consumed[i-1]].re,"perception order");}
 for(std::size_t i=1;i<r.owned.size();++i)need(r.f[r.owned[i]].os>=r.f[r.owned[i-1]].oe,"action order");
 Ns last_call=0;rows(r.root/"receipts.jsonl",8192,511,[&](const J& j){auto a=integer(j,"source_frame");need(a>0&&a<=2560&&r.f[a].owned,"receipt owned join");auto start=integer(j,"injection_start_ns"),end=integer(j,"injection_return_ns"),due=integer(j,"scheduled_ns");need(start>=due&&end>=start&&start>=last_call,"receipt time order");last_call=end;need(j.at("phase").get<int>()>=0&&j.at("phase").get<int>()<=2,"phase");r.receipts.push_back(j);});
 last_call=0;rows(r.root/"releases.jsonl",8192,511,[&](const J& j){need(integer(j,"call_index")==r.releases.size(),"release index");auto a=integer(j,"offline_attempt"),start=integer(j,"start_ns"),end=integer(j,"return_ns");need(a>0&&a<=2560&&end>=start&&start>=last_call,"release anchor/time");last_call=end;r.releases.push_back(j);});
 need(r.receipts.size()==s.at("receipt_n")&&r.releases.size()==s.at("release_n"),"action denominators");
 r.anchored.resize(2561);std::size_t last_anchor=0;rows(r.root/"event-timings.jsonl",event_limit,255,[&](const J& j){need(integer(j,"event_index")==r.timings.size(),"event index");Timing t;t.raw_anchor=integer(j,"attempt");need(t.raw_anchor>=last_anchor&&t.raw_anchor>0&&t.raw_anchor<=2560,"timing raw anchor");last_anchor=t.raw_anchor;auto val=[&](const char* k){need(j.at(k).is_number(),"unknown duration");double x=j.at(k);need(std::isfinite(x)&&x>=0,"invalid duration");return x;};t.enqueue=val("event_enqueue");t.serial=val("writer_serialize");t.write=val("writer_write");r.timings.push_back(t);});
 auto complete=read(r.root/"archive/round-1/summary.json");need(complete.at("status")=="offline_complete"&&!complete.value("partial",false),"archive complete");need(hash(r.root/"archive/round-1/manifest.json")==complete.at("manifest_sha256").get<std::string>(),"archive manifest SHA");auto segs=complete.at("event_segments");need(!segs.empty()&&segs.size()<=32,"segment count");
 std::size_t ev=0,ri=0,li=0,current_sequence=0,current_frame=0;std::uint64_t logical=0,physical=0;std::map<std::string,std::size_t> counts;std::set<std::size_t> decisions;
 for(std::size_t si=0;si<segs.size();++si){const auto& seg=segs[si];auto name2=seg.at("path").get<std::string>();need(name2=="events-"+std::to_string(si)+".jsonl","segment path/order");auto p=r.root/"archive/round-1"/name2;need(hash(p)==seg.at("sha256").get<std::string>(),"segment SHA");physical+=fs::file_size(p);
  rows(p,event_limit,256*1024,[&](J j){need(ev<r.timings.size()&&ev<event_limit,"archive count");need(j.at("schema_version")==2&&j.at("clock_domain")=="host_qpc_ns"&&j.at("round_id")==1,"event envelope");auto& t=r.timings[ev++];t.kind=j.at("event");t.bytes=j.dump().size()+1;logical+=t.bytes;++counts[t.kind];j.erase("schema_version");j.erase("clock_domain");j.erase("round_id");
   if(t.kind=="game_decision"){auto a=static_cast<std::size_t>(integer(j,"frame_sequence")),seq=static_cast<std::size_t>(integer(j,"sequence"));anchor_check(seq,a,t.raw_anchor,current_sequence,current_frame,r.f);need(decisions.insert(a).second,"duplicate decision");current_frame=a;current_sequence=seq;need(j.at("targets").size()==r.f[a].targets&&j.at("capture_complete_ns")==r.f[a].capture&&j.at("recognition_start_ns")==r.f[a].rs&&j.at("recognition_end_ns")==r.f[a].re,"decision archive join");}
   need(current_frame>0&&t.raw_anchor==current_sequence,"event current sequence");t.anchor=current_frame;r.anchored[t.anchor].push_back(ev-1);
   if(t.kind=="game_touch_receipt")need(ri<r.receipts.size()&&j==r.receipts[ri++],"archive receipt content");
   if(t.kind=="game_release_report")need(li<r.releases.size()&&j.at("offline_attempt")==current_sequence&&j==r.releases[li++],"archive release content/anchor");
  });
 }
 need(decisions.size()==r.owned.size()&&ev==r.timings.size()&&ev==s.at("journal_written")&&ev==s.at("journal_attempts")&&s.at("journal_admitted")==ev+1,"event denominators");need(ri==r.receipts.size()&&li==r.releases.size()&&J(counts)==s.at("event_counts"),"archive denominators");need(logical==s.at("journal_serialized_bytes")&&logical==s.at("journal_bytes_reserved")&&(physical==logical||physical==logical+ev),"archive bytes");
 for(auto a:r.owned){const auto& ids=r.anchored[a];need(ids.size()>=2&&r.timings[ids[0]].kind=="game_decision"&&r.timings[ids[1]].kind=="lifecycle_ui_evidence","two preaccept events");for(std::size_t k=2;k<ids.size();++k)need(r.timings[ids[k]].kind!="game_decision"&&r.timings[ids[k]].kind!="lifecycle_ui_evidence","anchor event ordering");}
 for(std::size_t i=1;i<r.owned.size();++i){Edge e;e.p=r.owned[i-1];e.q=r.owned[i];e.gap=r.f[e.q].os-r.f[e.p].oe;
  for(std::size_t a=e.p+1;a<e.q;++a)e.skips+=r.f[a].consumed&&!r.f[a].owned;
  auto [service,n]=contained_enqueue(r.timings,r.anchored[e.p],r.anchored[e.q]);e.enqueue=service;e.en=n;
  const auto lo=r.f[e.p].oe,hi=r.f[e.q].os;std::vector<std::pair<Ns,Ns>> calls;
  for(const auto& j:r.receipts){auto b=integer(j,"injection_start_ns"),end=integer(j,"injection_return_ns");if(b>=lo&&end<=hi)calls.emplace_back(b,end);}
  for(const auto& j:r.releases){auto b=integer(j,"start_ns"),end=integer(j,"return_ns");if(b>=lo&&end<=hi)calls.emplace_back(b,end);}
  std::sort(calls.begin(),calls.end());Ns last=lo;for(auto [b,end]:calls){need(b>=last,"call overlap");last=end;e.calls+=(end-b)/1e6;}
  e.residual=e.gap/1e6-e.enqueue-e.calls;need(e.residual>=-0.000001,"service exceeds enclosing gap");e.residual=std::max(0.0,e.residual);r.edges.push_back(e);
 }
 return r;
}
struct Output{fs::path root;std::size_t bytes=0;explicit Output(fs::path p):root(std::move(p)){need(!fs::exists(root),"existing output");fs::create_directory(root);}
 void write(const std::string& name,const std::string& s){need(s.size()<=output_limit-bytes,"output capacity");need(!fs::exists(root/name),"existing output file");std::ofstream f(root/name,std::ios::binary);need(bool(f),"output open");f<<s;f.flush();need(bool(f),"output write");bytes+=s.size();}
 void json(const std::string& name,const J& j){write(name,j.dump(2)+"\n");}
};
J edge_json(const Run& r,const Edge& e){return {{"run",r.name},{"previous_owned",e.p},{"next_owned",e.q},{"decision_skips",e.skips},{"attempt_distance",e.q-e.p},{"previous_window",wname(window(e.p))},{"previous_phase",(e.p-1)%16},{"previous_targets",r.f[e.p].targets},{"next_targets",r.f[e.q].targets},{"gap_start_ns",r.f[e.p].oe},{"gap_end_ns",r.f[e.q].os},{"gap_ms",e.gap/1e6},{"source_contained_enqueue_n",e.en},{"source_contained_enqueue_ms",e.enqueue},{"contained_fake_calls_ms",e.calls},{"unmetered_remainder_ms",e.residual},{"writer_added_to_action",false},{"causal_attribution","Unknown"}};}
const Edge* match(const Run& r,const Edge& e){const Edge* best=nullptr;std::size_t distance=SIZE_MAX;for(const auto& c:r.edges){if(c.skips||window(c.p)!=window(e.p)||(c.p-1)%16!=(e.p-1)%16||r.f[c.p].targets!=r.f[e.p].targets)continue;const auto d=c.p>e.p?c.p-e.p:e.p-c.p;if(d<distance||(d==distance&&(!best||c.p<best->p))){distance=d;best=&c;}}return best;}
J skip_row(const Run& r,std::size_t a,bool decision){const auto& x=r.f[a];auto pc=r.prev_c[a],nc=r.next_c[a],po=r.prev_o[a],no=r.next_o[a];auto get=[&](std::size_t i){return i?brief(r.f[i]):J(nullptr);};
 J j={{"run",r.name},{"kind",decision?"decision_skip":"consumer_skip"},{"attempt",a},{"window",wname(window(a))},{"measurement",a>256},{"typed_phase",(a-1)%16},{"frame",brief(x)},{"previous_consumed",get(pc)},{"next_consumed",get(nc)},{"previous_owned",get(po)},{"next_owned",get(no)},{"capture_previous_gap_ms",a>1?J((x.capture-r.f[a-1].capture)/1e6):J(nullptr)},{"capture_next_gap_ms",a<2560?J((r.f[a+1].capture-x.capture)/1e6):J(nullptr)},
 {"unknown",{"exact_decision_publication","exact_selection","lock_wait","scheduler_due","wake_reason","OS_scheduling","individual_enqueue_absolute_start_end","writer_absolute_start_end","causal_attribution"}}};
 if(decision){j["publication_bounds"]=bounds(publication(x,nc?&r.f[nc]:nullptr));j["previous_recognition_complete_gap_ms"]=pc?J((x.re-r.f[pc].re)/1e6):J(nullptr);j["next_recognition_complete_gap_ms"]=nc?J((r.f[nc].re-x.re)/1e6):J(nullptr);j["next_recognition_cluster"]=nc?J((r.f[nc].re-x.re)/1e6<=r.cadence/2):J(nullptr);auto nc2=nc?r.next_c[nc]:0;j["evidence"]=relation(x,po?&r.f[po]:nullptr,nc?&r.f[nc]:nullptr,nc2?&r.f[nc2]:nullptr);
  j["next_publication_bounds"]=nc?bounds(publication(r.f[nc],nc2?&r.f[nc2]:nullptr)):J(nullptr);
  j["decision_residency_duration_bounds_ns"]={{"lower",nc?J(r.f[nc].re-r.f[nc].rs):J(nullptr)},{"upper",nc2?J(r.f[nc2].rs-x.re):J(nullptr)}};
  j["previous_selection_bounds"]=po?bounds({std::max(r.f[po].re,r.prev_o[po]?r.f[r.prev_o[po]].oe:Ns(0)),r.f[po].os}):J(nullptr);
  j["next_selection_bounds"]=no?bounds({std::max(r.f[no].re,po?r.f[po].oe:Ns(0)),r.f[no].os}):J(nullptr);
 }else{j["publication_bounds"]=nullptr;j["recognition_interval"]=nullptr;j["causal_attribution"]="Unknown";}
 return j;
}
J metrics(const std::map<std::string,std::vector<double>>& m){J j=J::object();for(const auto& [k,v]:m)j[k]=dist(v);return j;}
J run_report(const Run& r){
 J result={{"run",r.name},{"workload",r.load},{"cadence_ms",r.cadence},{"cluster_cutoff_ms",r.cadence/2},{"windows",J::object()},{"phase_strata",J::array()},{"causal_attribution","Unknown"},{"event_partition_policy","resolved source attempt via archive sequence->frame_sequence; raw anchor is accepted decision sequence, not necessarily frame"},{"capture_first_ns",r.f[1].capture},{"capture_last_ns",r.f[2560].capture},{"capture_span_ms",(r.f[2560].capture-r.f[1].capture)/1e6},{"original_action_denominators",J::object()}};
 for(auto k:{"receipt_n","down","move","up","failed_receipts","unknown_receipts","release_n","release_requested_ids","release_failed_ids","release_unknown_ids","journal_written","journal_attempts","journal_admitted","contacts_at_exit"})result["original_action_denominators"][k]=r.summary.at(k);
 std::array<std::pair<std::size_t,std::size_t>,7> ranges{{{1,2560},{1,256},{257,2560},{257,832},{833,1408},{1409,1984},{1985,2560}}};std::array<std::string,7> names{{"all","warmup","measurement","block1","block2","block3","block4"}};
 for(std::size_t wi=0;wi<ranges.size();++wi){auto [lo,hi]=ranges[wi];J w={{"first_attempt",lo},{"last_attempt",hi},{"attempts",hi-lo+1}};std::size_t c=0,o=0,cs=0,ds=0,ca_cluster=0,rec_cluster=0,skip_next_cluster=0,owned_next_cluster=0,skip_next_n=0,owned_next_n=0;std::map<std::string,std::vector<double>> m;
  for(auto k:{"capture_gap_ms","published_consumed_gap_ms","recognition_complete_gap_ms","recognition_ms","accept_ms","capture_to_owner_ms","owned_start_gap_ms","owned_sequence_gap","nonaccept_gap_ms","contained_enqueue_ms","contained_fake_calls_ms","unmetered_remainder_ms","nonaccept_skip_gap_ms","nonaccept_nonskip_gap_ms","skip_next_recognition_gap_ms","owned_next_recognition_gap_ms","event_enqueue_anchor_ms","writer_serialize_anchor_ms","writer_write_anchor_ms"})m[k];
  J relations={{"before_start",0},{"inside_closed_interval",0},{"after_end",0},{"no_previous_owned",0},{"publication_definitely_after_previous_accept",0},{"publication_definitely_before_previous_accept_start",0},{"entire_opportunity_enclosed_by_previous_accept",0},{"exploratory_replacement_definitely_before_previous_accept_start",0}};
  for(std::size_t a=lo;a<=hi;++a){const auto& x=r.f[a];c+=x.consumed;o+=x.owned;cs+=!x.consumed;ds+=x.consumed&&!x.owned;
   if(a>1){double gap=(x.capture-r.f[a-1].capture)/1e6;m["capture_gap_ms"].push_back(gap);ca_cluster+=gap<=r.cadence/2;}
   if(x.consumed){m["recognition_ms"].push_back((x.re-x.rs)/1e6);auto pc=r.prev_c[a],nc=r.next_c[a];if(pc){m["published_consumed_gap_ms"].push_back((x.pub-r.f[pc].pub)/1e6);double gap=(x.re-r.f[pc].re)/1e6;m["recognition_complete_gap_ms"].push_back(gap);rec_cluster+=gap<=r.cadence/2;}
    if(nc){double gap=(r.f[nc].re-x.re)/1e6;m[x.owned?"owned_next_recognition_gap_ms":"skip_next_recognition_gap_ms"].push_back(gap);if(x.owned){++owned_next_n;owned_next_cluster+=gap<=r.cadence/2;}else{++skip_next_n;skip_next_cluster+=gap<=r.cadence/2;}}
    if(!x.owned){auto p=r.prev_o[a],nc2=nc?r.next_c[nc]:0;auto ev=relation(x,p?&r.f[p]:nullptr,nc?&r.f[nc]:nullptr,nc2?&r.f[nc2]:nullptr);auto k=ev.at("completion_relation_to_previous_accept").get<std::string>();relations[k]=relations[k].get<std::size_t>()+1;for(auto key:{"publication_definitely_after_previous_accept","publication_definitely_before_previous_accept_start","entire_opportunity_enclosed_by_previous_accept","exploratory_replacement_definitely_before_previous_accept_start"})relations[key]=relations[key].get<std::size_t>()+ev.at(key).get<bool>();}
   }
   if(x.owned){m["accept_ms"].push_back((x.oe-x.os)/1e6);m["capture_to_owner_ms"].push_back((x.oe-x.capture)/1e6);auto po=r.prev_o[a];if(po){m["owned_start_gap_ms"].push_back((x.os-r.f[po].os)/1e6);m["owned_sequence_gap"].push_back(double(a-po));}}
  }
  for(const auto& e:r.edges)if(e.q>=lo&&e.q<=hi){m["nonaccept_gap_ms"].push_back(e.gap/1e6);m["contained_enqueue_ms"].push_back(e.enqueue);m["contained_fake_calls_ms"].push_back(e.calls);m["unmetered_remainder_ms"].push_back(e.residual);m[e.skips?"nonaccept_skip_gap_ms":"nonaccept_nonskip_gap_ms"].push_back(e.gap/1e6);}
  for(const auto& t:r.timings)if(t.anchor>=lo&&t.anchor<=hi){m["event_enqueue_anchor_ms"].push_back(t.enqueue);m["writer_serialize_anchor_ms"].push_back(t.serial);m["writer_write_anchor_ms"].push_back(t.write);}
  w.update({{"published",hi-lo+1},{"consumed",c},{"owned",o},{"consumer_skips",cs},{"decision_skips",ds},{"capture_cluster_n",ca_cluster},{"recognition_cluster_n",rec_cluster},{"skip_with_next_n",skip_next_n},{"skip_next_cluster_n",skip_next_cluster},{"owned_with_next_n",owned_next_n},{"owned_next_cluster_n",owned_next_cluster},{"skip_evidence",relations},{"metrics",metrics(m)}});
  if(wi<3){const auto& old=r.summary.at("windows").at(names[wi]);need(old.at("attempts")==w.at("attempts")&&old.at("consumed")==c&&old.at("owner_seen")==o&&old.at("decision_skips")==ds,"window denominator reconciliation");}else{const auto& old=r.summary.at("windows").at("blocks").at(wi-3);need(old.at("consumed")==c&&old.at("owner_seen")==o&&old.at("decision_skips")==ds,"block reconciliation");}
  result["windows"][names[wi]]=w;
 }
 for(std::size_t phase=0;phase<16;++phase){J x={{"phase",phase},{"attempts",160}};std::size_t c=0,o=0;for(std::size_t a=phase+1;a<=2560;a+=16){c+=r.f[a].consumed;o+=r.f[a].owned;}x.update({{"consumed",c},{"owned",o},{"consumer_skips",160-c},{"decision_skips",c-o}});result["phase_strata"].push_back(x);}
 std::map<std::size_t,std::size_t> consumed_streaks,attempt_streaks;std::size_t streak=0;for(auto a:r.consumed){if(!r.f[a].owned)++streak;else if(streak){++consumed_streaks[streak];streak=0;}}if(streak)++consumed_streaks[streak];streak=0;for(std::size_t a=1;a<=2560;++a){if(r.f[a].consumed&&!r.f[a].owned)++streak;else if(streak){++attempt_streaks[streak];streak=0;}}if(streak)++attempt_streaks[streak];
 auto histogram=[](const auto& h){J j=J::array();for(auto [len,n]:h)j.push_back({{"length",len},{"streaks",n}});return j;};result["consecutive_skipped_consumed"]=histogram(consumed_streaks);result["consecutive_skipped_attempts"]=histogram(attempt_streaks);
 return result;
}
void timeline(Output& out,const Run& r,const Edge& e,const std::string& tag){auto lo=r.f[e.p].rs,hi=r.f[e.q].oe;double span=(hi-lo)/1e6;need(span>0,"timeline span");auto xx=[&](Ns t){return 210.0+double(t-lo)/double(hi-lo)*950;};std::ostringstream s;s<<std::fixed<<std::setprecision(3);
 s<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1240\" height=\""<<(270+45*e.skips)<<"\" viewBox=\"0 0 1240 "<<(270+45*e.skips)<<"\"><rect width=\"100%\" height=\"100%\" fill=\"#fff\"/><g font-family=\"Arial\" font-size=\"14\" fill=\"#172033\"><text x=\"20\" y=\"25\">"<<r.name<<" "<<e.p<<" to "<<e.q<<"; elapsed "<<span<<" ms; origin "<<lo<<" ns</text><text x=\"20\" y=\"48\">Green = measured accept; orange = publication bounds; gray = accept-external gap (causal Unknown)</text>";
 auto bar=[&](Ns a,Ns b,int y,const char* color,const std::string& label){s<<"<text x=\"20\" y=\""<<y+15<<"\">"<<label<<"</text><rect x=\""<<xx(a)<<"\" y=\""<<y<<"\" width=\""<<std::max(1.0,xx(b)-xx(a))<<"\" height=\"22\" fill=\""<<color<<"\"/><text x=\""<<xx(a)<<"\" y=\""<<y-3<<"\">"<<(a-lo)/1e6<<"</text><text x=\""<<xx(b)<<"\" y=\""<<y+38<<"\">"<<(b-lo)/1e6<<" ms</text>";};
 bar(r.f[e.p].os,r.f[e.p].oe,75,"#2b9966","accept "+std::to_string(e.p));bar(r.f[e.p].oe,r.f[e.q].os,125,"#d2d6dd","external gap");int y=185;for(std::size_t a=e.p+1;a<e.q;++a)if(r.f[a].consumed&&!r.f[a].owned){auto nc=r.next_c[a];auto b=publication(r.f[a],nc?&r.f[nc]:nullptr);if(b.hi)bar(b.lo,b.hi,y,"#eea238","skip "+std::to_string(a)+" P bounds");y+=45;}bar(r.f[e.q].os,r.f[e.q].oe,y,"#2b9966","accept "+std::to_string(e.q));s<<"</g></svg>\n";out.write("timeline-"+tag+".svg",s.str());}
void analyze(const fs::path& manifest,const fs::path& output){auto inputs=check_manifest(manifest);
 std::set<std::string> bound;for(const auto& f:inputs.at("files"))bound.insert(fs::path(f.at("path").get<std::string>()).lexically_normal().generic_string());
 for(const auto& name:{"aa-rgb-1","aa-rgb-2","aa-rgb-3","aa-rgb-4","aa-owner-1"})for(const auto& f:fs::recursive_directory_iterator(fs::path(inputs.at("r3_root").get<std::string>())/name))if(f.is_regular_file())need(bound.contains(f.path().lexically_normal().generic_string()),"unbound run input");
 Output out(output);J all={{"schema",1},{"protocol_sha256",inputs.at("protocol_sha256")},{"input_manifest_sha256",hash(manifest)},{"status","選項A交付完成，待總控獨立驗收；成本資格仍not-ready"},{"new_runtime_cost_stress_OS_live_runs",0},{"runs",J::array()},{"hypothesis_rules","protocol-before-analysis; cluster <= cadence/2; source-contained enqueue is elapsed lower bound; causal Unknown"}};
 std::string skips,consumers,edges,arrivals,matches,anchor_map;std::size_t decision_n=0,consumer_n=0;J examples=J::array();
 for(const auto& name:{"aa-rgb-1","aa-rgb-2","aa-rgb-3","aa-rgb-4","aa-owner-1"}){auto r=load_run(inputs.at("r3_root").get<std::string>(),name);all["runs"].push_back(run_report(r));
  for(std::size_t a=1;a<=2560;++a){const auto& x=r.f[a];if(!x.consumed){consumers+=skip_row(r,a,false).dump()+"\n";++consumer_n;}else if(!x.owned){skips+=skip_row(r,a,true).dump()+"\n";++decision_n;}
   if(x.consumed){auto pc=r.prev_c[a],nc=r.next_c[a];arrivals+=J{{"run",name},{"attempt",a},{"window",wname(window(a))},{"phase",(a-1)%16},{"targets",x.targets},{"owned",x.owned},{"previous_consumed",pc?J(pc):J(nullptr)},{"next_consumed",nc?J(nc):J(nullptr)},{"recognition_previous_gap_ms",pc?J((x.re-r.f[pc].re)/1e6):J(nullptr)},{"recognition_next_gap_ms",nc?J((r.f[nc].re-x.re)/1e6):J(nullptr)},{"capture_previous_gap_ms",a>1?J((x.capture-r.f[a-1].capture)/1e6):J(nullptr)}}.dump()+"\n";}
  }
  for(auto a:r.owned){const auto& ids=r.anchored[a];anchor_map+=J{{"run",name},{"decision_sequence",r.timings[ids[0]].raw_anchor},{"source_attempt",a},{"first_event_index",ids.front()},{"last_event_index",ids.back()},{"event_n",ids.size()}}.dump()+"\n";}
  const Edge *first=nullptr,*longest=nullptr,*ratio=nullptr;std::map<std::string,std::vector<double>> matched_metrics;std::size_t matched=0,unmatched=0,skip_arrival_n=0,skip_cluster=0,control_arrival_n=0,control_cluster=0;
  for(const auto& e:r.edges){edges+=edge_json(r,e).dump()+"\n";if(!e.skips)continue;if(!first)first=&e;if(!longest||e.gap>longest->gap)longest=&e;if(!ratio||e.enqueue/(e.gap/1e6)>ratio->enqueue/(ratio->gap/1e6))ratio=&e;auto c=match(r,e);J m={{"run",name},{"skip_edge",edge_json(r,e)},{"control_edge",c?edge_json(r,*c):J(nullptr)},{"match_policy","same run/previous warmup-or-block/previous phase/previous targets; nearest previous attempt, tie smaller; replacement"}};
   if(c){++matched;for(auto [k,x,y]:std::vector<std::tuple<std::string,double,double>>{{"gap_ms",e.gap/1e6,c->gap/1e6},{"enqueue_ms",e.enqueue,c->enqueue},{"remainder_ms",e.residual,c->residual}}){matched_metrics["skip_"+k].push_back(x);matched_metrics["control_"+k].push_back(y);}m["absolute_attempt_distance"]=e.p>c->p?e.p-c->p:c->p-e.p;
    J pair_arrivals=J::array();for(std::size_t a=e.p+1;a<e.q;++a)if(r.f[a].consumed&&!r.f[a].owned){auto nc=r.next_c[a],cn=r.next_c[c->q];bool sc=nc&&(r.f[nc].re-r.f[a].re)/1e6<=r.cadence/2,cc=cn&&(r.f[cn].re-r.f[c->q].re)/1e6<=r.cadence/2;skip_arrival_n+=nc>0;skip_cluster+=sc;control_arrival_n+=cn>0;control_cluster+=cc;pair_arrivals.push_back({{"skip_attempt",a},{"control_arrival_attempt",c->q},{"skip_next_cluster",nc?J(sc):J(nullptr)},{"control_next_cluster",cn?J(cc):J(nullptr)}});}m["arrival_control_with_replacement"]=pair_arrivals;
   }else ++unmatched;matches+=m.dump()+"\n";
  }
  all["runs"].back()["matched_control"]=J{{"matched_skip_edges",matched},{"unmatched_skip_edges",unmatched},{"metrics",metrics(matched_metrics)},{"causal_proof",false},{"exploratory_arrival_control",{{"policy","each skipped consumed arrival versus its edge matched control next owned arrival, with replacement; next phase and capture gap uncontrolled"},{"skip_n",skip_arrival_n},{"skip_cluster_n",skip_cluster},{"control_n",control_arrival_n},{"control_cluster_n",control_cluster}}}};
  std::set<std::size_t> picked;for(auto [label,e]:std::vector<std::pair<std::string,const Edge*>>{{"first",first},{"longest",longest},{"highest-enqueue-fraction",ratio}})if(e&&picked.insert(e->p).second){auto tag=r.name+"-"+label;timeline(out,r,*e,tag);auto c=match(r,*e);J ex={{"rule",label},{"edge",edge_json(r,*e)},{"timeline","timeline-"+tag+".svg"},{"control",c?edge_json(r,*c):J(nullptr)}};if(c){timeline(out,r,*c,tag+"-control");ex["control_timeline"]="timeline-"+tag+"-control.svg";}examples.push_back(ex);}
 }
 need(decision_n==194&&consumer_n==139,"five-run reconciliation");all["five_run_decision_skips"]=decision_n;all["five_run_consumer_skips"]=consumer_n;all["owner_decision_skips"]=193;all["owner_consumer_skips"]=85;all["examples"]=examples;all["unknown"]={"causal allocation of skips","publication and selection exact times","wake reason/due/lock wait/OS scheduling","noise not evaluated","B1 cost difference","real capture/RPC/game adoption","14 lost opportunities","77 Miss/cross-song"};
 out.write("decision-skips.jsonl",skips);out.write("consumer-skips.jsonl",consumers);out.write("owned-edges.jsonl",edges);out.write("consumed-arrivals.jsonl",arrivals);out.write("matched-controls.jsonl",matches);out.write("source-anchor-map.jsonl",anchor_map);out.json("report.json",all);
 J ledger={{"files",J::array()},{"output_limit_bytes",output_limit}};for(const auto& p:fs::directory_iterator(out.root))if(p.is_regular_file())ledger["files"].push_back({{"name",p.path().filename().string()},{"bytes",p.file_size()},{"sha256",hash(p.path())}});std::sort(ledger["files"].begin(),ledger["files"].end(),[](const J&a,const J&b){return a.at("name").get<std::string>()<b.at("name").get<std::string>();});out.json("output-ledger.json",ledger);std::cout<<J{{"decision_skips",decision_n},{"consumer_skips",consumer_n},{"output_bytes",out.bytes}}.dump()<<'\n';
}
void self_test(const fs::path& report){need(!fs::exists(report),"existing test report");J tests=J::array();auto test=[&](const std::string& name,auto f){f();tests.push_back({{"name",name},{"pass",true}});};auto rejects=[](auto f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}need(failed,"expected rejection");};
 const J base={{"attempt",1},{"capture_complete_ns",10},{"pixels_ready_ns",10},{"published_ns",20},{"recognition_start_ns",30},{"recognition_complete_ns",40},{"owner_start_ns",50},{"owner_end_ns",60},{"publish_cost_ns",10},{"targets",1},{"lines",0},{"published",true},{"consumed",true},{"owned",true}};
 test("join rejects duplicate/reversed/missing attempt",[&]{for(auto a:{0,2,3}){auto j=base;j["attempt"]=a;rejects([&]{frame(j,1);});}});
 test("unknown zero becomes null, never instantaneous",[&]{auto j=base;j["consumed"]=false;j["owned"]=false;for(auto k:{"published_ns","recognition_start_ns","recognition_complete_ns","owner_start_ns","owner_end_ns","targets"})j[k]=0;auto f=frame(j,1);need(brief(f).at("recognition_complete_ns").is_null(),"unknown null");rejects([&]{publication(f,nullptr);});j["recognition_complete_ns"]=40;rejects([&]{frame(j,1);});});
 test("negative/overflow/float/nonmonotonic time rejected",[&]{for(const J& v:std::vector<J>{-1,1.5,std::uint64_t(UINT64_MAX),9}){auto j=base;j["published_ns"]=v;rejects([&]{frame(j,1);});}});
 test("illegal owned without consumed rejected",[&]{auto j=base;j["consumed"]=false;rejects([&]{frame(j,1);});});
 test("RGB raw sequence differs from frame; wrong/reversed/unowned rejected",[&]{std::vector<Frame> f(34);f[33].owned=true;anchor_check(32,33,32,31,31,f);rejects([&]{anchor_check(32,33,33,31,31,f);});rejects([&]{anchor_check(32,33,32,32,31,f);});rejects([&]{anchor_check(32,32,32,31,31,f);});});
 test("service containment excludes previous preaccept, next tail and writer",[&]{std::vector<Timing> t(6);for(auto& x:t){x.enqueue=99;x.serial=x.write=999;}t[2].enqueue=1;t[3].enqueue=2;t[4].enqueue=3;auto [sum,n]=contained_enqueue(t,{0,1,2},{3,4,5});need(sum==6&&n==3,"critical containment");rejects([&]{contained_enqueue(t,{0},{3,4});});});
 test("bounded reader missing LF/empty/malformed/count/row",[&]{for(auto [s,n,b]:std::vector<std::tuple<std::string,std::size_t,std::size_t>>{{"{}",2,10},{"\n",2,10},{"x\n",2,10},{"{}\n{}\n",1,10},{"{}\n",2,1}}){std::istringstream f(s);rejects([&]{rows(f,n,b,[](const J&){});});}});
 test("exact bounded reader and CRLF",[&]{std::istringstream f("{}\r\n{}\n");std::size_t n=0;rows(f,2,3,[&](const J&){++n;});need(n==2,"reader count");});
 test("publication upper is next START; last censored",[&]{auto a=frame(base,1),b=a;b.a=2;b.rs=80;b.re=100;need(publication(a,&b).hi==80,"start bound");need(publication(a,nullptr).hi==0,"censored bound");b.rs=39;rejects([&]{publication(a,&b);});});
 test("accept exclusion strict greater and equality overlap",[&]{auto p=frame(base,1),s=p;p.os=50;p.oe=60;s.re=61;need(relation(s,&p,nullptr,nullptr).at("publication_definitely_after_previous_accept"),"after exclusion");s.re=60;need(!relation(s,&p,nullptr,nullptr).at("publication_definitely_after_previous_accept").get<bool>(),"equality not after");});
 test("completion inside does not mean publication inside",[&]{auto p=frame(base,1),s=p,n=p,n2=p;p.os=50;p.oe=100;s.re=60;n.rs=120;n.re=130;n2.rs=150;auto j=relation(s,&p,&n,&n2);need(j.at("completion_relation_to_previous_accept")=="inside_closed_interval"&&!j.at("entire_opportunity_enclosed_by_previous_accept").get<bool>()&&j.at("causal_attribution")=="Unknown","no false causal claim");});
 test("whole opportunity bound needs next-next start",[&]{auto p=frame(base,1),s=p,n=p,n2=p;p.os=50;p.oe=100;s.re=60;n.rs=70;n.re=80;n2.rs=90;need(relation(s,&p,&n,&n2).at("entire_opportunity_enclosed_by_previous_accept"),"enclosed bound");n2.rs=101;need(!relation(s,&p,&n,&n2).at("entire_opportunity_enclosed_by_previous_accept").get<bool>(),"upper exceeds");});
 test("exploratory preaccept replacement needs upper bound, not completion",[&]{auto p=frame(base,1),s=p,n=p,n2=p;p.os=100;p.oe=120;s.re=60;n.rs=70;n.re=80;n2.rs=99;need(relation(s,&p,&n,&n2).at("exploratory_replacement_definitely_before_previous_accept_start"),"replacement before");n2.rs=100;need(!relation(s,&p,&n,&n2).at("exploratory_replacement_definitely_before_previous_accept_start").get<bool>(),"equal censored");});
 test("same observed timestamps support wait or drain histories: Unknown",[&]{auto p=frame(base,1),s=p,n=p,n2=p;p.os=200;p.oe=300;s.re=400;n.rs=500;n.re=600;n2.rs=900;auto measured=relation(s,&p,&n,&n2);const J histories=J::array({{{"P_skip",450},{"P_next",650},{"S_next",800},{"unobserved_actor","wait"}},{{"P_skip",450},{"P_next",650},{"S_next",800},{"unobserved_actor","drain/JSON"}}});for(const auto& h:histories){need(h.at("P_skip").get<int>()>=400&&h.at("P_skip").get<int>()<=500&&h.at("P_next").get<int>()>=600&&h.at("P_next").get<int>()<=900,"history bounds");need(measured.at("causal_attribution")=="Unknown","ambiguity retained");}});
 test("match fixed phase/window/target, nearest tie and replacement",[&]{Run r;r.f.resize(200);for(auto& f:r.f)f.targets=32;r.edges={{17,18,0},{49,50,1},{81,82,0},{18,19,0}};need(match(r,r.edges[1])==&r.edges[0],"tie smaller phase");r.f[17].targets=1;need(match(r,r.edges[1])==&r.edges[2],"targets control");});
 test("SHA wrong/duplicate manifest rejected before output",[&]{auto root=report.parent_path()/(report.stem().string()+"-fixtures");need(!fs::exists(root),"existing test fixtures");fs::create_directory(root);auto data=root/"input.json";{std::ofstream f(data);f<<"{}\n";}auto m=root/"manifest.json";J j={{"schema",1},{"files",J::array({{{"path",fs::absolute(data).string()},{"bytes",fs::file_size(data)},{"sha256",std::string(64,'0')}}})}};auto save=[&]{std::ofstream f(m);f<<j.dump();};save();rejects([&]{check_manifest(m);});j["files"][0]["sha256"]=hash(data);save();check_manifest(m);j["files"].push_back(j["files"][0]);save();rejects([&]{check_manifest(m);});});
 test("empty distributions null; nearest rank and jitter",[&]{need(dist({}).at("max").is_null(),"empty Unknown");auto j=dist({1,2,3,4});need(j.at("p50")==2&&j.at("p99")==4&&j.at("jitter_p95_minus_p5")==3,"ranks");});
 std::ofstream f(report,std::ios::binary);f<<J{{"tests",tests},{"passed",tests.size()},{"failed",0},{"runtime_or_cost_runs",0},{"scope","independent reader/bounds/joins and nonidentifiability"}}.dump(2)<<'\n';f.flush();need(bool(f),"test output");std::cout<<"passed "<<tests.size()<<"\n";
}
int main(int argc,char** argv){try{if(argc==3&&std::string(argv[1])=="self-test"){self_test(argv[2]);return 0;}if(argc==4&&std::string(argv[1])=="verify-input"){auto inputs=check_manifest(argv[2]);need(!fs::exists(argv[3]),"existing verification");J j={{"runs",J::array()},{"new_runtime_runs",0}};for(auto name:{"aa-rgb-1","aa-rgb-2","aa-rgb-3","aa-rgb-4","aa-owner-1"})j["runs"].push_back(run_report(load_run(inputs.at("r3_root").get<std::string>(),name)));std::ofstream f(argv[3],std::ios::binary);f<<j.dump(2)<<'\n';f.flush();need(bool(f),"verification write");return 0;}need(argc==4&&std::string(argv[1])=="analyze","usage: analyze INPUT_MANIFEST NEW_OUTPUT; self-test NEW_REPORT; verify-input INPUT_MANIFEST NEW_REPORT");analyze(argv[2],argv[3]);return 0;}catch(const std::exception& e){std::cerr<<J{{"error",e.what()}}.dump()<<'\n';return 1;}}
