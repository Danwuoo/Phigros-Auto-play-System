#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <stdexcept>

// Diagnostic counterfactual, used only by the newly prepared X2 export.
// The original preserve flag and all later gates remain production expressions.
namespace pas::x2 {
inline thread_local bool winner_override_enabled=true;
inline thread_local bool capture_enabled=false;
inline thread_local nlohmann::json selections=nlohmann::json::array();
inline thread_local std::uint64_t eligible_count=0,different_winner_count=0;
inline thread_local nlohmann::json first_eligible=nullptr,first_different_winner=nullptr;
inline void selection(nlohmann::json row) {
    if(!capture_enabled)return;
    if(row.at("preserve_eligible").get<bool>()) {
        ++eligible_count;
        if(first_eligible.is_null())first_eligible=row;
        if(row.at("pre_override_winner")!=row.at("confirmed_line_id")) {
            ++different_winner_count;
            if(first_different_winner.is_null())first_different_winner=row;
        }
    }
    if(selections.size()>=128)throw std::runtime_error("x2_selection_capacity");
    selections.push_back(std::move(row));
}
inline nlohmann::json drain() {auto result=std::move(selections);selections=nlohmann::json::array();return result;}
inline void reset() {selections=nlohmann::json::array();eligible_count=different_winner_count=0;first_eligible=first_different_winner=nullptr;}
}
