#pragma once
#include "graph_reader.hpp"

namespace pas::preflight_graph {
inline std::set<std::string> cold_tests() {
    return {"diagnostic_writer_tests", "owner_checkpoint_tests", "owner_expiry_tests",
        "producer_wait_tests", "tap_reset_workload_tests", "owner_wait_tests", "current_tests",
        "active_body_tests", "tail_guard_tests", "tail_only_owner_review", "tail_only_pixels_review",
        "disabled_live_seam_tests", "live_seam_packet_review", "live_seam_runtime_review",
        "runtime_finish_tests", "cost_comparison_tests", "production_pipeline_audit_tests",
        "production_entry_controls", "cost_analysis_tests", "cost_analysis_review_tests",
        "qualification_reader_controls", "score_only_tap", "score_only_hold", "score_only_dense", "score_only_invalid"};
}
// Parameters are externally bound existing receipts. No new admission format.
// Each stage carries the original ProcessEvidence bindings, including the fresh
// post-exit input closure; a native exit0 alone is deliberately insufficient.
inline void verify_cooperative_build(Reader& r, const J& evaluation, const std::string& commit,
                                     const std::string& mode, const J& stages) {
    w::require(w::hex(commit,40) && (mode=="release" || mode=="debug" || mode=="asan"), "build source/mode pin");
    const auto e=r.object(evaluation);
    w::require(e.at("schema")=="pas.cooperative-cold-evaluation.v1" && e.at("source_commit")==commit &&
        e.at("affected_cold_pass").get<bool>() && !e.at("live_ready").get<bool>() && e.at("device_operations")==0 &&
        e.at("tests").is_array() && e.at("tests").size()==25, "complete cooperative cold evaluation");
    const auto b=r.object(e.at("build"));
    w::require(b.at("schema")=="pas.cooperative-build.v1" && b.at("source_commit")==commit &&
        b.at("configuration")==mode && !b.at("live_ready").get<bool>() && b.at("device_operations")==0,
        "same cooperative build identity");
    const auto source=r.object(b.at("source_manifest"));
    w::require(source.at("schema")=="pas.cooperative-source.v1" && source.at("source_commit")==commit &&
        !source.at("dirty").get<bool>() && !source.at("live_ready").get<bool>() && source.at("device_operations")==0,
        "clean complete source manifest");
    r.entries(source.at("files"));
    const auto deps=r.object(b.at("dependency_manifest"));
    w::require(deps.at("schema")=="pas.pre-emulator-dependency-closure.v1" && deps.at("source_commit")==commit &&
        deps.at("release_and_debug").get<bool>() && deps.at("protoc_and_plugin").get<bool>() &&
        deps.at("device_operations")==0 && !deps.at("live_ready").get<bool>(), "complete installed dependency scope");
    r.installed_dependencies(deps.at("files"),deps.at("root").get<std::string>());
    const auto plan=r.object(b.at("core_build_plan"));
    w::require(plan.at("schema")=="pas.fresh-core-build-plan.v1" && plan.at("source_commit")==commit &&
        plan.at("configuration")==mode && plan.at("translation_units")==22 &&
        !plan.at("old_donor_objects_used").get<bool>() && plan.at("device_operations")==0 &&
        !plan.at("live_ready").get<bool>(), "fresh 22-TU build plan");
    const auto tool=r.object(plan.at("compiler_snapshot"));
    w::require(tool.at("files").is_array() && !tool.at("files").empty() && tool.at("files").size()<=256,
        "compiler snapshot bound");
    for(const auto& f:tool.at("files"))
        r.bound(J{{"path",f.at("copy")},{"bytes",f.at("bytes")},{"sha256",f.at("sha256")}});
    r.entries(b.at("files"));
    const auto build_root=final_path(b.at("build_root").get<std::string>());
    std::set<std::string> objects,executables;
    for(const auto& f:b.at("files")) {
        const auto p=r.bound(f),rel=p.lexically_relative(build_root);
        w::require(!rel.empty() && !rel.is_absolute() && *rel.begin()!="..", "build product containment");
        const auto name=rel.generic_string();
        if(name.starts_with("production/CMakeFiles/pas_core.dir/") && name.find("/src/")!=std::string::npos && name.ends_with(".cpp.obj"))
            w::require(objects.insert(p.filename().string()).second,"duplicate formal core object");
        if(p.extension()==".exe") executables.insert(p.filename().string());
    }
    const std::set<std::string> formal={"core.cpp.obj","config.cpp.obj","analysis.cpp.obj","adb.cpp.obj",
        "journal.cpp.obj","touch_evidence.cpp.obj","preview.cpp.obj","game.cpp.obj","game_clip_replay.cpp.obj",
        "game_cold_pipeline.cpp.obj","game_cold_corpus.cpp.obj","game_motion.cpp.obj","game_tracking.cpp.obj",
        "game_tracking_methods.cpp.obj","game_tracking_analysis.cpp.obj","game_tracking_shadow.cpp.obj",
        "game_dataset.cpp.obj","game_dataset_tools.cpp.obj","game_session.cpp.obj","session_archive.cpp.obj",
        "session_pixel_clips.cpp.obj","session_recording.cpp.obj"};
    w::require(objects==formal,"complete fresh formal object set");
    for(const auto* exe:{"pas.exe","current_chain.exe","prelive_audit.exe","cost_analysis.exe",
        "cost_comparison.exe","production_pipeline_cold.exe","production_pipeline_audit.exe","fault_controls.exe"})
        w::require(executables.contains(exe),"required complete production/cold executable");
    std::map<fs::path,J> expected;
    const auto add=[&](const J& pin){w::require(expected.emplace(r.bound(pin),pin).second,"duplicate build/test receipt");};
    add(b.at("configure_receipt"));
    w::require(b.at("build_receipts").is_array() && !b.at("build_receipts").empty() &&
        b.at("build_receipts").size()<=64,"finite build group set");
    for(const auto& pin:b.at("build_receipts")) add(pin);
    auto tests=cold_tests();
    for(const auto& t:e.at("tests")) {
        const auto id=t.at("test").get<std::string>();
        w::require(tests.erase(id)==1,"exact cold test set");
        const auto report=r.object(t.at("report"),4*w::MiB);
        if(id=="cost_analysis_tests" || id=="cost_analysis_review_tests")
            w::require(report.at("pass").get<bool>(),"cold cost controls failed");
        else w::require(report.at("failed")==0,"cold functional controls failed");
        add(t.at("receipt"));
    }
    w::require(tests.empty() && stages.is_array() && stages.size()==expected.size(),"all build/test process evidence required");
    for(const auto& p:stages) {
        const auto vp=r.bound(p.at("verification"));
        const auto it=expected.find(vp);
        w::require(it!=expected.end() && it->second==p.at("verification"),"exact expected native verification");
        const auto v=r.owned(p.at("verification"),0,0,true);
        for(const auto* key:{"specification","input_closure","result","freeze","state","contract","command"})
            r.bound(p.at(key));
        r.process(p);
        expected.erase(it);
    }
    w::require(expected.empty(),"missing verified build/test stage");
    if(!b.at("serial_symbol_archive").is_null()){
        const auto proof=r.object(b.at("serial_symbol_archive"));
        w::require(proof.at("schema")=="pas.serial-final-symbol-archive.v1"&&
            proof.at("source_commit")==commit&&proof.at("readback_verified").get<bool>()&&
            proof.at("serial_before_build_seal").get<bool>()&&proof.at("device_operations")==0&&
            !proof.at("live_ready").get<bool>()&&fs::path(proof.at("root").get<std::string>())==build_root&&
            proof.at("final_stage_receipt")==b.at("build_receipts").back(),"serial final-symbol archive provenance");
        r.bound(proof.at("archive"));
        w::require(proof.at("files").is_array()&&!proof.at("files").empty()&&proof.at("files").size()<=64,
            "finite archived final-symbol set");
        std::set<std::string> symbols;std::uint64_t symbol_bytes=0;
        for(const auto& f:proof.at("files")){
            const fs::path relative(f.at("relative").get<std::string>());
            w::require(!relative.empty()&&!relative.is_absolute()&&relative.extension()==".pdb"&&
                symbols.insert(relative.generic_string()).second&&w::hex(f.at("sha256").get<std::string>(),64)&&
                f.at("bytes").is_number_unsigned()&&f.at("bytes").get<std::uint64_t>()<=1024ull*w::MiB,
                "bounded unique archived final symbol identity");
            const auto size=f.at("bytes").get<std::uint64_t>();w::require(size<=3ull*1024*w::MiB-symbol_bytes,"finite compiler symbol archive decoded sum");symbol_bytes+=size;
            for(const auto& part:relative)w::require(part!="."&&part!="..","archived symbol traversal");
            w::require(fs::path(f.at("path").get<std::string>())==build_root/relative,"archived symbol original path");
        }
    }else w::require(mode=="release","Debug/ASan final-symbol archive required");
}
}




