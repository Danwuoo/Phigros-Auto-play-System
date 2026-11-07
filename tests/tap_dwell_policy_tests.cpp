#include "../apps/chart_experiment/tap_dwell_policy.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
using Json=nlohmann::json;
using namespace rhythm::experimental;
namespace {
std::size_t checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}check(rejected,why);}
}
int main() {
try {
    Json old={{"expected_plan_sha256","unchanged-native-fingerprint"},{"judgment_policy_provenance","original compiler provenance"}};
    check(tap_dwell_ns_from_profile(old)==32'000'000,"missing field no longer means32ms");
    const auto before=old.dump();store_tap_dwell_profile_fields(old,tap_dwell_default_ns);
    check(old.dump()==before&&!old.contains("tap_duration_ns")&&!old.contains("tap_dwell_candidate"),"default changed legacy profile bytes/fields");
    for(const std::int64_t ms:{32,64,96}) {
        const auto ns=ms*1'000'000;
        check(tap_dwell_ns_from_json(Json(ns))==ns&&tap_dwell_ns_from_ms_json(Json(ms))==ns,"closed signed dwell value rejected");
        check(tap_dwell_ns_from_json(Json(static_cast<std::uint64_t>(ns)))==ns,"closed unsigned dwell value rejected");
        Json profile=old;store_tap_dwell_profile_fields(profile,ns);
        check(tap_dwell_ns_from_profile(profile)==ns,"nondefault profile round-trip changed duration");
        check(profile.at("expected_plan_sha256")==old.at("expected_plan_sha256")&&
            profile.at("judgment_policy_provenance")==old.at("judgment_policy_provenance"),"serialization rewrote expected fingerprint or compiled provenance");
        if(ms!=32)check(profile.at("tap_dwell_candidate").at("gameplay_qualified")==false&&
            profile.at("tap_dwell_candidate").at("tap_duration_ns")==ns,"candidate silently claimed gameplay qualification");
        store_tap_dwell_profile_fields(profile,tap_dwell_default_ns);check(profile==old,"returning to default retained opt-in fields");
    }
    for(const Json value:{Json(true),Json(false),Json(nullptr),Json(64'000'000.0),Json("64000000"),Json::array(),Json::object(),
            Json(-1),Json(0),Json(31'999'999),Json(32'000'001),Json(63'999'999),Json(96'000'001),
            Json(std::numeric_limits<std::int64_t>::min()),Json(std::numeric_limits<std::int64_t>::max()),
            Json(std::numeric_limits<std::uint64_t>::max())})
        rejects([&]{(void)tap_dwell_ns_from_json(value);},"unsupported/type-confused/overflow dwell accepted");
    for(const Json value:{Json(true),Json(64.0),Json("64"),Json(-32),Json(0),Json(31),Json(33),Json(63),Json(65),Json(95),Json(97),
            Json(std::numeric_limits<std::uint64_t>::max())})
        rejects([&]{(void)tap_dwell_ns_from_ms_json(value);},"CLI ms accepted a non-closed integer");
    Json inconsistent={{"tap_duration_ns",64'000'000},{"tap_dwell_candidate",tap_dwell_candidate_json<Json>(96'000'000)}};
    rejects([&]{(void)tap_dwell_ns_from_profile(inconsistent);},"mismatched candidate provenance accepted");
    rejects([&]{(void)tap_dwell_ns_from_profile(Json::array());},"nonobject profile accepted");
    check(tap_dwell_candidate_json<Json>(32'000'000).is_null(),"default invented candidate provenance");
    std::cout<<checks<<" Tap dwell policy checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
