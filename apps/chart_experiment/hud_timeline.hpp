#pragma once
#include "rhythm/screen_state.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace rhythm::experimental {
struct HudTimelineMeta {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    std::int64_t received_ns=-1,processed_ns=-1;
    std::optional<std::int64_t> source_timestamp_us;
    ObservedGameState raw_class=ObservedGameState::unknown;
    bool effective_HUD=false,source_valid=false;
};
struct HudTimelineSample {
    HudTimelineMeta frame;
    std::size_t byte_offset=0;
};
enum class HudTimelineOutcome {
    sampled,not_armed,invalid_source,invalid_geometry,invalid_metadata,
    non_increasing_sequence,non_increasing_received,interval_not_elapsed,capacity_exhausted
};
struct HudTimelineStats {
    std::size_t observe_calls=0,samples=0,not_armed=0,invalid_source=0,invalid_geometry=0,invalid_metadata=0;
    std::size_t sequence_rejects=0,timestamp_rejects=0,interval_skips=0,capacity_drops=0;
    std::int64_t first_capacity_drop_received_ns=-1,last_capacity_drop_received_ns=-1,max_sample_gap_ns=0;
};

// Diagnostic pixel timeline only: no OCR, game judgment, timing qualification,
// permission, input, hash or I/O. The owner constructs this before READY and
// associates source identity/clock semantics with the eventual saved artifact.
// All pixels/index slots are preallocated. Observe is bounded and allocation-
// free; a full buffer reports sample loss without changing input/runtime state.
class HudTimeline {
public:
    static constexpr PixelRegion score_region{1076,20,180,40};
    static constexpr PixelRegion combo_region{560,8,160,72};
    static constexpr std::size_t score_bytes=180u*40u,combo_bytes=160u*72u;
    static constexpr std::size_t sample_bytes=score_bytes+combo_bytes,maximum_samples=3601;
    static constexpr std::int64_t sample_interval_ns=50'000'000;
private:
    const std::size_t capacity_;
    const bool retain_pixels_;
    std::vector<std::uint8_t> pixels_;
    std::vector<HudTimelineSample> index_;
    HudTimelineStats stats_;
    std::uint64_t last_seen_sequence_=0;
    std::int64_t last_seen_received_=-1,last_cadence_received_=-1,last_saved_received_=-1;
    static std::size_t checked_capacity(std::size_t n) {
        if(!n||n>maximum_samples)throw std::invalid_argument("HUD timeline capacity must be 1..3601");
        return n;
    }
    static bool image_valid(const RgbImageView& image) noexcept {
        return image.width==1280&&image.height==720&&image.stride>=3840&&image.stride<=4096&&
            image.pixels.size()==static_cast<std::size_t>(image.stride)*720u;
    }
    static bool meta_valid(const HudTimelineMeta& meta) noexcept {
        return meta.sequence&&meta.epoch&&meta.generation&&meta.geometry_version&&meta.received_ns>=0&&
            meta.processed_ns>=meta.received_ns&&(!meta.source_timestamp_us||*meta.source_timestamp_us>=0);
    }
    static void copy_gray(const RgbImageView& image,PixelRegion region,std::uint8_t* destination) noexcept {
        for(int y=0;y<region.height;++y) {
            const auto* source=image.pixels.data()+static_cast<std::size_t>(region.y+y)*static_cast<std::size_t>(image.stride)+
                static_cast<std::size_t>(region.x)*3u;
            for(int x=0;x<region.width;++x,source+=3)
                *destination++=static_cast<std::uint8_t>((77u*source[0]+150u*source[1]+29u*source[2]+128u)>>8);
        }
    }
public:
    explicit HudTimeline(std::size_t capacity=maximum_samples,bool retain_pixels=true)
        :capacity_(checked_capacity(capacity)),retain_pixels_(retain_pixels),
         pixels_(retain_pixels_?capacity_*sample_bytes:0),index_(capacity_) {}
    HudTimelineOutcome observe(const RgbImageView& image,const HudTimelineMeta& meta,bool caller_armed) noexcept {
        ++stats_.observe_calls;
        if(!caller_armed) {++stats_.not_armed;return HudTimelineOutcome::not_armed;}
        if(!meta.source_valid) {++stats_.invalid_source;return HudTimelineOutcome::invalid_source;}
        if(!image_valid(image)) {++stats_.invalid_geometry;return HudTimelineOutcome::invalid_geometry;}
        if(!meta_valid(meta)) {++stats_.invalid_metadata;return HudTimelineOutcome::invalid_metadata;}
        if(meta.sequence<=last_seen_sequence_) {++stats_.sequence_rejects;return HudTimelineOutcome::non_increasing_sequence;}
        if(meta.received_ns<=last_seen_received_) {++stats_.timestamp_rejects;return HudTimelineOutcome::non_increasing_received;}
        last_seen_sequence_=meta.sequence;last_seen_received_=meta.received_ns;
        if(last_cadence_received_>=0&&meta.received_ns-last_cadence_received_<sample_interval_ns) {
            ++stats_.interval_skips;return HudTimelineOutcome::interval_not_elapsed;
        }
        // Cadence records an eligible capture attempt, distinct from the last
        // saved receipt. When full, count loss once per eligible 50ms interval;
        // never claim that a dropped attempt was stored or observed as HUD.
        last_cadence_received_=meta.received_ns;
        if(stats_.samples==capacity_) {
            ++stats_.capacity_drops;
            if(stats_.first_capacity_drop_received_ns<0)stats_.first_capacity_drop_received_ns=meta.received_ns;
            stats_.last_capacity_drop_received_ns=meta.received_ns;
            return HudTimelineOutcome::capacity_exhausted;
        }
        const auto offset=stats_.samples*sample_bytes;
        if(retain_pixels_) {
            copy_gray(image,score_region,pixels_.data()+offset);
            copy_gray(image,combo_region,pixels_.data()+offset+score_bytes);
        }
        index_[stats_.samples]={meta,retain_pixels_?offset:0};
        if(last_saved_received_>=0)stats_.max_sample_gap_ns=std::max(stats_.max_sample_gap_ns,meta.received_ns-last_saved_received_);
        last_saved_received_=meta.received_ns;++stats_.samples;
        return HudTimelineOutcome::sampled;
    }
    std::span<const HudTimelineSample> samples() const noexcept {return {index_.data(),stats_.samples};}
    std::span<const std::uint8_t> bytes() const noexcept {return {pixels_.data(),retain_pixels_?stats_.samples*sample_bytes:0};}
    bool pixels_retained() const noexcept {return retain_pixels_;}
    const HudTimelineStats& stats() const noexcept {return stats_;}
    std::size_t capacity() const noexcept {return capacity_;}
    std::size_t allocated_pixel_bytes() const noexcept {return pixels_.size();}
};
} // namespace rhythm::experimental
