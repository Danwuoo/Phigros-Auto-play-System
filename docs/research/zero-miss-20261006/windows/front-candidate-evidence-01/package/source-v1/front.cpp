#include "front.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace pas::hold_front {
using J=nlohmann::json;
std::string_view name(Kind k) {
    switch(k){case Kind::unknown:return "unknown";case Kind::visible_terminal:return "visible_terminal_proposed_head";
    case Kind::body_interior:return "body_interior";case Kind::clipped:return "clip_unknown";
    case Kind::unsupported:return "unsupported";}return "unknown";
}
std::string_view name(Reason r) {
    switch(r){case Reason::supported:return "current_rgb_terminal_and_rails";
    case Reason::frame_invalid:return "frame_storage_or_context_invalid";case Reason::context_mismatch:return "same_frame_context_required";
    case Reason::capacity:return "complete_batch_capacity";case Reason::geometry:return "finite_bounded_roi_required";
    case Reason::body_not_head:return "body_interior_cannot_be_head_or_new_down";
    case Reason::unsupported_kind:return "hold_only";case Reason::rejected_proposal:return "rejected_or_invisible_proposal";
    case Reason::normal_unknown:return "tail_missing_or_incompatible_normal_unknown";
    case Reason::roi_clipped:return "search_or_support_clipped";case Reason::no_terminal:return "no_supported_terminal";
    case Reason::multiple_terminals:return "multiple_supported_terminals_unknown";
    case Reason::seam_mismatch:return "typed_current_proof_mismatch";}return "unknown";
}
namespace {
SceneContext context(const Frame& f){return {f.epoch,f.generation,f.geometry_version,f.sequence,
    f.capture_complete_ns,f.width,f.height,f.source_rotation};}
bool frame_ok(const Frame& f) {
    return f.source_valid&&f.epoch&&f.generation&&f.geometry_version&&f.sequence&&
        f.width>0&&f.width<=1280&&f.height>0&&f.height<=720&&f.stride>=f.width*3&&f.stride<=3840&&
        f.source_rotation>=0&&f.source_rotation<=3&&f.capture_complete_ns>=0&&
        f.capture_complete_ns<=f.pixels_ready_ns&&f.rgb.size()==std::size_t(f.stride)*f.height;
}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool white(const Pixel& p){return p.rgb[0]>=240&&p.rgb[1]>=240&&p.rgb[2]>=240;}
struct Row {std::array<Pixel,lane_count> inner{};bool left=false,right=false;};
// Positive intensity discontinuity, independent of the frozen BVI blue test.
int contrast(const Pixel& a,const Pixel& b) {
    int sum=0,largest=0;
    for(int k=0;k<3;++k){const int d=int(a.rgb[k])-int(b.rgb[k]);sum+=d;largest=std::max(largest,d);}
    return sum>=60&&largest>=40?largest:0;
}
}
Result measure(const Frame& f,const SceneContext& expected,const TrackingCandidate& c) {
    Result out;out.context=context(f);out.pixels_ready_ns=f.pixels_ready_ns;out.candidate_id=c.candidate_id;
    const auto& n=c.note;out.observer_front=n.center;out.tangent=n.tangent;out.width=n.width;
    auto reject=[&](Reason r,Kind k=Kind::unknown){out.reason=r;out.kind=k;return out;};
    if(!frame_ok(f))return reject(Reason::frame_invalid);
    if(expected!=out.context)return reject(Reason::context_mismatch);
    if(c.origin.size()>64||!c.candidate_id)return reject(Reason::geometry);
    std::copy(c.origin.begin(),c.origin.end(),out.origin.begin());
    if(n.held_body_patch)return reject(Reason::body_not_head,Kind::body_interior);
    if(n.kind!=NoteKind::hold)return reject(Reason::unsupported_kind,Kind::unsupported);
    if(!c.head_visible||c.quality==ObservationQuality::rejected)return reject(Reason::rejected_proposal);
    if(!finite(n.center)||!finite(n.tangent)||!std::isfinite(n.width)||n.width<16||n.width>512||
        n.center.x<0||n.center.x>=f.width||n.center.y<0||n.center.y>=f.height||
        std::abs(std::hypot(n.tangent.x,n.tangent.y)-1)>.001)return reject(Reason::geometry);
    if(!n.tail||!finite(*n.tail))return reject(Reason::normal_unknown);
    Vec2 v{-n.tangent.y,n.tangent.x};const Vec2 delta{n.center.x-n.tail->x,n.center.y-n.tail->y};
    double depth=delta.x*v.x+delta.y*v.y;
    if(!std::isfinite(depth)||std::abs(depth)<1||std::abs(depth)>4095||
        std::abs(delta.x*n.tangent.x+delta.y*n.tangent.y)>2)return reject(Reason::normal_unknown);
    if(depth<0){v.x=-v.x;v.y=-v.y;}
    out.normal=v;out.current_tail_roi=true;
    // Every pixel coordinate is checked BEFORE rounding and indexing. The
    // search is bounded and all support must exist; clipping never invents end.
    auto pixel=[&](double along,int offset,Pixel& p) {
        const double x=n.center.x+along*n.tangent.x+offset*v.x,y=n.center.y+along*n.tangent.y+offset*v.y;
        if(!std::isfinite(x)||!std::isfinite(y)||x<-.49||x>f.width-.51||y<-.49||y>f.height-.51)return false;
        p.x=int(std::lround(x));p.y=int(std::lround(y));
        if(p.x<0||p.x>=f.width||p.y<0||p.y>=f.height)return false;
        const auto index=std::size_t(p.y)*f.stride+std::size_t(p.x)*3;
        std::copy_n(f.rgb.begin()+index,3,p.rgb.begin());++out.probes;return true;
    };
    constexpr std::array<double,lane_count> fractions{-.30,-.15,0,.15,.30};
    std::array<Row,2*radius+1> rows{};
    bool complete=true;
    for(int s=-radius;s<=radius;++s) {
        auto& row=rows[std::size_t(s+radius)];
        for(int k=0;k<lane_count;++k)complete=pixel(fractions[k]*n.width,s,row.inner[k])&&complete;
        for(int sign:{-1,1})for(int d=-2;d<=2;++d) {
            Pixel p;const bool ok=pixel(sign*n.width/2+d,s,p);complete=ok&&complete;
            if(ok&&white(p)){if(sign<0)row.left=true;else row.right=true;}
        }
    }
    if(!complete)return reject(Reason::roi_clipped,Kind::clipped);
    struct Edge {int offset=0,left=0,right=0,score=0;std::array<int,lane_count> last{};};
    std::array<Edge,2*edge_radius+1> edges{};int count=0;
    for(int s=-edge_radius;s<=edge_radius;++s) {
        Edge e;e.offset=s;bool supported=true;
        for(int k=0;k<lane_count;++k) {
            int best=0,last=s;
            for(int d=-1;d<=1;++d){const auto at=std::size_t(s+d+radius);
                const auto score=contrast(rows[at].inner[k],rows[at+1].inner[k]);
                if(score>best){best=score;last=s+d;}}
            if(!best){supported=false;break;}e.last[k]=last;e.score+=best;
        }
        if(!supported)continue;
        for(int d=4;d<=12;++d){const auto& row=rows[std::size_t(s-d+radius)];e.left+=row.left;e.right+=row.right;}
        int exterior_rails=0;for(int d=2;d<=4;++d){const auto& row=rows[std::size_t(s+d+radius)];exterior_rails+=row.left||row.right;}
        if(e.left<6||e.right<6||exterior_rails>1)continue;
        edges[std::size_t(count++)]=e;
    }
    if(!count)return reject(Reason::no_terminal);
    int clusters=1;for(int i=1;i<count;++i)if(edges[i].offset-edges[i-1].offset>3)++clusters;
    out.terminal_clusters=clusters;
    if(clusters!=1)return reject(Reason::multiple_terminals);
    const auto best=std::max_element(edges.begin(),edges.begin()+count,[](const Edge& a,const Edge& b){return a.score<b.score;});
    auto sorted=best->last;std::sort(sorted.begin(),sorted.end());
    out.offset_low=sorted.front();out.offset_high=sorted.back()+1.;
    const double mid=sorted[lane_count/2]+.5;
    out.boundary=Vec2{n.center.x+mid*v.x,n.center.y+mid*v.y};
    out.left_rail_rows=best->left;out.right_rail_rows=best->right;
    out.support_anchor_offset=best->offset;
    for(int k=0;k<lane_count;++k){const int last=best->last[k];out.lanes[k]={fractions[k]*n.width,last,
        rows[std::size_t(last+radius)].inner[k],rows[std::size_t(last+radius+1)].inner[k]};}
    return reject(Reason::supported,Kind::visible_terminal);
}
Batch produce(const Frame& f,const CandidateBatch& b) {
    Batch out;out.context=context(f);
    if(!frame_ok(f))return out;
    if(b.context!=out.context||!b.source_valid){out.reason=Reason::context_mismatch;return out;}
    if(!b.capacity_valid||b.candidates.size()>candidate_cap||b.lines.size()>16){out.reason=Reason::capacity;return out;}
    out.valid=true;out.reason=Reason::supported;
    for(const auto& c:b.candidates){auto r=measure(f,b.context,c);out.probes+=r.probes;out.results[out.count++]=r;}
    return out;
}
bool equivalent(const Result& a,const Result& b) {
    return a.context==b.context&&a.pixels_ready_ns==b.pixels_ready_ns&&a.candidate_id==b.candidate_id&&
        a.origin==b.origin&&same(a.observer_front,b.observer_front)&&same(a.normal,b.normal)&&same(a.tangent,b.tangent)&&
        a.width==b.width&&a.offset_low==b.offset_low&&a.offset_high==b.offset_high&&bool(a.boundary)==bool(b.boundary)&&
        (!a.boundary||same(*a.boundary,*b.boundary))&&a.kind==b.kind&&a.reason==b.reason&&a.current_tail_roi==b.current_tail_roi&&
        a.probes==b.probes&&a.left_rail_rows==b.left_rail_rows&&a.right_rail_rows==b.right_rail_rows&&
        a.terminal_clusters==b.terminal_clusters&&a.support_anchor_offset==b.support_anchor_offset&&a.lanes==b.lanes;
}
std::optional<Projection> consume(const Frame& f,const SceneContext& ctx,const TrackingCandidate& c,const Result& r) {
    if(r.kind!=Kind::visible_terminal||r.reason!=Reason::supported||!r.boundary)return std::nullopt;
    const auto verified=measure(f,ctx,c);
    if(!equivalent(r,verified))return std::nullopt;
    return Projection{*r.boundary,r.offset_low,r.offset_high,false,false};
}
J encode(const Result& r) {
    J lanes=J::array();auto pixel=[](const Pixel& p){return J{{"pixel",{p.x,p.y}},{"rgb",p.rgb}};};
    if(r.boundary)for(const auto& l:r.lanes)lanes.push_back({{"along",l.along},{"last_inside_offset",l.last_inside_offset},
        {"inside",pixel(l.inside)},{"outside",pixel(l.outside)}});
    return {{"candidate_id",r.candidate_id},{"origin",r.origin.data()},{"observer_front",{r.observer_front.x,r.observer_front.y}},
        {"context",{{"epoch",r.context.epoch},{"generation",r.context.generation},{"geometry",r.context.geometry},
            {"frame",r.context.frame},{"capture_ns",r.context.capture_ns},{"width",r.context.width},{"height",r.context.height},
            {"rotation",r.context.rotation}}},{"pixels_ready_ns",r.pixels_ready_ns},{"kind",name(r.kind)},{"reason",name(r.reason)},
        {"normal",{r.normal.x,r.normal.y}},{"tangent",{r.tangent.x,r.tangent.y}},{"width",r.width},
        {"boundary",r.boundary?J::array({r.boundary->x,r.boundary->y}):J(nullptr)},
        {"offset_interval",r.boundary?J::array({r.offset_low,r.offset_high}):J(nullptr)},
        {"left_rail_rows",r.left_rail_rows},{"right_rail_rows",r.right_rail_rows},{"terminal_clusters",r.terminal_clusters},
        {"probes",r.probes},{"support_anchor_offset",r.support_anchor_offset},{"edge_samples",lanes},{"tail_role","unknown; current tail proposal or missing; not measured here"},
        {"current_tail_roi",r.current_tail_roi},{"head_role",r.boundary?"proposed":"unknown"},{"action_authorized",false}};
}
} // namespace pas::hold_front
