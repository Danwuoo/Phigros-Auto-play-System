#include "pas/bench.hpp"

#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include "pas/core.hpp"
#include "pas/emulator.hpp"
#include "pas/journal.hpp"
#include "pas/native_capture.hpp"
#include "pas/bench_workloads.hpp"
#include "pas/preview.hpp"
#include "pas/scrcpy_capture.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
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
#include <tlhelp32.h>
#include <wincodec.h>
#include <wrl/client.h>

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
    int max_color_error = 0;
};

struct FixtureQuality {
    int line_expected_y = -1;
    int line_peak_y = -1;
    int line_width_px = 0;
    int line_contrast = 0;
    int square_left_x = -1;
    int square_width_px = 0;
    std::optional<int> square_expected_x;
    int low_contrast_delta = 0;
};

FixtureQuality fixture_quality(const Frame& frame) {
    FixtureQuality result;
    const auto pixel = [&](int x, int y, int channel) {
        return static_cast<int>(frame.rgb[static_cast<std::size_t>(y) * frame.stride + x * 3 + channel]);
    };
    const int expected_y = frame.height / 2 + 50;
    result.line_expected_y = expected_y;
    if (expected_y + 6 >= frame.height || expected_y < 6) return result;
    const std::array<int, 3> sample_x = {frame.width / 4, frame.width / 2, frame.width * 3 / 4};
    const auto luminance = [&](int y) {
        int sum = 0;
        for (const int x : sample_x)
            for (int channel = 0; channel < 3; ++channel) sum += pixel(x, y, channel);
        return sum / 9;
    };
    int peak = -1;
    for (int y = expected_y - 5; y <= expected_y + 5; ++y) {
        const int value = luminance(y);
        if (value > peak) { peak = value; result.line_peak_y = y; }
        if (value >= 128) ++result.line_width_px;
    }
    result.line_contrast = peak - (luminance(expected_y - 6) + luminance(expected_y + 6)) / 2;
    const int square_y = frame.height / 2;
    int right = -1;
    for (int x = 30; x < frame.width; ++x) {
        if (pixel(x, square_y, 0) > 160 && pixel(x, square_y, 1) > 160 &&
            pixel(x, square_y, 2) < 100) {
            if (result.square_left_x < 0) result.square_left_x = x;
            right = x;
        }
    }
    if (right >= result.square_left_x && result.square_left_x >= 0)
        result.square_width_px = right - result.square_left_x + 1;
    const auto red = [&](int x) { return pixel(x, 61, 0) > 180 &&
        pixel(x, 61, 1) < 90 && pixel(x, 61, 2) < 90; };
    if (frame.width > 440 && red(236) && red(431)) {
        int truth = 0;
        bool valid = true;
        for (int bit = 0; bit < 11; ++bit) {
            const int x = 256 + bit * 14;
            const int value = (pixel(x, 61, 0) + pixel(x, 61, 1) + pixel(x, 61, 2)) / 3;
            if (value > 180) truth |= 1 << bit;
            else if (value >= 90) valid = false;
        }
        if (valid && truth >= 40 && truth + 40 <= frame.width) result.square_expected_x = truth;
    }
    if (frame.width > 400 && frame.height > 200)
        result.low_contrast_delta = pixel(300, frame.height - 130, 0) -
            pixel(180, frame.height - 130, 0);
    return result;
}

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
        return FixtureCounter{static_cast<int>(*identity), "native_v2_four_region", 0};
    if (const auto legacy = legacy_fixture_counter(frame)) {
        int max_error = 0;
        for (int corner = 0; corner < 4; ++corner) {
            const int x = corner % 2 ? frame.width - 90 : 65;
            const int y = corner / 2 ? frame.height - 90 : 55;
            for (int chip = 0; chip < 2; ++chip) {
                const auto observed = value(x + (chip ? 37 : 12), y + 10);
                const auto expected = chip ? (static_cast<std::uint32_t>(*legacy) ^ 0xA5C37E) :
                    static_cast<std::uint32_t>(*legacy);
                for (int channel = 0; channel < 3; ++channel) {
                    const int shift = 16 - 8 * channel;
                    max_error = std::max(max_error,
                        std::abs(static_cast<int>((observed >> shift) & 255) -
                                 static_cast<int>((expected >> shift) & 255)));
                }
            }
        }
        if (max_error <= 32)
            return FixtureCounter{*legacy, "native_v2_lossy_validated", max_error};
        return FixtureCounter{*legacy, "legacy_counter", max_error};
    }
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
    json related_processes = json::array();
    json gpu_engines;
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
    sample.process_cpu_ns = (filetime_100ns(kernel) + filetime_100ns(user)) * 100;
    sample.working_set_bytes = memory.WorkingSetSize;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) do {
            const std::wstring name = entry.szExeFile;
            const char* ascii_name = !_wcsicmp(name.c_str(), L"qemu-system-x86_64.exe")
                ? "qemu-system-x86_64.exe" : !_wcsicmp(name.c_str(), L"emulator.exe")
                ? "emulator.exe" : !_wcsicmp(name.c_str(), L"studio64.exe")
                ? "studio64.exe" : nullptr;
            if (!ascii_name) continue;
            json process = {{"pid", entry.th32ProcessID},
                {"name", ascii_name},
                {"process_cpu_ns", nullptr}, {"working_set_bytes", nullptr}};
            HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                        FALSE, entry.th32ProcessID);
            if (handle) {
                FILETIME created{}, exited{}, k{}, u{};
                if (GetProcessTimes(handle, &created, &exited, &k, &u))
                    process["process_cpu_ns"] = (filetime_100ns(k) + filetime_100ns(u)) * 100;
                PROCESS_MEMORY_COUNTERS_EX process_memory{};
                if (GetProcessMemoryInfo(handle,
                    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process_memory),
                    static_cast<DWORD>(sizeof(process_memory))))
                    process["working_set_bytes"] = process_memory.WorkingSetSize;
                CloseHandle(handle);
            }
            sample.related_processes.push_back(std::move(process));
        } while (Process32NextW(snapshot, &entry));
        CloseHandle(snapshot);
    }
    sample.gpu_engines = sample_gpu_engines();
    sample.after_ns = clock.now_ns();
    return sample;
}

void save_diagnostic_png(const std::filesystem::path& path, const Frame& frame) {
    if (frame.width <= 0 || frame.height <= 0 || frame.stride != frame.width * 3 ||
        frame.rgb.size() != static_cast<std::size_t>(frame.stride) * frame.height)
        throw std::invalid_argument("diagnostic image is not tightly packed RGB24");
    const auto co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(co) && co != RPC_E_CHANGED_MODE)
        throw std::runtime_error("COM initialization for PNG failed");
    struct CoScope { HRESULT result; ~CoScope() { if (SUCCEEDED(result)) CoUninitialize(); } } scope{co};
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))))
        throw std::runtime_error("PNG imaging factory failed");
    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)))
        throw std::runtime_error("PNG output stream failed");
    ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
        FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)))
        throw std::runtime_error("PNG encoder initialization failed");
    ComPtr<IWICBitmapFrameEncode> bitmap;
    ComPtr<IPropertyBag2> properties;
    if (FAILED(encoder->CreateNewFrame(&bitmap, &properties)) ||
        FAILED(bitmap->Initialize(properties.Get())) ||
        FAILED(bitmap->SetSize(frame.width, frame.height)))
        throw std::runtime_error("PNG frame initialization failed");
    std::vector<std::uint8_t> bgr = frame.rgb;
    for (std::size_t pixel = 0; pixel < bgr.size(); pixel += 3)
        std::swap(bgr[pixel], bgr[pixel + 2]);
    GUID pixel_format = GUID_WICPixelFormat24bppBGR;
    if (FAILED(bitmap->SetPixelFormat(&pixel_format)) ||
        !InlineIsEqualGUID(pixel_format, GUID_WICPixelFormat24bppBGR) ||
        FAILED(bitmap->WritePixels(frame.height, frame.stride,
                                   static_cast<UINT>(bgr.size()), bgr.data())) ||
        FAILED(bitmap->Commit()) || FAILED(encoder->Commit()))
        throw std::runtime_error("PNG pixel encoding failed");
}
}

void run_capture_bench(const CaptureBenchOptions& options) {
    if (!std::isfinite(options.source_static_s) || options.source_static_s < 0 ||
        options.source_static_s > 10 || (options.source_static_s > 0 &&
        (!options.fixture || options.duration_s < options.source_static_s + 5)))
        throw std::invalid_argument("static source test needs native Fixture and enough recovery time");
    if (options.serial.empty() || options.width <= 0 || options.height <= 0 ||
        options.width > 4096 || options.height > 4096 ||
        options.source_rotation < 0 || options.source_rotation > 3 ||
        !std::isfinite(options.duration_s) || !std::isfinite(options.warmup_s) ||
        !std::isfinite(options.ready_timeout_s) || !std::isfinite(options.consumer_delay_ms) ||
        !std::isfinite(options.receiver_pause_ms) ||
        options.duration_s <= 0 || options.duration_s > 3600 || options.warmup_s < 0 ||
        options.ready_timeout_s <= 0 || options.consumer_delay_ms < 0 ||
        options.receiver_pause_ms < 0 || options.receiver_pause_ms > 5000 ||
        options.memory_load_mib < 0 || options.memory_load_mib > 512 ||
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
        (options.grpc_copy_mode != "legacy-rows" && options.grpc_copy_mode != "fast-memcpy") ||
        (options.run_class != "development_smoke" && options.run_class != "formal_campaign") ||
        (options.fixture && options.fixture_apk.empty()) ||
        (options.fixture_schema != "native-v2" && options.fixture_schema != "legacy") ||
        options.monitor_index < 0 || options.crop_x < 0 || options.crop_y < 0 ||
        options.crop_x > 16384 || options.crop_y > 16384 ||
        options.scrcpy_max_fps < 1 || options.scrcpy_max_fps > 120 ||
        options.scrcpy_video_bit_rate < 100'000 || options.scrcpy_video_bit_rate > 100'000'000 ||
        options.scrcpy_video_encoder.empty())
        throw std::invalid_argument("invalid capture benchmark options");
    if (options.backend == "adb-png" && (options.image_format != "rgb888" ||
        options.row_order != "top-down" || !options.grpc_endpoint.empty()))
        throw std::invalid_argument("ADB PNG does not use gRPC image options");
    if (options.transport == "mmap" && (!options.diagnostic_mmap || options.backend != "emulator-grpc"))
        throw std::invalid_argument("MMAP is diagnostic-only and requires explicit --diagnostic-mmap");
    if (options.grpc_rotate_ccw && options.backend != "emulator-grpc")
        throw std::invalid_argument("gRPC rotation is valid only for emulator-grpc");
    if (options.transport != "payload" && options.transport != "mmap")
        throw std::invalid_argument("unsupported capture transport");
    const bool native = options.backend == "wgc" || options.backend == "dxgi";
    if (native && (options.window_hwnd.empty() || options.transport != "payload" ||
                   !options.grpc_endpoint.empty() || !options.grpc_token_file.empty() ||
                   options.diagnostic_mmap || options.image_format != "rgb888" ||
                   options.row_order != "top-down"))
        throw std::invalid_argument("native capture requires window handle and RGB24 output profile");
    if (options.backend == "scrcpy" && (options.scrcpy_server.empty() ||
        options.transport != "payload" || !options.grpc_endpoint.empty() ||
        options.diagnostic_mmap || !options.window_hwnd.empty() ||
        options.crop_x || options.crop_y))
        throw std::invalid_argument("scrcpy requires pinned server file and uncropped output profile");
    if (std::filesystem::exists(options.output_dir))
        throw std::runtime_error("output directory already exists; choose a new run ID");
    std::filesystem::create_directories(options.output_dir);
    HostClock clock;
    const auto adb_for_preflight = find_adb();
    const auto preflight = device_preflight(adb_for_preflight, options.serial);
    const auto fixture_hash = options.fixture_apk.empty() ? std::string{} : sha256_file(options.fixture_apk);
    wchar_t executable[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, executable, MAX_PATH))
        throw std::runtime_error("capture executable path unavailable");
    const auto binary_hash = sha256_file(executable);
    const auto installed_fixture_hash = options.fixture
        ? installed_apk_sha256(adb_for_preflight, options.serial,
            options.fixture_schema == "legacy" ? "org.pas.capturefixture" : "org.pas.capturefixture.cpp")
        : std::string{};
    if (options.fixture && fixture_hash != installed_fixture_hash)
        throw std::runtime_error("installed native capture Fixture APK differs from supplied artifact");
    Journal journal(options.output_dir / "capture.jsonl");
    LatestFrame latest(options.width, options.height, 3, &clock);
    std::unique_ptr<GrpcCapture> capture;
    std::unique_ptr<NativeCapture> native_capture;
    std::unique_ptr<ScrcpyCapture> scrcpy_capture;
    std::string endpoint_target;
    if (options.backend == "emulator-grpc") {
        CaptureOptions capture_options;
        capture_options.width = options.grpc_rotate_ccw ? options.height : options.width;
        capture_options.height = options.grpc_rotate_ccw ? options.width : options.height;
        capture_options.source_rotation = options.source_rotation;
        capture_options.rgba = options.image_format == "rgba8888";
        capture_options.bottom_up = options.row_order == "bottom-up";
        capture_options.optimized_rgb_copy = options.grpc_copy_mode == "fast-memcpy";
        capture_options.rotate_ccw = options.grpc_rotate_ccw;
        capture_options.max_rgb_bytes = options.max_rgb_bytes;
        capture_options.diagnostic_mmap = options.transport == "mmap";
        if (options.max_relative_lag_ms)
            capture_options.max_relative_lag_ns = static_cast<Nanoseconds>(std::llround(*options.max_relative_lag_ms * 1e6));
        auto endpoint = benchmark_endpoint(options);
        endpoint_target = endpoint.target;
        capture = std::make_unique<GrpcCapture>(clock, std::move(endpoint), capture_options);
    } else if (native) {
        NativeCaptureOptions native_options;
        native_options.backend = options.backend;
        native_options.window_hwnd = options.window_hwnd;
        native_options.monitor_index = options.monitor_index;
        native_options.crop_x = options.crop_x;
        native_options.crop_y = options.crop_y;
        native_options.width = options.width;
        native_options.height = options.height;
        if (options.max_relative_lag_ms)
            native_options.max_relative_lag_ns = static_cast<Nanoseconds>(std::llround(*options.max_relative_lag_ms * 1e6));
        native_capture = std::make_unique<NativeCapture>(clock, std::move(native_options));
    } else if (options.backend == "scrcpy") {
        scrcpy_capture = std::make_unique<ScrcpyCapture>(clock,
            ScrcpyCaptureOptions{options.serial, options.scrcpy_server, options.width,
                                 options.height, options.scrcpy_max_fps, options.scrcpy_video_bit_rate,
                                 options.scrcpy_video_encoder});
    } else if (options.backend != "adb-png") throw std::invalid_argument("unsupported capture backend");
    const auto adb = options.backend == "adb-png" ? adb_for_preflight : std::filesystem::path{};
    const auto manifest = json{{"schema_version", 3}, {"serial", options.serial},
        {"binary_sha256", binary_hash},
        {"capture_backend", options.backend}, {"transport", native ? "native_gpu" :
            options.backend == "scrcpy" ? "h264" : options.transport},
        {"consistency", options.transport == "mmap" ? "unverified" : "payload"},
        {"execution", "single_process_thread"}, {"width", options.width}, {"height", options.height},
        {"source_rotation", options.source_rotation}, {"warmup_s", options.warmup_s},
        {"grpc_source_width", options.backend == "emulator-grpc" ?
            json(options.grpc_rotate_ccw ? options.height : options.width) : json(nullptr)},
        {"grpc_source_height", options.backend == "emulator-grpc" ?
            json(options.grpc_rotate_ccw ? options.width : options.height) : json(nullptr)},
        {"normalization_rotation_degrees", options.grpc_rotate_ccw ? -90 : 0},
        {"image_format", options.image_format}, {"row_order", options.row_order},
        {"grpc_copy_mode", options.backend == "emulator-grpc" ? json(options.grpc_copy_mode) : json(nullptr)},
        {"run_class", options.run_class},
        {"max_relative_lag_ms", options.backend == "wgc" ?
            json(options.max_relative_lag_ms.value_or(250)) : options.max_relative_lag_ms ?
            json(*options.max_relative_lag_ms) : json(nullptr)},
        {"relative_lag_semantics", "elapsed host minus elapsed source; absolute age unknown"},
        {"wgc_pool_drain_limit", options.backend == "wgc" ? json(2) : json(nullptr)},
        {"max_rgb_bytes", options.max_rgb_bytes}, {"grpc_endpoint", endpoint_target},
        {"window_hwnd", native ? json(options.window_hwnd) : json(nullptr)},
        {"monitor_index", native ? json(options.monitor_index) : json(nullptr)},
        {"crop_x", options.crop_x}, {"crop_y", options.crop_y},
        {"scrcpy_server_sha256", scrcpy_capture ? json(sha256_file(options.scrcpy_server)) : json(nullptr)},
        {"scrcpy_server_tag", scrcpy_capture ? json("v4.1") : json(nullptr)},
        {"scrcpy_server_source_commit", scrcpy_capture ?
            json("49c9501fb26f456bbf4a341dd68879f670c67452") : json(nullptr)},
        {"ffmpeg_runtime_version", scrcpy_capture ? json(ffmpeg_runtime_version()) : json(nullptr)},
        {"ffmpeg_avcodec_version", scrcpy_capture ? json(ffmpeg_avcodec_version()) : json(nullptr)},
        {"ffmpeg_swscale_version", scrcpy_capture ? json(ffmpeg_swscale_version()) : json(nullptr)},
        {"scrcpy_max_fps", scrcpy_capture ? json(options.scrcpy_max_fps) : json(nullptr)},
        {"scrcpy_video_bit_rate", scrcpy_capture ? json(options.scrcpy_video_bit_rate) : json(nullptr)},
        {"scrcpy_video_encoder", scrcpy_capture ? json(options.scrcpy_video_encoder) : json(nullptr)},
        {"capture_complete_semantics", options.backend == "wgc" ? "WGC frame acquired on callback" :
            options.backend == "dxgi" ? "DXGI AcquireNextFrame returned" :
            options.backend == "scrcpy" ? "complete encoded packet received" :
            options.transport == "mmap" ? "MMAP notification snapshot copied" : "complete payload received"},
        {"duration_s", options.duration_s}, {"ready_timeout_s", options.ready_timeout_s},
        {"consumer_delay_ms", options.consumer_delay_ms},
        {"consumer_recover_after_s", options.consumer_recover_after_s
            ? json(*options.consumer_recover_after_s) : json(nullptr)},
        {"receiver_pause_ms", options.receiver_pause_ms}, {"fixture_counter_basis", "consumed_frames"},
        {"receiver_pause_position", options.backend == "wgc" ?
            "inside FrameArrived after RGB publication, before callback returns" :
            options.backend == "dxgi" ? "after RGB publication, before ReleaseFrame" :
            options.backend == "scrcpy" ? "after decoded RGB publication, before next packet read" :
            options.backend == "emulator-grpc" ? "after RGB publication, before next gRPC Read" :
            "after ADB RGB publication, before next screencap"},
        {"host_load", options.load},
        {"memory_load_mib", options.memory_load_mib},
        {"preview", options.preview}, {"preview_placement", options.preview ?
            json{{"x",1460},{"y",125},{"client_width",400},{"client_height",225},{"activate",false}} : json(nullptr)},
        {"source_static_s", options.source_static_s}, {"gpu_load", options.gpu_load},
        {"qpc_frequency", clock.frequency()},
        {"source_absolute_age", nullptr}, {"produced_ns", nullptr},
        {"device_preflight", preflight},
        {"host_capture_environment", json::parse(capture_host_environment_json(options.window_hwnd))},
        {"fixture_schema_requested", options.fixture ? json(options.fixture_schema) : json(nullptr)},
        {"fixture_position_truth_required", options.fixture_position_truth},
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
    std::atomic<Nanoseconds> receiver_resumed = 0;
    std::mutex fault_mutex;
    std::exception_ptr worker_fault;
    std::jthread load_worker;
    std::jthread gpu_worker, source_worker, reference_worker;
    std::unique_ptr<PreviewWindow> preview;
    if (options.preview) preview = std::make_unique<PreviewWindow>(options.width, options.height, true);
    std::vector<std::uint8_t> memory_load;
    std::jthread worker([&](std::stop_token stop) {
        try {
            const auto publish = [&](Frame&& frame) {
                frame.epoch = 1;
                const bool accepted = latest.publish(frame.rgb.data(), frame.rgb.size(), frame);
                const auto publication = accepted ? latest.peek()->published_ns : 0;
                if (!journal.push({{"event", "capture"}, {"frame_sequence", frame.sequence},
                    {"capture_complete_ns", frame.capture_complete_ns},
                    {"pixels_ready_ns", frame.pixels_ready_ns},
                    {"capture_backend", frame.capture_backend},
                    {"published_to_latest", accepted},
                    {"source_pixel_format", frame.source_pixel_format},
                    {"source_stride", frame.source_stride},
                    {"source_valid", frame.source_valid},
                    {"crop", {frame.crop_x, frame.crop_y, frame.crop_width, frame.crop_height}},
                    {"receive_start_ns", frame.receive_start_ns ? json(*frame.receive_start_ns) : json(nullptr)},
                    {"parse_start_ns", frame.parse_start_ns ? json(*frame.parse_start_ns) : json(nullptr)},
                    {"parse_end_ns", frame.parse_end_ns ? json(*frame.parse_end_ns) : json(nullptr)},
                    {"notification_received_ns", frame.notification_received_ns ? json(*frame.notification_received_ns) : json(nullptr)},
                    {"copy_start_ns", frame.copy_start_ns ? json(*frame.copy_start_ns) : json(nullptr)},
                    {"copy_end_ns", frame.copy_end_ns ? json(*frame.copy_end_ns) : json(nullptr)},
                    {"readback_start_ns", frame.readback_start_ns ? json(*frame.readback_start_ns) : json(nullptr)},
                    {"readback_end_ns", frame.readback_end_ns ? json(*frame.readback_end_ns) : json(nullptr)},
                    {"decode_start_ns", frame.decode_start_ns ? json(*frame.decode_start_ns) : json(nullptr)},
                    {"decode_end_ns", frame.decode_end_ns ? json(*frame.decode_end_ns) : json(nullptr)},
                    {"source_qpc_ticks", frame.source_qpc_ticks ? json(*frame.source_qpc_ticks) : json(nullptr)},
                    {"source_system_relative_100ns", frame.source_system_relative_100ns ?
                        json(*frame.source_system_relative_100ns) : json(nullptr)},
                    {"codec_pts", frame.codec_pts ? json(*frame.codec_pts) : json(nullptr)},
                    {"source_sequence", frame.source_sequence ? json(*frame.source_sequence) : json(nullptr)},
                    {"source_timestamp_us", frame.source_timestamp_us ? json(*frame.source_timestamp_us) : json(nullptr)},
                    {"source_clock_domain", frame.source_qpc_ticks ? "windows_qpc_ticks" :
                        frame.source_system_relative_100ns ? "windows_system_relative_100ns" :
                        frame.codec_pts ? "scrcpy_pts_us_uncalibrated" :
                        frame.source_timestamp_us ? "emulator_unix_us_uncalibrated" : "unknown"},
                    {"source_rotation", frame.source_rotation},
                    {"normalization_rotation_degrees", frame.normalization_rotation_degrees},
                    {"width", frame.width}, {"height", frame.height},
                    {"pixel_format", "RGB24"}, {"stream_generation", frame.generation},
                    {"geometry_version", frame.geometry_version}, {"epoch", frame.epoch}}))
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
                    receiver_resumed = clock.now_ns();
                    if (!journal.push({{"event", "receiver_pause"}, {"start_ns", pause_start},
                        {"ended_ns", clock.now_ns()}, {"duration_ms", options.receiver_pause_ms},
                        {"frame_sequence", frame.sequence}}))
                        throw std::runtime_error("critical journal overrun");
                }
            };
            if (capture) capture->stream(stop, publish);
            else if (native_capture) native_capture->stream(stop, publish,
                [&](std::string_view kind, Nanoseconds when) {
                    if (!journal.push({{"event", kind}, {"monotonic_ns", when}}))
                        throw std::runtime_error("critical journal overrun");
                });
            else if (scrcpy_capture) scrcpy_capture->stream(stop, publish,
                [&](std::string_view kind, Nanoseconds when) {
                    if (!journal.push({{"event", kind}, {"monotonic_ns", when}}))
                        throw std::runtime_error("critical journal overrun");
                });
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
        for (auto* auxiliary : {&gpu_worker, &source_worker, &reference_worker})
            if (auxiliary->joinable()) { auxiliary->request_stop(); auxiliary->join(); }
        if (load_worker.joinable()) { load_worker.request_stop(); load_worker.join(); }
        worker.request_stop();
        if (capture) capture->cancel();
        if (native_capture) native_capture->cancel();
        if (scrcpy_capture) scrcpy_capture->cancel();
        worker.join();
        journal.close();
    };
    try {
        std::uint64_t last_sequence = 0;
        const auto ready_deadline = clock.now_ns() + static_cast<Nanoseconds>(std::llround(options.ready_timeout_s * 1e9));
        auto first = latest.read_after(0, static_cast<Nanoseconds>(std::llround(options.ready_timeout_s * 1e9)));
        if (!first || clock.now_ns() >= ready_deadline) {
            std::exception_ptr error;
            { std::lock_guard lock(fault_mutex); error = worker_fault; }
            throw std::runtime_error("capture source did not deliver a valid frame before ready timeout" +
                (error ? ": " + tokenless_failure(error) : std::string{}));
        }
        save_diagnostic_png(options.output_dir / "diagnostic.png", *first);
        last_sequence = first->sequence;
        if (options.memory_load_mib)
            memory_load.assign(static_cast<std::size_t>(options.memory_load_mib) * 1024 * 1024, 0xA5);
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
        const auto auxiliary_failed = [&] {
            { std::lock_guard lock(fault_mutex); worker_fault = std::current_exception(); }
            worker_done = true;
        };
        const auto reference = [&](const char* context) {
            const auto before = clock.now_ns();
            const auto frame = capture_adb_png(clock, adb_for_preflight, options.serial, 1);
            const auto counter = fixture_counter(frame);
            if (!counter) throw std::runtime_error("independent ADB Fixture reference undecodable");
            if (!journal.push({{"event", "fixture_reference"}, {"context", context},
                {"before_ns", before}, {"after_ns", clock.now_ns()}, {"counter",counter->value},
                {"basis","ADB screencap bracket; Android render age uncalibrated"}}))
                throw std::runtime_error("Fixture reference journal overrun");
        };
        if ((options.receiver_pause_ms > 0 || options.consumer_recover_after_s) && options.fixture)
            reference_worker = std::jthread([&, formal_start, reference, auxiliary_failed](std::stop_token stop) {
                try {
                    if (options.receiver_pause_ms > 0) {
                        while (!stop.stop_requested() && !receiver_resumed.load())
                            std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        if (!stop.stop_requested()) reference("receiver_resume");
                    } else {
                        const auto until = formal_start + static_cast<Nanoseconds>(*options.consumer_recover_after_s * 1e9);
                        while (!stop.stop_requested() && clock.now_ns() < until)
                            std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        if (!stop.stop_requested()) reference("consumer_resume");
                    }
                } catch (...) { auxiliary_failed(); }
            });
        if (options.gpu_load) gpu_worker = std::jthread([&, auxiliary_failed](std::stop_token stop) {
            try { run_bench_gpu_load(stop, clock, [&](json item) {
                if (!journal.push(std::move(item))) throw std::runtime_error("GPU load journal overrun");
            }); } catch (...) { auxiliary_failed(); }
        });
        if (options.source_static_s > 0) source_worker = std::jthread([&, formal_start, reference, auxiliary_failed](std::stop_token stop) {
            bool frozen = false;
            try {
                const auto wait_until = [&](Nanoseconds until) {
                    while (!stop.stop_requested() && clock.now_ns() < until)
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    return !stop.stop_requested();
                };
                const auto set_freeze = [&](bool value) {
                    const auto before = clock.now_ns();
                    adb_call(adb_for_preflight, {"-s", options.serial, "shell", "setprop",
                        "debug.pas.fixture_freeze", value ? "1" : "0"});
                    if (!journal.push({{"event", "source_freeze_command"}, {"freeze", value},
                        {"before_ns", before}, {"after_ns", clock.now_ns()}, {"poll_limit_ms",100}}))
                        throw std::runtime_error("source control journal overrun");
                };
                if (wait_until(formal_start + 2'000'000'000)) {
                    frozen = true; set_freeze(true);
                    if (wait_until(formal_start + 2'500'000'000)) reference("static_first");
                    if (wait_until(formal_start + 3'500'000'000)) reference("static_second");
                    wait_until(formal_start + 2'000'000'000 + static_cast<Nanoseconds>(options.source_static_s * 1e9));
                    set_freeze(false); frozen = false;
                    if (wait_until(formal_start + 2'200'000'000 + static_cast<Nanoseconds>(options.source_static_s * 1e9)))
                        reference("static_resume");
                }
            } catch (...) {
                if (frozen) try { adb_call(adb_for_preflight, {"-s", options.serial, "shell", "setprop",
                    "debug.pas.fixture_freeze", "0"}); } catch (...) {}
                auxiliary_failed();
            }
        });
        const auto resource_start = process_resources(clock);
        if (!journal.push({{"event", "resource_boundary"}, {"phase", "start"},
            {"before_ns", resource_start.before_ns}, {"after_ns", resource_start.after_ns},
            {"process_cpu_ns", resource_start.process_cpu_ns},
            {"working_set_bytes", resource_start.working_set_bytes},
            {"related_processes", resource_start.related_processes}, {"gpu_engines", resource_start.gpu_engines}}))
            throw std::runtime_error("critical journal overrun");
        const auto formal_end = formal_start + static_cast<Nanoseconds>(std::llround(options.duration_s * 1e9));
        std::uint64_t consumed = 0, skips = 0, decoded = 0;
        Nanoseconds next_resource_ns = formal_start;
        while (clock.now_ns() < formal_end && !worker_done) {
            if (preview && !preview->pump()) throw std::runtime_error("preview closed before benchmark end");
            if (clock.now_ns() >= next_resource_ns) {
                if (!memory_load.empty()) {
                    volatile std::uint8_t* pages = memory_load.data();
                    for (std::size_t offset = 0; offset < memory_load.size(); offset += 4096)
                        pages[offset] = static_cast<std::uint8_t>(pages[offset] + 1);
                }
                const auto sample = process_resources(clock);
                if (!journal.push({{"event", "resource_sample"},
                    {"before_ns", sample.before_ns}, {"after_ns", sample.after_ns},
                    {"process_cpu_ns", sample.process_cpu_ns},
                    {"working_set_bytes", sample.working_set_bytes},
                    {"related_processes", sample.related_processes}, {"gpu_engines", sample.gpu_engines}}))
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
            if (preview) preview->draw(*frame);
            if (!journal.push({{"event", "frame_consumed"}, {"frame_sequence", frame->sequence},
                {"consume_ns", consume}, {"capture_complete_ns", frame->capture_complete_ns},
                {"sequence_skip", sequence_skip}})) throw std::runtime_error("critical journal overrun");
            if (options.fixture) {
                if (auto counter = fixture_counter(*frame)) {
                    if ((options.fixture_schema == "legacy" &&
                         std::string_view(counter->schema) != "legacy_counter") ||
                        (options.fixture_schema == "native-v2" &&
                         std::string_view(counter->schema) != "native_v2_four_region" &&
                         !(options.backend == "scrcpy" &&
                           std::string_view(counter->schema) == "native_v2_lossy_validated")))
                        throw std::runtime_error("capture Fixture pixel schema differs from requested artifact");
                    ++decoded;
                    const auto quality = fixture_quality(*frame);
                    if (!journal.push({{"event", "fixture_counter"}, {"frame_sequence", frame->sequence},
                        {"capture_complete_ns", frame->capture_complete_ns},
                        {"counter", counter->value}, {"fixture_schema", counter->schema},
                        {"max_color_error", counter->max_color_error},
                        {"line_peak_y", quality.line_peak_y},
                        {"line_expected_y", quality.line_expected_y},
                        {"line_width_px", quality.line_width_px},
                        {"line_contrast", quality.line_contrast},
                        {"square_left_x", quality.square_left_x},
                        {"square_expected_x", quality.square_expected_x ? json(*quality.square_expected_x) : json(nullptr)},
                        {"low_contrast_delta", quality.low_contrast_delta},
                        {"square_width_px", quality.square_width_px}}))
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
            {"working_set_bytes", resource_end.working_set_bytes},
            {"related_processes", resource_end.related_processes}, {"gpu_engines", resource_end.gpu_engines}}))
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
        const auto frame_counters = latest.counters();
        summary["latest_frame_counters"] = {{"published", frame_counters.published},
            {"overwritten", frame_counters.overwritten}, {"pool_drops", frame_counters.pool_drops},
            {"consumer_skips", frame_counters.consumer_skips}};
        summary["capture_stats"] = capture ? json{{"valid", capture->stats().valid},
            {"inactive", capture->stats().inactive}, {"invalid", capture->stats().invalid},
            {"source_gaps", capture->stats().source_gaps},
            {"relative_stale_drops", capture->stats().relative_stale_drops}} : json(nullptr);
        summary["source_absolute_age"] = nullptr;
        summary["performance_pass"] = nullptr;
        summary["log_path"] = std::filesystem::absolute(raw).string();
        summary["diagnostic_png_sha256"] = sha256_file(options.output_dir / "diagnostic.png");
        std::ofstream file(options.output_dir / "summary.json", std::ios::binary);
        file << summary.dump(2) << '\n';
        std::cout << summary.dump(2) << '\n';
        if (options.fixture && decoded < consumed) throw std::runtime_error("Fixture pixels could not be decoded on all consumed frames");
        if (options.fixture_position_truth &&
            (summary.at("fixture_position_truth_missing").get<std::uint64_t>() != 0 ||
             summary.at("fixture_square_left_error_px").at("n").get<std::uint64_t>() != decoded))
            throw std::runtime_error("Fixture moving position truth could not be decoded on all consumed frames");
        if (options.fixture && (summary.at("fixture_samples").get<std::uint64_t>() < 2 ||
            summary.at("fixture_distinct").get<std::uint64_t>() < 2 ||
            !summary.at("fixture_counter_order_valid").get<bool>() ||
            !summary.at("fixture_counter_span_hz").is_number()))
            throw std::runtime_error("Fixture did not show a valid changing counter during formal window");
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

void run_five_capture_campaign(CaptureBenchOptions options, int normal_runs,
                               bool include_stress, double stability_s,
                               const std::string& source_revision,
                               const std::string& dirty_diff_sha256,
                               bool plan_only) {
    if (normal_runs < 3 || normal_runs > 20 || !std::isfinite(stability_s) ||
        stability_s < 0 || stability_s > 1800 || options.output_dir.empty() ||
        std::filesystem::exists(options.output_dir) || options.serial.empty() ||
        options.fixture_apk.empty() || options.window_hwnd.empty() ||
        options.scrcpy_server.empty() ||
        (!plan_only && (source_revision.empty() || dirty_diff_sha256.empty())))
        throw std::invalid_argument("five-candidate campaign requires frozen inputs and new output directory");
    struct Case { std::string name; std::string candidate; int target_hz; CaptureBenchOptions options; };
    const std::array<std::string, 5> candidates = {
        "grpc-control", "grpc-fast", "wgc", "dxgi", "scrcpy"};
    const auto configured = [&](const std::string& candidate) {
        auto choice = options;
        choice.fixture = true;
        choice.fixture_position_truth = true;
        choice.run_class = "formal_campaign";
        choice.warmup_s = 10;
        choice.duration_s = 60;
        if (candidate == "grpc-control" || candidate == "grpc-fast" || candidate == "wgc")
            choice.max_relative_lag_ms = 250;
        if (candidate == "grpc-control" || candidate == "grpc-fast") {
            choice.backend = "emulator-grpc";
            choice.grpc_copy_mode = candidate == "grpc-control" ? "legacy-rows" : "fast-memcpy";
        } else if (candidate == "mmap-diagnostic") {
            choice.backend = "emulator-grpc";
            choice.transport = "mmap";
            choice.diagnostic_mmap = true;
        } else {
            choice.backend = candidate;
            choice.source_rotation = 0;
            choice.grpc_rotate_ccw = false;
            if (candidate == "wgc") {
                if (choice.wgc_crop_x >= 0) choice.crop_x = choice.wgc_crop_x;
                if (choice.wgc_crop_y >= 0) choice.crop_y = choice.wgc_crop_y;
            } else if (candidate == "dxgi") {
                if (choice.dxgi_crop_x >= 0) choice.crop_x = choice.dxgi_crop_x;
                if (choice.dxgi_crop_y >= 0) choice.crop_y = choice.dxgi_crop_y;
            } else if (candidate == "scrcpy") {
                choice.crop_x = 0;
                choice.crop_y = 0;
                choice.window_hwnd.clear();
            }
        }
        return choice;
    };
    std::vector<Case> cases;
    const std::array<int, 3> targets = {40, 48, 57};
    for (std::size_t hz_index = 0; hz_index < targets.size(); ++hz_index) {
        for (int round = 0; round < normal_runs; ++round) {
            const auto shift = static_cast<std::size_t>((round + hz_index * 2) % candidates.size());
            for (std::size_t rank = 0; rank < candidates.size(); ++rank) {
                const auto& candidate = candidates[(rank + shift) % candidates.size()];
                cases.push_back({"normal-" + std::to_string(targets[hz_index]) + "-r" +
                    std::to_string(round + 1) + "-" + candidate,
                    candidate, targets[hz_index], configured(candidate)});
            }
        }
    }
    if (include_stress) {
        for (const auto& candidate : candidates) {
            for (const auto& kind : {"slow50", "recover100", "pause500", "cpu-load", "memory256",
                                    "static3", "gpu-load", "preview-off", "preview-on"}) {
                auto choice = configured(candidate);
                choice.duration_s = 30;
                if (std::string_view(kind) == "slow50") choice.consumer_delay_ms = 50;
                else if (std::string_view(kind) == "recover100") {
                    choice.consumer_delay_ms = 100;
                    choice.consumer_recover_after_s = 15;
                } else if (std::string_view(kind) == "pause500") choice.receiver_pause_ms = 500;
                else if (std::string_view(kind) == "cpu-load") choice.load = true;
                else if (std::string_view(kind) == "memory256") choice.memory_load_mib = 256;
                else if (std::string_view(kind) == "static3") choice.source_static_s = 3;
                else if (std::string_view(kind) == "gpu-load") choice.gpu_load = true;
                else if (std::string_view(kind) == "preview-on") choice.preview = true;
                cases.push_back({"stress-48-" + candidate + "-" + kind,
                                 candidate, 48, std::move(choice)});
            }
        }
    }
    if (stability_s > 0) {
        for (const auto& candidate : candidates) {
            auto choice = configured(candidate);
            choice.duration_s = stability_s;
            cases.push_back({"stability-48-" + candidate, candidate, 48, std::move(choice)});
        }
    }
    auto mmap = configured("mmap-diagnostic");
    mmap.duration_s = 60;
    cases.push_back({"diagnostic-48-mmap", "mmap-diagnostic", 48, std::move(mmap)});
    std::filesystem::create_directories(options.output_dir);
    HostClock clock;
    wchar_t executable[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, executable, MAX_PATH))
        throw std::runtime_error("campaign executable path unavailable");
    const auto adb = find_adb();
    const auto initial_preflight = device_preflight(adb, options.serial);
    const auto fixture_hash = sha256_file(options.fixture_apk);
    const auto installed_hash = installed_apk_sha256(adb, options.serial, "org.pas.capturefixture.cpp");
    if (fixture_hash != installed_hash)
        throw std::runtime_error("campaign Fixture APK differs from installed artifact");
    json planned = json::array();
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto& item = cases[i];
        planned.push_back({{"index", i}, {"name", item.name}, {"candidate", item.candidate},
            {"target_hz", item.target_hz}, {"warmup_s", item.options.warmup_s},
            {"duration_s", item.options.duration_s}, {"consumer_delay_ms", item.options.consumer_delay_ms},
            {"consumer_recover_after_s", item.options.consumer_recover_after_s ?
                json(*item.options.consumer_recover_after_s) : json(nullptr)},
            {"receiver_pause_ms", item.options.receiver_pause_ms}, {"host_load", item.options.load},
            {"memory_load_mib", item.options.memory_load_mib},
            {"source_static_s", item.options.source_static_s}, {"gpu_load", item.options.gpu_load},
            {"preview", item.options.preview},
            {"max_relative_lag_ms", item.options.max_relative_lag_ms ? json(*item.options.max_relative_lag_ms) : json(nullptr)},
            {"diagnostic_only", item.candidate == "mmap-diagnostic"}});
    }
    const auto plan = json{{"schema_version", 3}, {"kind", "five_capture_campaign"},
        {"order_fixed_before_runs", true}, {"rotation_rule", "candidate index (rank + round + 2*hz_index) mod 5"},
        {"normal_target_hz", targets}, {"normal_runs_per_target", normal_runs},
        {"source_revision", source_revision.empty() ? json(nullptr) : json(source_revision)},
        {"dirty_diff_sha256", dirty_diff_sha256.empty() ? json(nullptr) : json(dirty_diff_sha256)},
        {"binary_sha256", sha256_file(executable)}, {"fixture_apk_sha256", fixture_hash},
        {"installed_apk_sha256", installed_hash}, {"scrcpy_server_sha256", sha256_file(options.scrcpy_server)},
        {"serial", options.serial}, {"window_hwnd", options.window_hwnd},
        {"monitor_index", options.monitor_index}, {"crop_x", options.crop_x}, {"crop_y", options.crop_y},
        {"wgc_crop", {options.wgc_crop_x, options.wgc_crop_y}},
        {"dxgi_crop", {options.dxgi_crop_x, options.dxgi_crop_y}},
        {"width", options.width}, {"height", options.height},
        {"grpc_source_rotation", options.source_rotation},
        {"grpc_normalization_rotation_degrees", options.grpc_rotate_ccw ? -90 : 0},
        {"scrcpy_video_encoder", options.scrcpy_video_encoder},
        {"scrcpy_video_bit_rate", options.scrcpy_video_bit_rate},
        {"scrcpy_max_fps", options.scrcpy_max_fps},
        {"qpc_frequency", clock.frequency()}, {"preflight", initial_preflight},
        {"host_capture_environment", json::parse(capture_host_environment_json(options.window_hwnd))},
        {"performance_pass", nullptr}, {"cases", planned}};
    {
        std::ofstream out(options.output_dir / "campaign-plan.json", std::ios::binary);
        out << plan.dump(2) << '\n';
    }
    json completed = json::array();
    const auto save_results = [&](bool complete) {
        std::ofstream out(options.output_dir / "campaign-results.json", std::ios::binary);
        out << json{{"schema_version", 3}, {"complete", complete},
            {"all_succeeded", complete && std::all_of(completed.begin(), completed.end(),
                [](const json& item) { return item.value("returncode", 1) == 0; })},
            {"runs", completed}}.dump(2) << '\n';
    };
    save_results(false);
    if (plan_only) return;
    int active_hz = 0;
    adb_call(adb, {"-s", options.serial, "shell", "setprop", "debug.pas.fixture_freeze", "0"});
    for (auto& item : cases) {
        item.options.output_dir = options.output_dir / item.name;
        json ready_evidence = nullptr;
        try {
            if (item.target_hz != active_hz) {
                adb_call(adb, {"-s", options.serial, "shell", "am", "force-stop", "org.pas.capturefixture.cpp"});
                adb_call(adb, {"-s", options.serial, "shell", "setprop", "debug.pas.fixture_hz",
                               std::to_string(item.target_hz)});
                adb_call(adb, {"-s", options.serial, "shell", "am", "start", "-n",
                               "org.pas.capturefixture.cpp/android.app.NativeActivity"});
                active_hz = item.target_hz;
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
            const auto ready_start = clock.now_ns();
            const auto ready_deadline = ready_start + 10'000'000'000LL;
            std::optional<int> first_counter;
            while (clock.now_ns() < ready_deadline) {
                const auto sample = fixture_counter(capture_adb_png(clock, adb, options.serial, 1));
                if (sample && std::string_view(sample->schema) == "native_v2_four_region") {
                    if (!first_counter) first_counter = sample->value;
                    else if (sample->value != *first_counter) {
                        ready_evidence = {{"start_ns", ready_start}, {"end_ns", clock.now_ns()},
                            {"first_counter", *first_counter}, {"second_counter", sample->value},
                            {"basis", "two distinct four-region pixels via ADB diagnostic"}};
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (ready_evidence.is_null())
                throw std::runtime_error("Fixture READY preflight did not show two distinct valid pixel counters");
            run_capture_bench(item.options);
            std::ifstream input(item.options.output_dir / "summary.json", std::ios::binary);
            if (!input) throw std::runtime_error("campaign summary missing");
            const auto summary = json::parse(input);
            completed.push_back({{"name", item.name}, {"candidate", item.candidate},
                {"target_hz", item.target_hz}, {"returncode", 0},
                {"ready_preflight", ready_evidence},
                {"raw_sha256", summary.at("raw_sha256")},
                {"diagnostic_png_sha256", summary.at("diagnostic_png_sha256")}});
        } catch (const std::exception& error) {
            completed.push_back({{"name", item.name}, {"candidate", item.candidate},
                {"target_hz", item.target_hz}, {"returncode", 1},
                {"ready_preflight", ready_evidence}, {"error", error.what()}});
        }
        json artifacts = json::object();
        for (const auto* filename : {"capture.jsonl", "manifest.json", "diagnostic.png", "summary.json"})
            if (std::filesystem::is_regular_file(item.options.output_dir / filename))
                artifacts[filename] = sha256_file(item.options.output_dir / filename);
        completed.back()["artifacts_sha256"] = std::move(artifacts);
        save_results(completed.size() == cases.size());
    }
    const auto results = analyze_capture_campaign(options.output_dir);
    std::ofstream out(options.output_dir / "campaign-analysis.json", std::ios::binary);
    out << results.dump(2) << '\n';
    std::ofstream report(options.output_dir / "comparison.md", std::ios::binary);
    report << "# Five-path capture campaign\n\n"
           << "Source revision: `" << source_revision << "`; dirty diff SHA-256: `"
           << dirty_diff_sha256 << "`. Analysis schema 3; performance_pass is pending.\n\n"
           << "Each normal case uses 10 s warmup and a 60 s half-open measurement window. "
           << "Absolute Android source age is unknown; host timings start at the backend-specific "
           << "capture_complete point. MMAP is diagnostic-only.\n\n"
           << "| Case | Valid | Seconds | Captures | Distinct Fixture Hz | Arrival p50 / p95 / p99 / max ms | "
              "Host residence p50 / p95 / p99 / max ms | Longest no-frame ms | PAS CPU cores | Reason |\n"
           << "| --- | --- | ---: | ---: | ---: | --- | --- | ---: | ---: | --- |\n";
    const auto number = [](const json& object, const char* key) {
        if (!object.is_object() || !object.contains(key) || !object.at(key).is_number())
            return std::string("NA");
        std::ostringstream formatted;
        formatted << std::fixed << std::setprecision(2) << object.at(key).get<double>();
        return formatted.str();
    };
    const auto quartiles = [&](const json& distribution) {
        return number(distribution, "p50") + " / " + number(distribution, "p95") +
            " / " + number(distribution, "p99") + " / " + number(distribution, "max");
    };
    for (const auto& run : results.at("runs")) {
        report << "| " << run.at("name").get<std::string>() << " | "
               << (run.at("valid").get<bool>() ? "yes" : "no") << " | ";
        if (!run.contains("analysis")) {
            report << "NA | NA | NA | NA | NA | NA | NA | "
                   << run.value("error", "invalid evidence") << " |\n";
            continue;
        }
        const auto& item = run.at("analysis");
        const auto& resource = item.at("resource_window");
        report << number(item, "window_s") << " | " << item.at("capture_events") << " | "
               << number(item, "fixture_distinct_hz") << " | "
               << quartiles(item.at("arrival_interval_ms")) << " | "
               << quartiles(item.at("host_residency_ms")) << " | "
               << number(item.at("no_frame_gap_ms"), "max") << " | "
               << number(resource, "process_cpu_core_equivalent") << " | "
               << (run.at("diagnostic_only").get<bool>() ? "diagnostic only" : "") << " |\n";
    }
    report << "\nThe raw JSONL, per-case manifest and diagnostic PNG are indexed by "
              "`campaign-results.json`; `campaign-analysis.json` contains the recomputed "
              "distributions, quality checks and resource windows. No numerical performance "
              "threshold or main/backup selection is asserted by this generated report.\n";
    if (!results.at("formal_candidates_valid").get<bool>())
        throw std::runtime_error("five-candidate campaign has invalid or failed formal runs; all artifacts retained");
}

} // namespace pas
