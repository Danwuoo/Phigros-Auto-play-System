#pragma once
// Offline adapter only. No capture, device, network or injection implementation.
#include "pas/game.hpp"
#include "bvi.hpp"
#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace pas::bvi_offline {
struct Prepared {
    std::array<bvi::Query,128> queries{};
    std::array<bvi::Line,16> lines{};
    std::array<std::uint64_t,128> note_ids{},candidate_ids{};
    std::array<std::size_t,128> target_indices{};
    std::size_t count=0,line_count=0,body_patch_denied=0,unsupported=0,unmapped=0;
    bool valid=false;
    std::string_view reason="unprepared";
};
Prepared prepare(const Frame&,const CandidateBatch&,const DecisionSnapshot&,Nanoseconds);

struct BackendEvent {
    bool release=false;
    TouchCommand issued{};
    TouchReceipt receipt{};
    ReleaseReport report;
};
// Each frame drains <=64 events. Five physical slots; no unbounded receipt log.
class ReplayBackend final : public TouchBackend {
public:
    explicit ReplayBackend(const Clock& clock):clock_(clock) {}
    TouchReceipt inject(const TouchCommand&) override;
    ReleaseReport release_all() override;
    std::vector<BackendEvent> take_events();
    std::size_t active_count() const;
    bool overflow() const {return overflow_;}
    enum class Failure {none,unknown_down,unknown_move,unknown_up,wrong_contact,release_unknown};
    Failure failure=Failure::none; // deterministic offline failure controls
    std::uint64_t downs=0,moves=0,ups=0,failed=0;
private:
    const Clock& clock_;
    std::array<bool,5> active_{};
    std::array<BackendEvent,64> events_{};
    std::size_t event_count_=0;
    bool overflow_=false;
};
enum class Execution {pending,active,completed,unknown,cancelled};
struct Attachment {
    std::uint64_t note=0,intent=0,revision=0,prefix_offset=0;
    std::optional<std::uint64_t> cursor;
    int contact=-1;
    Execution state=Execution::pending;
    Nanoseconds last_contact=0,deadline=0;
};
// Cursor stays uint64_t/optional. The v3 Guard gets only a semantic phase token.
int phase_prefix(Execution,std::optional<std::uint64_t>);
class ExecutionLedger {
public:
    bool observe(std::span<const ContactPlan>,std::span<const BackendEvent>,const ContactScheduler&);
    const Attachment* find(std::uint64_t note) const;
    bvi::Guard guard(std::uint64_t note,int query,Nanoseconds now,Nanoseconds gate_deadline,
                     Nanoseconds plan_deadline,int free_contacts) const;
    std::size_t size() const {return count_;}
    std::size_t reservations() const;
    bool valid() const {return valid_;}
    std::string_view reason() const {return reason_;}
    bool context_matches(const SceneContext& c) const {
        return !context_||(context_->epoch==c.epoch&&context_->generation==c.generation&&context_->geometry==c.geometry);
    }
private:
    std::array<Attachment,128> attachments_{};
    std::size_t count_=0;
    bool valid_=true;
    std::string_view reason_="verified";
    std::optional<SceneContext> context_;
};
struct Evaluation {
    Prepared input;
    bvi::Observation observation;
    DecisionSnapshot filtered;
    std::size_t allowed=0;
};
Evaluation evaluate(bvi::Candidate&,const Frame&,const CandidateBatch&,
                    const DecisionSnapshot&,const ExecutionLedger&,Nanoseconds now);
} // namespace pas::bvi_offline
