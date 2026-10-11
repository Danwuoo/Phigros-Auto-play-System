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
 std::ofstream f(output,std::ios::binary);f<<report.dump(2)<<'\n';if(!f)throw std::runtime_error("report write");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
