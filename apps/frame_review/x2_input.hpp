#pragma once
#include "comparison_input.hpp"
#include "pas/analysis.hpp"
namespace pas::x2 {
using pas::review::json;
inline constexpr std::array<const char*,3> roles{{"c36h_reference","main50_control","main50_no_confirmed_winner_override"}};
inline const char* variant(const std::string& role) {
    if(role==roles[0])return "c36h_unmodified_reference";
    if(role==roles[1])return "winner_override_enabled";
    if(role==roles[2])return "winner_override_disabled_flag_preserved";
    throw std::runtime_error("x2_unknown_role");
}
inline void validate_manifest(const json& m) {
    pas::x1::validate_comparison_manifest(m);
    if(m.value("experiment","")!="confirmed_preserve_winner_only_x2"||
       !m.contains("batch_limit_bytes")||!m.at("batch_limit_bytes").is_number_integer()||
       m.at("batch_limit_bytes")!=67108864||!m.contains("roles")||m.at("roles").size()!=3)
        throw std::runtime_error("x2_manifest_contract");
    for(const auto role:roles) {
        if(!m.at("roles").contains(role))throw std::runtime_error("x2_manifest_roles");
        const auto& r=m.at("roles").at(role);
        if(r.value("variant","")!=variant(role)||r.value("lineage","")!=(role==roles[0]?"c36h":"main50"))
            throw std::runtime_error("x2_manifest_variant");
        for(const auto key:{"binary_sha256","source_provenance_sha256","tracking_source_sha256"})
            if(!r.contains(key)||!pas::x1::comparison_sha(r.at(key)))throw std::runtime_error("x2_manifest_source");
        for(const auto key:{"binary_path","source_provenance_path"})
            if(!r.contains(key)||!r.at(key).is_string())throw std::runtime_error("x2_manifest_source");
    }
}
inline void validate_binding(const json& m,const json& s,const std::string& manifest_sha,const char* role) {
    if(s.value("role","")!=role)throw std::runtime_error(std::string("x2_role_expected_")+role);
    if(s.value("input_manifest_sha256","")!=manifest_sha)throw std::runtime_error("x2_manifest_SHA_mismatch");
    const auto& r=m.at("roles").at(role);
    for(const auto key:{"lineage","variant","binary_sha256","source_provenance_sha256","tracking_source_sha256"})
        if(!s.contains(key)||s.at(key)!=r.at(key))throw std::runtime_error(std::string("x2_run_binding_")+key);
    const auto p=r.at("source_provenance_path").get<std::string>();
    if(std::filesystem::file_size(p)>2*1024*1024||pas::sha256_file(p)!=r.at("source_provenance_sha256").get<std::string>())
        throw std::runtime_error("x2_provenance_SHA");
    const auto provenance=pas::review::load(p);
    if(provenance.value("role","")!=role||provenance.at("variant")!=r.at("variant")||
       provenance.at("lineage")!=r.at("lineage")||
       provenance.at("instrumented_source_sha256").at("src/game_tracking.cpp")!=r.at("tracking_source_sha256"))
        throw std::runtime_error("x2_provenance_role_variant_source");
    if(pas::sha256_file(r.at("binary_path").get<std::string>())!=r.at("binary_sha256").get<std::string>())
        throw std::runtime_error("x2_binary_SHA");
}
inline void validate_runs(const json& m,const std::string& sha,const std::array<json,3>& s) {
    validate_manifest(m);
    for(std::size_t i=0;i<3;++i) {
        if(!s[i].contains("success")||s[i].at("success")!=true)throw std::runtime_error("x2_failed_run");
        validate_binding(m,s[i],sha,roles[i]);
        for(const auto& [key,value]:std::array<std::pair<const char*,const char*>,4>{{
            {"cadence","owner"},{"tie","frame-first"},{"recognition","zero_fake_time"},
            {"receipt_policy","success_zero_duration_five_contacts"}}})
            if(s[i].value(key,"")!=value)throw std::runtime_error("x2_policy_mismatch");
    }
}
}
