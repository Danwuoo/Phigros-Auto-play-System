#pragma once

#include "pas/core.hpp"
#include "pas/grpc_transport.hpp"

#include <grpcpp/grpcpp.h>
#include "emulator_controller.grpc.pb.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <stop_token>

namespace pas {

struct GrpcEndpoint {
    std::string target;
    std::string token;
    std::string instance;
};

GrpcEndpoint discover_endpoint(const std::string& serial,
                               const std::filesystem::path& running_dir = {});

struct CaptureOptions {
    int grpc_read_chunk_kib = default_grpc_read_chunk_kib;
    int width = 0;
    int height = 0;
    int source_rotation = -1;
    bool rgba = false;
    bool bottom_up = false;
    bool optimized_rgb_copy = true;
    bool rotate_ccw = false;
    bool diagnostic_mmap = false;
    std::size_t max_rgb_bytes = 16 * 1024 * 1024;
    std::optional<Nanoseconds> max_relative_lag_ns;
};

struct CaptureStats {
    std::uint64_t valid = 0;
    std::uint64_t inactive = 0;
    std::uint64_t invalid = 0;
    std::uint64_t source_gaps = 0;
    std::uint64_t relative_stale_drops = 0;
    Nanoseconds last_notification_ns = 0;
};

class GrpcCapture final {
public:
    GrpcCapture(const Clock& clock, GrpcEndpoint endpoint, CaptureOptions options);
    ~GrpcCapture();
    GrpcCapture(const GrpcCapture&) = delete;
    GrpcCapture& operator=(const GrpcCapture&) = delete;
    // Blocking worker call. Cancellation is safe from another thread.
    void stream(std::stop_token stop, const std::function<void(Frame&&)>& on_frame);
    void cancel();
    Frame snapshot(std::chrono::milliseconds timeout);
    std::string probe_pixels(const Frame& last, std::chrono::milliseconds timeout);
    CaptureStats stats() const;
    bool diagnostic_only() const { return options_.diagnostic_mmap; }
private:
    Frame normalize(const android::emulation::control::Image& image,
                    Nanoseconds arrival_ns, const std::uint8_t* alternate = nullptr,
                    std::size_t alternate_bytes = 0) const;
    const Clock& clock_;
    GrpcEndpoint endpoint_;
    CaptureOptions options_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<android::emulation::control::EmulatorController::Stub> stub_;
    mutable std::mutex mutex_;
    grpc::ClientContext* active_context_ = nullptr;
    CaptureStats stats_;
    std::optional<std::uint64_t> last_source_sequence_;
    std::optional<std::int64_t> last_source_time_us_;
    std::optional<Nanoseconds> last_host_arrival_ns_;
    std::optional<Nanoseconds> stagnant_source_since_ns_;
    std::optional<std::pair<Nanoseconds, std::int64_t>> lag_anchor_;
    Nanoseconds last_published_ns_ = 0;
    std::uint64_t sequence_ = 0;
};

class GrpcTouch final : public TouchBackend {
public:
    GrpcTouch(const Clock& clock, GrpcEndpoint endpoint, PixelCoordinateMap mapping,
              int width, int height, int max_contacts = 2,
              std::chrono::milliseconds timeout = std::chrono::milliseconds(500));
    TouchReceipt inject(const TouchCommand& command) override;
    std::vector<TouchReceipt> inject_batch(const std::vector<TouchCommand>& commands);
    ReleaseReport release_all() override;
    ReleaseReport emergency_release_all();
    void simulate_transport_loss_for_fixture_test();
    bool faulted() const { return faulted_; }
private:
    bool send(int id, int x, int y, int pressure);
    bool send_batch(const std::vector<std::array<int, 4>>& contacts);
    const Clock& clock_;
    GrpcEndpoint endpoint_;
    PixelCoordinateMap mapping_;
    int width_, height_, max_contacts_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<android::emulation::control::EmulatorController::Stub> stub_;
    std::map<int, std::array<int, 2>> positions_;
    std::set<int> unknown_;
    bool faulted_ = false;
};

} // namespace pas
