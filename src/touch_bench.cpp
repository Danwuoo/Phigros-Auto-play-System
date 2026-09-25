#include "pas/touch_bench.hpp"

#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include "pas/config.hpp"
#include "pas/core.hpp"
#include "pas/emulator.hpp"
#include "pas/touch_evidence.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>

namespace pas {
namespace {
using json = nlohmann::json;
struct Point { int phase, x, y; Nanoseconds offset; };
struct OrderedPoint { int pointer, phase, x, y; Nanoseconds due; };

json observed_trace(const TouchEvidence& evidence) {
    json result = json::array();
    for (const auto& item : evidence.samples)
        result.push_back({{"sequence", item.sequence}, {"phase", item.phase},
                          {"android_id", item.android_id}, {"x", item.x}, {"y", item.y}});
    return result;
}

json expected_trace(const std::vector<ExpectedPointer>& expected) {
    json result = json::array();
    for (const auto& item : expected)
        result.push_back({{"phase", item.phase}, {"android_id", item.android_id},
                          {"x", item.x}, {"y", item.y}});
    return result;
}

std::uint32_t header_sequence(const Frame& frame) {
    if (frame.stride != frame.width * 3 || frame.width < 320 || frame.height < 660)
        throw std::runtime_error("touch Fixture geometry invalid");
    const std::size_t pixel = static_cast<std::size_t>(50) * frame.stride + 84 * 3;
    return (static_cast<std::uint32_t>(frame.rgb[pixel]) << 16) |
           (static_cast<std::uint32_t>(frame.rgb[pixel + 1]) << 8) |
           frame.rgb[pixel + 2];
}

void foreground(const std::filesystem::path& adb, const std::string& serial) {
    const auto bytes = adb_call(adb, {"-s", serial, "shell", "dumpsys", "window"}, 5000);
    const std::string report(bytes.begin(), bytes.end());
    const auto focus = report.find("mCurrentFocus=");
    const auto end = focus == std::string::npos ? focus : report.find('\n', focus);
    if (focus == std::string::npos ||
        report.substr(focus, end - focus).find("org.pas.touchfixture.cpp") == std::string::npos)
        throw std::runtime_error("native touch Fixture is not foreground");
}

std::vector<std::vector<Point>> case_paths(const std::string& kind, int index, int width, int height) {
    static constexpr double grid[9][2] = {
        {.12,.29},{.50,.29},{.88,.29},{.12,.58},{.50,.58},{.88,.58},
        {.12,.87},{.50,.87},{.88,.87}};
    const int x = static_cast<int>(std::lround(grid[index % 9][0] * width));
    const int y = static_cast<int>(std::lround(grid[index % 9][1] * height));
    const auto ns = [](int ms) -> Nanoseconds { return static_cast<Nanoseconds>(ms) * 1'000'000; };
    if (kind == "tap") return {{{1,x,y,0},{3,x,y,ns(40)}}};
    if (kind == "hold") return {{{1,x,y,0},{3,x,y,ns(250)}}};
    if (kind == "move") return {{{1,x,y,0},{2,x+35,y,ns(70)},
                                  {2,x+70,y+20,ns(140)},{3,x+70,y+20,ns(230)}}};
    if (kind == "flick") return {{{1,x,y,0},{2,x+45,y,ns(35)},
                                   {2,x+90,y,ns(70)},{2,x+45,y,ns(105)},
                                   {2,x,y,ns(150)},{3,x,y,ns(210)}}};
    if (kind == "pair") {
        const int x2 = std::min(width-30,x+140), y2 = std::min(height-25,y+30);
        return {{{1,x,y,0},{2,x+35,y,ns(110)},{3,x+35,y,ns(270)}},
                {{1,x2,y2,ns(50)},{3,x2,y2,ns(190)}}};
    }
    if (kind == "simultaneous") {
        const int x2 = std::min(width-30,x+140), y2 = std::min(height-25,y+30);
        return {{{1,x,y,0},{3,x,y,ns(180)}},{{1,x2,y2,0},{3,x2,y2,ns(180)}}};
    }
    if (kind == "edge") {
        static constexpr int corners[4][2] = {{0,0},{1,0},{0,1},{1,1}};
        const int ex = corners[index % 4][0] ? width-1 : 0;
        const int ey = corners[index % 4][1] ? height-1 : 0;
        return {{{1,ex,ey,0},{3,ex,ey,ns(80)}}};
    }
    throw std::invalid_argument("unknown touch case kind");
}

std::vector<OrderedPoint> ordered(const std::vector<std::vector<Point>>& paths, Nanoseconds start) {
    std::vector<OrderedPoint> result;
    for (std::size_t pointer = 0; pointer < paths.size(); ++pointer)
        for (const auto& point : paths[pointer])
            result.push_back({static_cast<int>(pointer),point.phase,point.x,point.y,start+point.offset});
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.due != b.due) return a.due < b.due;
        return a.pointer < b.pointer;
    });
    return result;
}

Phase phase(int value) {
    if (value == 1) return Phase::down;
    if (value == 2) return Phase::move;
    return Phase::up;
}

GrpcEndpoint endpoint_for(const RuntimeConfig& config) {
    if (config.endpoint.empty()) return discover_endpoint(config.serial);
    GrpcEndpoint endpoint;
    std::ifstream token(config.token_file, std::ios::binary);
    if (!token || !std::getline(token, endpoint.token))
        throw std::runtime_error("cannot read gRPC token file");
    if (!endpoint.token.empty() && endpoint.token.back() == '\r') endpoint.token.pop_back();
    if (endpoint.token.empty()) throw std::runtime_error("empty gRPC token file");
    endpoint.target = config.endpoint;
    endpoint.instance = "explicit";
    return endpoint;
}
}

void run_touch_bench(const std::filesystem::path& config_path, int repetitions,
                     const std::vector<std::string>& kinds,
                     const std::filesystem::path& output_dir,
                     const std::filesystem::path& fixture_apk) {
    const auto config = load_config(config_path);
    if (config.touch_kind != "emulator-grpc" || config.capture_kind != "emulator-grpc" ||
        config.capture_transport != "payload" || repetitions < 1 || repetitions > 1000 ||
        kinds.empty()) throw std::invalid_argument("touch benchmark requires valid native gRPC Fixture profile");
    for (const auto& kind : kinds) (void)case_paths(kind, 0, config.width, config.height);
    const auto adb = find_adb();
    foreground(adb, config.serial);
    const auto device_report = probe_adb(adb, config.serial);
    const auto installed_hash = installed_apk_sha256(adb, config.serial, "org.pas.touchfixture.cpp");
    const auto local_hash = fixture_apk.empty() ? std::string{} : sha256_file(fixture_apk);
    if (!local_hash.empty() && local_hash != installed_hash)
        throw std::runtime_error("installed native touch Fixture APK differs from supplied artifact");
    const auto endpoint = endpoint_for(config);
    HostClock clock;
    CaptureOptions capture_options;
    capture_options.width = config.width;
    capture_options.height = config.height;
    capture_options.source_rotation = config.source_rotation;
    GrpcCapture reader(clock, endpoint, capture_options);
    PixelCoordinateMap mapping(config.width, config.height, config.touch_width,
                               config.touch_height, config.touch_rotation);
    GrpcTouch touch(clock, endpoint, mapping, config.touch_width, config.touch_height,
                    config.max_contacts, std::chrono::milliseconds(config.touch_timeout_ms));
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const auto run_dir = output_dir.empty()
        ? std::filesystem::path(config.log_dir) / ("touch-cpp-" + std::to_string(stamp))
        : output_dir;
    if (std::filesystem::exists(run_dir)) throw std::runtime_error("touch benchmark output already exists");
    std::filesystem::create_directories(run_dir);
    std::ofstream raw(run_dir / "touch.jsonl", std::ios::binary);
    if (!raw) throw std::runtime_error("cannot create touch evidence log");
    json cases = json::array();
    json cancel_cases = json::array();
    std::vector<double> call_ms, schedule_ms;
    std::string failure;
    std::uint64_t epoch = 0;
    try {
        for (const auto& kind : kinds) for (int index = 0; index < repetitions; ++index) {
            foreground(adb, config.serial);
            const auto before_frame = reader.snapshot(std::chrono::milliseconds(2000));
            const auto before = decode_touch_evidence(before_frame, header_sequence(before_frame));
            if (!before.complete || before.active != 0)
                throw std::runtime_error("Fixture signature missing or residual contact before case");
            const auto baseline = clock.now_ns();
            const auto start = baseline + 80'000'000;
            const auto paths = case_paths(kind, index, config.width, config.height);
            const auto expected_path = ordered(paths, start);
            ContactScheduler scheduler(clock, touch, config.max_contacts, config.max_plans,
                config.max_steps, static_cast<Nanoseconds>(config.horizon_ms)*1'000'000,
                2'000'000'000, 200'000'000);
            if (!scheduler.set_gate(++epoch, true, baseline))
                throw std::runtime_error("Fixture gate rejected");
            for (std::size_t pointer = 0; pointer < paths.size(); ++pointer) {
                ContactPlan plan;
                plan.epoch = epoch; plan.intent_id = pointer + 1; plan.revision = 1;
                plan.evidence_ns = baseline; plan.valid_until_ns = start + 250'000'000;
                plan.source_frame_sequence = before_frame.sequence;
                plan.basis = "isolated_native_touch_fixture";
                for (const auto& point : paths[pointer])
                    plan.steps.push_back({phase(point.phase), static_cast<double>(point.x),
                                          static_cast<double>(point.y), start + point.offset});
                if (!scheduler.submit(std::move(plan))) throw std::runtime_error("Fixture touch plan rejected");
            }
            int receipts = 0;
            while (scheduler.pending_count() && clock.now_ns() < start + 1'500'000'000) {
                for (const auto& receipt : scheduler.run_due()) {
                    ++receipts;
                    call_ms.push_back((receipt.injection_return_ns - receipt.injection_start_ns) / 1e6);
                    schedule_ms.push_back((receipt.injection_start_ns - receipt.command.scheduled_ns) / 1e6);
                    raw << json{{"event", "touch_receipt"}, {"case", kind}, {"index", index},
                        {"intent_id", receipt.command.intent_id},
                        {"phase", static_cast<int>(receipt.command.phase)},
                        {"scheduled_ns", receipt.command.scheduled_ns},
                        {"injection_start_ns", receipt.injection_start_ns},
                        {"injection_return_ns", receipt.injection_return_ns},
                        {"rpc_returned", receipt.success}, {"effect_verified_by_rpc", false}}.dump() << '\n';
                    if (!receipt.success) throw std::runtime_error("touch RPC result unknown");
                }
                if (!scheduler.fault().empty()) throw std::runtime_error(scheduler.fault());
                if (const auto due = scheduler.next_due_ns()) {
                    const auto remaining = *due - clock.now_ns();
                    if (remaining > 0)
                        std::this_thread::sleep_for(std::chrono::nanoseconds(std::min<Nanoseconds>(remaining, 2'000'000)));
                }
            }
            if (scheduler.pending_count()) throw std::runtime_error("touch plan did not finish by deadline");
            const auto release = scheduler.last_release();
            if (!release.failed_ids.empty() || !release.unknown_ids.empty())
                throw std::runtime_error("touch release status unknown");
            TouchEvidence after;
            const auto deadline = clock.now_ns() + 2'000'000'000;
            while (clock.now_ns() < deadline) {
                auto frame = reader.snapshot(std::chrono::milliseconds(2000));
                after = decode_touch_evidence(frame, before.sequence);
                if (after.complete && after.active == 0 &&
                    after.downs - before.downs == paths.size() &&
                    after.ups - before.ups == paths.size()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            std::vector<ExpectedPointer> expected;
            for (const auto& item : expected_path)
                expected.push_back({item.phase,item.pointer,item.x,item.y});
            std::vector<int> stationary;
            if (kind == "hold") stationary.push_back(0);
            if (kind == "pair") stationary.push_back(1);
            const auto path = verify_touch_path(after, expected, 4, 0, stationary);
            const bool counters_match = after.downs - before.downs == paths.size() &&
                after.ups - before.ups == paths.size() && after.cancels == before.cancels;
            const bool rpc_match = receipts == static_cast<int>(expected.size());
            const bool passed = counters_match && rpc_match && path.passed;
            json case_result = {{"kind", kind}, {"index", index}, {"passed", passed},
                {"path_reason", path.reason}, {"rpc_receipts", receipts},
                {"expected_receipts", expected.size()}, {"before_sequence", before.sequence},
                {"after_sequence", after.sequence}, {"evidence_samples", after.samples.size()},
                {"android_down_delta", after.downs - before.downs},
                {"android_up_delta", after.ups - before.ups},
                {"final_active", after.active}, {"evidence_overflow_total", after.overflow_total}};
            raw << json{{"event", "touch_case"}, {"result", case_result},
                        {"observed_trace", observed_trace(after)},
                        {"expected_trace", expected_trace(expected)}}.dump() << '\n';
            cases.push_back(case_result);
            scheduler.cancel("case_complete");
            if (!passed) throw std::runtime_error("visible per-pointer evidence failed for " + kind);
        }
        for (int index = 0; index < repetitions; ++index) {
            foreground(adb, config.serial);
            const auto before_frame = reader.snapshot(std::chrono::milliseconds(2000));
            const auto before = decode_touch_evidence(before_frame, header_sequence(before_frame));
            if (!before.complete || before.active != 0)
                throw std::runtime_error("Fixture signature missing or residual contact before cancellation");
            const auto baseline = clock.now_ns();
            const auto down_due = baseline + 80'000'000;
            const int x = config.width / 2;
            const int y = config.height * 55 / 100;
            ContactScheduler scheduler(clock, touch, config.max_contacts, config.max_plans,
                config.max_steps, static_cast<Nanoseconds>(config.horizon_ms)*1'000'000,
                2'000'000'000, 200'000'000);
            if (!scheduler.set_gate(++epoch, true, baseline))
                throw std::runtime_error("Fixture cancellation gate rejected");
            ContactPlan plan;
            plan.epoch = epoch; plan.intent_id = 1; plan.revision = 1;
            plan.evidence_ns = baseline; plan.valid_until_ns = down_due + 250'000'000;
            plan.source_frame_sequence = before_frame.sequence;
            plan.basis = "isolated_native_fixture_cancel";
            plan.steps = {{Phase::down, static_cast<double>(x), static_cast<double>(y), down_due},
                          {Phase::up, static_cast<double>(x), static_cast<double>(y),
                           down_due + 1'000'000'000}};
            if (!scheduler.submit(std::move(plan)))
                throw std::runtime_error("Fixture cancellation plan rejected");
            std::vector<TouchReceipt> downs;
            while (clock.now_ns() < down_due + 120'000'000 && downs.empty()) {
                downs = scheduler.run_due();
                if (downs.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (downs.size() != 1 || !downs.front().success ||
                downs.front().command.phase != Phase::down)
                throw std::runtime_error("Fixture cancellation down was not injected");
            bool saw_active = false;
            const auto active_deadline = clock.now_ns() + 700'000'000;
            while (clock.now_ns() < active_deadline) {
                const auto frame = reader.snapshot(std::chrono::milliseconds(1000));
                const auto middle = decode_touch_evidence(frame, before.sequence);
                if (middle.complete && middle.active == 1 && middle.downs == before.downs + 1) {
                    saw_active = true; break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            scheduler.cancel("fixture_cancel_before_planned_up");
            const auto release = scheduler.last_release();
            if (!release.failed_ids.empty() || !release.unknown_ids.empty())
                throw std::runtime_error("Fixture cancellation release unknown");
            TouchEvidence after;
            const auto release_deadline = clock.now_ns() + 2'000'000'000;
            while (clock.now_ns() < release_deadline) {
                const auto frame = reader.snapshot(std::chrono::milliseconds(1000));
                after = decode_touch_evidence(frame, before.sequence);
                if (after.complete && after.active == 0 &&
                    after.downs == before.downs + 1 && after.ups == before.ups + 1) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const auto path = verify_touch_path(after, {{1, 0, x, y}, {3, 0, x, y}}, 4, 0);
            const bool passed = saw_active && path.passed &&
                after.downs == before.downs + 1 && after.ups == before.ups + 1 &&
                after.cancels == before.cancels;
            json result = {{"index", index}, {"passed", passed},
                {"saw_active", saw_active}, {"path_reason", path.reason},
                {"before_sequence", before.sequence}, {"after_sequence", after.sequence},
                {"android_down_delta", after.downs - before.downs},
                {"android_up_delta", after.ups - before.ups},
                {"final_active", after.active},
                {"release_requested_ids", release.requested_ids},
                {"release_start_ns", release.start_ns}, {"release_return_ns", release.return_ns}};
            raw << json{{"event", "touch_cancel_case"}, {"result", result},
                        {"observed_trace", observed_trace(after)},
                        {"expected_trace", expected_trace({{1, 0, x, y}, {3, 0, x, y}})}}.dump() << '\n';
            cancel_cases.push_back(result);
            if (!passed) throw std::runtime_error("visible cancellation evidence failed");
        }
    } catch (const std::exception& error) {
        failure = error.what();
        const auto release = touch.release_all();
        raw << json{{"event", "emergency_release"}, {"failed_ids", release.failed_ids},
            {"unknown_ids", release.unknown_ids}}.dump() << '\n';
        if (!release.failed_ids.empty() || !release.unknown_ids.empty())
            failure += "; emergency release unknown";
    }
    const std::set<std::string> selected(kinds.begin(), kinds.end());
    static constexpr std::array required_kinds = {
        "tap", "hold", "move", "flick", "pair", "simultaneous"};
    const bool all_core_kinds = std::all_of(required_kinds.begin(), required_kinds.end(),
        [&](const char* kind) { return selected.contains(kind); });
    json summary = {{"schema_version", 2}, {"fixture_schema", "native_touch_v2"},
        {"serial", config.serial}, {"config", config.public_json},
        {"device_report", device_report},
        {"fixture_apk_sha256", local_hash.empty() ? json(nullptr) : json(local_hash)},
        {"installed_apk_sha256", installed_hash},
        {"repetitions", repetitions}, {"kinds", kinds}, {"cases", cases},
        {"cancel_cases", cancel_cases}, {"cancel_case_count", cancel_cases.size()},
        {"case_count", cases.size()}, {"rpc_call_ms", distribution(call_ms)},
        {"schedule_error_ms", distribution(schedule_ms)},
        {"failure", failure.empty() ? json(nullptr) : json(failure)},
        {"capability_verified", failure.empty() && !local_hash.empty() &&
                                local_hash == installed_hash && repetitions >= 30 && all_core_kinds &&
                                cases.size() == static_cast<std::size_t>(repetitions) * kinds.size() &&
                                cancel_cases.size() == static_cast<std::size_t>(repetitions)},
        {"clock_domain", "host_qpc_ns"}, {"qpc_frequency", clock.frequency()},
        {"raw_log", std::filesystem::absolute(run_dir / "touch.jsonl").string()}};
    std::ofstream report(run_dir / "summary.json", std::ios::binary);
    report << summary.dump(2) << '\n';
    std::cout << summary.dump(2) << '\n';
    if (!failure.empty()) throw std::runtime_error(failure);
}

void run_touch_batch_bench(const std::filesystem::path& config_path, int repetitions,
                           const std::filesystem::path& output_dir,
                           const std::filesystem::path& fixture_apk) {
    const auto config = load_config(config_path);
    if (config.capture_kind != "emulator-grpc" || config.touch_kind != "emulator-grpc" ||
        config.max_contacts < 2 || repetitions < 1 || repetitions > 1000)
        throw std::invalid_argument("batch touch requires two-contact native Fixture profile");
    const auto adb = find_adb();
    foreground(adb, config.serial);
    const auto device_report = probe_adb(adb, config.serial);
    const auto installed_hash = installed_apk_sha256(adb, config.serial, "org.pas.touchfixture.cpp");
    const auto local_hash = fixture_apk.empty() ? std::string{} : sha256_file(fixture_apk);
    if (!local_hash.empty() && local_hash != installed_hash)
        throw std::runtime_error("installed native touch Fixture APK differs from supplied artifact");
    const auto endpoint = endpoint_for(config);
    HostClock clock;
    CaptureOptions capture_options;
    capture_options.width = config.width;
    capture_options.height = config.height;
    capture_options.source_rotation = config.source_rotation;
    GrpcCapture reader(clock, endpoint, capture_options);
    GrpcTouch touch(clock, endpoint,
        PixelCoordinateMap(config.width, config.height, config.touch_width,
                           config.touch_height, config.touch_rotation),
        config.touch_width, config.touch_height, 2,
        std::chrono::milliseconds(config.touch_timeout_ms));
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const auto run_dir = output_dir.empty()
        ? std::filesystem::path(config.log_dir) / ("batch-cpp-" + std::to_string(stamp))
        : output_dir;
    if (std::filesystem::exists(run_dir)) throw std::runtime_error("batch output already exists");
    std::filesystem::create_directories(run_dir);
    std::ofstream raw(run_dir / "batch.jsonl", std::ios::binary);
    json cases = json::array();
    std::vector<double> call_ms;
    std::string failure;
    try {
        for (int index = 0; index < repetitions; ++index) {
            foreground(adb, config.serial);
            const auto before_frame = reader.snapshot(std::chrono::milliseconds(2000));
            const auto before = decode_touch_evidence(before_frame, header_sequence(before_frame));
            if (!before.complete || before.active != 0)
                throw std::runtime_error("Fixture signature missing or residual contact before batch");
            const int ax = static_cast<int>(std::lround(config.width * .3));
            const int ay = static_cast<int>(std::lround(config.height * .45));
            const int bx = static_cast<int>(std::lround(config.width * .7));
            const int by = static_cast<int>(std::lround(config.height * .55));
            const auto command = [&](int pointer, Phase state, int x, int y) {
                return TouchCommand{static_cast<std::uint64_t>(index+1),pointer,state,
                                    static_cast<double>(x),static_cast<double>(y),
                                    clock.now_ns(),before_frame.sequence};
            };
            const auto record = [&](const std::vector<TouchReceipt>& receipts, const char* stage) {
                if (receipts.size() != 2) throw std::runtime_error("batch receipt count mismatch");
                for (const auto& receipt : receipts) {
                    call_ms.push_back((receipt.injection_return_ns - receipt.injection_start_ns)/1e6);
                    raw << json{{"event", "batch_receipt"}, {"index", index}, {"stage", stage},
                        {"contact_id", receipt.command.contact_id},
                        {"injection_start_ns", receipt.injection_start_ns},
                        {"injection_return_ns", receipt.injection_return_ns},
                        {"rpc_returned", receipt.success}, {"effect_verified_by_rpc", false}}.dump() << '\n';
                    if (!receipt.success) throw std::runtime_error("batch RPC result unknown");
                }
            };
            record(touch.inject_batch({command(0,Phase::down,ax,ay),
                                       command(1,Phase::down,bx,by)}), "down");
            bool active_two = false;
            auto deadline = clock.now_ns() + 1'500'000'000;
            while (clock.now_ns() < deadline) {
                auto frame = reader.snapshot(std::chrono::milliseconds(1000));
                const auto middle = decode_touch_evidence(frame, before.sequence);
                if (middle.complete && middle.active == 2 && middle.downs - before.downs == 2) {
                    active_two = true; break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!active_two) throw std::runtime_error("two contacts were not simultaneously visible");
            record(touch.inject_batch({command(0,Phase::move,ax+40,ay),
                                       command(1,Phase::move,bx,by)}), "move");
            record(touch.inject_batch({command(0,Phase::up,ax+40,ay),
                                       command(1,Phase::up,bx,by)}), "up");
            TouchEvidence after;
            deadline = clock.now_ns() + 1'500'000'000;
            while (clock.now_ns() < deadline) {
                auto frame = reader.snapshot(std::chrono::milliseconds(1000));
                after = decode_touch_evidence(frame, before.sequence);
                if (after.complete && after.active == 0 && after.ups - before.ups == 2) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            std::vector<ExpectedPointer> expected = {{1,0,ax,ay},{1,1,bx,by},
                {2,0,ax+40,ay},{2,1,bx,by},{3,0,ax+40,ay},{3,1,bx,by}};
            auto path = verify_touch_path(after, expected, 4, 0, {1});
            if (!path.passed) {
                std::swap(expected[4], expected[5]);
                path = verify_touch_path(after, expected, 4, 0, {1});
            }
            const bool passed = active_two && path.passed &&
                after.downs - before.downs == 2 && after.ups - before.ups == 2 &&
                after.cancels == before.cancels;
            json case_result = {{"index", index}, {"passed", passed},
                {"two_active_visible", active_two}, {"path_reason", path.reason},
                {"before_sequence", before.sequence}, {"after_sequence", after.sequence},
                {"evidence_samples", after.samples.size()},
                {"android_down_delta", after.downs - before.downs},
                {"android_up_delta", after.ups - before.ups},
                {"evidence_overflow_total", after.overflow_total}};
            cases.push_back(case_result);
            raw << json{{"event", "batch_case"}, {"result", case_result},
                        {"observed_trace", observed_trace(after)},
                        {"expected_trace", expected_trace(expected)}}.dump() << '\n';
            if (!passed) throw std::runtime_error("batch per-pointer evidence failed");
        }
    } catch (const std::exception& error) { failure = error.what(); }
    const auto release = touch.release_all();
    if (!release.failed_ids.empty() || !release.unknown_ids.empty())
        failure += "; final release unknown";
    json summary = {{"schema_version", 2}, {"fixture_schema", "native_touch_v2"},
        {"kind", "two_pointer_grpc_batch"}, {"serial", config.serial},
        {"device_report", device_report},
        {"fixture_apk_sha256", local_hash.empty() ? json(nullptr) : json(local_hash)},
        {"installed_apk_sha256", installed_hash},
        {"requested", repetitions}, {"cases", cases}, {"case_count", cases.size()},
        {"rpc_call_ms", distribution(call_ms)},
        {"release_failed_ids", release.failed_ids}, {"release_unknown_ids", release.unknown_ids},
        {"capability_verified", failure.empty() && !local_hash.empty() &&
                                local_hash == installed_hash && cases.size() >= 30},
        {"failure", failure.empty() ? json(nullptr) : json(failure)},
        {"clock_domain", "host_qpc_ns"}, {"qpc_frequency", clock.frequency()},
        {"raw_log", std::filesystem::absolute(run_dir / "batch.jsonl").string()}};
    std::ofstream report(run_dir / "summary.json", std::ios::binary);
    report << summary.dump(2) << '\n';
    std::cout << summary.dump(2) << '\n';
    if (!failure.empty()) throw std::runtime_error(failure);
}

void run_touch_disconnect_smoke(const std::filesystem::path& config_path,
                                const std::filesystem::path& output_dir,
                                const std::filesystem::path& fixture_apk) {
    const auto config = load_config(config_path);
    if (config.capture_kind != "emulator-grpc" || config.touch_kind != "emulator-grpc")
        throw std::invalid_argument("disconnect smoke requires native gRPC Fixture profile");
    const auto adb = find_adb();
    foreground(adb, config.serial);
    const auto device_report = probe_adb(adb, config.serial);
    const auto installed_hash = installed_apk_sha256(adb, config.serial, "org.pas.touchfixture.cpp");
    const auto local_hash = fixture_apk.empty() ? std::string{} : sha256_file(fixture_apk);
    if (!local_hash.empty() && local_hash != installed_hash)
        throw std::runtime_error("installed native touch Fixture APK differs from supplied artifact");
    const auto endpoint = endpoint_for(config);
    HostClock clock;
    CaptureOptions capture_options;
    capture_options.width = config.width;
    capture_options.height = config.height;
    capture_options.source_rotation = config.source_rotation;
    GrpcCapture reader(clock, endpoint, capture_options);
    const auto mapping = PixelCoordinateMap(config.width, config.height, config.touch_width,
                                            config.touch_height, config.touch_rotation);
    GrpcTouch touch(clock, endpoint, mapping, config.touch_width, config.touch_height,
                    config.max_contacts, std::chrono::milliseconds(config.touch_timeout_ms));
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const auto run_dir = output_dir.empty()
        ? std::filesystem::path(config.log_dir) / ("disconnect-cpp-" + std::to_string(stamp))
        : output_dir;
    if (std::filesystem::exists(run_dir)) throw std::runtime_error("disconnect output already exists");
    std::filesystem::create_directories(run_dir);
    const auto before_frame = reader.snapshot(std::chrono::milliseconds(2000));
    const auto before = decode_touch_evidence(before_frame, header_sequence(before_frame));
    if (!before.complete || before.active != 0)
        throw std::runtime_error("Fixture signature missing or residual contact before disconnect");
    const int x = config.width / 2, y = config.height / 2;
    const auto command = [&](Phase state) {
        return TouchCommand{1,0,state,static_cast<double>(x),static_cast<double>(y),
                            clock.now_ns(),before_frame.sequence};
    };
    std::string failure;
    bool rpc_failure = false, active_seen = false, stable_zero = true;
    ReleaseReport initial_release;
    std::vector<ReleaseReport> rescue_reports;
    TouchEvidence after;
    try {
        const auto down = touch.inject(command(Phase::down));
        if (!down.success) throw std::runtime_error("setup down RPC failed");
        auto deadline = clock.now_ns() + 1'000'000'000;
        while (clock.now_ns() < deadline) {
            const auto frame = reader.snapshot(std::chrono::milliseconds(1000));
            const auto middle = decode_touch_evidence(frame, before.sequence);
            if (middle.complete && middle.active == 1 && middle.downs > before.downs) {
                active_seen = true; break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!active_seen) throw std::runtime_error("setup contact not visible in Fixture pixels");
        touch.simulate_transport_loss_for_fixture_test();
        const auto uncertain = touch.inject(command(Phase::up));
        rpc_failure = !uncertain.success && touch.faulted();
        initial_release = touch.release_all();
        GrpcTouch rescue(clock, endpoint, mapping, config.touch_width, config.touch_height,
                         config.max_contacts, std::chrono::milliseconds(config.touch_timeout_ms));
        for (int attempt = 0; attempt < 2; ++attempt) {
            rescue_reports.push_back(rescue.emergency_release_all());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        for (int sample = 0; sample < 5; ++sample) {
            const auto frame = reader.snapshot(std::chrono::milliseconds(1000));
            after = decode_touch_evidence(frame, before.sequence);
            stable_zero &= after.complete && after.active == 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!rpc_failure || !stable_zero ||
            std::any_of(rescue_reports.begin(), rescue_reports.end(),
                [](const auto& report) { return !report.failed_ids.empty(); }))
            throw std::runtime_error("disconnect recovery not visible or rescue RPC failed");
    } catch (const std::exception& error) { failure = error.what(); }
    // A fresh authenticated channel owns final release responsibility even
    // if an earlier stage failed after the Android contact became active.
    GrpcTouch final_rescue(clock, endpoint, mapping, config.touch_width, config.touch_height,
                           config.max_contacts, std::chrono::milliseconds(config.touch_timeout_ms));
    const auto final_release = final_rescue.emergency_release_all();
    if (!final_release.failed_ids.empty()) failure += "; final rescue RPC failed";
    json rescues = json::array();
    for (const auto& report : rescue_reports)
        rescues.push_back({{"failed_ids", report.failed_ids}, {"unknown_ids", report.unknown_ids},
                           {"start_ns", report.start_ns}, {"return_ns", report.return_ns}});
    json summary = {{"schema_version", 2}, {"fixture_schema", "native_touch_v2"},
        {"kind", "fixture_disconnect_recovery_smoke"}, {"serial", config.serial},
        {"device_report", device_report},
        {"fixture_apk_sha256", local_hash.empty() ? json(nullptr) : json(local_hash)},
        {"installed_apk_sha256", installed_hash},
        {"smoke_verified", failure.empty() && !local_hash.empty() &&
                           local_hash == installed_hash && rpc_failure && active_seen && stable_zero},
        {"rpc_failure_triggered", rpc_failure}, {"active_seen", active_seen},
        {"visible_zero_after_recovery", stable_zero && after.complete && after.active == 0},
        {"initial_release_failed_ids", initial_release.failed_ids},
        {"initial_release_unknown_ids", initial_release.unknown_ids},
        {"rescue_reports", rescues}, {"final_release_failed_ids", final_release.failed_ids},
        {"before_sequence", before.sequence}, {"after_sequence", after.sequence},
        {"failure", failure.empty() ? json(nullptr) : json(failure)},
        {"limitation", "deterministic local channel loss; external network fault and real RPC timeout unverified"},
        {"clock_domain", "host_qpc_ns"}, {"qpc_frequency", clock.frequency()}};
    std::ofstream report(run_dir / "summary.json", std::ios::binary);
    report << summary.dump(2) << '\n';
    std::cout << summary.dump(2) << '\n';
    if (!failure.empty()) throw std::runtime_error(failure);
}

} // namespace pas
