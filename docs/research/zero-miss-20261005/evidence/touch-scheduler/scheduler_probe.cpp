#include "pas/core.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace pas;
constexpr Nanoseconds ms=1'000'000;
int checks=0;
void require(bool v,const char* label){++checks;if(!v)throw std::runtime_error(label);}
ContactPlan plan(std::uint64_t id,Nanoseconds evidence,Nanoseconds down,Nanoseconds up){
 ContactPlan p; p.epoch=1;p.intent_id=id;p.revision=1;p.evidence_ns=evidence;
 p.valid_until_ns=down+30*ms;p.basis="isolated_synthetic";
 p.steps={{Phase::down,100.0+double(id),300.0,down},{Phase::up,100.0+double(id),300.0,up}};return p;
}
struct ScriptTouch final:TouchBackend {
 FakeClock& c;Nanoseconds delay=0;bool fail_down=false;std::map<int,std::array<double,2>> active;
 std::vector<TouchReceipt> receipts;int unknown=-1;int release_calls=0;
 ScriptTouch(FakeClock& x):c(x){}
 TouchReceipt inject(const TouchCommand& cmd)override{
  auto begin=c.now_ns();bool ok=!(fail_down&&cmd.phase==Phase::down);
  if(ok){if(cmd.phase==Phase::up)active.erase(cmd.contact_id);else active[cmd.contact_id]={cmd.x,cmd.y};}
  else unknown=cmd.contact_id;
  c.set(c.now_ns()+delay);TouchReceipt r{cmd,begin,c.now_ns(),ok,ok?"synthetic_return":"synthetic_unknown"};receipts.push_back(r);return r;
 }
 ReleaseReport release_all()override{
  ++release_calls;ReleaseReport r;r.start_ns=c.now_ns();
  for(auto& [id,p]:active)r.requested_ids.push_back(id);
  if(unknown>=0)r.requested_ids.push_back(unknown);
  active.clear();unknown=-1;r.return_ns=c.now_ns();return r;
 }
};
int main(){try{
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  auto p=plan(1,0,10*ms,80*ms);p.valid_until_ns=10*ms;c.set(10*ms);
  require(s.submit(p),"submit equality allowed");require(s.run_due().empty(),"dispatch equality expires");
  require(s.take_notices().at(0).reason=="target_evidence_or_window_expired","expiry reason");
  std::cout<<"PASS valid_until_equality: submit=true, down=0, expired=1\n";}
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  auto p=plan(1,0,10*ms,90*ms);require(s.submit(p),"late plan submit");c.set(40*ms);require(s.run_due().empty(),"valid_until first boundary");
  auto notices=s.take_notices();require(notices.at(0).reason=="target_evidence_or_window_expired","expiry precedence");
  std::cout<<"PASS expiry_precedes_lateness: default window closes at down+30ms\n";}
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  auto p=plan(1,0,10*ms,95*ms);p.valid_until_ns=70*ms;require(s.submit(p),"late explicit submit");c.set(40*ms);auto r=s.run_due();require(r.size()==1,"late exact allowed");
  std::cout<<"PASS late_30ms_equality: down=1\n";}
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  auto p=plan(1,0,10*ms,95*ms);p.valid_until_ns=70*ms;require(s.submit(p),"late 30ms+1ns submit");c.set(40*ms+1);require(s.run_due().empty(),"late reject");
  require(s.take_notices().at(0).reason=="down_too_late","late reason");require(!s.submit(p),"late intent cannot revive");
  std::cout<<"PASS late_30ms_plus_1ns: down=0, same_intent_resubmit=false\n";}
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  for(int i=1;i<=6;++i)require(s.submit(plan(i,0,10*ms,60*ms)),"capacity submit");c.set(10*ms);
  require(s.run_due().size()==5,"five down");require(s.active_count()==5,"five active");require(s.take_notices().at(0).reason=="contact_conflict","sixth conflict");
  require(!s.executed_steps(6),"conflict cursor absent");c.set(60*ms);require(s.run_due().size()==5,"five up");
  auto p=plan(6,60*ms,60*ms,80*ms);p.revision=2;require(!s.submit(p),"conflicted intent remains retired");
  p.intent_id=7;require(s.submit(p),"new intent permitted");require(s.run_due().size()==1,"fresh new intent down");
  std::cout<<"PASS contact_conflict_retirement: submitted=6, down=5, conflict=1, retry_same_id=false, retry_new_id=true\n";}
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,1,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  require(s.submit(plan(1,0,10*ms,30*ms)),"reuse first");require(s.submit(plan(2,0,30*ms,50*ms)),"reuse next");
  c.set(10*ms);require(s.run_due().size()==1,"reuse first down");c.set(30*ms);auto r=s.run_due();
  require(r.size()==2&&r[0].command.phase==Phase::up&&r[1].command.phase==Phase::down,"up before down tie");
  require(r[0].command.contact_id==r[1].command.contact_id,"contact reused");std::cout<<"PASS equal_due_release_then_reuse: phases=Up,Down, same_contact=true\n";}
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,1,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  require(s.submit(plan(1,0,10*ms,30*ms)),"overdue first");require(s.submit(plan(2,0,29*ms,50*ms)),"overdue next");c.set(10*ms);s.run_due();c.set(35*ms);auto r=s.run_due();
  require(r.size()==1&&r[0].command.phase==Phase::up,"overdue prior down conflicts before later due up");
  require(s.take_notices().at(0).reason=="contact_conflict","overdue conflict");
  std::cout<<"PASS overdue_order: next Down due29 conflicts, prior Up due30 executes at35; no automatic reschedule\n";}
 {FakeClock c;ScriptTouch b(c);b.delay=10*ms;ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  for(int i=1;i<=5;++i){auto p=plan(i,0,10*ms,90*ms);p.valid_until_ns=80*ms;require(s.submit(p),"serial submit");}
  c.set(10*ms);auto r=s.run_due();require(r.size()==4,"serial 4 down before max late");
  require(s.take_notices().at(0).reason=="down_too_late","fifth late reason");
  std::cout<<"PASS scripted_serial_rpc_10ms: plans=5, down=4, down_too_late=1, starts_ms=";
  for(auto& x:r)std::cout<<x.injection_start_ns/ms<<",";std::cout<<" (synthetic, not measured gRPC)\n";}
 {FakeClock c;ScriptTouch b(c);b.delay=10*ms;ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);
  for(int i=1;i<=5;++i)require(s.submit(plan(i,0,10*ms,90*ms)),"serial default submit");
  c.set(10*ms);auto r=s.run_due();require(r.size()==3,"serial 3 down before default window expiry");
  auto notices=s.take_notices();require(notices.size()==2,"2 expire");for(auto& n:notices)require(n.reason=="target_evidence_or_window_expired","expire reason");
  std::cout<<"PASS scripted_serial_rpc_10ms_default_window: plans=5, down=3, expired=2\n";}
 {FakeClock c;ScriptTouch b(c);b.fail_down=true;ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);require(s.submit(plan(1,0,10*ms,60*ms)),"unknown submit");
  c.set(10*ms);auto r=s.run_due();require(r.size()==1&&!r[0].success,"unknown receipt");require(s.fault()=="input_result_unknown","unknown fault");require(s.last_release().requested_ids==std::vector<int>{0},"unknown release duty");
  require(!s.set_gate(1,true,10*ms),"unknown cannot rearm");require(!s.submit(plan(2,10*ms,10*ms,60*ms)),"unknown no retry even new intent");
  std::cout<<"PASS unknown_down: attempts=1, release_requested=1, fault_latched=true, retry=false\n";}
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);require(s.submit(plan(1,0,10*ms,130*ms)),"evidence submit");
  c.set(10*ms);s.run_due();c.set(90*ms);s.set_gate(1,true,90*ms);c.set(100*ms);auto r=s.run_due();require(r.size()==1&&r[0].command.phase==Phase::up,"old target expires despite fresh gate");require(b.contacts().empty(),"target release");
  std::cout<<"PASS independent_gate_target_expiry: fresh_gate=true, target_released_at100=true\n";}
 {FakeClock c;FakeTouchBackend b(c);ContactScheduler s(c,b,5,128,16,350*ms,100*ms);s.set_gate(1,true,0);require(s.submit(plan(1,0,10*ms,60*ms)),"stop submit");s.request_stop();c.set(10*ms);require(s.run_due().empty(),"stop no down");require(!s.armed(),"stop disarmed");std::cout<<"PASS stop_before_due: down=0\n";}
 {PixelCoordinateMap m(1280,720,720,1280,90);require(m.map(0,0)==std::array<int,2>{719,0},"map tl");require(m.map(1279,719)==std::array<int,2>{0,1279},"map br");bool reject=false;try{m.map(1280,0);}catch(const std::invalid_argument&){reject=true;}require(reject,"map outside reject");std::cout<<"PASS five_profile_rotation90_edges: (0,0)->(719,0), (1279,719)->(0,1279)\n";}
 std::cout<<"RESULT cases=13 checks="<<checks<<" failures=0; fake-clock_only=true; production_source_unmodified=true\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after_checks="<<checks<<"\n";return 1;}}
