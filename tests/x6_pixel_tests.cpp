#include "x6_pixel.hpp"
#include <gtest/gtest.h>
#include <limits>
using namespace pas::x6;
namespace {
pas::Frame blank(){pas::Frame f;f.width=256;f.height=256;f.stride=768;f.rgb.resize(256*256*3);return f;}
void paint(pas::Frame& f,Region r,int color,double from=-60,double to=60){
    for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){const double a=(x-r.center.x)*r.tangent.x+(y-r.center.y)*r.tangent.y;
        const double n=-(x-r.center.x)*r.tangent.y+(y-r.center.y)*r.tangent.x;
        if(a<from||a>to||std::abs(n)>3)continue;
        auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
        if(color==2){p[0]=245;p[1]=235;p[2]=60;}else if(color==3){p[0]=245;p[1]=55;p[2]=110;}else if(color==1){p[0]=40;p[1]=190;p[2]=235;}else p[0]=p[1]=p[2]=0;
    }
}
Region region(double angle=0){return {{128,128},{std::cos(angle),std::sin(angle)},120,8,2};}
TEST(X6Pixel, CompleteCoreSupportsButDoesNotGrantMotionOrRole){auto f=blank();auto r=region();paint(f,r,2);auto m=measure(f,r);EXPECT_TRUE(m.supported);EXPECT_EQ(m.segments,1);EXPECT_EQ(m.max_gap,0);EXPECT_NEAR(m.span,121,1);EXPECT_NEAR(m.coverage,1,1e-9);EXPECT_NEAR(m.midpoint_offset,0,1e-9);EXPECT_EQ(classify_measurement(m),Reliability::current_core_supported);}
TEST(X6Pixel, OccludingForeignCoreLeavesFragmentationWitness){auto f=blank();auto r=region();paint(f,r,2);paint(f,r,3,25,36);auto m=measure(f,r);EXPECT_EQ(m.segments,2);EXPECT_GE(m.max_gap,10);EXPECT_GE(m.foreign_gap_bins,10);EXPECT_NEAR(m.span,121,1);EXPECT_EQ(classify_measurement(m),Reliability::foreign_overlap_fragmentation);}
TEST(X6Pixel, CroppedObserverRoiCanStillMeasureVisibleOtherFragment){auto f=blank();auto physical=region();paint(f,physical,2);paint(f,physical,3,25,36);auto observed=physical;observed.center.x-=12;observed.length-=24;auto m=measure(f,observed);EXPECT_EQ(m.segments,2);EXPECT_GT(m.midpoint_offset,10);EXPECT_NEAR(m.span,121,1);}
TEST(X6Pixel, MissingPixelsCannotCreateSecondFragment){auto f=blank();auto r=region();paint(f,r,2,-60,20);auto m=measure(f,r);EXPECT_EQ(m.segments,1);EXPECT_EQ(m.foreign_gap_bins,0);EXPECT_LT(m.span,90);EXPECT_NE(classify_measurement(m),Reliability::foreign_overlap_fragmentation);}
TEST(X6Pixel, BlackGapIsNotEvidenceOfForeignOcclusion){auto f=blank();auto r=region();paint(f,r,2);paint(f,r,0,25,36);auto m=measure(f,r);EXPECT_EQ(m.segments,2);EXPECT_EQ(m.foreign_gap_bins,0);EXPECT_NE(classify_measurement(m),Reliability::foreign_overlap_fragmentation);}
TEST(X6Pixel, RotatedAndTranslatedCorePreservesSupport){for(double a:{0.,.05,.6,1.5707963267948966}){auto f=blank();auto r=region(a);r.center={110,119};paint(f,r,2);const auto m=measure(f,r);EXPECT_TRUE(m.supported);EXPECT_EQ(m.segments,1);EXPECT_GT(m.coverage,.98);EXPECT_LE(std::abs(m.midpoint_offset),1);EXPECT_EQ(classify_measurement(m),Reliability::current_core_supported);}}
TEST(X6Pixel, TangentSignDoesNotChangeCanonicalMeasurements){auto f=blank();auto r=region(.6);paint(f,r,2);paint(f,r,3,25,36);auto a=measure(f,r);r.tangent={-r.tangent.x,-r.tangent.y};auto b=measure(f,r);EXPECT_EQ(a.span,b.span);EXPECT_EQ(a.max_gap,b.max_gap);EXPECT_EQ(a.midpoint_offset,b.midpoint_offset);}
TEST(X6Pixel, ParallelNeighborOutsideRibbonDoesNotAddVotes){auto f=blank();auto r=region();paint(f,r,2);const auto a=measure(f,r);auto neighbor=r;neighbor.center.y+=25;paint(f,neighbor,2);auto b=measure(f,r);EXPECT_EQ(a.target_votes,b.target_votes);EXPECT_EQ(a.span,b.span);}
TEST(X6Pixel, HighlightAndForeignPixelsAloneAreNotTargetSupport){auto f=blank();auto r=region();paint(f,r,1);const auto m=measure(f,r);EXPECT_FALSE(m.supported);EXPECT_EQ(classify_measurement(m),Reliability::unknown);}
TEST(X6Pixel, SameColorNeighborIsAnExplicitAmbiguityCounterexample){auto f=blank();auto r=region();paint(f,r,2,-60,-10);paint(f,r,2,10,60);paint(f,r,3,-9,9);auto m=measure(f,r);EXPECT_EQ(classify_measurement(m),Reliability::foreign_overlap_fragmentation);/* Same samples can be two physical notes. This is NOT a merge/center-repair rule. */}
TEST(X6Pixel, OffsetDifferenceUsesCurrentAxisAcrossCanonicalSignFlip){
    Measurement before,after;before.tangent={0,1};before.midpoint_offset=10;
    after.tangent={0,-1};after.midpoint_offset=-10;
    EXPECT_NEAR(midpoint_offset_change(after,before),0,1e-9);
    after.midpoint_offset=-7;
    EXPECT_NEAR(midpoint_offset_change(after,before),3,1e-9);
}
TEST(X6Pixel, ClippingAndCapacityFailClosed){
    auto f=blank();auto r=region();r.center.x=8;paint(f,r,2);
    EXPECT_EQ(classify_measurement(measure(f,r)),Reliability::unknown);
    r=region();r.length=465;
    EXPECT_THROW(measure(f,r),std::runtime_error);
    r=region();r.center.x=std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(measure(f,r),std::runtime_error);
}
TEST(X6Pixel, InvalidFrameAndOrientationReject){
    auto f=blank();auto r=region();r.tangent={2,0};
    EXPECT_THROW(measure(f,r),std::runtime_error);
    r=region();f.rgb.pop_back();
    EXPECT_THROW(measure(f,r),std::runtime_error);
}
}
