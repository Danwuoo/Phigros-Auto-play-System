#include "x10_claim.hpp"
#include <gtest/gtest.h>
#include <array>
using namespace pas;
namespace {
NoteCandidate body(Vec2 front,double depth,Vec2 tangent={1,0}) {
    NoteCandidate n;n.kind=NoteKind::hold;n.center=front;n.tangent=tangent;
    n.width=140;n.height=depth;n.rails_geometry=true;return n;
}
const LineCandidate horizontal{{640,575},{1,0},1280,4,.85};
TEST(X10Donor, NearDescriptionOfClaimedBodyIsSuppressed) {
    const std::array<NoteCandidate,1> held{{body({300,575},500)}};
    auto fragment=body({302,559},484);fragment.width=127;
    EXPECT_TRUE(x10::donor_fallback_claim(fragment,horizontal,held));
}
TEST(X10Donor, IndependentNextHoldWithDisjointNormalExtentMustSurvive) {
    // Old body occupies y375..575; next body y80..180, with a 195px gap.
    // Same lane/width does not make these current bodies the same object.
    const std::array<NoteCandidate,1> held{{body({300,575},200)}};
    const auto next=body({300,180},100);
    EXPECT_FALSE(x10::donor_fallback_claim(next,horizontal,held));
}
TEST(X10Donor, OrthogonalIndependentBodyMustSurvive) {
    // The incoming note agrees with its selected horizontal line; a claim
    // from a different, vertical line must not suppress it by X alone.
    const std::array<NoteCandidate,1> held{{body({300,575},200,{0,1})}};
    const auto crossing=body({300,300},100);
    EXPECT_FALSE(x10::donor_fallback_claim(crossing,horizontal,held));
}
TEST(X10Donor, AdjacentIndependentBodySurvives) {
    const std::array<NoteCandidate,1> held{{body({300,575},200)}};
    EXPECT_FALSE(x10::donor_fallback_claim(body({600,400},100),horizontal,held));
}
TEST(X10Donor, NoCurrentClaimCannotSuppress) {
    EXPECT_FALSE(x10::donor_fallback_claim(body({300,400},100),horizontal,{}));
}
}
