#include "runtime.hpp"
#include "cost_workload.hpp"
#include <fstream>
#include <iostream>
using namespace pas;using namespace pas::current_rails;using namespace pas::current_execution;using J=nlohmann::json;
constexpr Nanoseconds ms=1'000'000;
struct Tests{J checks=J::array();int failed=0;void check(std::string id,bool ok){checks.push_back({{"id",id},{"pass",ok}});failed+=!ok;}};
Frame frame(std::uint64_t sequence,const std::vector<std::uint8_t>&pixels){Frame f;f.sequence=sequence;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.capture_complete_ns=f.pixels_ready_ns=1'000*ms+Nanoseconds(sequence)*20*ms;f.rgb=pixels;return f;}
bool acquire(Runtime&rt,FakeClock&clock,std::uint64_t&sequence){const auto images=cost_workload::stimulus("tap");for(int i=0;i<120;++i){auto f=frame(++sequence,images[std::size_t(i)%images.size()]);clock.set(f.pixels_ready_ns);if(!rt.dispatch(f))return false;if(rt.backend.active_count())return rt.backend.downs>0&&rt.ledger.valid();}return false;}
void request_next_round(Runtime&rt,FakeClock&clock,std::uint64_t&sequence){
 // Typed lifecycle control represents a completed round followed by a current
 // menu. The next HUD, observer, owner creation and Down still use RGB dispatch.
 rt.lifecycle.stop("synthetic_round_boundary",false);SceneContext menu{1,1,1,++sequence,1'000*ms+Nanoseconds(sequence)*20*ms,1280,720,1};clock.set(menu.capture_ns);SessionUiEvidence ui;ui.menu=true;rt.lifecycle.observe(menu,ui,false,clock.now_ns());
}
void exercise(Tests&t,bool unknown){FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::uint64_t sequence=0;std::vector<BackendEvent> events;rt->receipt_output=[&](const auto&e){events.push_back(e);};const auto label=std::string(unknown?"unknown":"verified");
 t.check(label+"_rgb_dispatch_own_down",acquire(*rt,clock,sequence));events.clear();if(unknown)rt->backend.failure=ReplayBackend::Failure::release_unknown;
 bool result=false,threw=false;try{result=rt->finish();}catch(const std::exception&){threw=true;}
 const auto first=rt->final_release;
 t.check(label+"_first_finish_structured",!threw&&result==!unknown&&first.requested_ids==std::vector<int>{0}&&first.unknown_ids==(unknown?std::vector<int>{0}:std::vector<int>{}));
 t.check(label+"_first_release_event_once",events.size()==1&&events[0].release&&events[0].report.unknown_ids==first.unknown_ids);events.clear();rt->backend.failure=ReplayBackend::Failure::none;
 request_next_round(*rt,clock,sequence);
 if(unknown){const auto images=cost_workload::stimulus("tap");auto f=frame(++sequence,images[0]);clock.set(f.pixels_ready_ns);
  t.check("unknown_next_round_dispatch_denied",!rt->dispatch(f));
  t.check("unknown_next_round_not_started",rt->lifecycle.status().round==1&&rt->backend.downs==1&&rt->backend.active_count()==1);
  t.check("unknown_repeat_cannot_upgrade",!rt->finish()&&rt->final_release.unknown_ids==first.unknown_ids&&rt->final_release.start_ns==first.start_ns&&events.empty());
 }else{
  t.check("verified_next_round_gets_new_owned_down",acquire(*rt,clock,sequence)&&rt->lifecycle.status().round==2&&rt->backend.downs==2&&rt->backend.active_count()==1);events.clear();
  t.check("verified_next_round_finish_not_cached_success",rt->finish()&&rt->backend.active_count()==0&&rt->final_release.requested_ids==std::vector<int>{0});
  t.check("verified_next_round_new_release_evidence",rt->final_release.start_ns>first.start_ns&&events.size()==1&&events[0].release);
 }
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh report required");Tests t;exercise(t,false);exercise(t,true);J j={{"schema","pas.runtime-finish-pixels-rounds.v1"},{"assertions",t.checks.size()},{"failed",t.failed},{"checks",t.checks},{"entry","RGB observer/lifecycle/hook/owner/own receipt"},{"round_boundary","typed lifecycle menu control then actual RGB HUD"},{"device_endpoints",0},{"physical_gold",0}};std::ofstream out(argv[1]);out<<j.dump(2)<<'\n';if(!out)throw std::runtime_error("report write");std::cout<<"runtime finish pixels assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
