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
// Shared IN is an explicit experiment selected before READY. Pixel glyphs are
// still mandatory on every accepted frame, including raw template positives.
inline bool hud_candidate_eligible(bool shared_in,bool armed,bool source_valid,const ScreenStateResult& result) {
    if(!shared_in)return hud_fallback_eligible(armed,source_valid,result);
    return source_valid&&(result.state==ObservedGameState::unknown||result.state==ObservedGameState::playing)&&
        !hud_calibration_matches(result,ObservedGameState::paused)&&!hud_calibration_matches(result,ObservedGameState::result);
}
enum class HudTimestampKind { unknown, video_pts, host_arrival };
enum class HudGlyphPolicy { calibrated_title_v1, shared_in_prefix_score_v1, shared_in_cell_score_v2, shared_in_subpixel_prefix_score_v3 };
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
    bool glyph_present=false,negative_space_present=false,subpixel_tolerant=false;
    double reference_gray_error=255,unexpected_white_fraction=1,observed_ink_ratio=0;
    std::size_t literal_components=0;
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
    bool calibrated_cells=false;
    std::size_t supported_cells=0;
    int trimmed_left_pixels=0;
    std::optional<PixelRegion> original_left_bounds,retained_left_bounds;
};
struct HudGlyphEvidence {
    HudFrameMetadata frame;
    std::string calibration_id,reference_sha256,reason="invalid_observation";
    std::array<HudGlyphMetrics,2> fixed_glyphs;
    HudGlyphMetrics title_glyph;
    HudScoreMetrics score;
    HudGlyphPolicy policy=HudGlyphPolicy::calibrated_title_v1;
    int difficulty_alignment_x=0,difficulty_alignment_lower_x=0,difficulty_alignment_upper_x=0;
    bool difficulty_alignment_unique_family=false;
    std::size_t difficulty_alignment_candidates=0;
    bool title_required=true;
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
    PixelRegion title_region_;
    const HudGlyphPolicy policy_;
    PixelRegion score_band_{};
    int score_top_=0,score_bottom_=0,score_right_=0,score_height_=0,score_ink_width_=0;
    bool shared() const noexcept {return policy_!=HudGlyphPolicy::calibrated_title_v1;}
    bool cell_score() const noexcept {return policy_==HudGlyphPolicy::shared_in_cell_score_v2||policy_==HudGlyphPolicy::shared_in_subpixel_prefix_score_v3;}
    static constexpr std::size_t prefix_pixels=73*26;
    using PrefixMask=std::array<std::uint8_t,prefix_pixels>;
    PrefixMask literal_prefix_mask_{};
    std::vector<PixelRegion> literal_prefix_boxes_;
    static constexpr std::array<PixelRegion,3> fixed_regions={PixelRegion{21,23,28,33},PixelRegion{1152,673,101,26},PixelRegion{25,672,105,29}};
    // Actual 4.0.1 IN Lv.12/13 references place the decimal point at x1220..1222
    // and the first level digit at x1227. This ends before either level digit.
    static constexpr PixelRegion shared_in_prefix_region{1152,673,73,26};
    static constexpr int shared_in_max_alignment=8;
    static constexpr std::int64_t max_title_pixels=16384;
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
    static bool title_region_valid(PixelRegion r) {
        // Check dimensions before subtraction/addition so hostile signed values
        // cannot overflow a bound or cause unbounded mask construction.
        return r.x>=0&&r.y>=0&&r.width>0&&r.width<=1280&&r.height>0&&r.height<=720&&
            r.x<=1280-r.width&&r.y<=720-r.height&&
            static_cast<std::int64_t>(r.width)*r.height<=max_title_pixels;
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
    static HudGlyphMetrics measure(const RgbImageView& image,const Mask& mask,int shift_x=0) {
        HudGlyphMetrics m;m.foreground_pixels=mask.foreground.size();m.negative_space_pixels=mask.negative.size();
        double error=0,fg=0,negative=0,white_count=0,bright_negative=0;
        for(const auto& s:mask.foreground) {
            const auto* p=pixel(image,s.x+shift_x,s.y);const int value=gray(p);
            error+=std::abs(value-static_cast<int>(s.gray));fg+=value;white_count+=white(p)?1:0;
        }
        for(const auto& s:mask.negative) {
            const auto* p=pixel(image,s.x+shift_x,s.y);negative+=gray(p);bright_negative+=gray(p)>195?1:0;
        }
        const auto nf=static_cast<double>(m.foreground_pixels),nn=static_cast<double>(m.negative_space_pixels);
        m.foreground_mean_error=error/nf;m.reference_gray_error=m.foreground_mean_error;m.foreground_white_fraction=white_count/nf;m.foreground_mean=fg/nf;
        m.negative_space_mean=negative/nn;m.negative_space_bright_fraction=bright_negative/nn;
        m.local_contrast=m.foreground_mean-m.negative_space_mean;
        m.glyph_present=m.foreground_mean_error<=8&&m.foreground_white_fraction>=.98;
        m.negative_space_present=m.negative_space_bright_fraction<=.35&&m.local_contrast>=40;
        return m;
    }
    static std::vector<PixelRegion> prefix_boxes(PrefixMask mask) {
        std::vector<PixelRegion> boxes;
        std::array<int,prefix_pixels> queue{};
        for(int y=0;y<26;++y)for(int x=0;x<73;++x) {
            const auto at=static_cast<std::size_t>(y*73+x);if(!mask[at])continue;
            mask[at]=0;queue[0]=static_cast<int>(at);std::size_t used=1;
            int x0=x,x1=x,y0=y,y1=y;
            for(std::size_t i=0;i<used;++i) {
                const auto q=queue[i];const int qx=q%73,qy=q/73;
                x0=std::min(x0,qx);x1=std::max(x1,qx);y0=std::min(y0,qy);y1=std::max(y1,qy);
                // Four-neighbor literal strokes: an eight-neighbor checker
                // can mimic antialiasing while containing no connected letter.
                for(const auto [dx,dy]:std::array<std::array<int,2>,4>{{{-1,0},{1,0},{0,-1},{0,1}}}) {
                    const int nx=qx+dx,ny=qy+dy;if(nx<0||nx>=73||ny<0||ny>=26)continue;
                    const auto next=static_cast<std::size_t>(ny*73+nx);
                    if(mask[next]){mask[next]=0;queue[used++]=static_cast<int>(next);}
                }
            }
            if(used>=3){boxes.push_back({x0,y0,x1-x0+1,y1-y0+1});if(boxes.size()>5)return boxes;}
        }
        std::sort(boxes.begin(),boxes.end(),[](auto a,auto b){return a.x<b.x;});return boxes;
    }
    void calibrate_literal_prefix() {
        for(const auto& sample:masks_[1].foreground)
            literal_prefix_mask_[static_cast<std::size_t>((sample.y-shared_in_prefix_region.y)*73+sample.x-shared_in_prefix_region.x)]=1;
        literal_prefix_boxes_=prefix_boxes(literal_prefix_mask_);
        if(literal_prefix_boxes_.size()!=5)throw std::invalid_argument("subpixel shared IN reference requires five separated literal prefix strokes");
    }
    HudGlyphMetrics measure_literal_prefix(const RgbImageView& image,int shift_x) const {
        auto m=measure(image,masks_[1],shift_x);m.subpixel_tolerant=true;
        PrefixMask actual{};std::size_t ink=0,unexpected=0,covered=0;
        for(int y=0;y<26;++y)for(int x=0;x<73;++x) {
            if(!white(pixel(image,shared_in_prefix_region.x+x+shift_x,shared_in_prefix_region.y+y)))continue;
            actual[static_cast<std::size_t>(y*73+x)]=1;++ink;bool expected=false;
            for(int dy=-1;dy<=1&&!expected;++dy)for(int dx=-1;dx<=1;++dx) {
                const int nx=x+dx,ny=y+dy;if(nx>=0&&nx<73&&ny>=0&&ny<26&&literal_prefix_mask_[static_cast<std::size_t>(ny*73+nx)]){expected=true;break;}
            }
            if(!expected)++unexpected;
        }
        double peak_sum=0;
        for(const auto& sample:masks_[1].foreground) {
            int peak=0;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                const auto* value=pixel(image,sample.x+shift_x+dx,sample.y+dy);
                if(white(value))peak=std::max(peak,gray(value));
            }
            if(peak)++covered;peak_sum+=peak;
        }
        const auto count=static_cast<double>(masks_[1].foreground.size());
        // This explicitly versioned path compares observed white support with
        // the BINARY reference shape. The original gray error is retained;
        // no input pixels/brightness are normalized or reconstructed.
        m.foreground_mean_error=255-peak_sum/count;m.foreground_mean=peak_sum/count;
        m.foreground_white_fraction=static_cast<double>(covered)/count;
        m.local_contrast=m.foreground_mean-m.negative_space_mean;
        m.unexpected_white_fraction=ink?static_cast<double>(unexpected)/static_cast<double>(ink):1;
        m.observed_ink_ratio=static_cast<double>(ink)/count;
        const auto boxes=prefix_boxes(actual);m.literal_components=boxes.size();
        bool geometry=boxes.size()==literal_prefix_boxes_.size();
        if(geometry)for(std::size_t i=0;i<boxes.size();++i) {
            const auto a=boxes[i],b=literal_prefix_boxes_[i];
            if(std::abs(a.x-b.x)>1||std::abs(a.y-b.y)>1||
               std::abs(a.x+a.width-b.x-b.width)>1||std::abs(a.y+a.height-b.y-b.height)>1){geometry=false;break;}
        }
        m.glyph_present=geometry&&m.foreground_mean_error<=8&&m.foreground_white_fraction>=.98&&
            m.unexpected_white_fraction<=.10&&m.observed_ink_ratio>=.65;
        m.negative_space_present=m.negative_space_bright_fraction<=.35&&m.local_contrast>=40;
        return m;
    }
    static HudScoreMetrics score(const RgbImageView& image,PixelRegion r=score_region) {
        HudScoreMetrics out;
        std::array<std::uint8_t,score_pixels> mask{};
        std::array<int,score_pixels> queue{};
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
    void calibrate_score_cells(const RgbImageView& reference) {
        const auto clean=score(reference);
        if(!clean.glyph_structure_candidate)throw std::invalid_argument("shared IN reference requires seven score glyphs");
        score_top_=720;score_bottom_=0;score_ink_width_=0;
        for(const auto& g:clean.glyphs) {
            score_top_=std::min(score_top_,g.bounds.y);
            score_bottom_=std::max(score_bottom_,g.bounds.y+g.bounds.height);
            score_ink_width_=std::max(score_ink_width_,g.bounds.width);
        }
        score_height_=score_bottom_-score_top_;
        score_right_=clean.glyphs.back().bounds.x+clean.glyphs.back().bounds.width;
        const auto top=std::max(score_region.y,score_top_-4);
        const auto bottom=std::min(score_region.y+score_region.height,score_bottom_+4);
        score_band_={score_region.x,top,score_region.width,bottom-top};
    }
    HudScoreMetrics score_cells(const RgbImageView& image) const {
        // Keep the full-ROI component budget. The reference fixes the font
        // baseline and right anchor, not pitch: actual score digits have
        // proportional advances. Only observed, separated glyphs form cells.
        auto out=score(image);out.calibrated_cells=true;
        if(!out.bounded)return out;
        const auto band=score(image,score_band_);
        if(!band.bounded){out.bounded=false;out.glyph_structure_candidate=false;return out;}
        out.glyphs.clear();out.glyph_structure_candidate=out.placement_consistent=false;
        for(const auto& g:band.glyphs) {
            if(g.bounds.height<score_height_-6||g.bounds.height>score_height_+6||
               std::abs(g.bounds.y-score_top_)>3||
               std::abs(g.bounds.y+g.bounds.height-score_bottom_)>3)continue;
            out.glyphs.push_back(g);
        }
        if(out.glyphs.size()!=7)return out;
        const int max_width=std::min(24,score_ink_width_+6);
        for(std::size_t i=0;i<out.glyphs.size();++i) {
            auto& g=out.glyphs[i];auto window=g.bounds;
            if(window.x==score_region.x) {
                // One bar may enter from the left and touch the first digit.
                // Never split a merged multi-digit blob or invent a missing
                // cell: all seven baseline components must already exist.
                if(i||window.width>2*max_width+8)return out;
                const int retain=std::min(window.width,score_ink_width_+4);
                out.original_left_bounds=window;
                window.x+=window.width-retain;window.width=retain;
            } else if(window.width>max_width||window.x+window.width>=score_region.x+score_region.width)return out;
            int x0=1280,y0=720,x1=-1,y1=-1;
            for(int y=window.y;y<window.y+window.height;++y)for(int x=window.x;x<window.x+window.width;++x)
                if(white(pixel(image,x,y))) {x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);}
            if(x1<x0||y1<y0)return out;
            g.bounds={x0,y0,x1-x0+1,y1-y0+1};g.white_pixels=g.negative_pixels=0;
            std::size_t top_support=0,bottom_support=0;double white_sum=0,dark_sum=0;
            for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x) {
                const auto* value=pixel(image,x,y);
                if(white(value)) {++g.white_pixels;white_sum+=gray(value);if(y<=y0+2)++top_support;if(y>=y1-2)++bottom_support;}
                else if(gray(value)<128) {++g.negative_pixels;dark_sum+=gray(value);}
            }
            const auto area=static_cast<std::size_t>(g.bounds.width*g.bounds.height);
            g.occupancy=static_cast<double>(g.white_pixels)/static_cast<double>(area);
            const double contrast=g.white_pixels&&g.negative_pixels?
                white_sum/static_cast<double>(g.white_pixels)-dark_sum/static_cast<double>(g.negative_pixels):0;
            g.plausible_stroke=g.bounds.height>=score_height_-6&&g.bounds.height<=score_height_+6&&
                std::abs(y0-score_top_)<=3&&std::abs(y1+1-score_bottom_)<=3&&
                g.bounds.width>=3&&g.bounds.width<=max_width&&g.white_pixels>=12&&
                g.occupancy>=.12&&g.occupancy<=.85&&g.negative_pixels>=8&&contrast>=40&&
                top_support>=2&&bottom_support>=2;
            if(!g.plausible_stroke)return out;
            if(i) {
                const auto& prev=out.glyphs[i-1].bounds;const int gap=g.bounds.x-prev.x-prev.width;
                if(gap<1||gap>14)return out;
            } else if(out.original_left_bounds) {
                out.retained_left_bounds=g.bounds;
                out.trimmed_left_pixels=g.bounds.x-out.original_left_bounds->x;
            }
            ++out.supported_cells;
        }
        const auto& last=out.glyphs.back().bounds;
        if(std::abs(last.x+last.width-score_right_)>3)return out;
        out.placement_consistent=out.glyph_structure_candidate=true;
        return out;
    }
public:
    // Default: explicit per-reference title bounds, at most 16,384 source pixels.
    // Opt-in shared policy: actual Phigros 4.0.1 IN pause/prefix plus seven score
    // glyphs; no title/level/chart identity. Only Dlyrotz/Glaciaxion samples have
    // been checked. The owner still binds manual selection and real receipts.
    HudGlyphEvidenceProducer(const RgbImageView& reference,std::string reference_sha256,std::string calibration_id,
                             PixelRegion title_region=PixelRegion{25,672,105,29},
                             HudGlyphPolicy policy=HudGlyphPolicy::calibrated_title_v1)
        :reference_sha256_(std::move(reference_sha256)),calibration_id_(std::move(calibration_id)),title_region_(title_region),policy_(policy) {
        if(!image_valid(reference)||!sha(reference_sha256_)||!text(calibration_id_)||
           (policy_!=HudGlyphPolicy::calibrated_title_v1&&policy_!=HudGlyphPolicy::shared_in_prefix_score_v1&&
            policy_!=HudGlyphPolicy::shared_in_cell_score_v2&&policy_!=HudGlyphPolicy::shared_in_subpixel_prefix_score_v3)||
           (policy_==HudGlyphPolicy::calibrated_title_v1&&!title_region_valid(title_region_)))
            throw std::invalid_argument("invalid HUD diagnostic reference");
        masks_[0]=make_mask(reference,fixed_regions[0]);
        masks_[1]=make_mask(reference,shared()?shared_in_prefix_region:fixed_regions[1]);
        if(policy_==HudGlyphPolicy::calibrated_title_v1)masks_[2]=make_mask(reference,title_region_);
        else if(cell_score())calibrate_score_cells(reference);
        else if(!score(reference).glyph_structure_candidate)
            throw std::invalid_argument("shared IN reference requires seven score glyphs");
        if(policy_==HudGlyphPolicy::shared_in_subpixel_prefix_score_v3)calibrate_literal_prefix();
    }
    PixelRegion title_region() const noexcept {return title_region_;}
    HudGlyphPolicy policy() const noexcept {return policy_;}
    // Pure pixel classification; no per-frame hash or timing provenance is
    // constructed. An experiment owner must associate the returned features
    // with its actual source, frame and receipt before making a state decision.
    HudGlyphEvidence observe_features(const RgbImageView& image,
                             ObservedGameState same_frame_calibrated_state=ObservedGameState::unknown) const {
        HudGlyphEvidence out;out.reference_sha256=reference_sha256_;out.calibration_id=calibration_id_;
        out.policy=policy_;out.title_required=policy_==HudGlyphPolicy::calibrated_title_v1;
        if(!image_valid(image))return out;
        out.valid=true;
        out.denied_state_match=same_frame_calibrated_state==ObservedGameState::paused||same_frame_calibrated_state==ObservedGameState::result;
        out.fixed_glyphs[0]=measure(image,masks_[0]);
        if(policy_==HudGlyphPolicy::calibrated_title_v1) {
            out.fixed_glyphs[1]=measure(image,masks_[1]);out.title_glyph=measure(image,masks_[2]);
        } else {
            // A bounded translation handles the width of the following numeric
            // level. No resize/brightness normalization or title inference.
            for(int dx=-shared_in_max_alignment;dx<=shared_in_max_alignment;++dx) {
                const auto measured=measure(image,masks_[1],dx);
                const bool matched=measured.glyph_present&&measured.negative_space_present;
                const bool first_match=matched&&!out.difficulty_alignment_candidates;
                if(matched)++out.difficulty_alignment_candidates;
                if(first_match||((matched||!out.difficulty_alignment_candidates)&&
                   measured.foreground_mean_error<out.fixed_glyphs[1].foreground_mean_error)) {
                    out.fixed_glyphs[1]=measured;out.difficulty_alignment_x=dx;
                }
            }
        }
        bool unique_alignment=out.title_required||out.difficulty_alignment_candidates==1;
        out.difficulty_alignment_lower_x=out.difficulty_alignment_upper_x=out.difficulty_alignment_x;
        if(policy_==HudGlyphPolicy::shared_in_subpixel_prefix_score_v3&&out.difficulty_alignment_candidates==0) {
            // Preserve the exact v2 acceptance path. Subpixel fallback retains
            // all valid offsets, admitting only one contiguous <=2px family.
            int lower=0,upper=0;
            for(int dx=-shared_in_max_alignment;dx<=shared_in_max_alignment;++dx) {
                const auto measured=measure_literal_prefix(image,dx);
                const bool matched=measured.glyph_present&&measured.negative_space_present;
                const bool first=matched&&!out.difficulty_alignment_candidates;
                if(matched){if(first)lower=dx;upper=dx;++out.difficulty_alignment_candidates;}
                if(first||((matched||!out.difficulty_alignment_candidates)&&measured.foreground_mean_error<out.fixed_glyphs[1].foreground_mean_error)) {
                    out.fixed_glyphs[1]=measured;out.difficulty_alignment_x=dx;
                }
            }
            out.difficulty_alignment_lower_x=lower;out.difficulty_alignment_upper_x=upper;
            unique_alignment=out.difficulty_alignment_candidates>0&&upper-lower<=2&&
                out.difficulty_alignment_candidates==static_cast<std::size_t>(upper-lower+1);
        }
        out.difficulty_alignment_unique_family=unique_alignment;
        out.score=cell_score()?score_cells(image):score(image);
        const bool pair=std::all_of(out.fixed_glyphs.begin(),out.fixed_glyphs.end(),[](const auto& m){return m.glyph_present&&m.negative_space_present;});
        // The default calibrated title supplies the third fixed glyph.
        // Its score shape is diagnostic: crossing lines can connect
        // legitimate digits outside its ROI. It must not veto all such HUDs or
        // infer score/identity/AP. The explicit shared policy instead requires
        // that score structure and conservatively rejects those crossings.
        const bool title=!out.title_required||(out.title_glyph.glyph_present&&out.title_glyph.negative_space_present);
        const bool score_allowed=out.title_required?out.score.bounded:out.score.glyph_structure_candidate;
        out.hud_candidate=!out.denied_state_match&&pair&&title&&unique_alignment&&score_allowed;
        out.reason=out.denied_state_match?"calibrated_denied_state_has_priority":!out.score.bounded?"score_component_budget":
            !pair?"fixed_glyph_or_negative_space_missing":!title?"title_glyph_or_negative_space_missing":
            !unique_alignment?"shared_IN_prefix_alignment_ambiguous":!score_allowed?"score_digit_structure_missing":
            out.title_required?"experimental_HUD_feature_candidate_only":
            policy_==HudGlyphPolicy::shared_in_subpixel_prefix_score_v3?"experimental_shared_IN_subpixel_HUD_candidate_only":
            policy_==HudGlyphPolicy::shared_in_cell_score_v2?"experimental_shared_IN_cell_HUD_candidate_only":"experimental_shared_IN_HUD_candidate_only";
        return out;
    }
    HudGlyphEvidence observe(const RgbImageView& image,const HudFrameMetadata& frame,
                             ObservedGameState same_frame_calibrated_state=ObservedGameState::unknown) const {
        if(!metadata_valid(frame)) {
            HudGlyphEvidence out;out.reference_sha256=reference_sha256_;out.calibration_id=calibration_id_;
            out.policy=policy_;out.title_required=policy_==HudGlyphPolicy::calibrated_title_v1;return out;
        }
        auto out=observe_features(image,same_frame_calibrated_state);
        if(out.valid) {out.frame=frame;out.metadata_attached=true;}
        return out;
    }
};
} // namespace rhythm::experimental
