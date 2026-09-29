#include "pas/game_motion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
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
// At most 16 observations and 16 live line tracks. Dummy columns represent
// genuinely unmatched observations, while a forbidden edge tests whether an
// alternative *feasible complete assignment* is nearly as good.
struct LineAssignment {double cost=0;std::vector<int> prior;};
LineAssignment assign_lines(const std::vector<std::vector<double>>& costs,
                            std::size_t priors,int forbidden_row=-1,int forbidden_prior=-1) {
    const int rows=static_cast<int>(costs.size()),cols=static_cast<int>(priors)+rows;
    constexpr double absent=1e6,birth=80;
    std::vector<double> u(rows+1),v(cols+1);
    std::vector<int> p(cols+1),way(cols+1);
    for(int row=1;row<=rows;++row) {
        p[0]=row;int col=0;std::vector<double> minv(cols+1,absent);
        std::vector<bool> used(cols+1);
        do {
            used[col]=true;const int current=p[col];double delta=absent;int next=0;
            for(int j=1;j<=cols;++j) if(!used[j]) {
                const double edge=j<=static_cast<int>(priors)?
                    ((current-1==forbidden_row&&j-1==forbidden_prior)?absent:costs[current-1][j-1]):birth;
                const double reduced=edge-u[current]-v[j];
                if(reduced<minv[j]) {minv[j]=reduced;way[j]=col;}
                if(minv[j]<delta) {delta=minv[j];next=j;}
            }
            for(int j=0;j<=cols;++j) if(used[j]) {u[p[j]]+=delta;v[j]-=delta;}
                else minv[j]-=delta;
            col=next;
        } while(p[col]);
        do {const int previous=way[col];p[col]=p[previous];col=previous;} while(col);
    }
    LineAssignment result;result.prior.assign(rows,-1);
    for(int j=1;j<=cols;++j) if(p[j]) {
        const int row=p[j]-1;
        if(j<=static_cast<int>(priors)&&costs[row][j-1]<absent&&
           !(row==forbidden_row&&j-1==forbidden_prior)) {
            result.prior[row]=j-1;result.cost+=costs[row][j-1];
        } else result.cost+=birth;
    }
    return result;
}
}
bool current_line_ridge_support(const Frame& f,const LineCandidate& line) {
    if(!std::isfinite(line.center.x)||!std::isfinite(line.center.y)||!std::isfinite(line.tangent.x)||
       !std::isfinite(line.tangent.y)||!std::isfinite(line.thickness)||
       !std::isfinite(line.length)||line.length<f.width*.5||line.thickness>6||line.thickness<1||
       std::abs(std::hypot(line.tangent.x,line.tangent.y)-1)>.01)return false;
    const Vec2 u=line.tangent,n{-u.y,u.x};int supported=0;
    for(int k=-4;k<=4;++k) {
        const double along=k*line.length*.10;bool found=false;
        for(int shift=-2;shift<=2&&!found;++shift)
            found=ridge(f,{line.center.x+u.x*along+n.x*shift,line.center.y+u.y*along+n.y*shift},n);
        supported+=found;
    }
    return supported>=7;
}
std::optional<LineCandidate> observe_current_line_extent(const Frame& f,const LineCandidate& seed) {
    if(!std::isfinite(seed.center.x)||!std::isfinite(seed.center.y)||!std::isfinite(seed.tangent.x)||
       !std::isfinite(seed.tangent.y)||!std::isfinite(seed.length)||!std::isfinite(seed.thickness)||
       seed.length<f.width*.24||seed.thickness>6||seed.thickness<1||
       std::abs(std::hypot(seed.tangent.x,seed.tangent.y)-1)>.01)return {};
    const Vec2 u=seed.tangent,n{-u.y,u.x};const int radius=static_cast<int>(std::ceil(std::hypot(f.width,f.height)));
    int begin=0,last=0;bool have=false;std::optional<LineCandidate> best;
    const auto finish=[&] {
        if(!have||begin>0||last<0||last-begin<f.width*.5)return;
        auto line=seed;const double middle=(begin+last)/2.0;
        line.center={seed.center.x+u.x*middle,seed.center.y+u.y*middle};line.length=last-begin;
        if(current_line_ridge_support(f,line)&&(!best||line.length>best->length))best=line;
    };
    // A note can interrupt a thin line in any orientation. Measure both
    // current fragments along the PCA seed; never extrapolate invisible ends.
    // One bounded scan, 4 px samples, gaps <=22% width, no retained image.
    for(int along=-radius;along<=radius;along+=4) {
        const Vec2 p{seed.center.x+u.x*along,seed.center.y+u.y*along};
        bool supported=false;
        if(p.x>=4&&p.x<f.width-4&&p.y>=f.height*.12&&p.y<f.height*.95)
            for(int shift=-2;shift<=2&&!supported;++shift)
                supported=ridge(f,{p.x+n.x*shift,p.y+n.y*shift},n);
        if(supported) {
            if(have&&along-last>f.width*.22){finish();have=false;}
            if(!have){begin=along;have=true;}last=along;
        }
    }
    finish();return best;
}
void GameLineTracker::reset(){tracks_.clear();pending_births_.clear();context_={};}
void GameLineTracker::fit_motion(Track& track,LineCandidate& current,Nanoseconds time) {
    current.velocity={};current.angular_velocity=0;current.motion_valid=false;
    const Pose pose{current.center,current.tangent,time};
    while(!track.poses.empty()&&time-track.poses.front().time>90'000'000)track.poses.pop_front();
    if(!track.poses.empty()&&time-track.bucket_start<10'000'000)track.poses.back()=pose;
    else {track.poses.push_back(pose);track.bucket_start=time;}
    while(track.poses.size()>6)track.poses.pop_front();
    current.motion_samples=static_cast<int>(track.poses.size());
    current.motion_span_ns=time-track.poses.front().time;
    current.motion_residual=current.angular_residual=0;
    if(current.motion_samples<3||current.motion_span_ns<30'000'000)return;
    // A cropped segment's center may slide along a featureless line. Fit
    // each measured line equation at the CURRENT reference point, rather
    // than treating its endpoints/center as identifiable material points.
    std::array<double,6> ts{},offsets{},angles{};int i=0;
    double st=0,sd=0,sa=0,tt=0,td=0,ta=0;
    for(const auto& p:track.poses) {
        const double t=(p.time-time)/1e9;
        const double d=dot(sub(p.center,current.center),{-p.tangent.y,p.tangent.x});
        const double a=std::atan2(current.tangent.x*p.tangent.y-current.tangent.y*p.tangent.x,
                                 dot(current.tangent,p.tangent));
        ts[i]=t;offsets[i]=d;angles[i++]=a;
        st+=t;sd+=d;sa+=a;tt+=t*t;td+=t*d;ta+=t*a;
    }
    const double count=current.motion_samples,den=count*tt-st*st;
    if(den<=1e-12)return;
    const double velocity=(count*td-st*sd)/den,omega=(count*ta-st*sa)/den;
    const double intercept=(sd-velocity*st)/count,angle_intercept=(sa-omega*st)/count;
    for(int k=0;k<i;++k) {
        current.motion_residual=std::max(current.motion_residual,std::abs(offsets[k]-intercept-velocity*ts[k]));
        current.angular_residual=std::max(current.angular_residual,std::abs(angles[k]-angle_intercept-omega*ts[k]));
    }
    if(!std::isfinite(velocity)||!std::isfinite(omega)||std::abs(velocity)>2000||std::abs(omega)>12||
       current.motion_residual>2||current.angular_residual>.015)return;
    current.motion_valid=true;current.angular_velocity=omega;
    current.velocity={-current.tangent.y*velocity,current.tangent.x*velocity};
}
void GameLineTracker::update(std::vector<LineCandidate>& lines,const SceneContext& c) {
    if(lines.size()>16)throw std::invalid_argument("line tracking capacity");
    if(c.epoch!=context_.epoch||c.generation!=context_.generation||c.geometry!=context_.geometry||
       c.width!=context_.width||c.height!=context_.height||c.rotation!=context_.rotation||
       c.capture_ns<=context_.capture_ns||c.capture_ns-context_.capture_ns>=100'000'000) {
        tracks_.clear();pending_births_.clear();
    }
    context_=c;
    std::erase_if(tracks_,[&](const auto& t){return c.capture_ns-t.time>=90'000'000;});
    constexpr double absent=1e6;
    std::vector<std::vector<double>> costs(lines.size(),std::vector<double>(tracks_.size(),absent));
    for(std::size_t i=0;i<lines.size();++i)for(std::size_t j=0;j<tracks_.size();++j) {
        const auto& a=lines[i];const auto& t=tracks_[j];const auto& b=t.line;
        const double dt=(c.capture_ns-t.time)/1e9;
        const double angle=b.motion_valid?b.angular_velocity*dt:0;
        const Vec2 direction{b.tangent.x*std::cos(angle)-b.tangent.y*std::sin(angle),
                             b.tangent.x*std::sin(angle)+b.tangent.y*std::cos(angle)};
        const Vec2 expected{b.center.x+(b.motion_valid?b.velocity.x*dt:0),
                            b.center.y+(b.motion_valid?b.velocity.y*dt:0)};
        const double orientation=std::abs(dot(a.tangent,direction));
        const double across=std::abs(dot(sub(a.center,expected),{-a.tangent.y,a.tangent.x}));
        if(orientation<.90||across>64||std::min(a.length,b.length)<std::max(a.length,b.length)*.45)continue;
        costs[i][j]=across+40*(1-orientation);
    }
    const auto assignment=assign_lines(costs,tracks_.size());
    const auto& assigned=assignment.prior;
    std::vector<bool> contested(lines.size());
    for(std::size_t i=0;i<lines.size();++i) if(assigned[i]>=0) {
        const auto alternative=assign_lines(costs,tracks_.size(),static_cast<int>(i),assigned[i]);
        if(alternative.cost<=assignment.cost+3) {
            contested[i]=true;
            for(std::size_t j=0;j<lines.size();++j)
                if(alternative.prior[j]!=assigned[j])contested[j]=true;
        }
    }
    // A valid predecessor can be occupied by another observation. Its losing
    // neighbor is contested, not a new confirmed lineage. In particular a
    // one-frame duplicate must not poison future single-line observations.
    for(std::size_t i=0;i<lines.size();++i) if(assigned[i]<0)
        for(std::size_t k=0;k<lines.size();++k) if(assigned[k]>=0&&
            costs[i][assigned[k]]<=costs[k][assigned[k]]+3)contested[i]=true;
    std::vector<PendingBirth> next_pending;
    std::vector<bool> pending_used(pending_births_.size());
    for(std::size_t i=0;i<lines.size();++i) {
        auto& line=lines[i];line.observed_ns=c.capture_ns;
        line.association_valid=line.association_valid&&!contested[i];
        if(assigned[i]>=0) {
            auto& prior=tracks_[assigned[i]];const auto old=prior.line;
            if(dot(line.tangent,old.tangent)<0){line.tangent.x=-line.tangent.x;line.tangent.y=-line.tangent.y;}
            line.track_id=old.track_id;
            fit_motion(prior,line,c.capture_ns);
            prior.line=line;prior.time=c.capture_ns;
        } else {
            bool birth=!contested[i];
            if(contested[i]) {
                // Exact duplicates supply no separable geometry. A distinct
                // near neighbor must persist for three consecutive frames
                // before it may become another lineage. A transient twin
                // therefore cannot regenerate a track each frame.
                double separation=1e9;
                for(std::size_t k=0;k<lines.size();++k) if(assigned[k]>=0)
                    separation=std::min(separation,std::abs(dot(sub(line.center,lines[k].center),
                        {-line.tangent.y,line.tangent.x})));
                if(separation>=1.5) {
                    int count=1;
                    for(std::size_t p=0;p<pending_births_.size();++p) if(!pending_used[p]&&
                        c.capture_ns-pending_births_[p].time<90'000'000&&
                        std::abs(dot(line.tangent,pending_births_[p].line.tangent))>.98&&
                        std::abs(dot(sub(line.center,pending_births_[p].line.center),
                            {-line.tangent.y,line.tangent.x}))<=4) {
                        pending_used[p]=true;count=pending_births_[p].consecutive+1;break;
                    }
                    if(count>=3)birth=true;
                    else if(next_pending.size()<16)next_pending.push_back({line,c.capture_ns,count});
                }
            }
            if(birth&&tracks_.size()<16) {
                line.track_id=++next_id_;Track fresh;fit_motion(fresh,line,c.capture_ns);
                fresh.line=line;fresh.time=c.capture_ns;
                tracks_.push_back(std::move(fresh));
                if(contested[i])line.association_valid=false;
            } else {line.track_id=0;line.association_valid=false;}
        }
    }
    pending_births_=std::move(next_pending);
}
std::optional<NoteCandidate> observe_held_outline(const Frame& f,const NoteCandidate& anchor,const LineCandidate& line) {
    const double anchor_distance=dot(sub(anchor.center,line.center),{-line.tangent.y,line.tangent.x});
    // A recent approaching front can be clipped by the hit effect before its
    // descriptor reaches the line. It may search the current attached rails,
    // but a separated body still fails the same-frame attachment checks.
    if(!anchor.rails_geometry||(!anchor.head_on_line&&!anchor.held_body_evidence&&std::abs(anchor_distance)>48)||!line.association_valid||
       anchor.width<f.width*.035||anchor.width>f.width*.22)return {};
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
    // After a genuinely observed tail approaches the line, keep its search
    // window wide enough for the second fresh confirmation even if it moves
    // across the line by more than one narrow 12 px band per frame. The
    // closure and two attached rails below still come from current pixels.
    int tail_window=12;
    if(anchor.tail) {
        const double previous_tail_distance=dot(sub(*anchor.tail,line.center),n);
        if(previous_tail_distance>-40&&previous_tail_distance<40)tail_window=36;
    }
    for(int shift=-48;shift<=48;shift+=2)for(int depth=-tail_window;depth<=tail_window;++depth) {
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
            note.outline_evidence=true;note.direct_rails_evidence=false;note.held_body_patch=false;return note;
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
    note.width=paired.right-paired.left;note.head_on_line=true;note.outline_evidence=true;note.direct_rails_evidence=false;note.held_body_patch=false;note.tail.reset();
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
       !line.association_valid||anchor.width<f.width*.035||anchor.width>f.width*.22)return {};
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
        // A front can move away from the line while its current body remains
        // the contact region. Once the trailing end also passes on the
        // measured front side, those pixels cannot renew that contact.
        const double front_distance=dot(sub(current.center,line.center),n);
        const double tail_distance=front_distance-current.height;
        current.tail.reset();
        if(tail_distance>=-32&&tail_distance<=32) {
            const Vec2 measured_tail{current.center.x-n.x*current.height,
                                     current.center.y-n.y*current.height};
            int best_closure=0,best_offset=0;
            for(int offset=-4;offset<=4;++offset) {
                int closure=0;
                for(int k=-3;k<=3;++k)
                    closure+=ridge(f,{measured_tail.x+u.x*k*current.width*.13+n.x*offset,
                                      measured_tail.y+u.y*k*current.width*.13+n.y*offset},n);
                if(closure>best_closure){best_closure=closure;best_offset=offset;}
            }
            if(best_closure>=5)
                current.tail=Vec2{measured_tail.x+n.x*best_offset,measured_tail.y+n.y*best_offset};
        }
        if(front_distance>0&&tail_distance>2&&
           !(current.tail&&tail_distance>=-2&&tail_distance<=32))continue;
        current.head_on_line=false;current.held_body_evidence=true;current.held_body_patch=false;
        current.outline_evidence=true;current.direct_rails_evidence=false;
        if(best&&cost>=best_cost-1) {
            if(std::hypot(current.center.x-best->center.x,current.center.y-best->center.y)>8)ambiguous=true;
            continue;
        }
        best=current;best_cost=cost;ambiguous=false;
    }
    return ambiguous?std::nullopt:best;
}
std::optional<NoteCandidate> observe_held_body_patch(const Frame& f,const NoteCandidate& anchor,const LineCandidate& line) {
    if(!anchor.rails_geometry||(!anchor.head_on_line&&!anchor.held_body_evidence)||
       !line.association_valid||anchor.width<f.width*.035||anchor.width>f.width*.22)return {};
    const Vec2 u=line.tangent,n{-u.y,u.x};
    std::optional<NoteCandidate> best;double best_cost=1e9;
    // An already held body can remain visible while its front is hidden by
    // the finger effect. Pick only a currently filled interior inside both
    // measured rails. This region is not a head/tail observation or a birth.
    for(int shift=-32;shift<=32;shift+=2)for(int forward=-48;forward<=16;forward+=4) {
        const double cost=std::abs(shift)*.5+std::abs(forward)*.3;
        if(cost>=best_cost)continue;
        const Vec2 point{anchor.center.x+u.x*shift+n.x*forward,
                         anchor.center.y+u.y*shift+n.y*forward};
        if(std::abs(dot(sub(point,line.center),n))<=12)continue;
        bool filled=true;
        for(const int depth:{0,8,24}) {
            int count=0;for(int k=-3;k<=3;++k)
                count+=gray(f,{point.x+u.x*k*anchor.width*.13-n.x*depth,
                               point.y+u.y*k*anchor.width*.13-n.y*depth})>=80;
            filled=filled&&count>=6;
        }
        if(!filled)continue;
        std::array<Vec2,4> pairs{};bool valid=true;int section=0;
        for(const int depth:{0,8,24,40}) {
            std::array<double,2> edges{};int index=0;
            for(const int side:{-1,1}) {
                bool found=false;
                for(int radius=0;radius<=16&&!found;++radius)for(const int sign:{-1,1}) {
                    const double along=side*anchor.width*.5+sign*radius;
                    if(ridge(f,{point.x+u.x*along-n.x*depth,point.y+u.y*along-n.y*depth},u)) {
                        edges[index]=along;found=true;break;
                    }
                }
                ++index;if(!found)valid=false;
            }
            if(!valid||std::abs(edges[1]-edges[0]-anchor.width)>std::max(8.0,anchor.width*.20)) {valid=false;break;}
            pairs[section++]={edges[0],edges[1]};
        }
        if(!valid)continue;
        const auto pair=pairs[1];
        for(const auto& other:pairs)if(std::abs(other.x-pair.x)>4||std::abs(other.y-pair.y)>4)valid=false;
        if(!valid)continue;
        std::array<int,2> ends{};
        for(int side=0;side<2;++side) {
            int last=0,gap=0,support=0;
            for(int depth=4;depth<=static_cast<int>(std::hypot(f.width,f.height));depth+=2) {
                const double edge=side?pair.y:pair.x;
                const Vec2 p{point.x+u.x*edge-n.x*depth,point.y+u.y*edge-n.y*depth};
                if(p.x<4||p.x>=f.width-4||p.y<f.height*.10||p.y>=f.height-4)break;
                bool found=false;for(int offset=-3;offset<=3&&!found;++offset)
                    found=ridge(f,{p.x+u.x*offset,p.y+u.y*offset},u);
                if(found){last=depth;gap=0;++support;}else if((gap+=2)>12)break;
            }
            if(support<3||last<96){valid=false;break;}ends[side]=last;
        }
        if(!valid||std::abs(ends[0]-ends[1])>16)continue;
        auto current=anchor;const auto middle=(pair.x+pair.y)*.5;
        current.center={point.x+u.x*middle,point.y+u.y*middle};current.width=pair.y-pair.x;
        current.tangent=u;current.height=(ends[0]+ends[1])*.5;current.tail.reset();
        const double patch_distance=dot(sub(current.center,line.center),n);
        const double tail_distance=patch_distance-current.height;
        if(patch_distance>0&&tail_distance>2)continue;
        current.rails_geometry=true;current.head_on_line=false;current.held_body_evidence=true;
        current.held_body_patch=true;current.outline_evidence=true;current.direct_rails_evidence=false;
        best=current;best_cost=cost;
    }
    return best;
}
}
