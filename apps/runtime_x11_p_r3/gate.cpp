#include "contract.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
using json=nlohmann::json;
json read(const std::filesystem::path& p){std::ifstream f(p);if(!f)throw std::runtime_error("missing "+p.string());return json::parse(f);}
void save(const std::filesystem::path& p,const json& j){if(std::filesystem::exists(p))throw std::runtime_error("existing_output");std::ofstream f(p);if(!f)throw std::runtime_error("gate output open");f<<j.dump(2)<<'\n';f.flush();if(!f)throw std::runtime_error("gate output write");}
using r3::normal;
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("usage noise|evaluate BATCH");const std::filesystem::path root=argv[2];
 const std::array<const char*,3> keys{"recognition","owner","capture_to_owner"};
 json result;bool pass=true;
 if(std::string(argv[1])=="check"){auto r=read(root/"summary.json");std::cout<<json{{"normal_gate",normal(r)},{"action_timing",r.value("action_expectation","")=="none"?"not-applicable":"required"}}.dump();return normal(r)?0:2;}
 if(std::string(argv[1])=="noise"){
  for(auto load:{"rgb","owner"}){std::array<json,4> runs;for(int i=0;i<4;++i){runs[i]=read(root/("aa-"+std::string(load)+"-"+std::to_string(i+1))/"summary.json");pass=normal(runs[i])&&pass;result["normal_hard_gates"][load][std::to_string(i+1)]=normal(runs[i]);}
   for(int k=0;k<3;++k)for(auto q:{"p95","p99"}){double d=0,mn=1e20;for(int i=0;i<4;++i)mn=std::min(mn,runs[i]["windows"]["measurement"]["metrics_ms"][keys[k]][q].get<double>());
    for(int i=0;i<4;i+=2)d=std::max(d,std::abs(runs[i]["windows"]["measurement"]["metrics_ms"][keys[k]][q].get<double>()-runs[i+1]["windows"]["measurement"]["metrics_ms"][keys[k]][q].get<double>()));
    const double floor=std::array<double,3>{.5,.25,3}[k];const bool stable=2*d<=std::max(mn*.30,floor);pass=pass&&stable;
    result[load][keys[k]][q]={{"max_pair_difference_ms",d},{"floor_ms",floor},{"tolerance_ms",std::max(floor,2*d)},{"minimum_aa_metric_ms",mn},{"noise_adequate",stable}};
   }
  }result["noise_gate"]=pass;result["frozen_before_candidate_cost"]=true;save(root/"noise-frozen.json",result);
 }else if(std::string(argv[1])=="evaluate"){
  const auto noise=read(root/"noise-frozen.json");pass=noise.at("noise_gate").get<bool>();
  for(auto load:{"rgb","owner"})for(int b=1;b<=2;++b){std::array<json,4> r;int i=0;for(auto role:{"B0","B1","B1","B0"}){r[i]=read(root/("ab-"+std::string(load)+"-"+std::to_string(b)+"-"+std::to_string(i+1)+"-"+role)/"summary.json");pass=pass&&normal(r[i]);++i;}
   for(auto key:keys)for(auto q:{"p95","p99"}){double a=(r[0]["windows"] ["measurement"] ["metrics_ms"][key][q].get<double>()+r[3]["windows"] ["measurement"] ["metrics_ms"][key][q].get<double>())/2;
    double v=(r[1]["windows"] ["measurement"] ["metrics_ms"][key][q].get<double>()+r[2]["windows"] ["measurement"] ["metrics_ms"][key][q].get<double>())/2;double tol=noise[load][key][q]["tolerance_ms"];
    bool p=v-a<=tol;pass=pass&&p;result["comparisons"].push_back({{"workload",load},{"batch",b},{"metric",key},{"quantile",q},{"B0_run_quantile_mean_ms",a},{"B1_run_quantile_mean_ms",v},{"difference_ms",v-a},{"tolerance_ms",tol},{"passed",p}});
   }
  }
  for(const auto& f:std::filesystem::directory_iterator(root))if(f.is_directory()&&f.path().filename().string().starts_with("stress-")){const auto r=read(f.path()/"summary.json");pass=pass&&r.at("hard_gate").get<bool>();}
  result["cost_gate"]=pass;result["noise_gate"]=noise["noise_gate"];result["quantile_means_are_not_pooled"]=true;save(root/"cost-evaluation.json",result);
 }else throw std::runtime_error("mode");std::cout<<result.dump()<<'\n';return pass?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
