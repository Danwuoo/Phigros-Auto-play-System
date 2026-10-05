#include "pas/session_archive.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
using json=nlohmann::json;namespace fs=std::filesystem;
json read(const fs::path& p){std::ifstream f(p);if(!f)throw std::runtime_error("open "+p.string());return json::parse(f);}
void require(bool p,const char* message){if(!p)throw std::runtime_error(message);}
std::vector<json> rows(const fs::path& p){std::ifstream f(p,std::ios::binary);require(bool(f),"raw open");std::vector<json> v;std::string s;while(std::getline(f,s)){require(v.size()<16384&&s.size()<256*1024,"raw bound");v.push_back(json::parse(s));}require(f.eof(),"raw read");return v;}
void check(const json& m,std::vector<double> v){require(m.at("n")==v.size(),"metric denominator");if(v.empty()){require(m.at("p99").is_null(),"empty metric");return;}std::sort(v.begin(),v.end());auto q=[&](double p){return v.at(static_cast<std::size_t>(std::ceil(p*v.size()))-1);};for(auto [k,p]:std::initializer_list<std::pair<const char*,double>>{{"p50",.5},{"p95",.95},{"p99",.99},{"max",1}})require(std::abs(m.at(k).get<double>()-q(p))<1e-8,"metric quantile");require(std::abs(m.at("jitter_p95_minus_p5").get<double>()-(q(.95)-q(.05)))<1e-8,"metric jitter");}
int main(int argc,char** argv){try{
 require(argc==3,"usage SOURCE NEW_OUTPUT");fs::path source=argv[1],out=argv[2];require(!fs::exists(out),"existing output");fs::create_directories(out);json report;
 for(const auto& d:fs::directory_iterator(source)){
  const auto name=d.path().filename().string();if(!d.is_directory()||(!name.starts_with("aa-")&&!name.starts_with("stress-")))continue;
  const auto s=read(d.path()/"summary.json");auto frames=rows(d.path()/"frames.jsonl"),receipts=rows(d.path()/"receipts.jsonl"),releases=rows(d.path()/"releases.jsonl");
  require(frames.size()==s.at("attempts")&&receipts.size()==s.at("receipt_n")&&releases.size()==s.at("release_n"),"raw counts");
  std::map<std::string,std::vector<double>> m;std::size_t consumed=0,owned=0,published=0;std::uint64_t requested=0,failed=0,unknown=0;
  for(std::size_t i=0;i<frames.size();++i){const auto& f=frames[i];require(f.at("attempt")==i+1,"frame sequence");published+=f.at("published").get<bool>();m["publish"].push_back(f.at("publish_cost_ns").get<double>()/1e6);
   if(f.at("consumed")){++consumed;m["recognition"].push_back((f.at("recognition_complete_ns").get<std::int64_t>()-f.at("recognition_start_ns").get<std::int64_t>())/1e6);}
   if(f.at("owned")){++owned;m["owner"].push_back((f.at("owner_end_ns").get<std::int64_t>()-f.at("owner_start_ns").get<std::int64_t>())/1e6);m["capture_to_owner"].push_back((f.at("owner_end_ns").get<std::int64_t>()-f.at("capture_complete_ns").get<std::int64_t>())/1e6);}}
  require(consumed==s.at("consumed")&&owned==s.at("owner_seen")&&published==s.at("published"),"frame totals");
  for(const auto& r:receipts){auto seq=r.at("source_frame").get<std::size_t>();require(seq>0&&seq<=frames.size(),"receipt source join");auto start=r.at("injection_start_ns").get<std::int64_t>(),end=r.at("injection_return_ns").get<std::int64_t>(),due=r.at("scheduled_ns").get<std::int64_t>();const double late=(start-due)/1e6;
   m["lateness"].push_back(late);m[std::array<const char*,3>{"down_lateness","move_lateness","up_lateness"}.at(r.at("phase").get<std::size_t>())].push_back(late);m["injection_call"].push_back((end-start)/1e6);m["capture_to_injection_all_phases"].push_back((start-frames[seq-1].at("capture_complete_ns").get<std::int64_t>())/1e6);}
  for(std::size_t i=0;i<releases.size();++i){const auto& r=releases[i];require(r.at("call_index")==i,"release index");requested+=r.at("requested_ids").size();failed+=r.at("failed_ids").size();unknown+=r.at("unknown_ids").size();m["release_call"].push_back((r.at("return_ns").get<std::int64_t>()-r.at("start_ns").get<std::int64_t>())/1e6);}
  require(requested==s.at("release_requested_ids")&&failed==s.at("release_failed_ids")&&unknown==s.at("release_unknown_ids"),"release totals");
  for(auto k:{"recognition","owner","capture_to_owner","publish","lateness","down_lateness","move_lateness","up_lateness","injection_call","release_call","capture_to_injection_all_phases"})check(s.at("metrics_ms").at(k),m[k]);
  auto completion=read(d.path()/"archive/round-1/summary.json");std::map<std::string,std::uint64_t> counts;std::uint64_t logical=0,physical=0,eventRows=0;std::size_t ri=0,li=0;
  for(const auto& seg:completion.at("event_segments")){auto path=d.path()/"archive/round-1"/seg.at("path").get<std::string>();physical+=fs::file_size(path);for(auto j:rows(path)){++eventRows;logical+=j.dump().size()+1;auto kind=j.at("event").get<std::string>();++counts[kind];j.erase("schema_version");j.erase("clock_domain");j.erase("round_id");if(kind=="game_touch_receipt")require(ri<receipts.size()&&j==receipts[ri++],"archive receipt content");if(kind=="game_release_report")require(li<releases.size()&&j==releases[li++],"archive release content");}}
  require(json(counts)==s.at("event_counts")&&ri==receipts.size()&&li==releases.size(),"archive counts");require(logical==s.at("journal_serialized_bytes")&&eventRows==s.at("journal_written"),"archive bytes");
  report["runs"][name]={{"raw_metric_quantiles_equal",true},{"receipt_release_archive_content_equal",true},{"attempts",frames.size()},{"receipts",receipts.size()},{"releases",releases.size()},{"event_rows",eventRows},{"logical_bytes",logical},{"physical_bytes",physical},{"extra_disk_bytes",physical-logical}};
 }
 require(report["runs"].size()==12,"run count");
 // Deterministic archive size boundary, not a timing/stress run. Existing frozen core is linked unchanged.
 json event={{"event","boundary"},{"clock_domain","host_qpc_ns"},{"schema_version",2},{"round_id",1},{"padding",""}};
 const auto base=event.dump().size()+1;require(base<256,"boundary shape");event["padding"]=std::string(256-base,'x');require(event.dump().size()+1==256,"logical row exact");
 pas::SessionArchive a(out/"boundary",{{"offline",true}},256,256);for(int i=0;i<32;++i)a.event(1,event);a.complete(1,{{"status","complete"}});a.close();require(!a.faulted(),"boundary archive fault");
 const auto summary=read(out/"boundary/round-1/summary.json");require(summary.at("event_segments").size()==32,"boundary segments");std::uint64_t bytes=0;for(const auto& seg:summary.at("event_segments"))bytes+=fs::file_size(out/"boundary/round-1"/seg.at("path").get<std::string>());
 report["capacity_counterexample"]={{"logical_segment_limit",256},{"segments",32},{"logical_total",8192},{"physical_total",bytes},{"archive_fault",a.faulted()},{"physical_exceeds_logical_limit",bytes>8192}};
 std::ofstream f(out/"audit.json");f<<report.dump(2)<<'\n';f.flush();require(bool(f),"audit write");std::cout<<report.at("capacity_counterexample").dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
