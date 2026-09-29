#include "pas/analysis.hpp"
#include "pas/config.hpp"
#include "pas/game.hpp"
#include "pas/runtime.hpp"
#include "pas/strategy_version.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace pas;

namespace {
class TempJson final {
public:
    explicit TempJson(const std::string& contents) {
        path_ = std::filesystem::temp_directory_path() /
            ("pas-cpp-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
        std::ofstream output(path_, std::ios::binary);
        if (!output) throw std::runtime_error("cannot write test input");
        output << contents;
    }
    ~TempJson() { std::error_code ignored; std::filesystem::remove(path_, ignored); }
    const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path path_;
};
}

TEST(RuntimeBudget, DelayedManualPlayPreservesFullDurationAndCannotResetOnGateChurn) {
    FakeClock clock;GameRunBudget budget(0,185'000'000'000LL,60'000'000'000LL);
    clock.set(44'000'000'000LL);EXPECT_FALSE(budget.expired(clock.now_ns()));EXPECT_TRUE(budget.waiting());
    ASSERT_TRUE(budget.arm_from_playing(clock.now_ns()));EXPECT_EQ(budget.deadline(),229'000'000'000LL);
    EXPECT_FALSE(budget.waiting());EXPECT_FALSE(budget.arm_from_playing(100'000'000'000LL));
    clock.set(185'000'000'000LL);EXPECT_FALSE(budget.expired(clock.now_ns()));
    clock.set(229'000'000'000LL-1);EXPECT_FALSE(budget.expired(clock.now_ns()));
    clock.set(229'000'000'000LL);EXPECT_TRUE(budget.expired(clock.now_ns()));EXPECT_STREQ(budget.expiry_reason(),"duration");
}
TEST(RuntimeBudget, MissingOrLatePlayTimesOutAndNonmanualDurationRemainsUnchanged) {
    GameRunBudget pending(10,185'000'000'000LL,60'000'000'000LL);
    EXPECT_FALSE(pending.arm_from_playing(9));EXPECT_FALSE(pending.expired(60'000'000'009LL));
    EXPECT_TRUE(pending.expired(60'000'000'010LL));EXPECT_FALSE(pending.arm_from_playing(60'000'000'010LL));
    EXPECT_STREQ(pending.expiry_reason(),"waiting_for_play_timeout");EXPECT_FALSE(pending.origin());
    GameRunBudget normal(10,185'000'000'000LL);
    EXPECT_FALSE(normal.waiting());EXPECT_FALSE(normal.arm_from_playing(100));
    EXPECT_EQ(normal.deadline(),185'000'000'010LL);EXPECT_TRUE(normal.expired(normal.deadline()));
    EXPECT_THROW(GameRunBudget(0,1,60'000'000'001LL),std::invalid_argument);
    EXPECT_THROW(GameRunBudget(0,0,1),std::invalid_argument);
}
TEST(RuntimeInput, ManualPlayCannotBypassTheExplicitTouchPreflight) {
    const TempJson profile(R"({"schema":2,"name":"manual-play-negative","serial":"emulator-5554",
        "capture":{"kind":"fake","execution":"thread","transport":"payload",
                   "image_format":"rgb888","row_order":"top-down","width":64,
                   "height":36,"source_rotation":0},
        "touch":{"kind":"none","timeout_ms":100,"max_contacts":2},
        "scheduler":{"max_plans":4,"max_steps":4,"horizon_ms":350,
                     "evidence_max_age_ms":100},
        "preview":{"hz":0},"log_dir":"manual-play-must-not-create-output"})");
    for(const bool manual:{false,true}) {
        SCOPED_TRACE(manual);
        try {
            run_assist(profile.path().string(),"nonexistent-capability.json",1,true,false,false,"",manual);
            FAIL()<<"real assist must reject a no-touch profile before capture or input";
        } catch(const std::invalid_argument& e) {
            EXPECT_STREQ(e.what(),"real input requires explicit touch profile");
        }
    }
}
TEST(ConfigMigration, ExplicitProcessToThreadAndStrictSchema) {
    const TempJson old(R"({"schema":1,"name":"test","serial":"emulator-5554",
        "capture":{"kind":"fake","execution":"process","transport":"payload",
                   "image_format":"rgb888","row_order":"top-down","width":64,
                   "height":36,"source_rotation":0},
        "touch":{"kind":"none","timeout_ms":500,"max_contacts":2},
        "scheduler":{"max_plans":4,"max_steps":4,"horizon_ms":2000,
                     "evidence_max_age_ms":150},
        "preview":{"hz":0},"log_dir":"test-output"})");
    const auto converted = old.path().string() + ".migrated";
    EXPECT_THROW(load_config(old.path()), std::invalid_argument);
    ASSERT_NO_THROW(migrate_config(old.path(), converted));
    const auto config = load_config(converted);
    EXPECT_EQ(config.public_json.at("schema"), 2);
    EXPECT_EQ(config.public_json.at("capture").at("execution"), "thread");
    EXPECT_EQ(config.width, 64);
    EXPECT_EQ(config.grpc_read_chunk_kib, 256);
    EXPECT_EQ(config.max_relative_lag_ms, 250);
    EXPECT_EQ(config.public_json["capture"]["max_relative_lag_ms"], 250);
    for (const auto& lag : {nlohmann::json(1), nlohmann::json(250), nlohmann::json(1000),
                          nlohmann::json(0), nlohmann::json(1001), nlohmann::json(nullptr),
                          nlohmann::json(1.5), nlohmann::json("250")}) {
        auto edited = config.public_json;
        edited["capture"]["max_relative_lag_ms"] = lag;
        const TempJson input(edited.dump());
        if (lag.is_number_integer() && lag.get<int>() >= 1 && lag.get<int>() <= 1000)
            EXPECT_EQ(load_config(input.path()).max_relative_lag_ms, lag.get<int>());
        else EXPECT_THROW(load_config(input.path()), std::invalid_argument);
    }
    for (int kib : {8, 64, 256, 128}) {
        auto edited = config.public_json;
        edited["capture"]["grpc_read_chunk_kib"] = kib;
        const TempJson input(edited.dump());
        if (kib == 128) EXPECT_THROW(load_config(input.path()), std::invalid_argument);
        else EXPECT_EQ(load_config(input.path()).grpc_read_chunk_kib, kib);
    }
    for (const auto& invalid : {nlohmann::json(nullptr), nlohmann::json(64.5),
                               nlohmann::json("64"), nlohmann::json(-1)}) {
        auto edited = config.public_json;
        edited["capture"]["grpc_read_chunk_kib"] = invalid;
        const TempJson input(edited.dump());
        EXPECT_THROW(load_config(input.path()), std::invalid_argument);
    }
    EXPECT_THROW(migrate_config(old.path(), converted), std::runtime_error);
    std::filesystem::remove(converted);
}

TEST(GameConfig, ExplicitTypeSubsetAndCalibrationCannotSilentlyExpand) {
    auto profile=nlohmann::json::parse(R"({"schema":2,"name":"test","serial":"emulator-5554",
        "capture":{"kind":"fake","execution":"thread","transport":"payload","image_format":"rgb888",
        "row_order":"top-down","width":1280,"height":720,"source_rotation":1},
        "touch":{"kind":"none","timeout_ms":100,"max_contacts":2},
        "scheduler":{"max_plans":128,"max_steps":16,"horizon_ms":350,"evidence_max_age_ms":100},
        "preview":{"hz":0},"log_dir":"test-output",
        "game":{"enabled_types":["tap","hold"],"lead_ms":8,"uncertainty_ms":30}})");
    const TempJson good(profile.dump()); EXPECT_EQ(load_config(good.path()).game_type_mask,3);
    for(const auto& types:{nlohmann::json::array(),nlohmann::json::array({"tap","tap"}),
                           nlohmann::json::array({"chart"})}) {
        auto bad=profile; bad["game"]["enabled_types"]=types; const TempJson input(bad.dump());
        EXPECT_THROW(load_config(input.path()),std::invalid_argument);
    }
    profile["game"]["lead_ms"]=61; const TempJson too_far(profile.dump());
    EXPECT_THROW(load_config(too_far.path()),std::invalid_argument);
}

TEST(AnalysisWindow, HalfOpenCaptureAndConsumerBoundary) {
    const TempJson raw(
        "{\"event\":\"bench_phase\",\"phase\":\"MEASURING\",\"monotonic_ns\":1000000000}\n"
        "{\"event\":\"capture\",\"frame_sequence\":1,\"capture_complete_ns\":999999999,\"pixels_ready_ns\":1000000000}\n"
        "{\"event\":\"capture\",\"frame_sequence\":2,\"capture_complete_ns\":1000000000,\"pixels_ready_ns\":1001000000,\"width\":1280,\"height\":720,\"source_rotation\":1}\n"
        "{\"event\":\"frame_consumed\",\"frame_sequence\":2,\"consume_ns\":1010000000,\"capture_complete_ns\":1000000000,\"sequence_skip\":0}\n"
        "{\"event\":\"fixture_counter\",\"frame_sequence\":2,\"capture_complete_ns\":1000000000,\"counter\":7,\"fixture_schema\":\"native_v2_four_region\"}\n"
        "{\"event\":\"capture\",\"frame_sequence\":3,\"capture_complete_ns\":2000000000,\"pixels_ready_ns\":2001000000}\n"
        "{\"event\":\"bench_phase\",\"phase\":\"STOPPING\",\"monotonic_ns\":2000000000}\n");
    const auto result = analyze_capture_jsonl(raw.path());
    EXPECT_EQ(result.at("capture_events"), 1);
    EXPECT_EQ(result.at("consumer_events"), 1);
    EXPECT_EQ(result.at("fixture_samples"), 1);
    EXPECT_EQ(result.at("fixture_distinct"), 1);
    EXPECT_EQ(result.at("fixture_schemas"), nlohmann::json::array({"native_v2_four_region"}));
    EXPECT_TRUE(result.at("geometry_valid").get<bool>());
    EXPECT_DOUBLE_EQ(result.at("received_hz").get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(result.at("host_residency_ms").at("p50").get<double>(), 10.0);
    EXPECT_EQ(result.at("arrival_interval_ms").at("n"), 0);
}

TEST(AnalysisWindow, MalformedJsonIsRejected) {
    const TempJson raw("{this is not JSON}\n");
    EXPECT_THROW(analyze_capture_jsonl(raw.path()), std::runtime_error);
}

TEST(AnalysisTransport, CampaignRejectsChangedOrMissingTransportAndKeepsLegacyContract) {
    using json = nlohmann::json;
    const TempJson raw(
        "{\"event\":\"bench_phase\",\"phase\":\"MEASURING\",\"monotonic_ns\":1000000000}\n"
        "{\"event\":\"capture\",\"frame_sequence\":1,\"capture_complete_ns\":1100000000,\"pixels_ready_ns\":1101000000,\"width\":1280,\"height\":720,\"source_rotation\":1}\n"
        "{\"event\":\"frame_consumed\",\"frame_sequence\":1,\"consume_ns\":1102000000,\"capture_complete_ns\":1100000000,\"sequence_skip\":0}\n"
        "{\"event\":\"fixture_counter\",\"frame_sequence\":1,\"capture_complete_ns\":1100000000,\"counter\":7,\"fixture_schema\":\"native_v2_four_region\"}\n"
        "{\"event\":\"bench_phase\",\"phase\":\"STOPPING\",\"monotonic_ns\":2000000000}\n");
    struct Directory {
        std::filesystem::path root;
        explicit Directory(std::filesystem::path path) : root(std::move(path)) {
            if (!std::filesystem::create_directory(root)) throw std::runtime_error("test path exists");
            std::filesystem::create_directory(root / "check");
        }
        ~Directory() {
            std::error_code ignored;
            for (const auto* file : {"check/capture.jsonl", "check/manifest.json", "check",
                                     "campaign-plan.json", "campaign-results.json"})
                std::filesystem::remove(root / file, ignored);
            std::filesystem::remove(root, ignored);
        }
    } directory(raw.path().string() + ".campaign");
    std::filesystem::copy_file(raw.path(), directory.root / "check/capture.jsonl");
    const auto save = [&](const char* file, const json& data) {
        std::ofstream out(directory.root / file); out << data.dump();
    };
    const json transport = {{"read_chunk_bytes", 262144}, {"windows_read_patch_version", 1}};
    json plan = {{"schema_version", 2}, {"grpc_transport", transport},
                 {"cases", json::array({{{"name", "check"}}})}};
    json manifest = {{"capture_backend", "emulator-grpc"}, {"grpc_transport", transport},
                     {"fixture_apk_sha256", "fixture"}, {"installed_apk_sha256", "fixture"}};
    save("campaign-plan.json", plan);
    save("check/manifest.json", manifest);
    save("campaign-results.json", {{"complete", true}, {"runs", json::array({
        {{"name", "check"}, {"returncode", 0}, {"raw_sha256", sha256_file(raw.path())}}})}});
    EXPECT_EQ(analyze_capture_campaign(directory.root).at("all_valid"), true);
    manifest["grpc_transport"]["read_chunk_bytes"] = 8192;
    save("check/manifest.json", manifest);
    EXPECT_EQ(analyze_capture_campaign(directory.root).at("all_valid"), false);
    manifest.erase("grpc_transport");
    save("check/manifest.json", manifest);
    EXPECT_EQ(analyze_capture_campaign(directory.root).at("all_valid"), false);
    plan.erase("grpc_transport");
    save("campaign-plan.json", plan);
    EXPECT_EQ(analyze_capture_campaign(directory.root).at("all_valid"), true);
}

TEST(AnalysisPause, NativeTimestampWithNullSourceSequenceRemainsAnalyzable) {
    const TempJson raw(
        "{\"event\":\"capture\",\"frame_sequence\":1,\"capture_complete_ns\":2000000000,\"source_sequence\":null,\"source_system_relative_100ns\":20000000,\"stream_generation\":1}\n"
        "{\"event\":\"receiver_pause\",\"start_ns\":2010000000,\"ended_ns\":2510000000}\n"
        "{\"event\":\"capture\",\"frame_sequence\":2,\"capture_complete_ns\":2520000000,\"source_sequence\":null,\"source_system_relative_100ns\":25200000,\"stream_generation\":1}\n");
    const auto result = analyze_pause_jsonl(raw.path());
    const auto& point = result.at("first_second_after_end").at(0);
    EXPECT_TRUE(point.at("source_sequence").is_null());
    EXPECT_DOUBLE_EQ(point.at("extra_relative_lag_ms").get<double>(), 0.0);
}

TEST(GameAnalysis, FutureWaitAndAlreadyLateDecisionAreDifferentDistributions) {
    const TempJson raw(R"({"event":"game_plan_accepted","intent_id":1,"basis":"tap","accepted_ns":0,"steps":[{"phase":0,"due_ns":20000000}]}
{"event":"game_touch_receipt","intent_id":1,"phase":0,"scheduled_ns":20000000,"injection_start_ns":21000000,"injection_return_ns":22000000}
{"event":"game_touch_receipt","intent_id":1,"phase":2,"scheduled_ns":40000000,"injection_start_ns":40000000,"injection_return_ns":41000000}
{"event":"game_plan_accepted","intent_id":2,"basis":"hold","accepted_ns":50000000,"steps":[{"phase":0,"due_ns":40000000}]}
{"event":"game_touch_receipt","intent_id":2,"phase":0,"scheduled_ns":40000000,"injection_start_ns":52000000,"injection_return_ns":53000000}
{"event":"game_touch_receipt","intent_id":3,"phase":0,"scheduled_ns":60000000,"injection_start_ns":62000000,"injection_return_ns":63000000}
)");
    const auto result=analyze_game_jsonl(raw.path());
    EXPECT_EQ(result.at("future_at_accept_down_lateness_ms").at("n"),1);
    EXPECT_DOUBLE_EQ(result.at("future_at_accept_down_lateness_ms").at("p50").get<double>(),1);
    EXPECT_EQ(result.at("already_past_at_accept_down_lateness_ms").at("n"),1);
    EXPECT_DOUBLE_EQ(result.at("already_past_at_accept_down_lateness_ms").at("p50").get<double>(),12);
    EXPECT_EQ(result.at("unclassified_down_deadlines"),1);
    EXPECT_DOUBLE_EQ(result.at("contact_duration_ms_by_basis").at("tap").at("p50").get<double>(),19);
    EXPECT_EQ(result.at("real_downs_by_basis").at("unknown"),1);
    EXPECT_EQ(result.at("unknown_predicted_downs"),3);
}
TEST(GameAnalysis, LateRecoverySeparatesPredictionDeadlineFromActualDispatch) {
    const TempJson raw(R"({"event":"game_plan_accepted","intent_id":1,"basis":"tap","accepted_ns":50000000,"predicted_down_ns":20000000,"steps":[{"phase":0,"due_ns":50000000}]}
{"event":"game_touch_receipt","intent_id":1,"phase":0,"scheduled_ns":50000000,"injection_start_ns":51000000,"injection_return_ns":52000000}
)");
    const auto result=analyze_game_jsonl(raw.path());
    EXPECT_EQ(result.at("intentionally_clamped_downs"),1);EXPECT_EQ(result.at("unknown_predicted_downs"),0);
    EXPECT_DOUBLE_EQ(result.at("real_schedule_lateness_ms").at("p50").get<double>(),1);
    EXPECT_DOUBLE_EQ(result.at("predicted_deadline_down_lateness_ms").at("p50").get<double>(),31);
}
TEST(GameAnalysis, PlayingEvidenceExcludesResultCoverCandidates) {
    const TempJson raw(R"({"event":"game_decision","decision_schema":2,"ui":"PLAYING","playing_gate":true,"lines":[],"capture_complete_ns":1000000,"recognition_start_ns":1000000,"recognition_end_ns":2000000,"targets":[{"kind":"tap","reason":"line_unobservable","crossing_ns":null}]}
{"event":"game_decision","decision_schema":1,"ui":"UNKNOWN","playing_gate":false,"lines":[],"capture_complete_ns":3000000,"recognition_start_ns":3000000,"recognition_end_ns":4000000,"targets":[{"kind":"flick","reason":"line_unobservable","crossing_ns":null}]}
)");
    const auto result=analyze_game_jsonl(raw.path());
    EXPECT_EQ(result.at("playing_no_line_frames"),1);
    EXPECT_EQ(result.at("playing_note_candidate_occurrences").at("tap"),1);
    EXPECT_FALSE(result.at("playing_note_candidate_occurrences").contains("flick"));
    EXPECT_EQ(result.at("playing_target_reason_occurrences").at("line_unobservable"),1);
    EXPECT_EQ(result.at("target_reason_occurrences").at("line_unobservable"),2);
}
TEST(GameAnalysis, TrackOutcomesDistinguishMissingPredictionAndCanceledAcceptedIntent) {
    using nlohmann::json;
    const auto target=[](int id,json crossing,int uncertainty) {
        return json{{"note_id",id},{"kind","tap"},{"reason",crossing.is_null()?"line_unobservable":"prediction_observe_only"},
            {"crossing_ns",crossing},{"uncertainty_ns",uncertainty},{"residual_px",0}};
    };
    const json frame={{"event","game_decision"},{"decision_schema",2},{"ui","PLAYING"},{"playing_gate",true},
        {"lines",json::array()},{"capture_complete_ns",0},{"recognition_start_ns",0},{"recognition_end_ns",1},
        {"targets",json::array({target(1,nullptr,0),target(2,300000000,1000000),target(3,20000000,1000000),
            target(4,20000000,1000000),target(5,20000000,1000000)})}};
    std::string log=frame.dump()+"\n";
    for(int id:{4,5}) log+=json{{"event","game_plan_accepted"},{"intent_id",id},{"note_id",id},
        {"basis","tap"},{"steps",json::array()}}.dump()+"\n";
    log+=json{{"event","game_touch_receipt"},{"intent_id",5},{"phase",0},
        {"scheduled_ns",20000000},{"injection_start_ns",21000000},{"injection_return_ns",22000000}}.dump()+"\n";
    const TempJson raw(log);const auto result=analyze_game_jsonl(raw.path());
    const auto& outcomes=result.at("observed_track_outcomes_by_kind").at("tap");
    for(const auto* key:{"never_predicted","prediction_never_near","near_prediction_not_accepted",
        "accepted_without_down","actual_down"}) EXPECT_EQ(outcomes.at(key),1);
    EXPECT_EQ(result.at("observed_track_evictions"),0);
    EXPECT_NEAR(result.at("first_near_prediction_available_lead_ms_by_track_outcome").at("tap_near_prediction_not_accepted")
        .at("p50").get<double>(),19.999999,.000001);
}
TEST(GameAnalysis, ConflictDetailsFollowSuccessfulReceiptsAndExplicitOwnerReset) {
    const TempJson raw(R"({"event":"game_plan_accepted","intent_id":1,"note_id":11,"basis":"hold","source_frame":1,"steps":[]}
{"event":"game_plan_accepted","intent_id":2,"note_id":12,"basis":"tap","source_frame":1,"steps":[]}
{"event":"game_touch_receipt","intent_id":1,"contact_id":0,"success":true,"phase":0,"scheduled_ns":0,"injection_start_ns":1,"injection_return_ns":2}
{"event":"game_touch_receipt","intent_id":2,"contact_id":1,"success":false,"phase":0,"scheduled_ns":0,"injection_start_ns":1,"injection_return_ns":2}
{"event":"scheduler_rejection","intent_id":2,"reason":"contact_conflict","monotonic_ns":3}
{"event":"game_touch_receipt","intent_id":1,"contact_id":0,"success":true,"phase":2,"scheduled_ns":4,"injection_start_ns":4,"injection_return_ns":5}
{"event":"scheduler_rejection","intent_id":2,"reason":"contact_conflict","monotonic_ns":6}
{"event":"game_touch_receipt","intent_id":2,"contact_id":1,"success":true,"phase":0,"scheduled_ns":7,"injection_start_ns":7,"injection_return_ns":8}
{"event":"owner_revoked"}
{"event":"scheduler_rejection","intent_id":3,"reason":"contact_conflict","monotonic_ns":9}
)");
    const auto result=analyze_game_jsonl(raw.path());const auto& conflicts=result.at("contact_conflicts");
    ASSERT_EQ(conflicts.size(),3);ASSERT_EQ(conflicts[0].at("active_contacts").size(),1);
    EXPECT_EQ(conflicts[0].at("active_contacts")[0].at("intent_id"),1);
    EXPECT_EQ(conflicts[0].at("active_contacts")[0].at("plan").at("note_id"),11);
    EXPECT_TRUE(conflicts[1].at("active_contacts").empty());EXPECT_TRUE(conflicts[2].at("active_contacts").empty());
    EXPECT_TRUE(conflicts[2].at("plan").is_null());EXPECT_EQ(result.at("unknown_contact_receipts"),1);
    EXPECT_EQ(result.at("contact_history_resets"),1);
}
TEST(GameAnalysis, SharedDragCoverageNeedsARecordedSuccessfulLocalDown) {
    const TempJson raw(R"({"event":"game_drag_coverage","intent_id":1,"note_id":2,"real_input":true}
{"event":"game_plan_accepted","intent_id":1,"note_id":1,"basis":"drag","steps":[]}
{"event":"game_touch_receipt","intent_id":1,"contact_id":0,"success":true,"phase":0,"scheduled_ns":0,"injection_start_ns":1,"injection_return_ns":2}
{"event":"game_drag_coverage","intent_id":1,"note_id":3,"real_input":true}
{"event":"game_touch_receipt","intent_id":1,"contact_id":0,"success":true,"phase":2,"scheduled_ns":4,"injection_start_ns":4,"injection_return_ns":5}
{"event":"game_drag_coverage","intent_id":1,"note_id":4,"real_input":true}
)");
    const auto result=analyze_game_jsonl(raw.path());EXPECT_EQ(result.at("drag_coverage_updates"),3);
    EXPECT_EQ(result.at("drag_coverage_with_known_local_contact"),1);
    EXPECT_EQ(result.at("real_downs_by_basis").at("drag"),1);
}
TEST(GameAnalysis, PendingPredictionCancellationIsNotATouchOrGameJudgment) {
    const TempJson raw(R"({"event":"game_pending_prediction_cancelled","reason":"nonlinear_or_mismatch","no_down_injected":true,"game_effect":"unknown","real_input":true}
{"event":"game_pending_prediction_cancelled","reason":"root_past","no_down_injected":true,"game_effect":"unknown","real_input":true}
{"event":"game_pending_prediction_cancelled","reason":"nonlinear_or_mismatch","no_down_injected":true,"game_effect":"unknown","real_input":true}
)");
    const auto result=analyze_game_jsonl(raw.path());
    EXPECT_EQ(result.at("pending_prediction_cancellations"),3);
    EXPECT_EQ(result.at("pending_prediction_cancellations_by_reason").at("nonlinear_or_mismatch"),2);
    EXPECT_EQ(result.at("pending_prediction_cancellations_by_reason").at("root_past"),1);
    EXPECT_TRUE(result.at("real_downs_by_basis").empty());
    EXPECT_FALSE(result.at("gameplay_validated").get<bool>());
}
TEST(GameAnalysis, ContactReleaseDiagnosticsDoNotClaimGameJudgment) {
    const TempJson raw(R"({"event":"game_contact_cancelled","reason":"current_target_missing"}
{"event":"game_contact_up","reason":"target_evidence_expired"}
{"event":"game_contact_up","reason":"visible_tail_completed"}
{"event":"game_hold_tail_confirmed"}
{"event":"game_hold_contact_reassociated"}
)");
    const auto result=analyze_game_jsonl(raw.path());
    EXPECT_EQ(result.at("contact_cancellations_by_reason").at("current_target_missing"),1);
    EXPECT_EQ(result.at("contact_ups_by_reason").at("target_evidence_expired"),1);
    EXPECT_EQ(result.at("contact_ups_by_reason").at("visible_tail_completed"),1);
    EXPECT_EQ(result.at("hold_tail_confirmations"),1);
    EXPECT_EQ(result.at("hold_contact_reassociations"),1);
    EXPECT_TRUE(result.at("real_downs_by_basis").empty());
    EXPECT_FALSE(result.at("gameplay_validated").get<bool>());
}
TEST(GameAnalysis, StrategyMetadataUsesOneVersionSource) {
    EXPECT_EQ(game_strategy_name(),"main observer"+std::to_string(game_observer_version)+
        "/planner"+std::to_string(game_planner_version));
    EXPECT_EQ(game_observer_version,47);
    EXPECT_EQ(game_planner_version,23);
    EXPECT_EQ(game_diagnostics_version,11);
}
TEST(GameAnalysis, QpcComputeFieldsAreExplicitAndOptionalForLegacyDecisions) {
    DecisionSnapshot scene;
    EXPECT_FALSE(decision_json(scene).contains("compute_clock_domain"));
    scene.compute_timing_host_qpc=true;
    scene.components_compute_ns=1200;scene.base_scene_compute_ns=2200;
    scene.combo_glyph_compute_ns=100;scene.line_scan_compute_ns=1700;
    scene.component_line_decode_compute_ns=100;scene.note_decode_compute_ns=200;
    scene.line_tracking_compute_ns=100;
    scene.held_recovery_compute_ns=3200;scene.tracking_compute_ns=4200;
    const auto data=decision_json(scene);
    EXPECT_EQ(data.at("compute_clock_domain"),"host_qpc_ns");
    EXPECT_EQ(data.at("components_compute_ns"),1200);
    EXPECT_EQ(data.at("line_scan_compute_ns"),1700);
    EXPECT_EQ(data.at("component_line_decode_compute_ns"),100);
    EXPECT_EQ(data.at("tracking_compute_ns"),4200);
}
TEST(GameAnalysis, VerifiedRoundStreamsAcrossSegmentsAndRejectsTampering) {
    const auto directory=std::filesystem::temp_directory_path()/
        ("pas-round-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);} } cleanup{directory};
    const std::string decision=R"({"event":"game_decision","decision_schema":2,"ui":"PLAYING","playing_gate":true,"lines":[],"capture_complete_ns":100000000,"recognition_start_ns":100000000,"recognition_end_ns":100000001,"targets":[]})";
    {std::ofstream out(directory/"events-0.jsonl",std::ios::binary);out<<decision<<'\n';}
    {std::ofstream out(directory/"events-1.jsonl",std::ios::binary);out<<R"({"event":"game_contact_cancelled","kind":"hold","reason":"current_object_missing_or_region_lost","contact_started":true})"<<'\n';}
    nlohmann::json summary={{"round_id",1},{"status","result_confirmed"},{"event_segments",nlohmann::json::array()}};
    for(int i=0;i<2;++i){const auto name="events-"+std::to_string(i)+".jsonl";
        summary["event_segments"].push_back({{"path",name},{"sha256",sha256_file(directory/name)}});}
    {std::ofstream out(directory/"summary.json",std::ios::binary);out<<summary.dump();}
    const auto result=analyze_game_round(directory);
    EXPECT_EQ(result.at("frames"),1);EXPECT_EQ(result.at("segments_read"),2);
    EXPECT_EQ(result.at("contact_cancellations_by_reason").at("current_object_missing_or_region_lost"),1);
    EXPECT_TRUE(result.at("segment_integrity_verified"));
    {std::ofstream out(directory/"events-1.jsonl",std::ios::app|std::ios::binary);out<<" ";}
    EXPECT_THROW(analyze_game_round(directory),std::runtime_error);
}
TEST(GameAnalysis, HoldCancellationKeepsUnknownEffectAndExactFrameTiming) {
    const TempJson raw(R"({"event":"game_decision","decision_schema":2,"ui":"PLAYING","playing_gate":true,"frame_sequence":10,"lines":[],"capture_complete_ns":100000000,"recognition_start_ns":101000000,"recognition_end_ns":105000000,"targets":[{"note_id":7,"kind":"hold","reason":"prediction_observe_only","crossing_ns":120000000,"uncertainty_ns":1000000,"residual_px":1.0}]}
{"event":"game_plan_accepted","intent_id":2,"note_id":7,"basis":"hold","source_frame":10,"accepted_ns":107000000,"predicted_down_ns":110000000,"steps":[{"phase":0,"due_ns":110000000}]}
{"event":"game_touch_receipt","intent_id":2,"contact_id":0,"success":true,"source_frame":10,"phase":0,"scheduled_ns":110000000,"injection_start_ns":111000000,"injection_return_ns":112000000}
{"event":"game_contact_cancelled","intent_id":2,"note_id":7,"kind":"hold","reason":"identity_ambiguous","source_frame":10,"contact_started":true,"cancel_ns":160000000}
)");
    const auto result=analyze_game_jsonl(raw.path());
    EXPECT_EQ(result.at("hold_cancel_active"),1);
    EXPECT_EQ(result.at("hold_cancel_before_down"),0);
    EXPECT_EQ(result.at("hold_cancel_traces").at(0).at("visible_game_effect"),"unknown");
    EXPECT_EQ(result.at("hold_cancel_traces").at(0).at("plan").at("note_id"),7);
    EXPECT_EQ(result.at("decision_recognition_end_to_accept_ms").at("p50"),2);
    EXPECT_EQ(result.at("down_capture_complete_to_injection_start_ms").at("p50"),11);
    EXPECT_EQ(result.at("down_recognition_end_to_injection_start_ms").at("p50"),6);
}
TEST(GameAnalysis, PlayingIntervalsExcludeMenuBoundariesButPreserveSourceGaps) {
    const TempJson raw(R"({"event":"game_decision","decision_schema":2,"ui":"PLAYING","playing_gate":true,"lines":[],"capture_complete_ns":100000000,"recognition_start_ns":100000000,"recognition_end_ns":100000001,"targets":[]}
{"event":"game_decision","decision_schema":2,"ui":"PLAYING","playing_gate":true,"epoch":2,"lines":[],"capture_complete_ns":300000000,"recognition_start_ns":300000000,"recognition_end_ns":300000001,"targets":[]}
{"event":"game_decision","decision_schema":2,"ui":"UNKNOWN","playing_gate":false,"lines":[],"capture_complete_ns":400000000,"recognition_start_ns":400000000,"recognition_end_ns":400000001,"targets":[]}
{"event":"game_decision","decision_schema":2,"ui":"PLAYING","playing_gate":true,"lines":[],"capture_complete_ns":450000000,"recognition_start_ns":450000000,"recognition_end_ns":450000001,"targets":[]}
{"event":"game_decision","decision_schema":2,"ui":"PLAYING","playing_gate":true,"lines":[],"capture_complete_ns":500000000,"recognition_start_ns":500000000,"recognition_end_ns":500000001,"targets":[]}
)");
    const auto result=analyze_game_jsonl(raw.path());
    EXPECT_EQ(result.at("playing_capture_interval_ms").at("n"),2);
    EXPECT_EQ(result.at("playing_capture_interval_ms").at("max"),200);
    EXPECT_EQ(result.at("capture_interval_ms").at("n"),4);
}

TEST(GameClips, IndexedFramesAndUniqueRgbHashesHaveDifferentDenominators) {
    const auto directory=std::filesystem::temp_directory_path()/
        ("pas-clip-dedup-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory/"round-1/clip-1-uniform");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{directory};
    constexpr std::size_t bytes=1280ULL*720*3;
    const std::vector<std::uint8_t> rgb(bytes,0);
    nlohmann::json rows=nlohmann::json::array();
    for(int i=0;i<3;++i) {
        const auto relative="round-1/clip-1-uniform/frame-"+std::to_string(i)+".rgb";
        const auto file=directory/relative;
        {std::ofstream out(file,std::ios::binary);
         out.write(reinterpret_cast<const char*>(rgb.data()),static_cast<std::streamsize>(rgb.size()));}
        rows.push_back({{"round_id",1},{"clip_id",1},{"frame_in_clip",i},
            {"path",relative},{"layout","top_down_rgb888"},{"width",1280},{"height",720},
            {"stride",3840},{"source_frame",i+1},{"capture_complete_ns",1'000'000'000LL+i*20'000'000LL},
            {"sha256",sha256_file(file)},{"detected_lines",0},{"detected_targets",0}});
    }
    {std::ofstream out(directory/"index.jsonl",std::ios::binary);
     for(const auto& row:rows)out<<row.dump()<<'\n';}
    const auto result=replay_game_pixel_clips(directory);
    EXPECT_EQ(result.at("indexed_rgb_frames"),3);
    EXPECT_EQ(result.at("unique_rgb_frames"),1);
    EXPECT_EQ(result.at("clips"),1);
    EXPECT_EQ(result.at("frames").at(0).at("source_rotation"),1);
}

TEST(GameClips, CrossSessionCorpusKeepsOneSongFamilyAndVerifiesDuplicateRgb) {
    const auto directory=std::filesystem::temp_directory_path()/
        ("pas-corpus-dedup-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{directory};
    std::vector<std::uint8_t> rgb(1280ULL*720*3,0);
    for(int y=500;y<503;++y)for(int x=100;x<1180;++x)
        for(int channel=0;channel<3;++channel)
            rgb[static_cast<std::size_t>(y)*3840+x*3+channel]=255;
    const auto make_clips=[&](const std::string& name) {
        const auto root=directory/name;
        std::filesystem::create_directories(root/"round-1/clip-1-uniform");
        std::ofstream index(root/"index.jsonl",std::ios::binary);
        for(int i=0;i<3;++i) {
            const auto relative="round-1/clip-1-uniform/frame-"+std::to_string(i)+".rgb";
            const auto file=root/relative;
            {std::ofstream out(file,std::ios::binary);
             out.write(reinterpret_cast<const char*>(rgb.data()),static_cast<std::streamsize>(rgb.size()));}
            index<<nlohmann::json{{"round_id",1},{"clip_id",1},{"frame_in_clip",i},
                {"path",relative},{"layout","top_down_rgb888"},{"width",1280},
                {"height",720},{"stride",3840},{"source_frame",i+1},
                {"capture_complete_ns",1'000'000'000LL+i*20'000'000LL},
                {"sha256",sha256_file(file)},{"detected_lines",0},{"detected_targets",0}}.dump()<<'\n';
        }
        return root;
    };
    const auto old_root=make_clips("old"),new_root=make_clips("new");
    const auto session_manifest=directory/"manifest.json";
    {std::ofstream out(session_manifest);
     out<<R"({"capability_preflight":{"capture_geometry":{"width":1280,"height":720,"source_rotation":1}}})";}
    const auto old_meta=directory/"old-results.json",new_meta=directory/"new-results.json";
    {std::ofstream out(old_meta);out<<R"({"rounds":[{"round_id":1,"song":"same-family","difficulty":"HD"}]})";}
    {std::ofstream out(new_meta);out<<R"({"results":[{"round":1,"song":"same-family","mode":"IN"}]})";}
    const auto result=index_game_pixel_corpus(old_root,old_meta,new_root,new_meta);
    EXPECT_EQ(result.at("indexed_rgb_frames"),6);
    EXPECT_EQ(result.at("unique_rgb_hashes"),1);
    EXPECT_EQ(result.at("clip_count"),2);
    EXPECT_EQ(result.at("family_count"),1);
    EXPECT_EQ(result.at("selected_count"),2);
    EXPECT_FALSE(result.at("human_reviewed_truth").get<bool>());
    EXPECT_EQ(result.at("clips").at(0).at("truth_status"),"unreviewed_source_pixels");
    const auto index_file=directory/"corpus.json",proposal_dir=directory/"proposals";
    {std::ofstream out(index_file);out<<result.dump(2);}
    const auto written=write_game_corpus_proposals(index_file,proposal_dir);
    EXPECT_EQ(written.at("selected_clips"),2);
    EXPECT_EQ(written.at("proposal_frames"),6);
    const auto proposal_file=proposal_dir/"proposals.json";
    EXPECT_EQ(validate_game_corpus_proposals(index_file,proposal_file).at("source_frames_verified"),6);
    std::ifstream proposal_input(proposal_file);
    auto altered=nlohmann::json::parse(proposal_input);
    const auto& lines=altered.at("proposal_frames").at(0).at("line_instances");
    ASSERT_FALSE(lines.empty());
    EXPECT_FALSE(lines.at(0).at("visible_support_segments").empty());
    const auto altered_file=directory/"improper-gold.json";
    altered["gold_eligible"]=true;
    {std::ofstream out(altered_file);out<<altered.dump(2);}
    EXPECT_THROW(validate_game_corpus_proposals(index_file,altered_file),std::invalid_argument);
    {std::ofstream out(session_manifest,std::ios::trunc);
     out<<R"({"capability_preflight":{"capture_geometry":{"width":1280,"height":720,"source_rotation":0}}})";}
    EXPECT_THROW(index_game_pixel_corpus(old_root,old_meta,new_root,new_meta),std::invalid_argument);
    {std::ofstream out(session_manifest,std::ios::trunc);
     out<<R"({"capability_preflight":{"capture_geometry":{"width":1280,"height":720,"source_rotation":1}}})";}
    {std::ofstream out(new_meta,std::ios::trunc);out<<R"({"results":[{"round":1,"song":"","mode":"IN"}]})";}
    EXPECT_THROW(index_game_pixel_corpus(old_root,old_meta,new_root,new_meta),std::invalid_argument);
}

TEST(GameColdCoverage, PendingCasesAreNotSuccessAndProposedCannotBecomeGold) {
    nlohmann::json rows=nlohmann::json::array();
    const auto add=[&](const std::string& group,bool risk) {
        for(const std::string variant:{"normal","negative"})
            rows.push_back({{"case_id",group+"-"+variant},{"form_item","required scenario"},
                {"matrix_group",risk? nlohmann::json(nullptr):nlohmann::json(group)},
                {"risk_combo",risk?nlohmann::json(group):nlohmann::json(nullptr)},
                {"variant",variant},{"required_layers",{"RGB","oracle","fake-clock"}},
                {"verified_layers",nlohmann::json::array()},{"expected_result","known action or explicit refusal"},
                {"input_sha256",nullptr},{"seed",nullptr},{"test_name",nullptr},
                {"result_path",nullptr},{"status","pending"}});
    };
    for(const std::string group:{"G1","G2","G3","N1","N2","H1","H2","T1","T2"})add(group,false);
    for(const std::string risk:{"R1","R2","R3","R4","R5","R6","R7"})add(risk,true);
    nlohmann::json manifest={{"schema_version",1},{"case_count",32},{"cases",rows}};
    const TempJson pending(manifest.dump());
    const auto summary=validate_game_cold_coverage_manifest(pending.path());
    EXPECT_EQ(summary.at("required_cases"),32);
    EXPECT_EQ(summary.at("pending"),32);
    EXPECT_EQ(summary.at("passed"),0);
    EXPECT_FALSE(summary.at("complete").get<bool>());
    manifest["cases"][0]["status"]="passed";
    manifest["cases"][0]["truth_basis"]="proposed";
    manifest["cases"][0]["seed"]=20260929;
    manifest["cases"][0]["verified_layers"]={"RGB","oracle","fake-clock"};
    manifest["cases"][0]["test_name"]="unreviewed";
    manifest["cases"][0]["result_path"]="nonexistent.json";
    const TempJson invalid(manifest.dump());
    EXPECT_THROW(validate_game_cold_coverage_manifest(invalid.path()),std::invalid_argument);
}

TEST(GameClips, ReplayDiffRequiresMatchingVerifiedFrameHashesAndReportsOnlyCandidateCounts) {
    const std::string old_hash(64,'a'),new_hash(64,'b');
    const nlohmann::json corpus={{"indexed_rgb_frames",2},{"unique_rgb_hashes",2},
        {"clips",nlohmann::json::array({
            {{"source_strategy","observer36/planner18"},{"round_id",1},{"clip_number",1},
                {"family","family-one"},{"frames",nlohmann::json::array({
                    {{"frame_in_clip",0},{"sha256",old_hash}}})}},
            {{"source_strategy","observer37/planner19"},{"round_id",2},{"clip_number",3},
                {"family","family-two"},{"frames",nlohmann::json::array({
                    {{"frame_in_clip",0},{"sha256",new_hash}}})}}
        })}};
    const auto frame=[](int round,int clip,const std::string& hash,int lines,int targets) {
        return nlohmann::json{{"round_id",round},{"clip_id",clip},{"frame_in_clip",0},
            {"source_frame",17},{"sha256",hash},{"cold_lines",lines},{"cold_targets",targets}};
    };
    const auto replay=[](const nlohmann::json& value) {
        return nlohmann::json{{"input_integrity_verified",true},
            {"frames",nlohmann::json::array({value})}};
    };
    const TempJson index(corpus.dump()),before_old(replay(frame(1,1,old_hash,1,1)).dump()),
        after_old(replay(frame(1,1,old_hash,2,1)).dump()),
        before_new(replay(frame(2,3,new_hash,0,0)).dump()),
        after_new(replay(frame(2,3,new_hash,0,0)).dump());
    const auto result=compare_game_pixel_replays(index.path(),before_old.path(),after_old.path(),
        before_new.path(),after_new.path());
    EXPECT_EQ(result.at("indexed_frames_compared"),2);
    EXPECT_EQ(result.at("changed_frame_count"),1);
    EXPECT_EQ(result.at("by_family").size(),2);
    EXPECT_FALSE(result.at("ground_truth_comparison").get<bool>());
    const TempJson corrupt(replay(frame(1,1,std::string(64,'c'),2,1)).dump());
    EXPECT_THROW(compare_game_pixel_replays(index.path(),before_old.path(),corrupt.path(),
        before_new.path(),after_new.path()),std::invalid_argument);
}

TEST(GameColdPipeline, ConsumedPublicationTimesComeFromSynchronizedFrameLeases) {
    const TempJson journal("");
    const auto summary=benchmark_game_cold_pipeline(journal.path(),48,8,true,true,
        "tap",8192,0,0);
    EXPECT_EQ(summary.at("publication_time_basis"),"latest_frame_lease_under_publish_lock");
    EXPECT_EQ(summary.at("published"),48);
    EXPECT_EQ(summary.at("writer_debug_drops"),0);
    EXPECT_TRUE(summary.at("contacts_released").get<bool>());
    std::ifstream input(journal.path());
    std::string line;
    std::size_t consumed=0;
    while(std::getline(input,line)) {
        const auto event=nlohmann::json::parse(line);
        if(event.value("event","")!="cold_frame_consumed")continue;
        const auto capture=event.at("capture_complete_ns").get<Nanoseconds>();
        const auto published=event.at("published_ns").get<Nanoseconds>();
        const auto recognized=event.at("recognition_end_ns").get<Nanoseconds>();
        EXPECT_GT(published,0);
        EXPECT_LE(capture,published);
        EXPECT_LE(published,recognized);
        ++consumed;
    }
    EXPECT_GT(consumed,0);
    EXPECT_EQ(consumed,summary.at("consumed").get<std::size_t>());
}
