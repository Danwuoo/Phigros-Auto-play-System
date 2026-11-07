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
struct LearnedMergedScoreEvidence {
    bool candidate=false;
    std::string reason="not_measured";
    PixelRegion component;
    std::size_t spanned_windows=0,textured_windows=0,white_pixels=0,negative_pixels=0;
    std::array<PixelRegion,8> boundary_regions{};
    std::size_t boundary_fragments=0,clean_components=0,boundary_textured_windows=0;
    double occupancy=0;
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
    LearnedMergedScoreEvidence merged_score;
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
    std::optional<HudGlyphPolicy> learned_primary_policy_;
    std::vector<std::uint8_t> reference_,support_;
    std::array<PixelRegion,7> score_windows_{};
    bool score_windows_ready_=false;
    int score_top_=0,score_bottom_=0;
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
    static bool glyph_core_pass(const HudGlyphMetrics& m) {
        return m.negative_space_present&&m.foreground_pixels>=8&&m.negative_space_pixels>=16&&
            std::isfinite(m.foreground_mean_error)&&m.foreground_mean_error<=8&&
            std::isfinite(m.foreground_white_fraction)&&m.foreground_white_fraction>=.98&&
            std::isfinite(m.negative_space_bright_fraction)&&m.negative_space_bright_fraction<=.35&&
            std::isfinite(m.local_contrast)&&m.local_contrast>=40;
    }
    static bool glyph_pass(const HudGlyphMetrics& m) {return m.glyph_present&&glyph_core_pass(m);}
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
    static bool merged_score_boundary(const HudGlyphEvidence& p) {
        return prefix_pass(p)&&!p.hud_candidate&&p.reason=="score_digit_structure_missing"&&
            p.score.bounded&&p.score.calibrated_cells&&!p.score.glyph_structure_candidate&&
            p.score.boundary_components==1;
    }
    static bool separated_score_calibration_failure(const HudGlyphEvidence& p) {
        return prefix_pass(p)&&!p.hud_candidate&&p.reason=="score_digit_structure_missing"&&
            p.score.bounded&&p.score.calibrated_cells&&!p.score.glyph_structure_candidate&&
            p.score.boundary_components==0&&p.score.components==7;
    }
    static bool primary_prefix_mismatch_core(const HudGlyphEvidence& p) {
        const auto& g=p.fixed_glyphs[1];
        return (p.policy==HudGlyphPolicy::shared_in_subpixel_prefix_score_v3||
                p.policy==HudGlyphPolicy::shared_difficulty_subpixel_prefix_score_v4)&&p.valid&&!p.denied_state_match&&
            !p.title_required&&!p.hud_candidate&&p.reason=="fixed_glyph_or_negative_space_missing"&&
            glyph_pass(p.fixed_glyphs[0])&&!g.glyph_present&&glyph_core_pass(g)&&g.subpixel_tolerant&&
            std::isfinite(g.unexpected_white_fraction)&&g.unexpected_white_fraction>=0&&g.unexpected_white_fraction<=1&&
            std::isfinite(g.observed_ink_ratio)&&g.observed_ink_ratio>=.65;
    }
    static bool primary_prefix_mismatch(const HudGlyphEvidence& p) {
        return primary_prefix_mismatch_core(p)&&p.score.bounded&&p.score.calibrated_cells&&p.score.glyph_structure_candidate&&p.score.placement_consistent&&
            p.score.boundary_components==0&&p.score.supported_cells==7&&p.score.glyphs.size()==7&&plausible(p.score)==7;
    }
    static bool joint_ordinary_occlusion(const HudGlyphEvidence& p) {
        return primary_prefix_mismatch_core(p)&&p.score.bounded&&p.score.calibrated_cells&&
            !p.score.glyph_structure_candidate&&p.score.boundary_components>0&&plausible(p.score)>=4;
    }
    static bool joint_merged_score_boundary(const HudGlyphEvidence& p) {
        return primary_prefix_mismatch_core(p)&&p.score.bounded&&p.score.calibrated_cells&&
            !p.score.glyph_structure_candidate&&p.score.boundary_components==1;
    }
    static bool partial_score_boundary(const HudGlyphEvidence& p) {
        const bool primary_shape=(prefix_pass(p)&&!p.hud_candidate&&p.reason=="score_digit_structure_missing")||
            primary_prefix_mismatch_core(p);
        return primary_shape&&p.score.bounded&&p.score.calibrated_cells&&!p.score.glyph_structure_candidate&&
            p.score.boundary_components>=1&&p.score.boundary_components<=8&&p.score.glyphs.size()<=64;
    }
    static bool white(const std::uint8_t* p) {return p[0]>195&&p[1]>195&&p[2]>195&&std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]})<35;}
    static int gray(const std::uint8_t* p) {return static_cast<int>((77u*p[0]+150u*p[1]+29u*p[2]+128u)>>8);}
    static const std::uint8_t* pixel(const RgbImageView& image,int x,int y) {return image.pixels.data()+static_cast<std::size_t>(y)*3840+static_cast<std::size_t>(x)*3;}
    bool title_region(const RgbImageView& image) {
        std::array<std::uint8_t,title_pixels> mask{};std::array<int,title_pixels> queue{};
        std::array<bool,512> columns{};
        int right=-1;
        for(int y=0;y<29;++y)for(int x=0;x<512;++x)if(white(pixel(image,title_search.x+x,title_search.y+y))){
            if(x==0||y==0||y==28)return false;
            mask[static_cast<std::size_t>(y*512+x)]=1;columns[static_cast<std::size_t>(x)]=true;right=std::max(right,x);
        }
        int width=right+4;
        const bool crop_required=width>512;
        if(crop_required) {
            // Use only a bounded, complete prefix when the full title
            // crosses the search edge. A gap of three blank columns keeps
            // the crop from cutting a connected stroke.
            width=511;
            for(;width>=99;--width)
                if(!columns[width-1]&&!columns[width-2]&&!columns[width-3])break;
            if(width<99)return false;
        }
        int x0=512,x1=-1,y0=29,y1=-1;std::size_t ink=0;
        for(int y=0;y<29;++y)for(int x=0;x<width;++x)if(mask[static_cast<std::size_t>(y*512+x)]){
            ++ink;x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);
        }
        const int minimum_span=crop_required?96:12;
        if(ink<32||x1-x0+1<minimum_span||y1-y0+1<12)return false;
        const auto area=static_cast<double>((x1-x0+1)*(y1-y0+1));
        const auto occupancy=static_cast<double>(ink)/area;
        if(occupancy<.12||occupancy>.8)return false;
        double fg=0,dark=0;std::size_t dark_count=0;
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){const auto* p=pixel(image,25+x,672+y);if(white(p))fg+=gray(p);else if(gray(p)<128){dark+=gray(p);++dark_count;}}
        if(dark_count<16)return false;
        const auto contrast=fg/static_cast<double>(ink)-dark/static_cast<double>(dark_count);if(contrast<40)return false;
        std::size_t components=0,large_ink=0;bool supported_shape=false;
        for(int y=0;y<29;++y)for(int x=0;x<width;++x){
            const auto at=static_cast<std::size_t>(y*512+x);if(!mask[at])continue;
            if(++components>128)return false;
            mask[at]=0;queue[0]=static_cast<int>(at);std::size_t used=1;int cx0=x,cx1=x,cy0=y,cy1=y;
            for(std::size_t q=0;q<used;++q){const int px=queue[q]%512,py=queue[q]/512;cx0=std::min(cx0,px);cx1=std::max(cx1,px);cy0=std::min(cy0,py);cy1=std::max(cy1,py);
                for(const auto [dx,dy]:std::array<std::array<int,2>,4>{{{-1,0},{1,0},{0,-1},{0,1}}}){const int nx=px+dx,ny=py+dy;if(nx<0||nx>=width||ny<0||ny>=29)continue;const auto next=static_cast<std::size_t>(ny*512+nx);if(mask[next]){mask[next]=0;queue[used++]=static_cast<int>(next);}}
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
    LearnedMergedScoreEvidence merged_score_evidence(const RgbImageView& image,const HudScoreMetrics& score)const {
        LearnedMergedScoreEvidence result;result.reason="merged_score_component_missing";
        if(!score_windows_ready_||!score.bounded||score.boundary_components!=1||score.glyphs.size()>64)return result;
        const HudScoreGlyph* merged=nullptr;
        for(const auto& glyph:score.glyphs){
            const auto& b=glyph.bounds;
            // A line plus text can occupy the complete 40px score ROI; this
            // is not one font glyph. The normal digit 32px limit is unchanged.
            if(b.x<1076||b.y<20||b.width<3||b.height<16||b.height>40||b.x+b.width>1256||b.y+b.height>60||
               !(b.x==1076||b.y==20||b.x+b.width==1256||b.y+b.height==60)||
               glyph.white_pixels<80||glyph.negative_pixels<80||!std::isfinite(glyph.occupancy)||
               glyph.occupancy<.12||glyph.occupancy>.85)continue;
            std::size_t spanned=0;
            for(const auto& window:score_windows_)if(b.x<=window.x&&b.x+b.width>=window.x+window.width&&
                std::min(b.y+b.height,window.y+window.height)-std::max(b.y,window.y)>=12)++spanned;
            if(spanned<4)continue;
            if(merged){result.reason="merged_score_component_ambiguous";return result;}
            merged=&glyph;result.spanned_windows=spanned;
        }
        if(!merged)return result;
        result.component=merged->bounds;result.white_pixels=merged->white_pixels;
        result.negative_pixels=merged->negative_pixels;result.occupancy=merged->occupancy;
        // These fixed pre-arm coordinates are only separated texture windows,
        // not current digit cells or inferred proportional font advances. All
        // ink/dark/vertical-stroke evidence comes from the CURRENT raw pixels.
        // At most7*24*32 source pixels are examined; no image is reconstructed.
        for(const auto& window:score_windows_){
            if(merged->bounds.x>window.x||merged->bounds.x+merged->bounds.width<window.x+window.width)continue;
            std::size_t ink=0,dark=0,vertical_rows=0;double white_sum=0,dark_sum=0;
            int first=720,last=-1;
            const auto top=std::max(score_top_+3,window.y),bottom=std::min(score_bottom_-3,window.y+window.height);
            for(int y=top;y<bottom;++y){bool row_white=false,row_dark=false;
                for(int x=window.x;x<window.x+window.width;++x){const auto* p=pixel(image,x,y);
                    if(white(p)){++ink;white_sum+=gray(p);row_white=true;}
                    else if(gray(p)<128){++dark;dark_sum+=gray(p);row_dark=true;}}
                if(row_white&&row_dark){++vertical_rows;first=std::min(first,y);last=std::max(last,y);}
            }
            if(ink>=12&&dark>=8&&vertical_rows>=8&&last-first>=12&&
               white_sum/static_cast<double>(ink)-dark_sum/static_cast<double>(dark)>=40)++result.textured_windows;
        }
        result.candidate=result.textured_windows>=4;
        result.reason=result.candidate?"current_wide_boundary_score_with_distributed_vertical_texture":
            "merged_score_current_vertical_texture_missing";
        return result;
    }
    LearnedMergedScoreEvidence partial_score_evidence(const RgbImageView& image,const HudScoreMetrics& score)const {
        LearnedMergedScoreEvidence result;result.reason="partial_boundary_score_components_missing";
        if(!score_windows_ready_||!score.bounded||score.boundary_components<1||score.boundary_components>8||
           score.glyphs.size()>64)return result;
        std::array<bool,7> supported{},boundary_supported{};
        // A fragment must be an actual CURRENT score CC at the fixed ROI edge.
        // It must cover at least one whole learned texture window; no union
        // bounding box is manufactured and no old frame supplies score pixels.
        for(const auto& glyph:score.glyphs){
            const auto& b=glyph.bounds;
            const bool boundary=b.x==1076||b.y==20||b.x+b.width==1256||b.y+b.height==60;
            if(b.x<1076||b.y<20||b.width<3||b.height<16||b.height>40||b.x+b.width>1256||b.y+b.height>60||
               !boundary||glyph.white_pixels<80||glyph.negative_pixels<80||!std::isfinite(glyph.occupancy)||
               glyph.occupancy<.12||glyph.occupancy>.85)continue;
            std::array<bool,7> covered{};bool any=false;
            for(std::size_t i=0;i<score_windows_.size();++i){const auto& w=score_windows_[i];
                covered[i]=b.x<=w.x&&b.x+b.width>=w.x+w.width&&
                    std::min(b.y+b.height,w.y+w.height)-std::max(b.y,w.y)>=12;
                any=any||covered[i];
            }
            if(!any)continue;
            if(result.boundary_fragments>=result.boundary_regions.size()){
                result.reason="partial_boundary_score_fragment_budget";return result;
            }
            result.boundary_regions[result.boundary_fragments++]=b;
            for(std::size_t i=0;i<covered.size();++i)if(covered[i])supported[i]=boundary_supported[i]=true;
        }
        if(!result.boundary_fragments)return result;
        // The ordinary diagnostic's original plausible-digit requirements are
        // retained. One clean CC may support only its unique greatest-overlap
        // learned window, so a wide/ambiguous box cannot vote several times.
        for(const auto& glyph:score.glyphs){
            const auto& b=glyph.bounds;
            if(!glyph.plausible_stroke||b.width<3||b.width>24||b.height<16||b.height>32||
               glyph.white_pixels<12||glyph.negative_pixels<8||!std::isfinite(glyph.occupancy)||
               glyph.occupancy<.12||glyph.occupancy>.85)continue;
            int selected=-1,largest=0;bool unique=false;
            for(std::size_t i=0;i<score_windows_.size();++i){const auto& w=score_windows_[i];
                if(std::min(b.y+b.height,w.y+w.height)-std::max(b.y,w.y)<12)continue;
                const int overlap=std::min(b.x+b.width,w.x+w.width)-std::max(b.x,w.x);
                if(overlap<3||2*overlap<std::min(b.width,w.width))continue;
                if(overlap>largest){largest=overlap;selected=static_cast<int>(i);unique=true;}
                else if(overlap==largest)unique=false;
            }
            if(selected>=0&&unique){supported[static_cast<std::size_t>(selected)]=true;++result.clean_components;}
        }
        for(std::size_t i=0;i<score_windows_.size();++i){
            if(!supported[i])continue;
            ++result.spanned_windows;
            const auto& window=score_windows_[i];
            std::size_t ink=0,dark=0,vertical_rows=0;double white_sum=0,dark_sum=0;
            int first=720,last=-1;
            const auto top=std::max(score_top_+3,window.y),bottom=std::min(score_bottom_-3,window.y+window.height);
            for(int y=top;y<bottom;++y){bool row_white=false,row_dark=false;
                for(int x=window.x;x<window.x+window.width;++x){const auto* p=pixel(image,x,y);
                    if(white(p)){++ink;white_sum+=gray(p);row_white=true;}
                    else if(gray(p)<128){++dark;dark_sum+=gray(p);row_dark=true;}}
                if(row_white&&row_dark){++vertical_rows;first=std::min(first,y);last=std::max(last,y);}
            }
            if(ink>=12&&dark>=8&&vertical_rows>=8&&last-first>=12&&
               white_sum/static_cast<double>(ink)-dark_sum/static_cast<double>(dark)>=40){
                ++result.textured_windows;if(boundary_supported[i])++result.boundary_textured_windows;
            }
        }
        result.candidate=result.spanned_windows>=4&&result.textured_windows>=4&&result.boundary_textured_windows>=1;
        result.reason=result.candidate?"current_partial_boundary_and_clean_score_with_distributed_vertical_texture":
            "partial_boundary_score_current_vertical_texture_missing";
        return result;
    }
    void clear_model() {
        const auto attempts=value_.construction_attempts;value_=LearnedHudEvidence{};value_.construction_attempts=attempts;
        producer_.reset();learned_primary_policy_.reset();std::vector<std::uint8_t>().swap(reference_);std::vector<std::uint8_t>().swap(support_);
        score_windows_ready_=false;score_windows_={};score_top_=score_bottom_=0;
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
        value_.merged_score=LearnedMergedScoreEvidence{};
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
            // Alternate route after an IN/AT primary prefix shape mismatch.
            // No primary threshold is widened: independently require CURRENT
            // full learned difficulty/level, title, pause and seven clean digits.
            // A template learned under another primary policy is never reused.
            if(learned_primary_policy_==primary.policy&&primary_prefix_mismatch(primary)) {
                value_.auxiliary=producer_->observe_features(image,raw.state);
                value_.current_candidate=clean_auxiliary_score(value_.auxiliary);
                value_.reason=value_.current_candidate?"current_learned_full_HUD_after_primary_prefix_mismatch":
                    "current_auxiliary_full_HUD_missing_after_primary_prefix_mismatch";
                return value_;
            }
            if(learned_primary_policy_==primary.policy&&score_windows_ready_&&separated_score_calibration_failure(primary)) {
                value_.auxiliary=producer_->observe_features(image,raw.state);
                if(clean_auxiliary_score(value_.auxiliary)) {
                    const auto& current=value_.auxiliary.score.glyphs.back().bounds;
                    const auto& learned=score_windows_.back();
                    value_.current_candidate=std::abs(current.x+current.width-learned.x-learned.width)<=3;
                }
                value_.reason=value_.current_candidate?"current_learned_full_HUD_after_score_cell_mismatch":
                    "current_auxiliary_full_HUD_or_score_anchor_missing_after_score_cell_mismatch";
                return value_;
            }
            if(learned_primary_policy_==primary.policy&&joint_ordinary_occlusion(primary)) {
                // The two existing failure shapes can coincide. Neither the
                // primary thresholds nor the score witness are relaxed: all
                // complete learned anchors and ordinary score evidence must
                // be measured again from this exact current frame.
                value_.plausible_score_components=plausible(primary.score);
                value_.score_boundary_components=primary.score.boundary_components;
                value_.auxiliary=producer_->observe_features(image,raw.state);
                value_.current_candidate=value_.auxiliary.valid&&value_.auxiliary.hud_candidate&&
                    glyph_pass(value_.auxiliary.fixed_glyphs[0])&&glyph_pass(value_.auxiliary.fixed_glyphs[1])&&
                    glyph_pass(value_.auxiliary.title_glyph)&&value_.auxiliary.score.bounded&&
                    value_.auxiliary.score.boundary_components>0&&plausible(value_.auxiliary.score)>=4;
                value_.reason=value_.current_candidate?"current_learned_full_HUD_with_joint_prefix_and_ordinary_score_occlusion":
                    "current_auxiliary_anchors_or_ordinary_score_missing_after_joint_occlusion";
                return value_;
            }
            const bool ordinary_occlusion=occlusion(primary);
            const bool joint_merged=learned_primary_policy_==primary.policy&&joint_merged_score_boundary(primary);
            const bool merged_boundary=learned_primary_policy_==primary.policy&&
                (merged_score_boundary(primary)||joint_merged);
            const bool partial_boundary=learned_primary_policy_==primary.policy&&partial_score_boundary(primary);
            if(!ordinary_occlusion&&!merged_boundary&&!partial_boundary){value_.reason="auxiliary_not_measured_unsupported_primary_failure_shape";return value_;}
            value_.plausible_score_components=plausible(primary.score);value_.score_boundary_components=primary.score.boundary_components;
            value_.auxiliary=producer_->observe_features(image,raw.state);
            if(!ordinary_occlusion){
                // A single connected score line can merge all digit CCs. Three
                // independent CURRENT fixed anchors remain mandatory, as does
                // a bounded textured score witness measured without rewriting
                // pixels. A blank/solid/bare line or hollow rectangle cannot
                // borrow a past title/level/pause mask or synthetic digits.
                const bool anchors=value_.auxiliary.valid&&value_.auxiliary.hud_candidate&&
                    glyph_pass(value_.auxiliary.fixed_glyphs[0])&&glyph_pass(value_.auxiliary.fixed_glyphs[1])&&
                    glyph_pass(value_.auxiliary.title_glyph);
                if(anchors){
                    if(merged_boundary)value_.merged_score=merged_score_evidence(image,value_.auxiliary.score);
                    if(!value_.merged_score.candidate&&partial_boundary)
                        value_.merged_score=partial_score_evidence(image,value_.auxiliary.score);
                }
                else value_.merged_score.reason="current_full_learned_anchors_missing";
                value_.current_candidate=anchors&&value_.merged_score.candidate;
                if(value_.current_candidate&&value_.merged_score.boundary_fragments){
                    value_.reason=primary_prefix_mismatch_core(primary)?
                        "current_learned_three_anchors_with_joint_prefix_and_partial_boundary_score":
                        "current_learned_three_anchors_with_partial_boundary_score";
                    return value_;
                }
                value_.reason=value_.current_candidate?
                    (joint_merged?"current_learned_three_anchors_with_joint_prefix_and_merged_score_boundary":
                        "current_learned_three_anchors_with_merged_score_boundary"):
                    (joint_merged?"current_auxiliary_anchors_or_merged_score_texture_missing_after_joint_prefix_mismatch":
                        "current_auxiliary_anchors_or_merged_score_texture_missing");
                return value_;
            }
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
            learned_primary_policy_=primary.policy;
            value_.auxiliary=producer_->observe_features(reference,raw.state);
            if(!clean_auxiliary_score(value_.auxiliary)){clear_model();value_.reason="reference_full_anchors_or_clean_score_unsupported";return value_;}
            score_top_=720;score_bottom_=0;
            for(std::size_t i=0;i<7;++i){score_windows_[i]=value_.auxiliary.score.glyphs[i].bounds;
                score_top_=std::min(score_top_,score_windows_[i].y);score_bottom_=std::max(score_bottom_,score_windows_[i].y+score_windows_[i].height);}
            score_windows_ready_=true;
            value_.reference_frame=meta;value_.support_frames=1;value_.reason="waiting_for_distinct_60ms_support";
        }catch(const std::exception&){clear_model();value_.reason="reference_mask_construction_rejected";}
        return value_;
    }
};
} // namespace rhythm::experimental
