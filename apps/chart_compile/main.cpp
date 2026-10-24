#include "rhythm/contact_compiler.hpp"
#ifdef RHYTHM_HAS_PAS_BRIDGE
#include "rhythm/pas_bridge.hpp"
#endif
#ifdef RHYTHM_HAS_SESSION_RUNNER
#include "rhythm/session_runner.hpp"
#endif
#include <nlohmann/json.hpp>
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <tuple>

namespace {
rhythm::Ns integer(const char* text) {
    const std::string value=text; rhythm::Ns result=0;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size()) throw std::invalid_argument("invalid integer argument");
    return result;
}
}
int main(int argc,char** argv) {
    try {
        if(argc<9) throw std::invalid_argument(
            "Usage: chart_compile CHART SHA AUDIO_SHA SONG DIFFICULTY REVISION LAYOUT CAPACITY [--segment-ns FIRST LAST] [--fake-replay|--session-replay] [--normal-intersection-pixels LIMIT CALIBRATION_ID]\n"
            "Offline only; output is a candidate, never device calibration. Input geometry is 1280x720, no rotation.");
        if(std::filesystem::file_size(argv[1])>32*1024*1024) throw std::invalid_argument("chart size budget");
        std::ifstream file(argv[1],std::ios::binary); if(!file) throw std::runtime_error("cannot read chart");
        const std::string raw((std::istreambuf_iterator<char>(file)),{});
        auto chart=rhythm::parse_phigros(raw,{"local-chart-compile",argv[2],argv[4],argv[5],argv[6]});
        const auto capacity=integer(argv[8]); if(capacity<1 || capacity>10) throw std::invalid_argument("capacity 1..10");
        bool replay=false,session_replay=false,segment=false,projection=false;
        rhythm::ContactCompileOptions options; options.max_contacts=static_cast<int>(capacity);
        rhythm::Ns first=0,last=0;
        for(int a=9;a<argc;++a) {
            const std::string option=argv[a];
            if(option=="--fake-replay" && !replay) replay=true;
            else if(option=="--session-replay" && !replay) {replay=true;session_replay=true;}
            else if(option=="--normal-intersection-pixels" && !projection && a+2<argc) {
                projection=true;const auto limit=integer(argv[++a]);
                if(limit<1||limit>1000)throw std::invalid_argument("normal projection limit 1..1000 reference pixels");
                options.offscreen_policy=rhythm::OffscreenPolicy::bounded_normal_intersection_candidate_v1;
                options.max_normal_projection_reference_pixels=static_cast<double>(limit);
                options.calibration_id=argv[++a];
            }
            else if(option=="--segment-ns" && !segment && a+2<argc) {
                segment=true; first=integer(argv[++a]);last=integer(argv[++a]);
                if(first<0 || last<=first || last>3'600'000'000'000) throw std::invalid_argument("segment range");
            } else throw std::invalid_argument("unknown or duplicate option");
        }
        const auto original_notes=chart.note_count;
        if(segment) {
            chart.note_count=0;
            for(auto& line:chart.lines) {
                std::erase_if(line.notes,[&](const auto& n){return n.start_ns<first || n.end_ns>=last;});
                chart.note_count+=line.notes.size();
            }
        }
        const rhythm::Identity identity{rhythm::Game::phigros,argv[4],argv[5],argv[6],argv[2],argv[3],argv[7]};
        rhythm::PhigrosGeometry geometry; geometry.input_width=1280;geometry.input_height=720;
        const auto result=rhythm::compile_phigros_contacts(chart,identity,geometry,options);
        nlohmann::json out={{"schema","pas.compiled-contact-candidate.v1"},{"status",result.plan ? "compiled_offline" : "rejected"},
            {"live_qualified",false},{"chart_sha256",identity.chart_sha256},{"audio_sha256",identity.audio_sha256},
            {"audio_hash_scope","supplied identity, not audio bytes verified by this CLI"},
            {"song_id",identity.song_id},{"difficulty",identity.difficulty},{"revision",identity.revision},{"layout_id",identity.layout_id},
            {"input_geometry",{{"width",1280},{"height",720},{"rotation",0},{"fit","contain"}}},
            {"capacity",capacity},{"source_notes",original_notes},{"selected_notes",chart.note_count},
            {"selection",segment ? nlohmann::json{{"kind","explicit_subset"},{"start_ns",first},{"end_ns_exclusive",last},
                {"policy","entire judgment-to-hold-end inside interval; compiler lead/release may extend beyond interval"}}
                : nlohmann::json{{"kind","whole_chart"}}},
            {"diagnostics",nlohmann::json::array()}};
        if(result.required_peak_contacts) out["required_peak_contacts"]=*result.required_peak_contacts;
        for(const auto& d:result.diagnostics) out["diagnostics"].push_back({
            {"severity",d.severity==rhythm::CompileSeverity::error ? "error" : "warning"},{"code",d.code},{"message",d.message},
            {"source_note_id",d.source_note_id},{"related_note_id",d.related_note_id},{"song_ns",d.song_ns},
            {"line_index",d.line_index},{"note_index_in_selected_ir",d.note_index}});
        if(result.plan) {
            const auto& plan=*result.plan;
            out["compiler_version"]=plan.metadata.compiler_version;out["cache_sha256"]=plan.metadata.cache_sha256;
            out["strategy"]=plan.metadata.strategy;out["requires_calibration"]=true;
            out["projection_candidate"]=plan.metadata.requires_calibration;
            out["projection_policy_version"]=plan.metadata.projection_policy_version;
            out["projection_calibration_id"]=options.calibration_id;
            out["max_normal_projection_reference_pixels"]=options.max_normal_projection_reference_pixels;
            out["max_path_error_pixels"]=options.max_path_error_pixels;out["total_steps"]=plan.total_steps;
            out["contacts"]=nlohmann::json::array();out["ownership"]=nlohmann::json::array();
            for(const auto& c:plan.memory.contacts) {
                nlohmann::json steps=nlohmann::json::array();
                for(const auto& s:c.steps) steps.push_back({{"song_ns",s.song_ns},{"phase",static_cast<int>(s.phase)},{"point",{s.point.x,s.point.y}}});
                out["contacts"].push_back({{"id",c.id},{"steps",std::move(steps)}});
            }
            for(const auto& o:plan.ownership) {
                nlohmann::json spans=nlohmann::json::array();
                for(const auto& span:o.normal_projection_spans)spans.push_back({
                    {"begin_ns",span.begin_ns},{"end_ns",span.end_ns},
                    {"correction_lower_reference_pixels",span.correction_lower_reference_pixels},
                    {"correction_upper_reference_pixels",span.correction_upper_reference_pixels},
                    {"possible_edge_mask",span.possible_edge_mask},
                    {"speed_bound_input_pixels_per_ns",span.speed_bound_input_pixels_per_ns}});
                out["ownership"].push_back({
                    {"source_note_id",o.source_note_id},{"source_json_pointer",o.source_json_pointer},{"contact_id",o.contact_id},
                    {"kind",static_cast<int>(o.kind)},{"judgment_ns",o.judgment_ns},
                    {"normal_projection_pixels",o.normal_projection_reference_pixels},
                    {"time_varying_normal_projection",o.time_varying_normal_projection},
                    {"projection_min_reference_pixels",o.projection_min_reference_pixels},
                    {"projection_max_reference_pixels",o.projection_max_reference_pixels},
                    {"normal_projection_spans",std::move(spans)}});
            }
            if(replay) {
#ifdef RHYTHM_HAS_PAS_BRIDGE
                pas::FakeClock clock;pas::FakeTouchBackend touch(clock);
                rhythm::PasBridgeConfig bridge_config;bridge_config.max_contacts=static_cast<int>(capacity);
                rhythm::PasBridge bridge(plan.memory,clock,touch,bridge_config);
                const auto origin=1'000'000'000LL;
                rhythm::ExecutionObservation observed{identity,{0,origin,1},{true,1,1,0,150'000'000,1'000'000},1};
                const auto end=plan.events.back().song_ns+origin+1'000'000;
                if(end>3'601'000'000'000) throw std::runtime_error("fake duration budget");
                std::string replay_reason;
                bool release_acknowledged=false;
#ifdef RHYTHM_HAS_SESSION_RUNNER
                std::unique_ptr<rhythm::SessionRunner> runner;
                rhythm::StateObservation state;
                rhythm::SupervisorSyncObservation sync;
                if(session_replay) {
                    if(end+1'000'000>600'000'000'000)throw std::invalid_argument("session simulation bounded by 600 seconds");
                    rhythm::PlanQualification q;q.identity=identity;q.geometry_version=1;
                    q.compiled_cache_sha256=plan.metadata.cache_sha256;
                    q.all_notes_in_scope_compiled=true;q.requires_calibration=true;
                    if(segment) {q.coverage=rhythm::PlanCoverage::explicit_segment;q.segment_begin_ns=first;q.segment_end_ns=last;}
                    auto config=rhythm::fake_session_config();config.max_run_duration_ns=end+1'000'000;
                    runner=std::make_unique<rhythm::SessionRunner>(plan,q,clock,touch,config);
                    state.identity=identity;state.state=rhythm::ObservedGameState::playing;
                    state.source=rhythm::StateObservationSource::visual;
                    state.timestamp_kind=rhythm::StateTimestampKind::mapped_capture;
                    state.sequence=1;state.epoch=1;state.geometry_version=1;
                    state.evidence_id="synthetic-state-1";
                    state.observed_host_ns=0;state.received_host_ns=0;state.expires_host_ns=150'000'000;
                    state.timestamp_uncertainty_ns=1'000'000;
                    sync={identity,observed.clock,observed.lease,1,0,"synthetic-sync-1"};
                    runner->observe_state(state);runner->observe_sync(sync);
                    if(!runner->arm(1).running)throw std::runtime_error("fake supervised arm rejected");
                }
#else
                if(session_replay)throw std::invalid_argument("--session-replay requires RHYTHM_BUILD_SESSION_RUNNER");
#endif
                if(!session_replay&&!bridge.begin(observed)) throw std::runtime_error("fake begin rejected");
                rhythm::BridgeTick tick;std::size_t peak_pending=0,peak_active=0;
                for(rhythm::Ns now=0;now<=end;now+=1'000'000) {
                    clock.set(now);
                    if(now && now%40'000'000==0) {
                        ++observed.sequence;observed.lease.observed_host_ns=now;observed.lease.expires_host_ns=now+150'000'000;
#ifdef RHYTHM_HAS_SESSION_RUNNER
                        if(runner) {
                            state.sequence=observed.sequence;state.observed_host_ns=now;state.received_host_ns=now;
                            state.expires_host_ns=now+150'000'000;state.evidence_id="synthetic-state-"+std::to_string(state.sequence);
                            sync.sequence=observed.sequence;sync.lease=observed.lease;sync.received_host_ns=now;
                            sync.evidence_id="synthetic-sync-"+std::to_string(sync.sequence);
                            runner->observe_state(state);runner->observe_sync(sync);
                        } else
#endif
                        if(!bridge.update(observed)) break;
                    }
#ifdef RHYTHM_HAS_SESSION_RUNNER
                    if(runner) {
                        const auto status=runner->tick();
                        if(status.dispatched)tick=*status.dispatched;
                        else {tick.running=status.running;tick.finished=status.finished;}
                        replay_reason=status.terminal_reason;
                        if(status.finished&&touch.contacts().empty()) {
                            const rhythm::ReleaseAcknowledgement ack{status.supervision.revocation_id,status.supervision.revoked_epoch,
                                rhythm::ReleaseKnowledge::known_released,now,now,"actual empty FakeTouch map after finite simulation"};
                            release_acknowledged=!runner->acknowledge_release(ack).supervision.release_required;
                        }
                    } else
#endif
                    {tick=bridge.tick();replay_reason=bridge.reason();}
                    peak_pending=std::max(peak_pending,tick.pending);peak_active=std::max(peak_active,tick.active);
                    if(!tick.running) break;
                }
                std::vector<std::size_t> births(plan.memory.contacts.size());
                std::iota(births.begin(),births.end(),0);
                std::sort(births.begin(),births.end(),[&](auto a,auto b) {
                    const auto& ca=plan.memory.contacts[a];const auto& cb=plan.memory.contacts[b];
                    return std::pair(ca.steps.front().song_ns,ca.id)<std::pair(cb.steps.front().song_ns,cb.id);
                });
                std::map<std::uint64_t,std::uint64_t> intent;
                for(std::size_t i=0;i<births.size();++i) intent[plan.memory.contacts[births[i]].id]=i+1;
                auto expected=plan.events;
                std::sort(expected.begin(),expected.end(),[&](const auto& a,const auto& b) {
                    return std::tuple(a.song_ns,rhythm::phase_order(a.phase),intent.at(a.contact_id))<
                           std::tuple(b.song_ns,rhythm::phase_order(b.phase),intent.at(b.contact_id));
                });
                std::map<std::uint64_t,int> held;
                bool matched=tick.finished && touch.receipts().size()==expected.size();
                // Core ties use increasing admission birth IDs, not source array
                // IDs. Verify this stable order and each contact's fixed pointer.
                for(std::size_t i=0;matched && i<expected.size();++i) {
                    const auto& e=expected[i];const auto& r=touch.receipts()[i];
                    const auto pixel=rhythm::pixel_point(e.point,1280,720);
                    matched=r.success && r.command.scheduled_ns==e.song_ns+origin &&
                        r.command.intent_id==intent.at(e.contact_id) &&
                        static_cast<int>(r.command.phase)==static_cast<int>(e.phase) &&
                        r.command.x==pixel.first && r.command.y==pixel.second;
                    if(e.phase==rhythm::Phase::down) matched=held.emplace(e.contact_id,r.command.contact_id).second && matched;
                    else {
                        const auto owner=held.find(e.contact_id);
                        matched=matched && owner!=held.end() && owner->second==r.command.contact_id;
                        if(e.phase==rhythm::Phase::up) held.erase(e.contact_id);
                    }
                }
                matched=matched && held.empty();
                matched=matched&&(!session_replay||release_acknowledged);
                out["fake_replay"]={{"finished",tick.finished},{"reason",replay_reason},{"ordered_receipts_match",matched},
                    {"supervised_session",session_replay},{"supervisor_release_acknowledged",release_acknowledged},
                    {"receipts",touch.receipts().size()},{"completed_contacts",tick.completed_contacts},{"peak_pending",peak_pending},
                    {"peak_active",peak_active},{"contacts_released",touch.contacts().empty()},
                    {"observation_scope","synthetic exact clock renewed every 40ms; no acoustic or device proof"}};
                if(!matched) {out["status"]="fake_replay_failed";std::cout<<out.dump(2)<<'\n';return 3;}
#else
                throw std::invalid_argument("--fake-replay requires RHYTHM_BUILD_PAS_BRIDGE");
#endif
            }
        }
        std::cout<<out.dump(2)<<'\n';return result.plan ? 0 : 2;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
