#include "pas/lossless_event_stream.hpp"
#include "evidence_hash.hpp"
#include "receipt_stream.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
using J=nlohmann::json;namespace fs=std::filesystem;
void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
J read(const fs::path&p){require(fs::is_regular_file(p)&&fs::file_size(p)<=16*1024*1024,"bounded evidence file");std::ifstream f(p,std::ios::binary);return J::parse(f);}
int main(int argc,char**argv){try{
 require(argc==6,"usage root report-SHA source-commit manual-SHA fresh-audit");const fs::path root=fs::canonical(argv[1]),out=fs::absolute(argv[5]);require(!fs::exists(out),"fresh audit");
 const auto report_path=root/"report.json";require(pas::evidence::sha256(report_path)==argv[2],"externally bound actual report");const auto r=read(report_path);
 require(r.at("schema")=="pas.literal-production-boundary-controls.v1"&&r.at("requested")==1000&&r.at("warmup")==256&&r.at("measurement_attempts")==744&&r.at("cadence_ms")==20,"full planned attempt contract");
 require(r.at("source_build_commit")==argv[3]&&!r.at("source_build_dirty").get<bool>()&&r.at("actual_manual_translation_unit_sha256")==argv[4],"exact production source binding");
 require(r.at("public_entry_rejected").get<bool>()&&r.at("single_touch_owner").get<bool>()&&r.at("endpoint_free").get<bool>()&&r.at("device_operations")==0&&!r.at("physical_qualification").get<bool>()&&!r.at("cost_qualification").get<bool>()&&!r.at("live_ready").get<bool>(),"scope cannot grant live");
 const std::string scenario=r.at("scenario");require(scenario=="capture"||scenario=="writer"||scenario=="rpc"||scenario=="slow","declared boundary scenario");const bool fault=scenario!="slow";require(r.at("expected_fault")==fault&&r.at("actual_threw")==fault,"actual return and expected failure");
 std::vector<fs::path> sessions;for(const auto&e:fs::directory_iterator(root/"archive"))if(e.is_directory())sessions.push_back(e.path());require(sessions.size()==1,"one session");const auto summary=read(sessions[0]/"summary.json");require(summary==r.at("summary")&&summary.at("state")==(fault?"FAULT":"STOPPED"),"actual final supervisor summary");
 const auto attempts=read(root/"attempts.json");require(attempts.size()==1000,"no omitted planned attempts");std::size_t produced=0;bool stopped=false;std::int64_t prior=0;for(std::size_t i=0;i<attempts.size();++i){const auto&a=attempts[i];require(a.at("attempt")==i+1&&a.at("measurement")== (i>=256),"ordinal/measurement exact");const bool present=a.at("produced");require(present==(a.at("render_end_ns").get<std::int64_t>()>0),"produced status must agree with timestamps");if(present){require(!stopped&&a.at("render_start_ns").get<std::int64_t>()>=prior&&a.at("render_end_ns")>=a.at("render_start_ns"),"contiguous monotonic produced prefix");prior=a.at("render_end_ns");++produced;}else stopped=true;}
 require(r.at("produced")==produced&&r.at("not_produced")==1000-produced,"all denominator status retained");if(scenario=="capture")require(produced==256&&r.at("actual_error")=="endpoint_free_capture_fault_at_attempt_257","exact capture outage");
 if(scenario=="writer"){const auto failure=read(sessions[0]/"archive-failure.json");require(failure.at("schema")=="pas.session-archive-first-failure.v1"&&failure.at("first_error")==summary.at("archive_first_error"),"actual sticky writer first error");require(fs::is_directory(sessions[0]/"round-1"/"events-0.jsonl.gz"),"test-double filesystem collision retained");}
 std::size_t receipt_down=0,receipt_move=0,receipt_up=0,release=0,unknown=0,rows=0;std::uint64_t decoded=0;std::set<std::uint64_t> down_ids;
 const auto round=sessions[0]/"round-1";if(scenario!="writer"){
  const auto round_summary=read(round/"summary.json");require(round_summary.at("event_encoding")=="gzip-jsonl-v1"&&round_summary.at("event_segments").size()<=256,"bounded original full journal");
  for(const auto&b:round_summary.at("event_segments")){const fs::path rel=b.at("path").get<std::string>();require(!rel.is_absolute()&&rel.parent_path().empty(),"direct segment path");const auto p=round/rel;require(pas::evidence::sha256(p,16*1024*1024+65536)==b.at("sha256"),"original segment SHA");pas::LosslessEventReader stream(p,16*1024*1024);std::string line;
   while(stream.next(line)){require(++rows<=1000000,"bounded fault row count");const auto e=J::parse(line);if(e.value("event","")=="candidate_backend_receipt"){const auto&c=e.at("issued");const int phase=c.at("phase");require(phase>=0&&phase<=2&&!e.at("effect_verified").get<bool>(),"phase and no invented physical effect");if(phase==0){++receipt_down;require(down_ids.insert(c.at("intent").get<std::uint64_t>()).second,"no repeated Down intent");}else if(phase==1)++receipt_move;else ++receipt_up;unknown+=!e.at("success").get<bool>();}else if(e.value("event","")=="candidate_backend_release"&&e.at("dispatched").get<bool>())++release;
   }decoded+=stream.decoded_bytes();require(decoded<=4ull*1024*1024*1024,"decoded full journal bound");
  }
 }
 if(scenario=="rpc")require(r.at("down_attempts")==1&&r.at("release_calls").get<std::uint64_t>()==r.at("releases_before_rpc").get<std::uint64_t>()+1&&receipt_down==1&&unknown==1&&release==r.at("release_calls").get<std::uint64_t>(),"unknown Down once, actual fault receipt and one release");
 require(pas::evidence::sha256(report_path)==argv[2],"report stable after audit");J result={{"schema","pas.literal-production-boundary-audit.v1"},{"scenario",scenario},{"control_pass",true},{"requested",1000},{"produced",produced},{"not_produced",1000-produced},{"raw_rows_checked",rows},{"decoded_bytes_checked",decoded},{"receipt_down",receipt_down},{"receipt_move",receipt_move},{"receipt_up",receipt_up},{"release",release},{"unknown_injection_results",unknown},{"source_commit",argv[3]},{"integrity_scope",scenario=="writer"?"intentional writer fault metadata; no complete journal claim":"all original gzip segments and full planned attempts"},{"cost_qualification",false},{"physical_qualification",false},{"live_ready",false},{"device_operations",0}};
 std::ofstream f(out,std::ios::binary);f<<result.dump(2)<<'\n';require(bool(f),"audit write");std::cout<<scenario<<" independently audited literal boundary control PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}