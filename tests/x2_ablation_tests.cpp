#include "x2_ablation.hpp"
#include "x2_input.hpp"
#include "contact_replay.hpp"
#include "pas/game_tracking.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <sstream>
#include <iostream>

using namespace pas;
using pas::review::json;
namespace {
// Applies the offline variant to ALL original strategy tests in the variant
// regression invocation. No original expected result is removed or rewritten.
class VariantEnvironment final:public ::testing::Environment {
    void SetUp() override {x2::winner_override_enabled=std::getenv("PAS_X2_NO_WINNER_OVERRIDE")!=nullptr?false:true;}
};
auto* const environment=::testing::AddGlobalTestEnvironment(new VariantEnvironment);
struct SwitchScope {
    bool saved=x2::winner_override_enabled;
    bool capture=x2::capture_enabled;
    SwitchScope(bool enabled){x2::winner_override_enabled=enabled;x2::capture_enabled=true;x2::reset();}
    ~SwitchScope(){x2::winner_override_enabled=saved;x2::capture_enabled=capture;x2::reset();}
};
#ifdef PAS_X2_MAIN50
struct Result {DecisionSnapshot scene;json choice;GameTrackHistory history;};
Result one(bool enabled,bool confirmed,double old_y,double competing_y,bool old_visible=true) {
    SwitchScope scope(enabled);
    LineCandidate old;old.center={640,old_y};old.tangent={1,0};old.length=1200;old.confidence=1;
    old.track_id=10;old.association_valid=true;old.observed_ns=1'060'000'000;
    LineCandidate other=old;other.center.y=competing_y;other.track_id=20;
    NoteCandidate n;n.kind=NoteKind::drag;n.center={640,300};n.tangent={1,0};n.width=120;n.height=8;n.confidence=1;
    GameTrackHistory h;h.id=1;h.revision=3;h.kind=n.kind;h.last={640,300};h.observed=1'040'000'000;
    h.appearance=n;h.confirmed_line_id=confirmed?10:0;
    for(int i=0;i<3;++i){auto l=old;l.observed_ns=1'000'000'000+i*20'000'000;h.points.push_back({l.observed_ns,{640,300},l,{}});}
    DecisionSnapshot s;s.context={1,1,1,4,1'060'000'000,1280,720,1};s.playing_gate=true;s.ui=GameUi::playing;
    if(old_visible)s.lines.push_back(old);s.lines.push_back(other);
    std::vector<GameTrackHistory> tracks{h};std::uint64_t next=1;std::vector<int> assigned{0};
    track_legacy_batch(s,{n},{std::nullopt},tracks,next,&assigned);
    const auto records=x2::drain();return {s,records.at(0),tracks.at(0)};
}
TEST(X2WinnerOnly, EnabledKeepsOriginalWinnerEligibilityAndAmbiguityBypass) {
    const auto r=one(true,true,480,304);
    EXPECT_EQ(r.choice.at("pre_override_winner"),20);EXPECT_EQ(r.choice.at("post_override_winner"),10);
    EXPECT_EQ(r.choice.at("preserve_eligible"),true);EXPECT_EQ(r.choice.at("override_applied"),true);
    EXPECT_EQ(r.choice.at("ambiguity"),false);EXPECT_EQ(r.scene.targets[0].line_id,10);
}
TEST(X2WinnerOnly, DisabledSuppressesOnlyWinnerAssignmentAndKeepsConflictGuard) {
    const auto a=one(true,true,480,304),b=one(false,true,480,304);
    for(const auto key:{"best_score","second_score","preserve_eligible","preserve_flag","ambiguity","confirmed_line_id","pre_override_winner"})EXPECT_EQ(a.choice.at(key),b.choice.at(key))<<key;
    EXPECT_EQ(b.choice.at("post_override_winner"),20);EXPECT_EQ(b.choice.at("override_applied"),false);
    EXPECT_EQ(b.choice.at("relation_conflict"),true);EXPECT_EQ(b.choice.at("final_valid_relation"),false);
    EXPECT_EQ(b.scene.targets[0].reason,"confirmed_line_relation_conflict");EXPECT_FALSE(b.scene.targets[0].crossing_ns);
    EXPECT_EQ(b.history.confirmed_line_id,10);EXPECT_EQ(b.history.points.size(),3);
}
TEST(X2WinnerOnly, CloseScoresStillBypassAmbiguityWhenFlagIsPreserved) {
    // same_recent -120: old score=10, second score=6, margin=4.
    const auto a=one(true,true,430,306),b=one(false,true,430,306);
    EXPECT_LT(b.choice.at("second_score").get<double>()-b.choice.at("best_score").get<double>(),8);
    EXPECT_EQ(a.choice.at("preserve_flag"),true);EXPECT_EQ(b.choice.at("preserve_flag"),true);
    EXPECT_EQ(b.choice.at("ambiguity"),false);EXPECT_EQ(b.choice.at("relation_conflict"),true);
}
TEST(X2WinnerOnly, BestAlreadyConfirmedAndIneligiblePreserveRemainEquivalent) {
    for(const auto& args:std::array<std::array<double,3>,3>{{{1,330,500},{0,480,304},{1,480,304}}}) {
        const bool visible=args[0]!=1||args[1]!=480;
        const auto a=one(true,args[0]!=0,args[1],args[2],visible),b=one(false,args[0]!=0,args[1],args[2],visible);
        EXPECT_EQ(decision_json(a.scene),decision_json(b.scene));
        EXPECT_EQ(a.choice.at("post_override_winner"),b.choice.at("post_override_winner"));
    }
}
#endif

class X2Comparison:public ::testing::Test {
protected:
    std::filesystem::path root,manifest_path,output;
    json m;std::array<json,3> s;std::array<std::filesystem::path,3> runs;
    void SetUp() override {
        root=std::filesystem::temp_directory_path()/("pas-x2-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(root);manifest_path=root/"manifest.json";output=root/"report.json";
        m={{"schema",1},{"batch_root",root.string()},{"experiment","confirmed_preserve_winner_only_x2"},
            {"batch_limit_bytes",67108864},{"index_sha256",std::string(64,'1')},{"profile_sha256",std::string(64,'2')},
            {"campaign_root",root.string()},{"prior_research_root",(root/"missing").string()},{"roles",json::object()},{"windows",json::array()}};
        for(const auto& [id,n]:std::array<std::pair<const char*,int>,5>{{{"A3498",3498},{"B4986",4986},{"C5520",5520},{"D6214",6214},{"E5287",5287}}})m["windows"].push_back({{"id",id},{"first",n},{"last",n},{"anchor",n}});
        for(std::size_t i=0;i<3;++i) {
            const auto role=x2::roles[i];runs[i]=root/role;std::filesystem::create_directory(runs[i]);
            const auto binary=root/(std::string(role)+".exe");std::ofstream(binary)<<"test binary "<<i;
            const auto p=root/(std::string(role)+".json");
            const json provenance={{"role",role},{"variant",x2::variant(role)},{"lineage",i==0?"c36h":"main50"},
                {"instrumented_source_sha256",{{"src/game_tracking.cpp",std::string(64,char('a'+i))}}}};
            pas::review::save(p,provenance);
            m["roles"][role]={{"variant",x2::variant(role)},{"lineage",i==0?"c36h":"main50"},
                {"binary_path",binary.string()},{"binary_sha256",sha256_file(binary)},
                {"source_provenance_path",p.string()},{"source_provenance_sha256",sha256_file(p)},
                {"tracking_source_sha256",std::string(64,char('a'+i))}};
            s[i]=m["roles"][role];s[i]["role"]=role;s[i]["success"]=true;s[i]["input_frames"]=5;
            s[i]["cadence"]="owner";s[i]["tie"]="frame-first";s[i]["recognition"]="zero_fake_time";s[i]["receipt_policy"]="success_zero_duration_five_contacts";
        }
        bind();
    }
    void TearDown() override {std::filesystem::remove_all(root);}
    void summaries(){for(std::size_t i=0;i<3;++i)pas::review::save(runs[i]/"summary.json",s[i]);}
    void bind(){pas::review::save(manifest_path,m);for(auto& v:s)v["input_manifest_sha256"]=sha256_file(manifest_path);summaries();}
    std::pair<int,std::string> invoke(std::array<int,3> order={0,1,2}) {
        std::array<std::string,7> args{{"different-compare-tool","contact-x2-compare",manifest_path.string(),runs[order[0]].string(),runs[order[1]].string(),runs[order[2]].string(),output.string()}};
        std::array<char*,7> argv{};for(std::size_t i=0;i<7;++i)argv[i]=args[i].data();
        std::ostringstream error;auto* saved=std::cerr.rdbuf(error.rdbuf());const int code=contact_replay_main(7,argv.data());std::cerr.rdbuf(saved);return {code,error.str()};
    }
    void reject(const std::string& reason,std::array<int,3> order={0,1,2}) {
        summaries();const auto [code,error]=invoke(order);EXPECT_NE(code,0);EXPECT_NE(error.find(reason),std::string::npos)<<error;EXPECT_FALSE(std::filesystem::exists(output));
    }
    void tiny() {
        for(std::size_t i=0;i<3;++i) {
            std::ofstream trace(runs[i]/"trace.jsonl"),original(runs[i]/"original-recorded.jsonl"),digests(runs[i]/"state-digests.jsonl");
            for(const auto& w:m.at("windows")) {
                const json scene={{"targets",json::array()},{"lines",json::array()}};
                const json row={{"ordinal",w.at("anchor")},{"png_sha256",std::string(64,'c')},{"scene",scene},{"decision",scene},{"lifecycle",nullptr},{"owner",nullptr},{"diagnostics",json::array()}};
                trace<<row.dump()<<'\n';original<<row.dump()<<'\n';digests<<json{{"ordinal",w.at("anchor")},{"png_sha256",std::string(64,'c')},{"semantic_sha256",std::string(64,'e')}}.dump()<<'\n';
            }
            std::ofstream(runs[i]/"events.jsonl");
        }
    }
};
TEST_F(X2Comparison, ThreeRolesAreNamedAndBoundBeforeReadingTrace) {
    tiny();const auto [code,error]=invoke();ASSERT_EQ(code,0)<<error;
    const auto report=pas::review::load(output);EXPECT_EQ(report.at("roles").size(),3);
    EXPECT_TRUE(report.at("control_vs_variant").at("cases")[0].contains("main50_control_actions"));
    EXPECT_FALSE(report.at("control_vs_variant").at("cases")[0].contains("c36h_actions"));
}
TEST_F(X2Comparison, ManifestBytesIndexAndWindowChangesReject) {
    {std::ofstream(manifest_path,std::ios::app)<<'\n';}reject("x2_manifest_SHA_mismatch");
    for(const auto key:{"index_sha256","windows"}) {
        auto changed=m;if(std::string(key)=="windows")changed["windows"][0]["first"]=3497;else changed[key]=std::string(64,'0');
        pas::review::save(manifest_path,changed);reject("x2_manifest_SHA_mismatch");
    }
}
TEST_F(X2Comparison, SwappedDuplicateAndUnknownRolesReject) {
    reject("x2_role_expected_main50_control",{0,2,1});reject("x2_role_expected_main50_no_confirmed_winner_override",{0,1,1});
    reject("x2_role_expected_c36h_reference",{1,0,2});s[2]["role"]="unknown";reject("x2_role_expected_main50_no_confirmed_winner_override");
}
TEST_F(X2Comparison, VariantSourceBinaryAndPolicyMismatchReject) {
    const auto good=s[2];for(const auto key:{"variant","tracking_source_sha256","source_provenance_sha256","binary_sha256"}) {s[2][key]="wrong";reject(std::string("x2_run_binding_")+key);s[2]=good;}
    s[2]["tie"]="due-first";reject("x2_policy_mismatch");s[2]=good;s[1]["success"]=false;reject("x2_failed_run");
}
TEST_F(X2Comparison, RoleClaimMustMatchActualProvenanceAndBinary) {
    const auto p=m["roles"][x2::roles[2]]["source_provenance_path"].get<std::string>();auto provenance=pas::review::load(p);provenance["role"]="main50_control";pas::review::save(p,provenance);
    m["roles"][x2::roles[2]]["source_provenance_sha256"]=sha256_file(p);s[2]["source_provenance_sha256"]=sha256_file(p);bind();reject("x2_provenance_role_variant_source");
}
TEST_F(X2Comparison, BoundBudgetAndManifestRolesCannotBeRelaxed) {
    m["batch_limit_bytes"]=134217728;bind();reject("x2_manifest_contract");m["batch_limit_bytes"]=67108864;
    m["roles"].erase(x2::roles[2]);bind();reject("x2_manifest_contract");
}
}
