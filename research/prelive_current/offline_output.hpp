#pragma once
#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
namespace pas::current_rails {
// Refuse interrupted-run sidecars instead of truncating prior failed evidence.
inline void require_fresh_offline_report(const std::filesystem::path& report) {
    constexpr std::array<const char*,5> suffixes{
        "", ".journal.jsonl", ".attempts.json", ".rows.jsonl", ".events.jsonl"};
    for(const auto* suffix:suffixes) {
        auto path=report;path+=suffix;
        if(std::filesystem::exists(path)||std::filesystem::is_symlink(std::filesystem::symlink_status(path)))
            throw std::runtime_error("fresh report and all sidecars required");
    }
}
inline void reserve_fresh_offline_report(const std::filesystem::path& report) {
    require_fresh_offline_report(report);
    auto reservation=report;reservation+=".reservation";
    // Atomic directory creation arbitrates concurrent launches. Keep it after
    // success/failure, just like the attempt's raw evidence; never reopen it.
    if(!std::filesystem::create_directory(reservation))throw std::runtime_error("offline report already reserved");
    require_fresh_offline_report(report);
}
}
