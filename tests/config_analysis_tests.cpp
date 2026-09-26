#include "pas/analysis.hpp"
#include "pas/config.hpp"
#include "pas/game.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

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
}
