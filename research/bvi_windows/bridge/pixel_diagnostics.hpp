#pragma once
// Offline post-dispatch output only. Never supplies observer/owner inputs.
#include "bridge.hpp"
#include <nlohmann/json.hpp>

namespace pas::bvi_offline::diagnostic {
inline constexpr int normal_first=-16,normal_last=20;
inline constexpr std::size_t normal_count=37,tangent_count=5;
inline constexpr std::size_t points_per_query=normal_count*tangent_count;
inline constexpr std::size_t trace_cap_bytes=32*1024*1024;
nlohmann::json rgb_at(const Frame&,double x,double y);
nlohmann::json witness_review(const Frame&,const bvi::Query&);
} // namespace pas::bvi_offline::diagnostic
