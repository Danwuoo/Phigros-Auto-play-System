#include "reader/graph_reader.hpp"
#include "reader/narrow_preflight.hpp"
#include <iostream>
int main(int argc,char** argv){try{
 namespace g=pas::preflight_graph;namespace fs=std::filesystem;using J=nlohmann::json;
 if(argc!=2||fs::exists(argv[1]))throw std::runtime_error("fresh report required");
 const fs::path output=fs::absolute(argv[1]),root=output.string()+".fixtures";
 if(fs::exists(root))throw std::runtime_error("fresh fixture root required");fs::create_directory(root);
 J cases=J::array();const auto check=[&](const char* id,bool pass){cases.push_back({{"id",id},{"pass",pass}});if(!pass)throw std::runtime_error(id);};
 const auto save=[&](const char* name,const std::string& value){auto p=root/name;std::ofstream f(p,std::ios::binary);f<<value;f.close();if(!f)throw std::runtime_error("fixture write");return p;};
 const auto rejects=[&](auto&& fn){try{fn();return false;}catch(const std::exception&){return true;}};
 auto empty=save("empty","");check("cng_empty_sha",g::streamed_sha256(empty,0)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 auto abc=save("abc","abc");const auto sha="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
 check("cng_abc_sha",g::streamed_sha256(abc,3)==sha);check("streamed_cap_reject",rejects([&]{g::streamed_sha256(abc,2);}));
 auto blocks=save("blocks",std::string(131073,'x'));
 check("multi_buffer_portable_parity",g::streamed_sha256(blocks,131073)==pas::evidence::sha256(blocks));
 g::Reader r(root);J pin={{"path",abc.string()},{"bytes",std::uint64_t(3)},{"sha256",sha}};
 r.bound(pin);r.bound(pin);check("deduplicate_identical_binding",r.count()==1&&r.bytes()==3);
 auto bad=pin;bad["sha256"]=std::string(64,'0');check("conflicting_binding_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["bytes"]=std::uint64_t(4);check("wrong_size_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["path"]=(root/".."/root.filename()/"abc").string();check("traversal_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["path"]=abc.string()+":stream";check("ads_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["path"]=abc.string()+".";check("trailing_dot_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["path"]=abc.string()+" ";check("trailing_space_reject",rejects([&]{r.bound(bad);}));
 const auto outside=save("outside","xyz");fs::create_directory(root/"nested");
 check("workspace_containment_reject",rejects([&]{g::Reader isolated(root/"nested");isolated.bound(J{{"path",outside.string()},{"bytes",std::uint64_t(3)},{"sha256",g::streamed_sha256(outside,3)}});}));
 check("mixed_slash_case_comparison",g::same_path_text(fs::path("C:/Windows/System32/cmd.exe"),fs::path("c:\\WINDOWS\\system32\\CMD.EXE")));
 auto duplicate=save("duplicate.json","{\"x\":1,\"x\":2}");check("duplicate_key_reject",rejects([&]{g::w::read(duplicate);}));
 auto array=save("array.json","[]");check("nonobject_reject",rejects([&]{g::w::read(array);}));
 std::string deep;for(int i=0;i<66;++i)deep+="{\"x\":";deep+="0";deep+=std::string(66,'}');
 auto depth=save("depth.json",deep);check("valid_excessive_depth_reject",rejects([&]{g::w::read(depth);}));
 namespace n=g::narrow;
 auto role_pin=pin;role_pin["role"]="candidate_binary";
 check("narrow_pin_role_projection",n::same_pin(pin,role_pin));
 bad=pin;bad["path"]="relative.json";check("narrow_relative_pin_reject",rejects([&]{n::pin_shape(bad);}));
 bad=pin;bad["sha256"]=std::string(64,'z');check("narrow_malformed_sha_reject",rejects([&]{n::pin_shape(bad);}));
 check("narrow_formal_cpp_not_maintenance",!n::maintenance_path("src/manual_session.cpp")&&!n::maintenance_path("research/prelive_current/cold_chain.cpp"));
 J parent={{"source_commit",std::string(40,'a')},{"source_manifest",pin},{"core_build_plan",pin},{"changed_paths",J::array()},{"candidate_bytes",3},{"files",J::array({pin})},{"compiled_outputs",J::array({pin})},{"scope","noise_and_unrun_abba_only"},{"additional_inputs",J::array({J{{"path",(root/"docs/test.md").string()},{"bytes",std::uint64_t(3)},{"sha256",sha}}})}};
 auto amendment=parent;amendment["source_commit"]=std::string(40,'b');amendment["compiled_source_commit"]=std::string(40,'a');amendment["maintenance_only_amendment"]=true;amendment["maintenance_changed_paths"]=J::array({"docs/test.md"});amendment["parent_repair_build"]=pin;amendment["compiled_source_manifest"]=pin;amendment["formal_rebuilt"]=false;amendment["component_rebuilt"]=false;amendment["amendment_proof"]=pin;
 check("narrow_preserved_parent_positive",!rejects([&]{n::same_parent(amendment,parent);}));
 auto changed=amendment;changed["files"][0]["sha256"]=std::string(64,'0');check("narrow_changed_runtime_reject",rejects([&]{n::same_parent(changed,parent);}));
 changed=amendment;changed["compiled_outputs"][0]["sha256"]=std::string(64,'0');check("narrow_changed_object_reject",rejects([&]{n::same_parent(changed,parent);}));
 changed=amendment;changed["force_qualification"]=true;check("narrow_undeclared_field_reject",rejects([&]{n::same_parent(changed,parent);}));
 changed=amendment;changed["additional_inputs"][0]["sha256"]=std::string(64,'0');check("narrow_declared_doc_rebind_positive",!rejects([&]{n::same_parent(changed,parent);}));
 changed["maintenance_changed_paths"]=J::array();check("narrow_undeclared_doc_rebind_reject",rejects([&]{n::same_parent(changed,parent);}));
 changed=amendment;changed["additional_inputs"][0]["path"]=(root/"docs/other.md").string();check("narrow_changed_additional_path_reject",rejects([&]{n::same_parent(changed,parent);}));
 changed=amendment;changed["maintenance_changed_paths"]=J::array({"src/manual_session.cpp"});check("narrow_formal_amendment_reject",rejects([&]{n::same_parent(changed,parent);}));
 check("narrow_missing_cost_bindings",!n::supplied(J{{"aa_input",nullptr}},{"aa_input","aa_status"}));
 check("narrow_supplied_cost_bindings",n::supplied(J{{"aa_input",pin},{"aa_status",pin}},{"aa_input","aa_status"}));
 check("narrow_original_noise_positive",g::cc::noise({1.,1.,1.,1.},"observer_ms").adequate);
 check("narrow_original_noise_negative",!g::cc::noise({1.,2.,3.,4.},"observer_ms").adequate);
 J legacy_header={{"path",(root/"consumer_priority.hpp").string()},{"bytes",std::uint64_t(9155)},{"sha256","22e2ae774920f223a4f358d800912d86c6fe1976c4452edd8f22d95a1c024cd8"}};
 J legacy_tests={{"path",(root/"consumer_priority_tests.cpp").string()},{"bytes",std::uint64_t(11370)},{"sha256","083e294be6fe79d249bf8f89c65c3adedab28cc66beab09b85c5c2406a06a413"}};
 auto new_header=legacy_header,new_tests=legacy_tests;new_header["sha256"]=std::string(64,'1');new_tests["sha256"]=std::string(64,'2');
 J priority={{"requested",1},{"applied_readback",1},{"previous",0},{"restored_readback",0},{"applied",true},{"restored",true},{"verified",true},{"process_class",32},{"process_class_after_close",32},{"failure","none"},{"rollback_failure","none"},{"fallback_attempted",false}};
 check("narrow_priority_exact_legacy_one",n::priority_policy(J::object(),J::object(),legacy_header,legacy_tests)==1&&!rejects([&]{n::verify_priority(priority,1);}));
 const J highest={{"consumer_priority_requested",2}};priority["requested"]=2;priority["applied_readback"]=2;
 check("narrow_priority_explicit_new_two",n::priority_policy(highest,highest,new_header,new_tests)==2&&!rejects([&]{n::verify_priority(priority,2);}));
 check("narrow_priority_unknown_missing_reject",rejects([&]{n::priority_policy(J::object(),J::object(),new_header,new_tests);}));
 check("narrow_priority_plan_report_mismatch_reject",rejects([&]{n::priority_policy(highest,J{{"consumer_priority_requested",1}},new_header,new_tests);}));
 check("narrow_priority_new_scalar_legacy_mix_reject",rejects([&]{n::priority_policy(highest,highest,legacy_header,new_tests);})&&rejects([&]{n::priority_policy(highest,highest,new_header,legacy_tests);}));
 auto wrong_requested=priority,wrong_readback=priority;wrong_requested["requested"]=1;wrong_readback["applied_readback"]=1;
 check("narrow_priority_wrong_requested_readback_reject",rejects([&]{n::verify_priority(wrong_requested,2);})&&rejects([&]{n::verify_priority(wrong_readback,2);}));
 J tail_header1={{"path",(root/"producer_wait.hpp").string()},{"bytes",std::uint64_t(4804)},{"sha256","9e9700107d4f07301ef302351fa645cb705a57b1e049f4166eb70604a116ddc2"}},tail_tests1={{"path",(root/"producer_wait_tests.cpp").string()},{"bytes",std::uint64_t(12026)},{"sha256","ee3b63b9787b21ebe27bdc3e7b32e571c9a3dd1e7532f5ac1951d8e8932ff7a6"}};
 auto tail_header2=tail_header1,tail_tests2=tail_tests1;tail_header2["bytes"]=std::uint64_t(4998);tail_header2["sha256"]="ee2f57c862d0543a5c6855cd43f4a8aeab157b96bc706acf85b6c5a004595c38";tail_tests2["bytes"]=std::uint64_t(12056);tail_tests2["sha256"]="ffb5f56676b0e0ef23e92b1a1ffe071cbbe644568c5d660d2a8479ff2ddeb058";
 auto tail_new_header=tail_header2,tail_new_tests=tail_tests2;tail_new_header["sha256"]=std::string(64,'3');tail_new_tests["sha256"]=std::string(64,'4');const J tail_plan={{"producer_active_tail_ns",2'000'000}},tail_report={{"active_tail_ns",2'000'000}},tail_old_report={{"active_tail_ns",20'000'000}},tail_run={{"producer_active_tail_ns",2'000'000}};
 check("narrow_tail_exact_legacy_pairs_and_new2ms",n::active_tail_policy(J::object(),tail_old_report,tail_header1,tail_tests1)==20'000'000&&n::active_tail_policy(J::object(),tail_old_report,tail_header2,tail_tests2)==20'000'000&&n::active_tail_policy(tail_plan,tail_report,tail_new_header,tail_new_tests)==2'000'000&&!rejects([&]{n::verify_active_tail(tail_run,2'000'000);})&&!rejects([&]{n::verify_active_tail(J::object(),20'000'000);}));
 check("narrow_tail_missing_new_policy_reject",rejects([&]{n::active_tail_policy(J::object(),tail_report,tail_new_header,tail_new_tests);})&&rejects([&]{n::verify_active_tail(J::object(),2'000'000);}));
 check("narrow_tail_new_scalar_legacy_reject",rejects([&]{n::active_tail_policy(tail_plan,tail_report,tail_header1,tail_tests1);})&&rejects([&]{n::active_tail_policy(tail_plan,tail_report,tail_header2,tail_new_tests);}));
 check("narrow_tail_mismatched_report_pair_reject",rejects([&]{n::active_tail_policy(tail_plan,tail_old_report,tail_new_header,tail_new_tests);})&&rejects([&]{n::active_tail_policy(J::object(),tail_old_report,tail_header1,tail_tests2);}));
 check("narrow_tail_noninteger_type_reject",rejects([&]{n::active_tail_policy(J{{"producer_active_tail_ns",2'000'000.0}},tail_report,tail_new_header,tail_new_tests);})&&rejects([&]{n::active_tail_policy(tail_plan,J{{"active_tail_ns",2'000'000.0}},tail_new_header,tail_new_tests);})&&rejects([&]{n::verify_active_tail(J{{"producer_active_tail_ns","2000000"}},2'000'000);}));
 check("narrow_tail_wrong_run_scalar_reject",rejects([&]{n::verify_active_tail(J{{"producer_active_tail_ns",20'000'000}},2'000'000);})&&rejects([&]{n::verify_active_tail(J{{"producer_active_tail_ns",2'000'000}},20'000'000);}));
 namespace cpu=g::component_delta;
 const fs::path cpu_workspace=root/"cpu-workspace",cpu_build=cpu_workspace/"out/new-build";
 const auto fixture_pin=[&](const fs::path& path,char digest='a'){return J{{"path",path.string()},{"bytes",std::uint64_t(3)},{"sha256",std::string(64,digest)}};};
 std::map<std::string,J> cpu_base_sources,cpu_current_sources;
 for(const auto& path:cpu::units()){const auto text=path.get<std::string>();cpu_base_sources[text]=fixture_pin(cpu_workspace/text);}
 cpu_base_sources["include/pas/game.hpp"]=fixture_pin(cpu_workspace/"include/pas/game.hpp");cpu_base_sources["CMakeLists.txt"]=fixture_pin(cpu_workspace/"CMakeLists.txt");cpu_current_sources=cpu_base_sources;cpu_current_sources.at("src/game.cpp")["sha256"]=std::string(64,'b');
 J cpu_plan={{"component_cpu_delta",true},{"formal_rebuilt",true},{"translation_units",32},{"delta_targets",cpu::targets()},{"translation_unit_sources",cpu::units()},{"rebuilt_static_targets",J::array({"own_bridge","pas_core"})},{"formal_changed_paths",J::array({"src/game.cpp"})},{"build_target_groups",cpu::build_groups()}};
 J cpu_delta={{"component_cpu_delta",true},{"formal_rebuilt",true},{"formal_changed_paths",J::array({"src/game.cpp"})},{"changed_paths",J::array({"src/game.cpp","research/windows_production_build/CMakeLists.txt","research/bvi_owner_cooperative/cold_verification/CMakeLists.txt","research/bvi_owner_cooperative/cold_verification/run.ps1"})},{"build_root",cpu_build.string()},{"base_static_inputs",J::array()},{"linked_static_inputs",J::array()},{"rebuilt_static_inputs",J::array()},{"compiled_outputs",J::array()},{"build_target_groups",cpu::build_groups()}};
 std::map<std::string,J> cpu_base_files;
 for(const auto* name:{"current_rails.lib","current_front.lib","own_bridge.lib","exact_bvi_v3.lib","pas_core.lib"}){
  auto old=fixture_pin(cpu_workspace/"out/base"/name);cpu_delta["base_static_inputs"].push_back(old);cpu_base_files[fs::path(old.at("path").get<std::string>()).generic_string()]=old;
  if(std::string_view(name)=="own_bridge.lib"||std::string_view(name)=="pas_core.lib"){auto fresh=fixture_pin(cpu_build/name,'b');cpu_delta["linked_static_inputs"].push_back(fresh);cpu_delta["rebuilt_static_inputs"].push_back(fresh);cpu_delta["compiled_outputs"].push_back(fresh);}else cpu_delta["linked_static_inputs"].push_back(old);
 }
 const std::map<std::string,std::string> cpu_executables={{"research/prelive_current/chain.cpp","current_chain"},{"research/prelive_current/tests/consumer_priority_tests.cpp","consumer_priority_controls"},{"research/prelive_current/tests/worker_affinity_tests.cpp","worker_affinity_controls"},{"research/prelive_current/tests/producer_wait_tests.cpp","producer_wait_tests"},{"research/prelive_current/tests/owner_checkpoint_tests.cpp","owner_checkpoint_tests"},{"research/prelive_current/tests/owner_expiry_tests.cpp","owner_expiry_tests"},{"research/prelive_current/tests/component_mask_tests.cpp","component_mask_tests"},{"research/bvi_owner_cooperative/cold_verification/main.cpp","uniform_fnv_tests"}};
 std::string cpu_ninja="FLAGS = /O2 /Ob2 /DNDEBUG -MD\n",core_members,bridge_members;std::map<std::string,std::string> cpu_objects;
 for(const auto& item:cpu::units()){
  const auto source=item.get<std::string>();const std::string target=source.starts_with("src/")?"pas_core":(source=="research/prelive_current/own_bridge.cpp"||source=="research/bvi_owner_cooperative/extract.cpp")?"own_bridge":cpu_executables.at(source);
  const auto object="CMakeFiles/"+target+".dir/"+source+".obj";cpu_objects[source]=object;cpu_ninja+="build "+object+": CXX_COMPILER__"+target+"_Release "+(cpu_workspace/source).generic_string()+" || cmake_object_order\n";
  cpu_delta["compiled_outputs"].push_back(fixture_pin(cpu_build/object,'b'));if(target=="pas_core")core_members+=object+" ";else if(target=="own_bridge")bridge_members+=object+" ";
 }
 cpu_ninja+="build pas_core.lib: CXX_STATIC_LIBRARY_LINKER__pas_core_Release "+core_members+"|| cmake_order\n";
 cpu_ninja+="build own_bridge.lib: CXX_STATIC_LIBRARY_LINKER__own_bridge_Release "+bridge_members+"|| cmake_order\n";
 std::string cpu_link_arguments="own_bridge.lib pas_core.lib";for(const auto* name:{"current_rails.lib","current_front.lib","exact_bvi_v3.lib"})cpu_link_arguments+=" \""+fs::path(cpu::role(cpu_delta["linked_static_inputs"],name).at("path").get<std::string>()).generic_string()+"\"";cpu_link_arguments+=" bcrypt.lib";
 for(const auto& [source,target]:cpu_executables)cpu_ninja+="build "+target+".exe: CXX_EXECUTABLE_LINKER__"+target+"_Release "+cpu_objects.at(source)+" | pas_core.lib own_bridge.lib\n  LINK_LIBRARIES = "+cpu_link_arguments+"\n";
 const auto cpu_graph=cpu::graph(cpu_ninja,cpu_workspace,cpu_delta);
 check("cpu_delta_exact_source_roles32_archives22_2_positive",!rejects([&]{cpu::declaration(cpu_delta,cpu_plan,cpu_current_sources,cpu_base_sources);cpu::libraries(cpu_delta,cpu_plan,cpu_base_files);cpu::objects(cpu_delta,cpu_graph);}));
 auto cpu_bad=cpu_delta,cpu_bad_plan=cpu_plan;cpu_bad_plan["component_cpu_delta"]=false;
 check("cpu_delta_missing_or_false_branch_reject",rejects([&]{cpu::declaration(cpu_bad,cpu_bad_plan,cpu_current_sources,cpu_base_sources);}));
 cpu_bad=cpu_delta;cpu_bad["formal_rebuilt"]=false;check("cpu_delta_false_formal_rebuild_claim_reject",rejects([&]{cpu::declaration(cpu_bad,cpu_plan,cpu_current_sources,cpu_base_sources);}));
 auto bad_sources=cpu_current_sources;bad_sources.at("include/pas/game.hpp")["sha256"]=std::string(64,'c');check("cpu_delta_undeclared_actual_formal_header_reject",rejects([&]{cpu::declaration(cpu_delta,cpu_plan,bad_sources,cpu_base_sources);}));
 cpu_bad=cpu_delta;cpu_bad_plan=cpu_plan;cpu_bad["changed_paths"].push_back("src/core.cpp");cpu_bad["formal_changed_paths"].push_back("src/core.cpp");cpu_bad_plan["formal_changed_paths"]=cpu_bad["formal_changed_paths"];bad_sources=cpu_current_sources;bad_sources.at("src/core.cpp")["sha256"]=std::string(64,'c');bool cpu_scope_reject=rejects([&]{cpu::declaration(cpu_bad,cpu_bad_plan,bad_sources,cpu_base_sources);});cpu_bad=cpu_delta;cpu_bad["changed_paths"].push_back("research/unrelated/other.cpp");check("cpu_delta_no_generic_formal_scope_relaxation",cpu_scope_reject&&rejects([&]{cpu::declaration(cpu_bad,cpu_plan,cpu_current_sources,cpu_base_sources);}));
 bad_sources=cpu_current_sources;bad_sources.erase("src/core.cpp");check("cpu_delta_missing_compiled_TU_source_reject",rejects([&]{cpu::declaration(cpu_delta,cpu_plan,bad_sources,cpu_base_sources);}));
 cpu_bad_plan=cpu_plan;cpu_bad_plan["translation_units"]=31;const bool cpu_wrong_count_reject=rejects([&]{cpu::declaration(cpu_delta,cpu_bad_plan,cpu_current_sources,cpu_base_sources);});cpu_bad_plan["translation_units"]=32.0;check("cpu_delta_wrong_fresh32_denominator_reject",cpu_wrong_count_reject&&rejects([&]{cpu::declaration(cpu_delta,cpu_bad_plan,cpu_current_sources,cpu_base_sources);}));
 cpu_bad_plan=cpu_plan;cpu_bad_plan["translation_unit_sources"][31]=cpu_bad_plan["translation_unit_sources"][30];check("cpu_delta_duplicate_declared_TU_reject",rejects([&]{cpu::declaration(cpu_delta,cpu_bad_plan,cpu_current_sources,cpu_base_sources);}));
 cpu_bad=cpu_delta;cpu_bad["linked_static_inputs"].erase(0);check("cpu_delta_missing_effective_static_role_reject",rejects([&]{cpu::libraries(cpu_bad,cpu_plan,cpu_base_files);}));
 cpu_bad=cpu_delta;cpu_bad["linked_static_inputs"][1]=cpu_bad["linked_static_inputs"][0];check("cpu_delta_duplicate_effective_static_role_reject",rejects([&]{cpu::libraries(cpu_bad,cpu_plan,cpu_base_files);}));
 cpu_bad=cpu_delta;cpu_bad["linked_static_inputs"][4]=cpu::role(cpu_delta["base_static_inputs"],"pas_core.lib");check("cpu_delta_old_pas_core_effective_reject",rejects([&]{cpu::libraries(cpu_bad,cpu_plan,cpu_base_files);}));
 cpu_bad=cpu_delta;cpu_bad["linked_static_inputs"][2]=cpu::role(cpu_delta["base_static_inputs"],"own_bridge.lib");check("cpu_delta_old_own_bridge_effective_reject",rejects([&]{cpu::libraries(cpu_bad,cpu_plan,cpu_base_files);}));
 cpu_bad=cpu_delta;cpu_bad["compiled_outputs"][1]["sha256"]=std::string(64,'c');check("cpu_delta_fresh_library_output_SHA_conflict_reject",rejects([&]{cpu::libraries(cpu_bad,cpu_plan,cpu_base_files);}));
 auto bad_ninja=cpu_ninja;const auto first_line=bad_ninja.find("build "),first_end=bad_ninja.find('\n',first_line);bad_ninja.erase(first_line,first_end-first_line+1);check("cpu_delta_missing_compile_graph_object_reject",rejects([&]{cpu::graph(bad_ninja,cpu_workspace,cpu_delta);}));
 bad_ninja=cpu_ninja+"build CMakeFiles/pas_core.dir/duplicate.obj: CXX_COMPILER__pas_core_Release "+(cpu_workspace/"src/core.cpp").generic_string()+" || order\n";check("cpu_delta_duplicate_graph_source_reject",rejects([&]{cpu::graph(bad_ninja,cpu_workspace,cpu_delta);}));
 bad_ninja=cpu_ninja;auto at=bad_ninja.find("|| cmake_order",bad_ninja.find("build pas_core.lib:"));bad_ninja.insert(at,cpu_objects.at("research/prelive_current/own_bridge.cpp")+" ");check("cpu_delta_wrong_archive_member_role_reject",rejects([&]{cpu::graph(bad_ninja,cpu_workspace,cpu_delta);}));
 bad_ninja=cpu_ninja+"LINK_LIBRARIES = "+fs::path(cpu::role(cpu_delta["base_static_inputs"],"pas_core.lib").at("path").get<std::string>()).generic_string()+"\n";bool cpu_shadow_link_reject=rejects([&]{cpu::graph(bad_ninja,cpu_workspace,cpu_delta);});
 for(const auto* target:{"current_chain.exe","owner_checkpoint_tests.exe","owner_expiry_tests.exe"}){bad_ninja=cpu_ninja;at=bad_ninja.find('\n',bad_ninja.find("  LINK_LIBRARIES = ",bad_ninja.find("build "+std::string(target)+":")));bad_ninja.insert(at," \""+(cpu_workspace/"out/shadow-old-copy/pas_core.lib").generic_string()+"\"");cpu_shadow_link_reject=rejects([&]{cpu::graph(bad_ninja,cpu_workspace,cpu_delta);})&&cpu_shadow_link_reject;}
 check("cpu_delta_old_core_in_link_variables_reject",cpu_shadow_link_reject);
 bad_ninja=cpu_ninja;at=bad_ninja.find(cpu_objects.at("research/prelive_current/chain.cpp"),bad_ninja.find("build current_chain.exe:"));bad_ninja.replace(at,cpu_objects.at("research/prelive_current/chain.cpp").size(),cpu_objects.at("research/prelive_current/own_bridge.cpp"));bool cpu_actual_link_reject=rejects([&]{cpu::graph(bad_ninja,cpu_workspace,cpu_delta);});
 for(const auto* target:{"current_chain.exe","owner_checkpoint_tests.exe","owner_expiry_tests.exe"}){bad_ninja=cpu_ninja;at=bad_ninja.find("own_bridge.lib",bad_ninja.find("  LINK_LIBRARIES = ",bad_ninja.find("build "+std::string(target)+":")));bad_ninja.erase(at,std::string("own_bridge.lib ").size());cpu_actual_link_reject=rejects([&]{cpu::graph(bad_ninja,cpu_workspace,cpu_delta);})&&cpu_actual_link_reject;}
 check("cpu_delta_wrong_executable_object_join_reject",cpu_actual_link_reject);
 cpu_bad=cpu_delta;cpu_bad["compiled_outputs"].erase(2);check("cpu_delta_missing_sealed_fresh_object_reject",rejects([&]{cpu::objects(cpu_bad,cpu_graph);}));
 cpu_bad=cpu_delta;cpu_bad["compiled_outputs"].push_back(fixture_pin(cpu_build/"stale.obj"));check("cpu_delta_extra_foreign_object_reject",rejects([&]{cpu::objects(cpu_bad,cpu_graph);}));
 J component_reports=J::array();for(const auto& row:cpu::control_declarations()){
  J report={{"schema",row.at("schema")},{"assertions",row.at("assertions")},{"checks",J::array()},{"failed",0},{"device_endpoints",0},{"device_operations",0},{"pass",true},{"live_ready",false},{"cost_qualification",false}};
  for(int i=0;i<row.at("assertions").get<int>();++i)report["checks"].push_back({{"id",std::to_string(i)},{"pass",true}});
  if(row.at("target")=="component_mask_tests"){report["dark_block_sample_count"]=8;report["dark_block_source_bytes"]=48;report["dark_block_max_byte"]=165;report["SSE2_fast_path"]=true;}
  if(row.at("target")=="uniform_fnv_tests"){report["table_bytes"]=524288;report["frame_cache_bytes"]=0;report["uniform_state_comparisons"]=262144;}component_reports.push_back(report);
 }
 check("cpu_delta_four_exact_semantic_control_schemas_positive",!rejects([&]{const auto declarations=cpu::control_declarations();for(std::size_t i=0;i<4;++i)cpu::control_report(declarations[i],component_reports[i]);}));
 auto malformed_control=component_reports[2];malformed_control["assertions"]=358;check("cpu_delta_wrong_semantic_control_count_reject",rejects([&]{cpu::control_report(cpu::control_declarations()[2],malformed_control);}));
 malformed_control=component_reports[2];malformed_control["checks"][0]["pass"]=false;check("cpu_delta_semantic_failure_cannot_hide_in_failed_zero",rejects([&]{cpu::control_report(cpu::control_declarations()[2],malformed_control);}));
 malformed_control=component_reports[3];malformed_control["frame_cache_bytes"]=1;check("cpu_delta_uniform_frame_cache_or_wrong_bound_reject",rejects([&]{cpu::control_report(cpu::control_declarations()[3],malformed_control);}));
 malformed_control=component_reports[2];malformed_control.erase("device_operations");check("cpu_delta_new_control_missing_device_flag_reject",rejects([&]{cpu::control_report(cpu::control_declarations()[2],malformed_control);}));
 const auto cpu_process=[&](const std::string& stage){J process=J::object();for(const auto* field:{"specification","input_closure","result","verification","freeze","state","contract","command"})process[field]=fixture_pin(cpu_workspace/"out/proof"/stage/(std::string(field)+".json"));return process;};
 cpu_delta["configure_process"]=cpu_process("configure");cpu_delta["configure_receipt"]=cpu_delta["configure_process"]["verification"];cpu_delta["configure_input_closure"]=cpu_delta["configure_process"]["input_closure"];
 for(const auto* field:{"build_receipts","build_input_closures","build_processes"})cpu_delta[field]=J::array();
 const auto cpu_command=[&](const J& group,const fs::path& build){std::string line="\""+(cpu_workspace/"cmake.exe").string()+"\" \"--build\" \""+build.string()+"\" \"--parallel\" \"4\" \"--target\"";for(const auto& target:group.at("targets"))line+=" \""+target.get<std::string>()+"\"";return line+"\r\n";};
 for(std::size_t i=0;i<3;++i){auto process=cpu_process("build"+std::to_string(i));cpu_delta["build_processes"].push_back(process);cpu_delta["build_receipts"].push_back(process["verification"]);cpu_delta["build_input_closures"].push_back(process["input_closure"]);}
 const fs::path cpu_command_path=cpu_workspace/"out/build-inputs/build.cmd";const J cpu_build_spec={{"exe",(cpu_workspace/"cmd.exe").string()},{"argv",J::array({"/d","/s","/c",cpu_command_path.string()})}};
 check("cpu_three_build_groups_exact_native8_positive",!rejects([&]{cpu::build_declaration(cpu_delta,cpu_plan);for(const auto& group:cpu::build_groups())cpu::build_command(cpu_build_spec,cpu_command(group,cpu_build),cpu_build,group.at("targets"),cpu_command_path);}));
 cpu_bad=cpu_delta;cpu_bad.erase("build_target_groups");check("cpu_three_build_groups_missing_or_collapsed_reject",rejects([&]{cpu::build_declaration(cpu_bad,cpu_plan);})&&rejects([&]{auto p=cpu_plan;p["build_target_groups"]=J::array({J{{"targets",cpu::targets()}}});cpu::build_declaration(cpu_delta,p);}));
 cpu_bad=cpu_delta;std::swap(cpu_bad["build_target_groups"][0],cpu_bad["build_target_groups"][1]);check("cpu_three_build_groups_order_reject",rejects([&]{cpu::build_declaration(cpu_bad,cpu_plan);}));
 cpu_bad=cpu_delta;cpu_bad["build_target_groups"][1]["targets"].push_back("pas_core");check("cpu_three_build_groups_extra_cross_group_target_reject",rejects([&]{cpu::build_declaration(cpu_bad,cpu_plan);}));
 cpu_bad=cpu_delta;cpu_bad["build_receipts"].erase(2);check("cpu_three_build_groups_missing_receipt_reject",rejects([&]{cpu::build_declaration(cpu_bad,cpu_plan);}));
 cpu_bad=cpu_delta;cpu_bad["build_receipts"][1]=cpu_bad["build_receipts"][0];cpu_bad["build_processes"][1]["verification"]=cpu_bad["build_receipts"][0];check("cpu_three_build_groups_duplicate_native_reject",rejects([&]{cpu::build_declaration(cpu_bad,cpu_plan);}));
 cpu_bad=cpu_delta;cpu_bad["build_processes"][0]["verification"]=cpu_bad["build_receipts"][1];check("cpu_three_build_groups_wrong_native_projection_reject",rejects([&]{cpu::build_declaration(cpu_bad,cpu_plan);}));
 cpu_bad=cpu_delta;cpu_bad["build_processes"][1]["input_closure"]=cpu_bad["build_input_closures"][0];check("cpu_three_build_groups_wrong_closure_or_native8_reject",rejects([&]{cpu::build_declaration(cpu_bad,cpu_plan);})&&rejects([&]{auto d=cpu_delta;d["configure_process"].erase("freeze");cpu::build_declaration(d,cpu_plan);}));
 check("cpu_three_build_groups_actual_wrong_targets_reject",rejects([&]{cpu::build_command(cpu_build_spec,cpu_command(cpu::build_groups()[1],cpu_build),cpu_build,cpu::build_groups()[0].at("targets"),cpu_command_path);}));
 auto malformed_command=cpu_command(cpu::build_groups()[0],cpu_build);malformed_command.replace(malformed_command.find("\"4\""),3,"\"8\"");
 check("cpu_three_build_groups_wrong_root_parallel_or_extra_launch_reject",rejects([&]{cpu::build_command(cpu_build_spec,malformed_command,cpu_build,cpu::build_groups()[0].at("targets"),cpu_command_path);})&&rejects([&]{cpu::build_command(cpu_build_spec,cpu_command(cpu::build_groups()[0],cpu_build/"old"),cpu_build,cpu::build_groups()[0].at("targets"),cpu_command_path);})&&rejects([&]{auto line=cpu_command(cpu::build_groups()[0],cpu_build);cpu::build_command(cpu_build_spec,line+line,cpu_build,cpu::build_groups()[0].at("targets"),cpu_command_path);}));
 g::Reader missing_reader(root);n::Preflight missing(missing_reader,root,J{{"schema","pas.readonly-narrow-preflight-input.v1"},{"device_operations",0}});const auto missing_report=missing.run();
 check("narrow_missing_receipts_structured_no_admission",missing_report.at("status")=="REJECTED"&&!missing_report.at("pass").get<bool>()&&!missing_report.at("qualification_issued").get<bool>()&&!missing_report.at("live_ready").get<bool>()&&missing_report.at("device_operations")==0&&missing_report.at("issues")[0].at("reason")=="missing input binding: gate"&&!missing_report.at("raw_revalidation_performed").get<bool>());
 r.finish();save("abc","abd");check("post_read_mutation_reject",rejects([&]{r.finish();}));
 J active_cases=J::array();const auto active_check=[&](const char* id,bool pass){active_cases.push_back({{"id",id},{"pass",pass}});if(!pass)throw std::runtime_error(id);};
 J active_control={{"schema","pas.producer-active-qpc-controls.v1"},{"assertions",12},{"checks",J::array()},{"failed",0},{"pass",true},{"synthetic_api_controls",true},{"device_operations",0},{"cost_qualification",false},{"live_ready",false},{"policy","full_period_active_qpc"},{"horizon_ns",100'000'000},{"poll_cap",16'777'216},{"no_kernel_wait",true}};
 for(int i=0;i<12;++i)active_control["checks"].push_back({{"id",std::to_string(i)},{"pass",true}});
 J wait_diagnostic_control={{"schema","pas.producer-wait-diagnostic-controls.v1"},{"assertions",20},{"checks",J::array()},{"failed",0},{"pass",true},{"synthetic_api_controls",true},{"kernel_sample_cap",4},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
 for(int i=0;i<20;++i)wait_diagnostic_control["checks"].push_back({{"id",std::to_string(i)},{"pass",true}});
 const J active_plan={{"producer_wait_policy","full_period_active_qpc"},{"producer_active_tail_ns",0},{"producer_active_wait_horizon_ns",100'000'000},{"producer_active_wait_poll_cap",16'777'216},{"producer_wait_controls_count",43},{"producer_active_wait_controls_count",12}};
 const J active_report={{"active_tail_ns",2'000'000},{"active_wait_controls",active_control}};
 const J active_run={{"producer_wait_policy","full_period_active_qpc"},{"producer_active_tail_ns",0},{"producer_active_wait_horizon_ns",100'000'000},{"producer_active_wait_poll_cap",16'777'216},{"producer_no_kernel_wait",true}};
 const auto active_policy_rejects=[&](const J& plan,const J& report){return rejects([&]{n::active_tail_policy(plan,report,tail_new_header,tail_new_tests);});};
 const auto active_run_rejects=[&](const J& report){return rejects([&]{n::verify_active_tail(report,0);});};
 active_check("full_active_exact_policy_and_zero_tail_run_positive",n::active_tail_policy(active_plan,active_report,tail_new_header,tail_new_tests)==0&&!active_run_rejects(active_run)&&!rejects([&]{n::verify_wait_diagnostic_controls(wait_diagnostic_control);}));
 auto active_bad=active_plan;active_bad.erase("producer_wait_policy");active_check("full_active_missing_plan_policy_reject",active_policy_rejects(active_bad,active_report));
 active_bad=active_plan;active_bad["producer_wait_policy"]="unknown";bool active_reject=active_policy_rejects(active_bad,active_report);active_bad["producer_wait_policy"]=true;
 active_check("full_active_unknown_or_typed_policy_reject",active_reject&&active_policy_rejects(active_bad,active_report));
 active_bad=active_plan;active_bad.erase("producer_active_tail_ns");active_reject=active_policy_rejects(active_bad,active_report);active_bad=active_plan;active_bad["producer_active_tail_ns"]=0.0;active_reject=active_reject&&active_policy_rejects(active_bad,active_report);active_bad["producer_active_tail_ns"]=2'000'000;
 active_check("full_active_missing_type_or_nonzero_plan_tail_reject",active_reject&&active_policy_rejects(active_bad,active_report));
 active_reject=true;for(const auto* field:{"producer_active_wait_horizon_ns","producer_active_wait_poll_cap","producer_wait_controls_count","producer_active_wait_controls_count"}){active_bad=active_plan;active_bad.erase(field);active_reject=active_reject&&active_policy_rejects(active_bad,active_report);active_bad=active_plan;active_bad[field]=99;active_reject=active_reject&&active_policy_rejects(active_bad,active_report);active_bad=active_plan;active_bad[field]=active_plan.at(field).get<double>();active_reject=active_reject&&active_policy_rejects(active_bad,active_report);}
 active_check("full_active_exact_plan_bounds_missing_type_value_reject",active_reject);
 active_bad=active_report;active_bad.erase("active_wait_controls");active_reject=active_policy_rejects(active_plan,active_bad);active_bad=active_report;active_bad["active_wait_controls"]["schema"]="unknown";active_reject=active_reject&&active_policy_rejects(active_plan,active_bad);
 for(const auto* field:{"pass","synthetic_api_controls","no_kernel_wait","cost_qualification","live_ready","device_operations","policy","horizon_ns","poll_cap"}){active_bad=active_report;active_bad["active_wait_controls"].erase(field);active_reject=active_reject&&active_policy_rejects(active_plan,active_bad);active_bad=active_report;active_bad["active_wait_controls"][field]="malformed";active_reject=active_reject&&active_policy_rejects(active_plan,active_bad);}
 active_check("full_active_nested_schema_missing_or_typed_flags_reject",active_reject);
 active_bad=active_report;active_bad["active_wait_controls"]["assertions"]=13;active_reject=active_policy_rejects(active_plan,active_bad);active_bad["active_wait_controls"]["assertions"]=12.0;active_reject=active_reject&&active_policy_rejects(active_plan,active_bad);active_bad=active_report;active_bad["active_wait_controls"]["checks"].erase(11);active_reject=active_reject&&active_policy_rejects(active_plan,active_bad);active_bad=active_report;active_bad["active_wait_controls"]["checks"][0]["pass"]=false;active_reject=active_reject&&active_policy_rejects(active_plan,active_bad);active_bad=active_report;active_bad["active_wait_controls"]["checks"][1]["id"]="0";
 active_reject=active_reject&&active_policy_rejects(active_plan,active_bad);active_bad=wait_diagnostic_control;active_bad["checks"][0]["pass"]=false;active_reject=active_reject&&rejects([&]{n::verify_wait_diagnostic_controls(active_bad);});active_bad=wait_diagnostic_control;active_bad["assertions"]=19;active_reject=active_reject&&rejects([&]{n::verify_wait_diagnostic_controls(active_bad);});active_bad=wait_diagnostic_control;active_bad.erase("synthetic_api_controls");
 active_check("full_active_nested_denominator_hidden_failure_duplicate_reject",active_reject&&rejects([&]{n::verify_wait_diagnostic_controls(active_bad);}));
 active_reject=rejects([&]{n::active_tail_policy(active_plan,active_report,tail_header1,tail_new_tests);})&&rejects([&]{n::active_tail_policy(active_plan,active_report,tail_new_header,tail_tests2);});
 active_check("full_active_legacy_compiled_pair_or_partial_mix_reject",active_reject);
 active_reject=true;for(const auto* field:{"producer_wait_policy","producer_active_tail_ns","producer_active_wait_horizon_ns","producer_active_wait_poll_cap","producer_no_kernel_wait"}){active_bad=active_run;active_bad.erase(field);active_reject=active_reject&&active_run_rejects(active_bad);}
 active_check("full_active_zero_does_not_skip_missing_run_fields",active_reject);
 active_bad=active_run;active_bad["producer_wait_policy"]="unknown";active_reject=active_run_rejects(active_bad);active_bad=active_run;active_bad["producer_active_tail_ns"]=2'000'000;active_reject=active_reject&&active_run_rejects(active_bad);active_bad=active_run;active_bad["producer_no_kernel_wait"]=false;active_reject=active_reject&&active_run_rejects(active_bad);for(const auto* field:{"producer_active_wait_horizon_ns","producer_active_wait_poll_cap"}){active_bad=active_run;active_bad[field]=1;active_reject=active_reject&&active_run_rejects(active_bad);}
 active_check("full_active_runtime_policy_bounds_or_kernel_mismatch_reject",active_reject);
 active_reject=true;for(const auto* field:{"producer_wait_policy","producer_active_tail_ns","producer_active_wait_horizon_ns","producer_active_wait_poll_cap","producer_no_kernel_wait"}){active_bad=active_run;active_bad[field]=active_run.at(field).is_number()?J(active_run.at(field).get<double>()):J(1);active_reject=active_reject&&active_run_rejects(active_bad);}
 active_check("full_active_runtime_scalar_types_reject",active_reject);
 active_check("full_active_and_legacy_policies_cannot_cross",active_policy_rejects(tail_plan,active_report)&&rejects([&]{n::verify_active_tail(active_run,2'000'000);})&&rejects([&]{n::verify_active_tail(active_run,20'000'000);})&&active_run_rejects(tail_run));
 if(cases.size()!=81||active_cases.size()!=12)throw std::runtime_error("separate original81 and active policy12 control denominators");
 const J active_policy_controls={{"schema","pas.active-wait-policy-reader-controls.v1"},{"assertions",active_cases.size()},{"checks",active_cases},{"failed",0},{"pass",true},{"synthetic_fixture_only",true},{"qualification_issued",false},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
 J report={{"schema","pas.qualification-reader-controls.v1"},{"failed",0},{"cases",cases},{"cases_count",cases.size()},{"qualification_issued",false},{"live_ready",false},{"device_operations",0}};
 report["active_wait_policy_controls"]=active_policy_controls;
 J qos_cases=J::array();const auto qos_check=[&](const char* id,bool pass){qos_cases.push_back({{"id",id},{"pass",pass}});if(!pass)throw std::runtime_error(id);};
 J qos_snapshot={{"schema","pas.cold-process-qos.v1"},{"policy",n::process_qos_policy_name},{"version",1},{"execution_speed_mask",1},{"supported_mask",5},
  {"process_id",101},{"owner_thread_id",201},{"close_process_id",101},{"close_thread_id",201},{"process_class_before",32},{"process_class_after_close",32},
  {"failure","none"},{"rollback_failure","none"},{"fallback_failure","none"},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
 const J qos_previous={{"version",1},{"control_mask",5},{"state_mask",5}},qos_requested={{"version",1},{"control_mask",5},{"state_mask",4}};
 qos_snapshot["previous"]=qos_previous;qos_snapshot["requested"]=qos_requested;qos_snapshot["applied_readback"]=qos_requested;qos_snapshot["restored_readback"]=qos_previous;
 for(const auto* field:{"rollback_readback","fallback_readback"})qos_snapshot[field]={{"version",0},{"control_mask",0},{"state_mask",0}};
 for(const auto* field:{"verified","constructor_attempted","previous_available","requested_available","set_attempted","set_succeeded","applied_available","applied","close_attempted","restore_set_attempted","restore_set_succeeded","restored_available","restored"})qos_snapshot[field]=true;
 for(const auto* field:{"rollback_attempted","rollback_available","rollback_restored","fallback_attempted","fallback_available","fallback_restored"})qos_snapshot[field]=false;
 for(const auto* field:{"query_previous","set_requested","query_applied","set_restore","query_restored"})qos_snapshot[field]={{"sampled",true},{"status",1},{"error",0}};
 for(const auto* field:{"set_rollback","query_rollback","set_fallback","query_fallback"})qos_snapshot[field]={{"sampled",false},{"status",0},{"error",0}};
 J qos_control={{"schema","pas.cold-process-qos-controls.v1"},{"assertions",24},{"checks",J::array()},{"failed",0},{"pass",true},{"synthetic_api_controls",true},{"actual_windows_restore_control",true},
  {"policy",n::process_qos_policy_name},{"version",1},{"execution_speed_mask",1},{"supported_mask",5},{"device_operations",0},{"cost_qualification",false},{"live_ready",false},{"actual_windows_snapshot",qos_snapshot}};
 for(int i=0;i<24;++i)qos_control["checks"].push_back({{"id",std::to_string(i)},{"pass",true}});
 auto qos_plan=active_plan;qos_plan["process_qos_policy"]=n::process_qos_policy_name;qos_plan["process_qos_controls_count"]=24;
 auto qos_report=active_report;qos_report["process_qos_controls"]=qos_control;
 const J qos_run={{"process_qos",qos_snapshot},{"consumer_priority",{{"thread_id",201}}}};
 auto qos_old_header=tail_new_header;qos_old_header["bytes"]=std::uint64_t(13299);qos_old_header["sha256"]="925cbad472141db860990f349503a93a7cdb3dc042e39201988d02f36e04f1a2";
 auto qos_old_tests=tail_new_tests;qos_old_tests["bytes"]=std::uint64_t(32590);qos_old_tests["sha256"]="1be55d92023609a1feb84c20aaea0ae6e0adbeebde80923839add0bac15c118f";
 auto qos_two_ms_header1=tail_new_header;qos_two_ms_header1["bytes"]=std::uint64_t(4994);qos_two_ms_header1["sha256"]="60a22dadc78f71d895e2488c55ed32222af5cbd294a98c60f8b1fd0dc509dee1";
 auto qos_two_ms_tests1=tail_new_tests;qos_two_ms_tests1["bytes"]=std::uint64_t(12586);qos_two_ms_tests1["sha256"]="5054887016f4ebdc8a43681c14faabe99fb0740045df9c48b4ea49214ff723cb";
 auto qos_two_ms_header2=tail_new_header;qos_two_ms_header2["bytes"]=std::uint64_t(10760);qos_two_ms_header2["sha256"]="8fc675f9d1e95a4809379e643a7dc4487e01e30a92c52e2a080e578c5e7ef0f9";
 auto qos_two_ms_tests2=tail_new_tests;qos_two_ms_tests2["bytes"]=std::uint64_t(23479);qos_two_ms_tests2["sha256"]="9efc0314f7415bed15cb70f6de2c1b3fe3242aa9fc3f83acb66ff45b0bca9adb";
 const auto qos_policy_rejects=[&](const J& plan,const J& value){return rejects([&]{n::process_qos_policy(plan,value,tail_new_header,tail_new_tests,0,101);});};
 const auto qos_run_rejects=[&](const J& value){return rejects([&]{n::verify_process_qos(value,true,101);});};
 bool qos_ok=n::process_qos_policy(qos_plan,qos_report,tail_new_header,tail_new_tests,0,101)&&!qos_run_rejects(qos_run);
 for(unsigned mask:{0u,1u,4u,5u}){auto s=qos_snapshot;s["previous"]={{"version",1},{"control_mask",mask},{"state_mask",mask}};s["requested"]={{"version",1},{"control_mask",mask|1u},{"state_mask",mask&~1u}};s["applied_readback"]=s["requested"];s["restored_readback"]=s["previous"];qos_ok=qos_ok&&!rejects([&]{n::verify_process_qos_snapshot(s,101);});}
 qos_check("qos_current_exact_policy_and_timer_preservation_positive",qos_ok);
 qos_check("qos_exact_main54_and_identified_tail_legacy_positive",!n::process_qos_policy(active_plan,active_report,qos_old_header,qos_old_tests,0,101)&&!n::process_qos_policy(tail_plan,tail_report,qos_two_ms_header1,qos_two_ms_tests1,2'000'000,101)&&!n::process_qos_policy(tail_plan,tail_report,qos_two_ms_header2,qos_two_ms_tests2,2'000'000,101)&&!n::process_qos_policy(J::object(),tail_old_report,tail_header1,tail_tests1,20'000'000,101)&&!rejects([&]{n::verify_process_qos(J::object(),false,101);}));
 auto qos_bad=qos_plan;qos_bad.erase("process_qos_policy");qos_ok=qos_policy_rejects(qos_bad,qos_report)&&qos_policy_rejects(active_plan,active_report);qos_bad=active_plan;qos_bad["process_qos_controls_count"]=24;
 qos_check("qos_missing_new_or_unidentified_policy_reject",qos_ok&&qos_policy_rejects(qos_bad,active_report));
 qos_bad=qos_plan;qos_bad["process_qos_policy"]="unknown";qos_ok=qos_policy_rejects(qos_bad,qos_report);qos_bad["process_qos_policy"]=true;qos_ok=qos_ok&&qos_policy_rejects(qos_bad,qos_report);
 for(const J count:{J(23),J(24.0),J("24")}){qos_bad=qos_plan;qos_bad["process_qos_controls_count"]=count;qos_ok=qos_ok&&qos_policy_rejects(qos_bad,qos_report);}qos_bad=qos_plan;qos_bad.erase("process_qos_controls_count");
 qos_check("qos_unknown_plan_policy_count_or_type_reject",qos_ok&&qos_policy_rejects(qos_bad,qos_report));
 qos_ok=rejects([&]{n::process_qos_policy(qos_plan,qos_report,qos_old_header,qos_old_tests,0,101);})&&rejects([&]{n::process_qos_policy(qos_plan,qos_report,qos_old_header,tail_new_tests,0,101);})&&rejects([&]{n::process_qos_policy(qos_plan,qos_report,tail_new_header,qos_old_tests,0,101);})&&rejects([&]{n::process_qos_policy(qos_plan,qos_report,tail_header1,tail_tests1,0,101);});qos_bad=qos_report;qos_bad.erase("process_qos_controls");
 qos_check("qos_legacy_source_pair_partial_mix_or_controls_missing_reject",qos_ok&&qos_policy_rejects(qos_plan,qos_bad));
 qos_ok=true;for(const auto* field:{"schema","policy","pass","synthetic_api_controls","actual_windows_restore_control","version","execution_speed_mask","supported_mask","device_operations","cost_qualification","live_ready","actual_windows_snapshot"}){qos_bad=qos_report;qos_bad["process_qos_controls"].erase(field);qos_ok=qos_ok&&qos_policy_rejects(qos_plan,qos_bad);qos_bad=qos_report;qos_bad["process_qos_controls"][field]="malformed";qos_ok=qos_ok&&qos_policy_rejects(qos_plan,qos_bad);}
 qos_check("qos_controls_schema_missing_or_typed_flags_reject",qos_ok);
 qos_bad=qos_report;qos_bad["process_qos_controls"]["assertions"]=24.0;qos_ok=qos_policy_rejects(qos_plan,qos_bad);qos_bad=qos_report;qos_bad["process_qos_controls"]["checks"].erase(23);qos_ok=qos_ok&&qos_policy_rejects(qos_plan,qos_bad);qos_bad=qos_report;qos_bad["process_qos_controls"]["checks"][1]["id"]="0";qos_ok=qos_ok&&qos_policy_rejects(qos_plan,qos_bad);qos_bad=qos_report;qos_bad["process_qos_controls"]["checks"][0]["pass"]=false;
 qos_check("qos_controls_denominator_duplicate_or_hidden_failure_reject",qos_ok&&qos_policy_rejects(qos_plan,qos_bad));
 qos_ok=qos_run_rejects(J::object());for(const auto* field:{"schema","policy","version","execution_speed_mask","supported_mask","process_id","owner_thread_id","close_process_id","close_thread_id","previous","requested","applied_readback","restored_readback","rollback_readback","fallback_readback"}){qos_bad=qos_run;qos_bad["process_qos"].erase(field);qos_ok=qos_ok&&qos_run_rejects(qos_bad);qos_bad=qos_run;qos_bad["process_qos"][field]="malformed";qos_ok=qos_ok&&qos_run_rejects(qos_bad);}qos_bad=qos_run;qos_bad["process_qos"]["version"]=1.0;
 qos_check("qos_runtime_missing_snapshot_policy_or_malformed_types_reject",qos_ok&&qos_run_rejects(qos_bad));
 qos_ok=rejects([&]{n::verify_process_qos(qos_run,true,102);});for(const auto* field:{"process_id","owner_thread_id","close_process_id","close_thread_id","process_class_before","process_class_after_close"}){qos_bad=qos_run;qos_bad["process_qos"][field]=0;qos_ok=qos_ok&&qos_run_rejects(qos_bad);}qos_bad=qos_run;qos_bad["process_qos"]["close_thread_id"]=202;qos_ok=qos_ok&&qos_run_rejects(qos_bad);qos_bad=qos_run;qos_bad["consumer_priority"]["thread_id"]=202;
 qos_check("qos_runtime_native_pid_owner_close_or_class_reject",qos_ok&&qos_run_rejects(qos_bad));
 qos_ok=true;for(const auto& change:std::array<J,5>{J{{"state_mask",5}},J{{"state_mask",0}},J{{"control_mask",4}},J{{"version",2}},J{{"control_mask",13}}}){qos_bad=qos_run;for(const auto& [field,value]:change.items())qos_bad["process_qos"]["requested"][field]=value;qos_ok=qos_ok&&qos_run_rejects(qos_bad);}for(const auto* field:{"applied_readback","restored_readback"}){qos_bad=qos_run;qos_bad["process_qos"][field]["state_mask"]=0;qos_ok=qos_ok&&qos_run_rejects(qos_bad);}
 for(const auto* field:{"rollback_readback","fallback_readback"})for(const auto* member:{"version","control_mask","state_mask"})for(const J value:{J(1),J(0.0),J("0")}){qos_bad=qos_run;qos_bad["process_qos"][field][member]=value;qos_ok=qos_ok&&qos_run_rejects(qos_bad);}
 qos_check("qos_request_timer_bit_applied_or_exact_restore_reject",qos_ok);
 qos_ok=true;for(const auto* field:{"verified","constructor_attempted","previous_available","requested_available","set_attempted","set_succeeded","applied_available","applied","close_attempted","restore_set_attempted","restore_set_succeeded","restored_available","restored","rollback_attempted","rollback_available","rollback_restored","fallback_attempted","fallback_available","fallback_restored"}){qos_bad=qos_run;qos_bad["process_qos"][field]=!qos_snapshot.at(field).get<bool>();qos_ok=qos_ok&&qos_run_rejects(qos_bad);qos_bad=qos_run;qos_bad["process_qos"][field]=1;qos_ok=qos_ok&&qos_run_rejects(qos_bad);}for(const auto* field:{"query_previous","set_requested","query_applied","set_restore","query_restored"}){for(const auto* member:{"sampled","status","error"}){qos_bad=qos_run;qos_bad["process_qos"][field][member]=member==std::string_view("sampled")?J(false):member==std::string_view("status")?J(0):J(5);qos_ok=qos_ok&&qos_run_rejects(qos_bad);}}qos_bad=qos_run;qos_bad["process_qos"]["query_fallback"]["sampled"]=true;qos_ok=qos_ok&&qos_run_rejects(qos_bad);qos_bad=qos_run;qos_bad["process_qos"]["failure"]="readback";
 qos_check("qos_api_failure_availability_fallback_or_unverified_reject",qos_ok&&qos_run_rejects(qos_bad));
 qos_bad=qos_report;qos_bad["process_qos_controls"]["actual_windows_snapshot"]["process_id"]=102;
 qos_ok=rejects([&]{n::verify_process_qos(qos_run,false,101);})&&qos_policy_rejects(qos_plan,qos_bad)&&rejects([&]{n::process_qos_policy(qos_plan,qos_report,tail_new_header,tail_new_tests,2'000'000,101);})&&rejects([&]{n::process_qos_policy(qos_plan,qos_report,tail_new_header,tail_new_tests,20'000'000,101);});
 qos_ok=qos_ok&&rejects([&]{n::process_qos_policy(tail_plan,tail_report,tail_new_header,tail_new_tests,2'000'000,101);})&&rejects([&]{n::process_qos_policy(tail_plan,tail_report,qos_two_ms_header1,qos_two_ms_tests2,2'000'000,101);})&&rejects([&]{n::process_qos_policy(tail_plan,tail_report,qos_two_ms_header2,qos_two_ms_tests1,2'000'000,101);});
 qos_check("qos_legacy_runtime_control_pid_or_wait_policy_downgrade_reject",qos_ok);
 if(qos_cases.size()!=12)throw std::runtime_error("separate process QoS reader control12 denominator");
 report["process_qos_policy_controls"]={{"schema","pas.process-qos-policy-reader-controls.v1"},{"assertions",qos_cases.size()},{"checks",qos_cases},{"failed",0},{"pass",true},{"synthetic_fixture_only",true},{"qualification_issued",false},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
 // New placement/compile declarations have their own controls; original81
 // and the separate active/QoS policy controls retain their denominators.
 namespace cd=g::component_delta;J delta_cases=J::array();const auto delta_check=[&](const char* id,bool ok){delta_cases.push_back({{"id",id},{"pass",ok}});if(!ok)throw std::runtime_error(id);};
 J legacy_placement={{"schema","pas.cold-worker-placement.v2"},{"policy_id",cd::eligibility_policy},{"policy_version",2},{"valid",true},{"failure","none"},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},{"consumer_affinity_applied",false},{"topology_available",true},{"producer_anchor_role","identity anchor only; producer_mask is the complete eligible union"},{"group",0},{"active_groups",1},{"process_class",32},{"topology_error",0},{"topology_bytes",256},{"topology_cores",4},{"topology_logical_processors",8},{"process_mask",255},{"system_mask",255},{"consumer_mask",16},{"consumer_core",2},{"consumer_number",4},{"consumer_core_mask",48},{"producer_anchor_mask",64},{"producer_core",3},{"producer_number",6},{"producer_mask",69},{"efficiency_class",1},{"producer_eligible_core_count",3},
 {"topology_core_records",J::array({J{{"core",0},{"mask",3},{"lowest_lp",0},{"efficiency_class",1}},J{{"core",1},{"mask",12},{"lowest_lp",2},{"efficiency_class",1}},J{{"core",2},{"mask",48},{"lowest_lp",4},{"efficiency_class",1}},J{{"core",3},{"mask",192},{"lowest_lp",6},{"efficiency_class",1}}})},
 {"producer_eligible_cores",J::array({J{{"core",0},{"number",0},{"mask",1},{"physical_core_mask",3}},J{{"core",1},{"number",2},{"mask",4},{"physical_core_mask",12}},J{{"core",3},{"number",6},{"mask",64},{"physical_core_mask",192}}})}};
 J placement=legacy_placement;placement["schema"]="pas.cold-worker-placement.v3";placement["policy_id"]=cd::eligibility_policy_v3;placement["policy_version"]=3;
 placement["consumer_anchor_mask"]=16;placement["consumer_mask"]=17;placement["producer_mask"]=68;
 placement["consumer_physical_core_mask"]=51;placement["producer_physical_core_mask"]=204;
 placement["consumer_eligible_core_count"]=2;placement["producer_eligible_core_count"]=2;
 placement["consumer_eligible_cores"]=J::array({J{{"core",0},{"number",0},{"mask",1},{"physical_core_mask",3}},J{{"core",2},{"number",4},{"mask",16},{"physical_core_mask",48}}});
 placement["producer_eligible_cores"]=J::array({J{{"core",1},{"number",2},{"mask",4},{"physical_core_mask",12}},J{{"core",3},{"number",6},{"mask",64},{"physical_core_mask",192}}});
 cd::eligibility_placement(legacy_placement,false);cd::eligibility_placement(placement,false);auto placed=placement;placed["consumer_affinity_applied"]=true;cd::eligibility_placement(placed,true);
 auto restricted=placement;restricted["process_mask"]=254;restricted["consumer_mask"]=20;restricted["producer_mask"]=64;restricted["consumer_physical_core_mask"]=60;restricted["producer_physical_core_mask"]=192;restricted["producer_eligible_core_count"]=1;
 restricted["consumer_eligible_cores"]=J::array({J{{"core",1},{"number",2},{"mask",4},{"physical_core_mask",12}},J{{"core",2},{"number",4},{"mask",16},{"physical_core_mask",48}}});restricted["producer_eligible_cores"]=J::array({J{{"core",3},{"number",6},{"mask",64},{"physical_core_mask",192}}});cd::eligibility_placement(restricted,false);
 auto lower_class=placement;lower_class["topology_core_records"].push_back(J{{"core",4},{"mask",768},{"lowest_lp",8},{"efficiency_class",0}});lower_class["topology_cores"]=5;lower_class["topology_logical_processors"]=10;lower_class["process_mask"]=1023;lower_class["system_mask"]=1023;cd::eligibility_placement(lower_class,false);
 const J current_placement_policy={{"producer_eligibility_policy",cd::eligibility_policy_v3}};cd::eligibility_current_placement(placement,current_placement_policy,false);
 delta_check("balanced_current_v3_and_historical_v2_without_downgrade",rejects([&]{cd::eligibility_current_placement(legacy_placement,current_placement_policy,false);}));
 const auto placement_reject=[&](const char* id,const auto& mutate){auto p=placement;mutate(p);delta_check(id,rejects([&]{cd::eligibility_placement(p,false);}));};
 placement_reject("legacy_or_unknown_schema_reject",[](J& p){p["schema"]="pas.cold-worker-placement.v1";});
 placement_reject("policy_version_float_reject",[](J& p){p["policy_version"]=3.0;});
 placement_reject("missing_actual_topology_reject",[](J& p){p.erase("topology_available");});
 placement_reject("invented_highest_class_reject",[](J& p){p["efficiency_class"]=0;});
 auto bad_placement=placement;bad_placement["consumer_eligible_cores"].erase(0);bad_placement["consumer_eligible_core_count"]=1;bad_placement["consumer_mask"]=16;bad_placement["consumer_physical_core_mask"]=48;
 bool incomplete=rejects([&]{cd::eligibility_placement(bad_placement,false);});bad_placement=placement;bad_placement["producer_eligible_cores"].erase(0);bad_placement["producer_eligible_core_count"]=1;bad_placement["producer_mask"]=64;bad_placement["producer_physical_core_mask"]=192;incomplete=incomplete&&rejects([&]{cd::eligibility_placement(bad_placement,false);});
 bad_placement=placement;bad_placement["consumer_eligible_cores"][0]=placement["producer_eligible_cores"][0];bad_placement["producer_eligible_cores"][0]=placement["consumer_eligible_cores"][0];bad_placement["consumer_mask"]=20;bad_placement["producer_mask"]=65;bad_placement["consumer_physical_core_mask"]=60;bad_placement["producer_physical_core_mask"]=195;
 delta_check("eligible_omission_or_producer_first_partition_reject",incomplete&&rejects([&]{cd::eligibility_placement(bad_placement,false);}));
 placement_reject("whole_physical_core_overlap_reject",[](J& p){p["producer_physical_core_mask"]=255;});
 bad_placement=placement;bad_placement["consumer_core_mask"]=16;bool anchor_bad=rejects([&]{cd::eligibility_placement(bad_placement,false);});bad_placement=placement;bad_placement["process_mask"]=239;anchor_bad=anchor_bad&&rejects([&]{cd::eligibility_placement(bad_placement,false);});bad_placement=placement;bad_placement.erase("consumer_anchor_mask");
 delta_check("wrong_physical_or_unavailable_missing_anchor_reject",anchor_bad&&rejects([&]{cd::eligibility_placement(bad_placement,false);}));
 placement_reject("nonlowest_sibling_LP_reject",[](J& p){p["topology_core_records"][0]["lowest_lp"]=1;});
 placement_reject("duplicate_physical_core_or_mask_reject",[](J& p){p["topology_core_records"][1]["core"]=0;});
 placement_reject("malformed_scope_or_reserved_consumer_reject",[](J& p){p["consumer_affinity_applied"]=true;p["device_operations"]=0.0;});
 J delta={{"schema","pas.cold-producer-eligibility-delta-build.v1"},{"source_commit",std::string(40,'c')},{"base_source_commit",cd::eligibility_base_commit},{"configuration","release"},{"producer_eligibility_policy",cd::eligibility_policy_v3},{"cold_harness_delta",true},{"formal_rebuilt",false},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},{"targets",cd::eligibility_targets()},{"translation_unit_count",10},{"translation_units",cd::eligibility_units()},{"imported_static_libraries",J::array()}};
 for(const auto* role:{"cpu_current_rails","cpu_current_front","cpu_exact_bvi_v3","cpu_own_bridge","cpu_pas_core","production_pas_core","production_pas_current_candidate","production_pas_proto","verification_pas_emulator","verification_pas_bench"})delta["imported_static_libraries"].push_back({{"role",role},{"historical_import",true},{"freshly_rebuilt",false},{"compiled_source_commit",std::string(40,'a')}});
 auto delta_plan=delta;delta_plan["schema"]="pas.cold-producer-eligibility-delta-plan.v1";cd::eligibility_declaration(delta,delta_plan);auto bad_delta=delta;bad_delta["translation_unit_count"]=52;bool invalid=rejects([&]{cd::eligibility_declaration(bad_delta,delta_plan);});bad_delta=delta;bad_delta["targets"].push_back("pas_core");invalid=invalid&&rejects([&]{cd::eligibility_declaration(bad_delta,delta_plan);});bad_delta=delta;bad_delta["formal_rebuilt"]=true;invalid=invalid&&rejects([&]{cd::eligibility_declaration(bad_delta,delta_plan);});bad_delta=delta;bad_delta["producer_eligibility_policy"]=cd::eligibility_policy;invalid=invalid&&rejects([&]{cd::eligibility_declaration(bad_delta,delta_plan);});auto bad_policy_plan=delta_plan;bad_policy_plan["producer_eligibility_policy"]=cd::eligibility_policy;invalid=invalid&&rejects([&]{cd::eligibility_declaration(delta,bad_policy_plan);});
 // The existing twelfth case covers the source/object/link/member/actual-case
 // boundaries of the optional motion overlay. It does not alter the81 or12
 // denominators, and it does not use its in-memory fixtures as Native evidence.
 const auto mpin=[&](const fs::path& path){return J{{"path",path.string()},{"bytes",std::uint64_t(3)},{"sha256",sha}};};
 const auto motion_root=root/"motion-build";auto motion_delta=delta;motion_delta["build_root"]=motion_root.string();motion_delta["targets"]=cd::eligibility_targets(true);motion_delta["translation_unit_count"]=14;motion_delta["translation_units"]=cd::eligibility_units(true);
 motion_delta["base_app_object"]=mpin(root/"base/app.obj");motion_delta["files"]=J::array();motion_delta["compiled_outputs"]=J::array();
 J motion={{"schema","pas.cold-motion-source-delta.v1"},{"source_commit",delta["source_commit"]},{"base_source_commit",cd::eligibility_base_commit},{"source_path","src/game_motion.cpp"},{"literal_transform","ridge_guarded_gray_short_circuit.v1"},{"base_source",mpin(root/"base/game_motion.cpp")},{"current_source",mpin(root/"current/game_motion.cpp")},{"compile_source",mpin(root/"short/game_motion.cpp")},{"formal_libraries_rebuilt",false},{"fresh_formal_translation_units",1},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},{"consumer_targets",cd::motion_consumers()},{"consumer_images",J::array()},{"original_archive_members",J::array()}};
 const J old_motion_source=mpin(root/"original/game_motion.cpp");for(auto& library:motion_delta["imported_static_libraries"]){const auto role=library.at("role").get<std::string>();library["binary"]=mpin(root/(role+".lib"));if(role=="cpu_pas_core"||role=="production_pas_core"){const auto member=mpin(root/(role+"-game_motion.obj"));library["translation_unit_count"]=22;library["replaced_archive_sources"]=J::array({"src/game_motion.cpp"});library["replacement_target"]="game_motion_delta";library["translation_units"]=J::array({J{{"relative_source","src/game_motion.cpp"},{"historical_object",member},{"original_source",old_motion_source}}});motion["original_archive_members"].push_back({{"role",role},{"archive",library["binary"]},{"member",member},{"source",old_motion_source}});}}
 motion["regression"]={{"target","motion_source_regression"},{"test_sources",J::array({mpin(root/"tests/game_tests.cpp"),mpin(root/"tests/cold_scenario_tests.cpp")})},{"runtime_contract_source",mpin(root/"generated/runtime_contract.cpp")},{"runtime_contract_prefix",mpin(root/"src/runtime.cpp")},{"cases",26},{"case_names",cd::motion_case_names()},{"filter",cd::motion_filter}};
 std::map<std::string,std::string> object_by_target;std::map<std::string,std::vector<std::string>> own_objects;std::string ninja="  FLAGS = /O2 /Ob2 /DNDEBUG /MD \n";
 for(auto& unit:motion_delta["translation_units"]){const auto target=unit.at("target").get<std::string>(),relative=unit.at("relative_source").get<std::string>();const auto object="CMakeFiles/"+target+".dir/"+fs::path(relative).filename().string()+".obj";unit["source"]=mpin(root/"short"/relative);if(target=="game_motion_delta"){unit["source"]=motion["compile_source"];motion["new_object"]=mpin(motion_root/object);}object_by_target[target]=object;own_objects[target].push_back(object);motion_delta["compiled_outputs"].push_back(mpin(motion_root/object));ninja+="build "+object+": CXX_COMPILER_Release "+fs::path(unit["source"]["path"].get<std::string>()).generic_string()+" || order\n";}
 const auto consumers=cd::motion_consumers();for(const auto& target:consumers)motion["consumer_images"].push_back(mpin(motion_root/(target.get<std::string>()+".exe")));
 for(const auto* target:{"current_chain","worker_affinity_controls","production_pipeline_cold","score_only_controls","qualification_reader","qualification_reader_controls","pas","candidate_admission_controls","motion_source_regression"}){const auto exe=std::string(target)+".exe";motion_delta["files"].push_back(mpin(motion_root/exe));std::set<std::string> objects(own_objects[target].begin(),own_objects[target].end());if(std::any_of(consumers.begin(),consumers.end(),[&](const auto& t){return t==target;})||std::string_view(target)=="motion_source_regression")objects.insert(object_by_target.at("game_motion_delta"));if(std::string_view(target)=="pas"||std::string_view(target)=="candidate_admission_controls")objects.insert(object_by_target.at("candidate_admission_delta"));if(std::string_view(target)=="pas")objects.insert(fs::path(motion_delta["base_app_object"]["path"].get<std::string>()).generic_string());ninja+="build "+exe+": CXX_EXECUTABLE_LINKER_Release";for(const auto& object:objects)ninja+=' '+object;ninja+=" | dependencies\n  LINK_LIBRARIES = ";std::set<std::string> roles;if(std::string_view(target)=="current_chain")roles={"cpu_current_rails","cpu_current_front","cpu_exact_bvi_v3","cpu_own_bridge","cpu_pas_core"};else if(std::string_view(target)=="production_pipeline_cold"||std::string_view(target)=="score_only_controls")roles={"production_pas_core","production_pas_current_candidate","production_pas_proto"};else if(std::string_view(target)=="pas")roles={"production_pas_core","production_pas_current_candidate","production_pas_proto","verification_pas_emulator","verification_pas_bench"};else if(std::string_view(target)=="candidate_admission_controls")roles={"production_pas_core","production_pas_current_candidate","production_pas_proto","verification_pas_emulator"};else if(std::string_view(target)=="motion_source_regression")roles={"production_pas_core"};for(const auto& lib:motion_delta["imported_static_libraries"])if(roles.contains(lib["role"].get<std::string>()))ninja+='"'+fs::path(lib["binary"]["path"].get<std::string>()).generic_string()+"\" ";ninja+='\n';}
 motion_delta["motion_source_delta"]=motion;auto motion_plan=motion_delta;motion_plan["schema"]="pas.cold-producer-eligibility-delta-plan.v1";cd::eligibility_declaration(motion_delta,motion_plan);const auto motion_graph=cd::eligibility_graph(motion_delta,root,ninja);cd::eligibility_objects(motion_delta,motion_graph);cd::motion_archive_join(motion,motion_delta,old_motion_source);
 auto malformed_motion=motion_delta;malformed_motion["translation_units"][10]["relative_source"]="src/game.cpp";invalid=invalid&&rejects([&]{cd::eligibility_declaration(malformed_motion,motion_plan);});malformed_motion=motion_delta;malformed_motion["motion_source_delta"]["new_object"]["path"]=(root/"stale.obj").string();invalid=invalid&&rejects([&]{cd::eligibility_graph(malformed_motion,root,ninja);});
 auto wrong_link=ninja;const auto link_at=wrong_link.find("build production_pipeline_cold.exe:");const auto object_at=wrong_link.find(object_by_target.at("game_motion_delta"),link_at);wrong_link.erase(object_at,object_by_target.at("game_motion_delta").size());invalid=invalid&&rejects([&]{cd::eligibility_graph(motion_delta,root,wrong_link);});
 auto wrong_member=motion;wrong_member["original_archive_members"][0]["member"]["sha256"]=std::string(64,'0');invalid=invalid&&rejects([&]{cd::motion_archive_join(wrong_member,motion_delta,old_motion_source);});
 for(const auto* role:{"cpu_pas_core","production_pas_core"}){
  for(const auto* missing:{"replaced_archive_sources","replacement_target"}){auto missing_shape=motion_delta;for(auto& library:missing_shape["imported_static_libraries"])if(library["role"]==role)library.erase(missing);invalid=invalid&&rejects([&]{cd::motion_archive_join(motion,missing_shape,old_motion_source);});}
  auto wrong_target=motion_delta;for(auto& library:wrong_target["imported_static_libraries"])if(library["role"]==role)library["replacement_target"]="candidate_admission_delta";invalid=invalid&&rejects([&]{cd::motion_archive_join(motion,wrong_target,old_motion_source);});
 }
 std::string original_ridge="prefix\n"+std::string(cd::motion_old_ridge)+"\nsuffix\n",candidate_ridge="prefix\n"+std::string(cd::motion_new_ridge)+"\nsuffix\n",original_crlf;for(const char c:original_ridge){if(c=='\n')original_crlf+='\r';original_crlf+=c;}cd::motion_source_equivalence(original_crlf,candidate_ridge);invalid=invalid&&rejects([&]{cd::motion_source_equivalence(original_crlf,candidate_ridge+"extra");});
 J motion_report={{"tests",26},{"failures",0},{"disabled",0},{"errors",0},{"testsuites",J::array()}};std::map<std::string,J> suites;for(const auto& full:cd::motion_case_names()){const auto name=full.get<std::string>();const auto dot=name.find('.');const auto suite=name.substr(0,dot);if(!suites.contains(suite))suites[suite]={{"name",suite},{"tests",0},{"failures",0},{"disabled",0},{"errors",0},{"testsuite",J::array()}};auto& s=suites.at(suite);s["tests"]=s["tests"].get<unsigned>()+1;s["testsuite"].push_back({{"name",name.substr(dot+1)},{"status","RUN"},{"result","COMPLETED"}});}for(const auto& [name,suite]:suites){(void)name;motion_report["testsuites"].push_back(suite);}cd::motion_regression_report(motion_report);auto missing_case=motion_report;missing_case["testsuites"][0]["testsuite"].erase(0);invalid=invalid&&rejects([&]{cd::motion_regression_report(missing_case);});motion_report["testsuites"][0]["testsuite"][0]["result"]="SKIPPED";invalid=invalid&&rejects([&]{cd::motion_regression_report(motion_report);});
 auto motion_harness=motion_delta;for(const auto* field:{"source_manifest","core_build_plan","base_production_build"})motion_harness[field]=mpin(root/(std::string(field)+".json"));
 J motion_verification={{"schema","pas.cold-candidate-verification-delta.v1"},{"source_commit",delta["source_commit"]},{"source_dirty",false},{"base_source_commit",cd::eligibility_base_commit},{"formal_rebuilt",false},{"verification_only_delta",false},{"verification_and_motion_delta",true},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},{"motion_source_delta",motion},{"candidate_binary",mpin(motion_root/"pas.exe")},{"candidate_source",mpin(root/"src/candidate_admission.cpp")},{"tests_source",mpin(root/"tests/candidate_admission_tests.cpp")}};
 for(const auto* field:{"source_manifest","core_build_plan","base_production_build"})motion_verification[field]=motion_harness[field];J motion_context={{"source_commit",cd::eligibility_base_commit},{"candidate_binary_sha256",sha},{"verification_delta_identity",J{{"schema","pas.cold-candidate-verification-identity.v1"},{"source_commit",delta["source_commit"]},{"source_dirty",false},{"base_source_commit",cd::eligibility_base_commit},{"candidate_admission_sha256",sha},{"candidate_admission_tests_sha256",sha}}}};
 cd::verification_declaration(motion_verification,motion_harness,motion_context);auto wrong_verification=motion_verification;wrong_verification["verification_only_delta"]=true;invalid=invalid&&rejects([&]{cd::verification_declaration(wrong_verification,motion_harness,motion_context);});wrong_verification=motion_verification;wrong_verification["motion_source_delta"]["new_object"]["sha256"]=std::string(64,'0');invalid=invalid&&rejects([&]{cd::verification_declaration(wrong_verification,motion_harness,motion_context);});
 delta_check("historical10_current14_literal_object_link_member_actual26_and_verification_join_required",invalid);
 if(delta_cases.size()!=12)throw std::runtime_error("separate cold harness reader12 denominator");report["cold_harness_delta_controls"]={{"schema","pas.cold-harness-delta-reader-controls.v1"},{"assertions",delta_cases.size()},{"checks",delta_cases},{"failed",0},{"pass",true},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0}};
// Paste this block before the final report write in qualification_reader/controls.cpp.
// Requires existing namespace cd, J, fs, root, rejects, report, r, save.
// Synthetic structural fixtures NEVER stand in for actual Frame Native8 evidence.
{
 J fm_cases=J::array();std::size_t fm_expectations=0;
 const auto fm_check=[&](const char* id,bool ok){fm_cases.push_back({{"id",id},{"pass",ok}});if(!ok)throw std::runtime_error(id);};
 const auto fm_reject=[&](auto&& fn){++fm_expectations;return rejects(std::forward<decltype(fn)>(fn));};
 const auto fm_pin=[&](const fs::path& p){return J{{"path",fs::absolute(p).string()},{"bytes",std::uint64_t(3)},{"sha256",std::string(64,'a')}};};
 const std::string fm_commit(40,'a');
 J fm_frame={{"schema","pas.cold-frame-metadata-delta.v1"},{"policy",cd::frame_policy},{"source_commit",fm_commit},{"source_dirty",false},{"base_source_commit",cd::frame_base_commit},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},
 {"base_chain_source",fm_pin(root/"base80/research/prelive_current/chain.cpp")},{"current_chain_source",fm_pin(root/"current/research/prelive_current/chain.cpp")},{"helper_source",fm_pin(root/"current/research/prelive_current/frame_metadata.hpp")},{"tests_source",fm_pin(root/"current/research/prelive_current/tests/frame_metadata_tests.cpp")},
 {"frame_controls",J{{"target","frame_metadata_controls"},{"schema","pas.cold-frame-metadata-reuse-controls.v1"},{"assertions",8}}}};
 fm_frame["base_chain_source"]["bytes"]=29747;fm_frame["base_chain_source"]["sha256"]="76e8a662bfdb2947349a32620277da315a2f9cbd6981a74978f1b62cc2ba3f22";
 J fm_binding={{"source_commit",fm_commit},{"frame_metadata_delta",fm_frame}},fm_plan=fm_binding;
 const std::string fm_base=std::string(cd::frame_include_old)+"body\n"+cd::frame_begin_old+"\nconst auto due=due_epoch+i*cadence;\n"+cd::frame_loop_old+"\nlatest.publish(pixels,bytes,f);\n";
 const auto fm_current=cd::frame_expected_chain(fm_base);
 const J fm_manifest={{"files",J::array({fm_frame.at("current_chain_source"),fm_frame.at("helper_source"),fm_frame.at("tests_source")})}};
 J fm_report={{"schema","pas.cold-frame-metadata-reuse-controls.v1"},{"assertions",8},{"failed",0},{"pass",true},{"checks",J::array()},{"source_commit",fm_commit},{"source_build_commit",fm_commit},{"source_dirty",false},{"source_build_dirty",false},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},{"long_gap_root_cause_claim",false},{"performance_measurement",false},{"original_control_denominators_modified",false}};
 for(const auto& id:cd::frame_case_names())fm_report["checks"].push_back({{"id",id},{"pass",true}});
 cd::frame_metadata_declaration(fm_binding,fm_plan);cd::frame_manifest_join(fm_frame,fm_manifest);cd::frame_source_equivalence(fm_base,fm_current);cd::frame_helper_equivalence(cd::frame_helper_literal);cd::frame_report(fm_report,fm_binding);

 // 1: both legacy attachments absent is allowed ONLY for old source. A used
 // helper/getter cannot silently enter the old14 branch with a missing flag.
 bool fm_ok=true;J fm_legacy={{"source_commit",fm_commit}};
 cd::frame_metadata_declaration(fm_legacy,fm_legacy);cd::frame_require_declared_source(false,fm_base);
 fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_legacy,fm_plan);})&&fm_ok;
 fm_ok=fm_reject([&]{cd::frame_require_declared_source(false,fm_current);})&&fm_ok;
 fm_ok=fm_reject([&]{cd::frame_require_declared_source(false,fm_base+"producer_metadata.for_attempt(1,0);\n");})&&fm_ok;
 auto fm_bad=fm_binding;fm_bad["frame_metadata_delta"]=true;fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_bad,fm_plan);})&&fm_ok;
 fm_bad=fm_binding;fm_bad["frame_metadata_delta"]["policy"]="fixed_metadata_after_epoch.v1";fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_bad,fm_bad);})&&fm_ok;
 fm_check("explicit_attachment_and_undeclared_source_reject",fm_ok);

 // 2: the immutable old80 copy is independent of formal base55. Mutating its
 // byte/hash identity or binding a different current commit is rejected.
 fm_ok=true;
 for(const auto* key:{"bytes","sha256"}){fm_bad=fm_binding;if(std::string_view(key)=="bytes")fm_bad["frame_metadata_delta"]["base_chain_source"][key]=29748;else fm_bad["frame_metadata_delta"]["base_chain_source"][key]=std::string(64,'0');fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_bad,fm_bad);})&&fm_ok;}
 fm_bad=fm_binding;fm_bad["frame_metadata_delta"]["base_source_commit"]=std::string(40,'b');fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_bad,fm_bad);})&&fm_ok;
 fm_bad=fm_binding;fm_bad["frame_metadata_delta"]["source_commit"]=std::string(40,'b');fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_bad,fm_bad);})&&fm_ok;
 fm_bad=fm_binding;fm_bad["frame_metadata_delta"]["source_dirty"]=true;fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_bad,fm_bad);})&&fm_ok;
 fm_check("original80_copy_current_identity_and_dirty_reject",fm_ok);

 // 3: every byte outside the exact three literals is retained; clocks,
 // absolute epoch/cadence and publication calls cannot be smuggled in.
 fm_ok=true;
 for(const auto& edited:std::array<std::string,4>{{cd::frame_replace_once(fm_current,"clock.now_ns()","clock.now_ns()+1"),cd::frame_replace_once(fm_current,"due_epoch+i*cadence","clock.now_ns()+i*cadence"),cd::frame_replace_once(fm_current,"latest.publish(pixels,bytes,f)","latest.publish(pixels,bytes+1,f)"),fm_current+"extra_business_edit\n"}})
  fm_ok=fm_reject([&]{cd::frame_source_equivalence(fm_base,edited);})&&fm_ok;
 fm_check("clock_epoch_cadence_publication_body_byte_preservation",fm_ok);

 // 4: constructor remains before begin/epoch/thread launch exactly once;
 // stale per-attempt fresh Frame setup or an additional field write rejects.
 fm_ok=true;
 const auto fm_ctor=std::string(" cold::frame_metadata::FixedMetadata producer_metadata;\n");
 for(const auto& edited:std::array<std::string,4>{{fm_current+fm_ctor,cd::frame_replace_once(fm_current,cd::frame_begin_new,std::string(cd::frame_begin_old)+"\n"+fm_ctor),cd::frame_replace_once(fm_current,cd::frame_loop_new,cd::frame_loop_old),fm_current+"f.published_ns=1;\n"}})
  fm_ok=fm_reject([&]{cd::frame_source_equivalence(fm_base,edited);})&&fm_ok;
 fm_ok=fm_reject([&]{cd::frame_source_equivalence(fm_base+cd::frame_loop_old,fm_current);})&&fm_ok;
 fm_check("one_constructor_before_epoch_exact_getter_and_no_extra_fields",fm_ok);

 // 5: the complete helper body is fixed, rather than accepting a caller's
 // expectation hash/string or checking only its backend label.
 fm_ok=true;
 for(const auto& edited:std::array<std::string,4>{{cd::frame_replace_once(cd::frame_helper_literal,"offline_memory_stub","different_backend"),cd::frame_replace_once(cd::frame_helper_literal,"value_.width=1280","value_.width=1279"),cd::frame_replace_once(cd::frame_helper_literal,"return value_;","value_.published_ns=1;return value_;"),cd::frame_replace_once(cd::frame_helper_literal,"return value_;","(void)clock.now_ns();return value_;")}})
  fm_ok=fm_reject([&]{cd::frame_helper_equivalence(edited);})&&fm_ok;
 fm_check("whole_helper_literal_rejects_metadata_or_clock_change",fm_ok);

 // 6: current helper/tests/chain are actual current manifest members. Same
 // hash with a different path, missing members and stale test hashes reject.
 fm_ok=true;
 auto fm_bad_manifest=fm_manifest;fm_bad_manifest["files"].erase(1);fm_ok=fm_reject([&]{cd::frame_manifest_join(fm_frame,fm_bad_manifest);})&&fm_ok;
 fm_bad_manifest=fm_manifest;fm_bad_manifest["files"][2]["sha256"]=std::string(64,'0');fm_ok=fm_reject([&]{cd::frame_manifest_join(fm_frame,fm_bad_manifest);})&&fm_ok;
 auto fm_bad_frame=fm_frame;fm_bad_frame["current_chain_source"]["path"]=(root/"shadow/chain.cpp").string();fm_ok=fm_reject([&]{cd::frame_manifest_join(fm_bad_frame,fm_manifest);})&&fm_ok;
 fm_bad_manifest=fm_manifest;fm_bad_manifest["files"].push_back(fm_manifest["files"][0]);fm_ok=fm_reject([&]{cd::frame_manifest_join(fm_frame,fm_bad_manifest);})&&fm_ok;
 fm_check("manifest_missing_stale_shadow_and_duplicate_sources_reject",fm_ok);

 // 7: separate Frame8 retains exact case IDs and typed denominators; it
 // cannot replace any old control count or claim performance/live success.
 fm_ok=true;fm_bad=fm_binding;fm_bad["frame_metadata_delta"]["frame_controls"]["assertions"]=8.0;fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_bad,fm_bad);})&&fm_ok;
 fm_bad=fm_binding;fm_bad["frame_metadata_delta"]["frame_controls"]["target"]="old_component_mask_tests";fm_ok=fm_reject([&]{cd::frame_metadata_declaration(fm_bad,fm_bad);})&&fm_ok;
 for(const auto* key:{"assertions","source_build_commit","source_build_dirty","long_gap_root_cause_claim","checks"}){auto bad_report=fm_report;if(std::string_view(key)=="assertions")bad_report[key]=8.0;else if(std::string_view(key)=="source_build_commit")bad_report[key]=std::string(40,'b');else if(std::string_view(key)=="checks")bad_report[key][1]["id"]=bad_report[key][0]["id"];else bad_report[key]=true;fm_ok=fm_reject([&]{cd::frame_report(bad_report,fm_binding);})&&fm_ok;}
 auto bad_report=fm_report;bad_report["checks"][0]["pass"]=false;fm_ok=fm_reject([&]{cd::frame_report(bad_report,fm_binding);})&&fm_ok;
 fm_check("frame8_typed_counts_exact_ids_compiled_source_and_flags",fm_ok);

 // 8: fake PASS metadata never substitutes for real Native8. Graph structural
 // positives are synthetic only; effective old-core-only linkage is required.
 fm_ok=true;
 fm_ok=fm_reject([&]{(void)cd::eligibility_control_native(r,fm_binding,J{{"pass",true}},fm_pin(root/"frame_metadata_controls.exe"),fm_pin(root/"frame-report.json"),J::array({(root/"frame-report.json").string()}));})&&fm_ok;
 const auto fm_build=root/"frame-build";J fm_graph_binding=fm_binding;fm_graph_binding["build_root"]=fm_build.string();fm_graph_binding["imported_static_libraries"]=J::array({J{{"role","cpu_pas_core"},{"binary",fm_pin(root/"old-core/pas_core.lib")}}});
 cd::Graph fm_graph;const std::string fm_object="CMakeFiles/frame_metadata_controls.dir/frame_metadata_tests.cpp.obj";fm_graph.objects[fm_object]=cd::frame_tests_relative;fm_graph.links["frame_metadata_controls.exe"]=fm_object+" | dependencies";fm_graph.link_libraries["frame_metadata_controls.exe"]='"'+(root/"old-core/pas_core.lib").generic_string()+"\" kernel32.lib";
 cd::frame_link_shape(fm_graph_binding,fm_graph);
 auto fm_bad_graph=fm_graph;fm_bad_graph.link_libraries["frame_metadata_controls.exe"]='"'+(root/"shadow/pas_core.lib").generic_string()+"\" kernel32.lib";fm_ok=fm_reject([&]{cd::frame_link_shape(fm_graph_binding,fm_bad_graph);})&&fm_ok;
 fm_bad_graph=fm_graph;fm_bad_graph.link_libraries["frame_metadata_controls.exe"]+=" "+fm_graph.link_libraries["frame_metadata_controls.exe"];fm_ok=fm_reject([&]{cd::frame_link_shape(fm_graph_binding,fm_bad_graph);})&&fm_ok;
 fm_bad_graph=fm_graph;fm_bad_graph.links["frame_metadata_controls.exe"]=fm_object+" CMakeFiles/game_delta.dir/game.cpp.obj | dependencies";fm_ok=fm_reject([&]{cd::frame_link_shape(fm_graph_binding,fm_bad_graph);})&&fm_ok;
 fm_bad_graph=fm_graph;fm_bad_graph.link_libraries["frame_metadata_controls.exe"]="kernel32.lib";fm_ok=fm_reject([&]{cd::frame_link_shape(fm_graph_binding,fm_bad_graph);})&&fm_ok;
 // Exercise the ACTUAL absent-attachment entry, not only the string predicate.
 const auto fm_small_chain=save("fm-undeclared-chain.cpp",fm_current);
 const J fm_small_pin={{"path",fm_small_chain.string()},{"bytes",fm_current.size()},{"sha256",g::streamed_sha256(fm_small_chain,fm_current.size())},{"relative_path",cd::frame_chain_relative}};
 const auto fm_small_manifest_text=J{{"files",J::array({fm_small_pin})}}.dump();
 const auto fm_small_manifest_path=save("fm-undeclared-manifest.json",fm_small_manifest_text);
 const J fm_small_manifest_pin={{"path",fm_small_manifest_path.string()},{"bytes",fm_small_manifest_text.size()},{"sha256",g::streamed_sha256(fm_small_manifest_path,fm_small_manifest_text.size())}};
 fm_ok=fm_reject([&]{(void)cd::frame_metadata_validate(r,J{{"source_manifest",fm_small_manifest_pin}});})&&fm_ok;
 fm_check("actual_native_required_oldcore_effective_link_and_absent_entry",fm_ok);

 if(fm_cases.size()!=8)throw std::runtime_error("separate Frame reader8 denominator");
 report["frame_metadata_delta_controls"]={{"schema","pas.cold-frame-metadata-reader-controls.v1"},{"assertions",8},{"checks",fm_cases},{"failed",0},{"pass",true},{"synthetic_fixture_only",true},{"expectation_count",fm_expectations},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0}};
}

{
 J dcases=J::array();const auto dcheck=[&](const char* id,bool pass){dcases.push_back({{"id",id},{"pass",pass}});if(!pass)throw std::runtime_error(id);};
 const std::string db=std::string("prefix\n")+cd::dense_old_note_anchor+"\nsuffix\n",dn=std::string("prefix\n")+cd::dense_new_note_anchor+"\nsuffix\n";
 cd::dense_source_equivalence(db,dn);
 dcheck("dense_exact_bom_free_lf_literal_and_inverse",rejects([&]{cd::dense_source_equivalence(std::string("\xef\xbb\xbf")+db,dn);})&&rejects([&]{cd::dense_source_equivalence(db,std::string("\xef\xbb\xbf")+dn);})&&rejects([&]{cd::dense_source_equivalence(db,dn+"\r\n");}));
 dcheck("dense_nonwhite_body_byte_change_reject",rejects([&]{cd::dense_source_equivalence(db,dn+"other_pixel_or_strategy_edit");}));
 dcheck("dense_duplicate_or_missing_anchor_reject",rejects([&]{cd::dense_source_equivalence(db+cd::dense_old_note_anchor,dn);})&&rejects([&]{cd::dense_source_equivalence(db,db);}));
 const auto dp=[&](const fs::path& p){return J{{"path",p.string()},{"bytes",std::uint64_t(3)},{"sha256",std::string(64,'a')}};};
 J dm={{"schema","pas.cold-dense-source-delta.v1"},{"source_commit",std::string(40,'c')},{"base_source_commit",cd::eligibility_base_commit},{"source_path","src/game.cpp"},{"literal_transform","white_note_decode_pca_short_circuit.v1"},{"base_source",dp(root/"base-game.cpp")},{"current_source",dp(root/"current-game.cpp")},{"compile_source",dp(root/"copy-game.cpp")},{"consumer_targets",cd::dense_consumers()},{"formal_libraries_rebuilt",false},{"fresh_formal_translation_units",1},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0},{"original_archive_members",J::array()}};
 const auto dsource=dp(root/"src/game.cpp");J dh={{"source_commit",dm["source_commit"]},{"motion_source_delta",J::object()},{"dense_source_delta",dm},{"imported_static_libraries",J::array()}};
 for(const auto* role:{"cpu_pas_core","production_pas_core"}){const auto archive=dp(root/(std::string(role)+".lib")),member=dp(root/(std::string(role)+"-game.obj"));dm["original_archive_members"].push_back({{"role",role},{"archive",archive},{"member",member},{"source",dsource}});dh["imported_static_libraries"].push_back({{"role",role},{"binary",archive},{"translation_unit_count",22},{"replaced_archive_sources",J::array({"src/game_motion.cpp","src/game.cpp"})},{"replacement_targets",cd::dense_replacement_map()},{"translation_units",J::array({J{{"relative_source","src/game.cpp"},{"historical_object",member},{"original_source",dsource}}})}});}
 dm["regression"]={{"target","motion_source_regression"},{"test_sources",J::array({dp(root/"game_tests.cpp"),dp(root/"cold_scenario_tests.cpp")})},{"runtime_contract_source",dp(root/"runtime_contract.cpp")},{"runtime_contract_prefix",dp(root/"runtime.cpp")},{"cases",12},{"case_names",cd::dense_case_names()},{"filter",cd::dense_filter()}};dh["dense_source_delta"]=dm;auto dplan=dh;
 cd::dense_declaration(dh,dplan);auto bad=dh;bad["dense_source_delta"]["fresh_formal_translation_units"]=1.0;bool ok=rejects([&]{cd::dense_declaration(bad,bad);});bad=dh;bad["dense_source_delta"]["live_ready"]=true;ok=ok&&rejects([&]{cd::dense_declaration(bad,bad);});
 dcheck("dense_typed_source_denominator_and_scope_reject",ok);
 cd::dense_archive_join(dm,dh,dsource);bad=dh;bad["imported_static_libraries"][0]["replaced_archive_sources"]=J::array({"src/game_motion.cpp"});ok=rejects([&]{cd::dense_archive_join(dm,bad,dsource);});bad=dh;bad["imported_static_libraries"][1]["replacement_targets"][1]["target"]="game_motion_delta";ok=ok&&rejects([&]{cd::dense_archive_join(dm,bad,dsource);});bad=dh;bad["imported_static_libraries"][0]["replacement_target"]="game_motion_delta";ok=ok&&rejects([&]{cd::dense_archive_join(dm,bad,dsource);});
 dcheck("dense_two_core_source_maps_no_scalar_ambiguity",ok);
 J dr={{"tests",12},{"failures",0},{"disabled",0},{"errors",0},{"testsuites",J::array()}};J ds={{"name","GameObserver"},{"tests",12},{"failures",0},{"disabled",0},{"errors",0},{"testsuite",J::array()}};for(const auto& full:cd::dense_case_names()){const auto n=full.get<std::string>();ds["testsuite"].push_back({{"name",n.substr(n.find('.')+1)},{"status","RUN"},{"result","COMPLETED"}});}dr["testsuites"].push_back(ds);cd::dense_regression_report(dr);auto br=dr;br["testsuites"][0]["testsuite"][0]["result"]="SKIPPED";ok=rejects([&]{cd::dense_regression_report(br);});br=dr;br["testsuites"][0]["testsuite"][1]["name"]=br["testsuites"][0]["testsuite"][0]["name"];ok=ok&&rejects([&]{cd::dense_regression_report(br);});
 dcheck("dense_actual12_case_names_completion_and_uniqueness",ok);
 bad=dplan;bad["dense_source_delta"]["compile_source"]["sha256"]=std::string(64,'0');ok=rejects([&]{cd::dense_declaration(dh,bad);});bad=dh;bad.erase("dense_source_delta");ok=ok&&rejects([&]{cd::dense_declaration(dh,bad);});bad=dh;bad.erase("motion_source_delta");ok=ok&&rejects([&]{cd::dense_declaration(bad,bad);});
 dcheck("dense_plan_compile_copy_attachment_motion_dependency",ok);
 auto bm=dm;bm["original_archive_members"][0]["source"]["sha256"]=std::string(64,'0');ok=rejects([&]{cd::dense_static(bm,dh);});bm=dm;bm["original_archive_members"][1]["role"]="cpu_pas_core";ok=ok&&rejects([&]{cd::dense_static(bm,dh);});br=dr;br["tests"]=26;ok=ok&&rejects([&]{cd::dense_regression_report(br);});
 // The actual native entry must select all declared target branches. Missing,
 // reordered and undeclared targets reject rather than accepting old argv.
 const auto dcommand=root/"build.cmd";const auto dbuild=root/"delta-build";
 const J dspec={{"exe",(root/"cmd.exe").string()},{"argv",J::array({"/d","/s","/c",dcommand.string()})}};
 const auto dline=[&](const J& targets){std::string text;for(const auto& token:J::array({(root/"cmake.exe").string(),"--build",dbuild.string(),"--parallel","4","--target"})){if(!text.empty())text+=' ';text+='"'+token.get<std::string>()+'"';}for(const auto& target:targets)text+=" \""+target.get<std::string>()+'"';return text+'\n';};
 for(const auto& flags:std::array<std::array<bool,3>,5>{{{false,false,false},{true,false,false},{true,true,false},{true,false,true},{true,true,true}}}){
  J decl={{"build_root",dbuild.string()}};if(flags[0])decl["motion_source_delta"]=J::object();if(flags[1])decl["dense_source_delta"]=J::object();if(flags[2])decl["frame_metadata_delta"]=J::object();
  const bool split=flags[1]||flags[2];const auto groups=split?cd::eligibility_build_groups(flags[1],flags[2]):J::array({J{{"targets",cd::eligibility_targets(flags[0],false,false)}}});
  if(split)decl["build_target_groups"]=groups;
  for(std::size_t phase=0;phase<groups.size();++phase){const auto targets=groups[phase].at("targets");cd::eligibility_build_command(decl,dspec,dline(targets),dcommand,phase);
   auto wrong=targets;wrong.erase(wrong.size()-1);ok=ok&&rejects([&]{cd::eligibility_build_command(decl,dspec,dline(wrong),dcommand,phase);});
   wrong=targets;std::swap(wrong[0],wrong[1]);ok=ok&&rejects([&]{cd::eligibility_build_command(decl,dspec,dline(wrong),dcommand,phase);});
   wrong=targets;wrong.push_back("undeclared_target");ok=ok&&rejects([&]{cd::eligibility_build_command(decl,dspec,dline(wrong),dcommand,phase);});
   if(split)ok=ok&&rejects([&]{cd::eligibility_build_command(decl,dspec,dline(targets),dcommand,1-phase);});
  }
  ok=ok&&rejects([&]{cd::eligibility_build_command(decl,dspec,dline(groups[0].at("targets")),dcommand,groups.size());});
  if(!split)continue;
  ok=ok&&rejects([&]{cd::eligibility_build_command(decl,dspec,dline(cd::eligibility_targets(true,flags[1],flags[2])),dcommand);});
  auto bad_groups=decl;std::swap(bad_groups["build_target_groups"][0],bad_groups["build_target_groups"][1]);ok=ok&&rejects([&]{cd::eligibility_build_command(bad_groups,dspec,dline(groups[0].at("targets")),dcommand);});
  J static_plan={{"build_target_groups",groups},{"expected_native_build_stages",2}};decl["expected_native_build_stages"]=2;
  decl["build_processes"]=J::array({cpu_process("performance-build0"),cpu_process("performance-build1")});decl["build_receipts"]=J::array();decl["build_input_closures"]=J::array();
  for(const auto& proof:decl["build_processes"]){decl["build_receipts"].push_back(proof.at("verification"));decl["build_input_closures"].push_back(proof.at("input_closure"));}
  decl["build_process"]=decl["build_processes"][1];decl["compile_receipt"]=decl["build_receipts"][1];decl["input_closure"]=decl["build_input_closures"][1];
  decl["translation_units"]=cd::eligibility_units(true,flags[1],flags[2]);decl["compiled_outputs"]=J::array();decl["files"]=J::array();J prior=J::array();std::size_t ordinal=0;
  const auto first=groups[0].at("targets");for(const auto& unit:decl["translation_units"])if(std::any_of(first.begin(),first.end(),[&](const auto& target){return target==unit.at("target");})){const auto pin=fixture_pin(dbuild/("CMakeFiles/"+unit.at("target").get<std::string>()+".dir/"+std::to_string(ordinal++)+".obj"));decl["compiled_outputs"].push_back(pin);prior.push_back(pin);}
  for(const auto& target:first){const auto name=target.get<std::string>();if(name=="game_motion_delta"||name=="game_delta"||name=="candidate_admission_delta")continue;const auto pin=fixture_pin(dbuild/(name+".exe"));decl["compiled_outputs"].push_back(pin);decl["files"].push_back(pin);prior.push_back(pin);}
  decl["build_prior_outputs"]=J::array({J::array(),prior});cd::eligibility_build_declaration(decl,static_plan);cd::eligibility_prior_products(decl);
  auto broken=decl;broken["build_process"]=broken["build_processes"][0];ok=ok&&rejects([&]{cd::eligibility_build_declaration(broken,static_plan);});
  broken=decl;broken["build_processes"][1]=broken["build_processes"][0];broken["build_receipts"][1]=broken["build_receipts"][0];ok=ok&&rejects([&]{cd::eligibility_build_declaration(broken,static_plan);});
  broken=decl;broken["build_processes"][0].erase("freeze");ok=ok&&rejects([&]{cd::eligibility_build_declaration(broken,static_plan);});
  broken=decl;broken["build_input_closures"][0]=broken["build_input_closures"][1];ok=ok&&rejects([&]{cd::eligibility_build_declaration(broken,static_plan);});
  broken=decl;broken["expected_native_build_stages"]=2.0;ok=ok&&rejects([&]{cd::eligibility_build_declaration(broken,static_plan);});
  auto future_plan=static_plan;future_plan["build_processes"]=decl["build_processes"];ok=ok&&rejects([&]{cd::eligibility_build_declaration(decl,future_plan);});
  broken=decl;broken["build_prior_outputs"][1].erase(std::size_t{0});ok=ok&&rejects([&]{cd::eligibility_prior_products(broken);});
  broken=decl;broken["build_prior_outputs"][1][1]=broken["build_prior_outputs"][1][0];ok=ok&&rejects([&]{cd::eligibility_prior_products(broken);});
  broken=decl;broken["build_prior_outputs"][1].push_back(fixture_pin(dbuild/".ninja_log"));ok=ok&&rejects([&]{cd::eligibility_prior_products(broken);});
  broken=decl;broken["build_prior_outputs"][1][0]["sha256"]=std::string(64,'0');ok=ok&&rejects([&]{cd::eligibility_prior_products(broken);});
 }
 dcheck("dense_original_member_source_and_separate12_denominator",ok);
 if(dcases.size()!=8)throw std::runtime_error("separate Dense reader8 denominator");report["dense_source_delta_controls"]={{"schema","pas.cold-dense-reader-controls.v1"},{"assertions",8},{"checks",dcases},{"failed",0},{"pass",true},{"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"device_operations",0}};
}
 std::ofstream f(output,std::ios::binary);f<<report.dump(2)<<'\n';if(!f)throw std::runtime_error("report write");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
