// Independent contract expectations were frozen in TAIL_ONLY_REVIEW_PLAN.md.
// Reuse the existing unchanged independent raster; never call a production
// sampler to construct pixels and never seed a known-down/cursor by hand.
#define main preserved_current_contract_main
#include "../prelive_current/current_tests.cpp"
#undef main

struct ReviewedOwner {
 FakeClock clock;ReplayBackend backend{clock};SessionGameOwner owner{clock,backend,5,{3,0,30*ms}};
 ExecutionLedger ledger;Hook hook;std::vector<ContactPlan> plans;std::vector<BackendEvent> receipts;J events=J::array();
 ReviewedOwner(){owner.start(1);}
 bool sync(){auto p=owner.owner()->take_accepted_plans();auto r=backend.take_events();auto c=owner.owner()->take_coverage_updates();auto x=owner.owner()->take_plan_cancellations();for(auto&e:c)events.push_back(e);for(auto&e:x)events.push_back(e);plans.insert(plans.end(),p.begin(),p.end());receipts.insert(receipts.end(),r.begin(),r.end());return ledger.observe(p,r,*owner.scheduler());}
 void poll(Nanoseconds t){clock.set(t);owner.poll();if(!sync())throw std::runtime_error("independent ledger rejected receipts");}
 pas::current_rails::Evaluation evaluate(Input&a){clock.set(a.f.pixels_ready_ns);return hook.evaluate(a.f,a.b,a.s,ledger,clock.now_ns());}
 void accept(const DecisionSnapshot&s,Nanoseconds t){clock.set(t);owner.accept(s,true);owner.poll();if(!sync())throw std::runtime_error("independent ledger rejected accept");}
 pas::current_rails::Evaluation step(Input&a){poll(a.f.pixels_ready_ns);auto e=evaluate(a);accept(e.filtered,a.f.pixels_ready_ns);return e;}
 void entry(){for(int k=0;k<3;++k){auto a=make(k+1,(10+k*20)*ms,460+k*20);step(a);}auto body=make(4,70*ms,550,200,410);step(body);}
 bool finish(){auto release=owner.finish();backend.take_events();return ledger.finalize(release)&&backend.active_count()==0;}
 std::size_t confirmed()const{std::size_t n=0;for(const auto&e:events)n+=e.value("event","")=="game_hold_tail_confirmed";return n;}
 std::size_t normal_ups()const{std::size_t n=0;for(const auto&e:events)n+=e.value("event","")=="game_contact_up"&&e.value("reason","")=="visible_tail_completed";return n;}
};
Input terminal_input(int sequence,Nanoseconds time){return make(sequence,time,550,512,410);}
bool typed_tail(const pas::current_rails::Evaluation&e){return e.valid&&e.filtered.targets.size()==1&&e.filtered.targets[0].current_contact==GameTarget::CurrentContact::hold_tail_only;}
DecisionSnapshot rebound(DecisionSnapshot s,std::uint64_t frame,Nanoseconds time){s.sequence=frame;s.context.frame=frame;s.context.capture_ns=time;for(auto&line:s.lines)line.observed_ns=time;for(auto&t:s.targets){t.revision=frame;t.evidence_ns=time;t.expires_ns=time>std::numeric_limits<Nanoseconds>::max()-100*ms?std::numeric_limits<Nanoseconds>::max():time+100*ms;if(t.tail_terminal_proof){t.tail_terminal_proof->context=s.context;t.tail_terminal_proof->pixels_ready_ns=time;}}return s;}

int main(int argc,char**argv){Tests r;try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh independent tail review report required");
 {
  ReviewedOwner o;o.entry();const auto before_down=o.backend.downs,before_move=o.backend.moves;const auto original=o.plans.back();const auto contact=o.ledger.find(42)->contact;
  auto first=terminal_input(5,90*ms);auto a=o.step(first);r.check("own_terminal_without_body_positive",typed_tail(a)&&!a.support[0].body&&o.backend.downs==before_down&&o.backend.moves==before_move);r.check("single_terminal_cannot_confirm",o.confirmed()==0);
  r.check("first_terminal_keeps_plan_evidence",o.plans.back().evidence_ns==original.evidence_ns&&o.plans.back().source_frame_sequence==original.source_frame_sequence&&o.plans.back().valid_until_ns==original.valid_until_ns);
  auto second=terminal_input(6,110*ms);auto b=o.step(second);r.check("second_fresh_terminal_confirms",typed_tail(b)&&o.confirmed()==1&&o.backend.downs==before_down&&o.backend.moves==before_move);
  const auto revised=o.plans.back();r.check("tail_revision_retains_body_evidence_and_expiry",revised.evidence_ns==original.evidence_ns&&revised.source_frame_sequence==original.source_frame_sequence&&revised.valid_until_ns==original.valid_until_ns);
  r.check("tail_only_due_plus20ms_at_old_contact_position",revised.steps.back().phase==Phase::up&&revised.steps.back().due_ns==130*ms&&revised.steps.back().x==original.steps[original.steps.size()-2].x&&revised.steps.back().y==original.steps[original.steps.size()-2].y);
  o.poll(129*ms);r.check("no_early_normal_up",o.backend.ups==0);o.poll(130*ms);r.check("one_same_contact_normal_up",o.backend.ups==1&&o.normal_ups()==1&&o.backend.downs==before_down&&o.backend.moves==before_move&&o.ledger.find(42)->contact==contact);r.check("positive_release",o.finish());
 }
 const std::vector<std::string> negative_names={"reason_without_enum","enum_without_payload","not_line_aware","wrong_proof_note","wrong_proof_line","wrong_proof_frame","future_pixels_ready","edited_boundary","wrong_line_count","insufficient_left_rail","ambiguous_terminal_clusters","zero_probes","projected_line","changed_note_id","changed_line_id","changed_tangent","changed_width","neighbour_anchor","body_patch","stale_evidence","expired_target","invalid_current_line","competing_current_line","wrong_context","payload_without_enum_body_flags","payload_wrong_enum_body_flags","reason_only_body_flags"};
 for(std::size_t negative=0;negative<negative_names.size();++negative){
  ReviewedOwner o;o.entry();const auto down=o.backend.downs,move=o.backend.moves;
  auto mutate=[&](DecisionSnapshot&s){if(s.targets.empty())return;auto&t=s.targets[0];auto&p=*t.tail_terminal_proof;
   if(negative==0)t.current_contact=GameTarget::CurrentContact::none;
   if(negative==1){t.tail_terminal_proof.reset();return;}
   if(negative==2)p.line_aware=false;
   if(negative==3)++p.note_id;
   if(negative==4)++p.line_id;
   if(negative==5)++p.context.frame;
   if(negative==6)p.pixels_ready_ns=s.context.capture_ns+ms;
   if(negative==7)p.boundary.y+=1;
   if(negative==8)p.current_line_count=0;
   if(negative==9)p.left_rail_rows=5;
   if(negative==10)p.terminal_clusters=2;
   if(negative==11)p.probes=0;
   if(negative==12)t.line_projection_only=true;
   if(negative==13){++t.note_id;p.note_id=t.note_id;}
   if(negative==14){++t.line_id;p.line_id=t.line_id;}
   if(negative==15)t.note.tangent={0,1};
   if(negative==16)t.note.width+=20;
   if(negative==17)t.note.center.x+=200;
   if(negative==18)t.note.held_body_patch=true;
   if(negative==19)t.evidence_ns-=ms;
   if(negative==20)t.expires_ns=s.context.capture_ns;
   if(negative==21)s.lines[0].association_valid=false;
   if(negative==22){auto line=s.lines[0];++line.track_id;s.lines.push_back(line);p.current_line_count=s.lines.size();}
   if(negative==23){++s.context.geometry;p.context=s.context;}
   if(negative==24){t.current_contact=GameTarget::CurrentContact::none;t.note.held_body_evidence=true;}
   if(negative==25){t.current_contact=GameTarget::CurrentContact::hold_front_rails;t.note.held_body_evidence=true;}
   if(negative==26){t.current_contact=GameTarget::CurrentContact::none;t.note.held_body_evidence=true;t.tail_terminal_proof.reset();}
  };
  bool producer_positive=true;for(int k=0;k<2;++k){auto input=terminal_input(5+k,(90+k*20)*ms);auto e=o.evaluate(input);producer_positive&=typed_tail(e);if(!typed_tail(e))continue;mutate(e.filtered);o.accept(e.filtered,input.f.capture_complete_ns);}
  r.details[negative_names[negative]]={{"producer_positive_before_mutation",producer_positive},{"confirmed",o.confirmed()},{"downs",o.backend.downs},{"moves",o.backend.moves}};
  r.check("owner_rejects_"+negative_names[negative],o.confirmed()==0&&o.backend.downs==down&&o.backend.moves==move);r.check("negative_release_"+negative_names[negative],o.finish());
 }
 for(const auto&which:{std::string("equal_time"),std::string("same_frame"),std::string("nine_ms"),std::string("ten_ms")}){
  ReviewedOwner o;o.entry();auto first=terminal_input(5,90*ms);auto a=o.step(first);auto second=terminal_input(6,which=="nine_ms"?99*ms:100*ms);auto e=o.evaluate(second);r.check("timing_producer_positive_"+which,typed_tail(a)&&typed_tail(e));
  if(typed_tail(e)){if(which=="equal_time"){e.filtered=rebound(e.filtered,6,90*ms);}
   if(which=="same_frame"){e.filtered.context.frame=5;for(auto&t:e.filtered.targets)t.tail_terminal_proof->context=e.filtered.context;}
   o.accept(e.filtered,second.f.capture_complete_ns);}
  r.check("two_distinct_fresh_at_least10ms_"+which,o.confirmed()==(which=="ten_ms"?1:0));if(which=="ten_ms"){o.poll(119*ms);r.check("exact10ms_no_up_before20ms",o.backend.ups==0);o.poll(120*ms);r.check("exact10ms_normal_up_at20ms",o.normal_ups()==1);}r.check("timing_release_"+which,o.finish());
 }
 for(const auto&which:{std::string("absent"),std::string("line_occluded"),std::string("region_lost"),std::string("invalid_payload")}){
  ReviewedOwner o;o.entry();auto first=terminal_input(5,90*ms);o.step(first);auto middle=terminal_input(6,100*ms);auto e=o.evaluate(middle);if(which=="absent")e.filtered.targets.clear();if(which=="line_occluded")e.filtered.lines.clear();if(typed_tail(e)&&which=="region_lost")e.filtered.targets[0].note.center.x+=200;if(typed_tail(e)&&which=="invalid_payload")e.filtered.targets[0].tail_terminal_proof.reset();o.accept(e.filtered,100*ms);
  auto last=terminal_input(7,110*ms);o.step(last);r.check("unknown_gap_resets_first_witness_"+which,o.confirmed()==0);r.check("gap_release_"+which,o.finish());
 }
 {ReviewedOwner donor;donor.entry();auto input=terminal_input(5,90*ms);auto first=donor.evaluate(input);r.check("unowned_fixture_has_actual_producer_proof",typed_tail(first));ReviewedOwner unseen;if(typed_tail(first)){unseen.accept(first.filtered,90*ms);unseen.accept(rebound(first.filtered,6,110*ms),110*ms);}r.check("typed_tail_without_own_down_cannot_birth_or_confirm",unseen.backend.downs==0&&unseen.backend.moves==0&&unseen.backend.ups==0&&unseen.confirmed()==0);r.check("unowned_release",unseen.finish());
  for(const auto&state:{std::string("pending"),std::string("completed"),std::string("cancelled"),std::string("unknown")}){ReviewedOwner o;Nanoseconds begin=90*ms;if(state=="pending"){auto pending=make(4,70*ms,550,200,410);pending.s.targets[0].crossing_ns=120*ms;o.accept(pending.s,70*ms);const auto*a=o.ledger.find(42);r.check("pending_precondition_is_own_zero_cursor",a&&a->state==Execution::pending&&a->cursor&&*a->cursor==0&&o.backend.downs==0);}else{if(state=="unknown")o.backend.failure=ReplayBackend::Failure::unknown_down;o.entry();if(state=="completed"){o.poll(170*ms);begin=190*ms;}if(state=="cancelled"){o.owner.scheduler()->cancel("independent_cancelled_fixture");o.sync();}}
   const auto down=o.backend.downs,move=o.backend.moves;if(typed_tail(first)){o.accept(rebound(first.filtered,5,begin),begin);o.accept(rebound(first.filtered,6,begin+20*ms),begin+20*ms);}o.poll(begin+40*ms);r.check("typed_tail_state_denied_"+state,o.confirmed()==0&&o.backend.downs==down&&o.backend.moves==move);if(state!="unknown")r.check("state_release_"+state,o.finish());else r.check("unknown_first_release_retained",!o.owner.scheduler()->fault().empty()&&o.backend.active_count()==0&&o.backend.downs==1);
  }r.check("donor_release",donor.finish());}
 {ReviewedOwner o;o.entry();auto original=o.plans.back();auto first=terminal_input(5,90*ms);auto e=o.evaluate(first);if(typed_tail(e)){e.filtered.targets[0].hit={1100,650};o.accept(e.filtered,90*ms);}auto second=terminal_input(6,110*ms);e=o.evaluate(second);if(typed_tail(e)){e.filtered.targets[0].hit={std::numeric_limits<double>::quiet_NaN(),650};o.accept(e.filtered,110*ms);}r.check("tail_hit_is_never_move_permission",o.backend.moves==1&&o.backend.downs==1);if(o.confirmed()){const auto&p=o.plans.back();r.check("untrusted_tail_hit_cannot_replace_release_position",p.steps.back().x==original.steps[original.steps.size()-2].x&&p.steps.back().y==original.steps[original.steps.size()-2].y);}r.check("hit_mutation_release",o.finish());}
 for(const auto&kind:{std::string("shifted_body"),std::string("returned_rotated_body")}){ReviewedOwner o;o.entry();auto first=terminal_input(5,90*ms);o.step(first);auto second=terminal_input(6,110*ms);o.step(second);const auto closing=o.plans.back();const auto moves=o.backend.moves;auto body=make(7,120*ms,550,200,kind=="shifted_body"?420:415,kind=="shifted_body"?0:.05);auto observed=o.step(body);r.details["post_confirm_"+kind]={{"accepted",observed.accepted},{"confirmed",o.confirmed()},{"moves_before",moves},{"moves_after",o.backend.moves}};r.check("closing_identity_cannot_regain_move_"+kind,o.confirmed()==1&&o.backend.moves==moves&&o.backend.downs==1);const auto&p=o.plans.back();r.check("closing_identity_cannot_renew_or_retarget_"+kind,p.evidence_ns==closing.evidence_ns&&p.source_frame_sequence==closing.source_frame_sequence&&p.steps.back().due_ns==130*ms&&p.steps.back().x==closing.steps.back().x&&p.steps.back().y==closing.steps.back().y);o.poll(130*ms);r.check("closing_identity_keeps_normal_up20ms_"+kind,o.backend.ups==1&&o.normal_ups()==1);r.check("closing_identity_release_"+kind,o.finish());}
 for(const auto&kind:{std::string("same_tail_twice"),std::string("conflicting_tail"),std::string("tail_and_body")}){ReviewedOwner o;o.entry();for(int k=0;k<2;++k){auto input=terminal_input(5+k,(90+k*20)*ms);auto e=o.evaluate(input);if(!typed_tail(e))continue;auto other=e.filtered.targets[0];if(kind=="conflicting_tail"){other.note.tail->y+=1;other.tail_terminal_proof->boundary=*other.note.tail;++other.tail_terminal_proof->candidate_id;}if(kind=="tail_and_body"){other=make(5+k,(90+k*20)*ms,550,200,420).s.targets[0];other.note.held_body_evidence=true;other.note.head_on_line=false;}e.filtered.targets.push_back(other);o.accept(e.filtered,input.f.capture_complete_ns);}r.details["duplicate_"+kind]={{"confirmed",o.confirmed()},{"downs",o.backend.downs},{"moves",o.backend.moves}};r.check("ambiguous_duplicate_cannot_confirm_typed_tail_"+kind,o.confirmed()==0&&o.backend.downs==1);r.check("duplicate_release_"+kind,o.finish());}
 {ReviewedOwner o;o.entry();auto input=terminal_input(5,90*ms);auto e=o.evaluate(input);r.check("extreme_time_fixture_has_current_proof",typed_tail(e));if(typed_tail(e)){const auto at=std::numeric_limits<Nanoseconds>::max()-10*ms;auto scene=rebound(e.filtered,5,at);for(auto&t:scene.targets)t.expires_ns=std::numeric_limits<Nanoseconds>::max();o.accept(scene,at);}r.check("extreme_direct_tail_is_rejected",o.confirmed()==0&&o.backend.downs==1&&o.backend.moves==1);r.check("extreme_time_release",o.finish());}
 for(bool confirm:{false,true}){ReviewedOwner o;o.entry();auto first=terminal_input(5,110*ms);o.step(first);if(confirm){auto second=terminal_input(6,120*ms);o.step(second);}auto empty=make(7,130*ms,550,512,410).s;empty.targets.clear();o.accept(empty,130*ms);if(confirm){r.check("confirmed_closing_not_cancelled_by_erased_visibility",o.confirmed()==1&&o.backend.ups==0);o.poll(139*ms);r.check("confirmed_closing_waits_full20ms",o.backend.ups==0);o.poll(140*ms);r.check("confirmed_closing_normal_up_after_old_missing_grace",o.backend.ups==1&&o.normal_ups()==1&&o.plans.back().evidence_ns==70*ms);}else r.check("unconfirmed_terminal_retains_original_missing_grace",o.confirmed()==0&&o.backend.ups==1&&o.normal_ups()==0);r.check("missing_grace_case_release_"+std::to_string(confirm),o.finish());}
 {ReviewedOwner o;o.entry();auto first=terminal_input(5,90*ms);o.step(first);auto second=terminal_input(6,110*ms);o.step(second);o.backend.failure=ReplayBackend::Failure::unknown_up;o.poll(130*ms);const auto*state=o.ledger.find(42);r.check("typed_tail_unknown_up_stays_unknown",o.backend.ups==1&&state&&state->state==Execution::unknown&&!o.owner.scheduler()->fault().empty());o.poll(150*ms);r.check("typed_tail_unknown_up_no_retry_and_release_retained",o.backend.ups==1&&o.backend.downs==1&&o.backend.moves==1&&o.backend.active_count()==0&&!o.owner.scheduler()->last_release().requested_ids.empty());}
 {ReviewedOwner o;o.entry();auto alias=make(5,80*ms,550,200,410).s;alias.targets[0].note_id=43;alias.targets[0].note.held_body_evidence=true;alias.targets[0].note.head_on_line=false;o.accept(alias,80*ms);bool established_alias=false;for(const auto&e:o.events)established_alias|=e.value("event","")=="game_hold_contact_reassociated"&&e.value("candidate_note_id",0)==43&&e.value("note_id",0)==42;r.check("closing_alias_precondition_is_real_owner_reassociation",established_alias&&o.backend.downs==1&&o.ledger.find(42)&&o.ledger.find(42)->state==Execution::active);auto first=terminal_input(6,100*ms);o.step(first);auto second=terminal_input(7,120*ms);o.step(second);const auto closed=o.plans.back();const auto moves=o.backend.moves;auto returned=make(8,130*ms,550,200,420).s;returned.targets[0].note_id=43;returned.targets[0].note.held_body_evidence=true;returned.targets[0].note.head_on_line=false;o.accept(returned,130*ms);r.details["existing_alias_after_closing"]={{"established_alias",established_alias},{"confirmed",o.confirmed()},{"moves_before",moves},{"moves_after",o.backend.moves}};r.check("existing_alias_cannot_bypass_up_only_closing",o.confirmed()==1&&o.backend.moves==moves&&o.plans.back().evidence_ns==closed.evidence_ns&&o.plans.back().steps.back().x==closed.steps.back().x&&o.plans.back().steps.back().y==closed.steps.back().y);o.poll(140*ms);r.check("existing_alias_keeps_fixed_normal_up",o.normal_ups()==1&&o.backend.downs==1&&o.backend.ups==1);r.check("existing_alias_release",o.finish());}
 J out={{"schema","pas.independent-typed-tail-owner-review.v1"},{"assertions",r.rows.size()},{"failed",r.failed},{"checks",r.rows},{"details",r.details},{"device_endpoints",0},{"physical_gold",0}};std::ofstream f(argv[1]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("write");return r.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
