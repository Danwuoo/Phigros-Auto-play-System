#include "pas/analysis.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <deque>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

namespace pas {
namespace {
using json = nlohmann::json;

class Samples {
public:
    void add(double value) {
        if (values_.size() == 100'000) { values_.pop_front(); truncated_ = true; }
        values_.push_back(value);
    }
    json result() const {
        auto result = distribution({values_.begin(), values_.end()});
        result["sample_truncated"] = truncated_;
        return result;
    }
private:
    std::deque<double> values_;
    bool truncated_ = false;
};

template<class Function>
void each_jsonl(const std::filesystem::path& path, Function&& callback) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open JSONL file");
    std::string line;
    std::size_t number = 0;
    while (std::getline(input, line)) {
        ++number;
        if (line.empty()) continue;
        try { callback(json::parse(line), number); }
        catch (const json::exception& error) {
            throw std::runtime_error("invalid JSONL at line " + std::to_string(number) + ": " + error.what());
        }
    }
    if (!input.eof()) throw std::runtime_error("JSONL read failed");
}

std::int64_t integer(const json& event, const char* field) {
    if (!event.contains(field) || !event.at(field).is_number_integer())
        throw std::runtime_error(std::string("missing integer ") + field);
    return event.at(field).get<std::int64_t>();
}
}

json distribution(std::vector<double> values) {
    if (values.empty()) return {{"n", 0}};
    std::sort(values.begin(), values.end());
    const auto percentile = [&](double p) {
        const double rank = (values.size() - 1) * p;
        const auto lower = static_cast<std::size_t>(rank);
        const double fraction = rank - lower;
        return values[lower] * (1 - fraction) + values[std::min(lower + 1, values.size() - 1)] * fraction;
    };
    const double p50 = percentile(0.5);
    std::vector<double> absolute;
    absolute.reserve(values.size());
    for (double value : values) absolute.push_back(std::abs(value - p50));
    std::sort(absolute.begin(), absolute.end());
    return {{"n", values.size()}, {"min", values.front()}, {"p5", percentile(0.05)},
            {"p50", p50}, {"p95", percentile(0.95)}, {"p99", percentile(0.99)},
            {"max", values.back()},
            {"mean", std::accumulate(values.begin(), values.end(), 0.0) / values.size()},
            {"jitter_p95_minus_p50", percentile(0.95) - p50},
            {"jitter_p95_minus_p5", percentile(0.95) - percentile(0.05)},
            {"absolute_deviation_p95", absolute[std::min(absolute.size() - 1,
                static_cast<std::size_t>(std::ceil(0.95 * absolute.size())) - 1)]}};
}

std::string sha256_file(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("BCrypt SHA256 unavailable");
    auto cleanup = [&] {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    };
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        cleanup(); throw std::runtime_error("BCryptCreateHash failed");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) { cleanup(); throw std::runtime_error("cannot open file for SHA256"); }
    std::vector<char> buffer(1024 * 1024);
    while (input) {
        input.read(buffer.data(), buffer.size());
        const auto count = static_cast<ULONG>(input.gcount());
        if (count && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), count, 0) < 0) {
            cleanup(); throw std::runtime_error("BCryptHashData failed");
        }
    }
    if (!input.eof()) { cleanup(); throw std::runtime_error("hash source read failed"); }
    std::array<unsigned char, 32> digest{};
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
        cleanup(); throw std::runtime_error("BCryptFinishHash failed");
    }
    cleanup();
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto byte : digest) result << std::setw(2) << static_cast<int>(byte);
    return result.str();
}

json analyze_capture_jsonl(const std::filesystem::path& path) {
    std::optional<std::int64_t> start, end;
    each_jsonl(path, [&](const json& event, std::size_t) {
        if (event.value("event", "") != "bench_phase") return;
        const auto phase = event.value("phase", "");
        if (phase == "MEASURING") start = integer(event, "monotonic_ns");
        if (phase == "STOPPING") end = integer(event, "monotonic_ns");
    });
    if (!start || !end || *end <= *start) throw std::runtime_error("missing complete measurement window");
    const double duration = (*end - *start) / 1e9;
    Samples arrivals, no_frame_gaps, normalize_cost, residency, first15, after15, rss_mib;
    std::optional<json> resource_start, resource_end;
    std::optional<std::int64_t> previous_capture, first_counter_time, last_counter_time;
    std::optional<std::int64_t> previous_counter;
    std::uint64_t capture_count = 0, consumer_count = 0, counter_count = 0,
                  distinct = 0, counter_increment = 0, consumer_skips = 0, cross_boundary = 0;
    // Fixture counters are 24-bit. A fixed 2 MiB bitmap gives exact distinct
    // counts for arbitrarily long JSONL input without retaining events.
    std::vector<std::uint8_t> seen_counters(1u << 21);
    bool counter_order_valid = true;
    std::set<std::tuple<int, int, int>> geometry;
    std::set<std::string> fixture_schemas;
    std::unordered_map<std::uint64_t, std::int64_t> capture_by_sequence;
    std::deque<std::uint64_t> capture_order;
    each_jsonl(path, [&](const json& event, std::size_t) {
        const auto kind = event.value("event", "");
        if (kind == "capture") {
            const auto time = integer(event, "capture_complete_ns");
            if (event.contains("frame_sequence")) {
                const auto sequence = event.at("frame_sequence").get<std::uint64_t>();
                capture_by_sequence[sequence] = time;
                capture_order.push_back(sequence);
                if (capture_order.size() > 100'000) {
                    capture_by_sequence.erase(capture_order.front()); capture_order.pop_front();
                }
            }
            if (time < *start || time >= *end) return;
            ++capture_count;
            no_frame_gaps.add((time - (previous_capture ? *previous_capture : *start)) / 1e6);
            if (previous_capture) {
                if (time < *previous_capture) throw std::runtime_error("capture time regressed in JSONL");
                arrivals.add((time - *previous_capture) / 1e6);
            }
            previous_capture = time;
            if (event.contains("pixels_ready_ns") && event.at("pixels_ready_ns").is_number_integer())
                normalize_cost.add((integer(event, "pixels_ready_ns") - time) / 1e6);
            if (event.contains("width") && event.contains("height") && event.contains("source_rotation"))
                geometry.emplace(event.at("width").get<int>(), event.at("height").get<int>(),
                                 event.at("source_rotation").get<int>());
        } else if (kind == "frame_consumed") {
            const auto time = integer(event, "consume_ns");
            if (time < *start || time >= *end) return;
            ++consumer_count;
            consumer_skips += event.value("sequence_skip", 0ULL);
            std::optional<std::int64_t> captured;
            if (event.contains("capture_complete_ns") && event.at("capture_complete_ns").is_number_integer())
                captured = integer(event, "capture_complete_ns");
            else if (event.contains("frame_sequence")) {
                auto found = capture_by_sequence.find(event.at("frame_sequence").get<std::uint64_t>());
                if (found != capture_by_sequence.end()) captured = found->second;
            }
            if (!captured) throw std::runtime_error("consumer frame lacks capture timestamp linkage");
            if (*captured < *start) ++cross_boundary;
            const double ms = (time - *captured) / 1e6;
            residency.add(ms);
            if (time < *start + 15'000'000'000LL) first15.add(ms);
            else after15.add(ms);
        } else if (kind == "resource_boundary") {
            if (event.value("phase", "") == "start") resource_start = event;
            else if (event.value("phase", "") == "end") resource_end = event;
        } else if (kind == "resource_sample") {
            const auto time = integer(event, "before_ns");
            if (time >= *start && time < *end)
                rss_mib.add(integer(event, "working_set_bytes") / 1048576.0);
        } else if (kind == "fixture_counter") {
            const auto time = integer(event, "capture_complete_ns");
            if (time < *start || time >= *end) return;
            fixture_schemas.insert(event.value("fixture_schema", "legacy_unspecified"));
            const auto counter = integer(event, "counter");
            if (counter < 0 || counter >= (1 << 24))
                throw std::runtime_error("Fixture counter outside 24-bit encoding");
            if (!first_counter_time) first_counter_time = time;
            last_counter_time = time;
            ++counter_count;
            const auto byte = static_cast<std::size_t>(counter) >> 3;
            const auto bit = static_cast<std::uint8_t>(1u << (counter & 7));
            if (!(seen_counters[byte] & bit)) { seen_counters[byte] |= bit; ++distinct; }
            if (previous_counter) {
                const auto increment = (counter - *previous_counter + (1 << 24)) % (1 << 24);
                if (increment >= (1 << 23)) counter_order_valid = false;
                counter_increment += increment;
            }
            previous_counter = counter;
        }
    });
    json shape = json::array();
    json schema_names = json::array();
    no_frame_gaps.add((*end - (previous_capture ? *previous_capture : *start)) / 1e6);
    for (const auto& [w, h, r] : geometry) shape.push_back({w, h, r});
    for (const auto& schema : fixture_schemas) schema_names.push_back(schema);
    json resource_window = nullptr;
    if (resource_start && resource_end) {
        const auto start_mid = (integer(*resource_start, "before_ns") +
                                integer(*resource_start, "after_ns")) / 2.0;
        const auto end_mid = (integer(*resource_end, "before_ns") +
                              integer(*resource_end, "after_ns")) / 2.0;
        const auto cpu_start = integer(*resource_start, "process_cpu_ns");
        const auto cpu_end = integer(*resource_end, "process_cpu_ns");
        if (end_mid <= start_mid || cpu_end < cpu_start)
            throw std::runtime_error("resource sampling window regressed");
        resource_window = {{"start_before_ns", integer(*resource_start, "before_ns")},
            {"start_after_ns", integer(*resource_start, "after_ns")},
            {"end_before_ns", integer(*resource_end, "before_ns")},
            {"end_after_ns", integer(*resource_end, "after_ns")},
            {"cpu_process_ns_start", cpu_start}, {"cpu_process_ns_end", cpu_end},
            {"process_cpu_core_equivalent", (cpu_end - cpu_start) / (end_mid - start_mid)},
            {"rss_start_bytes", integer(*resource_start, "working_set_bytes")},
            {"rss_end_bytes", integer(*resource_end, "working_set_bytes")},
            {"rss_sample_mib", rss_mib.result()}};
    }
    const auto counter_span = first_counter_time && last_counter_time && *last_counter_time > *first_counter_time
        ? std::optional<double>(counter_increment / ((*last_counter_time - *first_counter_time) / 1e9)) : std::nullopt;
    return {{"measurement_start_ns", *start}, {"measurement_end_ns", *end}, {"window_s", duration},
            {"capture_events", capture_count}, {"consumer_events", consumer_count},
            {"received_hz", capture_count / duration}, {"fixture_samples", counter_count},
            {"fixture_distinct", distinct}, {"fixture_distinct_hz", distinct / duration},
            {"fixture_counter_span_hz", counter_order_valid && counter_span ? json(*counter_span) : json(nullptr)},
            {"fixture_follow_ratio_estimate", counter_order_valid && counter_span && *counter_span > 0
                ? json((distinct / duration) / *counter_span) : json(nullptr)},
            {"fixture_counter_order_valid", counter_order_valid},
            {"fixture_decoded_fraction", capture_count ? double(counter_count) / capture_count : 0.0},
            {"fixture_decoded_consumer_fraction", consumer_count ? double(counter_count) / consumer_count : 0.0},
            {"fixture_schemas", schema_names},
            {"geometry", shape}, {"geometry_valid", shape == json::array({json::array({1280, 720, 1})})},
            {"arrival_interval_ms", arrivals.result()},
            {"no_frame_gap_ms", no_frame_gaps.result()},
            {"capture_to_pixels_ready_ms", normalize_cost.result()},
            {"host_residency_ms", residency.result()},
            {"consumer_sequence_skips", consumer_skips},
            {"consumer_cross_boundary_frames", cross_boundary},
            {"host_residency_first15s_ms", first15.result()},
            {"host_residency_after15s_ms", after15.result()},
            {"resource_window", resource_window},
            {"raw_sha256", sha256_file(path)}, {"source_absolute_age", "unknown"}};
}

json analyze_pause_jsonl(const std::filesystem::path& path) {
    std::optional<json> pause, anchor;
    each_jsonl(path, [&](const json& event, std::size_t) {
        const auto kind = event.value("event", "");
        if (kind == "receiver_pause") pause = event;
        else if (kind == "capture" && event.contains("source_timestamp_us") &&
                 event.at("source_timestamp_us").is_number_integer() &&
                 (!pause || integer(event, "capture_complete_ns") <
                      (pause->contains("start_ns") ? integer(*pause, "start_ns") : integer(*pause, "started_ns"))))
            anchor = event;
    });
    if (!pause || !anchor) throw std::runtime_error("pause or pre-pause timestamp anchor missing");
    const auto start = pause->contains("start_ns") ? integer(*pause, "start_ns") : integer(*pause, "started_ns");
    const bool measured = pause->contains("ended_ns") && pause->at("ended_ns").is_number_integer();
    const auto end = measured ? integer(*pause, "ended_ns") :
        start + static_cast<std::int64_t>(std::llround(pause->value("duration_ms", 500.0) * 1e6));
    json points = json::array();
    bool points_truncated = false;
    each_jsonl(path, [&](const json& event, std::size_t) {
        if (event.value("event", "") != "capture" || !event.contains("source_timestamp_us") ||
            !event.at("source_timestamp_us").is_number_integer()) return;
        const auto time = integer(event, "capture_complete_ns");
        if (time < end || time >= end + 1'000'000'000LL) return;
        const bool same = event.value("stream_generation", 0) == anchor->value("stream_generation", 0);
        const auto relative = same ? json((time - integer(*anchor, "capture_complete_ns")) / 1e6 -
            (integer(event, "source_timestamp_us") - integer(*anchor, "source_timestamp_us")) / 1e3) : json(nullptr);
        if (points.size() >= 100'000) { points_truncated = true; return; }
        points.push_back({{"frame_sequence", event.value("frame_sequence", 0)},
                          {"source_sequence", event.value("source_sequence", 0)},
                          {"after_pause_start_ms", (time - start) / 1e6},
                          {"after_resume_ms", (time - end) / 1e6},
                          {"extra_relative_lag_ms", relative}});
    });
    return {{"start_ns", start}, {"end_ns", end},
            {"end_basis", measured ? "measured" : "requested duration; actual thread resume not logged"},
            {"sample_truncated", points_truncated},
            {"first_second_after_end", points}};
}

json analyze_capture_campaign(const std::filesystem::path& directory) {
    const auto read_json = [](const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("campaign file missing: " + path.filename().string());
        return json::parse(file);
    };
    const auto plan = read_json(directory / "campaign-plan.json");
    const auto results = read_json(directory / "campaign-results.json");
    if (!plan.contains("cases") || !plan.at("cases").is_array() ||
        !results.contains("runs") || !results.at("runs").is_array())
        throw std::runtime_error("invalid campaign plan or results schema");
    json runs = json::array();
    std::optional<json> environment;
    bool environment_consistent = true, all_valid = results.value("complete", false);
    std::vector<double> normal_source_hz, normal_arrival_p95, normal_residency_p99;
    for (const auto& item : plan.at("cases")) {
        const auto name = item.at("name").get<std::string>();
        if (name.empty() || !std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return std::isalnum(c) || c == '-' || c == '_';
            })) throw std::runtime_error("invalid campaign case name");
        const json* recorded = nullptr;
        for (const auto& candidate : results.at("runs"))
            if (candidate.value("name", "") == name) { recorded = &candidate; break; }
        if (!recorded || recorded->value("returncode", 1) != 0) {
            all_valid = false;
            runs.push_back({{"name", name}, {"valid", false}, {"reported_success", false},
                {"error", recorded ? recorded->value("error", "failed without error text")
                                     : "missing campaign result"}});
            continue;
        }
        const auto folder = directory / name;
        const auto manifest = read_json(folder / "manifest.json");
        const auto summary = analyze_capture_jsonl(folder / "capture.jsonl");
        const auto compared = json{{"serial", manifest.value("serial", "")},
            {"width", manifest.value("width", 0)}, {"height", manifest.value("height", 0)},
            {"source_rotation", manifest.value("source_rotation", -1)},
            {"image_format", manifest.value("image_format", "")},
            {"row_order", manifest.value("row_order", "")},
            {"grpc_endpoint", manifest.value("grpc_endpoint", "")},
            {"fixture_apk_sha256", manifest.value("fixture_apk_sha256", json(nullptr))},
            {"installed_apk_sha256", manifest.value("installed_apk_sha256", json(nullptr))},
            {"device_preflight", manifest.value("device_preflight", json(nullptr))}};
        if (!environment) environment = compared;
        else if (*environment != compared) environment_consistent = false;
        const bool hash_matches = recorded->value("raw_sha256", "") ==
            summary.at("raw_sha256").get<std::string>();
        const bool valid = hash_matches &&
            manifest.at("fixture_apk_sha256") == manifest.at("installed_apk_sha256") &&
            summary.at("geometry_valid").get<bool>() &&
            summary.at("fixture_counter_order_valid").get<bool>() &&
            summary.at("fixture_schemas") == json::array({"native_v2_four_region"}) &&
            summary.at("consumer_events").get<std::uint64_t>() ==
                summary.at("fixture_samples").get<std::uint64_t>();
        if (!valid) all_valid = false;
        if (name.rfind("normal-", 0) == 0 && valid) {
            if (summary.at("fixture_counter_span_hz").is_number())
                normal_source_hz.push_back(summary.at("fixture_counter_span_hz").get<double>());
            normal_arrival_p95.push_back(summary.at("arrival_interval_ms").at("p95").get<double>());
            normal_residency_p99.push_back(summary.at("host_residency_ms").at("p99").get<double>());
        }
        runs.push_back({{"name", name}, {"valid", valid}, {"reported_success", true},
                        {"raw_hash_matches", hash_matches}, {"analysis", summary}});
    }
    if (!environment_consistent) all_valid = false;
    return {{"schema_version", 2}, {"campaign_complete", results.value("complete", false)},
            {"all_valid", all_valid}, {"environment_consistent", environment_consistent},
            {"normal_source_hz", distribution(normal_source_hz)},
            {"normal_arrival_p95_ms", distribution(normal_arrival_p95)},
            {"normal_residency_p99_ms", distribution(normal_residency_p99)},
            {"performance_pass", nullptr}, {"runs", runs}};
}

} // namespace pas
