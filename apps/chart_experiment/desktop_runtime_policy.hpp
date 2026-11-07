#pragma once
#include <cstdint>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rhythm::experimental {
inline constexpr std::int64_t desktop_settlement_grace_ns=60'000'000'000LL;
struct DesktopRuntimeLimits {
    std::int64_t input_duration_limit_ns=0;
    std::int64_t manual_settlement_watchdog_ns=0;
    std::int64_t session_limit_ns=0;
};
inline constexpr const char* desktop_settlement_policy_name() {
    return "input-budget-plus60s-bounded-manual-settlement-v1";
}
// Input admission keeps its original 1..600s song budget. The manual lifecycle
// additionally needs time to observe CURRENT result evidence after the last UP.
// These are total first-HUD/anchor-relative caps, not 60s renewed at completion;
// neither actual observations nor planner ticks extend either deadline.
inline DesktopRuntimeLimits desktop_runtime_limits(std::int64_t input_duration_limit_ns) {
    if(input_duration_limit_ns<1'000'000'000LL||input_duration_limit_ns>600'000'000'000LL)
        throw std::invalid_argument("desktop input duration must remain within1..600 seconds");
    if(input_duration_limit_ns>std::numeric_limits<std::int64_t>::max()-desktop_settlement_grace_ns)
        throw std::invalid_argument("desktop settlement duration overflow");
    const auto settlement=input_duration_limit_ns+desktop_settlement_grace_ns;
    return {input_duration_limit_ns,settlement,settlement};
}
enum class CapacityPolicy { current_boot_fixture, configured_ten_contact };
inline const char* capacity_policy_name(CapacityPolicy policy) {
    return policy==CapacityPolicy::configured_ten_contact ?
        "configured-ten-contact-v1" : "current-boot-fixture-v1";
}
inline CapacityPolicy parse_capacity_policy(std::string_view name) {
    if(name=="configured-ten-contact-v1")return CapacityPolicy::configured_ten_contact;
    if(name=="current-boot-fixture-v1")return CapacityPolicy::current_boot_fixture;
    throw std::invalid_argument("unknown explicit contact-capacity policy");
}
inline bool capacity_evidence_required(CapacityPolicy policy,int capacity) {
    if(capacity<1||capacity>10)throw std::invalid_argument("contact capacity must be 1..10");
    return policy==CapacityPolicy::current_boot_fixture&&capacity>4;
}
// Current boot identity scopes release/epoch monitoring. This does not run a
// capacity fixture or turn a historical capacity result into a new test pass.
inline std::string configured_runtime_boot_id(std::string raw) {
    const auto begin=raw.find_first_not_of(" \t\r\n");
    if(begin==std::string::npos)throw std::invalid_argument("current Android boot identity unavailable");
    raw=raw.substr(begin,raw.find_last_not_of(" \t\r\n")-begin+1);
    if(!std::regex_match(raw,std::regex("[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}")))
        throw std::invalid_argument("invalid current Android boot identity");
    return raw;
}
inline std::string capture_profile_for_geometry(int width,int height,int rotation) {
    if(width==1280&&height==720&&rotation==1)return "landscape-r1";
    if(width==720&&height==1280&&rotation==0)return "sensor-portrait-r0-ccw90";
    throw std::invalid_argument("unsupported current capture geometry");
}
}
