#include "contract.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
using namespace pas;
using namespace pas::x10d_o;
namespace {
Frame frame(){Frame f;f.width=f.height=640;f.stride=1920;f.rgb.resize(640*640*3);f.epoch=f.generation=f.geometry_version=1;f.sequence=2;f.capture_complete_ns=50'000'000;return f;}
NoteCandidate body(Vec2 p={320,575},double h=300,Vec2 u={1,0}){NoteCandidate n;n.kind=NoteKind::hold;n.center=p;n.width=140;n.height=h;n.tangent=u;n.rails_geometry=n.head_on_line=true;return n;}
LineCandidate line(const Frame& f,Vec2 u={1,0}){LineCandidate l{{320,575},u,640,4,.85};l.track_id=7;l.observed_ns=f.capture_complete_ns;return l;}
Anchor anchor(const Frame& f,NoteCandidate n,std::uint64_t id=1){Anchor a;a.id=id;a.note=n;a.observed=f.capture_complete_ns-10'000'000;a.context={1,1,1,1,a.observed,640,640,0};return a;}
void render(Frame& f,const NoteCandidate& n){for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){Vec2 d{x-n.center.x,y-n.center.y};double u=d.x*n.tangent.x+d.y*n.tangent.y,v=-d.x*n.tangent.y+d.y*n.tangent.x;if(std::abs(u)<=n.width/2&&v>=-n.height&&v<=0){auto p=f.rgb.data()+y*f.stride+x*3;p[0]=40;p[1]=190;p[2]=255;}}}
Claim run(const Frame& f,const NoteCandidate& n){auto a=anchor(f,n);return resolve(f,n,line(f,n.tangent),std::span(&a,1));}
}
TEST(Ownership, SolidSupportPositive){auto f=frame();auto n=body();render(f,n);auto c=run(f,n);EXPECT_TRUE(c.pixel_support);EXPECT_EQ(c.owner,1);EXPECT_GE(c.measured_depth,290);}
TEST(Ownership, GapCannotBeBorrowed){auto f=frame();auto n=body();render(f,body({320,575},80));render(f,body({320,400},125));auto c=run(f,n);EXPECT_LE(c.measured_depth,85);}
TEST(Ownership, AbsentBodyIsUnknown){auto f=frame();auto c=run(f,body());EXPECT_FALSE(c.pixel_support);EXPECT_FALSE(c.owner);}
TEST(Ownership, OneSideAbsentIsUnknown){auto f=frame();auto n=body();render(f,n);for(int y=270;y<580;++y)for(int x=250;x<280;++x)std::fill_n(f.rgb.data()+y*f.stride+x*3,3,0);auto c=run(f,n);EXPECT_FALSE(c.pixel_support);EXPECT_FALSE(c.owner);}
TEST(Ownership, SecondAnchorIsUnknown){auto f=frame();auto n=body();render(f,n);std::array<Anchor,2> a{anchor(f,n,1),anchor(f,n,2)};auto c=resolve(f,n,line(f),a);EXPECT_FALSE(c.owner);}
TEST(Ownership, TouchingTailIncomingIsUnknown){
 auto single=frame(),touching=frame();auto n=body();render(single,n);
 // Independently rendered two physical bodies: old tail at 400 and incoming
 // front at 400. No visible separating cap is promised by the contract.
 render(touching,body({320,575},175));render(touching,body({320,400},125));
 ASSERT_EQ(single.rgb,touching.rgb);auto a=anchor(single,n);
 const auto positive=resolve(single,n,line(single),std::span(&a,1));
 const auto negative=resolve(touching,n,line(touching),std::span(&a,1));
 EXPECT_TRUE(positive.pixel_support);EXPECT_FALSE(negative.owner);
 // Physical identity cannot be recovered by inspecting this exact input.
 EXPECT_EQ(positive.owner,negative.owner);EXPECT_EQ(positive.probes,negative.probes);
}
TEST(Ownership, RotatingAndTranslatingSupportUsesCurrentNotePose){for(int k=0;k<8;++k){auto f=frame();const double angle=.05*k;auto n=body({300.0+k*3,500.0-k*2},200,{std::cos(angle),std::sin(angle)});render(f,n);auto c=run(f,n);EXPECT_TRUE(c.pixel_support);EXPECT_TRUE(c.owner);EXPECT_GE(c.measured_depth,190);}}
TEST(Ownership, ThinTapNeighborAndOtherDirectionCannotOwn){auto f=frame();auto n=body();render(f,n);auto a=anchor(f,n);auto tap=n;tap.kind=NoteKind::tap;tap.height=8;EXPECT_FALSE(resolve(f,tap,line(f),std::span(&a,1)).owner);auto other=a;other.note.center.x+=200;EXPECT_FALSE(resolve(f,n,line(f),std::span(&other,1)).owner);other=a;other.note.tangent={0,1};EXPECT_FALSE(resolve(f,n,line(f),std::span(&other,1)).owner);}
TEST(Ownership, LateAlignmentDoesNotConflateNoteAndLineAxes){auto f=frame();auto n=body({320,500},200,{std::cos(.25),std::sin(.25)});render(f,n);auto a=anchor(f,n);const auto c=resolve(f,n,line(f,{1,0}),std::span(&a,1));EXPECT_TRUE(c.pixel_support);EXPECT_TRUE(c.owner);/* association/root remains a separate caller requirement */}
TEST(Ownership, StaleContextAndCapacityRemainUnknown){auto f=frame();auto n=body();render(f,n);auto a=anchor(f,n);for(int k=0;k<4;++k){auto b=a;if(k==0)b.observed=-50'000'000;if(k==1)b.context.epoch=2;if(k==2)b.context.geometry=2;if(k==3)b.observed=f.capture_complete_ns+1;EXPECT_FALSE(resolve(f,n,line(f),std::span(&b,1)).owner);}auto l=line(f);l.association_valid=false;EXPECT_FALSE(resolve(f,n,l,std::span(&a,1)).owner);std::vector<Anchor> many(129,a);EXPECT_FALSE(resolve(f,n,line(f),many).owner);}
TEST(Ownership, InvalidFrameAndNonFiniteGeometryCannotClaim){auto f=frame();auto n=body();render(f,n);f.rgb.pop_back();EXPECT_FALSE(run(f,n).owner);f=frame();n.width=std::numeric_limits<double>::infinity();EXPECT_FALSE(run(f,n).owner);}
