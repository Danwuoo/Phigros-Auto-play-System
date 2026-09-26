#pragma once

#include "pas/core.hpp"

#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pas {

// Window handle is selected explicitly from capture-windows. Crops are in
// captured-window pixels for WGC and window-client pixels for DXGI.
struct NativeCaptureOptions {
    std::string backend;
    std::string window_hwnd;
    int monitor_index = 0;
    int crop_x = 0;
    int crop_y = 0;
    int width = 0;
    int height = 0;
    Nanoseconds max_relative_lag_ns = 250'000'000;
};

// Compares elapsed host and source time only; never estimates absolute age.
class NativeRelativeLagGuard {
public:
    explicit NativeRelativeLagGuard(Nanoseconds limit) : limit_(limit) {}
    bool accept(Nanoseconds host, Nanoseconds source);
private:
    Nanoseconds limit_;
    std::optional<Nanoseconds> offset_, last_host_, last_source_;
};

std::string enumerate_capture_windows_json();
std::string capture_host_environment_json(const std::string& window_hwnd = {});
void copy_bgra_rows_to_rgb24(const std::uint8_t* source, std::size_t source_bytes,
                             int source_stride, int width, int height,
                             std::vector<std::uint8_t>& target);
std::pair<int, int> map_desktop_crop(int screen_x, int screen_y, int width, int height,
                                     int desktop_left, int desktop_top,
                                     int desktop_right, int desktop_bottom);

class NativeCapture final {
public:
    NativeCapture(const Clock& clock, NativeCaptureOptions options);
    void stream(std::stop_token stop, const std::function<void(Frame&&)>& on_frame,
                const std::function<void(std::string_view, Nanoseconds)>& on_signal = {});
    void cancel();
private:
    const Clock& clock_;
    NativeCaptureOptions options_;
    std::atomic<bool> cancel_{false};
};

} // namespace pas
