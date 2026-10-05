#include "x5_scope.hpp"
#include "x3_features.hpp"
#include "pas/analysis.hpp"
#include <deque>
#include <iostream>
#include <set>
#define NOMINMAX
#include <windows.h>

// Fixed-source descriptive annex, deliberately separate from NDA-v1 output.
// stdout is captured create-new under the repair batch's bounded writer.
namespace {
using namespace pas::x5;
using pas::review::json;
constexpr auto parent_sha="8bf5b5a5973bd028a254a8cc6b7179720aaed6468c0d00858e0ea0f8c6d20770";
struct MemoryLimit {
    HANDLE h=CreateJobObjectW(nullptr,nullptr);
    MemoryLimit(){JOBOBJECT_EXTENDED_LIMIT_INFORMATION l{};l.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_MEMORY;l.ProcessMemoryLimit=512ULL*1024*1024;
        if(!h||!SetInformationJobObject(h,JobObjectExtendedLimitInformation,&l,sizeof(l))||!AssignProcessToJobObject(h,GetCurrentProcess()))throw std::runtime_error("scope_memory_cap_unavailable");}
    ~MemoryLimit(){if(h)CloseHandle(h);}
};
bool packet_frame(std::size_t n){return (n>=5286&&n<=5288)||(n>=6160&&n<=6164)||(n>=6170&&n<=6180)||(n>=6193&&n<=6195)||(n>=6210&&n<=6214);}
struct Prior {const json* frame=nullptr;const json* object=nullptr;};
Prior previous(const json& o,const std::deque<const json*>& history){
    const auto& track=o.at("identity_assignment");
    if(track.is_null()||track.at("identity_ambiguous")==true||track.at("assigned_prior_index").get<int>()<0)return {};
    for(const auto* f:history)if(f->at("fake_capture_ns")==track.at("prior_observed_ns"))
        for(const auto& old:f->at("objects"))if(old.at("note_id")==o.at("note_id")){
            const auto& old_track=old.at("identity_assignment");
            if(old_track.is_null()||old_track.at("identity_ambiguous")==true)return {};
            const auto p=pas::x3::point(old.at("center"));
            if(std::hypot(p.x-track.at("prior_x").get<double>(),p.y-track.at("prior_y").get<double>())>1e-6)throw std::runtime_error("scope_prior_binding");
            return {f,&old};
        }
    return {};
}
json turn(const json& f,const json& o,const std::deque<const json*>& history){
    const auto b=previous(o,history);if(!b.object)return nullptr;
    const auto a=previous(*b.object,history);if(!a.object)return nullptr;
    const auto now=f.at("fake_capture_ns").get<std::int64_t>(),bn=b.frame->at("fake_capture_ns").get<std::int64_t>(),an=a.frame->at("fake_capture_ns").get<std::int64_t>();
    if(now-an>90'000'000||bn-an<10'000'000||now-bn<10'000'000)return nullptr;
    const auto p=pas::x3::point(a.object->at("center")),q=pas::x3::point(b.object->at("center")),r=pas::x3::point(o.at("center"));
    const auto u=pas::x3::sub(q,p),v=pas::x3::sub(r,q);const double du=pas::x3::norm(u),dv=pas::x3::norm(v);
    if(du<3||dv<3||du/((bn-an)/1e9)>4000||dv/((now-bn)/1e9)>4000)return nullptr;
    const auto angle=std::acos(std::clamp(pas::x3::dot(u,v)/(du*dv),-1.,1.));
    if(angle<.5235987755982988)return nullptr;
    return {{"role",f.at("role")},{"ordinals",json::array({a.frame->at("ordinal"),b.frame->at("ordinal"),f.at("ordinal")})},
        {"note_id_local_only",o.at("note_id")},{"kind",o.at("kind")},{"centers",json::array({a.object->at("center"),b.object->at("center"),o.at("center")})},
        {"angle_rad",angle},{"dt_ns",json::array({bn-an,now-bn})},{"grade","measured_runtime_correspondence_turn_proposal_not_physical_gold"}};
}
}
int main(int argc,char** argv){try {
    if(argc!=2)throw std::runtime_error("x5_scope_report accepted_report_json > new_annex_json");
    MemoryLimit limit;const std::filesystem::path path=argv[1];
    if(std::filesystem::file_size(path)>6*1024*1024)throw std::runtime_error("scope_input_capacity");
    if(pas::sha256_file(path)!=parent_sha)throw std::runtime_error("scope_parent_SHA");
    std::ifstream stream(path);const auto report=json::parse(stream);
    if(report.at("frames").size()!=337)throw std::runtime_error("scope_frame_denominator");
    json out={{"schema",1},{"contract","X5-lineage-scope-v1"},{"parent_report_sha256",parent_sha},{"binary_sha256",pas::sha256_file(argv[0])},
        {"purpose","descriptive early-scope strata only; do not feed to NDA-v1 or generate actions"},{"new_full_contact_replays",0},{"human_gold_added",0},
        {"scope_definition","C36h strong unique non-Hold and preselection prior relation samples <3; main50 explicit unconfirmed. Neither grants selection or Down eligibility."},
        {"counts",json::object()},{"frames",json::array()},{"real_case_packets",json::array()},{"turn_proposals",json::array()},
        {"line_columns",report.at("line_columns")},{"latest_secant_columns",report.at("latest_secant_columns")}};
    std::deque<const json*> history;std::string last_role;std::size_t count=0,turn_count=0;std::set<std::size_t> packets;
    for(const auto& f:report.at("frames")){
        const auto role=f.at("role").get<std::string>();const auto lineage=role=="c36h_reference"?Lineage::c36h:Lineage::main50;
        if(role!="c36h_reference"&&role!="main50_control"&&role!="main50_preconfirmation_role_oracle")throw std::runtime_error("scope_role");
        if(role!=last_role){history.clear();last_role=role;}
        const auto now=f.at("fake_capture_ns").get<std::int64_t>();
        while(!history.empty()&&now-history.front()->at("fake_capture_ns").get<std::int64_t>()>90'000'000)history.pop_front();
        json objects=json::array();auto& counts=out["counts"][role];if(counts.is_null())counts={{"targets",0},{"early_comparison_eligible",0},{"reasons",json::object()}};
        if(f.at("objects").size()>128)throw std::runtime_error("scope_object_capacity");
        for(const auto& o:f.at("objects")){
            const auto e=scope_evidence(lineage,o);const auto s=research_scope(e);
            counts["targets"]=counts.at("targets").get<int>()+1;
            if(s.early_comparison_eligible)counts["early_comparison_eligible"]=counts.at("early_comparison_eligible").get<int>()+1;
            counts["reasons"][s.reason]=counts["reasons"].value(s.reason,0)+1;
            objects.push_back({{"note_id_local_only",o.at("note_id")},{"center",o.at("center")},{"prior_relation_samples",e.prior_samples?json(*e.prior_samples):json(nullptr)},
                {"current_preserve_observed",e.preserve?json(*e.preserve):json(nullptr)},{"confirmation_semantics",s.confirmation_semantics},{"early_comparison_eligible",s.early_comparison_eligible},{"reason",s.reason}});
            if(auto t=turn(f,o,history);!t.is_null()){if(++turn_count>128)throw std::runtime_error("scope_turn_capacity");out["turn_proposals"].push_back(std::move(t));}
            if(++count>1220)throw std::runtime_error("scope_target_capacity");
        }
        out["frames"].push_back({{"role",role},{"ordinal",f.at("ordinal")},{"objects",std::move(objects)}});
        const auto n=f.at("ordinal").get<std::size_t>();
        if(packet_frame(n)){
            const std::filesystem::path png=f.at("png_path").get<std::string>();
            if(packets.insert(n).second&&pas::sha256_file(png)!=f.at("png_sha256").get<std::string>())throw std::runtime_error("scope_png_SHA");
            out["real_case_packets"].push_back(f);
        }
        history.push_back(&f);while(history.size()>5)history.pop_front();
    }
    if(count!=1220)throw std::runtime_error("scope_target_denominator");
    out["target_occurrences"]=count;out["unique_packet_png_verified"]=packets.size();
    out["turn_search"]={{"scope","all 337 fixed role frames, 1220 targets; immediate explicit unique runtime edges only"},{"history_frames",6},{"history_ns",90000000},
        {"minimum_pair_dt_ns",10000000},{"minimum_displacement_px",3},{"maximum_speed_px_s",4000},{"minimum_angle_rad",.5235987755982988},
        {"proposal_count",turn_count},{"absence_is_not_proof_of_no_turn",true},{"rule_input",false}};
    const auto encoded=out.dump();if(encoded.size()+1>2*1024*1024)throw std::runtime_error("scope_output_capacity");
    std::cout<<encoded<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
