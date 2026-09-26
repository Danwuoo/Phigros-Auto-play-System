#include "pas/analysis.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <deque>
#include <fstream>
#include <iomanip>
#include <map>
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
    Samples readback_cost, copy_cost, receive_wait, parse_cost, decode_cost,
        publish_cost, capture_to_publish, fixture_color_error;
    Samples fixture_line_displacement, fixture_line_width, fixture_line_contrast,
        fixture_square_width, fixture_square_left_error, fixture_square_right_error, fixture_low_contrast;
    std::uint64_t position_truth_missing = 0, colour_binary_disagreements = 0;
    std::uint64_t square_missing = 0;
    std::optional<json> resource_start, resource_end;
    std::map<std::uint64_t, std::uint64_t> related_rss_peak;
    std::map<std::string, Samples> gpu_engine_samples;
    std::uint64_t gpu_available_samples = 0, gpu_missing_samples = 0;
    std::optional<std::int64_t> previous_capture, first_counter_time, last_counter_time;
    std::optional<std::int64_t> previous_counter;
    std::uint64_t capture_count = 0, consumer_count = 0, counter_count = 0,
                  distinct = 0, counter_increment = 0, consumer_skips = 0, cross_boundary = 0;
    std::uint64_t source_invalid = 0;
    std::uint64_t published_frames = 0, publish_drops = 0;
    std::uint64_t source_callbacks = 0, duplicate_pixels = 0, access_lost = 0,
                  device_lost_rebuilds = 0, resync_gaps = 0;
    std::uint64_t encoded_packets = 0, config_packets = 0, decoded_frames = 0,
                  relative_stale_drops = 0, pool_older_frame_drops = 0;
    // Fixture counters are 24-bit. A fixed 2 MiB bitmap gives exact distinct
    // counts for arbitrarily long JSONL input without retaining events.
    std::vector<std::uint8_t> seen_counters(1u << 21);
    bool counter_order_valid = true;
    std::set<std::tuple<int, int, int>> geometry;
    std::set<std::string> fixture_schemas;
    std::unordered_map<std::uint64_t, std::int64_t> capture_by_sequence;
    std::unordered_map<std::uint64_t, std::int64_t> pixels_ready_by_sequence;
    std::deque<std::uint64_t> capture_order;
    each_jsonl(path, [&](const json& event, std::size_t) {
        const auto kind = event.value("event", "");
        if (kind == "source_callback" || kind == "duplicate_pixels" ||
            kind == "access_lost" || kind == "device_lost_rebuild" || kind == "resync_gap" ||
            kind == "encoded_packet" || kind == "codec_config_packet" ||
            kind == "decoded_frame" || kind == "relative_stale_drop" || kind == "pool_older_frame_drop") {
            const auto time = integer(event, "monotonic_ns");
            if (time < *start || time >= *end) return;
            if (kind == "source_callback") ++source_callbacks;
            else if (kind == "duplicate_pixels") ++duplicate_pixels;
            else if (kind == "access_lost") ++access_lost;
            else if (kind == "device_lost_rebuild") ++device_lost_rebuilds;
            else if (kind == "resync_gap") ++resync_gaps;
            else if (kind == "encoded_packet") ++encoded_packets;
            else if (kind == "codec_config_packet") ++config_packets;
            else if (kind == "relative_stale_drop") ++relative_stale_drops;
            else if (kind == "pool_older_frame_drop") ++pool_older_frame_drops;
            else ++decoded_frames;
        } else if (kind == "capture") {
            const auto time = integer(event, "capture_complete_ns");
            if (event.contains("frame_sequence")) {
                const auto sequence = event.at("frame_sequence").get<std::uint64_t>();
                capture_by_sequence[sequence] = time;
                if (event.contains("pixels_ready_ns") && event.at("pixels_ready_ns").is_number_integer())
                    pixels_ready_by_sequence[sequence] = integer(event, "pixels_ready_ns");
                capture_order.push_back(sequence);
                if (capture_order.size() > 100'000) {
                    capture_by_sequence.erase(capture_order.front());
                    pixels_ready_by_sequence.erase(capture_order.front());
                    capture_order.pop_front();
                }
            }
            if (time < *start || time >= *end) return;
            ++capture_count;
            if (event.contains("published_to_latest")) {
                if (event.at("published_to_latest") == true) ++published_frames;
                else ++publish_drops;
            }
            if (event.contains("source_valid") && event.at("source_valid") == false)
                ++source_invalid;
            no_frame_gaps.add((time - (previous_capture ? *previous_capture : *start)) / 1e6);
            if (previous_capture) {
                if (time < *previous_capture) throw std::runtime_error("capture time regressed in JSONL");
                arrivals.add((time - *previous_capture) / 1e6);
            }
            previous_capture = time;
            if (event.contains("pixels_ready_ns") && event.at("pixels_ready_ns").is_number_integer())
                normalize_cost.add((integer(event, "pixels_ready_ns") - time) / 1e6);
            if (event.contains("readback_start_ns") && event.at("readback_start_ns").is_number_integer() &&
                event.contains("readback_end_ns") && event.at("readback_end_ns").is_number_integer())
                readback_cost.add((integer(event, "readback_end_ns") -
                                   integer(event, "readback_start_ns")) / 1e6);
            if (event.contains("copy_start_ns") && event.at("copy_start_ns").is_number_integer() &&
                event.contains("copy_end_ns") && event.at("copy_end_ns").is_number_integer())
                copy_cost.add((integer(event, "copy_end_ns") - integer(event, "copy_start_ns")) / 1e6);
            if (event.contains("receive_start_ns") && event.at("receive_start_ns").is_number_integer())
                receive_wait.add((time - integer(event, "receive_start_ns")) / 1e6);
            if (event.contains("parse_start_ns") && event.at("parse_start_ns").is_number_integer() &&
                event.contains("parse_end_ns") && event.at("parse_end_ns").is_number_integer())
                parse_cost.add((integer(event, "parse_end_ns") -
                                integer(event, "parse_start_ns")) / 1e6);
            if (event.contains("decode_start_ns") && event.at("decode_start_ns").is_number_integer() &&
                event.contains("decode_end_ns") && event.at("decode_end_ns").is_number_integer())
                decode_cost.add((integer(event, "decode_end_ns") -
                                 integer(event, "decode_start_ns")) / 1e6);
            if (event.contains("width") && event.contains("height") && event.contains("source_rotation"))
                geometry.emplace(event.at("width").get<int>(), event.at("height").get<int>(),
                                 event.at("source_rotation").get<int>());
        } else if (kind == "published") {
            const auto time = integer(event, "published_ns");
            if (time < *start || time >= *end) return;
            const auto sequence = event.at("frame_sequence").get<std::uint64_t>();
            if (const auto ready = pixels_ready_by_sequence.find(sequence);
                ready != pixels_ready_by_sequence.end())
                publish_cost.add((time - ready->second) / 1e6);
            if (const auto captured = capture_by_sequence.find(sequence);
                captured != capture_by_sequence.end())
                capture_to_publish.add((time - captured->second) / 1e6);
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
            if (time >= *start && time < *end) {
                rss_mib.add(integer(event, "working_set_bytes") / 1048576.0);
                if (event.contains("gpu_engines") && event.at("gpu_engines").value("available", false)) {
                    ++gpu_available_samples;
                    for (const auto& engine : event.at("gpu_engines").at("engines"))
                        gpu_engine_samples[engine.at("instance").get<std::string>()].add(engine.at("percent").get<double>());
                } else ++gpu_missing_samples;
                if (event.contains("related_processes") && event.at("related_processes").is_array())
                    for (const auto& process : event.at("related_processes"))
                        if (process.contains("pid") && process.contains("working_set_bytes") &&
                            process.at("working_set_bytes").is_number_unsigned()) {
                            const auto pid = process.at("pid").get<std::uint64_t>();
                            related_rss_peak[pid] = std::max(related_rss_peak[pid],
                                process.at("working_set_bytes").get<std::uint64_t>());
                        }
            }
        } else if (kind == "fixture_counter") {
            const auto time = integer(event, "capture_complete_ns");
            if (time < *start || time >= *end) return;
            fixture_schemas.insert(event.value("fixture_schema", "legacy_unspecified"));
            if (event.contains("binary_counter") && event.at("binary_counter").is_number_integer() &&
                event.contains("exact_colour_identity") && event.at("exact_colour_identity").is_number_integer() &&
                integer(event, "binary_counter") != integer(event, "exact_colour_identity"))
                ++colour_binary_disagreements;
            if (event.contains("max_color_error") && event.at("max_color_error").is_number_integer())
                fixture_color_error.add(event.at("max_color_error").get<double>());
            if (event.contains("line_peak_y") && event.at("line_peak_y").is_number_integer() &&
                event.contains("line_width_px") && event.at("line_width_px").is_number_integer()) {
                const auto peak_y = integer(event, "line_peak_y");
                if (peak_y >= 0 && event.contains("line_expected_y"))
                    fixture_line_displacement.add(std::abs(peak_y - integer(event, "line_expected_y")));
                fixture_line_width.add(integer(event, "line_width_px"));
            }
            if (event.contains("line_contrast") && event.at("line_contrast").is_number_integer())
                fixture_line_contrast.add(integer(event, "line_contrast"));
            if (event.contains("square_left_x") && integer(event, "square_left_x") >= 0)
                fixture_square_width.add(integer(event, "square_width_px"));
            else if (event.contains("square_left_x")) ++square_missing;
            if (event.contains("square_expected_x")) {
                if (event.at("square_expected_x").is_number_integer() &&
                    integer(event, "square_left_x") >= 0) {
                    const auto left = integer(event, "square_left_x");
                    const auto expected = integer(event, "square_expected_x");
                    fixture_square_left_error.add(left - expected);
                    fixture_square_right_error.add(left + integer(event, "square_width_px") - expected - 40);
                } else ++position_truth_missing;
            }
            if (event.contains("low_contrast_delta")) fixture_low_contrast.add(integer(event, "low_contrast_delta"));
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
        json related = json::array();
        if (resource_start->contains("related_processes") &&
            resource_end->contains("related_processes")) {
            for (const auto& process : resource_end->at("related_processes")) {
                const auto pid = process.at("pid").get<std::uint64_t>();
                const json* initial = nullptr;
                for (const auto& candidate : resource_start->at("related_processes"))
                    if (candidate.at("pid").get<std::uint64_t>() == pid) {
                        initial = &candidate; break;
                    }
                json cores = nullptr;
                if (initial && initial->at("process_cpu_ns").is_number_integer() &&
                    process.at("process_cpu_ns").is_number_integer()) {
                    const auto cpu_before = initial->at("process_cpu_ns").get<std::int64_t>();
                    const auto cpu_after = process.at("process_cpu_ns").get<std::int64_t>();
                    if (cpu_after >= cpu_before)
                        cores = (cpu_after - cpu_before) / (end_mid - start_mid);
                }
                related.push_back({{"pid", pid}, {"name", process.value("name", "")},
                    {"cpu_core_equivalent", cores},
                    {"rss_start_bytes", initial ? initial->at("working_set_bytes") : json(nullptr)},
                    {"rss_end_bytes", process.at("working_set_bytes")},
                    {"rss_peak_sample_bytes", related_rss_peak.contains(pid) ?
                        json(related_rss_peak.at(pid)) : json(nullptr)}});
            }
        }
        resource_window["related_processes"] = std::move(related);
    }
    const auto counter_span = first_counter_time && last_counter_time && *last_counter_time > *first_counter_time
        ? std::optional<double>(counter_increment / ((*last_counter_time - *first_counter_time) / 1e9)) : std::nullopt;
    json geometry_valid = shape == json::array({json::array({1280, 720, 1})});
    const auto manifest_path = path.parent_path() / "manifest.json";
    if (std::filesystem::exists(manifest_path)) {
        std::ifstream input(manifest_path, std::ios::binary);
        const auto manifest = json::parse(input);
        if (manifest.value("schema_version", 0) >= 3)
            geometry_valid = shape == json::array({json::array({manifest.at("width"),
                manifest.at("height"), manifest.value("capture_backend", "") == "wgc" ||
                manifest.value("capture_backend", "") == "dxgi" ||
                manifest.value("capture_backend", "") == "scrcpy" ? 0 :
                manifest.value("source_rotation", 0)})});
    }
    json gpu_engines = json::object();
    for (const auto& [instance, samples] : gpu_engine_samples) gpu_engines[instance] = samples.result();
    json references = json::array();
    std::optional<std::int64_t> pause_end, static_resume;
    each_jsonl(path, [&](const json& event, std::size_t) {
        if (event.value("event", "") == "receiver_pause" && event.contains("ended_ns"))
            pause_end = integer(event, "ended_ns");
        if (event.value("event", "") == "source_freeze_command" && !event.value("freeze",true))
            static_resume = integer(event, "after_ns");
        if (event.value("event", "") == "fixture_reference" && references.size() < 8)
            references.push_back(event);
    });
    for (auto& reference : references) {
        reference["first_consumed_matching_or_newer_ns"] = nullptr;
        each_jsonl(path, [&](const json& event, std::size_t) {
            if (!reference.at("first_consumed_matching_or_newer_ns").is_null() ||
                event.value("event", "") != "fixture_counter" ||
                integer(event, "capture_complete_ns") < integer(reference, "before_ns")) return;
            const auto increment = (integer(event, "counter") - integer(reference, "counter") + (1 << 24)) % (1 << 24);
            if (increment < (1 << 23)) reference["first_consumed_matching_or_newer_ns"] = integer(event, "capture_complete_ns");
        });
        const auto context = reference.value("context", "");
        const auto origin = context == "receiver_resume" ? pause_end : context == "static_resume" ? static_resume : std::nullopt;
        reference["recovery_from_resume_ms"] = origin && !reference.at("first_consumed_matching_or_newer_ns").is_null()
            ? json((integer(reference, "first_consumed_matching_or_newer_ns") - *origin) / 1e6) : json(nullptr);
        reference["recovery_semantics"] = "first consumed pixel counter matching/newer than bracketed ADB reference; includes diagnostic call delay; not absolute source age";
    }
    return {{"measurement_start_ns", *start}, {"measurement_end_ns", *end}, {"window_s", duration},
            {"gpu_engine_percent", gpu_engines}, {"gpu_available_samples", gpu_available_samples},
            {"gpu_missing_samples", gpu_missing_samples},
            {"fixture_references", references},
            {"capture_events", capture_count}, {"consumer_events", consumer_count},
            {"published_frames", published_frames}, {"publish_drops", publish_drops},
            {"source_invalid_events", source_invalid},
            {"source_callbacks", source_callbacks}, {"duplicate_pixel_callbacks", duplicate_pixels},
            {"access_lost_events", access_lost},
            {"device_lost_rebuilds", device_lost_rebuilds},
            {"resync_gaps", resync_gaps},
            {"encoded_packets", encoded_packets}, {"codec_config_packets", config_packets},
            {"decoded_frames", decoded_frames},
            {"relative_stale_drops", relative_stale_drops},
            {"pool_older_frame_drops", pool_older_frame_drops},
            {"received_hz", capture_count / duration}, {"fixture_samples", counter_count},
            {"fixture_distinct", distinct}, {"fixture_distinct_hz", distinct / duration},
            {"fixture_counter_span_hz", counter_order_valid && counter_span ? json(*counter_span) : json(nullptr)},
            {"fixture_follow_ratio_estimate", counter_order_valid && counter_span && *counter_span > 0
                ? json((distinct / duration) / *counter_span) : json(nullptr)},
            {"fixture_counter_order_valid", counter_order_valid},
            {"fixture_decoded_fraction", capture_count ? double(counter_count) / capture_count : 0.0},
            {"fixture_decoded_consumer_fraction", consumer_count ? double(counter_count) / consumer_count : 0.0},
            {"fixture_schemas", schema_names},
            {"fixture_colour_binary_disagreements", colour_binary_disagreements},
            {"fixture_max_color_error", fixture_color_error.result()},
            {"fixture_line_displacement_px", fixture_line_displacement.result()},
            {"fixture_line_width_px", fixture_line_width.result()},
            {"fixture_line_contrast", fixture_line_contrast.result()},
            {"fixture_square_width_px", fixture_square_width.result()},
            {"fixture_square_missing", square_missing},
            {"fixture_square_left_error_px", fixture_square_left_error.result()},
            {"fixture_square_right_error_px", fixture_square_right_error.result()},
            {"fixture_position_truth_missing", position_truth_missing},
            {"fixture_low_contrast_delta", fixture_low_contrast.result()},
            {"geometry", shape}, {"geometry_valid", geometry_valid},
            {"arrival_interval_ms", arrivals.result()},
            {"no_frame_gap_ms", no_frame_gaps.result()},
            {"capture_to_pixels_ready_ms", normalize_cost.result()},
            {"gpu_readback_ms", readback_cost.result()},
            {"cpu_copy_or_conversion_ms", copy_cost.result()},
            {"receive_wait_ms", receive_wait.result()},
            {"header_parse_ms", parse_cost.result()},
            {"software_decode_ms", decode_cost.result()},
            {"pixels_ready_to_publish_ms", publish_cost.result()},
            {"capture_to_publish_ms", capture_to_publish.result()},
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
    std::string source_field;
    each_jsonl(path, [&](const json& event, std::size_t) {
        const auto kind = event.value("event", "");
        if (kind == "receiver_pause") pause = event;
        else if (kind == "capture" &&
                 (!pause || integer(event, "capture_complete_ns") <
                      (pause->contains("start_ns") ? integer(*pause, "start_ns") : integer(*pause, "started_ns")))) {
            for (const auto* field : {"source_timestamp_us", "codec_pts",
                                      "source_qpc_ticks", "source_system_relative_100ns"})
                if (event.contains(field) && event.at(field).is_number_integer()) {
                    anchor = event;
                    source_field = field;
                    break;
                }
        }
    });
    if (!pause || !anchor) throw std::runtime_error("pause or pre-pause timestamp anchor missing");
    double source_units_per_ms = 1'000.0;
    if (source_field == "source_system_relative_100ns") source_units_per_ms = 10'000.0;
    else if (source_field == "source_qpc_ticks") {
        std::ifstream manifest(path.parent_path() / "manifest.json", std::ios::binary);
        if (!manifest) throw std::runtime_error("DXGI pause analysis requires QPC frequency manifest");
        const auto metadata = json::parse(manifest);
        source_units_per_ms = metadata.at("qpc_frequency").get<double>() / 1'000.0;
        if (source_units_per_ms <= 0) throw std::runtime_error("invalid QPC frequency in pause manifest");
    }
    const auto start = pause->contains("start_ns") ? integer(*pause, "start_ns") : integer(*pause, "started_ns");
    const bool measured = pause->contains("ended_ns") && pause->at("ended_ns").is_number_integer();
    const auto end = measured ? integer(*pause, "ended_ns") :
        start + static_cast<std::int64_t>(std::llround(pause->value("duration_ms", 500.0) * 1e6));
    json points = json::array();
    bool points_truncated = false;
    each_jsonl(path, [&](const json& event, std::size_t) {
        if (event.value("event", "") != "capture" || !event.contains(source_field) ||
            !event.at(source_field).is_number_integer()) return;
        const auto time = integer(event, "capture_complete_ns");
        if (time < end || time >= end + 1'000'000'000LL) return;
        const bool same = event.value("stream_generation", 0) == anchor->value("stream_generation", 0);
        const auto relative = same ? json((time - integer(*anchor, "capture_complete_ns")) / 1e6 -
            (integer(event, source_field.c_str()) - integer(*anchor, source_field.c_str())) /
                source_units_per_ms) : json(nullptr);
        if (points.size() >= 100'000) { points_truncated = true; return; }
        points.push_back({{"frame_sequence", event.value("frame_sequence", 0)},
                          {"source_sequence", event.contains("source_sequence") ? event.at("source_sequence") : json(nullptr)},
                          {"after_pause_start_ms", (time - start) / 1e6},
                          {"after_resume_ms", (time - end) / 1e6},
                          {"extra_relative_lag_ms", relative}});
    });
    return {{"start_ns", start}, {"end_ns", end}, {"source_field", source_field},
            {"source_units_per_ms", source_units_per_ms},
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
    if (plan.value("schema_version", 0) >= 3 &&
        plan.value("kind", "") == "five_capture_campaign") {
        json runs = json::array();
        bool all_valid = results.value("complete", false);
        bool formal_valid = all_valid;
        bool environment_consistent = true;
        std::map<std::string, std::vector<double>> arrival_p95, residency_p99, source_hz;
        std::map<std::string, std::uint64_t> valid_normal_count;
        for (const auto& item : plan.at("cases")) {
            const auto name = item.at("name").get<std::string>();
            if (name.empty() || !std::all_of(name.begin(), name.end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '-' || c == '_';
                })) throw std::runtime_error("invalid five-campaign case name");
            const auto candidate = item.at("candidate").get<std::string>();
            const bool diagnostic = item.value("diagnostic_only", false);
            const json* recorded = nullptr;
            for (const auto& result : results.at("runs"))
                if (result.value("name", "") == name) { recorded = &result; break; }
            if (!recorded || recorded->value("returncode", 1) != 0) {
                all_valid = false;
                if (!diagnostic) formal_valid = false;
                runs.push_back({{"name", name}, {"candidate", candidate}, {"target_hz", item.at("target_hz")},
                    {"valid", false}, {"diagnostic_only", diagnostic},
                    {"error", recorded ? recorded->value("error", "failed without error") : "not run"}});
                continue;
            }
            const auto folder = directory / name;
            const auto manifest = read_json(folder / "manifest.json");
            const auto summary = analyze_capture_jsonl(folder / "capture.jsonl");
            const bool hashes = summary.at("raw_sha256") == recorded->at("raw_sha256") &&
                sha256_file(folder / "diagnostic.png") ==
                    recorded->at("diagnostic_png_sha256").get<std::string>();
            const bool fixture = manifest.at("fixture_apk_sha256") == plan.at("fixture_apk_sha256") &&
                manifest.at("installed_apk_sha256") == plan.at("installed_apk_sha256");
            auto planned_device = plan.at("preflight");
            auto observed_device = manifest.at("device_preflight");
            planned_device.erase("fixture_target_hz_property");
            observed_device.erase("fixture_target_hz_property");
            auto planned_host = plan.at("host_capture_environment");
            auto observed_host = manifest.at("host_capture_environment");
            planned_host.erase("selected_window_dpi");
            observed_host.erase("selected_window_dpi");
            const bool environment = planned_device == observed_device && planned_host == observed_host;
            if (!environment) environment_consistent = false;
            const bool binary = manifest.at("binary_sha256") == plan.at("binary_sha256");
            const bool server = candidate != "scrcpy" ||
                manifest.at("scrcpy_server_sha256") == plan.at("scrcpy_server_sha256");
            const bool ready = recorded->contains("ready_preflight") &&
                recorded->at("ready_preflight").is_object() &&
                recorded->at("ready_preflight").value("first_counter", -1) !=
                    recorded->at("ready_preflight").value("second_counter", -1);
            bool schemas_valid = !summary.at("fixture_schemas").empty();
            for (const auto& schema : summary.at("fixture_schemas")) {
                const auto value = schema.get<std::string>();
                if (value != "native_v2_four_region" &&
                    !(candidate == "scrcpy" && value == "native_v2_lossy_validated"))
                    schemas_valid = false;
            }
            const auto& line_displacement = summary.at("fixture_line_displacement_px");
            const auto& line_width = summary.at("fixture_line_width_px");
            const bool position = !manifest.value("fixture_position_truth_required", false) ||
                (summary.at("fixture_position_truth_missing").get<std::uint64_t>() == 0 &&
                 summary.at("fixture_square_left_error_px").value("n", 0u) == summary.at("consumer_events"));
            const bool quality = summary.at("fixture_square_missing").get<std::uint64_t>() == 0 &&
                line_displacement.value("n", 0u) > 0 &&
                line_displacement.value("max", 999.0) <= 1.0 &&
                line_width.value("n", 0u) > 0 &&
                line_width.value("p50", 0.0) >= 1.0 &&
                line_width.value("p50", 999.0) <= 2.0;
            const bool source_valid = diagnostic ||
                summary.at("source_invalid_events").get<std::uint64_t>() == 0;
            const bool valid = hashes && fixture && environment && binary && server && ready &&
                summary.at("geometry_valid").get<bool>() &&
                summary.at("fixture_counter_order_valid").get<bool>() &&
                schemas_valid && quality && position && source_valid &&
                summary.at("consumer_events") == summary.at("fixture_samples") &&
                manifest.value("run_class", "") == "formal_campaign";
            if (!valid) {
                all_valid = false;
                if (!diagnostic) formal_valid = false;
            }
            if (valid && name.rfind("normal-", 0) == 0) {
                const auto key = candidate + "-" + std::to_string(item.at("target_hz").get<int>());
                ++valid_normal_count[key];
                if (summary.at("arrival_interval_ms").contains("p95"))
                    arrival_p95[key].push_back(summary.at("arrival_interval_ms").at("p95").get<double>());
                if (summary.at("host_residency_ms").contains("p99"))
                    residency_p99[key].push_back(summary.at("host_residency_ms").at("p99").get<double>());
                if (summary.at("fixture_counter_span_hz").is_number())
                    source_hz[key].push_back(summary.at("fixture_counter_span_hz").get<double>());
            }
            runs.push_back({{"name", name}, {"candidate", candidate}, {"target_hz", item.at("target_hz")},
                {"valid", valid}, {"diagnostic_only", diagnostic}, {"raw_hash_matches", hashes},
                {"fixture_hash_matches", fixture}, {"environment_matches", environment},
                {"binary_hash_matches", binary}, {"server_hash_matches", server},
                {"ready_preflight_valid", ready}, {"fixture_quality_valid", quality},
                {"fixture_position_truth_valid", position},
                {"source_valid", source_valid},
                {"analysis", summary}});
        }
        json groups = json::object();
        for (const auto& [key, count] : valid_normal_count)
            groups[key] = {{"valid_runs", count},
                {"source_counter_span_hz", distribution(source_hz[key])},
                {"arrival_p95_ms", distribution(arrival_p95[key])},
                {"residency_p99_ms", distribution(residency_p99[key])}};
        return {{"schema_version", 3}, {"campaign_complete", results.value("complete", false)},
            {"all_valid", all_valid}, {"formal_candidates_valid", formal_valid},
            {"environment_consistent", environment_consistent},
            {"groups", groups}, {"performance_pass", nullptr}, {"runs", runs}};
    }
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
