#include "pas/grpc_transport.hpp"
#include <grpc/grpc.h>
#include <grpc/pas_windows_read_config.h>
#include <stdexcept>

namespace pas {
grpc::ChannelArguments capture_channel_arguments(int read_chunk_kib) {
    if (read_chunk_kib != 8 && read_chunk_kib != 64 && read_chunk_kib != 256)
        throw std::invalid_argument("gRPC read chunk must be 8, 64 or 256 KiB");
    // Link-time and runtime verification: a stock gRPC cannot silently ignore
    // our private channel argument while reporting the optimization as enabled.
    if (grpc_pas_windows_read_patch_version() != GRPC_PAS_WINDOWS_READ_PATCH_VERSION)
        throw std::runtime_error("gRPC Windows read patch version mismatch");
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(80 * 1024 * 1024);
    args.SetInt(GRPC_PAS_WINDOWS_READ_ARG, read_chunk_kib * 1024);
    return args;
}
nlohmann::json grpc_transport_manifest(int read_chunk_kib) {
    (void)capture_channel_arguments(read_chunk_kib);
    return {{"package_version", "1.81.1"},
            {"client_version", grpc_version_string()},
            {"windows_read_patch_version", grpc_pas_windows_read_patch_version()},
            {"read_chunk_bytes", read_chunk_kib * 1024},
            {"channel_argument", GRPC_PAS_WINDOWS_READ_ARG},
            {"bdp_probe", "upstream_default"},
            {"http2_lookahead", "upstream_default"}};
}
}
