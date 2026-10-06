#pragma once
#include "evidence_hash.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace pas::cost_input_closure {
using Json=nlohmann::json;
namespace fs=std::filesystem;
inline Json read(const fs::path& p) {
    if(!fs::is_regular_file(p)||fs::file_size(p)>1024*1024)
        throw std::runtime_error("input closure JSON bound/type");
    std::ifstream f(p);if(!f)throw std::runtime_error("input closure absent");
    return Json::parse(f);
}
inline fs::path bound(const Json& b,std::uint64_t cap=1024*1024) {
    const auto p=fs::canonical(b.at("path").get<std::string>());
    if(fs::file_size(p)!=b.at("bytes").get<std::uint64_t>()||
       evidence::sha256(p,cap)!=b.at("sha256").get<std::string>())
        throw std::runtime_error("input closure binding changed");
    return p;
}
// The unchanged owned core directly freezes the small root and native anchors.
// A wrapper COMPLETE is insufficient: require the fresh post-exit leaf receipt,
// tied to that exact core state/verifier, and recompute every declared leaf now.
inline std::map<fs::path,std::string> verify(const Json& receipt_binding,const Json& process_bindings,
                   const Json& stage,const std::map<fs::path,std::string>& frozen) {
    const auto receipt_path=bound(receipt_binding);
    const auto receipt=read(receipt_path);
    if(receipt.at("schema")!="pas.current-input-closure-verification.v1"||
       receipt.at("stage")!=stage||receipt.at("status")!="COMPLETE"||!receipt.at("pass").get<bool>()||
       !receipt.at("before_verified").get<bool>()||!receipt.at("after_verified").get<bool>())
        throw std::runtime_error("input closure not verified before and after native stage");
    for(const auto* key:{"state","verification"}) {
        if(bound(receipt.at(key))!=bound(process_bindings.at(key))||
           receipt.at(key).at("sha256")!=process_bindings.at(key).at("sha256"))
            throw std::runtime_error("input closure core receipt join");
    }
    const auto root_path=bound(receipt.at("root"));
    if(!frozen.contains(root_path)||frozen.at(root_path)!=receipt.at("root").at("sha256").get<std::string>())
        throw std::runtime_error("input closure root not directly frozen");
    const auto root=read(root_path);
    if(root.at("schema")!="pas.current-input-closure.v1"||root.at("stage")!=stage||
       fs::canonical(root.at("verification_output").get<std::string>())!=receipt_path)
        throw std::runtime_error("input closure root/output identity");
    const auto& leaves=root.at("leaves");
    if(!leaves.is_array()||leaves.empty()||leaves.size()>128)
        throw std::runtime_error("input closure leaf capacity");
    std::set<fs::path> leaf_paths;std::map<fs::path,std::string> file_paths;
    std::size_t index=0;
    for(const auto& entry:leaves) {
        const auto path=bound(entry);
        if(path==root_path||path==receipt_path||!leaf_paths.insert(path).second)
            throw std::runtime_error("input closure duplicate leaf");
        const auto leaf=read(path);const auto& files=leaf.at("files");
        if(leaf.at("schema")!="pas.current-input-closure-leaf.v1"||leaf.at("stage")!=stage||leaf.at("index")!=index++||
           !files.is_array()||files.empty()||files.size()>64)
            throw std::runtime_error("input closure leaf file capacity");
        for(const auto& file:files) {
            const auto input=bound(file,256ull*1024*1024);
            if(input==root_path||input==receipt_path||input==path||!file_paths.emplace(input,file.at("sha256").get<std::string>()).second)
                throw std::runtime_error("input closure duplicate/self input");
        }
    }
    for(const auto& path:leaf_paths)if(file_paths.contains(path))
        throw std::runtime_error("input closure leaf is input");
    if(file_paths.size()>8192||receipt.at("files_count")!=file_paths.size()||
       receipt.at("leaf_count")!=leaf_paths.size()||root.at("files_count")!=file_paths.size()||root.at("leaf_count")!=leaf_paths.size())
        throw std::runtime_error("input closure complete denominator");
    // Catch changes during parsing too. This is local receipt verification,
    // not a remote attestation or a substitute for trusted build qualification.
    for(const auto& entry:leaves)bound(entry);
    bound(receipt.at("root"));bound(receipt_binding);
    return file_paths;
}
}
