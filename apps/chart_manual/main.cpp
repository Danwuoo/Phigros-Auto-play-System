#include "pas/adb.hpp"
#include "pas/emulator.hpp"
#include "rhythm/chart.hpp"
#include "rhythm/manual_lifecycle.hpp"
#include "rhythm/screen_state_io.hpp"
#include "foreground_watch.hpp"
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
using Json=nlohmann::json;
constexpr const char* package="com.PigeonGames.Phigros";
constexpr const char* usage=
    "chart_manual shadow SERIAL AVD CALIBRATION.json NEW_DIR --observe-only [--rounds 1..20] "
    "[--watchdog-seconds 1..600] [--session-seconds 1..3600]\n"
    "Read-only manual-mode: select the song and press Play yourself; Esc, Ctrl+C, or NEW_DIR/STOP stops.\n"
    "No touch backend, audio capture, song clock, automatic Play, or gameplay arming.\n";
std::atomic<bool> interrupted{false};
BOOL WINAPI stop_handler(DWORD signal) {
    if(signal==CTRL_C_EVENT||signal==CTRL_BREAK_EVENT||signal==CTRL_CLOSE_EVENT||signal==CTRL_SHUTDOWN_EVENT) {
        interrupted.store(true);return TRUE;
    }
    return FALSE;
}
struct StopHandler {
    StopHandler() {interrupted=false;if(!SetConsoleCtrlHandler(stop_handler,TRUE))throw std::runtime_error("stop handler failed");}
    ~StopHandler() {SetConsoleCtrlHandler(stop_handler,FALSE);}
};
int integer(const char* input,int maximum) {
    const std::string value=input;int result=0;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||result<1||result>maximum)
        throw std::invalid_argument("invalid bounded integer option");
    return result;
}
std::string adb_text(const std::filesystem::path& adb,const std::string& serial,const std::vector<std::string>& tail) {
    std::vector<std::string> args{"-s",serial};args.insert(args.end(),tail.begin(),tail.end());
    const auto raw=pas::adb_call(adb,args,2000);return {raw.begin(),raw.end()};
}
void check_foreground(const std::filesystem::path& adb,const std::string& serial) {
    const auto report=adb_text(adb,serial,{"shell","dumpsys","window"});
    const auto begin=report.find("mCurrentFocus=");
    if(begin==std::string::npos||report.substr(begin,report.find('\n',begin)-begin).find(std::string(package)+"/")==std::string::npos)
        throw std::runtime_error("Phigros is not foreground");
}
std::string field(const std::string& text,const std::string& key) {
    const auto position=text.find(key);
    if(position==std::string::npos)return {};
    const auto begin=position+key.size();return text.substr(begin,text.find_first_of(" \t\r\n",begin)-begin);
}
std::string executable_sha256() {
    wchar_t module[32768]{};const auto count=GetModuleFileNameW(nullptr,module,32768);
    if(!count||count==32768)throw std::runtime_error("executable path unavailable");
    const std::filesystem::path path(module);
    if(std::filesystem::file_size(path)>256*1024*1024)throw std::runtime_error("executable hash budget");
    std::ifstream stream(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(stream)),{});
    if(!stream||bytes.empty()||bytes.size()>256*1024*1024)throw std::runtime_error("executable hash read failed");
    return rhythm::chart_text_sha256(bytes);
}
Json status_json(const rhythm::ManualStatus& status) {
    return {{"state",rhythm::manual_state_name(status.state)},{"round",status.round},
        {"completed_rounds",status.completed_rounds},{"active",status.active},{"new_round",status.new_round},
        {"ended",status.ended},{"result_confirmed",status.result_confirmed},{"session_done",status.session_done},
        {"ui_playing",status.ui_playing},{"reason",status.reason},{"observation_sequence",status.observation_sequence},
        {"timestamp_kind",rhythm::manual_timestamp_kind_name(status.timestamp_kind)},
        {"observed_host_ns",status.observed_host_ns},{"received_host_ns",status.received_host_ns},
        {"capture_timestamp_known",status.capture_timestamp_known},{"evidence_id",status.evidence_id}};
}
void write_json(const std::filesystem::path& path,const Json& value) {
    std::ofstream stream(path,std::ios::binary);stream<<value.dump(2)<<'\n';stream.close();
    if(!stream)throw std::runtime_error("manifest write failed");
}
struct Journal {
    std::ofstream stream;
    std::size_t bytes=0,rows=0;
    explicit Journal(const std::filesystem::path& path):stream(path,std::ios::binary) {
        if(!stream)throw std::runtime_error("cannot create journal");
    }
    void append(const Json& row) {
        const auto text=row.dump()+"\n";
        if(rows>=250000||text.size()>64*1024*1024-bytes)throw std::runtime_error("journal budget exceeded");
        stream<<text;stream.flush();if(!stream)throw std::runtime_error("journal write failed");
        bytes+=text.size();++rows;
    }
};
int shadow(int argc,char** argv) {
    if(argc<7||std::string(argv[1])!="shadow"||(argc-7)%2)throw std::invalid_argument(usage);
    const std::string serial=argv[2],avd=argv[3];
    if(serial.empty()||avd.empty())throw std::invalid_argument("explicit serial and AVD required");
    unsigned rounds=1;int watchdog=180,seconds=600;
    bool rounds_set=false,watchdog_set=false,seconds_set=false;
    for(int i=7;i<argc;i+=2) {
        const std::string key=argv[i];
        if(key=="--rounds"&&!rounds_set) {rounds=static_cast<unsigned>(integer(argv[i+1],20));rounds_set=true;}
        else if(key=="--watchdog-seconds"&&!watchdog_set) {watchdog=integer(argv[i+1],600);watchdog_set=true;}
        else if(key=="--session-seconds"&&!seconds_set) {seconds=integer(argv[i+1],3600);seconds_set=true;}
        else throw std::invalid_argument("unknown or repeated option");
    }
    // The spelling is deliberately explicit; there is no implicit device/default live mode.
    if(std::string(argv[6])!="--observe-only")throw std::invalid_argument("explicit --observe-only is required after NEW_DIR");
    const auto calibration=rhythm::load_screen_state_calibration(argv[4]);
    if(calibration.width!=1280||calibration.height!=720)throw std::invalid_argument("Phigros profile requires 1280x720 calibration");
    const std::filesystem::path output=std::filesystem::absolute(argv[5]);
    if(std::filesystem::exists(output))throw std::invalid_argument("output must not exist");
    const auto binary_hash=executable_sha256();
    const auto adb=pas::find_adb();
    const auto avd_reply=adb_text(adb,serial,{"emu","avd","name"});
    if(avd_reply.substr(0,avd_reply.find_first_of("\r\n"))!=avd)throw std::runtime_error("selected AVD mismatch");
    const auto package_reply=adb_text(adb,serial,{"shell","dumpsys","package",package});
    if(field(package_reply,"versionName=")!="4.0.1"||field(package_reply,"versionCode=")!="157")
        throw std::runtime_error("Phigros version must be 4.0.1 / 157");
    pas::HostClock clock;
    const auto foreground_started=clock.now_ns();
    check_foreground(adb,serial);
    const auto foreground_returned=clock.now_ns();
    rhythm::manual_ui::ForegroundWatch foreground_watch(
        [adb,serial]{check_foreground(adb,serial);},[clock]{return clock.now_ns();},
        {foreground_started,foreground_returned});
    const auto endpoint=pas::discover_endpoint(serial);
    pas::CaptureOptions capture_options;capture_options.width=1280;capture_options.height=720;capture_options.source_rotation=1;
    capture_options.max_relative_lag_ns=250'000'000;
    pas::GrpcCapture capture(clock,endpoint,capture_options);
    rhythm::ManualConfig config;config.round_limit=rounds;
    config.round_watchdog_ns=static_cast<rhythm::Ns>(watchdog)*1'000'000'000;
    rhythm::ManualLifecycle lifecycle(config);
    if(!std::filesystem::create_directory(output))throw std::runtime_error("cannot create output");
    Journal journal(output/"events.jsonl");
    Json manifest={{"schema","pas.chart-manual-shadow.v1"},{"mode","manual-mode-shadow"},
        {"serial",serial},{"avd",avd},{"package",package},{"version_name","4.0.1"},{"version_code",157},
        {"executable_sha256",binary_hash},{"calibration_sha256",calibration.dataset_sha256},
        {"package_report_sha256",rhythm::chart_text_sha256(package_reply)},
        {"automatic_play_enabled",false},{"input_enabled",false},{"audio_enabled",false},
        {"song_identity_verified",false},{"timing_qualified",false},{"gameplay_qualified",false},
        {"timestamp_policy","host receipt for delivered UI observations; server wall stamp before readback unmapped; source pixel age unknown"},
        {"source_pixel_age_qualified",false},
        {"source_sequence_semantics","stream event counter, not rendered-frame identity"},
        {"input_lead_ns",nullptr},{"round_limit",rounds},{"round_watchdog_seconds",watchdog},
        {"session_wall_seconds",seconds},{"geometry",{{"width",1280},{"height",720},{"source_rotation",1}}},
        {"transport",pas::grpc_transport_manifest(capture_options.grpc_read_chunk_kib)},
        {"legacy_source_commit","2d479e17fa1566c089dce019cbb11eb883ba47fd"},
        {"lifecycle_scope","manual selection/Play; distinct delivered UI observation debounce; pause suspends; result latch; bounded rounds; explicit stop"}};
    manifest["foreground_watch_policy"]={{"poll_interval_ns",1'000'000'000},{"adb_timeout_ms",2000},
        {"stale_after_ns",4'000'000'000LL},{"stop_wait_ms",2500},
        {"deadline_basis","last successful foreground request start; separate from unchanged UI 100ms age"},
        {"preflight_started_ns",foreground_started},{"preflight_returned_ns",foreground_returned}};
    write_json(output/"plan.json",manifest);
    StopHandler signal_guard;
    pas::LatestFrame latest(1280,720,3,&clock);
    std::atomic<bool> capture_finished{false};std::mutex error_mutex;std::exception_ptr capture_error;
    std::jthread worker([&](std::stop_token stop) {
        try {
            capture.stream(stop,[&](pas::Frame&& frame) {
                // These identifiers describe this one stream/geometry instance, not a song epoch.
                frame.generation=1;frame.geometry_version=1;
                latest.publish(frame.rgb.data(),frame.rgb.size(),frame);
            });
        }catch(...) {std::lock_guard lock(error_mutex);capture_error=std::current_exception();}
        capture_finished=true;latest.close();
    });
    const auto started=clock.now_ns();const auto deadline=started+static_cast<rhythm::Ns>(seconds)*1'000'000'000;
    auto status=lifecycle.status(started);std::string failure,exit_reason,printed;
    std::uint64_t consumed=0;
    const auto foreground_healthy=[&] {
        const auto state=foreground_watch.status();
        if(!state.healthy)throw std::runtime_error(state.reason.empty()?"foreground_watch_unavailable":state.reason);
    };
    std::cout<<"MANUAL SHADOW: waiting for your Play. No touch or audio. Esc / Ctrl+C / STOP file ends observation.\n"<<std::flush;
    try {
        while(!status.session_done) {
            auto now=clock.now_ns();
            if(interrupted.load()||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||std::filesystem::exists(output/"STOP")) {
                lifecycle.request_user_stop();exit_reason="user_stop";
            } else if(now>=deadline) {lifecycle.request_user_stop();exit_reason="session_wall_limit_aborted";}
            // Expiry and stop are serviced before processing another delivered observation.
            status=lifecycle.poll(now);
            if(status.session_done)break;
            if(capture_finished.load()) {
                std::lock_guard lock(error_mutex);
                if(capture_error)std::rethrow_exception(capture_error);
                throw std::runtime_error("capture stream ended");
            }
            foreground_healthy();
            const auto frame=latest.read_after(consumed,10'000'000);
            if(frame) {
                consumed=frame->sequence;
                const auto classified=rhythm::classify_screen_state({frame->width,frame->height,frame->stride,frame->rgb},calibration.templates);
                rhythm::ManualObservation observation;
                observation.sequence=frame->sequence;observation.generation=frame->generation;observation.geometry_version=frame->geometry_version;
                observation.width=frame->width;observation.height=frame->height;observation.rotation=frame->source_rotation;
                observation.timestamp_kind=rhythm::ManualTimestampKind::arrival_only;
                observation.received_host_ns=frame->capture_complete_ns;observation.source_valid=frame->source_valid;
                observation.evidence_id="grpc-stream-1/frame-"+std::to_string(frame->sequence)+"/source-"+
                    (frame->source_sequence?std::to_string(*frame->source_sequence):"unknown");
                observation.hud=classified.state==rhythm::ObservedGameState::playing;
                observation.pause=classified.state==rhythm::ObservedGameState::paused;
                observation.result=classified.state==rhythm::ObservedGameState::result;
                foreground_healthy();
                now=clock.now_ns();status=lifecycle.observe(observation,now);
                Json scores=Json::array();
                for(const auto& score:classified.scores)scores.push_back({{"calibration_id",score.calibration_id},
                    {"all_anchors_match",score.all_anchors_match},{"worst_anchor_mean_error",score.worst_anchor_mean_error}});
                journal.append({{"event","frame_state"},{"host_processed_ns",now},{"source_sequence",frame->source_sequence?Json(*frame->source_sequence):Json(nullptr)},
                    {"source_timestamp_us",frame->source_timestamp_us?Json(*frame->source_timestamp_us):Json(nullptr)},
                    {"source_clock","emulator_unix_us_unmapped"},{"status",status_json(status)},
                    {"classification_reason",classified.reason},{"scores",std::move(scores)}});
                if(status.result_confirmed&&status.ended)
                    pas::write_diagnostic_png(output/("result-"+std::to_string(status.round)+".png"),*frame);
            }
            const auto next=std::string(rhythm::manual_state_name(status.state))+" round="+std::to_string(status.round)+" "+status.reason;
            if(next!=printed) {std::cout<<next<<'\n'<<std::flush;printed=next;}
        }
    }catch(const std::exception& error) {failure=error.what();lifecycle.request_user_stop();status=lifecycle.poll(clock.now_ns());}
    const bool foreground_joined=foreground_watch.stop();
    const auto foreground_final=foreground_watch.status();
    if(!foreground_joined&&failure.empty())failure="foreground_worker_stop_timeout";
    if(foreground_final.failed&&failure.empty())failure=foreground_final.reason;
    worker.request_stop();capture.cancel();latest.close();worker.join();
    if(status.state==rhythm::ManualState::fault||status.state==rhythm::ManualState::aborted)failure=status.reason;
    const auto counters=latest.counters();const auto stats=capture.stats();
    manifest["status"]=status_json(status);manifest["failure"]=failure.empty()?Json(nullptr):Json(failure);
    manifest["exit_reason"]=exit_reason.empty()?status.reason:exit_reason;
    manifest["foreground_watch_final"]={{"failed",foreground_final.failed},{"stale",foreground_final.stale},
        {"stop_joined",foreground_joined},{"successful_checks",foreground_final.successful_checks},
        {"successful_started_ns",foreground_final.successful_started_ns},
        {"successful_returned_ns",foreground_final.successful_returned_ns},{"reason",foreground_final.reason}};
    manifest["start_host_ns"]=started;manifest["stop_host_ns"]=clock.now_ns();
    manifest["journal_rows"]=journal.rows;manifest["journal_bytes"]=journal.bytes;
    manifest["capture"]={{"valid",stats.valid},{"source_gaps",stats.source_gaps},{"relative_stale_drops",stats.relative_stale_drops},
        {"inactive",stats.inactive},{"invalid",stats.invalid},{"published",counters.published},
        {"pool_drops",counters.pool_drops},{"consumer_skips",counters.consumer_skips}};
    write_json(output/"manifest.json",manifest);
    std::cout<<manifest.dump(2)<<'\n';
    return !failure.empty()?2:exit_reason=="session_wall_limit_aborted"?3:0;
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string(argv[1])=="--help") {std::cout<<usage;return 0;}
        return shadow(argc,argv);
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
