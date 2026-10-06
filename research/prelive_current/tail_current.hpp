#pragma once
#include "front.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
// Line-aware current tail measurement; immutable front donor remains unchanged.
namespace pas::current_tail {
using namespace pas::hold_front;
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
inline Result measure(const Frame& f,const CandidateBatch& batch,const TrackingCandidate& c) {
    const auto& expected=batch.context;
    Result out;out.context=context(f);out.pixels_ready_ns=f.pixels_ready_ns;out.candidate_id=c.candidate_id;
    const auto& n=c.note;out.observer_front=n.center;out.tangent=n.tangent;out.width=n.width;
    auto reject=[&](Reason r,Kind k=Kind::unknown){out.reason=r;out.kind=k;return out;};
    if(!frame_ok(f))return reject(Reason::frame_invalid);
    if(expected!=out.context)return reject(Reason::context_mismatch);
    if(batch.lines.size()>16||!batch.capacity_valid||!batch.source_valid)return reject(Reason::capacity);
    for(const auto& line:batch.lines)if(line.observed_ns!=f.capture_complete_ns||!finite(line.center)||!finite(line.tangent)||!std::isfinite(line.length)||line.length<=0||std::abs(std::hypot(line.tangent.x,line.tangent.y)-1)>.001)return reject(Reason::geometry);
    const auto masked_white=[&](const Pixel& p){if(!white(p))return false;for(const auto& line:batch.lines){const double dx=p.x-line.center.x,dy=p.y-line.center.y;if(std::abs(-dx*line.tangent.y+dy*line.tangent.x)<=2.8&&std::abs(dx*line.tangent.x+dy*line.tangent.y)<=line.length/2)return true;}return false;};
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
            if(ok&&white(p)&&!masked_white(p)){if(sign<0)row.left=true;else row.right=true;}
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
                if(masked_white(rows[at].inner[k])||masked_white(rows[at+1].inner[k]))continue;
                const auto score=contrast(rows[at].inner[k],rows[at+1].inner[k]);
                if(score>best){best=score;last=s+d;}}
            if(!best){supported=false;break;}e.last[k]=last;e.score+=best;
        }
        if(!supported)continue;
        int connected_fill=0;
        for(int d=4;d<=12;++d){const auto& row=rows[std::size_t(s-d+radius)];e.left+=row.left;e.right+=row.right;
            const bool fill=std::all_of(row.inner.begin(),row.inner.end(),[](const Pixel& p){
                return p.rgb[1]>=130&&p.rgb[2]>=180&&int(p.rgb[1])-p.rgb[0]>=12&&int(p.rgb[2])-p.rgb[1]>=6;
            });
            connected_fill+=row.left&&row.right&&fill;
        }
        int exterior_rails=0;for(int d=2;d<=4;++d){const auto& row=rows[std::size_t(s+d+radius)];exterior_rails+=row.left||row.right;}
        if(e.left<6||e.right<6||connected_fill<6||exterior_rails>1)continue;
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
} // namespace pas::current_tail
