#include "owner_candidate_step.hpp"
#include "../../bvi_owner_cooperative/extract.hpp"
#include "../../bvi_owner_cooperative/cold_verification/observation_equal.hpp"
#include "relation_policy.hpp"
#include "pas/candidate_admission.hpp"
#include <fstream>
#include <iostream>
using namespace pas;using namespace pas::current_rails;using namespace pas::current_execution;
using J=nlohmann::json;constexpr Nanoseconds ms=1'000'000;
struct Tests{J checks=J::array();int failed=0;void check(const std::string& id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}};
struct Pixels{
 std::vector<std::uint8_t> rgb=std::vector<std::uint8_t>(1280*720*3);
 Frame frame{};DecisionSnapshot scene{};CandidateBatch batch{};
 Pixels(NoteKind kind=NoteKind::tap){
  for(int y=200;y<=503;++y)for(int x=350;x<=450;++x){const auto i=std::size_t(y)*3840+x*3;rgb[i]=50;rgb[i+1]=170;rgb[i+2]=220;if(x==350||x==450)rgb[i]=rgb[i+1]=rgb[i+2]=255;}
  frame.rgb=rgb;frame.width=1280;frame.height=720;frame.stride=3840;frame.epoch=frame.generation=frame.geometry_version=1;frame.source_rotation=1;frame.sequence=2;frame.capture_complete_ns=frame.pixels_ready_ns=12*ms;frame.source_valid=true;
  scene.sequence=2;scene.context={1,1,1,2,12*ms,1280,720,1};scene.ui=GameUi::playing;scene.playing_gate=true;
  LineCandidate line;line.track_id=7;line.center={640,500};line.tangent={1,0};line.length=1200;line.observed_ns=12*ms;line.confidence=1;scene.lines={line};
  GameTarget target;target.note_id=42;target.revision=2;target.line_id=7;target.note.kind=kind;target.note.center={400,500};target.note.tangent={1,0};target.note.width=100;target.note.height=kind==NoteKind::hold?300:8;
  if(kind==NoteKind::hold){target.note.tail=Vec2{400,200};target.note.rails_geometry=true;target.note.head_on_line=true;target.current_contact=GameTarget::CurrentContact::hold_front_rails;target.reason="current_rails_v1_measured_front_overlap";}else target.reason="prediction_observe_only";
  target.hit={400,500};target.evidence_ns=12*ms;target.expires_ns=112*ms;target.crossing_ns=12*ms;target.samples=3;target.history_span_ns=40*ms;target.uncertainty_ns=2*ms;scene.targets={target};
  batch.context=scene.context;batch.source_valid=true;batch.capacity_valid=true;batch.playing_gate=true;batch.ui=GameUi::playing;batch.lines=scene.lines;
  TrackingCandidate candidate;candidate.candidate_id=1;candidate.note=target.note;candidate.head_visible=candidate.body_visible=candidate.action_support=true;candidate.quality=ObservationQuality::strong_current;batch.candidates={candidate};
 }
 DecisionSnapshot initial(){auto s=scene;s.context.frame=s.sequence=1;s.context.capture_ns=10*ms;for(auto& l:s.lines)l.observed_ns=10*ms;for(auto& t:s.targets){t.revision=1;t.evidence_ns=10*ms;t.expires_ns=110*ms;t.crossing_ns=10*ms;}return s;}
};
struct Context{FakeClock clock;ReplayBackend backend{clock};SessionGameOwner owner{clock,backend,5,{15,35*ms,30*ms}};OwnerCandidateStep step{clock,owner,backend.receipts()};std::vector<BackendEvent> events;
 Context(){clock.set(10*ms);step.receipt_output=[&](const auto& e){events.push_back(e);};if(!step.start(1))throw std::runtime_error("round setup");}
 bool start(Pixels& pixels){owner.accept(pixels.initial(),true);return step.poll();}
};
void equivalence(Tests& t){
 bvi::Candidate donor;std::vector<std::uint8_t> rgb(3848*720);for(std::size_t i=0;i<rgb.size();++i)rgb[i]=std::uint8_t((i*37+i/67)%256);
 const bvi::Key key{{1,1,1,1},10*ms,10*ms,1,true};const bvi::View view{rgb,1280,720,3848,key};
 const std::array<bvi::Query,4> queries={bvi::Query{{400,500},100,300,0,false},bvi::Query{{500,500},100,8,0,true},bvi::Query{{600,400},80,260,.7,false},bvi::Query{{800,500},70,400,-.6,false}};
 const std::array<bvi::Line,2> lines={bvi::Line{{640,500},0,1200,7},bvi::Line{{800,400},.7,500,8}};
 std::size_t polls=0;const auto original=donor.extract(view,queries,lines);const auto actual=bvi::owner_cooperative::extract(donor,view,queries,lines,[&](){++polls;return true;});
 bool parts=true;for(std::size_t i=0;i<original.count;++i)parts&=bvi::same_measurement(original.parts[i],actual.parts[i])&&original.parts[i].probes==actual.parts[i].probes;
 t.check("full_rgb_signature_identical_with_padding",original.rgb_signature==actual.rgb_signature);t.check("all_descriptors_and_probes_identical",parts&&original.probes==actual.probes&&original.count==actual.count&&original.invalid==actual.invalid&&original.reason==actual.reason);t.check("aliases_identical",original.canonical==actual.canonical&&original.aliases_valid==actual.aliases_valid);t.check("bounded_row_and_roi_polling",polls>=23&&polls<=100);
 std::size_t stops=0;const auto stopped=bvi::owner_cooperative::extract(donor,view,queries,lines,[&](){return ++stops<3;});t.check("abort_during_full_hash",stopped.invalid&&stopped.reason=="owner checkpoint stopped"&&stopped.probes==64*1280&&stops==3);
 bool threw=false;try{bvi::owner_cooperative::extract(donor,view,queries,lines,[]()->bool{throw std::runtime_error("checkpoint fault");});}catch(const std::runtime_error&){threw=true;}t.check("checkpoint_fault_propagates",threw);
}
void uniform_equivalence(Tests& t){
 bvi::Candidate donor;const bvi::Key key{{1,1,1,1},10*ms,10*ms,1,true};
 const std::array<bvi::Query,4> queries={bvi::Query{{400,500},100,300,0,false},bvi::Query{{500,500},100,8,0,true},bvi::Query{{600,400},80,260,.7,false},bvi::Query{{800,500},70,400,-.6,false}};
 const std::array<bvi::Line,2> lines={bvi::Line{{640,500},0,1200,7},bvi::Line{{800,400},.7,500,8}};
 const auto parity=[&](const std::string& id,int width,int height,int padding,std::uint8_t fill,bool mixed){
  const int stride=width*3+padding;std::vector<std::uint8_t> storage(static_cast<std::size_t>(stride)*height+3,fill);auto rgb=std::span<std::uint8_t>(storage).subspan(3);
  if(mixed)for(std::size_t i=0;i<rgb.size();++i)if((i/8)%3==1)rgb[i]=static_cast<std::uint8_t>((i*37+i/67)%256);
  const bvi::View view{rgb,width,height,stride,key};const auto expected=donor.extract(view,queries,lines);std::size_t polls=0;
  const auto actual=bvi::owner_cooperative::extract(donor,view,queries,lines,[&](){++polls;return true;});
  t.check(id,bvi::owner_cooperative::cold_verification::observation_equal(expected,actual)&&polls>=static_cast<std::size_t>((height+31)/32)+queries.size());
 };
 for(const auto byte:{0,30,255})parity("uniform_full_rgb_donor_"+std::to_string(byte),1280,720,8,static_cast<std::uint8_t>(byte),false);
 parity("mixed_uniform_and_scalar_blocks_donor",1280,720,8,30,true);
 for(const auto width:{1,2,5,7,8,9,1279})parity("unaligned_row_tail_donor_"+std::to_string(width),width,33,5,30,true);
 constexpr int width=1279,stride=width*3+5,height=33;std::vector<std::uint8_t> rgb(stride*height,30);const bvi::View view{rgb,width,height,stride,key};
 const auto before=bvi::owner_cooperative::extract(donor,view,queries,lines,[](){return true;});
 for(int y=0;y<height;++y)for(int x=width*3;x<stride;++x)rgb[static_cast<std::size_t>(y)*stride+x]=255;
 const auto padded=bvi::owner_cooperative::extract(donor,view,queries,lines,[](){return true;});
 t.check("uniform_padding_never_enters_signature",bvi::owner_cooperative::cold_verification::observation_equal(before,padded));
 rgb[static_cast<std::size_t>(height-1)*stride+width*3-1]^=1;
 const auto changed=bvi::owner_cooperative::extract(donor,view,queries,lines,[](){return true;});
 t.check("uniform_last_rgb_byte_is_read",changed.rgb_signature!=padded.rgb_signature&&bvi::owner_cooperative::cold_verification::observation_equal(changed,donor.extract(view,queries,lines)));
 std::vector<std::uint8_t> full(3840*720,30);const bvi::View full_view{full,1280,720,3840,key};std::size_t polls=0;
 const auto stopped=bvi::owner_cooperative::extract(donor,full_view,queries,lines,[&](){return ++polls<3;});const bvi::View prefix{{full.data(),3840*64},1280,64,3840,key};
 t.check("uniform_abort_preserves_row_signature_and_probes",stopped.invalid&&stopped.reason=="owner checkpoint stopped"&&stopped.probes==64*1280&&polls==3&&stopped.rgb_signature==donor.extract(prefix,{},{}).rgb_signature);
}
void pending_up(Tests& t){Pixels pixels;Context c;t.check("real_tap_down_setup",c.start(pixels)&&c.backend.downs==1);c.clock.set(12*ms);std::size_t polls=0;
 const OwnerCheckpoint checkpoint{c.clock,[&](){++polls;c.clock.set(c.clock.now_ns()+ms);return c.step.poll();}};const auto evaluated=c.step.hook.evaluate(pixels.frame,pixels.batch,pixels.scene,c.step.ledger,12*ms,&checkpoint);
 const auto up=std::find_if(c.events.begin(),c.events.end(),[](const auto& e){return !e.release&&e.issued.phase==Phase::up;});
 t.check("up_dispatched_inside_rgb_extraction",up!=c.events.end()&&polls>16&&up->receipt.injection_start_ns==28*ms);t.check("completion_observed_before_current_filter",c.step.ledger.find(42)&&c.step.ledger.find(42)->state==Execution::completed&&evaluated.filtered.targets.empty());
 c.owner.accept(evaluated.filtered,true);t.check("completed_note_not_reborn",c.step.poll()&&c.backend.downs==1&&c.backend.ups==1&&c.backend.active_count()==0);t.check("cooperative_tap_release_verified",c.step.finish());
}
void expiry(Tests& t){Pixels pixels(NoteKind::hold);Context c;t.check("real_hold_down_setup",c.start(pixels)&&c.backend.active_count()==1);c.clock.set(12*ms);
 const OwnerCheckpoint checkpoint{c.clock,[&](){c.clock.set(c.clock.now_ns()+5*ms);return c.step.poll();}};const auto evaluated=c.step.hook.evaluate(pixels.frame,pixels.batch,pixels.scene,c.step.ledger,12*ms,&checkpoint);
 t.check("old_gate_expires_during_extraction",c.backend.active_count()==0&&c.step.ledger.find(42)&&c.step.ledger.find(42)->state==Execution::cancelled);t.check("resumed_clock_rejects_expired_current_frame",!evaluated.valid&&!evaluated.filtered.playing_gate&&!evaluated.filtered.capacity_valid&&evaluated.filtered.targets.empty());
 c.owner.accept(evaluated.filtered,false);t.check("expired_hold_not_reborn",c.step.poll()&&c.backend.downs==1);t.check("expiry_release_verified",c.step.finish());
}
void uncertainty(Tests& t){Pixels pixels;Context c;t.check("unknown_up_setup",c.start(pixels));c.backend.failure=ReplayBackend::Failure::unknown_up;c.clock.set(12*ms);std::size_t polls=0;
 const OwnerCheckpoint checkpoint{c.clock,[&](){++polls;c.clock.set(c.clock.now_ns()+ms);return c.step.poll();}};const auto evaluated=c.step.hook.evaluate(pixels.frame,pixels.batch,pixels.scene,c.step.ledger,12*ms,&checkpoint);
 t.check("unknown_injection_aborts_extraction",!evaluated.valid&&c.step.blocked()&&polls==16&&c.backend.ups==1);const auto downs=c.backend.downs;c.clock.set(60*ms);t.check("unknown_result_no_down_or_up_retry",!c.step.poll()&&c.backend.downs==downs&&c.backend.ups==1);
 Context d;d.backend.failure=ReplayBackend::Failure::unknown_down;t.check("unknown_down_setup",!d.start(pixels)&&d.backend.downs==1);d.clock.set(12*ms);t.check("unknown_down_remains_blocked",!d.step.accept(pixels.frame,pixels.batch,pixels.scene,true)&&d.backend.downs==1);
}
int main(int argc,char** argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh output");Tests t;equivalence(t);uniform_equivalence(t);pending_up(t);expiry(t);uncertainty(t);t.check("live_entry_stays_closed",CandidateLiveAdmission{}.state()==CandidateLiveAdmission::State::unqualified);J report={{"schema","pas.owner-cooperative-checkpoint-tests.v1"},{"assertions",t.checks.size()},{"checks",t.checks},{"failed",t.failed},{"device_endpoints",0}};std::ofstream out(argv[1]);out<<report.dump(2)<<'\n';if(!out)throw std::runtime_error("write");std::cout<<"assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
