#include "pas/core.hpp"
#include <iostream>
#include <stdexcept>
using namespace pas;
constexpr Nanoseconds ms=1'000'000;
int main(){try{
 FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,5,128,16,350*ms,100*ms);
 s.set_gate(1,true,0);ContactPlan p;p.epoch=1;p.intent_id=1;p.revision=1;p.evidence_ns=0;p.valid_until_ns=40*ms;p.basis="isolated_flick_path";
 p.steps.push_back({Phase::down,400,400,10*ms});
 for(int k=1;k<=4;++k)p.steps.push_back({Phase::move,400,400.0+k*20,(10+k*12)*ms});
 p.steps.push_back({Phase::up,400,480,62*ms});
 if(!s.submit(p))throw std::runtime_error("submit");c.set(10*ms);if(s.run_due().size()!=1)throw std::runtime_error("down");
 c.set(60*ms);auto r=s.run_due();if(r.size()!=4)throw std::runtime_error("four moves");
 for(auto& x:r){if(x.command.phase!=Phase::move||x.injection_start_ns!=60*ms)throw std::runtime_error("move dispatch");std::cout<<"move planned_ms="<<x.command.scheduled_ns/ms<<" start_ms="<<x.injection_start_ns/ms<<" y="<<x.command.y<<"\n";}
 c.set(62*ms);auto up=s.run_due();if(up.size()!=1||up[0].command.phase!=Phase::up||!b.contacts().empty())throw std::runtime_error("up");
 std::cout<<"RESULT cases=1 failures=0; four overdue Moves all dispatch at fake60ms; transport_effect_unknown=true\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
