#include "pas/emulator.hpp"
#include <grpc/pas_windows_read_config.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <thread>

namespace {
namespace transport_pb = android::emulation::control;
class MixedService final : public transport_pb::EmulatorController::Service {
public:
    grpc::Status streamScreenshot(grpc::ServerContext* context, const transport_pb::ImageFormat* request,
                                  grpc::ServerWriter<transport_pb::Image>* writer) override {
        transport_pb::Image image;
        image.mutable_format()->set_width(1280);
        image.mutable_format()->set_height(720);
        image.mutable_format()->set_format(transport_pb::ImageFormat::RGB888);
        image.mutable_format()->mutable_rotation()->set_rotation(transport_pb::Rotation::LANDSCAPE);
        for (int seq = 1; seq <= 8 && !context->IsCancelled(); ++seq) {
            image.set_seq(seq);
            image.set_timestampus(seq * 1000);
            image.set_image(std::string(1280 * 720 * 3, static_cast<char>(seq)));
            if (!writer->Write(image)) break;
        }
        // The slow test RPC stays active even if transport flow control happens
        // to buffer all eight messages. Its client deadline bounds this wait.
        if (request->width() == 1)
            while (!context->IsCancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return grpc::Status::OK;
    }
    grpc::Status getScreenshot(grpc::ServerContext*, const transport_pb::ImageFormat*, transport_pb::Image* out) override {
        out->mutable_format()->set_width(2); out->mutable_format()->set_height(2);
        out->mutable_format()->set_format(transport_pb::ImageFormat::RGB888);
        out->set_image(std::string(12, '\x2a'));
        return grpc::Status::OK;
    }
};
struct Server {
    MixedService service;
    std::unique_ptr<grpc::Server> server;
    std::string address;
    Server() {
        grpc::ServerBuilder builder;
        int port = 0;
        builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
        builder.RegisterService(&service);
        server = builder.BuildAndStart();
        if (!server || !port) throw std::runtime_error("loopback server unavailable");
        address = "127.0.0.1:" + std::to_string(port);
    }
    ~Server() { server->Shutdown(); server->Wait(); }
};
}

TEST(GrpcTransport, PatchedLibraryBoundsUntrustedChannelValues) {
    EXPECT_EQ(grpc_pas_windows_read_patch_version(), GRPC_PAS_WINDOWS_READ_PATCH_VERSION);
    for (int bytes : {-1, 0, 1, 8192, 16384, 1048576, std::numeric_limits<int>::max()})
        EXPECT_EQ(grpc_pas_windows_read_chunk_bytes(bytes), 8192);
    EXPECT_EQ(grpc_pas_windows_read_chunk_bytes(65536), 65536);
    EXPECT_EQ(grpc_pas_windows_read_chunk_bytes(262144), 262144);
    EXPECT_THROW(pas::capture_channel_arguments(128), std::invalid_argument);
    EXPECT_THROW(pas::capture_channel_arguments(-1), std::invalid_argument);
}

TEST(GrpcTransport, FullSizeFramesSmallUnaryAndCancelledSlowStreamRemainIndependent) {
    Server server;
    for (int kib : {8, 64, 256}) {
        SCOPED_TRACE(kib);
        auto channel = grpc::CreateCustomChannel(server.address, grpc::InsecureChannelCredentials(),
                                                pas::capture_channel_arguments(kib));
        auto stub = transport_pb::EmulatorController::NewStub(channel);
        transport_pb::ImageFormat request;
        grpc::ClientContext slow, fast, unary;
        const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        slow.set_deadline(deadline); fast.set_deadline(deadline); unary.set_deadline(deadline);
        auto slow_request = request;
        slow_request.set_width(1);
        auto slow_reader = stub->streamScreenshot(&slow, slow_request);
        auto reader = stub->streamScreenshot(&fast, request);
        transport_pb::Image tiny;
        EXPECT_TRUE(stub->getScreenshot(&unary, request, &tiny).ok());
        EXPECT_EQ(tiny.image(), std::string(12, '\x2a'));
        transport_pb::Image image;
        int received = 0;
        while (reader->Read(&image)) {
            ++received;
            EXPECT_EQ(image.seq(), received);
            EXPECT_EQ(image.image().size(), 1280 * 720 * 3);
            EXPECT_TRUE(std::all_of(image.image().begin(), image.image().end(),
                [&](char byte) { return byte == static_cast<char>(received); }));
        }
        EXPECT_TRUE(reader->Finish().ok());
        EXPECT_EQ(received, 8);
        slow.TryCancel();
        EXPECT_EQ(slow_reader->Finish().error_code(), grpc::StatusCode::CANCELLED);

        grpc::ClientContext after_cancel;
        after_cancel.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));
        EXPECT_TRUE(stub->getScreenshot(&after_cancel, request, &tiny).ok());
        EXPECT_EQ(tiny.image(), std::string(12, '\x2a'));

        // An ordinary small-message channel continues to use upstream defaults.
        auto small_stub = transport_pb::EmulatorController::NewStub(
            grpc::CreateChannel(server.address, grpc::InsecureChannelCredentials()));
        grpc::ClientContext small;
        small.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));
        EXPECT_TRUE(small_stub->getScreenshot(&small, request, &tiny).ok());
        EXPECT_EQ(tiny.image(), std::string(12, '\x2a'));
    }
}
