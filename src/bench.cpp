#include "pas/bench.hpp"

#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include "pas/core.hpp"
#include "pas/emulator.hpp"
#include "pas/journal.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>
#define NOMINMAX
#include <windows.h>
#include <psapi.h>

namespace pas {
namespace {
using json = nlohmann::json;

std::optional<int> legacy_fixture_counter(const Frame& frame) {
    const auto pixel = [&](int x, int y) {
        const auto offset = (static_cast<std::size_t>(y) * frame.width + x) * 3;
        if (x < 0 || y < 0 || x >= frame.width || y >= frame.height ||
            offset + 2 >= frame.rgb.size()) return std::array<int, 3>{-1, -1, -1};
        return std::array<int, 3>{frame.rgb[offset], frame.rgb[offset + 1], frame.rgb[offset + 2]};
    };
    const auto red = [](const std::array<int, 3>& p) { return p[0] > 180 && p[1] < 90 && p[2] < 90; };
    int counter = 0;
    for (int row = 0; row < 2; ++row) {
        const int y = 96 + row * 26;
        if (!red(pixel(16, y)) || !red(pixel(206, y))) return {};
        for (int bit = 0; bit < 12; ++bit) {
            const auto value = pixel(36 + bit * 14, y);
            if (std::all_of(value.begin(), value.end(), [](int c) { return c > 180; }))
                counter |= 1 << (row * 12 + bit);
            else if (!std::all_of(value.begin(), value.end(), [](int c) { return c < 90; })) return {};
        }
    }
    return counter;
}

struct FixtureCounter {
    int value = 0;
    const char* schema = "";
};

std::optional<FixtureCounter> fixture_counter(const Frame& frame) {
    // The native Fixture repeats a 24-bit identity and its complement in all
    // four corners. Decode every region before accepting a visible update.
    if (frame.stride != frame.width * 3 || frame.width < 180 || frame.height < 180 ||
        frame.rgb.size() != static_cast<std::size_t>(frame.stride) * frame.height)
        return {};
    const auto value = [&](int x, int y) -> std::uint32_t {
        const auto offset = static_cast<std::size_t>(y) * frame.stride + x * 3;
        return (static_cast<std::uint32_t>(frame.rgb[offset]) << 16) |
               (static_cast<std::uint32_t>(frame.rgb[offset + 1]) << 8) |
               frame.rgb[offset + 2];
    };
    std::optional<std::uint32_t> identity;
    bool native_valid = true;
    for (int corner = 0; corner < 4; ++corner) {
        const int x = corner % 2 ? frame.width - 90 : 65;
        const int y = corner / 2 ? frame.height - 90 : 55;
        const auto part = value(x + 12, y + 10);
        const auto check = value(x + 37, y + 10);
        if ((part ^ 0xA5C37E) != check || (identity && part != *identity)) {
            native_valid = false;
            break;
        }
        identity = part;
    }
    if (native_valid && identity)
        return FixtureCounter{static_cast<int>(*identity), "native_v2_four_region"};
    if (const auto legacy = legacy_fixture_counter(frame))
        return FixtureCounter{*legacy, "legacy_counter"};
    return {};
}

std::string tokenless_failure(std::exception_ptr error) {
    if (!error) return {};
    try { std::rethrow_exception(error); }
    catch (const std::exception& e) { return e.what(); }
    catch (...) { return "unknown worker exception"; }
}

std::string adb_text(const std::filesystem::path& adb, const std::string& serial,
                     const std::vector<std::string>& args) {
    std::vector<std::string> command = {"-s", serial, "shell"};
    command.insert(command.end(), args.begin(), args.end());
    const auto bytes = adb_call(adb, command);
    return {bytes.begin(), bytes.end()};
}

json device_preflight(const std::filesystem::path& adb, const std::string& serial) {
    auto report = probe_adb(adb, serial);
    std::istringstream cpu(adb_text(adb, serial, {"cat", "/proc/cpuinfo"}));
    std::string line;
    int processors = 0;
    while (std::getline(cpu, line))
        if (line.rfind("processor", 0) == 0) ++processors;
    std::istringstream memory(adb_text(adb, serial, {"cat", "/proc/meminfo"}));
    std::uint64_t mem_kib = 0;
    while (std::getline(memory, line)) {
        if (line.rfind("MemTotal:", 0) != 0) continue;
        std::istringstream row(line.substr(9));
        row >> mem_kib;
        break;
    }
    report["guest_online_processors"] = processors;
    report["guest_memtotal_kib"] = mem_kib;
    report["fixture_target_hz_property"] =
        adb_text(adb, serial, {"getprop", "debug.pas.fixture_hz"});
    MEMORYSTATUSEX host{};
    host.dwLength = sizeof(host);
    if (GlobalMemoryStatusEx(&host))
        report["host_physical_memory_bytes"] = host.ullTotalPhys;
    report["host_logical_processors"] = std::thread::hardware_concurrency();
    return report;
}

GrpcEndpoint benchmark_endpoint(const CaptureBenchOptions& options) {
    if (options.grpc_endpoint.empty()) return discover_endpoint(options.serial);
    std::ifstream token(options.grpc_token_file, std::ios::binary);
    std::string value;
    if (!token || !std::getline(token, value) || value.empty())
        throw std::runtime_error("cannot read explicit gRPC token file");
    if (!value.empty() && value.back() == '\r') value.pop_back();
    if (value.empty()) throw std::runtime_error("empty explicit gRPC token");
    return {options.grpc_endpoint, value, "explicit"};
}

struct ResourceSample {
    Nanoseconds before_ns = 0;
    Nanoseconds after_ns = 0;
    std::uint64_t process_cpu_ns = 0;
    std::uint64_t working_set_bytes = 0;
};

std::uint64_t filetime_100ns(const FILETIME& value) {
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}

ResourceSample process_resources(const Clock& clock) {
    ResourceSample sample;
    sample.before_ns = clock.now_ns();
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        throw std::runtime_error("GetProcessTimes failed");
    PROCESS_MEMORY_COUNTERS_EX memory{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                              static_cast<DWORD>(sizeof(memory))))
        throw std::runtime_error("GetProcessMemoryInfo failed");
    sample.after_ns = clock.now_ns();
    sample.process_cpu_ns = (filetime_100ns(kernel) + filetime_100ns(user)) * 100;
    sample.working_set_bytes = memory.WorkingSetSize;
    return sample;
}
}

void run_capture_bench(const CaptureBenchOptions& options) {
    if (options.serial.empty() || options.width <= 0 || options.height <= 0 ||
        options.width > 4096 || options.height > 4096 ||
        options.source_rotation < 0 || options.source_rotation > 3 ||
        !std::isfinite(options.duration_s) || !std::isfinite(options.warmup_s) ||
        !std::isfinite(options.ready_timeout_s) || !std::isfinite(options.consumer_delay_ms) ||
        !std::isfinite(options.receiver_pause_ms) ||
        options.duration_s <= 0 || options.duration_s > 3600 || options.warmup_s < 0 ||
        options.ready_timeout_s <= 0 || options.consumer_delay_ms < 0 ||
        options.receiver_pause_ms < 0 || options.receiver_pause_ms > 5000 ||
        (options.consumer_recover_after_s && (!std::isfinite(*options.consumer_recover_after_s) ||
            *options.consumer_recover_after_s < 0 || *options.consumer_recover_after_s > options.duration_s)) ||
        (options.max_relative_lag_ms && (!std::isfinite(*options.max_relative_lag_ms) ||
            *options.max_relative_lag_ms <= 0 || *options.max_relative_lag_ms > 5000)) ||
        options.output_dir.empty() || options.max_rgb_bytes == 0 ||
        options.max_rgb_bytes > 16 * 1024 * 1024 ||
        static_cast<std::uint64_t>(options.width) * options.height * 3 > options.max_rgb_bytes ||
        (options.grpc_endpoint.empty() != options.grpc_token_file.empty()) ||
        (options.image_format != "rgb888" && options.image_format != "rgba8888") ||
        (options.row_order != "top-down" && options.row_order != "bottom-up") ||
        (options.fixture && options.fixture_apk.empty()) ||
        (options.fixture_schema != "native-v2" && options.fixture_schema != "legacy"))
        throw std::invalid_argument("invalid capture benchmark options");
    if (options.backend == "adb-png" && (options.image_format != "rgb888" ||
        options.row_order != "top-down" || !options.grpc_endpoint.empty()))
        throw std::invalid_argument("ADB PNG does not use gRPC image options");
    if (options.transport == "mmap" && (!options.diagnostic_mmap || options.backend != "emulator-grpc"))
        throw std::invalid_argument("MMAP is diagnostic-only and requires explicit --diagnostic-mmap");
    if (options.transport != "payload" && options.transport != "mmap")
        throw std::invalid_argument("unsupported capture transport");
    if (std::filesystem::exists(options.output_dir))
        throw std::runtime_error("output directory already exists; choose a new run ID");
    std::filesystem::create_directories(options.output_dir);
    HostClock clock;
    const auto adb_for_preflight = find_adb();
    const auto preflight = device_preflight(adb_for_preflight, options.serial);
    const auto fixture_hash = options.fixture_apk.empty() ? std::string{} : sha256_file(options.fixture_apk);
    const auto installed_fixture_hash = options.fixture
        ? installed_apk_sha256(adb_for_preflight, options.serial,
            options.fixture_schema == "legacy" ? "org.pas.capturefixture" : "org.pas.capturefixture.cpp")
        : std::string{};
    if (options.fixture && fixture_hash != installed_fixture_hash)
        throw std::runtime_error("installed native capture Fixture APK differs from supplied artifact");
    Journal journal(options.output_dir / "capture.jsonl");
    LatestFrame latest(options.width, options.height, 3, &clock);
    std::unique_ptr<GrpcCapture> capture;
    std::string endpoint_target;
    if (options.backend == "emulator-grpc") {
        CaptureOptions capture_options;
        capture_options.width = options.width;
        capture_options.height = options.height;
        capture_options.source_rotation = options.source_rotation;
        capture_options.rgba = options.image_format == "rgba8888";
        capture_options.bottom_up = options.row_order == "bottom-up";
        capture_options.max_rgb_bytes = options.max_rgb_bytes;
        capture_options.diagnostic_mmap = options.transport == "mmap";
        if (options.max_relative_lag_ms)
            capture_options.max_relative_lag_ns = static_cast<Nanoseconds>(std::llround(*options.max_relative_lag_ms * 1e6));
        auto endpoint = benchmark_endpoint(options);
        endpoint_target = endpoint.target;
        capture = std::make_unique<GrpcCapture>(clock, std::move(endpoint), capture_options);
    } else if (options.backend != "adb-png") throw std::invalid_argument("unsupported capture backend");
    const auto adb = options.backend == "adb-png" ? adb_for_preflight : std::filesystem::path{};
    const auto manifest = json{{"schema_version", 2}, {"serial", options.serial},
        {"capture_backend", options.backend}, {"transport", options.transport},
        {"consistency", options.transport == "mmap" ? "unverified" : "payload"},
        {"execution", "single_process_thread"}, {"width", options.width}, {"height", options.height},
        {"source_rotation", options.source_rotation}, {"warmup_s", options.warmup_s},
        {"image_format", options.image_format}, {"row_order", options.row_order},
        {"max_rgb_bytes", options.max_rgb_bytes}, {"grpc_endpoint", endpoint_target},
        {"duration_s", options.duration_s}, {"ready_timeout_s", options.ready_timeout_s},
        {"consumer_delay_ms", options.consumer_delay_ms},
        {"consumer_recover_after_s", options.consumer_recover_after_s
            ? json(*options.consumer_recover_after_s) : json(nullptr)},
        {"receiver_pause_ms", options.receiver_pause_ms}, {"fixture_counter_basis", "consumed_frames"},
        {"host_load", options.load},
        {"preview", false}, {"qpc_frequency", clock.frequency()},
        {"source_absolute_age", nullptr}, {"produced_ns", nullptr},
        {"device_preflight", preflight},
        {"fixture_schema_requested", options.fixture ? json(options.fixture_schema) : json(nullptr)},
        {"fixture_apk_sha256", fixture_hash.empty() ? json(nullptr) : json(fixture_hash)},
        {"installed_apk_sha256", installed_fixture_hash.empty() ? json(nullptr)
            : json(installed_fixture_hash)}};
    {
        std::ofstream file(options.output_dir / "manifest.json", std::ios::binary);
        file << manifest.dump(2) << '\n';
    }
    const auto event = [&](const std::string& phase, Nanoseconds time) {
        if (!journal.push({{"event", "bench_phase"}, {"phase", phase}, {"monotonic_ns", time}}))
            throw std::runtime_error("critical journal overrun");
    };
    event("CONNECTING", clock.now_ns());
    std::atomic<bool> worker_done = false;
    std::atomic<bool> pause_used = false;
    std::atomic<Nanoseconds> measurement_start = 0;
    std::mutex fault_mutex;
    std::exception_ptr worker_fault;
    std::jthread load_worker;
    std::jthread worker([&](std::stop_token stop) {
        try {
            const auto publish = [&](Frame&& frame) {
                frame.epoch = 1;
                const bool accepted = latest.publish(frame.rgb.data(), frame.rgb.size(), frame);
                const auto publication = accepted ? latest.peek()->published_ns : 0;
                if (!journal.push({{"event", "capture"}, {"frame_sequence", frame.sequence},
                    {"capture_complete_ns", frame.capture_complete_ns},
                    {"pixels_ready_ns", frame.pixels_ready_ns},
                    {"source_sequence", frame.source_sequence ? json(*frame.source_sequence) : json(nullptr)},
                    {"source_timestamp_us", frame.source_timestamp_us ? json(*frame.source_timestamp_us) : json(nullptr)},
                    {"source_clock_domain", "emulator_unix_us_uncalibrated"},
                    {"source_rotation", frame.source_rotation},
                    {"width", frame.width}, {"height", frame.height},
                    {"pixel_format", "RGB24"}, {"stream_generation", 0}}))
                    throw std::runtime_error("critical journal overrun");
                if (accepted && !journal.push({{"event", "published"},
                    {"frame_sequence", frame.sequence}, {"published_ns", publication}}))
                    throw std::runtime_error("critical journal overrun");
                const auto start = measurement_start.load();
                if (options.receiver_pause_ms > 0 && start &&
                    clock.now_ns() > start + 1'000'000'000 && !pause_used.exchange(true)) {
                    const auto pause_start = clock.now_ns();
                    std::this_thread::sleep_for(std::chrono::nanoseconds(
                        static_cast<Nanoseconds>(std::llround(options.receiver_pause_ms * 1e6))));
                    if (!journal.push({{"event", "receiver_pause"}, {"start_ns", pause_start},
                        {"ended_ns", clock.now_ns()}, {"duration_ms", options.receiver_pause_ms},
                        {"frame_sequence", frame.sequence}}))
                        throw std::runtime_error("critical journal overrun");
                }
            };
            if (capture) capture->stream(stop, publish);
            else {
                std::uint64_t sequence = 0;
                while (!stop.stop_requested())
                    publish(capture_adb_png(clock, adb, options.serial, ++sequence));
            }
        } catch (...) { std::lock_guard lock(fault_mutex); worker_fault = std::current_exception(); }
        worker_done = true;
        latest.close();
    });
    const auto stop_worker = [&] {
        if (load_worker.joinable()) { load_worker.request_stop(); load_worker.join(); }
        worker.request_stop();
        if (capture) capture->cancel();
        worker.join();
        journal.close();
    };
    try {
        std::uint64_t last_sequence = 0;
        const auto ready_deadline = clock.now_ns() + static_cast<Nanoseconds>(std::llround(options.ready_timeout_s * 1e9));
        auto first = latest.read_after(0, static_cast<Nanoseconds>(std::llround(options.ready_timeout_s * 1e9)));
        if (!first || clock.now_ns() >= ready_deadline)
            throw std::runtime_error("capture source did not deliver a valid frame before ready timeout");
        last_sequence = first->sequence;
        if (options.load) {
            load_worker = std::jthread([](std::stop_token stop) {
                std::atomic<std::uint64_t> sink = 1;
                while (!stop.stop_requested())
                    sink.store(sink.load(std::memory_order_relaxed) * 3 + 1,
                               std::memory_order_relaxed);
            });
        }
        const auto warmup_start = clock.now_ns();
        event("WARMUP", warmup_start);
        const auto warmup_end = warmup_start + static_cast<Nanoseconds>(std::llround(options.warmup_s * 1e9));
        while (clock.now_ns() < warmup_end && !worker_done) {
            auto frame = latest.read_after(last_sequence, 50'000'000);
            if (frame) last_sequence = frame->sequence;
        }
        if (worker_done) throw std::runtime_error("capture worker stopped during warmup");
        const auto formal_start = clock.now_ns();
        measurement_start = formal_start;
        event("MEASURING", formal_start);
        const auto resource_start = process_resources(clock);
        if (!journal.push({{"event", "resource_boundary"}, {"phase", "start"},
            {"before_ns", resource_start.before_ns}, {"after_ns", resource_start.after_ns},
            {"process_cpu_ns", resource_start.process_cpu_ns},
            {"working_set_bytes", resource_start.working_set_bytes}}))
            throw std::runtime_error("critical journal overrun");
        const auto formal_end = formal_start + static_cast<Nanoseconds>(std::llround(options.duration_s * 1e9));
        std::uint64_t consumed = 0, skips = 0, decoded = 0;
        Nanoseconds next_resource_ns = formal_start;
        while (clock.now_ns() < formal_end && !worker_done) {
            if (clock.now_ns() >= next_resource_ns) {
                const auto sample = process_resources(clock);
                if (!journal.push({{"event", "resource_sample"},
                    {"before_ns", sample.before_ns}, {"after_ns", sample.after_ns},
                    {"process_cpu_ns", sample.process_cpu_ns},
                    {"working_set_bytes", sample.working_set_bytes}}))
                    throw std::runtime_error("critical journal overrun");
                next_resource_ns += 1'000'000'000;
            }
            auto frame = latest.read_after(last_sequence, 50'000'000);
            if (!frame) continue;
            if (frame->capture_complete_ns < formal_start) {
                last_sequence = frame->sequence;
                continue;
            }
            const auto consume = clock.now_ns();
            if (consume >= formal_end) break;
            if (last_sequence && frame->sequence > last_sequence + 1)
                skips += frame->sequence - last_sequence - 1;
            const auto sequence_skip = last_sequence && frame->sequence > last_sequence + 1
                ? frame->sequence - last_sequence - 1 : 0;
            last_sequence = frame->sequence;
            ++consumed;
            if (!journal.push({{"event", "frame_consumed"}, {"frame_sequence", frame->sequence},
                {"consume_ns", consume}, {"capture_complete_ns", frame->capture_complete_ns},
                {"sequence_skip", sequence_skip}})) throw std::runtime_error("critical journal overrun");
            if (options.fixture) {
                if (auto counter = fixture_counter(*frame)) {
                    if ((options.fixture_schema == "legacy" &&
                         std::string_view(counter->schema) != "legacy_counter") ||
                        (options.fixture_schema == "native-v2" &&
                         std::string_view(counter->schema) != "native_v2_four_region"))
                        throw std::runtime_error("capture Fixture pixel schema differs from requested artifact");
                    ++decoded;
                    if (!journal.push({{"event", "fixture_counter"}, {"frame_sequence", frame->sequence},
                        {"capture_complete_ns", frame->capture_complete_ns},
                        {"counter", counter->value}, {"fixture_schema", counter->schema}}))
                        throw std::runtime_error("critical journal overrun");
                }
            }
            double delay = options.consumer_delay_ms;
            if (options.consumer_recover_after_s &&
                consume - formal_start >= static_cast<Nanoseconds>(std::llround(*options.consumer_recover_after_s * 1e9)))
                delay = 0;
            if (delay > 0)
                std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<Nanoseconds>(std::llround(delay * 1e6))));
        }
        if (worker_done && clock.now_ns() < formal_end) {
            std::exception_ptr error;
            { std::lock_guard lock(fault_mutex); error = worker_fault; }
            throw std::runtime_error("capture worker ended before formal measurement completed" +
                (error ? ": " + tokenless_failure(error) : std::string{}));
        }
        const auto actual_end = clock.now_ns();
        event("STOPPING", actual_end);
        const auto resource_end = process_resources(clock);
        if (!journal.push({{"event", "resource_boundary"}, {"phase", "end"},
            {"before_ns", resource_end.before_ns}, {"after_ns", resource_end.after_ns},
            {"process_cpu_ns", resource_end.process_cpu_ns},
            {"working_set_bytes", resource_end.working_set_bytes}}))
            throw std::runtime_error("critical journal overrun");
        stop_worker();
        std::exception_ptr error;
        { std::lock_guard lock(fault_mutex); error = worker_fault; }
        if (error) throw std::runtime_error(tokenless_failure(error));
        if (journal.faulted()) throw std::runtime_error("journal failed or overran");
        const auto raw = options.output_dir / "capture.jsonl";
        auto summary = analyze_capture_jsonl(raw);
        summary["consumed_count"] = consumed;
        summary["consumer_skip_count"] = skips;
        summary["fixture_consumer_decoded"] = decoded;
        summary["capture_stats"] = capture ? json{{"valid", capture->stats().valid},
            {"inactive", capture->stats().inactive}, {"invalid", capture->stats().invalid},
            {"source_gaps", capture->stats().source_gaps},
            {"relative_stale_drops", capture->stats().relative_stale_drops}} : json(nullptr);
        summary["source_absolute_age"] = nullptr;
        summary["performance_pass"] = nullptr;
        summary["log_path"] = std::filesystem::absolute(raw).string();
        std::ofstream file(options.output_dir / "summary.json", std::ios::binary);
        file << summary.dump(2) << '\n';
        std::cout << summary.dump(2) << '\n';
        if (options.fixture && decoded < consumed) throw std::runtime_error("Fixture pixels could not be decoded on all consumed frames");
    } catch (...) {
        if (worker.joinable()) stop_worker();
        throw;
    }
}

void run_capture_campaign(CaptureBenchOptions options, int normal_runs,
                          bool include_stress, double stability_s) {
    if (normal_runs < 1 || normal_runs > 20 || !std::isfinite(stability_s) ||
        stability_s < 0 || stability_s > 3600 ||
        options.output_dir.empty() || std::filesystem::exists(options.output_dir))
        throw std::invalid_argument("invalid or existing capture campaign output");
    const auto root = options.output_dir;
    std::filesystem::create_directories(root);
    struct Case { std::string name; CaptureBenchOptions options; };
    std::vector<Case> cases;
    for (int i = 0; i < normal_runs; ++i)
        cases.push_back({"normal-" + std::to_string(i+1), options});
    if (include_stress) {
        auto slow = options; slow.duration_s = 30; slow.consumer_delay_ms = 50;
        cases.push_back({"slow50", slow});
        auto recovery = options; recovery.duration_s = 30; recovery.consumer_delay_ms = 100;
        recovery.consumer_recover_after_s = recovery.duration_s / 2;
        cases.push_back({"recover100", recovery});
        auto pause = options; pause.duration_s = 30; pause.receiver_pause_ms = 500;
        cases.push_back({"pause500", pause});
        auto guarded = pause; guarded.max_relative_lag_ms = 100;
        cases.push_back({"pause500-guard100", guarded});
        auto loaded = options; loaded.duration_s = 30; loaded.load = true;
        cases.push_back({"host-load", loaded});
    }
    if (stability_s > 0) {
        auto long_run = options; long_run.duration_s = stability_s;
        cases.push_back({"stability", long_run});
    }
    json intended = json::array();
    for (const auto& item : cases)
        intended.push_back({{"name", item.name}, {"warmup_s", item.options.warmup_s},
                            {"duration_s", item.options.duration_s},
                            {"consumer_delay_ms", item.options.consumer_delay_ms},
                            {"host_load", item.options.load},
                            {"receiver_pause_ms", item.options.receiver_pause_ms},
                            {"max_relative_lag_ms", item.options.max_relative_lag_ms
                                ? json(*item.options.max_relative_lag_ms) : json(nullptr)}});
    {
        std::ofstream plan(root / "campaign-plan.json", std::ios::binary);
        plan << json{{"schema_version", 2}, {"order_fixed_before_runs", true},
                     {"serial", options.serial}, {"backend", options.backend},
                     {"transport", options.transport},
                     {"image_format", options.image_format}, {"row_order", options.row_order},
                     {"max_rgb_bytes", options.max_rgb_bytes},
                     {"grpc_endpoint", options.grpc_endpoint},
                     {"cases", intended}}.dump(2) << '\n';
    }
    json completed = json::array();
    bool all_succeeded = true;
    for (auto& item : cases) {
        item.options.output_dir = root / item.name;
        try {
            run_capture_bench(item.options);
            std::ifstream summary_file(item.options.output_dir / "summary.json", std::ios::binary);
            if (!summary_file) throw std::runtime_error("campaign summary missing");
            auto summary = json::parse(summary_file);
            completed.push_back({{"name", item.name}, {"returncode", 0},
                {"raw_sha256", summary.at("raw_sha256")},
                {"capture_events", summary.at("capture_events")},
                {"fixture_distinct_hz", summary.at("fixture_distinct_hz")},
                {"fixture_counter_span_hz", summary.at("fixture_counter_span_hz")},
                {"arrival_interval_ms", summary.at("arrival_interval_ms")},
                {"host_residency_ms", summary.at("host_residency_ms")},
                {"no_frame_gap_ms", summary.at("no_frame_gap_ms")}});
        } catch (const std::exception& error) {
            all_succeeded = false;
            completed.push_back({{"name", item.name}, {"returncode", 1}, {"error", error.what()}});
        }
        std::ofstream progress(root / "campaign-results.json", std::ios::binary);
        progress << json{{"schema_version", 2}, {"complete", completed.size() == cases.size()},
                         {"all_succeeded", all_succeeded},
                         {"runs", completed}}.dump(2) << '\n';
    }
    if (!all_succeeded) throw std::runtime_error("capture campaign completed with failed runs; inspect campaign-results.json");
}

} // namespace pas
