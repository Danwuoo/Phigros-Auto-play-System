#include "../apps/chart_experiment/clock_probe.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace rhythm;
using namespace rhythm::experimental;
namespace {
constexpr SongNs ms=1'000'000;
std::size_t checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}
    catch(const std::overflow_error&){rejected=true;}check(rejected,why);}
CompiledContactPlan fixture() {
    CompiledContactPlan p;p.metadata.cache_sha256=std::string(64,'a');
    p.memory.contacts={{1,{{100*ms,Phase::down,{.2,.3}},{132*ms,Phase::up,{.2,.3}}}},
        {2,{{500*ms,Phase::down,{.6,.7}},{532*ms,Phase::up,{.6,.7}}}},
        {3,{{500*ms,Phase::down,{.8,.7}},{532*ms,Phase::up,{.8,.7}}}}};
    for(const auto& c:p.memory.contacts)for(const auto& s:c.steps)p.events.push_back({s.song_ns,c.id,s.phase,s.point});
    std::sort(p.events.begin(),p.events.end(),[](const auto& a,const auto& b){
        if(a.song_ns!=b.song_ns)return a.song_ns<b.song_ns;
        if(a.phase!=b.phase)return phase_order(a.phase)<phase_order(b.phase);return a.contact_id<b.contact_id;});
    p.total_steps=p.events.size();return p;
}
}
int main() {
try {
    const auto plan=fixture();const ClockProbe disabled;
    const auto identity=validate_clock_probe(plan,disabled);
    check(!identity.enabled&&identity.prefix_events==6&&identity.suffix_events==0&&identity.prefix_contacts==3&&
        identity.suffix_contacts==0&&identity.map_identity=="disabled-identity","disabled proof changed identity");
    for(const auto& e:plan.events)check(disabled.offset_at(e.song_ns)==0&&disabled.mapped_song_ns(e.song_ns)==e.song_ns,"disabled mapping is not identity");
    check(plan.events.front().song_ns==100*ms&&plan.events.back().song_ns==532*ms,"validation mutated original song timeline");
    for(const std::string text:{"300:160","300:-160"}) {
        const auto probe=parse_clock_probe_ms(text),same=probe;const auto proof=validate_clock_probe(plan,probe);
        check(proof.source_cfp==plan.metadata.cache_sha256&&proof.prefix_events==2&&proof.suffix_events==4&&
            proof.prefix_contacts==1&&proof.suffix_contacts==2,"suffix proof source/counts changed");
        check(proof.original_first_ns==100*ms&&proof.original_last_ns==532*ms&&proof.mapped_first_ns==100*ms&&
            proof.mapped_last_ns==532*ms+probe.delta_ns,"mapped first/last bounds incorrect");
        check(probe.mapped_song_ns(532*ms)-probe.mapped_song_ns(500*ms)==32*ms,"suffix contact duration changed");
        check(probe.offset_at(299*ms)==0&&probe.offset_at(300*ms)==probe.delta_ns,"cutoff inclusivity changed");
        check(validate_clock_probe(plan,same).map_identity==proof.map_identity,"mapping identity is not deterministic");
    }
    check(parse_clock_probe_ms("122000:160").cut_ns==122'000*ms,"real cutoff parse changed");
    for(const std::string text:{"","300","300:",":160","300:160:0","300.0:160","300:160.0","true:160","300:false",
            " 300:160","300:160 ","-1:160","600001:160","300:251","300:-251","300:+160",
            "9223372036854775808:160","300:-9223372036854775809"})
        rejects([&]{(void)parse_clock_probe_ms(text);},"malformed/bounded CLI probe accepted");
    for(const SongNs cut:{100*ms+1,132*ms,500*ms+1,532*ms})
        rejects([&]{(void)validate_clock_probe(plan,ClockProbe{true,cut,160*ms});},"cut inside contact or exactly atUP accepted");
    check(validate_clock_probe(plan,ClockProbe{true,500*ms,160*ms}).suffix_contacts==2,"cut exactly atDOWN incorrectly rejected");
    auto tight=plan;tight.memory.contacts[1].steps[0].song_ns=292*ms;tight.memory.contacts[1].steps[1].song_ns=324*ms;
    tight.memory.contacts[2].steps[0].song_ns=292*ms;tight.memory.contacts[2].steps[1].song_ns=324*ms;
    tight.events.clear();for(const auto& c:tight.memory.contacts)for(const auto& s:c.steps)tight.events.push_back({s.song_ns,c.id,s.phase,s.point});
    std::sort(tight.events.begin(),tight.events.end(),[](const auto& a,const auto& b){return a.song_ns<b.song_ns||(a.song_ns==b.song_ns&&a.contact_id<b.contact_id);});
    rejects([&]{(void)validate_clock_probe(tight,ClockProbe{true,200*ms,-160*ms});},"newly collapsed event tie accepted");
    rejects([&]{(void)validate_clock_probe(tight,ClockProbe{true,200*ms,-161*ms});},"mapped event inversion accepted");
    auto swapped=plan;std::swap(swapped.events[2],swapped.events[3]);
    rejects([&]{(void)validate_clock_probe(swapped,ClockProbe{});},"original tied phase/id event order not checked");
    rejects([&]{(void)ClockProbe{true,0,-160*ms}.mapped_song_ns(159*ms);},"negative mapped timestamp accepted");
    rejects([&]{(void)ClockProbe{true,0,160*ms}.mapped_song_ns(std::numeric_limits<SongNs>::max());},"mapped signed overflow accepted");
    rejects([&]{validate_clock_probe_options(ClockProbe{false,1,0});},"disabled hidden cutoff accepted");
    rejects([&]{validate_clock_probe_options(ClockProbe{false,0,1});},"disabled hidden delta accepted");
    rejects([&]{validate_clock_probe_options(ClockProbe{true,-1,0});},"negative cutoff accepted");
    rejects([&]{validate_clock_probe_options(ClockProbe{true,600'000*ms+1,0});},"out-of-bounds cutoff accepted");
    rejects([&]{validate_clock_probe_options(ClockProbe{true,0,250*ms+1});},"out-of-bounds delta accepted");
    std::cout<<checks<<" static clock probe checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
