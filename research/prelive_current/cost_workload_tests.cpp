#include "runtime.hpp"
#include "cost_workload.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>

using namespace pas;
using namespace pas::current_rails;
using J = nlohmann::json;
namespace {
struct Tests {
    J checks = J::array();
    int failures = 0;
    void check(std::string name, bool pass) {
        checks.push_back({{"name", std::move(name)}, {"pass", pass}});
        failures += !pass;
    }
};
// Literal copy of the historical e9e305e generator. This independent reference
// prevents an extraction/refactor from quietly changing any frozen scene byte.
cost_workload::Images historical(std::string_view scene) {
    using cost_workload::box;
    cost_workload::Images images;
    for(int k=0;k<12;++k){auto& p=images[k];p.assign(1280*720*3,30);box(p,20,20,26,42,{255,255,255});box(p,34,20,40,42,{255,255,255});for(int d=0;d<6;++d)box(p,1020+d*24,20,1032+d*24,40,{255,255,255});
        if(scene=="dense"){for(int row=0;row<16;++row){const int y=150+row*31;box(p,40,y-1,1240,y+2,{255,255,255});for(int col=0;col<8;++col){const int x=190+col*130;box(p,x-39,y-12,x+39,y-3,{40,190,255});}}}
        else {box(p,115,499,1165,502,{255,255,255});const int y=440+k*8;
            if(scene=="hold"){box(p,350,y-100,451,y+1,{155,233,255});box(p,350,y-100,352,y+1,{255,255,255});box(p,449,y-100,451,y+1,{255,255,255});}
            else box(p,491,y-4,569,y+5,{40,190,255});}
    }
    return images;
}
J extraction_control(int variant) {
    auto candidate = std::make_unique<bvi::Candidate>();
    auto images = cost_workload::stimulus("hold");
    std::size_t bodies=0, fronts=0, contacts=0, relations=0, downs=0;
    J frames = J::array();
    for (int k=0; k<12; ++k) {
        auto& p=images[k];
        for (std::size_t i=0; i<p.size(); i+=3) {
            if ((variant&1) && p[i]==155 && p[i+1]==233 && p[i+2]==255)
                {p[i]=40;p[i+1]=190;}
            if ((variant&2) && p[i]==30 && p[i+1]==30 && p[i+2]==30)
                {p[i]=p[i+1]=p[i+2]=0;}
        }
        const bvi::Key key{{1,1,1,1},(k+1)*20'000'000LL,(k+1)*20'000'000LL,
                           std::uint64_t(k+1),true};
        const std::array<bvi::Query,1> queries={bvi::Query{{400.,double(440+k*8)},100.,100.,0.,false}};
        const std::array<bvi::Line,1> lines={bvi::Line{{640.,500.},0.,1050.,1}};
        auto observation=candidate->extract({p,1280,720,3840,key},queries,lines);
        candidate->relate(observation,key,key.capture);
        const auto allowed=bvi::constrain(observation,{key.capture,key.capture+100'000'000,
            key.capture+100'000'000,0,"never_executed",0,0,0,5,false});
        const auto& d=observation.parts[0];
        bodies+=d.body==bvi::Support::supported;
        fronts+=d.front_end; contacts+=d.contact==bvi::Support::supported;
        relations+=observation.relations[0].usable; downs+=allowed.down[0];
        frames.push_back({{"frame",k+1},{"body",bvi::name(d.body)},
            {"front",d.front_end},{"contact",bvi::name(d.contact)},
            {"relation_usable",observation.relations[0].usable},{"eligible_down",allowed.down[0]}});
    }
    return {{"variant",variant},{"blue_fill",bool(variant&1)},{"black_exterior",bool(variant&2)},
        {"body_frames",bodies},{"front_frames",fronts},{"contact_frames",contacts},
        {"relation_frames",relations},{"eligible_frames",downs},{"frames",frames},
        {"layer","frozen_v3_query_contract_only_not_owner_or_physical_opportunities"}};
}
J owner_control(std::string_view scene, bool candidate, int attempts=120, int head=503,
                cost_workload::Intervention intervention=cost_workload::Intervention::passage) {
    FakeClock clock;
    auto rt=std::make_unique<Runtime>(clock);
    const auto images=scene=="entry-release-diagnostic"?
        cost_workload::entry_release_probe(head,intervention):cost_workload::stimulus(scene);
    LatestFrame latest(1280,720,3,&clock);
    bool safe=true, all_leases=true;
    std::size_t processed=0, owned=0, receipt_count=0;
    std::array<std::size_t,3> phases{};
    std::array<std::vector<Nanoseconds>,3> lateness;
    J frames=J::array(), receipts=J::array(), owner_events=J::array();
    std::map<std::string,int> support_reasons;
    rt->event_output=[&](J event) {
        const auto type=event.at("event").get<std::string>();
        if(type!="current_observer_scene"&&type!="complete_raw_candidate_batch"&&type!="filtered_owner_scene")
            owner_events.push_back(std::move(event));
    };
    rt->receipt_output=[&](const current_execution::BackendEvent& e) {
        if(e.release) {
            receipts.push_back({{"event","release"},{"requested",e.report.requested_ids},
                {"failed",e.report.failed_ids},{"unknown",e.report.unknown_ids}});
            return;
        }
        const auto phase=std::size_t(e.issued.phase);
        ++phases.at(phase); ++receipt_count;
        lateness.at(phase).push_back(e.receipt.injection_start_ns-e.issued.scheduled_ns);
        receipts.push_back({{"event","injection"},{"phase",phase},{"intent",e.issued.intent_id},
            {"contact",e.issued.contact_id},{"frame",e.issued.source_frame_sequence},
            {"scheduled",e.issued.scheduled_ns},{"start",e.receipt.injection_start_ns},
            {"return",e.receipt.injection_return_ns},{"success",e.receipt.success}});
    };
    for(int i=1;i<=attempts;++i) {
        Frame f;
        f.sequence=i; f.epoch=f.generation=f.geometry_version=1;
        f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;
        f.capture_complete_ns=f.pixels_ready_ns=1'000'000'000LL+i*20'000'000LL;
        clock.set(f.capture_complete_ns);
        const bool published=latest.publish(images[(i-1)%12].data(),1280*720*3,f);
        auto lease=latest.read_after(i-1,0);
        const bool same=published&&lease&&lease->sequence==std::uint64_t(i);
        all_leases&=same;
        if(!same){safe=false;break;}
        const bool ok=rt->dispatch(*lease,candidate);
        ++processed;owned+=rt->owner_processed;
        for(std::size_t j=0;j<rt->last.count;++j)++support_reasons[std::string(rt->last.support[j].reason)];
        frames.push_back({{"frame",i},{"safe",ok},{"owner_processed",rt->owner_processed},
            {"raw_candidates",rt->observer.candidate_batch().candidates.size()},
            {"down_total",rt->backend.downs},{"move_total",rt->backend.moves},{"up_total",rt->backend.ups}});
        if(scene=="entry-release-diagnostic") {
            J support=J::array();
            for(std::size_t j=0;j<rt->last.count;++j) {
                const auto& s=rt->last.support[j];
                support.push_back({{"note",s.note},{"active",s.active},{"terminal",s.terminal},
                    {"body",s.body},{"tail",s.tail},{"accepted",s.accepted},{"reason",s.reason}});
            }
            frames.back()["current_support"]=std::move(support);
            frames.back()["raw_batch"]=candidate_batch_json(rt->observer.candidate_batch());
        }
        if(!ok){safe=false;break;}
    }
    const auto active_before_release=rt->backend.active_count();
    std::set<int> own_active_contacts;
    for(const auto& event:owner_events)if(event.at("event")=="own_accepted_plan") {
        const auto* attachment=rt->ledger.find(event.at("note").get<std::uint64_t>());
        if(attachment&&attachment->state==current_execution::Execution::active)
            own_active_contacts.insert(attachment->contact);
    }
    const bool released=rt->finish();
    const auto counters=latest.counters();latest.close();
    return {{"scene",scene},{"mode",candidate?"B":"A"},{"requested",attempts},{"processed",processed},
        {"owner_seen",owned},{"same_frame_leases",all_leases},{"pool_drops",counters.pool_drops},
        {"safe",safe},{"release_verified",released},{"active_final",rt->backend.active_count()},
        {"ledger_valid",rt->ledger.valid()},{"ledger_reason",rt->ledger.reason()},
        {"active_before_final_release",active_before_release},
        {"own_active_contacts_before_final_release",own_active_contacts},
        {"own_down",rt->backend.downs},{"own_move",rt->backend.moves},{"own_up",rt->backend.ups},
        {"failed_injections",rt->backend.failed},{"phase_counts",phases},{"receipt_count",receipt_count},
        {"phase_lateness_ns",lateness},{"support_reasons",support_reasons},
        {"frames",frames},{"receipts",receipts},{"owner_events",owner_events},
        {"head_at_frame5",head},{"intervention",cost_workload::name(intervention)},
        {"clock_domain","FakeClock_20ms_functional_only"}};
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("usage: cost_workload_tests FRESH_REPORT");
        Tests t;J layer=J::array(),runs=J::array(),sweep=J::array(),negatives=J::array(),common=J::array(),common_down_up=J::array();
        for(const auto scene:{"tap","hold","dense","slow","writer","rpc","fault"})
            t.check(std::string("historical_rgb_bytes_unchanged_")+scene,historical(scene)==cost_workload::stimulus(scene));
        for(int variant=0;variant<4;++variant)layer.push_back(extraction_control(variant));
        t.check("original_cyan_dark30_zero_body_contact_front_and_down",layer[0]["body_frames"]==0&&layer[0]["contact_frames"]==0&&layer[0]["front_frames"]==0&&layer[0]["eligible_frames"]==0);
        t.check("blue_only_repairs_body_not_front_or_down",layer[1]["body_frames"]==12&&layer[1]["contact_frames"].get<int>()>0&&layer[1]["front_frames"]==0&&layer[1]["eligible_frames"]==0);
        t.check("black_only_cannot_repair_cyan_body",layer[2]["body_frames"]==0&&layer[2]["eligible_frames"]==0);
        t.check("both_witness_repairs_produce_v3_eligibility",layer[3]["body_frames"]==12&&layer[3]["front_frames"]==12&&layer[3]["eligible_frames"].get<int>()>0);
        for(const auto scene:{"hold","tap"})for(bool candidate:{false,true}) {
            auto r=owner_control(scene,candidate);
            const std::string id=std::string(scene)+(candidate?"_B":"_A");
            t.check(id+"_complete_120_same_frame_inputs",r["processed"]==120&&r["same_frame_leases"]==true&&r["pool_drops"]==0);
            t.check(id+"_safe_own_ledger_and_release",r["safe"]==true&&r["ledger_valid"]==true&&r["release_verified"]==true&&r["active_final"]==0&&r["failed_injections"]==0);
            t.check(id+"_all_phase_receipts_counted",r["phase_counts"][0]==r["own_down"]&&r["phase_counts"][1]==r["own_move"]&&r["phase_counts"][2]==r["own_up"]);
            if(std::string_view(scene)=="hold")
                t.check(id+"_historical_zero_action_preserved",r["own_down"]==0&&r["own_up"]==0&&r["receipt_count"]==0);
            if(std::string_view(scene)=="tap")
                t.check(id+"_actual_own_down_and_up",r["own_down"].get<int>()>0&&r["own_up"].get<int>()>0);
            runs.push_back(std::move(r));
        }
        auto verify=[&](const J& r,const std::string& id) {
            t.check(id+"_complete_safe_release",r["processed"]==12&&r["same_frame_leases"]==true&&
                r["pool_drops"]==0&&r["safe"]==true&&r["ledger_valid"]==true&&
                r["release_verified"]==true&&r["active_final"]==0&&r["failed_injections"]==0);
            t.check(id+"_phase_receipts",r["phase_counts"][0]==r["own_down"]&&
                r["phase_counts"][1]==r["own_move"]&&r["phase_counts"][2]==r["own_up"]);
        };
        for(int head=490;head<=510;++head) {
            bool both=true,both_down_up=true;
            for(bool candidate:{false,true}) {
                auto r=owner_control("entry-release-diagnostic",candidate,12,head);
                verify(r,"sweep_"+std::to_string(head)+(candidate?"_B":"_A"));
                both&=r["own_down"].get<int>()>0;
                both_down_up&=r["own_down"].get<int>()>0&&r["own_up"].get<int>()>0;
                if(head==503&&candidate)
                    t.check("repaired_B_retains_real_own_contact_at_window_end",
                        r["own_down"]==1&&r["own_up"]==0&&r["active_before_final_release"]==1&&
                        r["own_active_contacts_before_final_release"].size()==1);
                sweep.push_back(std::move(r));
            }
            if(both)common.push_back(head);
            if(both_down_up)common_down_up.push_back(head);
        }
        // Characterizes this unchanged observer/owner, not a desired physical
        // oracle: the only common phase cannot qualify a robust shared load.
        t.check("common_entry_is_only_single_phase_not_robust_interval",common==J::array({503}));
        for(const auto intervention:{cost_workload::Intervention::stationary_body,
            cost_workload::Intervention::leave,cost_workload::Intervention::absence,
            cost_workload::Intervention::effect})for(bool candidate:{false,true}) {
            auto r=owner_control("entry-release-diagnostic",candidate,12,503,intervention);
            const std::string id=std::string(cost_workload::name(intervention))+(candidate?"_B":"_A");
            verify(r,id);
            t.check(id+"_no_down_retry",r["own_down"]==1);
            if(intervention!=cost_workload::Intervention::stationary_body)
                t.check(id+"_safe_up_after_evidence_loss",r["own_up"]==1);
            else
                t.check(id+"_continuation_difference_reported",r["own_up"]==(candidate?0:1));
            negatives.push_back(std::move(r));
        }
        J report={{"schema","pas.prelive-cost-workload-contract.v1"},{"checks",t.checks},
            {"assertions",t.checks.size()},{"failed",t.failures},{"extraction_controls",layer},{"owner_controls",runs},
            {"sweep",sweep},{"negative_controls",negatives},{"common_entry_head_positions",common},
            {"shared_down_up_within12frames",common_down_up},
            {"classification_change","shared entry now means own Down; prior diagnostic conflated it with early Up; old failure evidence retained"},
            {"device_endpoints",0},{"physical_gold",0},{"windows_cost_qualification",false},
            {"old_stopped_campaign_reopened",false},{"new_cost_scene_admitted",false},
            {"normal_hold_tail_completion_proved",false},{"study_disposition","NOT_READY_no_robust_common_Hold_workload"}};
        std::ofstream out(argv[1]);out<<report.dump(2)<<'\n';if(!out)throw std::runtime_error("report write");
        std::cout<<"workload checks="<<t.checks.size()<<" failed="<<t.failures<<'\n';return t.failures?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
