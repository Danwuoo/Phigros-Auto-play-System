// Independent admission review. All inputs are explicit process/auditor test
// doubles produced by cost_analysis_tests; nothing here measures Windows cost.
#define main reviewed_cost_analysis_entry
#include "../cost_analysis.cpp"
#undef main
#include <functional>
#include <sstream>

namespace {
J binding(const fs::path& p) { return {{"path",fs::canonical(p).string()},{"bytes",fs::file_size(p)},{"sha256",pas::evidence::sha256(p)}}; }
void save(const fs::path& p,const J& value) { std::ofstream f(p,std::ios::binary);f<<value.dump(2)<<'\n';if(!f)throw std::runtime_error("review fixture write"); }
J restore_receipts=J::array();
struct Restore {
 std::map<fs::path,std::string> files;
 void write(const fs::path& p,const J& value) { if(!files.contains(p)){std::ifstream f(p,std::ios::binary);files[p]={std::istreambuf_iterator<char>(f),{}};}save(p,value); }
 ~Restore() noexcept(false){for(const auto& [p,s]:files){
  {std::ofstream f(p,std::ios::binary);f<<s;f.flush();if(!f)throw std::runtime_error("fixture restore write");}
  pas::evidence::Sha256 digest;digest.update(s.data(),s.size());const auto expected=digest.finish(),actual=pas::evidence::sha256(p);
  if(actual!=expected)throw std::runtime_error("fixture restore SHA");
  restore_receipts.push_back({{"path",p.string()},{"original_sha256",expected},{"restored_sha256",actual},{"bytes",s.size()}});
 }}
};
}
int main(int argc,char**argv){try {
 if(argc!=3)throw std::runtime_error("usage generated-unit-fixture-root fresh-review-output-root");
 const auto fixture=fs::canonical(argv[1]),out=fs::absolute(argv[2]);if(fs::exists(out))throw std::runtime_error("fresh review root");fs::create_directories(out);
 const auto original=read(fixture/"positive-contract-double-input.json");const auto campaign_file=fs::path(original.at("campaign").at("path").get<std::string>());
 const auto cj=read(campaign_file);fs::path original_analyzer;for(const auto& b:cj.at("bindings"))if(b.at("role")=="analyzer_binary")original_analyzer=b.at("path").get<std::string>();
 const auto c=campaign(original,original_analyzer);const auto e0=original.at("runs").at(0);const auto plan0=c.value.at("planned_runs").at(0);const auto r0=read(e0.at("path").get<std::string>());J checks=J::array();
 auto reject=[&](const std::string& id,const std::string& expected,const std::function<void()>& f){std::string error;try{f();}catch(const std::exception& ex){error=ex.what();}const bool pass=error.find(expected)!=std::string::npos;checks.push_back({{"id",id},{"pass",pass},{"expected_error",expected},{"observed_error",error}});};
 auto check=[&](const std::string& id,bool pass){checks.push_back({{"id",id},{"pass",pass}});};
 const auto source_manifest=pas::cost_provenance::source_closure(c.value);
 J source_before=J::array();for(const auto& file:source_manifest.at("files"))source_before.push_back(binding(file.at("path").get<std::string>()));
 if(source_before.empty())throw std::runtime_error("nonempty source snapshot required");
 {
  Restore restore;const auto& second=original.at("runs").at(1);restore.write(second.at("path").get<std::string>()+".attempts.json",{{"unit_test_double",second.at("run_id")}});
  const auto ip=out/"positive-AA-input.json",op=out/"positive-AA-output.json";save(ip,original);
  std::string a0=original_analyzer.string(),a1="aa",a2=ip.string(),a3=op.string();char* args[]={a0.data(),a1.data(),a2.data(),a3.data()};
  const int rc=reviewed_cost_analysis_entry(4,args);if(rc!=0||!read(op).at("comparison_allowed").get<bool>()||read(op).at("live_ready").get<bool>())throw std::runtime_error("full positive AA fixture must pass before attacks");
  check("positive_AA_test_double_before_attacks",true);
 }
 check("positive_loader_test_double",load_run(e0,plan0,c).at("provenance")==r0.at("provenance"));
 // Final shared-step closure: omitting any new decision/receipt/admission/payload
 // member must fail even when the edited manifest itself is consistently sealed.
 for(const auto* required:{"research/prelive_current/input_closure.hpp","research/prelive_current/owner_candidate_step.hpp","research/prelive_current/receipt_stream.hpp","include/pas/candidate_admission.hpp","include/pas/candidate_packet.hpp","include/pas/runtime.hpp","src/manual_session.cpp","CMakeLists.txt"}){
  auto omitted=source_manifest;auto& members=omitted["files"];bool removed=false;
  for(auto it=members.begin();it!=members.end();)if(it->at("relative_path")==required){it=members.erase(it);removed=true;}else ++it;
  if(!removed)throw std::runtime_error("new seam fixture closure member absent");
  const auto manifest=out/(std::string("missing-")+fs::path(required).filename().string()+".json");save(manifest,omitted);
  auto changed=c.value;for(auto& b:changed["bindings"])if(b.at("role")=="source_manifest"){b=binding(manifest);b["role"]="source_manifest";}
  reject(std::string("missing_new_seam_source:")+required,"required source closure member absent",[&]{pas::cost_provenance::source_closure(changed);});
 }

 reject("cross_campaign_id","producer campaign/run mismatch",[&]{auto changed=c;changed.value["id"]="another-campaign";load_run(e0,plan0,changed);});
 reject("cross_campaign_sha","producer campaign/run mismatch",[&]{auto changed=c;changed.sha=std::string(64,'f');load_run(e0,plan0,changed);});
 reject("cross_source_commit","producer campaign/run mismatch",[&]{auto changed=c;changed.value["source_commit"]=std::string(40,'b');load_run(e0,plan0,changed);});
 {const auto other=out/"foreign-environment.json";save(other,{{"unrelated_environment",true}});reject("foreign_environment_binding","producer source/build/profile mismatch",[&]{auto changed=c;changed.bindings["environment"]=binding(other);load_run(e0,plan0,changed);});}
 {const auto other=out/"foreign-source-manifest.json";save(other,{{"unrelated_source_manifest",true}});reject("foreign_source_manifest_binding","producer source/build/profile mismatch",[&]{auto changed=c;changed.bindings["source_manifest"]=binding(other);load_run(e0,plan0,changed);});}
 {const auto other=out/"foreign-freeze.json";auto frozen=read(e0.at("process_evidence").at("freeze").at("path").get<std::string>());frozen["foreign_freeze"]=true;save(other,frozen);reject("foreign_freeze_with_valid_SHA","process state/freeze linkage",[&]{auto changed=e0;changed["process_evidence"]["freeze"]=binding(other);load_run(changed,plan0,c);});}
 // Rebind all downstream hashes so rejection must reach the semantic join,
 // rather than succeeding only because an old checksum happened to remain.
 auto reseal_closure=[&](J& entry,Restore& restore){auto& b=entry["process_evidence"];const fs::path p=b.at("input_closure").at("path").get<std::string>();auto receipt=read(p);receipt["state"]=b.at("state");receipt["verification"]=b.at("verification");restore.write(p,receipt);b["input_closure"]=binding(p);};
 auto reseal_result=[&](J& entry,Restore& restore,const std::function<void(J&)>& mutate){
  auto& b=entry["process_evidence"];const fs::path rp=b.at("result").at("path").get<std::string>(),vp=b.at("verification").at("path").get<std::string>(),sp=b.at("state").at("path").get<std::string>();
  auto result=read(rp);mutate(result);restore.write(rp,result);b["result"]=binding(rp);
  auto verified=read(vp);for(auto& v:verified["entries"])if(fs::path(v.at("path").get<std::string>())==rp)v=b["result"];restore.write(vp,verified);b["verification"]=binding(vp);
  auto state=read(sp);for(auto& v:state["receipts"])if(fs::path(v.at("path").get<std::string>())==vp)v=b["verification"];restore.write(sp,state);b["state"]=binding(sp);reseal_closure(entry,restore);
 };

 auto reseal_process_leaf=[&](J& entry,Restore& restore,const std::string& role,const std::function<void(J&)>& mutate){
  auto& b=entry["process_evidence"];const fs::path leaf=b.at(role).at("path").get<std::string>(),vp=b.at("verification").at("path").get<std::string>(),sp=b.at("state").at("path").get<std::string>(),fp=b.at("freeze").at("path").get<std::string>();
  auto value=read(leaf);mutate(value);restore.write(leaf,value);b[role]=binding(leaf);auto state=read(sp);
  if(role=="command") {auto verified=read(vp);for(auto& v:verified["entries"])if(fs::path(v.at("path").get<std::string>())==leaf)v=b[role];restore.write(vp,verified);b["verification"]=binding(vp);for(auto& v:state["receipts"])if(fs::path(v.at("path").get<std::string>())==vp)v=b["verification"];}
  if(role=="specification") {auto freeze=read(fp);for(auto& v:freeze["files"])if(fs::path(v.at("path").get<std::string>())==leaf)v=b[role];restore.write(fp,freeze);b["freeze"]=binding(fp);state["freeze_sha"]=b["freeze"]["sha256"];}
  if(role=="freeze")state["freeze_sha"]=b["freeze"]["sha256"];
  if(role=="contract")state["contract_sha"]=b["contract"]["sha256"];
  restore.write(sp,state);b["state"]=binding(sp);reseal_closure(entry,restore);
 };
 {Restore restore;auto changed=e0;reseal_process_leaf(changed,restore,"command",[](J& d){d["spec"]["argv"][2]="hold";});reject("rehashed_command_differs_from_spec","sealed launched command differs from spec",[&]{load_run(changed,plan0,c);});}
 {Restore restore;auto changed=e0;reseal_process_leaf(changed,restore,"specification",[](J& d){d["argv"][2]="hold";});reject("rehashed_spec_violates_declared_scene","process exact argv mismatch",[&]{load_run(changed,plan0,c);});}
 {Restore restore;auto changed=e0;reseal_process_leaf(changed,restore,"contract",[](J& d){d["device_allowed"]=true;});reject("rehashed_device_permission_contract","offline process contract",[&]{load_run(changed,plan0,c);});}
 {Restore restore;auto changed=e0;reseal_process_leaf(changed,restore,"freeze",[&](J& d){const auto specification=changed["process_evidence"]["specification"]["path"];auto& files=d["files"];for(auto it=files.begin();it!=files.end();)if(it->at("path")==specification)it=files.erase(it);else ++it;});reject("rehashed_freeze_missing_spec","process spec/campaign/candidate not frozen",[&]{load_run(changed,plan0,c);});}
 {Restore restore;auto changed=e0;reseal_result(changed,restore,[&](J& d){d["facts"]["root_image"]=fs::canonical(argv[0]).string();});reject("rehashed_foreign_root_image","direct producer process identity mismatch",[&]{load_run(changed,plan0,c);});}
 for(const auto* field:{"pid","creation_filetime"}){Restore restore;auto changed=e0;reseal_result(changed,restore,[&](J& r){r["facts"][field]=r["facts"][field].get<std::uint64_t>()+999;});reject(std::string("rehashed_native_")+field,"direct producer process identity mismatch",[&]{load_run(changed,plan0,c);});}
 {Restore restore;auto changed=e0;reseal_result(changed,restore,[](J& r){r["facts"]["identity_trusted"]=false;});reject("rehashed_untrusted_native","process identity/quiescence failure",[&]{load_run(changed,plan0,c);});}
 {Restore restore;auto changed=e0;const fs::path sp=changed["process_evidence"]["state"]["path"].get<std::string>();auto state=read(sp);state["status"]="STOP";restore.write(sp,state);changed["process_evidence"]["state"]=binding(sp);reject("rehashed_STOP_state","process state/freeze linkage",[&]{load_run(changed,plan0,c);});}
 // Unit generator intentionally leaves one raw-tamper control in run 2. Restore
 // its exact known test-double bytes only for this scoped duplicate test.
 auto invoke_reject=[&](const std::string& id,J in,const std::string& expected){const auto ip=out/(id+"-input.json"),op=out/(id+"-output.json");save(ip,in);std::string a0=original_analyzer.string(),a1="aa",a2=ip.string(),a3=op.string();char* args[]={a0.data(),a1.data(),a2.data(),a3.data()};std::ostringstream errors;auto* old=std::cerr.rdbuf(errors.rdbuf());int rc=reviewed_cost_analysis_entry(4,args);std::cerr.rdbuf(old);const bool pass=rc==2&&errors.str().find(expected)!=std::string::npos;checks.push_back({{"id",id},{"pass",pass},{"exit",rc},{"expected_error",expected},{"observed_error",errors.str()}});};
 {auto changed=original;changed["runs"][1]=changed["runs"][0];invoke_reject("duplicated_AA_report",changed,"duplicate AA report");}
 {Restore restore;auto changed=original;auto& entry=changed["runs"][1];const fs::path report=entry.at("path").get<std::string>(),audit=entry.at("audit_path").get<std::string>();
  const fs::path attempts=report.string()+".attempts.json";restore.write(attempts,{{"unit_test_double",entry.at("run_id")}});
  auto r=read(report);r["provenance"]["process"]["pid"]=r0["provenance"]["process"]["pid"];r["provenance"]["process"]["creation_filetime"]=r0["provenance"]["process"]["creation_filetime"];restore.write(report,r);auto rb=binding(report);entry["bytes"]=rb["bytes"];entry["sha256"]=rb["sha256"];
  auto a=read(audit);a["run_provenance"]=r["provenance"];a["input_bindings"]["report"]=rb;restore.write(audit,a);entry["audit_sha256"]=pas::evidence::sha256(audit);
  reseal_result(entry,restore,[&](J& result){result["facts"]["pid"]=r["provenance"]["process"]["pid"];result["facts"]["creation_filetime"]=r["provenance"]["process"]["creation_filetime"];});
  invoke_reject("rehashed_duplicate_native_producer",changed,"repackaged run/producer reused");
 }
 {Restore restore;auto changed=original;auto& entry=changed["runs"][1];const fs::path report=entry.at("path").get<std::string>(),audit=entry.at("audit_path").get<std::string>();const fs::path attempts=report.string()+".attempts.json";restore.write(attempts,read(e0.at("path").get<std::string>()+".attempts.json"));auto a=read(audit);a["input_bindings"]["attempts"]=binding(attempts);restore.write(audit,a);entry["audit_sha256"]=pas::evidence::sha256(audit);invoke_reject("rehashed_duplicate_raw_attempts",changed,"repackaged run/producer reused");}
 check("original_run_restored",load_run(e0,plan0,c).at("provenance")==r0.at("provenance"));
 for(const auto& before:source_before)check(std::string("source_unchanged:")+before.at("path").get<std::string>(),binding(before.at("path").get<std::string>())==before);
 bool pass=true;for(const auto& row:checks)pass=pass&&row.at("pass").get<bool>();J result={{"schema","pas.cost-analysis-independent-review-tests.v1"},{"scope","C++ contract tests over simulated process/auditor receipts; no Windows cost or device execution"},{"source",binding(fs::path(__FILE__))},{"fixture_campaign",original.at("campaign")},{"restored_fixture_files",restore_receipts},{"source_before_and_after",source_before},{"checks",checks},{"pass",pass},{"actual_cost_ready",false}};save(out/"review_result.json",result);std::cout<<result.dump(2)<<'\n';return pass?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
