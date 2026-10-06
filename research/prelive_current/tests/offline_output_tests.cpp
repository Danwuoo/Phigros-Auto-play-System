#include "../offline_output.hpp"
#include <fstream>
#include <iostream>
#include <atomic>
#include <thread>
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("fresh output directory argument required");
 namespace fs=std::filesystem;const fs::path root=argv[1];if(fs::exists(root))throw std::runtime_error("fresh root required");fs::create_directories(root);
 int checks=0;auto report=root/"report.json";pas::current_rails::require_fresh_offline_report(report);++checks;
 for(const auto* suffix:{"", ".journal.jsonl", ".attempts.json", ".rows.jsonl", ".events.jsonl"}){
  auto path=report;path+=suffix;{std::ofstream f(path);f<<"preserved failed evidence\n";}
  bool rejected=false;try{pas::current_rails::require_fresh_offline_report(report);}catch(...){rejected=true;}
  std::ifstream f(path);std::string line;std::getline(f,line);if(!rejected||line!="preserved failed evidence")throw std::runtime_error("orphan sidecar guard failed");++checks;
  // Each later case has its own fresh name; never erase the earlier test data.
  report=root/("report-"+std::to_string(checks)+".json");
 }
#ifndef _WIN32
 fs::create_symlink("missing-target",root/"dangling.json.events.jsonl");
 bool link_rejected=false;try{pas::current_rails::require_fresh_offline_report(root/"dangling.json");}catch(...){link_rejected=true;}
 if(!link_rejected)throw std::runtime_error("dangling sidecar symlink accepted");
 ++checks;
#endif
 const auto concurrent=root/"concurrent.json";std::atomic<bool> go=false;std::atomic<int> accepted=0;
 auto reserve=[&]{while(!go.load())std::this_thread::yield();try{pas::current_rails::reserve_fresh_offline_report(concurrent);++accepted;}catch(const std::exception&) {}};
 std::thread a(reserve),b(reserve);go=true;a.join();b.join();
 if(accepted!=1||!fs::is_directory(concurrent.string()+".reservation"))throw std::runtime_error("atomic reservation failed");
 ++checks;
 bool rerun=false;try{pas::current_rails::reserve_fresh_offline_report(concurrent);}catch(...){rerun=true;}
 if(!rerun)throw std::runtime_error("reservation reopened");
 ++checks;
 std::cout<<"{\"checks\":"<<checks<<",\"pass\":true,\"scope\":\"offline output path guard\"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
