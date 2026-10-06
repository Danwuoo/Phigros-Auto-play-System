#include "current.hpp"
#include "pas/game_session.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
using namespace pas;using namespace pas::current_rails;using namespace pas::current_execution;using J=nlohmann::json;
constexpr Nanoseconds ms=1'000'000;
struct Tests {J rows=J::array(),details=J::object();int failed=0;void check(std::string id,bool ok){rows.push_back({{"id",id},{"pass",ok}});failed+=!ok;std::cout<<id<<" "<<ok<<std::endl;}};
struct Input {Frame f;CandidateBatch b;DecisionSnapshot s;};
Input make(int sequence,Nanoseconds time,double front=500,double tail=200,double x=400,double angle=0,bool patch=false,std::array<int,3> rgb={155,233,255}){
 Input a;a.f.sequence=sequence;a.f.epoch=a.f.generation=a.f.geometry_version=1;a.f.width=1280;a.f.height=720;a.f.stride=3840;a.f.source_rotation=1;a.f.source_valid=true;a.f.capture_complete_ns=a.f.pixels_ready_ns=time;a.f.rgb.assign(1280*720*3,30);
 a.s.context={1,1,1,std::uint64_t(sequence),time,1280,720,1};a.s.sequence=sequence;a.s.playing_gate=true;a.s.ui=GameUi::playing;
 LineCandidate l;l.center={640,500};l.tangent={1,0};l.length=1200;l.track_id=7;l.observed_ns=time;l.confidence=1;a.s.lines={l};
 GameTarget t;t.note_id=42;t.revision=sequence;t.line_id=7;t.note.kind=NoteKind::hold;t.note.center={x,front};t.note.tangent={std::cos(angle),std::sin(angle)};t.note.width=100;t.note.height=front-tail;t.note.rails_geometry=true;t.note.held_body_patch=patch;t.note.held_body_evidence=patch;t.note.head_on_line=std::abs(front-500)<5;
 const Vec2 n{-t.note.tangent.y,t.note.tangent.x};if(!patch)t.note.tail=Vec2{x-n.x*(front-tail),front-n.y*(front-tail)};
 t.hit={x,500};t.crossing_ns=time;t.samples=4;t.history_span_ns=40*ms;t.uncertainty_ns=2*ms;t.evidence_ns=time;t.expires_ns=time+100*ms;t.reason="prediction_observe_only";a.s.targets={t};
 a.b.context=a.s.context;a.b.lines=a.s.lines;a.b.playing_gate=true;TrackingCandidate c;c.candidate_id=1;c.note=t.note;c.quality=ObservationQuality::strong_current;c.head_visible=!patch;c.origin="synthetic_current_pixels";a.b.candidates={c};
 // Independent raster: transform every local shape to pixels; no production sampler is used.
 const auto u=t.note.tangent;const Vec2 v{-u.y,u.x};const double depth=patch?100:front-tail;
 for(int y=0;y<720;++y)for(int px=0;px<1280;++px){const double dx=px-x,dy=y-front,along=dx*u.x+dy*u.y,normal=dx*v.x+dy*v.y;auto* p=a.f.rgb.data()+std::size_t(y)*3840+px*3;
  if(normal>=-depth&&normal<=0&&std::abs(along)<=50){for(int k=0;k<3;++k)p[k]=static_cast<std::uint8_t>(rgb[k]);}
  if(normal>=-depth&&normal<=0&&std::abs(std::abs(along)-50)<=1){p[0]=p[1]=p[2]=255;}}
 return a;
}
struct Own {FakeClock clock;ReplayBackend backend{clock};SessionGameOwner owner{clock,backend,5,{3,0,30*ms}};ExecutionLedger ledger;Hook hook;
 Own(){owner.start(1);}bool sync(){auto p=owner.owner()->take_accepted_plans();auto e=backend.take_events();owner.owner()->take_coverage_updates();owner.owner()->take_plan_cancellations();return ledger.observe(p,e,*owner.scheduler());}
 pas::current_rails::Evaluation step(Input& i){clock.set(i.f.pixels_ready_ns);owner.poll();if(!sync())throw std::runtime_error("prior receipts");auto e=hook.evaluate(i.f,i.b,i.s,ledger,clock.now_ns());owner.accept(e.filtered,true);owner.poll();if(!sync())throw std::runtime_error(std::string(ledger.reason()));return e;}
 bool finish(){const auto r=owner.finish();backend.take_events();return r.failed_ids.empty()&&r.unknown_ids.empty()&&!backend.active_count();}};
void entry(Tests& r,bool unknown=false){Own o;if(unknown)o.backend.failure=ReplayBackend::Failure::unknown_down;
 for(int i=0;i<3;++i){auto a=make(i+1,(10+i*20)*ms,460+i*20);auto e=o.step(a);r.check("hold_warming_"+std::to_string(i)+std::to_string(unknown),e.valid&&e.accepted==(i==2?1:0));}
 r.check(unknown?"own_unknown_down":"own_hold_down",o.backend.downs==1&&o.ledger.find(42)&&o.ledger.find(42)->state==(unknown?Execution::unknown:Execution::active));
 if(unknown){auto a=make(4,70*ms,520);o.step(a);r.check("unknown_down_not_retried",o.backend.downs==1&&o.backend.active_count()==0);r.check("unknown_release_retained",o.owner.scheduler()->fault()=="input_result_unknown");return;}
 const auto* first=o.ledger.find(42);if(!first){r.check("entry_receipt_missing",false);o.finish();return;}const auto finger=first->contact;
 for(int i=0;i<4;++i){auto a=make(4+i,(70+i*20)*ms,530+i*6,200,405+i*3,.08*i);auto e=o.step(a);r.check("rotating_active_body_"+std::to_string(i),e.accepted==1&&e.support[0].body&&o.ledger.find(42)&&o.ledger.find(42)->contact==finger&&o.backend.downs==1);}
 r.check("current_support_moves_same_contact",o.backend.moves>0);
 auto patch=make(8,150*ms,516,0,417,0,true);auto e=o.step(patch);r.check("body_patch_continues_own_receipt",e.accepted==1&&!e.support[0].terminal&&o.backend.downs==1);
 o.clock.set(260*ms);o.owner.poll();o.sync();r.check("evidence_expiry_releases",o.backend.active_count()==0&&!o.owner.scheduler()->last_release().requested_ids.empty()&&o.owner.scheduler()->last_release().failed_ids.empty()&&o.owner.scheduler()->last_release().unknown_ids.empty());
 auto returned=make(9,270*ms);o.step(returned);r.check("completed_return_cannot_down",o.backend.downs==1);r.check("final_release_verified",o.finish());
}
int main(int argc,char**argv){Tests r;try{if(argc!=2||std::filesystem::exists(argv[1]))return 2;entry(r);entry(r,true);
 for(const auto color:{std::array<int,3>{155,233,255},std::array<int,3>{192,226,237},std::array<int,3>{40,190,255}}){Own o;pas::current_rails::Evaluation e;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20,200,400,0,false,color);e=o.step(a);}r.check("cyan_variant_entry_"+std::to_string(color[0]),e.accepted==1&&o.backend.downs==1);o.finish();}
 for(int negative=0;negative<10;++negative){Own o;pas::current_rails::Evaluation e;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);
  if(negative==0){a.f.source_rotation=2;a.b.context.rotation=a.s.context.rotation=2;}
  if(negative==1)a.b.context.frame++;
  if(negative==2)a.f.rgb.pop_back();
  if(negative==3){a.b.lines[0].association_valid=false;a.s.lines=a.b.lines;}
  if(negative==4){auto l=a.b.lines[0];l.track_id=8;l.center.y=501;a.b.lines.push_back(l);a.s.lines=a.b.lines;}
  if(negative==5){a.b.candidates[0].note.held_body_patch=true;a.s.targets[0].note.held_body_patch=true;}
  if(negative==6){auto t=a.s.targets[0];t.note_id=43;a.s.targets.push_back(t);}
  if(negative==7){a=make(k+1,(10+k*20)*ms,460+k*20,200,400,0,false,{230,190,70});}
  if(negative==8){for(int y=200;y<=500;++y)for(int edge:{350,450})for(int d=-2;d<=2;++d)for(int c=0;c<3;++c)a.f.rgb[std::size_t(y)*3840+(edge+d)*3+c]=30;}
  if(negative==9){a.s.targets[0].line_projection_only=true;}
  e=o.step(a);}
  r.check("negative_never_down_"+std::to_string(negative),o.backend.downs==0&&e.accepted==0);o.finish();}
 // Capacity is whole-batch invalid, including unsupported entries.
 {Own o;auto a=make(1,10*ms);a.b.candidates.resize(129,a.b.candidates[0]);auto e=o.step(a);r.check("129_complete_batch_invalid",!e.valid&&o.backend.downs==0);o.finish();}
 {Own o;auto a=make(1,10*ms);a.b.lines.resize(17,a.b.lines[0]);auto e=o.step(a);r.check("17_lines_invalid",!e.valid&&o.backend.downs==0);o.finish();}
 {Own o;auto a=make(1,10*ms);a.b.candidates[0].note.center.x=std::numeric_limits<double>::infinity();a.s.targets[0].note=a.b.candidates[0].note;auto e=o.step(a);r.check("nonfinite_geometry_no_down",e.accepted==0);o.finish();}
 // Unsupported neighbouring proposals still compete; they are not dropped to
 // make the selected head unique. Geometry is generated independently.
 {Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);auto c=a.b.candidates[0];c.candidate_id=2;c.note.center.x+=15;c.quality=ObservationQuality::weak_current;a.b.candidates.push_back(c);o.step(a);}r.check("neighbour_not_filtered_out_for_entry",o.backend.downs==0);o.finish();}
 // Loss and reconnect retain completion and require a new current association.
 {Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);o.step(a);}auto lost=make(4,70*ms,520);lost.b.lines.clear();lost.s.lines.clear();o.step(lost);r.check("line_loss_no_new_down",o.backend.downs==1);o.clock.set(180*ms);o.owner.poll();o.sync();auto reconnect=make(5,190*ms);o.step(reconnect);r.check("line_reconnect_completed_no_replay",o.backend.downs==1&&o.backend.active_count()==0);o.finish();}
 // The note's tangent can approach alignment late. Distant current normals are
 // not a pairing prerequisite; only measured local overlap qualifies entry.
 {Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20,200,400,.16-.08*k);o.step(a);}r.check("late_note_alignment_entry",o.backend.downs==1);o.finish();}
 {Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);o.step(a);}for(int k=0;k<4;++k){auto a=make(k+4,(70+k*20)*ms,530,200,400,.06*k);a.b.lines[0].tangent={std::cos(.05*k),std::sin(.05*k)};a.s.lines=a.b.lines;o.step(a);}r.check("line_rotates_during_hold_same_down",o.backend.downs==1&&o.backend.moves>0);o.finish();}
 // All five contacts come from this candidate's own Down receipts.
 {Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20,200,140);for(int j=1;j<5;++j){auto extra=make(k+1,(10+k*20)*ms,460+k*20,200,140+j*220);for(std::size_t p=0;p<a.f.rgb.size();++p)if(extra.f.rgb[p]!=30)a.f.rgb[p]=extra.f.rgb[p];auto c=extra.b.candidates[0];c.candidate_id=j+1;a.b.candidates.push_back(c);auto t=extra.s.targets[0];t.note_id=42+j;a.s.targets.push_back(t);}o.step(a);}r.check("five_own_contacts_bounded",o.backend.downs==5&&o.backend.active_count()==5);r.check("five_finish_release",o.finish());}
 // Irregular business dt, too old evidence and front reversal never replay Down.
 {Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+(k==1?17:k==2?43:0))*ms,460+k*20);o.step(a);}r.check("irregular_dt_measured_entry",o.backend.downs==1);o.finish();}
 {Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,k==0?470:k==1?460:500);o.step(a);}r.check("reversing_front_no_entry",o.backend.downs==0);o.finish();}
 // Semantic owner proof is not interchangeable with the old reason string.
 {Own o;auto a=make(1,10*ms);auto t=a.s.targets[0];t.reason="current_rails_v1_measured_front_overlap";a.s.targets={t};o.clock.set(10*ms);o.owner.accept(a.s,true);o.owner.poll();o.sync();r.check("untyped_current_reason_no_down",o.backend.downs==0);o.finish();}
 for(bool unknown:{false,true}){Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);o.step(a);}for(int k=0;k<2;++k){auto a=make(k+4,(70+k*20)*ms,540,502);auto e=o.step(a);r.check("own_tail_current_supported_"+std::to_string(k)+std::to_string(unknown),e.accepted==1&&e.support[0].tail);}if(unknown)o.backend.failure=ReplayBackend::Failure::unknown_up;o.clock.set(110*ms);o.owner.poll();o.sync();r.check(unknown?"unknown_tail_up_fault":"normal_tail_up_receipt",o.backend.ups==1&&(unknown?!o.owner.scheduler()->fault().empty():o.backend.active_count()==0));r.check("tail_completion_no_down_retry_"+std::to_string(unknown),o.backend.downs==1);if(!unknown)o.finish();}
 for(auto failure:{ReplayBackend::Failure::unknown_move,ReplayBackend::Failure::release_unknown}){Own o;for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);o.step(a);}o.backend.failure=failure;if(failure==ReplayBackend::Failure::unknown_move){auto a=make(4,70*ms,530,200,410);o.step(a);o.clock.set(90*ms);o.owner.poll();o.sync();}else{o.owner.scheduler()->cancel("owned_release_probe");}r.check("fault_no_down_retry_"+std::to_string(int(failure)),o.backend.downs==1);if(failure==ReplayBackend::Failure::release_unknown)r.check("unknown_release_explicit",!o.owner.scheduler()->last_release().unknown_ids.empty());else r.check("unknown_injection_fault_explicit_"+std::to_string(int(failure)),!o.owner.scheduler()->fault().empty());}
 // Retirement is exercised by own owner plans and successful receipts, not
 // manually supplied cursors. Fixed 128 slots, then old IDs fail closed.
 {Own o;bool valid=true;for(int k=0;k<150;++k){auto a=make(k+1,(10+k*150)*ms);auto t=a.s.targets[0];t.note_id=k+100;t.note.kind=NoteKind::tap;t.note.tail.reset();t.note.rails_geometry=false;t.note.head_on_line=false;t.reason="prediction_observe_only";a.s.targets={t};o.clock.set(a.f.pixels_ready_ns);o.owner.accept(a.s,true);o.owner.poll();valid&=o.sync();o.clock.set(a.f.pixels_ready_ns+20*ms);o.owner.poll();valid&=o.sync();if(!valid)break;}r.details["ledger_retirement"]={{"valid",valid},{"down",o.backend.downs},{"up",o.backend.ups},{"size",o.ledger.size()},{"old_present",bool(o.ledger.find(100))},{"ledger_reason",o.ledger.reason()},{"owner_rejection",o.owner.owner()->last_rejection()},{"scheduler_fault",o.owner.scheduler()->fault()}};r.check("ledger_retirement_150_own_completed",valid&&o.backend.downs==150&&o.ledger.size()==128&&!o.ledger.find(100));o.finish();}
 J j={{"schema","pas.current-rails-v1.contract.v1"},{"checks",r.rows},{"details",r.details},{"assertions",r.rows.size()},{"failed",r.failed},{"device_endpoints",0},{"physical_gold",0}};std::ofstream out(argv[1]);out<<j.dump(2)<<'\n';std::cout<<"current checks="<<r.rows.size()<<" failed="<<r.failed<<'\n';return r.failed?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
