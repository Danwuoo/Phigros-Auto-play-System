#pragma once
#include <stdexcept>
#include <utility>
namespace pas {
// This build has no validator for current-candidate Windows qualifications.
// AA readiness/comparison_allowed is not live authorization. There is no
// qualified constructor, bool conversion, enabling flag or mutable state.
class CandidateLiveAdmission final {
public:
    enum class State { unqualified };
    constexpr CandidateLiveAdmission() noexcept=default;
    constexpr State state() const noexcept {return State::unqualified;}
};
template<class Continuation>
void enter_current_candidate_manual_session(const CandidateLiveAdmission&,Continuation&&) {
    throw std::runtime_error("current candidate live admission UNQUALIFIED: fresh validated Windows source/build/profile/cost and physical authorization receipts required; live validator is not implemented");
}
// A cold tag has no relationship or conversion to live admission. It permits
// only endpoint-free tests of the shared owner action seam.
struct CandidateColdTestTag final {};
}
