#include "x10b_claim.hpp"
#include <gtest/gtest.h>
using namespace pas;
namespace {
Frame frame(){Frame f;f.width=640;f.height=640;f.stride=1920;f.rgb.resize(640*640*3);f.capture_complete_ns=1;return f;}
NoteCandidate body(Vec2 front={320,575},double depth=300,Vec2 tangent={1,0}){NoteCandidate n;n.kind=NoteKind::hold;n.center=front;n.width=140;n.height=depth;n.tangent=tangent;n.rails_geometry=true;n.head_on_line=true;return n;}
NoteCandidate fragment(const NoteCandidate& held){auto n=held;n.center.x+=30*held.tangent.y;n.center.y-=30*held.tangent.x;n.head_on_line=false;n.outline_evidence=true;n.recent_identity=2;n.width=127;n.height-=30;return n;}
LineCandidate line(Vec2 tangent={1,0}){LineCandidate l{{320,575},tangent,640,4,.85};l.observed_ns=1;return l;}
void fill(Frame& f,const NoteCandidate& n){for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){const Vec2 d{x-n.center.x,y-n.center.y};const double a=d.x*n.tangent.x+d.y*n.tangent.y,v=-d.x*n.tangent.y+d.y*n.tangent.x;if(std::abs(a)<=n.width/2&&v>=-n.height&&v<=0){auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;p[0]=40;p[1]=190;p[2]=255;}}}
bool test(const Frame& f,const NoteCandidate& n,const NoteCandidate& h,LineCandidate l=line()){return x10b::current_interior_claim(f,n,l,std::span<const NoteCandidate>(&h,1));}
TEST(X10bClaim, CurrentInteriorFragmentWitness){auto f=frame();auto h=body();fill(f,h);EXPECT_TRUE(test(f,fragment(h),h));}
TEST(X10bClaim, DisjointNormalIntervalsPreserveIndependentNextHold){auto f=frame();auto h=body({320,575},200);fill(f,h);auto n=body({320,180},100);fill(f,n);n.head_on_line=false;n.outline_evidence=true;n.recent_identity=2;EXPECT_FALSE(test(f,n,h));}
TEST(X10bClaim, OrthogonalClaimCannotSuppressCompatibleIncomingFront){auto f=frame();auto h=body({320,575},200,{0,1});fill(f,h);auto n=fragment(body());fill(f,body());EXPECT_FALSE(test(f,n,h));}
TEST(X10bClaim, MissingCurrentPixelsDoNotClaim){auto f=frame();const auto h=body();EXPECT_FALSE(test(f,fragment(h),h));}
TEST(X10bClaim, MissingCurrentClaimDoesNotSuppress){auto f=frame();auto h=body();fill(f,h);EXPECT_FALSE(x10b::current_interior_claim(f,fragment(h),line(),{}));}
TEST(X10bClaim, OneSideMissingCannotClaim){auto f=frame();auto h=body();fill(f,h);for(int y=520;y<570;++y)for(int x=250;x<290;++x){auto* p=f.rgb.data()+y*f.stride+x*3;p[0]=p[1]=p[2]=0;}EXPECT_FALSE(test(f,fragment(h),h));}
TEST(X10bClaim, InteriorEffectMayCoverCenterButBothSidesMustContinue){auto f=frame();auto h=body();fill(f,h);for(int y=525;y<565;++y)for(int x=300;x<340;++x){auto* p=f.rgb.data()+y*f.stride+x*3;p[0]=245;p[1]=220;p[2]=60;}EXPECT_TRUE(test(f,fragment(h),h));}
TEST(X10bClaim, ARealLeadingEdgeDoesNotHaveFillOnBothSidesOfFront){auto f=frame();auto h=body();auto n=fragment(h);fill(f,n);EXPECT_FALSE(test(f,n,h));}
TEST(X10bClaim, StrongDirectFrontIsOutsideSuppressionScope){auto f=frame();auto h=body();fill(f,h);auto n=fragment(h);n.direct_rails_evidence=true;EXPECT_FALSE(test(f,n,h));}
TEST(X10bClaim, HeldContactObservationIsOutsideSuppressionScope){auto f=frame();auto h=body();fill(f,h);auto n=fragment(h);n.held_body_evidence=true;EXPECT_FALSE(test(f,n,h));}
TEST(X10bClaim, StaleLineAndMissingRailClaimFailClosed){auto f=frame();auto h=body();fill(f,h);auto n=fragment(h);auto l=line();l.observed_ns=0;EXPECT_FALSE(test(f,n,h,l));h.rails_geometry=false;EXPECT_FALSE(test(f,n,h));}
TEST(X10bClaim, RotatedCurrentBodyPreservesLocalPixelWitness){auto f=frame();const Vec2 u{std::cos(.35),std::sin(.35)};auto h=body({300,500},300,u);fill(f,h);EXPECT_TRUE(test(f,fragment(h),h,line(u)));}
TEST(X10bClaim, InvalidFrameAndCapacityCannotSupplyEvidence){auto f=frame();auto h=body();fill(f,h);f.rgb.pop_back();EXPECT_FALSE(test(f,fragment(h),h));}
}
