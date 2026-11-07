#pragma once
#include <cstdint>
#include <stdexcept>

namespace rhythm::experimental {
inline constexpr std::int64_t tap_dwell_default_ns=32'000'000;
inline bool supported_tap_dwell_ns(std::int64_t ns) {
    return ns==32'000'000||ns==64'000'000||ns==96'000'000;
}
inline void validate_tap_dwell_ns(std::int64_t ns) {
    if(!supported_tap_dwell_ns(ns))throw std::invalid_argument("Tap dwell must be exactly32,64 or96ms");
}
template<class Json> std::uint64_t tap_dwell_integer(const Json& value) {
    // JSON integer and unsigned integer are the only accepted representations.
    // In particular, bool/float/string and overflowing unsigned->signed casts
    // cannot silently select a different experimental contact duration.
    if(value.is_number_unsigned())return value.template get<std::uint64_t>();
    if(value.is_number_integer()) {
        const auto number=value.template get<std::int64_t>();
        if(number>=0)return static_cast<std::uint64_t>(number);
    }
    throw std::invalid_argument("Tap dwell requires a nonnegative JSON integer");
}
template<class Json> std::int64_t tap_dwell_ns_from_json(const Json& value) {
    const auto ns=tap_dwell_integer(value);
    if(ns!=32'000'000&&ns!=64'000'000&&ns!=96'000'000)
        throw std::invalid_argument("tap_duration_ns must be32000000,64000000 or96000000");
    return static_cast<std::int64_t>(ns);
}
template<class Json> std::int64_t tap_dwell_ns_from_ms_json(const Json& value) {
    const auto milliseconds_value=tap_dwell_integer(value);
    if(milliseconds_value!=32&&milliseconds_value!=64&&milliseconds_value!=96)throw std::invalid_argument("--tap-duration-ms requires32,64 or96");
    return static_cast<std::int64_t>(milliseconds_value)*1'000'000;
}
template<class Json> Json tap_dwell_candidate_json(std::int64_t ns) {
    validate_tap_dwell_ns(ns);
    if(ns==tap_dwell_default_ns)return Json(nullptr);
    return Json{{"policy_id","tap-dwell-candidate-v1"},{"tap_duration_ns",ns},{"default",false},
        {"gameplay_qualified",false},
        {"scope","Opt-in host Tap contact dwell only; actual game callback sampling and judgment improvement unverified. Source judgments and existing capacity, lateness and UI guards remain unchanged."}};
}
template<class Json> std::int64_t tap_dwell_ns_from_profile(const Json& profile) {
    if(!profile.is_object())throw std::invalid_argument("Tap dwell profile must be an object");
    const auto ns=profile.contains("tap_duration_ns")?tap_dwell_ns_from_json(profile.at("tap_duration_ns")):tap_dwell_default_ns;
    if(profile.contains("tap_dwell_candidate")&&profile.at("tap_dwell_candidate")!=tap_dwell_candidate_json<Json>(ns))
        throw std::invalid_argument("Tap dwell candidate provenance differs from exact duration");
    return ns;
}
template<class Json> void store_tap_dwell_profile_fields(Json& profile,std::int64_t ns) {
    validate_tap_dwell_ns(ns);if(!profile.is_object())throw std::invalid_argument("Tap dwell profile must be an object");
    if(ns==tap_dwell_default_ns){profile.erase("tap_duration_ns");profile.erase("tap_dwell_candidate");return;}
    profile["tap_duration_ns"]=ns;profile["tap_dwell_candidate"]=tap_dwell_candidate_json<Json>(ns);
}
} // namespace rhythm::experimental
