#pragma once
#include "rhythm/screen_state.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace rhythm::experimental {
// Unknown includes ambiguous calibrated matches. A denied-state match must
// retain priority even when the aggregate classifier cannot choose one state.
inline bool hud_calibration_matches(const ScreenStateResult& result,ObservedGameState state) {
    return result.state==state||std::any_of(result.scores.begin(),result.scores.end(),[state](const auto& score) {
        return score.state==state&&score.all_anchors_match;
    });
}
inline bool hud_fallback_eligible(bool armed,bool source_valid,const ScreenStateResult& result) {
    return armed&&source_valid&&result.state==ObservedGameState::unknown&&result.reason=="no_calibrated_state_match"&&
        !hud_calibration_matches(result,ObservedGameState::paused)&&!hud_calibration_matches(result,ObservedGameState::result);
}
enum class HudTimestampKind { unknown, video_pts, host_arrival };
struct HudFrameMetadata {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    std::string source_id,source_sha256,frame_id,frame_sha256,evidence_id,clock_id;
    HudTimestampKind timestamp_kind=HudTimestampKind::unknown;
    std::int64_t timestamp_ns=-1;
};
struct HudGlyphMetrics {
    std::size_t foreground_pixels=0,negative_space_pixels=0;
    double foreground_mean_error=255,foreground_white_fraction=0;
    double foreground_mean=0,negative_space_mean=255,negative_space_bright_fraction=1,local_contrast=0;
    bool glyph_present=false,negative_space_present=false;
};
struct HudScoreGlyph {
    PixelRegion bounds;
    std::size_t white_pixels=0,negative_pixels=0;
    double occupancy=0;
    bool plausible_stroke=false;
};
struct HudScoreMetrics {
    std::size_t components=0,boundary_components=0;
    std::vector<HudScoreGlyph> glyphs;
    bool bounded=true,placement_consistent=false,glyph_structure_candidate=false;
};
struct HudGlyphEvidence {
    HudFrameMetadata frame;
    std::string calibration_id,reference_sha256,reason="invalid_observation";
    std::array<HudGlyphMetrics,2> fixed_glyphs;
    HudGlyphMetrics title_glyph;
    HudScoreMetrics score;
    bool valid=false,hud_candidate=false,denied_state_match=false,metadata_attached=false;
    bool experiment_only=true,live_permitted=false,timing_qualified=false,source_pixel_age_qualified=false;
};

// Diagnostic feature producer only. This is not the calibrated state classifier,
// OCR, a chart-identity test, a lease or permission to inject. Thresholds define a
// frozen provisional policy for offline comparison; they are not measured error
// bounds. The owner must preserve denied-state priority and freshness/debounce.
// Metadata/hash strings are caller-supplied provenance, not authenticated bytes.
class HudGlyphEvidenceProducer {
    struct Sample {int x=0,y=0;std::uint8_t gray=0;};
    struct Mask {std::vector<Sample> foreground,negative;};
    std::array<Mask,3> masks_;
    std::string reference_sha256_,calibration_id_;
    static constexpr std::array<PixelRegion,3> fixed_regions={PixelRegion{21,23,28,33},PixelRegion{1152,673,101,26},PixelRegion{25,672,105,29}};
    static constexpr PixelRegion score_region{1076,20,180,40};
    static constexpr std::size_t score_pixels=180*40,max_components=64;
    static bool sha(const std::string& s) {
        return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
    }
    static bool text(const std::string& s) {return !s.empty()&&s.size()<=512;}
    static bool image_valid(const RgbImageView& image) {
        return image.width==1280&&image.height==720&&image.stride>=3840&&image.stride<=4096&&
            image.pixels.size()==static_cast<std::size_t>(image.stride)*720;
    }
    static const std::uint8_t* pixel(const RgbImageView& image,int x,int y) {
        return image.pixels.data()+static_cast<std::size_t>(y)*static_cast<std::size_t>(image.stride)+static_cast<std::size_t>(x)*3;
    }
    static int gray(const std::uint8_t* p) {return static_cast<int>((77u*p[0]+150u*p[1]+29u*p[2]+128u)>>8);}
    static bool white(const std::uint8_t* p) {
        return p[0]>195&&p[1]>195&&p[2]>195&&std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]})<35;
    }
    static bool metadata_valid(const HudFrameMetadata& f) {
        return f.sequence&&f.epoch&&f.generation&&f.geometry_version&&text(f.source_id)&&sha(f.source_sha256)&&
            text(f.frame_id)&&sha(f.frame_sha256)&&text(f.evidence_id)&&text(f.clock_id)&&f.timestamp_ns>=0&&
            (f.timestamp_kind==HudTimestampKind::video_pts||f.timestamp_kind==HudTimestampKind::host_arrival);
    }
    static Mask make_mask(const RgbImageView& reference,PixelRegion r) {
        Mask mask;
        for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x) {
            const auto* p=pixel(reference,x,y);
            // A line entering the reference ROI must not become a permanent
            // "glyph" requirement. A visually checked clean reference is still
            // required; this catches the observed boundary-crossing failure.
            if((x==r.x||y==r.y||x+1==r.x+r.width||y+1==r.y+r.height)&&white(p))
                throw std::invalid_argument("reference glyph ROI touches white boundary");
            if(white(p))mask.foreground.push_back({x,y,static_cast<std::uint8_t>(gray(p))});
            else if(gray(p)<128) {
                // A fixed two-pixel exclusion avoids antialiased glyph edges;
                // the remaining dark reference locations are negative space.
                bool near_glyph=false;
                for(int dy=-2;dy<=2&&!near_glyph;++dy)for(int dx=-2;dx<=2;++dx) {
                    const int nx=x+dx,ny=y+dy;
                    if(nx>=r.x&&ny>=r.y&&nx<r.x+r.width&&ny<r.y+r.height&&white(pixel(reference,nx,ny))) {
                        near_glyph=true;break;
                    }
                }
                if(!near_glyph)mask.negative.push_back({x,y,static_cast<std::uint8_t>(gray(p))});
            }
        }
        if(mask.foreground.size()<8||mask.negative.size()<16)throw std::invalid_argument("reference lacks glyph/negative-space support");
        return mask;
    }
    static HudGlyphMetrics measure(const RgbImageView& image,const Mask& mask) {
        HudGlyphMetrics m;m.foreground_pixels=mask.foreground.size();m.negative_space_pixels=mask.negative.size();
        double error=0,fg=0,negative=0,white_count=0,bright_negative=0;
        for(const auto& s:mask.foreground) {
            const auto* p=pixel(image,s.x,s.y);const int value=gray(p);
            error+=std::abs(value-static_cast<int>(s.gray));fg+=value;white_count+=white(p)?1:0;
        }
        for(const auto& s:mask.negative) {
            const auto* p=pixel(image,s.x,s.y);negative+=gray(p);bright_negative+=gray(p)>195?1:0;
        }
        const auto nf=static_cast<double>(m.foreground_pixels),nn=static_cast<double>(m.negative_space_pixels);
        m.foreground_mean_error=error/nf;m.foreground_white_fraction=white_count/nf;m.foreground_mean=fg/nf;
        m.negative_space_mean=negative/nn;m.negative_space_bright_fraction=bright_negative/nn;
        m.local_contrast=m.foreground_mean-m.negative_space_mean;
        m.glyph_present=m.foreground_mean_error<=8&&m.foreground_white_fraction>=.98;
        m.negative_space_present=m.negative_space_bright_fraction<=.35&&m.local_contrast>=40;
        return m;
    }
    static HudScoreMetrics score(const RgbImageView& image) {
        HudScoreMetrics out;
        std::array<std::uint8_t,score_pixels> mask{};
        std::array<int,score_pixels> queue{};
        const auto r=score_region;
        for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)
            mask[static_cast<std::size_t>(y*r.width+x)]=white(pixel(image,r.x+x,r.y+y))?1:0;
        for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x) {
            const auto index=static_cast<std::size_t>(y*r.width+x);
            if(!mask[index])continue;
            mask[index]=0;queue[0]=static_cast<int>(index);std::size_t used=1;
            int x0=x,x1=x,y0=y,y1=y;bool boundary=false;
            for(std::size_t i=0;i<used;++i) {
                const int q=queue[i],qx=q%r.width,qy=q/r.width;
                x0=std::min(x0,qx);x1=std::max(x1,qx);y0=std::min(y0,qy);y1=std::max(y1,qy);
                boundary|=qx==0||qy==0||qx+1==r.width||qy+1==r.height;
                for(const auto [dx,dy]:std::array<std::array<int,2>,4>{{{-1,0},{1,0},{0,-1},{0,1}}}) {
                    const int nx=qx+dx,ny=qy+dy;
                    if(nx<0||ny<0||nx>=r.width||ny>=r.height)continue;
                    const auto next=static_cast<std::size_t>(ny*r.width+nx);
                    if(mask[next]) {mask[next]=0;queue[used++]=static_cast<int>(next);}
                }
            }
            // Cropping cannot turn a long crossing line into a fake digit:
            // even a boundary component too small to be a glyph blocks the vote.
            if(boundary)++out.boundary_components;
            if(used<3)continue;
            if(++out.components>max_components) {out.bounded=false;return out;}
            const int width=x1-x0+1,height=y1-y0+1;
            const auto area=static_cast<std::size_t>(width*height);
            HudScoreGlyph g;g.bounds={r.x+x0,r.y+y0,width,height};g.white_pixels=used;
            g.negative_pixels=area-used;g.occupancy=static_cast<double>(used)/static_cast<double>(area);
            // A loose digit-shape diagnostic, not digit recognition. Empty
            // interiors/notches distinguish strokes from filled rectangles.
            g.plausible_stroke=!boundary&&height>=16&&height<=32&&width>=3&&width<=24&&
                g.occupancy>=.12&&g.occupancy<=.85&&g.negative_pixels>=8;
            out.glyphs.push_back(g);
        }
        std::sort(out.glyphs.begin(),out.glyphs.end(),[](const auto& a,const auto& b){return a.bounds.x<b.bounds.x;});
        if(out.glyphs.size()!=7||out.boundary_components)return out;
        int min_top=720,max_top=0,min_bottom=720,max_bottom=0;
        for(std::size_t i=0;i<out.glyphs.size();++i) {
            const auto& g=out.glyphs[i];if(!g.plausible_stroke)return out;
            min_top=std::min(min_top,g.bounds.y);max_top=std::max(max_top,g.bounds.y);
            min_bottom=std::min(min_bottom,g.bounds.y+g.bounds.height);max_bottom=std::max(max_bottom,g.bounds.y+g.bounds.height);
            if(i) {
                const auto& previous=out.glyphs[i-1].bounds;
                const int gap=g.bounds.x-previous.x-previous.width;
                if(gap<1||gap>14)return out;
            }
        }
        out.placement_consistent=max_top-min_top<=3&&max_bottom-min_bottom<=3;
        out.glyph_structure_candidate=out.placement_consistent;
        return out;
    }
public:
    HudGlyphEvidenceProducer(const RgbImageView& reference,std::string reference_sha256,std::string calibration_id)
        :reference_sha256_(std::move(reference_sha256)),calibration_id_(std::move(calibration_id)) {
        if(!image_valid(reference)||!sha(reference_sha256_)||!text(calibration_id_))
            throw std::invalid_argument("invalid HUD diagnostic reference");
        for(std::size_t i=0;i<masks_.size();++i)masks_[i]=make_mask(reference,fixed_regions[i]);
    }
    // Pure pixel classification; no per-frame hash or timing provenance is
    // constructed. An experiment owner must associate the returned features
    // with its actual source, frame and receipt before making a state decision.
    HudGlyphEvidence observe_features(const RgbImageView& image,
                             ObservedGameState same_frame_calibrated_state=ObservedGameState::unknown) const {
        HudGlyphEvidence out;out.reference_sha256=reference_sha256_;out.calibration_id=calibration_id_;
        if(!image_valid(image))return out;
        out.valid=true;
        out.denied_state_match=same_frame_calibrated_state==ObservedGameState::paused||same_frame_calibrated_state==ObservedGameState::result;
        for(std::size_t i=0;i<out.fixed_glyphs.size();++i)out.fixed_glyphs[i]=measure(image,masks_[i]);
        out.title_glyph=measure(image,masks_[2]);
        out.score=score(image);
        const bool pair=std::all_of(out.fixed_glyphs.begin(),out.fixed_glyphs.end(),[](const auto& m){return m.glyph_present&&m.negative_space_present;});
        // This version is scoped to the calibrated Dlyrotz title as a third
        // fixed glyph. Score shape is diagnostic: crossing lines can connect
        // legitimate digits outside its ROI. It must not veto all such HUDs or
        // be used to infer the score, chart identity or an AP result.
        const bool title=out.title_glyph.glyph_present&&out.title_glyph.negative_space_present;
        out.hud_candidate=!out.denied_state_match&&pair&&title&&out.score.bounded;
        out.reason=out.denied_state_match?"calibrated_denied_state_has_priority":!out.score.bounded?"score_component_budget":
            !pair?"fixed_glyph_or_negative_space_missing":!title?"title_glyph_or_negative_space_missing":
            "experimental_HUD_feature_candidate_only";
        return out;
    }
    HudGlyphEvidence observe(const RgbImageView& image,const HudFrameMetadata& frame,
                             ObservedGameState same_frame_calibrated_state=ObservedGameState::unknown) const {
        if(!metadata_valid(frame)) {
            HudGlyphEvidence out;out.reference_sha256=reference_sha256_;out.calibration_id=calibration_id_;return out;
        }
        auto out=observe_features(image,same_frame_calibrated_state);
        if(out.valid) {out.frame=frame;out.metadata_attached=true;}
        return out;
    }
};
} // namespace rhythm::experimental
