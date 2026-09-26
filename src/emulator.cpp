#include "pas/emulator.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#define NOMINMAX
#include <windows.h>

namespace pas {
namespace pb = android::emulation::control;

namespace {
std::filesystem::path default_running_dir() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD size = GetTempPathW(MAX_PATH, buffer);
    if (!size || size >= MAX_PATH) throw std::runtime_error("temporary directory unavailable");
    return std::filesystem::path(buffer) / L"avd" / L"running";
}

bool decimal(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(),
        [](unsigned char c) { return std::isdigit(c) != 0; });
}

std::string local_target(const std::string& target) {
    if (target.rfind("127.0.0.1:", 0) != 0 && target.rfind("localhost:", 0) != 0)
        throw std::invalid_argument("gRPC target must be local loopback");
    return target;
}

struct MmapSnapshot {
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE mapping = nullptr;
    const std::uint8_t* data = nullptr;
    std::filesystem::path path;
    std::size_t capacity = 0;
    ~MmapSnapshot() {
        if (data) UnmapViewOfFile(data);
        if (mapping) CloseHandle(mapping);
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        if (!path.empty()) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    }
    void create(std::size_t size) {
        wchar_t temp[MAX_PATH]{};
        if (!GetTempPathW(MAX_PATH, temp)) throw std::runtime_error("MMAP temp path unavailable");
        wchar_t name[MAX_PATH]{};
        if (!GetTempFileNameW(temp, L"pas", 0, name)) throw std::runtime_error("MMAP temp file unavailable");
        path = name;
        file = CreateFileW(name, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("MMAP file open failed");
        LARGE_INTEGER length{}; length.QuadPart = static_cast<LONGLONG>(size);
        if (!SetFilePointerEx(file, length, nullptr, FILE_BEGIN) || !SetEndOfFile(file))
            throw std::runtime_error("MMAP file sizing failed");
        mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
        if (!mapping) throw std::runtime_error("MMAP mapping failed");
        data = static_cast<const std::uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size));
        if (!data) throw std::runtime_error("MMAP view failed");
        capacity = size;
    }
    std::string uri() const {
        auto raw = path.generic_string();
        return "file:///" + raw;
    }
};
}

GrpcEndpoint discover_endpoint(const std::string& serial, const std::filesystem::path& directory) {
    constexpr std::string_view prefix = "emulator-";
    if (serial.rfind(prefix, 0) != 0 || !decimal(std::string_view(serial).substr(prefix.size())))
        throw std::invalid_argument("explicit emulator-NNNN serial required");
    const auto port = serial.substr(prefix.size());
    const auto root = directory.empty() ? default_running_dir() : directory;
    std::vector<GrpcEndpoint> matches;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ini" ||
            entry.path().filename().string().rfind("pid_", 0) != 0) continue;
        std::ifstream input(entry.path());
        std::map<std::string, std::string> fields;
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const auto eq = line.find('=');
            if (eq != std::string::npos) fields[line.substr(0, eq)] = line.substr(eq + 1);
        }
        if (fields["port.serial"] != port) continue;
        const auto grpc_port = fields["grpc.port"];
        if (!decimal(grpc_port) || std::stoul(grpc_port) > 65535 || std::stoul(grpc_port) == 0 ||
            fields["grpc.token"].empty())
            throw std::runtime_error("emulator discovery lacks valid port or token");
        matches.push_back({"127.0.0.1:" + grpc_port, fields["grpc.token"], entry.path().stem().string()});
    }
    if (matches.size() != 1)
        throw std::runtime_error("expected exactly one discovery file for selected emulator serial");
    return matches.front();
}

GrpcCapture::GrpcCapture(const Clock& clock, GrpcEndpoint endpoint, CaptureOptions options)
    : clock_(clock), endpoint_(std::move(endpoint)), options_(options) {
    local_target(endpoint_.target);
    if (endpoint_.token.empty() || options_.width < 0 || options_.height < 0 ||
        options_.max_rgb_bytes == 0 || options_.max_rgb_bytes > 16 * 1024 * 1024 ||
        (options_.diagnostic_mmap && (options_.width == 0 || options_.height == 0)))
        throw std::invalid_argument("invalid capture endpoint or options");
    auto args = capture_channel_arguments(options_.grpc_read_chunk_kib);
    channel_ = grpc::CreateCustomChannel(endpoint_.target, grpc::InsecureChannelCredentials(), args);
    stub_ = pb::EmulatorController::NewStub(channel_);
}

GrpcCapture::~GrpcCapture() { cancel(); }

Frame GrpcCapture::normalize(const pb::Image& image, Nanoseconds arrival_ns,
                             const std::uint8_t* alternate, std::size_t alternate_bytes) const {
    const auto width = image.format().width();
    const auto height = image.format().height();
    const int channels = options_.rgba ? 4 : 3;
    const auto expected_format = options_.rgba ? pb::ImageFormat::RGBA8888 : pb::ImageFormat::RGB888;
    if (!width || !height || width > 4096 || height > 4096 ||
        image.format().format() != expected_format ||
        (options_.width && width != static_cast<std::uint32_t>(options_.width)) ||
        (options_.height && height != static_cast<std::uint32_t>(options_.height)) ||
        (options_.source_rotation >= 0 && image.format().rotation().rotation() != options_.source_rotation))
        throw std::runtime_error("invalid or changed screenshot geometry/format/rotation: " +
            std::to_string(width) + "x" + std::to_string(height) +
            " format=" + std::to_string(static_cast<int>(image.format().format())) +
            " rotation=" + std::to_string(image.format().rotation().rotation()) +
            " expected=" + std::to_string(options_.width) + "x" +
            std::to_string(options_.height) + " format=" +
            std::to_string(static_cast<int>(expected_format)) + " rotation=" +
            std::to_string(options_.source_rotation));
    const auto pixels = static_cast<std::uint64_t>(width) * height;
    if (pixels * 3 > options_.max_rgb_bytes) throw std::runtime_error("RGB capacity exceeded");
    const auto raw_bytes = static_cast<std::size_t>(pixels * channels);
    const auto* raw = alternate ? alternate : reinterpret_cast<const std::uint8_t*>(image.image().data());
    const auto length = alternate ? alternate_bytes : image.image().size();
    if (length != raw_bytes) throw std::runtime_error("invalid screenshot payload length");
    Frame frame;
    frame.width = static_cast<int>(options_.rotate_ccw ? height : width);
    frame.height = static_cast<int>(options_.rotate_ccw ? width : height);
    frame.stride = frame.width * 3;
    frame.source_rotation = image.format().rotation().rotation();
    frame.normalization_rotation_degrees = options_.rotate_ccw ? -90 : 0;
    frame.capture_backend = options_.diagnostic_mmap ? "emulator-mmap-diagnostic" : "emulator-grpc";
    frame.source_pixel_format = options_.rgba ? "RGBA8888" : "RGB888";
    frame.source_stride = static_cast<int>(width) * channels;
    frame.crop_width = static_cast<int>(width);
    frame.crop_height = static_cast<int>(height);
    frame.source_valid = !options_.diagnostic_mmap;
    frame.capture_complete_ns = arrival_ns;
    frame.copy_start_ns = clock_.now_ns();
    frame.rgb.resize(static_cast<std::size_t>(pixels * 3));
    if (options_.rotate_ccw) {
        // The emulator's RGB screenshot follows the sensor's portrait axes even
        // while Android presents a landscape app. Preserve the source geometry
        // above and convert it to the same landscape RGB profile as other paths.
        std::vector<std::uint8_t> legacy_rows;
        if (!options_.optimized_rgb_copy && channels == 3 && !options_.bottom_up) {
            legacy_rows.resize(raw_bytes);
            for (std::uint32_t y = 0; y < height; ++y)
                std::copy_n(raw + static_cast<std::size_t>(y) * width * 3,
                            static_cast<std::size_t>(width) * 3,
                            legacy_rows.data() + static_cast<std::size_t>(y) * width * 3);
            raw = legacy_rows.data();
        }
        for (std::uint32_t target_y = 0; target_y < width; ++target_y) {
            const auto source_x = width - 1 - target_y;
            for (std::uint32_t target_x = 0; target_x < height; ++target_x) {
                const auto source_y = options_.bottom_up ? height - 1 - target_x : target_x;
                const auto* source = raw +
                    (static_cast<std::size_t>(source_y) * width + source_x) * channels;
                auto* target = frame.rgb.data() +
                    (static_cast<std::size_t>(target_y) * height + target_x) * 3;
                std::copy_n(source, 3, target);
            }
        }
    } else if (channels == 3 && !options_.bottom_up && options_.optimized_rgb_copy) {
        std::memcpy(frame.rgb.data(), raw, raw_bytes);
    } else for (std::uint32_t y = 0; y < height; ++y) {
        const auto source_y = options_.bottom_up ? height - 1 - y : y;
        const auto* row = raw + static_cast<std::size_t>(source_y) * width * channels;
        auto* target = frame.rgb.data() + static_cast<std::size_t>(y) * width * 3;
        if (channels == 3) std::copy_n(row, static_cast<std::size_t>(width) * 3, target);
        else for (std::uint32_t x = 0; x < width; ++x) {
            target[3*x] = row[4*x]; target[3*x+1] = row[4*x+1]; target[3*x+2] = row[4*x+2];
        }
    }
    frame.copy_end_ns = clock_.now_ns();
    frame.pixels_ready_ns = *frame.copy_end_ns;
    return frame;
}

void GrpcCapture::stream(std::stop_token stop, const std::function<void(Frame&&)>& on_frame) {
    pb::ImageFormat request;
    request.set_format(options_.rgba ? pb::ImageFormat::RGBA8888 : pb::ImageFormat::RGB888);
    request.set_width(options_.width);
    request.set_height(options_.height);
    MmapSnapshot mmap;
    if (options_.diagnostic_mmap) {
        mmap.create(std::min<std::size_t>(32 * 1024 * 1024, (options_.max_rgb_bytes * 4 + 2) / 3));
        request.mutable_transport()->set_channel(pb::ImageTransport::MMAP);
        request.mutable_transport()->set_handle(mmap.uri());
    }
    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer " + endpoint_.token);
    { std::lock_guard lock(mutex_); active_context_ = &context; }
    std::stop_callback on_stop(stop, [&] { cancel(); });
    try {
        auto reader = stub_->streamScreenshot(&context, request);
        pb::Image image;
        while (!stop.stop_requested()) {
            const auto receive_start = clock_.now_ns();
            if (!reader->Read(&image)) break;
            const auto arrival = clock_.now_ns();
            { std::lock_guard lock(mutex_); stats_.last_notification_ns = arrival; }
            if (!image.format().width() && !image.format().height()) {
                std::lock_guard lock(mutex_); ++stats_.inactive; continue;
            }
            std::vector<std::uint8_t> snapshot;
            const std::uint8_t* alternate = nullptr;
            std::size_t alternate_bytes = 0;
            Nanoseconds copy_complete = arrival;
            if (options_.diagnostic_mmap) {
                if (!image.image().empty()) throw std::runtime_error("MMAP notification contained payload");
                if (!image.format().width() || !image.format().height() ||
                    image.format().width() > 4096 || image.format().height() > 4096)
                    throw std::runtime_error("MMAP notification geometry invalid");
                const auto needed = static_cast<std::uint64_t>(image.format().width()) *
                                    image.format().height() * (options_.rgba ? 4 : 3);
                alternate_bytes = static_cast<std::size_t>(needed);
                if (!alternate_bytes || alternate_bytes > mmap.capacity)
                    throw std::runtime_error("MMAP notification exceeds mapping");
                snapshot.assign(mmap.data, mmap.data + alternate_bytes);
                alternate = snapshot.data();
                copy_complete = clock_.now_ns();
            }
            Frame frame;
            try { frame = normalize(image, copy_complete, alternate, alternate_bytes); }
            catch (...) { std::lock_guard lock(mutex_); ++stats_.invalid; throw; }
            frame.receive_start_ns = receive_start;
            frame.notification_received_ns = arrival;
            const auto source_sequence = static_cast<std::uint64_t>(image.seq());
            if (last_source_sequence_ && source_sequence <= *last_source_sequence_)
                throw std::runtime_error("source sequence reset; reconstruct capture source");
            if (last_source_sequence_ && source_sequence > *last_source_sequence_ + 1) {
                std::lock_guard lock(mutex_);
                stats_.source_gaps += source_sequence - *last_source_sequence_ - 1;
            }
            last_source_sequence_ = source_sequence;
            frame.source_sequence = source_sequence;
            // Unix source metadata is retained only as a separate domain.
            const auto source_us = static_cast<std::int64_t>(image.timestampus());
            if (source_us) frame.source_timestamp_us = source_us;
            if (options_.max_relative_lag_ns) {
                if (source_us <= 0) throw std::runtime_error("relative lag guard requires source timestamp");
                if (last_source_time_us_) {
                    if (source_us < *last_source_time_us_)
                        throw std::runtime_error("source timestamp regressed");
                    const auto source_delta_us = source_us - *last_source_time_us_;
                    if (source_delta_us > 10'000'000)
                        throw std::runtime_error("source timestamp jumped beyond 10 seconds");
                    const auto source_delta = source_delta_us * 1000;
                    const auto host_delta = arrival - *last_host_arrival_ns_;
                    if (host_delta < 0 || host_delta > 10'000'000'000 ||
                        source_delta > std::max<Nanoseconds>(1'000'000'000, host_delta + 500'000'000))
                        throw std::runtime_error("source timestamp discontinuity");
                    if (source_delta == 0) {
                        if (!stagnant_source_since_ns_) stagnant_source_since_ns_ = *last_host_arrival_ns_;
                        if (arrival - *stagnant_source_since_ns_ >= 250'000'000)
                            throw std::runtime_error("source timestamp stopped");
                    } else stagnant_source_since_ns_.reset();
                }
                last_source_time_us_ = source_us;
                last_host_arrival_ns_ = arrival;
                if (!lag_anchor_) lag_anchor_ = {arrival, source_us};
                const auto anchor_delta_us = source_us - lag_anchor_->second;
                if (anchor_delta_us < 0 || anchor_delta_us > std::numeric_limits<Nanoseconds>::max() / 1000)
                    throw std::runtime_error("source timestamp anchor overflow");
                const auto lag = arrival - lag_anchor_->first - anchor_delta_us * 1000;
                if (lag > *options_.max_relative_lag_ns) {
                    { std::lock_guard lock(mutex_); ++stats_.relative_stale_drops; }
                    if (last_published_ns_ && arrival - last_published_ns_ >= 1'000'000'000)
                        throw std::runtime_error("relative lag guard made no progress");
                    continue;
                }
            }
            frame.sequence = ++sequence_;
            on_frame(std::move(frame));
            last_published_ns_ = arrival;
            { std::lock_guard lock(mutex_); ++stats_.valid; }
        }
        const auto status = reader->Finish();
        if (!stop.stop_requested() && !status.ok())
            throw std::runtime_error("screenshot stream failed: " + std::to_string(status.error_code()));
    } catch (...) {
        { std::lock_guard lock(mutex_); active_context_ = nullptr; }
        throw;
    }
    { std::lock_guard lock(mutex_); active_context_ = nullptr; }
}

void GrpcCapture::cancel() {
    std::lock_guard lock(mutex_);
    if (active_context_) active_context_->TryCancel();
}

std::string GrpcCapture::probe_pixels(const Frame& last, std::chrono::milliseconds timeout) {
    try {
        auto frame = snapshot(timeout);
        return frame.width == last.width && frame.height == last.height && frame.rgb == last.rgb
            ? "static" : "changed";
    } catch (...) { return "failed"; }
}

Frame GrpcCapture::snapshot(std::chrono::milliseconds timeout) {
    pb::ImageFormat request;
    request.set_format(options_.rgba ? pb::ImageFormat::RGBA8888 : pb::ImageFormat::RGB888);
    request.set_width(options_.width); request.set_height(options_.height);
    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer " + endpoint_.token);
    context.set_deadline(std::chrono::system_clock::now() + timeout);
    pb::Image image;
    const auto status = stub_->getScreenshot(&context, request, &image);
    if (!status.ok()) throw std::runtime_error("screenshot probe failed: " + std::to_string(status.error_code()));
    auto frame = normalize(image, clock_.now_ns());
    frame.sequence = static_cast<std::uint64_t>(image.seq());
    frame.source_sequence = frame.sequence;
    if (image.timestampus()) frame.source_timestamp_us = static_cast<std::int64_t>(image.timestampus());
    return frame;
}

CaptureStats GrpcCapture::stats() const {
    std::lock_guard lock(mutex_);
    return stats_;
}

GrpcTouch::GrpcTouch(const Clock& clock, GrpcEndpoint endpoint, PixelCoordinateMap mapping,
                     int width, int height, int max_contacts, std::chrono::milliseconds timeout)
    : clock_(clock), endpoint_(std::move(endpoint)), mapping_(std::move(mapping)),
      width_(width), height_(height), max_contacts_(max_contacts), timeout_(timeout) {
    local_target(endpoint_.target);
    if (endpoint_.token.empty() || width < 2 || height < 2 || max_contacts < 1 || max_contacts > 10 ||
        timeout <= std::chrono::milliseconds(0) || timeout > std::chrono::seconds(2))
        throw std::invalid_argument("invalid gRPC touch configuration");
    channel_ = grpc::CreateChannel(endpoint_.target, grpc::InsecureChannelCredentials());
    stub_ = pb::EmulatorController::NewStub(channel_);
}

bool GrpcTouch::send(int id, int x, int y, int pressure) {
    return send_batch({{id, x, y, pressure}});
}

bool GrpcTouch::send_batch(const std::vector<std::array<int, 4>>& contacts) {
    pb::TouchEvent event;
    for (const auto& contact : contacts) {
        auto* touch = event.add_touches();
        touch->set_identifier(contact[0]); touch->set_x(contact[1]);
        touch->set_y(contact[2]); touch->set_pressure(contact[3]);
    }
    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer " + endpoint_.token);
    context.set_deadline(std::chrono::system_clock::now() + timeout_);
    google::protobuf::Empty response;
    const auto status = stub_->sendTouch(&context, event, &response);
    if (!status.ok()) faulted_ = true;
    return status.ok();
}

std::vector<TouchReceipt> GrpcTouch::inject_batch(const std::vector<TouchCommand>& commands) {
    if (faulted_) throw std::runtime_error("touch backend faulted; release and verify before reuse");
    if (commands.empty() || commands.size() > static_cast<std::size_t>(max_contacts_))
        throw std::invalid_argument("invalid touch batch size");
    std::set<int> ids;
    std::vector<std::array<int, 4>> mapped;
    for (const auto& command : commands) {
        if (command.contact_id < 0 || command.contact_id >= max_contacts_ ||
            !ids.insert(command.contact_id).second)
            throw std::invalid_argument("duplicate or out-of-range batch contact");
        const auto [x, y] = mapping_.map(command.x, command.y);
        if (x < 0 || y < 0 || x >= width_ || y >= height_)
            throw std::invalid_argument("batch touch outside display");
        const bool exists = positions_.contains(command.contact_id);
        if ((command.phase == Phase::down && exists) ||
            (command.phase != Phase::down && !exists))
            throw std::invalid_argument("invalid batch contact phase");
        mapped.push_back({command.contact_id, x, y, command.phase == Phase::up ? 0 : 1});
    }
    const auto start = clock_.now_ns();
    const bool success = send_batch(mapped);
    const auto end = clock_.now_ns();
    std::vector<TouchReceipt> result;
    for (std::size_t i = 0; i < commands.size(); ++i) {
        const auto& command = commands[i];
        if (success) {
            if (command.phase == Phase::up) positions_.erase(command.contact_id);
            else positions_[command.contact_id] = {mapped[i][1], mapped[i][2]};
        } else unknown_.insert(command.contact_id);
        result.push_back({command, start, end, success,
                          success ? "rpc_returned_effect_unverified" : "rpc_result_unknown"});
    }
    return result;
}

TouchReceipt GrpcTouch::inject(const TouchCommand& command) {
    if (faulted_) throw std::runtime_error("touch backend faulted; release and verify before reuse");
    if (command.contact_id < 0 || command.contact_id >= max_contacts_)
        throw std::invalid_argument("contact outside configured pool");
    auto [x, y] = mapping_.map(command.x, command.y);
    if (x < 0 || y < 0 || x >= width_ || y >= height_) throw std::invalid_argument("touch outside display");
    auto found = positions_.find(command.contact_id);
    if ((command.phase == Phase::down && found != positions_.end()) ||
        (command.phase != Phase::down && found == positions_.end()))
        throw std::invalid_argument("invalid contact phase");
    const auto start = clock_.now_ns();
    const bool success = send(command.contact_id, x, y, command.phase == Phase::up ? 0 : 1);
    const auto end = clock_.now_ns();
    if (success) {
        if (command.phase == Phase::up) positions_.erase(command.contact_id);
        else positions_[command.contact_id] = {x, y};
    } else unknown_.insert(command.contact_id);
    return TouchReceipt{command, start, end, success,
                        success ? "rpc_returned_effect_unverified" : "rpc_result_unknown"};
}

ReleaseReport GrpcTouch::release_all() {
    ReleaseReport report;
    report.start_ns = clock_.now_ns();
    std::set<int> ids = unknown_;
    for (const auto& [id, _] : positions_) ids.insert(id);
    for (const int id : ids) {
        report.requested_ids.push_back(id);
        auto position = positions_.count(id) ? positions_.at(id) : std::array<int, 2>{width_/2, height_/2};
        if (send(id, position[0], position[1], 0)) {
            positions_.erase(id); unknown_.erase(id);
        } else { unknown_.insert(id); report.failed_ids.push_back(id); }
    }
    report.unknown_ids.assign(unknown_.begin(), unknown_.end());
    report.return_ns = clock_.now_ns();
    return report;
}

ReleaseReport GrpcTouch::emergency_release_all() {
    ReleaseReport report;
    report.start_ns = clock_.now_ns();
    for (int id = 0; id < max_contacts_; ++id) {
        report.requested_ids.push_back(id);
        const auto position = positions_.contains(id) ? positions_.at(id)
            : std::array<int, 2>{width_/2, height_/2};
        if (!send(id, position[0], position[1], 0)) {
            report.failed_ids.push_back(id);
            report.unknown_ids.push_back(id);
        } else { positions_.erase(id); unknown_.erase(id); }
    }
    report.return_ns = clock_.now_ns();
    return report;
}

void GrpcTouch::simulate_transport_loss_for_fixture_test() {
    channel_ = grpc::CreateChannel("127.0.0.1:1", grpc::InsecureChannelCredentials());
    stub_ = pb::EmulatorController::NewStub(channel_);
}

} // namespace pas
