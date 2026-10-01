#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>

namespace pas {
// Offline CPU-only experiment. These APIs never construct capture/input,
// consume song/time metadata as model features, or feed proposals to owner.
nlohmann::json train_synthetic_cpu_vision(const std::filesystem::path&, int steps);
nlohmann::json train_reviewed_cpu_vision(const std::filesystem::path& packet,
                                        const std::filesystem::path& output, int steps);
nlohmann::json prepare_cpu_vision_packet(const std::filesystem::path& recording,
    const std::filesystem::path& focus, const std::filesystem::path& output);
nlohmann::json audit_cpu_vision_packet(const std::filesystem::path& packet);
nlohmann::json rasterize_cpu_vision_packet(const std::filesystem::path& packet,
    const std::filesystem::path& new_output);
nlohmann::json predict_cpu_vision(const std::filesystem::path& checkpoint_folder,
    const std::filesystem::path& packet, const std::filesystem::path& output);
nlohmann::json cpu_vision_self_test(const std::filesystem::path& output);
} // namespace pas
