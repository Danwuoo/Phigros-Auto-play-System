#include "replay_support.hpp"
#include "replay_trace.hpp"
#include "contact_replay.hpp"
#include "review_io.hpp"
#include "pas/analysis.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
#include <gtest/gtest.h>
using namespace pas;
using namespace pas::x1;
namespace {
DecisionSnapshot snapshot(Nanoseconds time) {
    DecisionSnapshot s;s.sequence=1;s.context={1,1,1,1,time,1280,720,1};s.playing_gate=true;s.ui=GameUi::playing;
    GameTarget t;t.note_id=1;t.revision=1;t.evidence_ns=time;t.expires_ns=time+100'000'000;t.note.kind=NoteKind::tap;t.note.width=120;t.note.height=8;
    t.note.center=t.hit={500,540};t.crossing_ns=time+55'000'000;t.uncertainty_ns=2'000'000;t.reason="prediction_observe_only";t.samples=3;t.line_id=1;s.targets={t};return s;
}
}
TEST(ContactReplay, BetweenFrameDueExecutesAtItsOwnClockAndEofReleases) {
    FakeClock clock;clock.set(1'000'000'000);ReplayTouch touch(clock);SessionGameOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.start(1);
    owner.accept(snapshot(clock.now_ns()),true);const auto plans=owner.owner()->take_accepted_plans();ASSERT_EQ(plans.size(),1);owner.poll();
    std::vector<TouchReceipt> found;touch.sink=[&](json j){if(j.at("event")=="fake_receipt")EXPECT_EQ(j.at("command").at("scheduled_ns"),j.at("injection_start_ns"));};
    advance_before_frame(owner,clock,1'025'000'000,false,[]{});EXPECT_EQ(touch.contacts.size(),1);owner.finish();EXPECT_TRUE(touch.contacts.empty());
}
TEST(ContactReplay, FrameBeforeDueCanRevokeButDueFirstDispatchesTie) {
    for(bool due_first:{false,true}){
        FakeClock clock;clock.set(1'000'000'000);ReplayTouch touch(clock);SessionGameOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.start(1);
        owner.accept(snapshot(clock.now_ns()),true);owner.owner()->take_accepted_plans();owner.poll();
        advance_before_frame(owner,clock,1'020'000'000,due_first,[]{});EXPECT_EQ(touch.contacts.size(),due_first?1:0);
        auto s=snapshot(clock.now_ns());s.sequence=2;s.context.frame=2;owner.accept(s,false);owner.poll();EXPECT_TRUE(touch.contacts.empty());owner.finish();
    }
}
TEST(ContactReplay, BetweenFrameGateExpiryHasProgress) {
    FakeClock clock;clock.set(1'000'000'000);ReplayTouch touch(clock);SessionGameOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.start(1);owner.accept(snapshot(clock.now_ns()),true);owner.owner()->take_accepted_plans();
    advance_before_frame(owner,clock,1'200'000'000,false,[]{});EXPECT_FALSE(owner.scheduler()->armed());EXPECT_TRUE(touch.contacts.empty());
}
TEST(ContactReplay, UnknownDownNeverRetriesAndPreservesReleaseReport) {
    FakeClock clock;clock.set(1'000'000'000);ReplayTouch touch(clock);touch.unknown_release=true;
    SessionGameOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.start(1);owner.accept(snapshot(clock.now_ns()),true);owner.owner()->take_accepted_plans();
    touch.fail_at=touch.count;clock.set(1'020'000'000);owner.poll();EXPECT_EQ(owner.scheduler()->fault(),"release_failed");auto report=owner.scheduler()->last_release();EXPECT_EQ(report.unknown_ids,std::vector<int>{0});
    const auto count=touch.count;owner.poll();EXPECT_EQ(touch.count,count);EXPECT_TRUE(touch.contacts.empty());
}
TEST(ContactReplay, ReceiptCapacityRejectsBeforeMutationAndEmergencyReleaseIsAvailable) {
    FakeClock clock;ReplayTouch touch(clock);touch.limit_count=2;touch.inject({1,0,Phase::down,20,20,0,1});
    EXPECT_THROW(touch.inject({1,0,Phase::move,30,30,0,1}),std::runtime_error);EXPECT_EQ(touch.contacts.at(0)[0],20);touch.release_all();EXPECT_TRUE(touch.contacts.empty());EXPECT_EQ(touch.count,touch.limit_count);
    ReplayTouch bytes(clock);bytes.limit_bytes=500;EXPECT_THROW(bytes.inject({1,0,Phase::down,20,20,0,1}),std::runtime_error);EXPECT_TRUE(bytes.contacts.empty());
    ReplayTouch output_failure(clock);output_failure.inject({1,0,Phase::down,20,20,0,1});
    output_failure.unknown_release=true;output_failure.sink=[](json){throw std::runtime_error("output_quota");};
    EXPECT_THROW(output_failure.release_all(),std::runtime_error);
    EXPECT_TRUE(output_failure.contacts.empty());
    EXPECT_EQ(output_failure.last_release_report.requested_ids,std::vector<int>{0});
    EXPECT_EQ(output_failure.last_release_report.unknown_ids,std::vector<int>{0});
}
TEST(ContactReplay, TraceCapacityIsExplicitAndDrainsMemory) {
    x1_trace_enabled=true;x1_emit({{"value",1}});EXPECT_EQ(x1_drain().size(),1);EXPECT_EQ(x1_trace_bytes,0);
    EXPECT_THROW(x1_emit({{"value",std::string(16*1024*1024,'x')}}),std::runtime_error);x1_drain();x1_trace_enabled=false;
}
TEST(ContactReplay, EarlyBodyHasNoBorrowedDownAndUnknownUiHasNoRound) {
    FakeClock clock;clock.set(1'000'000'000);ReplayTouch touch(clock);SessionGameOwner owner(clock,touch,5,{15,35'000'000,30'000'000});owner.start(1);
    auto s=snapshot(clock.now_ns());s.targets[0].note.kind=NoteKind::hold;s.targets[0].note.held_body_evidence=true;s.targets[0].note.rails_geometry=true;s.targets[0].crossing_ns.reset();owner.accept(s,true);owner.poll();EXPECT_TRUE(touch.contacts.empty());EXPECT_TRUE(owner.owner()->take_accepted_plans().empty());
    PlaySessionLifecycle lifecycle;auto status=lifecycle.observe(s.context,{},false,clock.now_ns());EXPECT_FALSE(status.active);EXPECT_FALSE(status.new_round);owner.finish();
}
TEST(ContactReplay, DelayIsScriptedAndIndependentOfCpuCost) {
    FakeClock clock;clock.set(100);ReplayTouch touch(clock);touch.delay_ns=40;auto receipt=touch.inject({1,0,Phase::down,20,20,100,1});EXPECT_EQ(receipt.injection_start_ns,100);EXPECT_EQ(receipt.injection_return_ns,140);touch.release_all();
}
TEST(ContactReplay, FailedReleaseAndClockRegressionAreExplicit) {
    FakeClock clock;clock.set(100);ReplayTouch touch(clock);touch.failed_release=true;
    touch.inject({1,0,Phase::down,20,20,100,1});auto release=touch.release_all();EXPECT_EQ(release.failed_ids,std::vector<int>{0});
    SessionGameOwner owner(clock,touch,5,{15,35'000'000,30'000'000});EXPECT_THROW(advance_before_frame(owner,clock,99,false,[]{}),std::runtime_error);
}
TEST(ContactReplay, TraceOnOffPreservesPixelsObserverOwnerAndReceipts) {
    const auto run=[](bool tracing){
        FakeClock clock;ReplayTouch touch(clock);SessionPerception perception(clock);SessionGameOwner owner(clock,touch,5,{15,35'000'000,30'000'000});json result=json::array();
        touch.sink=[&](json j){result.push_back(j);};
        for(int i=0;i<18;++i){Frame f;f.width=1280;f.height=720;f.stride=3840;f.rgb.resize(1280*720*3);f.epoch=f.generation=f.geometry_version=1;f.sequence=i+1;f.source_rotation=1;f.capture_complete_ns=1'000'000'000+i*17'000'000;clock.set(f.capture_complete_ns);
            const auto box=[&](int x,int y,int w,int h,std::array<std::uint8_t,3> color){for(int py=y;py<y+h;++py)for(int px=x;px<x+w;++px)std::copy(color.begin(),color.end(),f.rgb.data()+py*f.stride+px*3);};
            box(20,20,6,22,{255,255,255});box(34,20,6,22,{255,255,255});for(int j=0;j<6;++j)box(1020+j*24,20,12,20,{255,255,255});box(80,540,1120,3,{255,255,255});box(440,365+i*10,120,8,{40,190,255});
            x1_trace_enabled=tracing;auto p=perception.process(f);if(p.status.new_round)owner.start(p.status.round);owner.accept(p.scene,p.allow_down);owner.poll();
            result.push_back(decision_json(p.scene));if(owner.owner()){result.push_back(owner.owner()->replay_state());owner.owner()->take_accepted_plans();owner.owner()->take_coverage_updates();owner.owner()->take_plan_cancellations();owner.scheduler()->take_notices();}x1_drain();
        }
        owner.finish();x1_drain();x1_trace_enabled=false;return result;
    };
    EXPECT_EQ(run(false),run(true));
}

namespace {
// Five tiny scene rows exercise the actual compare CLI. Negative cases have
// no trace files, so their reason also proves rejection before trace reads.
class ContactComparison : public ::testing::Test {
protected:
    std::filesystem::path root,manifest_path,a,b,output;
    json manifest,sa,sb;
    void SetUp() override {
        const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();
        root=std::filesystem::temp_directory_path()/("pas-x1-compare-"+std::to_string(unique));
        ASSERT_TRUE(std::filesystem::create_directory(root));
        manifest_path=root/"manifest.json";a=root/"a";b=root/"b";output=root/"comparison.json";
        std::filesystem::create_directories(root/"source-v2");
        std::filesystem::create_directory(a);std::filesystem::create_directory(b);
        manifest={{"schema",1},{"batch_root",root.string()},{"index_sha256",std::string(64,'1')},
            {"profile_sha256",std::string(64,'2')},{"windows",json::array()}};
        for(const auto& [id,anchor]:std::array<std::pair<const char*,int>,5>{{
            {"A3498",3498},{"B4986",4986},{"C5520",5520},{"D6214",6214},{"E5287",5287}}})
            manifest["windows"].push_back({{"id",id},{"first",anchor},{"last",anchor},{"anchor",anchor}});
        for(const auto role:{"c36h","main50"})
            pas::review::save(root/"source-v2"/(std::string(role)+"-source-provenance.json"),{{"lineage",role}});
        sa={{"success",true},{"lineage","c36h"},{"binary_sha256",std::string(64,'a')},
            {"source_provenance_sha256",sha256_file(root/"source-v2"/"c36h-source-provenance.json")},
            {"cadence","owner"},{"tie","frame-first"},{"recognition","zero_fake_time"},
            {"receipt_policy","success_zero_duration_five_contacts"}};
        sb=sa;sb["lineage"]="main50";sb["binary_sha256"]=std::string(64,'b');
        sb["source_provenance_sha256"]=sha256_file(root/"source-v2"/"main50-source-provenance.json");
        bind_manifest();
    }
    void TearDown() override {if(!root.empty())std::filesystem::remove_all(root);}
    void write_summaries() {
        pas::review::save(a/"summary.json",sa);pas::review::save(b/"summary.json",sb);
    }
    void bind_manifest() {
        pas::review::save(manifest_path,manifest);
        sa["input_manifest_sha256"]=sb["input_manifest_sha256"]=sha256_file(manifest_path);
        write_summaries();
    }
    std::pair<int,std::string> invoke(bool swapped=false) {
        // Intentionally not an existing binary: comparison must accept runs
        // made by other executables without hashing argv[0].
        std::array<std::string,6> args{{"different-comparison-tool","contact-compare",manifest_path.string(),
            (swapped?b:a).string(),(swapped?a:b).string(),output.string()}};
        std::array<char*,6> argv{};for(std::size_t i=0;i<args.size();++i)argv[i]=args[i].data();
        std::ostringstream errors;auto* prior=std::cerr.rdbuf(errors.rdbuf());
        const auto code=contact_replay_main(static_cast<int>(argv.size()),argv.data());
        std::cerr.rdbuf(prior);return {code,errors.str()};
    }
    void rejected(const std::string& reason,bool swapped=false) {
        write_summaries();const auto [code,error]=invoke(swapped);
        EXPECT_NE(code,0);EXPECT_NE(error.find(reason),std::string::npos)<<error;
        EXPECT_FALSE(std::filesystem::exists(output));
    }
    void tiny_traces() {
        std::ofstream at(a/"trace.jsonl"),bt(b/"trace.jsonl"),original(a/"original-recorded.jsonl");
        for(const auto& w:manifest.at("windows")) {
            const json scene={{"targets",json::array()},{"lines",json::array()}};
            json row={{"ordinal",w.at("anchor")},{"png_sha256",std::string(64,'c')},
                {"scene",scene},{"lifecycle",nullptr},{"owner",nullptr},{"diagnostics",json::array()}};
            at<<row.dump()<<'\n';bt<<row.dump()<<'\n';row["decision"]=scene;original<<row.dump()<<'\n';
        }
        std::ofstream(a/"events.jsonl");std::ofstream(b/"events.jsonl");
    }
};
}
TEST_F(ContactComparison, ValidPairWithDifferentComparisonBinaryPasses) {
    tiny_traces();const auto [code,error]=invoke();ASSERT_EQ(code,0)<<error;
    const auto report=pas::review::load(output);EXPECT_EQ(report.at("cases_denominator"),5);
    EXPECT_EQ(report.at("frames_denominator"),5);EXPECT_EQ(report.at("manifest_sha256"),sha256_file(manifest_path));
    EXPECT_EQ(report.at("a_summary_sha256"),sha256_file(a/"summary.json"));
    EXPECT_EQ(report.at("b_summary_sha256"),sha256_file(b/"summary.json"));
}
TEST_F(ContactComparison, ChangedManifestBytesIndexAndWindowsRejectBeforeTraceRead) {
    const auto original=manifest;
    {std::ofstream append(manifest_path,std::ios::app);append<<'\n';}
    rejected("comparison_manifest_SHA_mismatch");
    manifest["index_sha256"]=std::string(64,'0');pas::review::save(manifest_path,manifest);
    rejected("comparison_manifest_SHA_mismatch");
    manifest=original;manifest["windows"][0]["first"]=3497;pas::review::save(manifest_path,manifest);
    rejected("comparison_manifest_SHA_mismatch");
}
TEST_F(ContactComparison, SuppliedManifestMustMatchEachSummary) {
    for(const bool first:{true,false}) {
        auto& summary=first?sa:sb;const auto saved=summary;
        summary["input_manifest_sha256"]=std::string(64,'0');rejected("comparison_manifest_SHA_mismatch");
        summary.erase("input_manifest_sha256");rejected("comparison_manifest_SHA_mismatch");summary=saved;
    }
}
TEST_F(ContactComparison, UnsupportedOrMissingSchemaRejectsEvenWhenHashesMatch) {
    for(const json schema:{json(2),json(1.0),json(nullptr)}) {
        manifest["schema"]=schema;bind_manifest();rejected("comparison_manifest_schema");
    }
    manifest.erase("schema");bind_manifest();rejected("comparison_manifest_schema");
}
TEST_F(ContactComparison, InvalidWindowsRejectEvenWhenHashesMatch) {
    const auto original=manifest;
    const auto check=[&]{bind_manifest();rejected("comparison_manifest_windows");manifest=original;};
    manifest["windows"][1]=manifest["windows"][0];check();
    manifest["windows"][0]["id"]="unknown";check();
    manifest["windows"].erase(0);check();
    manifest["windows"]=json::object();check();
    manifest["windows"][0]["first"]=-1;check();
    manifest["windows"][0]["last"]=3497;check();
    manifest["windows"][0]["last"]=3618;check();
    manifest["windows"][0]["anchor"]=3497;check();
    manifest["windows"][0]["last"]=36000;check();
    manifest["windows"][0]["first"]=3498.0;check();
    manifest["windows"][0].erase("anchor");check();
}
TEST_F(ContactComparison, SwappedSameMissingAndUnknownLineagesReject) {
    rejected("comparison_lineage_expected_c36h",true);
    for(const bool first:{true,false}) {
        auto& summary=first?sa:sb;const auto saved=summary;
        const auto expected=first?"comparison_lineage_expected_c36h":"comparison_lineage_expected_main50";
        for(const json lineage:{json(first?"main50":"c36h"),json("unknown"),json(nullptr)}) {
            summary["lineage"]=lineage;rejected(expected);
        }
        summary.erase("lineage");rejected(expected);summary=saved;
    }
}
TEST_F(ContactComparison, SourceHashesAndProvenanceMustSupportDeclaredRole) {
    const auto original=sa;
    for(const auto key:{"source_provenance_sha256","binary_sha256"}) {
        sa.erase(key);rejected("comparison_run_source_missing_c36h");sa=original;
        sa[key]="unknown";rejected("comparison_run_source_missing_c36h");sa=original;
    }
    sa["source_provenance_sha256"]=sb["source_provenance_sha256"];rejected("comparison_provenance_binding_c36h");sa=original;
    const auto provenance=root/"source-v2"/"c36h-source-provenance.json";
    pas::review::save(provenance,{{"lineage","main50"}});
    sa["source_provenance_sha256"]=sha256_file(provenance);rejected("comparison_provenance_lineage_c36h");
}
TEST_F(ContactComparison, OneReplayBinaryCannotClaimBothLineages) {
    sb["binary_sha256"]=sa["binary_sha256"];rejected("comparison_run_source_not_distinct");
}
TEST_F(ContactComparison, PolicyMismatchAndFailedRunStillRejectBeforeTraceRead) {
    const auto original=sb;
    for(const auto key:{"cadence","tie","recognition","receipt_policy"}) {
        sb[key]="different";rejected("comparison_policy_mismatch");sb=original;
    }
    for(const bool first:{true,false}) {
        auto& summary=first?sa:sb;const auto saved=summary;
        summary["success"]=false;rejected("cannot_compare_failed_run");summary=saved;
    }
}
TEST_F(ContactComparison, ExistingOutputIsPreserved) {
    const json sentinel={{"preserve","existing evidence"}};pas::review::save(output,sentinel);
    const auto hash=sha256_file(output);const auto [code,error]=invoke();
    EXPECT_NE(code,0);EXPECT_NE(error.find("output_exists"),std::string::npos);
    EXPECT_EQ(sha256_file(output),hash);
}
