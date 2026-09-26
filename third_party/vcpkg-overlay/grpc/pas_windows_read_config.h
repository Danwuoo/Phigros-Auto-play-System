// PAS extension to gRPC 1.81.1. Windows static library only.
#pragma once
#define GRPC_PAS_WINDOWS_READ_ARG "pas.grpc.windows_read_chunk_bytes"
#define GRPC_PAS_WINDOWS_READ_PATCH_VERSION 2
extern "C" int grpc_pas_windows_read_patch_version();
// Returns the supported size, or 8192 for absent/invalid values.
extern "C" int grpc_pas_windows_read_chunk_bytes(int requested);
