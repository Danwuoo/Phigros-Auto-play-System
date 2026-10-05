#include "x3_report.hpp"
#include "x3_features.hpp"
#include "x2_input.hpp"
#include "pas/analysis.hpp"
#include "pas/adb.hpp"
#include <deque>
#include <map>
#include <set>
#include <iostream>
#define NOMINMAX
#include <windows.h>

namespace pas::x3 {
using namespace pas::review;
namespace {
constexpr std::uint64_t report_cap=6*1024*1024,batch_cap=32*1024*1024,campaign_cap=8ULL*1024*1024*1024;
std::vector<json> bounded_rows(const fs::path& p,std::size_t count,std::size_t bytes){std::vector<json> out;each_row(p,count,[&](json j){out.push_back(std::move(j));},bytes);return out;}
json upper_band_probe(const fs::path& p){
    if(fs::file_size(p)>4*1024*1024)throw std::runtime_error("x3_png_capacity");const auto f=pas::load_diagnostic_png(p);
    if(f.width!=1280||f.height!=720||f.stride!=3840)throw std::runtime_error("x3_png_geometry");
    double best=-1;int row=0;json samples=json::array();
    for(int y=96;y<=160;++y){double sum=0;for(int x=96;x<=864;x+=96){const auto* a=f.rgb.data()+y*f.stride+x*3;sum+=(a[0]+a[1]+a[2])/3.;}if(sum>best){best=sum;row=y;}}
    for(int x=96;x<=864;x+=96){const auto* a=f.rgb.data()+row*f.stride+x*3;const auto* b=a+8*f.stride;const bool white=a[0]>195&&a[1]>195&&a[2]>195&&std::max({a[0],a[1],a[2]})-std::min({a[0],a[1],a[2]})<35;
        samples.push_back({{"x",x},{"y",row},{"rgb",json::array({a[0],a[1],a[2]})},{"rgb_below_8px",json::array({b[0],b[1],b[2]})},{"original_white_class_predicate",white}});}
    return {{"scope","manual_research_ROI_y96_160_x96_864_no_strategy_input"},{"brightest_sampled_row",row},{"mean_RGB",best/9},{"samples",samples},{"physical_line_continuity","proposed_not_gold"}};
}
struct Lease {
    HANDLE h=CreateMutexW(nullptr,FALSE,L"Local\\PAS_X1ContactReplayBudget");
    Lease(){if(!h)throw std::runtime_error("batch_writer_mutex");const auto v=WaitForSingleObject(h,0);if(v!=WAIT_OBJECT_0&&v!=WAIT_ABANDONED){CloseHandle(h);h=nullptr;throw std::runtime_error("batch_writer_busy");}}
    ~Lease(){if(h){ReleaseMutex(h);CloseHandle(h);}}
};
std::uint64_t tree_bytes(const fs::path& p){std::uint64_t n=0;for(const auto& e:fs::recursive_directory_iterator(p)){if(fs::is_symlink(e.symlink_status()))throw std::runtime_error("x3_reparse_input");if(e.is_regular_file())n+=e.file_size();}return n;}
void path_safe(const fs::path& p){for(auto a=fs::absolute(p);!a.empty();a=a.parent_path()){if(fs::exists(a)&&(GetFileAttributesW(a.c_str())&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("x3_reparse_path");if(a==a.root_path())break;}}
void save_report(const fs::path& p,const json& m,const json& r){
    const auto payload=r.dump()+"\n";if(payload.size()>report_cap)throw std::runtime_error("x3_report_capacity");
    path_safe(p);const auto batch=fs::canonical(m.at("batch_root").get<std::string>());
    if(fs::canonical(p.parent_path())!=batch)throw std::runtime_error("x3_output_outside_batch");
    if(tree_bytes(batch)+payload.size()>batch_cap||tree_bytes(m.at("campaign_root").get<std::string>())+tree_bytes(m.at("prior_research_root").get<std::string>())+payload.size()>campaign_cap)throw std::runtime_error("x3_output_quota");
    const auto h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("x3_output_exists_or_open");
    DWORD written=0;const bool ok=WriteFile(h,payload.data(),static_cast<DWORD>(payload.size()),&written,nullptr)&&written==payload.size();CloseHandle(h);if(!ok)throw std::runtime_error("x3_output_write_incomplete");
}
std::string case_id(std::size_t n,const json& m){if(n>=1533&&n<=1537)return "G1535";for(const auto& w:m.at("windows"))if(n>=w.at("first").get<std::size_t>()&&n<=w.at("last").get<std::size_t>())return w.at("id");throw std::runtime_error("x3_ordinal_outside_query");}
bool subject(const std::string& c,const json& t){const auto k=t.at("kind").get<std::string>();const double x=t.at("x"),y=t.at("y");if(c=="G1535")return t.at("note_id")==184;if(c=="D6214")return k=="drag"&&y>250&&y<325;if(c=="A3498")return k=="hold"&&x>240&&x<370&&y>450;if(c=="B4986")return k=="hold";if(c=="C5520")return k=="flick";return true;}
const json* diag(const json& r,const char* event,std::uint64_t id){const json* found=nullptr;for(const auto& d:r.at("diagnostics"))if(d.value("event","")==event&&d.contains("note_id")&&d.at("note_id")==id){if(found)throw std::runtime_error("x3_duplicate_diagnostic");found=&d;}return found;}
json prefix(const json& r,std::uint64_t id){if(!r.contains("prefix_identity_summary"))return nullptr;for(const auto& e:r.at("prefix_identity_summary"))if(e.at(0)==id)return e.at(1);return nullptr;}
json owner(const json& r,std::uint64_t id){if(r.at("owner").is_null())return nullptr;for(const auto& e:r.at("owner").at("identities"))if(e.at("note_id")==id)return e;return nullptr;}
struct Measured {Line l;Point p;std::int64_t ns=0;std::size_t ordinal=0;};
struct State {std::deque<std::vector<Line>> poses;std::map<std::uint64_t,Measured> accepted;std::map<std::uint64_t,json> confirm;};
std::optional<Line> previous_line(const State& s,std::uint64_t id,std::int64_t ns){for(const auto& frame:s.poses)for(const auto& l:frame)if(l.id==id&&l.observed==ns)return l;return {};}
json features(const json& row,const json& t,State& s,const std::vector<Line>& lines){
    const auto id=t.at("note_id").get<std::uint64_t>();const auto now=row.at("scene").at("capture_complete_ns").get<std::int64_t>();const Point p{t.at("x"),t.at("y")};
    const auto ct=diag(row,"candidate_track",id),selection=diag(row,"relation_selection",id),choice=diag(row,"x2_winner_only",id);
    const json* candidate=nullptr;if(ct)for(const auto& c:row.at("candidate_bank").at("candidates"))if(c.at("candidate_id")==ct->at("candidate_id"))candidate=&c;
    json measurements=json::array();
    for(const auto& raw:row.at("candidate_bank").at("lines")) {
        const auto l=line(raw);auto local=l;local.u=canonical(l.u);const double d=signed_distance(p,local);
        json pair={{"valid",false},{"reason","prior_note_geometry_unknown"}};
        if(ct&&!ct->at("identity_ambiguous").get<bool>())pair=measured_pair(p,l,{ct->at("prior_x"),ct->at("prior_y")},previous_line(s,l.id,ct->at("prior_observed_ns")),ct->at("prior_observed_ns"),now);
        else if(ct)pair["reason"]="ambiguous_note_identity";
        json score=nullptr,gate=nullptr;for(const auto& q:row.at("diagnostics"))if(q.contains("note_id")&&q.at("note_id")==id&&q.contains("line_id")&&q.at("line_id")==l.id){if(q.at("event")=="relation_score")score=q;if(q.at("event")=="relation_gate")gate=q;}
        const auto along=dot(sub(p,l.c),local.u);json overlap=nullptr;json alignment=nullptr;
        if(candidate){const auto nu=point(candidate->at("note").at("tangent"));const auto projected=hit(p,local);const auto offset=sub(projected,p);
            overlap=std::abs(dot(offset,nu))<=candidate->at("note").at("width").get<double>()*.5+2&&std::abs(dot(offset,{-nu.y,nu.x}))<=candidate->at("note").at("height").get<double>()*.5+2&&std::abs(along)<=l.length*.5;
            alignment=std::abs(dot(nu,l.u));}
        json instantaneous=nullptr;
        if(l.motion&&pair.value("valid",false)) {const auto v=point(pair.at("note_velocity_px_s")),lv=point(raw.at("velocity"));const auto delta=sub(p,l.c);const auto omega=raw.at("angular_velocity").get<double>();instantaneous=dot(sub(v,lv),{-local.u.y,local.u.x})-omega*dot(delta,local.u);}
        measurements.push_back({{"line_id",l.id},{"current_observed",l.observed==now},{"association_valid",l.valid},{"observed_ns",l.observed},{"evidence_age_ns",now-l.observed},{"center",vec(l.c)},{"tangent",vec(local.u)},{"normal",vec({-local.u.y,local.u.x})},{"signed_normal_distance_px",d},{"along_px",along},{"note_appearance_tangent_dot",alignment},{"core_contains_projected_hit",overlap},{"pair",pair},{"motion_model_valid",l.motion},{"motion_model_velocity_px_s",l.motion?raw.at("velocity"):json(nullptr)},{"angular_velocity_rad_s",l.motion?raw.at("angular_velocity"):json(nullptr)},{"relative_normal_velocity_model_px_s",instantaneous},{"relative_model_valid",!instantaneous.is_null()},{"score",score},{"gate",gate},{"judgment_role","unknown"}});
    }
    json confirmation={{"line_id",nullptr},{"established_ns",nullptr},{"established_ordinal",nullptr},{"source","trace_not_exported"}};
    if(selection&&selection->contains("confirmed_line_id")) {
        const auto cid=selection->at("confirmed_line_id").get<std::uint64_t>();confirmation["line_id"]=cid;
        if(cid) {
            if(s.confirm.contains(id)&&s.confirm[id].at("line_id")==cid)confirmation=s.confirm[id];
            else {confirmation["source"]="first_seen_pre_selection_confirmed_exact_establishment_unknown";confirmation["first_seen_confirmed_ordinal"]=row.at("ordinal");}
        } else confirmation["source"]="no_confirmed_relation";
    }
    json guard=nullptr;const auto winner=selection?selection->at("winner").get<std::uint64_t>():0;
    const auto cid=confirmation.at("line_id").is_null()?0:confirmation.at("line_id").get<std::uint64_t>();
    if(winner&&cid&&winner!=cid) {
        const bool visible=std::any_of(lines.begin(),lines.end(),[&](const auto& l){return l.id==cid&&l.valid;});
        auto found=std::find_if(lines.begin(),lines.end(),[&](const auto& l){return l.id==winner;});
        if(found==lines.end())throw std::runtime_error("x3_winner_missing_current_line");
        std::optional<Line> prior;Point pp;std::int64_t time=0;
        if(s.accepted.contains(id)&&s.accepted[id].l.id==cid){const auto& a=s.accepted[id];prior=a.l;pp=a.p;time=a.ns;}
        guard=continuation(prior,pp,time,*found,visible,now);guard["grade"]="source_predicates_reconstructed_from_last_exported_accepted_sample_strong_inference";
        guard["replacement_observations"]=nullptr;guard["replacement_first_ns"]=nullptr;guard["current_contact_support_geometry"]=std::abs(signed_distance(p,*found))<=40&&(t.at("kind")=="drag"||(t.at("kind")=="hold"&&t.at("held_body_evidence")==true&&t.at("rails_geometry")==true));
    }
    json current={{"target",t},{"candidate",candidate?*candidate:json(nullptr)},{"identity_assignment",ct?*ct:json(nullptr)},{"physical_continuity","proposed_runtime_ID_not_gold"}};
    std::size_t scored=0;for(const auto& f:measurements)if(!f.at("score").is_null())++scored;
    json result={{"note_id",id},{"current",current},{"confirmed",confirmation},{"selection",selection?*selection:json(nullptr)},{"x2_choice",choice?*choice:json(nullptr)},{"scored_candidate_count",scored},{"competitive_margin",selection&&scored>=2?json(selection->at("second_score").get<double>()-selection->at("best_score").get<double>()):json(nullptr)},{"line_features",measurements},{"continuation",guard},{"first_observer_block",t.at("samples")==0?t.at("reason"):json(nullptr)},{"owner",owner_status(owner(row,id),prefix(row,id))}};
    json resets=json::array();for(const auto& q:row.at("diagnostics"))if(q.value("event","")=="history_reset"&&q.at("note_id")==id)resets.push_back(q);result["history_resets"]=resets;
    // Report-only confirmation provenance. Export lacks explicit establishment event.
    if(cid){s.confirm[id]=confirmation;}else s.confirm.erase(id);
    const auto selected=std::find_if(lines.begin(),lines.end(),[&](const auto& l){return l.id==t.at("line_id").get<std::uint64_t>();});
    // C36h has no projection field. A selected ID measured in this exact frame
    // establishes direct geometry; missing optional schema is never imputed.
    if(selected!=lines.end()&&selected->valid&&selected->observed==now&&t.at("samples").get<int>()>0&&ct&&!ct->at("identity_ambiguous").get<bool>()) {
        s.accepted[id]={*selected,p,now,row.at("ordinal")};
        if(selection&&selection->contains("confirmed_line_id")&&!cid&&t.at("samples").get<int>()>=3&&t.at("history_span_ns").get<std::int64_t>()>=30'000'000) {
            s.confirm[id]={{"line_id",selected->id},{"established_ns",now},{"established_ordinal",row.at("ordinal")},{"source","proposed_from_original_3_samples_30ms_predicate_not_explicit_trace_event"}};
            result["confirmation_predicate_reached"]=s.confirm[id];
        }
    }
    return result;
}
}

int report_main(int argc,char** argv){try {
    if(argc!=3)throw std::runtime_error("x3_report query_manifest new_report");Lease lease;
    const fs::path output=argv[2];if(fs::exists(output))throw std::runtime_error("x3_output_exists");
    const auto m=load(argv[1]);if(!m.at("schema").is_number_integer()||m.at("schema")!=1||m.at("experiment")!="line_role_causal_features_x3"||!m.at("batch_limit_bytes").is_number_integer()||m.at("batch_limit_bytes")!=batch_cap)throw std::runtime_error("x3_manifest_contract");
    const fs::path campaign=m.at("campaign_root").get<std::string>();if(fs::canonical(m.at("batch_root").get<std::string>())!=fs::canonical(campaign/"line-role-x3"))throw std::runtime_error("x3_batch_binding");
    const auto x2sha=pas::sha256_file(m.at("x2_manifest_path").get<std::string>()),acceptsha=pas::sha256_file(m.at("acceptance_path").get<std::string>());
    if(x2sha!=m.at("x2_manifest_sha256").get<std::string>()||x2sha!="e1704c0c6ab0cc6bb9def4aad31a8e49416556947ba319d9487dbbd451577cbc"||acceptsha!=m.at("acceptance_sha256").get<std::string>()||acceptsha!="5436b5f6d981df40554dcce9bbbcb41e5aa15b7a2650ffb5a075883de8349dc8")throw std::runtime_error("x3_parent_SHA");
    const auto x2m=load(m.at("x2_manifest_path").get<std::string>());const fs::path x2root=x2m.at("batch_root").get<std::string>();
    for(const auto key:{"campaign_root","prior_research_root"})if(fs::canonical(m.at(key).get<std::string>())!=fs::canonical(x2m.at(key).get<std::string>()))throw std::runtime_error("x3_campaign_binding");
    const auto acceptance=load(m.at("acceptance_path").get<std::string>());const fs::path ledgerpath=x2root/"capacity-ledger.json";
    if(pas::sha256_file(ledgerpath)!=acceptance.at("source_checks").at("frozen_capacity_ledger_sha256").get<std::string>())throw std::runtime_error("x3_parent_ledger_SHA");const auto ledger=load(ledgerpath);
    if(m.at("runs").size()!=3)throw std::runtime_error("x3_roles");
    std::array<json,3> summaries;std::array<fs::path,3> roots;std::array<const char*,3> names{{"c36h-reference-on-1","main50-control-on-1","main50-no-override-on-1"}};
    for(std::size_t i=0;i<3;++i){const auto& input=m.at("runs").at(i);if(input.at("role")!=x2::roles[i])throw std::runtime_error("x3_role_order");roots[i]=input.at("root").get<std::string>();if(fs::canonical(roots[i])!=fs::canonical(x2root/names[i]))throw std::runtime_error("x3_run_path");
        std::set<std::string> files;for(const auto& f:input.at("files")){const auto name=f.at("name").get<std::string>();if(name!="summary.json"&&name!="trace.jsonl"&&name!="first-intervention.jsonl"&&name!="events.jsonl")throw std::runtime_error("x3_input_file");if(!files.insert(name).second)throw std::runtime_error("x3_duplicate_file");const auto hash=pas::sha256_file(roots[i]/name);if(hash!=f.at("sha256").get<std::string>())throw std::runtime_error("x3_input_SHA");
            const auto relative_name=std::string(names[i])+"/"+name;bool bound=false;for(const auto& a:ledger.at("artifacts"))if(a.at("path")==relative_name&&a.at("sha256")==hash)bound=true;if(!bound)throw std::runtime_error("x3_frozen_artifact_binding");}
        if(files.size()!=4)throw std::runtime_error("x3_missing_file_binding");summaries[i]=load(roots[i]/"summary.json");}
    x2::validate_runs(x2m,x2sha,summaries);
    const fs::path recording=fs::path(x2m.at("session_root").get<std::string>())/"full-recording",indexpath=recording/"index.jsonl";
    if(pas::sha256_file(indexpath)!=x2m.at("index_sha256").get<std::string>())throw std::runtime_error("x3_index_SHA");
    std::map<std::size_t,json> index;std::size_t count=0;std::int64_t last=0,origin=0;
    each_row(indexpath,36000,[&](const json& e){const auto n=e.at("ordinal").get<std::size_t>();const auto t=e.at("capture_complete_ns").get<std::int64_t>();if(n!=count++||t<=last)throw std::runtime_error("x3_index_order");if(n==0)origin=t;last=t;bool needed=n>=1533&&n<=1537;for(const auto& w:x2m.at("windows"))needed|=n>=w.at("first").get<std::size_t>()&&n<=w.at("last").get<std::size_t>();if(needed)index[n]=e;});
    json report={{"schema",1},{"experiment","line_role_causal_features_x3"},{"query_manifest_sha256",pas::sha256_file(argv[1])},{"analysis_binary_sha256",pas::sha256_file(argv[0])},{"x2_manifest_sha256",x2sha},{"parent_acceptance_sha256",acceptsha},{"input_roles",m.at("runs")},{"role_bindings",x2m.at("roles")},{"denominators",json::array()},{"frames",json::array()},{"shadow_rule",nullptr},{"new_full_replays",0},{"physical_gold",false},{"gameplay_outcome","unknown_fixed_C36g_pixels"},{"limits",{{"history_frames",6},{"history_ns",90000000},{"line_per_frame",16},{"note_state",128},{"report_bytes",report_cap},{"batch_bytes",batch_cap}}},{"validity_contract","past/current geometry only; secant distance rate includes observed line rotation; motion_valid=false velocities null; runtime-ID continuity proposed; no future rows; no role verdict or action eligibility"}};
    std::set<std::size_t> verified_pngs;std::size_t accumulated_frame_bytes=0;
    for(std::size_t role=0;role<3;++role){auto trace=bounded_rows(roots[role]/"trace.jsonl",109,16*1024*1024);if(trace.size()!=109)throw std::runtime_error("x3_trace_denominator");auto first=bounded_rows(roots[role]/"first-intervention.jsonl",5,2*1024*1024);if(role&&first.size()!=5)throw std::runtime_error("x3_first_context_denominator");if(!role&&!first.empty())throw std::runtime_error("x3_reference_context_unexpected");trace.insert(trace.end(),first.begin(),first.end());std::sort(trace.begin(),trace.end(),[](const auto& a,const auto& b){return a.at("ordinal")<b.at("ordinal");});
        State state;std::size_t priorordinal=0,objects=0,validpairs=0,invalidpairs=0;std::int64_t priornow=0;std::set<std::uint64_t> subjectids;
        for(const auto& row:trace){const auto n=row.at("ordinal").get<std::size_t>();if(!index.contains(n)||(!row.at("consumed").get<bool>())||row.at("scene").is_null()||row.at("candidate_bank").is_null()||(priornow&&n<=priorordinal))throw std::runtime_error("x3_trace_shape_order");const auto& e=index.at(n);const auto now=row.at("scene").at("capture_complete_ns").get<std::int64_t>();if(row.at("source_frame")!=e.at("source_frame")||row.at("png_sha256")!=e.at("png_sha256")||now!=e.at("capture_complete_ns").get<std::int64_t>()-origin+1'000'000'000||row.at("candidate_bank").at("context").at("capture_ns")!=now)throw std::runtime_error("x3_frame_join");
            if(verified_pngs.insert(n).second){const auto png=recording/relative(e.at("path"));if(pas::sha256_file(png)!=e.at("png_sha256").get<std::string>())throw std::runtime_error("x3_png_SHA");if(n>=6211&&n<=6214)report["RGB_upper_band_probes"][std::to_string(n)]=upper_band_probe(png);}
            if(priornow&&now-priornow>90'000'000)state=State{};priornow=now;priorordinal=n;
            std::erase_if(state.accepted,[&](const auto& a){return now-a.second.ns>90'000'000;});std::erase_if(state.confirm,[&](const auto& a){return !state.accepted.contains(a.first);});
            std::vector<Line> lines;for(const auto& l:row.at("candidate_bank").at("lines"))lines.push_back(line(l));if(lines.size()>16||row.at("scene").at("targets").size()>128||row.at("candidate_bank").at("candidates").size()>128)throw std::runtime_error("x3_scene_capacity");
            const auto c=case_id(n,x2m);json outputobjects=json::array();
            for(const auto& t:row.at("scene").at("targets")){auto f=features(row,t,state,lines);if(subject(c,t)){subjectids.insert(t.at("note_id").get<std::uint64_t>());++objects;for(const auto& l:f.at("line_features"))if(l.at("pair").at("valid")==true)++validpairs;else ++invalidpairs;outputobjects.push_back(std::move(f));}}
            json absent=json::array();if(c=="B4986"&&!row.at("owner").is_null())for(const auto& o:row.at("owner").at("identities"))if(o.at("kind")=="hold"){
                const bool visible=std::any_of(row.at("scene").at("targets").begin(),row.at("scene").at("targets").end(),[&](const auto& t){return t.at("note_id")==o.at("note_id");});
                if(!visible){subjectids.insert(o.at("note_id").get<std::uint64_t>());absent.push_back({{"note_id",o.at("note_id")},{"current_object_visible",false},{"owner",owner_status(o,prefix(row,o.at("note_id").get<std::uint64_t>()))}});}}
            json frame_output={{"role",x2::roles[role]},{"case",c},{"ordinal",n},{"source_frame",row.at("source_frame")},{"capture_complete_ns",e.at("capture_complete_ns")},{"fake_capture_ns",now},{"pixels_ready_ns",e.at("pixels_ready_ns")},{"png_path",(recording/relative(e.at("path"))).string()},{"png_sha256",e.at("png_sha256")},{"objects",outputobjects},{"absent_owner_identities",absent},{"contacts",row.at("contacts")},{"lifecycle",row.at("lifecycle")},{"scheduler",row.value("scheduler",json(nullptr))}};
            accumulated_frame_bytes+=frame_output.dump().size();if(accumulated_frame_bytes>report_cap)throw std::runtime_error("x3_report_capacity");report["frames"].push_back(std::move(frame_output));
            state.poses.push_back(lines);while(state.poses.size()>6||(!state.poses.empty()&&!state.poses.front().empty()&&now-state.poses.front()[0].observed>90'000'000))state.poses.pop_front();if(state.accepted.size()>128||state.confirm.size()>128)throw std::runtime_error("x3_note_history_capacity");
        }
        json actions=json::array();std::size_t event_count=0;each_row(roots[role]/"events.jsonl",100000,[&](const json& a){++event_count;if(a.at("event")=="fake_receipt"&&!a.at("note_id").is_null()&&subjectids.contains(a.at("note_id").get<std::uint64_t>())){if(actions.size()>=4096)throw std::runtime_error("x3_action_capacity");actions.push_back(a);}});
        report["denominators"].push_back({{"role",x2::roles[role]},{"trace_frames",trace.size()},{"subject_occurrences",objects},{"measured_pairs_valid",validpairs},{"measured_pairs_unknown",invalidpairs},{"full_events_read",event_count},{"subject_receipts",actions.size()},{"input_truncated",false},{"confirmed_establishment_explicit_export","missing; proposed predicate or first-seen bracket only"}});report["actions"][x2::roles[role]]=actions;
    }
    report["verified_unique_pngs"]=verified_pngs.size();save_report(output,m,report);std::cout<<"X3 read-only report: "<<report.at("frames").size()<<" role frames, "<<verified_pngs.size()<<" PNG references, no shadow/action changes\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
}
