#include "contact_replay.hpp"
#include "review_io.hpp"
#include "replay_trace.hpp"
#include "replay_support.hpp"
#include "comparison_input.hpp"
#ifdef PAS_X2_OFFLINE
#include "x2_input.hpp"
#include "x2_ablation.hpp"
#endif
#ifdef PAS_X4_OFFLINE
#include "x4_input.hpp"
#endif
#include "pas/game_tracking.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

using namespace pas;
using namespace pas::review;
using namespace pas::x1;
namespace {
// A single host writer owns the shared campaign budget for a full contact run.
// Fail immediately instead of racing another process's initial byte snapshot.
struct BatchWriterLease {
    HANDLE handle=CreateMutexW(nullptr,FALSE,L"Local\\PAS_X1ContactReplayBudget");
    BatchWriterLease() {
        if(!handle)throw std::runtime_error("batch_writer_mutex");
        const auto result=WaitForSingleObject(handle,0);
        if(result!=WAIT_OBJECT_0&&result!=WAIT_ABANDONED) {
            CloseHandle(handle);handle=nullptr;throw std::runtime_error("batch_writer_busy");
        }
    }
    ~BatchWriterLease(){if(handle){ReleaseMutex(handle);CloseHandle(handle);}}
};
struct Digest {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    Digest(){if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||
        BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("SHA init");}
    ~Digest(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    void add(const json& row){auto s=row.dump()+"\n";if(BCryptHashData(hash,reinterpret_cast<PUCHAR>(s.data()),static_cast<ULONG>(s.size()),0)<0)throw std::runtime_error("SHA update");}
    std::string finish(){std::array<unsigned char,32>d{};if(BCryptFinishHash(hash,d.data(),32,0)<0)throw std::runtime_error("SHA finish");std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:d)out<<std::setw(2)<<int(b);return out.str();}
};
std::uint64_t tree_bytes(const fs::path& root) {
    std::uint64_t n=0;if(fs::exists(root))for(const auto& p:fs::recursive_directory_iterator(root))if(p.is_regular_file())n+=p.file_size();return n;
}
struct Writer {
    fs::path root;std::uint64_t initial,used=0,limit;
    Writer(fs::path r,std::uint64_t current,std::uint64_t quota):root(std::move(r)),initial(current),limit(quota){}
    void row(std::ofstream& out,const json& value) {
        const auto s=value.dump()+"\n";if(initial+used+s.size()>limit)throw std::runtime_error("output_quota");
        out<<s;if(!out)throw std::runtime_error("output_write");used+=s.size();
    }
};
struct Input {
    json manifest;fs::path session,recording;std::vector<json> index;
    std::set<std::uint64_t> consumed;std::map<std::uint64_t,json> original;
    std::map<std::uint64_t,std::vector<json>> original_events;
    std::map<std::uint64_t,std::size_t> ordinal;
    json segment_sha=json::array(),original_prefix=json::array();std::size_t journal_rows=0,preroll=0,missing_join=0,verified=0;
    bool selected(std::size_t n) const {for(const auto& w:manifest.at("windows"))if(n>=w.at("first").get<std::size_t>()&&n<=w.at("last").get<std::size_t>())return true;return false;}
    Input(const fs::path& manifest_path):manifest(load(manifest_path)),session(manifest.at("session_root").get<std::string>()),recording(session/"full-recording") {
        if(sha256_file(recording/"index.jsonl")!=manifest.at("index_sha256").get<std::string>()||
           sha256_file(manifest.at("profile").get<std::string>())!=manifest.at("profile_sha256").get<std::string>())throw std::runtime_error("input_manifest_SHA");
        const auto profile=load(manifest.at("profile").get<std::string>());
        if(profile.at("game").at("lead_ms")!=35||profile.at("game").at("uncertainty_ms")!=30||
           profile.at("touch").at("max_contacts")!=5)throw std::runtime_error("profile_policy");
        if(manifest.at("windows").size()!=5)throw std::runtime_error("five_windows_required");
        for(const auto& w:manifest.at("windows"))if(w.at("last").get<std::size_t>()<w.at("first").get<std::size_t>()||
            w.at("last").get<std::size_t>()-w.at("first").get<std::size_t>()>=120)throw std::runtime_error("window_capacity");
        index=rows(recording/"index.jsonl",36000);if(index.empty())throw std::runtime_error("empty_index");
        Nanoseconds capture=0,ready=0;std::uint64_t source=0;
        for(std::size_t i=0;i<index.size();++i){const auto& e=index[i];const auto c=e.at("capture_complete_ns").get<Nanoseconds>(),r=e.at("pixels_ready_ns").get<Nanoseconds>();const auto f=e.at("source_frame").get<std::uint64_t>();
            if(e.at("ordinal")!=i||c<=capture||r< c||r<=ready||f<=source||e.at("width")!=1280||e.at("height")!=720||e.at("source_rotation")!=1||e.at("clock_domain")!="host_qpc_ns")throw std::runtime_error("index_context_order");
            capture=c;ready=r;source=f;ordinal[f]=i;relative(e.at("path"));if(e.at("pre_roll").get<bool>())++preroll;
        }
        if(preroll>32)throw std::runtime_error("preroll_capacity");
        const auto summary=load(session/"round-1"/"summary.json");if(summary.at("event_segments").size()>32)throw std::runtime_error("segments_capacity");
        std::uint64_t latest=0;
        for(const auto& seg:summary.at("event_segments")){
            const auto path=session/"round-1"/relative(seg.at("path"));const auto hash=sha256_file(path);
            if(hash!=seg.at("sha256").get<std::string>())throw std::runtime_error("journal_SHA");segment_sha.push_back({{"path",seg.at("path")},{"sha256",hash}});
            each_row(path,1000000,[&](json j){if(++journal_rows>1000000)throw std::runtime_error("journal_capacity");
                if(j.value("event","")=="game_decision"){
                    latest=j.at("frame_sequence");if(!consumed.insert(latest).second)throw std::runtime_error("duplicate_decision_frame");
                    if(!ordinal.contains(latest))throw std::runtime_error("decision_frame_missing_from_index");
                    if(selected(ordinal.at(latest)))original[latest]=j;
                }else {
                    const auto f=j.value("source_frame",latest);
                    if(ordinal.contains(f)&&selected(ordinal.at(f))){j["join_basis"]=j.contains("source_frame")?"exact_source_frame":"journal_order_latest_decision_not_exact_frame";original_events[f].push_back(std::move(j));}
                }
            },32*1024*1024);
        }
        for(std::size_t i=0;i<index.size();++i)if(selected(i)&&!consumed.contains(index[i].at("source_frame")))++missing_join;
        // External comparison only: no field below is supplied to perception,
        // owner, scheduler, dispatch guards, or the receipt policy.
        std::set<std::uint64_t> subjects,intents;for(const auto& [_,d]:original)for(const auto& t:d.at("targets"))subjects.insert(t.at("note_id").get<std::uint64_t>());
        for(const auto& [_,events]:original_events)for(const auto& event:events)if(event.contains("note_id"))subjects.insert(event.at("note_id").get<std::uint64_t>());
        std::size_t prefix_bytes=0;
        for(const auto& seg:summary.at("event_segments"))each_row(session/"round-1"/relative(seg.at("path")),1000000,[&](json j){
            if(j.value("event","")=="game_decision")return;
            if(j.contains("note_id")&&subjects.contains(j.at("note_id").get<std::uint64_t>())&&j.contains("intent_id"))intents.insert(j.at("intent_id").get<std::uint64_t>());
            if((j.contains("note_id")&&subjects.contains(j.at("note_id").get<std::uint64_t>()))||
               (j.contains("intent_id")&&intents.contains(j.at("intent_id").get<std::uint64_t>()))){
                prefix_bytes+=j.dump().size();if(original_prefix.size()>=100000||prefix_bytes>32*1024*1024)throw std::runtime_error("original_prefix_capacity");original_prefix.push_back(std::move(j));
            }
        },32*1024*1024);
    }
    Frame pixels(std::size_t i,Nanoseconds zero,bool normalize=true) {
        const auto& e=index.at(i);const auto path=recording/relative(e.at("path"));
        if(fs::file_size(path)>4*1024*1024||sha256_file(path)!=e.at("png_sha256").get<std::string>())throw std::runtime_error("PNG_SHA_capacity");++verified;
        auto f=load_diagnostic_png(path);if(f.width!=1280||f.height!=720||f.stride!=3840||f.rgb.size()!=1280*720*3)throw std::runtime_error("decoded_geometry");
        f.sequence=e.at("source_frame");f.epoch=f.generation=f.geometry_version=1;f.source_rotation=1;
        f.capture_complete_ns=e.at("capture_complete_ns").get<Nanoseconds>()-(normalize?zero-1'000'000'000:0);
        f.pixels_ready_ns=e.at("pixels_ready_ns").get<Nanoseconds>()-(normalize?zero-1'000'000'000:0);f.published_ns=f.pixels_ready_ns;
        f.source_timestamp_us=e.at("source_timestamp_us");f.source_sequence=e.at("source_sequence");return f;
    }
};
json plan_json(const ContactPlan& p) {
    json steps=json::array();for(auto& s:p.steps)steps.push_back({{"phase",static_cast<int>(s.phase)},{"x",s.x},{"y",s.y},{"due_ns",s.due_ns}});
    return {{"event","counterfactual_plan"},{"note_id",p.note_id},{"intent_id",p.intent_id},{"revision",p.revision},
        {"prefix_offset",p.prefix_offset},{"evidence_ns",p.evidence_ns},{"valid_until_ns",p.valid_until_ns},
        {"source_frame",p.source_frame_sequence},{"basis",p.basis},{"predicted_down_ns",p.predicted_down_ns?json(*p.predicted_down_ns):json(nullptr)},{"steps",steps}};
}
int replay(int argc,char** argv) {
    if(argc<6||argc>8)throw std::runtime_error("contact manifest provenance new-output on|off [owner|all] [frame-first|due-first]");
    const bool trace=std::string(argv[5])=="on";if(!trace&&std::string(argv[5])!="off")throw std::runtime_error("trace_policy");
    const std::string cadence=argc>6?argv[6]:"owner",tie=argc>7?argv[7]:"frame-first";
    if(cadence!="owner"&&cadence!="all")throw std::runtime_error("cadence_policy");if(tie!="frame-first"&&tie!="due-first")throw std::runtime_error("tie_policy");
    BatchWriterLease writer_lease;
    Input input(argv[2]);const auto provenance=load(argv[3]);const fs::path output=argv[4],campaign=input.manifest.at("campaign_root").get<std::string>(),batch=input.manifest.at("batch_root").get<std::string>();
    bool x2_run=false;
    bool x4_run=false;
#ifdef PAS_X4_OFFLINE
    x4_run=std::string(argv[1])=="contact-x4";
    if(x4_run) {
        x4::validate_parent(input.manifest);
        const json binding={{"role",provenance.at("role")},{"variant",provenance.at("variant")},{"lineage",provenance.at("lineage")},
            {"binary_sha256",sha256_file(argv[0])},{"source_provenance_sha256",sha256_file(argv[3])},
            {"tracking_source_sha256",provenance.at("instrumented_source_sha256").at("src/game_tracking.cpp")},
            {"input_manifest_sha256",sha256_file(argv[2])}};
        x4::validate_variant_binding(input.manifest,binding,sha256_file(argv[2]));
        if(cadence!="owner"||tie!="frame-first")throw std::runtime_error("x4_policy_mismatch");
        x4::load_packets(input.manifest,input.index);
    }
#endif
#ifdef PAS_X2_OFFLINE
    x2_run=std::string(argv[1])=="contact-x2";
    if(x2_run) {
        x2::validate_manifest(input.manifest);
        const auto role=provenance.at("role").get<std::string>();
        json binding={{"role",role},{"lineage",provenance.at("lineage")},{"variant",provenance.at("variant")},
            {"binary_sha256",sha256_file(argv[0])},{"source_provenance_sha256",sha256_file(argv[3])},
            {"tracking_source_sha256",provenance.at("instrumented_source_sha256").at("src/game_tracking.cpp")},
            {"input_manifest_sha256",sha256_file(argv[2])}};
        x2::validate_binding(input.manifest,binding,sha256_file(argv[2]),role.c_str());
#ifdef PAS_X2_MAIN50
        if(provenance.at("lineage")!="main50")throw std::runtime_error("x2_build_lineage_main50");
#else
        if(role!="c36h_reference")throw std::runtime_error("x2_build_lineage_c36h");
#endif
        if(cadence!="owner"||tie!="frame-first")throw std::runtime_error("x2_policy_mismatch");
        x2::winner_override_enabled=role!="main50_no_confirmed_winner_override";
        x2::capture_enabled=true;
        x2::reset();
    }
#endif
    if(fs::exists(output))throw std::runtime_error("output_exists");
    if(fs::absolute(output).lexically_normal().generic_string().find(fs::absolute(batch).lexically_normal().generic_string()+"/")!=0)throw std::runtime_error("output_outside_batch");
    const auto campaign_before=tree_bytes(campaign)+tree_bytes(input.manifest.at("prior_research_root").get<std::string>()),batch_before=tree_bytes(batch);
    const std::uint64_t reserve=(x2_run||x4_run)?2*1024*1024:1024*1024;
    const std::uint64_t stream_limit=x4_run?24*1024*1024-reserve:x2_run?64*1024*1024-reserve:127*1024*1024;
    if(campaign_before+reserve>=8ULL*1024*1024*1024||batch_before>=stream_limit)throw std::runtime_error("campaign_quota");
    Writer writer(output,batch_before,std::min<std::uint64_t>(stream_limit,batch_before+8ULL*1024*1024*1024-campaign_before-reserve));
    if(x4_run)writer.limit=std::min<std::uint64_t>(writer.limit,batch_before+(trace?12ULL:6ULL)*1024*1024);
    fs::create_directories(output);std::ofstream frames(output/"trace.jsonl"),events(output/"events.jsonl"),original(output/"original-recorded.jsonl"),prefix(output/"original-prefix-events.jsonl");
#ifdef PAS_X2_OFFLINE
    std::ofstream state_digests,mechanism,first_intervention;
    std::deque<json> x2_recent;std::size_t x2_extra_frames=0,x2_mechanism_rows=0;
    std::optional<std::size_t> x2_first_different_ordinal;
    json x2_first_eligible_frame=nullptr,x2_first_different_frame=nullptr;
    if(x2_run) {state_digests.open(output/"state-digests.jsonl");mechanism.open(output/"mechanism.jsonl");first_intervention.open(output/"first-intervention.jsonl");}
#endif
#ifdef PAS_X4_OFFLINE
    std::ofstream x4_digests,x4_mechanism,x4_context,x4_g;
    std::deque<json> x4_recent;std::optional<std::size_t> x4_first;
    if(x4_run){x4_digests.open(output/"state-digests.jsonl");x4_mechanism.open(output/"oracle-attempts.jsonl");x4_context.open(output/"first-intervention.jsonl");x4_g.open(output/"G1533-1537.jsonl");}
#endif
    for(auto& row:input.original_prefix)writer.row(prefix,row);prefix.close();
    Digest digest;FakeClock clock;ReplayTouch touch(clock);SessionPerception perception(clock);SessionGameOwner owner(clock,touch,5,{15,35'000'000,30'000'000});
    std::size_t frame_n=0,skipped=0,window_n=0,event_n=0;std::uint64_t current_ordinal=0,active_round=0;bool live_gate=false,result=false;Nanoseconds latest_capture=0;
    std::map<std::uint64_t,std::uint64_t> intent_note;std::map<std::uint64_t,json> ledger;
    std::map<std::string,std::size_t> counts;std::vector<double> costs;std::size_t peak_ledger=0;json final_release=nullptr;
    auto event=[&](json j){if(++event_n>100000)throw std::runtime_error("event_capacity");j["ordinal_at_delivery"]=current_ordinal;j["fake_now_ns"]=clock.now_ns();j["provenance"]="counterfactual";
        const auto name=j.value("event","");++counts[name];
        if(name=="fake_receipt"){
            auto& c=j.at("command");const auto intent=c.at("intent_id").get<std::uint64_t>();if(intent_note.contains(intent)){const auto note=intent_note.at(intent);j["note_id"]=note;
                if(ledger.contains(note)){ledger[note]["last_receipt"]=j;if(c.at("phase")==0&&j.at("success").get<bool>())ledger[note]["successful_down"]=j;}
            }else j["note_id"]=nullptr;
        }
        digest.add(j);writer.row(events,j);
    };touch.sink=event;
    auto drain=[&]{if(!owner.owner())return;for(const auto& p:owner.owner()->take_accepted_plans()) {intent_note[p.intent_id]=p.note_id;event(plan_json(p));}
        for(auto j:owner.owner()->take_plan_cancellations())event(std::move(j));for(auto j:owner.owner()->take_coverage_updates())event(std::move(j));
        for(auto n:owner.scheduler()->take_notices())event({{"event","scheduler_notice"},{"intent_id",n.intent_id},{"reason",n.reason},{"monotonic_ns",n.monotonic_ns}});
        if(!owner.scheduler()->fault().empty())throw std::runtime_error("scheduler_fault:"+owner.scheduler()->fault());
    };
    const auto zero=input.index.front().at("capture_complete_ns").get<Nanoseconds>();
    std::string failure;
    try {
        for(std::size_t i=0;i<input.index.size();++i){current_ordinal=i;const auto& e=input.index[i];const auto frame=e.at("source_frame").get<std::uint64_t>();const auto capture=e.at("capture_complete_ns").get<Nanoseconds>()-zero+1'000'000'000,ready=e.at("pixels_ready_ns").get<Nanoseconds>()-zero+1'000'000'000;
            x1_trace_enabled=trace&&(input.selected(i)||x2_run);
#ifdef PAS_X4_OFFLINE
            if(x4_run) {
                x1_trace_enabled=trace&&(input.selected(i)||(i>=6158&&i<=6162)||(i>=1533&&i<=1537));
                x4::begin_frame(i,frame,e.at("png_sha256").get<std::string>());
            }
#endif
            // Capture envelope arrival precedes pixel delivery; metadata only, no observer call.
            advance_before_frame(owner,clock,capture,tie=="due-first",drain);latest_capture=capture;
            advance_before_frame(owner,clock,ready,tie=="due-first",drain);
            auto pixels=input.pixels(i,zero);json scene=nullptr,state=nullptr,candidates=nullptr;
            const bool consume=cadence=="all"||input.consumed.contains(frame)||e.at("pre_roll").get<bool>();
            if(consume){HostClock meter;const auto start=meter.now_ns();const auto packet=perception.process(pixels);++frame_n;
                live_gate=packet.allow_down;
                if(packet.status.state==PlaySessionState::fault)throw std::runtime_error("lifecycle_fault:"+packet.status.reason);
                if(!active_round&&packet.status.active){active_round=packet.status.round;owner.start(active_round);owner.scheduler()->set_dispatch_guard([&]{return live_gate&&active_round&&latest_capture&&clock.now_ns()-latest_capture<100'000'000;});}
                scene=decision_json(packet.scene);
                state={{"state",name(packet.status.state)},{"round",packet.status.round},{"active",packet.status.active},{"new_round",packet.status.new_round},{"ended",packet.status.ended},{"reason",packet.status.reason},{"allow_down",packet.allow_down},{"hud",packet.evidence.hud},{"result_labels",packet.evidence.result_labels}};
                if(owner.owner()){
                    const auto prior_state=owner.owner()->replay_state();
                    std::set<std::uint64_t> retained;for(auto& id:prior_state.at("identities"))retained.insert(id.at("note_id").get<std::uint64_t>());
                    for(auto& t:packet.scene.targets)retained.insert(t.note_id);
                    std::erase_if(ledger,[&](const auto& entry){return !retained.contains(entry.first);});
                    for(auto& t:packet.scene.targets)if(!ledger.contains(t.note_id)){
                        if(ledger.size()>=128)throw std::runtime_error("active_identity_summary_capacity");
                        ledger[t.note_id]={{"birth_or_first_available_ordinal",i},{"birth_revision",t.revision},{"x",t.note.center.x},{"y",t.note.center.y},{"successful_down",nullptr},{"last_receipt",nullptr}};
                    }
                    if(packet.status.ended){result=packet.status.state==PlaySessionState::result;drain();final_release=release_json(owner.finish());active_round=0;}
                    else {owner.accept(packet.scene,packet.allow_down);drain();owner.poll();drain();}
                }
                peak_ledger=std::max(peak_ledger,ledger.size());
                if(intent_note.size()>128){std::set<std::uint64_t> kept;const auto saved=owner.owner()?owner.owner()->replay_state():json{{"identities",json::array()}};for(auto& id:saved.at("identities"))kept.insert(id.at("intent_id").get<std::uint64_t>());std::erase_if(intent_note,[&](auto& p){return !kept.contains(p.first);});if(intent_note.size()>128)throw std::runtime_error("intent_summary_capacity");}
                costs.push_back((meter.now_ns()-start)/1e6);
                if((input.selected(i)||x2_run)&&trace)candidates=candidate_batch_json(perception.replay_candidates());
#ifdef PAS_X4_OFFLINE
                if(x4_run&&x1_trace_enabled)candidates=candidate_batch_json(perception.replay_candidates());
#endif
            }else {++skipped;owner.poll();drain();}
            const auto owner_state=owner.owner()?owner.owner()->replay_state():json(nullptr);
            json semantic={{"ordinal",i},{"consumed",consume},{"scene",scene},{"lifecycle",state},{"owner",owner_state},{"contacts",touch.contacts}};
            digest.add(semantic);
            auto diagnostics=x1_drain();
#ifdef PAS_X4_OFFLINE
            if(x4_run) {
                Digest frame_digest;frame_digest.add(semantic);
                writer.row(x4_digests,{{"ordinal",i},{"source_frame",frame},{"png_sha256",e.at("png_sha256")},{"semantic_sha256",frame_digest.finish()}});
                const auto attempts=x4::drain();bool changed=false;
                for(const auto& choice:attempts){writer.row(x4_mechanism,choice);changed=changed||choice.at("changed").get<bool>();}
                if(trace) {
                    json context=semantic;context["source_frame"]=frame;context["png_sha256"]=e.at("png_sha256");
                    context["diagnostics"]=diagnostics;context["candidate_bank"]=candidates;context["oracle_attempts"]=attempts;
                    if(changed&&!x4_first){x4_first=i;for(const auto& prior:x4_recent)writer.row(x4_context,prior);}
                    if(x4_first&&i<=*x4_first+2)writer.row(x4_context,context);
                    if(i>=1533&&i<=1537)writer.row(x4_g,context);
                    if(i>=6158&&i<=6220) {
                        if(x4_recent.size()==2)x4_recent.pop_front();x4_recent.push_back(std::move(context));
                        std::size_t bytes=0;for(const auto& row:x4_recent)bytes+=row.dump().size();
                        if(bytes>2*1024*1024)throw std::runtime_error("x4_context_capacity");
                    }
                }
            }
#endif
#ifdef PAS_X2_OFFLINE
            if(x2_run) {
                Digest frame_digest;frame_digest.add(semantic);
                writer.row(state_digests,{{"ordinal",i},{"source_frame",frame},{"png_sha256",e.at("png_sha256")},{"semantic_sha256",frame_digest.finish()}});
                auto choices=x2::drain();bool changed=false;
                for(auto& choice:choices)if(choice.at("preserve_eligible").get<bool>()) {
                    choice["ordinal"]=i;
                    if(x2_first_eligible_frame.is_null())x2_first_eligible_frame=choice;
                    if(choice.at("pre_override_winner")!=choice.at("confirmed_line_id")) {
                        changed=true;
                        if(x2_first_different_frame.is_null())x2_first_different_frame=choice;
                        if(++x2_mechanism_rows>100000)throw std::runtime_error("x2_mechanism_rows_capacity");
                        writer.row(mechanism,choice);
                    }
                }
                if(trace) {
                    json context=semantic;context["source_frame"]=frame;context["png_sha256"]=e.at("png_sha256");
                    context["diagnostics"]=diagnostics;context["candidate_bank"]=candidates;
                    if(changed&&!x2_first_different_ordinal) {
                        x2_first_different_ordinal=i;
                        for(const auto& prior:x2_recent){writer.row(first_intervention,prior);++x2_extra_frames;}
                    }
                    if(x2_first_different_ordinal&&i<=*x2_first_different_ordinal+2) {writer.row(first_intervention,context);++x2_extra_frames;}
                    if(x2_recent.size()==2)x2_recent.pop_front();x2_recent.push_back(std::move(context));
                    std::size_t ring_bytes=0;for(const auto& r:x2_recent)ring_bytes+=r.dump().size();
                    if(ring_bytes>16*1024*1024)throw std::runtime_error("x2_context_ring_capacity");
                }
            }
#endif
            if(input.selected(i)){
                ++window_n;auto trace_row=semantic;trace_row["source_frame"]=frame;trace_row["png_sha256"]=e.at("png_sha256");trace_row["source_epoch_assumed"]=1;trace_row["candidate_bank"]=candidates;
                trace_row["diagnostics"]=std::move(diagnostics);trace_row["prefix_identity_summary"]=ledger;trace_row["scheduler"]=owner.scheduler()?json{{"armed",owner.scheduler()->armed()},{"active",owner.scheduler()->active_count()},{"pending",owner.scheduler()->pending_count()},{"next_due_ns",owner.scheduler()->next_due_ns()?json(*owner.scheduler()->next_due_ns()):json(nullptr)}}:json(nullptr);
                writer.row(frames,trace_row);
                writer.row(original,{{"ordinal",i},{"source_frame",frame},{"png_sha256",e.at("png_sha256")},{"provenance","original_C36g_recorded"},{"decision",input.original.contains(frame)?input.original.at(frame):json(nullptr)},{"events",input.original_events[frame]}});
            }
            if(i%1000==0)std::cout<<"progress "<<i<<"/"<<input.index.size()<<" contacts="<<touch.contacts.size()<<'\n'<<std::flush;
        }
        x1_trace_enabled=false;if(owner.owner()||final_release.is_null())final_release=release_json(owner.finish());digest.add({{"event","EOF"},{"input_truncated",!result},{"pixel_result_confirmed",result},{"release",final_release}});
    }catch(const std::exception& e){failure=e.what();x1_trace_enabled=false;live_gate=false;
        if(owner.scheduler())final_release=release_json(owner.scheduler()->last_release());
        // A sink can throw after backend release, before scheduler assignment.
        // Preserve that report before finish attempts another empty release.
        if(!touch.last_release_report.requested_ids.empty()&&
           (final_release.is_null()||final_release.at("requested_ids").empty()))
            final_release=release_json(touch.last_release_report);
        try{const auto report=owner.finish();if(final_release.is_null())final_release=release_json(report);}catch(...){}
        if(!touch.contacts.empty())try{final_release=release_json(touch.release_all());}catch(...){final_release=release_json(touch.last_release_report);}
    }
    frames.close();events.close();original.close();
    json summary={{"success",failure.empty()},{"failure",failure},{"semantic_sha256",digest.finish()},{"lineage",provenance.at("lineage")},{"source_provenance_sha256",sha256_file(argv[3])},{"binary_sha256",sha256_file(argv[0])},{"input_manifest_sha256",sha256_file(argv[2])},
        {"cadence",cadence},{"tie",tie},{"trace",trace},{"recognition","zero_fake_time"},{"receipt_policy","success_zero_duration_five_contacts"},{"time_origin_ns",zero},{"replay_origin_ns",1'000'000'000},
        {"input_frames",input.index.size()},{"verified_pngs",input.verified},{"consumed_frames",frame_n},{"skipped_perception_frames",skipped},{"window_frames",window_n},{"recorded_owner_frames",input.consumed.size()},{"preroll_frames",input.preroll},{"journal_rows",input.journal_rows},{"journal_segments",input.segment_sha},{"original_decision_join_missing",input.missing_join},
        {"early_state_unknown",true},{"generation_geometry_source_epoch","assumed_constant_1"},{"source_render_age","unknown"},{"source_perception_cadence","unknown"},{"pixel_result_confirmed",result},{"input_truncated",!result},{"final_release",final_release},{"contacts_at_exit",touch.contacts.size()},
        {"event_count",event_n},{"event_counts",counts},{"receipt_release_count",touch.count},{"receipt_release_bytes",touch.bytes},{"peak_contacts",touch.peak_contacts},{"peak_active_identity_summary",peak_ledger},{"trace_peak_bytes",x1_trace_peak},{"campaign_before_bytes",campaign_before},{"batch_before_bytes",batch_before},{"output_stream_bytes",writer.used},{"host_cpu_cost_scope","perception plus accept/poll/drain with digest and event writer; excludes PNG decode/hash; no fake-clock advance"},{"host_cpu_ms",distribution(costs)},{"jitter_p95_minus_p5_ms",costs.empty()?json(nullptr):json(distribution(costs).at("p95").get<double>()-distribution(costs).at("p5").get<double>())},{"gameplay_hit_rate","not_inferred_fixed_recorded_pixels"}};
#ifdef PAS_X2_OFFLINE
    if(x2_run) {
        state_digests.close();mechanism.close();first_intervention.close();
        for(const auto key:{"role","variant"})summary[key]=provenance.at(key);
        summary["tracking_source_sha256"]=provenance.at("instrumented_source_sha256").at("src/game_tracking.cpp");
        summary["batch_limit_bytes"]=67108864;
        summary["x2_mechanism"]={{"preserve_eligible_count",x2::eligible_count},{"different_winner_count",x2::different_winner_count},
            {"first_eligible",x2_first_eligible_frame},{"first_different_winner",x2_first_different_frame},
            {"automatic_context_frames",x2_extra_frames},{"intervention",provenance.value("intervention","unmodified_c36h_reference")}};
    }
#endif
#ifdef PAS_X4_OFFLINE
    if(x4_run) {
        x4_digests.close();x4_mechanism.close();x4_context.close();x4_g.close();
        for(const auto key:{"role","variant"})summary[key]=provenance.at(key);
        summary["tracking_source_sha256"]=provenance.at("instrumented_source_sha256").at("src/game_tracking.cpp");
        summary["batch_limit_bytes"]=25165824;summary["oracle_sha256"]=input.manifest.at("oracle").at("sha256");
        summary["old_control_input_manifest_sha256"]=input.manifest.at("parent").at("manifest_sha256");
        summary["x4_mechanism"]={{"applied_count",x4::applied_count},{"different_winner_count",x4::changed_count},{"intervention",provenance.at("intervention")}};
        x4::reset();
    }
#endif
    save(output/"summary.json",summary);std::cout<<summary.dump(2)<<'\n';return failure.empty()?0:1;
}
int compatibility(int argc,char** argv) {
    if(argc!=5)throw std::runtime_error("observer-compat manifest reference-replay new-summary");Input input(argv[2]);const fs::path output=argv[4];if(fs::exists(output))throw std::runtime_error("output_exists");
    std::map<std::size_t,json> reference;each_row(argv[3],18000,[&](json j){const auto ordinal=j.at("ordinal").get<std::size_t>();reference[ordinal]=std::move(j);});
    FakeClock clock;GameObserver observer(clock);std::size_t checked=0,different=0,calls=0;json examples=json::array();
    for(std::size_t i=0;i<input.index.size();++i){auto f=input.pixels(i,0,false);json current=nullptr;if(input.consumed.contains(f.sequence)||i<32){clock.set(f.capture_complete_ns);current=decision_json(observer.process(f));++calls;}
        if(reference.contains(i)){++checked;if(current!=reference.at(i).at("scene")){++different;if(examples.size()<5)examples.push_back({{"ordinal",i},{"current",current},{"reference",reference.at(i).at("scene")}});}}
    }
    save(output,{{"checked",checked},{"different",different},{"calls",calls},{"verified_pngs",input.verified},{"examples",examples},{"reference_sha256",sha256_file(argv[3])},{"binary_sha256",sha256_file(argv[0])}});return different?1:0;
}
bool subject(const std::string& id,const json& t) {
    const auto kind=t.value("kind","");const double x=t.value("x",0.0),y=t.value("y",0.0);
    if(id=="A3498")return kind=="hold"&&x>240&&x<370&&y>450;
    if(id=="B4986")return kind=="hold";
    if(id=="C5520")return kind=="flick";
    if(id=="D6214")return kind=="drag"&&y>250&&y<325;
    return true;
}
json selected_targets(const json& row,const std::string& id,bool recorded=false) {
    const auto& scene=row.at(recorded?"decision":"scene");json result=json::array();
    if(!scene.is_null())for(const auto& t:scene.at("targets"))if(subject(id,t))result.push_back(t);return result;
}
json line_pose(const json& row,const json& target,bool recorded=false) {
    const auto& scene=row.at(recorded?"decision":"scene");
    if(!scene.is_null())for(auto& line:scene.at("lines"))if(line.at("line_id")==target.at("line_id"))return line;
    return nullptr;
}
bool close_value(const json& a,const json& b,const char* key,double epsilon) {
    if(!a.contains(key)||!b.contains(key))return a.contains(key)==b.contains(key);
    if(a.at(key).is_null()||b.at(key).is_null())return a.at(key).is_null()==b.at(key).is_null();
    return std::abs(a.at(key).get<double>()-b.at(key).get<double>())<=epsilon;
}
json actions_for(const std::vector<json>& events,const std::set<std::uint64_t>& notes,std::size_t first,std::size_t last,bool include_prefix) {
    json output=json::array();std::map<int,std::uint64_t> contacts;
    for(const auto& e:events){const auto event=e.value("event","");const auto n=e.at("ordinal_at_delivery").get<std::size_t>();
        const bool keep=n<=last&&(include_prefix||n>=first);
        if(event=="fake_receipt"){
            const auto& c=e.at("command");const int contact=c.at("contact_id"),phase=c.at("phase");const auto note=e.contains("note_id")&&!e.at("note_id").is_null()?e.at("note_id").get<std::uint64_t>():0;
            if(keep&&notes.contains(note))output.push_back({{"ordinal",n},{"phase",phase},{"x",c.at("x")},{"y",c.at("y")},{"scheduled_ns",c.at("scheduled_ns")},{"success",e.at("success")},{"reason",e.at("reason")},{"source_frame",c.at("source_frame")},{"note_id_for_local_join",note},{"contact_id_for_local_join",contact}});
            if(phase==2)contacts.erase(contact);else contacts[contact]=note;
        } else if(event=="fake_release"){
            for(const auto& c:e.at("report").at("requested_ids")){const int contact=c;if(keep&&contacts.contains(contact)&&notes.contains(contacts.at(contact)))output.push_back({{"ordinal",n},{"phase","release_all"},{"start_ns",e.at("report").at("start_ns")},{"note_id_for_local_join",contacts.at(contact)},{"contact_id_for_local_join",contact}});}
            contacts.clear();
        }
    }return output;
}
json action_semantics(json j) {j.erase("note_id_for_local_join");j.erase("contact_id_for_local_join");return j;}
json compare_data(const json& manifest,const std::string& manifest_sha,const fs::path& aroot,
                  const fs::path& broot,const std::string& arole="c36h",const std::string& brole="main50") {
    const auto a=rows(aroot/"trace.jsonl",600),b=rows(broot/"trace.jsonl",600),original=rows(aroot/"original-recorded.jsonl",600);
    if(a.size()!=b.size()||a.size()!=original.size())throw std::runtime_error("comparison_frame_denominator");
    const auto ae=rows(aroot/"events.jsonl",100000),be=rows(broot/"events.jsonl",100000);json cases=json::array();
    for(const auto& w:manifest.at("windows")){
        const auto id=w.at("id").get<std::string>();const auto first=w.at("first").get<std::size_t>(),last=w.at("last").get<std::size_t>();
        json evidence=nullptr,state=nullptr,physical_pairs=json::array(),anchors=json::object();std::set<std::uint64_t> anotes,bnotes;std::size_t frames=0,unmatched=0;
        for(std::size_t i=0;i<a.size();++i){const auto ordinal=a[i].at("ordinal").get<std::size_t>();if(a[i].at("ordinal")!=b[i].at("ordinal")||a[i].at("png_sha256")!=b[i].at("png_sha256"))throw std::runtime_error("comparison_input_row");if(ordinal<first||ordinal>last)continue;++frames;
            const auto at=selected_targets(a[i],id),bt=selected_targets(b[i],id);for(auto& t:at)anotes.insert(t.at("note_id").get<std::uint64_t>());for(auto& t:bt)bnotes.insert(t.at("note_id").get<std::uint64_t>());
            if(id=="B4986") {for(auto& t:a[i].at("owner").is_null()?json::array():a[i].at("owner").at("identities"))if(subject(id,t))anotes.insert(t.at("note_id").get<std::uint64_t>());for(auto& t:b[i].at("owner").is_null()?json::array():b[i].at("owner").at("identities"))if(subject(id,t))bnotes.insert(t.at("note_id").get<std::uint64_t>());}
            bool evidence_equal=at.size()==bt.size(),state_equal=evidence_equal;json pairs=json::array();std::set<std::size_t> used;
            for(const auto& t:at){std::size_t best=bt.size();double cost=1e9;for(std::size_t k=0;k<bt.size();++k)if(!used.contains(k)&&bt[k].at("kind")==t.at("kind")){
                    const double d=std::hypot(t.at("x").get<double>()-bt[k].at("x").get<double>(),t.at("y").get<double>()-bt[k].at("y").get<double>());if(d<cost){cost=d;best=k;}}
                if(best==bt.size()||cost>16){evidence_equal=state_equal=false;++unmatched;continue;}used.insert(best);const auto& u=bt[best];
                pairs.push_back({{"a_note_id",t.at("note_id")},{"b_note_id",u.at("note_id")},{"centroid_distance_px",cost},{"review_status","proposed_geometric_match_not_human_gold"}});
                for(auto key:{"x","y","width","height","tail_x","tail_y"})evidence_equal&=close_value(t,u,key,2.0);
                for(auto key:{"rails_geometry","head_on_line","held_body_evidence","held_body_patch"})evidence_equal&=t.value(key,false)==u.value(key,false);
                const auto al=line_pose(a[i],t),bl=line_pose(b[i],u);bool same_line=al.is_null()==bl.is_null();if(!al.is_null()&&!bl.is_null())for(auto key:{"x","y","ux","uy"})same_line&=close_value(al,bl,key,key[0]=='u'?.02:2.0);
                state_equal&=same_line;for(auto key:{"reason","samples"})state_equal&=t.at(key)==u.at(key);state_equal&=close_value(t,u,"crossing_ns",10000);
            }
            state_equal&=a[i].at("lifecycle")==b[i].at("lifecycle");
            if(!evidence_equal&&evidence.is_null())evidence={{"ordinal",ordinal},{"a",at},{"b",bt}};
            if((!state_equal||!evidence_equal)&&state.is_null())state={{"ordinal",ordinal},{"a",at},{"b",bt},{"pairs",pairs},{"a_diagnostics",a[i].at("diagnostics")},{"b_diagnostics",b[i].at("diagnostics")}};
            physical_pairs.push_back({{"ordinal",ordinal},{"pairs",pairs}});
            if(ordinal==w.at("anchor").get<std::size_t>())anchors={{"ordinal",ordinal},{"original",original[i]},{arole,a[i]},{brole,b[i]}};
        }
        const auto aa=actions_for(ae,anotes,first,last,false),ba=actions_for(be,bnotes,first,last,false),ap=actions_for(ae,anotes,first,last,true),bp=actions_for(be,bnotes,first,last,true);json action_div=nullptr;
        for(std::size_t k=0;k<std::max(aa.size(),ba.size());++k){const auto av=k<aa.size()?action_semantics(aa[k]):json(nullptr),bv=k<ba.size()?action_semantics(ba[k]):json(nullptr);if(av!=bv){action_div={{"a",k<aa.size()?aa[k]:json(nullptr)},{"b",k<ba.size()?ba[k]:json(nullptr)},{"comparison","ordered_subject_commands_excluding_runtime_note_intent_contact_IDs"}};break;}}
        json down_a=json::array(),down_b=json::array();for(auto& t:ap)if(t.at("phase").is_number()&&t.at("phase")==0&&t.at("success").get<bool>())down_a.push_back(t);for(auto& t:bp)if(t.at("phase").is_number()&&t.at("phase")==0&&t.at("success").get<bool>())down_b.push_back(t);
        cases.push_back({{"id",id},{"window",w},{"frames",frames},{"first_evidence_divergence_in_window",evidence},{"first_state_divergence_in_window",state},{"first_action_divergence_in_window",action_div},{arole+"_successful_down_prefix",down_a},{brole+"_successful_down_prefix",down_b},{arole+"_actions",aa},{brole+"_actions",ba},{"geometry_pairs",physical_pairs},{"unmatched_target_occurrences",unmatched},{"anchor",anchors},{"physical_correspondence","proposed_geometric_many_to_many; human_gold=0"},{"scope","first in declared output window; successful Down prefix retained; original/perception prior unknown"},{"gameplay_outcome","unknown; normal E5287 remains user normal control"}});
        if(arole!="c36h") {
            json first_prefix=nullptr;
            for(std::size_t k=0;k<std::max(ap.size(),bp.size());++k)if(
                (k<ap.size()?action_semantics(ap[k]):json(nullptr))!=(k<bp.size()?action_semantics(bp[k]):json(nullptr))) {
                first_prefix={{"a",k<ap.size()?ap[k]:json(nullptr)},{"b",k<bp.size()?bp[k]:json(nullptr)}};break;
            }
            cases.back()["first_action_divergence_available_prefix"]=first_prefix;
            cases.back()[arole+"_prefix_action_count"]=ap.size();cases.back()[brole+"_prefix_action_count"]=bp.size();
        }
    }
    return {{"manifest_sha256",manifest_sha},{"a_summary_sha256",sha256_file(aroot/"summary.json")},{"b_summary_sha256",sha256_file(broot/"summary.json")},{"cases",cases},{"cases_denominator",5},{"frames_denominator",a.size()},{"unknown_in_denominator",true},{"thresholds",{{"pair_max_centroid_px",16},{"geometry_tolerance_px",2},{"line_tangent_tolerance",.02},{"root_time_tolerance_ns",10000}}}};
}
int compare(int argc,char** argv) {
    if(argc!=6)throw std::runtime_error("contact-compare manifest c36h-output main50-output new-report");const auto manifest=load(argv[2]);const fs::path aroot=argv[3],broot=argv[4],output=argv[5];if(fs::exists(output))throw std::runtime_error("output_exists");
    const auto sa=load(aroot/"summary.json"),sb=load(broot/"summary.json");const auto sha=sha256_file(argv[2]);
    validate_comparison_inputs(manifest,sha,sa,sb);
    save(output,compare_data(manifest,sha,aroot,broot));return 0;
}
#ifdef PAS_X2_OFFLINE
int compare_x2(int argc,char** argv) {
    if(argc!=7)throw std::runtime_error("contact-x2-compare manifest c36h_reference main50_control main50_no_confirmed_winner_override new-report");
    const fs::path output=argv[6];if(fs::exists(output))throw std::runtime_error("output_exists");
    BatchWriterLease lease;
    const auto manifest=load(argv[2]);const auto sha=sha256_file(argv[2]);
    const std::array<fs::path,3> roots{{argv[3],argv[4],argv[5]}};
    const std::array<json,3> summaries{{load(roots[0]/"summary.json"),load(roots[1]/"summary.json"),load(roots[2]/"summary.json")}};
    x2::validate_runs(manifest,sha,summaries);
    const auto control=rows(roots[1]/"state-digests.jsonl",36000),variant=rows(roots[2]/"state-digests.jsonl",36000);
    if(control.size()!=variant.size()||control.size()!=summaries[1].at("input_frames"))throw std::runtime_error("x2_digest_denominator");
    json first_state=nullptr;
    for(std::size_t i=0;i<control.size();++i) {
        if(control[i].at("ordinal")!=variant[i].at("ordinal")||control[i].at("png_sha256")!=variant[i].at("png_sha256"))throw std::runtime_error("x2_digest_input");
        if(first_state.is_null()&&control[i].at("semantic_sha256")!=variant[i].at("semantic_sha256"))first_state={{"ordinal",i},{"control",control[i]},{"variant",variant[i]}};
    }
    const auto ce=rows(roots[1]/"events.jsonl",100000),ve=rows(roots[2]/"events.jsonl",100000);
    json first_action=nullptr;std::vector<json> ca,va;
    for(auto e:ce)if(e.at("event")=="fake_receipt"||e.at("event")=="fake_release")ca.push_back(std::move(e));
    for(auto e:ve)if(e.at("event")=="fake_receipt"||e.at("event")=="fake_release")va.push_back(std::move(e));
    for(std::size_t i=0;i<std::max(ca.size(),va.size());++i) {
        auto a=i<ca.size()?ca[i]:json(nullptr),b=i<va.size()?va[i]:json(nullptr);
        const auto normalize=[](json j){if(!j.is_null()){j.erase("note_id");if(j.contains("command"))j["command"].erase("intent_id");}return j;};
        if(normalize(a)!=normalize(b)){first_action={{"control",a},{"variant",b},{"scope","ordered global receipt/release semantics excluding note and intent IDs"}};break;}
    }
    json report={{"experiment","confirmed_preserve_winner_only_x2"},{"manifest_sha256",sha},{"roles",manifest.at("roles")},
        {"summaries",summaries},{"first_state_divergence_available_prefix",first_state},{"first_action_divergence_available_prefix",first_action},
        {"reference_vs_control",compare_data(manifest,sha,roots[0],roots[1],x2::roles[0],x2::roles[1])},
        {"control_vs_variant",compare_data(manifest,sha,roots[1],roots[2],x2::roles[1],x2::roles[2])},
        {"physical_gold",false},{"gameplay_outcome","unknown_fixed_C36g_pixels"}};
    const fs::path batch=manifest.at("batch_root").get<std::string>();
    if(fs::absolute(output).lexically_normal().generic_string().find(fs::absolute(batch).lexically_normal().generic_string()+"/")!=0)throw std::runtime_error("output_outside_batch");
    const auto bytes=report.dump(2).size()+1;
    if(tree_bytes(batch)+bytes>67108864||tree_bytes(manifest.at("campaign_root").get<std::string>())+
       tree_bytes(manifest.at("prior_research_root").get<std::string>())+bytes>8ULL*1024*1024*1024)throw std::runtime_error("output_quota");
    save(output,report);return 0;
}
#endif
}
int contact_replay_main(int argc,char** argv) {try{
#ifdef PAS_X4_OFFLINE
    if(argc>1&&std::string(argv[1])=="contact-x4")return replay(argc,argv);
#endif
    if(std::string(argv[1])=="contact")return replay(argc,argv);
    if(std::string(argv[1])=="observer-compat")return compatibility(argc,argv);
    if(std::string(argv[1])=="contact-compare")return compare(argc,argv);
#ifdef PAS_X2_OFFLINE
    if(std::string(argv[1])=="contact-x2")return replay(argc,argv);
    if(std::string(argv[1])=="contact-x2-compare")return compare_x2(argc,argv);
#endif
    throw std::runtime_error("unknown_contact_command");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
