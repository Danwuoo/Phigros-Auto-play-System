#include "pas/adb.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <chrono>
#define NOMINMAX
#include <windows.h>

namespace {
nlohmann::json run_pas(const std::vector<std::string>& arguments) {
    wchar_t module[32768]{};
    const auto size = GetModuleFileNameW(nullptr, module, 32768);
    if (!size || size == 32768) throw std::runtime_error("test executable path unavailable");
    const auto executable = std::filesystem::path(module).parent_path() / "pas.exe";
    // The existing bounded Win32 launcher accepts an executable and argv.
    const auto output = pas::adb_call(executable, arguments, 10'000);
    return nlohmann::json::parse(output.begin(), output.end());
}
}

TEST(CliAcceptance, TrackingComparisonUsesFakeBackendAndWritesNoLiveInput){
 const auto path=std::filesystem::temp_directory_path()/("pas-tracking-cli-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
 const auto generated=run_pas({"dataset","tracking-challenge","--output",path.string()});EXPECT_FALSE(generated.at("input_created").get<bool>());
 const auto report=run_pas({"analyze","tracking",path.string(),"--methods","legacy,byte_association,oc_observation","--updates","10"});
 EXPECT_TRUE(report.at("offline_only").get<bool>());EXPECT_EQ(report.at("touch_backend"),"FakeTouchBackend");EXPECT_FALSE(report.at("real_input_created").get<bool>());EXPECT_EQ(report.at("methods").size(),3);
 EXPECT_EQ(report.at("methods").at("byte_association").at("correctness").at("fake_duplicate_down"),0);
 EXPECT_GT(report.at("methods").at("byte_association").at("correctness").at("fake_instances_with_down").get<int>(),0);
 std::error_code ignored;std::filesystem::remove(path,ignored);
}

TEST(CliAcceptance, TinyFrameCaptureRemainsWithinAllocatedPixels) {
    for (const auto* width : {"1", "2"}) {
        const auto report = run_pas({"offline-capture-bench", "--width", width, "--height", "1",
            "--warmup-s", "0", "--duration-s", "0.1"});
        EXPECT_GT(report.at("consumed").get<int>(), 0);
        EXPECT_EQ(report.at("pool_drops"), 0);
    }
}

TEST(CliAcceptance, SyntheticPixelEffectsAndTrackingSurviveRecognitionDelay) {
    for (const auto* delay : {"0", "5", "25"}) {
        const auto report = run_pas({"synthetic", "--count", "30", "--fps", "60",
            "--recognition-delay-ms", delay});
        EXPECT_EQ(report.at("hits"), 30) << "delay=" << delay;
        EXPECT_EQ(report.at("effects_seen"), 30) << "delay=" << delay;
        EXPECT_EQ(report.at("false_touches"), 0);
    }
}
