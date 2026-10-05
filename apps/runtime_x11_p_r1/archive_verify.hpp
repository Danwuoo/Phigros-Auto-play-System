#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
namespace r1 {
inline bool verify_archive(const std::filesystem::path& root,const std::map<std::string,std::uint64_t>& expected) {
    try {
        std::map<std::string,std::uint64_t> actual;
        std::size_t total=0;
        if(!std::filesystem::exists(root/"round-1/summary.json"))return false;
        std::ifstream sf(root/"round-1/summary.json");const auto summary=nlohmann::json::parse(sf);
        if(summary.value("partial",false))return false;
        for(const auto& e:summary.at("event_segments")) {
            std::ifstream f(root/"round-1"/e.at("path").get<std::string>());
            if(!f)return false;
            std::string row;
            while(std::getline(f,row)) {
                if(++total>16384||row.size()>256*1024)return false;
                auto j=nlohmann::json::parse(row);
                if(j.at("schema_version")!=2||j.at("round_id")!=1||j.at("clock_domain")!="host_qpc_ns")return false;
                ++actual[j.at("event").get<std::string>()];
            }
            if(!f.eof())return false;
        }
        return actual==expected;
    }catch(...){return false;}
}
}
