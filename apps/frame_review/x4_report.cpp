#include "x4_input.hpp"
#include <iostream>
#include <fstream>
#include <set>
#define NOMINMAX
#include <windows.h>
using namespace pas;
using namespace pas::review;
namespace {
std::uint64_t bytes(const fs::path& root){std::uint64_t n=0;if(fs::exists(root))for(const auto& f:fs::recursive_directory_iterator(root))if(f.is_regular_file())n+=f.file_size();return n;}
void output(const fs::path& path,const json& data,const fs::path& campaign) {
    const auto batch=fs::absolute(campaign/"preconfirmation-role-x4").lexically_normal();
    const auto p=fs::absolute(path).lexically_normal();
    if(p.parent_path()!=batch)throw std::runtime_error("x4_output_direct_batch_required");
    for(auto parent=p.parent_path();!parent.empty();parent=parent.parent_path()) {
        const auto a=GetFileAttributesW(parent.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("x4_reparse_path");
        if(parent==parent.parent_path())break;
    }
    HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\PAS_X1ContactReplayBudget");
    if(!mutex||WaitForSingleObject(mutex,0)!=WAIT_OBJECT_0){if(mutex)CloseHandle(mutex);throw std::runtime_error("batch_writer_busy");}
    try {
        const auto s=data.dump(2)+"\n";
        if(s.size()>2*1024*1024||bytes(batch)+s.size()>25165824||bytes(campaign)+bytes(campaign.parent_path().parent_path()/"research-next-20261001")+s.size()>8589934592ULL)throw std::runtime_error("x4_output_quota");
        HANDLE file=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("x4_output_exists_or_open");
        DWORD written=0;const auto ok=WriteFile(file,s.data(),static_cast<DWORD>(s.size()),&written,nullptr);CloseHandle(file);
        if(!ok||written!=s.size())throw std::runtime_error("x4_output_write");
    }catch(...){ReleaseMutex(mutex);CloseHandle(mutex);throw;}
    ReleaseMutex(mutex);CloseHandle(mutex);
}
json core_from_candidate(const json& c){const auto& n=c.at("note");return {{"x",n.at("center")[0]},{"y",n.at("center")[1]},{"ux",n.at("tangent")[0]},{"uy",n.at("tangent")[1]},{"width",n.at("width")},{"height",n.at("height")},{"kind",n.at("kind")}};}
json line_from_scene(const json& l){json result;for(const auto key:{"x","y","ux","uy","length"})result[key]=l.at(key);return result;}
int packet(const fs::path& manifest,const fs::path& destination) {
    if(sha256_file(manifest)!="e1704c0c6ab0cc6bb9def4aad31a8e49416556947ba319d9487dbbd451577cbc")throw std::runtime_error("x4_packet_parent_SHA");
    const auto m=load(manifest);x2::validate_manifest(m);
    const fs::path root=fs::path(m.at("batch_root").get<std::string>())/"main50-control-on-1";
    const auto summary=load(root/"summary.json");x2::validate_binding(m,summary,sha256_file(manifest),"main50_control");
    const auto index=rows(fs::path(m.at("session_root").get<std::string>())/"full-recording/index.jsonl",36000);
    json data={{"schema",1},{"grade","proposed"},{"human_gold",false},{"identity_gold",false},
        {"policy","same-frame D Drag 250<y<325; unique current vertical candidate passing original basic geometry; no ID binding"},
        {"parent_manifest_sha256",sha256_file(manifest)},{"control_trace_sha256",sha256_file(root/"trace.jsonl")},{"packets",json::array()}};
    for(const auto& r:rows(root/"trace.jsonl",120)) {
        const auto n=r.at("ordinal").get<std::size_t>();if(n<6158||n>6220)continue;
        const auto& e=index.at(n);const auto png=fs::path(m.at("session_root").get<std::string>())/"full-recording"/relative(e.at("path"));
        if(sha256_file(png)!=e.at("png_sha256").get<std::string>()||e.at("source_frame")!=r.at("source_frame")||e.at("png_sha256")!=r.at("png_sha256"))throw std::runtime_error("x4_packet_RGB_binding");
        json cores=json::array(),lines=json::array();
        for(const auto& c:r.at("candidate_bank").at("candidates")) {
            const auto core=core_from_candidate(c);
            if(core.at("kind")=="drag"&&core.at("y")>250&&core.at("y")<325)cores.push_back(core);
        }
        const json core=cores.size()==1?cores[0]:json(nullptr);
        if(!core.is_null())for(const auto& l:r.at("scene").at("lines")) {
            const double along=std::abs((core.at("x").get<double>()-l.at("x").get<double>())*l.at("ux").get<double>()+(core.at("y").get<double>()-l.at("y").get<double>())*l.at("uy").get<double>());
            if(l.at("association_valid")==true&&l.at("observed_ns")==r.at("scene").at("capture_complete_ns")&&std::abs(l.at("uy").get<double>())>=.97&&l.at("length")>=1280*.24&&along<=l.at("length").get<double>()*.5+core.at("width").get<double>()+24)lines.push_back(line_from_scene(l));
        }
        data["packets"].push_back({{"ordinal",n},{"source_frame",e.at("source_frame")},{"png_sha256",e.at("png_sha256")},{"png_path",png.string()},
            {"core",core},{"line",lines.size()==1?lines[0]:json(nullptr)},{"current_vertical_count",lines.size()},
            {"proposal_source","same-frame original RGB and accepted main50 current bank geometry; X3 role proposal, no future qualification"}});
    }
    if(data.at("packets").size()!=63)throw std::runtime_error("x4_packet_denominator");
    output(destination,data,m.at("campaign_root").get<std::string>());return 0;
}
json action(json e) {e.erase("note_id");if(e.contains("command"))e["command"].erase("intent_id");return e;}
json pick(const json& r) {json v;for(const auto key:{"ordinal","consumed","scene","lifecycle","owner","contacts"})v[key]=r.at(key);return v;}
int compare(const fs::path& query,const fs::path& on,const fs::path& off,const fs::path& destination) {
    const auto q=load(query);
    if(!q.at("schema").is_number_integer()||q.at("schema")!=1||q.value("experiment","")!="preconfirmation_role_oracle_x4_query")throw std::runtime_error("x4_query_contract");
    const fs::path manifest=q.at("input_manifest_path").get<std::string>();
    if(sha256_file(manifest)!=q.at("input_manifest_sha256").get<std::string>())throw std::runtime_error("x4_query_input_SHA");
    for(const auto& [role,root]:std::array<std::pair<const char*,fs::path>,2>{{{"variant_on",on},{"variant_off",off}}}) {
        const auto& binding=q.at("runs").at(role);
        if(binding.value("role","")!="main50_preconfirmation_role_oracle"||binding.value("trace","")!=(std::string(role)=="variant_on"?"on":"off"))throw std::runtime_error("x4_query_role_trace");
        if(fs::absolute(root).lexically_normal()!=fs::absolute(binding.at("root").get<std::string>()).lexically_normal())throw std::runtime_error("x4_query_run_role_root");
        for(const auto file:{"summary.json","trace.jsonl","events.jsonl","state-digests.jsonl","oracle-attempts.jsonl","first-intervention.jsonl","G1533-1537.jsonl"}) {
            if(!binding.at("files").contains(file))throw std::runtime_error("x4_query_missing_binding");
            if(sha256_file(root/file)!=binding.at("files").at(file).get<std::string>())throw std::runtime_error("x4_query_artifact_SHA");
        }
    }
    const auto m=load(manifest);x4::validate_parent(m);const auto sha=sha256_file(manifest);
    const auto a=load(on/"summary.json"),b=load(off/"summary.json");
    for(const auto& s:{a,b}) {
        x4::validate_variant_binding(m,s,sha);
        if(s.at("success")!=true||s.at("input_frames")!=7722||s.at("verified_pngs")!=7722||s.at("consumed_frames")!=7715||s.at("contacts_at_exit")!=0||s.at("oracle_sha256")!=m.at("oracle").at("sha256"))throw std::runtime_error("x4_failed_or_incomplete_variant");
        for(const auto& [key,val]:std::array<std::pair<const char*,const char*>,4>{{{"cadence","owner"},{"tie","frame-first"},{"recognition","zero_fake_time"},{"receipt_policy","success_zero_duration_five_contacts"}}})if(s.at(key)!=val)throw std::runtime_error("x4_variant_policy");
    }
    if(a.at("trace")!=true||b.at("trace")!=false)throw std::runtime_error("x4_trace_policy");
    json determinism;for(const auto file:{"state-digests.jsonl","events.jsonl","oracle-attempts.jsonl"}) {
        determinism[file]={{"on_sha256",sha256_file(on/file)},{"off_sha256",sha256_file(off/file)}};
        if(determinism[file]["on_sha256"]!=determinism[file]["off_sha256"])throw std::runtime_error("x4_nondeterministic");
    }
    if(a.at("semantic_sha256")!=b.at("semantic_sha256"))throw std::runtime_error("x4_semantic_nondeterministic");
    const fs::path control=m.at("reused_runs").at("main50_control").at("root").get<std::string>();
    const auto cd=rows(control/"state-digests.jsonl",36000),vd=rows(on/"state-digests.jsonl",36000);
    if(cd.size()!=7722||vd.size()!=7722)throw std::runtime_error("x4_state_denominator");
    json first=nullptr;std::size_t matching=0;
    for(std::size_t i=0;i<cd.size();++i) {
        for(const auto key:{"ordinal","source_frame","png_sha256"})if(cd[i].at(key)!=vd[i].at(key))throw std::runtime_error("x4_digest_input");
        if(cd[i].at("semantic_sha256")==vd[i].at("semantic_sha256"))++matching;
        else if(first.is_null())first=cd[i].at("ordinal");
    }
    const auto ce=rows(control/"events.jsonl",100000),ve=rows(on/"events.jsonl",100000);
    json ca=json::array(),va=json::array();
    for(const auto& e:ce)if(e.value("event","")=="fake_receipt"||e.value("event","")=="fake_release")ca.push_back(action(e));
    for(const auto& e:ve)if(e.value("event","")=="fake_receipt"||e.value("event","")=="fake_release")va.push_back(action(e));
    json first_action=nullptr;for(std::size_t i=0;i<std::max(ca.size(),va.size());++i)if(i>=ca.size()||i>=va.size()||ca[i]!=va[i]){first_action={{"ordered_index",i},{"control",i<ca.size()?ca[i]:json(nullptr)},{"variant",i<va.size()?va[i]:json(nullptr)}};break;}
    const auto ct=rows(control/"trace.jsonl",120),vt=rows(on/"trace.jsonl",120);
    if(ct.size()!=109||vt.size()!=109)throw std::runtime_error("x4_window_denominator");
    json controls=json::array(),d=json::array();std::set<std::uint64_t> subjects;
    for(const auto& w:m.at("windows")) {
        std::size_t total=0,equal=0;json first_window=nullptr;
        for(std::size_t i=0;i<ct.size();++i)if(ct[i].at("ordinal")>=w.at("first")&&ct[i].at("ordinal")<=w.at("last")) {
            ++total;if(pick(ct[i])==pick(vt[i])&&ct[i].at("scheduler")==vt[i].at("scheduler")&&ct[i].at("prefix_identity_summary")==vt[i].at("prefix_identity_summary"))++equal;else if(first_window.is_null())first_window=ct[i].at("ordinal");
            if(w.at("id")=="D6214") {
                json targets=json::array();for(const auto& t:vt[i].at("scene").at("targets"))if(t.at("kind")=="drag"&&t.at("y")>250&&t.at("y")<325){targets.push_back(t);subjects.insert(t.at("note_id").get<std::uint64_t>());}
                json owner=json::array();if(!vt[i].at("owner").is_null())for(const auto& id:vt[i].at("owner").at("identities"))if(subjects.contains(id.at("note_id").get<std::uint64_t>()))owner.push_back(id);
                json attempts=json::array();for(const auto& row:rows(on/"oracle-attempts.jsonl",128))if(row.at("ordinal")==vt[i].at("ordinal"))attempts.push_back(row);
                d.push_back({{"ordinal",vt[i].at("ordinal")},{"source_frame",vt[i].at("source_frame")},{"png_sha256",vt[i].at("png_sha256")},
                    {"current_targets",targets},{"owner",owner},{"contacts",vt[i].at("contacts")},{"scheduler",vt[i].at("scheduler")},{"attempts",attempts}});
            }
        }
        controls.push_back({{"case",w.at("id")},{"frames",total},{"full_exported_state_equal",equal},{"first_difference",first_window},{"scope","scene/lifecycle/owner/contact + scheduler and prefix summary; IDs retained"}});
    }
    json receipts=json::array();std::map<std::uint64_t,std::size_t> downs;std::size_t all_down=0,unknown_down=0,duplicate=0;
    for(const auto& e:ve)if(e.value("event","")=="fake_receipt") {
        if(e.at("command").at("phase")==0) {++all_down;if(e.at("success")!=true)++unknown_down;if(!e.at("note_id").is_null()&&++downs[e.at("note_id").get<std::uint64_t>()]>1)++duplicate;}
        if(!e.at("note_id").is_null()&&subjects.contains(e.at("note_id").get<std::uint64_t>()))receipts.push_back(e);
    }
    const auto cg=rows(control/"first-intervention.jsonl",5),vg=rows(on/"G1533-1537.jsonl",5);std::size_t g_equal=0;
    if(cg.size()!=5||vg.size()!=5)throw std::runtime_error("x4_G_context_denominator");for(std::size_t i=0;i<5;++i)if(pick(cg[i])==pick(vg[i]))++g_equal;
    json report={{"schema",1},{"experiment","preconfirmation_role_oracle_x4"},{"query_manifest_sha256",sha256_file(query)},{"manifest_sha256",sha},{"old_run_manifest_bridge",m.at("parent")},
        {"reused_roles",m.at("reused_runs")},{"variant_summary_on",a},{"variant_summary_off",b},{"determinism",determinism},
        {"first_raw_state_divergence",first},{"matching_full_prefix_frame_digests",matching},{"frame_denominator",7722},
        {"first_action_divergence_ignoring_note_intent_ids",first_action},{"control_command_release_count",ca.size()},{"variant_command_release_count",va.size()},
        {"controls",controls},{"G_exported_state_equal",g_equal},{"D",d},{"D_proposed_local_note_ids",subjects},{"D_receipts",receipts},
        {"safety",{{"all_downs",all_down},{"unknown_downs",unknown_down},{"duplicate_local_note_downs",duplicate},{"EOF_contacts",a.at("contacts_at_exit")},{"unknown_retry_live_denominator",0},{"physical_duplicate_or_role_gold","unknown"}}},
        {"grading",{{"geometry_correspondence","proposed"},{"gameplay_hit_rate","unknown_fixed_C36g_pixels"},{"source_age","unknown"}}}};
    output(destination,report,m.at("campaign_root").get<std::string>());return 0;
}
}
int main(int argc,char** argv){try {
    if(argc==4&&std::string(argv[1])=="packet")return packet(argv[2],argv[3]);
    if(argc==6&&std::string(argv[1])=="compare")return compare(argv[2],argv[3],argv[4],argv[5]);
    throw std::runtime_error("x4_tool packet old-manifest new-packet | compare manifest on-run off-run new-report");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
