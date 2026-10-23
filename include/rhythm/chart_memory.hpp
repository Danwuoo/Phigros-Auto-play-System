#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rhythm {
using Ns = std::int64_t;
enum class Game { phigros, project_sekai };
enum class Phase { down, move, up };

// Resolved-contact memory, NOT a parser for either game's native charts.
// Native adapters must preserve note semantics until geometry is resolved.
struct Identity {
    Game game = Game::phigros;
    std::string song_id, difficulty, revision;
    std::string chart_sha256, audio_sha256, layout_id;
};
struct Point { double x = 0, y = 0; }; // Normalized input surface, [0, 1].
struct Step { Ns song_ns = 0; Phase phase = Phase::down; Point point; };
struct Contact { std::uint64_t id = 0; std::vector<Step> steps; };
struct Memory { Identity identity; std::vector<Contact> contacts; };

inline bool sha256_text(const std::string& text) {
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
inline void validate_point(Point p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
        p.x < 0 || p.x > 1 || p.y < 0 || p.y > 1)
        throw std::invalid_argument("point must be finite normalized coordinates");
}
inline std::pair<int, int> pixel_point(Point p, int width, int height) {
    validate_point(p);
    if (width < 2 || height < 2) throw std::invalid_argument("invalid surface");
    return {static_cast<int>(std::lround(p.x * (width - 1))),
            static_cast<int>(std::lround(p.y * (height - 1)))};
}

// Validates syntax and declared capacity, not chart/audio hash authenticity,
// device support, timing accuracy, or whether a game will accept these inputs.
inline int validate(const Memory& memory, int max_contacts) {
    const auto& i = memory.identity;
    if ((i.game != Game::phigros && i.game != Game::project_sekai) ||
        i.song_id.empty() || i.difficulty.empty() || i.revision.empty() ||
        i.layout_id.empty() || !sha256_text(i.chart_sha256) || !sha256_text(i.audio_sha256))
        throw std::invalid_argument("incomplete chart/audio/layout identity");
    if (max_contacts < 1 || max_contacts > 10 || memory.contacts.empty())
        throw std::invalid_argument("invalid contact capacity or empty memory");
    std::set<std::uint64_t> ids;
    std::vector<std::pair<Ns, int>> edges;
    for (const auto& c : memory.contacts) {
        if (!c.id || !ids.insert(c.id).second || c.steps.size() < 2 ||
            c.steps.front().phase != Phase::down || c.steps.back().phase != Phase::up)
            throw std::invalid_argument("invalid contact lifecycle or duplicate id");
        for (std::size_t n = 0; n < c.steps.size(); ++n) {
            const auto& s = c.steps[n];
            validate_point(s.point);
            if (s.song_ns < 0 || (n && s.song_ns <= c.steps[n - 1].song_ns) ||
                (n && n + 1 < c.steps.size() && s.phase != Phase::move))
                throw std::invalid_argument("invalid step ordering or phase");
        }
        edges.emplace_back(c.steps.front().song_ns, 1);
        edges.emplace_back(c.steps.back().song_ns, -1);
    }
    // Endpoints are half-open: release before down at the same timestamp.
    std::sort(edges.begin(), edges.end());
    int active = 0, peak = 0;
    for (const auto& edge : edges) {
        active += edge.second;
        peak = std::max(peak, active);
    }
    if (peak > max_contacts) throw std::invalid_argument("contact capacity exceeded");
    return peak;
}

// This is a supplied affine mapping, NOT an audio synchronizer or drift estimator.
// Song offsets must already be normalized by the importer, exactly once.
struct ClockMap {
    Ns song_anchor_ns = 0;
    Ns host_anchor_ns = 0;
    long double host_per_song = 1;

    Ns to_host(Ns song_ns) const {
        if (!std::isfinite(host_per_song) || host_per_song <= 0)
            throw std::invalid_argument("invalid clock rate");
        const long double mapped = static_cast<long double>(host_anchor_ns) +
            (static_cast<long double>(song_ns) - static_cast<long double>(song_anchor_ns)) *
            host_per_song;
        const long double rounded = std::round(mapped);
        // Conservative upper bound also works where long double == double.
        if (!std::isfinite(rounded) ||
            rounded < static_cast<long double>(std::numeric_limits<Ns>::min()) ||
            rounded >= static_cast<long double>(std::numeric_limits<Ns>::max()))
            throw std::overflow_error("mapped timestamp overflow");
        return static_cast<Ns>(rounded);
    }
    Ns dispatch_at(Ns song_ns, Ns input_lead_ns) const {
        if (input_lead_ns < 0) throw std::invalid_argument("negative input lead");
        const Ns hit = to_host(song_ns);
        if (hit < 0 || input_lead_ns > hit)
            throw std::invalid_argument("dispatch precedes host clock origin");
        return hit - input_lead_ns;
    }
};

// A chart stays reusable; permission to dispatch expires independently.
// observed_host_ns must come from a real observation, never the planning loop.
struct SyncLease {
    bool locked = false;
    std::uint64_t epoch = 0, geometry_version = 0;
    Ns observed_host_ns = 0, expires_host_ns = 0, uncertainty_ns = 0;
    bool valid(Ns now, std::uint64_t expected_epoch, std::uint64_t expected_geometry,
               Ns max_uncertainty_ns) const {
        return locked && epoch != 0 && geometry_version != 0 &&
            epoch == expected_epoch && geometry_version == expected_geometry &&
            observed_host_ns >= 0 && now >= observed_host_ns && now < expires_host_ns &&
            expires_host_ns > observed_host_ns && uncertainty_ns >= 0 &&
            max_uncertainty_ns >= 0 && uncertainty_ns <= max_uncertainty_ns;
    }
};

struct Event {
    Ns host_ns;
    std::uint64_t contact_id;
    Phase phase;
    Point point;
};
inline int phase_order(Phase p) {
    return p == Phase::up ? 0 : (p == Phase::move ? 1 : 2);
}
// Offline mapping only: deliberately no device, scheduler, or automatic arming.
inline std::vector<Event> map_for_dry_run(const Memory& memory, const ClockMap& clock,
                                        Ns input_lead_ns, int max_contacts) {
    (void)validate(memory, max_contacts);
    std::vector<Event> events;
    for (const auto& c : memory.contacts) {
        Ns previous = -1;
        for (const auto& s : c.steps) {
            const Ns due = clock.dispatch_at(s.song_ns, input_lead_ns);
            if (due <= previous) throw std::invalid_argument("clock collapses contact steps");
            previous = due;
            events.push_back({due, c.id, s.phase, s.point});
        }
    }
    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        if (a.host_ns != b.host_ns) return a.host_ns < b.host_ns;
        if (a.phase != b.phase) return phase_order(a.phase) < phase_order(b.phase);
        return a.contact_id < b.contact_id;
    });
    return events;
}
} // namespace rhythm
