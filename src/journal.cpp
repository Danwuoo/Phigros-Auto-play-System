#include "pas/journal.hpp"

#include <fstream>
#include <stdexcept>
#include <zlib.h>

namespace pas {

Journal::Journal(const std::filesystem::path& path, std::size_t capacity,
                 std::function<void()> before_write,Encoding encoding)
    : capacity_(capacity), path_(path), before_write_(std::move(before_write)),encoding_(encoding) {
    if (capacity < 2 || capacity > 1'000'000) throw std::invalid_argument("invalid journal capacity");
    if(encoding==Encoding::gzip&&path.extension()!=".gz")throw std::invalid_argument("gzip journal path");
    std::filesystem::create_directories(path.parent_path());
    worker_ = std::jthread([this](std::stop_token stop) { writer(stop); });
}

Journal::~Journal() { close(); }

bool Journal::push(nlohmann::json record, bool critical) {
    return push(prepare(std::move(record)),critical);
}

PreparedJournalRecord Journal::prepare(nlohmann::json record) {
    record["schema_version"] = 2;
    record["clock_domain"] = "host_qpc_ns";
    return PreparedJournalRecord(record.dump());
}

bool Journal::push(PreparedJournalRecord record, bool critical) {
    return enqueue(std::move(record),critical);
}
bool Journal::push(DiagnosticSnapshot record,bool critical){return enqueue(std::move(record),critical);}
namespace {std::size_t charge(const std::variant<PreparedJournalRecord,DiagnosticSnapshot>&v){if(const auto*p=std::get_if<PreparedJournalRecord>(&v))return p->bytes()+128;return DiagnosticSnapshot::storage_charge;}}

bool Journal::enqueue(Item record, bool critical) {
    {
        std::lock_guard lock(mutex_);
        if (closed_ || faulted_) return false;
        const auto bytes=charge(record);
        if (queue_.size() >= capacity_ || bytes>16*1024*1024 || queued_bytes_>16*1024*1024-bytes) {
            if (critical) faulted_ = true;
            else ++debug_drops_;
            return false;
        }
        queued_bytes_+=bytes;
        queue_.push_back(std::move(record));
    }
    ready_.notify_one();
    return true;
}

void Journal::writer(std::stop_token stop) {
 gzFile compressed=nullptr;
 try {
    std::ofstream file;
    if(encoding_==Encoding::gzip){compressed=gzopen(path_.string().c_str(),"wb1");if(!compressed)throw std::runtime_error("gzip journal open");}
    else {file.open(path_, std::ios::binary | std::ios::trunc);if(!file)throw std::runtime_error("journal open");}
    while (true) {
        Item record=PreparedJournalRecord("");
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [&] { return stop.stop_requested() || !queue_.empty(); });
            if (queue_.empty() && stop.stop_requested()) break;
            queued_bytes_-=charge(queue_.front());record = std::move(queue_.front()); queue_.pop_front();
        }
        if (before_write_) before_write_();
        std::string row;if(auto* prepared=std::get_if<PreparedJournalRecord>(&record))row=std::move(prepared->row_);
        else {auto value=std::get<DiagnosticSnapshot>(record).encode();value["schema_version"]=2;value["clock_domain"]="host_qpc_ns";row=value.dump();}
        {std::lock_guard lock(mutex_);const auto bytes=row.size()+128;
         const auto byte_cap=encoding_==Encoding::gzip?4294967296ull:1073741824ull;
         if(bytes>262144||admitted_records_>=65536||bytes>byte_cap-admitted_bytes_)throw std::runtime_error("journal output bound");
         admitted_bytes_+=bytes;++admitted_records_;}
        if(compressed){row+='\n';const auto offset=gzoffset(compressed);const auto reserve=compressBound(static_cast<uLong>(row.size()))+65536ull;if(offset<0||static_cast<std::uint64_t>(offset)>1073741824ull-reserve)throw std::runtime_error("gzip journal physical bound");if(gzwrite(compressed,row.data(),static_cast<unsigned>(row.size()))!=static_cast<int>(row.size()))throw std::runtime_error("gzip journal write");}
        else {file << row << '\n';if(!file)throw std::runtime_error("journal write");}
    }
    if(compressed){const auto handle=compressed;compressed=nullptr;if(gzclose(handle)!=Z_OK||std::filesystem::file_size(path_)>1073741824)throw std::runtime_error("gzip journal close/bound");}
    else {file.flush();if(!file)throw std::runtime_error("journal flush");}
 }catch(...){if(compressed)gzclose(compressed);std::lock_guard lock(mutex_);faulted_=true;queue_.clear();queued_bytes_=0;}
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
std::uint64_t Journal::admitted_bytes()const{std::lock_guard lock(mutex_);return admitted_bytes_;}
std::uint64_t Journal::admitted_records()const{std::lock_guard lock(mutex_);return admitted_records_;}

} // namespace pas
