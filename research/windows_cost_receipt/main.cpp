// Reuse the exact frozen admission predicates, raw joins and source closure.
// Renaming the old CLI entry introduces no process or device launch.
#define main pas_frozen_cost_analysis_entry
#include "../prelive_current/cost_analysis.cpp"
#undef main
#include "../windows_qualification/verifier.hpp"
int main(int argc,char**argv){try{
 if(argc!=11||std::string_view(argv[1])!="--input"||std::string_view(argv[3])!="--expected-sha256"||std::string_view(argv[5])!="--source-commit"||std::string_view(argv[7])!="--analyzer"||std::string_view(argv[9])!="--output")
  throw std::runtime_error("usage --input bound-run-gate-input --expected-sha256 SHA --source-commit COMMIT --analyzer frozen-analyzer --output fresh-report");
 const fs::path input_path=fs::absolute(argv[2]),output_path=fs::absolute(argv[10]);const std::string sha=argv[4],commit=argv[6];
 if(!hash_text(sha)||!pas::windows_qualification::hex(commit,40)||fs::exists(output_path))throw std::runtime_error("external identity/fresh output required");
 const auto input=pas::windows_qualification::read(bound_file({{"path",input_path.string()},{"sha256",sha}},MiB));
 const auto c=campaign(input,argv[8]);if(c.value.at("source_commit")!=commit)throw std::runtime_error("external source commit mismatch");
 const auto index=input.at("plan_index").get<std::size_t>();if(index>=12)throw std::runtime_error("plan index bound");
 auto r=load_run(input.at("run"),c.value.at("planned_runs")[index],c);const auto result=validity(r);
 // Verification succeeds for an authentic NOT_READY receipt, while preserving
 // its failure. No mutable token, admission API, live CLI or device module.
 J out={{"schema","pas.windows-cost-receipt-verification.v1"},{"integrity_verified",true},
 {"offline_cost_valid",result.at("pass")},{"validity",result},{"input_sha256",sha},{"source_commit",commit},
 {"campaign_id",c.value.at("id")},{"campaign_sha256",c.sha},{"run_id",input.at("run").at("run_id")},
 {"live_ready",false},{"device_operations",0},{"scope","read-only bound offline run receipt; exact existing analyzer admission/raw/source predicates"},
 {"remaining_gates",J::array({"all twelve AA runs and frozen noise","ABBA comparison","independent fault/stress/configuration cost","production packet/action/wake/capture/RPC qualification","current emulator/app/profile/five-contact qualification","physical authorization and acceptance"})}};
 // Recheck the externally pinned input and the whole campaign/source graph.
 bound_file({{"path",input_path.string()},{"sha256",sha}},MiB);campaign(input,argv[8]);
 std::ofstream f(output_path,std::ios::binary);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("verification report write");
 std::cout<<(result.at("pass").get<bool>()?"VERIFIED offline cost PASS; live UNQUALIFIED":"VERIFIED offline cost NOT_READY; live UNQUALIFIED")<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
