// Reuse the frozen pixel/owner fixture, keeping its original 22 checks intact.
#define main cooperative_original_test_entry
#include "owner_checkpoint_tests.cpp"
#undef main
void expired_extraction(Tests& t){
 Pixels p;Context c;const auto original=c.start(p);t.check("tap_before_expiry",original&&c.backend.downs==1);
 c.clock.set(12*ms);std::size_t calls=0;
 const OwnerCheckpoint checkpoint{c.clock,[&](){++calls;if(calls==8)c.clock.set(135*ms);return c.step.poll();}};
 bvi::Candidate candidate;const auto e=pas::current_execution::evaluate(candidate,p.frame,p.batch,p.scene,c.step.ledger,12*ms,&checkpoint);
 t.check("expired_current_pixels_are_rejected",e.filtered.targets.empty()&&!e.filtered.playing_gate&&!e.filtered.capacity_valid);
 t.check("expiry_is_not_owner_fault",!e.interrupted&&c.step.ledger.valid()&&!c.step.blocked());
 c.owner.accept(e.filtered,false);t.check("no_replay_after_expiry",c.step.poll()&&c.backend.downs==1&&c.backend.active_count()==0);
 t.check("expired_tap_finishes_verified",c.step.finish());
}
void expiry_boundary(Tests& t){for(auto age:{100*ms-1,100*ms,100*ms+1}){
 Pixels p;Context c;c.clock.set(12*ms);const OwnerCheckpoint checkpoint{c.clock,[&](){c.clock.set(12*ms+age);return c.step.poll();}};
 bvi::Candidate candidate;const auto e=pas::current_execution::evaluate(candidate,p.frame,p.batch,p.scene,c.step.ledger,12*ms,&checkpoint);
 t.check("100ms_boundary_"+std::to_string(age),!e.interrupted&&(age<100*ms?e.filtered.capacity_valid:!e.filtered.capacity_valid));
 t.check("expiry_boundary_no_down_"+std::to_string(age),c.backend.downs==0&&c.step.ledger.valid());
}}
void unknown_and_reversed(Tests& t){
 Pixels p;Context c;c.start(p);c.backend.failure=ReplayBackend::Failure::unknown_up;c.clock.set(12*ms);
 const OwnerCheckpoint unknown{c.clock,[&](){c.clock.set(28*ms);return c.step.poll();}};bvi::Candidate candidate;
 const auto u=pas::current_execution::evaluate(candidate,p.frame,p.batch,p.scene,c.step.ledger,12*ms,&unknown);
 t.check("unknown_is_fatal_not_stale_rejection",u.interrupted&&c.step.blocked()&&c.backend.ups==1);c.clock.set(140*ms);c.step.poll();t.check("unknown_up_no_retry",c.backend.ups==1&&c.backend.downs==1);
 Context d;d.clock.set(12*ms);const OwnerCheckpoint reverse{d.clock,[&](){d.clock.set(11*ms);return d.step.poll();}};bvi::Candidate second;
 const auto r=pas::current_execution::evaluate(second,p.frame,p.batch,p.scene,d.step.ledger,12*ms,&reverse);
 t.check("clock_reversal_still_fatal",r.interrupted&&r.filtered.targets.empty());
}
void commit_expiry(Tests& t){for(bool candidate:{false,true}){
 Pixels p(NoteKind::hold);Context c;t.check("hold_setup_"+std::to_string(candidate),c.start(p)&&c.backend.active_count()==1);
 c.clock.set(12*ms);bool once=false;
 c.step.event_output=[&](J e){if(e.at("event")=="filtered_owner_scene"&&!once){once=true;c.clock.set(135*ms);}};
 const auto before=c.backend.moves;const bool ok=c.step.accept(p.frame,p.batch,p.scene,true,candidate);
 t.check("diagnostic_delay_rejects_before_commit_"+std::to_string(candidate),ok&&once&&c.backend.active_count()==0&&c.backend.moves==before&&c.backend.downs==1&&c.step.evaluation_outcome=="source_expired_before_commit");
 t.check("expiry_does_not_set_process_failure_"+std::to_string(candidate),c.step.first_failure().is_null()&&c.step.ledger.valid());
 p.frame.sequence=3;p.frame.capture_complete_ns=p.frame.pixels_ready_ns=140*ms;p.scene.sequence=3;p.scene.context.frame=3;p.scene.context.capture_ns=140*ms;p.batch.context=p.scene.context;
 for(auto& l:p.scene.lines)l.observed_ns=140*ms;p.batch.lines=p.scene.lines;for(auto& target:p.scene.targets){target.evidence_ns=140*ms;target.expires_ns=240*ms;target.crossing_ns=140*ms;target.revision=3;}
 c.clock.set(140*ms);t.check("fresh_frame_after_expiry_no_rebirth_"+std::to_string(candidate),c.step.accept(p.frame,p.batch,p.scene,true,candidate)&&c.backend.downs==1&&c.backend.active_count()==0);
 t.check("expiry_round_release_"+std::to_string(candidate),c.step.finish());
 }}
void first_cause(Tests& t){Pixels p;Context c;c.start(p);c.backend.failure=ReplayBackend::Failure::unknown_up;c.clock.set(28*ms);
 t.check("unknown_poll_records_first_failure",!c.step.poll()&&!c.step.first_failure().is_null()&&c.step.stop_reason()!="verified");const auto first=c.step.first_failure();c.clock.set(150*ms);c.step.poll();c.step.finish();
 t.check("release_does_not_replace_first_cause",c.step.first_failure()==first&&c.backend.downs==1&&c.backend.ups==1);
}
int main(int argc,char** argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh output");Tests t;expired_extraction(t);expiry_boundary(t);unknown_and_reversed(t);commit_expiry(t);first_cause(t);J r={{"schema","pas.owner-expiry-regression.v1"},{"checks",t.checks},{"assertions",t.checks.size()},{"failed",t.failed},{"device_endpoints",0},{"live_ready",false}};std::ofstream f(argv[1]);f<<r.dump(2)<<'\n';if(!f)throw std::runtime_error("write");std::cout<<"assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
