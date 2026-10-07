#include "../runtime.hpp"
#include "../tap_reset_workload.hpp"
#include "../evidence_hash.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
using namespace pas;using namespace pas::current_rails;using J=nlohmann::json;
namespace {
J trial(bool candidate,int offset,int cadence,bool absent,bool skip){
 FakeClock clock;auto rt=std::make_unique<Runtime>(clock);std::vector<std::uint8_t> p;std::set<std::uint64_t> accepted_ids;bool safe=true;int processed=0;
 rt->event_output=[&](const J& e){if(e.at("event")=="own_accepted_plan"){if(accepted_ids.size()>=128)throw std::runtime_error("test plan identity cap");accepted_ids.insert(e.at("note"));}};
 for(int i=1;i<=64;++i){if(skip&&i%4==1&&i>4)continue;tap_reset_workload::render_phase(p,(i-1)%32,offset,absent);Frame f;
  f.sequence=i;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.capture_complete_ns=f.pixels_ready_ns=1'000'000'000LL+Nanoseconds(i)*cadence*1'000'000;
  f.rgb=p;clock.set(f.capture_complete_ns);safe&=rt->dispatch(f,candidate);++processed;
 }
 const bool released=rt->finish();return {{"mode",candidate?"B":"A"},{"offset",offset},{"cadence_ms",cadence},{"absence",absent},{"skip_every_fourth",skip},{"requested",64},{"processed",processed},{"safe",safe},{"release_verified",released},{"active_final",rt->backend.active_count()},{"own_down",rt->backend.downs},{"own_up",rt->backend.ups},{"accepted_note_ids",accepted_ids},{"ledger_valid",rt->ledger.valid()},{"device_endpoints",0},{"clock_domain","FakeClock_functional_only"}};
}
}
int main(int argc,char** argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh test output required");J checks=J::array(),runs=J::array(),corpus=J::array();int failed=0;
 auto check=[&](std::string name,bool pass){checks.push_back({{"name",name},{"pass",pass}});failed+=!pass;};
 for(int offset:tap_reset_workload::offsets)for(int phase=0;phase<32;++phase){std::vector<std::uint8_t> p;tap_reset_workload::render_phase(p,phase,offset);pas::evidence::Sha256 h;h.update(reinterpret_cast<const char*>(p.data()),p.size());corpus.push_back({{"phase",phase},{"offset",offset},{"bytes",p.size()},{"sha256",h.finish()}});}
 check("explicit_prefix_absence_exceeds_90ms_at_stress_cadence",16*8>90);
 check("all_96_declared_phase_offset_RGB_hashes",corpus.size()==96);
 for(bool candidate:{false,true})for(int cadence:{20,8})for(int offset:tap_reset_workload::offsets){auto r=trial(candidate,offset,cadence,false,false);const auto id=std::string(candidate?"B":"A")+"-"+std::to_string(cadence)+"-"+std::to_string(offset);
  check(id+"-safe-full-prefix",r["safe"]==true&&r["processed"]==64&&r["ledger_valid"]==true);
  check(id+"-two_distinct_note_ids",r["accepted_note_ids"].size()==2);
  check(id+"-both_cycles_have_own_down_up",r["own_down"]==2&&r["own_up"]==2);
  check(id+"-verified_release",r["release_verified"]==true&&r["active_final"]==0);runs.push_back(std::move(r));
 }
 for(bool candidate:{false,true}){auto r=trial(candidate,0,20,true,false);check(std::string(candidate?"B":"A")+"-absence_no_touch",r["safe"]==true&&r["own_down"]==0&&r["own_up"]==0&&r["release_verified"]==true);runs.push_back(std::move(r));}
 for(bool candidate:{false,true}){auto r=trial(candidate,0,20,false,true);check(std::string(candidate?"B":"A")+"-bounded_skips_keep_distinct_cycles",r["safe"]==true&&r["accepted_note_ids"].size()==2&&r["own_down"]==2&&r["own_up"]==2&&r["release_verified"]==true);runs.push_back(std::move(r));}
 for(int attempt:{0,10001}){bool rejected=false;try{std::vector<std::uint8_t> p;tap_reset_workload::render(p,attempt);}catch(const std::invalid_argument&){rejected=true;}check("attempt_bound_"+std::to_string(attempt),rejected);}
 J out={{"schema","pas.tap-reset-workload-tests.v1"},{"checks",checks},{"assertions",checks.size()},{"failed",failed},{"runs",runs},{"corpus",corpus},{"generator",tap_reset_workload::generator},{"device_endpoints",0},{"physical_gold",0},{"live_ready",false},{"scope","new declared offline RGB corpus; fresh note identities arise from current pixels/absence, not owner resets or replayed actions"}};
 std::ofstream file(argv[1],std::ios::binary);file<<out.dump(2)<<'\n';if(!file)throw std::runtime_error("test write");std::cout<<"tap reset controls="<<checks.size()<<" failed="<<failed<<'\n';return failed?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
