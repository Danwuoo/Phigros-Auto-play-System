#include "pas/core.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <sstream>
#define NOMINMAX
#include <windows.h>

namespace pas {

HostClock::HostClock() {
    LARGE_INTEGER value{};
    if (!QueryPerformanceFrequency(&value) || value.QuadPart <= 0)
        throw std::runtime_error("QueryPerformanceFrequency failed");
    frequency_ = value.QuadPart;
}

Nanoseconds HostClock::now_ns() const {
    LARGE_INTEGER value{};
    if (!QueryPerformanceCounter(&value)) throw std::runtime_error("QueryPerformanceCounter failed");
    const auto seconds = value.QuadPart / frequency_;
    const auto ticks = value.QuadPart % frequency_;
    if (seconds > std::numeric_limits<Nanoseconds>::max() / 1'000'000'000)
        throw std::overflow_error("QPC nanoseconds overflow");
    return seconds * 1'000'000'000 + ticks * 1'000'000'000 / frequency_;
}

LatestFrame::LatestFrame(int width, int height, std::size_t slots, const Clock* clock)
    : width_(width), height_(height), clock_(clock) {
    if (width <= 0 || height <= 0 || slots < 2 || slots > 8 ||
        static_cast<std::uint64_t>(width) * height * 3 > 16 * 1024 * 1024)
        throw std::invalid_argument("invalid frame pool geometry or slots");
    bytes_ = static_cast<std::size_t>(width) * height * 3;
    for (std::size_t i = 0; i < slots; ++i) {
        auto frame = std::make_shared<Frame>();
        frame->rgb.resize(bytes_);
        slots_.push_back(std::move(frame));
    }
}

bool LatestFrame::publish(const std::uint8_t* pixels, std::size_t bytes, const Frame& metadata) {
    if (!pixels || bytes != bytes_ || metadata.width != width_ || metadata.height != height_ ||
        metadata.stride != width_ * 3) throw std::invalid_argument("invalid RGB frame");
    std::shared_ptr<Frame> target;
    bool reused_latest = false;
    {
        std::lock_guard lock(mutex_);
        if (closed_) return false;
        if (latest_ && metadata.sequence <= latest_->sequence)
            throw std::invalid_argument("frame sequence did not increase");
        // The pool reference plus the latest reference are the only owners
        // when no consumer holds a lease. Remove the latest publication before
        // selecting its storage; no reader can acquire it after this point.
        for (auto& slot : slots_) {
            if (slot.use_count() == 1) { target = slot; break; }
        }
        if (!target && latest_ && latest_.use_count() == 2) {
            target = latest_;
            if (!consumed_any_ || latest_->sequence > last_consumed_)
                ++counters_.overwritten;
            latest_.reset();
            reused_latest = true;
        }
        if (!target) { ++counters_.pool_drops; return false; }
    }
    std::copy_n(pixels, bytes_, target->rgb.begin());
    // Preserve preallocated storage, and copy only metadata fields.
    target->sequence = metadata.sequence;
    target->epoch = metadata.epoch;
    target->generation = metadata.generation;
    target->geometry_version = metadata.geometry_version;
    target->width = metadata.width;
    target->height = metadata.height;
    target->stride = metadata.stride;
    target->source_rotation = metadata.source_rotation;
    target->capture_complete_ns = metadata.capture_complete_ns;
    target->pixels_ready_ns = metadata.pixels_ready_ns;
    target->published_ns = metadata.published_ns;
    target->source_timestamp_us = metadata.source_timestamp_us;
    target->source_sequence = metadata.source_sequence;
    {
        std::lock_guard lock(mutex_);
        if (closed_) return false;
        if (!reused_latest && latest_ &&
            (!consumed_any_ || latest_->sequence > last_consumed_))
            ++counters_.overwritten;
        latest_ = std::move(target);
        if (clock_) latest_->published_ns = clock_->now_ns();
        ++counters_.published;
    }
    ready_.notify_all();
    return true;
}

std::shared_ptr<const Frame> LatestFrame::read_after(std::uint64_t sequence, Nanoseconds timeout_ns) {
    std::unique_lock lock(mutex_);
    const auto predicate = [&] { return closed_ || (latest_ && latest_->sequence > sequence); };
    if (timeout_ns < 0) ready_.wait(lock, predicate);
    else ready_.wait_for(lock, std::chrono::nanoseconds(timeout_ns), predicate);
    if (!latest_ || latest_->sequence <= sequence) return {};
    if (consumed_any_ && latest_->sequence > last_consumed_ + 1)
        counters_.consumer_skips += latest_->sequence - last_consumed_ - 1;
    last_consumed_ = latest_->sequence;
    consumed_any_ = true;
    return latest_;
}

std::shared_ptr<const Frame> LatestFrame::peek() const {
    std::lock_guard lock(mutex_);
    return latest_;
}

FrameCounters LatestFrame::counters() const {
    std::lock_guard lock(mutex_);
    return counters_;
}

void LatestFrame::close() {
    { std::lock_guard lock(mutex_); closed_ = true; latest_.reset(); }
    ready_.notify_all();
}

std::optional<Observation> GreenTargetDetector::detect(const Frame& frame) const {
    if (frame.width <= 0 || frame.height <= 0 || frame.stride != frame.width * 3 ||
        frame.rgb.size() != static_cast<std::size_t>(frame.stride) * frame.height)
        throw std::invalid_argument("invalid detector frame");
    std::uint64_t n = 0, sx = 0, sy = 0;
    for (int y = 0; y < frame.height; ++y) {
        const auto* row = frame.rgb.data() + static_cast<std::size_t>(y) * frame.stride;
        for (int x = 0; x < frame.width; ++x) {
            const auto* pixel = row + 3 * x;
            if (pixel[1] > 180 && pixel[0] < 80 && pixel[2] < 80) {
                ++n; sx += x; sy += y;
            }
        }
    }
    if (!n) return {};
    return Observation{frame.sequence, frame.capture_complete_ns, clock_.now_ns(),
                       static_cast<double>(sx) / n, static_cast<double>(sy) / n};
}

void VelocityTracker::clear() { history_.clear(); ++track_id_; }

std::optional<Track> VelocityTracker::update(std::optional<Observation> observation) {
    if (!observation) { if (!history_.empty()) clear(); return {}; }
    if (!history_.empty()) {
        const auto dt = observation->capture_complete_ns - history_.back().capture_complete_ns;
        if (dt <= 0 || dt > 200'000'000) clear();
    }
    history_.push_back(*observation);
    if (history_.size() > 8) history_.erase(history_.begin());
    if (history_.size() < 2) return {};
    std::vector<double> t;
    for (const auto& point : history_)
        t.push_back((point.capture_complete_ns - history_.front().capture_complete_ns) / 1e9);
    const double mt = std::accumulate(t.begin(), t.end(), 0.0) / t.size();
    const double mx = std::accumulate(history_.begin(), history_.end(), 0.0,
        [](double value, const Observation& p) { return value + p.x; }) / history_.size();
    const double my = std::accumulate(history_.begin(), history_.end(), 0.0,
        [](double value, const Observation& p) { return value + p.y; }) / history_.size();
    double denom = 0, vx = 0, vy = 0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        const double centered = t[i] - mt;
        denom += centered * centered;
        vx += centered * (history_[i].x - mx);
        vy += centered * (history_[i].y - my);
    }
    if (denom <= 0) return {};
    vx /= denom; vy /= denom;
    double residual = 0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        const double error = history_[i].y - (my + vy * (t[i] - mt));
        residual += error * error;
    }
    const auto& point = *observation;
    return Track{track_id_, point.frame_sequence, point.capture_complete_ns,
                 point.recognition_complete_ns, point.x, point.y, vx, vy,
                 std::sqrt(residual / t.size())};
}

std::optional<HitIntent> LineCrossingPredictor::predict(const Track& track) const {
    if (track.vy_px_s < 1) return {};
    const double remaining = (line_y_ - track.y) / track.vy_px_s;
    if (remaining < 0 || remaining > 2) return {};
    const auto predicted = track.capture_complete_ns + static_cast<Nanoseconds>(std::llround(remaining * 1e9));
    const auto uncertainty = static_cast<Nanoseconds>(std::llround(std::max(0.5, track.residual_px) / track.vy_px_s * 1e9));
    std::ostringstream basis;
    basis << "frame=" << track.frame_sequence << ";x=" << track.x << ";y=" << track.y
          << ";vx=" << track.vx_px_s << ";vy=" << track.vy_px_s << ";line=" << line_y_
          << ";residual=" << track.residual_px;
    return HitIntent{track.track_id + 1, track.track_id, track.frame_sequence,
                     track.x + track.vx_px_s * remaining, line_y_, predicted,
                     basis.str(), 1.0 / (1.0 + uncertainty / 10'000'000.0), uncertainty};
}

TouchReceipt FakeTouchBackend::inject(const TouchCommand& command) {
    const auto start = clock_.now_ns();
    bool success = true;
    std::string reason;
    const auto found = contacts_.find(command.contact_id);
    if (command.phase == Phase::down) {
        if (found != contacts_.end()) { success = false; reason = "already_down"; }
        else contacts_[command.contact_id] = {command.x, command.y};
    } else if (command.phase == Phase::move) {
        if (found == contacts_.end()) { success = false; reason = "not_down"; }
        else found->second = {command.x, command.y};
    } else if (command.phase == Phase::up) {
        if (found == contacts_.end()) { success = false; reason = "not_down"; }
        else contacts_.erase(found);
    }
    if (success && on_touch) on_touch(command, start);
    TouchReceipt receipt{command, start, clock_.now_ns(), success, reason};
    receipts_.push_back(receipt);
    return receipt;
}

ReleaseReport FakeTouchBackend::release_all() {
    ReleaseReport report;
    report.start_ns = clock_.now_ns();
    for (const auto& [id, _] : contacts_) report.requested_ids.push_back(id);
    contacts_.clear();
    report.return_ns = clock_.now_ns();
    return report;
}

PixelCoordinateMap::PixelCoordinateMap(int fw, int fh, int tw, int th, int rotation)
    : fw_(fw), fh_(fh), tw_(tw), th_(th), rotation_(rotation) {
    if (fw < 2 || fh < 2 || tw < 2 || th < 2 ||
        (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270))
        throw std::invalid_argument("invalid coordinate map");
}

std::array<int, 2> PixelCoordinateMap::map(double x, double y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x >= fw_ || y >= fh_)
        throw std::invalid_argument("point outside frame");
    double u = x / (fw_ - 1), v = y / (fh_ - 1);
    if (rotation_ == 90) { const auto old_u = u; u = 1 - v; v = old_u; }
    else if (rotation_ == 180) { u = 1 - u; v = 1 - v; }
    else if (rotation_ == 270) { const auto old_u = u; u = v; v = 1 - old_u; }
    return {static_cast<int>(std::lround(u * (tw_ - 1))),
            static_cast<int>(std::lround(v * (th_ - 1)))};
}

ContactScheduler::ContactScheduler(const Clock& clock, TouchBackend& backend, int max_contacts,
        int max_plans, int max_steps, Nanoseconds horizon_ns, Nanoseconds evidence_max_age_ns,
        Nanoseconds max_late_ns)
    : clock_(clock), backend_(backend), max_contacts_(max_contacts), max_plans_(max_plans),
      max_steps_(max_steps), horizon_ns_(horizon_ns), evidence_max_age_ns_(evidence_max_age_ns),
      max_late_ns_(max_late_ns) {
    if (max_contacts < 1 || max_contacts > 10 || max_plans < 1 || max_steps < 2 ||
        horizon_ns <= 0 || evidence_max_age_ns <= 0 || max_late_ns < 0)
        throw std::invalid_argument("invalid scheduler limits");
}

bool ContactScheduler::set_gate(std::uint64_t epoch, bool armed, Nanoseconds evidence_ns) {
    if (epoch < epoch_) return false;
    const bool new_epoch = epoch > epoch_;
    if (new_epoch || !armed) {
        cancel("epoch_or_gate_revoked");
        epoch_ = epoch;
        if (new_epoch) accepted_high_watermark_ = 0;
    }
    if (!armed || !fault_.empty()) return !armed;
    if (evidence_ns < gate_evidence_ns_ || evidence_ns > clock_.now_ns()) {
        fail("gate_evidence_invalid"); return false;
    }
    gate_evidence_ns_ = evidence_ns;
    armed_ = true;
    return true;
}

bool ContactScheduler::submit(ContactPlan plan) {
    const auto now = clock_.now_ns();
    if (!fault_.empty() || !armed_ || plan.epoch != epoch_ || plan.intent_id == 0 ||
        plan.basis.empty() || plan.steps.size() < 2 || plan.steps.size() > static_cast<std::size_t>(max_steps_) ||
        plan.steps.front().phase != Phase::down || plan.steps.back().phase != Phase::up ||
        plan.evidence_ns > now || now - plan.evidence_ns >= evidence_max_age_ns_ ||
        plan.steps.front().due_ns > now + horizon_ns_ || plan.valid_until_ns < now)
        return false;
    for (std::size_t i = 0; i < plan.steps.size(); ++i) {
        const auto& step = plan.steps[i];
        if (!std::isfinite(step.x) || !std::isfinite(step.y) ||
            (i > 0 && step.due_ns < plan.steps[i-1].due_ns) ||
            (i > 0 && i + 1 < plan.steps.size() && step.phase != Phase::move)) return false;
    }
    auto old = pending_.find(plan.intent_id);
    if (old != pending_.end()) {
        if (plan.revision <= old->second.plan.revision) return false;
        if (old->second.active && plan.steps.back().due_ns < now) return false;
        old->second.next_step = old->second.active ? 1 : 0;
        old->second.plan = std::move(plan);
        return true;
    }
    // A monotonic birth ID is an unbounded-lifetime dedupe watermark using
    // constant memory. Late revisions and evicted tombstones can never revive.
    if (plan.intent_id <= accepted_high_watermark_) return false;
    if (pending_.size() >= static_cast<std::size_t>(max_plans_)) { fail("plan_capacity"); return false; }
    std::vector<bool> used(static_cast<std::size_t>(max_contacts_));
    for (const auto& [_, p] : pending_) used[static_cast<std::size_t>(p.contact_id)] = true;
    int contact = 0;
    while (contact < max_contacts_ && used[static_cast<std::size_t>(contact)]) ++contact;
    if (contact == max_contacts_) { fail("contact_capacity"); return false; }
    accepted_high_watermark_ = plan.intent_id;
    pending_.emplace(plan.intent_id, Pending{std::move(plan), contact});
    return true;
}

bool ContactScheduler::evidence_expired(Nanoseconds now) const {
    if (!armed_) return false;
    if (now - gate_evidence_ns_ >= evidence_max_age_ns_) return true;
    for (const auto& [_, pending] : pending_)
        if (now - pending.plan.evidence_ns >= evidence_max_age_ns_) return true;
    return false;
}

std::optional<Nanoseconds> ContactScheduler::next_due_ns() const {
    if (!armed_ || !fault_.empty()) return {};
    std::optional<Nanoseconds> due = gate_evidence_ns_ + evidence_max_age_ns_;
    for (const auto& [_, pending] : pending_) {
        due = std::min(*due, pending.plan.evidence_ns + evidence_max_age_ns_);
        due = std::min(*due, pending.plan.steps[pending.next_step].due_ns);
    }
    return due;
}

void ContactScheduler::fail(const std::string& reason) {
    fault_ = reason;
    cancel(reason);
}

void ContactScheduler::cancel(const std::string&) {
    armed_ = false;
    pending_.clear();
    last_release_ = backend_.release_all();
    if (!last_release_.failed_ids.empty() || !last_release_.unknown_ids.empty()) fault_ = "release_failed";
}

void ContactScheduler::request_stop() {
    std::lock_guard lock(dispatch_mutex_);
    stop_requested_ = true;
}

std::vector<TouchReceipt> ContactScheduler::run_due() {
    std::vector<TouchReceipt> receipts;
    while (armed_ && fault_.empty()) {
        {
            std::lock_guard lock(dispatch_mutex_);
            if (stop_requested_) { cancel("supervisor_stop"); break; }
        }
        const auto now = clock_.now_ns();
        // Expiry is checked even when no command is due. The <= boundary is
        // intentional: exactly at the deadline evidence is no longer valid.
        if (evidence_expired(now)) { fail("evidence_expired"); break; }
        auto selected = pending_.end();
        for (auto it = pending_.begin(); it != pending_.end(); ++it)
            if (selected == pending_.end() || it->second.plan.steps[it->second.next_step].due_ns <
                                             selected->second.plan.steps[selected->second.next_step].due_ns)
                selected = it;
        if (selected == pending_.end()) break;
        auto& pending = selected->second;
        const auto step = pending.plan.steps[pending.next_step];
        if (step.due_ns > now) break;
        // Gate and plan are separate. A fresh gate never revives old target
        // pixels, and an active hold remains subject to both expiries.
        if (now - gate_evidence_ns_ >= evidence_max_age_ns_ ||
            now - pending.plan.evidence_ns >= evidence_max_age_ns_ ||
            (step.phase == Phase::down && (now > pending.plan.valid_until_ns || now - step.due_ns > max_late_ns_))) {
            fail("stale_expired_or_late_step"); break;
        }
        TouchCommand command{selected->first, pending.contact_id, step.phase, step.x, step.y,
                             step.due_ns, pending.plan.source_frame_sequence};
        std::optional<TouchReceipt> receipt;
        {
            std::lock_guard lock(dispatch_mutex_);
            if (stop_requested_) { cancel("supervisor_stop"); break; }
            receipt = backend_.inject(command);
        }
        receipts.push_back(*receipt);
        if (!receipt->success) { fail("input_result_unknown"); break; }
        pending.active = step.phase != Phase::up;
        ++pending.next_step;
        if (step.phase == Phase::up) pending_.erase(selected);
    }
    return receipts;
}

std::size_t ContactScheduler::active_count() const {
    return std::count_if(pending_.begin(), pending_.end(),
                         [](const auto& item) { return item.second.active; });
}

} // namespace pas
