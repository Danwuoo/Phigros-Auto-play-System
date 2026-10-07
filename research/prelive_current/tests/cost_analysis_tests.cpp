// Analyzer unit fixtures. Simulated auditor receipts are test doubles, not
// qualified measurements. Actual raw-auditor counterexamples are separate.
#define main cost_analysis_entry
#include "../cost_analysis.cpp"
#undef main
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("usage fresh-output-root repository");
 const auto root=fs::absolute(argv[1]);if(fs::exists(root))throw std::runtime_error("fresh root");fs::create_directories(root);const auto repo=fs::absolute(argv[2]);int checks=0;auto expect=[&](bool b,const char* text){++checks;if(!b)throw std::runtime_error(text);};
 auto hash=[&](const std::string& s){pas::evidence::Sha256 h;h.update(s.data(),s.size());return h.finish();};
 expect(hash("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","empty SHA");expect(hash("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","abc SHA");expect(hash(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","million-a SHA");expect(hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","multiblock SHA");
 // Byte-exact fixtures must match the binary-mode scoped review restorer on
 // Windows too; text mode would translate LF into CRLF before the SHA binding.
 auto save=[&](const fs::path&p,const J&j){std::ofstream f(p,std::ios::binary);f<<j.dump(2)<<'\n';if(!f)throw std::runtime_error("save");};
 auto bind=[&](const fs::path&p){return J{{"path",fs::canonical(p).string()},{"bytes",fs::file_size(p)},{"sha256",pas::evidence::sha256(p)}};};
 auto dist=[](int n,double v){return J{{"n",n},{"p50",v},{"p95",v},{"p99",v},{"max",v},{"jitter_p95_minus_p5",0}};};
 J report={{"schema","pas.prelive-complete-chain-cost.v1"},{"mode","A"},{"scene","tap"},{"requested",2560},{"attempts",2560},{"warmup_attempts",256},{"cadence_ms",20},{"clock_domain","host_QPC"},{"source_absolute_age",nullptr},{"device_endpoints",0},{"safe",true},{"supervisor_stopped",false},{"release_verified",true},{"final_release",{{"failed",J::array()},{"unknown",J::array()}}},{"pool_drops",0},{"writer_drops",0},{"writer_fault",false},{"journal_admission_failed",false},{"injection_failed",0},{"owner_seen_fraction",1},{"owner_seen",2560},{"own_down",1},{"own_up",1},{"capture_to_owner_ms",dist(2560,1)},{"all_phase_lateness_ms",dist(2,.1)},{"measurement",{{"attempts",2304},{"owner_seen",2304},{"owner_seen_fraction",1},{"observer_ms",dist(2304,.3)},{"hook_ms",dist(2304,.2)},{"owner_ms",dist(2304,.1)},{"capture_to_owner_ms",dist(2304,1)}}},{"rss_peak_os",1024}};
 expect(validity(report).at("pass"),"valid unit report");
 for(const auto* key:{"requested","attempts","warmup_attempts","cadence_ms"}){auto bad=report;bad[key]=0;expect(!validity(bad).at("pass").get<bool>(),"frozen denominator rejected");}
 {auto bad=report;bad["capture_to_owner_ms"]["p99"]=101;bad["capture_to_owner_ms"]["max"]=251;expect(!validity(bad).at("pass").get<bool>(),"latency hard gate");}
 {auto bad=report;bad["_audited_measurement"]={{"phase_counts",{0,0,0}},{"lateness",{{"n",0}}}};expect(!validity(bad).at("pass").get<bool>(),"warmup-only action rejected");}
 {auto bad=report;bad["rss_peak_os"]=128*MiB+1;expect(!validity(bad).at("pass").get<bool>(),"RSS hard gate");}
 {auto bad=report;bad["own_down"]=0;expect(!validity(bad).at("pass").get<bool>(),"empty action");}
 {auto bad=report;bad["measurement"]["observer_ms"]["n"]=1;expect(!validity(bad).at("pass").get<bool>(),"metric n");}
 {auto bad=report;bad["scene"]="dense";bad["all_phase_lateness_ms"]["p99"]=16;bad["all_phase_lateness_ms"]["max"]=101;expect(!validity(bad).at("pass").get<bool>(),"dense nonempty phase lateness");}
 {auto bad=report;bad["final_release"]["unknown"]=J::array({0});expect(!validity(bad).at("pass").get<bool>(),"release sticky");}
 auto self=bind(argv[0]);J bindings=J::array();for(const auto* role:{"candidate_binary","analyzer_binary","auditor_binary"}){auto b=self;b["role"]=role;bindings.push_back(b);}
 {auto b=bind(repo/"configs/phigros-hd-assist-five-lead35.json");b["role"]="profile";bindings.push_back(b);}
 {const auto p=root/"environment.json";save(p,{{"unit_test_double",true}});auto b=bind(p);b["role"]="environment";bindings.push_back(b);}
 {auto b=bind(repo/"docs/research/zero-miss-20261006/windows/PRELIVE_CURRENT_COST_PROTOCOL.md");b["role"]="cost_protocol";bindings.push_back(b);}
 J source_files=J::array();for(const auto* folder:{"src","include/pas","research/prelive_current","research/prelive_audit","research/bvi_cold_v3","research/bvi_windows/bridge","research/hold_front_current"})for(const auto& item:fs::directory_iterator(repo/folder))if(item.is_regular_file()&&(item.path().extension()==".cpp"||item.path().extension()==".hpp"||item.path().filename()=="CMakeLists.txt")){auto b=bind(item.path());b["relative_path"]=fs::relative(item.path(),repo).generic_string();source_files.push_back(b);}
 {auto b=bind(repo/"CMakeLists.txt");b["relative_path"]="CMakeLists.txt";source_files.push_back(b);}
 const auto sm=root/"source-manifest.json";save(sm,{{"source_commit",std::string(40,'a')},{"dirty",false},{"repo_root",fs::canonical(repo).string()},{"files",source_files}});{auto b=bind(sm);b["role"]="source_manifest";bindings.push_back(b);}
 J plans=J::array(),runs=J::array();for(int i=0;i<12;++i){const std::string scene=i<4?"tap":i<8?"hold":"dense",id="fixture-aa-"+std::to_string(i+1);const auto p=root/(id+".json");plans.push_back({{"run_id",id},{"scene",scene},{"mode","A"},{"report_path",p.string()}});}
 const auto cp=root/"campaign.json";J freeze={{"schema","pas.current-rails-campaign-freeze.v2"},{"configuration","Release"},{"platform","windows-x64"},{"id","unit-test-only-not-measurement"},{"source_commit",std::string(40,'a')},{"scenes",{"tap","hold","dense"}},{"bindings",bindings},{"planned_runs",plans}};save(cp,freeze);
 for(int i=0;i<12;++i){const auto& plan=plans[i];const std::string id=plan.at("run_id"),scene=plan.at("scene");const fs::path p=plan.at("report_path").get<std::string>();auto r=report;r["scene"]=scene;r["unit_test_fixture"]=id;
  r["provenance"]=pas::cost_provenance::producer(cp,id,argv[0],"A",scene,2560,p,{{"pid",i+1},{"creation_filetime",1000+i},{"run_start_ns",1000000000ll*(i+1)}});
  save(p,r);save(p.string()+".attempts.json",{{"unit_test_double",id}});save(p.string()+".journal.jsonl",{{"unit_test_double",id}});
  const auto audit=root/(id+"-audit.json");save(audit,{{"schema","pas.prelive-independent-integrity.v2"},{"unit_test_double",true},{"integrity",true},{"complete_prefix_evidence",true},{"all_input_processed",true},{"metrics_verified",true},{"plan_join_complete",true},{"coordinate_join_complete",true},{"release_uncertainty_seen",false},{"provenance_join_complete",true},{"run_provenance",r["provenance"]},{"measurement_phase_counts",{1,0,1}},{"measurement_all_phase_lateness_ms",dist(2,.1)},{"verifier_binary",self},{"input_bindings",{{"report",bind(p)},{"attempts",bind(p.string()+".attempts.json")},{"journal",bind(p.string()+".journal.jsonl")}}}});
  const auto specp=root/(id+"-spec.json"),resultp=root/(id+"-result.json"),verificationp=root/(id+"-verification.json"),processfreeze=root/(id+"-freeze.json"),commandp=root/(id+"-command.json"),statep=root/(id+"-state.json"),contractp=root/(id+"-contract.json");
  J spec={{"stage",id},{"exe",fs::canonical(argv[0]).string()},{"native_exit",0},{"runner_exit",0},{"total_s",900},{"stream_cap",4194304},{"argv",{"pipeline","A",scene,"2560",cp.string(),id,p.string()}}};save(specp,spec);auto commandspec=spec;commandspec["name"]=id;commandspec.erase("stage");save(commandp,{{"stage",id},{"spec",commandspec}});save(contractp,{{"spec",commandspec},{"device_allowed",false}});
  save(resultp,{{"pending",false},{"stage",id},{"facts",{{"root_image",fs::canonical(argv[0]).string()},{"pid",i+1},{"creation_filetime",1000+i},{"created",true},{"identity_trusted",true},{"streams_completed",true},{"held_all_signaled",true},{"natural_quiescence",true},{"active_final",0},{"runner_exit",0},{"exit_code",0},{"errors",J::array()},{"errors_overflow",false}}}});
  save(verificationp,{{"pending",false},{"producer_complete",true},{"stage",id},{"native_exit",0},{"runner_exit",0},{"verification_exit",0},{"entries",J::array({bind(resultp),bind(commandp)})}});
  const auto closure_root=root/(id+"-closure-root.json"),closure_receipt=root/(id+"-closure-verification.json");
  std::map<std::string,J> closure_files;for(const auto& b:bindings)closure_files[b.at("path").get<std::string>()]=b;for(const auto& b:source_files)closure_files[b.at("path").get<std::string>()]=b;
  J leaves=J::array(),members=J::array();std::size_t leaf_index=0;
  auto flush_leaf=[&]{if(members.empty())return;const auto p=root/(id+"-leaf-"+std::to_string(leaf_index)+".json");save(p,{{"schema","pas.current-input-closure-leaf.v1"},{"stage",id},{"index",leaf_index++},{"files",members}});leaves.push_back(bind(p));members=J::array();};
  for(const auto& [path,b]:closure_files){(void)path;members.push_back(b);if(members.size()==64)flush_leaf();}flush_leaf();
  save(closure_root,{{"schema","pas.current-input-closure.v1"},{"stage",id},{"verification_output",closure_receipt.string()},{"files_count",closure_files.size()},{"leaf_count",leaves.size()},{"leaves",leaves}});
  save(processfreeze,{{"files",J::array({bind(specp),bind(cp),self,bind(closure_root)})}});save(statep,{{"status","COMPLETE"},{"running",nullptr},{"freeze_sha",pas::evidence::sha256(processfreeze)},{"contract_sha",pas::evidence::sha256(contractp)},{"native_stages_consumed",1},{"native_launches_known",1},{"receipts",J::array({bind(verificationp)})}});
  save(closure_receipt,{{"schema","pas.current-input-closure-verification.v1"},{"stage",id},{"status","COMPLETE"},{"pass",true},{"before_verified",true},{"after_verified",true},{"root",bind(closure_root)},{"state",bind(statep)},{"verification",bind(verificationp)},{"files_count",closure_files.size()},{"leaf_count",leaves.size()}});
  auto e=bind(p);e["run_id"]=id;e["scene"]=scene;e["audit_path"]=audit.string();e["audit_sha256"]=pas::evidence::sha256(audit);e["process_evidence"]={{"specification",bind(specp)},{"result",bind(resultp)},{"verification",bind(verificationp)},{"freeze",bind(processfreeze)},{"command",bind(commandp)},{"contract",bind(contractp)},{"state",bind(statep)},{"input_closure",bind(closure_receipt)}};runs.push_back(e);
 }
 J input={{"schema","pas.current-rails-cost-input.v2"},{"campaign",bind(cp)},{"runs",runs},{"normal_stopped",false}};
 auto invoke=[&](J in,const std::string& name,int expected){const auto ip=root/(name+"-input.json"),op=root/(name+"-output.json");save(ip,in);std::string command="aa",a0=argv[0],a2=ip.string(),a3=op.string();char* args[]={a0.data(),command.data(),a2.data(),a3.data()};int rc=cost_analysis_entry(4,args);expect(rc==expected,name.c_str());if(rc==0)return read(op);return J::object();};
 expect(invoke(input,"positive-contract-double",0).at("comparison_allowed"),"positive analyzer contract");
 {auto bad=input;bad["runs"][1]=bad["runs"][0];invoke(bad,"duplicate-path",2);}
 {auto bad=input;bad["runs"][0]["scene"]="hold";invoke(bad,"scene-label",2);}
 {auto bad=input;bad["runs"][0]["sha256"]=std::string(64,'0');invoke(bad,"bad-report-sha",2);}
 {auto bad=input;bad["runs"][0]["audit_sha256"]=std::string(64,'0');invoke(bad,"bad-audit-sha",2);}
 {auto bad=input;std::swap(bad["runs"][0],bad["runs"][1]);invoke(bad,"reordered",2);}
 {auto bad=input;bad["runs"].erase(11);expect(!invoke(bad,"incomplete",0).at("comparison_allowed").get<bool>(),"missing cannot qualify");}
 {auto bad=input;bad["normal_stopped"]=true;expect(!invoke(bad,"stopped",0).at("comparison_allowed").get<bool>(),"STOP cannot qualify");}
 {auto bad=input;bad.erase("campaign");invoke(bad,"unbound",2);}
 {auto bad=input;bad["schema"]="old";invoke(bad,"legacy",2);}
 {auto bad=input;bad["campaign"]["sha256"]=std::string(64,'0');invoke(bad,"changed-campaign",2);}
 {const fs::path p=runs[0].at("audit_path").get<std::string>();auto original=read(p);for(const auto* field:{"metrics_verified","provenance_join_complete","coordinate_join_complete"}){auto a=original;a[field]=false;save(p,a);auto bad=input;bad["runs"][0]["audit_sha256"]=pas::evidence::sha256(p);invoke(bad,std::string("unverified-")+field,2);save(p,original);}}
 {pas::evidence::Sha256 h;h.update("abc",3);h.finish();bool repeated=false,after=false;try{h.finish();}catch(...){repeated=true;}try{h.update("x",1);}catch(...){after=true;}expect(repeated&&after,"finalized hash guard");}
 {bool rejected=false;try{pas::evidence::sha256(cp,1);}catch(...){rejected=true;}expect(rejected,"hash file capacity");}
 {std::ofstream f(runs[1].at("path").get<std::string>()+".attempts.json",std::ios::app);f<<' ';f.close();invoke(input,"raw-post-audit-tamper",2);}
 J result={{"schema","pas.cost-analyzer-tests.v1"},{"checks",checks},{"pass",true},{"scope","unit fixtures only; no measured Windows or live qualification"}};save(root/"result.json",result);std::cout<<result.dump()<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
