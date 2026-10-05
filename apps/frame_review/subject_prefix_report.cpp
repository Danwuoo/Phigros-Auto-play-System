#include "review_io.hpp"
#include "pas/analysis.hpp"
#include <set>
#include <map>
#include <iostream>

using namespace pas::review;
namespace {
json prefix_actions(const std::vector<json>& events, const std::set<std::uint64_t>& notes,
                    std::size_t last) {
    json actions=json::array();
    std::map<int,std::uint64_t> active;
    for(const auto& event:events) {
        const auto ordinal=event.at("ordinal_at_delivery").get<std::size_t>();
        if(ordinal>last) break;
        const auto type=event.at("event").get<std::string>();
        if(type=="fake_receipt") {
            const auto& command=event.at("command");
            const int contact=command.at("contact_id"),phase=command.at("phase");
            const auto note=event.at("note_id").is_null()?0:event.at("note_id").get<std::uint64_t>();
            if(notes.contains(note)) {
                auto semantic=command;semantic.erase("intent_id");semantic.erase("contact_id");
                semantic["ordinal"]=ordinal;semantic["success"]=event.at("success");
                semantic["reason"]=event.at("reason");
                actions.push_back({{"semantic",semantic},{"local_note_id",note},
                    {"local_contact_id",contact},{"injection_start_ns",event.at("injection_start_ns")},
                    {"injection_return_ns",event.at("injection_return_ns")}});
            }
            if(phase==2)active.erase(contact);else active[contact]=note;
        } else if(type=="fake_release") {
            for(const auto& value:event.at("report").at("requested_ids")) {
                const int contact=value;
                if(active.contains(contact)&&notes.contains(active.at(contact)))
                    actions.push_back({{"semantic",{{"ordinal",ordinal},{"phase","release_all"},
                        {"start_ns",event.at("report").at("start_ns")}}},
                        {"local_note_id",active.at(contact)},{"local_contact_id",contact}});
            }
            active.clear();
        }
    }
    return actions;
}
}
int main(int argc,char** argv) {try {
    if(argc!=5)throw std::runtime_error("x1_subject_prefix_report five-cases c36h-run main50-run new-report");
    if(fs::exists(argv[4]))throw std::runtime_error("output_exists");
    const auto report=load(argv[1]);
    if(report.at("cases").size()!=5)throw std::runtime_error("five_cases_required");
    for(const auto& pair:std::array<std::pair<fs::path,const char*>,2>{{
        {fs::path(argv[2])/"summary.json","a_summary_sha256"},
        {fs::path(argv[3])/"summary.json","b_summary_sha256"}}})
        if(pas::sha256_file(pair.first)!=report.at(pair.second).get<std::string>())
            throw std::runtime_error("comparison_summary_SHA");
    const auto apath=fs::path(argv[2])/"events.jsonl",bpath=fs::path(argv[3])/"events.jsonl";
    const auto a=rows(apath,100000),b=rows(bpath,100000);json cases=json::array();
    for(const auto& item:report.at("cases")) {
        std::set<std::uint64_t> anotes,bnotes;
        for(const auto& frame:item.at("geometry_pairs"))for(const auto& pair:frame.at("pairs")) {
            anotes.insert(pair.at("a_note_id").get<std::uint64_t>());
            bnotes.insert(pair.at("b_note_id").get<std::uint64_t>());
        }
        for(const auto& down:item.at("c36h_successful_down_prefix"))anotes.insert(down.at("note_id_for_local_join").get<std::uint64_t>());
        for(const auto& down:item.at("main50_successful_down_prefix"))bnotes.insert(down.at("note_id_for_local_join").get<std::uint64_t>());
        const auto last=item.at("window").at("last").get<std::size_t>();
        const auto aa=prefix_actions(a,anotes,last),bb=prefix_actions(b,bnotes,last);json first=nullptr;
        for(std::size_t i=0;i<std::max(aa.size(),bb.size());++i) {
            const auto av=i<aa.size()?aa[i].at("semantic"):json(nullptr);
            const auto bv=i<bb.size()?bb[i].at("semantic"):json(nullptr);
            if(av!=bv) {first={{"a",i<aa.size()?aa[i]:json(nullptr)},
                {"b",i<bb.size()?bb[i]:json(nullptr)}};break;}
        }
        cases.push_back({{"id",item.at("id")},{"c36h_subject_note_ids",anotes},
            {"main50_subject_note_ids",bnotes},{"c36h_prefix_action_count",aa.size()},
            {"main50_prefix_action_count",bb.size()},{"first_action_divergence_available_prefix",first}});
    }
    save(argv[4],{{"comparison_report",fs::absolute(argv[1]).generic_string()},
        {"comparison_report_sha256",pas::sha256_file(argv[1])},
        {"binary_sha256",pas::sha256_file(argv[0])},
        {"c36h_events_sha256",pas::sha256_file(apath)},{"main50_events_sha256",pas::sha256_file(bpath)},
        {"scope","ordered actions of proposed geometric subject cohort from earliest available input through window.last; not global physical first divergence"},
        {"cases",cases},{"cases_denominator",5},{"physical_gold",false}});return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
