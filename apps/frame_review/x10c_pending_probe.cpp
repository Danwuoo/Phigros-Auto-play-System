#include "pas/game.hpp"
#include <iostream>
using namespace pas;
namespace {
DecisionSnapshot snapshot(std::uint64_t seq,Nanoseconds time){DecisionSnapshot s;s.sequence=seq;s.context={1,1,1,seq,time,1280,720,1};s.ui=GameUi::playing;s.playing_gate=true;return s;}
nlohmann::json run(bool explicit_contradiction){
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,5,{15,0,30'000'000});
    auto s=snapshot(1,0);GameTarget t;t.note_id=1;t.revision=1;t.evidence_ns=0;t.expires_ns=100'000'000;t.samples=4;
    t.note.kind=NoteKind::hold;t.note.width=140;t.note.height=100;t.note.center={400,450};t.crossing_ns=50'000'000;
    t.hit={400,500};t.reason="prediction_observe_only";t.uncertainty_ns=2'000'000;s.targets={t};owner.accept(s);
    const auto pending_before=owner.scheduler().pending_count();clock.set(30'000'000);s=snapshot(2,clock.now_ns());
    if(explicit_contradiction){t.revision=2;t.evidence_ns=clock.now_ns();t.expires_ns=130'000'000;t.crossing_ns.reset();t.reason="root_past";s.targets={t};}
    owner.accept(s);const auto pending_after=owner.scheduler().pending_count();clock.set(50'000'000);owner.poll();
    nlohmann::json due_receipts=nlohmann::json::array();for(const auto& r:touch.receipts())due_receipts.push_back({{"phase",static_cast<int>(r.command.phase)},{"scheduled_ns",r.command.scheduled_ns},{"injection_start_ns",r.injection_start_ns},{"success",r.success}});
    const auto contacts_due=touch.contacts().size();clock.set(61'000'000);owner.accept(snapshot(3,clock.now_ns()));owner.poll();
    const auto contacts_after_grace=touch.contacts().size();owner.stop();
    return {{"explicit_contradiction",explicit_contradiction},{"pending_before",pending_before},{"pending_after_new_snapshot",pending_after},
        {"receipts_at_due",due_receipts},{"contacts_at_due",contacts_due},{"contacts_after_grace",contacts_after_grace},{"contacts_after_stop",touch.contacts().size()}};
}
}
int main(){try{const auto absent=run(false),contradicted=run(true);const bool reproduced=absent.at("pending_before")==1&&absent.at("pending_after_new_snapshot")==1&&absent.at("contacts_at_due")==1&&absent.at("contacts_after_grace")==0&&contradicted.at("pending_after_new_snapshot")==0&&contradicted.at("contacts_at_due")==0;
    std::cout<<nlohmann::json{{"schema",1},{"probe","unchanged C36h owner: current missing versus explicit root_past before pending Hold Down"},{"absence",absent},{"contradiction_control",contradicted},{"mechanism_reproduced",reproduced},{"gameplay_effect","unknown"},{"new_strategy_candidate",false}}.dump(2)<<'\n';return reproduced?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
