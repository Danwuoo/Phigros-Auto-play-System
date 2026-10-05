#pragma once
#include "review_io.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <array>
#include <set>
#include <string_view>

namespace pas::x1 {
using pas::review::json;
namespace fs=std::filesystem;

inline bool comparison_sha(const json& value) {
    if(!value.is_string())return false;
    const auto& s=value.get_ref<const std::string&>();
    return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');
    });
}
inline std::size_t comparison_ordinal(const json& w,const char* key) {
    if(!w.contains(key)||!w.at(key).is_number_integer()||
       w.at(key)<0||w.at(key)>=36000)throw std::runtime_error("comparison_manifest_windows");
    return w.at(key).get<std::size_t>();
}
inline void validate_comparison_manifest(const json& manifest) {
    if(!manifest.is_object()||!manifest.contains("schema")||
       !manifest.at("schema").is_number_integer()||manifest.at("schema")!=1)
        throw std::runtime_error("comparison_manifest_schema");
    if(!manifest.contains("windows")||!manifest.at("windows").is_array()||
       manifest.at("windows").size()!=5)throw std::runtime_error("comparison_manifest_windows");
    const std::array<std::pair<std::string_view,std::size_t>,5> cases{{
        {"A3498",3498},{"B4986",4986},{"C5520",5520},{"D6214",6214},{"E5287",5287}}};
    std::set<std::string> seen;
    for(const auto& w:manifest.at("windows")) {
        if(!w.is_object()||!w.contains("id")||!w.at("id").is_string())
            throw std::runtime_error("comparison_manifest_windows");
        const auto id=w.at("id").get<std::string>();
        const auto known=std::find_if(cases.begin(),cases.end(),[&](const auto& c){return c.first==id;});
        const auto first=comparison_ordinal(w,"first"),last=comparison_ordinal(w,"last"),anchor=comparison_ordinal(w,"anchor");
        if(known==cases.end()||!seen.insert(id).second||anchor!=known->second||
           last<first||last-first>=120||anchor<first||anchor>last)
            throw std::runtime_error("comparison_manifest_windows");
    }
    for(const auto key:{"index_sha256","profile_sha256"})
        if(!manifest.contains(key)||!comparison_sha(manifest.at(key)))
            throw std::runtime_error("comparison_manifest_source");
    if(!manifest.contains("batch_root")||!manifest.at("batch_root").is_string()||
       manifest.at("batch_root").get<std::string>().empty())
        throw std::runtime_error("comparison_manifest_source");
}
inline void validate_comparison_role(const json& summary,const json& manifest,const char* role) {
    if(!summary.contains("lineage")||!summary.at("lineage").is_string()||summary.at("lineage")!=role)
        throw std::runtime_error(std::string("comparison_lineage_expected_")+role);
    for(const auto key:{"binary_sha256","source_provenance_sha256"})
        if(!summary.contains(key)||!comparison_sha(summary.at(key)))
            throw std::runtime_error(std::string("comparison_run_source_missing_")+role);
    // These are the two existing X1 preparation layouts, not a new registry.
    // Match the recorded source hash before trusting the provenance's lineage.
    const fs::path batch=manifest.at("batch_root").get<std::string>();
    const auto filename=std::string(role)+"-source-provenance.json";
    for(const auto& path:{batch/"source-v2"/filename,batch/filename}) {
        if(!fs::is_regular_file(path))continue;
        if(fs::file_size(path)>2*1024*1024)throw std::runtime_error("comparison_provenance_capacity");
        if(pas::sha256_file(path)!=summary.at("source_provenance_sha256").get<std::string>())continue;
        const auto provenance=pas::review::load(path);
        if(!provenance.contains("lineage")||provenance.at("lineage")!=role)
            throw std::runtime_error(std::string("comparison_provenance_lineage_")+role);
        return;
    }
    throw std::runtime_error(std::string("comparison_provenance_binding_")+role);
}
inline void validate_comparison_inputs(const json& manifest,const std::string& manifest_sha,
                                       const json& a,const json& b) {
    validate_comparison_manifest(manifest);
    for(const auto* summary:{&a,&b})
        if(!summary->contains("input_manifest_sha256")||summary->at("input_manifest_sha256")!=manifest_sha)
            throw std::runtime_error("comparison_manifest_SHA_mismatch");
    for(const auto key:{"cadence","tie","recognition","receipt_policy"})
        if(!a.contains(key)||!b.contains(key)||a.at(key)!=b.at(key))
            throw std::runtime_error("comparison_policy_mismatch");
    for(const auto* summary:{&a,&b})
        if(!summary->contains("success")||!summary->at("success").is_boolean()||!summary->at("success").get<bool>())
            throw std::runtime_error("cannot_compare_failed_run");
    validate_comparison_role(a,manifest,"c36h");
    validate_comparison_role(b,manifest,"main50");
    if(a.at("binary_sha256")==b.at("binary_sha256")||
       a.at("source_provenance_sha256")==b.at("source_provenance_sha256"))
        throw std::runtime_error("comparison_run_source_not_distinct");
    // binary_sha256 identifies the recorded replay executable. It need not
    // match the executable performing this read-only comparison.
}
}
