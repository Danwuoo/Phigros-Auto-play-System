#include "pas/core.hpp"
#include "pas/config.hpp"
#include "pas/emulator.hpp"
#include "pas/analysis.hpp"
#include "pas/adb.hpp"
#include "pas/bench.hpp"
#include "pas/touch_bench.hpp"
#include "pas/preview.hpp"
#include "pas/journal.hpp"
#include "pas/native_capture.hpp"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using json = nlohmann::json;
using namespace pas;

class EventLog {
public:
    explicit EventLog(const std::string& path, std::string clock_domain = "host_qpc_ns")
        : clock_domain_(std::move(clock_domain)) {
        if (!path.empty()) {
            const auto parent = std::filesystem::path(path).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            file_.open(path, std::ios::binary);
            if (!file_) throw std::runtime_error("cannot open JSONL log");
        }
    }
    void write(json record) {
        record["schema_version"] = 2;
        record["clock_domain"] = clock_domain_;
        if (file_.is_open()) file_ << record.dump() << '\n';
    }
private:
    std::ofstream file_;
    std::string clock_domain_;
};

void synthetic(int count, int fps, double recognition_delay_ms, const std::string& log_path) {
    if (count < 1 || count > 10'000 || fps < 2 || fps > 1000 ||
        !std::isfinite(recognition_delay_ms) || recognition_delay_ms < 0 || recognition_delay_ms > 1000)
        throw std::invalid_argument("invalid synthetic arguments");
    FakeClock clock;
    GreenTargetDetector detector(clock);
    VelocityTracker tracker;
    LineCrossingPredictor predictor(48);
    FakeTouchBackend backend(clock);
    EventLog log(log_path, "synthetic_virtual_ns");
    const Nanoseconds interval = static_cast<Nanoseconds>(std::llround(1e9 / fps));
    const Nanoseconds delay = static_cast<Nanoseconds>(std::llround(recognition_delay_ms * 1e6));
    struct Truth { Nanoseconds appearance, crossing; bool hit = false; };
    std::vector<Truth> truths;
    for (int i = 0; i < count; ++i) truths.push_back({i * 1'000'000'000LL, i * 1'000'000'000LL + 500'000'000});
    int attempts = 0;
    backend.on_touch = [&](const TouchCommand& command, Nanoseconds when) {
        if (command.phase != Phase::down) return;
        ++attempts;
        const auto index = static_cast<std::size_t>(when / 1'000'000'000LL);
        if (index < truths.size() && !truths[index].hit &&
            std::abs(when - truths[index].crossing) <= 25'000'000 &&
            std::abs(command.x - 32) <= 3 && std::abs(command.y - 48) <= 3) truths[index].hit = true;
    };
    ContactScheduler scheduler(clock, backend);
    scheduler.set_gate(1, true, 0);
    std::vector<double> arrival, recognition, prediction_error, scheduling;
    Nanoseconds next_frame = 0;
    std::uint64_t sequence = 0;
    Nanoseconds last_frame = 0;
    std::uint64_t skipped = 0;
    std::uint64_t effects = 0;
    bool effect_visible_before = false;
    const auto dispatch_due = [&] {
        for (const auto& receipt : scheduler.run_due()) {
            if (receipt.command.phase == Phase::down)
                scheduling.push_back((receipt.injection_start_ns - receipt.command.scheduled_ns) / 1e6);
        }
    };
    while (next_frame < count * 1'000'000'000LL || scheduler.pending_count()) {
        Nanoseconds now = next_frame < count * 1'000'000'000LL ? next_frame : std::numeric_limits<Nanoseconds>::max();
        if (const auto due = scheduler.next_due_ns()) now = std::min(now, *due);
        if (now == std::numeric_limits<Nanoseconds>::max()) break;
        now = std::max(now, clock.now_ns());
        clock.set(now);
        dispatch_due();
        if (now < next_frame || next_frame >= count * 1'000'000'000LL) continue;
        Frame frame;
        frame.sequence = ++sequence; frame.width = 64; frame.height = 64; frame.stride = 192;
        frame.capture_complete_ns = now;
        frame.rgb.assign(64 * 64 * 3, 0);
        for (int x = 0; x < 64; ++x) {
            auto* p = frame.rgb.data() + (48 * 64 + x) * 3;
            p[0] = p[1] = p[2] = 128;
        }
        const auto index = static_cast<std::size_t>(now / 1'000'000'000LL);
        if (index < truths.size()) {
            const auto elapsed = now - truths[index].appearance;
            const int y = std::clamp(static_cast<int>(std::llround(8 + 40.0 * elapsed / 500'000'000)), 0, 63);
            if (truths[index].hit) {
                for (int dy = -3; dy <= 3; ++dy) for (int dx = -3; dx <= 3; ++dx) {
                    if (48 + dy >= 0 && 48 + dy < 64 && 32 + dx >= 0 && 32 + dx < 64)
                        frame.rgb[((48 + dy) * 64 + 32 + dx) * 3] = 255;
                }
            } else if (elapsed <= 650'000'000) {
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                    if (y + dy >= 0 && y + dy < 64) frame.rgb[((y + dy) * 64 + 32 + dx) * 3 + 1] = 255;
            }
        }
        if (sequence > 1) arrival.push_back((now - last_frame) / 1e6);
        last_frame = now;
        // Model the dedicated scheduler continuing to run while perception
        // processes this captured frame. Do not advance past pending deadlines.
        const auto recognition_complete = now + delay;
        while (const auto due = scheduler.next_due_ns()) {
            if (*due > recognition_complete) break;
            clock.set(std::max(clock.now_ns(), *due));
            dispatch_due();
        }
        clock.set(recognition_complete);
        // Effect evidence comes from the rendered pixels. Truth is used only
        // by the fixture renderer and offline scoring, never by perception.
        int effect_pixels = 0;
        for (std::size_t pixel = 0; pixel + 2 < frame.rgb.size(); pixel += 3)
            if (frame.rgb[pixel] > 180 && frame.rgb[pixel + 1] < 80 && frame.rgb[pixel + 2] < 80)
                ++effect_pixels;
        const bool effect_visible = effect_pixels >= 9;
        if (effect_visible && !effect_visible_before) ++effects;
        effect_visible_before = effect_visible;
        auto observation = detector.detect(frame);
        // Refresh the gate only after pixels have been processed; the
        // evidence timestamp remains the frame's capture time.
        scheduler.set_gate(1, true, frame.capture_complete_ns);
        recognition.push_back((clock.now_ns() - now) / 1e6);
        auto track = tracker.update(observation);
        if (track) if (auto intent = predictor.predict(*track)) {
            if (intent->predicted_hit_ns >= clock.now_ns()) {
                auto candidate = ContactPlan{1, intent->intent_id, sequence, now,
                    intent->predicted_hit_ns + 30'000'000, sequence, intent->basis,
                    {{Phase::down, intent->x, intent->y, intent->predicted_hit_ns},
                     {Phase::up, intent->x, intent->y, intent->predicted_hit_ns + 20'000'000}}};
                scheduler.submit(std::move(candidate));
                prediction_error.push_back((intent->predicted_hit_ns - truths[index].crossing) / 1e6);
            }
        }
        log.write({{"event", "frame_processed"}, {"frame_sequence", sequence},
                   {"capture_complete_ns", now}, {"recognition_complete_ns", clock.now_ns()}});
        next_frame += interval;
        if (clock.now_ns() > next_frame) {
            const auto missed = (clock.now_ns() - next_frame) / interval + 1;
            next_frame += missed * interval;
            skipped += static_cast<std::uint64_t>(missed);
        }
    }
    const auto hits = std::count_if(truths.begin(), truths.end(), [](const Truth& t) { return t.hit; });
    json result = {{"mode", "synthetic_virtual_clock"}, {"targets", count}, {"hits", hits},
                   {"misses", count - hits}, {"touch_attempts", attempts},
                   {"false_touches", attempts - hits}, {"effects_seen", effects},
                   {"frames_processed", sequence}, {"capture_deadlines_skipped", skipped},
                   {"capture_interval_ms", distribution(arrival)},
                   {"recognition_duration_ms", distribution(recognition)},
                   {"prediction_error_ms", distribution(prediction_error)},
                   {"schedule_error_ms", distribution(scheduling)},
                   {"log_path", log_path.empty() ? json(nullptr) : json(log_path)}};
    log.write(json{{"event", "summary"}, {"summary", result}});
    std::cout << result.dump(2) << '\n';
}

json schedule_bench(int samples, int warmup, double interval_ms, bool load,
                    const std::string& log_path) {
    if (samples < 1 || samples > 100'000 || warmup < 0 || warmup > 100'000 ||
        !std::isfinite(interval_ms) || interval_ms <= 0 || interval_ms > 1000)
        throw std::invalid_argument("invalid schedule benchmark options");
    HostClock clock;
    EventLog log(log_path);
    std::atomic<std::uint64_t> load_ticks = 0;
    std::jthread load_worker;
    if (load) load_worker = std::jthread([&](std::stop_token stop) {
        while (!stop.stop_requested()) load_ticks.fetch_add(1, std::memory_order_relaxed);
    });
    const auto interval = static_cast<Nanoseconds>(std::llround(interval_ms * 1e6));
    const auto start = clock.now_ns();
    std::vector<double> errors;
    errors.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < warmup + samples; ++i) {
        const auto due = start + static_cast<Nanoseconds>(i + 1) * interval;
        while (true) {
            const auto remaining = due - clock.now_ns();
            if (remaining <= 0) break;
            if (remaining > 2'000'000)
                std::this_thread::sleep_for(std::chrono::nanoseconds(remaining - 1'000'000));
            else std::this_thread::yield();
        }
        const auto actual = clock.now_ns();
        if (i < warmup) continue;
        const auto error_ms = (actual - due) / 1e6;
        errors.push_back(error_ms);
        log.write({{"event", "schedule_sample"}, {"sample", i - warmup},
                   {"scheduled_ns", due}, {"dispatch_ns", actual}, {"error_ms", error_ms}});
    }
    load_worker.request_stop();
    if (load_worker.joinable()) load_worker.join();
    json result = {{"mode", "schedule_bench_native"}, {"samples", samples},
                   {"warmup", warmup}, {"interval_ms", interval_ms},
                   {"load", load}, {"load_ticks", load_ticks.load()},
                   {"clock_domain", "host_qpc_ns"}, {"qpc_frequency", clock.frequency()},
                   {"schedule_error_ms", distribution(errors)}};
    log.write({{"event", "summary"}, {"summary", result}});
    return result;
}

json fake_capture_bench(double duration_s, double warmup_s, double interval_ms,
                        int width, int height, double consumer_delay_ms, bool load,
                        const std::string& log_path, const std::string& mode) {
    if (!std::isfinite(duration_s) || !std::isfinite(warmup_s) ||
        !std::isfinite(interval_ms) || !std::isfinite(consumer_delay_ms) ||
        duration_s <= 0 || duration_s > 3600 || warmup_s < 0 || warmup_s > 60 ||
        interval_ms < 0.001 || interval_ms > 1000 || width <= 0 || height <= 0 ||
        static_cast<std::uint64_t>(width) * height * 3 > 16 * 1024 * 1024 ||
        consumer_delay_ms < 0 || consumer_delay_ms > 1000)
        throw std::invalid_argument("invalid fake capture benchmark options");
    HostClock clock;
    LatestFrame latest(width, height, 3, &clock);
    EventLog log(log_path);
    std::atomic<std::uint64_t> generated = 0;
    std::atomic<std::uint64_t> load_ticks = 0;
    std::jthread load_worker;
    if (load) load_worker = std::jthread([&](std::stop_token stop) {
        while (!stop.stop_requested()) load_ticks.fetch_add(1, std::memory_order_relaxed);
    });
    const auto interval = static_cast<Nanoseconds>(std::llround(interval_ms * 1e6));
    const auto start = clock.now_ns();
    std::vector<double> source_intervals;
    std::jthread producer([&](std::stop_token stop) {
        std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
        std::uint64_t sequence = 0;
        Nanoseconds previous_source = 0;
        while (!stop.stop_requested()) {
            const auto now = clock.now_ns();
            if (previous_source && source_intervals.size() < 100'000)
                source_intervals.push_back((now - previous_source) / 1e6);
            previous_source = now;
            // The source changes visible pixels for each generated frame.
            for (std::size_t bit = 0; bit < std::min<std::size_t>(8, rgb.size()); ++bit)
                rgb[bit] = static_cast<std::uint8_t>((sequence >> (bit * 8)) & 255);
            Frame frame;
            frame.sequence = ++sequence;
            frame.width = width; frame.height = height; frame.stride = width * 3;
            frame.capture_complete_ns = now;
            frame.pixels_ready_ns = clock.now_ns();
            latest.publish(rgb.data(), rgb.size(), std::move(frame));
            generated = sequence;
            const auto due = start + static_cast<Nanoseconds>(sequence) * interval;
            const auto remaining = due - clock.now_ns();
            if (remaining > 0) std::this_thread::sleep_for(std::chrono::nanoseconds(remaining));
        }
        latest.close();
    });
    const auto formal_start = start + static_cast<Nanoseconds>(std::llround(warmup_s * 1e9));
    const auto formal_end = formal_start + static_cast<Nanoseconds>(std::llround(duration_s * 1e9));
    std::uint64_t last = 0, consumed = 0;
    std::vector<double> intervals, residency;
    Nanoseconds previous = 0;
    while (clock.now_ns() < formal_end) {
        auto frame = latest.read_after(last, 20'000'000);
        if (!frame) continue;
        const auto now = clock.now_ns();
        last = frame->sequence;
        if (now >= formal_end) break;
        if (now < formal_start || frame->capture_complete_ns < formal_start) continue;
        ++consumed;
        if (previous && intervals.size() < 100'000) intervals.push_back((now - previous) / 1e6);
        previous = now;
        if (residency.size() < 100'000)
            residency.push_back((now - frame->capture_complete_ns) / 1e6);
        log.write({{"event", "frame_consumed"}, {"frame_sequence", frame->sequence},
                   {"consume_ns", now}, {"capture_complete_ns", frame->capture_complete_ns}});
        if (consumer_delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::nanoseconds(
                static_cast<Nanoseconds>(std::llround(consumer_delay_ms * 1e6))));
    }
    producer.request_stop();
    producer.join();
    load_worker.request_stop();
    if (load_worker.joinable()) load_worker.join();
    const auto counts = latest.counters();
    json result = {{"mode", mode}, {"execution", "single_process_thread"},
                   {"clock_domain", "host_qpc_ns"}, {"qpc_frequency", clock.frequency()},
                   {"duration_s", duration_s}, {"warmup_s", warmup_s},
                   {"capture_interval_target_ms", interval_ms},
                   {"consumer_delay_ms", consumer_delay_ms},
                   {"generated", generated.load()}, {"published", counts.published},
                   {"overwritten", counts.overwritten}, {"pool_drops", counts.pool_drops},
                   {"consumer_skips", counts.consumer_skips}, {"consumed", consumed},
                   {"source_interval_ms", distribution(source_intervals)},
                   {"consume_interval_ms", distribution(intervals)},
                   {"host_residency_ms", distribution(residency)},
                   {"load", load}, {"load_ticks", load_ticks.load()}};
    log.write({{"event", "summary"}, {"summary", result}});
    return result;
}

void run_observe(const std::string& config_path, double duration_s, bool no_preview,
                 const std::string& launch_package = "", double stale_ms = 100) {
    if (!std::isfinite(duration_s) || duration_s <= 0 || duration_s > 3600)
        throw std::invalid_argument("duration outside 0..3600s");
    if (!std::isfinite(stale_ms) || stale_ms < 5 || stale_ms > 5000)
        throw std::invalid_argument("stale-ms outside 5..5000");
    const auto config = load_config(config_path);
    HostClock clock;
    LatestFrame latest(config.width, config.height, 3, &clock);
    const auto wall_id = std::chrono::system_clock::now().time_since_epoch().count();
    const auto run_dir = std::filesystem::path(config.log_dir) /
        ("cpp-observe-" + std::to_string(wall_id));
    std::filesystem::create_directories(run_dir);
    {
        std::ofstream manifest(run_dir / "manifest.json", std::ios::binary);
        manifest << json{{"schema_version", 2}, {"mode", "observe"},
                         {"config", config.public_json}, {"clock_domain", "host_qpc_ns"},
                         {"qpc_frequency", clock.frequency()}, {"input_created", false},
                         {"preview_requested", !no_preview && config.preview_hz > 0},
                         {"stale_ms", stale_ms}}.dump(2) << '\n';
    }
    Journal log(run_dir / "events.jsonl");
    const auto record = [&](json value) {
        if (!log.push(std::move(value))) throw std::runtime_error("observe journal fault or overrun");
    };
    std::unique_ptr<PreviewWindow> preview;
    if (!no_preview && config.preview_hz > 0)
        preview = std::make_unique<PreviewWindow>(config.width, config.height);
    std::unique_ptr<GrpcCapture> capture;
    if (config.capture_kind == "emulator-grpc") {
        GrpcEndpoint endpoint;
        if (config.endpoint.empty()) endpoint = discover_endpoint(config.serial);
        else {
            std::ifstream file(config.token_file, std::ios::binary);
            if (!file) throw std::runtime_error("cannot read gRPC token file");
            std::getline(file, endpoint.token);
            if (!endpoint.token.empty() && endpoint.token.back() == '\r') endpoint.token.pop_back();
            if (endpoint.token.empty()) throw std::runtime_error("empty gRPC token file");
            endpoint.target = config.endpoint;
            endpoint.instance = "explicit";
        }
        CaptureOptions options;
        options.width = config.width;
        options.height = config.height;
        options.source_rotation = config.source_rotation;
        capture = std::make_unique<GrpcCapture>(clock, std::move(endpoint), options);
    }
    std::mutex fault_mutex;
    std::exception_ptr worker_fault;
    std::atomic<bool> done = false;
    std::jthread worker([&](std::stop_token stop) {
        try {
            if (capture) {
                capture->stream(stop, [&](Frame&& frame) {
                    frame.epoch = 1;
                    latest.publish(frame.rgb.data(), frame.rgb.size(), frame);
                });
            } else {
                const auto interval = std::chrono::nanoseconds(16'666'667);
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(config.width) * config.height * 3);
                std::uint64_t sequence = 0;
                while (!stop.stop_requested()) {
                    Frame frame;
                    frame.sequence = ++sequence;
                    frame.epoch = 1;
                    frame.width = config.width;
                    frame.height = config.height;
                    frame.stride = config.width * 3;
                    frame.source_rotation = config.source_rotation;
                    frame.capture_complete_ns = clock.now_ns();
                    frame.pixels_ready_ns = frame.capture_complete_ns;
                    pixels[(sequence % pixels.size())] = static_cast<std::uint8_t>(sequence & 255);
                    latest.publish(pixels.data(), pixels.size(), frame);
                    std::this_thread::sleep_for(interval);
                }
            }
        } catch (...) {
            std::lock_guard lock(fault_mutex);
            worker_fault = std::current_exception();
        }
        done = true;
        latest.close();
    });
    const auto start = clock.now_ns();
    const auto end = start + static_cast<Nanoseconds>(std::llround(duration_s * 1e9));
    std::uint64_t sequence = 0, consumed = 0;
    std::vector<double> intervals, host_residency;
    Nanoseconds previous = 0;
    Nanoseconds previous_preview = 0;
    std::uint64_t preview_draws = 0;
    std::string state = "CONNECTING";
    record({{"event", "session_state"}, {"state", state}, {"monotonic_ns", start}});
    bool launched = false;
    while (clock.now_ns() < end && !done) {
        if (preview && !preview->pump()) preview.reset();
        auto frame = latest.read_after(sequence,
            static_cast<Nanoseconds>(std::llround(stale_ms * 1e6)));
        if (!frame) {
            if (state == "NAVIGATING") {
                std::string probe_result = "unavailable";
                const auto before_probe = sequence;
                if (capture) {
                    if (auto last = latest.peek())
                        probe_result = capture->probe_pixels(*last, std::chrono::milliseconds(500));
                }
                // A valid stream frame may arrive while the health probe is in flight.
                const auto newest = latest.peek();
                if (done) continue;
                if (newest && newest->sequence > before_probe) {
                    record({{"event", "probe_recovered_new_stream_frame"},
                            {"before_sequence", before_probe},
                            {"new_sequence", newest->sequence},
                            {"probe_result", probe_result}, {"monotonic_ns", clock.now_ns()}});
                    continue;
                }
                state = probe_result == "static" ? "DEGRADED_STATIC" : "DEGRADED_STREAM";
                record({{"event", "session_state"}, {"state", state},
                           {"probe_result", probe_result}, {"frame_fresh", false},
                           {"monotonic_ns", clock.now_ns()}});
            }
            continue;
        }
        if (previous && intervals.size() < 100'000)
            intervals.push_back((frame->capture_complete_ns - previous) / 1e6);
        previous = frame->capture_complete_ns;
        sequence = frame->sequence;
        ++consumed;
        if (host_residency.size() < 100'000)
            host_residency.push_back((clock.now_ns() - frame->capture_complete_ns) / 1e6);
        if (preview && (!previous_preview || clock.now_ns() - previous_preview >=
            static_cast<Nanoseconds>(std::llround(1e9 / config.preview_hz)))) {
            preview->draw(*frame);
            previous_preview = clock.now_ns();
            ++preview_draws;
        }
        if (!launch_package.empty() && !launched) {
            state = "LAUNCHING";
            record({{"event", "session_state"}, {"state", state},
                       {"monotonic_ns", clock.now_ns()}, {"source_frame", sequence}});
            launch_android_package(find_adb(), config.serial, launch_package);
            launched = true;
            continue;
        }
        if (state != "NAVIGATING") {
            state = "NAVIGATING";
            record({{"event", "session_state"}, {"state", state},
                       {"frame_fresh", true}, {"monotonic_ns", clock.now_ns()}});
        }
        record({{"event", "frame_consumed"}, {"frame_sequence", sequence},
                   {"capture_complete_ns", frame->capture_complete_ns},
                   {"pixels_ready_ns", frame->pixels_ready_ns},
                   {"published_ns", frame->published_ns},
                   {"consume_ns", clock.now_ns()}, {"width", frame->width},
                   {"height", frame->height}, {"source_rotation", frame->source_rotation},
                   {"source_sequence", frame->source_sequence ? json(*frame->source_sequence) : json(nullptr)},
                   {"source_timestamp_us", frame->source_timestamp_us ? json(*frame->source_timestamp_us) : json(nullptr)},
                   {"epoch", frame->epoch}, {"generation", frame->generation},
                   {"geometry_version", frame->geometry_version}});
    }
    const bool worker_ended_early = done && clock.now_ns() < end;
    worker.request_stop();
    if (capture) capture->cancel();
    worker.join();
    {
        std::lock_guard lock(fault_mutex);
        if (worker_fault) std::rethrow_exception(worker_fault);
    }
    if (worker_ended_early) throw std::runtime_error("observe capture worker ended before requested duration");
    const auto counts = latest.counters();
    if (!consumed) throw std::runtime_error("observe session had no valid frame");
    json summary = {{"mode", "observe"}, {"state", "STOPPED"}, {"input_created", false},
                    {"frames_consumed", consumed}, {"published", counts.published},
                    {"overwritten", counts.overwritten}, {"pool_drops", counts.pool_drops},
                    {"consumer_skips", counts.consumer_skips},
                    {"preview_draws", preview_draws},
                    {"capture_interval_ms", distribution(intervals)},
                    {"host_residency_ms", distribution(host_residency)},
                    {"source_absolute_age", nullptr}, {"run_dir", std::filesystem::absolute(run_dir).string()}};
    record({{"event", "summary"}, {"summary", summary}});
    log.close();
    if (log.faulted()) throw std::runtime_error("observe journal failed");
    std::ofstream output(run_dir / "summary.json", std::ios::binary);
    output << summary.dump(2) << '\n';
    std::cout << summary.dump(2) << '\n';
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"Phigros Auto-play System research CLI (C++20)"};
    app.require_subcommand(1);
    int count = 30, fps = 60;
    double recognition_delay_ms = 0;
    std::string log_path;
    auto* synthetic_cmd = app.add_subcommand("synthetic", "Run pixel-to-touch synthetic fixture");
    synthetic_cmd->add_option("--count", count)->check(CLI::PositiveNumber);
    synthetic_cmd->add_option("--fps", fps)->check(CLI::PositiveNumber);
    synthetic_cmd->add_option("--recognition-delay-ms", recognition_delay_ms)->check(CLI::NonNegativeNumber);
    synthetic_cmd->add_option("--log", log_path);
    int schedule_samples = 100, schedule_warmup = 10;
    double schedule_interval_ms = 10;
    bool schedule_load = false;
    std::string schedule_log;
    auto* schedule_cmd = app.add_subcommand("schedule-bench", "Measure host QPC scheduling latency");
    schedule_cmd->add_option("--samples", schedule_samples);
    schedule_cmd->add_option("--warmup", schedule_warmup);
    schedule_cmd->add_option("--interval-ms", schedule_interval_ms);
    schedule_cmd->add_flag("--load", schedule_load);
    schedule_cmd->add_option("--log", schedule_log);
    double fake_duration_s = 3, fake_warmup_s = 1, fake_interval_ms = 16.67;
    double fake_consumer_delay_ms = 0;
    int fake_width = 1280, fake_height = 720;
    bool fake_load = false, fake_child_load = false, fake_no_log_cost = false;
    std::string fake_log;
    auto* offline_cmd = app.add_subcommand("offline-capture-bench", "Single-process fake pixel capture benchmark");
    offline_cmd->add_option("--duration-s", fake_duration_s);
    offline_cmd->add_option("--warmup-s", fake_warmup_s);
    offline_cmd->add_option("--fake-interval-ms", fake_interval_ms);
    offline_cmd->add_option("--width", fake_width);
    offline_cmd->add_option("--height", fake_height);
    offline_cmd->add_option("--consumer-delay-ms", fake_consumer_delay_ms);
    offline_cmd->add_flag("--load", fake_load);
    offline_cmd->add_flag("--child-load", fake_child_load);
    offline_cmd->add_flag("--no-log-cost", fake_no_log_cost);
    offline_cmd->add_option("--log", fake_log);
    double buffer_duration_s = 1, buffer_interval_ms = 2, buffer_consumer_delay_ms = 20;
    std::string buffer_log;
    auto* buffer_cmd = app.add_subcommand("buffer-bench", "Latest-frame producer and slow consumer");
    buffer_cmd->add_option("--duration-s", buffer_duration_s);
    buffer_cmd->add_option("--capture-interval-ms", buffer_interval_ms);
    buffer_cmd->add_option("--consumer-delay-ms", buffer_consumer_delay_ms);
    buffer_cmd->add_option("--log", buffer_log);
    std::string config_path, run_mode = "observe";
    double run_duration_s = 30;
    double run_stale_ms = 100;
    bool no_preview = false;
    auto* run_cmd = app.add_subcommand("run", "Run capture-only observation; assist is disabled");
    run_cmd->add_option("--config", config_path)->required();
    run_cmd->add_option("--mode", run_mode);
    run_cmd->add_option("--duration-s", run_duration_s)->check(CLI::PositiveNumber);
    run_cmd->add_option("--stale-ms", run_stale_ms)->check(CLI::PositiveNumber);
    run_cmd->add_flag("--no-preview", no_preview);
    std::string probe_serial;
    auto* probe_cmd = app.add_subcommand("probe", "Read-only ADB device inventory");
    probe_cmd->add_option("--serial", probe_serial);
    std::string session_package, session_config;
    double session_duration_s = 30;
    auto* session_cmd = app.add_subcommand("start-session", "Capture-first Android package launch and observation");
    session_cmd->add_option("--config", session_config)->required();
    session_cmd->add_option("--package", session_package)->required();
    session_cmd->add_option("--duration-s", session_duration_s)->check(CLI::PositiveNumber);
    CaptureBenchOptions bench;
    std::string capture_execution = "thread", bench_output;
    auto* windows_cmd = app.add_subcommand("capture-windows", "List explicit Win32 capture targets and client geometry");
    auto* bench_cmd = app.add_subcommand("capture-bench", "Measure gRPC, WGC, DXGI, scrcpy or diagnostic ADB/MMAP capture");
    bench_cmd->add_option("--serial", bench.serial)->required();
    bench_cmd->add_option("--capture-backend", bench.backend);
    bench_cmd->add_option("--capture-execution", capture_execution);
    bench_cmd->add_option("--grpc-transport", bench.transport);
    bench_cmd->add_option("--grpc-endpoint", bench.grpc_endpoint);
    bench_cmd->add_option("--grpc-token-file", bench.grpc_token_file);
    bench_cmd->add_option("--image-format", bench.image_format);
    bench_cmd->add_option("--row-order", bench.row_order);
    bench_cmd->add_option("--grpc-copy-mode", bench.grpc_copy_mode);
    bench_cmd->add_flag("--grpc-rotate-ccw", bench.grpc_rotate_ccw);
    bench_cmd->add_option("--run-class", bench.run_class);
    bench_cmd->add_option("--window-hwnd", bench.window_hwnd);
    bench_cmd->add_option("--monitor-index", bench.monitor_index);
    bench_cmd->add_option("--crop-x", bench.crop_x);
    bench_cmd->add_option("--crop-y", bench.crop_y);
    bench_cmd->add_option("--scrcpy-server", bench.scrcpy_server);
    bench_cmd->add_option("--scrcpy-max-fps", bench.scrcpy_max_fps);
    bench_cmd->add_option("--scrcpy-video-bit-rate", bench.scrcpy_video_bit_rate);
    bench_cmd->add_option("--scrcpy-video-encoder", bench.scrcpy_video_encoder);
    bench_cmd->add_option("--memory-load-mib", bench.memory_load_mib);
    bench_cmd->add_option("--max-rgb-bytes", bench.max_rgb_bytes);
    bench_cmd->add_option("--width", bench.width);
    bench_cmd->add_option("--height", bench.height);
    bench_cmd->add_option("--source-rotation", bench.source_rotation);
    bench_cmd->add_option("--ready-timeout-s", bench.ready_timeout_s);
    bench_cmd->add_option("--warmup-s", bench.warmup_s);
    bench_cmd->add_option("--duration-s", bench.duration_s);
    bench_cmd->add_option("--consumer-delay-ms", bench.consumer_delay_ms);
    bench_cmd->add_option("--consumer-recover-after-s", bench.consumer_recover_after_s);
    bench_cmd->add_option("--receiver-pause-ms", bench.receiver_pause_ms);
    bench_cmd->add_flag("--load", bench.load);
    bench_cmd->add_option("--max-relative-lag-ms", bench.max_relative_lag_ms);
    bench_cmd->add_flag("--diagnostic-mmap", bench.diagnostic_mmap);
    bench_cmd->add_flag("--fixture", bench.fixture);
    bench_cmd->add_option("--fixture-schema", bench.fixture_schema);
    bench_cmd->add_option("--fixture-apk", bench.fixture_apk);
    bench_cmd->add_option("--output-dir", bench_output)->required();
    CaptureBenchOptions campaign;
    int campaign_normal_runs = 3;
    bool campaign_stress = false;
    double campaign_stability_s = 0;
    auto* campaign_cmd = app.add_subcommand("capture-campaign", "Run a fixed native Fixture capture sequence");
    campaign_cmd->add_option("--serial", campaign.serial)->required();
    campaign_cmd->add_option("--width", campaign.width);
    campaign_cmd->add_option("--height", campaign.height);
    campaign_cmd->add_option("--source-rotation", campaign.source_rotation);
    campaign_cmd->add_option("--capture-backend", campaign.backend);
    campaign_cmd->add_option("--window-hwnd", campaign.window_hwnd);
    campaign_cmd->add_option("--monitor-index", campaign.monitor_index);
    campaign_cmd->add_option("--crop-x", campaign.crop_x);
    campaign_cmd->add_option("--crop-y", campaign.crop_y);
    campaign_cmd->add_option("--scrcpy-server", campaign.scrcpy_server);
    campaign_cmd->add_option("--scrcpy-max-fps", campaign.scrcpy_max_fps);
    campaign_cmd->add_option("--scrcpy-video-bit-rate", campaign.scrcpy_video_bit_rate);
    campaign_cmd->add_option("--scrcpy-video-encoder", campaign.scrcpy_video_encoder);
    campaign_cmd->add_option("--memory-load-mib", campaign.memory_load_mib);
    campaign_cmd->add_option("--grpc-endpoint", campaign.grpc_endpoint);
    campaign_cmd->add_option("--grpc-token-file", campaign.grpc_token_file);
    campaign_cmd->add_option("--image-format", campaign.image_format);
    campaign_cmd->add_option("--row-order", campaign.row_order);
    campaign_cmd->add_option("--grpc-copy-mode", campaign.grpc_copy_mode);
    campaign_cmd->add_flag("--grpc-rotate-ccw", campaign.grpc_rotate_ccw);
    campaign_cmd->add_option("--max-rgb-bytes", campaign.max_rgb_bytes);
    campaign_cmd->add_option("--warmup-s", campaign.warmup_s);
    campaign_cmd->add_option("--duration-s", campaign.duration_s);
    campaign_cmd->add_option("--normal-runs", campaign_normal_runs);
    campaign_cmd->add_flag("--include-stress", campaign_stress);
    campaign_cmd->add_option("--stability-s", campaign_stability_s);
    campaign_cmd->add_option("--fixture-apk", campaign.fixture_apk)->required();
    campaign_cmd->add_option("--output-dir", campaign.output_dir)->required();
    CaptureBenchOptions five;
    int five_normal_runs = 3;
    double five_stability_s = 600;
    bool five_no_stress = false, five_plan_only = false;
    std::string five_revision, five_diff_hash;
    auto* five_cmd = app.add_subcommand("capture-five-campaign",
        "Prewrite and run a serial fixed-order five-path native Fixture campaign");
    five_cmd->add_option("--serial", five.serial)->required();
    five_cmd->add_option("--fixture-apk", five.fixture_apk)->required();
    five_cmd->add_option("--scrcpy-server", five.scrcpy_server)->required();
    five_cmd->add_option("--window-hwnd", five.window_hwnd)->required();
    five_cmd->add_option("--monitor-index", five.monitor_index);
    five_cmd->add_option("--crop-x", five.crop_x);
    five_cmd->add_option("--crop-y", five.crop_y);
    five_cmd->add_option("--wgc-crop-x", five.wgc_crop_x);
    five_cmd->add_option("--wgc-crop-y", five.wgc_crop_y);
    five_cmd->add_option("--dxgi-crop-x", five.dxgi_crop_x);
    five_cmd->add_option("--dxgi-crop-y", five.dxgi_crop_y);
    five_cmd->add_option("--width", five.width);
    five_cmd->add_option("--height", five.height);
    five_cmd->add_option("--source-rotation", five.source_rotation);
    five_cmd->add_option("--normal-runs", five_normal_runs);
    five_cmd->add_option("--stability-s", five_stability_s);
    five_cmd->add_option("--scrcpy-max-fps", five.scrcpy_max_fps);
    five_cmd->add_option("--scrcpy-video-bit-rate", five.scrcpy_video_bit_rate);
    five_cmd->add_option("--scrcpy-video-encoder", five.scrcpy_video_encoder);
    five_cmd->add_option("--source-revision", five_revision);
    five_cmd->add_option("--dirty-diff-sha256", five_diff_hash);
    five_cmd->add_flag("--no-stress", five_no_stress);
    five_cmd->add_flag("--plan-only", five_plan_only);
    five_cmd->add_option("--output-dir", five.output_dir)->required();
    std::string touch_config, touch_output;
    std::filesystem::path touch_fixture_apk;
    int touch_repetitions = 30;
    std::vector<std::string> touch_kinds = {"tap", "hold", "move", "flick", "pair", "simultaneous"};
    auto* touch_cmd = app.add_subcommand("touch-bench", "Verify isolated native Fixture pointer paths");
    touch_cmd->add_option("--config", touch_config)->required();
    touch_cmd->add_option("--repetitions", touch_repetitions);
    touch_cmd->add_option("--kinds", touch_kinds);
    touch_cmd->add_option("--output-dir", touch_output);
    touch_cmd->add_option("--fixture-apk", touch_fixture_apk);
    std::string batch_config, batch_output;
    std::filesystem::path batch_fixture_apk;
    int batch_repetitions = 30;
    auto* batch_cmd = app.add_subcommand("touch-batch-bench", "Verify two native Fixture pointers in one gRPC event");
    batch_cmd->add_option("--config", batch_config)->required();
    batch_cmd->add_option("--repetitions", batch_repetitions);
    batch_cmd->add_option("--output-dir", batch_output);
    batch_cmd->add_option("--fixture-apk", batch_fixture_apk);
    std::string disconnect_config, disconnect_output;
    std::filesystem::path disconnect_fixture_apk;
    auto* disconnect_cmd = app.add_subcommand("touch-disconnect-smoke", "Verify Fixture recovery after local touch channel loss");
    disconnect_cmd->add_option("--config", disconnect_config)->required();
    disconnect_cmd->add_option("--output-dir", disconnect_output);
    disconnect_cmd->add_option("--fixture-apk", disconnect_fixture_apk);
    std::string migrate_source, migrate_target;
    auto* config_cmd = app.add_subcommand("config", "Configuration utilities");
    auto* migrate_cmd = config_cmd->add_subcommand("migrate", "Convert schema 1 process profile to schema 2 thread profile");
    migrate_cmd->add_option("source", migrate_source)->required();
    migrate_cmd->add_option("target", migrate_target)->required();
    std::string analysis_path;
    auto* analyze_cmd = app.add_subcommand("analyze", "Recompute old or new JSONL without Python");
    auto* capture_analysis = analyze_cmd->add_subcommand("capture", "Capture window and distribution");
    capture_analysis->add_option("path", analysis_path)->required();
    auto* pause_analysis = analyze_cmd->add_subcommand("pause", "Receiver pause relative lag");
    pause_analysis->add_option("path", analysis_path)->required();
    auto* campaign_analysis = analyze_cmd->add_subcommand("campaign", "Recompute every campaign raw file and verify hashes");
    campaign_analysis->add_option("path", analysis_path)->required();
    try {
        app.parse(argc, argv);
        if (*synthetic_cmd) synthetic(count, fps, recognition_delay_ms, log_path);
        else if (*schedule_cmd)
            std::cout << schedule_bench(schedule_samples, schedule_warmup, schedule_interval_ms,
                                        schedule_load, schedule_log).dump(2) << '\n';
        else if (*offline_cmd) {
            if (fake_child_load) throw std::invalid_argument("--child-load is retired with process capture");
            if (fake_no_log_cost) throw std::invalid_argument("--no-log-cost is retired; this benchmark logs only consumed frames");
            std::cout << fake_capture_bench(fake_duration_s, fake_warmup_s, fake_interval_ms,
                fake_width, fake_height, fake_consumer_delay_ms, fake_load, fake_log,
                "offline_capture_native").dump(2) << '\n';
        } else if (*buffer_cmd)
            std::cout << fake_capture_bench(buffer_duration_s, 0, buffer_interval_ms,
                64, 64, buffer_consumer_delay_ms, false, buffer_log,
                "buffer_bench_native").dump(2) << '\n';
        else if (*run_cmd) {
            if (run_mode != "observe") throw std::invalid_argument("assist is disabled pending game and gate acceptance");
            run_observe(config_path, run_duration_s, no_preview, "", run_stale_ms);
        } else if (*config_cmd) {
            if (*migrate_cmd) migrate_config(migrate_source, migrate_target);
            else throw std::invalid_argument("choose a config subcommand");
        } else if (*analyze_cmd) {
            if (*capture_analysis) std::cout << analyze_capture_jsonl(analysis_path).dump(2) << '\n';
            else if (*pause_analysis) std::cout << analyze_pause_jsonl(analysis_path).dump(2) << '\n';
            else if (*campaign_analysis) std::cout << analyze_capture_campaign(analysis_path).dump(2) << '\n';
            else throw std::invalid_argument("choose an analyze subcommand");
        } else if (*windows_cmd) {
            std::cout << enumerate_capture_windows_json() << '\n';
        } else if (*probe_cmd) {
            std::cout << probe_adb(find_adb(), probe_serial).dump(2) << '\n';
        } else if (*session_cmd) {
            run_observe(session_config, session_duration_s, true, session_package);
        } else if (*bench_cmd) {
            if (capture_execution != "thread")
                throw std::invalid_argument("--capture-execution process is retired; use native thread mode explicitly");
            bench.output_dir = bench_output;
            run_capture_bench(bench);
        } else if (*campaign_cmd) {
            campaign.fixture = true;
            run_capture_campaign(campaign, campaign_normal_runs,
                                 campaign_stress, campaign_stability_s);
        } else if (*five_cmd) {
            run_five_capture_campaign(five, five_normal_runs, !five_no_stress,
                                      five_stability_s, five_revision, five_diff_hash,
                                      five_plan_only);
        } else if (*touch_cmd) {
            run_touch_bench(touch_config, touch_repetitions, touch_kinds, touch_output,
                            touch_fixture_apk);
        } else if (*batch_cmd) {
            run_touch_batch_bench(batch_config, batch_repetitions, batch_output,
                                  batch_fixture_apk);
        } else if (*disconnect_cmd) {
            run_touch_disconnect_smoke(disconnect_config, disconnect_output,
                                       disconnect_fixture_apk);
        }
        return 0;
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
