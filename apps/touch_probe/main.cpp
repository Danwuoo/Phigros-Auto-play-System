#include "pas/adb.hpp"
#include "pas/emulator.hpp"
#include "pas/touch_evidence.hpp"
#include "fixture_path.hpp"
#include "rhythm/chart_memory.hpp"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pas;
using json = nlohmann::json;
constexpr const char* package = "org.pas.touchfixture.evidence.cpp";
std::atomic<bool> interrupted{false};
BOOL WINAPI signal_stop(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        interrupted.store(true); return TRUE;
    }
    return FALSE;
}
int integer(const char* text) {
    const std::string value = text; int result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::invalid_argument("invalid integer");
    return result;
}
std::string adb_text(const std::filesystem::path& adb, const std::string& serial,
                     const std::vector<std::string>& tail, int timeout = 2000) {
    std::vector<std::string> args{"-s", serial}; args.insert(args.end(), tail.begin(), tail.end());
    const auto bytes = adb_call(adb, args, timeout); return {bytes.begin(), bytes.end()};
}
void foreground(const std::filesystem::path& adb, const std::string& serial) {
    const auto report = adb_text(adb, serial, {"shell", "dumpsys", "window"});
    const auto first = report.find("mCurrentFocus=");
    const auto end = first == std::string::npos ? first : report.find('\n', first);
    if (first == std::string::npos ||
        report.substr(first, end - first).find(std::string(package) + "/") == std::string::npos)
        throw std::runtime_error("timestamp touch fixture is not foreground");
}
std::uint32_t sequence(const Frame& frame) {
    if (frame.width < 320 || frame.height < 660 || frame.stride != frame.width * 3 ||
        frame.rgb.size() != static_cast<std::size_t>(frame.stride) * frame.height)
        throw std::runtime_error("invalid fixture frame geometry");
    const auto pos = static_cast<std::size_t>(50) * frame.stride + 84 * 3;
    return (static_cast<std::uint32_t>(frame.rgb[pos]) << 16) |
           (static_cast<std::uint32_t>(frame.rgb[pos + 1]) << 8) | frame.rgb[pos + 2];
}
json evidence_json(const TouchEvidence& evidence) {
    json samples = json::array();
    for (const auto& sample : evidence.samples)
        samples.push_back({{"sequence", sample.sequence}, {"phase", sample.phase},
            {"android_id", sample.android_id}, {"x", sample.x}, {"y", sample.y}});
    return {{"sequence", evidence.sequence}, {"active", evidence.active},
        {"downs", evidence.downs}, {"ups", evidence.ups}, {"moves", evidence.moves},
        {"cancels", evidence.cancels}, {"complete", evidence.complete},
        {"visual_ring_overwrites", evidence.overflow_total}, {"error", evidence.error}, {"samples", samples}};
}
json release_json(const ReleaseReport& report) {
    return {{"requested", report.requested_ids}, {"failed", report.failed_ids}, {"unknown", report.unknown_ids},
        {"host_start_ns", report.start_ns}, {"host_return_ns", report.return_ns},
        {"effect_verified_by_rpc", false}};
}
struct ProbePlan {
    std::string kind;
    int capacity = 0, width = 0, height = 0;
    bool batch = false, fault = false, cancel = false;
    std::vector<TouchCommand> down, move, up;
};
ProbePlan make_plan(std::string kind, int capacity, int width, int height) {
    if (capacity < 1 || capacity > 10 || width < 320 || width > 4096 || height < 660 || height > 4096)
        throw std::invalid_argument("fixture bounds: contacts 1..10, width 320..4096, height 660..4096");
    if (kind != "single" && kind != "capacity" && kind != "batch" && kind != "cancel" && kind != "transport-loss")
        throw std::invalid_argument("case must be single, capacity, batch, cancel, or transport-loss");
    const int count = kind == "single" ? 1 : capacity;
    ProbePlan result{kind, capacity, width, height, kind == "batch", kind == "transport-loss", kind == "cancel"};
    for (int id = 0; id < count; ++id) {
        const int x = width * (id + 1) / (count + 1), y = height * 3 / 4;
        result.down.push_back({static_cast<std::uint64_t>(id + 1), id, Phase::down,
            static_cast<double>(x), static_cast<double>(y), 0, 0});
        result.up.push_back({static_cast<std::uint64_t>(id + 1), id, Phase::up,
            static_cast<double>(x), static_cast<double>(y), 0, 0});
    }
    auto motion = result.down.front(); motion.phase = Phase::move; motion.y -= 60;
    result.move.push_back(motion); result.up.front().y = motion.y;
    return result;
}
json plan_json(const ProbePlan& plan) {
    json actions = json::array();
    for (const auto& commands : {plan.down, plan.move, plan.up}) for (const auto& command : commands)
        actions.push_back({{"contact_id", command.contact_id}, {"phase", static_cast<int>(command.phase)},
            {"x", command.x}, {"y", command.y}});
    return {{"schema", "pas.touch-probe.v1"}, {"fixture_package", package}, {"case", plan.kind},
        {"capacity_requested", plan.capacity}, {"contacts_in_case", plan.down.size()},
        {"width", plan.width}, {"height", plan.height}, {"rotation", 0}, {"batch_down_up", plan.batch},
        {"normal_actions", actions}, {"stages", "DOWN; verify active; MOVE id0; verify; UP or release; verify zero"},
        {"max_session_ns", 15'000'000'000LL}, {"rpc_timeout_ms", 100}, {"calibrates_game_input_lead", false}};
}
} // namespace

int main(int argc, char** argv) {
    try {
        const bool run = argc > 1 && std::string(argv[1]) == "run";
        if ((!run && (argc != 6 || std::string(argv[1]) != "plan")) || (run && argc != 12))
            throw std::invalid_argument(
                "touch_probe plan CASE CAPACITY WIDTH HEIGHT\n"
                "touch_probe run SERIAL AVD FIXTURE_SHA CASE CAPACITY WIDTH HEIGHT NEW_DIR PROFILE --fixture-input|--inspect-only\n"
                "PROFILE: portrait-r0, landscape-r1, sensor-portrait-ccw90, or sensor-portrait-r0-ccw90. WIDTH/HEIGHT are normalized fixture dimensions.\n"
                "Plan is offline. Run never installs/launches apps; exact fixture must already be foreground.");
        const auto plan = make_plan(argv[run ? 5 : 2], integer(argv[run ? 6 : 3]),
            integer(argv[run ? 7 : 4]), integer(argv[run ? 8 : 5]));
        if (!run) { std::cout << plan_json(plan).dump(2) << '\n'; return 0; }
        const bool inspect_only = std::string(argv[11]) == "--inspect-only";
        if (!inspect_only && std::string(argv[11]) != "--fixture-input")
            throw std::invalid_argument("explicit --fixture-input or --inspect-only required");
        const std::string profile = argv[10];
        if (profile != "portrait-r0" && profile != "landscape-r1" && profile != "sensor-portrait-ccw90" && profile != "sensor-portrait-r0-ccw90")
            throw std::invalid_argument("unknown geometry profile");
        const std::string serial = argv[2], avd = argv[3], hash = argv[4];
        if (avd.empty() || !rhythm::sha256_text(hash)) throw std::invalid_argument("AVD and fixture SHA required");
        const std::filesystem::path output = argv[9];
        if (std::filesystem::exists(output)) throw std::invalid_argument("output must not exist");
        const auto adb = find_adb();
        const auto name = adb_text(adb, serial, {"emu", "avd", "name"});
        if (name.substr(0, name.find_first_of("\r\n")) != avd) throw std::runtime_error("selected AVD mismatch");
        if (installed_apk_sha256(adb, serial, package) != hash) throw std::runtime_error("installed fixture SHA mismatch");
        foreground(adb, serial);
        const auto endpoint = discover_endpoint(serial);
        HostClock clock;
        CaptureOptions options; options.width = plan.width; options.height = plan.height; options.source_rotation = 0;
        const bool landscape = profile != "portrait-r0";
        const int touch_width = landscape ? plan.height : plan.width;
        const int touch_height = landscape ? plan.width : plan.height;
        const int touch_rotation = landscape ? 90 : 0;
        if (landscape) options.source_rotation = 1;
        if (profile == "sensor-portrait-ccw90" || profile == "sensor-portrait-r0-ccw90") {
            options.width = plan.height; options.height = plan.width; options.rotate_ccw = true;
            if (profile == "sensor-portrait-r0-ccw90") options.source_rotation = 0;
        }
        GrpcCapture reader(clock, endpoint, options);
        auto first_frame = reader.snapshot(std::chrono::milliseconds(1000));
        if (first_frame.width != plan.width || first_frame.height != plan.height)
            throw std::runtime_error("normalized fixture geometry mismatch");
        const auto baseline = decode_touch_evidence(first_frame, sequence(first_frame));
        if (!baseline.complete || baseline.active) throw std::runtime_error("fixture signature invalid or contacts already active");
        if (!std::filesystem::create_directory(output)) throw std::runtime_error("cannot create output");
        std::ofstream log(output / "events.jsonl", std::ios::binary);
        if (!log) throw std::runtime_error("cannot open log");
        json manifest = plan_json(plan);
        manifest["serial"] = serial; manifest["avd"] = avd; manifest["fixture_apk_sha256"] = hash;
        manifest["transport"] = grpc_transport_manifest(default_grpc_read_chunk_kib);
        manifest["input_enabled"] = !inspect_only; manifest["live_game_qualified"] = false;
        manifest["geometry"] = {{"profile", profile}, {"raw_width", options.width}, {"raw_height", options.height},
            {"raw_rotation", options.source_rotation}, {"normalize_ccw90", options.rotate_ccw},
            {"normalized_width", plan.width}, {"normalized_height", plan.height},
            {"touch_width", touch_width}, {"touch_height", touch_height}, {"touch_rotation", touch_rotation},
            {"status", "signature checked; touch mapping still requires actual path evidence"}};
        manifest["clock"] = "absolute Windows QPC ns; Android event timestamps remain separate";
        manifest["baseline"] = evidence_json(baseline);
        manifest["input_lead_ns"] = nullptr; manifest["android_jsonl_verified"] = false;
        manifest["visual_path_adapter"] = fixture_probe::path_adapter_version;
        std::ofstream planned(output / "plan.json", std::ios::binary); planned << manifest.dump(2) << '\n'; planned.close();
        if (!planned) throw std::runtime_error("cannot write plan before input");
        write_diagnostic_png(output / "before.png", first_frame);
        if (inspect_only) {
            manifest["status"] = "fixture_capture_signature_checked_no_input";
            std::ofstream summary(output / "manifest.json", std::ios::binary);
            summary << manifest.dump(2) << '\n'; summary.close();
            if (!summary) throw std::runtime_error("inspection manifest write failed");
            std::cout << manifest.dump(2) << '\n'; return 0;
        }
        PixelCoordinateMap mapping(plan.width, plan.height, touch_width, touch_height, touch_rotation);
        GrpcTouch touch(clock, endpoint, mapping, touch_width, touch_height, plan.capacity, std::chrono::milliseconds(100));
        const auto start = clock.now_ns(), deadline = start + 15'000'000'000LL;
        std::vector<ExpectedPointer> expected;
        bool down_started = false, path_verified = false, zero_verified = false, transport_fault_observed = false;
        bool release_rpc_known = false, pre_fault_path_verified = false;
        std::string failure;
        SetConsoleCtrlHandler(signal_stop, TRUE);
        const auto stopped = [&] { return interrupted.load() || std::filesystem::exists(output / "STOP") || clock.now_ns() >= deadline; };
        const auto guard = [&] {
            if (stopped()) throw std::runtime_error("user_stop_or_session_deadline");
            if (!log) throw std::runtime_error("evidence_write_failed");
        };
        const auto receipt = [&](const TouchReceipt& value) {
            log << json{{"event", "rpc"}, {"phase", static_cast<int>(value.command.phase)},
                {"id", value.command.contact_id}, {"x", value.command.x}, {"y", value.command.y},
                {"host_scheduled_ns", value.command.scheduled_ns}, {"host_start_ns", value.injection_start_ns},
                {"host_return_ns", value.injection_return_ns}, {"success", value.success}, {"reason", value.reason},
                {"effect_verified_by_rpc", false}}.dump() << '\n';
            if (!value.success) throw std::runtime_error("touch_rpc_result_unknown");
        };
        const auto dispatch = [&](std::vector<TouchCommand> commands, bool batch) {
            guard(); foreground(adb, serial); guard();
            for (auto& command : commands) { command.scheduled_ns = clock.now_ns(); command.source_frame_sequence = first_frame.sequence; }
            if (batch) {
                // One RPC request; Android simultaneity is independently observed, never inferred.
                for (const auto& value : touch.inject_batch(commands)) receipt(value);
                for (const auto& command : commands)
                    expected.push_back({command.phase == Phase::down ? 1 : command.phase == Phase::move ? 2 : 3,
                        command.contact_id, static_cast<int>(command.x), static_cast<int>(command.y)});
            } else for (const auto& command : commands) {
                guard();
                if (command.phase != Phase::up) { foreground(adb, serial); guard(); }
                receipt(touch.inject(command));
                expected.push_back({command.phase == Phase::down ? 1 : command.phase == Phase::move ? 2 : 3,
                    command.contact_id, static_cast<int>(command.x), static_cast<int>(command.y)});
            }
        };
        const auto snapshot = [&](const char* stage, bool honor_stop) {
            if (honor_stop) guard();
            foreground(adb, serial);
            const auto frame = reader.snapshot(std::chrono::milliseconds(1000));
            const auto observed = decode_touch_evidence(frame, baseline.sequence);
            log << json{{"event", "fixture_snapshot"}, {"stage", stage}, {"host_arrival_ns", frame.capture_complete_ns},
                {"source_timestamp_us", frame.source_timestamp_us ? json(*frame.source_timestamp_us) : json(nullptr)},
                {"source_clock", "emulator source metadata; not QPC"}, {"evidence", evidence_json(observed)}}.dump() << '\n';
            if (!observed.complete) throw std::runtime_error("fixture_evidence_incomplete");
            return observed;
        };
        const auto wait_ms = [&](int milliseconds) {
            const auto end = clock.now_ns() + static_cast<Nanoseconds>(milliseconds) * 1'000'000;
            while (clock.now_ns() < end) { guard(); Sleep(2); }
        };
        try {
            down_started = true; dispatch(plan.down, plan.batch); wait_ms(80);
            const auto middle = snapshot("held", true);
            if (middle.active != plan.down.size() || middle.downs - baseline.downs != plan.down.size())
                throw std::runtime_error("requested_capacity_not_observed");
            dispatch(plan.move, false); wait_ms(80);
            const auto moved = snapshot("moved", true);
            if (moved.active != plan.down.size()) throw std::runtime_error("contact_lost_during_move");
            std::vector<int> stationary_before_release;
            for (std::size_t id = 1; id < plan.down.size(); ++id) stationary_before_release.push_back(static_cast<int>(id));
            const auto held_path = fixture_probe::verify_fixture_path(moved, expected, 3,
                static_cast<int>(plan.down.size()), stationary_before_release);
            manifest["pre_release_redundant_stationary_moves"] = held_path.redundant_stationary_moves;
            manifest["pre_release_path_result"] = held_path.path.reason;
            pre_fault_path_verified = held_path.path.passed;
            if (!pre_fault_path_verified) throw std::runtime_error("held_contact_path_unverified");
            if (plan.fault) {
                guard(); touch.simulate_transport_loss_for_fixture_test();
                auto command = plan.up.front(); command.scheduled_ns = clock.now_ns();
                const auto failed = touch.inject(command);
                log << json{{"event", "simulated_client_transport_loss"}, {"rpc_success", failed.success},
                    {"host_start_ns", failed.injection_start_ns}, {"host_return_ns", failed.injection_return_ns}}.dump() << '\n';
                transport_fault_observed = !failed.success;
                if (!transport_fault_observed) throw std::runtime_error("transport_loss_not_observed");
                const auto failed_release = touch.release_all();
                log << json{{"event", "faulted_channel_release"}, {"report", release_json(failed_release)}}.dump() << '\n';
                if (failed_release.unknown_ids.empty()) throw std::runtime_error("unknown_release_was_not_retained");
            } else if (!plan.cancel) dispatch(plan.up, plan.batch);
            if (plan.cancel || plan.fault) for (const auto& command : plan.up)
                expected.push_back({3, command.contact_id, static_cast<int>(command.x), static_cast<int>(command.y)});
        } catch (const std::exception& error) { failure = error.what(); }
        // Every entered-input path attempts release, even stop/write failure/foreground change.
        // A fresh recovery backend is necessary for the intentional client transport-loss case.
        try {
            ReleaseReport release;
            if (plan.fault) {
                GrpcTouch recovery(clock, endpoint, mapping, touch_width, touch_height, plan.capacity, std::chrono::milliseconds(100));
                release = recovery.emergency_release_all();
            } else release = touch.release_all();
            release_rpc_known = release.failed_ids.empty() && release.unknown_ids.empty();
            manifest["release"] = release_json(release);
            Sleep(80);
            const auto final = snapshot("after_release", false);
            manifest["final_evidence"] = evidence_json(final);
            zero_verified = final.complete && final.active == 0;
            std::vector<int> stationary;
            for (std::size_t id = 1; id < plan.down.size(); ++id) stationary.push_back(static_cast<int>(id));
            if (plan.fault) {
                // Recovery does not know the previous positions and releases at center.
                // Validate the normal path before fault separately from final release counts.
                path_verified = pre_fault_path_verified && zero_verified &&
                    final.downs - baseline.downs == plan.down.size() &&
                    final.ups - baseline.ups == plan.down.size() && final.cancels == baseline.cancels;
                manifest["path_result"] = "pre-fault path plus final UP counts/zero; emergency release coordinates not calibrated";
            } else {
                const auto checked = fixture_probe::verify_fixture_path(final, expected, 3, 0, stationary);
                manifest["final_redundant_stationary_moves"] = checked.redundant_stationary_moves;
                path_verified = checked.path.passed; manifest["path_result"] = checked.path.reason;
            }
            write_diagnostic_png(output / "after-release.png", reader.snapshot(std::chrono::milliseconds(1000)));
        } catch (const std::exception& error) {
            if (!failure.empty()) failure += "; ";
            failure += std::string("release_or_observation: ") + error.what();
        }
        SetConsoleCtrlHandler(signal_stop, FALSE);
        manifest["down_attempted"] = down_started; manifest["path_verified"] = path_verified;
        manifest["zero_active_observed"] = zero_verified; manifest["release_rpc_known"] = release_rpc_known;
        manifest["transport_fault_observed"] = transport_fault_observed;
        manifest["user_stop_requested"] = interrupted.load() || std::filesystem::exists(output / "STOP");
        manifest["failure"] = failure; manifest["host_start_ns"] = start; manifest["host_end_ns"] = clock.now_ns();
        const bool passed = failure.empty() && path_verified && zero_verified && release_rpc_known &&
            (!plan.fault || transport_fault_observed);
        manifest["status"] = passed ? "fixture_visual_path_passed" : "failed_or_stopped";
        log.flush(); const bool log_ok = static_cast<bool>(log);
        manifest["events_log_complete"] = log_ok;
        std::ofstream summary(output / "manifest.json", std::ios::binary); summary << manifest.dump(2) << '\n'; summary.close();
        if (!summary || !log_ok) throw std::runtime_error("evidence finalization failed; inspect partial release records");
        std::cout << manifest.dump(2) << '\n'; return passed ? 0 : 2;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
