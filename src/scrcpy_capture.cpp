#include "pas/scrcpy_capture.hpp"

#include "pas/adb.hpp"
#include "pas/analysis.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <memory>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace pas {
namespace {
constexpr const char* server_hash = "deacb991ed2509715160ffdc7907e47b4160eb30d1566217e9047fd5b8850cae";
constexpr const char* server_version = "4.1";

std::wstring wide(const std::string& input) {
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                                            static_cast<int>(input.size()), nullptr, 0);
    if (!length) throw std::invalid_argument("invalid UTF-8 process argument");
    std::wstring out(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                        static_cast<int>(input.size()), out.data(), length);
    return out;
}

std::wstring quote(const std::wstring& arg) {
    std::wstring out = L"\"";
    unsigned slashes = 0;
    for (const auto ch : arg) {
        if (ch == L'\\') { ++slashes; continue; }
        if (ch == L'"') { out.append(slashes * 2 + 1, L'\\'); out += ch; slashes = 0; continue; }
        out.append(slashes, L'\\'); slashes = 0; out += ch;
    }
    out.append(slashes * 2, L'\\');
    return out + L'"';
}

struct Winsock {
    Winsock() {
        WSADATA state{};
        if (WSAStartup(MAKEWORD(2, 2), &state)) throw std::runtime_error("WSAStartup failed");
    }
    ~Winsock() { WSACleanup(); }
};

struct ServerRun {
    std::filesystem::path adb;
    std::string serial;
    std::string remote;
    std::string port;
    HANDLE process = nullptr;
    HANDLE thread = nullptr;
    SOCKET socket = INVALID_SOCKET;
    ~ServerRun() {
        if (socket != INVALID_SOCKET) closesocket(socket);
        if (process) {
            TerminateProcess(process, 0);
            WaitForSingleObject(process, 2000);
            CloseHandle(process);
        }
        if (thread) CloseHandle(thread);
        if (!port.empty()) {
            try { adb_call(adb, {"-s", serial, "forward", "--remove", "tcp:" + port}, 5000); }
            catch (...) {}
        }
        if (!remote.empty()) {
            try { adb_call(adb, {"-s", serial, "shell", "rm", remote}, 5000); }
            catch (...) {}
        }
    }
};

std::uint32_t big32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

std::uint64_t big64(const std::uint8_t* p) {
    return (static_cast<std::uint64_t>(big32(p)) << 32) | big32(p + 4);
}

bool read_exact(SOCKET socket, std::uint8_t* bytes, std::size_t count,
                std::stop_token stop, const std::atomic<bool>& cancelled) {
    std::size_t done = 0;
    while (done < count && !stop.stop_requested() && !cancelled.load()) {
        const auto size = static_cast<int>(std::min<std::size_t>(count - done, 64 * 1024));
        const int received = recv(socket, reinterpret_cast<char*>(bytes + done), size, 0);
        if (received > 0) { done += static_cast<std::size_t>(received); continue; }
        if (received == 0) {
            if (done) throw std::runtime_error("scrcpy socket closed in the middle of a packet");
            return false;
        }
        if (WSAGetLastError() == WSAETIMEDOUT) continue;
        if (stop.stop_requested() || cancelled.load()) return false;
        throw std::runtime_error("scrcpy video socket receive failed");
    }
    return done == count;
}

class Decoder {
public:
    Decoder() {
        const auto* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) throw std::runtime_error("FFmpeg H.264 decoder unavailable");
        context_ = avcodec_alloc_context3(codec);
        packet_ = av_packet_alloc();
        decoded_ = av_frame_alloc();
        if (!context_ || !packet_ || !decoded_) throw std::runtime_error("FFmpeg allocation failed");
        context_->thread_count = 1;
        context_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        context_->pkt_timebase = AVRational{1, 1'000'000};
        if (avcodec_open2(context_, codec, nullptr) < 0)
            throw std::runtime_error("FFmpeg H.264 decoder open failed");
    }
    ~Decoder() {
        sws_freeContext(scaler_);
        av_frame_free(&decoded_);
        av_packet_free(&packet_);
        avcodec_free_context(&context_);
    }
    AVPacket* packet(std::uint32_t size) {
        av_packet_unref(packet_);
        if (av_new_packet(packet_, static_cast<int>(size)) < 0)
            throw std::runtime_error("FFmpeg packet allocation failed");
        return packet_;
    }
    void reset() { avcodec_flush_buffers(context_); }
    void submit(const Clock& clock, const ScrcpyCaptureOptions& options,
                Nanoseconds receive_start, Nanoseconds parse_start, Nanoseconds parse_end,
                Nanoseconds complete, std::optional<std::int64_t> pts,
                std::uint64_t& sequence, std::uint64_t generation,
                const std::function<void(Frame&&)>& on_frame) {
        packet_->pts = pts.value_or(AV_NOPTS_VALUE);
        packet_->dts = packet_->pts;
        const auto decode_start = clock.now_ns();
        if (avcodec_send_packet(context_, packet_) < 0)
            throw std::runtime_error("FFmpeg H.264 packet rejected; stream requires resynchronization");
        while (true) {
            const auto result = avcodec_receive_frame(context_, decoded_);
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) break;
            if (result < 0) throw std::runtime_error("FFmpeg H.264 decode failed");
            const auto decode_end = clock.now_ns();
            if (decoded_->width != options.width || decoded_->height != options.height ||
                static_cast<std::uint64_t>(decoded_->width) * decoded_->height * 3 > 16 * 1024 * 1024)
                throw std::runtime_error("scrcpy decoded geometry changed or exceeded RGB limit");
            scaler_ = sws_getCachedContext(scaler_, decoded_->width, decoded_->height,
                static_cast<AVPixelFormat>(decoded_->format), options.width, options.height,
                AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!scaler_) throw std::runtime_error("FFmpeg RGB conversion unavailable");
            Frame frame;
            frame.sequence = ++sequence;
            frame.generation = generation;
            frame.geometry_version = generation;
            frame.width = options.width;
            frame.height = options.height;
            frame.stride = options.width * 3;
            frame.source_rotation = 0;
            frame.capture_backend = "scrcpy-h264";
            frame.source_pixel_format = "H264 decoded by FFmpeg software";
            frame.crop_width = options.width;
            frame.crop_height = options.height;
            frame.capture_complete_ns = complete;
            frame.receive_start_ns = receive_start;
            frame.parse_start_ns = parse_start;
            frame.parse_end_ns = parse_end;
            frame.decode_start_ns = decode_start;
            frame.decode_end_ns = decode_end;
            frame.codec_pts = decoded_->pts == AV_NOPTS_VALUE ? pts :
                std::optional<std::int64_t>(decoded_->pts);
            frame.copy_start_ns = clock.now_ns();
            frame.rgb.resize(static_cast<std::size_t>(frame.stride) * frame.height);
            std::uint8_t* target[] = {frame.rgb.data(), nullptr, nullptr, nullptr};
            int stride[] = {frame.stride, 0, 0, 0};
            if (sws_scale(scaler_, decoded_->data, decoded_->linesize, 0, decoded_->height,
                          target, stride) != decoded_->height)
                throw std::runtime_error("FFmpeg RGB conversion incomplete");
            frame.copy_end_ns = clock.now_ns();
            frame.pixels_ready_ns = *frame.copy_end_ns;
            on_frame(std::move(frame));
            av_frame_unref(decoded_);
        }
    }
private:
    AVCodecContext* context_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* decoded_ = nullptr;
    SwsContext* scaler_ = nullptr;
};
} // namespace

ScrcpyWireHeader parse_scrcpy_v41_header(const std::array<std::uint8_t, 12>& bytes) {
    ScrcpyWireHeader result;
    if (bytes[0] & 0x80) {
        if (bytes[0] != 0x80 || bytes[1] || bytes[2] || (bytes[3] & ~1))
            throw std::runtime_error("invalid scrcpy v4.1 session header");
        result.session = true;
        result.width = big32(bytes.data() + 4);
        result.height = big32(bytes.data() + 8);
        if (!result.width || !result.height)
            throw std::runtime_error("empty scrcpy v4.1 session geometry");
    } else {
        const auto raw_pts = big64(bytes.data());
        result.config = (raw_pts & (1ULL << 62)) != 0;
        result.key = (raw_pts & (1ULL << 61)) != 0;
        result.pts_us = static_cast<std::int64_t>(raw_pts & ((1ULL << 61) - 1));
        result.packet_size = big32(bytes.data() + 8);
        if (!result.packet_size || result.packet_size > 4 * 1024 * 1024)
            throw std::runtime_error("scrcpy encoded packet exceeds 4 MiB bound");
    }
    return result;
}

std::string ffmpeg_runtime_version() { return av_version_info(); }
std::uint32_t ffmpeg_avcodec_version() { return avcodec_version(); }
std::uint32_t ffmpeg_swscale_version() { return swscale_version(); }

ScrcpyCapture::ScrcpyCapture(const Clock& clock, ScrcpyCaptureOptions options)
    : clock_(clock), options_(std::move(options)) {
    if (options_.serial.empty() || options_.server_file.empty() || options_.width < 1 ||
        options_.height < 1 || options_.width > 4096 || options_.height > 4096 ||
        static_cast<std::uint64_t>(options_.width) * options_.height * 3 > 16 * 1024 * 1024 ||
        options_.max_fps < 1 || options_.max_fps > 120 ||
        options_.video_bit_rate < 100'000 || options_.video_bit_rate > 100'000'000 ||
        options_.video_encoder.empty())
        throw std::invalid_argument("invalid scrcpy capture options");
    if (sha256_file(options_.server_file) != server_hash)
        throw std::runtime_error("scrcpy server artifact does not match pinned v4.1 SHA-256");
}

ScrcpyCapture::~ScrcpyCapture() { cancel(); }

void ScrcpyCapture::stream_once(std::stop_token stop, const std::function<void(Frame&&)>& on_frame,
                                const std::function<void(std::string_view, Nanoseconds)>& on_signal) {
    Winsock winsock;
    ServerRun run;
    struct ActiveReset {
        std::atomic<std::uintptr_t>& slot;
        ~ActiveReset() { slot = ~std::uintptr_t{0}; }
    } active_reset{active_socket_};
    run.adb = find_adb();
    run.serial = options_.serial;
    const auto scid = static_cast<std::uint32_t>(clock_.now_ns() & 0x7fffffff);
    std::array<char, 9> scid_text{};
    std::snprintf(scid_text.data(), scid_text.size(), "%08x", scid);
    run.remote = std::string("/data/local/tmp/pas-scrcpy-") + scid_text.data() + ".jar";
    adb_call(run.adb, {"-s", run.serial, "push", options_.server_file.string(), run.remote}, 30'000);
    const auto forward = adb_call(run.adb, {"-s", run.serial, "forward", "tcp:0",
                                            std::string("localabstract:scrcpy_") + scid_text.data()}, 5000);
    run.port = std::string(forward.begin(), forward.end());
    run.port.erase(std::remove_if(run.port.begin(), run.port.end(),
        [](unsigned char c) { return std::isspace(c) != 0; }), run.port.end());
    if (run.port.empty() || !std::all_of(run.port.begin(), run.port.end(),
        [](unsigned char c) { return std::isdigit(c) != 0; }))
        throw std::runtime_error("ADB did not return a scrcpy forwarding port");
    const std::vector<std::string> args = {"-s", run.serial, "shell", "CLASSPATH=" + run.remote,
        "app_process", "/", "com.genymobile.scrcpy.Server", server_version,
        "scid=" + std::string(scid_text.data()), "log_level=warn", "video=true",
        "audio=false", "control=false", "video_codec=h264", "tunnel_forward=true",
        "video_encoder=" + options_.video_encoder, "downsize_on_error=false",
        "send_device_meta=false", "send_dummy_byte=false", "max_size=" +
        std::to_string(std::max(options_.width, options_.height)),
        "max_fps=" + std::to_string(options_.max_fps),
        "video_bit_rate=" + std::to_string(options_.video_bit_rate)};
    std::wstring command = quote(run.adb.wstring());
    for (const auto& arg : args) command += L" " + quote(wide(arg));
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nul == INVALID_HANDLE_VALUE) throw std::runtime_error("scrcpy null sink unavailable");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = startup.hStdOutput = startup.hStdError = nul;
    PROCESS_INFORMATION process{};
    const auto launched = CreateProcessW(run.adb.c_str(), mutable_command.data(), nullptr,
        nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(nul);
    if (!launched) throw std::runtime_error("scrcpy server ADB process failed to launch");
    run.process = process.hProcess;
    run.thread = process.hThread;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!stop.stop_requested() && !cancelled_ && std::chrono::steady_clock::now() < deadline) {
        if (WaitForSingleObject(run.process, 0) == WAIT_OBJECT_0)
            throw std::runtime_error("scrcpy server exited before video connection");
        SOCKET candidate = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (candidate == INVALID_SOCKET) throw std::runtime_error("scrcpy socket creation failed");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(static_cast<u_short>(std::stoi(run.port)));
        if (connect(candidate, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
            const DWORD handshake_timeout = 500;
            setsockopt(candidate, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&handshake_timeout), sizeof(handshake_timeout));
            char first = 0;
            if (recv(candidate, &first, 1, MSG_PEEK) == 1) {
                run.socket = candidate;
                break;
            }
        }
        closesocket(candidate);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (stop.stop_requested() || cancelled_) return;
    if (run.socket == INVALID_SOCKET) throw std::runtime_error("scrcpy video socket did not become ready");
    active_socket_ = static_cast<std::uintptr_t>(run.socket);
    const DWORD timeout = 250;
    setsockopt(run.socket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    const int receive_buffer = 512 * 1024;
    setsockopt(run.socket, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<const char*>(&receive_buffer), sizeof(receive_buffer));
    std::array<std::uint8_t, 12> header{};
    if (!read_exact(run.socket, header.data(), 4, stop, cancelled_)) return;
    if (big32(header.data()) != 0x68323634)
        throw std::runtime_error("scrcpy stream is not pinned H.264 protocol");
    Decoder decoder;
    bool saw_session = false;
    std::optional<std::pair<Nanoseconds, std::int64_t>> lag_anchor;
    std::vector<std::uint8_t> pending_config;
    while (!stop.stop_requested() && !cancelled_) {
        const auto receive_start = clock_.now_ns();
        if (!read_exact(run.socket, header.data(), 12, stop, cancelled_)) break;
        const auto parse_start = clock_.now_ns();
        const auto wire = parse_scrcpy_v41_header(header);
        const auto parse_end = clock_.now_ns();
        if (wire.session) {
            if (wire.width != static_cast<std::uint32_t>(options_.width) ||
                wire.height != static_cast<std::uint32_t>(options_.height))
                throw std::runtime_error("scrcpy session geometry differs from frozen output profile");
            ++generation_;
            saw_session = true;
            decoder.reset();
            lag_anchor.reset();
            pending_config.clear();
            continue;
        }
        if (!saw_session) throw std::runtime_error("scrcpy media packet arrived before session metadata");
        const auto prefix = wire.config ? 0u : pending_config.size();
        if (static_cast<std::uint64_t>(prefix) + wire.packet_size > 4 * 1024 * 1024)
            throw std::runtime_error("scrcpy merged config and media exceed 4 MiB bound");
        auto* packet = decoder.packet(static_cast<std::uint32_t>(prefix + wire.packet_size));
        if (prefix) std::copy(pending_config.begin(), pending_config.end(), packet->data);
        if (!read_exact(run.socket, packet->data + prefix, wire.packet_size, stop, cancelled_)) break;
        const auto complete = clock_.now_ns();
        if (on_signal) on_signal(wire.config ? "codec_config_packet" : "encoded_packet", complete);
        if (wire.config) {
            pending_config.assign(packet->data, packet->data + wire.packet_size);
            continue;
        }
        pending_config.clear();
        if (!wire.config) {
            if (!lag_anchor) lag_anchor = std::make_pair(complete, wire.pts_us);
            if (wire.pts_us < lag_anchor->second ||
                wire.pts_us - lag_anchor->second >
                    std::numeric_limits<Nanoseconds>::max() / 1000 ||
                complete - lag_anchor->first - (wire.pts_us - lag_anchor->second) * 1000 > 250'000'000)
                throw std::runtime_error("scrcpy decoder fell behind source PTS; restart stream");
        }
        packet->flags = wire.key ? AV_PKT_FLAG_KEY : 0;
        decoder.submit(clock_, options_, receive_start, parse_start, parse_end, complete,
            std::optional<std::int64_t>(wire.pts_us),
            sequence_, generation_, [&](Frame&& frame) {
                if (on_signal) on_signal("decoded_frame", clock_.now_ns());
                on_frame(std::move(frame));
            });
    }
    active_socket_ = ~std::uintptr_t{0};
    if (!stop.stop_requested() && !cancelled_)
        throw std::runtime_error("scrcpy stream ended before capture stopped");
}

void ScrcpyCapture::stream(std::stop_token stop, const std::function<void(Frame&&)>& on_frame,
                           const std::function<void(std::string_view, Nanoseconds)>& on_signal) {
    for (int attempt = 0; attempt < 2 && !stop.stop_requested() && !cancelled_; ++attempt) {
        try {
            stream_once(stop, on_frame, on_signal);
            return;
        } catch (const std::exception& error) {
            if (stop.stop_requested() || cancelled_) return;
            const std::string reason = error.what();
            const bool recoverable = reason.find("fell behind source PTS") != std::string::npos ||
                reason.find("video socket receive failed") != std::string::npos ||
                reason.find("stream ended before capture stopped") != std::string::npos;
            if (!recoverable || attempt == 1) throw;
            if (on_signal) on_signal("resync_gap", clock_.now_ns());
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
}

void ScrcpyCapture::cancel() {
    cancelled_ = true;
    const auto socket = active_socket_.load();
    if (socket != ~std::uintptr_t{0}) shutdown(static_cast<SOCKET>(socket), SD_BOTH);
}

} // namespace pas
