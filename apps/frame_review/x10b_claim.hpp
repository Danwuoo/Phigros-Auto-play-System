#pragma once
#include "pas/game.hpp"
#include <algorithm>
#include <cmath>
#include <span>
namespace pas::x10b {
// The caller supplies only same-frame, independently measured held outlines.
// Reject only a history-derived approaching FRONT whose side fill demonstrably
// continues through that front inside one compatible current held body.
// No identity merge, contact continuation, lease renewal or new action occurs.
inline bool current_interior_claim(const Frame& f,const NoteCandidate& note,
                                  const LineCandidate& line,std::span<const NoteCandidate> claimed){
    if(f.width<=0||f.height<=0||f.width>4096||f.height>4096||f.stride<f.width*3||
       f.rgb.size()!=static_cast<std::size_t>(f.stride)*f.height||!f.source_valid||
       !line.association_valid||line.observed_ns!=f.capture_complete_ns)return false;
    const auto valid=[&](const NoteCandidate& n){return n.kind==NoteKind::hold&&n.rails_geometry&&
        std::isfinite(n.center.x)&&std::isfinite(n.center.y)&&n.center.x>=0&&n.center.x<f.width&&n.center.y>=0&&n.center.y<f.height&&
        std::isfinite(n.width)&&n.width>0&&n.width<=4096&&std::isfinite(n.height)&&n.height>0&&n.height<=4096&&
        std::isfinite(n.tangent.x)&&std::isfinite(n.tangent.y)&&std::abs(std::hypot(n.tangent.x,n.tangent.y)-1)<.01;};
    if(!valid(note)||!note.recent_identity||!note.outline_evidence||note.direct_rails_evidence||note.head_on_line||note.held_body_evidence||claimed.size()>128)return false;
    if(!std::isfinite(line.tangent.x)||!std::isfinite(line.tangent.y)||std::abs(std::hypot(line.tangent.x,line.tangent.y)-1)>=.01)return false;
    const auto dot=[](Vec2 a,Vec2 b){return a.x*b.x+a.y*b.y;};
    if(std::abs(dot(note.tangent,line.tangent))<.995)return false;
    const bool interior=std::any_of(claimed.begin(),claimed.end(),[&](const NoteCandidate& used){
        if(!valid(used)||(!used.head_on_line&&!used.held_body_evidence)||dot(note.tangent,used.tangent)<.995)return false;
        const Vec2 delta{note.center.x-used.center.x,note.center.y-used.center.y};
        const double normal=-delta.x*used.tangent.y+delta.y*used.tangent.x;
        // Both ends of the +/-8 pixel continuity probe remain inside the body.
        return std::abs(note.width-used.width)<20&&std::abs(dot(delta,used.tangent))<=20&&
            normal>=-used.height+8&&normal<=-8;
    });
    if(!interior)return false;
    const Vec2 u=note.tangent,n{-u.y,u.x};
    // Same finite color/side-band witness as C36h fill_crosses_hold_front.
    // At most 60 probes per candidate, regardless of number of body claims.
    for(const int side:{-1,1}){
        bool continuous=false;
        for(const double fraction:{.38,.40,.42,.44,.46,.48}){
            bool supported=true;
            for(const int depth:{-8,-4,0,4,8}){
                const double px=note.center.x+side*fraction*note.width*u.x-depth*n.x;
                const double py=note.center.y+side*fraction*note.width*u.y-depth*n.y;
                if(px<0||py<0||px>=f.width-.5||py>=f.height-.5){supported=false;break;}
                const int x=static_cast<int>(std::lround(px)),y=static_cast<int>(std::lround(py));
                const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
                if(!(p[2]>145&&p[1]>130&&p[2]>p[0]+8)){supported=false;break;}
            }
            continuous=continuous||supported;
        }
        if(!continuous)return false;
    }
    return true;
}
}
