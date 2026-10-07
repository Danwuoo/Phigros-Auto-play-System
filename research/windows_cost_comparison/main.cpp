// Original run predicates and raw/process joins remain unchanged and frozen.
#define main pas_frozen_cost_analysis_entry
#include "../prelive_current/cost_analysis.cpp"
#undef main
#include "../windows_qualification/verifier.hpp"
#include "metrics.hpp"
namespace cc=pas::windows_cost_comparison;
namespace {
J strict_bound(const J& b){return pas::windows_qualification::read(bound_file(b));}
bool same_binding(const J& a,const J& b){
 return a.at("path")==b.at("path")&&a.at("sha256")==b.at("sha256")&&a.at("bytes")==b.at("bytes");
}
void unique_run(const J& entry,const J& report,std::set<std::string>& identities){
 const auto& p=report.at("provenance").at("process");
 for(const auto& token:{std::string("report:")+fs::canonical(entry.at("path").get<std::string>()).string(),
   std::string("sha:")+entry.at("sha256").get<std::string>(),std::string("process:")+p.at("pid").dump()+":"+p.at("creation_filetime").dump(),
   std::string("attempts:")+pas::evidence::sha256(entry.at("path").get<std::string>()+".attempts.json",12*MiB),
   std::string("journal:")+pas::evidence::sha256(entry.at("path").get<std::string>()+".journal.jsonl")})
  if(!identities.insert(token).second)throw std::runtime_error("reused report/process/raw run");
}
struct Context {Campaign aa,c;J noise;std::set<std::string> identities;};
Context context(const J& input,const fs::path& frozen_analyzer,const fs::path& self,const std::string& commit){
 if(input.at("schema")!="pas.windows-cost-comparison-input.v1")throw std::runtime_error("bound comparison input required");
 const auto aai=strict_bound(input.at("aa_input"));auto aa=campaign(aai,frozen_analyzer);
 if(aa.value.at("source_commit")!=commit||aai.at("normal_stopped").get<bool>()||aai.at("runs").size()!=12)throw std::runtime_error("complete unstopped same-candidate AA required");
 std::set<std::string> ids;std::map<std::string,std::vector<J>> loads;J rows=J::array();
 for(std::size_t i=0;i<12;++i){const auto& e=aai.at("runs")[i];auto r=load_run(e,aa.value.at("planned_runs")[i],aa);unique_run(e,r,ids);const auto val=validity(r);
  if(!val.at("pass").get<bool>())throw std::runtime_error("AA necessary validity failed");
  rows.push_back({{"run_id",e.at("run_id")},{"path",e.at("path")},{"sha256",e.at("sha256")},{"validity",val}});loads[e.at("scene")].push_back(std::move(r));
 }
 J calculated=J::object();
 for(const auto& scene:aa.scenes){const auto& reports=loads.at(scene);if(reports.size()!=4)throw std::runtime_error("four A runs per load");J metrics;
  for(const auto* metric:{"observer_ms","owner_ms","capture_to_owner_ms"})for(const auto* q:{"p95","p99"}){
   std::array<double,4> v{};for(int i=0;i<4;++i)v[i]=reports[i].at("measurement").at(metric).at(q);
   const auto n=cc::noise(v,metric);if(!n.adequate)throw std::runtime_error("AA noise inadequate");
   metrics[std::string(metric)+"."+q]={{"values",v},{"d",n.d},{"T",n.t},{"adequate",true}};
  }calculated[scene]={{"adequate",true},{"metrics",metrics}};
 }
 const auto noise=strict_bound(input.at("noise"));
 if(noise.at("schema")!="pas.current-rails-noise-freeze.v2"||!noise.at("ready").get<bool>()||!noise.at("comparison_allowed").get<bool>()||noise.at("candidate_results_used")!=0||noise.at("live_ready").get<bool>()||noise.at("campaign_id")!=aa.value.at("id")||noise.at("campaign_sha256")!=aa.sha||noise.at("normal_runs")!=rows||noise.at("noise")!=calculated)throw std::runtime_error("frozen noise differs from complete reverified AA");
 const auto& cb=input.at("campaign");Campaign c{strict_bound(cb),cb.at("sha256"),bound_file(cb),{},aa.scenes};
 if(c.value.at("schema")!="pas.current-rails-campaign-freeze.v2"||c.value.at("id")==aa.value.at("id")||c.value.at("id").get<std::string>().empty()||c.value.at("source_commit")!=commit||c.value.at("configuration")!="Release"||c.value.at("platform")!="windows-x64")throw std::runtime_error("comparison campaign identity");
 for(const auto* field:{"requested","warmup","measurement","cadence_ms","scenes","protocol_modified","live_ready","device_operations"})if(c.value.at(field)!=aa.value.at(field))throw std::runtime_error("comparison changes original cost contract");
 if(c.value.at("comparison_kind")!="ABBA"||c.value.at("old_STOP_resumed").get<bool>())throw std::runtime_error("fresh ABBA contract required");
 const auto& bindings=c.value.at("bindings");if(bindings.size()>32)throw std::runtime_error("comparison binding cap");
 for(const auto& b:bindings){const auto role=b.at("role").get<std::string>();if(!c.bindings.emplace(role,b).second)throw std::runtime_error("duplicate comparison role");bound_file(b,256*MiB);}
 // Only the fresh environment and maintenance orchestration may differ. The
 // same source, corpus, candidate, profile, guard and original analyzers are
 // required; all added AA/noise/tool bindings must be in each process closure.
 for(const auto& [role,b]:aa.bindings)if(role!="environment"&&role!="maintenance_recipe")if(c.bindings.at(role)!=b)throw std::runtime_error("comparison substitutes frozen AA member");
 if(!same_binding(c.bindings.at("aa_input"),input.at("aa_input"))||!same_binding(c.bindings.at("aa_noise"),input.at("noise"))||!same_binding(c.bindings.at("aa_campaign"),aai.at("campaign")))throw std::runtime_error("AA/noise not prebound to comparison campaign");
 if(bound_file(c.bindings.at("comparison_analyzer_binary"),256*MiB)!=fs::canonical(self))throw std::runtime_error("wrong comparison analyzer");
 const auto aenv=strict_bound(aa.bindings.at("environment")),benv=strict_bound(c.bindings.at("environment"));
 for(const auto* field:{"platform","os","processor","memory_bytes"})if(aenv.at(field)!=benv.at(field))throw std::runtime_error("comparison machine differs from AA");
 pas::cost_provenance::source_closure(c.value);pas::cost_provenance::effective_profile(c.bindings.at("profile"));
 const auto& plans=c.value.at("planned_runs");if(plans.size()!=36)throw std::runtime_error("36 frozen comparison runs required");
 std::set<std::string> planned_ids;std::set<fs::path> planned_paths;const std::array<const char*,4> modes={"A","B","B","A"};
 for(std::size_t i=0;i<36;++i){const auto& p=plans[i];if(p.at("scene")!=c.scenes[i/12]||p.at("mode")!=modes[i%4]||p.at("batch")!=(i%12)/4+1||!planned_ids.insert(p.at("run_id").get<std::string>()).second||!planned_paths.insert(fs::weakly_canonical(p.at("report_path").get<std::string>())).second)throw std::runtime_error("frozen load/batch/ABBA order");}
 return {std::move(aa),std::move(c),noise,std::move(ids)};
}
J batch_comparison(const std::array<J,4>& reports,const J& noise){
 J checks=J::object();bool pass=true,benefit=false;
 for(const auto* metric:{"observer_ms","owner_ms","capture_to_owner_ms"})for(const auto* q:{"p95","p99"}){
  std::array<double,4> values{};for(int i=0;i<4;++i)values[i]=reports[i].at("measurement").at(metric).at(q);
  const auto result=cc::compare(values,noise.at("metrics").at(std::string(metric)+"."+q).at("T"));pass&=result.non_regression;
  checks[std::string(metric)+"."+q]={{"values_ABBA",values},{"A_quantile_mean",result.a},{"B_quantile_mean",result.b},{"B_minus_A",result.b_minus_a},{"frozen_T",result.t},{"non_regression",result.non_regression},{"improved_beyond_T",result.improved}};
 }
 for(const auto* metric:{"observer_ms","owner_ms"})for(const auto* q:{"p95","p99"})benefit|=checks.at(std::string(metric)+"."+q).at("improved_beyond_T").get<bool>()&&checks.at(std::string("capture_to_owner_ms.")+q).at("improved_beyond_T").get<bool>();
 return {{"metrics",checks},{"non_regression_pass",pass},{"bottleneck_and_capture_improvement",benefit}};
}
}
int main(int argc,char** argv){try{
 if(argc!=13||std::string_view(argv[1])!="--operation"||std::string_view(argv[3])!="--input"||std::string_view(argv[5])!="--expected-sha256"||std::string_view(argv[7])!="--source-commit"||std::string_view(argv[9])!="--analyzer"||std::string_view(argv[11])!="--output")throw std::runtime_error("usage --operation run|compare --input INPUT --expected-sha256 SHA --source-commit COMMIT --analyzer FROZEN_ANALYZER --output FRESH");
 const std::string op=argv[2],sha=argv[6],commit=argv[8];const fs::path output=fs::absolute(argv[12]);
 if((op!="run"&&op!="compare")||!hash_text(sha)||!pas::windows_qualification::hex(commit,40)||fs::exists(output))throw std::runtime_error("operation/external identity/fresh output");
 const J pin={{"path",fs::absolute(argv[4]).string()},{"sha256",sha}};const auto input=strict_bound(pin);auto ctx=context(input,argv[10],argv[0],commit);J out;bool ready=false;
 if(op=="run"){
  const auto index=input.at("plan_index").get<std::size_t>();if(index>=36)throw std::runtime_error("comparison index bound");const auto& e=input.at("run");auto report=load_run(e,ctx.c.value.at("planned_runs")[index],ctx.c);unique_run(e,report,ctx.identities);const auto val=validity(report);ready=val.at("pass");
  out={{"schema","pas.windows-comparison-run-gate.v1"},{"run_id",e.at("run_id")},{"validity",val}};
 }else{
  const auto& entries=input.at("runs");if(entries.size()>36)throw std::runtime_error("comparison report cap");J rows=J::array(),batches=J::array();std::vector<J> reports;bool valid=true;
  for(std::size_t i=0;i<entries.size();++i){const auto& e=entries[i];auto report=load_run(e,ctx.c.value.at("planned_runs")[i],ctx.c);unique_run(e,report,ctx.identities);const auto val=validity(report);valid&=val.at("pass").get<bool>();rows.push_back({{"run_id",e.at("run_id")},{"validity",val}});reports.push_back(std::move(report));}
  valid&=entries.size()==36&&!input.at("normal_stopped").get<bool>();bool non_regression=valid,benefit_three_batches=false;
  if(valid)for(std::size_t load=0;load<3;++load){bool all_benefit=true;for(std::size_t batch=0;batch<3;++batch){const auto at=load*12+batch*4;const auto result=batch_comparison({reports[at],reports[at+1],reports[at+2],reports[at+3]},ctx.noise.at("noise").at(ctx.c.scenes[load]));non_regression&=result.at("non_regression_pass").get<bool>();all_benefit&=result.at("bottleneck_and_capture_improvement").get<bool>();batches.push_back({{"scene",ctx.c.scenes[load]},{"batch",batch+1},{"order","ABBA"},{"comparison",result}});}benefit_three_batches|=all_benefit;}
  ready=valid&&non_regression;out={{"schema","pas.windows-cost-comparison.v1"},{"normal_runs",rows},{"all_36_valid",valid},{"non_regression_pass",non_regression},{"C5_improvement_three_batches",benefit_three_batches},{"batches",batches}};
 }
 bound_file(pin);bound_file(input.at("campaign"));bound_file(input.at("aa_input"));bound_file(input.at("noise"));pas::cost_provenance::source_closure(ctx.c.value);
 out["ready"]=ready;out["campaign_id"]=ctx.c.value.at("id");out["campaign_sha256"]=ctx.c.sha;out["source_commit"]=commit;out["noise_binding"]=input.at("noise");out["live_ready"]=false;out["device_operations"]=0;
 out["scope"]="bound endpoint-free same-candidate ABBA cost only; improvement separately reported; no production/physical admission";
 std::ofstream file(output,std::ios::binary);file<<out.dump(2)<<'\n';if(!file)throw std::runtime_error("comparison output write");std::cout<<(ready?"OFFLINE COMPARISON PASS":"OFFLINE COMPARISON NOT_READY")<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
