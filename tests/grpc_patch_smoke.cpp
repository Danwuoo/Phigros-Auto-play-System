#include "pas/grpc_transport.hpp"
#include <grpc/pas_windows_read_config.h>
#include <iostream>
#include <stdexcept>

// This links the real patched runtime, without creating a channel or discovering
// any device. Merely producing a static backend archive does not verify symbols.
int main() {
    try {
        for (const int kib : {8, 64, 256}) {
            if (grpc_pas_windows_read_chunk_bytes(kib * 1024) != kib * 1024)
                throw std::runtime_error("patched runtime read sizing mismatch");
            const auto manifest = pas::grpc_transport_manifest(kib);
            if (manifest.at("package_version") != "1.81.1" ||
                manifest.at("windows_read_patch_version") != GRPC_PAS_WINDOWS_READ_PATCH_VERSION ||
                manifest.at("read_chunk_bytes") != kib * 1024 ||
                manifest.at("channel_argument") != GRPC_PAS_WINDOWS_READ_ARG ||
                manifest.at("client_version").get<std::string>().empty())
                throw std::runtime_error("patched runtime manifest mismatch");
            std::cout << manifest.dump() << '\n';
        }
        for (const int invalid : {0, -1, 32768, 262145})
            if (grpc_pas_windows_read_chunk_bytes(invalid) != 8192)
                throw std::runtime_error("patched runtime fallback mismatch");
        bool rejected = false;
        try { (void)pas::capture_channel_arguments(32); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("invalid read chunk accepted");
        std::cout << "Patched gRPC link/runtime contract passed; no channel or device opened.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
