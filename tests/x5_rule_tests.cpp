#include "x5_adapter.hpp"
#include "x5_scenarios.hpp"
#include "x5_report.hpp"
#include "x5_scope.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <iostream>
#include <sstream>
using namespace pas::x5;
namespace {
Input rightward(){auto in=approach_fixture();in.center={440,400};in.tangent={1,0};for(std::size_t k=0;k<3;++k)in.samples[k].note={400+20.*k,400};return in;}
Input transform(Input in,double a,Point offset){auto rotate=[&](Point p){return Point{p.x*std::cos(a)-p.y*std::sin(a),p.x*std::sin(a)+p.y*std::cos(a)};};auto translated=[&](Point p){auto r=rotate(p);return Point{r.x+offset.x,r.y+offset.y};};in.center=translated(in.center);in.tangent=rotate(in.tangent);for(std::size_t j=0;j<in.line_count;++j){in.lines[j].center=translated(in.lines[j].center);in.lines[j].tangent=rotate(in.lines[j].tangent);}for(std::size_t k=0;k<in.sample_count;++k){in.samples[k].note=translated(in.samples[k].note);for(std::size_t j=0;j<in.line_count;++j)if(in.samples[k].lines[j]){in.samples[k].lines[j]->center=translated(in.samples[k].lines[j]->center);in.samples[k].lines[j]->tangent=rotate(in.samples[k].lines[j]->tangent);}}return in;}
TEST(X5Rule, FirstObservationCannotBorrowNextFrameMotion){auto in=rightward();in.sample_count=1;in.samples[0]=in.samples[2];auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_EQ(d.features[1].reason,"paired_history_missing");}
TEST(X5Rule, TwoPosesKeepMeasuredSecantButDoNotMeetRuleHistory){auto in=rightward();in.sample_count=2;in.samples[0]=in.samples[1];in.samples[1]=in.samples[2];auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_EQ(d.features[1].measured_poses,2u);}
TEST(X5Rule, FarNormalApproachBeatsNearTangentialLine){auto in=rightward();in.lines[0].center.y=410;for(std::size_t k=0;k<3;++k)in.samples[k].lines[0]=in.lines[0];auto d=evaluate(in);ASSERT_TRUE(d.selected);EXPECT_EQ(*d.selected,1u);EXPECT_DOUBLE_EQ(*d.features[1].relative_signed_rate,-1000);EXPECT_DOUBLE_EQ(*d.features[0].note_normal_rate,0);EXPECT_LT(std::abs(d.features[0].distance),std::abs(d.features[1].distance));}
TEST(X5Rule, LateAlignmentAppearanceIsNotNecessary){auto in=approach_fixture();EXPECT_DOUBLE_EQ(dot(in.tangent,in.lines[0].tangent),0);auto d=evaluate(in);ASSERT_TRUE(d.selected);EXPECT_EQ(*d.selected,0u);in.tangent={1,0};EXPECT_EQ(evaluate(in).selected,d.selected);}
TEST(X5Rule, OriginalCrossingGeometryCannotStealConfirmedRole){auto in=approach_fixture();in.center={423,352};in.lines[1].center={423,490};in.lines[1].length=500;for(std::size_t k=0;k<3;++k){in.samples[k].note={423,326.+13*k};in.samples[k].lines[1]=in.lines[1];}in.confirmation=Confirmation::confirmed;auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_EQ(d.reason,"confirmed_relation_outside_early_rule");EXPECT_FALSE(d.features[1].eligible);}
TEST(X5Rule, CandidatePermutationChangesOnlyIndex){auto in=rightward();auto a=evaluate(in);std::swap(in.lines[0],in.lines[1]);for(std::size_t k=0;k<3;++k)std::swap(in.samples[k].lines[0],in.samples[k].lines[1]);auto b=evaluate(in);ASSERT_TRUE(a.selected);ASSERT_TRUE(b.selected);EXPECT_EQ(*a.selected,1u);EXPECT_EQ(*b.selected,0u);EXPECT_EQ(a.reason,b.reason);}
TEST(X5Rule, TranslationAndReasonableRotationPreserveGeometryConclusion){const auto in=rightward();const auto d=evaluate(in);for(double angle:{-.7,.3,1.1}){const auto t=transform(in,angle,{150,-80});const auto v=evaluate(t);EXPECT_EQ(v.selected,d.selected);EXPECT_EQ(v.reason,d.reason);for(std::size_t j=0;j<2;++j){EXPECT_NEAR(v.features[j].distance,d.features[j].distance,1e-8);EXPECT_NEAR(*v.features[j].relative_signed_rate,*d.features[j].relative_signed_rate,1e-7);}}}
TEST(X5Rule, TangentSignFlipIsNotRotationOrReversal){auto in=rightward();auto d=evaluate(in);for(std::size_t j=0;j<2;++j){in.lines[j].tangent={-in.lines[j].tangent.x,-in.lines[j].tangent.y};in.samples[1].lines[j]->tangent=in.lines[j].tangent;in.samples[2].lines[j]->tangent=in.lines[j].tangent;}EXPECT_EQ(evaluate(in).selected,d.selected);}
TEST(X5Rule, SameDirectionNeighborAndExactTieAbstain){auto in=rightward();in.lines[0]=in.lines[1];in.lines[0].center.x=880;for(std::size_t k=0;k<3;++k)in.samples[k].lines[0]=in.lines[0];auto d=evaluate(in);EXPECT_FALSE(d.selected);ASSERT_TRUE(d.margin);EXPECT_DOUBLE_EQ(*d.margin,0);EXPECT_EQ(d.reason,"competition_margin_below_015");}
TEST(X5Rule, MarginBelowDeclaredValueAbstains){auto in=rightward();const double a=.1;in.lines[0].center={800,400};in.lines[0].tangent={std::sin(a),std::cos(a)};for(std::size_t k=0;k<3;++k)in.samples[k].lines[0]=in.lines[0];auto d=evaluate(in);EXPECT_FALSE(d.selected);ASSERT_TRUE(d.margin);EXPECT_LT(*d.margin,.15);}
TEST(X5Rule, MovingLineClosingIsUnresolvedCompetition){auto in=rightward();in.lines[0].center.y=450;for(std::size_t k=0;k<3;++k){auto l=in.lines[0];l.center.y=490-20.*k;in.samples[k].lines[0]=l;}auto d=evaluate(in);EXPECT_DOUBLE_EQ(*d.features[0].closing_rate,-1000);EXPECT_DOUBLE_EQ(*d.features[0].note_normal_rate,0);EXPECT_TRUE(d.features[0].unresolved);EXPECT_FALSE(d.selected);EXPECT_EQ(d.reason,"unresolved_current_competitor");}
TEST(X5Rule, LineChasingStationaryNoteAbstainsInsteadOfUsingClosingAlone){auto in=approach_fixture();in.center={500,500};for(std::size_t k=0;k<3;++k){in.samples[k].note=in.center;auto l=in.lines[0];l.center.y=590-20.*k;in.samples[k].lines[0]=l;}in.lines[0]=*in.samples[2].lines[0];auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_DOUBLE_EQ(*d.features[0].closing_rate,-1000);EXPECT_DOUBLE_EQ(norm(*d.features[0].note_velocity),0);}
TEST(X5Rule, RotationWhileApproachingIsExplicitlyOutsideStableRegime){auto in=rightward();for(std::size_t k=0;k<3;++k){const double a=.2*k;auto l=in.lines[1];l.tangent={-std::sin(a),std::cos(a)};in.samples[k].lines[1]=l;}in.lines[1]=*in.samples[2].lines[1];auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_GT(*d.features[1].max_angle,.10);}
TEST(X5Rule, NoteTurnsNearLineWithoutEnoughNewStableHistoryAbstains){auto in=rightward();in.samples[1].note={400,420};in.samples[2].note={420,420};in.center=in.samples[2].note;auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_LT(*d.features[1].normal_fraction,.85);}
TEST(X5Rule, ShortFragmentNeverUsesLongLineSupport){auto in=rightward();in.lines[1].length=100;for(std::size_t k=0;k<3;++k)in.samples[k].lines[1]=in.lines[1];auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_EQ(d.features[1].reason,"span_extent_or_confidence_insufficient");}
TEST(X5Rule, MissingLineAndHistoryStayUnknown){auto in=rightward();in.samples[0].lines[1].reset();auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_EQ(d.features[1].reason,"paired_history_missing");in.line_count=0;EXPECT_FALSE(evaluate(in).selected);}
TEST(X5Rule, StaleOrInvalidCurrentCandidateIsNeverSelected){for(bool stale:{false,true}){auto in=rightward();in.lines[1].current=!stale;in.lines[1].valid=stale;auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_EQ(d.features[1].reason,"current_pose_invalid_or_stale");}}
TEST(X5Rule, AmbiguousOrUnknownIdentityAndMissingConfirmationAbstain){auto in=rightward();for(auto identity:{Identity::unknown,Identity::ambiguous}){in.identity=identity;EXPECT_FALSE(evaluate(in).selected);}in.identity=Identity::unique;in.confirmation=Confirmation::unknown;EXPECT_EQ(evaluate(in).reason,"confirmation_not_exported");}
TEST(X5Rule, HoldHeadBodyTailCannotGenerateContactOrAlterActiveOwner){auto in=rightward();in.hold_or_body=true;auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_EQ(d.reason,"hold_head_body_tail_outside_rule");}
TEST(X5Rule, OverlapAndPassedReturnAreNotRoleProof){auto in=rightward();in.lines[1].center.x=440;for(std::size_t k=0;k<3;++k)in.samples[k].lines[1]=in.lines[1];auto d=evaluate(in);EXPECT_FALSE(d.selected);EXPECT_EQ(d.features[1].reason,"overlap_or_crossed_not_role_proof");in.lines[1].center.x=430;for(std::size_t k=0;k<3;++k)in.samples[k].lines[1]=in.lines[1];EXPECT_FALSE(evaluate(in).selected);}
TEST(X5Rule, NonfiniteCapacityAndTimeFailClosed){
    auto in=rightward();in.line_count=17;
    EXPECT_THROW(evaluate(in),std::runtime_error);
    in=rightward();in.sample_count=7;
    EXPECT_THROW(evaluate(in),std::runtime_error);
    in=rightward();in.samples[0].ns=-1;
    EXPECT_THROW(evaluate(in),std::runtime_error);
    in=rightward();in.center.x=std::numeric_limits<double>::infinity();
    EXPECT_THROW(evaluate(in),std::runtime_error);
}
TEST(X5RuleFalsifier, IdenticalPrefixWithLateTurnDefeatsUniqueRoleSufficiency){
    const auto in=indistinguishable_late_turn_prefix();const auto d=evaluate(in);ASSERT_TRUE(d.selected);
    // Independent scenario labels: A hits y576, B turns toward x900 after
    // the available prefix. The rule cannot inspect those future motions.
    constexpr std::size_t world_a_role=0,world_b_role=1;
    EXPECT_EQ(*d.selected,world_a_role);EXPECT_NE(*d.selected,world_b_role);
    EXPECT_GT(std::abs(d.features[1].distance),0);EXPECT_EQ(d.features[1].reason,"not_sustained_closing");
    // This is a preserved semantic FAILURE of NDA-v1, not a passing role
    // classifier. Tests pass only when the falsifier remains reproducible.
}
json raw_line(std::uint64_t id,std::int64_t now){return {{"center",json::array({900.,360.})},{"tangent",json::array({0.,1.})},{"association_valid",true},{"motion_valid",false},{"length",720.},{"confidence",.9},{"observed_ns",now},{"line_id",id},{"velocity",json::array({0.,0.})},{"angular_velocity",0.}};}
json row(std::uint64_t id,std::uint64_t lineid,int k){const std::int64_t now=k*20'000'000;const double x=400+20.*k;return {{"candidate_bank",{{"context",{{"capture_ns",now},{"width",1280}}},{"source_valid",true},{"capacity_valid",true},{"lines",json::array({raw_line(lineid,now)})},{"candidates",json::array({{{"candidate_id",1},{"quality","strong_current"},{"note",{{"center",json::array({x,400.})},{"tangent",json::array({1.,0.})}}}}})}}},{"scene",{{"targets",json::array({{{"note_id",id},{"kind","drag"},{"x",x},{"y",400.},{"width",128.},{"height",8.},{"ux",1.},{"uy",0.}}})}}},{"diagnostics",json::array({{{"event","candidate_track"},{"note_id",id},{"candidate_id",1},{"identity_ambiguous",false},{"assigned_prior_index",k?0:-1},{"prior_observed_ns",k?(k-1)*20'000'000:now},{"prior_x",k?x-20:x},{"prior_y",400.}},{{"event","relation_selection"},{"note_id",id},{"confirmed_line_id",0},{"winner",lineid}}})}};}
TEST(X5Adapter, LocalIDRenamingPreservesGeometryAndInvalidModelDoesNotEraseSecant){
    for(auto id:{1ULL,987654ULL}){Adapter adapter;Input in;json last;for(int k=0;k<3;++k){last=row(id,id+100,k);const auto f=adapter.frame(last);std::string why;in=adapter.input(last,last.at("scene").at("targets")[0],f,why);adapter.commit(f);}const auto d=evaluate(in);ASSERT_TRUE(d.selected);EXPECT_EQ(*d.selected,0u);const auto f=feature_json(d.features[0],last.at("candidate_bank").at("lines")[0],in,0);EXPECT_EQ(f.at("motion_model_velocity"),nullptr);EXPECT_EQ(f.at("latest_measured_secant").at("valid"),true);EXPECT_EQ(f.at("latest_measured_secant").at("absolute_distance_rate_px_s"),-1000);}
}
TEST(X5Adapter, MissingOrAmbiguousPriorCannotBecomeHistory){Adapter adapter;auto a=row(1,2,0);a["diagnostics"][0]["identity_ambiguous"]=true;adapter.commit(adapter.frame(a));const auto b=row(1,2,1);const auto f=adapter.frame(b);std::string why;const auto in=adapter.input(b,b.at("scene").at("targets")[0],f,why);EXPECT_EQ(in.sample_count,1u);EXPECT_EQ(why,"prior_identity_ambiguous");}
TEST(X5Adapter, StalePriorCannotBorrowNoteTimeForMeasuredSecant){
    // Equal geometry and identity; only the previous line's actual timestamp
    // changes. Invalid fitted motion must not erase a valid fresh secant.
    for(const auto age:{0LL,1LL,5'000'000LL}){
        Adapter adapter;Input in;json last;
        for(int k=0;k<3;++k){
            last=row(1,2,k);
            if(k==1)last["candidate_bank"]["lines"][0]["observed_ns"]=20'000'000-age;
            const auto frame=adapter.frame(last);std::string why;
            in=adapter.input(last,last.at("scene").at("targets")[0],frame,why);adapter.commit(frame);
        }
        const auto d=evaluate(in);const auto f=feature_json(d.features[0],last["candidate_bank"]["lines"][0],in,0);
        const auto& s=f.at("latest_measured_secant");
        EXPECT_EQ(s.at("valid"),age==0);EXPECT_EQ(d.selected.has_value(),age==0);
        EXPECT_EQ(f.at("motion_model_valid"),false);
        if(age){EXPECT_EQ(s.at("reason"),"line_not_current_valid_at_both_note_times");EXPECT_TRUE(s.value("note_velocity_px_s",json(nullptr)).is_null());}
        else EXPECT_EQ(s.at("absolute_distance_rate_px_s"),-1000);
    }
}
TEST(X5Adapter, CurrentStaleAndInvalidPriorDoNotYieldSecants){
    for(const bool stale_current:{false,true}){
        Adapter adapter;Input in;json last;
        for(int k=0;k<3;++k){last=row(1,2,k);
            if(stale_current&&k==2)last["candidate_bank"]["lines"][0]["observed_ns"]=39'000'000;
            if(!stale_current&&k==1)last["candidate_bank"]["lines"][0]["association_valid"]=false;
            const auto frame=adapter.frame(last);std::string why;in=adapter.input(last,last["scene"]["targets"][0],frame,why);adapter.commit(frame);
        }
        const auto d=evaluate(in);EXPECT_FALSE(d.selected);
        const auto s=feature_json(d.features[0],last["candidate_bank"]["lines"][0],in,0).at("latest_measured_secant");
        EXPECT_EQ(s.at("valid"),false);EXPECT_EQ(s.at("reason"),"line_not_current_valid_at_both_note_times");
    }
}
ScopeEvidence early_c36h(){ScopeEvidence e;e.lineage=Lineage::c36h;e.strong_current=true;e.identity=Identity::unique;e.preserve=false;e.prior_samples=2;return e;}
TEST(X5Scope, C36hEarlyStratumUsesPreselectionRelationSampleCount){
    auto e=early_c36h();for(std::size_t n:{0u,1u,2u}){e.prior_samples=n;const auto s=research_scope(e);EXPECT_TRUE(s.early_comparison_eligible);EXPECT_STREQ(s.confirmation_semantics,"not_applicable_no_persistent_confirmation_state");}
    e.prior_samples=3;EXPECT_FALSE(research_scope(e).early_comparison_eligible);
    EXPECT_STREQ(research_scope(e).reason,"c36h_nonpreserved_mature_history_preconditions_unknown");
}
TEST(X5Scope, NonpreservationIsNotUnconfirmedAndMissingStateIsNotZero){
    auto e=early_c36h();e.prior_samples.reset();EXPECT_FALSE(research_scope(e).early_comparison_eligible);
    e=early_c36h();e.preserve.reset();EXPECT_FALSE(research_scope(e).early_comparison_eligible);
    e=early_c36h();e.confirmation=Confirmation::unconfirmed;EXPECT_STREQ(research_scope(e).reason,"lineage_state_contradiction");
    e=early_c36h();e.preserve=true;EXPECT_STREQ(research_scope(e).reason,"lineage_state_contradiction");
    e.prior_samples=3;EXPECT_STREQ(research_scope(e).reason,"c36h_current_preserve_taken");
}
TEST(X5Scope, Main50RequiresExplicitConfirmationInsteadOfC36hSampleThreshold){
    auto e=early_c36h();e.lineage=Lineage::main50;EXPECT_FALSE(research_scope(e).early_comparison_eligible);
    e.confirmation=Confirmation::unconfirmed;e.prior_samples=30;EXPECT_TRUE(research_scope(e).early_comparison_eligible);
    e.confirmation=Confirmation::confirmed;e.prior_samples=0;EXPECT_FALSE(research_scope(e).early_comparison_eligible);
    e.confirmation=Confirmation::unconfirmed;e.preserve=true;EXPECT_STREQ(research_scope(e).reason,"lineage_state_contradiction");
}
TEST(X5Scope, HoldWeakCurrentAmbiguousAndUnknownLineageRemainExcluded){
    auto e=early_c36h();e.hold_or_body=true;EXPECT_FALSE(research_scope(e).early_comparison_eligible);
    e=early_c36h();e.strong_current=false;EXPECT_FALSE(research_scope(e).early_comparison_eligible);
    for(auto id:{Identity::ambiguous,Identity::unknown}){e=early_c36h();e.identity=id;EXPECT_FALSE(research_scope(e).early_comparison_eligible);}
    e=early_c36h();e.lineage=Lineage::unknown;EXPECT_FALSE(research_scope(e).early_comparison_eligible);
}
TEST(X5Scope, ReportAdapterDoesNotCoerceMissingOrFloatState){
    json o={{"kind","drag"},{"current_strong",true},{"identity_assignment",{{"identity_ambiguous",false},{"prior_samples",2}}},{"original_selection_comparison_only",{{"preserve",false}}}};
    auto e=scope_evidence(Lineage::c36h,o);EXPECT_EQ(e.confirmation,Confirmation::unknown);EXPECT_TRUE(research_scope(e).early_comparison_eligible);
    o["decision_reason"]="hold_head_body_tail_outside_rule";EXPECT_TRUE(scope_evidence(Lineage::c36h,o).hold_or_body);o.erase("decision_reason");
    o["identity_assignment"]["prior_samples"]=2.5;EXPECT_THROW(scope_evidence(Lineage::c36h,o),std::runtime_error);
    o["identity_assignment"].erase("prior_samples");EXPECT_FALSE(research_scope(scope_evidence(Lineage::c36h,o)).early_comparison_eligible);
    o["original_selection_comparison_only"]["confirmed_line_id"]=0.0;EXPECT_THROW(scope_evidence(Lineage::main50,o),std::runtime_error);
}
TEST(X5Adapter, FloatIDAndBadGeometryRejectInsteadOfCoercing){
    EXPECT_THROW(integer(json(1.0)),std::runtime_error);
    auto r=row(1,2,0);r["candidate_bank"]["lines"][0]["tangent"]=json::array({2.,0.});Adapter adapter;
    EXPECT_THROW(adapter.frame(r),std::runtime_error);
}
TEST(X5Adapter, FrameHistoryIsBoundedAndExpires){Adapter adapter;for(int k=0;k<9;++k){const auto r=row(1,2,k);adapter.expire(k*20'000'000);adapter.commit(adapter.frame(r));EXPECT_LE(adapter.history.size(),5u);}adapter.expire(400'000'000);EXPECT_TRUE(adapter.history.empty());}
TEST(X5CLI, MissingInputIsActualPathRejectionNotMutexBusy){char app[]="x5_report",missing[]="Z:/x5-deliberately-absent-query.json",out[]="Z:/x5-never-created-report.json";char* argv[]={app,missing,out};std::ostringstream errors;auto* old=std::cerr.rdbuf(errors.rdbuf());const auto rc=report_main(3,argv);std::cerr.rdbuf(old);EXPECT_NE(rc,0);EXPECT_EQ(errors.str().find("batch_writer_busy"),std::string::npos);EXPECT_NE(errors.str().find("file_size"),std::string::npos);}
}
