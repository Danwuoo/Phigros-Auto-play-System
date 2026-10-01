#include "pas/session_recording.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <zlib.h>
#include <emmintrin.h>

namespace pas {
namespace {
using nlohmann::json;
class PngEncoder {
public:
    PngEncoder(int width,int height) {
        if(width<2||height<2||width>1280||height>720)throw std::invalid_argument("PNG geometry bound");
        width_=width;height_=height;
        filtered_.resize((static_cast<std::size_t>(width)*3+1)*height);
        if(deflateInit2(&stream_,1,Z_DEFLATED,15,8,Z_RLE)!=Z_OK)throw std::runtime_error("PNG zlib init");
        initialized_=true;
        try {compressed_.resize(deflateBound(&stream_,static_cast<uLong>(filtered_.size())));}
        catch(...) {deflateEnd(&stream_);throw;}
    }
    ~PngEncoder(){if(initialized_)deflateEnd(&stream_);}
    std::vector<std::uint8_t> encode(const Frame& input) {
        if(input.width!=width_||input.height!=height_||input.stride!=input.width*3||
           input.rgb.size()!=static_cast<std::size_t>(input.stride)*input.height)
            throw std::invalid_argument("recording RGB geometry");
        // PNG RGB8, non-interlaced, Sub filter: difference from the original
        // byte one RGB pixel to the left, modulo 256. No color transforms.
        for(int y=0;y<input.height;++y) {
            const auto* source=input.rgb.data()+y*input.stride;
            auto* row=filtered_.data()+static_cast<std::size_t>(y)*(input.stride+1);row[0]=1;
            std::copy_n(source,3,row+1);int x=3;
            for(;x+16<=input.stride;x+=16) {
                const auto current=_mm_loadu_si128(reinterpret_cast<const __m128i*>(source+x));
                const auto left=_mm_loadu_si128(reinterpret_cast<const __m128i*>(source+x-3));
                _mm_storeu_si128(reinterpret_cast<__m128i*>(row+x+1),_mm_sub_epi8(current,left));
            }
            for(;x<input.stride;++x)row[x+1]=static_cast<std::uint8_t>(source[x]-source[x-3]);
        }
        if(deflateReset(&stream_)!=Z_OK)throw std::runtime_error("PNG zlib reset");
        stream_.next_in=filtered_.data();stream_.avail_in=static_cast<uInt>(filtered_.size());
        stream_.next_out=compressed_.data();stream_.avail_out=static_cast<uInt>(compressed_.size());
        if(deflate(&stream_,Z_FINISH)!=Z_STREAM_END)
            throw std::runtime_error("PNG zlib compression failed");
        const auto size=stream_.total_out;
        std::vector<std::uint8_t> png{137,80,78,71,13,10,26,10};png.reserve(size+57);
        std::vector<std::uint8_t> header;be32(header,width_);be32(header,height_);
        header.insert(header.end(),{8,2,0,0,0});
        chunk(png,"IHDR",header.data(),header.size());chunk(png,"IDAT",compressed_.data(),size);
        chunk(png,"IEND",nullptr,0);
        if(png.size()>4*1024*1024)throw std::runtime_error("PNG encoded capacity");
        return png;
    }
private:
    static void be32(std::vector<std::uint8_t>& out,std::uint32_t value) {
        for(int shift:{24,16,8,0})out.push_back(static_cast<std::uint8_t>(value>>shift));
    }
    static void chunk(std::vector<std::uint8_t>& out,const char* type,const std::uint8_t* data,std::size_t size) {
        be32(out,static_cast<std::uint32_t>(size));const auto crc_start=out.size();
        out.insert(out.end(),type,type+4);if(size)out.insert(out.end(),data,data+size);
        be32(out,static_cast<std::uint32_t>(crc32(0,out.data()+crc_start,static_cast<uInt>(size+4))));
    }
    int width_=0,height_=0;std::vector<std::uint8_t> filtered_,compressed_;
    z_stream stream_{};bool initialized_=false;
};
}
std::vector<std::uint8_t> encode_recording_png(const Frame& frame) {
    PngEncoder encoder(frame.width,frame.height);return encoder.encode(frame);
}
SessionRecording::SessionRecording(std::filesystem::path root,const Clock& clock,int width,int height,
                                   FullRecordingOptions options)
    :root_(std::move(root)),clock_(clock),width_(width),height_(height),options_(options) {
    if(width<2||height<2||width>1280||height>720||options.pre_roll_frames>64||
       options.queue_capacity<1||options.queue_capacity>128||options.pre_roll_frames>options.queue_capacity||
       options.frame_limit<1||options.frame_limit>36000||options.byte_limit<1||options.byte_limit>5ULL*1024*1024*1024||
       options.duration_limit<=0||options.duration_limit>600'000'000'000LL||std::filesystem::exists(root_))
        throw std::invalid_argument("invalid/full recording root or bounds");
    // Three encoders hold at most one PNG each and commit in capture order.
    for(std::size_t i=0;i<options.pre_roll_frames+options.queue_capacity+3;++i) {
        auto sample=std::make_unique<Sample>();sample->frame.rgb.resize(static_cast<std::size_t>(width)*height*3);
        free_.push_back(sample.get());slots_.push_back(std::move(sample));
    }
    std::filesystem::create_directories(root_/"frames");
    index_.open(root_/"index.jsonl",std::ios::binary);
    if(!index_)throw std::runtime_error("full recording index open failed");
    for(int i=0;i<3;++i)workers_.emplace_back([this]{writer();});
}
SessionRecording::~SessionRecording(){try{close();}catch(...) {}}
void SessionRecording::observe(const Frame& frame) {
    if(!frame.source_valid||frame.width!=width_||frame.height!=height_||frame.stride!=width_*3||
       frame.rgb.size()!=static_cast<std::size_t>(width_)*height_*3)
        throw std::invalid_argument("full recording source geometry");
    Sample* sample=nullptr;
    {
        std::lock_guard lock(mutex_);++seen_;
        if(closed_||complete_||!error_.empty())return;
        if(round_&&accepted_-written_>=options_.queue_capacity) {
            error_="full_recording_queue_overflow_incomplete";ready_.notify_all();return;
        }
        if(!round_&&pre_.size()==options_.pre_roll_frames&& !pre_.empty()) {
            free_.push_back(pre_.front());pre_.pop_front();
        }
        if(!round_&&!options_.pre_roll_frames)return;
        if(free_.empty()){error_="full_recording_pool_exhausted_incomplete";ready_.notify_all();return;}
        sample=free_.back();free_.pop_back();
    }
    sample->copy_start=clock_.now_ns();sample->frame=frame;sample->copy_end=clock_.now_ns();sample->pre_roll=false;
    {
        std::lock_guard lock(mutex_);
        if(closed_||complete_||!error_.empty()){free_.push_back(sample);return;}
        if(!round_) {pre_.push_back(sample);return;}
        if(accepted_>=options_.frame_limit||frame.capture_complete_ns-first_ns_>options_.duration_limit||
           queue_.size()>=options_.queue_capacity) {
            error_="full_recording_frame_duration_or_queue_limit_incomplete";free_.push_back(sample);ready_.notify_all();return;
        }
        ++accepted_;copy_ms_.push_back((sample->copy_end-sample->copy_start)/1e6);
        queue_.push_back(sample);peak_queue_=std::max<std::uint64_t>(peak_queue_,accepted_-written_);
    }
    ready_.notify_all();
}
void SessionRecording::start(std::uint64_t round) {
    std::lock_guard lock(mutex_);
    if(!round||round_||complete_||closed_)throw std::runtime_error("full recording permits one round");
    round_=round;
    if(!pre_.empty())first_ns_=pre_.front()->frame.capture_complete_ns;
    else first_ns_=clock_.now_ns();
    while(!pre_.empty()) {
        auto* sample=pre_.front();pre_.pop_front();sample->pre_roll=true;
        if(accepted_>=options_.frame_limit){error_="full_recording_frame_limit_incomplete";free_.push_back(sample);continue;}
        ++accepted_;copy_ms_.push_back((sample->copy_end-sample->copy_start)/1e6);queue_.push_back(sample);
    }
    peak_queue_=std::max<std::uint64_t>(peak_queue_,accepted_-written_);ready_.notify_all();
}
void SessionRecording::finish(std::uint64_t round) {
    std::lock_guard lock(mutex_);
    if(round_==round)complete_=true;
}
void SessionRecording::writer() {
    try {
        PngEncoder encoder(width_,height_);
        while(true) {
            Sample* sample=nullptr;std::uint64_t round=0,ordinal=0;
            {
                std::unique_lock lock(mutex_);ready_.wait(lock,[&]{return closed_||!error_.empty()||!queue_.empty();});
                if(!error_.empty())break;
                if(queue_.empty()){if(closed_)break;continue;}
                sample=queue_.front();queue_.pop_front();round=round_;ordinal=next_ordinal_++;
            }
            const auto& frame=sample->frame;
            const auto begin=clock_.now_ns();auto png=encoder.encode(frame);const auto encoded=clock_.now_ns();
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock,[&]{return !error_.empty()||written_==ordinal;});
                if(!error_.empty())break;
            }
            const auto previous_frame=previous_frame_;const auto previous_source=previous_source_;
            const auto previous_time=previous_time_;
            const auto commit_start=clock_.now_ns();
            if(previous_frame&&(frame.sequence<=previous_frame||frame.capture_complete_ns<=previous_time))
                throw std::runtime_error("recording capture order invalid");
            std::ostringstream name;name<<"frames/frame-"<<std::setw(6)<<std::setfill('0')<<ordinal<<".png";
            const auto path=root_/name.str();
            const json metadata={{"schema",1},{"round_id",round},{"ordinal",ordinal},{"path",name.str()},
                {"source_frame",frame.sequence},{"capture_complete_ns",frame.capture_complete_ns},
                {"pixels_ready_ns",frame.pixels_ready_ns},{"source_sequence",frame.source_sequence?json(*frame.source_sequence):json(nullptr)},
                {"source_timestamp_us",frame.source_timestamp_us?json(*frame.source_timestamp_us):json(nullptr)},
                {"source_absolute_age",nullptr},{"clock_domain","host_qpc_ns"},
                {"source_time_domain","emulator_image_timestamp_us_unverified_absolute_age"},
                {"capture_sequence_gap",previous_frame?frame.sequence-previous_frame-1:0},
                {"source_sequence_gap",previous_source&&frame.source_sequence&&*frame.source_sequence>*previous_source?
                    json(*frame.source_sequence-*previous_source-1):json(nullptr)},
                {"dt_ns",previous_time?json(frame.capture_complete_ns-previous_time):json(nullptr)},
                {"width",width_},{"height",height_},{"source_rotation",frame.source_rotation},
                {"format","PNG_RGB24_lossless_unannotated"},{"pre_roll",sample->pre_roll},
                {"copy_start_ns",sample->copy_start},{"copy_end_ns",sample->copy_end},
                {"encode_start_ns",begin},{"encode_complete_ns",encoded},{"ordered_commit_start_ns",commit_start}};
            {
                std::lock_guard lock(mutex_);
                // Reserve index bytes including the fixed length SHA and newline before disk writes.
                if(bytes_+png.size()+metadata.dump().size()+128>options_.byte_limit)
                    throw std::runtime_error("full_recording_byte_limit_incomplete");
            }
            std::ofstream image(path,std::ios::binary);image.write(reinterpret_cast<const char*>(png.data()),png.size());
            image.close();if(!image)throw std::runtime_error("full recording PNG write failed");
            auto row=metadata;row["png_sha256"]=sha256_file(path);const auto line=row.dump()+"\n";
            index_<<line;
            if((ordinal+1)%60==0)index_.flush();
            if(!index_)throw std::runtime_error("full recording index write failed");
            const auto done=clock_.now_ns();
            {
                std::lock_guard lock(mutex_);
                previous_frame_=frame.sequence;previous_source_=frame.source_sequence;previous_time_=frame.capture_complete_ns;
                ++written_;bytes_+=png.size()+line.size();last_ns_=frame.capture_complete_ns;
                encode_ms_.push_back((encoded-begin)/1e6);write_ms_.push_back((done-commit_start)/1e6);
                commit_wait_ms_.push_back((commit_start-encoded)/1e6);
                if(!metadata.at("dt_ns").is_null())interval_ms_.push_back(metadata.at("dt_ns").get<Nanoseconds>()/1e6);
                free_.push_back(sample);
            }
            ready_.notify_all();
            if((ordinal+1)%60==0) {
                std::ofstream progress(root_/"progress.json");progress<<summary().dump(2)<<'\n';
                if(!progress)throw std::runtime_error("recording progress write failed");
            }
        }
    } catch(const std::exception& exception) {
        {std::lock_guard lock(mutex_);if(error_.empty())error_=exception.what();}
        ready_.notify_all();
    }
}
void SessionRecording::close() {
    {std::lock_guard lock(mutex_);if(closed_&&workers_.empty())return;closed_=true;}
    ready_.notify_all();for(auto& worker:workers_)if(worker.joinable())worker.join();workers_.clear();
    index_.flush();if(!index_){std::lock_guard lock(mutex_);if(error_.empty())error_="recording index flush failed";}index_.close();
    std::ofstream report(root_/"summary.json");auto value=summary();
    value["index_sha256"]=std::filesystem::exists(root_/"index.jsonl")?json(sha256_file(root_/"index.jsonl")):json(nullptr);
    report<<value.dump(2)<<'\n';
    if(!report){std::lock_guard lock(mutex_);if(error_.empty())error_="recording summary write failed";}
}
bool SessionRecording::faulted() const {std::lock_guard lock(mutex_);return !error_.empty();}
std::string SessionRecording::error() const {std::lock_guard lock(mutex_);return error_;}
nlohmann::json SessionRecording::summary() const {
    std::lock_guard lock(mutex_);
    return {{"enabled",true},{"format","PNG_RGB24_lossless_unannotated"},{"png_compression_level",1},
        {"png_filter","sub"},{"zlib_strategy","Z_RLE"},{"png_encoder","native PNG chunks + existing zlib"},{"png_encoder_threads",3},{"round_limit",1},{"round",round_},
        {"state",error_.empty()?(closed_?"closed":"recording_or_standby"):"incomplete_fault"},
        {"round_end_observed",complete_},{"error",error_.empty()?json(nullptr):json(error_)},
        {"capture_frames_seen",seen_},{"frames_accepted",accepted_},{"frames_written",written_},
        {"continuous_received_pixels_complete",closed_&&complete_&&error_.empty()&&written_==accepted_},
        {"source_frames_not_received_are_not_reconstructed",true},{"first_capture_ns",first_ns_},
        {"last_capture_ns",last_ns_},{"bytes_png_and_index",bytes_},{"max_bytes_png_and_index",options_.byte_limit},
        {"frame_limit",options_.frame_limit},{"duration_limit_ns",options_.duration_limit},
        {"pre_roll_capacity",options_.pre_roll_frames},{"queue_capacity",options_.queue_capacity},
        {"peak_queue",peak_queue_},{"physical_rgb_slots",slots_.size()},
        {"physical_rgb_bytes",static_cast<std::uint64_t>(slots_.size())*width_*height_*3},
        {"copy_ms",distribution(copy_ms_)},{"encode_ms",distribution(encode_ms_)},
        {"ordered_commit_wait_ms",distribution(commit_wait_ms_)},
        {"file_write_hash_index_ms",distribution(write_ms_)},{"capture_interval_ms",distribution(interval_ms_)},
        {"source_absolute_age",nullptr},{"runtime_feedback",false}};
}
}
