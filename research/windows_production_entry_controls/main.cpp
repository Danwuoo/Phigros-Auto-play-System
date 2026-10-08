#include "pas/runtime.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
// Calls the actual linked Win32 production entry, with deliberately missing
// configuration/capability paths. UNQUALIFIED must be rejected before file,
// emulator discovery, capture, backend creation, or donor-session fallback.
int main(int argc,char** argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh output required");
 nlohmann::json checks=nlohmann::json::array();int failed=0;
 for(int i=0;i<8;++i){bool rejected=false;try{
  pas::run_current_candidate_manual_session(pas::CandidateLiveAdmission{},"C:/pas-unqualified-missing/config.json","C:/pas-unqualified-missing/capability.json",bool(i&1),i==7?-1:0,bool(i&2),bool(i&4),true);
 }catch(const std::runtime_error& e){rejected=std::string_view(e.what()).starts_with("current candidate live admission UNQUALIFIED:");}
 checks.push_back({{"case",i},{"actual_production_entry_rejects_UNQUALIFIED",rejected}});failed+=!rejected;
 }
 bool score_rejected=false;try{
  pas::run_current_candidate_score_only_session(pas::CandidateLiveAdmission{},"C:/pas-unqualified-missing/config.json","C:/pas-unqualified-missing/capability.json");
 }catch(const std::runtime_error& e){score_rejected=std::string_view(e.what()).starts_with("current candidate live admission UNQUALIFIED:");}
 checks.push_back({{"case","score_only_closed"},{"actual_production_entry_rejects_UNQUALIFIED",score_rejected}});failed+=!score_rejected;
 pas::RuntimeConfig config;config.width=1280;config.height=720;config.source_rotation=1;
 for(int i=0;i<4;++i){pas::Frame frame;frame.width=i==1?720:1280;frame.height=i==2?1280:720;frame.source_rotation=i==3?0:1;
  const bool passed=pas::current_capture_geometry_matches(config,frame)==(i==0);
  checks.push_back({{"case","fresh_capture_geometry_"+std::to_string(i)},{"pass",passed}});failed+=!passed;
 }
 nlohmann::json r={{"schema","pas.production-entry-closed-controls.v1"},{"assertions",checks.size()},{"failed",failed},{"checks",checks},{"actual_production_function_called",true},{"qualification_issued",false},{"device_operations",0},{"live_ready",false}};
 std::ofstream f(argv[1],std::ios::binary);f<<r.dump(2)<<'\n';if(!f)throw std::runtime_error("report write");std::cout<<"production closed-entry controls="<<checks.size()<<" failed="<<failed<<'\n';return failed?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
