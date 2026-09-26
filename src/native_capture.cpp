#include "pas/native_capture.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwmapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

namespace pas {
namespace {
using winrt::com_ptr;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::Capture;

struct NativeCaptureError : std::runtime_error {
    HRESULT code;
    NativeCaptureError(HRESULT value, std::string message)
        : std::runtime_error(std::move(message)), code(value) {}
};

void check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << operation << " failed with HRESULT 0x" << std::hex
                << static_cast<unsigned long>(result);
        throw NativeCaptureError(result, message.str());
    }
}

HWND selected_window(const std::string& value) {
    if (value.empty()) throw std::invalid_argument("explicit --window-hwnd required");
    std::size_t used = 0;
    const auto handle = std::stoull(value, &used, 0);
    if (used != value.size() || handle == 0 || handle > UINTPTR_MAX)
        throw std::invalid_argument("invalid window handle");
    const auto hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(handle));
    if (!IsWindow(hwnd)) throw std::runtime_error("selected capture window no longer exists");
    return hwnd;
}

std::string utf8(const wchar_t* value) {
    const int length = static_cast<int>(wcslen(value));
    if (!length) return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length,
                                          nullptr, 0, nullptr, nullptr);
    if (!count) return {};
    std::string out(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length,
                        out.data(), count, nullptr, nullptr);
    return out;
}

std::string handle_text(HWND hwnd) {
    std::ostringstream out;
    out << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(hwnd);
    return out.str();
}

BOOL CALLBACK describe_window(HWND hwnd, LPARAM parameter) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    RECT client{};
    if (!GetClientRect(hwnd, &client) || client.right - client.left < 100 ||
        client.bottom - client.top < 100) return TRUE;
    POINT origin{};
    if (!ClientToScreen(hwnd, &origin)) return TRUE;
    wchar_t title[256]{}, klass[256]{};
    GetWindowTextW(hwnd, title, 255);
    GetClassNameW(hwnd, klass, 255);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    auto* entries = reinterpret_cast<nlohmann::json*>(parameter);
    entries->push_back({{"hwnd", handle_text(hwnd)}, {"pid", pid},
                        {"class", utf8(klass)}, {"title", utf8(title)},
                        {"minimized", IsIconic(hwnd) != FALSE}, {"dpi", GetDpiForWindow(hwnd)},
                        {"client_origin_screen", {origin.x, origin.y}},
                        {"client_size", {client.right, client.bottom}}});
    return TRUE;
}

BOOL CALLBACK describe_top_window(HWND hwnd, LPARAM parameter) {
    describe_window(hwnd, parameter);
    EnumChildWindows(hwnd, describe_window, parameter);
    return TRUE;
}

struct D3dContext {
    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    com_ptr<ID3D11Texture2D> staging;
    int stage_width = 0;
    int stage_height = 0;

    void create(IDXGIAdapter* adapter = nullptr) {
        check(D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                device.put(), nullptr, context.put()), "D3D11CreateDevice");
    }

    Frame read(const Clock& clock, ID3D11Texture2D* source, const NativeCaptureOptions& options,
               Nanoseconds capture_complete, std::optional<std::int64_t> source_qpc,
               std::uint64_t sequence, std::uint64_t generation) {
        D3D11_TEXTURE2D_DESC source_desc{};
        source->GetDesc(&source_desc);
        if (source_desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
            source_desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
            throw std::runtime_error("native capture source is not BGRA8");
        if (options.crop_x < 0 || options.crop_y < 0 || options.width <= 0 || options.height <= 0 ||
            static_cast<std::uint64_t>(options.crop_x) + options.width > source_desc.Width ||
            static_cast<std::uint64_t>(options.crop_y) + options.height > source_desc.Height)
            throw std::runtime_error("native capture crop exceeds source texture; geometry invalidated");
        if (stage_width != options.width || stage_height != options.height) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = static_cast<UINT>(options.width);
            desc.Height = static_cast<UINT>(options.height);
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = source_desc.Format;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            staging = nullptr;
            check(device->CreateTexture2D(&desc, nullptr, staging.put()), "CreateTexture2D staging");
            stage_width = options.width;
            stage_height = options.height;
        }
        Frame frame;
        frame.sequence = sequence;
        frame.generation = generation;
        frame.geometry_version = generation;
        frame.width = options.width;
        frame.height = options.height;
        frame.stride = options.width * 3;
        frame.source_rotation = 0;
        frame.capture_backend = options.backend;
        frame.source_pixel_format = "BGRA8";
        frame.crop_x = options.crop_x;
        frame.crop_y = options.crop_y;
        frame.crop_width = options.width;
        frame.crop_height = options.height;
        frame.capture_complete_ns = capture_complete;
        frame.source_qpc_ticks = source_qpc;
        frame.readback_start_ns = clock.now_ns();
        D3D11_BOX box{static_cast<UINT>(options.crop_x), static_cast<UINT>(options.crop_y), 0,
                      static_cast<UINT>(options.crop_x + options.width),
                      static_cast<UINT>(options.crop_y + options.height), 1};
        context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, source, 0, &box);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped), "D3D11 Map staging");
        frame.readback_end_ns = clock.now_ns();
        frame.source_stride = static_cast<int>(mapped.RowPitch);
        frame.copy_start_ns = *frame.readback_end_ns;
        copy_bgra_rows_to_rgb24(static_cast<const std::uint8_t*>(mapped.pData),
            static_cast<std::size_t>(mapped.RowPitch) * frame.height,
            static_cast<int>(mapped.RowPitch), frame.width, frame.height, frame.rgb);
        context->Unmap(staging.get(), 0);
        frame.copy_end_ns = clock.now_ns();
        frame.pixels_ready_ns = *frame.copy_end_ns;
        return frame;
    }
};

std::uint64_t fingerprint(const std::vector<std::uint8_t>& rgb) {
    std::uint64_t value = 14695981039346656037ULL;
    for (const auto byte : rgb) { value ^= byte; value *= 1099511628211ULL; }
    return value;
}

void check_window(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
        throw std::runtime_error("selected capture window hidden, minimized, or destroyed");
}

void retain_wgc_module() {
    // This Windows build can return from Close before its internal worker exits.
    // RoUninitialize then unloads GraphicsCapture.dll under that worker (WER:
    // GraphicsCapture.dll_unloaded / c0000005). Keep only the system module pinned
    // for process lifetime; sessions, textures and apartments still close normally.
    static std::once_flag once;
    std::call_once(once, [] {
        const auto loaded = LoadLibraryExW(L"GraphicsCapture.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!loaded) throw std::runtime_error("cannot load system GraphicsCapture.dll");
        HMODULE pinned = nullptr;
        const auto success = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN |
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(loaded), &pinned);
        FreeLibrary(loaded);
        if (!success) throw std::runtime_error("cannot retain GraphicsCapture.dll for safe worker shutdown");
    });
}

void stream_wgc(const Clock& clock, const NativeCaptureOptions& options, HWND hwnd,
                std::stop_token stop, const std::atomic<bool>& cancelled,
                const std::function<void(Frame&&)>& on_frame,
                const std::function<void(std::string_view, Nanoseconds)>& on_signal,
                std::uint64_t& sequence, std::uint64_t generation) {
    retain_wgc_module();
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    struct ApartmentScope { ~ApartmentScope() { winrt::uninit_apartment(); } } apartment_scope;
    check_window(hwnd);
    if (!GraphicsCaptureSession::IsSupported())
        throw std::runtime_error("Windows Graphics Capture unsupported on this host");
    auto interop = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    GraphicsCaptureItem item{nullptr};
    check(interop->CreateForWindow(hwnd, winrt::guid_of<GraphicsCaptureItem>(),
                                   winrt::put_abi(item)), "CreateForWindow");
    D3dContext d3d;
    d3d.create();
    auto dxgi = d3d.device.as<IDXGIDevice>();
    com_ptr<IInspectable> inspectable;
    check(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), inspectable.put()),
          "CreateDirect3D11DeviceFromDXGIDevice");
    auto winrt_device = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    const auto initial_size = item.Size();
    const auto initial_dpi = GetDpiForWindow(hwnd);
    auto pool = Direct3D11CaptureFramePool::CreateFreeThreaded(winrt_device,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2, initial_size);
    auto session = pool.CreateCaptureSession(item);
    session.IsCursorCaptureEnabled(false);
    struct CallbackState {
        std::mutex mutex;
        std::exception_ptr failure;
        std::optional<std::uint64_t> last_hash;
        bool closing = false;
    };
    const auto state = std::make_shared<CallbackState>();
    auto token = pool.FrameArrived([&, state](auto const& sender, auto const&) {
        std::lock_guard lock(state->mutex);
        // A queued callback may outlive event revocation. Its shared state stays
        // alive, and closing is checked before any stack reference is accessed.
        if (state->closing || state->failure || stop.stop_requested() || cancelled.load()) return;
        try {
            auto frame = sender.TryGetNextFrame();
            if (!frame) return;
            const auto received = clock.now_ns();
            if (on_signal) on_signal("source_callback", received);
            check_window(hwnd);
            if (GetDpiForWindow(hwnd) != initial_dpi)
                throw std::runtime_error("WGC window DPI changed; crop profile invalidated");
            const auto size = frame.ContentSize();
            if (size.Width != initial_size.Width || size.Height != initial_size.Height)
                throw std::runtime_error("WGC content size changed; explicit crop profile must be recalibrated");
            auto access = frame.Surface().as<
                ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
            com_ptr<ID3D11Texture2D> texture;
            check(access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), texture.put_void()),
                  "GetInterface captured texture");
            auto normalized = d3d.read(clock, texture.get(), options, received,
                std::nullopt, ++sequence, generation);
            normalized.source_system_relative_100ns = frame.SystemRelativeTime().count();
            const auto hash = fingerprint(normalized.rgb);
            if (state->last_hash && *state->last_hash == hash) {
                if (on_signal) on_signal("duplicate_pixels", received);
                return;
            }
            state->last_hash = hash;
            on_frame(std::move(normalized));
        } catch (...) { state->failure = std::current_exception(); }
    });
    const auto close_capture = [&] {
        {
            std::lock_guard lock(state->mutex);
            state->closing = true; // Drain active callback and prevent further stack use.
        }
        pool.FrameArrived(token);
        session.Close();
        pool.Close();
    };
    try {
        session.StartCapture();
        while (!stop.stop_requested() && !cancelled.load()) {
            {
                std::lock_guard lock(state->mutex);
                if (state->failure) break;
            }
            check_window(hwnd);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    } catch (...) {
        close_capture();
        throw;
    }
    close_capture();
    std::lock_guard lock(state->mutex);
    if (state->failure) std::rethrow_exception(state->failure);
}

void stream_dxgi(const Clock& clock, const NativeCaptureOptions& options, HWND hwnd,
                 std::stop_token stop, const std::atomic<bool>& cancelled,
                 const std::function<void(Frame&&)>& on_frame,
                 const std::function<void(std::string_view, Nanoseconds)>& on_signal,
                 std::uint64_t& sequence, std::uint64_t& generation) {
    com_ptr<IDXGIFactory1> factory;
    check(CreateDXGIFactory1(winrt::guid_of<IDXGIFactory1>(), factory.put_void()),
          "CreateDXGIFactory1");
    com_ptr<IDXGIAdapter1> chosen_adapter;
    com_ptr<IDXGIOutput> chosen_output;
    int index = 0;
    for (UINT ai = 0;; ++ai) {
        com_ptr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(ai, adapter.put()) == DXGI_ERROR_NOT_FOUND) break;
        for (UINT oi = 0;; ++oi) {
            com_ptr<IDXGIOutput> output;
            if (adapter->EnumOutputs(oi, output.put()) == DXGI_ERROR_NOT_FOUND) break;
            if (index++ == options.monitor_index) {
                chosen_adapter = adapter;
                chosen_output = output;
                break;
            }
        }
        if (chosen_output) break;
    }
    if (!chosen_output) throw std::runtime_error("DXGI monitor index does not exist");
    DXGI_OUTPUT_DESC output_desc{};
    check(chosen_output->GetDesc(&output_desc), "IDXGIOutput GetDesc");
    if (output_desc.Rotation != DXGI_MODE_ROTATION_IDENTITY)
        throw std::runtime_error("rotated DXGI output requires a separate geometry profile");
    D3dContext d3d;
    d3d.create(chosen_adapter.get());
    auto output1 = chosen_output.as<IDXGIOutput1>();
    std::optional<std::uint64_t> last_hash;
    RECT initial_client{};
    check_window(hwnd);
    if (!GetClientRect(hwnd, &initial_client))
        throw std::runtime_error("DXGI initial client geometry unavailable");
    const auto initial_dpi = GetDpiForWindow(hwnd);
    const auto checked_crop = [&] {
            check_window(hwnd);
            RECT client{};
            if (!GetClientRect(hwnd, &client) ||
                client.right != initial_client.right || client.bottom != initial_client.bottom ||
                GetDpiForWindow(hwnd) != initial_dpi)
                throw std::runtime_error("DXGI client size or DPI changed; crop profile invalidated");
            if (GetAncestor(GetForegroundWindow(), GA_ROOT) != GetAncestor(hwnd, GA_ROOT))
                throw std::runtime_error("DXGI target is no longer the foreground window");
            POINT origin{};
            if (!ClientToScreen(hwnd, &origin)) throw std::runtime_error("window client origin unavailable");
            const auto left = origin.x + options.crop_x;
            const auto top = origin.y + options.crop_y;
            const auto& desktop = output_desc.DesktopCoordinates;
            const auto [output_x, output_y] = map_desktop_crop(left, top,
                options.width, options.height, desktop.left, desktop.top,
                desktop.right, desktop.bottom);
            const RECT target_rect{left, top, left + options.width, top + options.height};
            const auto root = GetAncestor(hwnd, GA_ROOT);
            for (auto above = GetWindow(root, GW_HWNDPREV); above;
                 above = GetWindow(above, GW_HWNDPREV)) {
                if (!IsWindowVisible(above) || IsIconic(above)) continue;
                DWORD cloaked = 0;
                if (SUCCEEDED(DwmGetWindowAttribute(above, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
                    cloaked) continue;
                RECT overlay{}, overlap{};
                const bool bounds_known = SUCCEEDED(DwmGetWindowAttribute(above,
                    DWMWA_EXTENDED_FRAME_BOUNDS, &overlay, sizeof(overlay))) ||
                    GetWindowRect(above, &overlay);
                if (!bounds_known)
                    throw std::runtime_error("DXGI higher-window bounds unavailable; visibility unproven");
                if (IntersectRect(&overlap, &target_rect, &overlay)) {
                    wchar_t title[256]{};
                    GetWindowTextW(above, title, 255);
                    throw std::runtime_error("DXGI target region intersects higher window " +
                        handle_text(above) + " (" + utf8(title) + ") at [" +
                        std::to_string(overlap.left) + "," + std::to_string(overlap.top) + "," +
                        std::to_string(overlap.right) + "," + std::to_string(overlap.bottom) + "]");
                }
            }
            for (int gy = 0; gy < 3; ++gy) for (int gx = 0; gx < 3; ++gx) {
                POINT point{left + (2*gx + 1) * options.width / 6,
                            top + (2*gy + 1) * options.height / 6};
                const auto visible = WindowFromPoint(point);
                if (!visible || (visible != hwnd && !IsChild(hwnd, visible)))
                    throw std::runtime_error("DXGI target region is occluded at a sampled point");
            }
            auto actual = options;
            actual.crop_x = output_x;
            actual.crop_y = output_y;
            return actual;
    };
    while (!stop.stop_requested() && !cancelled.load()) {
        com_ptr<IDXGIOutputDuplication> duplication;
        check(output1->DuplicateOutput(d3d.device.get(), duplication.put()), "DuplicateOutput");
        ++generation;
        while (!stop.stop_requested() && !cancelled.load()) {
            const auto actual = checked_crop();
            DXGI_OUTDUPL_FRAME_INFO info{};
            com_ptr<IDXGIResource> resource;
            const auto acquired = duplication->AcquireNextFrame(100, &info, resource.put());
            if (acquired == DXGI_ERROR_WAIT_TIMEOUT) continue;
            if (acquired == DXGI_ERROR_ACCESS_LOST) {
                if (on_signal) on_signal("access_lost", clock.now_ns());
                break;
            }
            check(acquired, "AcquireNextFrame");
            try {
                const auto received = clock.now_ns();
                if (on_signal) on_signal("source_callback", received);
                auto texture = resource.as<ID3D11Texture2D>();
                auto normalized = d3d.read(clock, texture.get(), actual, received,
                    info.LastPresentTime.QuadPart ? std::optional<std::int64_t>(info.LastPresentTime.QuadPart)
                                                  : std::nullopt,
                    ++sequence, generation);
                const auto confirmed = checked_crop();
                if (confirmed.crop_x != actual.crop_x || confirmed.crop_y != actual.crop_y)
                    throw std::runtime_error("DXGI target moved during acquisition; frame invalidated");
                const auto hash = fingerprint(normalized.rgb);
                if (!last_hash || *last_hash != hash) {
                    last_hash = hash;
                    on_frame(std::move(normalized));
                } else if (on_signal) on_signal("duplicate_pixels", received);
            } catch (...) {
                duplication->ReleaseFrame();
                throw;
            }
            check(duplication->ReleaseFrame(), "ReleaseFrame");
        }
    }
}
} // namespace

void copy_bgra_rows_to_rgb24(const std::uint8_t* source, std::size_t source_bytes,
                             int source_stride, int width, int height,
                             std::vector<std::uint8_t>& target) {
    if (!source || width < 1 || height < 1 || source_stride < 4LL * width ||
        static_cast<std::uint64_t>(width) * height * 3 > 16 * 1024 * 1024 ||
        static_cast<std::uint64_t>(source_stride) * height > source_bytes)
        throw std::invalid_argument("invalid BGRA row pitch or dimensions");
    target.resize(static_cast<std::size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        const auto* row = source + static_cast<std::size_t>(y) * source_stride;
        auto* rgb = target.data() + static_cast<std::size_t>(y) * width * 3;
        for (int x = 0; x < width; ++x) {
            rgb[3*x] = row[4*x+2];
            rgb[3*x+1] = row[4*x+1];
            rgb[3*x+2] = row[4*x];
        }
    }
}

std::pair<int, int> map_desktop_crop(int screen_x, int screen_y, int width, int height,
                                     int desktop_left, int desktop_top,
                                     int desktop_right, int desktop_bottom) {
    if (width <= 0 || height <= 0 || desktop_right <= desktop_left ||
        desktop_bottom <= desktop_top || screen_x < desktop_left || screen_y < desktop_top ||
        static_cast<std::int64_t>(screen_x) + width > desktop_right ||
        static_cast<std::int64_t>(screen_y) + height > desktop_bottom)
        throw std::runtime_error("DXGI target crop moved outside selected monitor");
    return {screen_x - desktop_left, screen_y - desktop_top};
}

std::string enumerate_capture_windows_json() {
    const auto previous_dpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!previous_dpi) throw std::runtime_error("per-monitor DPI awareness unavailable for window enumeration");
    struct DpiScope {
        DPI_AWARENESS_CONTEXT previous;
        ~DpiScope() { SetThreadDpiAwarenessContext(previous); }
    } dpi_scope{previous_dpi};
    nlohmann::json entries = nlohmann::json::array();
    EnumWindows(describe_top_window, reinterpret_cast<LPARAM>(&entries));
    return entries.dump(2);
}

std::string capture_host_environment_json(const std::string& window_hwnd) {
    const auto previous_dpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!previous_dpi) throw std::runtime_error("per-monitor DPI awareness unavailable for environment probe");
    struct DpiScope {
        DPI_AWARENESS_CONTEXT previous;
        ~DpiScope() { SetThreadDpiAwarenessContext(previous); }
    } dpi_scope{previous_dpi};
    nlohmann::json report;
    report["screen_primary_pixels"] = {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    report["system_dpi"] = GetDpiForSystem();
    OSVERSIONINFOW windows_version{};
    windows_version.dwOSVersionInfoSize = sizeof(windows_version);
    using VersionFunction = LONG (WINAPI*)(OSVERSIONINFOW*);
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto version_function = ntdll ? reinterpret_cast<VersionFunction>(
        GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
    report["windows_version"] = version_function && version_function(&windows_version) == 0
        ? nlohmann::json::array({windows_version.dwMajorVersion,
            windows_version.dwMinorVersion, windows_version.dwBuildNumber}) : nlohmann::json(nullptr);
    SYSTEM_POWER_STATUS power{};
    report["ac_power"] = GetSystemPowerStatus(&power) && power.ACLineStatus != 255
        ? nlohmann::json(power.ACLineStatus == 1) : nlohmann::json(nullptr);
    report["selected_window_dpi"] = window_hwnd.empty() ? nlohmann::json(nullptr) :
        nlohmann::json(GetDpiForWindow(selected_window(window_hwnd)));
    report["gpu_engine_samples"] = nullptr;
    report["gpu_engine_missing_reason"] = "GPU engine performance counters not yet collected";
    com_ptr<IDXGIFactory1> factory;
    check(CreateDXGIFactory1(winrt::guid_of<IDXGIFactory1>(), factory.put_void()),
          "environment CreateDXGIFactory1");
    nlohmann::json adapters = nlohmann::json::array();
    for (UINT ai = 0;; ++ai) {
        com_ptr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(ai, adapter.put()) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 description{};
        check(adapter->GetDesc1(&description), "environment GetDesc1");
        LARGE_INTEGER driver_version{};
        const auto driver_known = SUCCEEDED(adapter->CheckInterfaceSupport(
            winrt::guid_of<IDXGIDevice>(), &driver_version));
        nlohmann::json outputs = nlohmann::json::array();
        for (UINT oi = 0;; ++oi) {
            com_ptr<IDXGIOutput> output;
            if (adapter->EnumOutputs(oi, output.put()) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_OUTPUT_DESC desc{};
            check(output->GetDesc(&desc), "environment output GetDesc");
            DEVMODEW mode{};
            mode.dmSize = sizeof(mode);
            const bool current = EnumDisplaySettingsW(desc.DeviceName, ENUM_CURRENT_SETTINGS, &mode);
            outputs.push_back({{"index_on_adapter", oi}, {"name", utf8(desc.DeviceName)},
                {"desktop_rect", {desc.DesktopCoordinates.left, desc.DesktopCoordinates.top,
                    desc.DesktopCoordinates.right, desc.DesktopCoordinates.bottom}},
                {"rotation", desc.Rotation}, {"attached", desc.AttachedToDesktop != FALSE},
                {"refresh_hz", current ? nlohmann::json(mode.dmDisplayFrequency) : nlohmann::json(nullptr)}});
        }
        adapters.push_back({{"adapter_index", ai}, {"name", utf8(description.Description)},
            {"vendor_id", description.VendorId}, {"device_id", description.DeviceId},
            {"driver_version_raw", driver_known ? nlohmann::json(driver_version.QuadPart)
                : nlohmann::json(nullptr)},
            {"dedicated_video_memory_bytes", description.DedicatedVideoMemory},
            {"outputs", outputs}});
    }
    report["dxgi_adapters"] = adapters;
    return report.dump(2);
}

NativeCapture::NativeCapture(const Clock& clock, NativeCaptureOptions options)
    : clock_(clock), options_(std::move(options)) {
    if ((options_.backend != "wgc" && options_.backend != "dxgi") ||
        options_.width < 1 || options_.height < 1 || options_.width > 4096 ||
        options_.height > 4096 || options_.monitor_index < 0 ||
        options_.crop_x < 0 || options_.crop_y < 0 ||
        options_.crop_x > 16384 || options_.crop_y > 16384)
        throw std::invalid_argument("invalid native capture options");
}

void NativeCapture::stream(std::stop_token stop, const std::function<void(Frame&&)>& on_frame,
                           const std::function<void(std::string_view, Nanoseconds)>& on_signal) {
    const auto hwnd = selected_window(options_.window_hwnd);
    const auto previous_dpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!previous_dpi) throw std::runtime_error("per-monitor DPI awareness unavailable for native capture");
    struct DpiScope {
        DPI_AWARENESS_CONTEXT previous;
        ~DpiScope() { SetThreadDpiAwarenessContext(previous); }
    } dpi_scope{previous_dpi};
    std::uint64_t sequence = 0, generation = 0;
    int rebuilds = 0;
    const auto recoverable = [](HRESULT code) {
        return code == DXGI_ERROR_DEVICE_REMOVED || code == DXGI_ERROR_DEVICE_RESET ||
            code == DXGI_ERROR_ACCESS_LOST;
    };
    while (!stop.stop_requested() && !cancel_.load()) {
        try {
            if (options_.backend == "wgc")
                stream_wgc(clock_, options_, hwnd, stop, cancel_, on_frame, on_signal,
                           sequence, ++generation);
            else
                stream_dxgi(clock_, options_, hwnd, stop, cancel_, on_frame, on_signal,
                            sequence, generation);
            return;
        } catch (const NativeCaptureError& error) {
            if (!recoverable(error.code) || rebuilds++ >= 2) throw;
        } catch (const winrt::hresult_error& error) {
            if (!recoverable(static_cast<HRESULT>(error.code())) || rebuilds++ >= 2) throw;
        }
        if (on_signal) on_signal("device_lost_rebuild", clock_.now_ns());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void NativeCapture::cancel() { cancel_ = true; }

} // namespace pas
