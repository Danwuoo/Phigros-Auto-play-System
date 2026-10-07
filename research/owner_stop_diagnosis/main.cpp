#include "../prelive_current/evidence_hash.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <deque>
#include <fstream>
#include <iostream>
using J=nlohmann::json;namespace fs=std::filesystem;
using Ns=std::int64_t;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
J binding(const fs::path& p,std::uint64_t cap){return {{"path",fs::canonical(p).string()},{"bytes",fs::file_size(p)},{"sha256",pas::evidence::sha256(p,cap)}};}
J read(const fs::path& p,std::uint64_t cap){require(fs::file_size(p)<=cap,"input byte cap");std::ifstream f(p);require(bool(f),"read");return J::parse(f);}
int main(int argc,char** argv){try{
 require(argc==4&&!fs::exists(argv[3]),"usage raw-report pinned-report-sha fresh-output");
 const fs::path report=fs::canonical(argv[1]),attempts=report.string()+".attempts.json",journal=report.string()+".journal.jsonl";
 const auto rb=binding(report,1048576);require(rb.at("sha256")==argv[2],"pinned raw SHA");
 const auto ab=binding(attempts,16ull*1024*1024),jb=binding(journal,1024ull*1024*1024);
 const auto r=read(report,1048576),a=read(attempts,16ull*1024*1024);
 const std::size_t n=r.at("requested");require(n>=1000&&n<=10000&&a.is_array()&&a.size()==n,"full denominator");
 std::size_t first=0,last_owned=0,owned=0;Ns previous_capture=0;J timing=J::array(),overruns=J::array();
 for(std::size_t i=0;i<n;++i){const auto& row=a[i];require(row.at("attempt")==i+1,"attempt order");if(row.at("owned").get<bool>()){++owned;last_owned=i+1;}else if(row.at("consumed").get<bool>()&&last_owned&&!first)first=i+1;}
 require(owned==r.at("owner_seen")&&first>0&&!r.at("safe").get<bool>()&&r.at("supervisor_stopped").get<bool>(),"stopped run and owner join");
 const Ns cadence=r.at("cadence_ms").get<Ns>()*1000000;
 for(std::size_t i=0;i<n;++i){const auto& row=a[i];const Ns capture=row.at("capture"),end=row.at("owner_complete"),due=row.at("producer_due_ns"),render=row.at("render_end_ns"),pub=row.at("publication_return_ns");
  if(i+1>=first-std::min<std::size_t>(first-1,8)&&i+1<=first+8)timing.push_back({{"raw",row},{"capture_gap_ns",previous_capture?J(capture-previous_capture):J(nullptr)},{"capture_to_complete_ns",end?J(end-capture):J(nullptr)}});
  if(render>due+cadence||pub>due+cadence){require(overruns.size()<256,"overrun diagnosis cap");overruns.push_back({{"attempt",i+1},{"render_overrun_ns",render-due-cadence},{"publication_overrun_ns",pub-due-cadence}});}previous_capture=capture;
 }
 std::ifstream f(journal);std::string line;std::size_t rows=0;std::uint64_t frame=0;J evidence=J::array();std::array<std::size_t,3> phases{};std::size_t releases=0;bool unknown=false;J last_observer,last_filtered;
 while(std::getline(f,line)){require(++rows<=65536&&line.size()<=262144,"journal row cap");const auto e=J::parse(line);const std::string event=e.at("event");J brief={{"row",rows},{"event",event}};
  if(event=="current_observer_scene"||event=="filtered_owner_scene"){const auto& s=e.at("scene");frame=s.at("frame_sequence");brief["frame"]=frame;for(const auto* key:{"capture_complete_ns","recognition_start_ns","recognition_end_ns","playing_gate","capacity_valid","ui"})brief[key]=s.at(key);brief["targets"]=s.at("targets").size();if(event=="current_observer_scene")last_observer=brief;else last_filtered=brief;}
  else if(event=="complete_raw_candidate_batch"){frame=e.at("batch").at("context").at("frame");brief["frame"]=frame;brief["candidates"]=e.at("batch").at("candidates").size();}
  else if(event=="complete_current_chain"){frame=e.at("frame");brief=e;brief["row"]=rows;}
  else if(event=="own_injection_receipt"){auto p=e.at("phase").get<std::size_t>();require(p<3,"phase");++phases[p];unknown|=!e.at("success").get<bool>();brief=e;brief["row"]=rows;}
  else if(event=="release_receipt"){++releases;unknown|=!e.at("failed").empty()||!e.at("unknown").empty();brief=e;brief["row"]=rows;}
  else if(event=="own_accepted_plan"){brief=e;brief["row"]=rows;}
  if(frame+2>=first&&frame<=first+2){require(evidence.size()<128,"timeline bound");evidence.push_back(brief);}
 }
 require(rows==r.at("journal_records")&&phases[0]==r.at("own_down")&&phases[1]==r.at("own_move")&&phases[2]==r.at("own_up")&&releases==r.at("release_calls"),"journal count join");
 J out={{"schema","pas.owner-stop-raw-diagnosis.v1"},{"inputs",{{"report",rb},{"attempts",ab},{"journal",jb}}},{"source_commit",r.at("provenance").at("source_commit")},{"first_nonowned_after_owner",first},{"last_owned_attempt",last_owned},{"full_owned",owned},{"near_stop_timing",timing},{"producer_publication_overruns",overruns},{"near_stop_journal",evidence},{"last_observer",last_observer},{"last_filtered",last_filtered},{"unknown_receipt_or_release",unknown},{"rows",rows},{"reported_stop",r.at("stop")},{"causal_scope","raw timing localization; missing first-failure diagnostic is not invented"},{"qualification",false},{"device_operations",0}};
 require(binding(report,1048576)==rb&&binding(attempts,16ull*1024*1024)==ab&&binding(journal,1024ull*1024*1024)==jb,"inputs unchanged");
 std::ofstream output(argv[3],std::ios::out);output<<out.dump(2)<<'\n';require(bool(output),"output write");std::cout<<"first_nonowned="<<first<<" last_owned="<<last_owned<<" rows="<<rows<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
