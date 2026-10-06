#include "owner_candidate_step.hpp"
#include "pas/candidate_admission.hpp"
#include "pas/candidate_packet.hpp"
#include <fstream>
#include <iostream>
#include <type_traits>
using namespace pas;using namespace pas::current_rails;using namespace pas::current_execution;
using J=nlohmann::json;constexpr Nanoseconds ms=1'000'000;
struct Tests {J checks=J::array();int failed=0;void check(std::string id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;}};
struct SpyBackend:TouchBackend {
 const Clock& clock;int injections=0,releases=0;bool unknown=false,wrong=false,unknown_release=false;std::array<bool,5> active{};
 explicit SpyBackend(const Clock& c):clock(c){}
 TouchReceipt inject(const TouchCommand& c)override {++injections;if(c.contact_id>=0&&c.contact_id<5){if(c.phase==Phase::down)active[c.contact_id]=true;if(c.phase==Phase::up&&!unknown)active[c.contact_id]=false;}auto returned=c;if(wrong)returned.contact_id=(c.contact_id+1)%5;return {returned,clock.now_ns(),clock.now_ns(),!unknown,unknown?"unknown":"success"};}
 ReleaseReport release_all()override {++releases;ReleaseReport r;r.start_ns=r.return_ns=clock.now_ns();for(int i=0;i<5;++i)if(active[i]){r.requested_ids.push_back(i);if(unknown_release)r.unknown_ids.push_back(i);else active[i]=false;}return r;}
};
DecisionSnapshot owned_hold_scene() {
 DecisionSnapshot s;s.sequence=1;s.context={1,1,1,1,10*ms,1280,720,1};s.ui=GameUi::playing;s.playing_gate=true;
 LineCandidate l;l.track_id=7;l.center={640,500};l.tangent={1,0};l.length=1200;l.observed_ns=10*ms;l.confidence=1;s.lines={l};
 GameTarget t;t.note_id=42;t.revision=1;t.line_id=7;t.note.kind=NoteKind::hold;t.note.center={400,500};t.note.tangent={1,0};t.note.width=100;t.note.height=300;t.note.tail=Vec2{400,200};t.note.rails_geometry=true;t.note.head_on_line=true;t.hit={400,500};t.evidence_ns=10*ms;t.expires_ns=110*ms;t.crossing_ns=10*ms;t.samples=3;t.history_span_ns=40*ms;t.uncertainty_ns=2*ms;t.current_contact=GameTarget::CurrentContact::hold_front_rails;t.reason="current_rails_v1_measured_front_overlap";s.targets={t};return s;
}
void admission(Tests& t) {
 static_assert(!std::is_constructible_v<CandidateLiveAdmission,bool>);
 static_assert(!std::is_convertible_v<CandidateLiveAdmission,bool>);
 static_assert(!std::is_convertible_v<CandidateColdTestTag,CandidateLiveAdmission>);
 int loads=0,preflights=0,discoveries=0,backends=0;bool rejected=false;
 try{enter_current_candidate_manual_session(CandidateLiveAdmission{},[&]{++loads;++preflights;++discoveries;++backends;});}catch(const std::runtime_error&){rejected=true;}
 t.check("typed_UNQUALIFIED_rejects",rejected&&CandidateLiveAdmission{}.state()==CandidateLiveAdmission::State::unqualified);
 t.check("load_preflight_discovery_backend_spies_zero",loads==0&&preflights==0&&discoveries==0&&backends==0);
}
void packet(Tests& t) {
 auto f=std::make_shared<Frame>();f->sequence=f->epoch=f->generation=f->geometry_version=1;f->width=1280;f->height=720;f->stride=3840;f->source_rotation=1;f->capture_complete_ns=f->pixels_ready_ns=10*ms;f->rgb.resize(1280*720*3);
 CandidatePacketEvidence e;e.frame=f;e.batch.context={1,1,1,1,10*ms,1280,720,1};SessionObservation p;p.scene.context=e.batch.context;p.scene.playing_gate=true;p.status.round=1;p.status.active=true;p.status.state=PlaySessionState::playing;p.allow_down=true;
 t.check("same_consumed_frame_eligible",e.eligible(p,10*ms));
 const auto original=p;for(int i=0;i<8;++i){p=original;switch(i){case 0:++p.scene.context.frame;break;case 1:++p.scene.context.epoch;break;case 2:++p.scene.context.generation;break;case 3:++p.scene.context.geometry;break;case 4:++p.scene.context.capture_ns;break;case 5:++p.scene.context.width;break;case 6:++p.status.round;break;case 7:p.status.new_round=true;break;}t.check("scene_or_round_mismatch_"+std::to_string(i),!e.eligible(p,10*ms));}
 p=original;++e.batch.context.frame;t.check("batch_from_different_frame_denied",!e.eligible(p,10*ms));--e.batch.context.frame;
 p.scene.playing_gate=false;t.check("inconsistent_gate_denied",!e.eligible(p,10*ms));p=original;p.evidence.result=true;t.check("result_evidence_denied",!e.eligible(p,10*ms));p=original;p.status.ended=true;t.check("ended_packet_denied",!e.eligible(p,10*ms));p=original;t.check("stale_packet_denied",!e.eligible(p,110*ms));t.check("future_pixels_denied",!e.eligible(p,9*ms));
 e.batch.candidates.resize(129);t.check("batch_overflow_denied",!e.eligible(p,10*ms));
 FakeClock c;c.set(10*ms);LatestFrame pool(2,2,3,&c);Frame meta;meta.width=2;meta.height=2;meta.stride=6;meta.sequence=1;meta.epoch=meta.generation=meta.geometry_version=1;meta.capture_complete_ns=10*ms;std::array<std::uint8_t,12> rgb{};
 t.check("pool_publish1",pool.publish(rgb.data(),rgb.size(),meta));auto a=pool.read_after(0,0);rgb[0]=1;++meta.sequence;t.check("pool_publish2",pool.publish(rgb.data(),rgb.size(),meta));auto b=pool.peek();rgb[0]=2;++meta.sequence;t.check("pool_publish3",pool.publish(rgb.data(),rgb.size(),meta));auto d=pool.peek();const auto count=pool.counters();++meta.sequence;
 t.check("three_physical_leases_exhaust_pool",!pool.publish(rgb.data(),rgb.size(),meta)&&pool.counters().pool_drops==count.pool_drops+1);
 t.check("immutable_lease_retains_old_pixels",a->rgb[0]==0&&b->rgb[0]==1&&d->rgb[0]==2);
 a.reset();rgb[0]=3;++meta.sequence;t.check("released_slot_reused_and_latest_overwritten",pool.publish(rgb.data(),rgb.size(),meta)&&pool.peek()->rgb[0]==3&&pool.counters().overwritten>0);
 auto newest=pool.read_after(b->sequence,0);t.check("slow_consumer_reads_only_latest",newest&&newest->sequence==pool.peek()->sequence&&pool.counters().consumer_skips>0);
}
void stream_controls(Tests& t) {
 FakeClock c;c.set(10*ms);SpyBackend spy(c);ReceiptStream stream;RecordingBackend b(spy,stream,c);
 for(int i=0;i<64;++i)b.inject({std::uint64_t(i+1),0,Phase::up,400,500,10*ms,1});
 const auto r=b.inject({65,0,Phase::down,400,500,10*ms,1});t.check("full_receipt_buffer_blocks_new_injection",!r.success&&spy.injections==64&&stream.overflow());
 b.release_all();t.check("overflow_cannot_prevent_release",spy.releases==1);t.check("receipt_stream_hard_bound64",stream.take_events().size()==64);t.check("overflow_sticky_after_drain",!stream.can_inject()&&!stream.reset_after_verified_round());
 for(int mode=0;mode<2;++mode){SpyBackend s(c);ReceiptStream rs;RecordingBackend rb(s,rs,c);s.unknown=mode==0;s.wrong=mode==1;rb.inject({1,0,Phase::down,400,500,10*ms,1});rs.take_events();s.unknown=s.wrong=false;rb.release_all();rs.take_events();t.check("unknown_or_wrong_returned_command_sticky_"+std::to_string(mode),!rs.healthy()&&!rs.can_inject()&&!rs.reset_after_verified_round());}
 SpyBackend s(c);ReceiptStream rs;RecordingBackend rb(s,rs,c);rb.inject({1,0,Phase::down,400,500,10*ms,1});rs.take_events();s.unknown_release=true;const auto first=rb.release_all();rs.take_events();s.unknown_release=false;c.set(20*ms);const auto again=rb.release_all();const auto events=rs.take_events();
 t.check("first_unknown_release_never_retried",s.releases==1&&again.unknown_ids==first.unknown_ids&&again.start_ns==first.start_ns);
 t.check("cached_release_call_has_raw_non_dispatch_record",events.size()==1&&events[0].release&&!events[0].dispatched);
 t.check("unknown_release_permanent_no_reset",rs.first_uncertain_release()&&!rs.reset_after_verified_round());
}
void owner_controls(Tests& t,bool unknown) {
 FakeClock c;c.set(10*ms);SpyBackend b(c);ReceiptStream stream;RecordingBackend recorded(b,stream,c);SessionGameOwner owner(c,recorded,5,{15,35*ms,30*ms});OwnerCandidateStep step(c,owner,stream);std::vector<BackendEvent> events;step.receipt_output=[&](const auto&e){events.push_back(e);};
 const auto label=unknown?"unknown":"verified";t.check(std::string(label)+"_step_starts_actual_owner",step.start(1));owner.accept(owned_hold_scene(),true);owner.poll();t.check(std::string(label)+"_own_down_joined",step.sync()&&b.injections==1&&step.ledger.find(42)&&step.ledger.find(42)->state==Execution::active);events.clear();
 b.unknown_release=unknown;owner.scheduler()->set_dispatch_guard([]{return false;});c.set(20*ms);const bool polled=step.poll();
 t.check(std::string(label)+"_implicit_dispatch_guard_release_captured",polled==!unknown&&events.size()==1&&events[0].release&&events[0].report.requested_ids==std::vector<int>{0});
 const int before=b.releases;const bool finished=step.finish();const auto first=step.final_release;
 t.check(std::string(label)+"_finish_sticky_verdict",finished==!unknown);b.unknown_release=false;c.set(30*ms);const int after=b.releases;t.check(std::string(label)+"_repeat_finish_no_release",step.finish()==!unknown&&b.releases==after&&step.final_release.start_ns==first.start_ns);
 t.check(std::string(label)+"_new_round_verified_only",step.start(2)==!unknown);
 if(unknown)t.check("unknown_guard_release_no_extra_underlying_call",b.releases==before&&step.final_release.unknown_ids==std::vector<int>{0});else step.finish();
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh output report required");Tests t;admission(t);packet(t);stream_controls(t);owner_controls(t,false);owner_controls(t,true);
 J result={{"schema","pas.disabled-live-seam.contract.v1"},{"assertions",t.checks.size()},{"failed",t.failed},{"checks",t.checks},{"device_endpoints",0},{"live_admission","UNQUALIFIED"},{"changed_Windows_branch_executed",false},{"physical_gold",0}};std::ofstream f(argv[1]);f<<result.dump(2)<<'\n';if(!f)throw std::runtime_error("report write failed");std::cout<<"disabled live seam assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
