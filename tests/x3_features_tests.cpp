#include "x3_features.hpp"
#include "x3_report.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
using namespace pas::x3;
namespace {
Line horizontal(double y,std::int64_t time,std::uint64_t id=1){return {{0,y},{1,0},true,false,1280,time,id};}
Line vertical(double x,std::int64_t time,std::uint64_t id=2){return {{x,0},{0,1},true,false,720,time,id};}
TEST(X3Features, CrossingControlKeepsClosingHorizontalAndZeroNormalCompetitor) {
    // Original crossing control frame4->5: x423, y339->352, horizontal y576.
    const auto h=measured_pair({423,352},horizontal(576,100'000'000),{423,339},horizontal(576,80'000'000),80'000'000,100'000'000);
    EXPECT_TRUE(h.at("valid"));EXPECT_NEAR(h.at("distance_rate_px_s").get<double>(),650,1e-9);
    EXPECT_NEAR(h.at("absolute_distance_rate_px_s").get<double>(),-650,1e-9);
    const auto v=measured_pair({423,352},vertical(423,100'000'000),{423,339},vertical(423,80'000'000),80'000'000,100'000'000);
    EXPECT_TRUE(v.at("valid"));EXPECT_EQ(v.at("distance_rate_px_s"),0);EXPECT_EQ(v.at("note_normal_velocity_px_s"),0);
}
TEST(X3Features, CorrectAndWrongInitialRelationsCanBeComparedAfterMeasuredMotion) {
    // Analytic truth: object translates right 20px/20ms toward stationary x100.
    const Point a{20,50},b{40,50};
    const auto right=measured_pair(b,vertical(100,20'000'000),a,vertical(100,0),0,20'000'000);
    const auto wrong=measured_pair(b,horizontal(45,20'000'000),a,horizontal(45,0),0,20'000'000);
    EXPECT_EQ(right.at("absolute_distance_rate_px_s"),-1000);EXPECT_EQ(wrong.at("absolute_distance_rate_px_s"),0);
    EXPECT_EQ(wrong.at("note_normal_velocity_px_s"),0);EXPECT_EQ(wrong.at("note_tangent_velocity_px_s"),1000);
    // Initial pose alone has no measured trajectory, even if appearance aligns.
    EXPECT_FALSE(measured_pair(a,vertical(100,0),a,{},0,0).at("valid"));
}
TEST(X3Features, MovingWrongLineAlsoClosesDistanceSoClosingIsNotRoleTruth) {
    const auto misleading=measured_pair({40,50},horizontal(48,20'000'000),{20,50},horizontal(40,0),0,20'000'000);
    EXPECT_EQ(misleading.at("absolute_distance_rate_px_s"),-400);
    EXPECT_EQ(misleading.at("note_normal_velocity_px_s"),0); // Line moved; no normal note approach.
}
TEST(X3Features, FragmentIDCannotSupplyMotionButCanPassLocalGeometry) {
    const auto old=horizontal(50,0,1),fragment=horizontal(52,20'000'000,17);
    EXPECT_FALSE(measured_pair({30,40},fragment,{10,40},old,0,20'000'000).at("valid"));
    const auto g=continuation(old,{10,40},0,fragment,false,20'000'000);
    EXPECT_TRUE(g.at("local_geometry_continuation"));EXPECT_EQ(g.at("prior_hit_normal_gap_px"),2);
}
TEST(X3Features, OldVisibleAndMissingOrthogonalHaveDifferentFirstGuards) {
    const auto old=horizontal(50,0),new_line=vertical(10,20'000'000);
    const auto a=continuation(old,{10,40},0,new_line,true,20'000'000),b=continuation(old,{10,40},0,new_line,false,20'000'000);
    EXPECT_EQ(a.at("first_geometry_guard"),"old_line_currently_visible");
    EXPECT_EQ(b.at("first_geometry_guard"),"tangent_not_local_continuation");EXPECT_EQ(b.at("abs_tangent_dot"),0);
}
TEST(X3Features, SameOrientationNeighborAtAnotherNormalPositionFailsContinuation) {
    const auto g=continuation(horizontal(50,0),{10,40},0,horizontal(100,20'000'000,3),false,20'000'000);
    EXPECT_FALSE(g.at("local_geometry_continuation"));EXPECT_EQ(g.at("prior_hit_normal_gap_px"),50);EXPECT_EQ(g.at("first_geometry_guard"),"prior_hit_normal_gap");
}
TEST(X3Features, LateAppearanceAlignmentDoesNotAlterMeasuredFeatures) {
    // Features take measured positions/line only; distant appearance is not a gate.
    const auto a=measured_pair({30,40},vertical(100,20'000'000),{10,40},vertical(100,0),0,20'000'000);
    EXPECT_TRUE(a.at("valid"));EXPECT_EQ(a.at("note_normal_velocity_px_s"),-1000);
}
TEST(X3Features, RotationWhileHeldIncludesChangingNormalAndSignFlipIsEquivalent) {
    // Stationary body point (10,0); line rotates 30deg about origin in 20ms.
    Line initial{{0,0},{1,0},true,false,1280,0,1},rotated{{0,0},{std::sqrt(3.)/2,.5},true,false,1280,20'000'000,1};
    const auto r=measured_pair({10,0},rotated,{10,0},initial,0,20'000'000);
    EXPECT_TRUE(r.at("valid"));EXPECT_NEAR(r.at("distance_rate_px_s").get<double>(),-250,1e-8);EXPECT_EQ(r.at("note_normal_velocity_px_s"),0);
    rotated.u={-rotated.u.x,-rotated.u.y};const auto flip=measured_pair({10,0},rotated,{10,0},initial,0,20'000'000);
    EXPECT_EQ(r,flip);
}
TEST(X3Features, InvalidMotionModelDoesNotInvalidateMeasuredPoseSecant) {
    auto l=horizontal(50,20'000'000);l.motion=false;
    const auto p=measured_pair({10,40},l,{10,30},horizontal(50,0),0,20'000'000);
    EXPECT_TRUE(p.at("valid"));EXPECT_EQ(p.at("distance_rate_px_s"),500);
    l.valid=false;EXPECT_FALSE(measured_pair({10,40},l,{10,30},horizontal(50,0),0,20'000'000).at("valid"));
}
TEST(X3Features, AbsenceWrongTimeAndUnknownHistoryDoNotFabricateVelocity) {
    for(const auto dt:{0LL,-1LL,90'000'001LL}){
        auto current=horizontal(50,std::max(0LL,dt));const auto p=measured_pair({10,40},current,{10,30},horizontal(50,0),0,dt);
        EXPECT_FALSE(p.at("valid"));EXPECT_TRUE(p.at("distance_rate_px_s").is_null());
    }
    EXPECT_FALSE(measured_pair({10,40},horizontal(50,20'000'000),{10,30},{},0,20'000'000).at("valid"));
    EXPECT_EQ(continuation({}, {},0,vertical(50,20'000'000),false,20'000'000).at("first_geometry_guard"),"prior_point_unknown");
}
TEST(X3Features, UnknownDownAndCompletedStatesNeverBecomeFreshEligibility) {
    json id={{"submitted",true},{"cursor",nullptr},{"prefix_offset",0}};
    const auto command=[](int phase){return json{{"command",{{"phase",phase},{"contact_id",2}}},{"success",true}};};
    auto receipts=json{{"successful_down",command(0)},{"last_receipt",command(2)}};
    EXPECT_EQ(owner_status(id,receipts).at("state"),"up_receipt_completed_no_resurrection");
    receipts["last_receipt"]=command(0);receipts["last_receipt"]["success"]=false;
    EXPECT_EQ(owner_status(id,receipts).at("state"),"unknown_receipt_no_retry");
    EXPECT_EQ(owner_status(id,nullptr).at("state"),"retired_or_completed_or_unknown_no_retry");
    EXPECT_EQ(owner_status(nullptr,nullptr).at("state"),"unknown");
}
TEST(X3Features, MalformedGeometryRejectsInsteadOfGuessing) {
    EXPECT_THROW(point(json::array({1})),std::runtime_error);
    EXPECT_THROW(point(json::array({std::numeric_limits<double>::infinity(),0})),std::runtime_error);
    json l={{"center",json::array({0,50})},{"tangent",json::array({0,0})},{"association_valid",true},{"motion_valid",false},{"length",100},{"observed_ns",0},{"line_id",1}};
    EXPECT_THROW(line(l),std::runtime_error);
    l["tangent"]=json::array({1,0});l.erase("motion_valid");
    EXPECT_THROW(line(l),json::exception);
}
TEST(X3Cli, MissingManifestIsNonzero) {
    std::string tool="x3-test",missing="missing-x3-manifest.json",output="missing-x3-output.json";char* args[]={tool.data(),missing.data(),output.data()};
    std::ostringstream errors;const auto old=std::cerr.rdbuf(errors.rdbuf());const auto code=report_main(3,args);std::cerr.rdbuf(old);
    EXPECT_NE(code,0);EXPECT_FALSE(std::filesystem::exists(output));
    EXPECT_EQ(errors.str().find("batch_writer_busy"),std::string::npos);
    EXPECT_NE(errors.str().find("missing-x3-manifest"),std::string::npos);
}
}
