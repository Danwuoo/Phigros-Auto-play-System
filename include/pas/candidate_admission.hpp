#pragma once
#include <stdexcept>
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
namespace pas {
struct CandidateAdmissionRequest final {
    std::filesystem::path workspace,input_path,config_path,capability_path;
    std::string input_sha256;
};
// The fixture result verifies the same typed receipt/current-preflight joins.
// It never contains an admission, a device provider or an enabling conversion.
class CandidateColdReceiptValidation final {
public:
    const nlohmann::json& report() const noexcept {return report_;}
private:
    explicit CandidateColdReceiptValidation(nlohmann::json report):report_(std::move(report)){}
    nlohmann::json report_;
    friend CandidateColdReceiptValidation validate_synthetic_candidate_receipts(
        const nlohmann::json&,const nlohmann::json&);
};
CandidateColdReceiptValidation validate_synthetic_candidate_receipts(
    const nlohmann::json& documents,const nlohmann::json& current_context);
// Read-only: validates actual pinned files and cold receipts; does no discovery,
// capture, ADB, process launch, touch or persistence and cannot issue admission.
nlohmann::json inspect_candidate_receipts(const CandidateAdmissionRequest&);
// The only factory additionally obtains an actual current read-only preflight.
// Missing/invalid receipts reject before that call. The resulting admission is
// single-use, bound to this executable/config/capability, and score-only.
class CandidateLiveAdmission;
CandidateLiveAdmission load_candidate_score_admission(const CandidateAdmissionRequest&);
class CandidateLiveAdmission final {
public:
    enum class State { unqualified, qualified_score_only };
    CandidateLiveAdmission() noexcept=default;
    State state() const noexcept {return check_?State::qualified_score_only:State::unqualified;}
private:
    std::function<void()> check_;
    std::filesystem::path config_path_,capability_path_;
    CandidateLiveAdmission(std::function<void()> check,std::filesystem::path config,std::filesystem::path capability)
        :check_(std::move(check)),config_path_(std::move(config)),capability_path_(std::move(capability)){}
    void require_score_only(const std::filesystem::path& config,const std::filesystem::path& capability) const {
        if(!check_)throw std::runtime_error("current candidate live admission UNQUALIFIED: admission receipt input missing");
        if(config!=config_path_||capability!=capability_path_)
            throw std::runtime_error("current candidate live admission UNQUALIFIED: score entry config/capability paths differ from admission");
        check_();
    }
    friend CandidateLiveAdmission load_candidate_score_admission(const CandidateAdmissionRequest&);
    template<class Continuation>friend void enter_current_candidate_score_only_session(const CandidateLiveAdmission&,
        const std::filesystem::path&,const std::filesystem::path&,Continuation&&);
};
template<class Continuation>
void enter_current_candidate_manual_session(const CandidateLiveAdmission&,Continuation&&) {
    throw std::runtime_error("current candidate live admission UNQUALIFIED: only the receipt-bound score-only scope is authorized");
}
template<class Continuation>
void enter_current_candidate_score_only_session(const CandidateLiveAdmission& admission,
    const std::filesystem::path& config,const std::filesystem::path& capability,Continuation&& continuation) {
    admission.require_score_only(config,capability);std::forward<Continuation>(continuation)();
}
// A cold tag has no relationship or conversion to live admission. It permits
// only endpoint-free tests of the shared owner action seam.
struct CandidateColdTestTag final {};
}
