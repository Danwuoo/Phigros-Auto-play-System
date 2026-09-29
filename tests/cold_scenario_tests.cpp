#include "pas/game.hpp"
#include "pas/game_tracking.hpp"
#include "pas/journal.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

using namespace pas;
namespace {
struct Truth {
    Vec2 line_center,line_tangent,note_center;
    NoteKind note_kind=NoteKind::tap;
    bool has_line=true,has_note=true;
    std::uint32_t seed=0;
};
Frame blank_frame(std::uint32_t seed) {
    Frame frame;frame.width=1280;frame.height=720;frame.stride=3840;
    frame.epoch=1;frame.generation=1;frame.geometry_version=1;
    frame.source_rotation=1;frame.sequence=1;
    frame.capture_complete_ns=1'000'000'000LL+static_cast<Nanoseconds>(seed)*1'000'000;
    frame.rgb.assign(1280ULL*720*3,0);
    return frame;
}
void fill_box(Frame& frame,Vec2 center,Vec2 tangent,double length,double thickness,
              std::array<std::uint8_t,3> color) {
    const Vec2 normal{-tangent.y,tangent.x};
    const int rx=static_cast<int>(std::ceil((std::abs(tangent.x)*length+
                std::abs(normal.x)*thickness)/2))+2;
    const int ry=static_cast<int>(std::ceil((std::abs(tangent.y)*length+
                std::abs(normal.y)*thickness)/2))+2;
    for(int y=std::max(0,static_cast<int>(center.y)-ry);
        y<std::min(frame.height,static_cast<int>(center.y)+ry+1);++y)
        for(int x=std::max(0,static_cast<int>(center.x)-rx);
            x<std::min(frame.width,static_cast<int>(center.x)+rx+1);++x) {
            const double dx=x-center.x,dy=y-center.y;
            if(std::abs(dx*tangent.x+dy*tangent.y)>length/2||
               std::abs(dx*normal.x+dy*normal.y)>thickness/2)continue;
            auto* p=frame.rgb.data()+static_cast<std::size_t>(y)*frame.stride+x*3;
            std::copy(color.begin(),color.end(),p);
        }
}
void draw_hud(Frame& frame) {
    fill_box(frame,{23,31},{0,1},22,6,{255,255,255});
    fill_box(frame,{37,31},{0,1},22,6,{255,255,255});
    for(int i=0;i<6;++i)
        fill_box(frame,{1026.0+i*24,30},{0,1},20,12,{255,255,255});
}
struct Generated { Frame pixels;Truth truth; };
Generated basic_scene(std::uint32_t seed,int orientation,NoteKind kind,int side,int approach_px=0) {
    auto frame=blank_frame(seed);draw_hud(frame);
    const double angle=orientation==0?0.0:orientation==1?1.5707963267948966:.42;
    const Vec2 tangent{std::cos(angle),std::sin(angle)},normal{-tangent.y,tangent.x};
    const double jitter=static_cast<int>(seed%11)-5;
    const Vec2 line_center{640+jitter*2,orientation==1?360.0:480+jitter};
    const double line_length=orientation==1?620:1050;
    fill_box(frame,line_center,tangent,line_length,3,{255,255,255});
    const Vec2 note_center{line_center.x-tangent.x*110+normal.x*side*(105-approach_px),
                           line_center.y-tangent.y*110+normal.y*side*(105-approach_px)};
    if(kind==NoteKind::hold) {
        fill_box(frame,note_center,tangent,112,145,{40,190,255});
        for(const int edge:{-1,1}) {
            const Vec2 rail{note_center.x+tangent.x*edge*58,
                            note_center.y+tangent.y*edge*58};
            fill_box(frame,rail,normal,145,3,{245,245,245});
        }
    } else {
        const std::array<std::uint8_t,3> color=kind==NoteKind::tap?
            std::array<std::uint8_t,3>{40,190,255}:kind==NoteKind::drag?
            std::array<std::uint8_t,3>{255,220,40}:
            std::array<std::uint8_t,3>{255,80,100};
        fill_box(frame,note_center,tangent,78,9,color);
    }
    return {std::move(frame),{line_center,tangent,note_center,kind,true,true,seed}};
}
Generated top_ui_line_negative(std::uint32_t seed) {
    auto frame=blank_frame(seed);draw_hud(frame);
    fill_box(frame,{640,55},{1,0},1050,3,{255,255,255});
    return {std::move(frame),{{640,55},{1,0},{},NoteKind::tap,false,false,seed}};
}
Frame mixed_hold_scene(std::uint32_t seed,int frame_number,NoteKind side_kind,
                       int last_side_frame=29) {
    auto frame=blank_frame(seed);draw_hud(frame);
    frame.sequence=frame_number+1;
    frame.capture_complete_ns=1'000'000'000LL+frame_number*20'000'000LL;
    fill_box(frame,{640,400},{1,0},1050,3,{255,255,255});
    const double head=280+frame_number*10,center_y=head-90;
    fill_box(frame,{492,center_y},{1,0},144,180,{40,190,255});
    fill_box(frame,{418,center_y},{0,1},180,4,{245,245,245});
    fill_box(frame,{566,center_y},{0,1},180,4,{245,245,245});
    fill_box(frame,{492,head-180},{1,0},148,3,{245,245,245});
    if(frame_number>=18&&frame_number<=last_side_frame) {
        const auto color=side_kind==NoteKind::flick?
            std::array<std::uint8_t,3>{255,80,100}:
            std::array<std::uint8_t,3>{255,220,40};
        fill_box(frame,{900.0,300.0+(frame_number-18)*12},{1,0},78,9,color);
    }
    return frame;
}
Frame rotating_hold_colocated_tap_scene(int frame_number,bool draw_tap=true,
                                       double late_angle_jump=0,bool visible_tail=true) {
    auto frame=blank_frame(8911);draw_hud(frame);
    frame.sequence=frame_number+1;
    frame.capture_complete_ns=1'000'000'000LL+frame_number*20'000'000LL;
    const double angle=-.08+frame_number*.0045+
        (frame_number>=29?late_angle_jump:0);
    const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
    const Vec2 line{640.0+frame_number*.8,400.0+frame_number*.2};
    fill_box(frame,line,u,1050,3,{255,255,255});
    const double distance=-120+frame_number*10.0;
    const Vec2 head{line.x-u.x*150+n.x*distance,
                    line.y-u.y*150+n.y*distance};
    const Vec2 body{head.x-n.x*90,head.y-n.y*90};
    fill_box(frame,body,u,144,180,{40,190,255});
    for(const int side:{-1,1}) {
        const Vec2 rail{body.x+u.x*side*74,body.y+u.y*side*74};
        fill_box(frame,rail,n,180,3,{245,245,245});
    }
    const Vec2 tail{head.x-n.x*180,head.y-n.y*180};
    if(visible_tail)fill_box(frame,tail,u,148,3,{245,245,245});
    else if(frame_number==30)
        fill_box(frame,tail,u,60,3,{245,245,245}); // not attached to both rails
    if(draw_tap&&frame_number>=16&&frame_number<=26) {
        const double tap_distance=145.0-(frame_number-16)*16.0;
        const Vec2 tap{line.x-u.x*150+n.x*tap_distance,
                       line.y-u.y*150+n.y*tap_distance};
        // The foreground core occludes a narrow strip of the held body.
        fill_box(frame,tap,u,86,15,{0,0,0});
        fill_box(frame,tap,u,78,9,{40,190,255});
    }
    return frame;
}
Frame crossing_lines_two_holds_scene(int frame_number,bool second_line_visible=true,
                                     bool independent_rotation=false) {
    auto frame=blank_frame(8923);draw_hud(frame);
    frame.sequence=frame_number+1;
    frame.capture_complete_ns=1'000'000'000LL+frame_number*20'000'000LL;
    const Vec2 crossing{640,480};
    for(const int index:{0,1}) {
        const double angle=(index==0?.25:-.25)+
            (independent_rotation?frame_number*(index==0?.003:-.004):0);
        const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
        if(index==0||second_line_visible||frame_number<4)
            fill_box(frame,crossing,u,1250,3,{255,255,255});
    }
    for(const int index:{0,1}) {
        const double angle=(index==0?.25:-.25)+
            (independent_rotation?frame_number*(index==0?.003:-.004):0);
        const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
        const double along=index==0?-250.0:250.0;
        const double distance=-150.0+frame_number*14.0;
        const Vec2 anchor{crossing.x+u.x*along+n.x*distance,
                          crossing.y+u.y*along+n.y*distance};
        fill_box(frame,anchor,u,112,145,{40,190,255});
        for(const int edge:{-1,1}) {
            const Vec2 rail{anchor.x+u.x*edge*58,anchor.y+u.y*edge*58};
            fill_box(frame,rail,n,145,3,{245,245,245});
        }
    }
    return frame;
}
Frame simultaneous_taps_scene(int frame_number) {
    auto frame=blank_frame(8937);draw_hud(frame);
    frame.sequence=frame_number+1;
    frame.capture_complete_ns=1'000'000'000LL+frame_number*20'000'000LL;
    fill_box(frame,{640,480},{1,0},1050,3,{255,255,255});
    for(const double x:{390.0,890.0})
        fill_box(frame,{x,380.0+frame_number*14},{1,0},78,9,{40,190,255});
    return frame;
}
Frame late_drag_during_hold_scene(int frame_number) {
    auto frame=mixed_hold_scene(8949,frame_number,NoteKind::drag,0);
    if(frame_number>=26&&frame_number<=28)
        fill_box(frame,{900,398},{1,0},78,9,{255,220,40});
    return frame;
}
constexpr std::array<int,10> late_align_times_ms{0,16,37,55,78,94,117,137,159,180};
Frame late_align_rotating_line_scene(int frame_number) {
    auto frame=blank_frame(8961);draw_hud(frame);
    frame.sequence=frame_number+1;
    frame.capture_complete_ns=1'000'000'000LL+
        static_cast<Nanoseconds>(late_align_times_ms[frame_number])*1'000'000;
    const double angle=-.15+frame_number*.01;
    const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
    const Vec2 center{640.0+frame_number*.8,480.0+frame_number*.4};
    fill_box(frame,center,u,1150,3,{255,255,255});
    const double d=-110.0+late_align_times_ms[frame_number]*.75;
    const Vec2 note{center.x-u.x*180+n.x*d,center.y-u.y*180+n.y*d};
    const double note_angle=angle+(frame_number<5?.4:0);
    fill_box(frame,note,{std::cos(note_angle),std::sin(note_angle)},78,9,{40,190,255});
    return frame;
}
Frame staggered_taps_scene(int frame_number,bool completed_flash=false) {
    auto frame=blank_frame(8973);draw_hud(frame);
    frame.sequence=frame_number+1;
    frame.capture_complete_ns=1'000'000'000LL+frame_number*20'000'000LL;
    fill_box(frame,{640,480},{1,0},1050,3,{255,255,255});
    if(frame_number<=9)
        fill_box(frame,{390.0,380.0+frame_number*14},{1,0},78,9,{40,190,255});
    fill_box(frame,{390.0,260.0+frame_number*14},{1,0},78,9,{40,190,255});
    if(completed_flash&&frame_number==12)
        fill_box(frame,{390,480},{1,0},78,9,{40,190,255});
    return frame;
}
Frame dense_capacity_scene(int frame_number,bool over_capacity=false) {
    auto frame=blank_frame(8987);draw_hud(frame);
    frame.sequence=frame_number+1;
    frame.capture_complete_ns=1'000'000'000LL+frame_number*20'000'000LL;
    for(int row=0;row<16;++row) {
        const double y=150.0+row*31.0;
        fill_box(frame,{640,y},{1,0},1200,3,{255,255,255});
        for(int col=0;col<8;++col)
            fill_box(frame,{190.0+col*130,y-8},{1,0},78,9,{40,190,255});
    }
    if(over_capacity)fill_box(frame,{1220,142},{1,0},78,9,{40,190,255});
    return frame;
}
class DelayedFakeTouch final:public TouchBackend {
public:
    DelayedFakeTouch(FakeClock& clock,Nanoseconds first_down_delay)
        :clock_(clock),inner_(clock),delay_(first_down_delay) {}
    TouchReceipt inject(const TouchCommand& command) override {
        const auto start=clock_.now_ns();
        auto receipt=inner_.inject(command);
        if(command.phase==Phase::down&&!delayed_) {
            delayed_=true;clock_.set(start+delay_);
        }
        receipt.injection_start_ns=start;
        receipt.injection_return_ns=clock_.now_ns();
        attempts.push_back(receipt);
        return receipt;
    }
    ReleaseReport release_all() override {
        ++release_calls;return inner_.release_all();
    }
    const auto& contacts() const {return inner_.contacts();}
    std::vector<TouchReceipt> attempts;
    int release_calls=0;
private:
    FakeClock& clock_;
    FakeTouchBackend inner_;
    Nanoseconds delay_;
    bool delayed_=false;
};
class UnknownFirstDownTouch final:public TouchBackend {
public:
    explicit UnknownFirstDownTouch(const Clock& clock):inner_(clock) {}
    TouchReceipt inject(const TouchCommand& command) override {
        auto receipt=inner_.inject(command);
        if(command.phase==Phase::down&&!unknown_sent_) {
            unknown_sent_=true;receipt.success=false;receipt.reason="unknown";
        }
        attempts.push_back(receipt);return receipt;
    }
    ReleaseReport release_all() override {
        ++release_calls;return inner_.release_all();
    }
    const auto& contacts() const {return inner_.contacts();}
    std::vector<TouchReceipt> attempts;
    int release_calls=0;
private:
    FakeTouchBackend inner_;
    bool unknown_sent_=false;
};
double normal_error(Vec2 point,Vec2 line_center,Vec2 tangent) {
    return std::abs((point.x-line_center.x)*(-tangent.y)+
                    (point.y-line_center.y)*tangent.x);
}
} // namespace

TEST(ColdRgbG1, BoundedRowPrescreenMatchesFullScanWithMaximumPermittedGaps) {
    for(int variant=0;variant<3;++variant) {
        auto frame=blank_frame(5100+variant);draw_hud(frame);
        fill_box(frame,{640,440+variant*13.0},{1,0},1030,1,{255,255,255});
        const int y=440+variant*13;
        // Four missing pixels at each 64-pixel sample block still permit
        // the long run. The fifth pixel must keep the prescreen exact.
        for(int x=64;x<1280;x+=64) for(int dx=0;dx<4;++dx) {
            auto* pixel=frame.rgb.data()+static_cast<std::size_t>(y)*frame.stride+(x+dx)*3;
            pixel[0]=pixel[1]=pixel[2]=0;
        }
        if(variant==1) fill_box(frame,{640,y-80.0},{1,0},78,9,{40,190,255});
        if(variant==2) fill_box(frame,{640,y+80.0},{1,0},78,9,{255,220,40});
        FakeClock clock;clock.set(frame.capture_complete_ns);
        GameObserver full(clock,false,false),screened(clock,false,true);
        const auto a=full.process(frame),b=screened.process(frame);
        const auto base_sum=b.combo_glyph_compute_ns+b.line_scan_compute_ns+
            b.component_line_decode_compute_ns+b.note_decode_compute_ns+
            b.line_tracking_compute_ns;
        EXPECT_EQ(b.base_scene_compute_ns,base_sum);
        ASSERT_EQ(a.lines.size(),b.lines.size());
        ASSERT_EQ(a.targets.size(),b.targets.size());
        for(std::size_t i=0;i<a.lines.size();++i) {
            EXPECT_NEAR(a.lines[i].center.x,b.lines[i].center.x,1e-9);
            EXPECT_NEAR(a.lines[i].center.y,b.lines[i].center.y,1e-9);
            EXPECT_NEAR(a.lines[i].length,b.lines[i].length,1e-9);
        }
        for(std::size_t i=0;i<a.targets.size();++i) {
            EXPECT_EQ(a.targets[i].note.kind,b.targets[i].note.kind);
            EXPECT_EQ(a.targets[i].line_id,b.targets[i].line_id);
            EXPECT_EQ(a.targets[i].reason,b.targets[i].reason);
        }
    }
}

TEST(ColdRgbG1, ThreeSeedsFourNoteTypesThreeLineAnglesAndBothSidesHaveIndependentTruth) {
    constexpr std::array<std::uint32_t,3> seeds{1101,2203,3307};
    constexpr std::array<NoteKind,4> kinds{NoteKind::tap,NoteKind::drag,NoteKind::hold,NoteKind::flick};
    for(const auto seed:seeds)for(int orientation=0;orientation<3;++orientation)
        for(const auto kind:kinds)for(const int side:{-1,1}) {
            SCOPED_TRACE("seed="+std::to_string(seed)+" angle="+std::to_string(orientation)+
                " kind="+name(kind)+" side="+std::to_string(side));
            auto generated=basic_scene(seed,orientation,kind,side);
            FakeClock clock;clock.set(generated.pixels.capture_complete_ns);
            GameObserver observer(clock);
            const auto scene=observer.process(generated.pixels);
            const auto line=std::find_if(scene.lines.begin(),scene.lines.end(),[&](const auto& candidate) {
                const double alignment=std::abs(candidate.tangent.x*generated.truth.line_tangent.x+
                    candidate.tangent.y*generated.truth.line_tangent.y);
                return alignment>.94&&normal_error(candidate.center,generated.truth.line_center,
                    generated.truth.line_tangent)<12;
            });
            ASSERT_NE(line,scene.lines.end())<<decision_json(scene).dump();
            const auto note=std::find_if(scene.targets.begin(),scene.targets.end(),[&](const auto& target) {
                const double dx=target.note.center.x-generated.truth.note_center.x;
                const double dy=target.note.center.y-generated.truth.note_center.y;
                return target.note.kind==kind&&std::hypot(dx,dy)<(kind==NoteKind::hold?90:30);
            });
            ASSERT_NE(note,scene.targets.end())<<decision_json(scene).dump();
            EXPECT_EQ(note->line_id,line->track_id);
        }
}

TEST(ColdRgbG1, TopUiLookalikeLineDoesNotBecomeGameGeometryOrNote) {
    for(const auto seed:std::array<std::uint32_t,3>{1101,2203,3307}) {
        auto generated=top_ui_line_negative(seed);
        FakeClock clock;clock.set(generated.pixels.capture_complete_ns);
        GameObserver observer(clock);
        const auto scene=observer.process(generated.pixels);
        EXPECT_TRUE(scene.lines.empty())<<"seed="<<seed<<" "<<decision_json(scene).dump();
        EXPECT_TRUE(scene.targets.empty())<<"seed="<<seed<<" "<<decision_json(scene).dump();
    }
}

TEST(ColdRgbG1, TopUiLookalikeNeverReachesFakeTouch) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,5,{15,0,30'000'000});
    for(int i=0;i<6;++i) {
        auto generated=top_ui_line_negative(2203);auto& frame=generated.pixels;
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        clock.set(frame.capture_complete_ns);const auto scene=observer.process(frame);
        owner.accept(scene);owner.poll();
    }
    clock.set(1'300'000'000);owner.poll();
    EXPECT_TRUE(touch.receipts().empty());EXPECT_TRUE(touch.contacts().empty());
    owner.stop();
}

TEST(ColdRgbG1, MovingTapUsesObserverPredictionOwnerAndFakeTouchWithoutDuplicateDown) {
    for(int orientation=0;orientation<3;++orientation)for(const int side:{-1,1}) {
        SCOPED_TRACE("angle="+std::to_string(orientation)+" side="+std::to_string(side));
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
        DecisionSnapshot final_scene;
        for(int i=0;i<6;++i) {
            auto generated=basic_scene(2203,orientation,NoteKind::tap,side,i*14);
            auto& frame=generated.pixels;frame.sequence=i+1;
            frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
            clock.set(frame.capture_complete_ns);
            final_scene=observer.process(frame);
            owner.accept(final_scene);
        }
        ASSERT_TRUE(final_scene.playing_gate);
        ASSERT_EQ(final_scene.targets.size(),1)<<decision_json(final_scene).dump();
        ASSERT_TRUE(final_scene.targets[0].crossing_ns)<<decision_json(final_scene).dump();
        const auto due=*final_scene.targets[0].crossing_ns;
        ASSERT_GT(due,clock.now_ns());
        clock.set(due);owner.poll();
        const auto downs=std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;});
        EXPECT_EQ(downs,1)<<decision_json(final_scene).dump()<<" rejection="<<owner.last_rejection();
        owner.poll();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdRgbG1, MovingFourNoteKindsReachFakeTouchFromPixels) {
    for(const auto kind:{NoteKind::tap,NoteKind::drag,NoteKind::hold,NoteKind::flick}) {
        SCOPED_TRACE(std::string("kind=")+name(kind));
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,2,{15,0,30'000'000});
        DecisionSnapshot final_scene;
        for(int i=0;i<(kind==NoteKind::hold?12:6);++i) {
            auto generated=basic_scene(3307,0,kind,1,i*14);
            auto& frame=generated.pixels;frame.sequence=i+1;
            frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
            clock.set(frame.capture_complete_ns);
            final_scene=observer.process(frame);owner.accept(final_scene);
        }
        const auto target=std::find_if(final_scene.targets.begin(),final_scene.targets.end(),
            [&](const auto& value){return value.note.kind==kind;});
        ASSERT_NE(target,final_scene.targets.end())<<decision_json(final_scene).dump();
        ASSERT_TRUE(target->crossing_ns)<<decision_json(final_scene).dump();
        ASSERT_GT(*target->crossing_ns,clock.now_ns());
        clock.set(*target->crossing_ns);owner.poll();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),1)
            <<decision_json(final_scene).dump()<<" rejection="<<owner.last_rejection();
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdRgbG1, HoldApproachingHorizontalLineFromAboveCanStartAContact) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
    bool saw_future_crossing=false;
    for(int i=0;i<12;++i) {
        auto generated=basic_scene(3308,0,NoteKind::hold,-1,i*14);
        auto& frame=generated.pixels;frame.sequence=i+1;
        frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        saw_future_crossing=saw_future_crossing||std::any_of(scene.targets.begin(),scene.targets.end(),
            [&](const auto& target){return target.note.kind==NoteKind::hold&&
                target.crossing_ns&&*target.crossing_ns>clock.now_ns();});
    }
    EXPECT_TRUE(saw_future_crossing);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),1)
        <<" rejection="<<owner.last_rejection();
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbG3, MovingLineChasesStationaryTapWithFreshRelativeCrossing) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    DecisionSnapshot scene;
    for(int i=0;i<14;++i) {
        auto frame=blank_frame(7711);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,530.0-i*12},{1,0},1050,3,{255,255,255});
        fill_box(frame,{530,350},{1,0},78,9,{40,190,255});
        clock.set(frame.capture_complete_ns);scene=observer.process(frame);
        owner.accept(scene);owner.poll();
    }
    ASSERT_TRUE(scene.playing_gate);
    ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
    const auto& target=scene.targets.front();
    ASSERT_EQ(target.note.kind,NoteKind::tap);
    EXPECT_NEAR(target.note.center.y,350,3);
    ASSERT_TRUE(target.crossing_ns)<<decision_json(scene).dump();
    EXPECT_GT(*target.crossing_ns,clock.now_ns());
    EXPECT_LT(*target.crossing_ns-clock.now_ns(),100'000'000);
    clock.set(*target.crossing_ns);owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),1)
        <<decision_json(scene).dump()<<" rejection="<<owner.last_rejection();
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbG3, RotatingLinePairsTapThatAlignsOnlyNearCrossingWithIrregularDt) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::uint64_t line_id=0,note_id=0;
    for(int i=0;i<10;++i) {
        auto frame=late_align_rotating_line_scene(i);
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);
        ASSERT_EQ(scene.lines.size(),1)<<decision_json(scene).dump();
        ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
        const auto& line=scene.lines.front();
        const auto& target=scene.targets.front();
        if(i==3) {line_id=line.track_id;note_id=target.note_id;}
        if(i>=3) {
            EXPECT_EQ(line.track_id,line_id);
            EXPECT_EQ(target.note_id,note_id);
            EXPECT_EQ(target.line_id,line_id)<<decision_json(scene).dump();
        }
        owner.accept(scene);owner.poll();
        if(target.crossing_ns&&*target.crossing_ns>clock.now_ns()&&
           (i==9||*target.crossing_ns<
               1'000'000'000LL+static_cast<Nanoseconds>(late_align_times_ms[i+1])*1'000'000)) {
            clock.set(*target.crossing_ns);owner.poll();
        }
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdOracleG3, IrregularDtLateNoteAlignmentStillUsesLocalMotionNotFarAngle) {
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0,note_id=0;
    for(int i=0;i<10;++i) {
        const Nanoseconds now=1'000'000'000LL+
            static_cast<Nanoseconds>(late_align_times_ms[i])*1'000'000;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        const double angle=-.15+i*.01;
        const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
        const Vec2 center{640.0+i*.8,480.0+i*.4};
        LineCandidate line{center,u,1150,3,.9};
        line.track_id=1;line.observed_ns=now;scene.lines={line};
        const double d=-110.0+late_align_times_ms[i]*.75;
        NoteCandidate note;note.kind=NoteKind::tap;
        note.center={center.x-u.x*180+n.x*d,center.y-u.y*180+n.y*d};
        const double note_angle=angle+(i<5?.4:0);
        note.tangent={std::cos(note_angle),std::sin(note_angle)};
        note.width=78;note.height=9;note.confidence=.9;
        track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),1);
        const auto& target=scene.targets.front();
        if(!note_id)note_id=target.note_id;
        EXPECT_EQ(target.note_id,note_id);
        EXPECT_EQ(target.line_id,1)<<decision_json(scene).dump();
        owner.accept(scene);owner.poll();
        if(target.crossing_ns&&*target.crossing_ns>clock.now_ns()&&
           (i==9||*target.crossing_ns<
               1'000'000'000LL+static_cast<Nanoseconds>(late_align_times_ms[i+1])*1'000'000)) {
            clock.set(*target.crossing_ns);owner.poll();
        }
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbG3, ReversedMovingLineRevokesPendingTapBeforeItsOldDueTime) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::optional<Nanoseconds> prior_due;
    for(int i=0;i<15;++i) {
        auto frame=blank_frame(7712);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        const double line_y=i<13?530.0-i*12:386.0+(i-12)*48;
        fill_box(frame,{640,line_y},{1,0},1050,3,{255,255,255});
        fill_box(frame,{530,350},{1,0},78,9,{40,190,255});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        if(i==12) {
            ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
            prior_due=scene.targets.front().crossing_ns;
            ASSERT_TRUE(prior_due)<<decision_json(scene).dump();
            ASSERT_GT(owner.scheduler().pending_count(),0)
                <<decision_json(scene).dump()<<" rejection="<<owner.last_rejection();
        }
    }
    ASSERT_TRUE(prior_due);
    clock.set(std::max(*prior_due,clock.now_ns()+10'000'000));owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),0);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbG3, TeleportedTapCannotCarryTheOldPendingDownToNewPixels) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::optional<Nanoseconds> prior_due;
    std::uint64_t prior_id=0;
    for(int i=0;i<9;++i) {
        auto frame=blank_frame(8967);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,480},{1,0},1050,3,{255,255,255});
        fill_box(frame,i<=6?Vec2{390.0,375.0+i*14.5}:Vec2{600,330},
            {1,0},78,9,{40,190,255});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
        if(i==6) {
            prior_due=scene.targets.front().crossing_ns;
            prior_id=scene.targets.front().note_id;
            ASSERT_TRUE(prior_due)<<decision_json(scene).dump();
            ASSERT_GT(*prior_due,clock.now_ns());
            ASSERT_GT(owner.scheduler().pending_count(),0U);
        }
        if(i>=7) {
            EXPECT_NE(scene.targets.front().note_id,prior_id)<<decision_json(scene).dump();
            EXPECT_FALSE(scene.targets.front().crossing_ns);
        }
    }
    ASSERT_TRUE(prior_due);
    clock.set(std::max(*prior_due,clock.now_ns()+10'000'000));owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),0);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdOracleG3, NewDistantCandidateAfterTeleportCannotReviveOldRoot) {
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0,prior_id=0;
    std::optional<Nanoseconds> prior_due;
    for(int i=0;i<9;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        LineCandidate line{{640,480},{1,0},1050,3,.9};
        line.track_id=1;line.observed_ns=now;scene.lines={line};
        NoteCandidate note;note.kind=NoteKind::tap;
        note.center=i<=6?Vec2{390.0,375.0+i*14.5}:Vec2{600,330};
        note.tangent={1,0};note.width=78;note.height=9;note.confidence=.9;
        track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),1);
        owner.accept(scene);owner.poll();
        if(i==6) {
            prior_due=scene.targets.front().crossing_ns;
            prior_id=scene.targets.front().note_id;
            ASSERT_TRUE(prior_due)<<decision_json(scene).dump();
            ASSERT_GT(owner.scheduler().pending_count(),0U);
        }
        if(i>=7) {
            EXPECT_NE(scene.targets.front().note_id,prior_id);
            EXPECT_FALSE(scene.targets.front().crossing_ns);
        }
    }
    ASSERT_TRUE(prior_due);
    clock.set(std::max(*prior_due,clock.now_ns()+10'000'000));owner.poll();
    EXPECT_TRUE(touch.receipts().empty());
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbR5, ReversingLineAfterDragDownCannotReplayTheStartedContact) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{4,0,30'000'000});
    std::optional<Nanoseconds> due;
    for(int i=0;i<13;++i) {
        auto frame=blank_frame(7755);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,530.0-i*12},{1,0},1050,3,{255,255,255});
        fill_box(frame,{530,350},{1,0},78,9,{255,220,40});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);
        ASSERT_EQ(scene.lines.size(),1)<<decision_json(scene).dump();
        ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
        EXPECT_NEAR(scene.lines.front().center.y,530.0-i*12,3);
        owner.accept(scene);owner.poll();
        if(i==12) {
            ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
            ASSERT_EQ(scene.targets.front().note.kind,NoteKind::drag);
            due=scene.targets.front().crossing_ns;
            ASSERT_TRUE(due)<<decision_json(scene).dump();
            ASSERT_GT(*due,clock.now_ns());
        }
    }
    clock.set(*due);owner.poll();
    ASSERT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),1);
    const auto contact=touch.contacts().begin()->first;
    for(int i=1;i<=8;++i) {
        auto frame=blank_frame(7756);draw_hud(frame);
        frame.sequence=13+i;
        frame.capture_complete_ns=*due+i*20'000'000LL;
        fill_box(frame,{640,386.0+i*28},{1,0},1050,3,{255,255,255});
        fill_box(frame,{530,350},{1,0},78,9,{255,220,40});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);
        ASSERT_EQ(scene.lines.size(),1)<<decision_json(scene).dump();
        ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
        EXPECT_NEAR(scene.lines.front().center.y,386.0+i*28,3);
        owner.accept(scene);owner.poll();
        for(const auto& receipt:touch.receipts())if(receipt.command.phase==Phase::down)
            EXPECT_EQ(receipt.command.contact_id,contact);
    }
    clock.set(*due+300'000'000);owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),1);
    EXPECT_TRUE(touch.contacts().empty());
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbR5, ReversingLineWithCurrentDragSupportKeepsTheSameContact) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{4,0,30'000'000});
    std::optional<Nanoseconds> due;
    for(int i=0;i<13;++i) {
        auto frame=blank_frame(7757);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,530.0-i*12},{1,0},1050,3,{255,255,255});
        fill_box(frame,{530,350},{1,0},78,9,{255,220,40});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        if(i==12) {
            ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
            due=scene.targets.front().crossing_ns;
            ASSERT_TRUE(due)<<decision_json(scene).dump();
        }
    }
    clock.set(*due);owner.poll();
    ASSERT_EQ(touch.contacts().size(),1);
    const auto contact=touch.contacts().begin()->first;
    for(int i=1;i<=3;++i) {
        auto frame=blank_frame(7758);draw_hud(frame);
        frame.sequence=13+i;frame.capture_complete_ns=*due+i*20'000'000LL;
        const double y=386.0+i*28;
        fill_box(frame,{640,y},{1,0},1050,3,{255,255,255});
        fill_box(frame,{530,y-36},{1,0},78,9,{255,220,40});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);
        ASSERT_EQ(scene.lines.size(),1)<<decision_json(scene).dump();
        ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
        owner.accept(scene);owner.poll();
        ASSERT_EQ(touch.contacts().size(),1)<<decision_json(scene).dump();
        EXPECT_EQ(touch.contacts().begin()->first,contact);
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& r){return r.command.phase==Phase::down;}),1);
    }
    clock.set(*due+100'000'000);owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::up;}),1);
    EXPECT_TRUE(touch.contacts().empty());
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbT1, LatestMailboxFeedsFreshPixelsThroughObserverOwnerAndFakeTouch) {
    FakeClock clock;LatestFrame latest(1280,720,3,&clock);
    FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::uint64_t consumed=0;DecisionSnapshot scene;
    for(int i=0;i<6;++i) {
        auto frame=basic_scene(3319,0,NoteKind::tap,1,i*14).pixels;
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        clock.set(frame.capture_complete_ns);
        ASSERT_TRUE(latest.publish(frame.rgb.data(),frame.rgb.size(),frame));
        auto current=latest.read_after(consumed,0);
        ASSERT_TRUE(current);consumed=current->sequence;
        scene=observer.process(*current);current.reset();
        owner.accept(scene);owner.poll();
    }
    ASSERT_EQ(consumed,6);
    ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
    ASSERT_TRUE(scene.targets.front().crossing_ns)<<decision_json(scene).dump();
    clock.set(*scene.targets.front().crossing_ns);owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),1);
    const auto counters=latest.counters();
    EXPECT_EQ(counters.published,6);EXPECT_EQ(counters.consumer_skips,0);
    EXPECT_EQ(counters.pool_drops,0);owner.stop();latest.close();
}

TEST(ColdRgbT1, BurstSkipsOldFramesAndMissingCurrentNoteRevokesPendingDown) {
    FakeClock clock;LatestFrame latest(1280,720,3,&clock);
    FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::uint64_t consumed=0;std::optional<Nanoseconds> old_due;
    for(int i=0;i<6;++i) {
        auto frame=basic_scene(3320,0,NoteKind::tap,1,i*14).pixels;
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        clock.set(frame.capture_complete_ns);
        ASSERT_TRUE(latest.publish(frame.rgb.data(),frame.rgb.size(),frame));
        auto current=latest.read_after(consumed,0);
        ASSERT_TRUE(current);consumed=current->sequence;
        const auto scene=observer.process(*current);current.reset();
        owner.accept(scene);owner.poll();
        if(i==5) {
            ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
            old_due=scene.targets.front().crossing_ns;
            ASSERT_TRUE(old_due)<<decision_json(scene).dump();
            ASSERT_GT(owner.scheduler().pending_count(),0);
        }
    }
    ASSERT_TRUE(old_due);
    for(int i=6;i<9;++i) {
        auto frame=blank_frame(3320);draw_hud(frame);
        frame.sequence=i+1;
        frame.capture_complete_ns=1'100'000'000LL+(i-5)*5'000'000LL;
        fill_box(frame,{640,480},{1,0},1050,3,{255,255,255});
        clock.set(frame.capture_complete_ns);
        ASSERT_TRUE(latest.publish(frame.rgb.data(),frame.rgb.size(),frame));
    }
    auto current=latest.read_after(consumed,0);
    ASSERT_TRUE(current);EXPECT_EQ(current->sequence,9);consumed=current->sequence;
    const auto scene=observer.process(*current);current.reset();
    EXPECT_TRUE(scene.targets.empty())<<decision_json(scene).dump();
    owner.accept(scene);owner.poll();
    EXPECT_EQ(owner.scheduler().pending_count(),0);
    const auto cancellations=owner.take_plan_cancellations();
    EXPECT_TRUE(std::any_of(cancellations.begin(),cancellations.end(),[](const auto& event){
        return event.value("reason",std::string{})=="pending_down_current_object_missing";
    }))<<nlohmann::json(cancellations).dump();
    const auto counters=latest.counters();
    EXPECT_EQ(counters.published,9);EXPECT_EQ(counters.consumer_skips,2);
    EXPECT_EQ(counters.pool_drops,0);
    clock.set(*old_due);owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),0);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());latest.close();
}

TEST(ColdOracleT1, IrregularFreshSourceTimesStillUseOneMeasuredTapRoot) {
    constexpr std::array<int,7> times_ms{0,16,37,55,78,94,117};
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0,note_id=0;
    std::optional<Nanoseconds> last_due;
    for(int i=0;i<7;++i) {
        const Nanoseconds now=1'000'000'000LL+
            static_cast<Nanoseconds>(times_ms[i])*1'000'000;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        LineCandidate line{{640,480},{1,0},1050,3,.9};
        line.track_id=1;line.observed_ns=now;scene.lines={line};
        NoteCandidate note;note.kind=NoteKind::tap;
        note.center={390,375.0+times_ms[i]*.7};note.tangent={1,0};
        note.width=78;note.height=9;note.confidence=.9;
        track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),1);
        if(!note_id)note_id=scene.targets.front().note_id;
        EXPECT_EQ(scene.targets.front().note_id,note_id);
        owner.accept(scene);owner.poll();
        if(scene.targets.front().crossing_ns)
            last_due=scene.targets.front().crossing_ns;
    }
    ASSERT_TRUE(last_due);
    ASSERT_GT(*last_due,clock.now_ns());
    clock.set(*last_due);owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdOracleT1, UnknownFirstDownReleasesAndCannotRetryConcurrentFreshIntents) {
    FakeClock clock;UnknownFirstDownTouch touch(clock);
    GamePlanOwner owner(clock,touch,2,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
    DecisionSnapshot last;
    for(int i=0;i<6;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        LineCandidate line{{640,480},{1,0},1050,3,.9};
        line.track_id=1;line.observed_ns=now;scene.lines={line};
        std::vector<NoteCandidate> notes;
        for(const double x:{390.0,890.0}) {
            NoteCandidate note;note.kind=NoteKind::tap;
            note.center={x,375.0+i*14};note.tangent={1,0};
            note.width=78;note.height=9;note.confidence=.9;
            notes.push_back(note);
        }
        track_legacy_batch(scene,notes,{std::nullopt,std::nullopt},history,next_id);
        owner.accept(scene);last=scene;
    }
    ASSERT_EQ(last.targets.size(),2);
    ASSERT_TRUE(last.targets[0].crossing_ns);
    ASSERT_EQ(last.targets[0].crossing_ns,last.targets[1].crossing_ns);
    const auto due=*last.targets[0].crossing_ns;
    clock.set(due);owner.poll();
    ASSERT_EQ(touch.attempts.size(),1U);
    EXPECT_EQ(touch.attempts.front().command.phase,Phase::down);
    EXPECT_FALSE(touch.attempts.front().success);
    EXPECT_EQ(owner.scheduler().fault(),"input_result_unknown");
    EXPECT_GE(touch.release_calls,1);
    EXPECT_TRUE(touch.contacts().empty());
    auto fresh=last;fresh.sequence++;fresh.context.frame++;
    fresh.context.capture_ns=due+10'000'000;
    for(auto& target:fresh.targets) {
        target.revision++;
        target.evidence_ns=fresh.context.capture_ns;
        target.expires_ns=target.evidence_ns+100'000'000;
        target.crossing_ns=target.evidence_ns+20'000'000;
    }
    clock.set(fresh.context.capture_ns);owner.accept(fresh);owner.poll();
    clock.set(fresh.context.capture_ns+20'000'000);owner.poll();
    EXPECT_EQ(touch.attempts.size(),1U);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbT2, SixteenLinesAndOneHundredTwentyEightNotesStayBoundedOrFailClosed) {
    for(const bool over_capacity:{false,true}) {
        SCOPED_TRACE(over_capacity?"129th note":"exact 16 lines 128 notes");
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,5,{1,0,30'000'000});
        std::array<std::uint64_t,128> note_ids{};
        for(int i=0;i<6;++i) {
            auto frame=dense_capacity_scene(i,over_capacity);
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);
            EXPECT_LE(scene.lines.size(),16U)<<decision_json(scene).dump();
            EXPECT_LE(scene.targets.size(),128U)<<decision_json(scene).dump();
            if(over_capacity) {
                EXPECT_FALSE(scene.capacity_valid)<<decision_json(scene).dump();
                EXPECT_FALSE(scene.playing_gate);
            } else {
                EXPECT_TRUE(scene.capacity_valid)<<decision_json(scene).dump();
                EXPECT_EQ(scene.lines.size(),16U)<<decision_json(scene).dump();
                ASSERT_EQ(scene.targets.size(),128U)<<decision_json(scene).dump();
                if(i==0)for(std::size_t k=0;k<128;++k)
                    note_ids[k]=scene.targets[k].note_id;
                else for(std::size_t k=0;k<128;++k)
                    EXPECT_EQ(scene.targets[k].note_id,note_ids[k]);
            }
            owner.accept(scene);owner.poll();
        }
        EXPECT_TRUE(touch.receipts().empty());
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdOracleT2, BoundaryCapacityAccepts128CandidatesAndRejects129) {
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
    DecisionSnapshot scene;scene.sequence=1;
    scene.context={1,1,1,1,1'000'000'000LL,1280,720,1};
    scene.ui=GameUi::playing;scene.playing_gate=true;
    std::vector<NoteCandidate> notes;
    for(int row=0;row<16;++row) {
        const double y=150.0+row*31.0;
        LineCandidate line{{640,y},{1,0},1200,3,.9};
        line.track_id=row+1;line.observed_ns=scene.context.capture_ns;
        scene.lines.push_back(line);
        for(int col=0;col<8;++col) {
            NoteCandidate note;note.kind=NoteKind::tap;
            note.center={190.0+col*130,y-8};note.tangent={1,0};
            note.width=78;note.height=9;note.confidence=.9;
            notes.push_back(note);
        }
    }
    track_legacy_batch(scene,notes,
        std::vector<std::optional<NoteCandidate>>(notes.size()),history,next_id);
    ASSERT_EQ(scene.targets.size(),128U);
    EXPECT_EQ(history.size(),128U);
    notes.push_back(notes.back());
    EXPECT_THROW(track_legacy_batch(scene,notes,
        std::vector<std::optional<NoteCandidate>>(notes.size()),history,next_id),
        std::invalid_argument);
    EXPECT_EQ(history.size(),128U);
}

TEST(ColdOracleC4, IrregularDtAndThreeRelativeSpeedsKeepMeasuredCrossingRoot) {
    constexpr std::array<int,6> times_ms{0,12,31,47,68,90};
    for(const double velocity:{500.0,750.0,1000.0})
        for(const int side:{-1,1}) {
            SCOPED_TRACE("velocity="+std::to_string(velocity)+
                         " side="+std::to_string(side));
            std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
            for(std::size_t i=0;i<times_ms.size();++i) {
                const Nanoseconds now=1'000'000'000LL+
                    static_cast<Nanoseconds>(times_ms[i])*1'000'000;
                DecisionSnapshot scene;scene.sequence=i+1;
                scene.context={1,1,1,i+1,now,1280,720,1};
                scene.ui=GameUi::playing;scene.playing_gate=true;
                LineCandidate line{{640,480},{1,0},1050,3,.9};
                line.track_id=1;line.observed_ns=now;scene.lines={line};
                const double signed_distance=side*(90.0-velocity*times_ms[i]/1000.0);
                NoteCandidate note;note.kind=NoteKind::tap;
                note.center={530,480+signed_distance};note.tangent={1,0};
                note.width=78;note.height=9;note.confidence=.9;
                track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
                ASSERT_EQ(scene.targets.size(),1U);
                if(i<3)continue;
                const auto& target=scene.targets.front();
                ASSERT_TRUE(target.crossing_ns)<<decision_json(scene).dump();
                const double expected_ms=1000.0*90.0/velocity;
                const double actual_ms=(*target.crossing_ns-1'000'000'000LL)/1e6;
                EXPECT_NEAR(actual_ms,expected_ms,1.0)<<decision_json(scene).dump();
                EXPECT_NEAR(target.hit.y,480.0,1.0);
                EXPECT_GE(target.uncertainty_ns,0);
            }
        }
    for(const double velocity:{0.0,-500.0}) {
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        for(std::size_t i=0;i<times_ms.size();++i) {
            const Nanoseconds now=1'000'000'000LL+
                static_cast<Nanoseconds>(times_ms[i])*1'000'000;
            DecisionSnapshot scene;scene.sequence=i+1;
            scene.context={1,1,1,i+1,now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            LineCandidate line{{640,480},{1,0},1050,3,.9};
            line.track_id=1;line.observed_ns=now;scene.lines={line};
            NoteCandidate note;note.kind=NoteKind::tap;
            note.center={530,390+velocity*times_ms[i]/1000.0};
            note.tangent={1,0};note.width=78;note.height=9;note.confidence=.9;
            track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
            ASSERT_EQ(scene.targets.size(),1U);
            EXPECT_FALSE(scene.targets.front().crossing_ns)<<decision_json(scene).dump();
        }
    }
}

TEST(ColdOracleT2, SaturatedWriterDropsDebugAndFaultsOnCriticalWithoutGrowingQueue) {
    for(const bool critical:{false,true}) {
        SCOPED_TRACE(critical?"critical overflow":"debug overflow");
        std::mutex mutex;std::condition_variable cv;
        bool entered=false,release=false;
        const auto path=std::filesystem::temp_directory_path() /
            ("pas-t2-journal-"+std::to_string(
                reinterpret_cast<std::uintptr_t>(&mutex))+
                (critical?"-critical.jsonl":"-debug.jsonl"));
        {
            Journal journal(path,2,[&] {
                std::unique_lock lock(mutex);
                entered=true;cv.notify_all();
                cv.wait(lock,[&]{return release;});
            });
            ASSERT_TRUE(journal.push({{"seq",1}},false));
            {
                std::unique_lock lock(mutex);
                cv.wait(lock,[&]{return entered;});
            }
            ASSERT_TRUE(journal.push({{"seq",2}},false));
            ASSERT_TRUE(journal.push({{"seq",3}},false));
            EXPECT_FALSE(journal.push({{"seq",4}},critical));
            EXPECT_EQ(journal.debug_drops(),critical?0U:1U);
            EXPECT_EQ(journal.faulted(),critical);
            EXPECT_FALSE(journal.push({{"seq",5}},false));
            {
                std::lock_guard lock(mutex);release=true;
            }
            cv.notify_all();journal.close();
        }
        std::ifstream file(path);
        std::string line;int persisted=0;
        while(std::getline(file,line))++persisted;
        EXPECT_EQ(persisted,3);
        file.close();
        std::filesystem::remove(path);
    }
}

TEST(ColdRgbT2, DenseSceneAcrossThreeEpochsCannotCarryOldGateOrContacts) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,5,{1,0,30'000'000});
    std::array<std::uint64_t,3> first_ids{};
    for(int round=0;round<3;++round) {
        for(int step=0;step<8;++step) {
            auto frame=dense_capacity_scene(step,step==5);
            frame.epoch=round+1;frame.generation=round+1;
            frame.sequence=round*8+step+1;
            frame.capture_complete_ns=1'000'000'000LL+round*500'000'000LL+
                step*20'000'000LL;
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);
            EXPECT_EQ(scene.context.epoch,static_cast<std::uint64_t>(round+1));
            EXPECT_LE(scene.lines.size(),16U);
            EXPECT_LE(scene.targets.size(),128U);
            if(step==0)EXPECT_FALSE(scene.playing_gate);
            if(step==4) {
                EXPECT_TRUE(scene.capacity_valid);
                EXPECT_TRUE(scene.playing_gate);
                ASSERT_EQ(scene.targets.size(),128U);
                first_ids[round]=scene.targets.front().note_id;
            }
            if(step==5) {
                EXPECT_FALSE(scene.capacity_valid);
                EXPECT_FALSE(scene.playing_gate);
            }
            if(step==6)EXPECT_FALSE(scene.playing_gate);
            owner.accept(scene);owner.poll();
            EXPECT_TRUE(touch.contacts().empty());
        }
    }
    EXPECT_LT(first_ids[0],first_ids[1]);
    EXPECT_LT(first_ids[1],first_ids[2]);
    owner.stop();EXPECT_TRUE(touch.receipts().empty());
}

TEST(ColdRgbT2, ThousandDenseFramesKeepStableBoundedIdentityAndNoStaleTouch) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,5,{1,0,30'000'000});
    std::array<std::uint64_t,128> ids{};
    for(int i=0;i<1000;++i) {
        auto frame=dense_capacity_scene(i);
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);
        ASSERT_TRUE(scene.capacity_valid)<<"frame="<<i;
        ASSERT_EQ(scene.lines.size(),16U)<<"frame="<<i;
        ASSERT_EQ(scene.targets.size(),128U)<<"frame="<<i;
        if(i==3)for(std::size_t k=0;k<128;++k)ids[k]=scene.targets[k].note_id;
        if(i>3)for(std::size_t k=0;k<128;++k)
            EXPECT_EQ(scene.targets[k].note_id,ids[k])<<"frame="<<i<<" target="<<k;
        owner.accept(scene);owner.poll();
        EXPECT_TRUE(touch.contacts().empty())<<"frame="<<i;
    }
    owner.stop();EXPECT_TRUE(touch.receipts().empty());
}

TEST(ColdOracleG1, IndependentMovingCandidatesAssociateAndPredictWithoutPixelLabels) {
    for(const auto kind:{NoteKind::tap,NoteKind::drag,NoteKind::hold,NoteKind::flick})
        for(int orientation=0;orientation<3;++orientation)for(const int side:{-1,1}) {
            SCOPED_TRACE(std::string("kind=")+name(kind)+" angle="+
                std::to_string(orientation)+" side="+std::to_string(side));
            const double angle=orientation==0?0:orientation==1?1.5707963267948966:.42;
            const Vec2 tangent{std::cos(angle),std::sin(angle)},normal{-tangent.y,tangent.x};
            std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
            DecisionSnapshot scene;
            for(int i=0;i<6;++i) {
                const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
                scene={};scene.sequence=i+1;
                scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
                scene.ui=GameUi::playing;scene.playing_gate=true;
                LineCandidate line{{640,480},tangent,orientation==1?620.0:1050.0,3,.9};
                line.track_id=1;line.observed_ns=now;scene.lines={line};
                NoteCandidate note;note.kind=kind;
                note.center={640-tangent.x*110+normal.x*side*(105-i*14),
                             480-tangent.y*110+normal.y*side*(105-i*14)};
                note.tangent=tangent;note.width=kind==NoteKind::hold?114:78;
                note.height=kind==NoteKind::hold?142:9;note.confidence=.9;
                if(kind==NoteKind::hold)note.rails_geometry=true;
                track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
                ASSERT_EQ(scene.targets.size(),1);
                EXPECT_EQ(scene.targets[0].line_id,1)<<decision_json(scene).dump();
            }
            EXPECT_TRUE(scene.targets[0].crossing_ns)<<decision_json(scene).dump();
        }
}

TEST(ColdOracleG1, NearLineFirstAppearanceHasNoTimingAndCannotDown) {
    FakeClock clock;FakeTouchBackend touch(clock);GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
    for(int i=0;i<2;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        LineCandidate line{{640,480},{1,0},1050,3,.9};line.track_id=1;
        line.observed_ns=now;scene.lines={line};
        NoteCandidate note;note.kind=NoteKind::tap;note.center={530,482};
        note.tangent={1,0};note.width=78;note.height=9;note.confidence=.9;
        track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),1);
        EXPECT_FALSE(scene.targets[0].crossing_ns);
        owner.accept(scene);owner.poll();
    }
    clock.set(1'200'000'000);owner.poll();
    EXPECT_TRUE(touch.receipts().empty());EXPECT_TRUE(touch.contacts().empty());owner.stop();
}

TEST(ColdOracleR1, IndependentRotatingHoldBodyAndColocatedTapPreserveContactPriority) {
    for(const int contact_limit:{1,2}) {
        SCOPED_TRACE("contact_limit="+std::to_string(contact_limit));
        FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,contact_limit,{3,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        std::optional<int> held_finger;
        std::size_t peak_contacts=0;
        bool tail_confirmed=false;
        for(int i=0;i<36;++i) {
            SCOPED_TRACE("frame="+std::to_string(i));
            const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
            clock.set(now);
            DecisionSnapshot scene;scene.sequence=i+1;
            scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            const double angle=-.08+i*.0045;
            const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
            const Vec2 center{640.0+i*.8,400.0+i*.2};
            LineCandidate line{center,u,1050,3,.9};
            line.track_id=1;line.observed_ns=now;scene.lines={line};
            const double head_distance=-120+i*10.0;
            const Vec2 head{center.x-u.x*150+n.x*head_distance,
                            center.y-u.y*150+n.y*head_distance};
            NoteCandidate hold;hold.kind=NoteKind::hold;hold.center=head;
            hold.tangent=u;hold.width=144;hold.height=180;hold.confidence=.9;
            hold.rails_geometry=true;hold.head_on_line=std::abs(head_distance)<=8;
            hold.held_body_evidence=i>=13;
            hold.tail=Vec2{head.x-n.x*180,head.y-n.y*180};
            std::vector<NoteCandidate> notes{hold};
            if(i>=16&&i<=26) {
                const double tap_distance=145.0-(i-16)*16.0;
                NoteCandidate tap;tap.kind=NoteKind::tap;
                tap.center={center.x-u.x*150+n.x*tap_distance,
                            center.y-u.y*150+n.y*tap_distance};
                tap.tangent=u;tap.width=78;tap.height=9;tap.confidence=.9;
                notes.push_back(tap);
            }
            track_legacy_batch(scene,notes,std::vector<std::optional<NoteCandidate>>(notes.size()),
                history,next_id);
            owner.accept(scene);owner.poll();
            for(const auto& target:scene.targets)if(target.crossing_ns&&
                *target.crossing_ns>clock.now_ns()&&*target.crossing_ns<now+20'000'000LL) {
                clock.set(*target.crossing_ns);owner.poll();
            }
            for(const auto& event:owner.take_coverage_updates())
                tail_confirmed|=event.value("event",std::string{})=="game_hold_tail_confirmed";
            if(i>=14&&i<=30) {
                ASSERT_FALSE(touch.contacts().empty())<<decision_json(scene).dump();
                if(!held_finger)held_finger=touch.contacts().begin()->first;
                EXPECT_TRUE(touch.contacts().contains(*held_finger))<<decision_json(scene).dump();
            }
            peak_contacts=std::max(peak_contacts,touch.contacts().size());
        }
        EXPECT_TRUE(held_finger.has_value());
        EXPECT_EQ(peak_contacts,static_cast<std::size_t>(contact_limit));
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),contact_limit);
        EXPECT_TRUE(tail_confirmed);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdOracleR6, AngleJumpWithVisibleTailOrUnattachedFlashKeepsTheStartedHoldSafe) {
    for(const bool visible_tail:{true,false}) {
        SCOPED_TRACE(visible_tail?"visible tail":"no attached tail");
        FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        std::optional<int> held_finger;
        bool tail_confirmed=false;
        for(int i=0;i<37;++i) {
            SCOPED_TRACE("frame="+std::to_string(i));
            const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
            clock.set(now);
            DecisionSnapshot scene;scene.sequence=i+1;
            scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            const double angle=-.08+i*.0045+(i>=29?.30:0);
            const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
            const Vec2 center{640.0+i*.8,400.0+i*.2};
            LineCandidate line{center,u,1050,3,.9};
            line.track_id=1;line.observed_ns=now;scene.lines={line};
            const double head_distance=-120+i*10.0;
            const Vec2 head{center.x-u.x*150+n.x*head_distance,
                            center.y-u.y*150+n.y*head_distance};
            NoteCandidate hold;hold.kind=NoteKind::hold;hold.center=head;
            hold.tangent=u;hold.width=144;hold.height=180;hold.confidence=.9;
            hold.rails_geometry=i<=32;hold.head_on_line=std::abs(head_distance)<=8;
            hold.held_body_evidence=i>=13&&i<=32;
            if(visible_tail)hold.tail=Vec2{head.x-n.x*180,head.y-n.y*180};
            track_legacy_batch(scene,{hold},{std::nullopt},history,next_id);
            owner.accept(scene);owner.poll();
            for(const auto& target:scene.targets)if(target.crossing_ns&&
                *target.crossing_ns>clock.now_ns()&&*target.crossing_ns<now+20'000'000LL) {
                clock.set(*target.crossing_ns);owner.poll();
            }
            for(const auto& event:owner.take_coverage_updates())
                tail_confirmed|=event.value("event",std::string{})=="game_hold_tail_confirmed";
            if(i>=14&&i<=(visible_tail?30:28)) {
                ASSERT_FALSE(touch.contacts().empty())<<decision_json(scene).dump();
                if(!held_finger)held_finger=touch.contacts().begin()->first;
                EXPECT_TRUE(touch.contacts().contains(*held_finger))<<decision_json(scene).dump();
            }
        }
        EXPECT_TRUE(held_finger.has_value());
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
        EXPECT_EQ(tail_confirmed,visible_tail);
        EXPECT_TRUE(touch.contacts().empty());
        owner.stop();
    }
}

TEST(ColdOracleR2, IndependentOppositeSideHoldsAndFlickRespectTwoOrThreeFingerCapacity) {
    for(const int contacts:{2,3}) {
        SCOPED_TRACE("contacts="+std::to_string(contacts));
        FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,contacts,{10,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        std::size_t peak=0;
        for(int i=0;i<8;++i) {
            const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
            clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
            scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            LineCandidate line{{640,480},{1,0},1050,3,.9};
            line.track_id=1;line.observed_ns=now;scene.lines={line};
            std::vector<NoteCandidate> notes;
            for(const auto [x,side]:{std::pair{410.0,-1},std::pair{760.0,1}}) {
                NoteCandidate hold;hold.kind=NoteKind::hold;
                hold.center={x,480.0+side*(105-i*14)};
                hold.tangent={1,0};hold.width=112;hold.height=145;
                hold.confidence=.9;hold.rails_geometry=true;
                notes.push_back(hold);
            }
            NoteCandidate flick;flick.kind=NoteKind::flick;
            flick.center={1030.0,375.0+i*14};flick.tangent={1,0};
            flick.width=78;flick.height=9;flick.confidence=.9;
            notes.push_back(flick);
            track_legacy_batch(scene,notes,{std::nullopt,std::nullopt,std::nullopt},history,next_id);
            ASSERT_EQ(scene.targets.size(),3)<<decision_json(scene).dump();
            owner.accept(scene);owner.poll();
            for(const auto& target:scene.targets) if(target.crossing_ns&&
                *target.crossing_ns>clock.now_ns()&&
                *target.crossing_ns<now+20'000'000LL) {
                clock.set(*target.crossing_ns);owner.poll();
            }
            peak=std::max(peak,touch.contacts().size());
        }
        int hold1_down=0,hold2_down=0,flick_down=0;
        for(const auto& receipt:touch.receipts()) if(receipt.command.phase==Phase::down) {
            if(receipt.command.x<550)++hold1_down;
            else if(receipt.command.x<900)++hold2_down;
            else ++flick_down;
        }
        EXPECT_EQ(hold1_down,1);EXPECT_EQ(hold2_down,1);
        EXPECT_EQ(flick_down,contacts==3?1:0);
        EXPECT_EQ(peak,static_cast<std::size_t>(contacts));
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdOracleR5, IndependentLineReversalKeepsOnlyTheStartedDragIntent) {
    for(const bool current_support:{true,false}) {
        SCOPED_TRACE(current_support?"current support":"separating line");
        FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,1,{4,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        std::optional<Nanoseconds> due;
        const auto send=[&](int frame_number,Nanoseconds now,double line_y,double note_y,
                            double line_velocity) {
            clock.set(now);DecisionSnapshot scene;scene.sequence=frame_number;
            scene.context={1,1,1,static_cast<std::uint64_t>(frame_number),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            LineCandidate line{{640,line_y},{1,0},1050,3,.9};
            line.track_id=1;line.observed_ns=now;
            line.velocity={0,line_velocity};line.motion_valid=frame_number>=4;
            line.motion_samples=4;line.motion_span_ns=60'000'000;
            scene.lines={line};
            NoteCandidate note;note.kind=NoteKind::drag;note.center={530,note_y};
            note.tangent={1,0};note.width=78;note.height=9;note.confidence=.9;
            track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
            EXPECT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
            owner.accept(scene);owner.poll();
            return scene;
        };
        for(int i=0;i<19;++i) {
            const auto scene=send(i+1,1'000'000'000LL+i*20'000'000LL,
                530.0-i*12,350,-600);
            if(i==12) {
                due=scene.targets.front().crossing_ns;
                ASSERT_TRUE(due)<<decision_json(scene).dump();
                ASSERT_GT(*due,clock.now_ns());
            }
        }
        clock.set(*due);owner.poll();
        ASSERT_EQ(touch.contacts().size(),1);
        const auto contact=touch.contacts().begin()->first;
        for(int i=1;i<=3;++i) {
            const auto line_y=386.0+i*28;
            send(13+i,*due+i*20'000'000LL,line_y,
                current_support?line_y-36:350,1400);
            if(current_support) {
                ASSERT_EQ(touch.contacts().size(),1);
                EXPECT_EQ(touch.contacts().begin()->first,contact);
            }
        }
        clock.set(*due+300'000'000);owner.poll();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& r){return r.command.phase==Phase::down;}),1);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdOracleG3, StationaryTapHasFutureRootWhenOnlyTheLineApproaches) {
    for(const bool flip_tangent:{false,true}) {
        SCOPED_TRACE(flip_tangent?"alternating mod pi":"stable tangent");
        FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        DecisionSnapshot final_scene;
        for(int i=0;i<13;++i) {
            const auto now=1'000'000'000LL+i*20'000'000LL;
            clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
            scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            const double sign=flip_tangent&&i%2?-1.0:1.0;
            LineCandidate line{{640,530.0-i*12},{sign,0},1050,3,.9};
            line.track_id=1;line.observed_ns=now;line.velocity={0,-600};
            line.motion_valid=i>=3;line.motion_samples=4;line.motion_span_ns=60'000'000;
            scene.lines={line};
            NoteCandidate note;note.kind=NoteKind::tap;note.center={530,350};
            note.tangent={1,0};note.width=78;note.height=9;note.confidence=.9;
            track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
            ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
            final_scene=scene;owner.accept(scene);owner.poll();
        }
        ASSERT_TRUE(final_scene.targets.front().crossing_ns)
            <<decision_json(final_scene).dump();
        EXPECT_GT(*final_scene.targets.front().crossing_ns,clock.now_ns());
        clock.set(*final_scene.targets.front().crossing_ns);owner.poll();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& r){return r.command.phase==Phase::down;}),1);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdOracleG3, ReversedLineWithdrawsTheUnexecutedStationaryTapRoot) {
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
    std::optional<Nanoseconds> old_due;
    for(int i=0;i<16;++i) {
        const auto now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        const double line_y=i<=12?530.0-i*12:386.0+(i-12)*28;
        const double velocity=i<=12?-600:1400;
        LineCandidate line{{640,line_y},{1,0},1050,3,.9};
        line.track_id=1;line.observed_ns=now;line.velocity={0,velocity};
        line.motion_valid=i>=3;line.motion_samples=4;line.motion_span_ns=60'000'000;
        scene.lines={line};
        NoteCandidate note;note.kind=NoteKind::tap;note.center={530,350};
        note.tangent={1,0};note.width=78;note.height=9;note.confidence=.9;
        track_legacy_batch(scene,{note},{std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
        owner.accept(scene);owner.poll();
        if(i==12) {
            old_due=scene.targets.front().crossing_ns;
            ASSERT_TRUE(old_due)<<decision_json(scene).dump();
            ASSERT_GT(owner.scheduler().pending_count(),0);
        }
    }
    clock.set(std::max(*old_due,clock.now_ns()+10'000'000));owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),0);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbN2, FourEdgeFlicksKeepFullDownMoveUpPathInsideScreen) {
    for(int edge=0;edge<4;++edge) {
        SCOPED_TRACE("edge="+std::to_string(edge));
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,1,{8,0,30'000'000});
        DecisionSnapshot approach;
        for(int i=0;i<13;++i) {
            auto frame=blank_frame(8871+edge);draw_hud(frame);
            frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
            for(int dispatches=0;dispatches<16;++dispatches) {
                const auto due=owner.scheduler().next_due_ns();
                if(!due||*due>=frame.capture_complete_ns)break;
                clock.set(*due);owner.poll();
            }
            const bool vertical=edge<2;
            const Vec2 tangent=vertical?Vec2{0,1}:Vec2{1,0};
            const Vec2 center=edge==0?Vec2{80,360}:edge==1?Vec2{1200,360}:
                edge==2?Vec2{640,100}:Vec2{640,660};
            fill_box(frame,center,tangent,vertical?600:1100,3,{255,255,255});
            const Vec2 normal{-tangent.y,tangent.x};
            const int side=edge==0?-1:edge==1?1:edge==2?1:-1;
            const double distance=105-i*14;
            fill_box(frame,{center.x+normal.x*side*distance,
                            center.y+normal.y*side*distance},
                tangent,78,9,{255,80,100});
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
            if(i==5)approach=scene;
        }
        const auto flick=std::find_if(approach.targets.begin(),approach.targets.end(),
            [](const auto& target){return target.note.kind==NoteKind::flick;});
        ASSERT_NE(flick,approach.targets.end())<<decision_json(approach).dump();
        ASSERT_TRUE(flick->crossing_ns)<<decision_json(approach).dump();
        const auto plans=owner.take_accepted_plans();
        ASSERT_FALSE(plans.empty())<<decision_json(approach).dump();
        const auto& steps=plans.back().steps;
        ASSERT_EQ(steps.size(),6);
        const auto receipts=touch.receipts();
        EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
        EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::move;}),4);
        EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::up;}),1)
            <<" down_due="<<steps.front().due_ns<<" up_due="<<steps.back().due_ns
            <<" valid_until="<<plans.back().valid_until_ns
            <<" now="<<clock.now_ns()<<" armed="<<owner.scheduler().armed()
            <<" fault="<<owner.scheduler().fault()
            <<" rejection="<<owner.last_rejection();
        for(const auto& receipt:receipts) {
            EXPECT_GE(receipt.command.x,0);EXPECT_LT(receipt.command.x,1280);
            EXPECT_GE(receipt.command.y,720*.12);EXPECT_LT(receipt.command.y,720);
        }
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdRgbN2, TwoVisibleDragsMergeThenSplitWithoutExceedingPhysicalContacts) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,2,{4,0,30'000'000});
    int initial_down=0;
    for(int i=0;i<17;++i) {
        auto frame=blank_frame(8993);draw_hud(frame);
        frame.sequence=i+1;
        frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,480},{1,0},1100,3,{255,255,255});
        if(i<13) {
            const double left=i<6?480.0+i*22:i<9?612.0:612.0-(i-8)*18;
            const double right=i<6?800.0-i*22:i<9?668.0:668.0+(i-8)*18;
            fill_box(frame,{left,480},{1,0},78,9,{255,220,40});
            fill_box(frame,{right,480},{1,0},78,9,{255,220,40});
        }
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        if(i==4) {
            initial_down=static_cast<int>(std::count_if(touch.receipts().begin(),
                touch.receipts().end(),[](const auto& r){return r.command.phase==Phase::down;}));
            EXPECT_EQ(initial_down,2)<<decision_json(scene).dump();
        }
        EXPECT_LE(touch.contacts().size(),2U)<<decision_json(scene).dump();
        EXPECT_TRUE(owner.scheduler().fault().empty())<<decision_json(scene).dump();
    }
    EXPECT_EQ(initial_down,2);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),2);
    EXPECT_TRUE(touch.contacts().empty());
    owner.stop();
}

TEST(ColdRgbN2, ZigzagDragStringMovesOneContactWithFreshVisibleCore) {
    constexpr std::array<std::array<int,16>,3> patterns{{
        {0,28,56,84,112,84,56,28,0,-28,-56,-28,0,28,56,84},
        {112,84,56,28,0,-28,-56,-84,-56,-28,0,28,56,84,112,84},
        {0,28,56,28,0,-28,-56,-28,0,28,56,28,0,-28,-56,-28}}};
    for(std::size_t pattern=0;pattern<patterns.size();++pattern) {
        SCOPED_TRACE("pattern="+std::to_string(pattern));
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,1,{4,0,30'000'000});
        for(int i=0;i<20;++i) {
            auto frame=blank_frame(8999+static_cast<int>(pattern));draw_hud(frame);
            frame.sequence=i+1;
            frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
            fill_box(frame,{640,480},{1,0},1100,3,{255,255,255});
            if(i<16)fill_box(frame,{500.0+patterns[pattern][i],480},
                {1,0},78,9,{255,220,40});
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
            EXPECT_LE(touch.contacts().size(),1U)<<decision_json(scene).dump();
        }
        const auto& receipts=touch.receipts();
        EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
            [](const auto& r){return r.command.phase==Phase::down;}),1);
        EXPECT_GT(std::count_if(receipts.begin(),receipts.end(),
            [](const auto& r){return r.command.phase==Phase::move;}),0);
        EXPECT_TRUE(touch.contacts().empty());
        owner.stop();
    }
}

TEST(ColdRgbN2, StaggeredTapThenFlickExecutesTwoDistinctVisibleIntents) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,2,{9,0,30'000'000});
    for(int i=0;i<25;++i) {
        auto frame=blank_frame(9007);draw_hud(frame);
        frame.sequence=i+1;
        frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,480},{1,0},1050,3,{255,255,255});
        if(i<=9)fill_box(frame,{450.0,380.0+i*14},{1,0},78,9,{40,190,255});
        if(i<=18)fill_box(frame,{790.0,260.0+i*14},{1,0},78,9,{255,80,100});
        for(int dispatches=0;dispatches<16;++dispatches) {
            const auto due=owner.scheduler().next_due_ns();
            if(!due||*due>=frame.capture_complete_ns)break;
            clock.set(*due);owner.poll();
        }
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        EXPECT_LE(touch.contacts().size(),2U)<<decision_json(scene).dump();
    }
    const auto& receipts=touch.receipts();
    EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
        [](const auto& r){return r.command.phase==Phase::down;}),2);
    EXPECT_GE(std::count_if(receipts.begin(),receipts.end(),
        [](const auto& r){return r.command.phase==Phase::move;}),4);
    EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
        [](const auto& r){return r.command.phase==Phase::up;}),2);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbN2, MissingFreshPixelsReleaseAnActiveFlickBeforeItsOldUpDeadline) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{8,0,30'000'000});
    DecisionSnapshot last;
    for(int i=0;i<6;++i) {
        auto frame=blank_frame(8877);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,660},{1,0},1100,3,{255,255,255});
        fill_box(frame,{640.0,555.0+i*14},{1,0},78,9,{255,80,100});
        clock.set(frame.capture_complete_ns);
        last=observer.process(frame);owner.accept(last);
    }
    const auto flick=std::find_if(last.targets.begin(),last.targets.end(),
        [](const auto& target){return target.note.kind==NoteKind::flick;});
    ASSERT_NE(flick,last.targets.end())<<decision_json(last).dump();
    ASSERT_TRUE(flick->crossing_ns);
    const auto plans=owner.take_accepted_plans();ASSERT_FALSE(plans.empty());
    const auto& steps=plans.back().steps;ASSERT_EQ(steps.size(),6);
    clock.set(steps.front().due_ns);owner.poll();
    ASSERT_EQ(touch.contacts().size(),1);
    clock.set(steps.back().due_ns);owner.poll();
    EXPECT_TRUE(touch.contacts().empty());
    EXPECT_FALSE(owner.scheduler().armed());
    EXPECT_TRUE(owner.scheduler().fault().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
    owner.stop();
}

TEST(ColdOracleN2, IndependentFourEdgeFlicksCompleteBoundedPaths) {
    for(int edge=0;edge<4;++edge) {
        SCOPED_TRACE("edge="+std::to_string(edge));
        FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,1,{8,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        for(int i=0;i<13;++i) {
            const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
            for(int dispatches=0;dispatches<16;++dispatches) {
                const auto due=owner.scheduler().next_due_ns();
                if(!due||*due>=now)break;
                clock.set(*due);owner.poll();
            }
            clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
            scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            const bool vertical=edge<2;
            const Vec2 tangent=vertical?Vec2{0,1}:Vec2{1,0};
            const Vec2 center=edge==0?Vec2{80,360}:edge==1?Vec2{1200,360}:
                edge==2?Vec2{640,100}:Vec2{640,660};
            LineCandidate line{center,tangent,vertical?600.0:1100.0,3,.9};
            line.track_id=1;line.observed_ns=now;scene.lines={line};
            const Vec2 normal{-tangent.y,tangent.x};
            const int side=edge==0?-1:edge==1?1:edge==2?1:-1;
            const double distance=105-i*14;
            NoteCandidate flick;flick.kind=NoteKind::flick;
            flick.center={center.x+normal.x*side*distance,
                          center.y+normal.y*side*distance};
            flick.tangent=tangent;flick.width=78;flick.height=9;flick.confidence=.9;
            track_legacy_batch(scene,{flick},{std::nullopt},history,next_id);
            owner.accept(scene);owner.poll();
        }
        const auto& receipts=touch.receipts();
        EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
            [](const auto& r){return r.command.phase==Phase::down;}),1);
        EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
            [](const auto& r){return r.command.phase==Phase::move;}),4);
        EXPECT_EQ(std::count_if(receipts.begin(),receipts.end(),
            [](const auto& r){return r.command.phase==Phase::up;}),1);
        for(const auto& receipt:receipts) {
            EXPECT_GE(receipt.command.x,0);EXPECT_LT(receipt.command.x,1280);
            EXPECT_GE(receipt.command.y,720*.12);EXPECT_LT(receipt.command.y,720);
        }
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdOracleN2, MissingCurrentFlickEvidenceReleasesStartedContact) {
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,1,{8,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
    DecisionSnapshot last;
    for(int i=0;i<6;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        LineCandidate line{{640,660},{1,0},1100,3,.9};
        line.track_id=1;line.observed_ns=now;scene.lines={line};
        NoteCandidate flick;flick.kind=NoteKind::flick;
        flick.center={640.0,555.0+i*14};flick.tangent={1,0};
        flick.width=78;flick.height=9;flick.confidence=.9;
        track_legacy_batch(scene,{flick},{std::nullopt},history,next_id);
        owner.accept(scene);last=scene;
    }
    ASSERT_TRUE(last.targets.front().crossing_ns);
    const auto plans=owner.take_accepted_plans();ASSERT_FALSE(plans.empty());
    const auto& steps=plans.back().steps;ASSERT_EQ(steps.size(),6);
    clock.set(steps.front().due_ns);owner.poll();
    ASSERT_EQ(touch.contacts().size(),1U);
    clock.set(steps.back().due_ns);owner.poll();
    EXPECT_TRUE(touch.contacts().empty());
    EXPECT_FALSE(owner.scheduler().armed());
    EXPECT_TRUE(owner.scheduler().fault().empty());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),1);
    owner.stop();
}

TEST(ColdRgbN1, OneThroughFiveIndependentMovingTapCoresUseDistinctContacts) {
    for(int count=1;count<=5;++count) {
        SCOPED_TRACE("count="+std::to_string(count));
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,5,{1,0,30'000'000});
        DecisionSnapshot last;
        for(int i=0;i<6;++i) {
            auto frame=blank_frame(8851);draw_hud(frame);
            frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
            fill_box(frame,{640,480},{1,0},1200,3,{255,255,255});
            for(int note=0;note<count;++note)
                fill_box(frame,{200.0+note*210,375.0+i*14},{1,0},78,9,{40,190,255});
            clock.set(frame.capture_complete_ns);
            last=observer.process(frame);owner.accept(last);
        }
        ASSERT_EQ(last.targets.size(),count)<<decision_json(last).dump();
        Nanoseconds last_due=clock.now_ns();
        for(const auto& target:last.targets) {
            ASSERT_TRUE(target.crossing_ns)<<decision_json(last).dump();
            EXPECT_NE(target.line_id,0);
            last_due=std::max(last_due,*target.crossing_ns);
        }
        clock.set(last_due);owner.poll();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),count)
            <<decision_json(last).dump()<<" rejection="<<owner.last_rejection();
        EXPECT_EQ(touch.contacts().size(),count);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdRgbN1, SixthCurrentTapCannotStealFiveStartedContacts) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,5,{1,0,30'000'000});
    DecisionSnapshot last;
    for(int i=0;i<6;++i) {
        auto frame=blank_frame(8852);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,480},{1,0},1200,3,{255,255,255});
        for(int note=0;note<6;++note)
            fill_box(frame,{170.0+note*185,375.0+i*14},{1,0},78,9,{40,190,255});
        clock.set(frame.capture_complete_ns);
        last=observer.process(frame);owner.accept(last);
    }
    ASSERT_EQ(last.targets.size(),6)<<decision_json(last).dump();
    Nanoseconds last_due=clock.now_ns();
    for(const auto& target:last.targets) {
        ASSERT_TRUE(target.crossing_ns)<<decision_json(last).dump();
        last_due=std::max(last_due,*target.crossing_ns);
    }
    clock.set(last_due);owner.poll();
    const auto downs=std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;});
    EXPECT_EQ(downs,5)<<decision_json(last).dump()<<" rejection="<<owner.last_rejection();
    EXPECT_EQ(touch.contacts().size(),5);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbN1, StaggeredSameLaneTapsSurviveCompletedNearLineFlash) {
    for(const bool completed_flash:{false,true}) {
        SCOPED_TRACE(completed_flash?"completed flash":"two sequential taps");
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
        std::uint64_t first_id=0,second_id=0;
        for(int i=0;i<18;++i) {
            auto frame=staggered_taps_scene(i,completed_flash);
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);
            const auto second=std::find_if(scene.targets.begin(),scene.targets.end(),[&](const auto& t){
                return std::abs(t.note.center.y-(260.0+i*14))<4;
            });
            ASSERT_NE(second,scene.targets.end())<<decision_json(scene).dump();
            if(!second_id)second_id=second->note_id;
            EXPECT_EQ(second->note_id,second_id);
            if(i<=9) {
                const auto first=std::find_if(scene.targets.begin(),scene.targets.end(),[&](const auto& t){
                    return std::abs(t.note.center.y-(380.0+i*14))<4;
                });
                ASSERT_NE(first,scene.targets.end())<<decision_json(scene).dump();
                if(!first_id)first_id=first->note_id;
                EXPECT_EQ(first->note_id,first_id);
            }
            owner.accept(scene);owner.poll();
            for(const auto& target:scene.targets)if(target.crossing_ns&&
                *target.crossing_ns>clock.now_ns()&&
                *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
                clock.set(*target.crossing_ns);owner.poll();
            }
        }
        EXPECT_NE(first_id,second_id);
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),2);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdOracleN1, SequentialSameLaneIdentityAndCompletedFlashNeverReplayDown) {
    for(const bool completed_flash:{false,true}) {
        SCOPED_TRACE(completed_flash?"completed flash":"two sequential taps");
        FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        std::uint64_t first_id=0,second_id=0;
        for(int i=0;i<18;++i) {
            const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
            clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
            scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            LineCandidate line{{640,480},{1,0},1050,3,.9};
            line.track_id=1;line.observed_ns=now;scene.lines={line};
            std::vector<NoteCandidate> notes;
            if(i<=9) {
                NoteCandidate first;first.kind=NoteKind::tap;
                first.center={390.0,380.0+i*14};first.tangent={1,0};
                first.width=78;first.height=9;first.confidence=.9;
                notes.push_back(first);
            }
            NoteCandidate second;second.kind=NoteKind::tap;
            second.center={390.0,260.0+i*14};second.tangent={1,0};
            second.width=78;second.height=9;second.confidence=.9;
            notes.push_back(second);
            if(completed_flash&&i==12) {
                NoteCandidate flash=second;flash.center={390,480};
                notes.push_back(flash);
            }
            track_legacy_batch(scene,notes,
                std::vector<std::optional<NoteCandidate>>(notes.size()),history,next_id);
            const auto target=std::find_if(scene.targets.begin(),scene.targets.end(),[&](const auto& t){
                return std::abs(t.note.center.y-second.center.y)<2;
            });
            ASSERT_NE(target,scene.targets.end());
            if(!second_id)second_id=target->note_id;
            EXPECT_EQ(target->note_id,second_id);
            if(i<=9) {
                if(!first_id)first_id=scene.targets.front().note_id;
                EXPECT_EQ(scene.targets.front().note_id,first_id);
            }
            owner.accept(scene);owner.poll();
            for(const auto& item:scene.targets)if(item.crossing_ns&&
                *item.crossing_ns>clock.now_ns()&&*item.crossing_ns<now+20'000'000LL) {
                clock.set(*item.crossing_ns);owner.poll();
            }
        }
        EXPECT_NE(first_id,second_id);
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),2);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdRgbG2, ParallelAndCrossedLinesRemainSeparateWithTwoNotes) {
    for(int shape=0;shape<2;++shape) {
        SCOPED_TRACE("shape="+std::to_string(shape));
        auto frame=blank_frame(4409);draw_hud(frame);
        const Vec2 a_tangent=shape==0?Vec2{1,0}:Vec2{std::cos(.25),std::sin(.25)};
        const Vec2 b_tangent=shape==0?Vec2{1,0}:Vec2{std::cos(-.25),std::sin(-.25)};
        const Vec2 a_center=shape==0?Vec2{640,330}:Vec2{640,480};
        const Vec2 b_center=shape==0?Vec2{640,560}:Vec2{640,480};
        fill_box(frame,a_center,a_tangent,shape==0?1000:1250,3,{255,255,255});
        fill_box(frame,b_center,b_tangent,shape==0?1000:1250,3,{255,255,255});
        const auto note_point=[](Vec2 center,Vec2 tangent,double along,double side) {
            return Vec2{center.x+tangent.x*along-tangent.y*side,
                        center.y+tangent.y*along+tangent.x*side};
        };
        const auto note_a=note_point(a_center,a_tangent,-250,-65);
        const auto note_b=note_point(b_center,b_tangent,250,shape==0?65:-65);
        fill_box(frame,note_a,a_tangent,78,9,{40,190,255});
        fill_box(frame,note_b,b_tangent,78,9,{255,220,40});
        FakeClock clock;clock.set(frame.capture_complete_ns);GameObserver observer(clock);
        const auto scene=observer.process(frame);
        ASSERT_GE(scene.lines.size(),2)<<decision_json(scene).dump();
        const auto match=[&](Vec2 center,Vec2 tangent) {
            return std::find_if(scene.lines.begin(),scene.lines.end(),[&](const auto& line){
                return std::abs(line.tangent.x*tangent.x+line.tangent.y*tangent.y)>.94&&
                    normal_error(line.center,center,tangent)<12;
            });
        };
        const auto line_a=match(a_center,a_tangent),line_b=match(b_center,b_tangent);
        ASSERT_NE(line_a,scene.lines.end())<<decision_json(scene).dump();
        ASSERT_NE(line_b,scene.lines.end())<<decision_json(scene).dump();
        EXPECT_NE(line_a->track_id,line_b->track_id);
        const auto match_note=[&](Vec2 point,NoteKind kind) {
            return std::find_if(scene.targets.begin(),scene.targets.end(),[&](const auto& target){
                return target.note.kind==kind&&
                    std::hypot(target.note.center.x-point.x,target.note.center.y-point.y)<25;
            });
        };
        const auto detected_a=match_note(note_a,NoteKind::tap);
        const auto detected_b=match_note(note_b,NoteKind::drag);
        ASSERT_NE(detected_a,scene.targets.end())<<decision_json(scene).dump();
        ASSERT_NE(detected_b,scene.targets.end())<<decision_json(scene).dump();
        EXPECT_EQ(detected_a->line_id,line_a->track_id)<<decision_json(scene).dump();
        EXPECT_EQ(detected_b->line_id,line_b->track_id)<<decision_json(scene).dump();
    }
}

TEST(ColdRgbG2, EquidistantCrossingRemainsUnknownAndCannotDown) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    for(int i=0;i<6;++i) {
        auto frame=blank_frame(5501);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        const Vec2 a{std::cos(.25),std::sin(.25)},b{std::cos(-.25),std::sin(-.25)};
        fill_box(frame,{640,480},a,1250,3,{255,255,255});
        fill_box(frame,{640,480},b,1250,3,{255,255,255});
        // At x=382 the two visible ridges are equidistant from this core.
        fill_box(frame,{382,481},a,78,9,{40,190,255});
        clock.set(frame.capture_complete_ns);const auto scene=observer.process(frame);
        ASSERT_EQ(scene.lines.size(),2)<<decision_json(scene).dump();
        ASSERT_EQ(scene.targets.size(),1)<<decision_json(scene).dump();
        EXPECT_EQ(scene.targets[0].line_id,0)<<decision_json(scene).dump();
        EXPECT_FALSE(scene.targets[0].crossing_ns);
        owner.accept(scene);owner.poll();
    }
    clock.set(1'300'000'000);owner.poll();
    EXPECT_TRUE(touch.receipts().empty());owner.stop();
}

TEST(ColdRgbG2, TwoCrossingLinesDriveIndependentCurrentPixelContacts) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,2,{5,0,30'000'000});
    DecisionSnapshot final_scene;
    for(int i=0;i<7;++i) {
        auto frame=blank_frame(6601);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        const Vec2 a{std::cos(.25),std::sin(.25)},b{std::cos(-.25),std::sin(-.25)};
        fill_box(frame,{640,480},a,1250,3,{255,255,255});
        fill_box(frame,{640,480},b,1250,3,{255,255,255});
        const double distance=105-i*14;
        fill_box(frame,{640-a.x*250+a.y*distance,480-a.y*250-a.x*distance},
            a,78,9,{40,190,255});
        fill_box(frame,{640+b.x*250+b.y*distance,480+b.y*250-b.x*distance},
            b,78,9,{255,220,40});
        clock.set(frame.capture_complete_ns);
        final_scene=observer.process(frame);owner.accept(final_scene);
    }
    ASSERT_TRUE(final_scene.playing_gate);
    ASSERT_EQ(final_scene.lines.size(),2)<<decision_json(final_scene).dump();
    ASSERT_EQ(final_scene.targets.size(),2)<<decision_json(final_scene).dump();
    ASSERT_TRUE(final_scene.targets[0].crossing_ns)<<decision_json(final_scene).dump();
    ASSERT_TRUE(final_scene.targets[1].crossing_ns)<<decision_json(final_scene).dump();
    EXPECT_NE(final_scene.targets[0].line_id,final_scene.targets[1].line_id);
    const auto due=std::max(*final_scene.targets[0].crossing_ns,
        *final_scene.targets[1].crossing_ns);
    ASSERT_GT(due,clock.now_ns());clock.set(due);owner.poll();
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),2)
        <<decision_json(final_scene).dump()<<" rejection="<<owner.last_rejection();
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbG2, JoinedVBranchesAreTwoIndependentCurrentLines) {
    auto frame=blank_frame(8801);draw_hud(frame);
    const Vec2 vertex{640,260},left{260,600},right{1020,600};
    const auto draw_branch=[&](Vec2 end) {
        const Vec2 delta{end.x-vertex.x,end.y-vertex.y};
        const double length=std::hypot(delta.x,delta.y);
        fill_box(frame,{(vertex.x+end.x)/2,(vertex.y+end.y)/2},
            {delta.x/length,delta.y/length},length,3,{255,255,255});
    };
    draw_branch(left);draw_branch(right);
    FakeClock clock;clock.set(frame.capture_complete_ns);GameObserver observer(clock);
    const auto scene=observer.process(frame);
    ASSERT_GE(scene.lines.size(),2)<<decision_json(scene).dump();
    for(const auto end:{left,right}) {
        const Vec2 delta{end.x-vertex.x,end.y-vertex.y};
        const double length=std::hypot(delta.x,delta.y);
        const Vec2 tangent{delta.x/length,delta.y/length};
        const Vec2 center{(vertex.x+end.x)/2,(vertex.y+end.y)/2};
        EXPECT_NE(std::find_if(scene.lines.begin(),scene.lines.end(),[&](const auto& line){
            return std::abs(line.tangent.x*tangent.x+line.tangent.y*tangent.y)>.94&&
                normal_error(line.center,center,tangent)<10;
        }),scene.lines.end())<<decision_json(scene).dump();
    }
}

TEST(ColdRgbG2, ConnectedFrameAndRadialRidgesKeepDistinctLocalSegments) {
    for(const bool radial:{false,true}) {
        SCOPED_TRACE(radial?"radial":"frame");
        auto frame=blank_frame(8803);draw_hud(frame);
        struct Expected {Vec2 center,tangent;};
        std::vector<Expected> expected;
        if(radial) {
            const Vec2 center{640,390};
            for(const Vec2 tangent:std::array<Vec2,4>{
                Vec2{1,0},Vec2{0,1},Vec2{std::sqrt(.5),std::sqrt(.5)},
                Vec2{std::sqrt(.5),-std::sqrt(.5)}}) {
                fill_box(frame,center,tangent,tangent.x==1?1050:600,3,{255,255,255});
                expected.push_back({center,tangent});
            }
        } else {
            for(const double y:{230.0,570.0}) {
                fill_box(frame,{640,y},{1,0},640,3,{255,255,255});
                expected.push_back({{640,y},{1,0}});
            }
            for(const double x:{320.0,960.0}) {
                fill_box(frame,{x,400},{0,1},340,3,{255,255,255});
                expected.push_back({{x,400},{0,1}});
            }
        }
        FakeClock clock;clock.set(frame.capture_complete_ns);GameObserver observer(clock);
        const auto scene=observer.process(frame);
        ASSERT_GE(scene.lines.size(),4)<<decision_json(scene).dump();
        ASSERT_LE(scene.lines.size(),16);
        for(const auto& item:expected)
            EXPECT_NE(std::find_if(scene.lines.begin(),scene.lines.end(),[&](const auto& line){
                return std::abs(line.tangent.x*item.tangent.x+
                                line.tangent.y*item.tangent.y)>.94&&
                    normal_error(line.center,item.center,item.tangent)<12;
            }),scene.lines.end())<<decision_json(scene).dump();
    }
}

TEST(ColdRgbG2, TwoSeparatedCrossingCentersKeepFourCurrentRidges) {
    auto frame=blank_frame(8811);draw_hud(frame);
    struct Expected {Vec2 center,tangent;};
    std::vector<Expected> expected;
    for(const double x:{350.0,930.0})for(const double angle:{-.55,.55}) {
        const Vec2 tangent{std::cos(angle),std::sin(angle)};
        const Vec2 center{x,430};
        fill_box(frame,center,tangent,400,3,{255,255,255});
        expected.push_back({center,tangent});
    }
    FakeClock clock;clock.set(frame.capture_complete_ns);GameObserver observer(clock);
    const auto scene=observer.process(frame);
    ASSERT_GE(scene.lines.size(),4)<<decision_json(scene).dump();
    for(const auto& item:expected) {
        const auto found=std::find_if(scene.lines.begin(),scene.lines.end(),[&](const auto& line){
            return std::abs(line.tangent.x*item.tangent.x+
                            line.tangent.y*item.tangent.y)>.94&&
                normal_error(line.center,item.center,item.tangent)<10&&
                std::hypot(line.center.x-item.center.x,line.center.y-item.center.y)<80;
        });
        ASSERT_NE(found,scene.lines.end())<<decision_json(scene).dump();
    }
}

TEST(ColdRgbG2, FullyOverlappedParallelLinesRemainUnknownUntilCurrentSeparation) {
    FakeClock clock;GameObserver observer(clock);
    std::array<DecisionSnapshot,4> scenes;
    for(int i=0;i<4;++i) {
        auto frame=blank_frame(8821);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        const double top=i==1||i==2?460:410;
        const double bottom=i==1||i==2?460:510;
        fill_box(frame,{640,top},{1,0},950,3,{255,255,255});
        fill_box(frame,{640,bottom},{1,0},950,3,{255,255,255});
        clock.set(frame.capture_complete_ns);scenes[i]=observer.process(frame);
    }
    ASSERT_EQ(scenes[0].lines.size(),2)<<decision_json(scenes[0]).dump();
    EXPECT_LE(scenes[1].lines.size(),1)<<decision_json(scenes[1]).dump();
    EXPECT_LE(scenes[2].lines.size(),1)<<decision_json(scenes[2]).dump();
    ASSERT_EQ(scenes[3].lines.size(),2)<<decision_json(scenes[3]).dump();
    EXPECT_NE(scenes[3].lines[0].track_id,scenes[3].lines[1].track_id);
}

TEST(ColdRgbG2, LongHoldSideRailsDoNotBecomePerpendicularJudgmentLines) {
    auto frame=blank_frame(8831);draw_hud(frame);
    fill_box(frame,{640,310},{0,1},600,160,{140,210,240});
    fill_box(frame,{547,310},{0,1},600,3,{245,245,245});
    fill_box(frame,{733,310},{0,1},600,3,{245,245,245});
    fill_box(frame,{640,610},{1,0},1200,3,{255,255,255});
    FakeClock clock;clock.set(frame.capture_complete_ns);GameObserver observer(clock);
    const auto scene=observer.process(frame);
    ASSERT_EQ(scene.lines.size(),1)<<decision_json(scene).dump();
    EXPECT_GT(std::abs(scene.lines[0].tangent.x),.95);
}

TEST(ColdRgbH1, RotatingTranslatingLineKeepsOneSupportedContactUntilVisibleTail) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
    std::optional<std::uint64_t> finger;
    for(int i=0;i<36;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=blank_frame(8841);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        const double angle=-.08+i*.0045;
        const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
        const Vec2 line{640.0+i*.8,400.0+i*.2};
        fill_box(frame,line,u,1050,3,{255,255,255});
        const double distance=-120+i*10.0;
        const Vec2 head{line.x-u.x*150+n.x*distance,
                        line.y-u.y*150+n.y*distance};
        const Vec2 body{head.x-n.x*90,head.y-n.y*90};
        fill_box(frame,body,u,144,180,{40,190,255});
        for(const int side:{-1,1}) {
            const Vec2 rail{body.x+u.x*side*74,body.y+u.y*side*74};
            fill_box(frame,rail,n,180,3,{245,245,245});
        }
        const Vec2 tail{head.x-n.x*180,head.y-n.y*180};
        fill_box(frame,tail,u,148,3,{245,245,245});
        clock.set(frame.capture_complete_ns);const auto scene=observer.process(frame);
        owner.accept(scene);owner.poll();
        // The scheduler has its own clocked worker in the real pipeline.
        // Exercise a due time between capture frames, then continue with
        // the next fresh RGB observation.
        for(const auto& target:scene.targets)if(target.note.kind==NoteKind::hold&&
            target.crossing_ns&&*target.crossing_ns>clock.now_ns()&&
            *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        const auto downs=std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& r){return r.command.phase==Phase::down;});
        ASSERT_LE(downs,1)<<decision_json(scene).dump();
        if(!touch.contacts().empty()) {
            ASSERT_EQ(touch.contacts().size(),1);
            const auto current_finger=touch.contacts().begin()->first;
            if(!finger)finger=current_finger;
            EXPECT_EQ(current_finger,*finger);
        }
        if(i<28)EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& r){return r.command.phase==Phase::up;}),0)
            <<decision_json(scene).dump();
    }
    EXPECT_TRUE(finger.has_value());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::down;}),1);
    EXPECT_GT(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::move;}),0);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& r){return r.command.phase==Phase::up;}),1);
    const auto updates=owner.take_coverage_updates();
    EXPECT_TRUE(std::any_of(updates.begin(),updates.end(),[](const auto& update){
        return update.value("event",std::string{})=="game_hold_tail_confirmed";
    }))<<nlohmann::json(updates).dump();
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbH1, LongVisibleHoldHeadBodyAndTailUseOneContact) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
    std::uint64_t finger=0;DecisionSnapshot final_scene;
    for(int i=0;i<35;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=blank_frame(7703);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,400},{1,0},1050,3,{255,255,255});
        const double head=280+i*10,center_y=head-90;
        fill_box(frame,{492,center_y},{1,0},144,180,{40,190,255});
        fill_box(frame,{418,center_y},{0,1},180,4,{245,245,245});
        fill_box(frame,{566,center_y},{0,1},180,4,{245,245,245});
        fill_box(frame,{492,head-180},{1,0},148,3,{245,245,245});
        clock.set(frame.capture_complete_ns);const auto scene=observer.process(frame);final_scene=scene;
        owner.accept(scene);owner.poll();
        const auto downs=std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;});
        EXPECT_LE(downs,1)<<decision_json(scene).dump();
        if(i==17) {
            ASSERT_EQ(downs,1)<<decision_json(scene).dump()<<" rejection="<<owner.last_rejection();
            ASSERT_EQ(touch.contacts().size(),1);
            finger=touch.contacts().begin()->first;
        }
        if(i>=17&&i<=27) {
            ASSERT_EQ(touch.contacts().size(),1)<<decision_json(scene).dump();
            EXPECT_EQ(touch.contacts().begin()->first,finger);
        }
    }
    const auto ups=std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::up;});
    EXPECT_EQ(ups,1)<<decision_json(final_scene).dump()<<" rejection="<<owner.last_rejection();
    EXPECT_TRUE(touch.contacts().empty())<<decision_json(final_scene).dump();
    const auto updates=owner.take_coverage_updates();
    EXPECT_TRUE(std::any_of(updates.begin(),updates.end(),[](const auto& update){
        return update.value("event",std::string{})=="game_hold_tail_confirmed";
    }))<<nlohmann::json(updates).dump();
    owner.stop();
}

TEST(ColdRgbR1, RotatingHoldAndColocatedTapKeepIndependentContactsOrRejectAtCapacity) {
    for(const int contact_limit:{1,2}) {
        SCOPED_TRACE("contact_limit="+std::to_string(contact_limit));
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,contact_limit,{3,0,30'000'000});
        std::optional<int> held_finger;
        std::size_t peak_contacts=0;
        bool current_tap_observed=false,tail_confirmed=false,unsupported_cancel=false;
        for(int i=0;i<36;++i) {
            SCOPED_TRACE("frame="+std::to_string(i));
            auto frame=rotating_hold_colocated_tap_scene(i);
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
            for(const auto& target:scene.targets) if(target.crossing_ns&&
                *target.crossing_ns>clock.now_ns()&&
                *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
                clock.set(*target.crossing_ns);owner.poll();
            }
            for(const auto& event:owner.take_coverage_updates())
                tail_confirmed|=event.value("event",std::string{})=="game_hold_tail_confirmed";
            for(const auto& event:owner.take_plan_cancellations())
                unsupported_cancel|=event.value("kind",std::string{})=="hold"&&
                    event.value("reason",std::string{})=="current_object_missing_or_region_lost";
            if(i>=18&&i<=23) current_tap_observed|=std::any_of(
                scene.targets.begin(),scene.targets.end(),[](const auto& target){
                    return target.note.kind==NoteKind::tap;
                });
            if(i>=14&&i<=30) {
                const auto down_count=std::count_if(touch.receipts().begin(),touch.receipts().end(),
                    [](const auto& receipt){return receipt.command.phase==Phase::down;});
                ASSERT_GE(down_count,1)<<decision_json(scene).dump();
                if(!held_finger) {
                    ASSERT_FALSE(touch.contacts().empty())<<decision_json(scene).dump();
                    held_finger=touch.contacts().begin()->first;
                }
                EXPECT_TRUE(touch.contacts().contains(*held_finger))<<decision_json(scene).dump();
            }
            peak_contacts=std::max(peak_contacts,touch.contacts().size());
        }
        EXPECT_TRUE(current_tap_observed);
        EXPECT_TRUE(held_finger.has_value());
        EXPECT_TRUE(tail_confirmed);
        EXPECT_FALSE(unsupported_cancel);
        EXPECT_EQ(peak_contacts,static_cast<std::size_t>(contact_limit));
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),contact_limit);
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdRgbR6, AngleJumpAtIncomingTailNeedsCurrentClosureAndTwoFreshConfirmations) {
    for(const bool visible_tail:{true,false}) {
        SCOPED_TRACE(visible_tail?"attached visible tail":"unattached flash");
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
        std::optional<int> held_finger;
        std::uint64_t line_id_before_jump=0;
        bool tail_confirmed=false,missing_cancel=false;
        int first_tail_evidence_frame=-1;
        for(int i=0;i<37;++i) {
            SCOPED_TRACE("frame="+std::to_string(i));
            auto frame=rotating_hold_colocated_tap_scene(i,false,.30,visible_tail);
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
            for(const auto& target:scene.targets)if(target.crossing_ns&&
                *target.crossing_ns>clock.now_ns()&&
                *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
                clock.set(*target.crossing_ns);owner.poll();
            }
            if(i==28) {
                ASSERT_EQ(scene.lines.size(),1)<<decision_json(scene).dump();
                line_id_before_jump=scene.lines.front().track_id;
            }
            if(i==29) {
                ASSERT_EQ(scene.lines.size(),1)<<decision_json(scene).dump();
                EXPECT_EQ(scene.lines.front().track_id,line_id_before_jump);
            }
            for(const auto& event:owner.take_coverage_updates())
                if(event.value("event",std::string{})=="game_hold_tail_confirmed") {
                    tail_confirmed=true;first_tail_evidence_frame=i;
                }
            for(const auto& event:owner.take_plan_cancellations())
                missing_cancel|=event.value("kind",std::string{})=="hold"&&
                    event.value("reason",std::string{})=="current_object_missing_or_region_lost";
            if(i>=14&&i<=(visible_tail?30:28)) {
                ASSERT_FALSE(touch.contacts().empty())<<decision_json(scene).dump();
                if(!held_finger)held_finger=touch.contacts().begin()->first;
                EXPECT_TRUE(touch.contacts().contains(*held_finger))<<decision_json(scene).dump();
            }
        }
        EXPECT_TRUE(held_finger.has_value());
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
        EXPECT_EQ(tail_confirmed,visible_tail);
        if(visible_tail) {EXPECT_GE(first_tail_evidence_frame,30);EXPECT_FALSE(missing_cancel);}
        EXPECT_TRUE(touch.contacts().empty());
        owner.stop();
    }
}

TEST(ColdRgbR3, CrossingLinesKeepTwoHoldsOnTheirOwnVisibleLineOrRejectMissingLine) {
    for(const bool second_line_visible:{true,false}) {
        SCOPED_TRACE(second_line_visible?"two crossing lines":"second line absent");
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,2,{2,0,30'000'000});
        std::size_t peak_contacts=0;
        DecisionSnapshot final_scene;
        std::array<std::uint64_t,2> expected_line_ids{};
        for(int i=0;i<13;++i) {
            SCOPED_TRACE("frame="+std::to_string(i));
            auto frame=crossing_lines_two_holds_scene(i,second_line_visible);
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
            final_scene=scene;
            for(const auto& target:scene.targets)if(target.crossing_ns&&
                *target.crossing_ns>clock.now_ns()&&
                *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
                clock.set(*target.crossing_ns);owner.poll();
            }
            const Vec2 crossing{640,480};
            for(const int index:{0,1}) {
                if(index==1&&!second_line_visible&&i>=4)continue;
                const double angle=index==0?.25:-.25;
                const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
                const auto line=std::find_if(scene.lines.begin(),scene.lines.end(),[&](const auto& candidate){
                    return std::abs(candidate.tangent.x*u.x+candidate.tangent.y*u.y)>.96&&
                        normal_error(candidate.center,crossing,u)<14;
                });
                ASSERT_NE(line,scene.lines.end())<<decision_json(scene).dump();
                if(i>=3) {
                    if(!expected_line_ids[index])expected_line_ids[index]=line->track_id;
                    EXPECT_EQ(line->track_id,expected_line_ids[index]);
                }
                const double along=index==0?-250.0:250.0;
                const double d=-150.0+i*14.0;
                // The measured Hold candidate is the leading color edge,
                // 72 px ahead of the generator's body center.
                const Vec2 expected{crossing.x+u.x*along+n.x*(d+72),
                                    crossing.y+u.y*along+n.y*(d+72)};
                const auto hold=std::find_if(scene.targets.begin(),scene.targets.end(),[&](const auto& target){
                    return target.note.kind==NoteKind::hold&&
                        std::hypot(target.note.center.x-expected.x,
                                   target.note.center.y-expected.y)<55;
                });
                if(i>=3&&i<=8) {
                    ASSERT_NE(hold,scene.targets.end())<<decision_json(scene).dump();
                    EXPECT_EQ(hold->line_id,line->track_id)<<decision_json(scene).dump();
                }
            }
            peak_contacts=std::max(peak_contacts,touch.contacts().size());
        }
        EXPECT_EQ(peak_contacts,second_line_visible?2U:1U)<<decision_json(final_scene).dump();
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),
            second_line_visible?2:1)<<decision_json(final_scene).dump();
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdRgbG2, IndependentlyRotatingCrossingLinesKeepTwoHoldRelations) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,2,{2,0,30'000'000});
    std::array<std::uint64_t,2> line_ids{},note_ids{};
    std::size_t peak=0;
    for(int i=0;i<13;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=crossing_lines_two_holds_scene(i,true,true);
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        for(const auto& target:scene.targets)if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&
            *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        if(i>=3&&i<=8)for(const int index:{0,1}) {
            const double angle=(index==0?.25:-.25)+i*(index==0?.003:-.004);
            const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
            const auto line=std::find_if(scene.lines.begin(),scene.lines.end(),[&](const auto& candidate){
                return std::abs(candidate.tangent.x*u.x+candidate.tangent.y*u.y)>.96&&
                    normal_error(candidate.center,{640,480},u)<14;
            });
            ASSERT_NE(line,scene.lines.end())<<decision_json(scene).dump();
            const double along=index==0?-250.0:250.0;
            const double d=-150.0+i*14.0+72;
            const Vec2 expected{640+u.x*along+n.x*d,480+u.y*along+n.y*d};
            const auto hold=std::find_if(scene.targets.begin(),scene.targets.end(),[&](const auto& target){
                return target.note.kind==NoteKind::hold&&
                    std::hypot(target.note.center.x-expected.x,
                               target.note.center.y-expected.y)<55;
            });
            ASSERT_NE(hold,scene.targets.end())<<decision_json(scene).dump();
            if(!line_ids[index])line_ids[index]=line->track_id;
            if(!note_ids[index])note_ids[index]=hold->note_id;
            EXPECT_EQ(line->track_id,line_ids[index]);
            EXPECT_EQ(hold->note_id,note_ids[index]);
            EXPECT_EQ(hold->line_id,line->track_id)<<decision_json(scene).dump();
        }
        peak=std::max(peak,touch.contacts().size());
    }
    EXPECT_NE(line_ids[0],line_ids[1]);
    EXPECT_NE(note_ids[0],note_ids[1]);
    EXPECT_EQ(peak,2U);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),2);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdOracleG2, PermutedCandidateOrderKeepsIndependentRotatingLineRelations) {
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
    std::array<std::uint64_t,2> note_ids{};
    for(int i=0;i<8;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        std::array<NoteCandidate,2> notes;
        std::array<LineCandidate,2> lines;
        for(const int index:{0,1}) {
            const double angle=(index==0?.25:-.25)+i*(index==0?.003:-.004);
            const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
            lines[index]={{640,480},u,1250,3,.9};
            lines[index].track_id=index+1;lines[index].observed_ns=now;
            const double along=index==0?-250.0:250.0,d=-105.0+i*14.0;
            auto& note=notes[index];note.kind=NoteKind::tap;
            note.center={640+u.x*along+n.x*d,480+u.y*along+n.y*d};
            note.tangent=u;note.width=78;note.height=9;note.confidence=.9;
        }
        const int first=i%2;
        scene.lines={lines[first],lines[1-first]};
        track_legacy_batch(scene,{notes[first],notes[1-first]},
            {std::nullopt,std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),2);
        for(const int index:{0,1}) {
            const auto& note=notes[index];
            const auto target=std::find_if(scene.targets.begin(),scene.targets.end(),[&](const auto& t){
                return std::hypot(t.note.center.x-note.center.x,
                                  t.note.center.y-note.center.y)<2;
            });
            ASSERT_NE(target,scene.targets.end());
            EXPECT_EQ(target->line_id,static_cast<std::uint64_t>(index+1))
                <<decision_json(scene).dump();
            if(!note_ids[index])note_ids[index]=target->note_id;
            EXPECT_EQ(target->note_id,note_ids[index]);
        }
        owner.accept(scene);owner.poll();
        for(const auto& target:scene.targets)if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&*target.crossing_ns<now+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
    }
    EXPECT_NE(note_ids[0],note_ids[1]);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),2);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdOracleG2, SwappedLineIdsAtCrossingCannotStealConfirmedNoteRelation) {
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
    std::array<std::uint64_t,2> note_ids{};
    for(int i=0;i<8;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        std::vector<NoteCandidate> notes;
        for(const int index:{0,1}) {
            const double angle=index==0?.25:-.25;
            const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
            LineCandidate line{{640,480},u,1250,3,.9};
            line.track_id=i<4?index+1:2-index;
            line.observed_ns=now;scene.lines.push_back(line);
            const double along=index==0?-250.0:250.0,d=-105.0+i*14.0;
            NoteCandidate note;note.kind=NoteKind::tap;
            note.center={640+u.x*along+n.x*d,480+u.y*along+n.y*d};
            note.tangent=u;note.width=78;note.height=9;note.confidence=.9;
            notes.push_back(note);
        }
        track_legacy_batch(scene,notes,{std::nullopt,std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),2);
        for(const int index:{0,1}) {
            const auto& target=scene.targets[index];
            if(!note_ids[index])note_ids[index]=target.note_id;
            EXPECT_EQ(target.note_id,note_ids[index]);
            EXPECT_EQ(target.line_id,i<4?static_cast<std::uint64_t>(index+1):0U)
                <<decision_json(scene).dump();
            if(i>=4)EXPECT_FALSE(target.crossing_ns);
        }
        owner.accept(scene);owner.poll();
    }
    EXPECT_TRUE(touch.receipts().empty());
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbG2, OneOccludedRidgeCannotBecomeTwoExecutableLineIdentities) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::uint64_t note_id=0;
    for(int i=0;i<8;++i) {
        auto frame=blank_frame(8953);draw_hud(frame);
        frame.sequence=i+1;
        frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,480},{1,0},1050,3,{255,255,255});
        fill_box(frame,{640,480},{1,0},60,10,{0,0,0});
        fill_box(frame,{390.0,380.0+i*14},{1,0},78,9,{40,190,255});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);
        ASSERT_EQ(scene.lines.size(),1U)<<decision_json(scene).dump();
        ASSERT_EQ(scene.targets.size(),1U)<<decision_json(scene).dump();
        EXPECT_EQ(scene.targets.front().line_id,scene.lines.front().track_id)
            <<decision_json(scene).dump();
        if(!note_id)note_id=scene.targets.front().note_id;
        EXPECT_EQ(scene.targets.front().note_id,note_id);
        owner.accept(scene);owner.poll();
        if(scene.targets.front().crossing_ns&&
           *scene.targets.front().crossing_ns>clock.now_ns()&&
           *scene.targets.front().crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*scene.targets.front().crossing_ns);owner.poll();
        }
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdOracleR3, EstablishedHoldRelationCannotMoveToSurvivingCrossingLine) {
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,1,{1,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0,note_id=0;
    for(int i=0;i<8;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);
        DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        const Vec2 a{std::cos(.25),std::sin(.25)},b{std::cos(-.25),std::sin(-.25)};
        LineCandidate surviving{{640,480},a,1250,3,.9};
        surviving.track_id=1;surviving.observed_ns=now;
        LineCandidate lost{{640,480},b,1250,3,.9};
        lost.track_id=2;lost.observed_ns=now;
        scene.lines={surviving};if(i<4)scene.lines.push_back(lost);
        const Vec2 n{-b.y,b.x};const double d=-120.0+i*18.0;
        NoteCandidate hold;hold.kind=NoteKind::hold;
        hold.center={640+b.x*250+n.x*d,480+b.y*250+n.y*d};
        hold.tangent=b;hold.width=112;hold.height=145;hold.confidence=.9;
        hold.rails_geometry=true;
        track_legacy_batch(scene,{hold},{std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),1);
        const auto& target=scene.targets.front();
        if(!note_id)note_id=target.note_id;
        EXPECT_EQ(target.note_id,note_id);
        if(i<4)EXPECT_EQ(target.line_id,2)<<decision_json(scene).dump();
        else {
            EXPECT_EQ(target.line_id,0)<<decision_json(scene).dump();
            EXPECT_FALSE(target.crossing_ns);
        }
        owner.accept(scene);owner.poll();
    }
    EXPECT_TRUE(touch.receipts().empty());
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdOracleR3, TwoCrossingLinesKeepTwoIndependentHoldContacts) {
    FakeClock clock;FakeTouchBackend touch(clock);
    GamePlanOwner owner(clock,touch,2,{2,0,30'000'000});
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
    std::array<std::uint64_t,2> note_ids{};
    std::size_t peak=0;
    for(int i=0;i<8;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        clock.set(now);
        DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        std::vector<NoteCandidate> notes;
        for(const int index:{0,1}) {
            const double angle=index==0?.25:-.25;
            const Vec2 u{std::cos(angle),std::sin(angle)},n{-u.y,u.x};
            LineCandidate line{{640,480},u,1250,3,.9};
            line.track_id=index+1;line.observed_ns=now;scene.lines.push_back(line);
            const double along=index==0?-250.0:250.0,d=-105.0+i*14.0;
            NoteCandidate hold;hold.kind=NoteKind::hold;
            hold.center={640+u.x*along+n.x*d,480+u.y*along+n.y*d};
            hold.tangent=u;hold.width=112;hold.height=145;
            hold.confidence=.9;hold.rails_geometry=true;
            notes.push_back(hold);
        }
        track_legacy_batch(scene,notes,{std::nullopt,std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),2);
        for(const int index:{0,1}) {
            const auto& target=scene.targets[index];
            EXPECT_EQ(target.line_id,static_cast<std::uint64_t>(index+1))
                <<decision_json(scene).dump();
            if(!note_ids[index])note_ids[index]=target.note_id;
            EXPECT_EQ(target.note_id,note_ids[index]);
        }
        owner.accept(scene);owner.poll();
        for(const auto& target:scene.targets)if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&*target.crossing_ns<now+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        peak=std::max(peak,touch.contacts().size());
    }
    EXPECT_NE(note_ids[0],note_ids[1]);
    EXPECT_EQ(peak,2U);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),2);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdOracleR3, GeometricallyContinuousNewLineIdNeedsTwoFreshFramesAndNewFit) {
    std::vector<GameTrackHistory> history;std::uint64_t next_id=0,note_id=0;
    for(int i=0;i<8;++i) {
        const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
        DecisionSnapshot scene;scene.sequence=i+1;
        scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
        scene.ui=GameUi::playing;scene.playing_gate=true;
        LineCandidate line{{640,i>=4?482.0:480.0},{1,0},1050,3,.9};
        line.track_id=i<4?2:3;line.observed_ns=now;scene.lines={line};
        NoteCandidate hold;hold.kind=NoteKind::hold;
        hold.center={500,340.0+i*16};hold.tangent={1,0};
        hold.width=112;hold.height=145;hold.confidence=.9;hold.rails_geometry=true;
        track_legacy_batch(scene,{hold},{std::nullopt},history,next_id);
        ASSERT_EQ(scene.targets.size(),1);
        const auto& target=scene.targets.front();
        if(!note_id)note_id=target.note_id;
        EXPECT_EQ(target.note_id,note_id);
        if(i<4)EXPECT_EQ(target.line_id,2);
        else if(i==4) {
            EXPECT_EQ(target.line_id,0);
            EXPECT_FALSE(target.crossing_ns);
        } else if(i==5) {
            EXPECT_EQ(target.line_id,3);
            EXPECT_EQ(target.samples,1);
            EXPECT_FALSE(target.crossing_ns);
        } else EXPECT_EQ(target.line_id,3);
    }
}

TEST(ColdRgbR7, SameDeadlineTapsRespectRpcReturnAndExpiredSecondPlan) {
    for(const Nanoseconds delay:{5'000'000LL,45'000'000LL}) {
        SCOPED_TRACE("first Down RPC ns="+std::to_string(delay));
        FakeClock clock;DelayedFakeTouch touch(clock,delay);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,2,{1,0,30'000'000});
        DecisionSnapshot scene;
        for(int i=0;i<8;++i) {
            auto frame=simultaneous_taps_scene(i);
            clock.set(frame.capture_complete_ns);
            scene=observer.process(frame);
            ASSERT_EQ(scene.targets.size(),2)<<decision_json(scene).dump();
            owner.accept(scene);owner.poll();
        }
        ASSERT_TRUE(scene.targets[0].crossing_ns)<<decision_json(scene).dump();
        ASSERT_TRUE(scene.targets[1].crossing_ns)<<decision_json(scene).dump();
        EXPECT_EQ(*scene.targets[0].crossing_ns,*scene.targets[1].crossing_ns);
        EXPECT_NE(scene.targets[0].note_id,scene.targets[1].note_id);
        EXPECT_EQ(scene.targets[0].line_id,scene.targets[1].line_id);
        const auto due=std::max(*scene.targets[0].crossing_ns,*scene.targets[1].crossing_ns);
        ASSERT_GT(due,clock.now_ns());
        clock.set(due);owner.poll();
        const auto notices=owner.scheduler().take_notices();
        const auto downs=std::count_if(touch.attempts.begin(),touch.attempts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;});
        EXPECT_EQ(downs,delay==5'000'000?2:1)<<decision_json(scene).dump();
        const auto first_down=std::find_if(touch.attempts.begin(),touch.attempts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;});
        ASSERT_NE(first_down,touch.attempts.end());
        EXPECT_EQ(first_down->injection_return_ns-first_down->injection_start_ns,delay);
        EXPECT_EQ(std::any_of(notices.begin(),notices.end(),[](const auto& notice){
            return notice.reason=="target_evidence_or_window_expired";
        }),delay==45'000'000);
        clock.set(due+85'000'000);owner.poll();owner.stop();
        EXPECT_TRUE(touch.contacts().empty());
        EXPECT_GE(touch.release_calls,1);
        EXPECT_EQ(std::count_if(touch.attempts.begin(),touch.attempts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),downs);
    }
}

TEST(ColdOracleR7, IndependentSimultaneousTapsDoNotRetryExpiredSecondDown) {
    for(const Nanoseconds delay:{5'000'000LL,45'000'000LL}) {
        SCOPED_TRACE("first Down RPC ns="+std::to_string(delay));
        FakeClock clock;DelayedFakeTouch touch(clock,delay);
        GamePlanOwner owner(clock,touch,2,{1,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        DecisionSnapshot scene;
        for(int i=0;i<8;++i) {
            const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
            clock.set(now);scene={};scene.sequence=i+1;
            scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            LineCandidate line{{640,480},{1,0},1050,3,.9};
            line.track_id=1;line.observed_ns=now;scene.lines={line};
            std::vector<NoteCandidate> notes;
            for(const double x:{390.0,890.0}) {
                NoteCandidate note;note.kind=NoteKind::tap;
                note.center={x,380.0+i*14};note.tangent={1,0};
                note.width=78;note.height=9;note.confidence=.9;
                notes.push_back(note);
            }
            track_legacy_batch(scene,notes,{std::nullopt,std::nullopt},history,next_id);
            ASSERT_EQ(scene.targets.size(),2);
            owner.accept(scene);owner.poll();
        }
        ASSERT_TRUE(scene.targets[0].crossing_ns)<<decision_json(scene).dump();
        ASSERT_TRUE(scene.targets[1].crossing_ns)<<decision_json(scene).dump();
        EXPECT_EQ(*scene.targets[0].crossing_ns,*scene.targets[1].crossing_ns);
        const auto due=std::max(*scene.targets[0].crossing_ns,*scene.targets[1].crossing_ns);
        ASSERT_GT(due,clock.now_ns());
        clock.set(due);owner.poll();
        const auto notices=owner.scheduler().take_notices();
        const auto downs=std::count_if(touch.attempts.begin(),touch.attempts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;});
        EXPECT_EQ(downs,delay==5'000'000?2:1);
        const auto first_down=std::find_if(touch.attempts.begin(),touch.attempts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;});
        ASSERT_NE(first_down,touch.attempts.end());
        EXPECT_EQ(first_down->injection_return_ns-first_down->injection_start_ns,delay);
        EXPECT_EQ(std::any_of(notices.begin(),notices.end(),[](const auto& notice){
            return notice.reason=="target_evidence_or_window_expired";
        }),delay==45'000'000);
        clock.set(due+85'000'000);owner.poll();owner.stop();
        EXPECT_TRUE(touch.contacts().empty());
        EXPECT_EQ(std::count_if(touch.attempts.begin(),touch.attempts.end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down;}),downs);
    }
}

TEST(ColdRgbR4, NearLineLateDragUsesFreeFingerOrRejectsFullHoldContact) {
    for(const int contact_limit:{1,2}) {
        SCOPED_TRACE("contacts="+std::to_string(contact_limit));
        FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
        GamePlanOwner owner(clock,touch,contact_limit,{6,0,30'000'000});
        std::optional<int> hold_contact;
        for(int i=0;i<=28;++i) {
            auto frame=late_drag_during_hold_scene(i);
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);
            owner.accept(scene);owner.poll();
            if(i==18) {
                ASSERT_EQ(touch.contacts().size(),1U)<<decision_json(scene).dump();
                hold_contact=touch.contacts().begin()->first;
            }
            if(i>=26) {
                ASSERT_TRUE(hold_contact);
                EXPECT_TRUE(touch.contacts().contains(*hold_contact))
                    <<decision_json(scene).dump();
                const auto drag_downs=std::count_if(touch.receipts().begin(),touch.receipts().end(),
                    [](const auto& receipt){return receipt.command.phase==Phase::down&&
                        receipt.command.x>750;});
                if(i==26)EXPECT_EQ(drag_downs,0);
                if(i>=27)EXPECT_EQ(drag_downs,contact_limit==2?1:0)
                    <<decision_json(scene).dump();
            }
        }
        const auto notices=owner.scheduler().take_notices();
        if(contact_limit==1)EXPECT_TRUE(std::any_of(notices.begin(),notices.end(),
            [](const auto& notice){return notice.reason=="contact_conflict";}));
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
        EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::down&&
                receipt.command.x<750;}),1);
    }
}

TEST(ColdOracleR4, IndependentLateDragNeedsTwoCurrentFramesAndAvailableContact) {
    for(const int contact_limit:{1,2}) {
        SCOPED_TRACE("contacts="+std::to_string(contact_limit));
        FakeClock clock;FakeTouchBackend touch(clock);
        GamePlanOwner owner(clock,touch,contact_limit,{6,0,30'000'000});
        std::vector<GameTrackHistory> history;std::uint64_t next_id=0;
        std::optional<int> hold_contact;
        for(int i=0;i<=28;++i) {
            const Nanoseconds now=1'000'000'000LL+i*20'000'000LL;
            clock.set(now);DecisionSnapshot scene;scene.sequence=i+1;
            scene.context={1,1,1,static_cast<std::uint64_t>(i+1),now,1280,720,1};
            scene.ui=GameUi::playing;scene.playing_gate=true;
            LineCandidate line{{640,400},{1,0},1050,3,.9};
            line.track_id=1;line.observed_ns=now;scene.lines={line};
            NoteCandidate hold;hold.kind=NoteKind::hold;
            hold.center={492,280.0+i*10};hold.tangent={1,0};
            hold.width=144;hold.height=180;hold.confidence=.9;
            hold.rails_geometry=true;hold.head_on_line=std::abs(hold.center.y-400)<=8;
            hold.held_body_evidence=i>=13;
            hold.tail=Vec2{492,hold.center.y-180};
            std::vector<NoteCandidate> notes{hold};
            if(i>=26) {
                NoteCandidate drag;drag.kind=NoteKind::drag;
                drag.center={900,398};drag.tangent={1,0};
                drag.width=78;drag.height=9;drag.confidence=.9;
                notes.push_back(drag);
            }
            track_legacy_batch(scene,notes,
                std::vector<std::optional<NoteCandidate>>(notes.size()),history,next_id);
            owner.accept(scene);owner.poll();
            for(const auto& target:scene.targets)if(target.crossing_ns&&
                *target.crossing_ns>clock.now_ns()&&*target.crossing_ns<now+20'000'000LL) {
                clock.set(*target.crossing_ns);owner.poll();
            }
            if(i==18) {
                ASSERT_EQ(touch.contacts().size(),1U)<<decision_json(scene).dump();
                hold_contact=touch.contacts().begin()->first;
            }
            if(i>=26) {
                ASSERT_TRUE(hold_contact);
                EXPECT_TRUE(touch.contacts().contains(*hold_contact))
                    <<decision_json(scene).dump();
                const auto drag_downs=std::count_if(touch.receipts().begin(),touch.receipts().end(),
                    [](const auto& receipt){return receipt.command.phase==Phase::down&&
                        receipt.command.x>750;});
                if(i==26)EXPECT_EQ(drag_downs,0);
                if(i>=27)EXPECT_EQ(drag_downs,contact_limit==2?1:0)
                    <<decision_json(scene).dump();
            }
        }
        owner.stop();EXPECT_TRUE(touch.contacts().empty());
    }
}

TEST(ColdRgbH2, VisibleHoldAndLaterTapUseTwoContactsWithoutReplacingTheHeldFinger) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,2,{3,0,30'000'000});
    std::optional<std::uint64_t> hold_finger;
    std::size_t peak_contacts=0;
    for(int i=0;i<35;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=blank_frame(8861);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,400},{1,0},1050,3,{255,255,255});
        const double head=280+i*10,center_y=head-90;
        fill_box(frame,{492,center_y},{1,0},144,180,{40,190,255});
        fill_box(frame,{418,center_y},{0,1},180,4,{245,245,245});
        fill_box(frame,{566,center_y},{0,1},180,4,{245,245,245});
        fill_box(frame,{492,head-180},{1,0},148,3,{245,245,245});
        if(i<=22)fill_box(frame,{900.0,220.0+i*10},{1,0},78,9,{40,190,255});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        for(const auto& target:scene.targets)if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&
            *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        peak_contacts=std::max(peak_contacts,touch.contacts().size());
        if(i>=17&&i<=29) {
            const auto held=std::find_if(scene.targets.begin(),scene.targets.end(),
                [](const auto& target){return target.note.kind==NoteKind::hold;});
            ASSERT_NE(held,scene.targets.end())<<decision_json(scene).dump();
            const auto current=std::find_if(touch.contacts().begin(),touch.contacts().end(),
                [&](const auto& contact){return std::abs(contact.second[0]-held->hit.x)<80;});
            ASSERT_NE(current,touch.contacts().end())<<decision_json(scene).dump();
            if(!hold_finger)hold_finger=current->first;
            EXPECT_EQ(current->first,*hold_finger);
        }
    }
    EXPECT_TRUE(hold_finger.has_value());
    EXPECT_EQ(peak_contacts,2);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),2);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbH2, OneFingerHoldCannotBePreemptedByLaterEligibleFlick) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{10,0,30'000'000});
    std::optional<int> held_finger;bool flick_eligible=false;
    for(int i=0;i<35;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=mixed_hold_scene(8862,i,NoteKind::flick,25);
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        for(const auto& target:scene.targets) if(target.note.kind==NoteKind::flick&&
            target.crossing_ns&&target.reason=="prediction_observe_only")
            flick_eligible=true;
        for(const auto& target:scene.targets) if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&
            *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        if(i>=17&&i<=29) {
            ASSERT_EQ(touch.contacts().size(),1)<<decision_json(scene).dump();
            const int current=touch.contacts().begin()->first;
            if(!held_finger)held_finger=current;
            EXPECT_EQ(current,*held_finger);
        }
    }
    EXPECT_TRUE(flick_eligible);
    EXPECT_TRUE(held_finger.has_value());
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),1);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbH2, VisibleHoldAndLaterFlickKeepSeparateCompleteContacts) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,2,{10,0,30'000'000});
    std::optional<int> held_finger;
    std::size_t peak_contacts=0;
    for(int i=0;i<35;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=mixed_hold_scene(8863,i,NoteKind::flick);
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        for(const auto& target:scene.targets) if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&
            *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        peak_contacts=std::max(peak_contacts,touch.contacts().size());
        if(i>=17&&i<=29) {
            const auto held=std::find_if(scene.targets.begin(),scene.targets.end(),
                [](const auto& target){return target.note.kind==NoteKind::hold;});
            ASSERT_NE(held,scene.targets.end())<<decision_json(scene).dump();
            const auto current=std::find_if(touch.contacts().begin(),touch.contacts().end(),
                [&](const auto& contact){return std::abs(contact.second[0]-held->hit.x)<80;});
            ASSERT_NE(current,touch.contacts().end())<<decision_json(scene).dump();
            if(!held_finger)held_finger=current->first;
            EXPECT_EQ(current->first,*held_finger);
        }
    }
    EXPECT_TRUE(held_finger.has_value());EXPECT_EQ(peak_contacts,2);
    std::optional<std::uint64_t> flick_intent;
    for(const auto& receipt:touch.receipts()) if(receipt.command.phase==Phase::down&&
        receipt.command.x>750) flick_intent=receipt.command.intent_id;
    ASSERT_TRUE(flick_intent);
    std::vector<Phase> phases;
    for(const auto& receipt:touch.receipts()) if(receipt.command.intent_id==*flick_intent)
        phases.push_back(receipt.command.phase);
    EXPECT_EQ(phases,(std::vector<Phase>{Phase::down,Phase::move,Phase::move,
        Phase::move,Phase::move,Phase::up}));
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),2);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbH2, VisibleHoldAndLaterDragKeepSeparateContactOwnership) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,2,{6,0,30'000'000});
    std::optional<int> held_finger;std::size_t peak_contacts=0;
    bool drag_observed=false;
    for(int i=0;i<35;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=mixed_hold_scene(8864,i,NoteKind::drag);
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        drag_observed=drag_observed||std::any_of(scene.targets.begin(),scene.targets.end(),
            [](const auto& target){return target.note.kind==NoteKind::drag&&target.crossing_ns;});
        for(const auto& target:scene.targets) if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&
            *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        peak_contacts=std::max(peak_contacts,touch.contacts().size());
        if(i>=17&&i<=29) {
            const auto held=std::find_if(scene.targets.begin(),scene.targets.end(),
                [](const auto& target){return target.note.kind==NoteKind::hold;});
            ASSERT_NE(held,scene.targets.end())<<decision_json(scene).dump();
            const auto current=std::find_if(touch.contacts().begin(),touch.contacts().end(),
                [&](const auto& contact){return std::abs(contact.second[0]-held->hit.x)<80;});
            ASSERT_NE(current,touch.contacts().end())<<decision_json(scene).dump();
            if(!held_finger)held_finger=current->first;
            EXPECT_EQ(current->first,*held_finger);
        }
    }
    EXPECT_TRUE(drag_observed);EXPECT_TRUE(held_finger.has_value());
    EXPECT_EQ(peak_contacts,2);
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),2);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbR2, OppositeSideHoldsAndFlickRequireThreeIndependentContacts) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,3,{10,0,30'000'000});
    std::size_t peak_contacts=0;
    DecisionSnapshot final_scene;
    for(int i=0;i<16;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=blank_frame(8865);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,480},{1,0},1050,3,{255,255,255});
        for(const auto [x,side]:{std::pair{410.0,-1},std::pair{760.0,1}}) {
            const double phase=side>0?4.0:0.0;
            const double y=480+side*(105-(i+phase)*14);
            fill_box(frame,{x,y},{1,0},112,145,{40,190,255});
            for(const int edge:{-1,1})
                fill_box(frame,{x+edge*58,y},{0,1},145,3,{245,245,245});
        }
        fill_box(frame,{1030.0,375.0+i*14},{1,0},78,9,{255,80,100});
        clock.set(frame.capture_complete_ns);
        final_scene=observer.process(frame);owner.accept(final_scene);owner.poll();
        for(const auto& target:final_scene.targets) if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&
            *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        peak_contacts=std::max(peak_contacts,touch.contacts().size());
    }
    EXPECT_EQ(std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;}),3)
        <<decision_json(final_scene).dump()<<" rejection="<<owner.last_rejection();
    nlohmann::json receipt_summary=nlohmann::json::array();
    Nanoseconds first_hold_down=0,second_hold_down=0,flick_down=0;
    for(const auto& receipt:touch.receipts()) receipt_summary.push_back({
        {"phase",static_cast<int>(receipt.command.phase)},
        {"x",receipt.command.x},{"time_ns",receipt.injection_start_ns},
        {"contact",receipt.command.contact_id}});
    for(const auto& receipt:touch.receipts()) if(receipt.command.phase==Phase::down) {
        if(receipt.command.x<550)first_hold_down=receipt.injection_start_ns;
        else if(receipt.command.x<900)second_hold_down=receipt.injection_start_ns;
        else flick_down=receipt.injection_start_ns;
    }
    EXPECT_GT(first_hold_down,0);EXPECT_GT(second_hold_down,0);EXPECT_GT(flick_down,0);
    EXPECT_LE(first_hold_down,flick_down);EXPECT_LE(second_hold_down,flick_down);
    EXPECT_EQ(peak_contacts,3)<<receipt_summary.dump()<<decision_json(final_scene).dump();
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbR2, TwoFingerLimitRejectsFlickWithoutReleasingOppositeSideHolds) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,2,{10,0,30'000'000});
    bool contention_observed=false;std::size_t peak_contacts=0;
    for(int i=0;i<16;++i) {
        SCOPED_TRACE("frame="+std::to_string(i));
        auto frame=blank_frame(8866);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,480},{1,0},1050,3,{255,255,255});
        for(const auto [x,side]:{std::pair{410.0,-1},std::pair{760.0,1}}) {
            const double phase=side>0?4.0:0.0;
            const double y=480+side*(105-(i+phase)*14);
            fill_box(frame,{x,y},{1,0},112,145,{40,190,255});
            for(const int edge:{-1,1})
                fill_box(frame,{x+edge*58,y},{0,1},145,3,{245,245,245});
        }
        fill_box(frame,{1030.0,375.0+i*14},{1,0},78,9,{255,80,100});
        clock.set(frame.capture_complete_ns);
        const auto scene=observer.process(frame);owner.accept(scene);owner.poll();
        for(const auto& target:scene.targets) if(target.crossing_ns&&
            *target.crossing_ns>clock.now_ns()&&
            *target.crossing_ns<frame.capture_complete_ns+20'000'000LL) {
            clock.set(*target.crossing_ns);owner.poll();
        }
        const auto eligible=std::any_of(scene.targets.begin(),scene.targets.end(),
            [&](const auto& target){return target.note.kind==NoteKind::flick&&
                target.crossing_ns&&target.reason=="prediction_observe_only"&&
                *target.crossing_ns-clock.now_ns()<=60'000'000;});
        if(eligible&&touch.contacts().size()==2) {
            contention_observed=true;
            EXPECT_TRUE(std::all_of(touch.contacts().begin(),touch.contacts().end(),
                [](const auto& contact){return contact.second[0]<900;}));
        }
        peak_contacts=std::max(peak_contacts,touch.contacts().size());
    }
    EXPECT_TRUE(contention_observed);EXPECT_EQ(peak_contacts,2);
    int first_hold_down=0,second_hold_down=0,flick_down=0;
    for(const auto& receipt:touch.receipts()) if(receipt.command.phase==Phase::down) {
        if(receipt.command.x<550)++first_hold_down;
        else if(receipt.command.x<900)++second_hold_down;
        else ++flick_down;
    }
    EXPECT_EQ(first_hold_down,1);EXPECT_EQ(second_hold_down,1);EXPECT_EQ(flick_down,0);
    owner.stop();EXPECT_TRUE(touch.contacts().empty());
}

TEST(ColdRgbH1, UnknownTailCannotExtendBodyContactAfterLineLosesCurrentPixels) {
    FakeClock clock;FakeTouchBackend touch(clock);GameObserver observer(clock);
    GamePlanOwner owner(clock,touch,1,{2,0,30'000'000});
    std::size_t moves_at_last_contact=0;
    for(int i=0;i<35;++i) {
        auto frame=blank_frame(7703);draw_hud(frame);
        frame.sequence=i+1;frame.capture_complete_ns=1'000'000'000LL+i*20'000'000LL;
        fill_box(frame,{640,400},{1,0},1050,3,{255,255,255});
        const double head=280+i*10,center_y=head-90;
        fill_box(frame,{492,center_y},{1,0},144,180,{40,190,255});
        fill_box(frame,{418,center_y},{0,1},180,4,{245,245,245});
        fill_box(frame,{566,center_y},{0,1},180,4,{245,245,245});
        clock.set(frame.capture_complete_ns);const auto scene=observer.process(frame);
        owner.accept(scene);owner.poll();
        const auto moves=std::count_if(touch.receipts().begin(),touch.receipts().end(),
            [](const auto& receipt){return receipt.command.phase==Phase::move;});
        if(i==30)moves_at_last_contact=moves;
        if(i>30)EXPECT_EQ(moves,moves_at_last_contact)<<"frame="<<i<<" "<<decision_json(scene).dump();
    }
    const auto downs=std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::down;});
    const auto ups=std::count_if(touch.receipts().begin(),touch.receipts().end(),
        [](const auto& receipt){return receipt.command.phase==Phase::up;});
    EXPECT_EQ(downs,1);EXPECT_EQ(ups,1);EXPECT_TRUE(touch.contacts().empty());
    const auto updates=owner.take_coverage_updates();
    EXPECT_FALSE(std::any_of(updates.begin(),updates.end(),[](const auto& update){
        return update.value("event",std::string{})=="game_hold_tail_confirmed";
    }));
    owner.stop();
}
