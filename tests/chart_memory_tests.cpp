#include "rhythm/chart_memory.hpp"
#include <functional>
#include <iostream>

using namespace rhythm;
void check(bool result) { if (!result) throw std::runtime_error("check failed"); }
void rejects(const std::function<void()>& f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    check(rejected);
}
Memory fixture() {
    return {{Game::phigros, "synthetic", "fixture", "v1", std::string(64, '0'),
             std::string(64, '1'), "fixture-layout"},
            {{1, {{100, Phase::down, {0.2, 0.8}}, {200, Phase::up, {0.2, 0.8}}}}}};
}
int main() {
    int passed = 0;
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"phigros identity", [] { check(validate(fixture(), 2) == 1); }},
        {"project sekai identity", [] { auto m=fixture(); m.identity.game=Game::project_sekai; check(validate(m,2)==1); }},
        {"missing identity", [] { auto m=fixture(); m.identity.layout_id.clear(); rejects([&]{validate(m,2);}); }},
        {"invalid hash", [] { auto m=fixture(); m.identity.chart_sha256="bad"; rejects([&]{validate(m,2);}); }},
        {"duplicate id", [] { auto m=fixture(); m.contacts.push_back(m.contacts[0]); rejects([&]{validate(m,2);}); }},
        {"missing release", [] { auto m=fixture(); m.contacts[0].steps.pop_back(); rejects([&]{validate(m,2);}); }},
        {"invalid middle phase", [] { auto m=fixture(); m.contacts[0].steps.insert(m.contacts[0].steps.begin()+1,{150,Phase::down,{.2,.8}}); rejects([&]{validate(m,2);}); }},
        {"reversed time", [] { auto m=fixture(); m.contacts[0].steps[1].song_ns=99; rejects([&]{validate(m,2);}); }},
        {"negative time", [] { auto m=fixture(); m.contacts[0].steps[0].song_ns=-1; rejects([&]{validate(m,2);}); }},
        {"nan coordinate", [] { auto m=fixture(); m.contacts[0].steps[0].point.x=std::numeric_limits<double>::quiet_NaN(); rejects([&]{validate(m,2);}); }},
        {"outside surface", [] { rejects([]{pixel_point({1.1,0},1920,1080);}); }},
        {"pixel edges", [] { check(pixel_point({1,1},1920,1080)==std::make_pair(1919,1079)); }},
        {"invalid dimensions", [] { rejects([]{pixel_point({0,0},1,1080);}); }},
        {"overlap capacity", [] { auto m=fixture(); auto c=m.contacts[0]; c.id=2; m.contacts.push_back(c); rejects([&]{validate(m,1);}); check(validate(m,2)==2); }},
        {"release before reuse", [] { auto m=fixture(); m.contacts.push_back({2,{{200,Phase::down,{0,0}},{300,Phase::up,{0,0}}}}); check(validate(m,1)==1); auto e=map_for_dry_run(m,{0,0,1},0,1); check(e[1].phase==Phase::up && e[2].phase==Phase::down); }},
        {"invalid capacity", [] { rejects([]{validate(fixture(),11);}); }},
        {"affine anchor", [] { check(ClockMap{500,1000,1}.to_host(600)==1100); }},
        {"clock drift", [] { check(ClockMap{0,0,1.001L}.to_host(1'000'000)==1'001'000); }},
        {"input lead", [] { check(ClockMap{0,1000,1}.dispatch_at(500,35)==1465); }},
        {"invalid rate", [] { rejects([]{ClockMap{0,0,0}.to_host(1);}); rejects([]{ClockMap{0,0,std::numeric_limits<long double>::quiet_NaN()}.to_host(1);}); }},
        {"overflow", [] { rejects([]{ClockMap{0,std::numeric_limits<Ns>::max(),2}.to_host(1);}); }},
        {"invalid lead", [] { rejects([]{ClockMap{}.dispatch_at(1,-1);}); rejects([]{ClockMap{}.dispatch_at(1,2);}); }},
        {"rounding collapse", [] { auto m=fixture(); m.contacts[0].steps[1].song_ns=101; rejects([&]{map_for_dry_run(m,{0,1000,.001L},0,2);}); }},
        {"lease boundaries", [] { SyncLease s{true,1,2,100,200,10}; check(s.valid(100,1,2,10)); check(s.valid(199,1,2,10)); check(!s.valid(200,1,2,10)); check(!s.valid(99,1,2,10)); }},
        {"context revoked", [] { SyncLease s{true,1,2,100,200,10}; check(!s.valid(150,2,2,10)); check(!s.valid(150,1,3,10)); s.locked=false; check(!s.valid(150,1,2,10)); }},
        {"uncertainty rejected", [] { SyncLease s{true,1,2,100,200,11}; check(!s.valid(150,1,2,10)); }},
        {"default disarmed", [] { check(!SyncLease{}.valid(0,0,0,0)); }},
        {"sorted offline mapping", [] { auto e=map_for_dry_run(fixture(),{0,1000,1},10,2); check(e.size()==2 && e[0].host_ns==1090 && e[1].host_ns==1190); }}
    };
    for (const auto& [name, run] : tests) {
        try { run(); ++passed; }
        catch (const std::exception& e) { std::cerr << "FAIL " << name << ": " << e.what() << '\n'; return 1; }
    }
    std::cout << passed << " offline checks passed\n";
}
