#include "pas/runtime.hpp"
#include "pas/candidate_receipts.hpp"
#include "pas/game_session.hpp"
#include "pas/candidate_packet.hpp"
#include "owner_candidate_step.hpp"
#include "pas/session_archive.hpp"
#include "pas/session_pixel_clips.hpp"
#include "pas/session_recording.hpp"
#include "pas/analysis.hpp"
#include "pas/preview.hpp"
#include "pas/strategy_version.hpp"
#include "pas/manual_score_budget.hpp"
#include "pas/manual_score_diagnostics.hpp"
#include "session_build_provenance.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace pas {
namespace {
std::atomic<bool> manual_stop=false;
BOOL WINAPI stop_handler(DWORD e) {
    if(e==CTRL_C_EVENT||e==CTRL_BREAK_EVENT||e==CTRL_CLOSE_EVENT||e==CTRL_SHUTDOWN_EVENT) {
        manual_stop=true;return TRUE;
    }return FALSE;
}
class StopGuard {
public:
    StopGuard(){manual_stop=false;if(!SetConsoleCtrlHandler(stop_handler,TRUE))throw std::runtime_error("stop handler failed");}
    ~StopGuard(){SetConsoleCtrlHandler(stop_handler,FALSE);}
};
class Wake {
public:
    Wake() {
        event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);
        if(!event_||!timer_){if(event_)CloseHandle(event_);if(timer_)CloseHandle(timer_);throw std::runtime_error("action wake creation failed");}
    }
    ~Wake(){CancelWaitableTimer(timer_);CloseHandle(timer_);CloseHandle(event_);}
    void notify(){SetEvent(event_);}
    void wait(Nanoseconds delay) {
        if(delay<=0)return;
        LARGE_INTEGER due;due.QuadPart=-((delay+99)/100);
        if(!SetWaitableTimerEx(timer_,&due,0,nullptr,nullptr,nullptr,0))throw std::runtime_error("action timer failed");
        HANDLE handles[]{event_,timer_};const auto result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
        if(result!=WAIT_OBJECT_0&&result!=WAIT_OBJECT_0+1)throw std::runtime_error("action wait failed");
    }
private:HANDLE event_{},timer_{};
};
enum class ManualVariant {legacy,current_candidate};
struct Packet : SessionObservation {
    std::optional<CandidatePacketEvidence> candidate;
    Nanoseconds published_ns=0;
    std::uint64_t packet_sequence=0;
    std::shared_ptr<const Frame> result;
    std::optional<std::int64_t> source_time;
    std::optional<std::uint64_t> source_sequence;
};
using nlohmann::json;
json release_json(const ReleaseReport& r,const std::string& reason) {
    return {{"event","game_release_report"},{"reason",reason},{"requested_ids",r.requested_ids},
        {"failed_ids",r.failed_ids},{"unknown_ids",r.unknown_ids},{"start_ns",r.start_ns},{"return_ns",r.return_ns},
        {"effect_verified",false}};
}
}
static void run_manual_session_impl(const std::string& config_path,const std::string& capability_path,bool no_preview,Nanoseconds watchdog,bool pixel_clips,bool full_recording,bool one_round,ManualVariant variant,
    SessionArchive::Persistence persistence=SessionArchive::Persistence::diagnostic_archive,CandidatePhysicalAggregate* physical=nullptr,unsigned score_round_limit=1) {
    const bool candidate_mode=variant==ManualVariant::current_candidate;
    const bool score_only=persistence==SessionArchive::Persistence::none;
    if(score_only&&(!no_preview||pixel_clips||full_recording||one_round!=(score_round_limit==1)||score_round_limit<1||score_round_limit>20||watchdog<=0||watchdog>600'000'000'000LL))
        throw std::invalid_argument("score-only trial requires no preview/recording/clips, 1..20 rounds and watchdog at most 600s");
    const auto config=load_config(config_path);
    if(config.capture_kind!="emulator-grpc"||config.touch_kind!="emulator-grpc"||
       config.width!=1280||config.height!=720||config.source_rotation!=1||config.grpc_read_chunk_kib!=256||
       config.max_relative_lag_ms!=250||config.touch_width!=720||config.touch_height!=1280||config.touch_rotation!=90||
       config.touch_timeout_ms!=100||config.max_plans!=128||config.max_steps!=16||config.horizon_ms!=350||
       config.evidence_max_age_ms!=100||config.max_contacts!=5||config.game_type_mask!=15||
       config.game_lead_ms<30||config.game_lead_ms>45||config.game_uncertainty_ms!=30)
        throw std::invalid_argument("manual-session requires 1280x720 rotation1 / 256KiB / five contacts / all types / lead30..45 / uncertainty30 profile");
    if(watchdog<0||watchdog>3'600'000'000'000LL)throw std::invalid_argument("invalid round watchdog");
    StopGuard stop_guard;
    const auto preflight=game_preflight(config_path,capability_path);
    if(!preflight.at("fingerprint_matches").get<bool>())throw std::runtime_error("manual-session capability fingerprint mismatch");
    HostClock clock;
    const auto session_start=clock.now_ns();
    std::optional<ManualScoreBudget> score_budget;
    if(score_only)score_budget.emplace(session_start,watchdog,score_round_limit);
    wchar_t module[32768]{};const auto size=GetModuleFileNameW(nullptr,module,32768);
    if(!size||size==32768)throw std::runtime_error("runtime executable unavailable");
    const auto root=std::filesystem::path(config.log_dir)/("manual-session-"+std::to_string(clock.now_ns()));
    auto manifest=json{{"schema_version",1},{"mode","manual-session"},{"strategy",game_strategy_name()+" plus manual standby lifecycle"},
        {"baseline_source_commit","5ad759ef5004ab89be8f1a326e75fb96e7cb9a0e"},
        {"baseline_historical_binary_sha256","d7ce474576a0283711b046b82720f9c10e8de0bb92203eb999a6c8decc157162"},
        {"integration_parent_commits",json::array({"39366fdc068e85a7fa91f929ddf02acab09879f7",
            "98169a575bd7c3b7503849ceb4b91a934d50ce0f"})},
        {"executable_sha256",sha256_file(module)},{"config_sha256",sha256_file(config_path)},
        {"source_build_commit",pas_session_build_commit},{"source_build_dirty",pas_session_build_dirty},
        {"compiled_source_sha256",json::parse(pas_session_source_hashes)},
        {"config",config.public_json},{"capability_preflight",preflight},{"clock_domain","host_qpc_ns"},{"qpc_frequency",clock.frequency()},
        {"game_observer_version",game_observer_version},{"game_planner_version",game_planner_version},
        {"effective_game_lead_ns",static_cast<Nanoseconds>(config.game_lead_ms)*1'000'000},
        {"effective_game_uncertainty_ns",static_cast<Nanoseconds>(config.game_uncertainty_ms)*1'000'000},
        {"game_diagnostics_version",game_diagnostics_version},{"lifecycle_version",2},{"candidate_variant","main-worktree-integration-20261001"},
        {"automatic_play_enabled",false},{"round_watchdog_ns",watchdog},{"watchdog_is_result",false},
        {"standby_timeout",nullptr},{"source_absolute_age",nullptr},{"input_policy","manual_PLAY_only_gated_gameplay"},
        {"capture",grpc_transport_manifest(256)},{"capture_pool_slots",3},{"decision_slots",1},
        {"journal_queue_capacity",8192},{"journal_segment_bytes",16*1024*1024},{"round_max_segments",32},
        {"standby_segment_bytes",1024*1024},{"standby_segments",4},{"stats_samples_per_round_max",100000},
        {"dataset_sampling",false},{"pixel_clip_sampling",pixel_clips},
        {"full_recording",full_recording},{"full_recording_version",full_recording?json(1):json(nullptr)},
        {"full_recording_policy",full_recording?"one round; every received same-stream frame; 32 pre-roll; 64 outstanding copies; 3 PNG encoders with ordered disk commit; 99 physical RGB slots; lossless PNG RGB24; 36000 frames/600s/5GiB PNG+index; overflow faults instead of sampling; no runtime feedback":"disabled"},
        {"session_round_limit",(full_recording||one_round)?json(1):json(nullptr)},
        {"pixel_clip_policy",pixel_clips?"at most 20 rounds x (8 uniform + 2 complex-line) x 3 full 1280x720 RGB888 frames; 4-frame writer mailbox; no runtime feedback":"disabled"},
        {"result_image_policy","one_same_capture_frame_per_confirmed_round_after_owner_release"},
        {"result_ui_profile","English six static labels / 1280x720 / translation <=3px"}};
    if(candidate_mode) {
        manifest["mode"]="manual-session-current-candidate";manifest["candidate_variant"]="current-rails-shared-owner-step-disabled";
        manifest["candidate_live_admission"]="UNQUALIFIED";manifest["changed_windows_branch_executed"]=false;
        manifest["candidate_packet_slots"]=1;manifest["candidate_receipt_capacity"]=64;
        manifest["candidate_packet_policy"]="same immutable consumed frame lease and complete observer batch; transition/context mismatch ineligible";
        manifest["diagnostic_snapshot_policy"]="owned current scalars only; writer encoding; 256KiB charged per snapshot; 16MiB mailbox plus one in-flight item";
        manifest["journal_encoding"]="gzip-jsonl-v1";manifest["round_max_segments"]=256;manifest["round_decoded_byte_cap"]=4ull*1024*1024*1024;manifest["round_physical_byte_cap"]=512ull*1024*1024;
    }
    SessionArchive archive(root,manifest,16*1024*1024,1024*1024,candidate_mode?SessionArchive::Encoding::gzip:SessionArchive::Encoding::plain,persistence);
    std::unique_ptr<SessionPixelClips> clips;
    if(pixel_clips)clips=std::make_unique<SessionPixelClips>(root/"pixel-clips",clock,config.width,config.height);
    std::unique_ptr<SessionRecording> recording;
    if(full_recording)recording=std::make_unique<SessionRecording>(root/"full-recording",clock,config.width,config.height);
    auto endpoint=discover_endpoint(config.serial);
    if(!config.endpoint.empty()) {
        std::ifstream f(config.token_file);std::getline(f,endpoint.token);
        if(!endpoint.token.empty()&&endpoint.token.back()=='\r')endpoint.token.pop_back();
        if(endpoint.token.empty())throw std::runtime_error("gRPC token unavailable");
        endpoint.target=config.endpoint;endpoint.instance="explicit";
    }
    GrpcCapture capture(clock,endpoint,runtime_capture_options(config));
    LatestFrame latest(config.width,config.height,3,&clock);
    Wake wake;
    std::atomic<bool> stopping=false,live_gate=false;
    std::atomic<std::uint64_t> round_epoch=1,revocation=0;
    std::atomic<Nanoseconds> last_capture=0;
    std::atomic<Nanoseconds> invalid_through=0;
    std::atomic<Nanoseconds> active_round_start=0;
    std::mutex packet_mutex,fault_mutex;
    std::shared_ptr<const Packet> packet;
    std::atomic<std::uint64_t> candidate_packet_published=0,candidate_packet_overwritten=0,candidate_packet_consumed=0,candidate_packet_skips=0;
    std::shared_ptr<const Frame> display;
    std::exception_ptr fault;
    const auto fail=[&] {
        {std::lock_guard lock(fault_mutex);if(!fault)fault=std::current_exception();}
        live_gate=false;stopping=true;++revocation;wake.notify();latest.close();capture.cancel();
    };
    std::jthread capture_worker([&](std::stop_token stop) {
        try {
            capture.stream(stop,[&](Frame&& f) {
                f.epoch=round_epoch.load();f.generation=1;f.geometry_version=1;
                if(f.width!=config.width||f.height!=config.height||f.source_rotation!=config.source_rotation)
                    throw std::runtime_error("capture geometry changed");
                last_capture=f.capture_complete_ns;latest.publish(f.rgb.data(),f.rgb.size(),f);
                if(recording)recording->observe(f); // independent of perception/owner frame skips
            });
            if(!stopping&&!stop.stop_requested())throw std::runtime_error("capture stream ended");
        } catch(...) {fail();}
    });
    std::jthread perception_worker([&](std::stop_token stop) {
        try {
            SessionPerception perception(clock,watchdog);
            std::uint64_t sequence=0;Nanoseconds preview_at=0;
            std::shared_ptr<const Frame> result_frame;
            while(!stopping&&!stop.stop_requested()) {
                auto f=latest.read_after(sequence,10'000'000);if(!f)continue;sequence=f->sequence;
                if(clock.now_ns()-f->capture_complete_ns>=100'000'000){live_gate=false;continue;}
                auto p=std::make_shared<Packet>();static_cast<SessionObservation&>(*p)=perception.process(*f,candidate_mode);
                if(candidate_mode&&perception.candidate_batch())p->candidate=CandidatePacketEvidence{f,*perception.candidate_batch()};
                if(clips)clips->observe(*f,*p);
                if(p->status.new_round) {
                    round_epoch=p->status.round;result_frame.reset();
                    if(recording)recording->start(p->status.round);
                }
                if(recording&&p->status.ended)recording->finish(p->status.round);
                // This independent latch closes dispatch before the action owner consumes the packet.
                live_gate=p->allow_down&&f->capture_complete_ns>invalid_through.load()&&
                    (!candidate_mode||(p->candidate&&p->candidate->eligible(*p,clock.now_ns())));
                if(!score_only&&p->status.ended&&p->status.state==PlaySessionState::result&&!result_frame)
                    result_frame=std::make_shared<Frame>(*f); // One bounded copy; never a pool lease.
                p->result=result_frame;p->source_time=f->source_timestamp_us;p->source_sequence=f->source_sequence;
                const auto now=clock.now_ns();
                if(!no_preview&&config.preview_hz>0&&now-preview_at>=static_cast<Nanoseconds>(1e9/config.preview_hz)) {
                    auto view=std::make_shared<Frame>(*f);draw_game_overlay(*view,p->scene);
                    std::lock_guard lock(packet_mutex);display=std::move(view);preview_at=now;
                }
                p->published_ns=clock.now_ns();
                {std::lock_guard lock(packet_mutex);
                    if(candidate_mode) {
                        p->packet_sequence=++candidate_packet_published;
                        if(packet&&packet->packet_sequence>candidate_packet_consumed.load())++candidate_packet_overwritten;
                    }
                    packet=std::move(p);
                }
                wake.notify();
            }
        } catch(...) {fail();}
    });
    std::jthread action_worker([&](std::stop_token stop) {
        std::unique_ptr<GrpcTouch> backend;
        current_execution::ReceiptStream candidate_receipts;
        std::unique_ptr<current_execution::RecordingBackend> candidate_backend;
        std::unique_ptr<SessionGameOwner> game_storage;
        std::unique_ptr<current_rails::OwnerCandidateStep> candidate_step;
        std::uint64_t active=0,completed=0,seen=0,rev=0,commands=0,frames=0;
        Nanoseconds round_start=0,previous=0,previous_playing=0;
        std::vector<double> intervals,playing_intervals,recognition,residency;
        PlaySessionState last_state=PlaySessionState::standby;
        struct Alignment {std::uint64_t note=0;bool unaligned=false;};std::array<Alignment,128> alignment{};
        const auto record=[&](json value){archive.event(active,std::move(value));};
        const auto record_lazy=[&](auto&& factory){emit_manual_diagnostic(archive,active,score_only,std::forward<decltype(factory)>(factory));};
        try {
            backend=std::make_unique<GrpcTouch>(clock,endpoint,PixelCoordinateMap(config.width,config.height,config.touch_width,config.touch_height,config.touch_rotation),
                config.touch_width,config.touch_height,config.max_contacts,std::chrono::milliseconds(config.touch_timeout_ms));
            if(candidate_mode)candidate_backend=std::make_unique<current_execution::RecordingBackend>(*backend,candidate_receipts,clock);
            game_storage=std::make_unique<SessionGameOwner>(clock,candidate_mode?static_cast<TouchBackend&>(*candidate_backend):static_cast<TouchBackend&>(*backend),config.max_contacts,
                GameActionOptions{config.game_type_mask,static_cast<Nanoseconds>(config.game_lead_ms)*1'000'000,
                 static_cast<Nanoseconds>(config.game_uncertainty_ms)*1'000'000});
            auto& game=*game_storage;
            if(candidate_mode)candidate_step=std::make_unique<current_rails::OwnerCandidateStep>(clock,game,candidate_receipts);
            if(candidate_step) {
                candidate_step->event_output=record;
                candidate_step->snapshot_output=[&](DiagnosticSnapshot value){archive.event(active,std::move(value));};
                candidate_step->receipt_output=[&](const current_execution::BackendEvent& e){if(!e.release){++commands;if(physical)physical->receipt(e.dispatched&&e.receipt.success);}record(current_execution::backend_event_json(e));};
            }
            const auto receipts=[&](const std::vector<TouchReceipt>& rs) {
                for(const auto& r:rs) {
                    ++commands;record_lazy([&]{return json{{"event","game_touch_receipt"},{"intent_id",r.command.intent_id},{"contact_id",r.command.contact_id},
                        {"phase",static_cast<int>(r.command.phase)},{"scheduled_ns",r.command.scheduled_ns},
                        {"source_frame",r.command.source_frame_sequence},{"injection_start_ns",r.injection_start_ns},
                        {"injection_return_ns",r.injection_return_ns},{"success",r.success},{"reason",r.reason},{"real_input",true}};});
                }
            };
            const auto finish=[&](const std::string& reason,bool result,std::shared_ptr<const Frame> image) {
                live_gate=false;ReleaseReport release;
                if(candidate_step) {if(!candidate_step->finish())throw std::runtime_error("candidate first release not verified");release=candidate_step->final_release;}
                else release=game.finish();
                if(physical)physical->release(release.failed_ids.empty()&&release.unknown_ids.empty(),result);
                record(release_json(release,reason));
                record({{"event","round_finished"},{"state",result?"RESULT":"ABORTED"},{"reason",reason},{"monotonic_ns",clock.now_ns()}});
                archive.complete(active,{{"status",result?"result_confirmed":"aborted"},{"stop_reason",reason},
                    {"round_start_ns",round_start},{"round_stop_ns",clock.now_ns()},{"commands",commands},{"decisions",frames},
                    {"capture_interval_ms",distribution(intervals)},{"playing_capture_interval_ms",distribution(playing_intervals)},
                    {"recognition_ms",distribution(recognition)},{"host_residency_ms",distribution(residency)},
                    {"interval_scope","latest decisions consumed by action owner; includes skipped frames; first 100000 samples"},
                    {"source_absolute_age",nullptr},{"release_requested_ids",release.requested_ids},
                    {"release_failed_ids",release.failed_ids},{"release_unknown_ids",release.unknown_ids}},std::move(image));
                // Publish the new standby deadline before the supervisor can
                // observe active_round_start==0 after a long completed song.
                const bool score_done=score_budget&&score_budget->finish(result,clock.now_ns());
                completed=active;active=0;active_round_start=0;
                if(score_budget) {
                    std::cout<<"Score-only completed "<<score_budget->completed()<<'/'<<score_budget->limit()
                             <<(score_done?"; session stopping.\n":"; waiting for your next Play.\n")<<std::flush;
                    if(score_done){stopping=true;wake.notify();}
                } else if(full_recording||one_round) {stopping=true;wake.notify();}
            };
            archive.event(0,{{"event","session_state"},{"state","STANDBY"},{"reason","capability_preflight_passed"},{"monotonic_ns",clock.now_ns()}});
            std::cout<<"STANDBY: manually select a chart and press Play. Escape / Ctrl+C stops.\n"<<std::flush;
            while(!stopping&&!stop.stop_requested()) {
                std::shared_ptr<const Packet> p;{std::lock_guard lock(packet_mutex);p=packet;}
                if(rev!=revocation.load()) {
                    rev=revocation.load();if(active) {
                        if(candidate_step) {if(!candidate_step->suspend("source_or_supervisor_revoke"))throw std::runtime_error("candidate revoke not verified");}
                        else record(release_json(game.suspend("source_or_supervisor_revoke"),"source_or_supervisor_revoke"));
                    }
                }
                if(p&&p->scene.context.frame>seen) {
                    if(candidate_mode) {
                        const auto consumed=candidate_packet_consumed.exchange(p->packet_sequence);
                        if(p->packet_sequence>consumed+1)candidate_packet_skips+=p->packet_sequence-consumed-1;
                    }
                    seen=p->scene.context.frame;
                    if(p->status.state==PlaySessionState::fault)throw std::runtime_error(p->status.reason);
                    if(active&&p->status.round!=active)throw std::runtime_error("round transition missed by action owner");
                    if(!active&&p->status.active&&p->status.round>completed) {
                        active=p->status.round;
                        if(candidate_step) {if(!candidate_step->start(active))throw std::runtime_error("candidate round cannot reset without verified release");}
                        else game.start(active);
                        round_start=p->scene.context.capture_ns;
                        active_round_start=round_start;
                        intervals.clear();playing_intervals.clear();recognition.clear();residency.clear();
                        previous=previous_playing=0;commands=frames=0;
                        game.scheduler()->set_dispatch_guard([&]{return !stopping&&!manual_stop&&live_gate&&round_epoch.load()==active&&
                            revocation.load()==rev&&last_capture.load()&&clock.now_ns()-last_capture.load()<100'000'000;});
                    }
                    if(p->status.state!=last_state) {
                        last_state=p->status.state;
                        record({{"event","session_state"},{"state",name(last_state)},{"reason",p->status.reason},
                            {"source_frame",seen},{"capture_complete_ns",p->scene.context.capture_ns},{"monotonic_ns",clock.now_ns()}});
                        std::cout<<name(last_state)<<" round="<<p->status.round<<" "<<p->status.reason<<'\n'<<std::flush;
                    }
                    if(active) {
                        const auto& s=p->scene;const auto t=s.context.capture_ns;++frames;
                        if(previous&&intervals.size()<100000)intervals.push_back((t-previous)/1e6);previous=t;
                        if(s.playing_gate&&p->status.state==PlaySessionState::playing) {
                            if(previous_playing&&playing_intervals.size()<100000)playing_intervals.push_back((t-previous_playing)/1e6);
                            previous_playing=t;
                        }
                        if(recognition.size()<100000)recognition.push_back((s.recognition_end_ns-s.recognition_start_ns)/1e6);
                        if(residency.size()<100000)residency.push_back((clock.now_ns()-t)/1e6);
                        if(candidate_mode)archive.event(active,DiagnosticSnapshot::decision(s));else record_lazy([&]{return decision_json(s);});
                        record_lazy([&]{return json{{"event","lifecycle_ui_evidence"},{"source_frame",seen},{"capture_complete_ns",t},
                            {"hud",p->evidence.hud},{"result_labels",p->evidence.result_labels},{"result_similarity",p->evidence.result_similarity},
                            {"source_timestamp_us",p->source_time?json(*p->source_time):json(nullptr)},
                            {"source_sequence",p->source_sequence?json(*p->source_sequence):json(nullptr)}};});
                        if(!p->status.active) {
                            finish(p->status.reason,candidate_none_result_confirmed(score_only,p->status.ended,p->status.state==PlaySessionState::result,static_cast<bool>(p->result)),p->result);
                            // Result state is already released. A later fresh packet controls new Down.
                        } else {
                            const bool allow=p->allow_down&&s.context.capture_ns>invalid_through.load();
                            if(candidate_step) {
                                const auto owner_start=clock.now_ns();const bool eligible=p->candidate&&p->candidate->eligible(*p,owner_start);
                                record({{"event","candidate_packet_owner_qpc"},{"source_frame",seen},{"capture_complete_ns",s.context.capture_ns},
                                    {"packet_sequence",p->packet_sequence},{"packet_published_ns",p->published_ns},{"owner_start_ns",owner_start},{"eligible",eligible}});
                                if(!eligible) {
                                    archive.event(active,DiagnosticSnapshot::current(s));
                                    if(p->candidate)archive.event(active,DiagnosticSnapshot::batch(p->candidate->batch));
                                    if(!candidate_step->suspend("candidate_same_consumed_frame_ineligible"))throw std::runtime_error("candidate ineligible packet release not verified");}
                                else if(!candidate_step->accept(*p->candidate->frame,p->candidate->batch,s,allow))throw std::runtime_error("candidate shared owner step failed");
                                if(physical&&eligible){CandidatePhysicalFrame observed;observed.frame=p->candidate->frame->sequence;observed.same_current_frame=s.context.frame==observed.frame;observed.owner_unknown=(!candidate_step->ledger.valid()||!candidate_receipts.healthy())?1:0;
                                    for(const auto& target:s.targets){const auto line=std::find_if(s.lines.begin(),s.lines.end(),[&](const auto& v){return v.track_id==target.line_id;});const auto support=std::find_if(candidate_step->last.support.begin(),candidate_step->last.support.begin()+candidate_step->last.count,[&](const auto& v){return v.note==target.note_id;});
                                        if(target.note.kind==NoteKind::tap)++observed.opportunities[0];
                                        if(target.note.kind==NoteKind::hold&&support!=candidate_step->last.support.begin()+candidate_step->last.count){observed.opportunities[1]+=support->body&&!support->active;observed.opportunities[2]+=support->body&&support->active;observed.opportunities[3]+=support->terminal;observed.opportunities[4]+=support->body&&support->active&&line!=s.lines.end()&&line->motion_valid&&std::abs(line->angular_velocity)>0;}
                                        auto item=std::find_if(alignment.begin(),alignment.end(),[&](const auto& v){return v.note==target.note_id;});if(item==alignment.end())item=std::find_if(alignment.begin(),alignment.end(),[](const auto& v){return !v.note;});if(item!=alignment.end()&&line!=s.lines.end()){item->note=target.note_id;const bool aligned=std::abs(target.note.tangent.x*line->tangent.x+target.note.tangent.y*line->tangent.y)>=.95;observed.opportunities[5]+=item->unaligned&&aligned&&target.note.head_on_line;item->unaligned=!aligned;}
                                    }for(auto& item:alignment)if(item.note&&std::none_of(s.targets.begin(),s.targets.end(),[&](const auto& v){return v.note_id==item.note;}))item={};physical->frame(observed);}
                                record({{"event","candidate_owner_return_qpc"},{"source_frame",seen},{"owner_return_ns",clock.now_ns()},
                                    {"hook_compute_ns",candidate_step->hook_compute},{"owner_compute_ns",candidate_step->owner_compute}});
                            } else {
                            const bool was_armed=game.scheduler()->armed();
                            receipts(game.accept(s,allow));
                            if(was_armed&&!allow)record(release_json(game.scheduler()->last_release(),"current_UI_gate_lost"));
                            for(const auto& plan:game.owner()->take_accepted_plans()) {
                                record_lazy([&]{
                                    json steps=json::array();for(const auto& st:plan.steps)steps.push_back({{"phase",static_cast<int>(st.phase)},{"x",st.x},{"y",st.y},{"due_ns",st.due_ns}});
                                    return json{{"event","game_plan_accepted"},{"epoch",plan.epoch},{"intent_id",plan.intent_id},{"note_id",plan.note_id},
                                        {"revision",plan.revision},{"prefix_offset",plan.prefix_offset},{"evidence_ns",plan.evidence_ns},
                                        {"valid_until_ns",plan.valid_until_ns},{"source_frame",plan.source_frame_sequence},{"accepted_ns",clock.now_ns()},
                                        {"predicted_down_ns",plan.predicted_down_ns?json(*plan.predicted_down_ns):json(nullptr)},
                                        {"steps",steps},{"basis",plan.basis},{"real_input",true}};
                                });
                            }
                            for(auto e:game.owner()->take_plan_cancellations())record(std::move(e));
                            }
                        }
                    }
                }
                if(candidate_step) {
                    if(!candidate_step->poll())throw std::runtime_error("candidate owner poll failed");
                    json attachments=json::array();
                    for(const auto& a:candidate_step->ledger.attachments())attachments.push_back({{"note",a.note},{"intent",a.intent},
                        {"revision",a.revision},{"prefix_offset",a.prefix_offset},{"cursor",a.cursor?json(*a.cursor):json(nullptr)},
                        {"contact",a.contact},{"state",int(a.state)},{"last_contact_ns",a.last_contact},{"deadline_ns",a.deadline}});
                    record({{"event","candidate_execution_ledger"},{"source_frame",seen},{"owner_qpc_ns",clock.now_ns()},
                        {"valid",candidate_step->ledger.valid()},{"reason",candidate_step->ledger.reason()},{"attachments",attachments}});
                }
                else receipts(game.poll());
                if(game.scheduler()) {
                    if(!game.scheduler()->fault().empty())throw std::runtime_error("gameplay input fault: "+game.scheduler()->fault());
                    for(auto e:game.owner()->take_coverage_updates())record(std::move(e));
                    for(const auto& n:game.scheduler()->take_notices())record_lazy([&]{return json{{"event","scheduler_rejection"},{"intent_id",n.intent_id},{"reason",n.reason},{"monotonic_ns",n.monotonic_ns}};});
                }
                const auto due=game.scheduler()?game.scheduler()->next_due_ns():std::optional<Nanoseconds>{};
                wake.wait(due?std::clamp(*due-clock.now_ns(),Nanoseconds{0},Nanoseconds{10'000'000}):10'000'000);
            }
            if(active)finish(manual_stop?"user_stop_aborted":"runtime_stop_aborted",false,{});
            archive.event(0,{{"event","session_state"},{"state","STOPPED"},{"monotonic_ns",clock.now_ns()}});
        } catch(...) {
            fail();
            if(candidate_mode) {
                // Outer lifetime preserves the shared first-uncertain-release
                // state. No raw backend emergency retry after its first report.
                if(candidate_step)try {
                    const bool verified=candidate_step->finish();
                    if(physical)physical->release(verified,false);
                    archive.event(active,release_json(candidate_step->final_release,"candidate_fault_first_release"));
                    if(active)archive.complete(active,{{"status","aborted"},{"stop_reason","candidate_fault"},{"round_start_ns",round_start},
                        {"round_stop_ns",clock.now_ns()},{"release_verified",verified},{"release_unknown_ids",candidate_step->final_release.unknown_ids},
                        {"release_failed_ids",candidate_step->final_release.failed_ids}});
                }catch(...){}
            } else if(backend)try {
                const auto r=backend->emergency_release_all();archive.event(active,release_json(r,"fault_emergency_release"));
                if(active)archive.complete(active,{{"status","aborted"},{"stop_reason","fault"},{"round_start_ns",round_start},
                    {"round_stop_ns",clock.now_ns()},{"release_unknown_ids",r.unknown_ids},{"release_failed_ids",r.failed_ids}});
            }catch(...){}
        }
    });
    std::unique_ptr<PreviewWindow> preview;
    try {
        if(!no_preview&&config.preview_hz>0)preview=std::make_unique<PreviewWindow>(config.width,config.height);
        bool stale=false;std::uint64_t lag=0,inactive=0;Nanoseconds heartbeat=clock.now_ns();
        while(!stopping&&!manual_stop) {
            if(GetAsyncKeyState(VK_ESCAPE)&0x8000){manual_stop=true;break;}
            if(archive.faulted())throw std::runtime_error(archive.error());
            if(recording&&recording->faulted())throw std::runtime_error(recording->error());
            const auto now=clock.now_ns(),last=last_capture.load();const auto stats=capture.stats();
            if(score_budget&&score_budget->wall_expired(now))
                throw std::runtime_error("score-only session wall limit reached");
            const bool expired=last&&now-last>=100'000'000;
            if((expired&&!stale)||stats.relative_stale_drops!=lag||stats.inactive!=inactive) {
                invalid_through=last;live_gate=false;++revocation;wake.notify();lag=stats.relative_stale_drops;inactive=stats.inactive;
            }
            stale=expired;
            const auto active_start=active_round_start.load();
            if(score_budget&&!active_start&&score_budget->waiting_expired(now))
                throw std::runtime_error("score-only waiting-for-Play limit reached");
            if(watchdog&&active_start&&now-active_start>=watchdog)
                throw std::runtime_error("round_watchdog_aborted");
            if(now-heartbeat>=60'000'000'000LL) {
                archive.event(0,{{"event","session_health"},{"monotonic_ns",now},{"published",latest.counters().published},{"capture_stale",stale}});heartbeat=now;
            }
            if(preview) {
                if(!preview->pump()){manual_stop=true;break;}
                std::shared_ptr<const Frame> view;std::shared_ptr<const Packet> p;
                {std::lock_guard lock(packet_mutex);view=display;p=packet;}
                if(view){preview->draw(*view);preview->title(std::string("PAS manual / ")+(p?name(p->status.state):"STANDBY")+
                    (p&&p->status.active&&!live_gate?" / touch paused":""));}
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }catch(...){fail();}
    live_gate=false;stopping=true;++revocation;wake.notify();latest.close();capture.cancel();
    action_worker.request_stop();perception_worker.request_stop();capture_worker.request_stop();
    action_worker.join();perception_worker.join();capture_worker.join();archive.close();
    if(clips)clips->close();
    if(recording)recording->close();
    std::exception_ptr error;{std::lock_guard lock(fault_mutex);error=fault;}
    if(recording&&recording->faulted()&&!error)error=std::make_exception_ptr(std::runtime_error(recording->error()));
    if(score_only){
        if(error)std::rethrow_exception(error);
        if(archive.faulted())throw std::runtime_error(archive.error());
        std::cout<<"Score-only session stopped; no gameplay files saved.\n";
        return;
    }
    json summary={{"state",error||archive.faulted()?"FAULT":"STOPPED"},{"run_dir",std::filesystem::absolute(root).string()},
        {"published",latest.counters().published},{"pool_drops",latest.counters().pool_drops},
        {"consumer_skips",latest.counters().consumer_skips},{"journal_peak_queue",archive.peak_queue()},
        {"automatic_play_enabled",false},{"pixel_clips",clips?clips->summary():json{{"enabled",false}}},
        {"full_recording",recording?recording->summary():json{{"enabled",false}}}};
    if(candidate_mode)summary["candidate_packet_counters"]={{"published",candidate_packet_published.load()},
        {"overwritten",candidate_packet_overwritten.load()},{"consumed_latest_sequence",candidate_packet_consumed.load()},
        {"consumer_skips",candidate_packet_skips.load()},{"physical_pool_drops",latest.counters().pool_drops}};
    if(error)try{std::rethrow_exception(error);}catch(const std::exception& e){summary["fault"]=e.what();}
    if(archive.faulted())summary["archive_first_error"]=archive.error();
    std::ofstream file(root/"summary.json");file<<summary.dump(2)<<'\n';file.close();
    if(!file)throw std::runtime_error("session summary write failed");
    std::cout<<summary.dump(2)<<'\n';if(error)std::rethrow_exception(error);
    if(archive.faulted())throw std::runtime_error(archive.error());
}
void run_manual_session(const std::string& config_path,const std::string& capability_path,bool no_preview,Nanoseconds watchdog,bool pixel_clips,bool full_recording,bool one_round) {
    run_manual_session_impl(config_path,capability_path,no_preview,watchdog,pixel_clips,full_recording,one_round,ManualVariant::legacy);
}
void run_manual_score_only_session(const std::string& config_path,const std::string& capability_path,Nanoseconds watchdog,unsigned rounds) {
    run_manual_session_impl(config_path,capability_path,true,watchdog,false,false,rounds==1,
                            ManualVariant::legacy,SessionArchive::Persistence::none,nullptr,rounds);
}
void run_current_candidate_manual_session(const CandidateLiveAdmission& admission,const std::string& config_path,const std::string& capability_path,bool no_preview,Nanoseconds watchdog,bool pixel_clips,bool full_recording,bool one_round) {
    enter_current_candidate_manual_session(admission,[&]{
        run_manual_session_impl(config_path,capability_path,no_preview,watchdog,pixel_clips,full_recording,one_round,ManualVariant::current_candidate);
    });
}
void run_current_candidate_score_only_session(const CandidateLiveAdmission& admission,const std::string& config_path,const std::string& capability_path) {
    enter_current_candidate_score_only_session(admission,config_path,capability_path,[&]{
        run_manual_session_impl(config_path,capability_path,true,600'000'000'000LL,
            false,false,true,ManualVariant::current_candidate,SessionArchive::Persistence::none);
    });
}
void run_current_candidate_physical_qualification(const CandidatePhysicalPermit& permit,const std::string& config_path,const std::string& capability_path,CandidatePhysicalAggregate& aggregate){
    enter_candidate_physical_qualification(permit,config_path,capability_path,[&]{run_manual_session_impl(config_path,capability_path,true,600'000'000'000LL,false,false,true,ManualVariant::current_candidate,SessionArchive::Persistence::none,&aggregate);});
}

}
