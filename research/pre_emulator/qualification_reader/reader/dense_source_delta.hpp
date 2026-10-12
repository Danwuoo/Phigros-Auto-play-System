#pragma once
// Exact optional Dense source/member/regression declaration.
// Historical motion and original numerical controls remain separate.
namespace pas::preflight_graph::component_delta {
inline bool dense_delta(const J& d) {
    if(!d.contains("dense_source_delta"))return false;
    w::require(d.at("dense_source_delta").is_object(),"explicit Dense overlay object");
    return true;
}
inline J dense_consumers(){return J::array({"current_chain","production_pipeline_cold","score_only_controls","pas","candidate_admission_controls","motion_source_regression"});}
inline constexpr const char* dense_old_note_anchor=R"(    // Finish the current line set before interpreting note ribbons. Source
    // component order changes when a rotating line is interrupted by a note.
    // A note's local dimensions must not depend on which fragment came first.
    for(const auto& c:all) {
        const double w=c.x1-c.x0+2.0,h=c.y1-c.y0+2.0;)";
inline constexpr const char* dense_new_note_anchor=R"(    // Finish the current line set before interpreting note ribbons. Source
    // component order changes when a rotating line is interrupted by a note.
    // A note's local dimensions must not depend on which fragment came first.
    for(const auto& c:all) {
        if(c.color==4) continue;
        const double w=c.x1-c.x0+2.0,h=c.y1-c.y0+2.0;)";
inline void dense_source_equivalence(const std::string& base,const std::string& current){
    // Preserve the actual old game.cpp's BOM and LF; do not reuse motion_lf(),
    // which intentionally requires BOM-free game_motion.cpp.
    w::require(base.size()<=4*w::MiB&&current.size()<=4*w::MiB&&base.starts_with("\xef\xbb\xbf")&&current.starts_with("\xef\xbb\xbf")&&base.find('\r')==std::string::npos&&current.find('\r')==std::string::npos,"exact BOM/LF game source encoding");
    const std::string old=dense_old_note_anchor,fresh=dense_new_note_anchor;
    auto expected=base;const auto at=expected.find(old);
    w::require(at!=std::string::npos&&expected.find(old,at+old.size())==std::string::npos,"one exact second-pass white anchor");
    expected.replace(at,old.size(),fresh);w::require(expected==current,"sole white note-PCA short circuit; all other bytes retained");
    auto inverse=current;const auto back=inverse.find(fresh);
    w::require(back!=std::string::npos&&inverse.find(fresh,back+fresh.size())==std::string::npos,"one exact Dense replacement anchor");
    inverse.replace(back,fresh.size(),old);w::require(inverse==base,"Dense inverse restores exact base source payload");
}
inline J dense_case_names(){return J::array({
 "GameObserver.MultiplePixelsTracksAndRelativeMovingLinePrediction",
 "GameObserver.CrossingCurrentVerticalRidgeSurvivesConnectedHorizontalLine",
 "GameObserver.CurrentVerticalSearchRejectsHoldSideRailsAndBroadWhiteFill",
 "GameObserver.CrossingVerticalWhiteRidgeSurvivesCurrentWarmHitTintButCannotBeBornFromTint",
 "GameObserver.SplitsPixelConnectedCrossingRidgesWithoutInventingHoldRails",
 "GameObserver.UiLossCancelsAndCapacityIsBounded",
 "GameObserver.OnePixelLineJoinedToHoldBorderStaysObservable",
 "GameObserver.NestedRibbonsMergeButEqualWidthOverlapsRemainDistinct",
 "GameObserver.YellowOutlineDoesNotDuplicateBlueCoreAndHoldHasLeadingEdgeAndTail",
 "GameObserver.SmallHitParticlesAndYellowRingsCannotBecomeNoteTargets",
 "GameObserver.InterruptedThinBandIsOneLineButSeparatedBandsRemainDistinct",
 "GameObserver.PerpendicularColoredRibbonsKeepTheirOwnGeometryAndKind"});}
inline std::string dense_filter(){std::string out;for(const auto& name:dense_case_names()){if(!out.empty())out+=':';out+=name.get<std::string>();}return out;}
inline void dense_static(const J& m,const J& d){
    w::require(m.at("schema")=="pas.cold-dense-source-delta.v1"&&m.at("source_commit")==d.at("source_commit")&&m.at("base_source_commit")==eligibility_base_commit&&m.at("source_path")=="src/game.cpp"&&m.at("literal_transform")=="white_note_decode_pca_short_circuit.v1","literal Dense overlay identity");
    eligibility_scope(m);eligibility_bool(m,"formal_libraries_rebuilt",false);
    w::require(eligibility_uint(m,"fresh_formal_translation_units")==1&&m.at("consumer_targets")==dense_consumers(),"one game formal TU and exact six consumers");
    for(const auto* key:{"base_source","current_source","compile_source"})motion_pin_shape(m.at(key));
    w::require(m.at("current_source").at("bytes")==m.at("compile_source").at("bytes")&&m.at("current_source").at("sha256")==m.at("compile_source").at("sha256"),"game short compile copy exact current bytes");
    std::set<std::string> roles;const auto& members=m.at("original_archive_members");
    w::require(members.is_array()&&members.size()==2,"two original game archive members");
    for(const auto& member:members){w::require(roles.insert(member.at("role").get<std::string>()).second,"unique original game role");for(const auto* key:{"archive","member","source"})motion_pin_shape(member.at(key));w::require(member.at("source").at("bytes")==m.at("base_source").at("bytes")&&member.at("source").at("sha256")==m.at("base_source").at("sha256"),"old game member source equals preserved copy bytes");}
    w::require(roles==std::set<std::string>{"cpu_pas_core","production_pas_core"},"both old core22 archives preserved");
    const auto& reg=m.at("regression");w::require(reg.at("target")=="motion_source_regression"&&eligibility_uint(reg,"cases")==12&&reg.at("case_names")==dense_case_names()&&reg.at("filter")==dense_filter(),"separate exact twelve original observer cases");
    w::require(reg.at("test_sources").is_array()&&reg.at("test_sources").size()==2,"two unchanged shared regression source TUs");
    for(const auto& pin:reg.at("test_sources"))motion_pin_shape(pin);
    for(const auto* key:{"runtime_contract_source","runtime_contract_prefix"})motion_pin_shape(reg.at(key));
}
inline void dense_declaration(const J& d,const J& plan){
    w::require(dense_delta(d)==dense_delta(plan),"Dense plan/build branch agrees");if(!dense_delta(d))return;
    w::require(motion_delta(d)&&motion_delta(plan),"Dense branch preserves the complete motion overlay");
    const auto& a=d.at("dense_source_delta");const auto& b=plan.at("dense_source_delta");dense_static(a,d);dense_static(b,plan);
    for(const auto* key:{"schema","source_commit","base_source_commit","source_path","literal_transform","base_source","current_source","compile_source","original_archive_members","consumer_targets","formal_libraries_rebuilt","fresh_formal_translation_units"})w::require(a.at(key)==b.at(key),"Dense immutable static plan/build join");
    for(const auto* key:{"target","test_sources","runtime_contract_source","runtime_contract_prefix","cases","case_names","filter"})w::require(a.at("regression").at(key)==b.at("regression").at(key),"Dense regression immutable static join");
}
inline void dense_regression_report(const J& report){
    w::require(eligibility_uint(report,"tests")==12&&eligibility_uint(report,"failures")==0&&eligibility_uint(report,"disabled")==0&&eligibility_uint(report,"errors")==0&&report.at("testsuites").is_array()&&report.at("testsuites").size()<=12,"actual separate twelve GTest denominator");
    std::set<std::string> names,expected;std::size_t total=0;
    for(const auto& suite:report.at("testsuites")){w::require(eligibility_uint(suite,"failures")==0&&eligibility_uint(suite,"disabled")==0&&eligibility_uint(suite,"errors")==0&&suite.at("testsuite").is_array()&&suite.at("testsuite").size()==eligibility_uint(suite,"tests"),"actual Dense suite denominator");for(const auto& row:suite.at("testsuite")){const auto name=suite.at("name").get<std::string>()+"."+row.at("name").get<std::string>();w::require(row.at("status")=="RUN"&&row.at("result")=="COMPLETED"&&names.insert(name).second,"unique executed Dense case");for(const auto* key:{"failures","errors"})if(row.contains(key))w::require(row.at(key).is_array()&&row.at(key).empty(),"Dense case failures/errors absent");++total;}}
    for(const auto& name:dense_case_names())expected.insert(name.get<std::string>());
    w::require(total==12&&names==expected,"all and only exact twelve actual observer cases");
}
inline J dense_replacement_map(){return J::array({J{{"source","src/game_motion.cpp"},{"target","game_motion_delta"}},J{{"source","src/game.cpp"},{"target","game_delta"}}});}
inline void dense_archive_join(const J& m,const J& d,const J& old){
    for(const auto& member:m.at("original_archive_members")){const J* library=nullptr;for(const auto& row:d.at("imported_static_libraries"))if(row.at("role")==member.at("role")){w::require(!library,"unique original game archive role");library=&row;}
        w::require(library&&eligibility_uint(*library,"translation_unit_count")==22&&pin_equal(library->at("binary"),member.at("archive")),"preserved original22 game archive binary");
        std::size_t found=0;for(const auto& unit:library->at("translation_units"))if(unit.at("relative_source")=="src/game.cpp"){++found;w::require(pin_equal(unit.at("historical_object"),member.at("member"))&&pin_equal(unit.at("original_source"),member.at("source"))&&pin_equal(member.at("source"),old),"original game tuple and source pin unchanged");}
        w::require(found==1&&library->at("replaced_archive_sources")==J::array({"src/game_motion.cpp","src/game.cpp"})&&library->at("replacement_targets")==dense_replacement_map()&&!library->contains("replacement_target"),"exact two member replacement mapping; no ambiguous scalar");
    }
}
// Call ONLY after complete original motion_validate and its source/runtime/native
// checks have passed. It reuses the unchanged regression test/runtime declarations.
inline bool dense_validate(Reader& r,const J& d,bool motion_validated){
    if(!dense_delta(d))return false;w::require(motion_validated,"Dense requires validated motion overlay");
    const auto& m=d.at("dense_source_delta");dense_static(m,d);
    const auto manifest=r.object(d.at("source_manifest")),cpu=r.object(d.at("base_cpu_build")),base_manifest=r.object(cpu.at("source_manifest")),snapshot=r.object(d.at("base_source_snapshot")),projection=r.object(d.at("base_source_projection"));
    const auto old=eligibility_manifest_pin(base_manifest,"src/game.cpp"),current=eligibility_manifest_pin(manifest,"src/game.cpp");
    w::require(pin_equal(current,m.at("current_source"))&&old.at("bytes")==m.at("base_source").at("bytes")&&old.at("sha256")==m.at("base_source").at("sha256"),"exact base55/current game manifest identities");
    const J* original_copy=nullptr;for(const auto& row:snapshot.at("files"))if(pin_equal(row.at("original"),old)){w::require(!original_copy,"unique old game snapshot source");original_copy=&row.at("copy");}w::require(original_copy,"old game payload preserved");
    bool copied=pin_equal(m.at("base_source"),*original_copy);for(const auto& row:projection.at("files"))if(row.at("relative_path")=="src/game.cpp"){w::require(pin_equal(row.at("original_copy"),*original_copy)&&row.at("copy").at("bytes")==old.at("bytes")&&row.at("copy").at("sha256")==old.at("sha256"),"game short copy joins original snapshot");copied|=pin_equal(m.at("base_source"),row.at("copy"));}w::require(copied,"exact old game payload uses preserved snapshot/projection copy");
    const auto read=[&](const J& pin){w::require(eligibility_uint(pin,"bytes")<=4*w::MiB,"bounded game source text");std::ifstream f(r.bound(pin),std::ios::binary);std::ostringstream out;out<<f.rdbuf();w::require(bool(f)&&out.str().size()==eligibility_uint(pin,"bytes"),"whole game payload read");return out.str();};
    dense_source_equivalence(read(m.at("base_source")),read(current));w::require(read(m.at("compile_source"))==read(current),"actual compile copy is exact literal game candidate");
    dense_archive_join(m,d,old);for(const auto& member:m.at("original_archive_members")){r.bound(member.at("archive"));r.bound(member.at("member"));}
    std::size_t occurrences=0;for(const auto& unit:d.at("translation_units"))if(unit.at("target")=="game_delta"){++occurrences;w::require(unit.at("relative_source")=="src/game.cpp"&&pin_equal(unit.at("original_source"),current)&&pin_equal(unit.at("source"),m.at("compile_source")),"actual unique game compile occurrence");}w::require(occurrences==1,"exactly one fresh game source");
    const auto& reg=m.at("regression");const auto& motion_reg=d.at("motion_source_delta").at("regression");for(const auto* key:{"test_sources","runtime_contract_source","runtime_contract_prefix","binary"})w::require(reg.at(key)==motion_reg.at(key),"Dense uses the same freshly linked and validated original test/runtime inputs");
    w::require(pin_equal(reg.at("binary"),eligibility_image(d,"motion_source_regression.exe")),"exact shared Dense regression image");
    const auto argv=J::array({"--gtest_filter="+dense_filter(),"--gtest_output=json:"+reg.at("report").at("path").get<std::string>()});
    const auto report=eligibility_control_native(r,d,reg.at("process"),reg.at("binary"),reg.at("report"),argv);const auto inputs=r.process(reg.at("process"));
    for(const auto& pin:{m.at("base_source"),m.at("current_source"),m.at("compile_source"),reg.at("runtime_contract_prefix"),reg.at("runtime_contract_source")})w::require(inputs.contains(r.bound(pin)),"Dense regression Native binds old/current/compile/runtime payloads");
    dense_regression_report(report);return true;
}
}
