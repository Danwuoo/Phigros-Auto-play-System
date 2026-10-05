#pragma once
#include "x5_rule.hpp"
#include "review_io.hpp"

// Read-only research strata. No selection, prediction, owner or action API.
// C36h has a per-frame preserve predicate, not main50 confirmation state.
namespace pas::x5 {
enum class Lineage { unknown, c36h, main50 };
struct ScopeEvidence {
    Lineage lineage=Lineage::unknown;
    bool strong_current=false,hold_or_body=false;
    Identity identity=Identity::unknown;
    std::optional<std::size_t> prior_samples;
    std::optional<bool> preserve;
    Confirmation confirmation=Confirmation::unknown;
};
struct ResearchScope {
    bool early_comparison_eligible=false;
    const char* reason="lineage_unknown";
    const char* confirmation_semantics="unknown";
};
inline ResearchScope research_scope(const ScopeEvidence& e) {
    ResearchScope r;
    if(e.lineage==Lineage::unknown)return r;
    r.confirmation_semantics=e.lineage==Lineage::c36h?
        "not_applicable_no_persistent_confirmation_state":"explicit_preselection_state";
    if(e.hold_or_body){r.reason="hold_outside_early_scope";return r;}
    if(!e.strong_current){r.reason="current_support_unknown";return r;}
    if(e.identity!=Identity::unique){r.reason="identity_not_unique";return r;}
    if(!e.preserve){r.reason="selection_trace_unknown";return r;}
    if(e.lineage==Lineage::c36h) {
        if(e.confirmation!=Confirmation::unknown){r.reason="lineage_state_contradiction";return r;}
        if(!e.prior_samples){r.reason="prior_samples_unknown";return r;}
        // Necessary branch condition in frozen C36h: points.size() >= 3.
        // Absence of that condition is a sufficient, conservative early
        // stratum. These are relation samples, not raw frames or physical age.
        if(*e.preserve)r.reason=*e.prior_samples<3?
            "lineage_state_contradiction":"c36h_current_preserve_taken";
        else if(*e.prior_samples<3){r.early_comparison_eligible=true;r.reason="c36h_preserve_unreachable_lt3_prior_relation_samples";}
        else r.reason="c36h_nonpreserved_mature_history_preconditions_unknown";
    } else {
        if(e.confirmation==Confirmation::unknown)r.reason="main50_confirmation_unknown";
        else if(e.confirmation==Confirmation::confirmed)r.reason="main50_confirmed_outside_early_scope";
        else if(*e.preserve)r.reason="lineage_state_contradiction";
        else {r.early_comparison_eligible=true;r.reason="main50_explicit_unconfirmed";}
    }
    return r;
}
inline std::uint64_t scope_integer(const pas::review::json& j) {
    if(!j.is_number_integer()||(j.is_number_integer()&&!j.is_number_unsigned()&&j.get<std::int64_t>()<0))
        throw std::runtime_error("x5_scope_integer");
    return j.get<std::uint64_t>();
}
inline ScopeEvidence scope_evidence(Lineage lineage,const pas::review::json& o) {
    ScopeEvidence e;e.lineage=lineage;
    e.strong_current=o.at("current_strong").get<bool>();
    // The frozen report retains the original Hold/body exclusion reason,
    // including any shortened body whose exported kind alone is insufficient.
    e.hold_or_body=o.at("kind")=="hold"||o.value("decision_reason","")=="hold_head_body_tail_outside_rule";
    const auto& track=o.at("identity_assignment");
    if(!track.is_null()) {
        e.identity=track.at("identity_ambiguous").get<bool>()?Identity::ambiguous:Identity::unique;
        if(track.contains("prior_samples"))e.prior_samples=scope_integer(track.at("prior_samples"));
    }
    const auto& selection=o.at("original_selection_comparison_only");
    if(!selection.is_null()) {
        if(selection.contains("preserve"))e.preserve=selection.at("preserve").get<bool>();
        if(selection.contains("confirmed_line_id"))e.confirmation=scope_integer(selection.at("confirmed_line_id"))?
            Confirmation::confirmed:Confirmation::unconfirmed;
    }
    return e;
}
}
