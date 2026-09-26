#pragma once
#include "pas/core.hpp"
#include <deque>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace pas {

enum class GameUi { unknown, menu, loading, playing, paused, result };
enum class NoteKind { tap, hold, drag, flick, ambiguous };
const char* name(GameUi value);
const char* name(NoteKind value);
struct Vec2 { double x = 0, y = 0; };
struct SceneContext {
    std::uint64_t epoch = 0, generation = 0, geometry = 0, frame = 0;
    Nanoseconds capture_ns = 0;
    int width = 0, height = 0, rotation = 0;
    bool operator==(const SceneContext&) const = default;
};
struct LineCandidate {
    Vec2 center, tangent;
    double length = 0, thickness = 0, confidence = 0;
};
struct NoteCandidate {
    Vec2 center;
    NoteKind kind = NoteKind::ambiguous;
    double width = 0, height = 0, confidence = 0;
};
struct GameTarget {
    std::uint64_t note_id = 0, revision = 0;
    NoteCandidate note;
    Nanoseconds evidence_ns = 0, expires_ns = 0;
    std::optional<Nanoseconds> crossing_ns;
    Nanoseconds uncertainty_ns = 0;
    Vec2 hit;
    double distance = 0, velocity = 0, residual = 0;
    std::string reason;
    int samples = 0;
};
struct DecisionSnapshot {
    std::uint64_t sequence = 0;
    SceneContext context;
    GameUi ui = GameUi::unknown;
    bool playing_gate = false, capacity_valid = true;
    Nanoseconds recognition_start_ns = 0, recognition_end_ns = 0;
    std::string ui_basis;
    std::optional<Vec2> play_button;
    std::vector<LineCandidate> lines;
    std::vector<GameTarget> targets;
};

// All thresholds are development hypotheses. The observer supplies pixel
// evidence without injecting input. Gameplay remains dry in this runtime;
// only explicit auto-start consumes MENU evidence for one UI PLAY attempt.
// History has <=128 tracks, <=8 points each, no retained frames.
class GameObserver final {
public:
    explicit GameObserver(const Clock& clock) : clock_(clock) {}
    DecisionSnapshot process(const Frame& frame);
    void reset();
private:
    struct Point { Nanoseconds t; Vec2 p; LineCandidate line; };
    struct History {
        std::uint64_t id = 0, revision = 0;
        NoteKind kind;
        Vec2 last;
        Nanoseconds observed = 0;
        std::deque<Point> points;
    };
    const Clock& clock_;
    SceneContext previous_;
    std::vector<History> tracks_;
    std::uint64_t next_id_ = 0, sequence_ = 0;
    int playing_confirmations_ = 0;
    int menu_confirmations_ = 0;
};

// Single-thread owner. Note identity is distinct from monotonically assigned
// submission identity; snapshots are complete sets so skipped snapshots do
// not lose cancellations. No real backend is constructed by observe runtime.
class GamePlanOwner final {
public:
    GamePlanOwner(const Clock& clock, TouchBackend& backend, int contacts = 2);
    std::vector<TouchReceipt> accept(const DecisionSnapshot& snapshot);
    std::vector<TouchReceipt> poll();
    void stop();
    ContactScheduler& scheduler() { return scheduler_; }
    const std::string& last_rejection() const { return last_rejection_; }
private:
    struct Identity { std::uint64_t intent, revision; Nanoseconds expires; bool submitted = false; };
    const Clock& clock_;
    ContactScheduler scheduler_;
    std::map<std::uint64_t, Identity> identities_;
    std::uint64_t next_intent_ = 0, last_snapshot_ = 0, epoch_ = 0;
    SceneContext context_;
    std::string last_rejection_;
};

class PlayButtonPlanner final {
public:
    std::optional<ContactPlan> take(const DecisionSnapshot& scene, Nanoseconds now);
private:
    bool attempted_ = false;
};

nlohmann::json decision_json(const DecisionSnapshot& scene);
nlohmann::json analyze_game_jsonl(const std::filesystem::path& path);
void draw_game_overlay(Frame& frame, const DecisionSnapshot& scene);
} // namespace pas
