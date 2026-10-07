#pragma once

#include <nlohmann/json.hpp>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>
#include <string>
#include <utility>
#include <variant>

namespace pas {

// Constructed only by Journal::prepare, with the same clock/schema fields as
// the ordinary JSON overload. Callers cannot supply an arbitrary raw line.
class PreparedJournalRecord final {
public:
    std::size_t bytes() const noexcept { return row_.size(); }
private:
    friend class Journal;
    explicit PreparedJournalRecord(std::string row) : row_(std::move(row)) {}
    std::string row_;
};

class Journal final {
public:
    explicit Journal(const std::filesystem::path& path, std::size_t capacity = 8192,
                     std::function<void()> before_write = {});
    ~Journal();
    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;
    bool push(nlohmann::json record, bool critical = true);
    static PreparedJournalRecord prepare(nlohmann::json record);
    bool push(PreparedJournalRecord record, bool critical = true);
    void close();
    std::uint64_t debug_drops() const;
    bool faulted() const;
private:
    using Item = std::variant<nlohmann::json,PreparedJournalRecord>;
    bool enqueue(Item record, bool critical);
    void writer(std::stop_token stop);
    const std::size_t capacity_;
    const std::filesystem::path path_;
    const std::function<void()> before_write_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Item> queue_;
    std::jthread worker_;
    std::uint64_t debug_drops_ = 0;
    bool faulted_ = false;
    bool closed_ = false;
};

} // namespace pas
