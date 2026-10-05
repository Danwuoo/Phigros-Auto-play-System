#pragma once
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
namespace pas::review {
namespace fs=std::filesystem;
using json=nlohmann::json;
inline json load(const fs::path& p) {if(fs::file_size(p)>2*1024*1024)throw std::runtime_error("JSON capacity");std::ifstream in(p);if(!in)throw std::runtime_error("JSON open");return json::parse(in);}
inline fs::path relative(const std::string& s) {fs::path p(s);if(p.is_absolute()||p.has_root_name())throw std::runtime_error("absolute path");for(auto& x:p)if(x=="..")throw std::runtime_error("escaping path");return p;}
template<class F> void each_row(const fs::path& p,std::size_t cap,F consume,std::size_t byte_cap=64*1024*1024) {
    if(fs::file_size(p)>byte_cap)throw std::runtime_error("index capacity");
    std::ifstream in(p);if(!in)throw std::runtime_error("rows open");std::string s;std::size_t count=0;
    while(std::getline(in,s)){if(s.size()>2*1024*1024||++count>cap)throw std::runtime_error("row capacity");consume(json::parse(s));}
}
inline std::vector<json> rows(const fs::path& p,std::size_t cap) {std::vector<json> out;each_row(p,cap,[&](json j){out.push_back(std::move(j));});return out;}
inline void save(const fs::path& p,const json& j){std::ofstream out(p);out<<j.dump(2)<<'\n';if(!out)throw std::runtime_error("JSON write");}
}
