#include "pas/game.hpp"
#include "pas/game_tracking.hpp"
#include "pas/game_motion.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace pas {
const char* name(GameUi v) {
    switch(v) {
    case GameUi::menu: return "MENU"; case GameUi::loading: return "LOADING";
    case GameUi::playing: return "PLAYING"; case GameUi::paused: return "PAUSED";
    case GameUi::result: return "RESULT"; default: return "UNKNOWN";
    }
}
const char* name(NoteKind v) {
    switch(v) {
    case NoteKind::tap: return "tap"; case NoteKind::hold: return "hold";
    case NoteKind::drag: return "drag"; case NoteKind::flick: return "flick";
    default: return "ambiguous";
    }
}
namespace {
constexpr Nanoseconds minimum_fit_span_ns=30'000'000;
constexpr Nanoseconds history_bucket_ns=10'000'000;
struct Component {
    int color = 0, count = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double sx = 0, sy = 0, xx = 0, yy = 0, xy = 0;
    Vec2 ribbon_center{};
    double ribbon_width=0,ribbon_height=0;
};
int classify(const std::uint8_t* p) {
    const int r=p[0], g=p[1], b=p[2];
    if (b > 165 && g > 115 && b > r + 35) return 1;
    if (r > 180 && g > 160 && b < g - 55) return 2;
    if (r > 190 && r > g + 45 && b > 75) return 3;
    if (r > 195 && g > 195 && b > 195 && std::max({r,g,b})-std::min({r,g,b}) < 35) return 4;
    return 0;
}
std::vector<Component> components(const Frame& f, bool& capacity) {
    constexpr int scale=2;
    const int w=(f.width+1)/2, h=(f.height+1)/2;
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(w)*h);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x)
        mask[static_cast<std::size_t>(y)*w+x]=static_cast<std::uint8_t>(classify(
            f.rgb.data()+static_cast<std::size_t>(y*scale)*f.stride+x*scale*3));
    std::vector<int> queue; queue.reserve(mask.size());
    std::vector<Component> output;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const int index=y*w+x, color=mask[index];
        if(!color) continue;
        mask[index]=0; queue.clear(); queue.push_back(index);
        Component c; c.color=color; c.x0=c.x1=x*scale; c.y0=c.y1=y*scale;
        for(std::size_t i=0;i<queue.size();++i) {
            const int q=queue[i], qx=q%w, qy=q/w, px=qx*scale, py=qy*scale;
            ++c.count; c.sx+=px; c.sy+=py; c.xx+=px*px; c.yy+=py*py; c.xy+=px*py;
            c.x0=std::min(c.x0,px); c.x1=std::max(c.x1,px);
            c.y0=std::min(c.y0,py); c.y1=std::max(c.y1,py);
            for(const auto [dx,dy] : {std::pair{-1,0},{1,0},{0,-1},{0,1}}) {
                const int nx=qx+dx, ny=qy+dy;
                if(nx<0||ny<0||nx>=w||ny>=h) continue;
                const int ni=ny*w+nx;
                if(mask[ni]==color) {mask[ni]=0; queue.push_back(ni);}
            }
        }
        if(c.count<3) continue;
        if(c.color==2) {
            const double cx=c.sx/c.count,cy=c.sy/c.count;
            const double xx=c.xx/c.count-cx*cx,yy=c.yy/c.count-cy*cy,xy=c.xy/c.count-cx*cy;
            const double theta=.5*std::atan2(2*xy,xx-yy),spread=std::hypot(xx-yy,2*xy);
            const double major=std::sqrt(std::max(0.0,(xx+yy+spread)/2)),
                         minor=std::sqrt(std::max(0.0,(xx+yy-spread)/2));
            if(major>5*std::max(1.0,minor)&&major<f.width*.08&&std::abs(std::sin(theta))>.2) {
                const Vec2 u{std::cos(theta),std::sin(theta)},n{-u.y,u.x};
                double s0=1e9,s1=-1e9,d0=1e9,d1=-1e9;
                // Use only this connected component's measured pixels.
                // Variance assumes uniform fill and inflates U-shaped
                // highlights; actual projected extrema preserve their caps.
                for(const int q:queue) {
                    const double px=(q%w)*scale,py=(q/w)*scale;
                    const double s=px*u.x+py*u.y,d=px*n.x+py*n.y;
                    s0=std::min(s0,s);s1=std::max(s1,s);d0=std::min(d0,d);d1=std::max(d1,d);
                }
                c.ribbon_center={u.x*(s0+s1)/2+n.x*(d0+d1)/2,u.y*(s0+s1)/2+n.y*(d0+d1)/2};
                c.ribbon_width=s1-s0+2;c.ribbon_height=d1-d0+2;
            }
        }
        if(output.size()==2048) {capacity=false; return output;}
        output.push_back(c);
    }
    return output;
}
int combo_digit_shapes(const Frame& f) {
    // Clip before segmentation: a decorative rail may connect to a digit
    // outside this region. This is a diagnostic shape count, not OCR.
    constexpr int scale=2;
    const int x0=static_cast<int>(f.width*.43),y0=static_cast<int>(f.height*.017),
        w=static_cast<int>(f.width*.14)/scale,h=static_cast<int>(f.height*.066)/scale;
    if(w<=0||h<=0||static_cast<std::size_t>(w)*h>65'536) return 0;
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(w)*h);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) mask[static_cast<std::size_t>(y)*w+x]=
        classify(f.rgb.data()+static_cast<std::size_t>(y0+y*scale)*f.stride+(x0+x*scale)*3)==4;
    std::vector<int> queue;queue.reserve(mask.size());int count=0;
    std::vector<int> column_ink(w,0),touched_columns;touched_columns.reserve(w);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const int index=y*w+x;if(!mask[index]) continue;
        queue.clear();touched_columns.clear();queue.push_back(index);mask[index]=0;
        int left=x,right=x,top=y,bottom=y;
        for(std::size_t i=0;i<queue.size();++i) {
            const int px=queue[i]%w,py=queue[i]/w;
            if(column_ink[px]++==0) touched_columns.push_back(px);
            left=std::min(left,px);right=std::max(right,px);
            top=std::min(top,py);bottom=std::max(bottom,py);
            for(const auto [dx,dy]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}) {
                const int nx=px+dx,ny=py+dy;
                if(nx<0||ny<0||nx>=w||ny>=h) continue;
                const int next=ny*w+nx;
                if(mask[next]) {mask[next]=0;queue.push_back(next);}
            }
        }
        const int width=(right-left+1)*scale,height=(bottom-top+1)*scale;
        int vertical_ink_width=0;
        for(const int column:touched_columns) {
            if(column_ink[column]*scale>=f.height*.025) vertical_ink_width+=scale;
            column_ink[column]=0;
        }
        // A thin white ribbon can join several digits into a wide cluster.
        // Require localized tall ink columns before keeping such a cluster;
        // a horizontal border crossed by one narrow rail is insufficient.
        if(height>=f.height*.025&&width>=f.width*.005&&
           (width<=f.width*.05||vertical_ink_width>=f.width*.0045))
            count=std::min(16,count+1);
    }
    return count;
}
double distance(Vec2 a, Vec2 b) {return std::hypot(a.x-b.x,a.y-b.y);}
double normal_distance(Vec2 p, const LineCandidate& l) {
    return (p.x-l.center.x)*(-l.tangent.y)+(p.y-l.center.y)*l.tangent.x;
}
bool same_visual_geometry(const SceneContext& a,const SceneContext& b) {
    return a.generation==b.generation&&a.geometry==b.geometry&&
        a.width==b.width&&a.height==b.height&&a.rotation==b.rotation;
}
bool same_geometry(const SceneContext& a,const SceneContext& b) {
    return a.epoch==b.epoch&&same_visual_geometry(a,b);
}
struct HoldRails {double depth=0;std::optional<Vec2> tail;};
std::optional<HoldRails> visible_hold_rails(const Frame& f,const NoteCandidate& note,double minimum_depth,
                                          int maximum_gap=12,int lateral_radius=16,bool bound_by_fill=false,
                                          bool allow_yellow=false,bool allow_warm_tint=false) {
    const Vec2 normal{-note.tangent.y,note.tangent.x};
    std::array<int,2> extent{};
    for(int side=0;side<2;++side) {
        int last=-1,gaps=0,neutral_support=0;
        for(int depth=0;depth<std::hypot(f.width,f.height);depth+=2) {
            if(bound_by_fill&&depth>=minimum_depth) {
                int support=0;
                for(int k=-3;k<=3;++k) {
                    const int x=static_cast<int>(std::lround(note.center.x-normal.x*depth+k*note.width*.115*note.tangent.x)),
                              y=static_cast<int>(std::lround(note.center.y-normal.y*depth+k*note.width*.115*note.tangent.y));
                    if(x<0||x>=f.width||y<f.height*.10||y>=f.height) continue;
                    const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
                    if((p[2]>110&&p[1]>100&&p[2]>p[0]+5)||classify(p)==2) ++support;
                }
                // A rail crossing an unrelated white decoration must end
                // with the body fill, rather than follow that decoration.
                if(support<5) break;
            }
            const double cx=note.center.x-normal.x*depth+(side?1:-1)*note.width*.5*note.tangent.x;
            const double cy=note.center.y-normal.y*depth+(side?1:-1)*note.width*.5*note.tangent.y;
            if(cy<f.height*.10||cy>=f.height||cx<0||cx>=f.width) break;
            bool found=false,neutral=false;
            for(int offset=-lateral_radius;offset<=lateral_radius&&!found;offset+=2) {
                const int x=static_cast<int>(std::lround(cx+offset*note.tangent.x)),
                          y=static_cast<int>(std::lround(cy+offset*note.tangent.y));
                if(x<0||y<0||x>=f.width||y>=f.height) continue;
                const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
                neutral=std::min({p[0],p[1],p[2]})>170 &&
                    std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]})<30;
                // A translucent hit effect tints an established rail warm
                // without making it saturated yellow. Keep this narrow
                // corridor exclusive to current-fill-validated held bodies.
                const bool warm=allow_warm_tint&&depth<=96&&std::abs(offset)<=4&&p[0]>180&&p[1]>160&&
                    p[2]>125&&p[0]>=p[1]&&p[0]-p[1]<35&&p[1]-p[2]>25&&p[0]-p[2]<=110;
                found=neutral||(allow_yellow&&classify(p)==2)||warm;
            }
            if(neutral&&depth>=16) ++neutral_support;
            if(found) {last=depth;gaps=0;}
            else if((last>=0&&(gaps+=2)>maximum_gap)||(last<0&&depth>=maximum_gap)) break;
        }
        // Both rails must reconnect to several actual white samples in this
        // frame. Warm outlines by themselves cannot extend an old identity.
        if(allow_warm_tint&&neutral_support<3) return {};
        extent[side]=last;
    }
    if(std::min(extent[0],extent[1])<minimum_depth||std::abs(extent[0]-extent[1])>20) return {};
    const double depth=(extent[0]+extent[1])/2.0;
    Vec2 tail{note.center.x-normal.x*depth,note.center.y-normal.y*depth};
    return HoldRails{depth,tail.y>f.height*.11?std::optional<Vec2>{tail}:std::nullopt};
}
std::optional<Vec2> visible_hold_tail(const Frame& f,const NoteCandidate& note) {
    const auto rails=visible_hold_rails(f,note,note.width*.65);
    return rails?rails->tail:std::nullopt;
}
bool hold_fill_near_head(const Frame& f,const NoteCandidate& note) {
    const Vec2 normal{-note.tangent.y,note.tangent.x};
    int support=0;
    for(const int depth:{4,8,12}) for(const double fraction:{-.35,-.175,0.0,.175,.35}) {
        const int x=static_cast<int>(std::lround(note.center.x-normal.x*depth+fraction*note.width*note.tangent.x)),
                  y=static_cast<int>(std::lround(note.center.y-normal.y*depth+fraction*note.width*note.tangent.y));
        if(x<0||y<0||x>=f.width||y>=f.height) continue;
        const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
        if(p[2]>145&&p[1]>130&&p[2]>p[0]+8) ++support;
    }
    return support>=5;
}
bool hold_fill_at_front(const Frame& f,const NoteCandidate& note) {
    const Vec2 normal{-note.tangent.y,note.tangent.x};int support=0;
    for(const double fraction:{-.35,-.175,0.0,.175,.35}) {
        const int x=static_cast<int>(std::lround(note.center.x-normal.x*4+fraction*note.width*note.tangent.x)),
                  y=static_cast<int>(std::lround(note.center.y-normal.y*4+fraction*note.width*note.tangent.y));
        if(x<0||y<0||x>=f.width||y>=f.height) continue;
        const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
        if(p[2]>145&&p[1]>130&&p[2]>p[0]+8) ++support;
    }
    return support>=3;
}
std::optional<NoteCandidate> current_held_rail_section(const Frame& f,const NoteCandidate& note) {
    const Vec2 u=note.tangent,n{-u.y,u.x};
    std::optional<NoteCandidate> best;double best_cost=1e9;
    // Re-measure neutral rails behind a known body front. This does not
    // search a new head or supply evidence for a missing front.
    for(const int depth:{128,192}) {
        const Vec2 section{note.center.x-n.x*depth,note.center.y-n.y*depth};
        const int radius=static_cast<int>(std::ceil(note.width*.5))+20;
        std::vector<double> bands;bands.reserve(16);std::optional<int> begin;
        const auto pixel=[&](double along)->const std::uint8_t* {
            const int x=static_cast<int>(std::lround(section.x+u.x*along)),
                      y=static_cast<int>(std::lround(section.y+u.y*along));
            if(x<0||x>=f.width||y<f.height*.10||y>=f.height) return nullptr;
            return f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
        };
        for(int offset=-radius;offset<=radius+1;++offset) {
            const auto* p=offset<=radius?pixel(offset):nullptr;
            const bool white=p&&std::min({p[0],p[1],p[2]})>170&&
                std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]})<30;
            if(white) {if(!begin) begin=offset;}
            else if(begin) {
                if(offset-*begin<=12) {
                    if(bands.size()==16) return {};
                    bands.push_back((*begin+offset-1)/2.0);
                }
                begin.reset();
            }
        }
        for(std::size_t a=0;a<bands.size();++a) for(std::size_t b=a+1;b<bands.size();++b) {
            const double width=bands[b]-bands[a],middle=(bands[b]+bands[a])/2;
            if(bands[a]>=0||bands[b]<=0||width<note.width*.8||width>note.width*1.2||std::abs(middle)>16) continue;
            int fill=0;
            for(int k=-3;k<=3;++k) {
                const auto* p=pixel(middle+k*width*.115);
                if(p&&p[2]>145&&p[1]>130&&p[2]>p[0]+8) ++fill;
            }
            if(fill<5) continue;
            const double cost=std::abs(width-note.width)+2*std::abs(middle);
            if(cost>=best_cost) continue;
            auto current=note;current.width=width;
            current.center={note.center.x+u.x*middle,note.center.y+u.y*middle};
            best=current;best_cost=cost;
        }
    }
    return best;
}
bool fill_crosses_hold_front(const Frame& f,const NoteCandidate& note) {
    const Vec2 u=note.tangent,n{-u.y,u.x};
    // A real leading edge ends the fill. An interior effect can interrupt
    // the center while the same narrow blue side bands cross that edge.
    // Require a continuous current band on BOTH sides, rather than nearby
    // centers, white rails alone, or a warm particle in the middle.
    for(const int side:{-1,1}) {
        bool continuous=false;
        for(const double fraction:{.38,.40,.42,.44,.46,.48}) {
            bool supported=true;
            for(const int depth:{-8,-4,0,4,8}) {
                const int x=static_cast<int>(std::lround(note.center.x+side*fraction*note.width*u.x-depth*n.x)),
                          y=static_cast<int>(std::lround(note.center.y+side*fraction*note.width*u.y-depth*n.y));
                if(x<0||x>=f.width||y<0||y>=f.height) {supported=false;break;}
                const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
                if(!(p[2]>145&&p[1]>130&&p[2]>p[0]+8)) {supported=false;break;}
            }
            continuous=continuous||supported;
        }
        if(!continuous) return false;
    }
    return true;
}
// Reconstruct a body from this frame, before tracking. The seed may be only
// one saturated fragment; neither its width nor a past body defines the rails.
std::optional<NoteCandidate> current_hold_body(const Frame& f,const Component& seed,
                                              const LineCandidate& line) {
    const Vec2 u=line.tangent,n{-u.y,u.x};
    const Vec2 head{(seed.x0+seed.x1)/2.0,seed.y1-2.0};
    const auto pixel=[&](Vec2 p)->const std::uint8_t* {
        const int x=static_cast<int>(std::lround(p.x)),y=static_cast<int>(std::lround(p.y));
        if(x<0||x>=f.width||y<f.height*.10||y>=f.height) return nullptr;
        return f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
    };
    const auto fill=[&](Vec2 center,double width,int depth,bool include_occlusion=false) {
        int support=0;bool left=false,right=false;
        for(int k=-4;k<=4;++k) {
            const double along=k*width*.09;
            const auto* p=pixel({center.x-n.x*depth+u.x*along,center.y-n.y*depth+u.y*along});
            if(p&&((p[2]>145&&p[1]>130&&p[2]>p[0]+8)||
                (include_occlusion&&((p[2]>110&&p[1]>100&&p[2]>p[0]+5)||classify(p)==2)))) {
                ++support;left=left||k<=-3;right=right||k>=3;
            }
        }
        return support>=7&&left&&right;
    };
    const auto inner_sides_filled=[&](Vec2 center,double width,int depth) {
        for(const int side:{-1,1}) {
            const double inset=width*.5-std::max(6.0,width*.04);
            const auto* p=pixel({center.x-n.x*depth+u.x*side*inset,
                                 center.y-n.y*depth+u.y*side*inset});
            if(!p||!(p[2]>145&&p[1]>130&&p[2]>p[0]+8)) return false;
        }
        return true;
    };
    std::optional<NoteCandidate> best;
    // Two bounded cross sections cover both a short head and a longer body.
    for(const int behind:{24,64}) {
        std::vector<double> bands;bands.reserve(16);
        std::optional<int> begin;
        const int radius=static_cast<int>(f.width*.18);
        for(int offset=-radius;offset<=radius+1;++offset) {
            const auto* p=pixel({head.x-n.x*behind+u.x*offset,head.y-n.y*behind+u.y*offset});
            const bool white=offset<=radius&&p&&std::min({p[0],p[1],p[2]})>170&&
                std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]})<30;
            if(white) {if(!begin) begin=offset;}
            else if(begin) {
                if(offset-*begin<=12&&bands.size()<16) bands.push_back((*begin+offset-1)/2.0);
                begin.reset();
            }
        }
        for(std::size_t a=0;a<bands.size();++a) for(std::size_t b=a+1;b<bands.size();++b) {
            const double width=bands[b]-bands[a],middle=(bands[a]+bands[b])/2;
            if(width<f.width*.04||width>f.width*.20||bands[a]>0||bands[b]<0||
               (best&&width<best->width-4)) continue;
            NoteCandidate candidate;candidate.kind=NoteKind::hold;candidate.width=width;
            candidate.tangent=u;candidate.confidence=.7;candidate.direct_rails_evidence=true;
            const Vec2 base{head.x+u.x*middle,head.y+u.y*middle};
            for(int forward=96;forward>=-32;--forward) {
                const Vec2 probe{base.x+n.x*forward,base.y+n.y*forward};
                if(!fill(probe,width,4)||!fill(probe,width,24)) continue;
                // The wider pair must not borrow a neighboring Hold rail
                // across an empty gap. Check fill immediately inside BOTH
                // rails, where the central nine samples cannot see that gap.
                if(!inner_sides_filled(probe,width,4)||!inner_sides_filled(probe,width,24)) continue;
                // It must be a visible leading edge. After a long rail gap,
                // an arbitrary interior cross section cannot become a head.
                if(fill(probe,width,-4,true)) continue;
                candidate.center={probe.x-n.x*6,probe.y-n.y*6};
                // Both front rows already prove current fill. A bounded
                // particle occlusion may interrupt either attached rail;
                // body fill still bounds the tail and both ends must agree.
                const auto rails=visible_hold_rails(f,candidate,std::max(24.0,width*.25),32,4,true,true);
                if(!rails) continue;
                candidate.tail=rails->tail;candidate.height=rails->depth;candidate.rails_geometry=true;
                candidate.head_on_line=std::abs(normal_distance(candidate.center,line))<=8;
                if(!best||width>best->width||candidate.center.y>best->center.y) best=candidate;
                break;
            }
        }
    }
    return best;
}
std::optional<LineCandidate> spanning_line(const Frame& f) {
    struct Column {int x; std::vector<double> y;};
    std::array<Column,9> columns;
    const auto bright=[&](int x,int y) {
        if(x<0||x>=f.width||y<0||y>=f.height) return false;
        const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
        const int color=classify(p);
        return color==4||color==2||color==1;
    };
    for(int k=0;k<9;++k) {
        auto& column=columns[k]; column.x=static_cast<int>(f.width*(.02+k*.12));
        int begin=-1;
        for(int y=static_cast<int>(f.height*.12);y<static_cast<int>(f.height*.95);++y) {
            if(bright(column.x,y)) {if(begin<0) begin=y;}
            else if(begin>=0) {
                if(y-begin<=f.height*.02&&column.y.size()<16) column.y.push_back((begin+y-1)/2.0);
                begin=-1;
            }
        }
    }
    int best_count=0; double best_slope=0,best_intercept=0;
    for(int a=0;a<4;++a) for(int b=a+5;b<9;++b)
        for(const double ya:columns[a].y) for(const double yb:columns[b].y) {
            const double slope=(yb-ya)/(columns[b].x-columns[a].x);
            if(std::abs(slope)>.8) continue;
            const double intercept=ya-slope*columns[a].x;
            int count=0; double sx=0,sy=0,xx=0,xy=0;
            for(int k=0;k<48;++k) {
                const int x=static_cast<int>(f.width*(.02+k*.96/47));
                const int expected=static_cast<int>(std::lround(intercept+slope*x));
                int found=0; bool seen=false;
                for(int d=0;d<=4&&!seen;++d) for(const int sign:{-1,1}) {
                    const int y=expected+sign*d;
                    if(y>=f.height*.12&&y<f.height*.95&&bright(x,y)) {found=y;seen=true;break;}
                }
                if(seen) {++count; sx+=x;sy+=found;xx+=static_cast<double>(x)*x;xy+=static_cast<double>(x)*found;}
            }
            if(count>best_count&&count>=38) {
                const double den=count*xx-sx*sx;
                if(den>0) {best_slope=(count*xy-sx*sy)/den;best_intercept=(sy-best_slope*sx)/count;best_count=count;}
            }
        }
    if(best_count<38) return {};
    const double norm=std::hypot(1.0,best_slope);
    return LineCandidate{{f.width/2.0,best_intercept+best_slope*f.width/2.0},
        {1/norm,best_slope/norm},f.width*norm,4,.85};
}
std::optional<Vec2> song_play_button(const Frame& f) {
    const auto white=[&](int x,int y) {
        if(x<0||y<0||x>=f.width||y>=f.height) return false;
        return classify(f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3)==4;
    };
    const auto fraction=[&](double x0,double y0,double x1,double y1) {
        int count=0,total=0;
        for(int y=static_cast<int>(y0*f.height);y<static_cast<int>(y1*f.height);y+=2)
            for(int x=static_cast<int>(x0*f.width);x<static_cast<int>(x1*f.width);x+=2) {
                ++total; if(white(x,y)) ++count;
            }
        return total?static_cast<double>(count)/total:0.0;
    };
    // Two independent selection-page structures, observed on the current
    // 1280x720 UI: selected difficulty badge and white Play trapezoid.
    // The title screen and settlement's Next button do not satisfy both.
    if(fraction(.35,.34,.43,.46)<.30 || fraction(.91,.82,.995,.94)<.65) return {};
    const int x0=static_cast<int>(f.width*.92),x1=static_cast<int>(f.width*.98),
              y0=static_cast<int>(f.height*.835),y1=static_cast<int>(f.height*.925);
    const int radius=std::max(6,static_cast<int>(f.height*.024));
    int count=0,minimum_x=x1,maximum_x=x0,minimum_y=y1,maximum_y=y0;
    double sx=0,sy=0;
    for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
        const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
        if(std::max({p[0],p[1],p[2]})>90) continue;
        const int surrounding=white(x-radius,y)+white(x+radius,y)+white(x,y-radius)+white(x,y+radius);
        if(surrounding<3) continue;
        ++count; sx+=x; sy+=y; minimum_x=std::min(minimum_x,x); maximum_x=std::max(maximum_x,x);
        minimum_y=std::min(minimum_y,y); maximum_y=std::max(maximum_y,y);
    }
    if(count<12 || count>f.width*f.height*.003 || maximum_x-minimum_x<f.width*.009 ||
       maximum_y-minimum_y<f.height*.016) return {};
    return Vec2{sx/count,sy/count};
}
}
void GameObserver::reset() {tracks_.clear();line_tracker_.reset(); playing_confirmations_=0; menu_confirmations_=0; previous_={};}

bool ComboVisibilityDiagnostic::observe(const DecisionSnapshot& s) {
    const auto gap=s.context.capture_ns-previous_.capture_ns;
    if(!s.playing_gate||!s.capacity_valid||!same_visual_geometry(previous_,s.context)||
       s.context.frame<=previous_.frame||gap<=0||gap>250'000'000) {
        present_=absent_=0;armed_=false;absent_since_=0;
    }
    previous_=s.context;
    if(!s.playing_gate||!s.capacity_valid) return false;
    if(s.combo_digit_glyphs>0) {
        present_=std::min(2,present_+1);absent_=0;absent_since_=0;
        if(present_>=2) armed_=true;
        return false;
    }
    present_=0;
    if(!armed_) return false;
    if(absent_++==0) absent_since_=s.context.capture_ns;
    absent_=std::min(2,absent_);
    if(absent_>=2&&s.context.capture_ns-absent_since_>=12'000'000) {
        armed_=false;absent_=0;absent_since_=0;return true;
    }
    return false;
}

std::optional<HoldDisappearance> HoldVisibilityDiagnostic::observe(const DecisionSnapshot& s) {
    const LineCandidate* line=nullptr;
    for(const auto& candidate:s.lines) if(candidate.confidence>=.8&&candidate.length>=s.context.width*.8&&
        (!line||candidate.confidence>line->confidence)) line=&candidate;
    if(!s.playing_gate||!s.capacity_valid||!line) {
        previous_count_=0;previous_=s.context;return {};
    }
    const auto gap=s.context.capture_ns-previous_.capture_ns;
    if(!same_geometry(previous_,s.context)||s.context.frame<=previous_.frame||gap<=0||gap>100'000'000)
        previous_count_=0;
    std::optional<HoldDisappearance> missing;
    for(std::size_t i=0;i<previous_count_&&!missing;++i) {
        const auto& prior=previous_holds_[i];
        const bool present=std::any_of(s.targets.begin(),s.targets.end(),[&](const auto& t) {
            return t.note.kind==NoteKind::hold&&t.note.rails_geometry&&
                t.note.width>=prior.width*.65&&t.note.width<=prior.width*1.5&&
                distance(t.note.center,prior.head)<=80;
        });
        if(!present) missing=prior;
    }
    previous_count_=0;previous_=s.context;
    for(const auto& t:s.targets) {
        if(t.note.kind!=NoteKind::hold||!t.note.rails_geometry||
           (!t.note.head_on_line&&!t.note.held_body_evidence)||t.note.width<s.context.width*.08||
           t.note.height<std::max(128.0,t.note.width*.8)||
           (!t.note.held_body_evidence&&std::abs(normal_distance(t.note.center,*line))>8)) continue;
        if(previous_count_==previous_holds_.size()) {previous_count_=0;return {};}
        previous_holds_[previous_count_++]={s.context.frame,t.note_id,s.context.capture_ns,
            t.note.center,t.note.width,t.note.height};
    }
    return missing;
}

DecisionSnapshot GameObserver::process(const Frame& f) {
    if(f.width<2||f.height<2||f.stride!=f.width*3||
       f.rgb.size()!=static_cast<std::size_t>(f.stride)*f.height)
        throw std::invalid_argument("invalid game frame");
    DecisionSnapshot out; out.sequence=++sequence_;
    out.context={f.epoch,f.generation,f.geometry_version,f.sequence,f.capture_complete_ns,
                 f.width,f.height,f.source_rotation};
    out.recognition_start_ns=clock_.now_ns();
    const auto gap=f.capture_complete_ns-previous_.capture_ns;
    // UI classification and motion fitting have different continuity needs.
    // A short gap/revocation discards every motion anchor; an independently
    // visible, fresh HUD can still confirm the same scene. No old frame or
    // motion evidence survives, and input keeps its separate 100 ms expiry.
    if(!same_visual_geometry(previous_,out.context)||f.sequence<=previous_.frame||
       gap<=0||gap>250'000'000||!f.source_valid) reset();
    else if(f.epoch!=previous_.epoch||gap>100'000'000) tracks_.clear();
    const bool distinct=f.sequence>previous_.frame;
    previous_=out.context;
    const auto all=components(f,out.capacity_valid);
    out.combo_digit_glyphs=combo_digit_shapes(f);
    std::vector<NoteCandidate> notes;
    int pause_bars=0, score_glyphs=0;
    // Live HD exposed a one-pixel horizontal line joined to Hold borders.
    // Component PCA alone then loses it. Scan every source row for long thin
    // visible runs; position is discovered from pixels, never a fixed Y.
    struct RowLine { int y, x0, x1; };
    std::vector<RowLine> rows;
    for(int y=static_cast<int>(f.height*.12);y<static_cast<int>(f.height*.95);++y) {
        int begin=-1, last=-1, gaps=0;
        const auto finish=[&] {
            if(begin>=0 && last-begin>f.width*.32 && rows.size()<static_cast<std::size_t>(f.height))
                rows.push_back({y,begin,last});
        };
        for(int x=0;x<f.width;++x) {
            const int color=classify(f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3);
            if(color==4||color==2||color==1) {if(begin<0) begin=x; last=x; gaps=0;}
            else if(begin>=0 && ++gaps>4) {finish(); begin=-1; last=-1; gaps=0;}
        }
        finish();
    }
    for(std::size_t i=0;i<rows.size();) {
        std::size_t j=i+1;
        int left=rows[i].x0, right=rows[i].x1;
        // Collinear support remains one geometric line even when a Hold
        // interrupts it. Group the whole thin band, not individual runs;
        // separate Y bands must retain separate association candidates.
        while(j<rows.size() && rows[j].y-rows[j-1].y<=2) {
            left=std::min(left,rows[j].x0); right=std::max(right,rows[j].x1); ++j;
        }
        const double thickness=rows[j-1].y-rows[i].y+1;
        if(thickness<=f.height*.018 && out.lines.size()<16)
            out.lines.push_back({{(left+right)/2.0,(rows[i].y+rows[j-1].y)/2.0},
                {1,0},static_cast<double>(right-left),thickness,.65});
        i=j;
    }
    if(const auto spanning=spanning_line(f)) {
        std::erase_if(out.lines,[&](const LineCandidate& line) {
            return std::abs(normal_distance(line.center,*spanning))<16;
        });
        out.lines.push_back(*spanning);
    }
    for(const auto& c:all) {
        const double w=c.x1-c.x0+2.0,h=c.y1-c.y0+2.0;
        const Vec2 center{c.sx/c.count,c.sy/c.count};
        if(c.color==4 && c.x1<f.width*.12 && c.y1<f.height*.12 &&
           h>f.height*.013 && h>1.4*w && w<f.width*.02) ++pause_bars;
        if(c.color==4 && c.x0>f.width*.78 && c.y1<f.height*.12 &&
           h>f.height*.01 && h<f.height*.07 && w<f.width*.035) ++score_glyphs;
        const double xx=c.xx/c.count-center.x*center.x,
                     yy=c.yy/c.count-center.y*center.y, xy=c.xy/c.count-center.x*center.y;
        const double theta=.5*std::atan2(2*xy,xx-yy);
        const double spread=std::hypot(xx-yy,2*xy);
        const double major=std::sqrt(std::max(0.0,(xx+yy+spread)/2)),
                     minor=std::sqrt(std::max(0.0,(xx+yy-spread)/2));
        if((c.color==4||c.color==2||c.color==1) && major>f.width*.07 &&
           major>12*std::max(1.0,minor) && center.y>f.height*.12 &&
           center.y<f.height*.95) {
            int adjacent_note_samples=0;
            for(int k=-2;k<=2;++k) {
                const double along=k*major*.6;
                bool adjacent=false;
                for(const int side:{-1,1}) {
                    const int x=static_cast<int>(std::lround(center.x+along*std::cos(theta)-side*8*std::sin(theta))),
                              y=static_cast<int>(std::lround(center.y+along*std::sin(theta)+side*8*std::cos(theta)));
                    if(x>=0&&y>=0&&x<f.width&&y<f.height) {
                        const int color=classify(f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3);
                        adjacent=adjacent||color==1||color==3;
                    }
                }
                if(adjacent) ++adjacent_note_samples;
            }
            if(adjacent_note_samples>=3) continue; // Hold outline, not independent line evidence.
            const bool existing=std::any_of(out.lines.begin(),out.lines.end(),[&](const LineCandidate& l) {
                return std::abs(normal_distance(center,l))<8 &&
                    std::abs(l.tangent.x*std::cos(theta)+l.tangent.y*std::sin(theta))>.995;
            });
            if(!existing) {
                if(out.lines.size()==16) {out.capacity_valid=false; continue;}
                LineCandidate line{center,{std::cos(theta),std::sin(theta)},
                                   major*3.46,std::max(2.0,minor*3.46),.6};
                if(const auto current=observe_current_line_extent(f,line)) {line=*current;line.confidence=.85;}
                else if(current_line_ridge_support(f,line))line.confidence=.85;
                out.lines.push_back(line);
            }
            continue;
        }
    }
    // Finish the current line set before interpreting note ribbons. Source
    // component order changes when a rotating line is interrupted by a note.
    // A note's local dimensions must not depend on which fragment came first.
    for(const auto& c:all) {
        const double w=c.x1-c.x0+2.0,h=c.y1-c.y0+2.0;
        const Vec2 center{c.sx/c.count,c.sy/c.count};
        const double xx=c.xx/c.count-center.x*center.x,
                     yy=c.yy/c.count-center.y*center.y,xy=c.xy/c.count-center.x*center.y;
        const double theta=.5*std::atan2(2*xy,xx-yy),spread=std::hypot(xx-yy,2*xy);
        const double major=std::sqrt(std::max(0.0,(xx+yy+spread)/2)),
                     minor=std::sqrt(std::max(0.0,(xx+yy-spread)/2));
        if((c.color==4||c.color==2||c.color==1)&&major>f.width*.07&&
           major>12*std::max(1.0,minor)&&center.y>f.height*.12&&center.y<f.height*.95)continue;
        const double minimum_width=f.width*(c.color==3?.018:.04);
        const bool aligned_ribbon=major>5*std::max(1.0,minor)&&
            std::any_of(out.lines.begin(),out.lines.end(),[&](const auto& l){
                return (l.confidence>=.8||l.length>=f.width*.5)&&
                    std::abs(l.tangent.x*std::cos(theta)+l.tangent.y*std::sin(theta))>.95;
            });
        const double observed_width=aligned_ribbon?major*std::sqrt(12.0)+2:w;
        if(c.color==2&&major<5*std::max(1.0,minor)) continue; // Rings/body borders are not Drag ribbons.
        if(c.color==4||center.y<f.height*.10 || observed_width<minimum_width||observed_width>f.width*.20||
           h<4 || h>f.height*.75 || c.count*4<w*h*.13) continue;
        if(notes.size()==128) {out.capacity_valid=false; continue;}
        NoteKind kind=c.color==2?NoteKind::drag:c.color==3?NoteKind::flick:
            h>w*.65?NoteKind::hold:NoteKind::tap;
        // A rotated thin blue ribbon has a tall SCREEN bbox. Determine its
        // local thickness before interpreting that bbox as a Hold body.
        if(c.color==1&&aligned_ribbon)kind=NoteKind::tap;
        if(kind==NoteKind::tap&&major<5*std::max(1.0,minor)) continue;
        NoteCandidate candidate{{(c.x0+c.x1)/2.0,(c.y0+c.y1)/2.0},kind,w,h,.55};
        candidate.tangent={std::cos(theta),std::sin(theta)};
        if(kind==NoteKind::tap&&std::abs(candidate.tangent.y)>.4) {
            candidate.center=center;candidate.width=major*std::sqrt(12.0)+2;
            candidate.height=minor*std::sqrt(12.0)+2;
        }
        // Near-horizontal caps retain their established screen geometry and
        // highlight suppression. Measure local extents when tilt can turn a
        // thin core's screen bbox into a falsely thick region.
        if(kind==NoteKind::drag&&aligned_ribbon&&c.ribbon_width>0) {
            candidate.center=c.ribbon_center;candidate.width=c.ribbon_width;candidate.height=c.ribbon_height;
        }
        if(kind==NoteKind::hold) {
            Vec2 axis{std::cos(theta),std::sin(theta)};
            const LineCandidate* main=nullptr;
            for(const auto& line:out.lines) if(line.confidence>=.8&&line.length>=f.width*.8&&
                (!main||line.confidence>main->confidence)) main=&line;
            if(main) {
                const Vec2 minor{-axis.y,axis.x},normal{-main->tangent.y,main->tangent.x};
                if(std::abs(minor.x*normal.x+minor.y*normal.y)>
                   std::abs(axis.x*normal.x+axis.y*normal.y)) axis=minor;
            }
            if(axis.y<0) {axis.x=-axis.x;axis.y=-axis.y;}
            candidate.tangent={axis.y,-axis.x};
            const double den=std::abs(candidate.tangent.x*axis.y)-std::abs(candidate.tangent.y*axis.x);
            if(den>.2) {
                const double length=(h*std::abs(candidate.tangent.x)-w*std::abs(candidate.tangent.y))/den;
                candidate.width=std::max(8.0,(w*std::abs(axis.y)-h*std::abs(axis.x))/den);
                candidate.center.x+=axis.x*(length/2-2); candidate.center.y+=axis.y*(length/2-2);
            } else candidate.center.y=c.y1-2;
            if(const auto rails=visible_hold_rails(f,candidate,candidate.width*.65)) {
                candidate.tail=rails->tail;candidate.rails_geometry=true;
            }
            if(candidate.tail) candidate.height=candidate.center.y-candidate.tail->y;
        }
        notes.push_back(candidate);
    }
    line_tracker_.update(out.lines,out.context);
    // A white/yellow Flick arrow interrupts its red ribbon into two cores.
    // Require that visible central vertical marker before joining fragments.
    for(std::size_t i=0;i<notes.size();++i) for(std::size_t j=i+1;j<notes.size();) {
        auto& a=notes[i]; const auto& b=notes[j];
        const double gap=std::abs(a.center.x-b.center.x)-(a.width+b.width)/2;
        bool marker=false;
        if(a.kind==NoteKind::flick&&b.kind==NoteKind::flick&&
           std::max(a.height,b.height)<=20&&std::abs(a.center.y-b.center.y)<8&&
           gap>=10&&gap<=f.width*.04&&std::abs(a.width-b.width)<12&&
           a.width+b.width+gap<=f.width*.18) {
            const int x=static_cast<int>(std::lround((a.center.x+b.center.x)/2));
            const int y=static_cast<int>(std::lround((a.center.y+b.center.y)/2));
            int support=0;
            for(int dy=-12;dy<=12;dy+=2) if(x>=0&&x<f.width&&y+dy>=0&&y+dy<f.height) {
                const auto color=classify(f.rgb.data()+static_cast<std::size_t>(y+dy)*f.stride+x*3);
                if(color==4||color==2) ++support;
            }
            marker=support>=8;
        }
        if(marker) {
            const double left=std::min(a.center.x-a.width/2,b.center.x-b.width/2),
                         right=std::max(a.center.x+a.width/2,b.center.x+b.width/2);
            a.center={(left+right)/2,(a.center.y+b.center.y)/2}; a.width=right-left;
            a.height=std::max(a.height,b.height); a.confidence=.65;
            notes.erase(notes.begin()+static_cast<std::ptrdiff_t>(j));
        } else ++j;
    }
    // A simultaneous-note highlight is a yellow outline around a blue/red
    // core. Its thin edge fragments must not create extra Drag identities.
    std::erase_if(notes,[&](const NoteCandidate& yellow) {
        if(yellow.kind!=NoteKind::drag) return false;
        for(const auto& core:notes) {
            if(core.kind!=NoteKind::tap&&core.kind!=NoteKind::hold&&core.kind!=NoteKind::flick) continue;
            // A complete simultaneous-note outline has taller end caps than
            // a Drag core. Compare its normal thickness to the visible core
            // width, including rotation, rather than a fixed screen-Y cap.
            const double ux=std::abs(yellow.tangent.x),uy=std::abs(yellow.tangent.y),den=ux*ux-uy*uy;
            const double thickness=den>.2?std::max(0.0,(yellow.height*ux-yellow.width*uy)/den):yellow.height;
            if(thickness>std::max(24.0,core.width*.25)) continue;
            if(std::abs(yellow.center.y-core.center.y)>16) continue;
            const double gap=std::abs(yellow.center.x-core.center.x)-(yellow.width+core.width)/2;
            if(gap>12||yellow.width>core.width*1.35) continue;
            for(const double fraction:{-.25,0.0,.25}) {
                const int x=static_cast<int>(std::lround(core.center.x+fraction*core.width*core.tangent.x)),
                          y=static_cast<int>(std::lround(core.center.y+fraction*core.width*core.tangent.y));
                if(x>=0&&y>=0&&x<f.width&&y<f.height) {
                    const int colour=classify(f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3);
                    if(colour==1||colour==3) return true;
                }
            }
        }
        return false;
    });
    // Independent HUD evidence: line/note presence never arms gameplay.
    // Non-playing classes require live evidence before templates are enabled.
    const bool hud=pause_bars>=2&&score_glyphs>=4;
    if(distinct) playing_confirmations_=hud&&f.source_valid&&out.capacity_valid?
        std::min(3,playing_confirmations_+1):0;
    out.playing_gate=hud&&f.source_valid&&out.capacity_valid&&playing_confirmations_>=3;
    out.ui=out.playing_gate?GameUi::playing:GameUi::unknown;
    out.ui_basis="development HUD: pause_bars="+std::to_string(pause_bars)+
        ";score_glyphs="+std::to_string(score_glyphs)+";confirmations="+
        std::to_string(playing_confirmations_)+";nonplaying_classes_unvalidated";
    const auto play=song_play_button(f);
    if(distinct) menu_confirmations_=!hud&&play?std::min(3,menu_confirmations_+1):0;
    if(!out.playing_gate&&menu_confirmations_>=3&&out.capacity_valid&&f.source_valid) {
        out.ui=GameUi::menu; out.play_button=play;
        out.ui_basis="song_selection_difficulty_badge_and_play_icon;confirmations="+
            std::to_string(menu_confirmations_);
    }
    const auto now=f.capture_complete_ns;
    std::erase_if(tracks_,[&](const History& t){return now-t.observed>=100'000'000;});
    if(f.source_valid&&out.capacity_valid) {
        const LineCandidate* main=nullptr;
        for(const auto& line:out.lines) if(line.confidence>=.8&&line.length>=f.width*.8&&
            (!main||line.confidence>main->confidence)) main=&line;
        std::vector<NoteCandidate> bodies;bodies.reserve(128);
        std::size_t seeds=0;
        if(main) for(const auto& c:all) {
            const double w=c.x1-c.x0+2.0,h=c.y1-c.y0+2.0;
            const bool clipped_top=c.y0<=2;
            if(c.color!=1||w<16||w>f.width*.20||h<18||
               h>f.height*(clipped_top?.95:.75)||
               (clipped_top?c.y1<f.height*.20:c.y0<f.height*.10)||c.count*4<w*h*.13) continue;
            // Clipping may make a valid body taller than the generic color
            // model permits. It still needs current paired rails, two front
            // fill rows and a visible leading edge; the tail stays unknown.
            if(++seeds>128) {out.capacity_valid=false;break;}
            const auto body=current_hold_body(f,c,*main);if(!body) continue;
            // A recently validated, on-line Hold already owns this visible
            // body region. Interior hit effects must not create fresh heads
            // there. Its existing path must still validate current rails.
            if(std::any_of(tracks_.begin(),tracks_.end(),[&](const History& track) {
                if(!track.rail_anchor||!track.rail_anchor->head_on_line||now-track.rail_observed>90'000'000) return false;
                const auto& held=*track.rail_anchor;
                const Vec2 delta{body->center.x-held.center.x,body->center.y-held.center.y};
                const double along=delta.x*main->tangent.x+delta.y*main->tangent.y,
                    depth=normal_distance(body->center,*main);
                return std::abs(along)+body->width*.5<=held.width*.5+16&&body->width<=held.width*1.2&&
                    depth<=12&&depth>=-held.height-16;
            })) continue;
            if(std::any_of(bodies.begin(),bodies.end(),[&](const auto& other) {
                return distance(body->center,other.center)<8&&std::abs(body->width-other.width)<8;
            })) continue;
            bodies.push_back(*body);
        }
        for(const auto& body:bodies) {
            // A connected outer color body and its enclosed effect seed can
            // describe different fronts of the SAME current rail pair. Only
            // discard the interior description when a recent rail history
            // supports the full front, its current head fill is present,
            // re-measured rails agree, and both blue side bands visibly
            // continue across the supposed interior edge.
            if(std::any_of(notes.begin(),notes.end(),[&](const auto& current) {
                if(current.kind!=NoteKind::hold||!current.rails_geometry||current.direct_rails_evidence||
                   std::abs(current.width-body.width)>=20||
                   std::abs(current.tangent.x*body.tangent.x+current.tangent.y*body.tangent.y)<.995) return false;
                const Vec2 delta{current.center.x-body.center.x,current.center.y-body.center.y};
                const double ahead=-delta.x*body.tangent.y+delta.y*body.tangent.x;
                if(ahead<=8||ahead>std::min(96.0,current.height)||!hold_fill_near_head(f,current)) return false;
                const auto pair=current_held_rail_section(f,current);
                if(!pair||std::abs(pair->width-body.width)>4||
                   std::abs((pair->center.x-body.center.x)*body.tangent.x+
                            (pair->center.y-body.center.y)*body.tangent.y)>4||!fill_crosses_hold_front(f,body)) return false;
                return std::any_of(tracks_.begin(),tracks_.end(),[&](const History& track) {
                    if(track.kind!=NoteKind::hold||!track.rail_anchor||now-track.rail_observed>90'000'000||
                       std::abs(track.rail_anchor->width-current.width)>=20) return false;
                    double mt=0,mx=0,my=0,den=0,nx=0,ny=0;int count=0;
                    Nanoseconds first=now,last=0;
                    for(const auto& p:track.points) if(p.rails&&now-p.t<=90'000'000) {
                        mt+=(p.t-now)/1e9;mx+=p.p.x;my+=p.p.y;++count;
                        first=std::min(first,p.t);last=std::max(last,p.t);
                    }
                    if(count<3||last-first<minimum_fit_span_ns) return false;
                    mt/=count;mx/=count;my/=count;
                    for(const auto& p:track.points) if(p.rails&&now-p.t<=90'000'000) {
                        const double dt=(p.t-now)/1e9-mt;den+=dt*dt;nx+=dt*(p.p.x-mx);ny+=dt*(p.p.y-my);
                    }
                    if(den<=0) return false;
                    const Vec2 expected{mx-nx/den*mt,my-ny/den*mt};
                    return distance(expected,current.center)<=16&&std::abs(track.rail_anchor->width-current.width)<20;
                });
            })) continue;
            // Preserve an already complete color head with its validated
            // rails. Reconstruction supplies missing geometry, rather than
            // truncating that head's tail to its saturated color patch.
            if(std::any_of(notes.begin(),notes.end(),[&](const auto& current) {
                return current.kind==NoteKind::hold&&current.rails_geometry&&
                    std::abs(current.width-body.width)<20&&distance(current.center,body.center)<8;
            })) continue;
            std::erase_if(notes,[&](const NoteCandidate& incoming) {
                if(incoming.kind!=NoteKind::hold&&incoming.kind!=NoteKind::tap) return false;
                // A tilted thin ribbon has a taller bounding box. Preserve
                // its thickness along the line normal, not screen Y alone.
                if(incoming.kind==NoteKind::tap&&
                   incoming.height<12+std::abs(body.tangent.y)*incoming.width) return false;
                const Vec2 delta{incoming.center.x-body.center.x,incoming.center.y-body.center.y};
                const double along=delta.x*body.tangent.x+delta.y*body.tangent.y,
                             across=-delta.x*body.tangent.y+delta.y*body.tangent.x;
                return std::abs(along)+incoming.width*.5<=body.width*.5+16&&
                    across>=-body.height-8&&across<=12;
            });
            if(notes.size()==128) {out.capacity_valid=false;break;}
            notes.push_back(body);
        }
        if(!out.capacity_valid) {out.playing_gate=false;out.ui=GameUi::unknown;}
    }
    // A judged Hold can lose or fragment its saturated core. Same-frame
    // parallel rails take precedence over those enclosed fragments; otherwise
    // a partial core moves the identity away from the still-visible head.
    std::vector<NoteCandidate> claimed_outlines;
    claimed_outlines.reserve(128);
    if(out.playing_gate) for(const auto& track:tracks_) {
        if(track.kind!=NoteKind::hold||track.points.empty()) continue;
        const bool recent_rails=track.rail_anchor&&now-track.rail_observed<=90'000'000;
        auto note=recent_rails?*track.rail_anchor:track.appearance;
        if(note.width<f.width*.04) continue;
        const LineCandidate* outline_line=nullptr;
        if(recent_rails) for(const auto& current:out.lines) {
            if(!current.association_valid||std::abs(current.tangent.x*note.tangent.x+
               current.tangent.y*note.tangent.y)<.95||
               (!note.head_on_line&&!note.held_body_evidence&&std::abs(normal_distance(note.center,current))>48))continue;
            const bool prior_line=!track.points.empty()&&track.points.back().line.track_id==current.track_id;
            if(!outline_line||prior_line)outline_line=&current;
            if(prior_line)break;
        }
        if(outline_line&&!note.head_on_line&&!note.held_body_evidence&&std::any_of(notes.begin(),notes.end(),[&](const auto& front){
            if(front.kind!=NoteKind::hold||!front.rails_geometry||front.recent_identity||
               std::abs(front.width-note.width)>=20) return false;
            const double along=(front.center.x-note.center.x)*outline_line->tangent.x+
                (front.center.y-note.center.y)*outline_line->tangent.y;
            const double current_distance=normal_distance(front.center,*outline_line);
            return std::abs(along)<=20&&current_distance>=-48&&current_distance<=8;
        }))outline_line=nullptr; // preserve a complete currently measured front
        if(outline_line) {
          auto outer=observe_moving_held_front(f,note,*outline_line);
          if(!outer)outer=observe_held_outline(f,note,*outline_line);
          if(!outer)outer=observe_held_body_patch(f,note,*outline_line);
          if(outer) {
            // A current physical pair is evidence for exactly one identity.
            // Otherwise several old anchors can each refresh themselves from
            // the same rails forever and consume all five contacts.
            if(std::any_of(claimed_outlines.begin(),claimed_outlines.end(),[&](const auto& used){
                return std::abs(used.width-outer->width)<=4&&distance(used.center,outer->center)<=12&&
                    std::abs(used.height-outer->height)<=16;
            }))continue;
            claimed_outlines.push_back(*outer);
            outer->recent_identity=track.id;
            std::erase_if(notes,[&](const NoteCandidate& other){
                if(other.kind==NoteKind::hold)
                    return std::abs(other.width-outer->width)<20&&distance(other.center,outer->center)<48;
                if(other.kind!=NoteKind::tap||other.height<12+std::abs(outer->tangent.y)*other.width)return false;
                const Vec2 delta{other.center.x-outer->center.x,other.center.y-outer->center.y};
                const double along=delta.x*outer->tangent.x+delta.y*outer->tangent.y,
                    across=-delta.x*outer->tangent.y+delta.y*outer->tangent.x;
                return std::abs(along)+other.width*.5<=outer->width*.5+16&&
                    across>=-outer->height-8&&across<=12;
            });
            if(notes.size()==128){out.capacity_valid=false;break;}
            notes.push_back(*outer);continue;
          }
        }
        if(note.held_body_evidence)continue; // a failed current body test cannot reuse its historical flag
        const LineCandidate* line=nullptr;
        for(const auto& candidate:out.lines) if(candidate.confidence>=.8&&candidate.length>=f.width*.8&&
            std::abs(candidate.tangent.x*note.tangent.x+
                     candidate.tangent.y*note.tangent.y)>.95) {
            if(!line||candidate.confidence>line->confidence) line=&candidate;
        }
        if(!line) continue;
        const bool approaching=recent_rails&&!note.head_on_line&&!note.held_body_evidence&&
            std::abs(normal_distance(note.center,*line))>8;
        // A complete current front supplies its own position. Re-fitting a
        // past front and quantizing a second search would add artificial
        // motion jitter. Already-held heads still use the on-line path.
        if(approaching&&std::any_of(notes.begin(),notes.end(),[&](const auto& current) {
            return current.direct_rails_evidence&&distance(current.center,note.center)<f.width*.08&&
                std::abs(current.width-note.width)<20;
        })) continue;
        std::optional<HoldRails> rails;
        if(approaching) {
            // Fit only recent, rail-validated heads. A decorative line can
            // split the next core without changing its outer Hold geometry.
            double mt=0,mx=0,my=0,den=0,nx=0,ny=0;int count=0;
            for(const auto& p:track.points) if(p.rails&&now-p.t<=90'000'000) {
                mt+=(p.t-now)/1e9;mx+=p.p.x;my+=p.p.y;++count;
            }
            if(count<3||track.points.back().t-track.points.front().t<minimum_fit_span_ns) continue;
            mt/=count;mx/=count;my/=count;
            for(const auto& p:track.points) if(p.rails&&now-p.t<=90'000'000) {
                const double dt=(p.t-now)/1e9-mt;den+=dt*dt;nx+=dt*(p.p.x-mx);ny+=dt*(p.p.y-my);
            }
            if(den<=0) continue;
            const Vec2 expected{mx-nx/den*mt,my-ny/den*mt};
            note.tangent=line->tangent;
            const Vec2 normal{-note.tangent.y,note.tangent.x};
            for(int offset=32;offset>=-32;offset-=2) {
                note.center={expected.x+normal.x*offset,expected.y+normal.y*offset};
                if(!hold_fill_at_front(f,note)) continue;
                // Front support is sampled 4 px behind this probe. Keep the
                // same 2 px inner leading-edge anchor as the color detector.
                note.center.x-=normal.x*6;note.center.y-=normal.y*6;
                rails=visible_hold_rails(f,note,note.width*.65);
                if(rails) break;
            }
            if(!rails) continue;
            note.head_on_line=std::abs(normal_distance(note.center,*line))<=8;
        } else {
            if(std::abs(normal_distance(note.center,*line))>40) continue;
            const double d=normal_distance(note.center,*line);
            note.center={note.center.x+d*line->tangent.y,note.center.y-d*line->tangent.x};
            note.tangent=line->tangent;
            const bool held_fill=recent_rails&&hold_fill_near_head(f,note);
            rails=visible_hold_rails(f,note,16,held_fill?32:12,16,false,held_fill,held_fill);
            if(!rails&&recent_rails) {
                if(const auto current=current_held_rail_section(f,note);current&&hold_fill_near_head(f,*current)) {
                    rails=visible_hold_rails(f,*current,16,32,16,false,true,true);
                    if(rails) note=*current;
                }
            }
            if(!rails) continue;
            note.head_on_line=true;
        }
        note.outline_evidence=true;note.rails_geometry=true;note.direct_rails_evidence=false;note.confidence=.7;
        // The hit effect briefly covers a rail after it has been confirmed.
        // Bridge at most 32 px only with current blue/gray fill at the head;
        // a new body separated from the line cannot refresh this identity.
        note.tail=rails->tail;note.height=rails->depth;
        note.recent_identity=track.id;
        std::erase_if(notes,[&](const NoteCandidate& incoming) {
            if(incoming.kind!=NoteKind::hold&&incoming.kind!=NoteKind::tap) return false;
            // An independent thin Tap can coexist with a Hold. Require a
            // body-shaped component enclosed by the currently observed rails.
            if(incoming.kind==NoteKind::tap&&incoming.height<12+std::abs(note.tangent.y)*incoming.width) return false;
            const double along=(incoming.center.x-note.center.x)*note.tangent.x+
                               (incoming.center.y-note.center.y)*note.tangent.y;
            const double across=normal_distance(incoming.center,*line)-normal_distance(note.center,*line);
            return std::abs(along)+incoming.width*.5<=note.width*.5+16&&
                   across>=-rails->depth-8&&across<=12;
        });
        if(notes.size()==128) {out.capacity_valid=false;out.playing_gate=false;out.ui=GameUi::unknown;break;}
        notes.push_back(note);
    }
    // Merge complementary nested color ribbons only when one is distinctly
    // narrower and enclosed by the other. Equal-width overlapping candidates
    // retain separate identities; proximity alone is not merge evidence.
    for(std::size_t i=0;i<notes.size();++i) for(std::size_t j=i+1;j<notes.size();) {
        auto& a=notes[i]; const auto& b=notes[j];
        const double ratio=std::min(a.width,b.width)/std::max(a.width,b.width);
        if(a.kind==b.kind&&a.kind!=NoteKind::hold&&ratio>.6&&ratio<.93&&
           std::abs(a.center.x-b.center.x)<3&&std::abs(a.center.y-b.center.y)<10&&
           std::max(a.height,b.height)<20) {
            const double top=std::min(a.center.y-a.height/2,b.center.y-b.height/2),
                         bottom=std::max(a.center.y+a.height/2,b.center.y+b.height/2);
            a.center.y=(top+bottom)/2; a.height=bottom-top; a.width=std::max(a.width,b.width);
            notes.erase(notes.begin()+static_cast<std::ptrdiff_t>(j));
        } else ++j;
    }
    std::vector<std::optional<NoteCandidate>> shortened_holds(notes.size());
    for(std::size_t i=0;i<notes.size();++i) {
        const auto& n=notes[i];if(n.kind!=NoteKind::tap) continue;
        const Vec2 axis{-n.tangent.y,n.tangent.x};
        const double den=std::abs(n.tangent.x*axis.y)-std::abs(n.tangent.y*axis.x);
        if(den<=.2) continue;
        const double length=(n.height*std::abs(n.tangent.x)-n.width*std::abs(n.tangent.y))/den;
        if(length<12) continue; // A thin new Tap must never inherit a completed Hold.
        auto h=n;h.kind=NoteKind::hold;
        h.center.x+=axis.x*(length/2-2);h.center.y+=axis.y*(length/2-2);
        h.tail=visible_hold_tail(f,h);shortened_holds[i]=h;
    }
    candidate_batch_=make_candidate_batch(out,f,notes,shortened_holds,tracks_);
    candidate_batch_.extraction_end_ns=clock_.now_ns();
    track_legacy_batch(out,notes,shortened_holds,tracks_,next_id_);
    if(!out.capacity_valid) {out.playing_gate=false; out.ui=GameUi::unknown;}
    out.recognition_end_ns=clock_.now_ns();
    return out;
}

GamePlanOwner::GamePlanOwner(const Clock& clock, TouchBackend& backend, int contacts,GameActionOptions options)
    : clock_(clock),scheduler_(clock,backend,contacts,128,16,350'000'000,100'000'000),options_(options) {}
std::vector<ContactPlan> GamePlanOwner::take_accepted_plans() {
    auto result=std::move(accepted_plans_); accepted_plans_.clear(); return result;
}
std::vector<nlohmann::json> GamePlanOwner::take_coverage_updates() {
    auto result=std::move(coverage_updates_);coverage_updates_.clear();return result;
}
std::vector<nlohmann::json> GamePlanOwner::take_plan_cancellations() {
    auto result=std::move(plan_cancellations_);plan_cancellations_.clear();return result;
}
std::vector<TouchReceipt> GamePlanOwner::accept(const DecisionSnapshot& incoming) {
    auto s=incoming; // <=128 current observations; aliases never change observer history.
    std::vector<TouchReceipt> receipts;
    if(s.sequence<=last_snapshot_) {last_rejection_="snapshot_order"; return receipts;}
    if(s.context.epoch<epoch_ || (s.context.epoch==epoch_&&
       (s.context.generation<context_.generation || s.context.geometry<context_.geometry))) {
        last_rejection_="context_order"; return receipts;
    }
    last_snapshot_=s.sequence;
    if(!same_geometry(context_,s.context)) {
        scheduler_.cancel("scene_context_changed"); identities_.clear(); contact_aliases_.clear();
        epoch_=s.context.epoch; context_=s.context;
    }
    const bool fresh=s.context.capture_ns<=clock_.now_ns()&&
        clock_.now_ns()-s.context.capture_ns<100'000'000;
    if(!scheduler_.set_context(epoch_,s.context.generation,s.context.geometry,
        s.playing_gate&&s.capacity_valid&&fresh,s.context.capture_ns)) return receipts;
    if(!s.playing_gate||!s.capacity_valid||!fresh) {last_rejection_="gate_invalid"; return receipts;}
    const auto cancel_contact=[&](std::uint64_t note_id,Identity& id,const std::string& reason) {
        const auto cursor=scheduler_.executed_steps(id.intent);
        if(cursor) {
            if(plan_cancellations_.size()>=128) throw std::runtime_error("contact cancellation diagnostic capacity");
            plan_cancellations_.push_back({{"event","game_contact_cancelled"},{"note_id",note_id},
                {"intent_id",id.intent},{"kind",name(id.kind)},{"reason",reason},
                {"source_frame",s.context.frame},{"cancel_ns",clock_.now_ns()},
                {"last_evidence_ns",id.plan.evidence_ns},{"executed_steps",*cursor},
                {"contact_started",*cursor>id.plan.prefix_offset},{"game_effect","unknown"}});
        }
        auto canceled=scheduler_.cancel_intent(id.intent);
        receipts.insert(receipts.end(),canceled.begin(),canceled.end());
    };
    // Unique current outer-body support can retain a finger across candidate
    // ID churn. Completed contacts can never be revived by this association.
    const auto held_support=[&](const GameTarget& t) {
        return t.note.kind==NoteKind::hold&&t.note.rails_geometry&&
            (t.note.head_on_line||t.note.held_body_evidence)&&t.samples>0&&
            t.evidence_ns==s.context.capture_ns&&t.expires_ns>clock_.now_ns()&&
            t.reason!="association_ambiguous"&&t.reason!="line_unobservable"&&
            t.reason!="multiple_line_association_unvalidated";
    };
    const auto compatible_body=[](const Identity& id,const GameTarget& t) {
        const auto& prior=id.last_note;
        const double alignment=std::abs(prior.tangent.x*t.note.tangent.x+prior.tangent.y*t.note.tangent.y);
        const Vec2 delta{t.note.center.x-prior.center.x,t.note.center.y-prior.center.y};
        return (!id.line_id||!t.line_id||id.line_id==t.line_id)&&alignment>=.98&&
            std::abs(prior.width-t.note.width)<=std::max(4.0,prior.width*.12)&&
            std::abs(delta.x*t.note.tangent.x+delta.y*t.note.tangent.y)<=std::min(48.0,prior.width*.3)&&
            std::abs(-delta.x*t.note.tangent.y+delta.y*t.note.tangent.x)<=48;
    };
    std::set<std::uint64_t> claimed;
    std::vector<bool> ignored(s.targets.size());
    for(const auto& t:s.targets) if(identities_.contains(t.note_id)&&held_support(t)) claimed.insert(t.note_id);
    for(std::size_t ti=0;ti<s.targets.size();++ti) {
        auto& t=s.targets[ti];
        if(const auto alias=contact_aliases_.find(t.note_id);alias!=contact_aliases_.end()) {
            alias->second.last_seen_ns=s.context.capture_ns;
            const auto key=alias->second.owner;const auto owner=identities_.find(key);
            const auto cursor=owner==identities_.end()?decltype(scheduler_.executed_steps(0)){}:
                scheduler_.executed_steps(owner->second.intent);
            // Aliases are identity hints, never a substitute for CURRENT
            // support. Invalid duplicate descriptions must not cancel the
            // independently visible owner, nor become a fresh Down.
            if(owner==identities_.end()||!cursor||*cursor<=owner->second.plan.prefix_offset||
               claimed.contains(key)||!held_support(t)||!compatible_body(owner->second,t)||
               clock_.now_ns()-owner->second.plan.evidence_ns>=60'000'000) {
                ignored[ti]=true;continue;
            }
            for(std::size_t oi=0;oi<s.targets.size();++oi)
                if(oi!=ti&&s.targets[oi].note_id==key&&!held_support(s.targets[oi]))ignored[oi]=true;
            t.note_id=key;claimed.insert(key);continue;
        }
        const auto prior_candidate=identities_.find(t.note_id);
        const bool submitted_candidate=prior_candidate!=identities_.end()&&prior_candidate->second.submitted;
        if(submitted_candidate||!held_support(t)) continue;
        std::uint64_t sole=0;bool ambiguous=false;
        for(const auto& [key,id]:identities_) {
            const auto cursor=scheduler_.executed_steps(id.intent);
            if(claimed.contains(key)||id.kind!=NoteKind::hold||!id.submitted||!cursor||
               *cursor<=id.plan.prefix_offset||id.hold_tail_release_ns||!id.last_note.rails_geometry||
               clock_.now_ns()-id.plan.evidence_ns>=60'000'000||!compatible_body(id,t)) continue;
            if(sole){ambiguous=true;break;}sole=key;
        }
        if(sole&&!ambiguous) {
            if(contact_aliases_.size()>=128||coverage_updates_.size()>=128) {
                last_rejection_="contact_alias_capacity";continue;
            }
            const auto candidate=t.note_id;
            // Previously observed but unsubmitted candidates have no finger
            // to preserve. They can become a new description of this contact.
            identities_.erase(candidate);
            for(std::size_t oi=0;oi<s.targets.size();++oi)
                if(oi!=ti&&s.targets[oi].note_id==sole&&!held_support(s.targets[oi]))ignored[oi]=true;
            contact_aliases_[candidate]={sole,s.context.capture_ns};t.note_id=sole;claimed.insert(sole);
            coverage_updates_.push_back({{"event","game_hold_contact_reassociated"},{"note_id",sole},
                {"candidate_note_id",candidate},{"source_frame",s.context.frame},
                {"evidence_ns",t.evidence_ns},{"basis","unique_current_outer_body"},{"game_effect","unknown"}});
        }
    }
    std::size_t target_index=0;
    std::erase_if(s.targets,[&](const auto&){return ignored[target_index++];});
    const auto visible_tail_passed=[&](const GameTarget& t) {
        if(t.note.kind!=NoteKind::hold||!t.note.tail||!t.note.rails_geometry||t.samples<=0||
           t.evidence_ns!=s.context.capture_ns||t.reason=="association_ambiguous"||
           t.reason=="line_unobservable"||t.reason=="multiple_line_association_unvalidated") return false;
        const auto tail=*t.note.tail;
        if(!std::isfinite(tail.x)||!std::isfinite(tail.y)||tail.x<0||tail.x>=s.context.width||
           tail.y<0||tail.y>=s.context.height) return false;
        for(const auto& line:s.lines) {
            const double norm=std::hypot(line.tangent.x,line.tangent.y);
            if(norm<.99||norm>1.01||
               std::abs(line.tangent.x*t.note.tangent.x+line.tangent.y*t.note.tangent.y)<.95||
               std::abs(normal_distance(t.hit,line))>3) continue;
            Vec2 normal{-line.tangent.y,line.tangent.x};
            const double depth=(t.note.center.x-tail.x)*normal.x+(t.note.center.y-tail.y)*normal.y;
            if(std::abs(depth)<2) continue;
            if(depth<0) {normal.x=-normal.x;normal.y=-normal.y;}
            if((tail.x-t.hit.x)*normal.x+(tail.y-t.hit.y)*normal.y>=0) return true;
        }
        return false;
    };
    const auto current_drag=[&](const GameTarget& t) {
        return t.note.kind==NoteKind::drag&&t.samples>0&&t.reason!="association_ambiguous"&&
            t.reason!="line_unobservable"&&t.reason!="multiple_line_association_unvalidated"&&
            t.evidence_ns==s.context.capture_ns&&
            t.evidence_ns<=clock_.now_ns()&&clock_.now_ns()-t.evidence_ns<100'000'000&&
            t.expires_ns>clock_.now_ns()&&t.hit.x>=0&&t.hit.x<s.context.width&&
            t.hit.y>=s.context.height*.12&&t.hit.y<s.context.height;
    };
    const auto current_drag_overlap=[&](const GameTarget& t) {
        // Drag judges a contact in its CURRENT region. A failed temporal fit
        // need not prohibit that spatial action, but cannot manufacture a
        // crossing time. Require independent current line and core geometry.
        if(!current_drag(t)||t.samples<2||t.history_span_ns<10'000'000||!t.line_id||
           t.note.outline_evidence||t.note.rails_geometry||t.note.held_body_evidence||
           !std::isfinite(t.note.width)||!std::isfinite(t.note.height)||
           t.note.width<s.context.width*.05||t.note.width>s.context.width*.22||
           t.note.height<4||t.note.height>t.note.width*.35||t.note.confidence<.5||
           !std::isfinite(t.note.center.x)||!std::isfinite(t.note.center.y)||
           !std::isfinite(t.hit.x)||!std::isfinite(t.hit.y)) return false;
        for(const auto& line:s.lines) {
            if(line.track_id!=t.line_id||!line.association_valid||
               line.observed_ns!=s.context.capture_ns||line.confidence<.8||
               line.length<s.context.width*.5||
               std::abs(std::hypot(line.tangent.x,line.tangent.y)-1)>.01||
               std::abs(line.tangent.x*t.note.tangent.x+line.tangent.y*t.note.tangent.y)<.95)
                continue;
            const double d=normal_distance(t.note.center,line);
            const Vec2 projected{t.note.center.x+d*line.tangent.y,
                                 t.note.center.y-d*line.tangent.x};
            if(std::abs(d)<=std::min(8.0,t.note.height*.5+2)&&
               std::hypot(t.hit.x-projected.x,t.hit.y-projected.y)<=2&&
               std::abs((t.note.center.x-line.center.x)*line.tangent.x+
                        (t.note.center.y-line.center.y)*line.tangent.y)<=line.length*.5)
                return true;
        }
        return false;
    };
    const auto drag_release=[&](const GameTarget& t)->std::optional<Nanoseconds> {
        const auto now=clock_.now_ns();
        if(!current_drag(t)) return {};
        if(current_drag_overlap(t)) return t.evidence_ns+100'000'000;
        if(t.line_id&&std::abs(t.distance)<=std::max(8.0,t.note.height*.5+4))
            return t.evidence_ns+100'000'000;
        if(!t.crossing_ns||t.reason!="prediction_observe_only"||
           t.uncertainty_ns>options_.uncertainty_ns||*t.crossing_ns-now< -40'000'000||
           *t.crossing_ns-options_.lead_ns-now>60'000'000) return {};
        return std::max(*t.crossing_ns-options_.lead_ns,now+15'000'000)+75'000'000;
    };
    const auto drag_can_cover=[&](const Identity& leader,const GameTarget& t) {
        const auto cursor=scheduler_.executed_steps(leader.intent);
        if(!cursor||*cursor<=leader.plan.prefix_offset||!current_drag(t)||
           t.evidence_ns<leader.plan.evidence_ns||leader.plan.steps.back().due_ns<=clock_.now_ns()) return false;
        const auto executed=static_cast<std::size_t>(*cursor-leader.plan.prefix_offset);
        const auto& at=leader.plan.steps.at(executed-1);
        // A stationary contact can cover the next currently observed yellow
        // core. Use its conservative region, never a whole future lane.
        const double tangent_norm=std::hypot(t.note.tangent.x,t.note.tangent.y);
        if(tangent_norm<.99||tangent_norm>1.01||!std::isfinite(t.note.width)||t.note.width<0) return false;
        const double dx=t.hit.x-at.x,dy=t.hit.y-at.y;
        const double along=std::abs(dx*t.note.tangent.x+dy*t.note.tangent.y);
        const double across=std::abs(-dx*t.note.tangent.y+dy*t.note.tangent.x);
        const double half_region=std::max(2.0,std::min(48.0,t.note.width*.30));
        if(along>half_region&&(!t.line_id||leader.line_id!=t.line_id||
           along>std::max(48.0,t.note.width*.5))) return false;
        if(across>2&&(!t.line_id||leader.line_id!=t.line_id||across>48)) return false;
        return true;
    };
    const auto refresh_drag=[&](Identity& leader,const GameTarget& t,bool attach) {
        if(!drag_can_cover(leader,t)) return false;
        const auto release=drag_release(t);if(attach&&!release) return false;
        if(attach&&*release-90'000'000>leader.plan.steps.back().due_ns) return false;
        auto plan=leader.plan;plan.revision++;plan.evidence_ns=t.evidence_ns;
        plan.source_frame_sequence=s.context.frame;
        const auto cursor=scheduler_.executed_steps(leader.intent);
        if(!cursor) return false;
        const auto executed=static_cast<std::size_t>(*cursor-plan.prefix_offset);
        const auto at=plan.steps.at(executed-1);
        const auto release_due=release&&(attach||leader.shared_drag||t.line_id)?
            std::max(plan.steps.back().due_ns,*release):plan.steps.back().due_ns;
        const double dx=t.hit.x-at.x,dy=t.hit.y-at.y;
        const double along=std::abs(dx*t.note.tangent.x+dy*t.note.tangent.y);
        const double across=std::abs(-dx*t.note.tangent.y+dy*t.note.tangent.x);
        if(along>std::max(2.0,std::min(48.0,t.note.width*.3))||across>2) {
            plan.steps.resize(executed);
            plan.steps.push_back({Phase::move,t.hit.x,t.hit.y,clock_.now_ns()});
            plan.steps.push_back({Phase::up,t.hit.x,t.hit.y,std::max(clock_.now_ns(),release_due)});
        } else plan.steps.back().due_ns=release_due;
        if(executed>1) {
            plan.steps.erase(plan.steps.begin(),plan.steps.begin()+static_cast<std::ptrdiff_t>(executed-1));
            plan.prefix_offset+=executed-1;
        }
        if(!scheduler_.submit(plan)) return false;
        leader.plan=plan;leader.line_id=t.line_id;leader.last_note=t.note;
        if(accepted_plans_.size()>=256) throw std::runtime_error("accepted plan diagnostic capacity");
        accepted_plans_.push_back(std::move(plan));return true;
    };
    std::set<std::uint64_t> group_seen,group_alive;
    for(const auto& t:s.targets) {
        const auto member=identities_.find(t.note_id);if(member==identities_.end()) continue;
        const auto progress=scheduler_.executed_steps(member->second.intent);
        if(!progress||*progress<=member->second.plan.prefix_offset) continue;
        const auto leader_id=member->second.drag_leader?member->second.drag_leader:
            member->second.kind==NoteKind::drag?t.note_id:0;
        if(!leader_id) continue;
        group_seen.insert(leader_id);
        const auto leader=identities_.find(leader_id);
        if(leader!=identities_.end()&&refresh_drag(leader->second,t,false)) group_alive.insert(leader_id);
    }
    // Match a fresh successor before declaring the old member missing. No
    // pixels or non-overlapping windows means no extension of this contact.
    for(const auto& t:s.targets) {
        if(identities_.contains(t.note_id)) continue;
        const auto candidate_release=drag_release(t);
        if(!candidate_release) continue;
        Identity* sole=nullptr;std::uint64_t leader_id=0;bool multiple=false;
        for(auto& [id,leader]:identities_) if(!leader.drag_leader&&leader.submitted&&
            leader.kind==NoteKind::drag&&drag_can_cover(leader,t)&&
            *candidate_release-90'000'000<=leader.plan.steps.back().due_ns) {
            if(sole) {multiple=true;break;} sole=&leader;leader_id=id;
        }
        if(sole&&!multiple&&refresh_drag(*sole,t,true)) {
            sole->shared_drag=true;group_alive.insert(leader_id);
        }
    }
    std::set<std::uint64_t> visible;
    for(const auto& t:s.targets) visible.insert(t.note_id);
    for(const auto leader:group_alive) visible.insert(leader);
    for(auto& [id,identity]:identities_) if(!identity.drag_leader&&
        (identity.shared_drag?!group_alive.contains(id):!visible.contains(id))) {
        const auto grace=identity.kind==NoteKind::hold?60'000'000:identity.kind==NoteKind::flick?75'000'000:40'000'000;
        if(!group_seen.contains(id)&&clock_.now_ns()-identity.plan.evidence_ns<grace) continue;
        cancel_contact(id,identity,"current_object_missing_or_region_lost");
    }
    // Keep bounded retirement evidence beyond the observer's occlusion window.
    std::erase_if(identities_,[&](const auto& e) {
        return !visible.contains(e.first)&&clock_.now_ns()>=e.second.expires;
    });
    std::erase_if(contact_aliases_,[&](const auto& alias){return s.context.capture_ns-alias.second.last_seen_ns>=100'000'000;});
    for(const auto& t:s.targets) {
        auto found=identities_.find(t.note_id);
        if(found==identities_.end()) {
            if(identities_.size()==128) {last_rejection_="identity_capacity"; scheduler_.cancel(last_rejection_); break;}
            found=identities_.emplace(t.note_id,Identity{0,0,t.expires_ns+250'000'000,false}).first;
        }
        auto& id=found->second; id.expires=t.expires_ns+250'000'000;
        if(id.submitted) {
            if(id.drag_leader||id.shared_drag) continue;
            const auto cursor=scheduler_.executed_steps(id.intent);
            if(!cursor) continue;
            if(t.expires_ns<=clock_.now_ns()||t.reason=="association_ambiguous"||
               t.evidence_ns<id.plan.evidence_ns||t.samples==0) {
                cancel_contact(t.note_id,id,t.reason=="association_ambiguous"?"identity_ambiguous":
                    t.samples==0?"current_geometry_unsupported":"target_evidence_invalid");
                continue;
            }
            if(id.kind==NoteKind::hold&&*cursor>id.plan.prefix_offset&&id.last_note.held_body_evidence&&
               (!held_support(t)||!compatible_body(id,t))) {
                // A color fragment is not the current held region. Retain the
                // last touch only within the existing missing grace; never
                // refresh its evidence or move it back to a line projection.
                if(clock_.now_ns()-id.plan.evidence_ns>=60'000'000)
                    cancel_contact(t.note_id,id,"current_held_region_unsupported");
                continue;
            }
            // An unexecuted Down must not inherit a deadline explicitly
            // contradicted by newer pixels. A later valid fit can retry with
            // a new intent; active or completed contacts never replay Down.
            const bool spatial_drag=*cursor==0&&id.kind==NoteKind::drag&&current_drag_overlap(t);
            const bool timing_uncertain=t.crossing_ns&&t.uncertainty_ns>options_.uncertainty_ns;
            if(*cursor==0&&!spatial_drag&&(t.reason=="nonlinear_or_mismatch"||
               t.reason=="outside_short_horizon"||t.reason=="root_past"||timing_uncertain)) {
                auto canceled=scheduler_.cancel_intent(id.intent);
                receipts.insert(receipts.end(),canceled.begin(),canceled.end());
                if(plan_cancellations_.size()>=128) throw std::runtime_error("pending cancellation diagnostic capacity");
                plan_cancellations_.push_back({{"event","game_pending_prediction_cancelled"},
                    {"note_id",t.note_id},{"intent_id",id.intent},{"kind",name(id.kind)},
                    {"source_frame",s.context.frame},{"evidence_ns",t.evidence_ns},
                    {"prior_source_frame",id.plan.source_frame_sequence},{"prior_evidence_ns",id.plan.evidence_ns},
                    {"cancel_ns",clock_.now_ns()},{"reason",timing_uncertain?"timing_uncertainty_exceeds_limit":t.reason},
                    {"prior_predicted_down_ns",id.plan.predicted_down_ns? nlohmann::json(*id.plan.predicted_down_ns):nlohmann::json(nullptr)},
                    {"distance_px",t.distance},{"velocity_px_per_s",t.velocity},{"residual_px",t.residual},
                    {"no_down_injected",true},{"game_effect","unknown"}});
                id.submitted=false;
                continue;
            }
            auto plan=id.plan; plan.revision=std::max(t.revision,plan.revision+1);
            plan.evidence_ns=t.evidence_ns; plan.source_frame_sequence=s.context.frame;
            if(spatial_drag) {
                plan.basis="live_pixels_current_drag_overlap";
                plan.predicted_down_ns.reset();
                plan.steps={{Phase::down,t.hit.x,t.hit.y,clock_.now_ns()},
                            {Phase::up,t.hit.x,t.hit.y,t.evidence_ns+100'000'000}};
                plan.valid_until_ns=clock_.now_ns()+30'000'000;
            } else if(*cursor==0&&t.crossing_ns&&t.reason=="prediction_observe_only"&&
               t.uncertainty_ns<=options_.uncertainty_ns&&
               t.hit.x>=0&&t.hit.x<s.context.width&&t.hit.y>=s.context.height*.12&&t.hit.y<s.context.height) {
                const auto due=*t.crossing_ns-options_.lead_ns;
                if(*t.crossing_ns-clock_.now_ns()>=-40'000'000&&due-clock_.now_ns()<=60'000'000) {
                    const auto down=std::max(clock_.now_ns(),due-(id.kind==NoteKind::drag?15'000'000:0));
                    plan.predicted_down_ns=due-(id.kind==NoteKind::drag?15'000'000:0);
                    const auto shift=down-plan.steps.front().due_ns;
                    const double dx=t.hit.x-plan.steps.front().x,dy=t.hit.y-plan.steps.front().y;
                    for(auto& step:plan.steps) {
                        step.due_ns+=shift;step.x+=dx;
                        step.y=std::min(s.context.height*.94,step.y+dy);
                    }
                    plan.valid_until_ns=down+45'000'000;
                }
            }
            if(id.kind==NoteKind::hold&&*cursor==0) {
                // A revised collision deadline can move Down earlier, but
                // cannot shift the Hold lease from an old frame with it.
                // Use the same latest evidence +100ms bound as active Holds.
                plan.steps.back().due_ns=t.evidence_ns+100'000'000;
            }
            if(id.kind==NoteKind::hold&&*cursor>plan.prefix_offset) {
                if(t.note.held_body_evidence)plan.basis=t.note.held_body_patch?
                    "live_pixels_held_body_patch_continuation":"live_pixels_held_body_continuation";
                const auto executed=static_cast<std::size_t>(*cursor-plan.prefix_offset);
                plan.steps.resize(executed);
                const auto last=plan.steps.back();
                if(std::hypot(t.hit.x-last.x,t.hit.y-last.y)>2&&t.samples>0&&
                   t.hit.x>=0&&t.hit.x<s.context.width&&t.hit.y>=s.context.height*.12&&t.hit.y<s.context.height)
                    plan.steps.push_back({Phase::move,t.hit.x,t.hit.y,clock_.now_ns()});
                const auto limit=t.evidence_ns+100'000'000;
                if(!id.hold_tail_release_ns&&visible_tail_passed(t)) {
                    if(!id.tail_first_pass_ns) id.tail_first_pass_ns=t.evidence_ns;
                } else if(!id.hold_tail_release_ns) id.tail_first_pass_ns.reset();
                if(!id.hold_tail_release_ns&&id.tail_first_pass_ns&&
                   t.evidence_ns-*id.tail_first_pass_ns>=10'000'000) {
                    id.hold_tail_release_ns=t.evidence_ns+20'000'000;
                    if(coverage_updates_.size()>=128) throw std::runtime_error("hold tail diagnostic capacity");
                    coverage_updates_.push_back({{"event","game_hold_tail_confirmed"},{"note_id",t.note_id},
                        {"intent_id",id.intent},{"source_frame",s.context.frame},{"evidence_ns",t.evidence_ns},
                        {"release_ns",*id.hold_tail_release_ns},{"basis","current_visible_tail_crossed_line"},
                        {"game_effect","unknown"}});
                }
                const auto release=id.hold_tail_release_ns.value_or(limit);
                const auto due=std::max(clock_.now_ns(),std::min(release,limit));
                const auto at=plan.steps.back(); plan.steps.push_back({Phase::up,at.x,at.y,due});
                if(executed>1) {
                    plan.steps.erase(plan.steps.begin(),plan.steps.begin()+static_cast<std::ptrdiff_t>(executed-1));
                    plan.prefix_offset+=executed-1;
                }
            }
            if(scheduler_.submit(plan)) {
                id.plan=plan;id.last_note=t.note;id.line_id=t.line_id;
                if(accepted_plans_.size()>=256) throw std::runtime_error("accepted plan diagnostic capacity");
                accepted_plans_.push_back(std::move(plan));
            }
            continue;
        }
        const int bit=t.note.kind==NoteKind::tap?1:t.note.kind==NoteKind::hold?2:
                      t.note.kind==NoteKind::drag?4:t.note.kind==NoteKind::flick?8:0;
        const bool spatial_drag=current_drag_overlap(t);
        if(t.note.held_body_evidence||!(options_.enabled_types&bit)||t.expires_ns<=clock_.now_ns()||
           (!spatial_drag&&(!t.crossing_ns||t.reason!="prediction_observe_only"||
                            t.uncertainty_ns>options_.uncertainty_ns))) continue;
        const auto now=clock_.now_ns();
        const auto predicted_due=spatial_drag?now+15'000'000:*t.crossing_ns-options_.lead_ns;
        const auto remaining=predicted_due-now;
        if(!spatial_drag&&(*t.crossing_ns-now< -40'000'000||remaining>60'000'000)) continue;
        // Late but bounded live evidence may still be recoverable. Dispatch
        // immediately; retain the original prediction separately in the journal.
        const auto due=std::max(predicted_due,now+(t.note.kind==NoteKind::drag?15'000'000:0));
        if(t.hit.x<0||t.hit.y<s.context.height*.12||t.hit.x>=s.context.width||t.hit.y>=s.context.height) {
            last_rejection_="unsafe_region"; continue;
        }
        if(t.note.kind==NoteKind::drag) {
            bool covered=false;
            Identity* sole=nullptr;std::uint64_t sole_note=0;bool multiple=false;
            for(auto& [leader_note,leader]:identities_) {
                if(leader_note==t.note_id||leader.drag_leader||!leader.submitted||leader.kind!=NoteKind::drag||
                   due-15'000'000>leader.plan.steps.back().due_ns||!drag_can_cover(leader,t)) continue;
                if(sole) {multiple=true;break;} sole=&leader;sole_note=leader_note;
            }
            if(sole&&!multiple&&refresh_drag(*sole,t,true)) {
                auto& leader=*sole;
                leader.shared_drag=true;id.drag_leader=sole_note;id.intent=leader.intent;
                id.submitted=true;id.kind=NoteKind::drag;id.plan=leader.plan;covered=true;
                if(coverage_updates_.size()>=128) throw std::runtime_error("drag coverage diagnostic capacity");
                coverage_updates_.push_back({{"event","game_drag_coverage"},{"note_id",t.note_id},
                    {"leader_note_id",sole_note},{"intent_id",leader.intent},{"source_frame",s.context.frame},
                    {"evidence_ns",t.evidence_ns},{"accepted_ns",clock_.now_ns()},
                    {"crossing_ns",t.crossing_ns?nlohmann::json(*t.crossing_ns):nlohmann::json(nullptr)},
                    {"release_ns",leader.plan.steps.back().due_ns},
                    {"basis","fresh_drag_region_covers_active_contact"},{"game_effect","unknown"}});
            }
            if(covered) continue;
        }
        if(t.note.kind==NoteKind::hold&&due>=t.evidence_ns+100'000'000) continue;
        if(t.note.kind==NoteKind::hold&&visible_tail_passed(t)) {last_rejection_="hold_tail_already_passed";continue;}
        id.intent=++next_intent_; id.revision=t.revision;
        ContactPlan plan{epoch_,id.intent,t.revision,t.evidence_ns,due+30'000'000,
            s.context.frame,std::string("live_pixels_short_linear_fit_")+name(t.note.kind),
            {{Phase::down,t.hit.x,t.hit.y,due},
             {Phase::up,t.hit.x,t.hit.y,due+18'000'000}}};
        if(t.note.kind==NoteKind::hold) {
            const auto limit=t.evidence_ns+100'000'000;
            // Tail predictions remain diagnostic. Fresh body evidence renews
            // the bounded lease; normal Up needs an actually visible tail.
            plan.steps.back().due_ns=limit;
        } else if(t.note.kind==NoteKind::drag) {
            plan.steps.front().due_ns=due-15'000'000;
            plan.steps.back().due_ns=spatial_drag?t.evidence_ns+100'000'000:due+75'000'000;
            if(spatial_drag)plan.basis="live_pixels_current_drag_overlap";
        } else if(t.note.kind==NoteKind::flick) {
            plan.steps.pop_back();
            for(int k=1;k<=4;++k) plan.steps.push_back({Phase::move,t.hit.x,
                std::min(s.context.height*.94,t.hit.y+k*20),due+k*12'000'000});
            const auto last=plan.steps.back(); plan.steps.push_back({Phase::up,last.x,last.y,due+52'000'000});
        }
        plan.note_id=t.note_id; plan.generation=s.context.generation; plan.geometry_version=s.context.geometry;
        if(!spatial_drag)plan.predicted_down_ns=predicted_due-(t.note.kind==NoteKind::drag?15'000'000:0);
        id.submitted=scheduler_.submit(plan); id.kind=t.note.kind; id.plan=plan;
        id.last_note=t.note;id.line_id=t.line_id;
        if(id.submitted) {
            if(accepted_plans_.size()>=256) throw std::runtime_error("accepted plan diagnostic capacity");
            accepted_plans_.push_back(std::move(plan));
        }
        if(!id.submitted) last_rejection_="scheduler_rejected";
    }
    return receipts;
}
std::vector<TouchReceipt> GamePlanOwner::poll() {
    auto receipts=scheduler_.run_due();
    for(const auto& r:receipts) if(r.command.phase==Phase::up) {
        for(const auto& [note,id]:identities_) if(id.intent==r.command.intent_id&&!id.drag_leader) {
            if(coverage_updates_.size()>=128)throw std::runtime_error("contact Up diagnostic capacity");
            const auto reason=clock_.now_ns()-id.plan.evidence_ns>=100'000'000?"target_evidence_expired":
                id.hold_tail_release_ns&&clock_.now_ns()>=*id.hold_tail_release_ns?"visible_tail_completed":"contact_window_completed";
            coverage_updates_.push_back({{"event","game_contact_up"},{"note_id",note},{"intent_id",id.intent},
                {"kind",name(id.kind)},{"reason",reason},{"last_evidence_ns",id.plan.evidence_ns},
                {"source_frame",r.command.source_frame_sequence},{"scheduled_ns",r.command.scheduled_ns},
                {"injection_return_ns",r.injection_return_ns},{"success",r.success},{"game_effect","unknown"}});
            break;
        }
    }
    return receipts;
}
void GamePlanOwner::stop() {scheduler_.request_stop(); scheduler_.run_due(); scheduler_.cancel("runtime_stop");}

std::optional<ContactPlan> PlayButtonPlanner::take(const DecisionSnapshot& s,Nanoseconds now) {
    if(attempted_ || s.ui!=GameUi::menu || !s.play_button || s.playing_gate || !s.capacity_valid ||
       !s.context.epoch || !s.context.frame || s.context.capture_ns>now ||
       now-s.context.capture_ns>=60'000'000) return {};
    const auto p=*s.play_button;
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<s.context.width*.88||p.x>=s.context.width||
       p.y<s.context.height*.78||p.y>=s.context.height*.96) return {};
    attempted_=true;
    ContactPlan plan{s.context.epoch,1,1,s.context.capture_ns,now+30'000'000,s.context.frame,
        "pixels_song_selection_PLAY",{{Phase::down,p.x,p.y,now},{Phase::up,p.x,p.y,now+20'000'000}}};
    plan.generation=s.context.generation; plan.geometry_version=s.context.geometry;
    return plan;
}

nlohmann::json decision_json(const DecisionSnapshot& s) {
    using nlohmann::json;
    json lines=json::array(),targets=json::array();
    for(const auto& l:s.lines) lines.push_back({{"x",l.center.x},{"y",l.center.y},
        {"ux",l.tangent.x},{"uy",l.tangent.y},{"length",l.length},{"confidence",l.confidence},
        {"line_id",l.track_id},{"observed_ns",l.observed_ns},{"association_valid",l.association_valid},
        {"vx",l.velocity.x},{"vy",l.velocity.y},{"angular_velocity",l.angular_velocity},
        {"motion_valid",l.motion_valid},{"motion_samples",l.motion_samples},{"motion_span_ns",l.motion_span_ns},
        {"motion_residual_px",l.motion_residual},{"angular_residual_rad",l.angular_residual}});
    for(const auto& t:s.targets) targets.push_back({{"note_id",t.note_id},{"revision",t.revision},
        {"kind",name(t.note.kind)},{"x",t.note.center.x},{"y",t.note.center.y},
        {"width",t.note.width},{"height",t.note.height},{"evidence_ns",t.evidence_ns},
        {"expires_ns",t.expires_ns},{"crossing_ns",t.crossing_ns?json(*t.crossing_ns):json(nullptr)},
        {"uncertainty_ns",t.uncertainty_ns},{"hit_x",t.hit.x},{"hit_y",t.hit.y},
        {"line_id",t.line_id},{"hit_vx",t.hit_velocity.x},{"hit_vy",t.hit_velocity.y},
        {"tail_crossing_ns",t.tail_crossing_ns?json(*t.tail_crossing_ns):json(nullptr)},
        {"tail_x",t.note.tail?json(t.note.tail->x):json(nullptr)},{"tail_y",t.note.tail?json(t.note.tail->y):json(nullptr)},
        {"observation_basis",t.note.direct_rails_evidence?"hold_current_parallel_rails_and_fill":
            t.note.outline_evidence?"hold_parallel_rails_and_recent_identity":"color_core"},
        {"rails_geometry",t.note.rails_geometry},{"head_on_line",t.note.head_on_line},
        {"held_body_evidence",t.note.held_body_evidence},
        {"held_body_patch",t.note.held_body_patch},
        {"relative_distance_px",t.distance},{"relative_velocity_px_s",t.velocity},
        {"residual_px",t.residual},{"prediction_error_px",t.prediction_error_px},
        {"fit_residual_limit_px",t.fit_residual_limit_px},{"samples",t.samples},
        {"history_span_ns",t.history_span_ns},{"reason",t.reason}});
    return {{"event","game_decision"},{"decision_schema",2},{"sequence",s.sequence},
        {"note_anchor_semantics","tap_flick_core_center_hold_leading_edge"},
        {"epoch",s.context.epoch},{"generation",s.context.generation},{"geometry_version",s.context.geometry},
        {"frame_sequence",s.context.frame},{"capture_complete_ns",s.context.capture_ns},
        {"recognition_start_ns",s.recognition_start_ns},{"recognition_end_ns",s.recognition_end_ns},
        {"ui",name(s.ui)},{"ui_basis",s.ui_basis},{"playing_gate",s.playing_gate},
        {"combo_digit_glyphs",s.combo_digit_glyphs},{"combo_glyph_semantics","diagnostic_shapes_only_not_ocr_or_judgment"},
        {"play_button",s.play_button?json{{"x",s.play_button->x},{"y",s.play_button->y}}:json(nullptr)},
        {"capacity_valid",s.capacity_valid},{"lines",lines},{"targets",targets},
        {"source_absolute_age",nullptr},{"per_note_feedback","unknown"}};
}

nlohmann::json analyze_game_jsonl(const std::filesystem::path& path) {
    using nlohmann::json;
    std::ifstream file(path); if(!file) throw std::runtime_error("cannot open game journal");
    std::uint64_t frames=0,predictions=0,dry=0,real=0,revokes=0,rejections=0,gate_frames=0,multi_line=0;
    std::uint64_t playing_no_line=0;
    std::map<std::string,std::uint64_t> kinds,reasons,uis,accepted_kinds,real_phases,rejection_reasons,revoke_reasons;
    std::map<std::string,std::uint64_t> playing_kinds,playing_reasons;
    struct IntentEvidence {std::string kind; std::optional<Nanoseconds> down; std::optional<bool> planned_future;
        std::uint64_t note=0;std::optional<Nanoseconds> predicted_down;json plan=nullptr;};
    struct TrackEvidence {std::string kind;Nanoseconds last=0;bool predicted=false,near=false,accepted=false,down=false,covered=false;
        std::optional<double> first_near_available_lead_ms;};
    std::map<std::uint64_t,TrackEvidence> observed_tracks;
    std::map<std::string,std::map<std::string,std::uint64_t>> track_outcomes;
    std::map<std::string,std::vector<double>> first_near_leads;
    std::uint64_t track_evictions=0;
    std::map<std::uint64_t,IntentEvidence> intent_evidence;
    std::map<int,std::uint64_t> active_contacts;
    json latest_targets=json::array(),conflicts=json::array();
    Nanoseconds latest_capture=0;
    std::uint64_t conflicts_omitted=0,contact_history_resets=0,unknown_contact_receipts=0;
    std::uint64_t drag_coverage_updates=0,drag_coverage_with_known_contact=0;
    std::uint64_t pending_prediction_cancellations=0;
    std::map<std::string,std::uint64_t> pending_cancellation_reasons;
    std::map<std::string,std::uint64_t> contact_cancellations,contact_ups;
    std::map<std::string,std::map<std::string,std::uint64_t>> contact_cancellations_by_kind;
    std::uint64_t hold_tail_confirmations=0,hold_contact_reassociations=0;
    const auto describe_intent=[&](std::uint64_t intent) {
        json result={{"intent_id",intent},{"plan",nullptr},{"current_target",nullptr},
            {"target_capture_ns",latest_capture}};
        const auto found=intent_evidence.find(intent);
        if(found==intent_evidence.end()) return result;
        result["plan"]=found->second.plan;
        for(const auto& target:latest_targets) if(target.value("note_id",std::uint64_t{0})==found->second.note) {
            result["current_target"]=target;break;
        }
        return result;
    };
    std::map<std::string,std::uint64_t> actual_down_kinds;
    std::map<std::string,std::vector<double>> contact_durations,down_lateness;
    std::uint64_t intent_evidence_evictions=0;
    std::uint64_t latest_intent=0;
    std::vector<double> processing,uncertainty,residual,intervals,playing_intervals,down_skew;
    bool previous_playing=false;
    std::vector<double> actual_lateness,rpc_duration;
    std::vector<double> future_down_lateness,already_past_down_lateness;
    std::vector<double> predicted_deadline_down_lateness;
    std::uint64_t intentionally_clamped_downs=0,unknown_predicted_downs=0;
    std::uint64_t unclassified_down_deadlines=0;
    Nanoseconds previous=0,last_down_due=-1,last_down_start=0;
    const auto sample=[](std::vector<double>& values,double value) {if(values.size()<100'000) values.push_back(value);};
    const auto count=[](auto& counts,const std::string& key) {
        if(!counts.contains(key)&&counts.size()>=64) throw std::runtime_error("game analyzer key capacity");
        ++counts[key];
    };
    const auto finish_track=[&](const TrackEvidence& evidence) {
        const std::string outcome=evidence.down?"actual_down":evidence.covered?"covered_by_active_drag":evidence.accepted?"accepted_without_down":
            evidence.near?"near_prediction_not_accepted":evidence.predicted?"prediction_never_near":"never_predicted";
        count(track_outcomes[evidence.kind],outcome);
        if(evidence.first_near_available_lead_ms)
            sample(first_near_leads[evidence.kind+"_"+outcome],*evidence.first_near_available_lead_ms);
    };
    std::string line;
    while(std::getline(file,line)) {
        if(line.empty()) continue;
        const auto e=json::parse(line); const auto event=e.value("event","");
        if(event=="game_decision") {
            const auto schema=e.value("decision_schema",0);
            if(schema!=1&&schema!=2) throw std::runtime_error("unknown game decision schema");
            ++frames; count(uis,e.at("ui").get<std::string>());
            const bool playing=e.at("playing_gate").get<bool>();
            if(playing) {++gate_frames;if(e.at("lines").empty()) ++playing_no_line;}
            if(e.at("lines").size()>1) ++multi_line;
            const auto t=e.at("capture_complete_ns").get<Nanoseconds>();
            if(e.at("targets").size()>128) throw std::runtime_error("game analyzer target capacity");
            latest_targets=e.at("targets");latest_capture=t;
            for(auto it=observed_tracks.begin();it!=observed_tracks.end();) {
                if(t-it->second.last>200'000'000) {finish_track(it->second);it=observed_tracks.erase(it);}
                else ++it;
            }
            if(previous&&t>previous) {
                sample(intervals,(t-previous)/1e6);
                if(previous_playing&&playing)sample(playing_intervals,(t-previous)/1e6);
            }
            previous=t;previous_playing=playing;
            sample(processing,(e.at("recognition_end_ns").get<Nanoseconds>()-
                               e.at("recognition_start_ns").get<Nanoseconds>())/1e6);
            for(const auto& target:e.at("targets")) {
                count(kinds,target.at("kind").get<std::string>());
                count(reasons,target.at("reason").get<std::string>());
                if(playing) {
                    count(playing_kinds,target.at("kind").get<std::string>());
                    count(playing_reasons,target.at("reason").get<std::string>());
                    const auto note=target.value("note_id",std::uint64_t{0});
                    if(note) {
                        if(!observed_tracks.contains(note)&&observed_tracks.size()>=512) {
                            finish_track(observed_tracks.begin()->second);observed_tracks.erase(observed_tracks.begin());++track_evictions;
                        }
                        auto& evidence=observed_tracks[note];evidence.kind=target.at("kind").get<std::string>();evidence.last=t;
                        if(!target.at("crossing_ns").is_null()) {
                            evidence.predicted=true;
                            const auto gap=target.at("crossing_ns").get<Nanoseconds>()-t;
                            const bool near=gap>=-40'000'000&&gap<=100'000'000&&
                                target.at("uncertainty_ns").get<Nanoseconds>()<=30'000'000;
                            if(near&&!evidence.first_near_available_lead_ms)
                                evidence.first_near_available_lead_ms=(target.at("crossing_ns").get<Nanoseconds>()-
                                    e.at("recognition_end_ns").get<Nanoseconds>())/1e6;
                            evidence.near=evidence.near||near;
                        }
                    }
                }
                if(!target.at("crossing_ns").is_null()) {
                    ++predictions; sample(uncertainty,target.at("uncertainty_ns").get<Nanoseconds>()/1e6);
                    sample(residual,target.at("residual_px").get<double>());
                }
            }
        } else if(event=="game_plan_accepted") {
            const auto intent=e.at("intent_id").get<std::uint64_t>();
            if(!intent_evidence.contains(intent)) {
                if(intent_evidence.size()>=256) {intent_evidence.erase(intent_evidence.begin());++intent_evidence_evictions;}
                intent_evidence.emplace(intent,IntentEvidence{e.value("basis","unknown"),{}, {},e.value("note_id",std::uint64_t{0})});
            }
            auto& evidence=intent_evidence.at(intent);
            if(e.at("steps").size()>16) throw std::runtime_error("game analyzer plan capacity");
            evidence.plan=e;
            if(auto track=observed_tracks.find(evidence.note);track!=observed_tracks.end()) track->second.accepted=true;
            if(!evidence.down&&e.contains("accepted_ns")&&!e.at("steps").empty()&&e.at("steps").front().at("phase")==0)
                evidence.planned_future=e.at("steps").front().at("due_ns").get<Nanoseconds>()>e.at("accepted_ns").get<Nanoseconds>();
            if(!evidence.down&&e.contains("predicted_down_ns")&&!e.at("predicted_down_ns").is_null())
                evidence.predicted_down=e.at("predicted_down_ns").get<Nanoseconds>();
            if(intent>latest_intent) {
                latest_intent=intent;
                count(accepted_kinds,e.value("basis","unknown"));
            }
        } else if(event=="game_pending_prediction_cancelled") {
            ++pending_prediction_cancellations;
            count(pending_cancellation_reasons,e.value("reason","unknown"));
        } else if(event=="game_drag_coverage") {
            ++drag_coverage_updates;
            const auto intent=e.at("intent_id").get<std::uint64_t>();
            const bool known=e.value("real_input",false)&&std::any_of(active_contacts.begin(),active_contacts.end(),
                [&](const auto& contact){return contact.second==intent;});
            if(known) ++drag_coverage_with_known_contact;
            if(auto track=observed_tracks.find(e.at("note_id").get<std::uint64_t>());track!=observed_tracks.end()) {
                track->second.accepted=true;track->second.covered=track->second.covered||known;
            }
        } else if(event=="game_contact_cancelled") {
            count(contact_cancellations,e.value("reason","unknown"));
            count(contact_cancellations_by_kind[e.value("kind","unknown")],e.value("reason","unknown"));
        } else if(event=="game_contact_up") {
            count(contact_ups,e.value("reason","unknown"));
        } else if(event=="game_hold_tail_confirmed") {
            ++hold_tail_confirmations;
        } else if(event=="game_hold_contact_reassociated") {
            ++hold_contact_reassociations;
        } else if(event=="dry_touch_receipt") {
            ++dry;
            if(e.at("phase")==0) {
                const auto due=e.at("scheduled_ns").get<Nanoseconds>(),start=e.at("injection_start_ns").get<Nanoseconds>();
                if(due==last_down_due) sample(down_skew,(start-last_down_start)/1e6);
                last_down_due=due; last_down_start=start;
            }
        } else if(event=="game_touch_receipt") {
            ++real;
            count(real_phases,std::to_string(e.at("phase").get<int>()));
            const auto start=e.at("injection_start_ns").get<Nanoseconds>();
            sample(actual_lateness,(start-e.at("scheduled_ns").get<Nanoseconds>())/1e6);
            sample(rpc_duration,(e.at("injection_return_ns").get<Nanoseconds>()-start)/1e6);
            const auto found=intent_evidence.find(e.at("intent_id").get<std::uint64_t>());
            if(e.contains("contact_id")&&e.contains("success")&&e.at("success")==true) {
                const auto contact=e.at("contact_id").get<int>();
                if(e.at("phase")==0) {
                    if(!active_contacts.contains(contact)&&active_contacts.size()>=16)
                        throw std::runtime_error("game analyzer active contact capacity");
                    active_contacts[contact]=e.at("intent_id").get<std::uint64_t>();
                } else if(e.at("phase")==2) active_contacts.erase(contact);
            } else ++unknown_contact_receipts;
            const std::string kind=found==intent_evidence.end()?"unknown":found->second.kind;
            if(e.at("phase")==0) {
                count(actual_down_kinds,kind);
                sample(down_lateness[kind],(start-e.at("scheduled_ns").get<Nanoseconds>())/1e6);
                if(found!=intent_evidence.end()) {
                    found->second.down=start;
                    if(found->second.predicted_down) {
                        sample(predicted_deadline_down_lateness,(start-*found->second.predicted_down)/1e6);
                        if(*found->second.predicted_down<e.at("scheduled_ns").get<Nanoseconds>()) ++intentionally_clamped_downs;
                    } else ++unknown_predicted_downs;
                    if(auto track=observed_tracks.find(found->second.note);track!=observed_tracks.end()) track->second.down=true;
                    if(found->second.planned_future) sample(*found->second.planned_future?future_down_lateness:
                        already_past_down_lateness,(start-e.at("scheduled_ns").get<Nanoseconds>())/1e6);
                    else ++unclassified_down_deadlines;
                } else {++unclassified_down_deadlines;++unknown_predicted_downs;}
            } else if(e.at("phase")==2&&found!=intent_evidence.end()) {
                if(found->second.down) sample(contact_durations[kind],(start-*found->second.down)/1e6);
                intent_evidence.erase(found);
            }
        } else if(event=="runtime_revoke") {++revokes; count(revoke_reasons,e.value("reason","unknown"));}
        else if(event=="owner_revoked") {active_contacts.clear();++contact_history_resets;}
        else if(event=="scheduler_rejection") {
            ++rejections;count(rejection_reasons,e.value("reason","unknown"));
            if(e.value("reason","")=="contact_conflict") {
                if(conflicts.size()>=64) {++conflicts_omitted;continue;}
                auto detail=describe_intent(e.at("intent_id").get<std::uint64_t>());
                detail["rejection_ns"]=e.value("monotonic_ns",Nanoseconds{0});
                detail["active_contacts"]=json::array();
                for(const auto& [contact,intent]:active_contacts) {
                    auto active=describe_intent(intent);active["contact_id"]=contact;
                    detail["active_contacts"].push_back(std::move(active));
                }
                conflicts.push_back(std::move(detail));
            }
        }
    }
    for(const auto& [id,evidence]:observed_tracks) finish_track(evidence);
    json durations=json::object(),lateness_by_kind=json::object(),first_leads=json::object();
    for(const auto& [kind,values]:contact_durations) durations[kind]=distribution(values);
    for(const auto& [kind,values]:down_lateness) lateness_by_kind[kind]=distribution(values);
    for(const auto& [outcome,values]:first_near_leads) first_leads[outcome]=distribution(values);
    return {{"schema_version",1},{"frames",frames},{"playing_gate_frames",gate_frames},
        {"ui_occurrences",uis},{"note_candidate_occurrences",kinds},{"target_reason_occurrences",reasons},
        {"playing_no_line_frames",playing_no_line},{"playing_note_candidate_occurrences",playing_kinds},
        {"playing_target_reason_occurrences",playing_reasons},
        {"observed_track_outcomes_by_kind",track_outcomes},{"observed_track_evictions",track_evictions},
        {"first_near_prediction_available_lead_ms_by_track_outcome",first_leads},
        {"track_outcome_semantics","pixels identities, not chart notes; near means crossing relative to capture [-40,100]ms and uncertainty<=30ms"},
        {"prediction_occurrences",predictions},{"multi_line_frames",multi_line},
        {"dry_command_count",dry},{"runtime_revokes",revokes},{"scheduler_rejections",rejections},
        {"real_command_count",real},{"real_schedule_lateness_ms",distribution(actual_lateness)},
        {"accepted_intents_by_basis",accepted_kinds},{"real_commands_by_phase",real_phases},
        {"real_downs_by_basis",actual_down_kinds},{"contact_duration_ms_by_basis",durations},
        {"down_lateness_ms_by_basis",lateness_by_kind},{"intent_evidence_evictions",intent_evidence_evictions},
        {"future_at_accept_down_lateness_ms",distribution(future_down_lateness)},
        {"already_past_at_accept_down_lateness_ms",distribution(already_past_down_lateness)},
        {"unclassified_down_deadlines",unclassified_down_deadlines},
        {"predicted_deadline_down_lateness_ms",distribution(predicted_deadline_down_lateness)},
        {"intentionally_clamped_downs",intentionally_clamped_downs},{"unknown_predicted_downs",unknown_predicted_downs},
        {"scheduler_rejections_by_reason",rejection_reasons},{"runtime_revokes_by_reason",revoke_reasons},
        {"contact_conflicts",conflicts},{"contact_conflicts_omitted",conflicts_omitted},
        {"contact_history_resets",contact_history_resets},{"unknown_contact_receipts",unknown_contact_receipts},
        {"drag_coverage_updates",drag_coverage_updates},{"drag_coverage_with_known_local_contact",drag_coverage_with_known_contact},
        {"pending_prediction_cancellations",pending_prediction_cancellations},
        {"pending_prediction_cancellations_by_reason",pending_cancellation_reasons},
        {"contact_cancellations_by_reason",contact_cancellations},
        {"contact_cancellations_by_kind_and_reason",contact_cancellations_by_kind},
        {"contact_ups_by_reason",contact_ups},
        {"hold_tail_confirmations",hold_tail_confirmations},
        {"hold_contact_reassociations",hold_contact_reassociations},
        {"pending_cancellation_semantics","current pixels explicitly invalidate an unexecuted Down prediction; later valid pixels may retry, game effect remains unknown"},
        {"drag_coverage_semantics","fresh pixels share an already active local Drag contact; successful RPC history supports local coverage only, game judgment remains unknown"},
        {"contact_conflict_semantics","journal-order successful RPC receipts reconstruct local contacts; game effect and true simultaneous note count remain unknown; owner revoke clears local history"},
        {"game_rpc_duration_ms",distribution(rpc_duration)},
        {"capture_interval_ms",distribution(intervals)},{"recognition_duration_ms",distribution(processing)},
        {"playing_capture_interval_ms",distribution(playing_intervals)},
        {"playing_interval_scope","consecutive decisions with playing_gate=true; real gaps across epoch are included"},
        {"prediction_uncertainty_ms",distribution(uncertainty)},{"prediction_residual_px",distribution(residual)},
        {"dry_equal_deadline_down_skew_ms",distribution(down_skew)},
        {"distribution_sample_cap",100000},{"per_note_feedback","unknown"},{"gameplay_validated",false},
        {"raw_sha256",sha256_file(path)}};
}
void draw_game_overlay(Frame& f,const DecisionSnapshot& s) {
    const auto pixel=[&](int x,int y,std::array<std::uint8_t,3> color) {
        if(x>=0&&y>=0&&x<f.width&&y<f.height) {
            auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
            std::copy(color.begin(),color.end(),p);
        }
    };
    const auto line=[&](Vec2 a,Vec2 b,std::array<std::uint8_t,3> color) {
        const int steps=static_cast<int>(std::max(std::abs(a.x-b.x),std::abs(a.y-b.y)))+1;
        for(int i=0;i<=steps;++i) {const double u=static_cast<double>(i)/steps;
            pixel(static_cast<int>(std::lround(a.x+(b.x-a.x)*u)),
                  static_cast<int>(std::lround(a.y+(b.y-a.y)*u)),color);}
    };
    for(const auto& l:s.lines) line({l.center.x-l.tangent.x*l.length/2,l.center.y-l.tangent.y*l.length/2},
        {l.center.x+l.tangent.x*l.length/2,l.center.y+l.tangent.y*l.length/2},{0,255,0});
    for(const auto& t:s.targets) {
        const auto p=t.note.center; const double w=t.note.width/2,h=t.note.height/2;
        const std::array<std::uint8_t,3> color=t.crossing_ns?std::array<std::uint8_t,3>{0,255,255}:
            std::array<std::uint8_t,3>{255,100,0};
        line({p.x-w,p.y-h},{p.x+w,p.y-h},color); line({p.x-w,p.y+h},{p.x+w,p.y+h},color);
        line({p.x-w,p.y-h},{p.x-w,p.y+h},color); line({p.x+w,p.y-h},{p.x+w,p.y+h},color);
        if(t.crossing_ns) {line({t.hit.x-6,t.hit.y},{t.hit.x+6,t.hit.y},color);
            line({t.hit.x,t.hit.y-6},{t.hit.x,t.hit.y+6},color);}
    }
}
} // namespace pas
