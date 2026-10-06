#include <nlohmann/json.hpp>
#include "evidence_hash.hpp"
#include "campaign_provenance.hpp"
#include "input_closure.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
using J=nlohmann::json;
namespace fs=std::filesystem;
namespace {
constexpr std::uint64_t MiB=1024*1024;
J read(const fs::path& p){if(!fs::is_regular_file(p)||fs::file_size(p)>MiB)throw std::runtime_error("input file bound/type");std::ifstream f(p);if(!f)throw std::runtime_error("input missing");return J::parse(f);}
bool hash_text(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
fs::path bound_file(const J& b,std::uint64_t cap=MiB){auto p=fs::canonical(b.at("path").get<std::string>());const auto sha=b.at("sha256").get<std::string>();if(!hash_text(sha)||pas::evidence::sha256(p,cap)!=sha)throw std::runtime_error("input SHA mismatch");if(b.contains("bytes")&&fs::file_size(p)!=b.at("bytes").get<std::uint64_t>())throw std::runtime_error("input size mismatch");return p;}
void verify_raw_binding(const J& binding,const fs::path& actual,std::uint64_t cap){if(bound_file(binding,cap)!=fs::canonical(actual))throw std::runtime_error("audit raw path binding");}
J validity(const J& r){J failures=J::array();auto check=[&](bool yes,const char* why){if(!yes)failures.push_back(why);};
 check(r.at("schema")=="pas.prelive-complete-chain-cost.v1","pipeline_schema");
 check(r.at("requested")==2560&&r.at("attempts")==2560&&r.at("warmup_attempts")==256&&r.at("cadence_ms")==20,"frozen_attempt_contract");
 check(r.at("clock_domain")=="host_QPC"&&r.at("device_endpoints")==0&&r.at("source_absolute_age").is_null(),"clock_transport_contract");
 check(r.at("safe").get<bool>()&&!r.at("supervisor_stopped").get<bool>(),"chain_safe");check(r.at("release_verified").get<bool>(),"release");
 const auto& release=r.at("final_release");check(release.at("failed").empty()&&release.at("unknown").empty(),"release_uncertainty");
 check(r.at("pool_drops")==0,"pool_drop");check(r.at("writer_drops")==0&&!r.at("writer_fault").get<bool>()&&!r.at("journal_admission_failed").get<bool>(),"writer");check(r.at("injection_failed")==0,"input_failure");
 check(r.at("owner_seen_fraction").get<double>()>=.9,"full_coverage");const auto&m=r.at("measurement");check(m.at("attempts")==2304&&m.at("owner_seen_fraction").get<double>()>=.9&&m.at("owner_seen").get<int>()>=2000,"measurement_coverage_n");
 auto dist=[&](const J& d){if(d.at("n").get<std::size_t>()==0)return false;double prior=0;for(const auto* k:{"p50","p95","p99","max"}){double v=d.at(k);if(!std::isfinite(v)||v<prior)return false;prior=v;}double jitter=d.at("jitter_p95_minus_p5");return std::isfinite(jitter)&&jitter>=0&&jitter<=prior;};
 const auto&c=r.at("capture_to_owner_ms");check(dist(c)&&c.at("n")==r.at("owner_seen")&&c.value("p99",1e9)<=100&&c.value("max",1e9)<=250,"capture_owner_hard");
 if(r.at("scene")!="dense")check(r.at("own_down").get<int>()>0&&r.at("own_up").get<int>()>0,"action_opportunities_unknown_or_zero");
 const auto& l=r.at("all_phase_lateness_ms");if(r.at("scene")!="dense"||l.at("n").get<std::size_t>()>0)check(dist(l)&&l.value("p99",1e9)<=15&&l.value("max",1e9)<=100,"all_phase_lateness");
 for(const auto* metric:{"observer_ms","hook_ms","owner_ms","capture_to_owner_ms"})check(dist(m.at(metric))&&m.at(metric).at("n")==m.at("owner_seen"),"measurement_distribution");
 if(r.contains("_audited_measurement")){const auto& a=r.at("_audited_measurement");const auto& phases=a.at("phase_counts");const auto& lateness=a.at("lateness");if(r.at("scene")!="dense")check(phases.at(0).get<std::uint64_t>()>0&&phases.at(2).get<std::uint64_t>()>0,"measurement_action_opportunities_absent");if(lateness.at("n").get<std::uint64_t>()>0)check(dist(lateness)&&lateness.at("p99").get<double>()<=15&&lateness.at("max").get<double>()<=100,"measurement_phase_lateness");}
 if(r.at("scene")=="hold-closed-reset-v2")check(r.contains("producer_deadline_overruns")&&r.at("producer_deadline_overruns")==0&&r.at("publication_deadline_overruns")==0&&r.at("render_start_to_publication_return_ms").at("n")==r.at("requested")&&r.at("producer_render_ms").at("n")==r.at("requested")&&r.at("renderer_storage_bytes")==2764800&&r.at("generator")=="closed_reset_v2_streamed_all_offsets","streamed_workload_and_cadence");
 check(r.at("rss_peak_os").get<std::uint64_t>()<=128*MiB,"rss");return {{"pass",failures.empty()},{"failures",failures}};
}
struct Campaign { J value;std::string sha;fs::path path;std::map<std::string,J> bindings;std::vector<std::string> scenes;};
Campaign campaign(const J& input,const fs::path& self){
 if(input.at("schema")!="pas.current-rails-cost-input.v2")throw std::runtime_error("v2 bound input required; legacy input cannot qualify");
 const auto& b=input.at("campaign");Campaign c{read(bound_file(b)),b.at("sha256"),bound_file(b),{}, {}};
 if(c.value.at("schema")!="pas.current-rails-campaign-freeze.v2"||c.value.at("configuration")!="Release"||c.value.at("platform")!="windows-x64"||c.value.at("id").get<std::string>().empty())throw std::runtime_error("campaign identity/configuration");
 const auto commit=c.value.at("source_commit").get<std::string>();if(commit.size()!=40||commit.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("full source commit required");
 c.scenes=c.value.at("scenes").get<std::vector<std::string>>();if(c.scenes.size()!=3||c.scenes[0]!="tap"||(c.scenes[1]!="hold"&&c.scenes[1]!="hold-closed-reset-v2")||c.scenes[2]!="dense")throw std::runtime_error("predeclared scene order");
 const auto& binds=c.value.at("bindings");if(binds.size()>32)throw std::runtime_error("campaign binding capacity");
 for(const auto& entry:binds){const auto role=entry.at("role").get<std::string>();if(!c.bindings.emplace(role,entry).second)throw std::runtime_error("duplicate binding role");bound_file(entry,256*MiB);}
 for(const auto* role:{"candidate_binary","analyzer_binary","auditor_binary","profile","cost_protocol","environment","source_manifest"})if(!c.bindings.contains(role))throw std::runtime_error("incomplete source/build/profile binding");
 if(bound_file(c.bindings.at("analyzer_binary"),256*MiB)!=fs::canonical(self))throw std::runtime_error("wrong analyzer executable");
 if(c.bindings.at("cost_protocol").at("sha256")!="58d6136fcbce1ebf6c25a23795f10aab78013d46318c2b6261b7f6206abe91e6")throw std::runtime_error("original hard protocol changed");
 pas::cost_provenance::source_closure(c.value);
 pas::cost_provenance::effective_profile(c.bindings.at("profile"));
 const auto& plan=c.value.at("planned_runs");if(plan.size()!=12)throw std::runtime_error("frozen AA run count");std::set<std::string> ids;std::set<fs::path> paths;
 for(std::size_t i=0;i<plan.size();++i){const auto& p=plan[i];if(p.at("scene")!=c.scenes[i/4]||p.at("mode")!="A"||!ids.insert(p.at("run_id").get<std::string>()).second||!paths.insert(fs::weakly_canonical(p.at("report_path").get<std::string>())).second)throw std::runtime_error("frozen AA order/identity");}
 return c;
}
void process_stage(const J& entry,const J& planned,const Campaign& c,const fs::path& report,const J& provenance){
 const auto& b=entry.at("process_evidence");const auto sp=bound_file(b.at("specification")),fp=bound_file(b.at("freeze"),65536),vp=bound_file(b.at("verification")),rp=bound_file(b.at("result")),cp=bound_file(b.at("command")),tp=bound_file(b.at("state")),contractp=bound_file(b.at("contract"));
 const auto spec=read(sp),freeze=read(fp),v=read(vp),r=read(rp),command=read(cp),state=read(tp),contract=read(contractp);
 const auto stage=spec.at("stage");if(v.at("pending").get<bool>()||r.at("pending").get<bool>()||v.at("native_exit")!=0||v.at("runner_exit")!=0||v.at("verification_exit")!=0||!v.at("producer_complete").get<bool>()||v.at("stage")!=stage||r.at("stage")!=stage||command.at("stage")!=stage)throw std::runtime_error("process stage not verified");
 if(state.at("status")!="COMPLETE"||!state.at("running").is_null()||state.at("freeze_sha")!=b.at("freeze").at("sha256")||state.at("contract_sha")!=b.at("contract").at("sha256")||state.at("native_stages_consumed")!=1||state.at("native_launches_known")!=1)throw std::runtime_error("process state/freeze linkage");
 bool sealed=false;for(const auto& receipt:state.at("receipts"))if(bound_file(receipt)==vp&&receipt.at("sha256")==b.at("verification").at("sha256"))sealed=true;if(!sealed)throw std::runtime_error("verification not in COMPLETE state");
 const auto candidate=bound_file(c.bindings.at("candidate_binary"),256*MiB);const auto& f=r.at("facts");
 if(!f.at("created").get<bool>()||!f.at("identity_trusted").get<bool>()||!f.at("streams_completed").get<bool>()||!f.at("held_all_signaled").get<bool>()||!f.at("natural_quiescence").get<bool>()||f.at("active_final")!=0||f.at("runner_exit")!=0||f.at("exit_code")!=0||!f.at("errors").empty()||f.at("errors_overflow").get<bool>())throw std::runtime_error("process identity/quiescence failure");
 // Bound campaigns launch the candidate DIRECTLY through the unchanged owned
 // job core. A cmd.exe root cannot be substituted for this native identity.
 if(fs::canonical(spec.at("exe").get<std::string>())!=candidate||fs::canonical(f.at("root_image").get<std::string>())!=candidate||f.at("pid")!=provenance.at("process").at("pid")||f.at("creation_filetime")!=provenance.at("process").at("creation_filetime")||spec.at("native_exit")!=0||spec.at("runner_exit")!=0)throw std::runtime_error("direct producer process identity mismatch");
 const auto& argv=spec.at("argv");if(argv.size()!=7||argv[0]!="pipeline"||argv[1]!=planned.at("mode")||argv[2]!=planned.at("scene")||argv[3]!="2560"||fs::canonical(argv[4].get<std::string>())!=c.path||argv[5]!=planned.at("run_id")||fs::canonical(argv[6].get<std::string>())!=report)throw std::runtime_error("process exact argv mismatch");
 for(const auto* k:{"exe","argv","native_exit","runner_exit","total_s","stream_cap"})if(command.at("spec").at(k)!=spec.at(k)||contract.at("spec").at(k)!=spec.at(k))throw std::runtime_error("sealed launched command differs from spec");
 if(contract.at("device_allowed").get<bool>()||contract.at("spec").at("name")!=stage)throw std::runtime_error("offline process contract");
 std::map<fs::path,std::string> recorded;const auto& entries=v.at("entries");if(entries.empty()||entries.size()>128)throw std::runtime_error("verification entry bound");for(const auto& item:entries)recorded.emplace(bound_file(item,16*MiB),item.at("sha256").get<std::string>());for(const auto& item:{std::pair{rp,std::string("result")},std::pair{cp,std::string("command")}})if(!recorded.contains(item.first)||recorded.at(item.first)!=b.at(item.second).at("sha256").get<std::string>())throw std::runtime_error("process output not sealed by verifier");
 std::map<fs::path,std::string> inputs;const auto& files=freeze.at("files");if(files.empty()||files.size()>4096)throw std::runtime_error("process freeze capacity");for(const auto& item:files)if(!inputs.emplace(bound_file(item,256*MiB),item.at("sha256").get<std::string>()).second)throw std::runtime_error("duplicate process frozen input");
 for(const auto& needed:{sp,c.path,candidate})if(!inputs.contains(needed))throw std::runtime_error("process spec/campaign/candidate not frozen");
 if(!b.contains("input_closure"))throw std::runtime_error("verified input closure receipt absent");
 const auto indirect=pas::cost_input_closure::verify(b.at("input_closure"),b,stage,inputs);
 for(const auto& [path,sha]:indirect){const auto [at,inserted]=inputs.emplace(path,sha);if(!inserted&&at->second!=sha)throw std::runtime_error("input closure/direct source conflict");}
 const auto require_input=[&](const J& required){const auto path=bound_file(required,256*MiB);if(!inputs.contains(path)||inputs.at(path)!=required.at("sha256"))throw std::runtime_error("campaign/source input absent from verified process closure");};
 for(const auto& [role,binding]:c.bindings){(void)role;require_input(binding);}
 const auto source_inputs=pas::cost_provenance::source_closure(c.value);
 for(const auto& file:source_inputs.at("files"))require_input(file);
 for(const auto& binding:b.items())bound_file(binding.value());
}
J load_run(const J& entry,const J& planned,const Campaign& c){
 if(entry.at("run_id")!=planned.at("run_id")||entry.at("scene")!=planned.at("scene"))throw std::runtime_error("run order/label mismatch");
 const auto path=bound_file(entry);
 if(path!=fs::canonical(planned.at("report_path").get<std::string>()))throw std::runtime_error("run output path mismatch");
 auto r=read(path);
 process_stage(entry,planned,c,path,r.at("provenance"));
 if(r.at("scene")!=entry.at("scene")||r.at("mode")!=planned.at("mode"))throw std::runtime_error("report scene/mode mismatch");
 const J audit_binding={{"path",entry.at("audit_path")},{"sha256",entry.at("audit_sha256")}};auto audit=read(bound_file(audit_binding));
 if(audit.at("schema")!="pas.prelive-independent-integrity.v2"||!audit.at("integrity").get<bool>()||!audit.at("complete_prefix_evidence").get<bool>()||!audit.at("all_input_processed").get<bool>()||!audit.at("metrics_verified").get<bool>()||!audit.at("plan_join_complete").get<bool>()||!audit.at("coordinate_join_complete").get<bool>()||audit.at("release_uncertainty_seen").get<bool>())throw std::runtime_error("raw integrity/cost gate incomplete");
 if(r.at("scene")=="hold-closed-reset-v2"&&!audit.value("producer_metrics_verified",false))throw std::runtime_error("new workload producer timing not verified");
 if(!audit.at("provenance_join_complete").get<bool>()||audit.at("run_provenance")!=r.at("provenance"))throw std::runtime_error("producer report/Journal identity not joined");
 const auto& provenance=r.at("provenance");
 if(provenance.at("schema")!="pas.current-rails-run-provenance.v1"||provenance.at("campaign_id")!=c.value.at("id")||provenance.at("campaign_sha256")!=c.sha||provenance.at("source_commit")!=c.value.at("source_commit")||provenance.at("run_id")!=entry.at("run_id")||provenance.at("scene")!=entry.at("scene")||provenance.at("mode")!=planned.at("mode")||provenance.at("requested")!=2560||fs::canonical(provenance.at("report_path").get<std::string>())!=path)throw std::runtime_error("producer campaign/run mismatch");
 for(const auto* role:{"candidate_binary","source_manifest","profile","environment","cost_protocol"})if(provenance.at(std::string(role)+"_sha256")!=c.bindings.at(role).at("sha256"))throw std::runtime_error("producer source/build/profile mismatch");
 if(provenance.at("effective_profile")!=pas::cost_provenance::effective_profile(c.bindings.at("profile")))throw std::runtime_error("producer effective profile mismatch");
 const auto& process=provenance.at("process");if(process.at("pid").get<std::uint64_t>()==0||process.at("creation_filetime").get<std::uint64_t>()==0||process.at("run_start_ns").get<std::int64_t>()<=0)throw std::runtime_error("producer process identity invalid");
 const auto& measured=audit.at("measurement_phase_counts");const auto& lateness=audit.at("measurement_all_phase_lateness_ms");
 r["_audited_measurement"]={{"phase_counts",measured},{"lateness",lateness}};
 const auto& a=audit.at("input_bindings");verify_raw_binding(a.at("report"),path,MiB);verify_raw_binding(a.at("attempts"),path.string()+".attempts.json",12*MiB);verify_raw_binding(a.at("journal"),path.string()+".journal.jsonl",1073807360ull);
 if(audit.at("verifier_binary").at("sha256")!=c.bindings.at("auditor_binary").at("sha256"))throw std::runtime_error("auditor binary mismatch");
 // Bindings are checked again after parsed content has been consumed.
 bound_file(entry);bound_file(audit_binding);return r;
}
}
int main(int argc,char**argv){try{if(argc!=4||fs::exists(argv[3]))throw std::runtime_error("fresh analysis output");const auto input=read(argv[2]);const auto c=campaign(input,argv[0]);J out;bool ok=true;
 const std::string operation=argv[1];
 if(operation=="aa"){
  const auto& entries=input.at("runs");if(entries.size()>12)throw std::runtime_error("AA run bound");J rows=J::array(),noise=J::object();std::map<std::string,std::vector<J>> loads;std::set<fs::path> paths;std::set<std::string> hashes,processes,raw_attempts,raw_journals;
  for(std::size_t i=0;i<entries.size();++i){const auto& entry=entries[i];if(!paths.insert(fs::canonical(entry.at("path").get<std::string>())).second||!hashes.insert(entry.at("sha256").get<std::string>()).second)throw std::runtime_error("duplicate AA report");auto r=load_run(entry,c.value.at("planned_runs")[i],c);
   const auto& process=r.at("provenance").at("process");const auto identity=process.at("pid").dump()+":"+process.at("creation_filetime").dump();
   if(!processes.insert(identity).second||!raw_attempts.insert(pas::evidence::sha256(entry.at("path").get<std::string>()+".attempts.json",12*MiB)).second||!raw_journals.insert(pas::evidence::sha256(entry.at("path").get<std::string>()+".journal.jsonl")).second)throw std::runtime_error("repackaged run/producer reused");
   auto val=validity(r);ok&=val.at("pass").get<bool>();rows.push_back({{"run_id",entry.at("run_id")},{"path",entry.at("path")},{"sha256",entry.at("sha256")},{"validity",val}});loads[entry.at("scene")].push_back(std::move(r));}
  for(const auto& load:c.scenes){const auto& r=loads[load];if(r.size()!=4){ok=false;noise[load]={{"adequate",false},{"reason","four_frozen_A_runs_missing"},{"n",r.size()}};continue;}
   J m;bool adequate=true;for(const auto& metric:{"observer_ms","owner_ms","capture_to_owner_ms"})for(const auto& q:{"p95","p99"}){double a[4];for(int i=0;i<4;++i)a[i]=r[i].at("measurement").at(metric).at(q);const auto d=std::max(std::abs(a[0]-a[1]),std::abs(a[2]-a[3]));const double floor=std::string_view(metric)=="observer_ms"?.5:std::string_view(metric)=="owner_ms"?.25:3.;const bool yes=2*d<=std::max(*std::min_element(a,a+4)*.3,floor);adequate&=yes;m[std::string(metric)+"."+q]={{"values",a},{"d",d},{"T",std::max(floor,2*d)},{"adequate",yes}};}ok&=adequate;noise[load]={{"adequate",adequate},{"metrics",m}};
  }
  ok&=!input.at("normal_stopped").get<bool>();out={{"schema","pas.current-rails-noise-freeze.v2"},{"ready",ok},{"normal_runs",rows},{"noise",noise},{"comparison_allowed",ok},{"candidate_results_used",0}};
 }else if(operation=="run"){
  const auto index=input.at("plan_index").get<std::size_t>();if(index>=12)throw std::runtime_error("plan index bound");auto r=load_run(input.at("run"),c.value.at("planned_runs")[index],c);auto val=validity(r);out={{"schema","pas.current-rails-run-gate.v2"},{"path",input.at("run").at("path")},{"validity",val}};ok=val.at("pass");
 }else throw std::runtime_error("analysis operation");
 out["campaign_id"]=c.value.at("id");out["campaign_sha256"]=c.sha;out["scope"]="bound offline cold cost evidence only; Windows process qualification and physical/live gates are separate";out["live_ready"]=false;
 std::ofstream f(argv[3]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("write");std::cout<<(ok?"READY":"NOT_READY")<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
