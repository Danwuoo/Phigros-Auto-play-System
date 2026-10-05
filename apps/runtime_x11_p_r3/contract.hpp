#pragma once
#include "../runtime_x11_p_r1/gate_contract.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>
namespace r3 {
using json=nlohmann::json;
constexpr std::size_t attempts=2560,warmup=256,block=576,receipts=8192,releases=8192,events=65536;
constexpr std::size_t journal_bytes=40*1024*1024,row_bytes=256*1024,run_bytes=80*1024*1024;
inline void require(bool p,const char* m){if(!p)throw std::runtime_error(m);}
struct Admission {
 std::size_t byte_limit=journal_bytes,count_limit=events,bytes=0,count=0;
 std::size_t admit(const json& j){auto envelope=j;envelope["schema_version"]=2;envelope["clock_domain"]="host_qpc_ns";envelope["round_id"]=1;const auto size=envelope.dump().size()+1;
  require(size<=row_bytes&&count<count_limit&&size<=byte_limit-bytes,"diagnostic_capacity");bytes+=size;++count;return size;
 }
};
inline json distribution(std::vector<double> v) {
 for(auto x:v)require(std::isfinite(x),"nonfinite sample");
 std::sort(v.begin(),v.end());auto q=[&](double p)->json{return v.empty()?json(nullptr):json(v.at(static_cast<std::size_t>(std::ceil(p*v.size()))-1));};
 return {{"n",v.size()},{"p50",q(.5)},{"p95",q(.95)},{"p99",q(.99)},{"max",q(1)},{"jitter_p95_minus_p5",v.empty()?json(nullptr):json(q(.95).get<double>()-q(.05).get<double>())}};
}
// Read byte-by-byte to enforce the row limit BEFORE allocating an arbitrary line.
// A final unterminated record is truncation, even when its JSON parses.
template<class F> void rows(std::istream& in,std::size_t count_limit,std::size_t byte_limit,F emit) {
 std::size_t count=0;std::string row;row.reserve(std::min<std::size_t>(byte_limit,4096));char c;
 while(in.get(c)) {
  if(c=='\n'){require(++count<=count_limit,"raw count bound");if(!row.empty()&&row.back()=='\r')row.pop_back();require(!row.empty(),"empty raw row");emit(json::parse(row));row.clear();}
  else {require(row.size()<byte_limit,"raw row bound");row.push_back(c);}
 }
 require(in.eof()&&!in.bad(),"raw read");require(row.empty(),"truncated final raw row");
}
template<class F> void rows(const std::filesystem::path& p,std::size_t count_limit,std::size_t byte_limit,F emit) {
 std::ifstream f(p,std::ios::binary);require(bool(f),"raw open");rows(f,count_limit,byte_limit,emit);
}
inline json read(const std::filesystem::path& p) {require(std::filesystem::file_size(p)<=2*1024*1024,"summary size");std::ifstream f(p);require(bool(f),"summary open");return json::parse(f);}
inline void check(const json& m,const std::vector<double>& v) {
 const auto d=distribution(v);require(m.at("n")==d.at("n"),"metric denominator");
 for(auto k:{"p50","p95","p99","max","jitter_p95_minus_p5"})if(v.empty())require(m.at(k).is_null(),"empty metric");else require(m.at(k).is_number()&&std::abs(m.at(k).get<double>()-d.at(k).get<double>())<1e-8,"metric quantile");
}
struct Window {
 std::size_t first,last;std::uint64_t attempts=0,published=0,consumed=0,owned=0,receipt_n=0,down=0,move=0,up=0,failed=0,unknown=0,late=0,release_n=0,requested=0,release_failed=0,release_unknown=0;
 std::map<std::string,std::vector<double>> metrics;
 bool includes(std::size_t n)const{return first<=n&&n<=last;}
 json report()const{
  json m=json::object();for(auto k:{"publish","recognition","owner","capture_to_owner","lateness","down_lateness","move_lateness","up_lateness","injection_call","capture_to_injection_all_phases","release_call","event_enqueue","writer_serialize","writer_write"}){auto it=metrics.find(k);m[k]=distribution(it==metrics.end()?std::vector<double>{}:it->second);}
  return {{"first_attempt",first},{"last_attempt",last},{"attempts",attempts},{"published",published},{"pool_drops",attempts-published},{"consumed",consumed},{"owner_seen",owned},{"consumer_skips",published-consumed},{"decision_skips",consumed-owned},{"receipt_n",receipt_n},{"down",down},{"move",move},{"up",up},{"failed_receipts",failed},{"unknown_receipts",unknown},{"late_over_15ms",late},{"release_n",release_n},{"release_requested_ids",requested},{"release_failed_ids",release_failed},{"release_unknown_ids",release_unknown},{"metrics_ms",m}};
 }
};
struct Windows {
 std::array<Window,7> w{{{1,2560},{1,256},{257,2560},{257,832},{833,1408},{1409,1984},{1985,2560}}};
 void frame(const json& f){const auto n=f.at("attempt").get<std::size_t>();for(auto& x:w)if(x.includes(n)) {
  ++x.attempts;x.published+=f.at("published").get<bool>();x.consumed+=f.at("consumed").get<bool>();x.owned+=f.at("owned").get<bool>();
  x.metrics["publish"].push_back(f.at("publish_cost_ns").get<double>()/1e6);
  if(f.at("consumed"))x.metrics["recognition"].push_back((f.at("recognition_complete_ns").get<std::int64_t>()-f.at("recognition_start_ns").get<std::int64_t>())/1e6);
  if(f.at("owned")){x.metrics["owner"].push_back((f.at("owner_end_ns").get<std::int64_t>()-f.at("owner_start_ns").get<std::int64_t>())/1e6);x.metrics["capture_to_owner"].push_back((f.at("owner_end_ns").get<std::int64_t>()-f.at("capture_complete_ns").get<std::int64_t>())/1e6);}
 }}
 void receipt(const json& r,std::int64_t capture) {const auto n=r.at("source_frame").get<std::size_t>();const int phase=r.at("phase");require(phase>=0&&phase<3,"receipt phase");const auto start=r.at("injection_start_ns").get<std::int64_t>();const auto late=(start-r.at("scheduled_ns").get<std::int64_t>())/1e6;
  for(auto& x:w)if(x.includes(n)){++x.receipt_n;if(phase==0)++x.down;else if(phase==1)++x.move;else ++x.up;x.failed+=!r.at("success").get<bool>();x.unknown+=r.at("reason").get<std::string>().find("unknown")!=std::string::npos;x.late+=late>15;x.metrics["lateness"].push_back(late);x.metrics[std::array<const char*,3>{"down_lateness","move_lateness","up_lateness"}[phase]].push_back(late);x.metrics["injection_call"].push_back((r.at("injection_return_ns").get<std::int64_t>()-start)/1e6);x.metrics["capture_to_injection_all_phases"].push_back((start-capture)/1e6);}
 }
 void release(const json& r){const auto n=r.at("offline_attempt").get<std::size_t>();for(auto& x:w)if(x.includes(n)){++x.release_n;x.requested+=r.at("requested_ids").size();x.release_failed+=r.at("failed_ids").size();x.release_unknown+=r.at("unknown_ids").size();x.metrics["release_call"].push_back((r.at("return_ns").get<std::int64_t>()-r.at("start_ns").get<std::int64_t>())/1e6);}}
 void timing(const json& t){for(auto& x:w)if(x.includes(t.at("attempt").get<std::size_t>()))for(auto k:{"event_enqueue","writer_serialize","writer_write"})x.metrics[k].push_back(t.at(k).get<double>());}
 json report()const{json j={{"all",w[0].report()},{"warmup",w[1].report()},{"measurement",w[2].report()},{"blocks",json::array()}};for(int i=3;i<7;++i)j["blocks"].push_back(w[i].report());j["partition_policy"]="frames/receipts by source attempt; release/enqueue/writer by last accepted attempt; blocks descriptive only";return j;}
};
inline bool normal(const json& r) {
 try {if(!r1::normal(r)||r.at("attempts")!=attempts||r.at("action_expectation")!="required")return false;const auto& w=r.at("windows").at("measurement");const auto c=w.at("consumed").get<std::uint64_t>(),o=w.at("owner_seen").get<std::uint64_t>();
  return w.at("attempts")==2304&&c>=2000&&o>=2000&&c>=2304*.9&&o>=2304*.9&&o<=c&&r1::metric(w.at("metrics_ms").at("recognition"),c)&&r1::metric(w.at("metrics_ms").at("owner"),o)&&r1::metric(w.at("metrics_ms").at("capture_to_owner"),o,100,250);
 }catch(...){return false;}
}
}
