#include "pas/session_recording.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <chrono>

namespace {
std::filesystem::path recording_root(const char* name) {
    return std::filesystem::temp_directory_path()/(std::string("pas-recording-")+name+"-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}
pas::Frame pixels(std::uint64_t sequence) {
    pas::Frame f;f.sequence=sequence;f.width=32;f.height=16;f.stride=96;f.source_rotation=1;
    f.capture_complete_ns=static_cast<pas::Nanoseconds>(sequence)*16'000'000;
    f.pixels_ready_ns=f.capture_complete_ns+100;f.source_sequence=sequence+100;
    f.rgb.resize(1536);for(std::size_t i=0;i<f.rgb.size();++i)f.rgb[i]=static_cast<std::uint8_t>((i*17+sequence*31)%256);
    return f;
}
}
TEST(FullRecording, LosslessPngDecodedByIndependentWicPreservesAllRgbBytes) {
    const auto root=recording_root("rgb");std::filesystem::create_directories(root);
    const auto f=pixels(1);const auto png=pas::encode_recording_png(f);
    std::ofstream file(root/"test.png",std::ios::binary);file.write(reinterpret_cast<const char*>(png.data()),png.size());file.close();
    const auto decoded=pas::load_diagnostic_png(root/"test.png");
    EXPECT_EQ(decoded.rgb,f.rgb);EXPECT_EQ(decoded.width,f.width);EXPECT_EQ(decoded.height,f.height);
    std::filesystem::remove_all(root);
}
TEST(FullRecording, PreRollAndEverySubmittedFrameKeepOrderTimingAndExactPixels) {
    const auto root=recording_root("continuous");pas::HostClock clock;
    pas::FullRecordingOptions options;options.pre_roll_frames=2;options.queue_capacity=8;
    pas::SessionRecording recorder(root,clock,32,16,options);
    for(int i=1;i<=4;++i)recorder.observe(pixels(i));
    recorder.start(1);for(int i=5;i<=8;++i)recorder.observe(pixels(i));
    recorder.finish(1);recorder.close();
    EXPECT_FALSE(recorder.faulted());const auto report=recorder.summary();
    EXPECT_EQ(report.at("frames_written"),6);EXPECT_TRUE(report.at("continuous_received_pixels_complete").get<bool>());
    EXPECT_EQ(report.at("physical_rgb_slots"),13);
    std::ifstream index(root/"index.jsonl");std::string row;int ordinal=0;
    while(std::getline(index,row)) {
        const auto entry=nlohmann::json::parse(row);const auto expected=pixels(ordinal+3);
        EXPECT_EQ(entry.at("source_frame"),ordinal+3);EXPECT_EQ(entry.at("ordinal"),ordinal);
        EXPECT_EQ(entry.at("capture_complete_ns"),expected.capture_complete_ns);
        EXPECT_EQ(entry.at("pre_roll"),ordinal<2);EXPECT_EQ(entry.at("capture_sequence_gap"),0);
        const auto path=root/entry.at("path").get<std::string>();
        EXPECT_EQ(entry.at("png_sha256"),pas::sha256_file(path));
        EXPECT_EQ(pas::load_diagnostic_png(path).rgb,expected.rgb);++ordinal;
    }
    EXPECT_EQ(ordinal,6);index.close();std::filesystem::remove_all(root);
}
TEST(FullRecording, FrameAndByteLimitsFaultWithoutClaimingComplete) {
    pas::HostClock clock;
    for(const bool byte_limit:{false,true}) {
        const auto root=recording_root(byte_limit?"byte-limit":"frame-limit");
        pas::FullRecordingOptions options;options.pre_roll_frames=0;options.queue_capacity=8;
        if(byte_limit)options.byte_limit=1;else options.frame_limit=1;
        pas::SessionRecording recorder(root,clock,32,16,options);recorder.start(1);
        recorder.observe(pixels(1));if(!byte_limit)recorder.observe(pixels(2));
        recorder.finish(1);recorder.close();
        EXPECT_TRUE(recorder.faulted());EXPECT_FALSE(recorder.summary().at("continuous_received_pixels_complete").get<bool>());
        EXPECT_EQ(recorder.error(),byte_limit?"full_recording_byte_limit_incomplete":"full_recording_frame_duration_or_queue_limit_incomplete");
        EXPECT_LE(recorder.summary().at("bytes_png_and_index").get<std::uint64_t>(),options.byte_limit);
        std::filesystem::remove_all(root);
    }
}
TEST(FullRecording, OneRoundAndExistingRootDoNotOverwriteEvidence) {
    const auto root=recording_root("quota");pas::HostClock clock;
    pas::FullRecordingOptions options;options.pre_roll_frames=0;
    pas::SessionRecording recorder(root,clock,32,16,options);recorder.start(1);
    recorder.observe(pixels(1));recorder.finish(1);
    EXPECT_THROW(recorder.start(2),std::runtime_error);recorder.close();
    const auto sha=pas::sha256_file(root/"index.jsonl");
    EXPECT_THROW(pas::SessionRecording(root,clock,32,16,options),std::invalid_argument);
    EXPECT_EQ(pas::sha256_file(root/"index.jsonl"),sha);std::filesystem::remove_all(root);
}
