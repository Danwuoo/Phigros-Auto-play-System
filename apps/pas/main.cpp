#include "pas/core.hpp"
#include "pas/runtime.hpp"
#include "pas/game.hpp"
#include "pas/game_tracking.hpp"
#include "pas/game_dataset.hpp"
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
    std::string manual_config,manual_capability;
    bool manual_no_preview=false,manual_pixel_clips=false,manual_full_recording=false,manual_one_round=false;
    double round_watchdog_s=0;
    auto* manual_cmd=app.add_subcommand("manual-session","Current strategy: standby, manual Play, result, standby; Escape/Ctrl+C stops");
    manual_cmd->add_option("--config",manual_config)->required();
    manual_cmd->add_option("--capability",manual_capability)->required();
    manual_cmd->add_flag("--no-preview",manual_no_preview);
    manual_cmd->add_flag("--pixel-clips",manual_pixel_clips,"Bounded full-frame RGB clips for offline line research");
    manual_cmd->add_flag("--full-recording",manual_full_recording,"One manual round; lossless PNG for every received capture frame plus bounded pre-roll; automatic stop after result");
    manual_cmd->add_flag("--one-round",manual_one_round,"Stop and release after one manual round; does not enable full recording");
    manual_cmd->add_option("--round-watchdog-s",round_watchdog_s,"Optional abnormal round limit; zero disables; never a result detector")->check(CLI::Range(0.0,3600.0));
    std::string run_capability;
    double run_duration_s = 30;
    double run_stale_ms = 100;
    bool no_preview = false;
    bool manual_play = false;
    double wait_play_s = 60;
    bool keep_diagnostic_anomalies = false;
    bool keep_vision_dataset=false;
    std::string tracking_shadow;
    auto* run_cmd = app.add_subcommand("run", "Pixels-only observe, automatic PLAY, or gated real assist");
    run_cmd->add_option("--config", config_path)->required();
    run_cmd->add_option("--mode", run_mode);
    run_cmd->add_option("--capability", run_capability, "Historical touch report required for auto-start or assist");
    run_cmd->add_option("--duration-s", run_duration_s)->check(CLI::PositiveNumber);
    run_cmd->add_option("--stale-ms", run_stale_ms)->check(CLI::PositiveNumber);
    run_cmd->add_flag("--no-preview", no_preview);
    run_cmd->add_flag("--manual-play",manual_play,
        "Assist only: user presses PLAY; runtime injects gated gameplay only");
    auto* wait_play_option=run_cmd->add_option("--wait-play-s",wait_play_s,
        "Manual PLAY only: bounded wait before gameplay duration starts (default 60 seconds)")->check(CLI::Range(0.001,60.0));
    run_cmd->add_flag("--keep-diagnostic-anomalies",keep_diagnostic_anomalies,
        "Assist only: at most two events x four native ROI frames, encoded after input stops");
    run_cmd->add_flag("--keep-vision-dataset",keep_vision_dataset,
        "Assist only: opt-in 16 clips / 128 native ROI frames, encoded after input stops");
    run_cmd->add_option("--tracking-shadow",tracking_shadow,
        "Assist only: one dry latest-only byte_association or oc_observation worker");
    std::string preflight_config, preflight_capability;
    auto* preflight_cmd = app.add_subcommand("game-preflight", "Read-only live geometry and historical touch fingerprint check");
    preflight_cmd->add_option("--config", preflight_config)->required();
    preflight_cmd->add_option("--capability", preflight_capability)->required();
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
    bench_cmd->add_option("--grpc-read-chunk-kib", bench.grpc_read_chunk_kib)->check(CLI::IsMember({8, 64, 256}));
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
    bench_cmd->add_flag("--gpu-load", bench.gpu_load);
    bench_cmd->add_flag("--preview", bench.preview);
    bench_cmd->add_flag("--keep-diagnostic-image", bench.keep_diagnostic_image,
        "Keep one diagnostic PNG per run (default: no images written)");
    bench_cmd->add_option("--source-static-s", bench.source_static_s);
    bench_cmd->add_option("--max-relative-lag-ms", bench.max_relative_lag_ms);
    bench_cmd->add_flag("--diagnostic-mmap", bench.diagnostic_mmap);
    bench_cmd->add_flag("--fixture", bench.fixture);
    bench_cmd->add_flag("--fixture-position-truth", bench.fixture_position_truth);
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
    campaign_cmd->add_option("--max-relative-lag-ms", campaign.max_relative_lag_ms);
    campaign_cmd->add_option("--grpc-endpoint", campaign.grpc_endpoint);
    campaign_cmd->add_option("--grpc-token-file", campaign.grpc_token_file);
    campaign_cmd->add_option("--image-format", campaign.image_format);
    campaign_cmd->add_option("--row-order", campaign.row_order);
    campaign_cmd->add_option("--grpc-copy-mode", campaign.grpc_copy_mode);
    campaign_cmd->add_option("--grpc-read-chunk-kib", campaign.grpc_read_chunk_kib)->check(CLI::IsMember({8, 64, 256}));
    campaign_cmd->add_flag("--grpc-rotate-ccw", campaign.grpc_rotate_ccw);
    campaign_cmd->add_option("--max-rgb-bytes", campaign.max_rgb_bytes);
    campaign_cmd->add_option("--warmup-s", campaign.warmup_s);
    campaign_cmd->add_option("--duration-s", campaign.duration_s);
    campaign_cmd->add_option("--normal-runs", campaign_normal_runs);
    campaign_cmd->add_flag("--include-stress", campaign_stress);
    campaign_cmd->add_flag("--keep-diagnostic-image", campaign.keep_diagnostic_image,
        "Keep one diagnostic PNG per run (default: no images written)");
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
    five_cmd->add_flag("--grpc-rotate-ccw", five.grpc_rotate_ccw);
    five_cmd->add_option("--grpc-read-chunk-kib", five.grpc_read_chunk_kib)->check(CLI::IsMember({8, 64, 256}));
    five_cmd->add_option("--normal-runs", five_normal_runs);
    five_cmd->add_option("--stability-s", five_stability_s);
    five_cmd->add_option("--scrcpy-max-fps", five.scrcpy_max_fps);
    five_cmd->add_option("--scrcpy-video-bit-rate", five.scrcpy_video_bit_rate);
    five_cmd->add_option("--scrcpy-video-encoder", five.scrcpy_video_encoder);
    five_cmd->add_option("--source-revision", five_revision);
    five_cmd->add_option("--dirty-diff-sha256", five_diff_hash);
    five_cmd->add_flag("--no-stress", five_no_stress);
    five_cmd->add_flag("--keep-diagnostic-image", five.keep_diagnostic_image,
        "Keep one diagnostic PNG per run (default: no images written)");
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
    auto* game_analysis = analyze_cmd->add_subcommand("game", "Recompute candidate, prediction and dry-run evidence");
    auto* game_round_analysis = analyze_cmd->add_subcommand("game-round", "Verify summary event segments and analyze a whole offline round");
    auto* game_clips_analysis = analyze_cmd->add_subcommand("game-clips", "Verify saved RGB clips and replay each three-frame clip from cold observer state");
    auto* game_clips_bench = analyze_cmd->add_subcommand("game-clips-bench", "Interleaved bounded A/B for observer scratch reuse on saved RGB");
    auto* game_cold_pipeline = analyze_cmd->add_subcommand("game-cold-pipeline", "Offline QPC latest-frame to FakeTouch pipeline benchmark; no device access");
    auto* game_cold_pipeline_ab = analyze_cmd->add_subcommand("game-cold-pipeline-ab", "Validate frozen A/A tolerance against three interleaved offline pipeline A/B batches");
    auto* game_cold_pipeline_aa = analyze_cmd->add_subcommand("game-cold-pipeline-aa", "Measure three all-full-scan pipeline A/A pairs before freezing tolerance");
    auto* game_line_gap_sweep = analyze_cmd->add_subcommand("game-line-gap-sweep", "Finite offline synthetic truth sweep for horizontal line gap; no device access");
    auto* game_cold_c5_gate = analyze_cmd->add_subcommand("game-cold-c5-gate", "Apply original cold C5 rule across tap and dense paired runs plus load evidence");
    auto* game_corpus_index = analyze_cmd->add_subcommand("game-corpus", "Verify two existing RGB sessions and freeze family-grouped inspection clips; offline only");
    auto* game_corpus_propose = analyze_cmd->add_subcommand("game-corpus-propose", "Write 30-clip source sheets and proposed observer overlays; offline only");
    auto* game_corpus_validate = analyze_cmd->add_subcommand("game-corpus-validate", "Verify proposed markers and refuse gold promotion; offline only");
    auto* game_coverage_validate = analyze_cmd->add_subcommand("game-coverage-validate", "Audit required G1-T2 and risk cases without treating pending as coverage; offline only");
    auto* game_replay_diff = analyze_cmd->add_subcommand("game-replay-diff", "Compare frozen and current candidate counts on verified RGB frames; offline only");
    std::string corpus_old_clips,corpus_old_results,corpus_new_clips,corpus_new_results;
    std::string corpus_index_path,corpus_output_path,corpus_proposals_path;
    std::string replay_diff_index,replay_before_old,replay_after_old,replay_before_new,replay_after_new;
    game_corpus_index->add_option("old-clips",corpus_old_clips)->required();
    game_corpus_index->add_option("old-results",corpus_old_results)->required();
    game_corpus_index->add_option("new-clips",corpus_new_clips)->required();
    game_corpus_index->add_option("new-results",corpus_new_results)->required();
    game_corpus_propose->add_option("corpus-index",corpus_index_path)->required();
    game_corpus_propose->add_option("output-dir",corpus_output_path)->required();
    game_corpus_validate->add_option("corpus-index",corpus_index_path)->required();
    game_corpus_validate->add_option("proposals",corpus_proposals_path)->required();
    game_coverage_validate->add_option("manifest",analysis_path)->required();
    game_replay_diff->add_option("corpus-index",replay_diff_index)->required();
    game_replay_diff->add_option("before-old",replay_before_old)->required();
    game_replay_diff->add_option("after-old",replay_after_old)->required();
    game_replay_diff->add_option("before-new",replay_before_new)->required();
    game_replay_diff->add_option("after-new",replay_after_new)->required();
    std::string clips_overlay_dir;int clips_overlay_round=0,clips_overlay_clip=0;
    int clips_line_gap=4;
    bool clips_no_row_prescreen=false,clips_bench_row_prescreen=false,clips_bench_aa=false;
    int clips_bench_batches=3,clips_bench_replays=6;
    auto* game_image_analysis = analyze_cmd->add_subcommand("game-image", "Offline single PNG geometry diagnostics; no input or timing prediction");
    game_image_analysis->add_option("path",analysis_path)->required();
    game_analysis->add_option("path", analysis_path)->required();
    game_round_analysis->add_option("path", analysis_path)->required();
    game_clips_analysis->add_option("path", analysis_path)->required();
    game_clips_analysis->add_option("--overlay-dir",clips_overlay_dir,"Write source and proposed overlay PNGs for one selected clip");
    game_clips_analysis->add_option("--overlay-round",clips_overlay_round);
    game_clips_analysis->add_option("--overlay-clip",clips_overlay_clip);
    game_clips_analysis->add_flag("--no-row-prescreen",clips_no_row_prescreen,
        "Use original full row scan for offline comparison");
    game_clips_analysis->add_option("--line-gap",clips_line_gap,
        "Bounded offline horizontal line gap parameter sweep; default runtime value is four")
        ->check(CLI::Range(3,5));
    game_clips_bench->add_option("path",analysis_path)->required();
    game_clips_bench->add_option("--batches",clips_bench_batches)->check(CLI::Range(1,5));
    game_clips_bench->add_option("--replays",clips_bench_replays)->check(CLI::Range(1,10));
    game_clips_bench->add_flag("--row-prescreen",clips_bench_row_prescreen,
        "Interleave original full scan and bounded prescreen instead of scratch allocation modes");
    game_clips_bench->add_flag("--aa",clips_bench_aa,
        "Use original full scan in both interleaved modes to quantify measurement noise");
    std::string pipeline_journal;
    int pipeline_frames=1000,pipeline_cadence_ms=20;
    int pipeline_writer_capacity=8192,pipeline_writer_delay_us=0,pipeline_rpc_delay_ms=0;
    std::string pipeline_scene="tap";
    bool pipeline_jitter=false,pipeline_full_scan=false;
    game_cold_pipeline->add_option("--journal",pipeline_journal)->required();
    game_cold_pipeline->add_option("--frames",pipeline_frames)->check(CLI::Range(12,10000));
    game_cold_pipeline->add_option("--cadence-ms",pipeline_cadence_ms)->check(CLI::Range(8,50));
    game_cold_pipeline->add_flag("--jitter",pipeline_jitter);
    game_cold_pipeline->add_flag("--full-scan",pipeline_full_scan);
    game_cold_pipeline->add_option("--scene",pipeline_scene)->check(CLI::IsMember({"tap","dense"}));
    game_cold_pipeline->add_option("--writer-capacity",pipeline_writer_capacity)->check(CLI::Range(2,8192));
    game_cold_pipeline->add_option("--writer-delay-us",pipeline_writer_delay_us)->check(CLI::Range(0,50000));
    game_cold_pipeline->add_option("--rpc-delay-ms",pipeline_rpc_delay_ms)->check(CLI::Range(0,50));
    game_cold_pipeline_ab->add_option("directory",analysis_path)->required();
    std::string pipeline_meter;
    game_cold_pipeline_aa->add_option("directory",analysis_path)->required();
    game_cold_pipeline_aa->add_option("--meter",pipeline_meter)->required();
    game_cold_c5_gate->add_option("directory",analysis_path)->required();
    capture_analysis->add_option("path", analysis_path)->required();
    auto* pause_analysis = analyze_cmd->add_subcommand("pause", "Receiver pause relative lag");
    pause_analysis->add_option("path", analysis_path)->required();
    auto* campaign_analysis = analyze_cmd->add_subcommand("campaign", "Recompute every campaign raw file and verify hashes");
    campaign_analysis->add_option("path", analysis_path)->required();
    std::vector<std::string> tracking_methods{"legacy","byte_association","oc_observation"};int tracking_updates=10000;bool tracking_reupdate=false;
    auto* tracking_analysis=analyze_cmd->add_subcommand("tracking","Offline paired candidate bank comparison; FakeTouchBackend only");
    tracking_analysis->add_option("path",analysis_path)->required();
    tracking_analysis->add_option("--methods",tracking_methods)->delimiter(',');
    tracking_analysis->add_option("--updates",tracking_updates)->check(CLI::Range(1,100000));
    tracking_analysis->add_flag("--oc-reupdate",tracking_reupdate);
    std::string dataset_path,dataset_output;
    auto* dataset_cmd=app.add_subcommand("dataset","Local pixel dataset preparation; no model training or upload");
    auto* dataset_validate=dataset_cmd->add_subcommand("validate","Check hashes, native ROI, masks, objects and split leakage");
    dataset_validate->add_option("root",dataset_path)->required();
    auto* dataset_export=dataset_cmd->add_subcommand("export","Write a validated local index");
    dataset_export->add_option("root",dataset_path)->required();dataset_export->add_option("--output",dataset_output)->required();
    auto* dataset_pilot=dataset_cmd->add_subcommand("pilot-v75","Create two proposed single-frame annotations from verified v75 PNGs");
    dataset_pilot->add_option("source",dataset_path)->required();dataset_pilot->add_option("--output",dataset_output)->required();
    auto* dataset_rasterize=dataset_cmd->add_subcommand("rasterize","Create semantic/instance masks and overlay from sample annotation.json");
    dataset_rasterize->add_option("sample",dataset_path)->required();
    auto* tracking_challenge=dataset_cmd->add_subcommand("tracking-challenge","Write short deterministic geometric candidate cases; not real pixels");
    tracking_challenge->add_option("--output",dataset_output)->required();
    int copy_updates=10000;auto* dataset_copy_bench=dataset_cmd->add_subcommand("copy-bench","Offline bounded sampler memory copy A/B; synthetic pixels only");
    dataset_copy_bench->add_option("--updates",copy_updates)->check(CLI::Range(1,100000));
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
        else if (*manual_cmd) run_manual_session(manual_config,manual_capability,manual_no_preview,
            static_cast<Nanoseconds>(std::llround(round_watchdog_s*1e9)),manual_pixel_clips,manual_full_recording,manual_one_round);
        else if (*run_cmd) {
            if(manual_play&&run_mode!="assist")
                throw std::invalid_argument("--manual-play requires assist mode");
            if(wait_play_option->count()&&!manual_play)
                throw std::invalid_argument("--wait-play-s requires --manual-play");
            if((keep_diagnostic_anomalies||keep_vision_dataset||!tracking_shadow.empty())&&run_mode!="assist")
                throw std::invalid_argument("diagnostics, dataset and tracking shadow require assist mode");
            if (run_mode == "observe") run_observe(config_path, run_duration_s, no_preview, "", run_stale_ms);
            else if (run_mode == "auto-start" && !run_capability.empty())
                run_auto_start(config_path, run_capability, run_duration_s, no_preview);
            else if(run_mode=="assist"&&!run_capability.empty())
                run_assist(config_path,run_capability,run_duration_s,no_preview,keep_diagnostic_anomalies,keep_vision_dataset,tracking_shadow,manual_play,wait_play_s);
            else throw std::invalid_argument("choose observe, auto-start, or assist; input modes require --capability");
        } else if (*dataset_cmd) {
            if(*dataset_validate) {const auto result=validate_vision_dataset(dataset_path);std::cout<<result.dump(2)<<'\n';if(!result.at("valid").get<bool>())return 1;}
            else if(*dataset_export)std::cout<<export_vision_dataset(dataset_path,dataset_output).dump(2)<<'\n';
            else if(*dataset_pilot)std::cout<<create_v75_dataset_pilot(dataset_path,dataset_output).dump(2)<<'\n';
            else if(*dataset_rasterize){const auto file=std::filesystem::path(dataset_path)/"annotation.json";if(std::filesystem::file_size(file)>16*1024*1024)throw std::invalid_argument("annotation capacity");std::ifstream in(file);rasterize_annotation(dataset_path,json::parse(in));std::cout<<json{{"rasterized",true},{"input_created",false}}.dump()<<'\n';}
            else if(*tracking_challenge){write_tracking_challenge(dataset_output);std::cout<<json{{"output",dataset_output},{"input_created",false},{"synthetic",true}}.dump()<<'\n';}
            else if(*dataset_copy_bench)std::cout<<benchmark_sampling_copy(copy_updates).dump(2)<<'\n';
            else throw std::invalid_argument("choose a dataset subcommand");
        } else if (*preflight_cmd) {
            std::cout << game_preflight(preflight_config, preflight_capability).dump(2) << '\n';
        } else if (*config_cmd) {
            if (*migrate_cmd) migrate_config(migrate_source, migrate_target);
            else throw std::invalid_argument("choose a config subcommand");
        } else if (*analyze_cmd) {
            if (*game_image_analysis) {
                FakeClock clock; GameObserver observer(clock);
                auto result=decision_json(observer.process(load_diagnostic_png(analysis_path)));
                result["offline_only"]=true; result["input_created"]=false;
                result["diagnostic_png_sha256"]=sha256_file(analysis_path);
                std::cout<<result.dump(2)<<'\n';
            } else if (*tracking_analysis){if(tracking_reupdate&&std::find(tracking_methods.begin(),tracking_methods.end(),"oc_observation")==tracking_methods.end())throw std::invalid_argument("--oc-reupdate requires oc_observation");
                auto result=analyze_tracking_bank(analysis_path,tracking_methods,tracking_updates,tracking_reupdate);wchar_t module[32768]{};if(GetModuleFileNameW(nullptr,module,32768))result["binary_sha256"]=sha256_file(module);std::cout<<result.dump(2)<<'\n';}
            else if (*game_analysis) std::cout << analyze_game_jsonl(analysis_path).dump(2) << '\n';
            else if (*game_round_analysis) std::cout << analyze_game_round(analysis_path).dump(2) << '\n';
            else if (*game_clips_analysis) std::cout << replay_game_pixel_clips(analysis_path,clips_overlay_dir,
                clips_overlay_round,clips_overlay_clip,false,!clips_no_row_prescreen,
                clips_line_gap).dump(2) << '\n';
            else if (*game_clips_bench) std::cout << benchmark_game_pixel_clips(analysis_path,
                clips_bench_batches,clips_bench_replays,clips_bench_row_prescreen,clips_bench_aa).dump(2) << '\n';
            else if (*game_cold_pipeline) std::cout << benchmark_game_cold_pipeline(pipeline_journal,
                pipeline_frames,pipeline_cadence_ms,pipeline_jitter,!pipeline_full_scan,
                pipeline_scene,pipeline_writer_capacity,pipeline_writer_delay_us,
                pipeline_rpc_delay_ms).dump(2) << '\n';
            else if (*game_cold_pipeline_ab) std::cout << analyze_game_cold_pipeline_ab(analysis_path).dump(2) << '\n';
            else if (*game_cold_pipeline_aa) std::cout << analyze_game_cold_pipeline_aa(analysis_path,pipeline_meter).dump(2) << '\n';
            else if (*game_line_gap_sweep) std::cout << analyze_game_line_gap_sweep().dump(2) << '\n';
            else if (*game_cold_c5_gate) std::cout << analyze_game_cold_c5_gate(analysis_path).dump(2) << '\n';
            else if (*game_corpus_index) std::cout << index_game_pixel_corpus(corpus_old_clips,
                corpus_old_results,corpus_new_clips,corpus_new_results).dump(2) << '\n';
            else if (*game_corpus_propose) std::cout << write_game_corpus_proposals(corpus_index_path,
                corpus_output_path).dump(2) << '\n';
            else if (*game_corpus_validate) std::cout << validate_game_corpus_proposals(corpus_index_path,
                corpus_proposals_path).dump(2) << '\n';
            else if (*game_coverage_validate) std::cout << validate_game_cold_coverage_manifest(analysis_path).dump(2) << '\n';
            else if (*game_replay_diff) std::cout << compare_game_pixel_replays(replay_diff_index,
                replay_before_old,replay_after_old,replay_before_new,replay_after_new).dump(2) << '\n';
            else if (*capture_analysis) std::cout << analyze_capture_jsonl(analysis_path).dump(2) << '\n';
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
