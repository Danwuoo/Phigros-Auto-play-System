#pragma once
#include "adaptive_first_note.hpp"
#include "classic_hold_head.hpp"
#include "classic_oriented_hold_head.hpp"
#include "classic_flick_head.hpp"

namespace rhythm::experimental {
// Keep every old typed center and add independently bounded classic heads.
// Never repair a clipped/ambiguous/exhausted detector by ignoring its evidence.
inline calibration::OpeningDetection detect_single_model_opening_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,
        const AdaptiveOpeningDescription& description) {
    if(description.includes_outlined_drag_heads) {
        auto outlined=detect_outlined_horizontal_drag_opening_centers(width,height,stride,rgb);
        if(outlined.valid)outlined.reason=description.detector_policy_id+";completeness_unverified";
        return outlined;
    }
    if(description.includes_extended_drag_heads) {
        auto extended=detect_extended_drag_opening_centers(width,height,stride,rgb);
        if(extended.valid)extended.reason=description.detector_policy_id+";completeness_unverified";
        return extended;
    }
    auto original=detect_adaptive_opening_centers(width,height,stride,rgb,description.note_kind);
    if(!description.includes_hold_heads&&!description.includes_classic_flick_heads)return original;
    const auto empty=[&](const calibration::OpeningDetection& value){
        return !value.valid&&(value.reason=="no_supported_typed_head_centers"||value.reason=="no_supported_cyan_centers"||
            (description.includes_classic_flick_heads&&value.reason=="no_supported_yellow_centers"));
    };
    if(!original.valid&&!empty(original))return original;
    auto classic=description.includes_classic_flick_heads?detect_classic_flick_head_centers(width,height,stride,rgb):
        detect_classic_oriented_hold_head_centers(width,height,stride,rgb);
    const auto no_classic=description.includes_classic_flick_heads?"no_supported_classic_flick_head":"no_supported_classic_oriented_hold_head";
    if(!classic.valid&&classic.reason!=no_classic)return classic;
    if(!classic.valid)return original;
    const auto fail=[&](const char* reason){original.valid=false;original.reason=reason;original.centers.clear();original.metrics.clear();return original;};
    if(original.centers.size()+classic.centers.size()>8)return fail("combined_typed_head_center_budget");
    for(const auto& candidate:classic.centers){
        if(std::any_of(original.centers.begin(),original.centers.end(),[&](const auto& prior){
                return candidate.id==prior.id||std::hypot(candidate.x-prior.x,candidate.y-prior.y)<=3;
            }))return fail("cross_detector_head_ownership_ambiguous");
        original.centers.push_back(candidate);
    }
    original.metrics.insert(original.metrics.end(),classic.metrics.begin(),classic.metrics.end());
    original.qualifying_runs+=classic.qualifying_runs;original.groups+=classic.groups;
    original.ignored_short_runs+=classic.ignored_short_runs;original.ignored_long_runs+=classic.ignored_long_runs;
    original.valid=true;original.centers_complete_unoccluded=false;
    original.reason=description.detector_policy_id+";completeness_unverified";
    return original;
}
struct ModelOpeningDetection:calibration::OpeningDetection {std::vector<int> center_families;};
inline ModelOpeningDetection detect_model_opening_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,
        const AdaptiveOpeningDescription& description) {
    ModelOpeningDetection result;
    if(!description.includes_mixed_earliest_heads){
        static_cast<calibration::OpeningDetection&>(result)=detect_single_model_opening_centers(width,height,stride,rgb,description);
        if(result.valid)result.center_families.assign(result.centers.size(),description.note_kind==NoteKind::drag?2:description.note_kind==NoteKind::flick?4:1);
        return result;
    }
    const auto fail=[&](const char* reason){result.valid=false;result.reason=reason;result.centers.clear();result.metrics.clear();result.center_families.clear();return result;};
    if(description.required_center_families.size()<2||description.required_center_families.size()>3)return fail("invalid_mixed_detector_families");
    int previous=0;
    for(const int family:description.required_center_families){
        if((family!=1&&family!=2&&family!=4)||family<=previous)return fail("invalid_mixed_detector_families");previous=family;
        auto single=description;single.includes_mixed_earliest_heads=false;
        single.note_kind=family==1?NoteKind::tap:family==2?NoteKind::drag:NoteKind::flick;
        single.includes_hold_heads=family==1&&description.includes_hold_heads;
        single.includes_classic_flick_heads=family==4;single.includes_extended_drag_heads=family==2;
        single.includes_outlined_drag_heads=family==2&&description.includes_outlined_drag_heads;
        const auto detected=detect_single_model_opening_centers(width,height,stride,rgb,single);
        if(!detected.valid){
            if(detected.reason=="no_supported_typed_head_centers"||detected.reason=="no_supported_cyan_centers"||
               detected.reason=="no_supported_yellow_centers"||detected.reason=="no_supported_classic_flick_head"||
               detected.reason=="no_supported_classic_hold_head"||detected.reason=="no_supported_classic_oriented_hold_head")continue;
            return fail("mixed_family_detector_rejected_current_frame");
        }
        if(result.centers.size()+detected.centers.size()>8)return fail("mixed_typed_head_center_budget");
        for(const auto& center:detected.centers){
            if(std::any_of(result.centers.begin(),result.centers.end(),[&](const auto& prior){return prior.id==center.id||std::hypot(prior.x-center.x,prior.y-center.y)<=3;}))
                return fail("mixed_cross_family_head_ownership_ambiguous");
            result.centers.push_back(center);result.center_families.push_back(family);
        }
        result.metrics.insert(result.metrics.end(),detected.metrics.begin(),detected.metrics.end());
        result.qualifying_runs+=detected.qualifying_runs;result.groups+=detected.groups;
        result.ignored_short_runs+=detected.ignored_short_runs;result.ignored_long_runs+=detected.ignored_long_runs;
    }
    result.valid=!result.centers.empty();result.centers_complete_unoccluded=false;
    result.reason=result.valid?description.detector_policy_id+";completeness_unverified":"no_supported_mixed_earliest_head_centers";
    return result;
}
} // namespace rhythm::experimental
