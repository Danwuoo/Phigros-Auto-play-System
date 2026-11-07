#include "rhythm/chart.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <nlohmann/json.hpp>

namespace rhythm {
namespace {
using Json = nlohmann::json;
[[noreturn]] void fail(const std::string& path, const std::string& reason) {
    throw std::invalid_argument(path + ": " + reason);
}
void reject_block_area_fields(const Json& value, const std::string& path, bool allow_v3_root) {
    if (value.is_object()) {
        for (const auto& [key, child] : value.items()) {
            if (allow_v3_root && path.empty() && key == "blockAreaList") continue;
            std::string normalized;
            for (const unsigned char c : key) {
                if (c == ' ' || c == '_' || c == '-') continue;
                normalized += static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
            }
            const auto child_path = path + "/" + key;
            if (normalized == "blockarealist")
                fail(child_path, "unsupported blockAreaList declaration; expected exact top-level v3 field");
            reject_block_area_fields(child, child_path, false);
        }
    } else if (value.is_array()) {
        for (std::size_t i = 0; i < value.size(); ++i)
            reject_block_area_fields(value[i], path + "/" + std::to_string(i), false);
    }
}
void object_fields(const Json& object, const std::string& path,
                   std::initializer_list<std::string_view> required,
                   std::initializer_list<std::string_view> optional = {}) {
    if (!object.is_object()) fail(path, "expected object");
    for (const auto key : required)
        if (!object.contains(key)) fail(path + "/" + std::string(key), "missing field");
    for (const auto& [key, value] : object.items()) {
        (void)value;
        if (std::find(required.begin(), required.end(), key) == required.end() &&
            std::find(optional.begin(), optional.end(), key) == optional.end())
            fail(path + "/" + key, "unsupported field in strict v3 dialect");
    }
}
double number(const Json& value, const std::string& path, double max_abs) {
    if (!value.is_number()) fail(path, "expected finite number");
    const double result = value.get<double>();
    if (!std::isfinite(result) || std::abs(result) > max_abs) fail(path, "numeric range exceeded");
    return result;
}
const Json& array(const Json& value, const std::string& path) {
    if (!value.is_array()) fail(path, "expected array");
    return value;
}
SourceLocation location(const ChartSource& source, std::string path) {
    return {source.chart_sha256 + "#" + path, std::move(path)};
}
BlockAreaPoint block_area_point(const Json& value, const std::string& path, const ChartImportLimits& limits) {
    object_fields(value, path, {"x", "y"});
    return {number(value["x"], path + "/x", limits.max_abs_value),
            number(value["y"], path + "/y", limits.max_abs_value)};
}
SongNs block_area_time(const Json& value, const std::string& path, double offset, const ChartImportLimits& limits) {
    const auto seconds = number(value, path, limits.max_abs_note_seconds);
    const auto ns = std::round((static_cast<long double>(seconds) + offset) * 1e9L);
    if (!std::isfinite(ns) || std::abs(ns) > static_cast<long double>(limits.max_abs_note_seconds) * 1e9L ||
        ns < static_cast<long double>(std::numeric_limits<SongNs>::min()) ||
        ns >= static_cast<long double>(std::numeric_limits<SongNs>::max()))
        fail(path, "block area song-time range exceeded");
    return static_cast<SongNs>(ns);
}
int block_area_ease(const Json& object, const char* key, const std::string& path) {
    if (!object.contains(key)) return 0;
    const auto& value = object[key];
    if (!value.is_number_integer() || value < 0 || value > 14) fail(path + "/" + key, "unknown block area easing");
    return value.get<int>();
}
template<class Event, class Parse>
void block_area_track(std::vector<Event>& track, const Json& value, const std::string& path,
                      std::size_t& remaining, Parse parse) {
    const auto& events = array(value, path);
    if (events.size() > remaining) fail(path, "block area keyframe limit exceeded");
    remaining -= events.size();
    track.reserve(events.size());
    for (std::size_t i = 0; i < events.size(); ++i)
        track.push_back(parse(events[i], path + "/" + std::to_string(i)));
    std::stable_sort(track.begin(), track.end(), [](const Event& a, const Event& b) { return a.time_ns < b.time_ns; });
}
void import_block_areas(const Json& value, SemanticChart& chart, const ChartImportLimits& limits) {
    if (value.is_null()) return;
    const auto& areas = array(value, "/blockAreaList");
    constexpr std::size_t area_limit = 20000;
    auto remaining = limits.max_events - chart.event_count;
    if (areas.size() > area_limit || areas.size() > remaining) fail("/blockAreaList", "block area limit exceeded");
    remaining -= areas.size();
    chart.block_areas.reserve(areas.size());
    for (std::size_t i = 0; i < areas.size(); ++i) {
        const auto path = "/blockAreaList/" + std::to_string(i);
        const auto& item = areas[i];
        object_fields(item, path, {"topRightPercentage", "bottomLeftPercentage", "appearTime", "enableTime",
                                   "disableTime", "disappearTime", "isSubtract", "moveEvents", "rotateEvents", "scaleEvents"});
        const auto time = [&](const Json& keyframe, const std::string& event_path, const char* key) {
            return block_area_time(keyframe[key], event_path + "/" + key, chart.offset_seconds, limits);
        };
        const auto point = [&](const Json& keyframe, const std::string& event_path, const char* key) {
            return block_area_point(keyframe[key], event_path + "/" + key, limits);
        };
        ChartBlockArea area;
        area.source = location(chart.source, path);
        area.top_right = point(item, path, "topRightPercentage");
        area.bottom_left = point(item, path, "bottomLeftPercentage");
        if (!item["isSubtract"].is_boolean()) fail(path + "/isSubtract", "expected boolean");
        area.subtract = item["isSubtract"].get<bool>();
        // Candidate phase and keyframe conventions follow the community implementation
        // at TeamFlos/phira#901 (0afa0fa11320a7c457f2fcce536c446df4b898ed).
        area.appear_ns = time(item, path, "appearTime");
        area.enable_ns = std::max(area.appear_ns, time(item, path, "enableTime"));
        area.disable_ns = std::max(area.enable_ns, time(item, path, "disableTime"));
        area.disappear_ns = std::max(area.disable_ns, time(item, path, "disappearTime"));
        block_area_track(area.moves, item["moveEvents"], path + "/moveEvents", remaining,
            [&](const Json& event, const std::string& event_path) {
                object_fields(event, event_path, {"time", "endPosition"}, {"easeTypeX", "easeTypeY"});
                return BlockAreaMove{time(event, event_path, "time"), point(event, event_path, "endPosition"),
                    block_area_ease(event, "easeTypeX", event_path), block_area_ease(event, "easeTypeY", event_path)};
            });
        block_area_track(area.rotations, item["rotateEvents"], path + "/rotateEvents", remaining,
            [&](const Json& event, const std::string& event_path) {
                object_fields(event, event_path, {"time", "anchor", "rotation"}, {"easeType"});
                return BlockAreaRotation{time(event, event_path, "time"), point(event, event_path, "anchor"),
                    number(event["rotation"], event_path + "/rotation", limits.max_abs_value),
                    block_area_ease(event, "easeType", event_path)};
            });
        block_area_track(area.scales, item["scaleEvents"], path + "/scaleEvents", remaining,
            [&](const Json& event, const std::string& event_path) {
                object_fields(event, event_path, {"time", "anchor", "scale"}, {"easeTypeX", "easeTypeY"});
                return BlockAreaScale{time(event, event_path, "time"), point(event, event_path, "anchor"),
                    point(event, event_path, "scale"), block_area_ease(event, "easeTypeX", event_path),
                    block_area_ease(event, "easeTypeY", event_path)};
            });
        chart.block_areas.push_back(std::move(area));
    }
    chart.import_interpretation_unqualified |= !chart.block_areas.empty();
}
ChartEventTime event_time(const Json& event, const std::string& path, double bpm,
                          double offset, const ChartImportLimits& limits, bool candidate_pose=false) {
    const double start = number(event.at("startTime"), path + "/startTime", limits.max_abs_tick);
    const double end = number(event.at("endTime"), path + "/endTime", limits.max_abs_tick);
    if (end < start && !candidate_pose) fail(path, "reversed event interval");
    return {start, end, phigros_time_ns(start, bpm, offset), phigros_time_ns(end, bpm, offset)};
}
void validate_limits(const ChartImportLimits& limits) {
    if (!limits.max_json_bytes || !limits.max_json_depth || !limits.max_lines ||
        !limits.max_notes || !limits.max_events) throw std::invalid_argument("zero import resource limit");
    for (double value : {limits.max_abs_tick, limits.max_abs_value, limits.max_bpm,
                         limits.max_abs_offset_seconds, limits.max_abs_note_seconds})
        if (!std::isfinite(value) || value <= 0) throw std::invalid_argument("invalid import numeric limit");
}
bool constant_event(const NumericLineEvent& e) {return e.start==e.end;}
bool constant_event(const MoveLineEvent& e) {return e.start_x==e.end_x&&e.start_y==e.end_y;}
template<class Event> Event evaluated_slice(const Event& original, ChartEventTime time) {
    const auto fraction=[&](SongNs at) {
        return static_cast<double>((static_cast<long double>(at)-original.time.start_ns)/
            (static_cast<long double>(original.time.end_ns)-original.time.start_ns));
    };
    Event result=original;
    result.time=time;
    if constexpr(std::is_same_v<Event,MoveLineEvent>) {
        result.start_x=std::lerp(original.start_x,original.end_x,fraction(time.start_ns));
        result.end_x=std::lerp(original.start_x,original.end_x,fraction(time.end_ns));
        result.start_y=std::lerp(original.start_y,original.end_y,fraction(time.start_ns));
        result.end_y=std::lerp(original.start_y,original.end_y,fraction(time.end_ns));
    } else {
        result.start=std::lerp(original.start,original.end,fraction(time.start_ns));
        result.end=std::lerp(original.start,original.end,fraction(time.end_ns));
    }
    return result;
}
// Candidate semantics, motivated by local phisap 8ac9a67 judge_line.py's
// first-source-match scan. Half-open intervals and before/gap/after endpoint
// hold deliberately remain this adapter's declared conventions. This is not an
// assertion about the proprietary game engine or a general malformed-data fix.
// Original declarations are never sorted, swapped or overwritten. A source
// priority sweep creates a separate disjoint EVALUATION representation.
template<class Event> void candidate_pose_track(std::vector<Event>& evaluated,const Json& original_json,
        SemanticChart& chart,std::size_t line_index,const std::string& key,
        const ChartImportLimits& limits,const ImportOptions& options) {
    const auto original=evaluated;
    const bool general_empty=options.policy==PhigrosImportPolicy::native_v1_source_priority_empty_interval_candidate_v1;
    bool needs_evaluation=false;
    std::optional<std::size_t> previous;
    for(std::size_t i=0;i<original.size();++i) {
        const auto& event=original[i];const auto& t=event.time;
        if(t.end_tick<t.start_tick) {
            if(!constant_event(event)&&!general_empty)fail(event.source.json_pointer,"candidate policy rejects nonconstant reversed pose events");
            needs_evaluation=true;continue;
        }
        if(previous) {
            const auto& before=original[*previous].time;
            if(t.start_ns<before.start_ns||t.start_ns<before.end_ns||t.start_tick<before.start_tick||t.start_tick<before.end_tick)
                needs_evaluation=true;
        }
        previous=i;
    }
    const auto account=[&](std::size_t count) {
        const auto bound=std::min(options.max_evaluated_pose_events,limits.max_events);
        if(count>bound-chart.evaluated_pose_event_count)fail("/judgeLineList","candidate evaluated-pose event limit exceeded");
        chart.evaluated_pose_event_count+=count;
    };
    if(!needs_evaluation) {account(evaluated.size());return;}
    struct Boundary {double tick=0;std::vector<std::size_t> begin,end;};
    std::map<SongNs,Boundary> boundaries;
    const auto boundary=[&](SongNs time,double tick)->Boundary& {
        const auto [it,added]=boundaries.try_emplace(time,Boundary{tick,{},{}});
        if(!added&&it->second.tick!=tick)
            fail(original.front().source.json_pointer,"candidate pose boundaries collide after nanosecond normalization");
        return it->second;
    };
    for(std::size_t i=0;i<original.size();++i) {
        const auto& e=original[i];
        if(e.time.end_tick<e.time.start_tick)continue;
        // Pure zero-time steps retain their existing meaning on ordered tracks.
        // Mixed with malformed/overlapping declarations, source-first behavior
        // at that instant is unproved, so the candidate explicitly refuses it.
        if(e.time.start_ns==e.time.end_ns)
            fail(e.source.json_pointer,"candidate mixed zero-time/overlapping pose interpretation is unsupported");
        boundary(e.time.start_ns,e.time.start_tick).begin.push_back(i);
        boundary(e.time.end_ns,e.time.end_tick).end.push_back(i);
    }
    if(boundaries.empty())fail(original.front().source.json_pointer,"candidate pose track contains no evaluable declaration");
    struct Segment {std::size_t source=0;ChartEventTime time;};
    std::vector<Segment> segments;
    std::set<std::size_t> active;
    const auto event_limit=std::min(options.max_evaluated_pose_events,limits.max_events);
    for(auto it=boundaries.begin();it!=boundaries.end();++it) {
        for(const auto i:it->second.end)active.erase(i);
        for(const auto i:it->second.begin)active.insert(i);
        const auto next=std::next(it);
        if(next==boundaries.end()||active.empty())continue;
        const auto selected=*active.begin(); // Native declaration order, not nearest time.
        const ChartEventTime time{it->second.tick,next->second.tick,it->first,next->first};
        if(!segments.empty()&&segments.back().source==selected&&segments.back().time.end_ns==time.start_ns) {
            segments.back().time.end_ns=time.end_ns;segments.back().time.end_tick=time.end_tick;
        } else {
            if(segments.size()>=event_limit)fail(original[selected].source.json_pointer,"candidate pose subdivision limit exceeded");
            segments.push_back({selected,time});
        }
    }
    account(segments.size());
    std::vector<std::vector<ChartEventTime>> pieces(original.size());
    evaluated.clear();evaluated.reserve(segments.size());
    for(const auto& segment:segments) {
        evaluated.push_back(evaluated_slice(original[segment.source],segment.time));
        pieces[segment.source].push_back(segment.time);
    }
    for(std::size_t i=0;i<original.size();++i) {
        if(chart.import_anomalies.size()>=options.max_anomalies)
            fail(original[i].source.json_pointer,"candidate import anomaly provenance limit exceeded");
        const bool empty=original[i].time.end_tick<original[i].time.start_tick;
        ChartImportAnomaly anomaly;
        anomaly.source=original[i].source;anomaly.line_index=line_index;anomaly.declaration_index=i;anomaly.event_track=key;
        anomaly.declared_time=original[i].time;anomaly.original_declaration_json=original_json[i].dump();
        anomaly.code=empty?(constant_event(original[i])?"empty_reversed_constant_pose_event":"empty_reversed_pose_event"):"source_priority_pose_track";
        anomaly.reason=empty?(constant_event(original[i])?"Constant reversed declaration has no time satisfying start<=t<end; retained without evaluation":
            "Explicit native-v1 empty-interval candidate: no time satisfies start<=t<end, irrespective of values; native declaration retained without evaluation; community Phira parse/pgr.rs validate_events reference, not official engine qualification"):
            "Explicit candidate source-first active half-open interpretation; original declaration retained separately";
        anomaly.declaration_evaluated=!pieces[i].empty();anomaly.evaluated_segments=std::move(pieces[i]);
        chart.import_anomalies.push_back(std::move(anomaly));
    }
}
bool hex_sha(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
} // namespace

std::string_view phigros_import_policy_id(PhigrosImportPolicy policy) {
    switch(policy) {
    case PhigrosImportPolicy::strict_v3:return "phigros-v3-strict-v1";
    case PhigrosImportPolicy::source_priority_constant_empty_candidate_v1:
        return "phigros-v3-source-priority-constant-empty-candidate-v1";
    case PhigrosImportPolicy::native_v1_source_priority_constant_empty_candidate_v1:
        return "phigros-v1-source-priority-constant-empty-candidate-v1";
    case PhigrosImportPolicy::native_v1_source_priority_empty_interval_candidate_v1:
        return "phigros-v1-source-priority-empty-interval-candidate-v1";
    }
    throw std::invalid_argument("unknown Phigros import policy");
}

SongNs phigros_time_ns(double tick, double bpm, double offset_seconds) {
    // Community primary reference, not a Pigeon Games specification:
    // TeamFlos/phira@191b56e8404e26ab4bdf6c7d38dc577a265510a7,
    // prpr/src/parse/pgr.rs:220 and prpr/src/scene/game.rs:979 (music_time - offset).
    if (!std::isfinite(tick) || !std::isfinite(bpm) || bpm <= 0 || !std::isfinite(offset_seconds))
        throw std::invalid_argument("nonfinite time or nonpositive BPM");
    const long double ns = std::round((static_cast<long double>(tick) * 1.875L /
                                      static_cast<long double>(bpm) + offset_seconds) * 1000000000.L);
    if (!std::isfinite(ns) || ns < static_cast<long double>(std::numeric_limits<SongNs>::min()) ||
        ns >= static_cast<long double>(std::numeric_limits<SongNs>::max()))
        throw std::overflow_error("chart timestamp exceeds signed nanoseconds");
    return static_cast<SongNs>(ns);
}

std::string chart_text_sha256(std::string_view text) {
    // FIPS 180-4 SHA-256 compression; hashes original bytes, not reserialized JSON.
    constexpr std::array<std::uint32_t, 64> k{
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t, 8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                  0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const auto block = [&](const unsigned char* bytes) {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16; ++i)
            for (std::size_t j = 0; j < 4; ++j) w[i] = (w[i] << 8) | bytes[i * 4 + j];
        for (std::size_t i = 16; i < 64; ++i) {
            const auto a = std::rotr(w[i-15], 7) ^ std::rotr(w[i-15], 18) ^ (w[i-15] >> 3);
            const auto b = std::rotr(w[i-2], 17) ^ std::rotr(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + a + w[i-7] + b;
        }
        auto v = h;
        for (std::size_t i = 0; i < 64; ++i) {
            const auto s1 = std::rotr(v[4], 6) ^ std::rotr(v[4], 11) ^ std::rotr(v[4], 25);
            const auto t1 = v[7] + s1 + ((v[4] & v[5]) ^ (~v[4] & v[6])) + k[i] + w[i];
            const auto s0 = std::rotr(v[0], 2) ^ std::rotr(v[0], 13) ^ std::rotr(v[0], 22);
            const auto t2 = s0 + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
            v = {t1 + t2, v[0], v[1], v[2], v[3] + t1, v[4], v[5], v[6]};
        }
        for (std::size_t i = 0; i < 8; ++i) h[i] += v[i];
    };
    std::size_t offset = 0;
    while (text.size() - offset >= 64) {
        block(reinterpret_cast<const unsigned char*>(text.data() + offset)); offset += 64;
    }
    std::array<unsigned char, 128> tail{};
    const auto remaining = text.size() - offset;
    for (std::size_t i = 0; i < remaining; ++i) tail[i] = static_cast<unsigned char>(text[offset+i]);
    tail[remaining] = 0x80;
    const std::size_t length = remaining < 56 ? 64 : 128;
    const auto bits = static_cast<std::uint64_t>(text.size()) * 8;
    for (std::size_t i = 0; i < 8; ++i) tail[length-1-i] = static_cast<unsigned char>(bits >> (i*8));
    block(tail.data());
    if (length == 128) block(tail.data()+64);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto value : h) out << std::setw(8) << value;
    return out.str();
}

SemanticChart parse_phigros(std::string_view text, ChartSource source, const ChartImportLimits& limits,const ImportOptions& options) {
    validate_limits(limits);
    const auto policy_id=phigros_import_policy_id(options.policy);
    const bool native_v1=options.policy==PhigrosImportPolicy::native_v1_source_priority_constant_empty_candidate_v1||
        options.policy==PhigrosImportPolicy::native_v1_source_priority_empty_interval_candidate_v1;
    const bool candidate=options.policy==PhigrosImportPolicy::source_priority_constant_empty_candidate_v1||native_v1;
    if(options.max_anomalies==0||options.max_anomalies>2000000||options.max_evaluated_pose_events==0||options.max_evaluated_pose_events>2000000)
        fail("import_options","invalid candidate provenance/evaluation resource limit");
    if (text.size() > limits.max_json_bytes) fail("/", "JSON byte limit exceeded");
    if (source.label.empty() || source.label.size() > 4096) fail("source", "missing/oversized source label");
    const std::string actual_sha = chart_text_sha256(text);
    if (!source.chart_sha256.empty() && (!hex_sha(source.chart_sha256) || source.chart_sha256 != actual_sha))
        fail("source/chart_sha256", "expected hash does not match original JSON bytes");
    source.chart_sha256 = actual_sha;
    Json root;
    try {
        std::vector<std::set<std::string>> objects;
        root = Json::parse(text.begin(), text.end(), [&](int depth, Json::parse_event_t event, Json& parsed) {
            if (depth < 0 || static_cast<std::size_t>(depth) > limits.max_json_depth) fail("/", "JSON depth limit exceeded");
            if (event == Json::parse_event_t::object_start) objects.emplace_back();
            if (event == Json::parse_event_t::key && !objects.back().insert(parsed.get<std::string>()).second)
                fail("/", "duplicate JSON object key");
            if (event == Json::parse_event_t::object_end) objects.pop_back();
            return true;
        });
    } catch (const Json::exception&) { fail("/", "invalid JSON or numeric overflow"); }
    reject_block_area_fields(root, "", !native_v1);
    object_fields(root, "", {"formatVersion", "offset", "judgeLineList"}, {"blockAreaList"});
    const int required_version=native_v1 ? 1 : 3;
    if (!root["formatVersion"].is_number_integer() || root["formatVersion"] != required_version)
        fail("/formatVersion",native_v1 ? "explicit native v1 policy requires version 1" : "only version 3 is supported under this import policy");
    SemanticChart chart;
    chart.source_format_version=required_version;
    chart.source = std::move(source);
    chart.import_policy_id=policy_id;chart.import_interpretation_unqualified=candidate;
    chart.offset_seconds = number(root["offset"], "/offset", limits.max_abs_offset_seconds);
    const auto& lines = array(root["judgeLineList"], "/judgeLineList");
    if (lines.size() > limits.max_lines) fail("/judgeLineList", "line limit exceeded");
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto path = "/judgeLineList/" + std::to_string(index);
        const auto& input = lines[index];
        object_fields(input, path, {"bpm", "notesAbove", "notesBelow", "speedEvents", "judgeLineMoveEvents",
                                   "judgeLineRotateEvents", "judgeLineDisappearEvents"});
        ChartLine line;
        line.source = location(chart.source, path); line.source_index = index;
        line.bpm = number(input["bpm"], path + "/bpm", limits.max_bpm);
        if (line.bpm <= 0) fail(path + "/bpm", "BPM must be positive");
        for (const auto side : {NoteSide::above, NoteSide::below}) {
            const std::string key = side == NoteSide::above ? "notesAbove" : "notesBelow";
            const auto& notes = array(input[key], path + "/" + key);
            if (notes.size() > limits.max_notes - chart.note_count) fail(path, "note limit exceeded");
            for (std::size_t n = 0; n < notes.size(); ++n) {
                const auto p = path + "/" + key + "/" + std::to_string(n);
                const auto& value = notes[n];
                object_fields(value, p, {"type", "time", "holdTime", "positionX", "speed", "floorPosition"});
                if (!value["type"].is_number_integer() || value["type"] < 1 || value["type"] > 4) fail(p + "/type", "unknown note type");
                SemanticNote note;
                note.source = location(chart.source, p); note.source_index = n; note.side = side;
                note.kind = static_cast<NoteKind>(value["type"].get<int>());
                note.time_tick = number(value["time"], p + "/time", limits.max_abs_tick);
                note.hold_ticks = number(value["holdTime"], p + "/holdTime", limits.max_abs_tick);
                if (note.time_tick < 0 || note.hold_ticks < 0 ||
                    (note.kind != NoteKind::hold && note.hold_ticks != 0)) fail(p, "invalid note/hold duration");
                note.start_ns = phigros_time_ns(note.time_tick, line.bpm, chart.offset_seconds);
                note.end_ns = phigros_time_ns(note.time_tick + note.hold_ticks, line.bpm, chart.offset_seconds);
                const long double bound = static_cast<long double>(limits.max_abs_note_seconds) * 1e9L;
                if (std::abs(static_cast<long double>(note.start_ns)) > bound ||
                    std::abs(static_cast<long double>(note.end_ns)) > bound) fail(p, "note song-time range exceeded");
                note.position_x = number(value["positionX"], p + "/positionX", limits.max_abs_value);
                note.speed = number(value["speed"], p + "/speed", limits.max_abs_value);
                note.floor_position = number(value["floorPosition"], p + "/floorPosition", limits.max_abs_value);
                line.notes.push_back(std::move(note)); ++chart.note_count;
            }
        }
        for (const std::string key : {"speedEvents", "judgeLineMoveEvents", "judgeLineRotateEvents", "judgeLineDisappearEvents"}) {
            const auto& events = array(input[key], path + "/" + key);
            if (events.size() > limits.max_events - chart.event_count) fail(path, "event limit exceeded");
            for (std::size_t e = 0; e < events.size(); ++e) {
                const auto p = path + "/" + key + "/" + std::to_string(e);
                const auto& value = events[e];
                if (key == "speedEvents") {
                    object_fields(value, p, {"startTime", "endTime", "value"}, {"floorPosition"});
                    SpeedLineEvent event;
                    event.source = location(chart.source, p);
                    event.time = event_time(value, p, line.bpm, chart.offset_seconds, limits);
                    event.value = number(value["value"], p + "/value", limits.max_abs_value);
                    if (value.contains("floorPosition")) event.floor_position = number(value["floorPosition"], p + "/floorPosition", limits.max_abs_value);
                    line.speed_events.push_back(std::move(event));
                } else if (key == "judgeLineMoveEvents") {
                    if(native_v1) {
                        object_fields(value,p,{"startTime","endTime","start","end"});
                        const auto start=number(value["start"],p+"/start",limits.max_abs_value);
                        const auto end=number(value["end"],p+"/end",limits.max_abs_value);
                        // Community primary implementation: TeamFlos/phira
                        // @750579396cbc5dc61e6379fbfb2aa1adb70751a6,
                        // prpr/src/parse/pgr.rs, parse_move_events_fv1. Remainder
                        // is truncating (including negative inputs), not floor.
                        // Decode endpoint coordinates BEFORE interpolating them.
                        const auto start_y=std::fmod(start,1000.0),end_y=std::fmod(end,1000.0);
                        line.move_events.push_back({location(chart.source,p),event_time(value,p,line.bpm,chart.offset_seconds,limits,candidate),
                            (start-start_y)/880000.0,(end-end_y)/880000.0,start_y/520.0,end_y/520.0,start,end});
                    } else {
                        object_fields(value, p, {"startTime", "endTime", "start", "end", "start2", "end2"});
                        line.move_events.push_back({location(chart.source, p), event_time(value, p, line.bpm, chart.offset_seconds, limits,candidate),
                            number(value["start"], p + "/start", limits.max_abs_value), number(value["end"], p + "/end", limits.max_abs_value),
                            number(value["start2"], p + "/start2", limits.max_abs_value), number(value["end2"], p + "/end2", limits.max_abs_value)});
                    }
                } else {
                    object_fields(value, p, {"startTime", "endTime", "start", "end"});
                    NumericLineEvent event{location(chart.source, p), event_time(value, p, line.bpm, chart.offset_seconds, limits,candidate),
                        number(value["start"], p + "/start", limits.max_abs_value), number(value["end"], p + "/end", limits.max_abs_value)};
                    (key == "judgeLineRotateEvents" ? line.rotate_events : line.opacity_events).push_back(std::move(event));
                }
                ++chart.event_count;
            }
        }
        if(candidate) {
            line.native_event_declarations=NativeLineEventDeclarations{line.move_events,line.rotate_events,line.opacity_events,line.speed_events};
            candidate_pose_track(line.move_events,input["judgeLineMoveEvents"],chart,index,"judgeLineMoveEvents",limits,options);
            candidate_pose_track(line.rotate_events,input["judgeLineRotateEvents"],chart,index,"judgeLineRotateEvents",limits,options);
            candidate_pose_track(line.opacity_events,input["judgeLineDisappearEvents"],chart,index,"judgeLineDisappearEvents",limits,options);
        } else chart.evaluated_pose_event_count+=line.move_events.size()+line.rotate_events.size()+line.opacity_events.size();
        chart.lines.push_back(std::move(line));
    }
    if (root.contains("blockAreaList")) import_block_areas(root["blockAreaList"], chart, limits);
    return chart;
}
} // namespace rhythm
