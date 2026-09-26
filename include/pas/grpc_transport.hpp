#pragma once
#include <grpcpp/support/channel_arguments.h>
#include <nlohmann/json.hpp>

namespace pas {
inline constexpr int default_grpc_read_chunk_kib = 256;
// The dependency extension preserves 8 KiB for channels without this argument.
grpc::ChannelArguments capture_channel_arguments(int read_chunk_kib);
nlohmann::json grpc_transport_manifest(int read_chunk_kib);
}
