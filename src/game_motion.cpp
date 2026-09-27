#include "pas/game_motion.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace pas {
namespace {
double dot(Vec2 a,Vec2 b){return a.x*b.x+a.y*b.y;}
Vec2 sub(Vec2 a,Vec2 b){return {a.x-b.x,a.y-b.y};}
double gray(const Frame& f,Vec2 p) {
    const int x=static_cast<int>(std::lround(p.x)),y=static_cast<int>(std::lround(p.y));
    if(x<0||x>=f.width||y<0||y>=f.height)return -1;
    const auto* rgb=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
    return (rgb[0]*77+rgb[1]*150+rgb[2]*29)/256.0;
}
// A thin ridge must have contrast on BOTH sides, excluding broad fill/effects.
bool ridge(const Frame& f,Vec2 p,Vec2 transverse) {
    const double v=gray(f,p),a=gray(f,{p.x-transverse.x*4,p.y-transverse.y*4}),
        b=gray(f,{p.x+transverse.x*4,p.y+transverse.y*4});
    return a>=0&&b>=0&&v>=80&&v-a>=18&&v-b>=18;
}
}
void GameLineTracker::reset(){tracks_.clear();context_={};}
void GameLineTracker::update(std::vector<LineCandidate>& lines,const SceneContext& c) {
    if(lines.size()>16)throw std::invalid_argument("line tracking capacity");
    if(c.epoch!=context_.epoch||c.generation!=context_.generation||c.geometry!=context_.geometry||
       c.width!=context_.width||c.height!=context_.height||c.rotation!=context_.rotation||
       c.capture_ns<=context_.capture_ns||c.capture_ns-context_.capture_ns>=100'000'000) tracks_.clear();
    context_=c;
    std::erase_if(tracks_,[&](const auto& t){return c.capture_ns-t.time>=90'000'000;});
    struct Pair{std::size_t observed,prior;double cost;};std::vector<Pair> pairs;pairs.reserve(256);
    for(std::size_t i=0;i<lines.size();++i)for(std::size_t j=0;j<tracks_.size();++j) {
        const auto& a=lines[i];const auto& t=tracks_[j];const auto& b=t.line;
        const double dt=(c.capture_ns-t.time)/1e9;
        const Vec2 expected{b.center.x+b.velocity.x*dt,b.center.y+b.velocity.y*dt};
        const double orientation=std::abs(dot(a.tangent,b.tangent));
        const double across=std::abs(dot(sub(a.center,expected),{-a.tangent.y,a.tangent.x}));
        if(orientation<.90||across>64||std::min(a.length,b.length)<std::max(a.length,b.length)*.45)continue;
        pairs.push_back({i,j,across+40*(1-orientation)});
    }
    std::sort(pairs.begin(),pairs.end(),[](const auto& a,const auto& b){return a.cost<b.cost;});
    std::vector<int> assigned(lines.size(),-1);std::vector<bool> used(tracks_.size());
    for(const auto& p:pairs) {
        if(assigned[p.observed]>=0||used[p.prior])continue;
        const bool contested=std::any_of(pairs.begin(),pairs.end(),[&](const auto& q){
            return ((q.observed==p.observed&&q.prior!=p.prior)||(q.prior==p.prior&&q.observed!=p.observed))&&q.cost<=p.cost+3;
        });
        if(contested){lines[p.observed].association_valid=false;continue;}
        assigned[p.observed]=static_cast<int>(p.prior);used[p.prior]=true;
    }
    for(std::size_t i=0;i<lines.size();++i) {
        auto& line=lines[i];line.observed_ns=c.capture_ns;
        if(assigned[i]>=0) {
            auto& prior=tracks_[assigned[i]];const auto old=prior.line;const double dt=(c.capture_ns-prior.time)/1e9;
            if(dot(line.tangent,old.tangent)<0){line.tangent.x=-line.tangent.x;line.tangent.y=-line.tangent.y;}
            line.track_id=old.track_id;
            // Line center can slide along an indistinguishable segment; only
            // normal displacement is identifiable from the current line.
            const Vec2 n{-line.tangent.y,line.tangent.x};const double d=dot(sub(line.center,old.center),n);
            line.velocity={n.x*d/dt,n.y*d/dt};
            line.angular_velocity=std::atan2(old.tangent.x*line.tangent.y-old.tangent.y*line.tangent.x,
                                            dot(old.tangent,line.tangent))/dt;
            prior={line,c.capture_ns};
        } else {
            line.track_id=++next_id_;line.velocity={};line.angular_velocity=0;
            if(tracks_.size()<16)tracks_.push_back({line,c.capture_ns});
            else line.association_valid=false;
        }
    }
}
std::optional<NoteCandidate> observe_held_outline(const Frame& f,const NoteCandidate& anchor,const LineCandidate& line) {
    const double anchor_distance=dot(sub(anchor.center,line.center),{-line.tangent.y,line.tangent.x});
    // A recent approaching front can be clipped by the hit effect before its
    // descriptor reaches the line. It may search the current attached rails,
    // but a separated body still fails the same-frame attachment checks.
    if(!anchor.rails_geometry||(!anchor.head_on_line&&!anchor.held_body_evidence&&std::abs(anchor_distance)>48)||!line.association_valid||
       anchor.width<f.width*.035||anchor.width>f.width*.22||
       std::abs(dot(anchor.tangent,line.tangent))<.95)return {};
    const Vec2 u=line.tangent,n{-u.y,u.x};
    const double d=dot(sub(anchor.center,line.center),n);
    const Vec2 projected{anchor.center.x-n.x*d,anchor.center.y-n.y*d};
    if(anchor.held_body_evidence&&!anchor.head_on_line) {
        // A hit-effect box can reconnect old rails to the line after the
        // actual body front has moved away. Re-entry needs current interior
        // support across two full rows, not those decorative side edges.
        for(const int depth:{6,12}) {
            int interior=0;for(int k=-3;k<=3;++k)
                interior+=gray(f,{projected.x+u.x*k*anchor.width*.13-n.x*depth,
                                  projected.y+u.y*k*anchor.width*.13-n.y*depth})>=80;
            if(interior<5)return {};
        }
    }
    // A terminal cap needs two short attached side rails BELOW it and no
    // continuing side rails ABOVE it. This excludes the judgment line and
    // internal hit-effect rectangles, even when their color has changed.
    for(int shift=-48;shift<=48;shift+=2)for(int depth=-12;depth<=12;++depth) {
        const Vec2 tail{projected.x+u.x*shift+n.x*depth,projected.y+u.y*shift+n.y*depth};
        int closure=0;
        for(int k=-3;k<=3;++k)closure+=ridge(f,{tail.x+u.x*k*anchor.width*.13,
                                                             tail.y+u.y*k*anchor.width*.13},n);
        if(closure<5||ridge(f,{tail.x+u.x*anchor.width*.65,tail.y+u.y*anchor.width*.65},n)||
           ridge(f,{tail.x-u.x*anchor.width*.65,tail.y-u.y*anchor.width*.65},n))continue;
        int last=0;bool valid=true;
        for(const int side:{-1,1}) {
            int below=0,above=0;
            for(const int offset:{3,6,9,12}) {
                const Vec2 rail{tail.x+u.x*side*anchor.width*.5,tail.y+u.y*side*anchor.width*.5};
                bool b=false,a=false;
                for(int lateral=-3;lateral<=3;++lateral) {
                    b=b||ridge(f,{rail.x+u.x*lateral+n.x*offset,rail.y+u.y*lateral+n.y*offset},u);
                    a=a||ridge(f,{rail.x+u.x*lateral-n.x*offset,rail.y+u.y*lateral-n.y*offset},u);
                }
                if(b){++below;last=std::max(last,offset);}above+=a;
            }
            if(below<2||above>0){valid=false;break;}
        }
        if(valid) {
            auto note=anchor;note.tangent=u;note.tail=tail;note.height=last;note.head_on_line=true;
            note.center={tail.x+n.x*last,tail.y+n.y*last};
            note.outline_evidence=true;note.direct_rails_evidence=false;return note;
        }
    }
    struct Section{double left,right,center;};std::vector<Section> sections;
    // Multiple current paired sections away from the hit effect. Search the
    // measured body's local ROI, not all bright geometry in the frame.
    for(const int depth:{6,12,24,40,64,88}) {
        std::vector<double> edges;edges.reserve(32);
        const int radius=static_cast<int>(std::ceil(anchor.width*.5+48));
        for(int a=-radius;a<=radius;++a) {
            const Vec2 p{projected.x-n.x*depth+u.x*a,projected.y-n.y*depth+u.y*a};
            if(ridge(f,p,u)&&edges.size()<32) {
                if(edges.empty()||a-edges.back()>4)edges.push_back(a);
            }
        }
        std::optional<Section> best;double score=1e9;bool ambiguous=false;
        for(std::size_t a=0;a<edges.size();++a)for(std::size_t b=a+1;b<edges.size();++b) {
            const double w=edges[b]-edges[a],middle=(edges[a]+edges[b])/2;
            // The birth descriptor measures the colored core; held pixels
            // expose the outer rails instead (live 136px core / 154px rails).
            if(std::abs(w-anchor.width)>std::max(8.0,anchor.width*.20)||std::abs(middle)>48)continue;
            const double cost=std::abs(w-anchor.width)+std::abs(middle)*.25;
            if(cost<score-1){best=Section{edges[a],edges[b],middle};score=cost;ambiguous=false;}
            else if(std::abs(cost-score)<=1)ambiguous=true;
        }
        if(best&&!ambiguous)sections.push_back(*best);
    }
    if(sections.size()<2)return {};
    std::sort(sections.begin(),sections.end(),[](const auto& a,const auto& b){return a.center<b.center;});
    const auto paired=sections[sections.size()/2];
    int consistent=0;for(const auto& a:sections)if(std::abs(a.left-paired.left)<=4&&std::abs(a.right-paired.right)<=4)++consistent;
    if(consistent<2)return {};
    for(const double along:{paired.left,paired.right}) {
        bool attached=false;
        // Before the first observed on-line head, distant rails at 10/14px
        // are insufficient: they would pull a still-visible approaching
        // front onto the line and corrupt its crossing velocity.
        const std::array<int,3> depths=anchor.head_on_line?std::array<int,3>{6,10,14}:
            std::array<int,3>{2,3,4};
        for(const int depth:depths)for(int offset=-3;offset<=3&&!attached;++offset)
            attached=ridge(f,{projected.x+u.x*(along+offset)-n.x*depth,
                              projected.y+u.y*(along+offset)-n.y*depth},u);
        if(!attached)return {};
    }
    NoteCandidate note=anchor;note.tangent=u;note.center={projected.x+u.x*paired.center,projected.y+u.y*paired.center};
    note.width=paired.right-paired.left;note.head_on_line=true;note.outline_evidence=true;note.direct_rails_evidence=false;note.tail.reset();
    std::array<int,2> ends{};bool clipped=false;
    for(int side=0;side<2;++side) {
        int last=0,gap=0,support=0;
        for(int depth=2;depth<=static_cast<int>(std::hypot(f.width,f.height));depth+=2) {
            const double along=side?paired.right:paired.left;
            const Vec2 base{projected.x+u.x*along-n.x*depth,projected.y+u.y*along-n.y*depth};
            if(base.x<4||base.x>=f.width-4||base.y<f.height*.10||base.y>=f.height-4){clipped=true;break;}
            bool found=false;for(int offset=-3;offset<=3&&!found;++offset)
                found=ridge(f,{base.x+u.x*offset,base.y+u.y*offset},u);
            if(found){last=depth;gap=0;++support;}else if((gap+=2)>12)break;
        }
        if(support<3||last<6)return {};
        ends[side]=last;
    }
    if(std::abs(ends[0]-ends[1])>16)return {};
    note.height=(ends[0]+ends[1])/2.0;
    // A tail must be a CURRENT transverse closing edge joining both rails.
    if(!clipped) {
        const Vec2 tail{note.center.x-n.x*note.height,note.center.y-n.y*note.height};
        int best_closure=0,best_offset=0;
        for(int offset=-4;offset<=4;++offset) {
            int closure=0;for(int k=-3;k<=3;++k)
                closure+=ridge(f,{tail.x+u.x*k*note.width*.13+n.x*offset,
                                  tail.y+u.y*k*note.width*.13+n.y*offset},n);
            if(closure>best_closure){best_closure=closure;best_offset=offset;}
        }
        if(best_closure>=5)note.tail=Vec2{tail.x+n.x*best_offset,tail.y+n.y*best_offset};
    }
    if(!clipped&&!note.tail&&note.height<std::min(96.0,anchor.height*.4))return {};
    return note;
}
std::optional<NoteCandidate> observe_moving_held_front(const Frame& f,const NoteCandidate& anchor,const LineCandidate& line) {
    if(!anchor.rails_geometry||(!anchor.head_on_line&&!anchor.held_body_evidence)||
       !line.association_valid||anchor.width<f.width*.035||anchor.width>f.width*.22||
       std::abs(dot(anchor.tangent,line.tangent))<.95)return {};
    const Vec2 u=line.tangent,n{-u.y,u.x};
    std::optional<NoteCandidate> best;double best_cost=1e9;bool ambiguous=false;
    // A local CURRENT leading luminance edge must span the interior of both
    // measured rails. Bright particles and internal color transitions are
    // excluded by requiring dark outside pixels across the same span.
    for(int shift=-48;shift<=48;shift+=2)for(int forward=-48;forward<=48;++forward) {
        const Vec2 front{anchor.center.x+u.x*shift+n.x*forward,anchor.center.y+u.y*shift+n.y*forward};
        if(std::abs(dot(sub(front,line.center),n))<=12)continue;
        int closure=0;
        for(int k=-3;k<=3;++k) {
            const Vec2 p{front.x+u.x*k*anchor.width*.13,front.y+u.y*k*anchor.width*.13};
            const double inside=gray(f,{p.x-n.x*4,p.y-n.y*4}),outside=gray(f,{p.x+n.x*4,p.y+n.y*4});
            closure+=inside>=80&&outside>=0&&outside<65&&inside-outside>=18;
        }
        if(closure<5)continue;
        // The coarse +/-4 contrast test admits several adjacent rows. Locate
        // their actual current luminance termination before choosing the touch
        // point, so the search cost cannot pull the contact deeper each frame.
        std::array<int,7> edge_offsets{};int edge_count=0;
        for(int k=-3;k<=3;++k) {
            const Vec2 p{front.x+u.x*k*anchor.width*.13,front.y+u.y*k*anchor.width*.13};
            for(int offset=-4;offset<=4;++offset) {
                const Vec2 q{p.x+n.x*offset,p.y+n.y*offset};
                if(gray(f,{q.x-n.x,q.y-n.y})>=80&&gray(f,{q.x+n.x,q.y+n.y})<65) {
                    edge_offsets[edge_count++]=offset;break;
                }
            }
        }
        if(edge_count<5)continue;
        std::sort(edge_offsets.begin(),edge_offsets.begin()+edge_count);
        const int edge_offset=edge_offsets[edge_count/2];
        struct Pair {double left,right;};std::array<Pair,4> sections{};int paired_sections=0;
        for(const int depth:{8,16,32,48}) {
            bool both=true;std::array<double,2> edges{};int index=0;
            for(const int side:{-1,1}) {
                const Vec2 rail{front.x+u.x*side*anchor.width*.5-n.x*depth,
                                front.y+u.y*side*anchor.width*.5-n.y*depth};
                bool found=false;
                for(int radius=0;radius<=16&&!found;++radius)for(const int sign:{-1,1}) {
                    const int offset=sign*radius;
                    if(ridge(f,{rail.x+u.x*offset,rail.y+u.y*offset},u)) {
                        found=true;edges[index]=side*anchor.width*.5+offset;break;
                    }
                }
                ++index;
                both=both&&found;
            }
            if(both&&std::abs(edges[1]-edges[0]-anchor.width)<=std::max(8.0,anchor.width*.20))
                sections[paired_sections++]={edges[0],edges[1]};
        }
        if(paired_sections<3)continue;
        const auto paired=sections[paired_sections/2];int consistent=0;
        for(int i=0;i<paired_sections;++i)consistent+=std::abs(sections[i].left-paired.left)<=4&&
            std::abs(sections[i].right-paired.right)<=4;
        if(consistent<3)continue;
        bool continues=false;
        for(const double edge:{paired.left,paired.right}) {
            int ahead=0;
            for(const int depth:{8,16,24}) {
                bool found=false;for(int offset=-1;offset<=1&&!found;++offset)
                    found=ridge(f,{front.x+u.x*(edge+offset)+n.x*depth,
                                   front.y+u.y*(edge+offset)+n.y*depth},u);
                ahead+=found;
            }
            continues=continues||ahead>=2;
        }
        if(continues) {
            int filled_ahead=0;
            for(int k=-3;k<=3;++k)filled_ahead+=gray(f,{front.x+u.x*k*anchor.width*.13+n.x*24,
                                                                     front.y+u.y*k*anchor.width*.13+n.y*24})>=80;
            if(filled_ahead>=5)continue; // internal stripe; effect rails alone are insufficient
        }
        const double cost=std::abs(shift)*.25+std::abs(forward)*.1;
        if(cost>best_cost+1)continue;
        std::array<int,2> ends{};bool valid=true;
        for(int side=0;side<2;++side) {
            int last=0,gap=0,support=0;
            for(int depth=4;depth<=static_cast<int>(std::hypot(f.width,f.height));depth+=2) {
                const double edge=side?paired.right:paired.left;
                const Vec2 p{front.x+u.x*edge-n.x*depth,front.y+u.y*edge-n.y*depth};
                if(p.x<4||p.x>=f.width-4||p.y<f.height*.10||p.y>=f.height-4)break;
                bool found=false;for(int offset=-3;offset<=3&&!found;++offset)
                    found=ridge(f,{p.x+u.x*offset,p.y+u.y*offset},u);
                if(found){last=depth;gap=0;++support;}else if((gap+=2)>12)break;
            }
            if(support<3||last<96){valid=false;break;}ends[side]=last;
        }
        if(!valid||std::abs(ends[0]-ends[1])>16)continue;
        const double middle=(paired.left+paired.right)*.5;
        auto current=anchor;current.center={front.x+u.x*middle+n.x*(edge_offset-3),
                                           front.y+u.y*middle+n.y*(edge_offset-3)};
        current.width=paired.right-paired.left;
        current.tangent=u;current.height=(ends[0]+ends[1])/2.0-4;
        current.tail.reset();current.head_on_line=false;current.held_body_evidence=true;
        current.outline_evidence=true;current.direct_rails_evidence=false;
        if(best&&cost>=best_cost-1) {
            if(std::hypot(current.center.x-best->center.x,current.center.y-best->center.y)>8)ambiguous=true;
            continue;
        }
        best=current;best_cost=cost;ambiguous=false;
    }
    return ambiguous?std::nullopt:best;
}
}
