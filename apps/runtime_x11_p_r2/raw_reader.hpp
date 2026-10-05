#pragma once
// Bounded reader/check reused from tools/runtime_x11_p_r1_audit/main.cpp.
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
namespace r2 {
using json=nlohmann::json;namespace fs=std::filesystem;
inline void require(bool p,const char* message){if(!p)throw std::runtime_error(message);}
inline json read(const fs::path& p){std::ifstream f(p);if(!f)throw std::runtime_error("open "+p.string());return json::parse(f);}
inline std::vector<json> rows(const fs::path& p){std::ifstream f(p,std::ios::binary);require(bool(f),"raw open");std::vector<json> v;std::string s;while(std::getline(f,s)){require(v.size()<16384&&s.size()<256*1024,"raw bound");v.push_back(json::parse(s));}require(f.eof(),"raw read");return v;}
inline json distribution(std::vector<double> v){std::sort(v.begin(),v.end());auto q=[&](double p)->json{return v.empty()?json(nullptr):json(v.at(static_cast<std::size_t>(std::ceil(p*v.size()))-1));};return {{"n",v.size()},{"p50",q(.5)},{"p95",q(.95)},{"p99",q(.99)},{"max",q(1)},{"jitter_p95_minus_p5",v.empty()?json(nullptr):json(q(.95).get<double>()-q(.05).get<double>())}};}
inline void check(const json& m,const std::vector<double>& v){const auto actual=distribution(v);require(m.at("n")==actual.at("n"),"metric denominator");for(auto k:{"p50","p95","p99","max","jitter_p95_minus_p5"}){if(v.empty())require(m.at(k).is_null(),"empty metric");else require(std::abs(m.at(k).get<double>()-actual.at(k).get<double>())<1e-8,"metric quantile");}}
inline void save(const fs::path& p,const json& j){require(!fs::exists(p),"existing output");std::ofstream f(p);require(bool(f),"output open");f<<j.dump(2)<<'\n';f.flush();require(bool(f),"output write");}
}
