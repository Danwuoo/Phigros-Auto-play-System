#include "pas/session_pixel_clips.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace pas {
using nlohmann::json;

SessionPixelClips::SessionPixelClips(std::filesystem::path root,const Clock& clock,int width,int height,PixelClipOptions options)
    :root_(std::move(root)),clock_(clock),width_(width),height_(height),options_(options) {
    if(width<=0||height<=0||options.max_rounds<1||options.max_rounds>20||
       options.uniform_clips<0||options.uniform_clips>8||options.event_clips<0||options.event_clips>2||
       options.frames_per_clip<1||options.frames_per_clip>3||options.queue_capacity<1||options.queue_capacity>4||
       options.uniform_interval_ns<1'000'000'000LL||options.event_cooldown_ns<0)
        throw std::invalid_argument("invalid bounded pixel clip options");
    std::filesystem::create_directories(root_);
    worker_=std::jthread([this]{writer();});
}
SessionPixelClips::~SessionPixelClips(){close();}
void SessionPixelClips::finish_clip(){trigger_.clear();clip_frame_=0;}
void SessionPixelClips::observe(const Frame& f,const SessionObservation& p) {
    if(closed_||p.status.round>static_cast<std::uint64_t>(options_.max_rounds))return;
    if(p.status.new_round) {
        round_=p.status.round;clip_=0;uniform_=event_=clip_frame_=enqueued_round_=0;
        round_start_=last_event_ns_=0;trigger_.clear();previous_complex_=false;
    }
    if(!round_||p.status.round!=round_||!p.status.active||p.status.state!=PlaySessionState::playing||
       !f.source_valid||f.width!=width_||f.height!=height_||f.stride!=width_*3||
       f.rgb.size()!=static_cast<std::size_t>(width_)*height_*3)return;
    const auto now=f.capture_complete_ns;
    if(!round_start_)round_start_=now;
    const bool complex=p.scene.lines.size()>1||std::any_of(p.scene.lines.begin(),p.scene.lines.end(),
        [](const LineCandidate& line){return line.motion_valid&&std::abs(line.angular_velocity)>.15;});
    const bool became_complex=complex&&!previous_complex_;
    previous_complex_=complex;
    if(trigger_.empty()) {
        if(uniform_<options_.uniform_clips&&now-round_start_>=options_.uniform_interval_ns*uniform_) {
            trigger_="uniform";++uniform_;++clip_;
        } else if(became_complex&&event_<options_.event_clips&&
                  (!last_event_ns_||now-last_event_ns_>=options_.event_cooldown_ns)) {
            trigger_="complex_line_event";++event_;++clip_;last_event_ns_=now;
        } else return;
    }
    const auto index=clip_frame_++;
    const auto clip_trigger=trigger_;
    if(clip_frame_>=options_.frames_per_clip)finish_clip();
    const auto per_round=(options_.uniform_clips+options_.event_clips)*options_.frames_per_clip;
    if(enqueued_round_>=per_round)return;
    {
        std::lock_guard lock(mutex_);
        if(!error_.empty()||queue_.size()>=options_.queue_capacity){++dropped_;return;}
    }
    Sample sample;
    sample.round=round_;sample.clip=clip_;sample.index=index;
    sample.trigger=clip_trigger;
    // The trigger is fixed for a clip, including its last frame.
    sample.source_frame=f.sequence;sample.capture_ns=now;
    sample.recognition_start_ns=p.scene.recognition_start_ns;
    sample.recognition_end_ns=p.scene.recognition_end_ns;
    sample.source_timestamp_us=f.source_timestamp_us;sample.source_sequence=f.source_sequence;
    sample.line_count=static_cast<int>(p.scene.lines.size());sample.target_count=static_cast<int>(p.scene.targets.size());
    sample.copy_start_ns=clock_.now_ns();sample.rgb=f.rgb;sample.copy_end_ns=clock_.now_ns();
    const auto copy_ms=(sample.copy_end_ns-sample.copy_start_ns)/1e6;
    {
        std::lock_guard lock(mutex_);
        if(!error_.empty()){++dropped_;return;}
        queue_.push_back(std::move(sample));
        peak_queue_=std::max(peak_queue_,queue_.size());++accepted_;++enqueued_round_;
        ++copy_samples_;copy_sum_ms_+=copy_ms;copy_max_ms_=std::max(copy_max_ms_,copy_ms);
        copy_latencies_ms_.push_back(copy_ms);
    }
    ready_.notify_one();
}
void SessionPixelClips::writer() {
    try {
        std::ofstream index(root_/"index.jsonl",std::ios::app);
        if(!index)throw std::runtime_error("pixel clip index open failed");
        while(true) {
            Sample sample;
            {
                std::unique_lock lock(mutex_);ready_.wait(lock,[&]{return closed_||!queue_.empty();});
                if(queue_.empty()&&closed_)break;
                sample=std::move(queue_.front());queue_.pop_front();
            }
            const auto folder=root_/("round-"+std::to_string(sample.round))/
                ("clip-"+std::to_string(sample.clip)+"-"+sample.trigger);
            std::filesystem::create_directories(folder);
            const auto path=folder/("frame-"+std::to_string(sample.index)+".rgb");
            std::ofstream pixels(path,std::ios::binary|std::ios::trunc);
            pixels.write(reinterpret_cast<const char*>(sample.rgb.data()),static_cast<std::streamsize>(sample.rgb.size()));
            pixels.close();if(!pixels)throw std::runtime_error("pixel clip frame write failed");
            index<<json{{"round_id",sample.round},{"clip_id",sample.clip},{"clip_trigger",sample.trigger},
                {"frame_in_clip",sample.index},{"path",std::filesystem::relative(path,root_).generic_string()},
                {"sha256",sha256_file(path)},{"width",width_},{"height",height_},{"stride",width_*3},
                {"layout","top_down_rgb888"},{"source_frame",sample.source_frame},
                {"capture_complete_ns",sample.capture_ns},{"recognition_start_ns",sample.recognition_start_ns},
                {"recognition_end_ns",sample.recognition_end_ns},{"sampling_copy_start_ns",sample.copy_start_ns},
                {"sampling_copy_end_ns",sample.copy_end_ns},{"source_timestamp_us",sample.source_timestamp_us},
                {"source_sequence",sample.source_sequence},{"detected_lines",sample.line_count},
                {"detected_targets",sample.target_count}}.dump()<<'\n';
            index.flush();if(!index)throw std::runtime_error("pixel clip index write failed");
            {std::lock_guard lock(mutex_);++written_;}
        }
    }catch(const std::exception& e){std::lock_guard lock(mutex_);error_=e.what();queue_.clear();}
}
void SessionPixelClips::close(){
    {std::lock_guard lock(mutex_);closed_=true;}
    ready_.notify_one();if(worker_.joinable())worker_.join();
}
json SessionPixelClips::summary() const {
    std::lock_guard lock(mutex_);
    return {{"enabled",true},{"status",error_.empty()?"closed":"write_error"},{"error",error_.empty()?json(nullptr):json(error_)},
        {"round_limit",options_.max_rounds},{"uniform_clips_per_round",options_.uniform_clips},
        {"event_clips_per_round",options_.event_clips},{"frames_per_clip",options_.frames_per_clip},
        {"uniform_interval_ns",options_.uniform_interval_ns},{"event_cooldown_ns",options_.event_cooldown_ns},
        {"queue_capacity",options_.queue_capacity},{"peak_queue",peak_queue_},{"frames_accepted",accepted_},
        {"frames_written",written_},{"frames_dropped_queue_or_error",dropped_},
        {"copy_samples",copy_samples_},{"copy_mean_ms",copy_samples_?copy_sum_ms_/copy_samples_:0},
        {"copy_max_ms",copy_max_ms_},{"copy_distribution_ms",distribution(copy_latencies_ms_)},
        {"max_raw_frame_bytes",static_cast<std::uint64_t>(width_)*height_*3},
        {"max_raw_total_bytes",static_cast<std::uint64_t>(width_)*height_*3*options_.max_rounds*
            (options_.uniform_clips+options_.event_clips)*options_.frames_per_clip},
        {"format","full_frame_top_down_rgb888_unannotated"}};
}
}
