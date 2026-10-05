#pragma once
#include "contract.hpp"
#include "pas/analysis.hpp"
namespace r3 {
inline bool verify_archive(const std::filesystem::path& root,const std::map<std::string,std::uint64_t>& expected) {
    try {
        std::map<std::string,std::uint64_t> actual;
        std::size_t total=0;
        const auto summary=read(root/"round-1/summary.json");
        if(summary.value("partial",false))return false;
        if(summary.at("event_segments").empty()||summary.at("event_segments").size()>32)return false;
        for(const auto& e:summary.at("event_segments")) {
            auto name=e.at("path").get<std::string>();if(std::filesystem::path(name).filename()!=std::filesystem::path(name))return false;
            const auto p=root/"round-1"/name;if(pas::sha256_file(p)!=e.at("sha256").get<std::string>())return false;
            rows(p,events,row_bytes,[&](const json& j){
                require(++total<=events,"archive total bound");
                require(j.at("schema_version")==2&&j.at("round_id")==1&&j.at("clock_domain")=="host_qpc_ns","archive envelope");
                ++actual[j.at("event").get<std::string>()];
            });
        }
        return actual==expected;
    }catch(...){return false;}
}
}
