#include "pas/candidate_admission.hpp"
#include "pas/candidate_receipts.hpp"
#include "pas/analysis.hpp"
#include <fstream>
#include <iostream>
#include <type_traits>

namespace {
using J=nlohmann::json;namespace fs=std::filesystem;
const std::string commit(40,'a'),sha(64,'a'),different_sha(64,'b');
J pin(const std::string& name){return {{"path","C:/fixture/"+name},{"bytes",std::uint64_t{4}},{"sha256",sha}};}
J identity(const char* schema){return {{"schema",schema},{"evidence_kind","synthetic"},{"source_commit",commit},
    {"candidate_binary_sha256",sha},{"profile_sha256",sha},{"capability_sha256",sha},{"pass",true}};}
std::pair<J,J> fixture(){
    J input={{"schema","pas.current-candidate-admission-input.v1"},{"evidence_kind","synthetic"},{"source_commit",commit}};
    for(const auto* role:{"candidate_binary","source_manifest","production_build","cold_preflight_input","runtime_profile","capability","production_cost","capture_rpc","device","physical","authorization"})input[role]=pin(std::string(role)+".json");
    input["candidate_binary"]=pin("pas.exe");
    J profile={{"serial","fixture-device"},{"capture",{{"kind","emulator-grpc"},{"transport","payload"},{"image_format","rgb888"},{"row_order","top-down"},{"width",1280},{"height",720},{"source_rotation",1},{"grpc_read_chunk_kib",256},{"max_relative_lag_ms",250}}},
        {"touch",{{"kind","emulator-grpc"},{"width",720},{"height",1280},{"rotation_deg",90},{"max_contacts",5},{"timeout_ms",100}}},
        {"scheduler",{{"max_plans",128},{"max_steps",16},{"horizon_ms",350},{"evidence_max_age_ms",100}}},
        {"game",{{"lead_ms",35},{"uncertainty_ms",30},{"enabled_types",J::array({"tap","hold","drag","flick"})}}}};
    J source={{"schema","pas.cooperative-source.v1"},{"source_commit",commit},{"dirty",false},{"files",J::array({pin("src/game.cpp"),pin("src/candidate_admission.cpp")})}};
    J files=J::array({input.at("candidate_binary"),pin("pas_core.lib"),pin("pas_current_candidate.lib"),pin("pas_emulator.lib"),pin("pas_proto.lib")});
    J build={{"schema","pas.cooperative-build.v1"},{"configuration","release"},{"source_commit",commit},{"source_manifest",input.at("source_manifest")},{"files",files}};
    J checks=J::object();for(const auto* role:{"base","prefix","faults","production","narrow_build","aa","noise","abba"})checks[role]={{"pass",true}};
    J cold={{"schema","pas.readonly-narrow-preflight-check.v1"},{"status","COLD_BUNDLE_PASS"},{"pass",true},{"checks",checks},{"provenance",{{"execution_source_commit",commit}}}};
    auto production=identity("pas.current-production-chain-qualification.v1");production.update({{"status",pin("normal3-status.json")},{"production_packet_action_wake_pool_verified",true},{"sole_touch_owner_verified",true},{"bounded_all_attempts_verified",true},
        {"cold_preflight_input",input.at("cold_preflight_input")},{"normal_scenes",J::array({"tap","hold","dense"})},{"failed",0},{"unknown",0},{"final_active_contacts",0}});
    auto capture=identity("pas.current-capture-rpc-qualification.v1");capture.update({{"real_capture_rpc_measured",true},{"all_attempts_verified",true}});
    J rpc={{"attempted_notifications",10},{"accepted_frames",10},{"inactive_notifications",0},{"invalid_notifications",0},{"relative_stale_drops",0},{"source_gaps",0},{"failed",0},{"unknown",0},{"ready_budget_ns",pas::candidate_capture_ready_ns},{"warmup_ns",pas::candidate_capture_warmup_ns},{"measurement_ns",pas::candidate_capture_measurement_ns},{"ready_start_ns",0},{"first_frame_ns",1},{"measurement_start_ns",1+pas::candidate_capture_warmup_ns},{"measurement_end_ns",1+pas::candidate_capture_warmup_ns+pas::candidate_capture_measurement_ns},{"window_complete",true},{"source_absolute_age",nullptr},{"performance_pass",nullptr}};
    for(const auto* key:{"full_rpc_read_ms","measurement_rpc_read_ms","measurement_copy_ms","measurement_host_residence_ms"})rpc[key]={{"n",10},{"p50",1},{"p95",2},{"p99",3},{"max",4}};capture["metrics"]=rpc;
    auto device=identity("pas.current-device-profile-receipt.v1");device.update({{"serial","fixture-device"},{"game_package","fixture.package"},{"game_apk_sha256",sha},{"fixture_apk_sha256",sha},
        {"device",{{"android_release","fixture"},{"android_sdk","fixture"},{"cpu_abi","fixture"},{"model","fixture"},{"wm_size","fixture"},{"wm_density","fixture"}}},
        {"capture_geometry",{{"width",1280},{"height",720},{"source_rotation",1}}},{"corner_mapping",J::array({{{"frame",{0,0}},{"touch",{0,0}}},{{"frame",{1279,0}},{"touch",{0,1279}}},{{"frame",{0,719}},{"touch",{719,0}}},{{"frame",{1279,719}},{"touch",{719,1279}}}})}});
    auto physical=identity("pas.current-physical-qualification.v1");physical.update({{"real_input_created",true},{"same_current_pixel_owner_verified",true},{"five_contact_mapping_verified",true},{"identity_1519_verified",true},
        {"device_receipt",input.at("device")},{"failed",0},{"unknown",0},{"final_active_contacts",0},{"release_verified",true},{"phases",J::array()}});
    for(const auto* phase:{"tap","hold_entry","hold_body","hold_tail","held_rotation","late_alignment"})physical["phases"].push_back({{"phase",phase},{"opportunities",2},{"passed",2},{"failed",0},{"unknown",0}});
    J owner=identity("pas.current-physical-owner-observation.v1");owner["evidence_kind"]="actual";owner["device_receipt"]=input["device"];owner["capture_rpc_receipt"]=input["capture_rpc"];
    J owner_phases=J::array();for(const auto& row:physical["phases"])owner_phases.push_back({{"phase",row["phase"]},{"owner_opportunities",2}});
    owner["owner_aggregate"]={{"schema","pas.current-physical-owner-aggregate.v1"},{"frames",10},{"receipts",10},{"owner_unknown",0},{"failed_injection",0},{"release_verified",true},{"result_confirmed",true},{"physical_gold",false},{"qualification_issued",false},{"phases",owner_phases}};
    owner["fixture_mapping"]={{"schema","pas.current-fixture-mapping-aggregate.v1"},{"pass",true},{"failed",0},{"unknown",0},{"final_active_contacts",0},{"release_verified",true},{"max_contacts_verified",5},{"path_tolerance_px",4},{"cases",J::array()}};for(int i=0;i<6;++i)owner["fixture_mapping"]["cases"].push_back({{"case",i<4?"corner-"+std::to_string(i):(i==4?"hold_move":"five")},{"attempts",i<4?2:(i==4?3:11)},{"verified",i<4?2:(i==4?3:11)},{"pass",true}});
    J review=owner;review["schema"]="pas.current-physical-independent-review.v1";review["owner_observation"]=pin("owner.json");review["reviewed_after_owner"]=true;review["independent_of_candidate"]=true;review["candidate_predictions_used_as_gold"]=false;review["review_basis"]="current_live_pixels";review["reviewer"]="synthetic independent fixture";review["phases"]=physical["phases"];review["identity_1519"]={{"state","verified_current_identity_and_association"},{"opportunities",1},{"passed",1},{"failed",0},{"unknown",0},{"historical_evidence_unchanged",true}};
    physical["owner_observation"]=pin("owner.json");physical["independent_review"]=pin("review.json");
    auto authorization=identity("pas.current-score-only-authorization.v1");authorization.update({{"scope","chapter_legacy_available_in_single_pass_scores_only"},{"persistence","none"},{"one_round",true},{"no_unlock",true},{"no_retry",true},{"stop_after_pass",true},{"device_operations_authorized",true}});
    J capability={{"schema_version",2},{"fixture_schema","native_touch_v2"},{"capability_verified",true},{"serial","fixture-device"},{"installed_apk_sha256",sha},{"fixture_apk_sha256",sha},{"max_contacts_verified",5},{"config",profile},{"device_report",device.at("device")}};
    J d={{"input",input},{"source_manifest",source},{"production_build",build},{"runtime_profile",profile},{"capability",capability},{"cold_preflight",cold},{"production_cost",production},{"capture_rpc",capture},{"device",device},{"physical",physical},{"authorization",authorization},{"physical_owner_observation",owner},{"physical_review",review}};
    J context={{"workspace","C:/fixture"},{"source_commit",commit},{"source_dirty",false},{"candidate_binary_sha256",sha},{"profile_sha256",sha},{"capability_sha256",sha},
        {"compiled_source_sha256",{{"src/game.cpp",sha},{"src/candidate_admission.cpp",sha}}},
        {"preflight",{{"fingerprint_matches",true},{"real_input_created",false},{"config",profile},{"device",device.at("device")},{"capture_geometry",device.at("capture_geometry")},{"corner_mapping",device.at("corner_mapping")},{"capability_report_sha256",sha},{"current_fixture_apk_sha256",sha},{"current_game_apk_sha256",sha}}}};
    return {d,context};
}
struct Tests {J checks=J::array();int failed=0;void check(const std::string& id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}};
bool reason(const J& r,const std::string& code){for(const auto& i:r.at("issues"))if(i.at("code")==code)return true;return false;}
void denied_spies(Tests& t,const std::string& label){int loads=0,preflight=0,discovery=0,backend=0,persistence=0;bool rejected=false;
    try{pas::enter_current_candidate_score_only_session(pas::CandidateLiveAdmission{},"missing-config","missing-capability",[&]{++loads;++preflight;++discovery;++backend;++persistence;});}catch(const std::runtime_error&){rejected=true;}
    t.check(label+"_I_O_spies_zero",rejected&&loads==0&&preflight==0&&discovery==0&&backend==0&&persistence==0);
}
}
int main(int argc,char** argv){try{
    static_assert(!std::is_constructible_v<pas::CandidateLiveAdmission,bool>);
    static_assert(!std::is_constructible_v<pas::CandidateLiveAdmission,std::function<void()>>);
    static_assert(!std::is_convertible_v<pas::CandidateColdTestTag,pas::CandidateLiveAdmission>);
    static_assert(!std::is_convertible_v<pas::CandidateColdReceiptValidation,pas::CandidateLiveAdmission>);
    static_assert(!std::is_convertible_v<pas::CandidateLiveAdmission,bool>);
    if(argc!=2||fs::exists(argv[1]))throw std::runtime_error("fresh report path required");Tests t;const auto [base,current]=fixture();
    const auto positive=pas::validate_synthetic_candidate_receipts(base,current).report();
    t.check("complete_synthetic_typed_contract_positive",positive.at("pass").get<bool>()&&positive.at("status")=="SYNTHETIC_CONTRACT_PASS"&&positive.at("synthetic").get<bool>()&&!positive.at("qualification_issued").get<bool>()&&!positive.at("live_ready").get<bool>());
    denied_spies(t,"cold_positive_cannot_admit");
    int archived=0;bool closed=false;try{pas::enter_current_candidate_manual_session(pas::CandidateLiveAdmission{},[&]{++archived;});}catch(const std::runtime_error&){closed=true;}t.check("ordinary_manual_archive_entry_closed",closed&&archived==0);
    const auto reject=[&](const std::string& id,const std::string& code,const auto& mutation){auto d=base,c=current;mutation(d,c);const auto r=pas::validate_synthetic_candidate_receipts(d,c).report();t.check(id,!r.at("pass").get<bool>()&&reason(r,code));denied_spies(t,id);};
    for(const auto* role:{"source_manifest","production_build","runtime_profile","capability","production_cost","capture_rpc","device","physical","authorization"})reject(std::string("missing_")+role,"MISSING_RECEIPT",[&](J& d,J&){d["input"].erase(role);});
    reject("malformed_schema","MALFORMED_RECEIPT",[](J& d,J&){d["physical"]["schema"]="unknown";});
    reject("tampered_compiled_source","SOURCE_MISMATCH",[](J&,J& c){c["compiled_source_sha256"]["src/game.cpp"]=different_sha;});
    reject("source_commit_mismatch","SOURCE_MISMATCH",[](J&,J& c){c["source_commit"]=std::string(40,'b');});
    reject("historical_physical","HISTORICAL_RECEIPT",[](J& d,J&){d["physical"]["source_commit"]=std::string(40,'b');});
    reject("actual_kind_in_synthetic_validator","SYNTHETIC_EVIDENCE",[](J& d,J&){d["input"]["evidence_kind"]="actual";});
    reject("binary_mismatch","BINARY_MISMATCH",[](J&,J& c){c["candidate_binary_sha256"]=different_sha;});
    reject("profile_mismatch","PROFILE_MISMATCH",[](J&,J& c){c["profile_sha256"]=different_sha;});
    reject("capability_mismatch","CAPABILITY_MISMATCH",[](J&,J& c){c["capability_sha256"]=different_sha;});
    reject("capability_wrong_five_contact_mapping","CAPABILITY_MISMATCH",[](J& d,J&){d["capability"]["max_contacts_verified"]=2;});
    reject("cost_not_ready","COST_NOT_READY",[](J& d,J&){d["cold_preflight"]["status"]="NOT_READY";});
    for(const auto* gate:{"aa","noise","abba"})reject(std::string("cost_")+gate+"_failed","COST_NOT_READY",[&](J& d,J&){d["cold_preflight"]["checks"][gate]["pass"]=false;});
    reject("production_unknown","PRODUCTION_NOT_READY",[](J& d,J&){d["production_cost"]["unknown"]=1;});
    reject("capture_wrong_denominator","CAPTURE_RPC_NOT_READY",[](J& d,J&){d["capture_rpc"]["metrics"]["attempted_notifications"]=9;});
    reject("capture_bad_quantile_despite_pass_flags","CAPTURE_RPC_NOT_READY",[](J& d,J&){d["capture_rpc"]["metrics"]["measurement_rpc_read_ms"]["p95"]=0;});
    reject("capture_bad_coverage_despite_pass_flags","CAPTURE_RPC_NOT_READY",[](J& d,J&){d["capture_rpc"]["metrics"]["window_complete"]=false;});
    reject("capture_pool_drop","CAPTURE_RPC_NOT_READY",[](J& d,J&){d["capture_rpc"]["metrics"]["invalid_notifications"]=1;});
    reject("capture_float_counter","CAPTURE_RPC_NOT_READY",[](J& d,J&){d["capture_rpc"]["metrics"]["accepted_frames"]=10.0;});
    reject("physical_unknown","PHYSICAL_NOT_READY",[](J& d,J&){d["physical"]["phases"][0]["unknown"]=1;});
    reject("physical_missing_tail","PHYSICAL_NOT_READY",[](J& d,J&){d["physical"]["phases"].erase(2);});
    reject("physical_zero_opportunities","PHYSICAL_NOT_READY",[](J& d,J&){d["physical"]["phases"][0]["opportunities"]=0;d["physical"]["phases"][0]["passed"]=0;});
    reject("physical_wrong_device_pin","HISTORICAL_RECEIPT",[](J& d,J&){d["physical"]["device_receipt"]["sha256"]=different_sha;});
    reject("current_preflight_false","CURRENT_PREFLIGHT_MISMATCH",[](J&,J& c){c["preflight"]["fingerprint_matches"]=false;});
    reject("current_device_changed","CURRENT_PREFLIGHT_MISMATCH",[](J&,J& c){c["preflight"]["device"]["model"]="changed";});
    reject("current_game_apk_changed","CURRENT_PREFLIGHT_MISMATCH",[](J&,J& c){c["preflight"]["current_game_apk_sha256"]=different_sha;});
    reject("authorization_archives","AUTHORIZATION_SCOPE",[](J& d,J&){d["authorization"]["persistence"]="diagnostic_archive";});
    reject("authorization_retries","AUTHORIZATION_SCOPE",[](J& d,J&){d["authorization"]["no_retry"]=false;});
    const pas::CandidateAdmissionRequest missing{};const auto absent=pas::inspect_candidate_receipts(missing);t.check("actual_inspection_missing_input_structured",!absent.at("pass").get<bool>()&&reason(absent,"MISSING_INPUT"));
    bool actual_denied=false;try{auto unused=pas::load_candidate_score_admission(missing);(void)unused;}catch(const std::runtime_error& e){actual_denied=std::string_view(e.what()).starts_with("current candidate live admission UNQUALIFIED:");}t.check("actual_factory_missing_input_rejects",actual_denied);
    // Only short contract files are written, then removed; no gameplay data.
    const fs::path output=fs::absolute(argv[1]);const auto fixture_dir=output.parent_path()/(output.stem().string()+"-small-fixtures");if(fs::exists(fixture_dir))throw std::runtime_error("fresh fixture directory required");fs::create_directory(fixture_dir);
    const auto file=fixture_dir/"synthetic-input.json";{std::ofstream f(file);f<<base.at("input").dump();}
    pas::CandidateAdmissionRequest request{output.parent_path(),file,fixture_dir/"must-not-read-config",fixture_dir/"must-not-read-capability",pas::sha256_file(file)};
    const auto synthetic_denied=pas::inspect_candidate_receipts(request);t.check("actual_file_loader_rejects_synthetic_before_config_I_O",reason(synthetic_denied,"SYNTHETIC_EVIDENCE"));
    request.input_sha256=different_sha;const auto tampered=pas::inspect_candidate_receipts(request);t.check("actual_file_SHA_tamper_rejected",reason(tampered,"TAMPERED_OR_MALFORMED_INPUT"));
    {std::ofstream f(file);f<<"{not-json";}request.input_sha256=pas::sha256_file(file);const auto malformed=pas::inspect_candidate_receipts(request);t.check("actual_file_malformed_JSON_rejected",reason(malformed,"TAMPERED_OR_MALFORMED_INPUT"));
    fs::remove(file);fs::remove(fixture_dir);
    // Explicit verification overlays are a separate denominator. The original
    // 82 admission controls and their fixture source remain unchanged.
    Tests delta_tests;auto [delta_doc,delta_context]=fixture();const std::string formal="12ec0550e882e3b06751d67fa67c43dda3e9312b",execution(40,'c');
    delta_doc["input"]["source_commit"]=formal;delta_context["source_commit"]=formal;delta_doc["source_manifest"]["source_commit"]=formal;delta_doc["production_build"]["source_commit"]=formal;
    delta_doc["input"]["candidate_binary"]=pin("delta/pas.exe");delta_doc["input"]["candidate_binary"]["sha256"]=different_sha;delta_context["candidate_binary_sha256"]=different_sha;
    for(const auto* role:{"production_cost","capture_rpc","device","physical","authorization","physical_owner_observation","physical_review"}){delta_doc[role]["source_commit"]=formal;delta_doc[role]["candidate_binary_sha256"]=different_sha;}
    auto harness_pin=pin("cold-harness-delta.json"),verification_pin=pin("verification-delta.json");delta_doc["input"]["cold_harness_delta"]=harness_pin;delta_doc["input"]["verification_delta"]=verification_pin;
    J harness={{"schema","pas.cold-producer-eligibility-delta-build.v1"},{"source_commit",execution},{"base_source_commit",formal},{"cold_harness_delta",true},{"formal_rebuilt",false},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},{"source_manifest",pin("new-source.json")},{"core_build_plan",pin("delta-plan.json")},{"base_production_build",delta_doc["input"]["production_build"]},{"verification_delta",verification_pin}};
    J verification={{"schema","pas.cold-candidate-verification-delta.v1"},{"source_commit",execution},{"source_dirty",false},{"base_source_commit",formal},{"formal_rebuilt",false},{"verification_only_delta",true},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},{"candidate_binary",delta_doc["input"]["candidate_binary"]},{"candidate_source",pin("src/candidate_admission.cpp")},{"tests_source",pin("tests/candidate_admission_tests.cpp")},{"source_manifest",harness["source_manifest"]},{"core_build_plan",harness["core_build_plan"]},{"base_production_build",harness["base_production_build"]}};
    delta_doc["cold_harness_delta"]=harness;delta_doc["verification_delta"]=verification;delta_context["verification_delta_identity"]={{"schema","pas.cold-candidate-verification-identity.v1"},{"source_commit",execution},{"source_dirty",false},{"base_source_commit",formal},{"candidate_admission_sha256",sha},{"candidate_admission_tests_sha256",sha}};
    delta_doc["cold_preflight"]["provenance"]={{"execution_source_commit",execution},{"base_formal_source_commit",formal},{"cold_harness_delta",true},{"verification_only_delta",true}};
    const auto delta_check=[&](const char* id,const auto& mutate,bool expected=false){auto d=delta_doc,c=delta_context;mutate(d,c);const auto report=pas::validate_synthetic_candidate_receipts(d,c).report();delta_tests.check(id,report.at("pass")==expected&&!report.at("qualification_issued").get<bool>()&&!report.at("live_ready").get<bool>());};
    delta_check("explicit_formal55_new_verification_positive",[](J&,J&){},true);
    delta_check("missing_harness_pin_reject",[](J& d,J&){d["input"].erase("cold_harness_delta");});
    delta_check("missing_verification_pin_reject",[](J& d,J&){d["input"].erase("verification_delta");});
    delta_check("uncompiled_delta_identity_reject",[](J&,J& c){c.erase("verification_delta_identity");});
    delta_check("wrong_compiled_commit_reject",[](J&,J& c){c["verification_delta_identity"]["source_commit"]=commit;});
    delta_check("wrong_formal_base_reject",[](J& d,J&){d["verification_delta"]["base_source_commit"]=commit;});
    delta_check("wrong_factory_source_SHA_reject",[](J&,J& c){c["verification_delta_identity"]["candidate_admission_sha256"]=different_sha;});
    delta_check("wrong_tests_source_SHA_reject",[](J&,J& c){c["verification_delta_identity"]["candidate_admission_tests_sha256"]=different_sha;});
    delta_check("dirty_or_fresh_formal_claim_reject",[](J& d,J& c){d["verification_delta"]["formal_rebuilt"]=true;c["verification_delta_identity"]["source_dirty"]=true;});
    delta_check("frozen_files_cannot_adopt_new_candidate_reject",[](J& d,J&){d["production_build"]["files"].push_back(d["input"]["candidate_binary"]);});
    delta_check("execution_cost_must_bind_new_delta_reject",[](J& d,J&){d["cold_preflight"]["provenance"]["execution_source_commit"]=formal;});
    delta_check("numerical_gate_never_replaced_by_delta_reject",[](J& d,J&){d["cold_preflight"]["checks"]["noise"]["pass"]=false;});
    if(delta_tests.checks.size()!=12)throw std::runtime_error("separate verification12 denominator");
    J report={{"schema","pas.candidate-admission-controls.v1"},{"assertions",t.checks.size()},{"checks",t.checks},{"failed",t.failed},{"pass",t.failed==0},{"synthetic_fixture_only",true},{"qualification_issued",false},{"live_ready",false},{"device_operations",0},{"gameplay_files_created",0}};
    report["verification_delta_controls"]={{"schema","pas.cold-candidate-verification-controls.v1"},{"assertions",delta_tests.checks.size()},{"checks",delta_tests.checks},{"failed",delta_tests.failed},{"pass",delta_tests.failed==0},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0}};report["pass"]=t.failed==0&&delta_tests.failed==0;
    std::ofstream f(output,std::ios::binary);f<<report.dump(2)<<'\n';if(!f)throw std::runtime_error("report write");std::cout<<"candidate admission controls="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed||delta_tests.failed?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
