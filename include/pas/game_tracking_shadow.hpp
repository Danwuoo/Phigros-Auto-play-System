#pragma once
#include "pas/game_tracking.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace pas {
// One independent value slot. Never stores a Frame, backend or owner reference.
class TrackingShadow final {
public:
 explicit TrackingShadow(const std::string& method);
 ~TrackingShadow();
 void submit(const CandidateBatch&);
 void stop();
 nlohmann::json stats() const;
private:
 const std::string method_;
 mutable std::mutex mutex_;
 std::condition_variable cv_;
 std::optional<CandidateBatch> slot_;
 bool stopping_=false;
 std::uint64_t submitted_=0,skipped_=0,processed_=0,predictions_=0,action_evidence_=0;
 std::string fault_;
 std::vector<double> updates_,residency_;
 std::jthread worker_;
};
class TrackingBankRecorder final {
public:
 explicit TrackingBankRecorder(std::size_t budget_bytes=16*1024*1024);
 void append(const CandidateBatch&);
 nlohmann::json stats() const;
 void save(const std::filesystem::path&,const nlohmann::json& provenance) const;
private:
 std::vector<CandidateBatch> batches_;
 std::size_t budget_,charged_=0,dropped_=0;
};
} // namespace pas
