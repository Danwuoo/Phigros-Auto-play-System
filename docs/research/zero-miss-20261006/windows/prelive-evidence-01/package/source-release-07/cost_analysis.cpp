#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
using J=nlohmann::json;
J read(const std::string& p){std::ifstream f(p);if(!f)throw std::runtime_error("input missing");return J::parse(f);}
J validity(const J& r){J failures=J::array();auto check=[&](bool yes,const char* why){if(!yes)failures.push_back(why);};
 check(r.at("attempts")==r.at("requested"),"all_attempts");check(r.at("safe").get<bool>(),"chain_safe");check(r.at("release_verified").get<bool>(),"release");
 check(r.at("pool_drops")==0,"pool_drop");check(r.at("writer_drops")==0&&!r.at("writer_fault").get<bool>(),"writer");check(r.at("injection_failed")==0,"input_failure");
 check(r.at("owner_seen_fraction").get<double>()>=.9,"full_coverage");const auto&m=r.at("measurement");check(m.at("owner_seen_fraction").get<double>()>=.9&&m.at("owner_seen").get<int>()>=2000,"measurement_coverage_n");
 const auto&c=r.at("capture_to_owner_ms");check(c.value("n",0)>0&&c.value("p99",1e9)<=100&&c.value("max",1e9)<=250,"capture_owner_hard");
 if(r.at("scene")!="dense"){const auto&l=r.at("all_phase_lateness_ms");check(r.at("own_down").get<int>()>0&&r.at("own_up").get<int>()>0,"action_opportunities_unknown_or_zero");check(l.value("n",0)>0&&l.value("p99",1e9)<=15&&l.value("max",1e9)<=100,"all_phase_lateness");}
 check(r.at("rss_peak_sampled").get<std::uint64_t>()<=128ull*1024*1024,"rss");return {{"pass",failures.empty()},{"failures",failures}};
}
int main(int argc,char**argv){try{if(argc!=4||std::filesystem::exists(argv[3]))throw std::runtime_error("fresh analysis output");const auto input=read(argv[2]);J out;bool ok=true;
 if(std::string_view(argv[1])=="aa"){J rows=J::array(),noise=J::object();for(const auto&load:{"tap","hold","dense"}){std::vector<J> r;for(const auto&v:input.at("runs")){if(v.at("scene")==load){auto x=read(v.at("path"));if(x.at("mode")!="A")throw std::runtime_error("noise cannot use candidate");auto val=validity(x);ok&=val.at("pass").get<bool>();rows.push_back({{"path",v.at("path")},{"validity",val}});r.push_back(x);}}
  if(r.size()!=4){ok=false;noise[load]={{"adequate",false},{"reason","four_frozen_A_runs_missing"},{"n",r.size()}};continue;}
  J m;bool adequate=true;for(const auto&metric:{"observer_ms","owner_ms","capture_to_owner_ms"})for(const auto&q:{"p95","p99"}){double a[4];for(int i=0;i<4;++i)a[i]=r[i].at("measurement").at(metric).at(q);const auto d=std::max(std::abs(a[0]-a[1]),std::abs(a[2]-a[3]));const double floor=std::string_view(metric)=="observer_ms"?.5:std::string_view(metric)=="owner_ms"?.25:3.;const bool yes=2*d<=std::max(*std::min_element(a,a+4)*.3,floor);adequate&=yes;m[std::string(metric)+"."+q]={{"values",a},{"d",d},{"T",std::max(floor,2*d)},{"adequate",yes}};}ok&=adequate;noise[load]={{"adequate",adequate},{"metrics",m}};
 }out={{"schema","pas.current-rails-noise-freeze.v1"},{"ready",ok},{"normal_runs",rows},{"noise",noise},{"comparison_allowed",ok},{"candidate_results_used",0}};
 }else if(std::string_view(argv[1])=="run"){auto r=read(input.at("path"));out={{"schema","pas.current-rails-run-gate.v1"},{"path",input.at("path")},{"validity",validity(r)}};ok=out.at("validity").at("pass");}
 else throw std::runtime_error("analysis operation");std::ofstream f(argv[3]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("write");std::cout<<(ok?"READY":"NOT_READY")<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
