#include "pas/candidate_admission.hpp"
#include "pas/candidate_receipts.hpp"
#include "pas/config.hpp"
#include "pas/analysis.hpp"
#include "pas/adb.hpp"
#include "session_build_provenance.hpp"
#include "research/pre_emulator/qualification_reader/reader/narrow_preflight.hpp"
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <set>

namespace pas {
// Runtime entry definitions stay in runtime.cpp. The receipt verifier does not
// need capture/transport objects or generated protobuf headers to validate.
nlohmann::json game_preflight(const std::string&,const std::string&);
nlohmann::json match_touch_capability(const RuntimeConfig&,const nlohmann::json&,
                                    const nlohmann::json&,const std::string&);
namespace {
namespace g=preflight_graph;namespace n=g::narrow;namespace w=g::w;
using J=nlohmann::json;namespace fs=std::filesystem;
struct Reject final:std::runtime_error {
    std::string code;
    Reject(std::string c,std::string detail):std::runtime_error(std::move(detail)),code(std::move(c)){}
};
void need(bool value,const char* code,const char* detail){if(!value)throw Reject(code,detail);}
bool integer(const J& v){return v.is_number_integer()||v.is_number_unsigned();}
bool zero(const J& v){return integer(v)&&v==0;}
bool positive(const J& v){return integer(v)&&v.get<std::int64_t>()>0;}
const J& member(const J& value,const char* key){
    need(value.is_object()&&value.contains(key)&&!value.at(key).is_null(),"MISSING_RECEIPT",key);return value.at(key);
}
std::string text(const J& value,const char* key){const auto& v=member(value,key);need(v.is_string(),"MALFORMED_RECEIPT",key);return v.get<std::string>();}
bool flag(const J& value,const char* key){const auto& v=member(value,key);need(v.is_boolean(),"MALFORMED_RECEIPT",key);return v.get<bool>();}
void schema(const J& v,const char* expected){need(text(v,"schema")==expected,"MALFORMED_RECEIPT","unsupported receipt schema");}
bool pin_equal(const J& a,const J& b){n::pin_shape(a);n::pin_shape(b);return n::same_pin(a,b);}
void distribution_gate(const J& value,double p99,double maximum){
    need(positive(member(value,"n")),"CAPTURE_RPC_NOT_READY","nonempty distribution denominator required");
    double previous=0;for(const auto* key:{"p50","p95","p99","max"}){const auto& v=member(value,key);need(v.is_number(),"MALFORMED_RECEIPT","numeric distribution required");const double x=v.get<double>();need(std::isfinite(x)&&x>=previous,"CAPTURE_RPC_NOT_READY","finite ordered complete quantiles required");previous=x;}
    need(member(value,"p99").get<double>()<=p99&&member(value,"max").get<double>()<=maximum,"CAPTURE_RPC_NOT_READY","original production latency gate failed");
}
const char* roles[]={"candidate_binary","source_manifest","production_build","cold_preflight_input","runtime_profile","capability","production_cost","capture_rpc","device","physical","authorization"};
J result_base(bool synthetic){return {{"schema","pas.current-candidate-receipt-validation.v1"},{"status","REJECTED"},{"pass",false},
    {"synthetic",synthetic},{"qualification_issued",false},{"live_ready",false},{"device_operations",0},{"issues",J::array()}};}
bool collection_role(std::string_view role){return role!="capture_rpc"&&role!="device"&&role!="physical";}
J contract(const J& d,const J& current,bool synthetic,bool collection=false){
    J out=result_base(synthetic);bool ok=true;
    const auto section=[&](const char* name,const auto& fn){if(collection&&(!collection_role(name)||std::string_view(name)=="current_preflight"))return;try{fn();}catch(const Reject& e){ok=false;out["issues"].push_back({{"section",name},{"code",e.code},{"reason",e.what()}});}catch(const std::exception& e){ok=false;out["issues"].push_back({{"section",name},{"code","MALFORMED_RECEIPT"},{"reason",e.what()}});}};
    section("input",[&]{const auto& i=member(d,"input");schema(i,collection?"pas.current-candidate-collection-input.v1":"pas.current-candidate-admission-input.v1");
        need(text(i,"evidence_kind")== (synthetic?"synthetic":"actual"),"SYNTHETIC_EVIDENCE","fixture evidence cannot issue admission");
        need(w::hex(text(i,"source_commit"),40),"MALFORMED_RECEIPT","40-hex source commit required");
        for(const auto* role:roles)if(!collection||collection_role(role))n::pin_shape(member(i,role));
        need(text(i,"source_commit")==text(current,"source_commit"),"SOURCE_MISMATCH","input differs from compiled executable");
        need(member(i,"candidate_binary").at("sha256")==member(current,"candidate_binary_sha256"),"BINARY_MISMATCH","actual executing binary differs");
        need(member(i,"runtime_profile").at("sha256")==member(current,"profile_sha256"),"PROFILE_MISMATCH","requested config differs from bound profile");
        need(member(i,"capability").at("sha256")==member(current,"capability_sha256"),"CAPABILITY_MISMATCH","requested capability differs from bound capability");
    });
    section("source",[&]{const auto& i=member(d,"input");const auto& s=member(d,"source_manifest");schema(s,"pas.cooperative-source.v1");
        need(text(s,"source_commit")==text(i,"source_commit"),"SOURCE_MISMATCH","source manifest commit differs");
        need(!flag(s,"dirty")&&!flag(current,"source_dirty"),"SOURCE_MISMATCH","dirty compiled or receipt source");
        const auto& files=member(s,"files");need(files.is_array()&&!files.empty()&&files.size()<=8192,"MALFORMED_RECEIPT","bounded full source manifest required");
        std::map<std::string,std::string> sources;
        for(const auto& p:files){n::pin_shape(p);const auto relative=fs::path(text(p,"path")).lexically_relative(fs::path(text(current,"workspace")));const auto path=relative.generic_string();
            need(!relative.empty()&&!relative.is_absolute(),"MALFORMED_RECEIPT","relative source path required");
            for(const auto& part:relative)need(part!="."&&part!="..","MALFORMED_RECEIPT","source traversal");
            need(sources.emplace(path,text(p,"sha256")).second,"MALFORMED_RECEIPT","duplicate source member");}
        const auto& embedded=member(current,"compiled_source_sha256");need(embedded.is_object()&&!embedded.empty(),"SOURCE_MISMATCH","embedded production source hashes absent");
        for(const auto& [path,hash]:embedded.items())need(hash.is_string()&&sources.contains(path)&&sources.at(path)==hash.get<std::string>(),"SOURCE_MISMATCH","embedded production source bytes differ from manifest");
    });
    section("build",[&]{const auto& i=member(d,"input");const auto& b=member(d,"production_build");schema(b,"pas.cooperative-build.v1");
        need(text(b,"configuration")=="release"&&text(b,"source_commit")==text(i,"source_commit"),"HISTORICAL_RECEIPT","current Release production build required");
        need(pin_equal(member(b,"source_manifest"),member(i,"source_manifest")),"SOURCE_MISMATCH","production build/source manifest join");
        bool binary=false;std::set<std::string> libraries;
        const auto& files=member(b,"files");need(files.is_array()&&!files.empty()&&files.size()<=8192,"MALFORMED_RECEIPT","production build file set required");
        for(const auto& p:files){n::pin_shape(p);binary|=pin_equal(p,member(i,"candidate_binary"));libraries.insert(fs::path(text(p,"path")).filename().string());}
        need(binary&&libraries.contains("pas.exe")&&libraries.contains("pas_core.lib")&&libraries.contains("pas_current_candidate.lib")&&libraries.contains("pas_emulator.lib")&&libraries.contains("pas_proto.lib"),"BINARY_MISMATCH","complete production executable/library closure required");
    });
    section("cost",[&]{const auto& c=member(d,"cold_preflight");schema(c,"pas.readonly-narrow-preflight-check.v1");
        need(text(c,"status")=="COLD_BUNDLE_PASS"&&flag(c,"pass"),"COST_NOT_READY","AA/noise/ABBA bundle is NOT_READY");
        const auto& checks=member(c,"checks");for(const auto* k:{"base","prefix","faults","production","narrow_build","aa","noise","abba"})need(flag(member(checks,k),"pass"),"COST_NOT_READY",k);
        need(member(member(c,"provenance"),"execution_source_commit")==member(member(d,"input"),"source_commit"),"SOURCE_MISMATCH","cost execution source differs");
    });
    const auto identity=[&](const J& receipt){const auto& i=member(d,"input");
        need(text(receipt,"evidence_kind")== (synthetic?"synthetic":"actual"),"HISTORICAL_RECEIPT","actual current receipts required");
        need(text(receipt,"source_commit")==text(i,"source_commit"),"HISTORICAL_RECEIPT","receipt source differs from current candidate");
        need(member(receipt,"candidate_binary_sha256")==member(i,"candidate_binary").at("sha256"),"BINARY_MISMATCH","receipt production binary differs");
        need(member(receipt,"profile_sha256")==member(i,"runtime_profile").at("sha256"),"PROFILE_MISMATCH","receipt profile differs");
        need(member(receipt,"capability_sha256")==member(i,"capability").at("sha256"),"CAPABILITY_MISMATCH","receipt capability differs");};
    section("production",[&]{const auto& p=member(d,"production_cost");schema(p,"pas.current-production-chain-qualification.v1");identity(p);
        n::pin_shape(member(p,"status"));
        need(flag(p,"pass")&&flag(p,"production_packet_action_wake_pool_verified")&&flag(p,"sole_touch_owner_verified")&&flag(p,"bounded_all_attempts_verified"),"PRODUCTION_NOT_READY","production packet/action/wake/pool qualification missing");
        need(pin_equal(member(p,"cold_preflight_input"),member(member(d,"input"),"cold_preflight_input")),"COST_NOT_READY","production/cold prerequisite join");
        need(member(p,"normal_scenes")==J::array({"tap","hold","dense"}),"PRODUCTION_NOT_READY","three declared production scenes required");
        need(zero(member(p,"failed"))&&zero(member(p,"unknown"))&&zero(member(p,"final_active_contacts")),"PRODUCTION_NOT_READY","production failure/unknown/active contacts");
    });
    section("capture_rpc",[&]{const auto& p=member(d,"capture_rpc");schema(p,"pas.current-capture-rpc-qualification.v1");identity(p);
        need(flag(p,"pass")&&flag(p,"real_capture_rpc_measured")&&flag(p,"all_attempts_verified")&&candidate_capture_metrics_valid(member(p,"metrics")),"CAPTURE_RPC_NOT_READY","complete actual capture bench windows/attempts/relative guard required; diagnostic warnings are not added gates");
    });
    section("device",[&]{const auto& p=member(d,"device");schema(p,"pas.current-device-profile-receipt.v1");identity(p);
        need(flag(p,"pass")&&!text(p,"serial").empty()&&!text(p,"game_package").empty(),"DEVICE_NOT_READY","current device/package receipt required");
        need(w::hex(text(p,"game_apk_sha256"),64)&&w::hex(text(p,"fixture_apk_sha256"),64),"DEVICE_NOT_READY","current installed app/fixture hashes required");
        need(member(p,"capture_geometry")==J{{"width",1280},{"height",720},{"source_rotation",1}},"PROFILE_MISMATCH","current capture geometry differs");
        const auto& device=member(p,"device");for(const auto* k:{"android_release","android_sdk","cpu_abi","model","wm_size","wm_density"})member(device,k);
        need(member(p,"corner_mapping").is_array()&&member(p,"corner_mapping").size()==4,"PROFILE_MISMATCH","four corner mapping receipt required");
    });
    section("physical",[&]{const auto& p=member(d,"physical");schema(p,"pas.current-physical-qualification.v1");identity(p);
        need(flag(p,"pass")&&flag(p,"real_input_created")&&flag(p,"same_current_pixel_owner_verified")&&flag(p,"five_contact_mapping_verified")&&flag(p,"identity_1519_verified"),"PHYSICAL_NOT_READY","current physical/mapping/identity qualification missing");
        need(pin_equal(member(p,"device_receipt"),member(member(d,"input"),"device")),"HISTORICAL_RECEIPT","physical current-device receipt join");
        need(zero(member(p,"failed"))&&zero(member(p,"unknown"))&&zero(member(p,"final_active_contacts"))&&flag(p,"release_verified"),"PHYSICAL_NOT_READY","physical failed/unknown/release state");
        const auto& phases=member(p,"phases");need(phases.is_array()&&phases.size()==6,"PHYSICAL_NOT_READY","Hold entry/body/tail evidence required");
        std::set<std::string> names;for(const auto& phase:phases){const auto name=text(phase,"phase");need(names.insert(name).second,"PHYSICAL_NOT_READY","duplicate physical phase");
            const auto& count=member(phase,"opportunities");need(positive(count)&&integer(member(phase,"passed"))&&member(phase,"passed")==count&&zero(member(phase,"failed"))&&zero(member(phase,"unknown")),"PHYSICAL_NOT_READY","nonempty complete physical phase denominator required");}
        need(names==std::set<std::string>{"tap","hold_entry","hold_body","hold_tail","held_rotation","late_alignment"},"PHYSICAL_NOT_READY","physical current Tap/Hold/rotation phase set differs");
        need(candidate_physical_review_valid(member(d,"physical_owner_observation"),member(d,"physical_review"),member(p,"owner_observation"))&&member(d,"physical_review").at("phases")==phases,"PHYSICAL_NOT_READY","completed actual owner observation and subsequent independent review required");
    });
    section("authorization",[&]{const auto& p=member(d,"authorization");schema(p,"pas.current-score-only-authorization.v1");identity(p);
        need(text(p,"scope")=="chapter_legacy_available_in_single_pass_scores_only"&&text(p,"persistence")=="none"&&flag(p,"one_round")&&flag(p,"no_unlock")&&flag(p,"no_retry")&&flag(p,"stop_after_pass")&&flag(p,"device_operations_authorized"),"AUTHORIZATION_SCOPE","single-pass scores-only human scope required");
    });
    section("current_preflight",[&]{const auto& p=member(current,"preflight");const auto& device=member(d,"device");
        need(flag(p,"fingerprint_matches")&&!flag(p,"real_input_created"),"CURRENT_PREFLIGHT_MISMATCH","fresh read-only preflight failed");
        auto public_profile=member(d,"runtime_profile");if(public_profile.at("capture").contains("token_file"))public_profile["capture"]["token_file"]="<redacted>";
        need(member(p,"config")==public_profile,"PROFILE_MISMATCH","current loaded public config differs");
        need(member(p,"device")==member(device,"device")&&member(p,"capture_geometry")==member(device,"capture_geometry")&&member(p,"corner_mapping")==member(device,"corner_mapping"),"CURRENT_PREFLIGHT_MISMATCH","current device/geometry/mapping differs");
        need(member(p,"capability_report_sha256")==member(current,"capability_sha256")&&member(p,"current_fixture_apk_sha256")==member(device,"fixture_apk_sha256")&&member(p,"current_game_apk_sha256")==member(device,"game_apk_sha256"),"CURRENT_PREFLIGHT_MISMATCH","current installed app/capability differs");
        const auto& config=member(d,"runtime_profile");need(text(config,"serial")==text(device,"serial"),"PROFILE_MISMATCH","current serial differs");
        const auto& capture=member(config,"capture");const auto& touch=member(config,"touch");const auto& scheduler=member(config,"scheduler");const auto& game=member(config,"game");
        need(text(capture,"kind")=="emulator-grpc"&&text(capture,"transport")=="payload"&&text(capture,"image_format")=="rgb888"&&text(capture,"row_order")=="top-down"&&member(capture,"width")==1280&&member(capture,"height")==720&&member(capture,"source_rotation")==1&&member(capture,"grpc_read_chunk_kib")==256&&member(capture,"max_relative_lag_ms")==250,"PROFILE_MISMATCH","fixed current capture profile");
        need(text(touch,"kind")=="emulator-grpc"&&member(touch,"width")==720&&member(touch,"height")==1280&&member(touch,"rotation_deg")==90&&member(touch,"max_contacts")==5&&member(touch,"timeout_ms")==100,"PROFILE_MISMATCH","fixed current five-contact mapping");
        need(member(scheduler,"max_plans")==128&&member(scheduler,"max_steps")==16&&member(scheduler,"horizon_ms")==350&&member(scheduler,"evidence_max_age_ms")==100&&member(game,"lead_ms")==35&&member(game,"uncertainty_ms")==30&&member(game,"enabled_types")==J::array({"tap","hold","drag","flick"}),"PROFILE_MISMATCH","same original cost-effective scheduler/game profile");
        RuntimeConfig c;c.serial=text(config,"serial");c.width=1280;c.height=720;c.source_rotation=1;c.touch_width=720;c.touch_height=1280;c.touch_rotation=90;c.max_contacts=5;
        const auto matched=match_touch_capability(c,member(d,"capability"),member(device,"device"),text(device,"fixture_apk_sha256"));
        need(matched.at("fingerprint_matches").get<bool>(),"CAPABILITY_MISMATCH","capability schema/config/installed-device/five-contact fingerprint differs");
    });
    out["pass"]=ok;out["status"]=ok?(synthetic?"SYNTHETIC_CONTRACT_PASS":"VALIDATED_RECEIPTS"):"REJECTED";return out;
}
J current_context(const CandidateAdmissionRequest& request){
    wchar_t module[32768]{};const auto size=GetModuleFileNameW(nullptr,module,32768);need(size>0&&size<32768,"BINARY_MISMATCH","current executable path unavailable");
    return {{"workspace",fs::absolute(request.workspace).string()},{"source_commit",pas_session_build_commit},{"source_dirty",pas_session_build_dirty},{"compiled_source_sha256",J::parse(pas_session_source_hashes)},
        {"candidate_binary_sha256",g::streamed_sha256(module,256*w::MiB)},
        {"profile_sha256",g::streamed_sha256(request.config_path,4*w::MiB)},{"capability_sha256",g::streamed_sha256(request.capability_path,4*w::MiB)}};
}
struct Loaded {std::shared_ptr<g::Reader> reader;J documents,context;std::vector<J> files;};
Loaded load(const CandidateAdmissionRequest& request,bool collection=false){
    need(!request.input_path.empty(),"MISSING_INPUT","admission input path missing");
    need(w::hex(request.input_sha256,64),"MALFORMED_INPUT","externally pinned admission SHA256 required");
    need(fs::is_regular_file(request.input_path),"MISSING_INPUT","admission input file missing");
    auto reader=std::make_shared<g::Reader>(request.workspace);J input_pin={{"path",fs::absolute(request.input_path).string()},{"bytes",fs::file_size(request.input_path)},{"sha256",request.input_sha256}};
    J input;try{input=reader->object(input_pin);}catch(const std::exception& e){throw Reject("TAMPERED_OR_MALFORMED_INPUT",e.what());}
    schema(input,collection?"pas.current-candidate-collection-input.v1":"pas.current-candidate-admission-input.v1");need(text(input,"evidence_kind")=="actual","SYNTHETIC_EVIDENCE","fixture evidence cannot enter the actual factory");
    for(const auto* role:roles)if(!collection||collection_role(role))n::pin_shape(member(input,role));
    Loaded result{reader,{{"input",input}},current_context(request),{input_pin}};
    need(member(input,"candidate_binary").at("sha256")==result.context.at("candidate_binary_sha256"),"BINARY_MISMATCH","receipt does not bind current executing binary");
    need(text(input,"source_commit")==text(result.context,"source_commit")&&!flag(result.context,"source_dirty"),"SOURCE_MISMATCH","compiled current clean source identity required");
    need(member(input,"runtime_profile").at("sha256")==result.context.at("profile_sha256")&&g::same_path_text(text(input.at("runtime_profile"),"path"),request.config_path),"PROFILE_MISMATCH","config path/hash differs from pinned profile");
    need(member(input,"capability").at("sha256")==result.context.at("capability_sha256")&&g::same_path_text(text(input.at("capability"),"path"),request.capability_path),"CAPABILITY_MISMATCH","capability path/hash differs");
    reader->bound(input.at("candidate_binary"),256*w::MiB);
    for(const auto* role:roles)if(std::string_view(role)!="candidate_binary"&&(!collection||input.contains(role))){result.documents[role]=reader->object(input.at(role));result.files.push_back(input.at(role));}
    if(collection){candidate_collection_review_mode(input);for(const auto* role:{"physical_owner_observation","physical_review"})if(input.contains(role))result.documents[role]=reader->object(input.at(role));}
    else{const auto& physical=result.documents.at("physical");result.documents["physical_owner_observation"]=reader->object(physical.at("owner_observation"));result.documents["physical_review"]=reader->object(physical.at("independent_review"));}
    reader->entries(result.documents.at("source_manifest").at("files"));
    const auto& build=result.documents.at("production_build");reader->entries(build.at("files"));const auto core_plan=reader->object(build.at("core_build_plan"));
    need(core_plan.at("schema")=="pas.fresh-core-build-plan.v1"&&core_plan.at("source_commit")==input.at("source_commit")&&core_plan.at("configuration")=="release"&&core_plan.at("translation_units")==22&&!flag(core_plan,"old_donor_objects_used")&&zero(member(core_plan,"device_operations"))&&!flag(core_plan,"live_ready"),"SOURCE_MISMATCH","existing current fresh22/no-donor production core plan required");
    reader->object(core_plan.at("compiler_snapshot"));
    const auto build_root=g::final_path(text(build,"build_root"));std::map<std::string,std::set<std::string>> objects;std::set<std::string> products;
    for(const auto& pin:build.at("files")){const auto path=reader->bound(pin);const auto relative=path.lexically_relative(build_root);need(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=".."&&products.insert(relative.generic_string()).second,"SOURCE_MISMATCH","unique contained current production products required");
        const auto name=relative.generic_string();for(const auto* target:{"pas_core","pas_current_candidate","pas_emulator","pas_proto","pas_bench"})if(name.starts_with("production/CMakeFiles/"+std::string(target)+".dir/")&&name.ends_with(".obj"))need(objects[target].insert(path.filename().string()).second,"SOURCE_MISMATCH","duplicate current production target object");}
    std::set<std::string> formal;for(const auto& path:g::component_delta::units())if(path.get<std::string>().starts_with("src/"))formal.insert(fs::path(path.get<std::string>()).filename().string()+".obj");
    need(objects["pas_core"]==formal&&objects["pas_current_candidate"]==std::set<std::string>{"bvi.cpp.obj","contact_policy.cpp.obj","relation_policy.cpp.obj","constraint_policy.cpp.obj","front.cpp.obj","own_bridge.cpp.obj","extract.cpp.obj","current.cpp.obj"}&&objects["pas_emulator"]==std::set<std::string>{"emulator.cpp.obj","grpc_transport.cpp.obj","runtime.cpp.obj","manual_session.cpp.obj","candidate_admission.cpp.obj","candidate_receipts.cpp.obj"}&&objects["pas_proto"]==std::set<std::string>{"emulator_controller.pb.cc.obj","emulator_controller.grpc.pb.cc.obj"}&&objects["pas_bench"]==std::set<std::string>{"bench.cpp.obj","bench_workloads.cpp.obj","touch_bench.cpp.obj","native_capture.cpp.obj","scrcpy_capture.cpp.obj"},"SOURCE_MISMATCH","complete fresh production core22/action8/emulator6/proto2/bench5 object sets required");
    // Existing owned-process/native-input schema; only the bounded metadata is
    // traversed here. Historical ZIP/raw members are never restored or rehashed.
    const auto& catalogue=member(input,"native_processes");need(catalogue.is_array()&&!catalogue.empty()&&catalogue.size()<=128,"MALFORMED_RECEIPT","bounded native evidence catalogue required");
    std::map<std::string,J> native_inputs;
    const auto native=[&](const J& proof,bool device_allowed){
        need(proof.is_object()&&proof.size()==8,"MALFORMED_RECEIPT","eight native evidence pins required");
        for(const auto* k:{"specification","input_closure","result","verification","freeze","state","contract","command"})reader->bound(proof.at(k));
        const auto ver=reader->owned(proof.at("verification"),0,0,true),state=reader->object(proof.at("state")),closure=reader->object(proof.at("input_closure"));
        const auto spec=reader->object(proof.at("specification")),command=reader->object(proof.at("command")),declared=reader->object(proof.at("contract")),res=reader->object(proof.at("result"));const auto stage=ver.at("stage");
        need(spec.at("stage")==stage&&command.at("stage")==stage&&res.at("stage")==stage&&declared.at("spec").at("name")==stage,"MALFORMED_RECEIPT","native stage join");
        need(g::same_path_text(text(proof.at("result"),"path"),fs::path(text(proof.at("verification"),"path")).parent_path()/(stage.get<std::string>()+"-result.json")),"MALFORMED_RECEIPT","owned native result exact path join");
        for(const auto* k:{"exe","argv","native_exit","runner_exit","total_s","stream_cap"})need(spec.at(k)==command.at("spec").at(k)&&spec.at(k)==declared.at("spec").at(k),"MALFORMED_RECEIPT","native launch/freeze command join");
        need(spec.at("native_exit")==0&&spec.at("runner_exit")==0&&g::same_path_text(spec.at("exe").get<std::string>(),res.at("facts").at("root_image").get<std::string>()),"MALFORMED_RECEIPT","native exact image/exit join");
        need(state.at("status")=="COMPLETE"&&state.at("running").is_null()&&state.at("native_stages_consumed")==1&&state.at("native_launches_known")==1&&state.at("freeze_sha")==proof.at("freeze").at("sha256")&&state.at("contract_sha")==proof.at("contract").at("sha256"),"MALFORMED_RECEIPT","native COMPLETE seal");
        bool sealed=false;for(const auto& p:state.at("receipts"))sealed|=pin_equal(p,proof.at("verification"));need(sealed,"MALFORMED_RECEIPT","native verification not sealed");
        need(closure.at("schema")=="pas.current-input-closure-verification.v1"&&closure.at("stage")==stage&&closure.at("status")=="COMPLETE"&&flag(closure,"pass")&&flag(closure,"before_verified")&&flag(closure,"after_verified")&&pin_equal(closure.at("verification"),proof.at("verification"))&&pin_equal(closure.at("state"),proof.at("state"))&&flag(declared,"device_allowed")==device_allowed,"MALFORMED_RECEIPT","native before/after/device scope closure");
        const auto root=reader->object(closure.at("root"),32768);need(root.at("schema")=="pas.current-input-closure.v1"&&root.at("stage")==stage&&root.at("leaf_count")==closure.at("leaf_count")&&root.at("files_count")==closure.at("files_count")&&candidate_native_closure_output_valid(root,proof)&&root.at("leaves").is_array()&&!root.at("leaves").empty()&&root.at("leaves").size()<=128,"MALFORMED_RECEIPT","native closure root denominator/output/bound");
        std::set<std::string> leaves;std::map<std::string,J> inputs;std::size_t count=0;for(const auto& pin:root.at("leaves")){const auto key=fs::path(text(pin,"path")).generic_string();need(!pin_equal(pin,closure.at("root"))&&!pin_equal(pin,proof.at("input_closure"))&&leaves.insert(key).second,"MALFORMED_RECEIPT","native closure duplicate/self leaf");const auto leaf=reader->object(pin,32768);need(leaf.at("schema")=="pas.current-input-closure-leaf.v1"&&leaf.at("stage")==stage&&leaf.at("index")==count++&&leaf.at("files").is_array()&&!leaf.at("files").empty()&&leaf.at("files").size()<=64,"MALFORMED_RECEIPT","native ordered leaf");for(const auto& p:leaf.at("files")){n::pin_shape(p);need(!pin_equal(p,closure.at("root"))&&!pin_equal(p,proof.at("input_closure"))&&inputs.emplace(fs::path(text(p,"path")).generic_string(),p).second,"MALFORMED_RECEIPT","duplicate/self native closure input");}}
        need(count==closure.at("leaf_count")&&inputs.size()==closure.at("files_count")&&inputs.size()<=8192,"MALFORMED_RECEIPT","native closure complete denominator");
        for(const auto& key:leaves)need(!inputs.contains(key),"MALFORMED_RECEIPT","native closure leaf/input overlap");
        const auto frozen=reader->object(proof.at("freeze"),65536);bool root_frozen=false;for(const auto& p:frozen.at("files")){n::pin_shape(p);root_frozen|=pin_equal(p,closure.at("root"));const auto [found,added]=inputs.emplace(fs::path(text(p,"path")).generic_string(),p);need(added||pin_equal(found->second,p),"MALFORMED_RECEIPT","conflicting frozen/native closure input");}need(root_frozen,"MALFORMED_RECEIPT","native input root not frozen");
        const auto direct_key=fs::path(text(proof.at("specification"),"path")).generic_string();need(inputs.contains(direct_key)&&pin_equal(inputs.at(direct_key),proof.at("specification")),"MALFORMED_RECEIPT","native direct specification not frozen");
        native_inputs=std::move(inputs);return spec;
    };
    const auto find_native=[&](const J& verification)->const J& {const J* found=nullptr;for(const auto& p:catalogue)if(pin_equal(p.at("verification"),verification)){need(found==nullptr,"MALFORMED_RECEIPT","duplicate native verification join");found=&p;}need(found!=nullptr,"MISSING_RECEIPT","native evidence for production build missing");return *found;};
    native(find_native(build.at("configure_receipt")),false);need(build.at("build_receipts").is_array()&&!build.at("build_receipts").empty()&&build.at("build_receipts").size()<=64,"MISSING_RECEIPT","production build native groups required");for(const auto& pin:build.at("build_receipts")){native(find_native(pin),false);for(const auto* binding:{"source_manifest","core_build_plan"}){const auto key=fs::path(text(build.at(binding),"path")).generic_string();need(native_inputs.contains(key)&&pin_equal(native_inputs.at(key),build.at(binding)),"SOURCE_MISMATCH","production native/source manifest/core plan join");}
        for(const auto& source:result.documents.at("source_manifest").at("files")){const auto key=fs::path(text(source,"path")).generic_string();need(native_inputs.contains(key)&&pin_equal(native_inputs.at(key),source),"SOURCE_MISMATCH","actual production compiled source member not frozen in native closure");}}
    // These are only the seven newly affected production controls. Original75
    // and production9 remain the separately verified preserved prerequisites.
    std::set<std::string> controls={"candidate_admission_controls","candidate_receipt_collector_controls","production_entry_controls","score_only_invalid","score_only_tap","score_only_hold","score_only_dense"};
    need(build.at("controls").is_array()&&build.at("controls").size()==controls.size(),"MISSING_RECEIPT","all seven affected current production controls required");
    for(const auto& row:build.at("controls")){const auto target=text(row,"target");need(controls.erase(target)==1,"MALFORMED_RECEIPT","exact unique affected production controls");const auto report=reader->object(row.at("report"));const auto& proof=find_native(row.at("receipt"));need(pin_equal(proof.at("input_closure"),row.at("input_closure")),"MALFORMED_RECEIPT","affected production control native closure join");const auto spec=native(proof,false);
        bool built=false;for(const auto& pin:build.at("files"))built|=pin_equal(pin,row.at("binary"));need(built&&g::same_path_text(text(spec,"exe"),text(row.at("binary"),"path"))&&zero(member(report,"failed"))&&zero(member(report,"device_operations"))&&!flag(report,"live_ready"),"PRODUCTION_NOT_READY","current affected control binary/native/scope join");
        for(const auto* binding:{"source_manifest","core_build_plan"}){const auto key=fs::path(text(build.at(binding),"path")).generic_string();need(native_inputs.contains(key)&&pin_equal(native_inputs.at(key),build.at(binding)),"SOURCE_MISMATCH","affected production controls current source/plan join");}
        if(target=="candidate_admission_controls"){schema(report,"pas.candidate-admission-controls.v1");need(report.at("assertions")==82&&report.at("checks").is_array()&&report.at("checks").size()==82&&flag(report,"pass")&&flag(report,"synthetic_fixture_only")&&!flag(report,"qualification_issued")&&zero(member(report,"gameplay_files_created")),"PRODUCTION_NOT_READY","complete82 synthetic-only current admission controls required");std::set<std::string> ids;for(const auto& check:report.at("checks"))need(flag(check,"pass")&&ids.insert(text(check,"id")).second,"PRODUCTION_NOT_READY","unique complete admission control results");need(spec.at("argv")==J::array({row.at("report").at("path")}),"MALFORMED_RECEIPT","current admission control report command join");}
        else if(target=="candidate_receipt_collector_controls"){schema(report,"pas.candidate-receipt-collector-controls.v1");need(report.at("assertions")==85&&report.at("checks").is_array()&&report.at("checks").size()==85&&flag(report,"pass")&&flag(report,"synthetic_fixture_only")&&!flag(report,"qualification_issued")&&zero(member(report,"device_endpoints"))&&zero(member(report,"gameplay_files_created")),"PRODUCTION_NOT_READY","complete85 endpoint-free collector controls required");std::set<std::string> ids;for(const auto& check:report.at("checks"))need(flag(check,"pass")&&ids.insert(text(check,"id")).second,"PRODUCTION_NOT_READY","unique collector control results");need(spec.at("argv")==J::array({row.at("report").at("path")}),"MALFORMED_RECEIPT","collector controls command join");}
        else if(target=="production_entry_controls"){schema(report,"pas.production-entry-closed-controls.v1");need(report.at("assertions")==13&&report.at("checks").is_array()&&report.at("checks").size()==13&&flag(report,"actual_production_function_called")&&!flag(report,"qualification_issued"),"PRODUCTION_NOT_READY","actual production closed-entry13 controls required");for(std::size_t i=0;i<13;++i)need(flag(report.at("checks")[i],i<9?"actual_production_entry_rejects_UNQUALIFIED":"pass"),"PRODUCTION_NOT_READY","actual production closed-entry/geometry control failed");need(spec.at("argv")==J::array({row.at("report").at("path")}),"MALFORMED_RECEIPT","current closed-entry control report command join");}
        else{schema(report,"pas.score-only-literal-controls.v1");const auto scene=target.substr(11);need(flag(report,"pass")&&!flag(report,"cost_qualification")&&!flag(report,"physical_qualification")&&!flag(report,"whole_candidate_qualification_issued")&&flag(report,"no_gameplay_files_created")&&report.at("base_component_source_commit")==input.at("source_commit")&&!flag(report,"base_component_source_dirty")&&report.at("actual_manual_translation_unit_sha256")==result.context.at("compiled_source_sha256").at("src/manual_session.cpp"),"PRODUCTION_NOT_READY","current score-only None lifecycle/manual source controls required");need(spec.at("argv")==J::array({scene,fs::path(text(row.at("report"),"path")).parent_path().string()}),"MALFORMED_RECEIPT","current score-only exact scene/output command join");if(scene=="invalid")need(report.at("invalid_requests_rejected_before_io")==8&&report.at("controlled_stop_lifecycle_cases")==2&&report.at("result_tail_fake_clock_cases")==4,"PRODUCTION_NOT_READY","all affected invalid/Stop/result-tail controls required");else need(report.at("scene")==scene&&flag(report,"literal_production_source_executed")&&flag(report,"all_downs_terminalized")&&zero(member(report,"contacts_final"))&&report.at("down").get<std::uint64_t>()==report.at("up").get<std::uint64_t>()+report.at("contacts_released_by_cancel").get<std::uint64_t>()&&positive(member(report,"release_calls"))&&(scene=="dense"||positive(member(report,"down"))),"PRODUCTION_NOT_READY","score-only complete contact balance/release coverage required");}
    }
    // The small production envelope points to the existing full normal3
    // status/report/audit schemas; its pass booleans are not proof by themselves.
    const auto& production=result.documents.at("production_cost");const auto status=reader->object(member(production,"status"));
    need(status.at("schema")=="pas.pre-emulator-production-status.v1"&&status.at("source_commit")==input.at("source_commit")&&!flag(status,"stopped")&&status.at("failure").is_null()&&status.at("remaining").empty()&&status.at("rows").size()==3,"PRODUCTION_NOT_READY","fresh complete current normal3 production status required");
    const auto production_plan=reader->object(status.at("plan"));need(production_plan.at("schema")=="pas.pre-emulator-production-plan.v1"&&production_plan.at("source_commit")==input.at("source_commit")&&production_plan.at("plans").size()==3,"PRODUCTION_NOT_READY","current normal3 production plan required");
    const std::array<const char*,3> scenes={"tap","hold","dense"};
    for(std::size_t index=0;index<3;++index){const auto& row=status.at("rows")[index];const std::string scene=scenes[index];const auto& planned=production_plan.at("plans")[index];need(planned.at("id")=="normal-"+scene&&planned.at("mode")=="release"&&planned.at("scene")==scene&&planned.at("n")==2560&&planned.at("cadence")==20,"PRODUCTION_NOT_READY","exact original normal3 current production plan");const auto report=reader->object(row.at("report")),audit=reader->object(row.at("audit"));
        need(row.at("id")=="normal-"+scene&&flag(row,"normal_cost_valid")&&report.at("schema")=="pas.windows-production-pipeline-cold.v1"&&report.at("scene")==scene&&report.at("source_build_commit")==input.at("source_commit")&&!flag(report,"source_build_dirty")&&report.at("actual_manual_translation_unit_sha256")==result.context.at("compiled_source_sha256").at("src/manual_session.cpp")&&flag(report,"endpoint_free")&&flag(report,"public_entry_rejected")&&flag(report,"production_packet_action_wake_archive_executed")&&!flag(report,"real_capture_rpc_measured")&&report.at("device_operations")==0&&!flag(report,"live_ready")&&report.at("requested")==2560&&report.at("warmup")==256&&report.at("measurement_attempts")==2304&&report.at("cadence_ms")==20,"PRODUCTION_NOT_READY","current full normal production report/source/protocol join");
        need(audit.at("schema")=="pas.windows-production-pipeline-audit.v5"&&flag(audit,"integrity_verified")&&audit.at("source_commit")==input.at("source_commit")&&pin_equal(audit.at("report"),row.at("report"))&&flag(audit,"normal_cost_valid")&&audit.at("normal_cost_failures").empty(),"PRODUCTION_NOT_READY","independent complete raw v5 audit/original normal gates required");
        const auto producer=native(row.at("process"),false);const auto raw_root=fs::path(text(row.at("report"),"path")).parent_path().string();
        bool producer_built=false;for(const auto& p:build.at("files"))if(fs::path(text(p,"path")).filename()=="production_pipeline_cold.exe")producer_built|=g::same_path_text(producer.at("exe").get<std::string>(),text(p,"path"));
        need(producer_built&&producer.at("argv")==J::array({scene,"2560","20",raw_root}),"PRODUCTION_NOT_READY","exact current literal production image/command");
        const J* auditor_proof=nullptr;for(const auto& p:catalogue){const auto spec=reader->object(p.at("specification"));if(std::find(spec.at("argv").begin(),spec.at("argv").end(),row.at("audit").at("path"))!=spec.at("argv").end()){need(auditor_proof==nullptr,"MALFORMED_RECEIPT","duplicate production auditor output");auditor_proof=&p;}}
        need(auditor_proof!=nullptr,"MISSING_RECEIPT","current production full raw auditor native proof missing");const auto auditor=native(*auditor_proof,false);bool auditor_built=false;for(const auto& p:build.at("files"))if(fs::path(text(p,"path")).filename()=="production_pipeline_audit.exe")auditor_built|=g::same_path_text(auditor.at("exe").get<std::string>(),text(p,"path"));
        need(auditor_built&&auditor.at("argv")==J::array({"--root",raw_root,"--expected-report-sha256",row.at("report").at("sha256"),"--source-commit",input.at("source_commit"),"--output",row.at("audit").at("path")}),"PRODUCTION_NOT_READY","exact current independent raw auditor command");
        const auto report_key=fs::path(text(row.at("report"),"path")).generic_string();need(native_inputs.contains(report_key)&&pin_equal(native_inputs.at(report_key),row.at("report")),"PRODUCTION_NOT_READY","full raw audit frozen report SHA join");
    }
    for(const auto* role:{"capture_rpc","device","physical","physical_owner"}){
        if(std::string_view(role)=="physical_owner"){if(!result.documents.contains("physical_owner_observation"))continue;}
        else if(collection&&(!input.contains(role)||std::string_view(role)=="physical"))continue;
        const auto& qualification=member(input,"qualification_processes");
        const auto& proof=member(qualification,role);const auto spec=native(proof,std::string_view(role)!="production_cost");
        const auto role_name=std::string_view(role)=="physical_owner"?"physical_owner_observation":role;const auto& report=result.documents.at(role_name);
        const auto& producer=member(report,"producer_binary");reader->bound(producer,256*w::MiB);bool built=false;for(const auto& pin:build.at("files"))built|=pin_equal(pin,producer);
        need(built&&g::same_path_text(spec.at("exe").get<std::string>(),text(producer,"path")),"BINARY_MISMATCH","qualification producer must be in the current production build closure");
        need(pin_equal(producer,input.at("candidate_binary")),"BINARY_MISMATCH","qualification producer must be this actual production executable");
        const auto report_pin=std::string_view(role)=="physical_owner"?(collection?input.at("physical_owner_observation"):result.documents.at("physical").at("owner_observation")):input.at(role);
        const auto& argv=member(spec,"argv");need(argv.is_array()&&argv.size()==15&&argv[2].is_string(),"MALFORMED_RECEIPT","exact collector command shape required");
        const J* frozen_collection=nullptr;for(const auto& [path,pin]:native_inputs)if(g::same_path_text(path,argv[2].get<std::string>())){need(frozen_collection==nullptr,"MALFORMED_RECEIPT","duplicate frozen collector input");frozen_collection=&pin;}
        need(frozen_collection!=nullptr,"SOURCE_MISMATCH","collector input must be in actual frozen native closure");
        const auto prior_input=reader->object(*frozen_collection);
        const auto owner_pin=std::string_view(role)=="physical"?result.documents.at("physical").at("owner_observation"):J{};
        const auto review_pin=std::string_view(role)=="physical"?result.documents.at("physical").at("independent_review"):J{};
        need(candidate_collection_protocol_valid(argv,role,*frozen_collection,prior_input,input,report_pin,text(result.context,"workspace"),text(result.documents.at("device"),"game_package"),owner_pin,review_pin),"SOURCE_MISMATCH","exact collector command/input SHA/current prerequisites and review mode join");
        for(const auto* binding:{"candidate_binary","source_manifest","runtime_profile","capability"}){const auto key=fs::path(text(input.at(binding),"path")).generic_string();need(native_inputs.contains(key)&&pin_equal(native_inputs.at(key),input.at(binding)),"SOURCE_MISMATCH","qualification native exact input join");}
        if(std::string_view(role)!="device"){for(const auto* binding:{"device","capture_rpc"}){if(std::string_view(role)=="capture_rpc"&&std::string_view(binding)=="capture_rpc")continue;const auto key=fs::path(text(input.at(binding),"path")).generic_string();need(native_inputs.contains(key)&&pin_equal(native_inputs.at(key),input.at(binding)),"SOURCE_MISMATCH","qualification current device/capture frozen input join");}}
        if(std::string_view(role)=="physical"){for(const auto* binding:{"owner_observation","independent_review"}){const auto& pin=result.documents.at("physical").at(binding);const auto key=fs::path(text(pin,"path")).generic_string();need(native_inputs.contains(key)&&pin_equal(native_inputs.at(key),pin),"SOURCE_MISMATCH","physical finalizer frozen actual owner/subsequent review join");}}
    }
    n::Preflight preflight(*reader,request.workspace,result.documents.at("cold_preflight_input"));result.documents["cold_preflight"]=preflight.run();
    need(result.documents.at("cold_preflight").at("pass").get<bool>(),"COST_NOT_READY","existing AA/noise/ABBA verifier did not pass");
    reader->finish();return result;
}
J actual_preflight(const CandidateAdmissionRequest& request,const J& documents){
    auto p=game_preflight(request.config_path.string(),request.capability_path.string());
    p["current_game_apk_sha256"]=installed_apk_sha256(find_adb(),text(documents.at("device"),"serial"),text(documents.at("device"),"game_package"));return p;
}
J rejection(const std::exception& e){auto r=result_base(false);const auto* detail=dynamic_cast<const Reject*>(&e);r["issues"].push_back({{"section","files"},{"code",detail?detail->code:"MALFORMED_RECEIPT"},{"reason",e.what()}});if(detail&&(detail->code.starts_with("MISSING_")||detail->code.ends_with("_NOT_READY")))r["status"]="NOT_READY";return r;}
J stored_structure(const Loaded& loaded){auto context=loaded.context;const auto& d=loaded.documents;const auto& dev=d.at("device");
    context["preflight"]={{"fingerprint_matches",true},{"real_input_created",false},{"config",d.at("runtime_profile")},{"device",dev.at("device")},{"capture_geometry",dev.at("capture_geometry")},{"corner_mapping",dev.at("corner_mapping")},{"capability_report_sha256",loaded.context.at("capability_sha256")},{"current_fixture_apk_sha256",dev.at("fixture_apk_sha256")},{"current_game_apk_sha256",dev.at("game_apk_sha256")}};return context;}
void assert_report(const J& report){if(!report.at("pass").get<bool>())throw std::runtime_error("current candidate live admission UNQUALIFIED: "+report.at("issues").dump());}
}
bool candidate_native_closure_output_valid(const nlohmann::json& root,
    const nlohmann::json& proof) noexcept {try{
    return preflight_graph::same_path_text(
        root.at("verification_output").get<std::string>(),
        proof.at("input_closure").at("path").get<std::string>());
}catch(...){return false;}}

CandidateColdReceiptValidation validate_synthetic_candidate_receipts(const J& documents,const J& current){return CandidateColdReceiptValidation(contract(documents,current,true));}
CandidateCollectionInputs prepare_candidate_collection(const CandidateAdmissionRequest& request){auto loaded=load(request,true);assert_report(contract(loaded.documents,loaded.context,false,true));auto held=std::make_shared<Loaded>(std::move(loaded));return CandidateCollectionInputs(held->documents,held->context,[held,request]{held->reader->finish();const auto now=current_context(request);need(now==held->context,"SOURCE_MISMATCH","collection binary/source/profile changed");});}
J inspect_candidate_receipts(const CandidateAdmissionRequest& request){try{
    auto loaded=load(request);
    // A successful read-only inspection stops exactly at the current-device
    // boundary. It does not substitute the stored receipt for an actual probe.
    auto report=contract(loaded.documents,stored_structure(loaded),false);
    if(report.at("pass").get<bool>())report["status"]="NOT_READY";report["pass"]=false;report["issues"].push_back({{"section","current_preflight"},{"code","CURRENT_PREFLIGHT_REQUIRED"},{"reason","actual current read-only device preflight has not been performed"}});return report;
}catch(const std::exception& e){return rejection(e);}}
CandidateLiveAdmission load_candidate_score_admission(const CandidateAdmissionRequest& request){
    Loaded loaded;try{loaded=load(request);}catch(const std::exception& e){throw std::runtime_error("current candidate live admission UNQUALIFIED: "+rejection(e).at("issues").dump());}
    // Verify every non-device join before discovery/capture. The stored device
    // fields are used only to check the contract; they cannot authorize entry.
    const auto& d=loaded.documents;assert_report(contract(d,stored_structure(loaded),false));loaded.context["preflight"]=actual_preflight(request,d);assert_report(contract(d,loaded.context,false));loaded.reader->finish();
    auto sealed=std::make_shared<Loaded>(std::move(loaded));auto used=std::make_shared<std::atomic<bool>>(false);
    return CandidateLiveAdmission([sealed,used,request]{
        if(used->exchange(true))throw std::runtime_error("current candidate live admission UNQUALIFIED: score-only admission already consumed");
        sealed->reader->finish();auto current=current_context(request);current["preflight"]=actual_preflight(request,sealed->documents);assert_report(contract(sealed->documents,current,false));sealed->reader->finish();
    },request.config_path,request.capability_path);
}
}
