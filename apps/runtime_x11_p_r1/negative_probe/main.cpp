#include "pas/game_session.hpp"
#include "pas/session_events.hpp"
#include "meter_touch.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
using namespace pas;using nlohmann::json;
int main(int argc,char** argv) {try {
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("new output required");std::filesystem::create_directories(argv[1]);
 std::ofstream raw(std::filesystem::path(argv[1])/"releases-and-receipts.jsonl");json summary=json::array();
 for(bool unknown:{false,true}) {
  FakeClock c;r1::MeterTouch t(c,0);SessionGameOwner g(c,t,5,{15,35'000'000,30'000'000});g.start(1);
  auto scene=[&](std::uint64_t id){DecisionSnapshot s;s.sequence=id;s.context={1,1,1,id,c.now_ns(),1280,720,1};s.playing_gate=true;s.ui=GameUi::playing;return s;};
  auto s=scene(1);GameTarget n;n.note_id=n.revision=1;n.evidence_ns=0;n.expires_ns=100'000'000;n.note.kind=NoteKind::hold;
  n.note.center=n.hit={400,500};n.note.width=120;n.note.height=300;n.note.rails_geometry=n.note.head_on_line=true;n.crossing_ns=45'000'000;
  n.reason="prediction_observe_only";n.uncertainty_ns=2'000'000;n.samples=4;n.line_id=7;s.targets={n};g.accept(s,true);c.set(10'000'000);g.poll();
  if(t.receipts.size()!=1)throw std::runtime_error("initial Down missing");t.fail_release=!unknown;t.unknown_release=unknown;
  c.set(20'000'000);g.accept(scene(2),false);if(g.scheduler()->fault()!="release_failed")throw std::runtime_error("release fault missing");
  c.set(30'000'000);s=scene(3);n.evidence_ns=c.now_ns();n.expires_ns=c.now_ns()+100'000'000;n.crossing_ns=80'000'000;s.targets={n};g.accept(s,true);g.poll();
  if(t.receipts.size()!=1)throw std::runtime_error("Down retried");bool threw=false;try{g.finish();}catch(const std::runtime_error&){threw=true;}
  if(!threw)throw std::runtime_error("failed release accepted at finish");
  const auto failure_calls=t.release_calls.size();t.fail_release=t.unknown_release=false;t.release_all();
  std::uint64_t requested=0,failed=0,unk=0;
  for(std::size_t i=0;i<t.release_calls.size();++i){auto j=release_json(t.release_calls[i],i<failure_calls?"contract_under_test":"explicit_fake_cleanup");j["case"]=unknown?"unknown":"failed";j["call_index"]=i;raw<<j.dump()<<'\n';
   requested+=t.release_calls[i].requested_ids.size();failed+=t.release_calls[i].failed_ids.size();unk+=t.release_calls[i].unknown_ids.size();}
  for(const auto& r:t.receipts){auto j=receipt_json(r,false);j["case"]=unknown?"unknown":"failed";raw<<j.dump()<<'\n';}
  if(!t.contacts.empty()||(!unknown&&!failed)||(unknown&&!unk))throw std::runtime_error("release negative denominator lost");
  summary.push_back({{"case",unknown?"unknown":"failed"},{"release_calls",t.release_calls.size()},{"calls_before_cleanup",failure_calls},{"requested_ids",requested},{"failed_ids",failed},{"unknown_ids",unk},{"receipts",t.receipts.size()},{"contacts_after_fake_cleanup",t.contacts.size()},{"expected_rejection",true}});
 }
 raw.flush();if(!raw)throw std::runtime_error("raw write failed");std::ofstream f(std::filesystem::path(argv[1])/"summary.json");f<<summary.dump(2);f.flush();if(!f)throw std::runtime_error("summary write failed");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
