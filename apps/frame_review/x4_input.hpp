#pragma once
#include "x2_input.hpp"
#include "x4_oracle.hpp"
namespace pas::x4 {
inline void validate_parent(const json& m) {
    pas::x1::validate_comparison_manifest(m);
    if(m.value("experiment","")!="preconfirmation_role_oracle_x4"||m.at("batch_limit_bytes")!=25165824||
       !m.at("batch_limit_bytes").is_number_integer())throw std::runtime_error("x4_manifest_contract");
    const auto& p=m.at("parent");const auto path=p.at("manifest_path").get<std::string>();
    if(sha256_file(path)!=p.at("manifest_sha256").get<std::string>())throw std::runtime_error("x4_parent_SHA");
    const auto old=pas::review::load(path);x2::validate_manifest(old);
    for(const auto key:{"session_root","index_sha256","profile","profile_sha256","windows","assumptions","campaign_root","prior_research_root"})
        if(m.at(key)!=old.at(key))throw std::runtime_error(std::string("x4_input_bridge_")+key);
    for(const auto role:{"c36h_reference","main50_control"}) {
        const auto& binding=m.at("reused_runs").at(role);const auto root=binding.at("root").get<std::string>();
        for(const auto file:{"summary.json","trace.jsonl","events.jsonl","state-digests.jsonl"})
            if(!binding.at("files").contains(file))throw std::runtime_error("x4_missing_file_binding");
        if(std::string(role)=="main50_control"&&!binding.at("files").contains("first-intervention.jsonl"))throw std::runtime_error("x4_missing_G_binding");
        for(const auto& [file,hash]:binding.at("files").items())
            if(sha256_file(std::filesystem::path(root)/file)!=hash.get<std::string>())throw std::runtime_error("x4_reused_artifact_SHA");
        const auto s=pas::review::load(std::filesystem::path(root)/"summary.json");
        x2::validate_binding(old,s,p.at("manifest_sha256"),role);
        if(s.at("success")!=true)throw std::runtime_error("x4_failed_parent_run");
        for(const auto& [key,value]:std::array<std::pair<const char*,const char*>,4>{{{"cadence","owner"},{"tie","frame-first"},{"recognition","zero_fake_time"},{"receipt_policy","success_zero_duration_five_contacts"}}})
            if(s.value(key,"")!=value)throw std::runtime_error("x4_parent_policy");
    }
    for(const auto key:{"acceptance_path","x3_acceptance_path"})
        if(sha256_file(p.at(key).get<std::string>())!=p.at(std::string(key)+"_sha256").get<std::string>())throw std::runtime_error("x4_acceptance_SHA");
}
inline void validate_variant_binding(const json& m,const json& s,const std::string& manifest_sha) {
    if(s.value("role","")!="main50_preconfirmation_role_oracle"||s.value("variant","")!="proposed_role_winner_only"||s.value("lineage","")!="main50")throw std::runtime_error("x4_variant_role");
    if(s.value("input_manifest_sha256","")!=manifest_sha)throw std::runtime_error("x4_manifest_SHA");
    const auto& r=m.at("variant_source");
    for(const auto key:{"binary_sha256","source_provenance_sha256","tracking_source_sha256"})
        if(s.at(key)!=r.at(key))throw std::runtime_error(std::string("x4_variant_binding_")+key);
    if(sha256_file(r.at("binary_path").get<std::string>())!=r.at("binary_sha256").get<std::string>()||
       sha256_file(r.at("source_provenance_path").get<std::string>())!=r.at("source_provenance_sha256").get<std::string>())throw std::runtime_error("x4_source_SHA");
    const auto provenance=pas::review::load(r.at("source_provenance_path").get<std::string>());
    for(const auto key:{"role","variant","lineage"})if(provenance.at(key)!=s.at(key))throw std::runtime_error("x4_provenance_role");
    if(provenance.at("instrumented_source_sha256").at("src/game_tracking.cpp")!=r.at("tracking_source_sha256")||
       sha256_file(std::filesystem::path(provenance.at("export_root").get<std::string>())/"src/game_tracking.cpp")!=r.at("tracking_source_sha256").get<std::string>())throw std::runtime_error("x4_tracking_SHA");
}
inline void load_packets(const json& m,const std::vector<json>& index) {
    const auto& o=m.at("oracle");if(sha256_file(o.at("path").get<std::string>())!=o.at("sha256").get<std::string>())throw std::runtime_error("x4_oracle_SHA");
    const auto data=pas::review::load(o.at("path").get<std::string>());
    if(!data.at("schema").is_number_integer()||data.at("schema")!=1||data.value("grade","")!="proposed"||data.at("packets").size()!=63)throw std::runtime_error("x4_oracle_contract");
    reset();
    for(const auto& row:data.at("packets")) {
        const auto n=pas::x1::comparison_ordinal(row,"ordinal");
        if(n<6158||n>6220||packets.contains(n)||index.at(n).at("source_frame")!=row.at("source_frame")||index.at(n).at("png_sha256")!=row.at("png_sha256"))throw std::runtime_error("x4_packet_index_binding");
        for(const auto key:{"core","line"})if(!row.at(key).is_null()) {
            const bool is_note=std::string(key)=="core";
            for(const auto field:is_note?std::vector<const char*>{"x","y","width","height","ux","uy"}:std::vector<const char*>{"x","y","length","ux","uy"})
                if(!row.at(key).at(field).is_number()||!std::isfinite(row.at(key).at(field).get<double>()))throw std::runtime_error("x4_packet_geometry");
            if(is_note&&row.at(key).value("kind","")!="drag")throw std::runtime_error("x4_packet_non_Drag");
            if(!is_note&&std::abs(row.at(key).at("uy").get<double>())<.97)throw std::runtime_error("x4_packet_nonvertical");
        }
        packets.emplace(n,row);
    }
    enabled=true;
}
}
