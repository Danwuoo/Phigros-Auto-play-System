#pragma once
#include "graph_reader.hpp"
#include <array>

namespace pas::preflight_graph {
inline void verify_prefix(Reader& r,const J& pin,const std::string& commit,const J& candidate_binary,
                          const J& auditor_binary,const J& producer,const J& auditor){
    const auto p=r.object(pin);
    w::require(p.at("schema")=="pas.cooperative-prefix-receipt.v1"&&p.at("source_commit")==commit&&
        p.at("pass").get<bool>()&&p.at("frames")==3063&&p.at("physical_gold")==0&&
        p.at("device_operations")==0&&!p.at("live_ready").get<bool>()&&p.at("binary")==candidate_binary,
        "same complete 3063-frame prefix");
    r.bound(candidate_binary);
    const auto source=r.object(p.at("source_manifest"));
    w::require(source.at("schema")=="pas.current-source-closure.v1"&&source.at("source_commit")==commit&&
        !source.at("dirty").get<bool>(),"prefix source closure identity");
    r.entries(source.at("files"));
    const auto selection=r.object(p.at("selection"),8*w::MiB);
    w::require(selection.at("frames").is_array()&&selection.at("frames").size()==3063,"entire original prefix selection");
    r.entries(selection.at("frames"));
    const auto report=r.object(p.at("report")),audit=r.object(p.at("audit"));
    w::require(report.at("schema")=="pas.prelive-current-pixels.v1"&&report.at("frames")==3063&&
        report.at("processed")==3063&&report.at("release_verified").get<bool>()&&
        report.at("stop")==""&&report.at("device_endpoints")==0&&report.at("physical_gold")==0,
        "all prefix frames processed with verified release");
    w::require(audit.at("schema")=="pas.prelive-independent-integrity.v2"&&audit.at("integrity").get<bool>()&&
        audit.at("complete_prefix_evidence").get<bool>()&&audit.at("all_input_processed").get<bool>()&&
        audit.at("metrics_verified").get<bool>()&&audit.at("plan_join_complete").get<bool>()&&
        audit.at("coordinate_join_complete").get<bool>()&&audit.at("provenance_join_complete").get<bool>()&&
        !audit.at("declared_writer_loss").get<bool>()&&!audit.at("release_uncertainty_seen").get<bool>()&&
        audit.at("errors").empty()&&audit.at("unknown_release").empty()&&audit.at("device_endpoints")==0&&
        audit.at("verifier_binary").at("sha256")==auditor_binary.at("sha256"),"complete independent prefix joins");
    w::require(producer.at("verification")==p.at("producer_receipt")&&
        auditor.at("verification")==p.at("auditor_receipt"),"original prefix native receipts");
    const auto producer_inputs=r.process(producer),auditor_inputs=r.process(auditor);
    const auto ps=r.object(producer.at("specification")),as=r.object(auditor.at("specification"));
    w::require(final_path(ps.at("exe").get<std::string>())==r.bound(candidate_binary)&&
        ps.at("argv").size()==3&&ps.at("argv")[0]=="pixels"&&
        final_path(ps.at("argv")[1].get<std::string>())==r.bound(p.at("selection"))&&
        final_path(ps.at("argv")[2].get<std::string>())==r.bound(p.at("report")),"actual full-prefix producer command");
    w::require(final_path(as.at("exe").get<std::string>())==r.bound(auditor_binary)&&
        as.at("argv").size()==2&&final_path(as.at("argv")[0].get<std::string>())==r.bound(p.at("report"))&&
        final_path(as.at("argv")[1].get<std::string>())==r.bound(p.at("audit")),"actual full-prefix independent auditor command");
    w::require(producer_inputs.contains(r.bound(p.at("selection")))&&
        producer_inputs.contains(r.bound(p.at("source_manifest"))),"prefix source and selection frozen before launch");
    for(const auto& f:source.at("files"))w::require(producer_inputs.contains(r.bound(f)),"entire prefix source input closure");
    for(const auto& f:selection.at("frames"))w::require(producer_inputs.contains(r.bound(f)),"every selected frame frozen before launch");
    const auto& raw=audit.at("input_bindings");
    for(const auto& [key,suffix]:std::array<std::pair<const char*,const char*>,3>{{{"report",""},{"rows",".rows.jsonl"},{"events",".events.jsonl"}}})
        w::require(r.bound(raw.at(key))==final_path(r.bound(p.at("report")).string()+suffix)&&
            auditor_inputs.contains(r.bound(raw.at(key))),"all full-prefix raw files in auditor closure");
}
}

