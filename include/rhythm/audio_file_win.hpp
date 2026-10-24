#pragma once
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rhythm {
struct LoadedMonoWav {
    std::vector<float> mono;
    unsigned sample_rate=0,channels=0,bits=0;
    std::string sha256; // Actual complete RIFF file bytes, not PCM-only identity.
    std::size_t frames=0;
};
// Windows-only, offline read. Strict RIFF/WAVE PCM16 or IEEE float32;
// <=512MiB file, <=600 seconds, 1..8 channels, 8k..192kHz.
// Float samples must be finite. No normalization/resampling: arithmetic channel
// mean matches the capture decoder. No endpoints or device access.
LoadedMonoWav load_audio_wav(const std::filesystem::path& path,
    std::optional<std::string> expected_sha256=std::nullopt);
} // namespace rhythm
