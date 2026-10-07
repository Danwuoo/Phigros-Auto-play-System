#include "../evidence_hash.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using J=nlohmann::json;namespace fs=std::filesystem;
int main(int argc,char**argv){try{if(argc!=4||fs::exists(argv[3]))throw std::runtime_error("usage report journal fresh-output");
 const fs::path report=argv[1],journal=argv[2];if(fs::file_size(report)>1048576||fs::file_size(journal)>1073741824)throw std::runtime_error("input bounds");std::ifstream f(report),events(journal);J r=J::parse(f),late=J::array();std::size_t rows=0;std::array<std::size_t,3>counts{},late_counts{};
 std::string line;while(std::getline(events,line)){if(++rows>65536||line.size()>262144)throw std::runtime_error("row bounds");const auto e=J::parse(line);if(e.value("event","")!="own_injection_receipt")continue;const auto phase=e.at("phase").get<std::size_t>();if(phase>=3)throw std::runtime_error("phase");++counts[phase];const auto lateness=e.at("injection_start").get<std::int64_t>()-e.at("scheduled").get<std::int64_t>();if(lateness>5000000){++late_counts[phase];late.push_back({{"phase",phase},{"source_frame",e.at("source_frame")},{"scheduled",e.at("scheduled")},{"injection_start",e.at("injection_start")},{"late_ms",lateness/1e6}});}}
 if(rows!=r.at("journal_records")||counts[0]!=r.at("own_down")||counts[1]!=r.at("own_move")||counts[2]!=r.at("own_up"))throw std::runtime_error("raw counts mismatch");
 J out={{"schema","pas.phase-lateness-diagnosis.v1"},{"report",{{"path",fs::canonical(report).string()},{"sha256",pas::evidence::sha256(report)}}},{"journal",{{"path",fs::canonical(journal).string()},{"sha256",pas::evidence::sha256(journal,1073741824)}}},{"phase_counts",counts},{"over_5ms_phase_counts",late_counts},{"late_receipts",late},{"all_phase_lateness_ms",r.at("all_phase_lateness_ms")},{"diagnosis_scope","receipt phase attribution; causality requires scheduling regression"},{"live_ready",false},{"device_endpoints",0}};
 std::ofstream result(argv[3]);result<<out.dump(2)<<'\n';if(!result)throw std::runtime_error("write");std::cout<<"late phase counts="<<late_counts[0]<<'/'<<late_counts[1]<<'/'<<late_counts[2]<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
