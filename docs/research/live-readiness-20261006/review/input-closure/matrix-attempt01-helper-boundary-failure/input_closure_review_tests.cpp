// Independent bounded input-closure admission controls. Seeded process/auditor
// receipts are explicit test doubles. Only fresh copies are changed here; no
// Windows process, physical device, measured-cost or live qualification occurs.
#define main reviewed_input_closure_cost_analysis_entry
#include "../cost_analysis.cpp"
#undef main
#include <functional>

namespace {
J closure_binding(const fs::path& p) {
 return {{"path",fs::canonical(p).string()},{"bytes",fs::file_size(p)},
         {"sha256",pas::evidence::sha256(p)}};
}
void closure_save(const fs::path& p,const J& j) {
 std::ofstream f(p,std::ios::binary);f<<j.dump(2)<<'\n';f.flush();
 if(!f)throw std::runtime_error("closure review fixture write");
}
struct ClosureFixture {
 fs::path dir,root_path,receipt_path;
 J entry,root,receipt,freeze,state,verification;
 std::vector<fs::path> leaf_paths;
 std::vector<J> leaves;
 std::map<std::string,fs::path> core_paths;
 // Reseal affected copies in dependency order. This is deliberately not a
 // permissive fixer: semantic mistakes in the root/leaf/receipt remain intact.
 void seal(bool counts=true,bool keep_root_frozen=true) {
  for(std::size_t i=0;i<leaves.size();++i)closure_save(leaf_paths[i],leaves[i]);
  root["leaves"]=J::array();std::size_t files_count=0;
  for(std::size_t i=0;i<leaves.size();++i){root["leaves"].push_back(closure_binding(leaf_paths[i]));files_count+=leaves[i].at("files").size();}
  if(counts){root["files_count"]=files_count;root["leaf_count"]=leaves.size();receipt["files_count"]=files_count;receipt["leaf_count"]=leaves.size();}
  closure_save(root_path,root);
  bool found=false;for(auto& b:freeze["files"])if(fs::path(b.at("path").get<std::string>())==root_path){b=closure_binding(root_path);found=true;}
  for(auto& b:freeze["files"]){const fs::path p=b.at("path").get<std::string>();if(std::find(leaf_paths.begin(),leaf_paths.end(),p)!=leaf_paths.end())b=closure_binding(p);}
  if(keep_root_frozen&&!found)throw std::runtime_error("review root direct anchor absent");
  closure_save(core_paths.at("freeze"),freeze);entry["process_evidence"]["freeze"]=closure_binding(core_paths.at("freeze"));
  closure_save(core_paths.at("verification"),verification);entry["process_evidence"]["verification"]=closure_binding(core_paths.at("verification"));
  state["freeze_sha"]=entry["process_evidence"]["freeze"]["sha256"];
  state["contract_sha"]=entry["process_evidence"]["contract"]["sha256"];
  state["receipts"]=J::array({entry["process_evidence"]["verification"]});
  closure_save(core_paths.at("state"),state);entry["process_evidence"]["state"]=closure_binding(core_paths.at("state"));
  receipt["root"]=closure_binding(root_path);
  receipt["state"]=entry["process_evidence"]["state"];
  receipt["verification"]=entry["process_evidence"]["verification"];
  seal_receipt();
 }
 void seal_receipt(){closure_save(receipt_path,receipt);entry["process_evidence"]["input_closure"]=closure_binding(receipt_path);}
};
ClosureFixture clone_closure(const J& original,const fs::path& dir) {
 fs::create_directories(dir);ClosureFixture f;f.dir=dir;f.entry=original;
 const auto original_bindings=original.at("process_evidence");std::map<fs::path,fs::path> moved;
 for(const auto* role:{"specification","result","verification","freeze","command","contract","state"}){
  const fs::path old=original_bindings.at(role).at("path").get<std::string>(),p=dir/(std::string(role)+".json");
  closure_save(p,read(old));f.core_paths[role]=p;moved[fs::canonical(old)]=fs::canonical(p);f.entry["process_evidence"][role]=closure_binding(p);
 }
 f.receipt=read(original_bindings.at("input_closure").at("path").get<std::string>());
 const fs::path old_root=f.receipt.at("root").at("path").get<std::string>();f.root=read(old_root);
 f.root_path=dir/"input-root.json";f.receipt_path=dir/"input-verification.json";
 f.root["verification_output"]=f.receipt_path.string();
 for(std::size_t i=0;i<f.root.at("leaves").size();++i){
  const fs::path old=f.root.at("leaves")[i].at("path").get<std::string>(),p=dir/("input-leaf-"+std::to_string(i)+".json");
  f.leaves.push_back(read(old));f.leaf_paths.push_back(p);closure_save(p,f.leaves.back());moved[fs::canonical(old)]=fs::canonical(p);
 }
 closure_save(f.root_path,f.root);moved[fs::canonical(old_root)]=fs::canonical(f.root_path);
 f.freeze=read(f.core_paths.at("freeze"));
 for(auto& b:f.freeze["files"]){const auto old=fs::canonical(b.at("path").get<std::string>());if(moved.contains(old))b=closure_binding(moved.at(old));}
 f.verification=read(f.core_paths.at("verification"));
 for(auto& b:f.verification["entries"]){const auto old=fs::canonical(b.at("path").get<std::string>());if(moved.contains(old))b=closure_binding(moved.at(old));}
 f.state=read(f.core_paths.at("state"));f.seal();return f;
}
void remove_member(ClosureFixture& f,const fs::path& required) {
 const auto target=fs::canonical(required);std::size_t removed=0;
 for(auto& leaf:f.leaves){auto& files=leaf["files"];for(auto i=files.begin();i!=files.end();)if(fs::canonical(i->at("path").get<std::string>())==target){i=files.erase(i);++removed;}else ++i;}
 if(removed!=1)throw std::runtime_error("review omission must remove exactly one indirect member");
}
}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("usage generated-unit-fixture-root fresh-review-output-root");
 const auto fixture=fs::canonical(argv[1]),out=fs::absolute(argv[2]);if(fs::exists(out))throw std::runtime_error("fresh review root");fs::create_directories(out);
 J source_before=J::array(),seed_before=J::array(),checks=J::array();
 // Preserve every seed artifact, including the generator's intentionally failed
 // controls and tampered run-2 sidecar, rather than rewriting/restoring them.
 for(const auto& item:fs::recursive_directory_iterator(fixture))if(item.is_regular_file())seed_before.push_back(closure_binding(item.path()));
 const auto input=read(fixture/"positive-contract-double-input.json"),campaign_json=read(input.at("campaign").at("path").get<std::string>());
 fs::path analyzer;for(const auto& b:campaign_json.at("bindings"))if(b.at("role")=="analyzer_binary")analyzer=b.at("path").get<std::string>();
 if(analyzer.empty())throw std::runtime_error("seed analyzer binding absent");
 const auto c=campaign(input,analyzer);const auto original=input.at("runs").at(0),planned=c.value.at("planned_runs").at(0);
 const auto report_path=fs::canonical(original.at("path").get<std::string>());const auto report=read(report_path);
 const auto source=pas::cost_provenance::source_closure(c.value);
 for(const auto& b:source.at("files"))source_before.push_back(closure_binding(b.at("path").get<std::string>()));
 for(const auto& b:c.value.at("bindings"))source_before.push_back(closure_binding(b.at("path").get<std::string>()));
 const auto test_source=closure_binding(fs::path(__FILE__));source_before.push_back(test_source);
 auto record=[&](const std::string& id,bool pass){checks.push_back({{"id",id},{"pass",pass}});if(!pass)throw std::runtime_error(id);};
 // Positive baseline must succeed before any attack. Its output is never called
 // actual cost READY; the complete loader consumes explicit seeded doubles.
 record("positive_full_loader_before_attacks",load_run(original,planned,c).at("provenance")==report.at("provenance"));
 auto admit=[&](const ClosureFixture& f){process_stage(f.entry,planned,c,report_path,report.at("provenance"));};
 auto attack=[&](const std::string& id,const std::string& expected,const std::function<void(ClosureFixture&)>& mutate){
  auto f=clone_closure(original,out/id);admit(f); // Every copy has a positive precondition.
  mutate(f);std::string error;try{admit(f);}catch(const std::exception& e){error=e.what();}
  const bool pass=error.find(expected)!=std::string::npos;
  checks.push_back({{"id",id},{"positive_clone_before_mutation",true},{"expected_error",expected},{"observed_error",error},{"pass",pass},
                    {"mutated_entry",f.entry},{"core_state_status",f.state.at("status")}});
 };
 attack("missing_closure_with_core_COMPLETE","verified input closure receipt absent",[](auto& f){f.entry["process_evidence"].erase("input_closure");});
 for(const auto* field:{"pass","before_verified","after_verified"})attack(std::string("false_")+field+"_with_core_COMPLETE","input closure not verified before and after native stage",[&](auto& f){f.receipt[field]=false;f.seal_receipt();});
 attack("STOP_receipt_with_core_COMPLETE","input closure not verified before and after native stage",[](auto& f){f.receipt["status"]="STOP";f.seal_receipt();});
 attack("foreign_receipt_stage","input closure not verified before and after native stage",[](auto& f){f.receipt["stage"]="foreign-stage";f.seal_receipt();});
 for(const auto* role:{"state","verification"})attack(std::string("foreign_")+role+"_same_bytes","input closure core receipt join",[&](auto& f){const fs::path p=f.dir/(std::string("foreign-")+role+".json");fs::copy_file(f.core_paths.at(role),p);f.receipt[role]=closure_binding(p);f.seal_receipt();});
 attack("root_not_directly_frozen","input closure root not directly frozen",[](auto& f){auto& files=f.freeze["files"];for(auto i=files.begin();i!=files.end();)if(fs::path(i->at("path").template get<std::string>())==f.root_path)i=files.erase(i);else ++i;f.seal(true,false);});
 attack("wrong_existing_verification_output","input closure root/output identity",[](auto& f){const auto p=f.dir/"wrong-output.json";closure_save(p,{{"unit_test_double",true}});f.root["verification_output"]=p.string();f.seal();});
 for(const auto* field:{"schema","stage"})attack(std::string("foreign_root_")+field,"input closure root/output identity",[&](auto& f){f.root[field]="foreign";f.seal();});
 for(const auto* field:{"schema","stage","index"})attack(std::string("foreign_leaf_")+field,"input closure leaf file capacity",[&](auto& f){if(std::string_view(field)=="index")f.leaves[0][field]=999;else f.leaves[0][field]="foreign";f.seal();});
 attack("duplicate_leaf","input closure duplicate leaf",[](auto& f){f.leaf_paths.push_back(f.leaf_paths[0]);f.leaves.push_back(f.leaves[0]);f.seal();});
 attack("duplicate_file_canonical_alias","input closure duplicate/self input",[](auto& f){auto b=f.leaves.back()["files"][0];const fs::path p=b["path"].template get<std::string>();b["path"]=(p.parent_path()/"."/p.filename()).string();if(f.leaves.back()["files"].size()==64)f.leaves[0]["files"][1]=b;else f.leaves.back()["files"].push_back(b);f.seal();});
 attack("leaf_65_files","input closure leaf file capacity",[](auto& f){auto& files=f.leaves[0]["files"];while(files.size()<65)files.push_back(files[0]);f.seal();});
 attack("empty_leaf","input closure leaf file capacity",[](auto& f){f.leaves[0]["files"]=J::array();f.seal();});
 attack("empty_root_leaves","input closure leaf capacity",[](auto& f){f.leaves.clear();f.leaf_paths.clear();f.seal();});
 // 129 root bindings cannot fit the unchanged 64 KiB native freeze. Only the
 // small root is a required direct anchor; drop any optional direct leaf anchors
 // in this copy, then let the analyzer reach the root's own capacity check.
 attack("root_129_leaves","input closure leaf capacity",[](auto& f){
  auto& files=f.freeze["files"];for(auto i=files.begin();i!=files.end();)if(std::find(f.leaf_paths.begin(),f.leaf_paths.end(),fs::path(i->at("path").template get<std::string>()))!=f.leaf_paths.end())i=files.erase(i);else ++i;
  while(f.leaves.size()<129){f.leaf_paths.push_back(f.leaf_paths[0]);f.leaves.push_back(f.leaves[0]);}f.seal();
 });
 for(const auto* field:{"files_count","leaf_count"}){
  attack(std::string("root_")+field+"_tamper","input closure complete denominator",[&](auto& f){f.root[field]=f.root[field].template get<std::size_t>()+1;f.seal(false);});
  attack(std::string("receipt_")+field+"_tamper","input closure complete denominator",[&](auto& f){f.receipt[field]=f.receipt[field].template get<std::size_t>()+1;f.seal_receipt();});
 }
 attack("verified_source_omitted_with_resealed_counts","campaign/source input absent from verified process closure",[&](auto& f){fs::path target;for(const auto& b:source.at("files"))if(b.at("relative_path")=="research/prelive_current/input_closure.hpp")target=b.at("path").get<std::string>();if(target.empty())throw std::runtime_error("required header absent in seed");remove_member(f,target);f.seal();});
 for(const auto* role:{"environment","source_manifest"})attack(std::string("campaign_")+role+"_omitted_with_resealed_counts","campaign/source input absent from verified process closure",[&](auto& f){remove_member(f,c.bindings.at(role).at("path").get<std::string>());f.seal();});
 attack("whole_leaf_omitted_with_resealed_counts","campaign/source input absent from verified process closure",[](auto& f){if(f.leaves.size()<2)throw std::runtime_error("two seeded leaves required for whole-leaf omission");f.leaves.erase(f.leaves.begin());f.leaf_paths.erase(f.leaf_paths.begin());for(std::size_t i=0;i<f.leaves.size();++i)f.leaves[i]["index"]=i;f.seal();});
 attack("file_size_binding_tamper_resealed_leaf","input closure binding changed",[](auto& f){auto& b=f.leaves[0]["files"][0];b["bytes"]=b["bytes"].template get<std::uint64_t>()+1;f.seal();});
 attack("unchanged_native_freeze_64KiB_cap","hash file bound/type",[](auto& f){f.freeze["test_double_padding"]=std::string(65536,'x');f.seal();});
 record("original_loader_still_positive",load_run(original,planned,c).at("provenance")==report.at("provenance"));
 bool seed_unchanged=true,source_unchanged=true;J seed_after=J::array(),source_after=J::array();
 for(const auto& before:seed_before){const auto after=closure_binding(before.at("path").get<std::string>());seed_after.push_back(after);seed_unchanged=seed_unchanged&&before==after;}
 for(const auto& before:source_before){const auto after=closure_binding(before.at("path").get<std::string>());source_after.push_back(after);source_unchanged=source_unchanged&&before==after;}
 record("all_seed_artifacts_and_original_failures_unchanged",seed_unchanged);record("all_real_sources_and_test_source_unchanged",source_unchanged);
 bool pass=true;for(const auto& row:checks)pass=pass&&row.at("pass").get<bool>();
 const J result={{"schema","pas.input-closure-independent-review-tests.v1"},{"pass",pass},{"checks",checks},{"test_source",test_source},
  {"fixture_campaign",input.at("campaign")},{"source_before",source_before},{"source_after",source_after},{"seed_before",seed_before},{"seed_after",seed_after},
  {"scope","Independent semantic admission controls over explicit cloned core/closure/auditor test doubles. No real source or seed artifact is rewritten."},
  {"native_freeze_cap_bytes",65536},{"actual_cost_ready",false},{"live_ready",false}};
 closure_save(out/"review_result.json",result);std::cout<<J{{"pass",pass},{"checks",checks.size()},{"actual_cost_ready",false},{"live_ready",false}}.dump()<<'\n';return pass?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
