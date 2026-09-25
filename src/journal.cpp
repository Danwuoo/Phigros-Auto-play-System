#include "pas/journal.hpp"

#include <fstream>
#include <stdexcept>

namespace pas {

Journal::Journal(const std::filesystem::path& path, std::size_t capacity)
    : capacity_(capacity), path_(path) {
    if (capacity < 2 || capacity > 1'000'000) throw std::invalid_argument("invalid journal capacity");
    std::filesystem::create_directories(path.parent_path());
    worker_ = std::jthread([this](std::stop_token stop) { writer(stop); });
}

Journal::~Journal() { close(); }

bool Journal::push(nlohmann::json record, bool critical) {
    record["schema_version"] = 2;
    record["clock_domain"] = "host_qpc_ns";
    {
        std::lock_guard lock(mutex_);
        if (closed_ || faulted_) return false;
        if (queue_.size() >= capacity_) {
            if (critical) faulted_ = true;
            else ++debug_drops_;
            return false;
        }
        queue_.push_back(std::move(record));
    }
    ready_.notify_one();
    return true;
}

void Journal::writer(std::stop_token stop) {
    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file) { std::lock_guard lock(mutex_); faulted_ = true; return; }
    while (true) {
        nlohmann::json record;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [&] { return stop.stop_requested() || !queue_.empty(); });
            if (queue_.empty() && stop.stop_requested()) break;
            record = std::move(queue_.front()); queue_.pop_front();
        }
        file << record.dump() << '\n';
        if (!file) { std::lock_guard lock(mutex_); faulted_ = true; return; }
    }
    file.flush();
    if (!file) { std::lock_guard lock(mutex_); faulted_ = true; }
}

void Journal::close() {
    {
        std::lock_guard lock(mutex_);
        if (closed_) return;
        closed_ = true;
    }
    worker_.request_stop();
    ready_.notify_all();
    worker_.join();
}

std::uint64_t Journal::debug_drops() const {
    std::lock_guard lock(mutex_);
    return debug_drops_;
}

bool Journal::faulted() const {
    std::lock_guard lock(mutex_);
    return faulted_;
}

} // namespace pas
