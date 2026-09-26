#include "pas/native_capture.hpp"
#include "pas/scrcpy_capture.hpp"

#include <gtest/gtest.h>

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
