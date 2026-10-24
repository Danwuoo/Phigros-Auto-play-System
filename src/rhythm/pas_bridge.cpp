#include "rhythm/pas_bridge.hpp"
#include <algorithm>
#include <atomic>
#include <limits>
#include <numeric>

namespace rhythm {
namespace {
bool same_identity(const Identity& a, const Identity& b) {
    return a.game == b.game && a.song_id == b.song_id && a.difficulty == b.difficulty &&
           a.revision == b.revision && a.chart_sha256 == b.chart_sha256 &&
           a.audio_sha256 == b.audio_sha256 && a.layout_id == b.layout_id;
}
Ns add_time(Ns a, Ns b) {
    if (b < 0 || a > std::numeric_limits<Ns>::max() - b) throw std::overflow_error("bridge timestamp overflow");
    return a + b;
}
pas::Phase phase(Phase p) {
    return p == Phase::down ? pas::Phase::down : p == Phase::move ? pas::Phase::move : pas::Phase::up;
}
} // namespace

PasBridge::PasBridge(Memory memory, const pas::Clock& clock, pas::TouchBackend& backend, PasBridgeConfig config)
    : memory_(std::move(memory)), clock_(clock), backend_(backend), config_(config) {
    (void)validate(memory_, config_.max_contacts);
    std::size_t steps = 0;
    for (const auto& contact : memory_.contacts) {
        if (contact.steps.size() > config_.max_steps_in_song - steps) throw std::invalid_argument("song step budget");
        steps += contact.steps.size();
    }
    if (memory_.contacts.size() > config_.max_contacts_in_song || config_.max_steps < 3 ||
        config_.lookahead_ns <= 0 || config_.lookahead_ns > config_.horizon_ns ||
        config_.commit_window_ns < 0 || config_.commit_window_ns > config_.lookahead_ns ||
        config_.max_uncertainty_ns < 0 || config_.input_lead_ns < 0 ||
        config_.input_width < 2 || config_.input_height < 2)
        throw std::invalid_argument("invalid bridge limits");
    scheduler_ = std::make_unique<pas::ContactScheduler>(clock_, backend_, config_.max_contacts,
        config_.max_plans, config_.max_steps, config_.horizon_ns, config_.evidence_max_age_ns, config_.max_late_ns);
    scheduler_->set_dispatch_guard([this] { return dispatch_valid(); });
    order_.resize(memory_.contacts.size());
    std::iota(order_.begin(), order_.end(), 0);
    std::sort(order_.begin(), order_.end(), [this](std::size_t a, std::size_t b) {
        const auto& x = memory_.contacts[a]; const auto& y = memory_.contacts[b];
        if (x.steps.front().song_ns != y.steps.front().song_ns) return x.steps.front().song_ns < y.steps.front().song_ns;
        return x.id < y.id;
    });
}
PasBridge::~PasBridge() {
    try { if (running_ || scheduler_->active_count()) stop("bridge_destroyed"); } catch (...) {}
}
bool PasBridge::release_ok() const {
    const auto report = scheduler_->last_release();
    return report.failed_ids.empty() && report.unknown_ids.empty();
}
bool PasBridge::observation_valid(const ExecutionObservation& value, Ns now) const {
    return same_identity(value.identity, memory_.identity) && value.sequence != 0 &&
        value.lease.valid(now, value.lease.epoch, value.lease.geometry_version, config_.max_uncertainty_ns) &&
        now - value.lease.observed_host_ns < config_.evidence_max_age_ns &&
        std::isfinite(value.clock.host_per_song) && value.clock.host_per_song > 0;
}
bool PasBridge::begin(const ExecutionObservation& value) {
    const auto now = clock_.now_ns();
    if (running_) { stop("explicit_new_epoch"); }
    if (user_stop_requested_.load() || value.lease.epoch <= epoch_ || !observation_valid(value, now) ||
        !scheduler_->fault().empty() || !release_ok()) return false;
    observation_ = value; epoch_ = value.lease.epoch;
    pending_.clear(); cursor_ = completed_ = 0; next_intent_ = 1;
    reason_.clear(); guard_reason_.clear(); finished_ = false;
    last_tick_ns_ = now;
    // Preserve the original scheduler stop latch: a user stop requires a new
    // bridge/runtime, even if a later observation offers a new epoch.
    running_ = scheduler_->set_context(epoch_, 1, value.lease.geometry_version, true, value.lease.observed_host_ns);
    if (!running_) reason_ = "scheduler_arm_rejected";
    return running_;
}
bool PasBridge::update(const ExecutionObservation& value) {
    if (!running_) return false;
    const auto now = clock_.now_ns();
    if (!observation_valid(observation_, now)) { stop("previous_sync_expired"); return false; }
    if (!observation_valid(value, now)) { stop("invalid_sync_or_identity"); return false; }
    if (value.lease.epoch != epoch_ || value.lease.geometry_version != observation_.lease.geometry_version) {
        stop("epoch_or_geometry_changed"); return false;
    }
    if (value.sequence < observation_.sequence || value.lease.observed_host_ns < observation_.lease.observed_host_ns) {
        stop("observation_regressed"); return false;
    }
    if (value.sequence == observation_.sequence) {
        // Same observation may be delivered more than once. It cannot extend a
        // lease or quietly change the clock under the same evidence identity.
        if (value.lease.observed_host_ns != observation_.lease.observed_host_ns ||
            value.lease.expires_host_ns != observation_.lease.expires_host_ns ||
            value.lease.uncertainty_ns != observation_.lease.uncertainty_ns ||
            value.clock.song_anchor_ns != observation_.clock.song_anchor_ns ||
            value.clock.host_anchor_ns != observation_.clock.host_anchor_ns ||
            value.clock.host_per_song != observation_.clock.host_per_song) {
            stop("observation_reused_with_new_data"); return false;
        }
        return true;
    }
    observation_ = value;
    return true;
}
void PasBridge::stop(const std::string& reason) {
    running_ = false; finished_ = false; reason_ = reason;
    scheduler_->cancel(reason);
    pending_.clear();
    if (!release_ok()) reason_ = "release_failed";
}
void PasBridge::request_stop() { user_stop_requested_.store(true); scheduler_->request_stop(); }

std::optional<Ns> PasBridge::next_wake_ns() const {
    if (!running_) return std::nullopt;
    const auto now = clock_.now_ns();
    Ns next = std::min(observation_.lease.expires_host_ns,
                      add_time(observation_.lease.observed_host_ns, config_.evidence_max_age_ns));
    if (auto due = scheduler_->next_due_ns()) next = std::min(next, *due);
    for (const auto& [intent, state] : pending_) {
        (void)intent;
        if (state.coverage_until) next = std::min(next, *state.coverage_until - 1'000'000);
    }
    if (cursor_ < order_.size()) {
        const auto due = mapped(memory_.contacts[order_[cursor_]].steps.front()).due_ns;
        next = std::min(next, due - config_.lookahead_ns);
    }
    return std::max(now, next);
}

pas::ContactStep PasBridge::mapped(const Step& step) const {
    const auto pixel = pixel_point(step.point, config_.input_width, config_.input_height);
    return {phase(step.phase), static_cast<double>(pixel.first), static_cast<double>(pixel.second),
            observation_.clock.dispatch_at(step.song_ns, config_.input_lead_ns)};
}
bool PasBridge::revise(std::uint64_t intent, Pending& state, std::uint64_t executed, Ns now) {
    const auto& contact = memory_.contacts[state.contact_index];
    if (executed >= contact.steps.size()) { stop("invalid_scheduler_progress"); return false; }
    pas::ContactPlan plan;
    plan.epoch = epoch_; plan.generation = 1; plan.geometry_version = observation_.lease.geometry_version;
    plan.intent_id = intent; plan.note_id = contact.id;
    plan.revision = state.submitted.revision + 1;
    plan.evidence_ns = observation_.lease.observed_host_ns;
    plan.valid_until_ns = observation_.lease.expires_host_ns;
    plan.source_frame_sequence = observation_.sequence;
    plan.basis = "chart-memory/actual-sync-observation";
    plan.predicted_down_ns = observation_.clock.dispatch_at(contact.steps.front().song_ns, config_.input_lead_ns);
    const std::size_t begin = executed ? static_cast<std::size_t>(executed - 1) : 0;
    plan.prefix_offset = begin;
    const auto frozen_until = add_time(now, config_.commit_window_ns);
    auto make_step = [&](std::size_t ordinal) {
        // The old final UP may stand after omitted moves, so it must not be
        // mistaken for the next source ordinal during prefix matching.
        const auto old_first = state.submitted.prefix_offset;
        const auto old_count = state.submitted.steps.size();
        const bool old_tail = ordinal + 1 == contact.steps.size() && old_count;
        const bool old_regular = ordinal >= old_first && old_count > 1 && ordinal - old_first < old_count - 1;
        if (old_tail || old_regular) {
            const auto& old = old_tail ? state.submitted.steps.back() : state.submitted.steps[ordinal - old_first];
            if (ordinal < executed || old.due_ns <= frozen_until) return old;
        }
        if (ordinal < executed) throw std::logic_error("executed prefix missing");
        return mapped(contact.steps[ordinal]);
    };
    const auto final_index = contact.steps.size() - 1;
    const auto exclusive = std::min(final_index, begin + static_cast<std::size_t>(config_.max_steps - 1));
    for (std::size_t i = begin; i < exclusive; ++i) plan.steps.push_back(make_step(i));
    plan.steps.push_back(make_step(final_index)); // Real UP; never a fabricated short-hold release.
    state.coverage_until = exclusive < final_index ? std::optional<Ns>(mapped(contact.steps[exclusive]).due_ns) : std::nullopt;
    for (std::size_t i = 1; i < plan.steps.size(); ++i) {
        if (plan.steps[i].due_ns <= plan.steps[i - 1].due_ns) {
            stop("clock_revision_reorders_steps"); return false;
        }
    }
    if (!scheduler_->submit(plan)) { stop("scheduler_submit_rejected"); return false; }
    state.submitted = std::move(plan);
    return true;
}

void PasBridge::refresh_plans(Ns now) {
    for (auto& [intent, pending] : pending_) {
        const auto executed = scheduler_->executed_steps(intent);
        if (!executed) { stop("unexpected_contact_disappearance"); return; }
        // The preserved scheduler rejects an active revision whose real UP is
        // already in the past. A slightly late owner tick must dispatch that
        // existing, still-valid tail instead. The guard above already checks
        // coverage and lateness; never move UP to now or fabricate new evidence.
        if (*executed && !pending.coverage_until && pending.submitted.steps.back().due_ns < now)
            continue;
        if (!revise(intent, pending, *executed, now)) return;
    }
    while (running_ && cursor_ < order_.size()) {
        const auto index = order_[cursor_];
        const auto due = mapped(memory_.contacts[index].steps.front()).due_ns;
        if (due > add_time(now, config_.lookahead_ns)) break;
        if (now > due && now - due > config_.max_late_ns) { stop("unadmitted_contact_too_late"); return; }
        if (pending_.size() >= static_cast<std::size_t>(config_.max_plans)) { stop("bridge_plan_capacity"); return; }
        const auto intent = next_intent_++;
        Pending pending; pending.contact_index = index;
        if (!revise(intent, pending, 0, now)) return;
        pending_.emplace(intent, std::move(pending));
        ++cursor_;
    }
}
bool PasBridge::dispatch_valid() {
    const auto now = clock_.now_ns();
    if (user_stop_requested_.load()) { guard_reason_ = "user_stop"; return false; }
    if (!running_ || !observation_valid(observation_, now)) { guard_reason_ = "sync_expired"; return false; }
    if (now < last_tick_ns_) { guard_reason_ = "host_clock_regressed"; return false; }
    for (const auto& [intent, state] : pending_) {
        if (state.coverage_until && now >= *state.coverage_until) { guard_reason_ = "trajectory_coverage_expired"; return false; }
        const auto executed = scheduler_->executed_steps(intent);
        if (!executed) continue; // A successful UP can erase it within run_due.
        const auto local = *executed - state.submitted.prefix_offset;
        if (local >= state.submitted.steps.size()) { guard_reason_ = "invalid_step_cursor"; return false; }
        const auto due = state.submitted.steps[static_cast<std::size_t>(local)].due_ns;
        if (now > due && now - due > config_.max_late_ns) { guard_reason_ = "contact_step_too_late"; return false; }
    }
    return true;
}
BridgeTick PasBridge::tick() {
    BridgeTick result;
    try {
        const auto now = clock_.now_ns();
        if (running_ && user_stop_requested_.load()) stop("user_stop");
        if (running_ && now < last_tick_ns_) stop("host_clock_regressed");
        if (running_ && !observation_valid(observation_, now)) stop("sync_expired");
        // Check the previously admitted trajectory before replacing its window.
        // A late owner tick must not erase an already missed coverage deadline.
        if (running_ && !dispatch_valid()) stop(guard_reason_);
        last_tick_ns_ = now;
        if (running_ && !scheduler_->set_context(epoch_, 1, observation_.lease.geometry_version,
                                               true, observation_.lease.observed_host_ns)) stop("gate_rejected");
        if (running_) refresh_plans(now);
        if (running_) {
            result.receipts = scheduler_->run_due();
            result.notices = scheduler_->take_notices();
            for (const auto& receipt : result.receipts) {
                if (receipt.success && receipt.command.phase == pas::Phase::up) {
                    pending_.erase(receipt.command.intent_id); ++completed_;
                }
            }
            if (!scheduler_->armed() || !scheduler_->fault().empty()) {
                running_ = false;
                reason_ = !scheduler_->fault().empty() ? scheduler_->fault() :
                          !guard_reason_.empty() ? guard_reason_ : "scheduler_stopped";
                pending_.clear();
            } else if (!result.notices.empty()) stop(result.notices.front().reason);
            if (running_ && cursor_ == order_.size() && pending_.empty()) {
                scheduler_->cancel("song_plan_finished"); running_ = false;
                finished_ = release_ok(); reason_ = finished_ ? "finished" : "release_failed";
            }
        }
    } catch (const std::exception& error) { stop(std::string("bridge_error: ") + error.what()); }
    result.running = running_; result.finished = finished_; result.reason = reason_;
    result.pending = scheduler_->pending_count(); result.active = scheduler_->active_count();
    result.completed_contacts = completed_; result.release = scheduler_->last_release();
    return result;
}
} // namespace rhythm
