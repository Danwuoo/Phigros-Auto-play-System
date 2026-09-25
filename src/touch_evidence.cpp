#include "pas/touch_evidence.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace pas {
namespace {
std::uint32_t pixel24(const Frame& frame, int x, int y) {
    if (frame.stride != frame.width * 3 || x < 0 || x >= frame.width ||
        y < 0 || y >= frame.height ||
        frame.rgb.size() != static_cast<std::size_t>(frame.stride) * frame.height)
        throw std::invalid_argument("touch evidence frame geometry invalid");
    const auto position = static_cast<std::size_t>(y) * frame.stride + x * 3;
    return (static_cast<std::uint32_t>(frame.rgb[position]) << 16) |
           (static_cast<std::uint32_t>(frame.rgb[position + 1]) << 8) |
           frame.rgb[position + 2];
}
}

TouchEvidence decode_touch_evidence(const Frame& frame, std::uint32_t since_sequence) {
    TouchEvidence result;
    if (frame.width < 320 || frame.height < 660) {
        result.error = "touch Fixture geometry too small"; return result;
    }
    const auto header = [&](int field) { return pixel24(frame, 24 + field * 30, 50); };
    if (header(0) != 0x504153 || header(1) != 2) {
        result.error = "touch Fixture magic/schema mismatch"; return result;
    }
    result.sequence = header(2);
    result.overflow_total = header(3);
    result.active = header(4);
    result.downs = header(5); result.ups = header(6);
    result.moves = header(7); result.cancels = header(8);
    const auto delta = (result.sequence - since_sequence) & 0xFFFFFF;
    if (delta > 64) { result.error = "touch evidence ring overflow or missing segment"; return result; }
    for (std::uint32_t i = 0; i < delta; ++i) {
        const auto sequence = (since_sequence + i + 1) & 0xFFFFFF;
        const auto index = (sequence - 1) % 64;
        const int y = 143 + static_cast<int>(index) * 8;
        const auto field = [&](int part) { return pixel24(frame, 32 + part * 30, y); };
        const auto stored_seq = field(0);
        const auto meta = field(1);
        const auto x = field(2), yy = field(3), checksum = field(4);
        if (stored_seq != sequence || checksum != ((stored_seq ^ meta ^ x ^ yy ^ 0xA5C37E) & 0xFFFFFF)) {
            result.error = "touch evidence checksum or sequence mismatch"; return result;
        }
        result.samples.push_back({sequence, static_cast<int>((meta >> 16) & 255),
                                  static_cast<int>(meta & 0xFFFF), static_cast<int>(x), static_cast<int>(yy)});
    }
    result.complete = true;
    return result;
}

PathCheck verify_touch_path(const TouchEvidence& evidence,
                            const std::vector<ExpectedPointer>& expected,
                            int tolerance_px, int final_active,
                            const std::vector<int>& stationary_ids) {
    if (!evidence.complete) return {false, evidence.error.empty() ? "evidence incomplete" : evidence.error};
    if (tolerance_px < 0 || evidence.active != static_cast<std::uint32_t>(final_active))
        return {false, "invalid tolerance or final active count"};
    if (expected.empty()) return {false, "empty expected path"};
    std::map<int, std::pair<int, int>> stationary_anchor;
    std::map<int, std::pair<int, int>> last_checkpoint;
    std::map<int, double> segment_progress;
    std::size_t matched = 0;
    for (const auto& sample : evidence.samples) {
        if (sample.phase < 1 || sample.phase > 4) return {false, "unknown Android pointer phase"};
        if (std::find(stationary_ids.begin(), stationary_ids.end(), sample.android_id) != stationary_ids.end()) {
            auto [it, inserted] = stationary_anchor.emplace(sample.android_id, std::pair{sample.x, sample.y});
            if (!inserted && (std::abs(sample.x - it->second.first) > tolerance_px ||
                              std::abs(sample.y - it->second.second) > tolerance_px))
                return {false, "stationary pointer drifted"};
        }
        if (matched < expected.size()) {
            const auto& target = expected[matched];
            if (sample.phase == target.phase && sample.android_id == target.android_id &&
                std::abs(sample.x - target.x) <= tolerance_px &&
                std::abs(sample.y - target.y) <= tolerance_px) {
                last_checkpoint[sample.android_id] = {target.x, target.y};
                segment_progress[sample.android_id] = 0;
                ++matched;
                continue;
            }
            // Android can resample a fast MOVE after a requested waypoint. Keep
            // the strict perpendicular tolerance and require a visible point
            // inside the immediately following segment before skipping one
            // unobserved waypoint. Down, up, and pointer identity stay exact.
            if (sample.phase == 2 && target.phase == 2 &&
                sample.android_id == target.android_id && matched + 1 < expected.size()) {
                const auto& following = expected[matched + 1];
                if (following.phase == 2 && following.android_id == sample.android_id &&
                    last_checkpoint.contains(sample.android_id)) {
                    const double dx = following.x - target.x;
                    const double dy = following.y - target.y;
                    const double length2 = dx * dx + dy * dy;
                    if (length2 > 0) {
                        const double px = sample.x - target.x;
                        const double py = sample.y - target.y;
                        const double progress = (px * dx + py * dy) / length2;
                        const double perpendicular = std::abs(px * dy - py * dx) / std::sqrt(length2);
                        const double slack = tolerance_px / std::sqrt(length2);
                        if (progress > slack && progress < 1 - slack &&
                            perpendicular <= tolerance_px) {
                            last_checkpoint[sample.android_id] = {target.x, target.y};
                            segment_progress[sample.android_id] = progress;
                            ++matched;
                            continue;
                        }
                    }
                }
            }
        }
        // Extra MOVE samples may be interpolation, but unexpected down/up,
        // cancel, or a move for the wrong ID cannot be hidden by the match.
        if (sample.phase != 2) return {false, "unexpected pointer phase or position"};
        if (matched >= expected.size()) return {false, "extra move after expected path"};
        const auto& next = expected[matched];
        if (sample.android_id != next.android_id &&
            std::find(stationary_ids.begin(), stationary_ids.end(), sample.android_id) == stationary_ids.end())
            return {false, "pointer identity mismatch"};
        if (sample.android_id == next.android_id) {
            const auto previous = last_checkpoint.find(sample.android_id);
            if (previous == last_checkpoint.end()) return {false, "move before pointer down checkpoint"};
            const double dx = next.x - previous->second.first;
            const double dy = next.y - previous->second.second;
            const double length2 = dx * dx + dy * dy;
            const double px = sample.x - previous->second.first;
            const double py = sample.y - previous->second.second;
            if (length2 == 0) {
                if (std::hypot(px, py) > tolerance_px) return {false, "zero-length segment drift"};
            } else {
                const double progress = (px * dx + py * dy) / length2;
                const double perpendicular = std::abs(px * dy - py * dx) / std::sqrt(length2);
                const double slack = tolerance_px / std::sqrt(length2);
                if (progress < segment_progress[sample.android_id] - slack ||
                    progress < -slack || progress > 1 + slack || perpendicular > tolerance_px)
                    return {false, "off-path or reversed intermediate move"};
                segment_progress[sample.android_id] = std::max(segment_progress[sample.android_id], progress);
            }
        }
    }
    if (matched != expected.size()) return {false, "missing pointer path segment"};
    return {true, "fixture_pixels_verified"};
}

} // namespace pas
