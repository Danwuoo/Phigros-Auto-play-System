#pragma once
#include "pas/core.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <deque>
#include <thread>

namespace pas {
struct ArchiveMeterOptions {const Clock* clock=nullptr;int writer_delay_ms=0;std::size_t mailbox_items=8192,mailbox_bytes=16*1024*1024;};
struct ArchiveMeterStats {
    std::uint64_t admitted=0,written=0,discarded_on_fault=0,serialized_bytes=0;
    std::size_t queue_byte_peak=0,row_byte_peak=0;
    std::vector<double> serialize_ms,write_ms;
};
// One writer, bounded mailbox, one active round stream. No historical frame bank.
class SessionArchive final {
public:
    SessionArchive(std::filesystem::path root,nlohmann::json provenance,
                   std::size_t segment_bytes=16*1024*1024,std::size_t standby_bytes=1024*1024,ArchiveMeterOptions meter={});
    ~SessionArchive();
    void event(std::uint64_t round,nlohmann::json value);
    void complete(std::uint64_t round,nlohmann::json summary,std::shared_ptr<const Frame> result={});
    void close();
    bool faulted() const {return faulted_;}
    std::string error() const;
    std::size_t peak_queue() const;
    ArchiveMeterStats meter_stats() const;
private:
    struct Item {std::uint64_t round; nlohmann::json value; bool complete=false;std::shared_ptr<const Frame> image;};
    void push(Item);
    void writer();
    std::filesystem::path root_;nlohmann::json provenance_;
    std::size_t segment_bytes_,standby_bytes_,peak_=0,queued_bytes_=0;
    mutable std::mutex mutex_;std::condition_variable ready_;
    std::deque<Item> queue_;std::jthread worker_;
    std::atomic<bool> faulted_=false,image_pending_=false;bool closed_=false;
    std::string error_;
    ArchiveMeterOptions meter_;ArchiveMeterStats stats_;
};
}
