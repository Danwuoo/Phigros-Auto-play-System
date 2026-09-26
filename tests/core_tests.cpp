#include "pas/core.hpp"
#include "pas/touch_evidence.hpp"
#include <gtest/gtest.h>

#include <array>
#include <vector>

using namespace pas;

namespace {
ContactPlan plan(std::uint64_t id, std::uint64_t revision, Nanoseconds evidence,
                 Nanoseconds down, Nanoseconds up, std::uint64_t epoch = 1) {
    return ContactPlan{epoch, id, revision, evidence, down + 30'000'000, 1,
                       "visible_target", {{Phase::down, 10, 20, down},
                                          {Phase::up, 10, 20, up}}};
}
}

TEST(FramePool, LatestAndLeasesStayBounded) {
    LatestFrame frames(2, 2, 2);
    std::array<std::uint8_t, 12> a{}; a[0] = 1;
    Frame meta; meta.width = 2; meta.height = 2; meta.stride = 6; meta.sequence = 1;
    ASSERT_TRUE(frames.publish(a.data(), a.size(), meta));
    auto lease_a = frames.read_after(0, 0);
    ASSERT_TRUE(lease_a);
    a[0] = 2; meta.sequence = 2;
    ASSERT_TRUE(frames.publish(a.data(), a.size(), meta));
    auto lease_b = frames.read_after(1, 0);
    ASSERT_TRUE(lease_b);
    a[0] = 3; meta.sequence = 3;
    EXPECT_FALSE(frames.publish(a.data(), a.size(), meta));
    EXPECT_EQ(lease_a->rgb[0], 1);
    EXPECT_EQ(lease_b->rgb[0], 2);
    lease_a.reset();
    ASSERT_TRUE(frames.publish(a.data(), a.size(), meta));
    EXPECT_EQ(frames.counters().pool_drops, 1);
}

TEST(FramePool, InvalidGeometryAndPayloadNeverAllocateUnboundedStorage) {
    EXPECT_THROW(LatestFrame(1'000'000'000, 1'000'000'000), std::invalid_argument);
    LatestFrame frames(2, 2);
    Frame metadata; metadata.width = 2; metadata.height = 2;
    metadata.stride = 6; metadata.sequence = 1;
    std::array<std::uint8_t, 11> short_pixels{};
    EXPECT_THROW(frames.publish(short_pixels.data(), short_pixels.size(), metadata),
                 std::invalid_argument);
    EXPECT_EQ(frames.counters().published, 0);
}

TEST(SchedulerP1, EvidenceExpiresBeforeNextCommandAndAtBoundary) {
    FakeClock clock;
    FakeTouchBackend touch(clock);
    ContactScheduler owner(clock, touch);
    ASSERT_TRUE(owner.set_gate(1, true, 0));
    ASSERT_TRUE(owner.submit(plan(1, 1, 0, 10'000'000, 1'000'000'000)));
    clock.set(10'000'000);
    ASSERT_EQ(owner.run_due().size(), 1);
    EXPECT_EQ(owner.active_count(), 1);
    EXPECT_EQ(owner.next_due_ns(), 150'000'000);
    clock.set(149'999'999);
    EXPECT_TRUE(owner.run_due().empty());
    EXPECT_EQ(owner.active_count(), 1);
    clock.set(150'000'000);
    EXPECT_TRUE(owner.run_due().empty());
    EXPECT_EQ(owner.active_count(), 0);
    EXPECT_FALSE(owner.armed());
    EXPECT_EQ(owner.fault(), "evidence_expired");
    EXPECT_TRUE(touch.contacts().empty());
}

TEST(SchedulerP1, FreshGateDoesNotRefreshOldPlanAtDispatch) {
    FakeClock clock;
    FakeTouchBackend touch(clock);
    ContactScheduler owner(clock, touch);
    ASSERT_TRUE(owner.set_gate(1, true, 0));
    ASSERT_TRUE(owner.submit(plan(1, 1, 0, 300'000'000, 320'000'000)));
    clock.set(149'000'000);
    ASSERT_TRUE(owner.set_gate(1, true, 149'000'000));
    EXPECT_EQ(owner.next_due_ns(), 150'000'000);
    clock.set(150'000'000);
    EXPECT_TRUE(owner.run_due().empty());
    EXPECT_TRUE(touch.receipts().empty());
    EXPECT_TRUE(owner.fault().empty());
    EXPECT_TRUE(owner.armed());
    EXPECT_EQ(owner.pending_count(), 0);
}

TEST(SchedulerP1, RevisionCannotReviveCompletedOrEvictedIntent) {
    FakeClock clock;
    FakeTouchBackend touch(clock);
    ContactScheduler owner(clock, touch);
    ASSERT_TRUE(owner.set_gate(1, true, 0));
    ASSERT_TRUE(owner.submit(plan(1, 1, 0, 1, 2)));
    clock.set(2);
    ASSERT_EQ(owner.run_due().size(), 2);
    EXPECT_FALSE(owner.submit(plan(1, 2, 0, 3, 4)));
    for (std::uint64_t id = 2; id <= 140; ++id) {
        ASSERT_TRUE(owner.submit(plan(id, 1, clock.now_ns(), clock.now_ns() + 1,
                                      clock.now_ns() + 2)));
        clock.set(clock.now_ns() + 2);
        ASSERT_EQ(owner.run_due().size(), 2);
    }
    EXPECT_EQ(owner.accepted_high_watermark(), 140);
    EXPECT_FALSE(owner.submit(plan(1, 10'000, clock.now_ns(), clock.now_ns() + 1,
                                   clock.now_ns() + 2)));
    EXPECT_EQ(touch.receipts().size(), 280);
}

TEST(Scheduler, EpochAndGateRevocationKeepBirthIdentity) {
    FakeClock clock;
    FakeTouchBackend touch(clock);
    ContactScheduler owner(clock, touch);
    ASSERT_TRUE(owner.set_gate(1, true, 0));
    ASSERT_TRUE(owner.submit(plan(8, 1, 0, 1, 2)));
    ASSERT_TRUE(owner.set_gate(1, false));
    ASSERT_TRUE(owner.set_gate(1, true, 0));
    EXPECT_FALSE(owner.submit(plan(8, 2, 0, 1, 2)));
    ASSERT_TRUE(owner.set_gate(2, true, 0));
    EXPECT_TRUE(owner.submit(plan(8, 1, 0, 1, 2, 2)));
}

TEST(Scheduler, StopBeforeDuePreventsDownAndReleases) {
    FakeClock clock;
    FakeTouchBackend touch(clock);
    ContactScheduler owner(clock, touch);
    ASSERT_TRUE(owner.set_gate(1, true, 0));
    ASSERT_TRUE(owner.submit(plan(1, 1, 0, 10, 20)));
    owner.request_stop();
    clock.set(10);
    EXPECT_TRUE(owner.run_due().empty());
    EXPECT_FALSE(owner.armed());
    EXPECT_TRUE(touch.receipts().empty());
    EXPECT_TRUE(touch.contacts().empty());
}

TEST(Vision, PixelsTrackAndPredictCrossing) {
    FakeClock clock;
    GreenTargetDetector detector(clock);
    VelocityTracker tracker;
    LineCrossingPredictor predictor(48);
    Frame frame; frame.width = 64; frame.height = 64; frame.stride = 192;
    frame.rgb.resize(64 * 64 * 3);
    std::optional<HitIntent> intent;
    for (int i = 0; i < 4; ++i) {
        std::fill(frame.rgb.begin(), frame.rgb.end(), 0);
        frame.rgb[(static_cast<std::size_t>(8 + i * 4) * 64 + 32) * 3 + 1] = 255;
        frame.sequence = i + 1;
        frame.capture_complete_ns = i * 50'000'000;
        clock.set(frame.capture_complete_ns);
        auto observation = detector.detect(frame);
        ASSERT_TRUE(observation);
        auto track = tracker.update(observation);
        if (track) intent = predictor.predict(*track);
    }
    ASSERT_TRUE(intent);
    EXPECT_NEAR(intent->x, 32, 0.01);
    EXPECT_EQ(intent->predicted_hit_ns, 500'000'000);
    EXPECT_NE(intent->basis.find("frame=4"), std::string::npos);
}

TEST(Coordinates, QuarterTurnsMapInclusiveEdges) {
    PixelCoordinateMap map(1280, 720, 720, 1280, 90);
    EXPECT_EQ(map.map(0, 0), (std::array<int, 2>{719, 0}));
    EXPECT_EQ(map.map(1279, 719), (std::array<int, 2>{0, 1279}));
    EXPECT_THROW(map.map(1280, 0), std::invalid_argument);
}

TEST(TouchEvidence, PerPointerPathAndNegativeVariants) {
    TouchEvidence evidence;
    evidence.complete = true;
    evidence.active = 0;
    evidence.samples = {
        {1, 1, 11, 10, 10}, {2, 1, 12, 30, 30},
        {3, 2, 11, 20, 10}, {4, 2, 12, 30, 30},
        {5, 2, 11, 10, 10}, {6, 2, 12, 30, 30},
        {7, 3, 12, 30, 30}, {8, 3, 11, 10, 10}};
    const std::vector<ExpectedPointer> expected = {
        {1, 11, 10, 10}, {1, 12, 30, 30},
        {2, 11, 20, 10}, {2, 12, 30, 30},
        {2, 11, 10, 10}, {2, 12, 30, 30},
        {3, 12, 30, 30}, {3, 11, 10, 10}};
    EXPECT_TRUE(verify_touch_path(evidence, expected, 2, 0, {12}).passed);
    auto both_drift = evidence;
    both_drift.samples[3].x = 40;
    EXPECT_FALSE(verify_touch_path(both_drift, expected, 2, 0, {12}).passed);
    auto reverse_wrong = evidence;
    reverse_wrong.samples[4].x = 25;
    EXPECT_FALSE(verify_touch_path(reverse_wrong, expected, 2, 0, {12}).passed);
    auto missing_up = evidence;
    missing_up.samples.pop_back();
    EXPECT_FALSE(verify_touch_path(missing_up, expected, 2, 0, {12}).passed);
    auto mixed_id = evidence;
    mixed_id.samples[4].android_id = 12;
    EXPECT_FALSE(verify_touch_path(mixed_id, expected, 2, 0, {12}).passed);
    auto missing_segment = evidence;
    missing_segment.samples.erase(missing_segment.samples.begin() + 4);
    EXPECT_FALSE(verify_touch_path(missing_segment, expected, 2, 0, {12}).passed);
    auto overflow = evidence;
    overflow.complete = false;
    overflow.error = "ring overflow";
    EXPECT_FALSE(verify_touch_path(overflow, expected, 2, 0, {12}).passed);
}

TEST(TouchEvidence, AcceptsOneAndroidResampledMoveInsideNextSegment) {
    TouchEvidence evidence;
    evidence.complete = true;
    evidence.active = 0;
    evidence.samples = {
        {1, 1, 0, 640, 210}, {2, 2, 0, 708, 210},
        {3, 2, 0, 730, 210}, {4, 2, 0, 685, 210},
        {5, 2, 0, 640, 210}, {6, 3, 0, 640, 210}};
    const std::vector<ExpectedPointer> expected = {
        {1, 0, 640, 210}, {2, 0, 685, 210},
        {2, 0, 730, 210}, {2, 0, 685, 210},
        {2, 0, 640, 210}, {3, 0, 640, 210}};
    EXPECT_TRUE(verify_touch_path(evidence, expected, 4, 0).passed);
    evidence.samples[1].y = 222;
    EXPECT_FALSE(verify_touch_path(evidence, expected, 4, 0).passed);
    evidence.samples[1].y = 210;
    evidence.samples[1].x = 610;
    EXPECT_FALSE(verify_touch_path(evidence, expected, 4, 0).passed);
}

TEST(TouchEvidence, NativePixelRingChecksEverySegment) {
    Frame frame;
    frame.width = 320; frame.height = 720; frame.stride = 960;
    frame.rgb.resize(static_cast<std::size_t>(frame.stride) * frame.height);
    const auto cell = [&](int x, int y, std::uint32_t value) {
        const auto offset = static_cast<std::size_t>(y) * frame.stride + x * 3;
        frame.rgb[offset] = static_cast<std::uint8_t>((value >> 16) & 255);
        frame.rgb[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 255);
        frame.rgb[offset + 2] = static_cast<std::uint8_t>(value & 255);
    };
    cell(24,50,0x504153); cell(54,50,2); cell(84,50,2);
    cell(174,50,1); cell(204,50,1);
    const auto trace = [&](int index, std::uint32_t sequence, int phase) {
        const auto y = 143 + index * 8;
        const auto meta = static_cast<std::uint32_t>(phase) << 16;
        cell(32,y,sequence); cell(62,y,meta);
        cell(92,y,100); cell(122,y,200);
        cell(152,y,(sequence ^ meta ^ 100 ^ 200 ^ 0xA5C37E) & 0xFFFFFF);
    };
    trace(0,1,1); trace(1,2,3);
    const auto evidence = decode_touch_evidence(frame,0);
    ASSERT_TRUE(evidence.complete) << evidence.error;
    ASSERT_EQ(evidence.samples.size(),2);
    EXPECT_EQ(evidence.samples[0].phase,1);
    EXPECT_EQ(evidence.samples[1].phase,3);
    cell(152,151,0);
    EXPECT_FALSE(decode_touch_evidence(frame,0).complete);
    EXPECT_FALSE(decode_touch_evidence(frame,0xFFFFC0).complete);
}
