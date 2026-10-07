#include "pas/session_archive.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <fstream>
#include <stdexcept>
#include <zlib.h>

namespace pas {
using nlohmann::json;
namespace {
void save(const std::filesystem::path& path,const json& value) {
    std::ofstream f(path);f<<value.dump(2)<<'\n';
    if(!f)throw std::runtime_error("session archive write failed");
}
void flush(std::ofstream& f) {f.flush();if(!f)throw std::runtime_error("session archive flush failed");f.close();}
}
SessionArchive::SessionArchive(std::filesystem::path root,json provenance,std::size_t segments,std::size_t standby,Encoding encoding)
    :root_(std::move(root)),provenance_(std::move(provenance)),segment_bytes_(segments),standby_bytes_(standby),encoding_(encoding) {
    if(segments<128||standby<128||segments>16*1024*1024||standby>1024*1024)throw std::invalid_argument("archive segment bound");
    std::filesystem::create_directories(root_);save(root_/"manifest.json",provenance_);
    worker_=std::jthread([this]{writer();});
}
SessionArchive::~SessionArchive() {close();}
void SessionArchive::push(Item item) {
    // Serialize once before the mailbox lock. Complete summaries retain JSON
    // for writer-owned manifest/result additions; ordinary events retain only
    // their immutable serialized row. The byte admission rule is unchanged.
    if(item.snapshot)item.charged_bytes=DiagnosticSnapshot::storage_charge;
    else {auto row=item.value.dump();item.charged_bytes=row.size();if(!item.complete){item.row=std::move(row);item.value=nullptr;}}
    {std::lock_guard lock(mutex_);
        if(faulted_)throw std::runtime_error(error_);if(closed_)throw std::runtime_error("session archive closed");
        const auto bytes=item.charged_bytes;
        if(queue_.size()>=8192||queued_bytes_+bytes>16*1024*1024) {faulted_=true;error_="session archive mailbox overrun";throw std::runtime_error(error_);}
        queued_bytes_+=bytes;
        queue_.push_back(std::move(item));peak_=std::max(peak_,queue_.size());}
    ready_.notify_one();
}
void SessionArchive::event(std::uint64_t round,DiagnosticSnapshot value){Item item{round,nullptr};item.snapshot.emplace(std::move(value));push(std::move(item));}
void SessionArchive::event(std::uint64_t round,json value) {
    value["schema_version"]=2;value["clock_domain"]="host_qpc_ns";value["round_id"]=round;
    push({round,std::move(value)});
}
void SessionArchive::complete(std::uint64_t round,json summary,std::shared_ptr<const Frame> result) {
    if(!round)throw std::invalid_argument("zero round completion");
    if(result&&image_pending_.exchange(true))throw std::runtime_error("previous result image still pending archive");
    push({round,std::move(summary),true,std::move(result)});
}
std::string SessionArchive::error() const {std::lock_guard lock(mutex_);return error_;}
std::size_t SessionArchive::peak_queue() const {std::lock_guard lock(mutex_);return peak_;}
void SessionArchive::close() {
    {std::lock_guard lock(mutex_);closed_=true;}
    ready_.notify_one();if(worker_.joinable())worker_.join();
}
void SessionArchive::writer() {
    gzFile compressed=nullptr;
    try {
        std::ofstream standby,events;
        std::size_t standby_size=0,event_size=0,standby_segment=0,segment=0;
        std::uint64_t active=0,last_completed=0,decoded_bytes=0,physical_bytes=0;
        json hashes=json::array();std::filesystem::path directory,event_path;
        const auto close_segment=[&] {
            if(events.is_open()||compressed) {
                if(compressed){auto handle=compressed;compressed=nullptr;if(gzclose(handle)!=Z_OK)throw std::runtime_error("gzip archive close failed");}else flush(events);
                physical_bytes+=std::filesystem::file_size(event_path);if(physical_bytes>512ull*1024*1024)throw std::runtime_error("round journal physical byte limit reached");
                hashes.push_back({{"path",event_path.filename().string()},{"sha256",sha256_file(event_path)}});
            }
        };
        while(true) {
            Item item;
            {std::unique_lock lock(mutex_);ready_.wait(lock,[&]{return closed_||!queue_.empty();});
                if(queue_.empty()&&closed_)break;
                item=std::move(queue_.front());queue_.pop_front();queued_bytes_-=item.charged_bytes;}
            if(item.snapshot){auto value=item.snapshot->encode();value["schema_version"]=2;value["clock_domain"]="host_qpc_ns";value["round_id"]=item.round;item.row=value.dump();if(item.row.size()>1024*1024)throw std::runtime_error("diagnostic archive encoded row bound");}
            if(!item.round) {
                const auto row=std::move(item.row)+"\n";
                if(row.size()>standby_bytes_)throw std::runtime_error("standby record exceeds segment bound");
                if(!standby.is_open()||standby_size+row.size()>standby_bytes_) {
                    if(standby.is_open()){flush(standby);standby_segment=(standby_segment+1)%4;}
                    standby.open(root_/("standby-"+std::to_string(standby_segment)+".jsonl"),std::ios::trunc);
                    standby_size=0;
                }
                standby<<row;standby_size+=row.size();
                if(!standby)throw std::runtime_error("standby journal failed");
                continue;
            }
            if(item.round!=active) {
                if(active||item.round<=last_completed)throw std::runtime_error("archive round ordering violation");
                active=item.round;directory=root_/("round-"+std::to_string(active));
                std::filesystem::create_directory(directory);
                auto manifest=provenance_;manifest["round_id"]=active;save(directory/"manifest.json",manifest);
                segment=event_size=0;decoded_bytes=physical_bytes=0;hashes=json::array();
            }
            if(item.complete) {
                close_segment();auto summary=std::move(item.value);
                summary["round_id"]=active;summary["event_segments"]=hashes;
                summary["event_encoding"]=encoding_==Encoding::gzip?"gzip-jsonl-v1":"plain-jsonl-v1";summary["event_decoded_bytes"]=decoded_bytes;summary["event_physical_bytes"]=physical_bytes;
                summary["manifest_sha256"]=sha256_file(directory/"manifest.json");
                summary["result_image_sha256"]=nullptr;
                if(item.image) {
                    write_diagnostic_png(directory/"result.png",*item.image);
                    summary["result_image_sha256"]=sha256_file(directory/"result.png");
                    image_pending_=false;
                }
                summary["result_numbers"]="unknown_until_result_pixels_reviewed";
                save(directory/"summary.json",summary);
                std::ofstream index(root_/"rounds.jsonl",std::ios::app);
                index<<json{{"round_id",active},{"path",directory.filename().string()},
                    {"summary_sha256",sha256_file(directory/"summary.json")},{"status",summary.at("status")}}.dump()<<'\n';
                if(!index)throw std::runtime_error("round index write failed");
                last_completed=active;active=0;hashes=json::array();continue;
            }
            auto row=std::move(item.row)+"\n";
#ifdef _WIN32
            if(encoding_==Encoding::gzip){row.pop_back();row+="\r\n";}
#endif
            if(row.size()>segment_bytes_)throw std::runtime_error("round record exceeds segment bound");
            if((!events.is_open()&&!compressed)||event_size+row.size()>segment_bytes_) {
                if(events.is_open()||compressed){close_segment();++segment;}
                const auto segment_cap=encoding_==Encoding::gzip?256u:32u;
                if(segment>=segment_cap)throw std::runtime_error("round journal "+std::to_string(segment_cap)+"-segment limit reached");
                if(encoding_==Encoding::gzip&&physical_bytes>512ull*1024*1024-segment_bytes_-65536)throw std::runtime_error("round journal physical reservation limit reached");
                event_path=directory/("events-"+std::to_string(segment)+(encoding_==Encoding::gzip?".jsonl.gz":".jsonl"));
                if(encoding_==Encoding::gzip){compressed=gzopen(event_path.string().c_str(),"wb1");if(!compressed)throw std::runtime_error("gzip archive open failed");}else events.open(event_path,std::ios::trunc);event_size=0;
            }
            std::size_t decoded_row_bytes=row.size();
#ifdef _WIN32
            if(encoding_==Encoding::plain)++decoded_row_bytes;
#endif
            if(decoded_row_bytes>4ull*1024*1024*1024-decoded_bytes)throw std::runtime_error("round journal decoded byte limit reached");decoded_bytes+=decoded_row_bytes;
            if(compressed){if(gzwrite(compressed,row.data(),static_cast<unsigned>(row.size()))!=static_cast<int>(row.size()))throw std::runtime_error("gzip round events write failed");}else {events<<row;if(!events)throw std::runtime_error("round events write failed");}event_size+=row.size();
        }
        close_segment();
        if(active)save(directory/"summary.json",{{"round_id",active},{"status","aborted"},
            {"stop_reason","archive_closed_before_completion"},{"event_segments",hashes},{"partial",true},
            {"event_encoding",encoding_==Encoding::gzip?"gzip-jsonl-v1":"plain-jsonl-v1"},{"event_decoded_bytes",decoded_bytes},{"event_physical_bytes",physical_bytes},
            {"manifest_sha256",sha256_file(directory/"manifest.json")},{"result_image_sha256",nullptr}});
        if(standby.is_open())flush(standby);
    } catch(const std::exception& e) {
        if(compressed){gzclose(compressed);compressed=nullptr;}
        {std::lock_guard lock(mutex_);if(error_.empty())error_=std::string(e.what()).substr(0,512);faulted_=true;queue_.clear();queued_bytes_=0;}
        try{save(root_/"archive-failure.json",{{"schema","pas.session-archive-first-failure.v1"},{"first_error",error()},{"clock_domain","host_qpc_ns"},{"qpc_ns",HostClock{}.now_ns()}});}catch(...){}
    }
}
}
