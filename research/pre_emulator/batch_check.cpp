#include "evidence_hash.hpp"
#include "metrics.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
using J=nlohmann::json;namespace fs=std::filesystem;namespace c=pas::windows_cost_comparison;
J read(const fs::path&p){if(!fs::is_regular_file(p)||fs::file_size(p)>1048576)throw std::runtime_error("batch input bound");std::ifstream f(p,std::ios::binary);return J::parse(f);}
J bound(const J&b){fs::path p=b.at("path").get<std::string>();if(fs::file_size(p)!=b.at("bytes").get<std::uint64_t>()||pas::evidence::sha256(p,1048576)!=b.at("sha256").get<std::string>())throw std::runtime_error("batch file changed");return read(p);}
int main(int argc,char**argv){try{if(argc!=3||fs::exists(argv[2]))throw std::runtime_error("usage bound-batch-input fresh-output");const auto input=read(argv[1]);if(input.at("schema")!="pas.pre-emulator-abba-batch.v1"||input.at("runs").size()!=4)throw std::runtime_error("four ABBA runs required");const auto noise=bound(input.at("noise"));if(!noise.at("ready").get<bool>()||!noise.at("comparison_allowed").get<bool>())throw std::runtime_error("adequate pre-frozen AA noise required");const std::string scene=input.at("scene");std::array<J,4> runs;for(int i=0;i<4;++i){runs[i]=bound(input.at("runs")[i]);if(runs[i].at("scene")!=scene||runs[i].at("mode")!= (i==0||i==3?"A":"B")||runs[i].at("provenance").at("source_commit")!=input.at("source_commit"))throw std::runtime_error("exact ABBA scene/source order");}
 bool pass=true;J checks=J::object();for(const char*metric:{"observer_ms","owner_ms","capture_to_owner_ms"})for(const char*q:{"p95","p99"}){std::array<double,4> v;for(int i=0;i<4;++i)v[i]=runs[i].at("measurement").at(metric).at(q);const double t=noise.at("noise").at(scene).at("metrics").at(std::string(metric)+"."+q).at("T");const auto result=c::compare(v,t);pass&=result.non_regression;checks[std::string(metric)+"."+q]={{"ABBA",v},{"frozen_T",t},{"B_minus_A",result.b_minus_a},{"non_regression",result.non_regression}};}
 const J out={{"schema","pas.pre-emulator-abba-batch-result.v1"},{"scene",scene},{"non_regression",pass},{"checks",checks},{"scope","four already independently audited/gated runs; necessary early batch stop, full 36-run original verifier remains required"},{"live_ready",false},{"device_operations",0}};std::ofstream f(argv[2],std::ios::binary);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("batch output");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
