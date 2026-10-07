#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
using J=nlohmann::json;namespace fs=std::filesystem;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 require(argc==3&&!fs::exists(argv[2]),"root and fresh output required");
 const fs::path root=fs::canonical(argv[1]);require(fs::file_size(root/"report.json")<=1024*1024,"report bound");
 std::ifstream rf(root/"report.json");const auto report=J::parse(rf);
 const fs::path session=fs::canonical(report.at("archive_root").get<std::string>());
 const auto relative=session.lexically_relative(root/"archive");require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..","archive containment");
 std::multimap<std::uint64_t,J> plans;std::vector<J> receipts;std::uint64_t bytes=0,rows=0;
 const auto& segments=report.at("round_summary").at("event_segments");require(segments.size()<=32,"segment bound");
 for(std::size_t i=0;i<segments.size();++i){const auto name=segments[i].at("path").get<std::string>();require(name=="events-"+std::to_string(i)+".jsonl","segment order");const auto path=session/"round-1"/name;
  bytes+=fs::file_size(path);require(bytes<=512ull*1024*1024,"journal bytes");std::ifstream f(path);std::string line;
  while(std::getline(f,line)){require(line.size()<=1024*1024&&++rows<=1000000,"row bound");const auto v=J::parse(line);const auto event=v.at("event").get<std::string>();
   if(event=="own_accepted_plan"){require(plans.size()<65536,"plan bound");plans.emplace(v.at("intent").get<std::uint64_t>(),v);}
   if(event=="candidate_backend_receipt"){require(receipts.size()<20000,"receipt bound");receipts.push_back(v);}
  }
 }
 J missing=J::array();std::size_t joined=0;
 for(const auto& receipt:receipts){const auto& c=receipt.at("issued");bool matched=false;J related=J::array();const auto [begin,end]=plans.equal_range(c.at("intent").get<std::uint64_t>());
  for(auto it=begin;it!=end;++it){const auto& p=it->second;if(p.at("source_frame")!=c.at("source_frame"))continue;related.push_back(p);
   for(const auto& s:p.at("steps"))matched|=s.at("phase")==c.at("phase")&&s.at("due")==c.at("scheduled")&&s.at("x")==c.at("x")&&s.at("y")==c.at("y");
  }
  if(matched)++joined;else {require(missing.size()<128,"missing join diagnostic bound");missing.push_back({{"receipt",receipt},{"same_intent_frame_plans",related}});}
 }
 J output={{"schema","pas.receipt-join-diagnosis.v1"},{"source_commit",report.at("source_build_commit")},{"plans",plans.size()},{"receipts",receipts.size()},{"exact_step_joins",joined},{"unjoined",missing},{"journal_rows",rows},{"journal_bytes",bytes},{"qualification",false},{"device_operations",0}};
 std::ofstream out(argv[2]);out<<output.dump(2)<<'\n';require(bool(out),"output write");std::cout<<"receipts="<<receipts.size()<<" exact_joins="<<joined<<" unjoined="<<missing.size()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
