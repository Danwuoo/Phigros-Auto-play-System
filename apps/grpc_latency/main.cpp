// Diagnostic only: no Session, input RPC, image files, or gameplay decisions.
#include "pas/emulator.hpp"
#include "pas/analysis.hpp"
#include <CLI/CLI.hpp>
#include <grpc/impl/channel_arg_names.h>
#include <grpc/grpc.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

namespace {
using namespace pas;
using json = nlohmann::json;
namespace pb = android::emulation::control;
constexpr int width = 1280, height = 720, payload_bytes = width * height * 3;
constexpr std::size_t max_samples = 4096;
struct Case { std::string name, variant; int hz = 48; int read_delay_ms = 0;
    int read_chunk_kib = default_grpc_read_chunk_kib; };
struct Sample {
    Nanoseconds read_begin, read_end, previous_end;
    std::uint64_t seq, source_us;
    int counter;
};
struct Sent { std::uint64_t seq; Nanoseconds scheduled, ready, write_end; bool ok; };

void write_json(const std::filesystem::path& path, const json& value) {
    std::ofstream file(path, std::ios::binary);
    file << value.dump(2) << '\n';
    if (!file) throw std::runtime_error("cannot write diagnostic metadata");
}
std::uint64_t cpu_100ns() {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        throw std::runtime_error("GetProcessTimes failed");
    const auto value = [](FILETIME t) {
        return (static_cast<std::uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return value(kernel) + value(user);
}
// Decode only diagnostic Fixture pixels directly from the received payload.
// Cross-check all four exact colour chips against the independent binary counter.
int counter(std::string_view pixels) {
    const auto rgb = [&](int x, int y) {
        const auto i = (static_cast<std::size_t>(y) * width + x) * 3;
        return (static_cast<unsigned char>(pixels[i]) << 16) |
            (static_cast<unsigned char>(pixels[i + 1]) << 8) |
            static_cast<unsigned char>(pixels[i + 2]);
    };
    const int identity = rgb(77, 65);
    for (int c = 0; c < 4; ++c) {
        const int x = c % 2 ? width - 90 : 65, y = c / 2 ? height - 90 : 55;
        if (rgb(x + 12, y + 10) != identity || rgb(x + 37, y + 10) != (identity ^ 0xA5C37E))
            return -1;
    }
    int binary = 0;
    for (int row = 0; row < 2; ++row) {
        const int y = 96 + row * 26;
        const auto red = [](int p) { return ((p >> 16) & 255) > 180 &&
            ((p >> 8) & 255) < 90 && (p & 255) < 90; };
        if (!red(rgb(16, y)) || !red(rgb(206, y))) return -1;
        for (int b = 0; b < 12; ++b) {
            const int p = rgb(36 + b * 14, y);
            const int r = p >> 16, g = (p >> 8) & 255, blue = p & 255;
            if (r > 180 && g > 180 && blue > 180) binary |= 1 << (row * 12 + b);
            else if (!(r < 90 && g < 90 && blue < 90)) return -1;
        }
    }
    return binary == identity ? identity : -1;
}

class Producer final : public pb::EmulatorController::Service {
public:
    Producer(const Clock& clock, int hz, int seconds) : clock_(clock), hz_(hz), seconds_(seconds) {
        sent.reserve(max_samples);
    }
    grpc::Status streamScreenshot(grpc::ServerContext* context, const pb::ImageFormat* request,
                                  grpc::ServerWriter<pb::Image>* writer) override {
        pb::Image image;
        image.mutable_format()->CopyFrom(*request);
        image.mutable_format()->mutable_rotation()->set_rotation(pb::Rotation::LANDSCAPE);
        image.set_image(std::string(payload_bytes, '\x5a'));
        const auto origin = clock_.now_ns();
        const auto steady_origin = std::chrono::steady_clock::now();
        for (std::uint64_t seq = 1; seq <= static_cast<std::uint64_t>(hz_ * seconds_) &&
             !context->IsCancelled() && sent.size() < max_samples; ++seq) {
            const auto offset = static_cast<Nanoseconds>((seq - 1) * 1'000'000'000 / hz_);
            std::this_thread::sleep_until(steady_origin + std::chrono::nanoseconds(offset));
            image.set_seq(static_cast<std::uint32_t>(seq));
            (*image.mutable_image())[0] = static_cast<char>(seq & 255);
            (*image.mutable_image())[payload_bytes - 1] = static_cast<char>((seq >> 8) & 255);
            const auto ready = clock_.now_ns();
            image.set_timestampus(static_cast<std::uint64_t>(ready / 1000));
            const bool ok = writer->Write(image);
            sent.push_back({seq, origin + offset, ready, clock_.now_ns(), ok});
            if (!ok) break;
        }
        return grpc::Status::OK;
    }
    std::vector<Sent> sent; // Metadata only; read after Server::Wait.
private:
    const Clock& clock_;
    int hz_, seconds_;
};
struct ServerOwner {
    std::unique_ptr<grpc::Server> server;
    void stop() {
        if (server) { server->Shutdown(); server->Wait(); server.reset(); }
    }
    ~ServerOwner() { stop(); }
};

json run(const Case& item, const std::string& source, const GrpcEndpoint& emulator,
         int warmup_s, int duration_s, const std::filesystem::path& root) {
    const auto folder = root / item.name;
    if (!std::filesystem::create_directory(folder)) throw std::runtime_error("run directory exists");
    HostClock clock;
    Producer producer(clock, item.hz, warmup_s + duration_s + 2);
    ServerOwner owner;
    auto endpoint = emulator;
    if (source == "loopback") {
        grpc::ServerBuilder builder;
        int port = 0;
        builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
        builder.RegisterService(&producer);
        owner.server = builder.BuildAndStart();
        if (!owner.server || !port) throw std::runtime_error("loopback server failed");
        endpoint = {"127.0.0.1:" + std::to_string(port), "", "same-process-loopback"};
    }
    auto args = capture_channel_arguments(item.read_chunk_kib);
    json channel_args = {{"grpc.max_receive_message_length", 80 * 1024 * 1024}};
    channel_args["pas.grpc.windows_read_chunk_bytes"] = item.read_chunk_kib * 1024;
    if (item.variant == "lookahead4m") {
        args.SetInt(GRPC_ARG_HTTP2_STREAM_LOOKAHEAD_BYTES, 4 * 1024 * 1024);
        channel_args[GRPC_ARG_HTTP2_STREAM_LOOKAHEAD_BYTES] = 4 * 1024 * 1024;
    } else if (item.variant == "bdp-off") {
        args.SetInt(GRPC_ARG_HTTP2_BDP_PROBE, 0);
        channel_args[GRPC_ARG_HTTP2_BDP_PROBE] = 0;
    } else if (item.variant != "default") throw std::runtime_error("unknown variant");
    auto channel = grpc::CreateCustomChannel(endpoint.target, grpc::InsecureChannelCredentials(), args);
    if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(3)))
        throw std::runtime_error("channel did not connect");
    auto stub = pb::EmulatorController::NewStub(channel);
    grpc::ClientContext context;
    if (!endpoint.token.empty()) context.AddMetadata("authorization", "Bearer " + endpoint.token);
    // Deadlines are API wall-clock values, never used in latency arithmetic.
    context.set_deadline(std::chrono::system_clock::now() +
        std::chrono::seconds(warmup_s + duration_s + 5));
    pb::ImageFormat request;
    request.set_width(width); request.set_height(height); request.set_format(pb::ImageFormat::RGB888);
    auto transport_manifest = grpc_transport_manifest(item.read_chunk_kib);
    if (item.variant == "lookahead4m") transport_manifest["http2_lookahead"] = 4 * 1024 * 1024;
    if (item.variant == "bdp-off") transport_manifest["bdp_probe"] = false;
    json manifest = {{"source", source}, {"variant", item.variant}, {"target_hz", item.hz},
        {"warmup_s", warmup_s}, {"duration_s", duration_s}, {"read_delay_ms", item.read_delay_ms},
        {"payload_bytes", payload_bytes}, {"channel_args", channel_args},
        {"grpc_client_version", grpc_version_string()}, {"qpc_frequency", clock.frequency()},
        {"source_timestamp_domain", source == "loopback" ? "host_qpc_us" : "uncalibrated_emulator_us"},
        {"clock_domain", "host_qpc_ns"}, {"diagnostic_image_retention", "none"},
        {"max_metadata_samples", max_samples}, {"timer_resolution_ms", 1},
        {"server_read_chunk_bytes", source == "loopback" ? json(8192) : json(nullptr)},
        {"cpu_scope", source == "loopback" ? "client_and_test_server_process" : "client_process"},
        {"grpc_transport", transport_manifest}};
    write_json(folder / "manifest.json", manifest);
    std::vector<Sample> samples;
    samples.reserve(max_samples);
    auto reader = stub->streamScreenshot(&context, request);
    pb::Image image;
    Nanoseconds first = 0, formal_start = 0, formal_end = 0, previous_end = 0;
    std::uint64_t cpu_begin = 0, cpu_end = 0;
    Nanoseconds cpu_begin_ns = 0, cpu_end_ns = 0;
    bool measured = false, requested_cancel = false;
    std::string failure;
    while (samples.size() < max_samples) {
        const auto begin = clock.now_ns();
        if (!reader->Read(&image)) break;
        const auto end = clock.now_ns();
        if (!first) { first = end; formal_start = first + warmup_s * 1'000'000'000LL;
            formal_end = formal_start + duration_s * 1'000'000'000LL; }
        if (!measured && end >= formal_start) {
            cpu_begin_ns = clock.now_ns(); cpu_begin = cpu_100ns(); measured = true;
        }
        if (image.format().width() != width || image.format().height() != height ||
            image.format().format() != pb::ImageFormat::RGB888 ||
            image.format().rotation().rotation() != 1 || image.image().size() != payload_bytes) {
            failure = "invalid payload geometry/format/rotation"; break;
        }
        const int visible = source == "emulator" ? counter(image.image()) : -1;
        if (source == "emulator" && visible < 0) { failure = "exact Fixture pixels missing"; break; }
        if (source == "loopback" &&
            (static_cast<unsigned char>(image.image().front()) != (image.seq() & 255) ||
             static_cast<unsigned char>(image.image().back()) != ((image.seq() >> 8) & 255))) {
            failure = "loopback payload sentinels mismatch"; break;
        }
        if (!samples.empty() && (image.seq() <= samples.back().seq ||
            image.timestampus() < samples.back().source_us)) { failure = "source regressed"; break; }
        samples.push_back({begin, end, previous_end, image.seq(), image.timestampus(), visible});
        previous_end = end;
        if (end >= formal_end) { requested_cancel = true; break; }
        if (item.read_delay_ms) std::this_thread::sleep_for(std::chrono::milliseconds(item.read_delay_ms));
    }
    cpu_end = cpu_100ns(); cpu_end_ns = clock.now_ns();
    context.TryCancel();
    const auto status = reader->Finish();
    owner.stop();
    if (!requested_cancel && failure.empty()) failure = "stream ended before complete window";
    if (!status.ok() && status.error_code() != grpc::StatusCode::CANCELLED && failure.empty())
        failure = "unexpected gRPC status " + std::to_string(status.error_code());
    std::ofstream raw(folder / "samples.jsonl", std::ios::binary);
    std::vector<double> receive, interval, gap, latency, scheduled_age, write, relative, source_interval;
    std::uint64_t gaps = 0;
    int distinct = 0, first_counter = -1, last_counter = -1;
    const Sample* last = nullptr;
    const Sample* first_formal = nullptr;
    for (const auto& s : samples) {
        const bool formal = s.read_end >= formal_start && s.read_end < formal_end;
        json row = {{"read_begin_ns", s.read_begin}, {"read_end_ns", s.read_end},
            {"previous_read_end_ns", s.previous_end}, {"seq", s.seq}, {"source_timestamp_us", s.source_us},
            {"counter", s.counter}, {"formal", formal}};
        if (source == "loopback") {
            const auto it = std::find_if(producer.sent.begin(), producer.sent.end(),
                [&](const Sent& sent) { return sent.seq == s.seq; });
            if (it == producer.sent.end()) throw std::runtime_error("missing server trace");
            row["server_scheduled_ns"] = it->scheduled; row["server_ready_ns"] = it->ready;
            row["server_write_end_ns"] = it->write_end; row["server_write_ok"] = it->ok;
            if (formal) {
                latency.push_back((s.read_end - it->ready) / 1e6);
                scheduled_age.push_back((s.read_end - it->scheduled) / 1e6);
                write.push_back((it->write_end - it->ready) / 1e6);
            }
        }
        raw << row.dump() << '\n';
        if (!formal) continue;
        if (!first_formal) first_formal = &s;
        receive.push_back((s.read_end - s.read_begin) / 1e6);
        gap.push_back((s.read_begin - s.previous_end) / 1e6);
        relative.push_back((s.read_end - first_formal->read_end) / 1e6 -
            static_cast<double>(s.source_us - first_formal->source_us) / 1000.0);
        if (last) {
            interval.push_back((s.read_end - last->read_end) / 1e6);
            source_interval.push_back(static_cast<double>(s.source_us - last->source_us) / 1000.0);
            gaps += s.seq - last->seq - 1;
        }
        if (s.counter >= 0) {
            if (first_counter < 0) first_counter = s.counter;
            if (last_counter != s.counter) ++distinct;
            if (last_counter > s.counter) failure = "Fixture counter regressed";
            last_counter = s.counter;
        }
        last = &s;
    }
    raw.close();
    if (!raw) throw std::runtime_error("raw metadata write failed");
    const double span = last && first_formal ? (last->read_end - first_formal->read_end) / 1e9 : 0;
    json result = {{"name", item.name}, {"valid", failure.empty() && receive.size() >= 2},
        {"failure", failure}, {"samples", receive.size()}, {"formal_start_ns", formal_start},
        {"formal_end_ns", formal_end}, {"receive_wait_ms", distribution(receive)},
        {"arrival_interval_ms", distribution(interval)}, {"between_reads_ms", distribution(gap)},
        {"source_interval_ms", distribution(source_interval)},
        {"ready_to_read_ms", distribution(latency)}, {"scheduled_to_read_ms", distribution(scheduled_age)},
        {"server_write_ms", distribution(write)}, {"relative_delay_change_ms", distribution(relative)},
        {"absolute_emulator_source_age_ms", nullptr}, {"source_seq_gaps", gaps},
        {"delivered_hz", span > 0 ? (receive.size() - 1) / span : 0},
        {"visible_source_hz", span > 0 && first_counter >= 0 ? json((last_counter - first_counter) / span) : json(nullptr)},
        {"visible_follow_ratio", last_counter > first_counter && first_counter >= 0 ?
             json(static_cast<double>(distinct - 1) / (last_counter - first_counter)) : json(nullptr)},
        {"process_cpu_core_percent", measured && cpu_end_ns > cpu_begin_ns ?
            json((cpu_end - cpu_begin) * 10'000.0 / (cpu_end_ns - cpu_begin_ns)) : json(nullptr)},
        {"cpu_begin_ns", cpu_begin_ns}, {"cpu_end_ns", cpu_end_ns},
        {"cpu_begin_100ns", measured ? json(cpu_begin) : json(nullptr)}, {"cpu_end_100ns", cpu_end},
        {"raw_sha256", sha256_file(folder / "samples.jsonl")}};
    write_json(folder / "summary.json", result);
    std::cout << item.name << " valid=" << result["valid"] << " samples=" << receive.size()
              << " ready_p99=" << result["ready_to_read_ms"].value("p99", -1.0)
              << " arrival_p99=" << result["arrival_interval_ms"].value("p99", -1.0) << std::endl;
    return result;
}
}

int main(int argc, char** argv) {
    CLI::App app{"Bounded gRPC transport research; never saves image payloads"};
    std::string source = "loopback", serial = "emulator-5554", output, variant = "default";
    int hz = 48, warmup = 1, duration = 6, delay = 0;
    int read_chunk_kib = default_grpc_read_chunk_kib;
    bool matrix = false, read_matrix = false;
    app.add_option("--source", source)->check(CLI::IsMember({"loopback", "emulator"}));
    app.add_option("--serial", serial);
    app.add_option("--output-dir", output)->required();
    app.add_option("--variant", variant)->check(CLI::IsMember({"default", "lookahead4m", "bdp-off"}));
    app.add_option("--hz", hz)->check(CLI::Range(40, 59));
    app.add_option("--warmup-s", warmup)->check(CLI::Range(1, 5));
    app.add_option("--duration-s", duration)->check(CLI::Range(1, 15));
    app.add_option("--read-delay-ms", delay)->check(CLI::Range(0, 50));
    app.add_option("--read-chunk-kib", read_chunk_kib)->check(CLI::IsMember({8, 64, 256}));
    app.add_flag("--matrix", matrix, "Predeclared balanced nine-run matrix; loopback adds four controls");
    app.add_flag("--read-size-matrix", read_matrix, "Nine runs comparing 8/64/256 KiB on the same patched dependency")->excludes("--matrix");
    CLI11_PARSE(app, argc, argv);
    try {
        const std::filesystem::path root(output);
        if (std::filesystem::exists(root)) throw std::runtime_error("output directory already exists");
        std::filesystem::create_directories(root);
        struct Timer { Timer() { if (timeBeginPeriod(1) != TIMERR_NOERROR)
            throw std::runtime_error("timer resolution unavailable"); } ~Timer() { timeEndPeriod(1); } } timer;
        const auto endpoint = source == "emulator" ? discover_endpoint(serial) : GrpcEndpoint{};
        std::vector<Case> cases;
        if (read_matrix) {
            const std::array<int, 3> sizes = {8, 64, 256};
            for (int round = 0; round < 3; ++round) for (int slot = 0; slot < 3; ++slot) {
                const int size = sizes[(round + slot) % 3];
                cases.push_back({"r" + std::to_string(round + 1) + "-read" + std::to_string(size),
                                 "default", hz, 0, size});
            }
        } else if (matrix) {
            const std::array<std::string, 3> variants = {"default", "lookahead4m", "bdp-off"};
            for (int round = 0; round < 3; ++round)
                for (int slot = 0; slot < 3; ++slot) {
                    const auto v = variants[(slot + round) % 3];
                    cases.push_back({"r" + std::to_string(round + 1) + "-" + v, v, hz, 0});
                }
            if (source == "loopback") {
                cases.push_back({"control-40hz", "default", 40, 0});
                cases.push_back({"control-59hz", "default", 59, 0});
                cases.push_back({"control-read-delay5", "default", hz, 5});
                cases.push_back({"control-read-delay25", "default", hz, 25});
            }
        } else cases.push_back({"single", variant, hz, delay});
        if (!read_matrix) for (auto& c : cases) c.read_chunk_kib = read_chunk_kib;
        json planned = json::array();
        for (const auto& c : cases) planned.push_back({{"name", c.name}, {"variant", c.variant},
            {"target_hz", c.hz}, {"read_delay_ms", c.read_delay_ms}, {"read_chunk_kib", c.read_chunk_kib}});
        write_json(root / "plan.json", {{"source", source}, {"cases", planned},
            {"warmup_s", warmup}, {"duration_s", duration}, {"order_fixed_before_measurement", true},
            {"binary_sha256", sha256_file(std::filesystem::absolute(argv[0]))},
            {"intent", "short exploratory research; no production performance acceptance"}});
        json results = json::array();
        bool all_valid = true;
        for (const auto& c : cases) {
            try {
                auto result = run(c, source, endpoint, warmup, duration, root);
                all_valid = all_valid && result.at("valid").get<bool>();
                results.push_back(std::move(result));
            } catch (const std::exception& e) {
                all_valid = false;
                results.push_back({{"name", c.name}, {"valid", false}, {"failure", e.what()}});
            }
            write_json(root / "results.json", {{"all_valid", all_valid}, {"runs", results}});
        }
        return all_valid ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
