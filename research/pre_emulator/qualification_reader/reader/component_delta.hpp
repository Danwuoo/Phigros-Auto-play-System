#pragma once
#include "graph_reader.hpp"
#include <regex>

// Exact current CPU-repair declaration. Historical main32 qualification stays
// historical; these checks bind fresh objects/libraries and cold controls only.
namespace pas::preflight_graph::component_delta {
inline J targets(){return J::array({"current_chain","consumer_priority_controls","worker_affinity_controls","producer_wait_tests","pas_core","own_bridge","owner_checkpoint_tests","owner_expiry_tests","component_mask_tests","uniform_fnv_tests"});}
inline J units(){
    J out=J::array();for(const auto* name:{"core","config","analysis","adb","journal","touch_evidence","preview","game","game_clip_replay","game_cold_pipeline","game_cold_corpus","game_motion","game_tracking","game_tracking_methods","game_tracking_analysis","game_tracking_shadow","game_dataset","game_dataset_tools","game_session","session_archive","session_pixel_clips","session_recording"})out.push_back("src/"+std::string(name)+".cpp");
    for(const auto* path:{"research/prelive_current/own_bridge.cpp","research/bvi_owner_cooperative/extract.cpp","research/prelive_current/chain.cpp","research/prelive_current/tests/consumer_priority_tests.cpp","research/prelive_current/tests/worker_affinity_tests.cpp","research/prelive_current/tests/producer_wait_tests.cpp","research/prelive_current/tests/owner_checkpoint_tests.cpp","research/prelive_current/tests/owner_expiry_tests.cpp","research/prelive_current/tests/component_mask_tests.cpp","research/bvi_owner_cooperative/cold_verification/main.cpp"})out.push_back(path);return out;
}
inline bool pin_equal(const J& a,const J& b){return a.is_object()&&b.is_object()&&same_path_text(a.at("path").get<std::string>(),b.at("path").get<std::string>())&&a.at("bytes")==b.at("bytes")&&a.at("sha256")==b.at("sha256");}
inline bool formal_path(std::string_view path){return path.starts_with("src/")||path.starts_with("include/")||path.starts_with("apps/pas/")||path=="CMakeLists.txt";}
inline bool allowed_formal_path(std::string_view path){return path=="src/game.cpp"||path=="src/manual_session.cpp"||path=="src/candidate_admission.cpp"||path=="src/candidate_receipts.cpp"||path=="include/pas/candidate_receipts.hpp"||path=="include/pas/candidate_admission.hpp"||path=="apps/pas/main.cpp"||path=="CMakeLists.txt";}
inline bool allowed_change(std::string_view path){
    if(allowed_formal_path(path))return true;
    const std::set<std::string_view> exact={"AGENTS.md","README.md","research/pre_emulator/PLAN.md","research/prelive_current/chain.cpp","research/prelive_current/consumer_priority.hpp","research/prelive_current/tests/consumer_priority_tests.cpp","research/prelive_current/worker_affinity.hpp","research/prelive_current/tests/worker_affinity_tests.cpp","research/prelive_current/producer_wait.hpp","research/prelive_current/tests/producer_wait_tests.cpp","research/prelive_current/diagnostic_processor.hpp","research/prelive_current/diagnostic_phase_profile.hpp","research/prelive_current/diagnostic_phase_summary.cpp","research/prelive_current/owner_candidate_step.hpp","research/prelive_current/runtime.hpp","research/prelive_current/CMakeLists.txt","research/pre_emulator/qualification_reader/main.cpp","research/pre_emulator/qualification_reader/controls.cpp","research/pre_emulator/qualification_reader/reader/narrow_preflight.hpp","research/pre_emulator/qualification_reader/reader/component_delta.hpp","tests/candidate_admission_tests.cpp","tests/candidate_receipt_collector_tests.cpp","tests/score_session_tests.cpp","research/bvi_owner_cooperative/extract.cpp","research/bvi_owner_cooperative/cold_verification/main.cpp","research/bvi_owner_cooperative/cold_verification/observation_equal.hpp","research/bvi_owner_cooperative/cold_verification/CMakeLists.txt","research/bvi_owner_cooperative/cold_verification/run.ps1","research/prelive_current/tests/owner_checkpoint_tests.cpp","research/prelive_current/tests/component_mask_tests.cpp","research/windows_production_build/CMakeLists.txt"};
    if(exact.contains(path))return true;
    if(path.starts_with("research/pre_emulator/")){const auto tail=path.substr(22);if((tail.ends_with(".ps1")&&tail.find('/')==std::string_view::npos)||tail=="CMakeLists.txt"||tail.ends_with("/CMakeLists.txt"))return true;}
    return path.starts_with("docs/")&&(path.ends_with(".md")||path.ends_with(".json"))&&path!="docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md";
}
inline std::set<std::string> strings(const J& rows,const char* why){
    w::require(rows.is_array()&&rows.size()<=2048,why);std::set<std::string> out;
    for(const auto& row:rows){const auto path=row.get<std::string>();const fs::path p(path);w::require(!path.empty()&&!p.is_absolute()&&path.find('\\')==std::string::npos&&path.find(':')==std::string::npos,why);for(const auto& part:p)w::require(part!="."&&part!="..",why);w::require(out.insert(path).second,why);}return out;
}
inline void declaration(const J& delta,const J& plan,const std::map<std::string,J>& current,const std::map<std::string,J>& base){
    w::require(delta.at("component_cpu_delta")==true&&delta.at("component_cpu_delta").is_boolean()&&delta.at("formal_rebuilt")==true&&delta.at("formal_rebuilt").is_boolean()&&
        plan.at("component_cpu_delta")==true&&plan.at("component_cpu_delta").is_boolean()&&plan.at("formal_rebuilt")==true&&plan.at("formal_rebuilt").is_boolean()&&
        !delta.value("maintenance_only_amendment",false)&&plan.at("translation_units").is_number_integer()&&plan.at("translation_units")==32&&plan.at("delta_targets")==targets()&&plan.at("translation_unit_sources")==units()&&
        plan.at("rebuilt_static_targets")==J::array({"own_bridge","pas_core"}),"exact fresh32/two-library CPU delta declaration");
    for(const auto& path:units())w::require(current.contains(path.get<std::string>()),"every exact fresh TU must be present in compiled source manifest");
    const auto changed=strings(delta.at("changed_paths"),"unique declared CPU changed paths"),declared=strings(delta.at("formal_changed_paths"),"unique declared formal changes");
    for(const auto& path:changed)w::require(allowed_change(path),"change outside exact CPU/cold maintenance scope");
    w::require(strings(plan.at("formal_changed_paths"),"plan formal changes")==declared,"plan/build formal changed-path join");
    std::set<std::string> expected,actual;for(const auto& path:changed)if(formal_path(path)){w::require(allowed_formal_path(path),"formal change outside exact CPU repair scope");expected.insert(path);}
    for(const auto& [path,pin]:base)if(formal_path(path)){w::require(current.contains(path),"formal source removal rejected");if(!pin_equal(pin,current.at(path)))actual.insert(path);}
    for(const auto& [path,pin]:current){(void)pin;if(formal_path(path)&&!base.contains(path))actual.insert(path);}
    w::require(!declared.empty()&&declared.contains("src/game.cpp")&&declared==expected&&declared==actual,"exact actual/declaration/git formal-source delta");
    for(const auto& [path,pin]:base)if(current.contains(path)&&!pin_equal(pin,current.at(path)))w::require(changed.contains(path),"undeclared compiled source byte change");
    for(const auto& [path,pin]:current){(void)pin;if(!base.contains(path))w::require(changed.contains(path),"undeclared new compiled source");}
}
inline const J& role(const J& rows,const std::string& name){
    w::require(rows.is_array(),"static/library binding array");const J* found=nullptr;for(const auto& pin:rows)if(fs::path(pin.at("path").get<std::string>()).filename()==name){w::require(!found,"duplicate logical static/output role");found=&pin;}w::require(found,"missing logical static/output role");return *found;
}
inline void libraries(const J& delta,const J& plan,const std::map<std::string,J>& base_files){
    w::require(delta.at("linked_static_inputs").is_array()&&delta.at("linked_static_inputs").size()==5&&delta.at("rebuilt_static_inputs").is_array()&&delta.at("rebuilt_static_inputs").size()==2&&plan.at("rebuilt_static_targets")==J::array({"own_bridge","pas_core"}),"exact five effective/two rebuilt static inputs");
    const fs::path build=delta.at("build_root").get<std::string>();w::require(build.is_absolute(),"absolute fresh CPU build root");
    for(const auto* name:{"current_rails.lib","current_front.lib","exact_bvi_v3.lib","own_bridge.lib","pas_core.lib"}){
        const auto& pin=role(delta.at("linked_static_inputs"),name);const auto path=fs::path(pin.at("path").get<std::string>()).lexically_normal().generic_string();
        if(std::string_view(name)=="own_bridge.lib"||std::string_view(name)=="pas_core.lib"){
            w::require(same_path_text(path,build/name)&&pin_equal(pin,role(delta.at("rebuilt_static_inputs"),name))&&pin_equal(pin,role(delta.at("compiled_outputs"),name)),"fresh effective/rebuilt/output library join");
            for(const auto& old:delta.at("base_static_inputs"))if(fs::path(old.at("path").get<std::string>()).filename()==name)w::require(!same_path_text(path,old.at("path").get<std::string>()),"old core/bridge cannot become effective");
        }else{
            const auto old=base_files.find(path);w::require(old!=base_files.end()&&pin_equal(pin,old->second),"three exact preserved effective static roles");
        }
    }
    for(const auto& pin:delta.at("rebuilt_static_inputs")){const auto name=fs::path(pin.at("path").get<std::string>()).filename().string();w::require(name=="own_bridge.lib"||name=="pas_core.lib","undeclared rebuilt static role");}
}
struct Graph {std::map<std::string,std::string> objects;std::map<std::string,std::set<std::string>> archives;std::map<std::string,std::string> links;std::map<std::string,std::string> link_libraries;};
inline std::string normalize_ninja(std::string text){
    std::replace(text.begin(),text.end(),'\\','/');for(const auto* escape:{"$:","$ "}){const std::string from=escape,to(1,escape[1]);std::size_t at=0;while((at=text.find(from,at))!=std::string::npos){text.replace(at,from.size(),to);at+=to.size();}}return text;
}
inline std::vector<std::string> link_tokens(std::string_view line){
    std::vector<std::string> out;std::string token;bool quoted=false;
    for(const char c:line){if(c=='"')quoted=!quoted;else if((c==' '||c=='\t')&&!quoted){if(!token.empty()){out.push_back(std::move(token));token.clear();}}else token+=c;}
    w::require(!quoted,"balanced quotes in actual linker library arguments");if(!token.empty())out.push_back(std::move(token));return out;
}
inline Graph graph(std::string text,const fs::path& workspace,const J& delta){
    text=normalize_ninja(std::move(text));w::require(text.size()<=4*w::MiB,"bounded CPU Ninja graph");Graph out;
    const std::regex compile(R"(^build ([^\r\n:]+[.]obj): CXX_COMPILER[^ ]+ ([^\r\n]+?) \|\|)");
    const std::regex archive(R"(^build ([^ ]+[.]lib): CXX_STATIC_LIBRARY_LINKER[^ ]+ (.*)$)");
    const std::regex link(R"(^build ([^ ]+[.]exe): CXX_EXECUTABLE_LINKER[^ ]+ (.*)$)");
    const std::regex link_libraries(R"(^[ \t]+LINK_LIBRARIES = (.*)$)");
    std::istringstream lines(text);std::string line,link_target;std::map<std::string,std::string> sources;
    while(std::getline(lines,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();std::smatch match;
        if(!line.empty()&&line.front()!=' '&&line.front()!='\t')link_target.clear();
        if(std::regex_search(line,match,compile)){
            const auto object=match[1].str();const fs::path source=match[2].str();const auto relative=source.lexically_relative(workspace).generic_string();
            w::require(!relative.empty()&&!fs::path(relative).is_absolute()&&!relative.starts_with("../")&&out.objects.emplace(object,relative).second&&sources.emplace(relative,object).second,"unique workspace CPU source/object graph");
        }else if(std::regex_match(line,match,archive)){
            std::istringstream members(match[2].str());std::set<std::string> objects;std::string member;while(members>>member){if(member=="|"||member=="||")break;w::require(member.ends_with(".obj")&&objects.insert(member).second,"archive exact distinct object members");}
            w::require(out.archives.emplace(match[1].str(),objects).second,"duplicate CPU archive graph role");
        }else if(std::regex_match(line,match,link)){link_target=match[1].str();w::require(out.links.emplace(link_target,match[2].str()).second,"duplicate CPU executable graph role");}
        else if(!link_target.empty()&&std::regex_match(line,match,link_libraries))w::require(out.link_libraries.emplace(link_target,match[1].str()).second,"duplicate executable LINK_LIBRARIES field");
    }
    w::require(out.objects.size()==32&&sources.size()==32&&out.archives.size()==2&&out.links.size()==8,"exact fresh32/two archive/eight executable graph denominator");
    const std::map<std::string,std::string> executable_sources={{"research/prelive_current/chain.cpp","current_chain"},{"research/prelive_current/tests/consumer_priority_tests.cpp","consumer_priority_controls"},{"research/prelive_current/tests/worker_affinity_tests.cpp","worker_affinity_controls"},{"research/prelive_current/tests/producer_wait_tests.cpp","producer_wait_tests"},{"research/prelive_current/tests/owner_checkpoint_tests.cpp","owner_checkpoint_tests"},{"research/prelive_current/tests/owner_expiry_tests.cpp","owner_expiry_tests"},{"research/prelive_current/tests/component_mask_tests.cpp","component_mask_tests"},{"research/bvi_owner_cooperative/cold_verification/main.cpp","uniform_fnv_tests"}};
    const auto expected=units();std::set<std::string> core,bridge;for(const auto& item:expected){const auto path=item.get<std::string>();w::require(sources.contains(path),"missing exact CPU source graph member");const auto& object=sources.at(path);
        if(path.starts_with("src/")){w::require(object.starts_with("CMakeFiles/pas_core.dir/"),"formal source belongs to fresh core role");core.insert(object);}
        else if(path=="research/prelive_current/own_bridge.cpp"||path=="research/bvi_owner_cooperative/extract.cpp"){w::require(object.starts_with("CMakeFiles/own_bridge.dir/"),"cooperative source belongs to fresh bridge role");bridge.insert(object);}
        else{const auto& target=executable_sources.at(path);w::require(object.starts_with("CMakeFiles/"+target+".dir/")&&out.links.contains(target+".exe"),"exact executable source/object/target role");std::istringstream inputs(out.links.at(target+".exe"));std::set<std::string> members;std::string member;while(inputs>>member){if(member=="|"||member=="||")break;w::require(member.ends_with(".obj")&&members.insert(member).second,"distinct declared executable objects");}w::require(members==std::set<std::string>{object},"one exact fresh TU per CPU executable");}
    }
    w::require(core.size()==22&&bridge.size()==2&&out.archives.at("pas_core.lib")==core&&out.archives.at("own_bridge.lib")==bridge,"exact fresh core22/bridge2 archive object joins");
    const auto has_token=[](const std::string& line,const std::string& expected){std::istringstream tokens(line);std::string token;while(tokens>>token)if(token==expected)return true;return false;};
    const auto lower=[](std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return value;};
    const fs::path build=delta.at("build_root").get<std::string>();std::map<std::string,fs::path> effective;
    for(const auto* name:{"current_rails.lib","current_front.lib","exact_bvi_v3.lib","own_bridge.lib","pas_core.lib"})effective.emplace(name,fs::path(role(delta.at("linked_static_inputs"),name).at("path").get<std::string>()));
    for(const auto* target:{"current_chain.exe","owner_checkpoint_tests.exe","owner_expiry_tests.exe"}){
        w::require(out.links.contains(target)&&has_token(out.links.at(target),"pas_core.lib")&&has_token(out.links.at(target),"own_bridge.lib")&&out.link_libraries.contains(target),"current action/checkpoint/expiry dependencies and actual linker field");
        std::set<std::string> seen;for(const auto& token:link_tokens(out.link_libraries.at(target))){const fs::path library(token);const auto name=lower(library.filename().string());if(!effective.contains(name))continue;
            const auto resolved=library.is_absolute()?library:build/library;w::require(seen.insert(name).second&&same_path_text(resolved.lexically_normal(),effective.at(name)),"actual linker role must occur once at its exact fresh/preserved path");}
        w::require(seen.size()==5,"all five effective roles must be actual linker arguments");
    }
    const auto lowered=lower(text);
    for(const auto& old:delta.at("base_static_inputs"))if(fs::path(old.at("path").get<std::string>()).filename()=="pas_core.lib"||fs::path(old.at("path").get<std::string>()).filename()=="own_bridge.lib")w::require(lowered.find(lower(fs::path(old.at("path").get<std::string>()).generic_string()))==std::string::npos,"old core/bridge forbidden in effective link graph");
    w::require(text.find("/O2 /Ob2 /DNDEBUG")!=std::string::npos&&std::regex_search(text,std::regex(R"((?:/|-)MD(?: |\r?\n))")),"original Release/MD CPU compile flags");return out;
}
inline void objects(const J& delta,const Graph& graph){
    const fs::path build=delta.at("build_root").get<std::string>();std::set<std::string> seen;
    for(const auto& pin:delta.at("compiled_outputs")){const fs::path path=pin.at("path").get<std::string>();if(path.extension()!=".obj")continue;const auto relative=path.lexically_relative(build).generic_string();
        w::require(!relative.empty()&&!fs::path(relative).is_absolute()&&!relative.starts_with("../"),"fresh object within CPU build root");
        if(graph.objects.contains(relative))w::require(seen.insert(relative).second,"duplicate fresh32 object output");
        else w::require(relative.ends_with("/CompilerIdCXX/CMakeCXXCompilerId.obj")||relative=="CMakeFiles/ShowIncludes/main.obj","unexpected compiled object outside CPU graph");
    }w::require(seen.size()==32,"all exact fresh32 objects sealed as outputs");
}
inline J control_declarations(){return J::array({J{{"target","owner_checkpoint_tests"},{"schema","pas.owner-cooperative-checkpoint-tests.v1"},{"assertions",36}},J{{"target","owner_expiry_tests"},{"schema","pas.owner-expiry-regression.v1"},{"assertions",26}},J{{"target","component_mask_tests"},{"schema","pas.component-mask-tests.v1"},{"assertions",359}},J{{"target","uniform_fnv_tests"},{"schema","pas.uniform-rgb-fnv-controls.v1"},{"assertions",19}}});}
inline void control_report(const J& row,const J& report){
    const auto declarations=control_declarations();const J* expected=nullptr;for(const auto& item:declarations)if(item.at("target")==row.at("target"))expected=&item;
    w::require(expected&&row.at("schema")==expected->at("schema")&&row.at("assertions").is_number_integer()&&row.at("assertions")==expected->at("assertions")&&report.at("schema")==row.at("schema")&&report.at("assertions").is_number_integer()&&report.at("assertions")==row.at("assertions")&&report.at("checks").is_array()&&report.at("checks").size()==report.at("assertions")&&report.at("failed").is_number_integer()&&report.at("failed")==0&&report.at("device_endpoints").is_number_integer()&&report.at("device_endpoints")==0,"exact CPU component control schema/count/result");
    for(const auto& item:report.at("checks"))w::require(item.at("pass").is_boolean()&&item.at("pass")==true,"CPU semantic control failed");
    const bool legacy=row.at("target")=="owner_checkpoint_tests"||row.at("target")=="owner_expiry_tests";
    for(const auto* key:{"pass","device_operations","live_ready","cost_qualification"}){if(!report.contains(key)){w::require(legacy,"new CPU controls require explicit endpoint/qualification flags");continue;}if(std::string_view(key)=="pass")w::require(report.at(key).is_boolean()&&report.at(key)==true,"CPU controls PASS boolean");else if(std::string_view(key)=="device_operations")w::require(report.at(key).is_number_integer()&&report.at(key)==0,"zero CPU control device operations");else w::require(report.at(key).is_boolean()&&report.at(key)==false,"CPU controls cannot issue qualification");}
    if(row.at("target")=="component_mask_tests")w::require(report.at("dark_block_sample_count")==8&&report.at("dark_block_source_bytes")==48&&report.at("dark_block_max_byte")==165&&report.at("SSE2_fast_path")==true,"actual Windows eight-sample dark-block policy");
    if(row.at("target")=="uniform_fnv_tests")w::require(report.at("table_bytes")==524288&&report.at("frame_cache_bytes")==0&&report.at("uniform_state_comparisons")==262144,"exact bounded uniform FNV table/no frame cache controls");
}
}
