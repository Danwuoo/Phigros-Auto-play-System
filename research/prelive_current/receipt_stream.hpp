#pragma once
#include "pas/core.hpp"
#include <array>
#include <span>
#include <nlohmann/json.hpp>

namespace pas::current_execution {
struct BackendEvent {
    bool release=false;
    TouchCommand issued{};
    TouchReceipt receipt{};
    ReleaseReport report;
    // A repeated uncertain release is reported, but never sent again.
    bool dispatched=true;
};
// Owner-thread only. Diagnostics never grant action permission. Bounds are
// 64 events, five physical contacts, 512 receipt-reason bytes and five IDs/list.
class ReceiptStream final {
public:
    static constexpr std::size_t capacity=64;
    bool can_inject();
    void record_injection(const TouchCommand&,const TouchReceipt&);
    void record_release(const ReleaseReport&,bool dispatched=true);
    std::vector<BackendEvent> take_events();
    bool overflow() const {return overflow_;}
    std::size_t active_count() const;
    const std::optional<ReleaseReport>& first_uncertain_release() const {return uncertain_release_;}
    bool healthy() const {return !overflow_&&!uncertain_release_&&!command_uncertain_;}
    // A release uncertainty/overflow is permanent for this process instance.
    bool reset_after_verified_round();
private:
    std::array<BackendEvent,capacity> events_{};
    std::array<bool,5> active_{};
    std::size_t count_=0;
    bool overflow_=false,command_uncertain_=false;
    std::optional<ReleaseReport> uncertain_release_;
};
// Exactly one underlying backend. Captures implicit scheduler releases as well
// as issued/returned commands; a full diagnostic buffer never prevents release.
class RecordingBackend final : public TouchBackend {
public:
    RecordingBackend(TouchBackend& backend,ReceiptStream& stream,const Clock& clock)
      :backend_(backend),stream_(stream),clock_(clock) {}
    TouchReceipt inject(const TouchCommand&) override;
    ReleaseReport release_all() override;
private:
    TouchBackend& backend_;ReceiptStream& stream_;const Clock& clock_;
};
nlohmann::json backend_event_json(const BackendEvent&);
}
