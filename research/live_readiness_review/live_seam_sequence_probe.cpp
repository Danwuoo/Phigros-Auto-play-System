// One parent-requested late source-sequencing check; no new raster or suite.
#define main preserved_runtime_reviewer_main
#include "live_seam_runtime_review_v2.cpp"
#undef main
int main(int argc,char**argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh sequencing probe output required");
 J runs=J::array();for(bool pre_poll:{false,true}){FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::uint64_t sequence=0;int phase=0;
  if(!acquire(*rt,clock,sequence,phase))throw std::runtime_error("fixed positive entry did not establish real owner Down");
  const auto prior_evidence=clock.now_ns();const auto old_due=rt->owner.scheduler()->next_due_ns();
  auto f=fixed_frame(phase+1,++sequence);clock.set(prior_evidence+100*ms);
  bool ok=false;if(pre_poll)ok=rt->dispatch(f);else{auto s=rt->observer.process(f);ok=rt->step.accept(f,rt->observer.candidate_batch(),s,true);}
  const auto* a=rt->ledger.attachments().empty()?nullptr:&rt->ledger.attachments()[0];
  runs.push_back({{"order",pre_poll?"Runtime_poll_before":"production_accept_before_poll"},{"prior_body_evidence",prior_evidence},
   {"old_next_due",old_due?J(*old_due):J(nullptr)},{"fresh_frame_capture",f.capture_complete_ns},{"owner_now",clock.now_ns()},
   {"packet_age",clock.now_ns()-f.capture_complete_ns},{"step_ok",ok},{"last_valid",rt->last.valid},{"last_accepted",rt->last.accepted},
   {"active_after",rt->backend.active_count()},{"Down",rt->backend.downs},{"Up",rt->backend.ups},
   {"ledger_valid",rt->ledger.valid()},{"ledger_state",a?J(int(a->state)):J(nullptr)},{"ledger_deadline",a?J(a->deadline):J(nullptr)}});
  rt->finish();}
 std::ofstream f(argv[1]);f<<J{{"schema","pas.disabled-live-seam-order-probe.v1"},{"case_count",1},{"input","unchanged family-v2 offset0 positive"},{"runs",runs},{"device_endpoints",0},{"physical_gold",0}}.dump(2)<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
