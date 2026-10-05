#include "review_io.hpp"
#include "x2_input.hpp"
#include "pas/analysis.hpp"
#include <iostream>
#include <set>
#include <map>
using namespace pas::review;
namespace {
json core(json row) {
    json result;for(const auto key:{"ordinal","consumed","scene","lifecycle","owner","contacts","candidate_bank","prefix_identity_summary","scheduler"})
        if(row.contains(key))result[key]=row.at(key);return result;
}
bool subject(const std::string& id,const json& t) {
    const auto kind=t.value("kind","");const double x=t.value("x",0.),y=t.value("y",0.);
    if(id=="A3498")return kind=="hold"&&x>240&&x<370&&y>450;
    if(id=="B4986")return kind=="hold";
    if(id=="C5520")return kind=="flick";
    if(id=="D6214")return kind=="drag"&&y>250&&y<325;
    return true;
}
json slim(const json& t) {
    json j;for(const auto key:{"note_id","kind","x","y","line_id","samples","history_span_ns","reason","crossing_ns","head_on_line","held_body_evidence","held_body_patch","rails_geometry","line_projection_only"})if(t.contains(key))j[key]=t.at(key);return j;
}
void bounded_save(const json& m,const fs::path& output,const json& j) {
    const auto bytes=j.dump(2).size()+1;
    const auto total=[](const fs::path& root){std::uint64_t b=0;if(fs::exists(root))for(const auto& e:fs::recursive_directory_iterator(root))if(e.is_regular_file())b+=e.file_size();return b;};
    const fs::path batch=m.at("batch_root").get<std::string>();
    if(fs::absolute(output).lexically_normal().generic_string().find(fs::absolute(batch).lexically_normal().generic_string()+"/")!=0)throw std::runtime_error("output_outside_batch");
    if(total(batch)+bytes>67108864||total(m.at("campaign_root").get<std::string>())+total(m.at("prior_research_root").get<std::string>())+bytes>8ULL*1024*1024*1024)throw std::runtime_error("output_quota");
    save(output,j);
}
}
int main(int argc,char** argv){try {
    if(argc!=5)throw std::runtime_error("x2_report x2_manifest x2_comparison x1_batch new_report");
    const fs::path output=argv[4];if(fs::exists(output))throw std::runtime_error("output_exists");
    const auto m=load(argv[1]),comparison=load(argv[2]);const fs::path batch=m.at("batch_root").get<std::string>(),x1=argv[3];
    const auto sha=pas::sha256_file(argv[1]);if(comparison.at("manifest_sha256")!=sha)throw std::runtime_error("comparison_manifest_SHA");
    const std::array<fs::path,3> roots{{batch/"c36h-reference-on-1",batch/"main50-control-on-1",batch/"main50-no-override-on-1"}};
    const std::array<json,3> s{{load(roots[0]/"summary.json"),load(roots[1]/"summary.json"),load(roots[2]/"summary.json")}};
    pas::x2::validate_runs(m,sha,s);
    json repeat=json::array();
    for(const auto name:{"main50-no-override-on-2","main50-no-override-off"}) {
        const fs::path root=batch/name;const auto summary=load(root/"summary.json");
        for(const auto key:{"semantic_sha256","binary_sha256","source_provenance_sha256","input_manifest_sha256","role","variant","cadence","tie","recognition","receipt_policy"})
            if(summary.at(key)!=s[2].at(key))throw std::runtime_error(std::string("determinism_")+key);
        if(summary.at("success")!=true||summary.at("verified_pngs")!=7722||summary.at("contacts_at_exit")!=0)throw std::runtime_error("repeat_failed_or_incomplete");
        for(const auto name2:{"events.jsonl","state-digests.jsonl","mechanism.jsonl"})if(pas::sha256_file(root/name2)!=pas::sha256_file(roots[2]/name2))throw std::runtime_error(std::string("repeat_file_")+name2);
        const bool tracing=summary.at("trace").get<bool>();
        if(tracing&&pas::sha256_file(root/"trace.jsonl")!=pas::sha256_file(roots[2]/"trace.jsonl"))throw std::runtime_error("repeat_trace_SHA");
        repeat.push_back({{"run",name},{"semantic_sha256",summary.at("semantic_sha256")},{"events_state_and_mechanism_byte_equal",true},{"trace_byte_equal",tracing?json(true):json("off")}});
    }
    json controls=json::array();
    for(std::size_t i=0;i<2;++i) {
        const fs::path old=x1/(i==0?"c36h-verified-on-1":"main50-verified-on-1");const auto previous=load(old/"summary.json");
        if(previous.at("semantic_sha256")!=s[i].at("semantic_sha256")||pas::sha256_file(old/"events.jsonl")!=pas::sha256_file(roots[i]/"events.jsonl"))throw std::runtime_error("X1_control_semantics_changed");
        const auto a=rows(old/"trace.jsonl",600),b=rows(roots[i]/"trace.jsonl",600);if(a.size()!=b.size())throw std::runtime_error("X1_control_trace_denominator");
        for(std::size_t n=0;n<a.size();++n)if(core(a[n])!=core(b[n]))throw std::runtime_error("X1_control_trace_semantics_changed");
        controls.push_back({{"role",pas::x2::roles[i]},{"X1_semantic_digest_equal",true},{"events_byte_equal",true},{"trace_core_frames_equal",a.size()},
            {"normalization","exclude readonly diagnostics (new x2 fields and shifted export __LINE__ sites), metadata paths and cost; scene/lifecycle/owner/contacts/candidates/prefix/scheduler exact"}});
    }
    json cases=json::array();std::array<std::vector<json>,3> trace{{rows(roots[0]/"trace.jsonl",600),rows(roots[1]/"trace.jsonl",600),rows(roots[2]/"trace.jsonl",600)}};
    for(const auto& w:m.at("windows")) {
        const auto id=w.at("id").get<std::string>();json per_frame=json::array();
        for(std::size_t i=0;i<trace[0].size();++i) {
            const auto ordinal=trace[0][i].at("ordinal").get<std::size_t>();if(ordinal<w.at("first").get<std::size_t>()||ordinal>w.at("last").get<std::size_t>())continue;
            json frame={{"ordinal",ordinal},{"png_sha256",trace[0][i].at("png_sha256")},{"roles",json::object()}};
            for(std::size_t r=0;r<3;++r) {
                const auto& row=trace[r][i];json targets=json::array(),choices=json::array(),history=json::array(),support=json::array(),owners=json::array();std::set<std::uint64_t> ids;
                if(!row.at("scene").is_null())for(const auto& target:row.at("scene").at("targets"))if(subject(id,target)){targets.push_back(slim(target));ids.insert(target.at("note_id").get<std::uint64_t>());}
                for(const auto& d:row.at("diagnostics"))if(d.contains("note_id")&&ids.contains(d.at("note_id").get<std::uint64_t>())) {
                    if(d.at("event")=="x2_winner_only")choices.push_back(d);
                    if(d.at("event")=="history_reset")history.push_back(d);
                    if(d.at("event")=="owner_current_support")support.push_back(d);
                }
                if(!row.at("owner").is_null())for(const auto& owner:row.at("owner").at("identities"))if(ids.contains(owner.at("note_id").get<std::uint64_t>()))owners.push_back(owner);
                frame["roles"][pas::x2::roles[r]]={{"targets",targets},{"winner_choices",choices},{"history_resets",history},{"owner_current_support",support},{"owners",owners},{"contacts",row.at("contacts")}};
            }
            per_frame.push_back(std::move(frame));
        }
        const auto& pair=comparison.at("control_vs_variant").at("cases");const auto at=std::find_if(pair.begin(),pair.end(),[&](const auto& c){return c.at("id")==id;});
        json c={{"id",id},{"window",w},{"per_frame",per_frame}};
        for(const auto key:{"first_evidence_divergence_in_window","first_state_divergence_in_window","first_action_divergence_in_window","first_action_divergence_available_prefix","main50_control_successful_down_prefix","main50_no_confirmed_winner_override_successful_down_prefix","main50_control_actions","main50_no_confirmed_winner_override_actions","main50_control_prefix_action_count","main50_no_confirmed_winner_override_prefix_action_count"})c[key]=at->at(key);
        cases.push_back(std::move(c));
    }
    json safety=json::array();
    for(std::size_t i=0;i<3;++i) {
        const auto events=rows(roots[i]/"events.jsonl",100000);std::map<std::uint64_t,int> downs;std::size_t duplicates=0,unknown=0,unknown_retry=0;std::set<std::uint64_t> unknown_intents;
        for(const auto& e:events)if(e.at("event")=="fake_receipt") {
            const auto& c=e.at("command");if(c.at("phase")!=0)continue;
            const auto intent=c.at("intent_id").get<std::uint64_t>();if(unknown_intents.contains(intent))++unknown_retry;
            if(!e.at("success").get<bool>()){++unknown;unknown_intents.insert(intent);}
            else if(!e.at("note_id").is_null()&&++downs[e.at("note_id").get<std::uint64_t>()]>1)++duplicates;
        }
        safety.push_back({{"role",pas::x2::roles[i]},{"successful_downs_same_local_note_repeated",duplicates},{"unknown_downs",unknown},{"unknown_down_retry",unknown_retry},
            {"contacts_at_exit",s[i].at("contacts_at_exit")},{"guard_scope","ReplayTouch rejects duplicate active contact IDs; local-note count does not establish physical identity gold"},
            {"unsupported_Move_premature_Up_lost_contact_invalid_resurrection","compare per-case prefix/scene/current support; global physical correctness unknown; original fake-clock negatives retained"}});
    }
    bounded_save(m,output,{{"experiment",m.at("experiment")},{"manifest_sha256",sha},{"comparison_sha256",pas::sha256_file(argv[2])},
        {"analysis_binary_sha256",pas::sha256_file(argv[0])},{"roles",m.at("roles")},{"repeat_and_trace_off",repeat},{"X1_control_equivalence",controls},
        {"first_state_divergence_available_prefix",comparison.at("first_state_divergence_available_prefix")},
        {"first_action_divergence_available_prefix",comparison.at("first_action_divergence_available_prefix")},{"cases",cases},{"safety",safety},
        {"primary_runs_denominator",5},{"physical_gold",false},{"gameplay_outcome","unknown; fixed C36g pixels, C36h remains reference baseline"}});
    std::cout<<"repeat/off and X1 controls exactly verified; five case chains saved\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
