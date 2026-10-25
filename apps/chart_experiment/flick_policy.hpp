#pragma once

#include "rhythm/contact_compiler.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rhythm::experimental {
// Bounded experimental controls, not measured game judgment windows. Defaults
// retain the archived 30/60/60/24 candidate and its compiled fingerprint.
struct FlickParameters {
    std::int64_t lead_ms=30,travel_ms=60,release_ms=24;
    double distance_reference_pixels=60;
};
inline void validate_flick_parameters(const FlickParameters& p) {
    if(p.lead_ms<1||p.lead_ms>100||p.travel_ms<2||p.travel_ms>200||p.lead_ms>=p.travel_ms||
       p.release_ms<1||p.release_ms>100||!std::isfinite(p.distance_reference_pixels)||
       p.distance_reference_pixels<1||p.distance_reference_pixels>240)
        throw std::invalid_argument("Flick candidate requires lead 1..100ms, travel 2..200ms with lead<travel, distance 1..240 reference pixels, release 1..100ms");
}
inline std::int64_t flick_integer(const nlohmann::json& value,std::int64_t lower,std::int64_t upper) {
    if(!value.is_number_integer()||
       (value.is_number_unsigned()&&value.get<std::uint64_t>()>static_cast<std::uint64_t>(upper)))
        throw std::invalid_argument("Flick timing must be a bounded integer, not bool/float/string");
    const auto number=value.get<std::int64_t>();
    if(number<lower||number>upper)throw std::invalid_argument("Flick timing outside experiment bounds");
    return number;
}
inline double flick_distance(const nlohmann::json& value) {
    if(!value.is_number())throw std::invalid_argument("Flick distance must be a finite number");
    const double number=value.get<double>();
    if(!std::isfinite(number)||number<1||number>240)throw std::invalid_argument("Flick distance outside 1..240 reference pixels");
    return number;
}
// Validate the relationship once all CLI fields are read: option order must
// not reject an otherwise valid pair because of an intermediate default.
inline bool set_flick_option(FlickParameters& p,std::string_view option,const nlohmann::json& value) {
    if(option=="--flick-lead-ms")p.lead_ms=flick_integer(value,1,100);
    else if(option=="--flick-travel-ms")p.travel_ms=flick_integer(value,2,200);
    else if(option=="--flick-distance-pixels")p.distance_reference_pixels=flick_distance(value);
    else if(option=="--flick-release-ms")p.release_ms=flick_integer(value,1,100);
    else return false;
    return true;
}
inline FlickParameters flick_parameters_from_profile(const nlohmann::json& value) {
    if(!value.is_object())throw std::invalid_argument("Flick profile must be an object");
    FlickParameters p;
    if(value.contains("flick_lead_ms"))p.lead_ms=flick_integer(value.at("flick_lead_ms"),1,100);
    if(value.contains("flick_travel_ms"))p.travel_ms=flick_integer(value.at("flick_travel_ms"),2,200);
    if(value.contains("flick_release_ms"))p.release_ms=flick_integer(value.at("flick_release_ms"),1,100);
    if(value.contains("flick_distance_reference_pixels"))p.distance_reference_pixels=flick_distance(value.at("flick_distance_reference_pixels"));
    validate_flick_parameters(p);return p;
}
inline nlohmann::json flick_parameters_json(const FlickParameters& p) {
    validate_flick_parameters(p);
    return {{"flick_lead_ms",p.lead_ms},{"flick_travel_ms",p.travel_ms},
        {"flick_distance_reference_pixels",p.distance_reference_pixels},{"flick_release_ms",p.release_ms}};
}
inline void apply_flick_parameters(ContactCompileOptions& options,const FlickParameters& p) {
    validate_flick_parameters(p);
    options.flick_lead_ns=p.lead_ms*1'000'000;
    options.flick_travel_ns=p.travel_ms*1'000'000;
    options.flick_release_ns=p.release_ms*1'000'000;
    options.flick_distance_reference_pixels=p.distance_reference_pixels;
}
} // namespace rhythm::experimental
