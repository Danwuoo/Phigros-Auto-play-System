#include "current.hpp"
#include "pas/game_session.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
using namespace pas;using namespace pas::current_rails;using namespace pas::current_execution;using J=nlohmann::json;
constexpr Nanoseconds ms=1'000'000;
struct Tests {J rows=J::array(),details=J::object();int failed=0;void check(std::string id,bool ok){rows.push_back({{"id",id},{"pass",ok}});failed+=!ok;std::cout<<id<<" "<<ok<<std::endl;}};
struct Input {Frame f;CandidateBatch b;DecisionSnapshot s;};
Input make(int sequence,Nanoseconds time,double front=500,double tail=200,double x=400,double angle=0,bool patch=false,std::array<int,3> rgb={155,233,255},double line_angle=0){
 Input a;a.f.sequence=sequence;a.f.epoch=a.f.generation=a.f.geometry_version=1;a.f.width=1280;a.f.height=720;a.f.stride=3840;a.f.source_rotation=1;a.f.source_valid=true;a.f.capture_complete_ns=a.f.pixels_ready_ns=time;a.f.rgb.assign(1280*720*3,30);
 a.s.context={1,1,1,std::uint64_t(sequence),time,1280,720,1};a.s.sequence=sequence;a.s.playing_gate=true;a.s.ui=GameUi::playing;
 LineCandidate l;l.center={640,500};l.tangent={std::cos(line_angle),std::sin(line_angle)};l.length=1200;l.track_id=7;l.observed_ns=time;l.confidence=1;a.s.lines={l};
 GameTarget t;t.note_id=42;t.revision=sequence;t.line_id=7;t.note.kind=NoteKind::hold;t.note.center={x,front};t.note.tangent={std::cos(angle),std::sin(angle)};t.note.width=100;t.note.height=front-tail;t.note.rails_geometry=true;t.note.held_body_patch=patch;t.note.held_body_evidence=patch;t.note.head_on_line=std::abs(front-500)<5;
 const Vec2 n{-t.note.tangent.y,t.note.tangent.x};if(!patch)t.note.tail=Vec2{x-n.x*(front-tail),front-n.y*(front-tail)};
 t.hit={x,500};t.crossing_ns=time;t.samples=4;t.history_span_ns=40*ms;t.uncertainty_ns=2*ms;t.evidence_ns=time;t.expires_ns=time+100*ms;t.reason="prediction_observe_only";a.s.targets={t};
 a.b.context=a.s.context;a.b.lines=a.s.lines;a.b.playing_gate=true;TrackingCandidate c;c.candidate_id=1;c.note=t.note;c.quality=ObservationQuality::strong_current;c.head_visible=!patch;c.origin="synthetic_current_pixels";a.b.candidates={c};
 // Independent raster: transform every local shape to pixels; no production sampler is used.
 const auto u=t.note.tangent;const Vec2 v{-u.y,u.x};const double depth=patch?100:front-tail;
 for(int y=0;y<720;++y)for(int px=0;px<1280;++px){const double dx=px-x,dy=y-front,along=dx*u.x+dy*u.y,normal=dx*v.x+dy*v.y;auto* p=a.f.rgb.data()+std::size_t(y)*3840+px*3;
  const double lx=px-640,ly=y-500;if(std::abs(-lx*l.tangent.y+ly*l.tangent.x)<=.5&&std::abs(lx*l.tangent.x+ly*l.tangent.y)<=600)p[0]=p[1]=p[2]=255;
  if(normal>=-depth&&normal<=0&&std::abs(along)<=50){for(int k=0;k<3;++k)p[k]=static_cast<std::uint8_t>(rgb[k]);}
  if(normal>=-depth&&normal<=0&&std::abs(std::abs(along)-50)<=1){p[0]=p[1]=p[2]=255;}}
 return a;
}
struct Own {FakeClock clock;ReplayBackend backend{clock};SessionGameOwner owner{clock,backend,5,{3,0,30*ms}};ExecutionLedger ledger;Hook hook;
 Own(){owner.start(1);}bool sync(){auto p=owner.owner()->take_accepted_plans();auto e=backend.take_events();owner.owner()->take_coverage_updates();owner.owner()->take_plan_cancellations();return ledger.observe(p,e,*owner.scheduler());}
 pas::current_rails::Evaluation step(Input& i){clock.set(i.f.pixels_ready_ns);owner.poll();if(!sync())throw std::runtime_error("prior receipts");auto e=hook.evaluate(i.f,i.b,i.s,ledger,clock.now_ns());owner.accept(e.filtered,true);owner.poll();if(!sync())throw std::runtime_error(std::string(ledger.reason()));return e;}
 bool finish(){const auto r=owner.finish();backend.take_events();return ledger.finalize(r)&&r.failed_ids.empty()&&r.unknown_ids.empty()&&!backend.active_count();}};
void own_entry(Own& o) {for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);o.step(a);}}
Input body(int sequence=4,Nanoseconds time=70*ms) {auto a=make(sequence,time,550,200,410);auto change=[](NoteCandidate& n){n.tail.reset();n.held_body_evidence=true;n.head_on_line=false;n.rails_geometry=true;n.outline_evidence=true;};change(a.b.candidates[0].note);change(a.s.targets[0].note);return a;}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh active-body report required");Tests r;
 {Own o;own_entry(o);const int contact=o.ledger.find(42)?o.ledger.find(42)->contact:-1;auto a=body();const auto e=o.step(a);o.clock.set(90*ms);o.owner.poll();o.sync();r.check("own_down_before_no_tail_body",contact>=0&&o.backend.downs==1);r.check("active_no_tail_measured_depth_current_body_positive",e.accepted==1&&e.support[0].active&&e.support[0].body&&o.backend.moves>0&&o.ledger.find(42)->contact==contact);r.check("no_tail_body_cannot_grant_tail_or_new_down",o.backend.downs==1&&e.count==1&&!e.support[0].tail&&(e.filtered.targets.empty()||!e.filtered.targets[0].note.tail));r.check("positive_final_release",o.finish());}
 for(int negative=0;negative<12;++negative){Own o;own_entry(o);auto a=body();auto change=[&](auto fn){fn(a.b.candidates[0].note);fn(a.s.targets[0].note);};
  if(negative==0){for(std::size_t p=0;p<a.f.rgb.size();p+=3)if(a.f.rgb[p]==155&&a.f.rgb[p+1]==233&&a.f.rgb[p+2]==255)a.f.rgb[p]=a.f.rgb[p+1]=a.f.rgb[p+2]=30;}
  if(negative==1){for(int y=200;y<=550;++y)for(int x=459;x<=461;++x)for(int c=0;c<3;++c)a.f.rgb[(std::size_t(y)*1280+x)*3+c]=30;}
  if(negative==2){auto l=a.b.lines[0];l.track_id=8;l.center.y=501;a.b.lines.push_back(l);a.s.lines=a.b.lines;}
  if(negative==3)change([](auto&n){n.height=4096;});
  if(negative==4)change([](auto&n){n.height=std::numeric_limits<double>::quiet_NaN();});
  if(negative==5)change([](auto&n){n.height=0;});
  if(negative==6)change([](auto&n){n.held_body_evidence=false;});
  if(negative==7)change([](auto&n){n.rails_geometry=false;});
  if(negative==8)change([](auto&n){n.height=20;});
  if(negative==9)change([](auto&n){n.tangent={-1,0};}); // depth must stay behind the directed normal
  if(negative==10)change([](auto&n){n.center.y=450;}); // line is ahead of this alleged head
  if(negative==11){for(int y=480;y<=520;++y)for(int x=365;x<=455;++x)for(int c=0;c<3;++c)a.f.rgb[(std::size_t(y)*1280+x)*3+c]=30;} // visible head cannot replace current hit-region fill
  auto e=o.step(a);r.check("invalid_no_tail_body_denied_"+std::to_string(negative),e.accepted==0&&o.backend.downs==1);r.check("negative_release_"+std::to_string(negative),o.finish());
 }
 {Own o;auto a=body(1,10*ms);auto e=o.step(a);r.check("unowned_body_has_no_entry_permission",e.accepted==0&&o.backend.downs==0);r.check("unowned_final_release",o.finish());}
 {Own o;auto a=make(1,10*ms);auto&t=a.s.targets[0];t.note.kind=NoteKind::tap;t.note.tail.reset();t.note.rails_geometry=false;t.note.head_on_line=false;t.reason="prediction_observe_only";o.clock.set(10*ms);o.owner.accept(a.s,true);o.owner.poll();o.sync();o.clock.set(40*ms);o.owner.poll();o.sync();const auto*attachment=o.ledger.find(42);r.check("completed_fixture_has_real_down_up",attachment&&attachment->state==Execution::completed&&o.backend.downs==1&&o.backend.ups==1);auto b=body(2,70*ms);auto e=o.step(b);r.check("completed_no_tail_body_cannot_revive",e.accepted==0&&o.backend.downs==1&&o.backend.active_count()==0);r.check("completed_final_release",o.finish());}
 {Own o;own_entry(o);auto a=body();o.clock.set(a.f.pixels_ready_ns+100*ms);auto e=o.hook.evaluate(a.f,a.b,a.s,o.ledger,o.clock.now_ns());r.check("expired_body_frame_denied",!e.valid&&e.accepted==0&&o.backend.downs==1);r.check("expired_final_release",o.finish());}
 J out={{"schema","pas.current-active-no-tail-body.v1"},{"assertions",r.rows.size()},{"failed",r.failed},{"checks",r.rows},{"device_endpoints",0},{"physical_gold",0},{"new_down_permission",false},{"new_tail_permission",false}};std::ofstream f(argv[1]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("report write");return r.failed?1:0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
