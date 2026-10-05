#include "contract.hpp"
#include <cmath>
#include <algorithm>
namespace pas::x10d_o {
namespace {
bool unit(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(std::hypot(v.x,v.y)-1)<.01;}
bool valid(const NoteCandidate& n){return n.kind==NoteKind::hold&&n.rails_geometry&&unit(n.tangent)&&std::isfinite(n.width)&&n.width>=16&&n.width<=4096&&std::isfinite(n.height)&&n.height>=16&&n.height<=4096&&std::isfinite(n.center.x)&&std::isfinite(n.center.y);}
}
Claim resolve(const Frame& f,const NoteCandidate& n,const LineCandidate& l,std::span<const Anchor> anchors){
 Claim out;
 if(f.width<=0||f.height<=0||f.width>4096||f.height>4096||f.stride!=f.width*3||
    f.rgb.size()!=static_cast<std::size_t>(f.stride)*f.height||!f.source_valid||!valid(n)||
    n.center.x<0||n.center.x>=f.width||n.center.y<0||n.center.y>=f.height||anchors.size()>128||
    !l.association_valid||!l.track_id||l.observed_ns!=f.capture_complete_ns||!unit(l.tangent)){
    out.reason="invalid_or_unknown_current_context";return out;
 }
 // Current two-sided fill, in NOTE local coordinates. No projected pose,
 // historical blue flag, gap bridge, deletion or new head suppression.
 const Vec2 u=n.tangent,v{-u.y,u.x};
 for(int depth=2;depth<=std::min(4096,static_cast<int>(n.height));++depth){
    bool paired=true;
    for(int side:{-1,1}){
      bool supported=false;
      for(double fraction:{.38,.40,.42,.44,.46,.48}){
        ++out.probes;
        double px=n.center.x+side*fraction*n.width*u.x-depth*v.x;
        double py=n.center.y+side*fraction*n.width*u.y-depth*v.y;
        if(px<0||py<0||px>=f.width-.5||py>=f.height-.5)continue;
        int x=static_cast<int>(std::lround(px)),y=static_cast<int>(std::lround(py));
        const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
        supported=supported||(p[2]>145&&p[1]>130&&p[2]>p[0]+8);
      }
      paired=paired&&supported;
    }
    if(!paired)break;
    out.measured_depth=depth;
 }
 if(out.measured_depth<16){out.reason="no_bilateral_current_body";return out;}
 out.pixel_support=true;std::size_t eligible=0;
 for(const auto& a:anchors){
   if(!a.id||!valid(a.note)||a.observed>f.capture_complete_ns||
      f.capture_complete_ns-a.observed>90'000'000||
      a.context.epoch!=f.epoch||a.context.generation!=f.generation||a.context.geometry!=f.geometry_version||
      a.context.width!=f.width||a.context.height!=f.height||a.context.rotation!=f.source_rotation)continue;
   const Vec2 d{n.center.x-a.note.center.x,n.center.y-a.note.center.y};
   const double along=d.x*u.x+d.y*u.y,normal=d.x*v.x+d.y*v.y;
   if(std::abs(a.note.width-n.width)>=20||std::abs(a.note.tangent.x*u.x+a.note.tangent.y*u.y)<.97||
      std::abs(along)>20||normal>8||normal < -a.note.height)continue;
   ++eligible;out.owner=a.id;
 }
 if(eligible!=1){out.owner.reset();out.reason=eligible?"multiple_recent_anchors_unknown":"no_recent_anchor_unknown";}
 else out.reason="BCC_v1_exclusive_owner_hypothesis";
 return out;
}
}
