#include "evidence_hash.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <set>
#include <vector>
using J=nlohmann::json;
namespace {
void require(bool v,const char*why){if(!v)throw std::runtime_error(why);}
J read_json(const std::filesystem::path&p,std::uint64_t cap){require(std::filesystem::file_size(p)<=cap,"JSON byte cap");std::ifstream f(p);return J::parse(f);}
J binding(const std::filesystem::path&p,std::uint64_t cap){return {{"path",p.string()},{"bytes",std::filesystem::file_size(p)},{"sha256",pas::evidence::sha256(p,cap)}};}
J distribution(std::vector<double> v){if(v.empty())return {{"n",0}};std::sort(v.begin(),v.end());auto p=[&](double x){double a=(v.size()-1)*x;auto lo=std::size_t(a),hi=std::min(lo+1,v.size()-1);return v[lo]+(v[hi]-v[lo])*(a-lo);};return {{"n",v.size()},{"p50",p(.5)},{"p95",p(.95)},{"p99",p(.99)},{"max",v.back()},{"jitter_p95_minus_p5",p(.95)-p(.05)}};}
struct Stats{std::vector<double> wall,cpu,cycles;double wall_sum=0,cpu_sum=0,cycles_sum=0;};
std::string processor_key(const J& p){require(p.is_object()&&p.size()==2,"processor endpoint shape");const auto group=p.at("group").get<unsigned>(),number=p.at("number").get<unsigned>();require(group<=65535&&number<64,"processor endpoint range");return std::to_string(group)+":"+std::to_string(number);}
J stats_json(const Stats&s){return {{"wall_ms",distribution(s.wall)},{"wall_sum_ms",s.wall_sum},{"thread_kernel_user_ms",distribution(s.cpu)},{"thread_kernel_user_sum_ms",s.cpu_sum},{"thread_cycles",distribution(s.cycles)},{"thread_cycles_sum",s.cycles_sum}};}
void add_stats(Stats&s,const J&v){const auto ns=v.at("wall_qpc_ns").get<std::int64_t>();s.wall.push_back(ns/1e6);s.wall_sum+=ns/1e6;if(!v.at("thread_kernel_user_ns").is_null()){const auto value=v.at("thread_kernel_user_ns").get<std::uint64_t>()/1e6;s.cpu.push_back(value);s.cpu_sum+=value;}if(!v.at("thread_cycles").is_null()){const auto value=v.at("thread_cycles").get<double>();s.cycles.push_back(value);s.cycles_sum+=value;}}
bool journal_row(std::istream& f,std::string& line){
 line.clear();char c;
 while(f.get(c)){if(c=='\n')return true;require(line.size()<262144,"journal row byte bound");line.push_back(c);}
 require(f.eof()&&!f.bad()&&line.empty(),"complete journal row termination");return false;
}
}
int main(int argc,char**argv){try{
 require(argc==4,"report expected-sha output");const std::filesystem::path report=argv[1],out=argv[3],attempts=report.string()+".attempts.json",journal=report.string()+".journal.jsonl";
 require(!std::filesystem::exists(out),"fresh output");const auto rb=binding(report,1048576),ab=binding(attempts,16777216),jb=binding(journal,1073741824);
 require(rb.at("sha256")==argv[2],"pinned report SHA");const auto r=read_json(report,1048576),a=read_json(attempts,16777216);
 const auto requested=r.at("requested").get<std::size_t>();require(requested>=1000&&requested<=10000&&a.size()==requested,"bounded complete attempt denominator");
 require(r.at("diagnostic_phase_profile_enabled")==true&&r.at("cost_qualification")==false&&r.at("device_endpoints")==0,"diagnostic scope");
 std::map<std::string,unsigned> processor_classes;const auto topology=r.value("processor_topology",J(nullptr));
 if(!topology.is_null()){require(topology.at("schema")=="pas.diagnostic-processor-topology.v1"&&topology.at("byte_cap")==65536&&topology.at("logical_processor_cap")==256&&topology.at("logical_processors").size()<=256&&topology.at("cost_qualification")==false,"bounded diagnostic topology");if(topology.at("complete")==true){require(topology.at("status")=="complete","complete topology status");for(const auto&p:topology.at("logical_processors")){const auto key=processor_key(J{{"group",p.at("group")},{"number",p.at("number")}});const auto c=p.at("efficiency_class").get<unsigned>();require(c<=255&&processor_classes.emplace(key,c).second,"unique raw topology class");}}}
 const std::set<std::string> json_names{"observer_process","current_scene_json_build","current_scene_output","candidate_batch_json_build","candidate_batch_output","hook_evaluate","filtered_scene_json_build","filtered_scene_output","owner_accept_poll_sync"};
 const std::set<std::string> snapshot_names{"observer_process","current_scene_snapshot_build","current_scene_output","candidate_batch_snapshot_build","candidate_batch_output","hook_evaluate","filtered_scene_snapshot_build","filtered_scene_output","owner_accept_poll_sync"};
 std::map<std::string,Stats> stats;std::map<std::string,std::map<std::string,Stats>> class_stats;std::map<std::string,std::map<std::string,std::size_t>> begins,ends;std::map<std::string,std::size_t> endpoint_changes,endpoint_unknown;std::vector<J> by_frame(requested+1);std::ifstream f(journal);std::string line;std::size_t rows=0,profile_count=0;std::uint64_t bytes=0;
 while(journal_row(f,line)){require(++rows<=65536&&line.size()<=262144,"journal row bounds");bytes+=line.size()+1;require(bytes<=1073741824,"journal byte bound");const auto e=J::parse(line);if(e.value("event","")!="diagnostic_phase_profile")continue;
  const auto&p=e.at("profile");const auto frame=p.at("source_frame").get<std::size_t>();require(frame>0&&frame<=requested&&by_frame[frame].is_null(),"unique bounded profile frame");
  const bool processors=p.at("schema")=="pas.diagnostic-phase-profile.v2";require((processors||p.at("schema")=="pas.diagnostic-phase-profile.v1")&&p.at("enabled")==true&&p.at("row_cap")==9&&p.at("rows").size()==9&&p.at("cost_qualification")==false,"profile scope and complete phases");
  std::set<std::string> found;double sum=0;for(const auto&v:p.at("rows")){const auto name=v.at("phase").get<std::string>();require((json_names.contains(name)||snapshot_names.contains(name))&&found.insert(name).second,"phase identities");const auto ns=v.at("wall_qpc_ns").get<std::int64_t>();require(ns>=0,"phase QPC monotonic");sum+=ns/1e6;
   std::string begin,end,cls="unknown";if(processors){const auto&b=v.at("processor_begin");const auto&e=v.at("processor_end");if(!b.is_null())begin=processor_key(b);if(!e.is_null())end=processor_key(e);if(!processor_classes.empty()){require((begin.empty()||processor_classes.contains(begin))&&(end.empty()||processor_classes.contains(end)),"endpoint joins complete topology");if(!begin.empty())cls=std::to_string(processor_classes.at(begin));}}
   if(frame>256){add_stats(stats[name],v);if(processors){if(begin.empty()||end.empty())++endpoint_unknown[name];else{++begins[name][begin];++ends[name][end];if(begin!=end)++endpoint_changes[name];}add_stats(class_stats[name][cls],v);}else ++endpoint_unknown[name];}
  }require(found==json_names||found==snapshot_names,"consistent encoding phases");by_frame[frame]={{"phases",p.at("rows")},{"phase_wall_sum_ms",sum}};++profile_count;
 }require(f.eof(),"complete journal read");
 std::vector<double> totals,phase_sums,residuals;std::vector<J> timeline;std::size_t owned=0,measured=0;
 for(std::size_t i=1;i<=requested;++i){const auto&t=a.at(i-1);require(t.at("attempt")==i,"all original attempt slots ordered");if(t.at("owned")!=true)continue;++owned;require(!by_frame[i].is_null(),"all owned frames have profile");if(i<=256)continue;++measured;
  const auto total=(t.at("owner_complete").get<std::int64_t>()-t.at("capture").get<std::int64_t>())/1e6;const double sum=by_frame[i].at("phase_wall_sum_ms");require(total>=sum,"phase wall fits same-frame complete chain");totals.push_back(total);phase_sums.push_back(sum);residuals.push_back(total-sum);timeline.push_back({{"frame",i},{"capture_to_owner_ms",total},{"phase_wall_sum_ms",sum},{"unmetered_wall_ms",total-sum},{"phases",by_frame[i].at("phases")}});
 }
 require(owned==profile_count&&owned==r.at("owner_seen")&&measured==r.at("measurement").at("owner_seen"),"complete owner/profile join");
 J metrics=J::object();for(const auto&[name,s]:stats){require(s.wall.size()==measured,"every phase measured denominator");metrics[name]=stats_json(s);metrics[name]["processor_begin_counts"]=begins[name];metrics[name]["processor_end_counts"]=ends[name];metrics[name]["different_processor_endpoints"]=endpoint_changes[name];metrics[name]["unknown_processor_endpoints"]=endpoint_unknown[name];J groups=J::object();for(const auto&[cls,values]:class_stats[name])groups[cls]=stats_json(values);metrics[name]["by_raw_begin_efficiency_class"]=groups;}
 std::sort(timeline.begin(),timeline.end(),[](const auto&x,const auto&y){return x.at("capture_to_owner_ms").template get<double>()>y.at("capture_to_owner_ms").template get<double>();});if(timeline.size()>32)timeline.resize(32);
 require(binding(report,1048576)==rb&&binding(attempts,16777216)==ab&&binding(journal,1073741824)==jb,"all raw inputs unchanged after analysis");
 J result={{"schema","pas.diagnostic-phase-summary.v2"},{"report",rb},{"attempts",ab},{"journal",jb},{"requested",requested},{"full_owned",owned},{"measured_owned",measured},{"latest_skipped_or_unowned",requested-owned},{"journal_rows",rows},{"phase_metrics",metrics},{"capture_to_owner_ms",distribution(totals)},{"same_frame_phase_wall_sum_ms",distribution(phase_sums)},{"same_frame_unmetered_wall_ms",distribution(residuals)},{"slowest_same_frame_components",timeline},{"processor_topology",topology},{"matching_processor_endpoints_do_not_exclude_migration",true},{"raw_integrity_join",true},{"cpu_quantization_measured",false},{"cpu_and_cycles_not_used_as_policy_or_converted_to_QPC",true},{"cost_qualification",false},{"live_ready",false},{"device_operations",0}};
 std::ofstream file(out,std::ios::binary);file<<result.dump(2)<<'\n';require(bool(file),"summary write");std::cout<<"complete diagnostic phase joins "<<owned<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
