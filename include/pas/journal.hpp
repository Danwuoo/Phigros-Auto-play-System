#pragma once

#include <nlohmann/json.hpp>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>

namespace pas {

class Journal final {
public:
    explicit Journal(const std::filesystem::path& path, std::size_t capacity = 8192);
    ~Journal();
    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;
    bool push(nlohmann::json record, bool critical = true);
    void close();
    std::uint64_t debug_drops() const;
    bool faulted() const;
private:
    void writer(std::stop_token stop);
    const std::size_t capacity_;
    const std::filesystem::path path_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<nlohmann::json> queue_;
    std::jthread worker_;
    std::uint64_t debug_drops_ = 0;
    bool faulted_ = false;
    bool closed_ = false;
};

} // namespace pas
