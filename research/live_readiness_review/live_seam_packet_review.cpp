// Independent bounded admission, packet, lifetime, and recording controls.
#include "pas/candidate_admission.hpp"
#include "pas/candidate_packet.hpp"
#include "receipt_stream.hpp"
#include "current.hpp"
#include "cost_workload_family_v2.hpp"
#include <fstream>
#include <iostream>
#include <type_traits>
using namespace pas;
using namespace pas::current_execution;
using namespace pas::current_rails;
using J=nlohmann::json;
constexpr Nanoseconds ms=1'000'000;
struct Tests{J checks=J::array(),details=J::array();int failed=0;void check(std::string id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}};
Frame fixed_frame(int phase,std::uint64_t sequence){Frame f;f.sequence=sequence;f.epoch=f.generation=f.geometry_version=1;
 f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.capture_complete_ns=f.pixels_ready_ns=1'000*ms+Nanoseconds(sequence)*20*ms;
 f.rgb=cost_family_v2::image(phase,0,cost_family_v2::Control::positive);return f;}
struct PacketFixture{CandidatePacketEvidence packet;SessionObservation observation;Nanoseconds now=0;};
PacketFixture make_packet(){FakeClock clock;GameObserver observer(clock);PacketFixture out;
 for(int phase=0;phase<=20;++phase){auto f=std::make_shared<const Frame>(fixed_frame(phase,std::uint64_t(phase+1)));clock.set(f->pixels_ready_ns);
  auto s=observer.process(*f);out.packet.frame=std::move(f);out.packet.batch=observer.candidate_batch();out.observation.scene=std::move(s);}
 out.now=clock.now_ns();out.observation.status.state=PlaySessionState::playing;out.observation.status.active=true;out.observation.status.round=1;
 out.observation.evidence.hud=true;out.observation.allow_down=true;return out;}
void admission(Tests&t){CandidateLiveAdmission live;CandidateColdTestTag cold;(void)cold;
 t.check("only_constructible_live_state_UNQUALIFIED",live.state()==CandidateLiveAdmission::State::unqualified);
 t.check("cold_tag_cannot_convert_to_live_admission",!std::is_convertible_v<CandidateColdTestTag,CandidateLiveAdmission>&&!std::is_constructible_v<CandidateLiveAdmission,CandidateColdTestTag>);
 bool ran=false,threw=false;std::string reason;try{enter_current_candidate_manual_session(live,[&]{ran=true;});}catch(const std::exception&e){threw=true;reason=e.what();}
 t.check("live_barrier_rejects_before_continuation",threw&&!ran&&reason.find("UNQUALIFIED")!=std::string::npos);
 for(int k=0;k<3;++k){bool later=false;try{enter_current_candidate_manual_session(live,[&]{later=true;});}catch(...){}t.check("repeated_live_admission_remains_disabled_"+std::to_string(k),!later);}
}
void packet_identity(Tests&t){const auto base=make_packet();
 static_assert(std::is_same_v<decltype(CandidatePacketEvidence::frame),std::shared_ptr<const Frame>>);
 t.check("actual_same_frame_complete_packet_positive",base.packet.eligible(base.observation,base.now)&&base.observation.scene.playing_gate&&base.packet.batch.lines.size()>0);
 const std::vector<std::string> names={"missing_lease","frame_epoch","frame_generation","frame_geometry","frame_sequence","frame_capture","frame_width","frame_height","frame_rotation","frame_stride","frame_RGB_size","frame_invalid_source","batch_context","scene_context","round_identity","new_round","inactive_round","not_playing","allow_down_false","batch_invalid_source","batch_invalid_capacity","scene_invalid_capacity","batch_candidate_overflow","batch_line_overflow","scene_target_overflow","expired_at_100ms","future_pixels","negative_capture","capture_overflow","scene_gate_false","result_UI_true","round_ended"};
 for(std::size_t k=0;k<names.size();++k){auto a=base;auto f=std::make_shared<Frame>(*base.packet.frame);a.packet.frame=f;
  if(k==0)a.packet.frame.reset();if(k==1)++f->epoch;if(k==2)++f->generation;if(k==3)++f->geometry_version;if(k==4)++f->sequence;
  if(k==5)++f->capture_complete_ns;if(k==6)--f->width;if(k==7)--f->height;if(k==8)f->source_rotation=0;if(k==9)--f->stride;
  if(k==10)f->rgb.pop_back();if(k==11)f->source_valid=false;if(k==12)++a.packet.batch.context.frame;if(k==13)++a.observation.scene.context.frame;
  if(k==14)++a.observation.status.round;if(k==15)a.observation.status.new_round=true;if(k==16)a.observation.status.active=false;
  if(k==17)a.observation.status.state=PlaySessionState::starting;if(k==18)a.observation.allow_down=false;if(k==19)a.packet.batch.source_valid=false;
  if(k==20)a.packet.batch.capacity_valid=false;if(k==21)a.observation.scene.capacity_valid=false;
  if(k==22)a.packet.batch.candidates.resize(129);if(k==23)a.packet.batch.lines.resize(17);if(k==24)a.observation.scene.targets.resize(129);
  if(k==25)a.now=f->capture_complete_ns+100*ms;if(k==26)f->pixels_ready_ns=a.now+1;if(k==27)f->capture_complete_ns=-1;
  if(k==28)f->capture_complete_ns=std::numeric_limits<Nanoseconds>::max();if(k==29)a.observation.scene.playing_gate=false;
  if(k==30)a.observation.evidence.result=true;if(k==31)a.observation.status.ended=true;
  t.check("packet_denies_"+names[k],!a.packet.eligible(a.observation,a.now));
 }
 ExecutionLedger ledger;Hook hook;
 auto first=hook.evaluate(*base.packet.frame,base.packet.batch,base.observation.scene,ledger,base.now);
 auto repeated=hook.evaluate(*base.packet.frame,base.packet.batch,base.observation.scene,ledger,base.now);
 t.check("nonmonotonic_same_frame_cannot_refresh_hook",first.valid&&!repeated.valid&&repeated.accepted==0);
 auto subset=base;subset.packet.batch.lines.pop_back();Hook complete;
 auto mismatched=complete.evaluate(*subset.packet.frame,subset.packet.batch,subset.observation.scene,ledger,subset.now);
 t.check("incomplete_line_subset_denied_by_owner_filter",!mismatched.valid&&mismatched.accepted==0);
}
void frame_lifetime(Tests&t){FakeClock clock;LatestFrame latest(1280,720,3,&clock);auto f=fixed_frame(0,1);clock.set(f.pixels_ready_ns);
 t.check("pool_first_publish",latest.publish(f.rgb.data(),f.rgb.size(),f));CandidatePacketEvidence p;p.frame=latest.read_after(0,0);auto weak=std::weak_ptr<const Frame>(p.frame);const auto first_pixels=p.frame->rgb;
 std::vector<std::shared_ptr<const Frame>> held;for(int k=2;k<=3;++k){f=fixed_frame(k-1,std::uint64_t(k));clock.set(f.pixels_ready_ns);latest.publish(f.rgb.data(),f.rgb.size(),f);held.push_back(latest.read_after(std::uint64_t(k-1),0));}
 f=fixed_frame(3,4);clock.set(f.pixels_ready_ns);
 t.check("retained_packet_three_slot_pool_bounded",!latest.publish(f.rgb.data(),f.rgb.size(),f)&&latest.counters().pool_drops==1);
 t.check("retained_immutable_lease_not_overwritten",p.frame->sequence==1&&p.frame->rgb==first_pixels&&!weak.expired());
 held.clear();latest.close();t.check("packet_survives_closed_mailbox",p.frame&&p.frame->rgb==first_pixels&&!weak.expired());p.frame.reset();t.check("consumer_lease_released_when_packet_drops",weak.use_count()==1);
}
// The following stub only checks decorator forwarding/limits. It is never
// treated as owner evidence and supplies no ledger plans, cursors, or labels.
struct FixedBackend final:TouchBackend{const Clock&clock;int injections=0,releases=0;bool active=false,unknown_release=false,mismatched=false,throw_injection=false,throw_release=false;
 explicit FixedBackend(const Clock&c):clock(c){}
 TouchReceipt inject(const TouchCommand&c)override{++injections;if(c.phase==Phase::down)active=true;if(c.phase==Phase::up)active=false;
  if(throw_injection)throw std::runtime_error("fixed_backend_injection_throw");
  auto returned=c;if(mismatched)++returned.contact_id;return {returned,clock.now_ns(),clock.now_ns(),true,"bounded_independent_stub"};}
 ReleaseReport release_all()override{++releases;if(throw_release)throw std::runtime_error("fixed_backend_release_throw");ReleaseReport r;r.start_ns=r.return_ns=clock.now_ns();if(active){r.requested_ids={0};if(unknown_release)r.unknown_ids={0};else active=false;}return r;}};
J report_value(const ReleaseReport&r){return {{"requested",r.requested_ids},{"failed",r.failed_ids},{"unknown",r.unknown_ids},{"start",r.start_ns},{"return",r.return_ns}};}
void decorator(Tests&t){
 {FakeClock clock;clock.set(ms);FixedBackend underlying(clock);ReceiptStream stream;RecordingBackend backend(underlying,stream,clock);
  TouchCommand down{7,0,Phase::down,400,500,ms,21};backend.inject(down);auto events=stream.take_events();
  t.check("decorator_records_issued_and_returned_once",underlying.injections==1&&events.size()==1&&!events[0].release&&events[0].issued.intent_id==7&&events[0].receipt.command.intent_id==7&&stream.active_count()==1);
  underlying.unknown_release=true;const auto first=backend.release_all();events=stream.take_events();
  t.check("decorator_first_unknown_release_sticky",underlying.releases==1&&stream.first_uncertain_release()&&report_value(*stream.first_uncertain_release())==report_value(first)&&stream.active_count()==1&&!stream.healthy());
  underlying.unknown_release=false;clock.set(2*ms);const auto later=backend.release_all();events=stream.take_events();
  t.check("decorator_unknown_release_not_retried",underlying.releases==1&&report_value(later)==report_value(first)&&stream.active_count()==1&&!underlying.unknown_release);
  t.check("decorator_repeat_report_marks_no_dispatch",events.empty()||(events.size()==1&&events[0].release&&!events[0].dispatched));
  t.check("decorator_unknown_cannot_reset_round",!stream.reset_after_verified_round());
 }
 {FakeClock clock;FixedBackend underlying(clock);ReceiptStream stream;RecordingBackend backend(underlying,stream,clock);TouchCommand down{9,0,Phase::down,400,500,0,1};
  for(std::size_t k=0;k<ReceiptStream::capacity;++k){auto command=down;command.phase=k?Phase::move:Phase::down;backend.inject(command);}const auto before=underlying.injections;
  const auto denied=backend.inject(down);t.check("overflow_stops_before_backend_injection",stream.overflow()&&!denied.success&&underlying.injections==before);
  const auto release=backend.release_all();t.check("overflow_does_not_suppress_safety_release",underlying.releases==1&&!underlying.active&&release.unknown_ids.empty());
  t.check("overflow_receipt_capacity_never_expands",stream.take_events().size()<=ReceiptStream::capacity&&!stream.healthy()&&!stream.reset_after_verified_round());
 }
 {FakeClock clock;FixedBackend underlying(clock);ReceiptStream stream;RecordingBackend backend(underlying,stream,clock);underlying.mismatched=true;backend.inject({11,0,Phase::down,400,500,0,1});
  t.check("mismatched_return_keeps_worst_case_contact",!stream.healthy()&&stream.active_count()>=1);const auto event=stream.take_events();
  t.check("mismatched_return_never_rewrites_issued",event.size()==1&&event[0].issued.contact_id==0&&event[0].receipt.command.contact_id==1);
 }
 {FakeClock clock;FixedBackend underlying(clock);ReceiptStream stream;RecordingBackend backend(underlying,stream,clock);underlying.throw_injection=true;bool threw=false;
  try{backend.inject({13,0,Phase::down,400,500,0,1});}catch(...){threw=true;}const auto events=stream.take_events();
  t.check("backend_throw_records_original_unknown_Down",threw&&underlying.injections==1&&events.size()==1&&!events[0].release&&!events[0].receipt.success&&events[0].issued.intent_id==13&&events[0].receipt.command.intent_id==13&&stream.active_count()==1&&!stream.healthy());
  underlying.throw_injection=false;const auto denied=backend.inject({13,0,Phase::down,400,500,0,1});
  t.check("backend_throw_unknown_Down_cannot_retry",!denied.success&&underlying.injections==1);
  backend.release_all();t.check("known_release_after_unknown_command_cannot_reset",!stream.healthy()&&!stream.reset_after_verified_round());
 }
 {FakeClock clock;FixedBackend underlying(clock);ReceiptStream stream;RecordingBackend backend(underlying,stream,clock);backend.inject({15,0,Phase::down,400,500,0,1});stream.take_events();underlying.throw_release=true;bool threw=false;
  try{backend.release_all();}catch(...){threw=true;}const auto events=stream.take_events();const auto first=stream.first_uncertain_release();
  t.check("backend_release_throw_preserves_bounded_unknown_report",threw&&underlying.releases==1&&first&&first->unknown_ids==std::vector<int>({0,1,2,3,4})&&events.size()==1&&events[0].release&&report_value(events[0].report)==report_value(*first));
  underlying.throw_release=false;const auto repeated=backend.release_all();
  t.check("backend_release_throw_not_retried_by_catch_path",underlying.releases==1&&first&&report_value(repeated)==report_value(*first)&&!stream.healthy());
 }
 {FakeClock clock;FixedBackend underlying(clock);ReceiptStream stream;RecordingBackend backend(underlying,stream,clock);
  for(std::size_t k=0;k<ReceiptStream::capacity;++k)backend.inject({17,0,k?Phase::move:Phase::down,400,500,0,1});underlying.unknown_release=true;
  const auto first=backend.release_all();t.check("full_receipt_buffer_keeps_first_unknown_release",stream.overflow()&&stream.first_uncertain_release()&&report_value(first)==report_value(*stream.first_uncertain_release())&&stream.active_count()==1);
  stream.take_events();underlying.unknown_release=false;backend.release_all();t.check("full_buffer_unknown_release_cannot_retry_after_drain",underlying.releases==1&&stream.active_count()==1&&!stream.healthy());
 }
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh packet reviewer report required");Tests t;admission(t);packet_identity(t);frame_lifetime(t);decorator(t);
 J out={{"schema","pas.independent-disabled-live-packet-review.v1"},{"assertions",t.checks.size()},{"failed",t.failed},{"checks",t.checks},{"details",t.details},{"device_endpoints",0},{"physical_gold",0},{"Windows_qualification",false}};
 std::ofstream file(argv[1]);file<<out.dump(2)<<'\n';if(!file)throw std::runtime_error("report write");std::cout<<"independent live packet assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
