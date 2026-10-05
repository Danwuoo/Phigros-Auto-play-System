#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
namespace r1 {
using json=nlohmann::json;
inline bool metric(const json& m,std::uint64_t expected,double p99=1e99,double mx=1e99) {
    try {
        if(m.at("n").get<std::uint64_t>()!=expected||!expected)return false;
        for(auto q:{"p50","p95","p99","max","jitter_p95_minus_p5"})if(!m.at(q).is_number()||!std::isfinite(m.at(q).get<double>()))return false;
        return m.at("p99").get<double>()<=p99&&m.at("max").get<double>()<=mx;
    }catch(...){return false;}
}
inline bool normal(const json& r) {
    try {
        const auto attempts=r.at("attempts").get<std::uint64_t>(),consumed=r.at("consumed").get<std::uint64_t>(),owned=r.at("owner_seen").get<std::uint64_t>();
        const auto n=r.at("receipt_n").get<std::uint64_t>();const auto& m=r.at("metrics_ms");
        const auto policy=r.at("action_expectation").get<std::string>();
        if(policy!="required"&&policy!="none")return false;
        const bool action=policy=="none"?n==0:metric(m.at("lateness"),n,15,100);
        return attempts>0&&consumed>=attempts*.9&&owned>=attempts*.9&&owned<=consumed&&consumed<=r.at("published").get<std::uint64_t>()&&
            r.at("published").get<std::uint64_t>()+r.at("pool_drops").get<std::uint64_t>()==attempts&&
            r.at("hard_gate").get<bool>()&&r.at("raw_complete").get<bool>()&&r.at("release_n").get<std::uint64_t>()>0&&
            metric(m.at("recognition"),consumed)&&metric(m.at("owner"),owned)&&metric(m.at("capture_to_owner"),owned,100,250)&&action;
    }catch(...){return false;}
}
}
