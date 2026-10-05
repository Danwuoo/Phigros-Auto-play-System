#include "contract.hpp"
#include <iostream>
using namespace r3;
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
 require(argc==3,"usage RUN NEW_REPORT");fs::path root=argv[1],out=argv[2];require(!fs::exists(out),"existing report");const auto s=read(root/"summary.json");Windows windows;
 std::vector<json> frames,receipts_raw,releases_raw;std::size_t published=0,consumed=0,owned=0;
 rows(root/"frames.jsonl",attempts,1023,[&](const json& f){require(f.at("attempt")==frames.size()+1,"frame sequence");auto t=[&](const char* k){return f.at(k).get<std::int64_t>();};
  require(f.at("owned").get<bool>()<=f.at("consumed").get<bool>()&&f.at("consumed").get<bool>()<=f.at("published").get<bool>(),"frame state");
  if(f.at("consumed"))require(t("capture_complete_ns")<=t("pixels_ready_ns")&&t("pixels_ready_ns")<=t("published_ns")&&t("published_ns")<=t("recognition_start_ns")&&t("recognition_start_ns")<=t("recognition_complete_ns"),"frame time order");
  if(f.at("owned"))require(t("recognition_complete_ns")<=t("owner_start_ns")&&t("owner_start_ns")<=t("owner_end_ns"),"owner time order");
  published+=f.at("published").get<bool>();consumed+=f.at("consumed").get<bool>();owned+=f.at("owned").get<bool>();windows.frame(f);frames.push_back(f);
 });
 require(frames.size()==s.at("attempts")&&published==s.at("published")&&consumed==s.at("consumed")&&owned==s.at("owner_seen"),"frame denominator");
 rows(root/"receipts.jsonl",receipts,511,[&](const json& r){auto seq=r.at("source_frame").get<std::size_t>();require(seq>0&&seq<=frames.size(),"receipt source join");require(r.at("injection_start_ns").get<std::int64_t>()>=r.at("scheduled_ns").get<std::int64_t>()&&r.at("injection_return_ns").get<std::int64_t>()>=r.at("injection_start_ns").get<std::int64_t>(),"receipt time order");windows.receipt(r,frames.at(seq-1).at("capture_complete_ns"));receipts_raw.push_back(r);});
 rows(root/"releases.jsonl",releases,511,[&](const json& r){require(r.at("call_index")==releases_raw.size(),"release index");require(r.at("return_ns").get<std::int64_t>()>=r.at("start_ns").get<std::int64_t>(),"release time order");auto a=r.at("offline_attempt").get<std::size_t>();require(a>0&&a<=frames.size(),"release anchor");windows.release(r);releases_raw.push_back(r);});
 require(receipts_raw.size()==s.at("receipt_n")&&releases_raw.size()==s.at("release_n"),"action denominator");
 std::size_t timing_n=0;std::map<std::string,std::vector<double>> timing_values;
 rows(root/"event-timings.jsonl",events,255,[&](const json& t){require(t.at("event_index")==timing_n++,"timing sequence");auto a=t.at("attempt").get<std::size_t>();require(a>0&&a<=frames.size(),"timing anchor");for(auto k:{"event_enqueue","writer_serialize","writer_write"}){require(t.at(k).is_number()&&std::isfinite(t.at(k).get<double>())&&t.at(k).get<double>()>=0,"missing or invalid writer sample");timing_values[k].push_back(t.at(k));}windows.timing(t);});
 require(timing_n==s.at("journal_written")&&timing_n==s.at("journal_attempts")&&s.at("journal_admitted").get<std::uint64_t>()==timing_n+1,"timing denominator");
 for(const auto& [k,v]:timing_values)check(s.at("metrics_ms").at(k),v);
 const auto wr=windows.report();require(wr==s.at("windows"),"full warmup/measurement/block report mismatch");
 for(auto k:{"recognition","owner","capture_to_owner","publish","lateness","down_lateness","move_lateness","up_lateness","injection_call","release_call","capture_to_injection_all_phases"})require(wr.at("all").at("metrics_ms").at(k)==s.at("metrics_ms").at(k),"all metric mismatch");
 for(auto k:{"receipt_n","down","move","up","failed_receipts","unknown_receipts","late_over_15ms","release_n","release_requested_ids","release_failed_ids","release_unknown_ids"})require(wr.at("all").at(k)==s.at(k),"all action totals");
 auto completion=read(root/"archive/round-1/summary.json");require(!completion.value("partial",false)&&completion.at("status")=="offline_complete","partial archive");std::map<std::string,std::uint64_t> counts,logical_by_type;std::uint64_t logical=0,physical=0,event_n=0;std::size_t ri=0,li=0;auto segs=completion.at("event_segments");require(!segs.empty()&&segs.size()<=32,"segments");
 for(const auto& seg:segs){auto name=seg.at("path").get<std::string>();require(fs::path(name).filename()==fs::path(name),"segment path");const auto path=root/"archive/round-1"/name;physical+=fs::file_size(path);
  rows(path,events,row_bytes,[&](json j){require(++event_n<=events,"total archive rows");require(j.at("schema_version")==2&&j.at("round_id")==1&&j.at("clock_domain")=="host_qpc_ns","event envelope");auto size=j.dump().size()+1;logical+=size;auto kind=j.at("event").get<std::string>();++counts[kind];logical_by_type[kind]+=size;j.erase("schema_version");j.erase("clock_domain");j.erase("round_id");
   if(kind=="game_touch_receipt")require(ri<receipts_raw.size()&&j==receipts_raw.at(ri++),"archive receipt content");if(kind=="game_release_report")require(li<releases_raw.size()&&j==releases_raw.at(li++),"archive release content");
  });
 }
 require(ri==receipts_raw.size()&&li==releases_raw.size()&&json(counts)==s.at("event_counts")&&event_n==s.at("journal_written")&&logical==s.at("journal_serialized_bytes")&&logical==s.at("journal_bytes_reserved"),"archive denominator/bytes");
 require(physical==logical+event_n||physical==logical,"LF/CRLF bytes");std::uint64_t bytes=0;for(const auto& f:fs::recursive_directory_iterator(root))if(f.is_regular_file())bytes+=f.file_size();require(bytes<=run_bytes,"physical run cap");
 json report={{"raw_integrity",true},{"normal_gate",normal(s)},{"attempts",frames.size()},{"consumed",consumed},{"owner_seen",owned},{"receipts",receipts_raw.size()},{"releases",releases_raw.size()},{"event_timings",timing_n},{"raw_metric_families_recomputed",14},{"windows",wr},{"archive_rows",event_n},{"archive_logical_bytes",logical},{"archive_physical_bytes",physical},{"archive_CR_extra_bytes",physical-logical},{"event_counts",counts},{"event_logical_bytes",logical_by_type},{"run_file_bytes",bytes},{"independent_samples","unknown"}};
 std::ofstream f(out,std::ios::binary);require(bool(f),"audit open");f<<report.dump(2)<<'\n';f.flush();require(bool(f),"audit write");std::cout<<json{{"integrity",true},{"normal_gate",normal(s)},{"bytes",bytes}}.dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
