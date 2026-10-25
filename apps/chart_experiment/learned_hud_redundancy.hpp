#pragma once
#include "hud_glyph_evidence.hpp"
#include "rhythm/chart.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <span>
#include <vector>

namespace rhythm::experimental {
// Caller-supplied capture provenance. processed_ns is the caller's current
// clock reading, never a new timestamp assigned to the captured pixels.
struct LearnedHudFrame {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    std::string source_id;
    std::int64_t received_ns=-1,processed_ns=-1;
    bool source_valid=false;
    int rotation=0;
};
struct LearnedHudEvidence {
    bool ready=false,current_candidate=false;
    std::string reason="waiting_for_clean_reference",policy_id="learned-title-redundancy-v1";
    std::size_t construction_attempts=0,support_frames=0;
    LearnedHudFrame reference_frame,support_frame;
    std::string reference_sha256,support_sha256;
    PixelRegion title_region;
    std::size_t title_foreground_pixels=0,plausible_score_components=0,score_boundary_components=0;
    double title_occupancy=0,title_local_contrast=0;
    HudGlyphEvidence auxiliary;
};

// Optional, round-local visual redundancy, not song identity or a permission
// lease. Two clean actual frames establish a fixed template before arming.
// Every fallback still measures the CURRENT captured frame; historical frame
// hashes or a planner tick never refresh actual UI receipt freshness.
class LearnedHudRedundancy {
    static constexpr std::int64_t ms=1'000'000,fresh=500*ms,cadence=100*ms,support_gap=60*ms;
    static constexpr PixelRegion title_search{25,672,512,29};
    static constexpr std::size_t max_attempts=8,title_pixels=512*29,rgb_bytes=1280u*720u*3u;
    LearnedHudEvidence value_;
    LearnedHudFrame context_;
    bool context_set_=false,armed_ever_=false;
    std::uint64_t expected_epoch_=0,last_sequence_=0;
    std::int64_t last_received_=-1,last_processed_=-1,last_attempt_=-1;
    std::unique_ptr<HudGlyphEvidenceProducer> producer_;
    std::vector<std::uint8_t> reference_,support_;
    static bool image_valid(const RgbImageView& v) {
        return v.width==1280&&v.height==720&&v.stride==3840&&v.pixels.size()==rgb_bytes;
    }
    static bool metadata_valid(const LearnedHudFrame& m) {
        return m.sequence&&m.epoch&&m.generation&&m.geometry_version&&m.source_valid&&
            !m.source_id.empty()&&m.source_id.size()<=512&&
            std::none_of(m.source_id.begin(),m.source_id.end(),[](unsigned char c){return c<32||c==127;})&&
            m.received_ns>=0&&m.processed_ns>=m.received_ns&&m.processed_ns-m.received_ns<fresh&&
            m.rotation>=0&&m.rotation<=3;
    }
    static bool same_context(const LearnedHudFrame& a,const LearnedHudFrame& b) {
        return a.source_id==b.source_id&&a.epoch==b.epoch&&a.generation==b.generation&&
            a.geometry_version==b.geometry_version&&a.rotation==b.rotation;
    }
    static bool glyph_pass(const HudGlyphMetrics& m) {
        return m.glyph_present&&m.negative_space_present&&m.foreground_pixels>=8&&m.negative_space_pixels>=16&&
            std::isfinite(m.foreground_mean_error)&&m.foreground_mean_error<=8&&
            std::isfinite(m.foreground_white_fraction)&&m.foreground_white_fraction>=.98&&
            std::isfinite(m.negative_space_bright_fraction)&&m.negative_space_bright_fraction<=.35&&
            std::isfinite(m.local_contrast)&&m.local_contrast>=40;
    }
    static std::size_t plausible(const HudScoreMetrics& s) {
        if(s.glyphs.size()>64)return 0;
        return static_cast<std::size_t>(std::count_if(s.glyphs.begin(),s.glyphs.end(),[](const auto& g){
            return g.plausible_stroke&&g.bounds.width>=3&&g.bounds.width<=24&&g.bounds.height>=16&&g.bounds.height<=32&&
                g.white_pixels>=12&&g.negative_pixels>=8&&std::isfinite(g.occupancy)&&g.occupancy>=.12&&g.occupancy<=.85;
        }));
    }
    static bool prefix_pass(const HudGlyphEvidence& p) {
        if((p.policy!=HudGlyphPolicy::shared_in_subpixel_prefix_score_v3&&p.policy!=HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4)||!p.valid||p.denied_state_match||p.title_required||
           !glyph_pass(p.fixed_glyphs[0])||!glyph_pass(p.fixed_glyphs[1])||!p.difficulty_alignment_unique_family||
           p.difficulty_alignment_candidates<1||p.difficulty_alignment_candidates>3||
           p.difficulty_alignment_lower_x< -8||p.difficulty_alignment_upper_x>8||
           p.difficulty_alignment_lower_x>p.difficulty_alignment_upper_x||
           p.difficulty_alignment_upper_x-p.difficulty_alignment_lower_x>2||
           p.difficulty_alignment_candidates!=static_cast<std::size_t>(p.difficulty_alignment_upper_x-p.difficulty_alignment_lower_x+1))return false;
        const auto& g=p.fixed_glyphs[1];
        return !g.subpixel_tolerant||(g.literal_components==5&&std::isfinite(g.unexpected_white_fraction)&&
            g.unexpected_white_fraction<=.10&&std::isfinite(g.observed_ink_ratio)&&g.observed_ink_ratio>=.65);
    }
    static bool teacher(const HudGlyphEvidence& p) {
        const auto expected=p.policy==HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4 ?
            "experimental_shared_difficulty_subpixel_HUD_candidate_only" : "experimental_shared_IN_subpixel_HUD_candidate_only";
        return prefix_pass(p)&&p.hud_candidate&&p.reason==expected&&
            p.score.bounded&&p.score.calibrated_cells&&p.score.glyph_structure_candidate&&p.score.placement_consistent&&
            p.score.supported_cells==7&&p.score.glyphs.size()==7&&plausible(p.score)==7;
    }
    static bool occlusion(const HudGlyphEvidence& p) {
        return prefix_pass(p)&&!p.hud_candidate&&p.reason=="score_digit_structure_missing"&&
            p.score.bounded&&p.score.calibrated_cells&&!p.score.glyph_structure_candidate&&
            p.score.boundary_components>0&&plausible(p.score)>=4;
    }
    static bool white(const std::uint8_t* p) {return p[0]>195&&p[1]>195&&p[2]>195&&std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]})<35;}
    static int gray(const std::uint8_t* p) {return static_cast<int>((77u*p[0]+150u*p[1]+29u*p[2]+128u)>>8);}
    static const std::uint8_t* pixel(const RgbImageView& image,int x,int y) {return image.pixels.data()+static_cast<std::size_t>(y)*3840+static_cast<std::size_t>(x)*3;}
    bool title_region(const RgbImageView& image) {
        std::array<std::uint8_t,title_pixels> mask{};std::array<int,title_pixels> queue{};
        int x0=512,x1=-1,y0=29,y1=-1;std::size_t ink=0;
        for(int y=0;y<29;++y)for(int x=0;x<512;++x)if(white(pixel(image,title_search.x+x,title_search.y+y))){
            if(x==0||x==511||y==0||y==28)return false;
            mask[static_cast<std::size_t>(y*512+x)]=1;++ink;x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);
        }
        if(ink<32||x1-x0+1<12||y1-y0+1<12||x1+4>512)return false;
        const auto area=static_cast<double>((x1-x0+1)*(y1-y0+1));
        const auto occupancy=static_cast<double>(ink)/area;
        if(occupancy<.12||occupancy>.8)return false;
        double fg=0,dark=0;std::size_t dark_count=0;
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){const auto* p=pixel(image,25+x,672+y);if(white(p))fg+=gray(p);else if(gray(p)<128){dark+=gray(p);++dark_count;}}
        if(dark_count<16)return false;
        const auto contrast=fg/static_cast<double>(ink)-dark/static_cast<double>(dark_count);if(contrast<40)return false;
        std::size_t components=0,large_ink=0;bool supported_shape=false;
        for(int y=0;y<29;++y)for(int x=0;x<512;++x){
            const auto at=static_cast<std::size_t>(y*512+x);if(!mask[at])continue;
            if(++components>128)return false;
            mask[at]=0;queue[0]=static_cast<int>(at);std::size_t used=1;int cx0=x,cx1=x,cy0=y,cy1=y;
            for(std::size_t q=0;q<used;++q){const int px=queue[q]%512,py=queue[q]/512;cx0=std::min(cx0,px);cx1=std::max(cx1,px);cy0=std::min(cy0,py);cy1=std::max(cy1,py);
                for(const auto [dx,dy]:std::array<std::array<int,2>,4>{{{-1,0},{1,0},{0,-1},{0,1}}}){const int nx=px+dx,ny=py+dy;if(nx<0||nx>=512||ny<0||ny>=29)continue;const auto next=static_cast<std::size_t>(ny*512+nx);if(mask[next]){mask[next]=0;queue[used++]=static_cast<int>(next);}}
            }
            if(used>=3)large_ink+=used;
            if(used>=12&&cx1-cx0+1>=4&&cy1-cy0+1>=10)supported_shape=true;
        }
        if(!supported_shape||static_cast<double>(large_ink)<.8*static_cast<double>(ink))return false;
        value_.title_region={25,672,x1+4,29}; // Three observed blank columns after the final complete stroke.
        value_.title_foreground_pixels=ink;value_.title_occupancy=occupancy;value_.title_local_contrast=contrast;return true;
    }
    static bool clean_auxiliary_score(const HudGlyphEvidence& e) {
        return e.valid&&e.hud_candidate&&e.score.bounded&&e.score.glyph_structure_candidate&&
            e.score.boundary_components==0&&e.score.glyphs.size()==7&&plausible(e.score)==7;
    }
    void clear_model() {
        const auto attempts=value_.construction_attempts;value_=LearnedHudEvidence{};value_.construction_attempts=attempts;
        producer_.reset();std::vector<std::uint8_t>().swap(reference_);std::vector<std::uint8_t>().swap(support_);
    }
    static std::string hash(const std::vector<std::uint8_t>& raw) {return chart_text_sha256({reinterpret_cast<const char*>(raw.data()),raw.size()});}
public:
    const LearnedHudEvidence& status()const noexcept{return value_;}
    std::span<const std::uint8_t> reference_rgb()const noexcept{return reference_;}
    std::span<const std::uint8_t> support_rgb()const noexcept{return support_;}
    int reference_stride()const noexcept{return reference_.empty()?0:3840;}
    int support_stride()const noexcept{return support_.empty()?0:3840;}
    void invalidate(std::uint64_t new_epoch=0) {
        clear_model();context_set_=false;expected_epoch_=new_epoch;last_sequence_=0;value_.reason="explicit_epoch_invalidation";
        // Lifetime attempt budget and the no-post-arm-training latch survive.
    }
    const LearnedHudEvidence& observe(const RgbImageView& image,const HudGlyphEvidence& primary,
            const ScreenStateResult& raw,const LearnedHudFrame& meta,bool armed) {
        value_.current_candidate=false;value_.auxiliary=HudGlyphEvidence{};
        value_.plausible_score_components=0;value_.score_boundary_components=0;
        armed_ever_=armed_ever_||armed;
        if(!image_valid(image)){clear_model();value_.reason="invalid_learned_image_geometry";return value_;}
        if(!metadata_valid(meta)){value_.reason="invalid_or_stale_actual_frame";return value_;}
        if(expected_epoch_&&meta.epoch!=expected_epoch_){value_.reason="invalidated_epoch_mismatch";return value_;}
        if(meta.received_ns<=last_received_||meta.processed_ns<last_processed_){value_.reason="duplicate_or_nonmonotonic_receipt";return value_;}
        if(context_set_&&meta.source_id==context_.source_id&&(meta.generation<context_.generation||
           (meta.generation==context_.generation&&(meta.epoch<context_.epoch||meta.geometry_version<context_.geometry_version)))){
            clear_model();value_.reason="source_context_rollback";return value_;
        }
        if(!context_set_||!same_context(meta,context_)){clear_model();context_=meta;context_set_=true;expected_epoch_=0;last_sequence_=0;}
        if(meta.sequence<=last_sequence_){value_.reason="duplicate_or_nonmonotonic_sequence";return value_;}
        last_sequence_=meta.sequence;last_received_=meta.received_ns;last_processed_=meta.processed_ns;
        if(raw.scores.size()>8||!hud_candidate_eligible(true,armed,meta.source_valid,raw)){
            value_.reason="denied_or_unsupported_current_screen";return value_;
        }
        if(value_.ready){
            if(teacher(primary)){value_.reason="primary_admitted";return value_;}
            if(!armed){value_.reason="learned_reference_ready_unarmed";return value_;}
            if(!occlusion(primary)){value_.reason="not_score_only_boundary_occlusion";return value_;}
            value_.plausible_score_components=plausible(primary.score);value_.score_boundary_components=primary.score.boundary_components;
            value_.auxiliary=producer_->observe_features(image,raw.state);
            // Independently verify CURRENT pixels too: forged primary metrics
            // cannot turn a blank/solid score into an occlusion observation.
            value_.current_candidate=value_.auxiliary.valid&&value_.auxiliary.hud_candidate&&
                value_.auxiliary.score.bounded&&value_.auxiliary.score.boundary_components>0&&plausible(value_.auxiliary.score)>=4;
            value_.reason=value_.current_candidate?"current_learned_three_anchors_with_observed_score_occlusion":"current_auxiliary_anchors_or_score_evidence_missing";
            return value_;
        }
        if(armed_ever_){value_.reason="not_learned_before_arm";return value_;}
        if(producer_&&meta.processed_ns-value_.reference_frame.received_ns>=fresh){clear_model();value_.reason="candidate_reference_expired";}
        if(!teacher(primary)){value_.reason="waiting_for_clean_primary_HUD";return value_;}
        if(producer_){
            const auto dt=meta.received_ns-value_.reference_frame.received_ns;
            if(dt<support_gap){value_.reason="waiting_for_distinct_60ms_support";return value_;}
            value_.auxiliary=producer_->observe_features(image,raw.state);
            if(!clean_auxiliary_score(value_.auxiliary)){value_.reason="second_frame_full_anchors_or_clean_score_mismatch";return value_;}
            support_.assign(image.pixels.begin(),image.pixels.end());value_.support_sha256=hash(support_);value_.support_frame=meta;
            value_.support_frames=2;value_.ready=true;value_.reason="two_distinct_clean_frames_locked_before_arm";return value_;
        }
        if(value_.construction_attempts>=max_attempts){value_.reason="learned_reference_attempt_budget";return value_;}
        if(last_attempt_>=0&&meta.processed_ns-last_attempt_<cadence){value_.reason="learned_reference_construction_cadence";return value_;}
        last_attempt_=meta.processed_ns;++value_.construction_attempts;
        if(!title_region(image)){value_.reason="title_geometry_or_negative_space_unsupported";return value_;}
        try{
            reference_.assign(image.pixels.begin(),image.pixels.end());value_.reference_sha256=hash(reference_);
            const RgbImageView reference{1280,720,3840,reference_};
            producer_=std::make_unique<HudGlyphEvidenceProducer>(reference,value_.reference_sha256,
                value_.policy_id+'/'+value_.reference_sha256,value_.title_region,HudGlyphPolicy::calibrated_title_v1,
                primary.policy==HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4 ?
                    std::optional<PixelRegion>{PixelRegion{1144,673,109,26}} : std::nullopt,
                primary.policy==HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4 ?
                    HudPauseMaskPolicy::eroded_strong_white_core_v2 : HudPauseMaskPolicy::reference_white_v1);
            value_.auxiliary=producer_->observe_features(reference,raw.state);
            if(!clean_auxiliary_score(value_.auxiliary)){clear_model();value_.reason="reference_full_anchors_or_clean_score_unsupported";return value_;}
            value_.reference_frame=meta;value_.support_frames=1;value_.reason="waiting_for_distinct_60ms_support";
        }catch(const std::exception&){clear_model();value_.reason="reference_mask_construction_rejected";}
        return value_;
    }
};
} // namespace rhythm::experimental
