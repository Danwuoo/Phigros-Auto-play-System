#include "pas/native_capture.hpp"
#include "pas/scrcpy_capture.hpp"
#include "pas/bench.hpp"
#include "pas/analysis.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <fstream>

namespace {
struct DiagnosticTestDirectory {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("pas-image-retention-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    DiagnosticTestDirectory() {
        if (!std::filesystem::create_directory(root))
            throw std::runtime_error("test directory already exists");
    }
    ~DiagnosticTestDirectory() {
        std::error_code ignored;
        std::filesystem::remove(root / "diagnostic.png", ignored);
        std::filesystem::remove(root, ignored);
    }
};
}

TEST(CaptureImages, DefaultNeverWritesOrDeletesAnExistingImage) {
    DiagnosticTestDirectory directory;
    const auto path = directory.root / "diagnostic.png";
    pas::CaptureBenchOptions options;
    EXPECT_FALSE(options.keep_diagnostic_image);
    pas::Frame empty;
    EXPECT_EQ(pas::save_capture_diagnostic(path, empty, options.keep_diagnostic_image), std::nullopt);
    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_TRUE(pas::diagnostic_image_matches(path, "none", nullptr));
    EXPECT_FALSE(pas::diagnostic_image_matches(path, "keep", "missing"));
    EXPECT_FALSE(pas::diagnostic_image_matches(path, "none", "unexpected-hash"));
    { std::ofstream file(path); file << "existing evidence"; }
    const auto original = pas::sha256_file(path);
    EXPECT_EQ(pas::save_capture_diagnostic(path, empty, false), std::nullopt);
    EXPECT_EQ(pas::sha256_file(path), original);
    EXPECT_FALSE(pas::diagnostic_image_matches(path, "none", nullptr));
}

TEST(CaptureImages, ExplicitKeepWritesVerifiablePngAndNeverOverwrites) {
    DiagnosticTestDirectory directory;
    const auto path = directory.root / "diagnostic.png";
    pas::Frame frame;
    frame.width = 2; frame.height = 1; frame.stride = 6;
    frame.rgb = {255, 0, 0, 0, 255, 0};
    const auto hash = pas::save_capture_diagnostic(path, frame, true);
    ASSERT_TRUE(hash.has_value());
    EXPECT_TRUE(pas::diagnostic_image_matches(path, "keep", *hash));
    EXPECT_FALSE(pas::diagnostic_image_matches(path, "keep", nullptr));
    EXPECT_FALSE(pas::diagnostic_image_matches(path, "unknown", *hash));
    EXPECT_THROW(pas::save_capture_diagnostic(path, frame, true), std::runtime_error);
    { std::ofstream file(path, std::ios::app | std::ios::binary); file << "tampered"; }
    EXPECT_FALSE(pas::diagnostic_image_matches(path, "keep", *hash));
    std::filesystem::remove(path);
    EXPECT_FALSE(pas::diagnostic_image_matches(path, "keep", *hash));
}

TEST(NativeRgb, HonorsMappedRowPitchAndRejectsTruncation) {
    const std::array<std::uint8_t, 24> bgra = {
        0, 0, 255, 255, 0, 255, 0, 255, 9, 9, 9, 9,
        255, 0, 0, 255, 255, 255, 255, 255, 8, 8, 8, 8};
    std::vector<std::uint8_t> rgb;
    pas::copy_bgra_rows_to_rgb24(bgra.data(), bgra.size(), 12, 2, 2, rgb);
    EXPECT_EQ(rgb, (std::vector<std::uint8_t>{255, 0, 0, 0, 255, 0,
                                               0, 0, 255, 255, 255, 255}));
    EXPECT_THROW(pas::copy_bgra_rows_to_rgb24(bgra.data(), 23, 12, 2, 2, rgb),
                 std::invalid_argument);
    EXPECT_THROW(pas::copy_bgra_rows_to_rgb24(bgra.data(), bgra.size(), 7, 2, 2, rgb),
                 std::invalid_argument);
}

TEST(NativeGeometry, MapsNegativeDesktopOriginAndRejectsCrossMonitorCrop) {
    EXPECT_EQ(pas::map_desktop_crop(-1800, 100, 1280, 720, -1920, 0, 0, 1080),
              (std::pair<int, int>{120, 100}));
    EXPECT_THROW(pas::map_desktop_crop(-300, 100, 1280, 720, -1920, 0, 0, 1080),
                 std::runtime_error);
}

TEST(NativeGeometry, CropMustFitTheEntireClientBeyondVisibilitySamplePoints) {
    EXPECT_NO_THROW(pas::validate_client_crop(0, 0, 1280, 720, 1280, 720));
    EXPECT_THROW(pas::validate_client_crop(1, 0, 1280, 720, 1280, 720), std::runtime_error);
    EXPECT_THROW(pas::validate_client_crop(0, 1, 1280, 720, 1280, 720), std::runtime_error);
    EXPECT_THROW(pas::validate_client_crop(-1, 0, 1280, 720, 1280, 720), std::runtime_error);
}

TEST(FixtureIdentity, FourIdenticalXorChipsCannotOverrideIndependentBinaryTruth) {
    pas::Frame frame;
    frame.width = 1280; frame.height = 720; frame.stride = 3840;
    frame.rgb.resize(static_cast<std::size_t>(frame.stride) * frame.height);
    const auto pixel = [&](int x, int y, std::uint32_t value) {
        const auto offset = static_cast<std::size_t>(y) * frame.stride + x * 3;
        frame.rgb[offset] = static_cast<std::uint8_t>(value >> 16);
        frame.rgb[offset+1] = static_cast<std::uint8_t>(value >> 8);
        frame.rgb[offset+2] = static_cast<std::uint8_t>(value);
    };
    constexpr std::uint32_t binary_truth = 23904, wrong_chip = 484957;
    for (int row = 0; row < 2; ++row) {
        const int y = 96 + row * 26;
        pixel(16, y, 0xFF0000); pixel(206, y, 0xFF0000);
        for (int bit = 0; bit < 12; ++bit)
            pixel(36 + bit * 14, y, binary_truth & (1u << (row * 12 + bit)) ? 0xFFFFFF : 0);
    }
    for (int corner = 0; corner < 4; ++corner) {
        const int x = corner % 2 ? 1190 : 65, y = corner / 2 ? 630 : 55;
        pixel(x+12, y+10, wrong_chip);
        pixel(x+37, y+10, wrong_chip ^ 0xA5C37E);
    }
    const auto decoded = pas::decode_capture_fixture_counter(frame);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->value, binary_truth);
    EXPECT_STREQ(decoded->schema, "native_v2_lossy_validated");
    EXPECT_EQ(decoded->binary_value, binary_truth);
    EXPECT_EQ(decoded->exact_colour_value, wrong_chip);
    EXPECT_GT(decoded->max_color_error, 0);
    EXPECT_LE(decoded->max_color_error, 32);
    for (int corner = 0; corner < 4; ++corner) {
        const int x = corner % 2 ? 1190 : 65, y = corner / 2 ? 630 : 55;
        pixel(x+12, y+10, binary_truth);
        pixel(x+37, y+10, binary_truth ^ 0xA5C37E);
    }
    const auto exact = pas::decode_capture_fixture_counter(frame);
    ASSERT_TRUE(exact);
    EXPECT_STREQ(exact->schema, "native_v2_four_region");
    EXPECT_EQ(exact->value, binary_truth);
    pixel(16, 96, 0);
    EXPECT_FALSE(pas::decode_capture_fixture_counter(frame));
}

TEST(NativeTiming, RejectsQueuedPauseFrameThenAcceptsCurrentAndClockRegressionFails) {
    pas::NativeRelativeLagGuard guard(250'000'000);
    EXPECT_TRUE(guard.accept(1'000'000'000, 800'000'000));
    EXPECT_TRUE(guard.accept(1'020'000'000, 820'000'000));
    EXPECT_FALSE(guard.accept(1'520'000'000, 840'000'000));
    EXPECT_TRUE(guard.accept(1'540'000'000, 1'340'000'000));
    EXPECT_THROW(guard.accept(1'560'000'000, 1'300'000'000), std::runtime_error);
}

TEST(ScrcpyProtocol, DistinguishesSessionAndBoundedMediaPackets) {
    const std::array<std::uint8_t, 12> session = {
        0x80, 0, 0, 0, 0, 0, 5, 0, 0, 0, 2, 0};
    auto parsed = pas::parse_scrcpy_v41_header(session);
    EXPECT_TRUE(parsed.session);
    EXPECT_EQ(parsed.width, 1280u);
    EXPECT_EQ(parsed.height, 512u);
    const std::array<std::uint8_t, 12> packet = {
        0x60, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 16};
    parsed = pas::parse_scrcpy_v41_header(packet);
    EXPECT_FALSE(parsed.session);
    EXPECT_TRUE(parsed.config);
    EXPECT_TRUE(parsed.key);
    EXPECT_EQ(parsed.pts_us, 1);
    EXPECT_EQ(parsed.packet_size, 16u);
    auto malformed = packet;
    malformed[8] = malformed[9] = malformed[10] = malformed[11] = 0;
    EXPECT_THROW(pas::parse_scrcpy_v41_header(malformed), std::runtime_error);
    malformed = session;
    malformed[1] = 1;
    EXPECT_THROW(pas::parse_scrcpy_v41_header(malformed), std::runtime_error);
}
