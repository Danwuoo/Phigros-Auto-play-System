#include "../prelive_current/evidence_hash.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <fstream>
#include <iostream>
#include <map>
using J=nlohmann::json;namespace fs=std::filesystem;
struct Note {std::uint64_t first=0,last=0,observed=0,filtered=0,accepted=0,measurement_observed=0;double min_y=1e9,max_y=-1e9;};
int main(int argc,char** argv){try{
 if(argc!=5||fs::exists(argv[4]))throw std::runtime_error("usage report journal verified-audit fresh-output");
 const fs::path report=argv[1],journal=argv[2],auditp=argv[3];
 if(fs::file_size(report)>1048576||fs::file_size(auditp)>1048576||fs::file_size(journal)>1073741824)throw std::runtime_error("input size bound");
 std::ifstream rf(report),af(auditp),raw(journal);const auto r=J::parse(rf),a=J::parse(af);
 auto same=[&](const char* key,const fs::path& path,std::uint64_t cap){const auto& b=a.at("input_bindings").at(key);return fs::canonical(b.at("path").get<std::string>())==fs::canonical(path)&&b.at("sha256")==pas::evidence::sha256(path,cap)&&b.at("bytes")==fs::file_size(path);};
 if(!a.at("integrity").get<bool>()||!a.at("metrics_verified").get<bool>()||!a.at("plan_join_complete").get<bool>()||!same("report",report,1048576)||!same("journal",journal,1073741824))throw std::runtime_error("verified audit binding required");
 const auto cut=a.at("measurement_window_start_ns").get<std::int64_t>();std::map<std::uint64_t,Note> notes;std::array<std::uint64_t,3> phases{},measured{};std::uint64_t rows=0,scenes=0,blank=0;std::map<std::string,std::uint64_t> reasons;
 auto note=[&](std::uint64_t id)->Note&{if(!id||(!notes.contains(id)&&notes.size()>=4096))throw std::runtime_error("note identity capacity");return notes[id];};
 std::array<char,262145> buffer{};
 while(raw.getline(buffer.data(),buffer.size())){
  if(++rows>65536)throw std::runtime_error("journal row cap");const auto e=J::parse(buffer.data());const auto type=e.value("event","");
  if(type=="current_observer_scene"||type=="filtered_owner_scene"){
   const auto& s=e.at("scene");const auto frame=s.at("frame_sequence").get<std::uint64_t>();const auto& ts=s.at("targets");if(ts.size()>128)throw std::runtime_error("target row capacity");
   if(type=="current_observer_scene"){++scenes;blank+=ts.empty();}
   for(const auto& t:ts){auto& n=note(t.at("note_id"));if(type=="filtered_owner_scene"){++n.filtered;continue;}
    if(!n.first)n.first=frame;n.last=frame;++n.observed;n.measurement_observed+=frame>256;const double y=t.at("y");n.min_y=std::min(n.min_y,y);n.max_y=std::max(n.max_y,y);
    const auto why=t.at("reason").get<std::string>();if(why.size()>128||(!reasons.contains(why)&&reasons.size()>=64))throw std::runtime_error("reason capacity");++reasons[why];
   }
  }else if(type=="own_accepted_plan")++note(e.at("note")).accepted;
  else if(type=="own_injection_receipt"){const auto phase=e.at("phase").get<std::size_t>();if(phase>=3)throw std::runtime_error("receipt phase");++phases[phase];measured[phase]+=e.at("injection_start").get<std::int64_t>()>=cut;}
 }
 if(!raw.eof())throw std::runtime_error("oversized/malformed journal line");
 if(rows!=r.at("journal_records")||phases[0]!=r.at("own_down")||phases[1]!=r.at("own_move")||phases[2]!=r.at("own_up")||J(measured)!=a.at("measurement_phase_counts"))throw std::runtime_error("raw/audit denominator mismatch");
 J identities=J::array();for(const auto& [id,n]:notes)identities.push_back({{"note_id",id},{"first_frame",n.first},{"last_frame",n.last},{"observed_target_rows",n.observed},{"measurement_target_rows",n.measurement_observed},{"filtered_target_rows",n.filtered},{"accepted_plans",n.accepted},{"y_min",n.min_y},{"y_max",n.max_y}});
 auto binding=[](const fs::path& p,std::uint64_t cap){return J{{"path",fs::canonical(p).string()},{"bytes",fs::file_size(p)},{"sha256",pas::evidence::sha256(p,cap)}};};
 J out={{"schema","pas.tap-identity-attribution.v1"},{"report",binding(report,1048576)},{"journal",binding(journal,1073741824)},{"audit",binding(auditp,1048576)},{"rows",rows},{"observer_scenes",scenes},{"empty_target_scenes",blank},{"phase_counts",phases},{"measurement_phase_counts",measured},{"note_identities",identities},{"observer_reasons",reasons},{"scope","raw observer/filtered/plan identity attribution only; repeating pixels do not prove a new physical note"},{"device_operations",0},{"live_ready",false}};
 std::ofstream file(argv[4],std::ios::binary);file<<out.dump(2)<<'\n';if(!file)throw std::runtime_error("write");std::cout<<"notes="<<notes.size()<<" measured D/M/U="<<measured[0]<<'/'<<measured[1]<<'/'<<measured[2]<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
