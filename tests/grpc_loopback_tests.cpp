#include "pas/emulator.hpp"

#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace pas;
namespace emulator_pb = android::emulation::control;

namespace {
class ScreenshotService final : public emulator_pb::EmulatorController::Service {
public:
    explicit ScreenshotService(bool invalid = false, bool frozen = false, bool reset = false)
        : invalid_(invalid), frozen_(frozen), reset_(reset) {}
    grpc::Status streamScreenshot(grpc::ServerContext* context,
                                  const emulator_pb::ImageFormat* request,
                                  grpc::ServerWriter<emulator_pb::Image>* writer) override {
        emulator_pb::Image image;
        image.mutable_format()->CopyFrom(*request);
        image.mutable_format()->set_width(2);
        image.mutable_format()->set_height(2);
        image.set_seq(1);
        image.set_timestampus(123);
        image.set_image(invalid_ ? std::string(11, '\x10') : std::string(12, '\x10'));
        writer->Write(image);
        wrote = true;
        if (frozen_ || reset_) {
            for (int sequence = 2; sequence <= 10 && !context->IsCancelled(); ++sequence) {
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
                image.set_seq(reset_ && sequence == 2 ? 0 : sequence);
                if (!frozen_) image.set_timestampus(123 + sequence * 40'000);
                if (!writer->Write(image)) break;
            }
        } else if (!invalid_)
            while (!context->IsCancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return grpc::Status::OK;
    }
    grpc::Status getScreenshot(grpc::ServerContext*, const emulator_pb::ImageFormat* request,
                               emulator_pb::Image* result) override {
        result->mutable_format()->CopyFrom(*request);
        result->mutable_format()->set_width(2);
        result->mutable_format()->set_height(2);
        result->set_image(std::string(12, '\x10'));
        return grpc::Status::OK;
    }
    std::atomic<bool> wrote = false;
private:
    bool invalid_;
    bool frozen_;
    bool reset_;
};

class LoopbackServer {
public:
    explicit LoopbackServer(ScreenshotService& service) {
        grpc::ServerBuilder builder;
        builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port_);
        builder.RegisterService(&service);
        server_ = builder.BuildAndStart();
        if (!server_ || port_ == 0) throw std::runtime_error("loopback gRPC server failed");
    }
    ~LoopbackServer() { server_->Shutdown(); server_->Wait(); }
    GrpcEndpoint endpoint() const { return {"127.0.0.1:" + std::to_string(port_), "fixture-test", "loopback"}; }
private:
    int port_ = 0;
    std::unique_ptr<grpc::Server> server_;
};
}

TEST(GrpcLoopback, PayloadPixelsAndCooperativeCancel) {
    ScreenshotService service;
    LoopbackServer server(service);
    HostClock clock;
    CaptureOptions options;
    options.width = 2; options.height = 2;
    GrpcCapture capture(clock, server.endpoint(), options);
    std::atomic<int> count = 0;
    std::exception_ptr error;
    std::jthread worker([&](std::stop_token stop) {
        try {
            capture.stream(stop, [&](Frame&& frame) {
                if (frame.width == 2 && frame.height == 2 && frame.rgb.size() == 12 &&
                    frame.source_sequence == 1) ++count;
            });
        } catch (...) { error = std::current_exception(); }
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (count == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    worker.request_stop();
    capture.cancel();
    worker.join();
    EXPECT_EQ(count, 1);
    EXPECT_FALSE(error);
    EXPECT_EQ(capture.stats().valid, 1);
    Frame last; last.width = 2; last.height = 2; last.rgb.assign(12, 0x10);
    EXPECT_EQ(capture.probe_pixels(last, std::chrono::milliseconds(500)), "static");
}

TEST(GrpcLoopback, TruncatedPayloadFaultsBeforePublish) {
    ScreenshotService service(true);
    LoopbackServer server(service);
    HostClock clock;
    CaptureOptions options;
    options.width = 2; options.height = 2;
    GrpcCapture capture(clock, server.endpoint(), options);
    std::stop_source stop;
    int published = 0;
    EXPECT_THROW(capture.stream(stop.get_token(), [&](Frame&&) { ++published; }), std::runtime_error);
    EXPECT_EQ(published, 0);
    EXPECT_EQ(capture.stats().invalid, 1);
}

TEST(GrpcLoopback, RepeatedFrozenSourceTimeFaults) {
    ScreenshotService service(false, true);
    LoopbackServer server(service);
    HostClock clock;
    CaptureOptions options;
    options.width = 2; options.height = 2;
    options.max_relative_lag_ns = 1'000'000'000;
    GrpcCapture capture(clock, server.endpoint(), options);
    std::stop_source stop;
    int published = 0;
    EXPECT_THROW(capture.stream(stop.get_token(), [&](Frame&&) { ++published; }), std::runtime_error);
    EXPECT_GE(published, 1);
    EXPECT_LT(published, 10);
}

TEST(GrpcLoopback, SourceSequenceResetFaultsBeforeSecondPublish) {
    ScreenshotService service(false, false, true);
    LoopbackServer server(service);
    HostClock clock;
    CaptureOptions options;
    options.width = 2; options.height = 2;
    GrpcCapture capture(clock, server.endpoint(), options);
    std::stop_source stop;
    int published = 0;
    EXPECT_THROW(capture.stream(stop.get_token(), [&](Frame&&) { ++published; }), std::runtime_error);
    EXPECT_EQ(published, 1);
}
