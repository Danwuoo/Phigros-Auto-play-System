#include "bvi.hpp"
#include "contact_policy.hpp"
#include "relation_policy.hpp"
// Isolated cold v3 candidate, derived from research/bvi_cold_v2/bvi.cpp
// at a53a9b7021bcc900f498047f5cc017b42e4711b0. The frozen donor stays unchanged.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace bvi {
namespace {
constexpr std::uint64_t budget=6291456,seed=1469598103934665603ULL;
std::uint64_t hash(std::uint64_t h,std::uint8_t b){return (h^b)*1099511628211ULL;}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y};}Point mul(Point a,double k){return {a.x*k,a.y*k};}Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
Point u(double a){return {std::cos(a),std::sin(a)};}Point v(double a){return {-std::sin(a),std::cos(a)};}
bool finite(const Query&q){return std::isfinite(q.front.x)&&std::isfinite(q.front.y)&&std::isfinite(q.width)&&std::isfinite(q.depth)&&std::isfinite(q.angle)&&q.width>=4&&q.width<=4096&&q.depth>0&&q.depth<=4095;}
struct Pixel {int r{},g{},b{};bool in{};bool blue()const{return in&&r>=20&&r<=100&&g>=130&&g<=230&&b>=180&&b<=255&&b>=g+20;}bool white()const{return in&&r>=240&&g>=240&&b>=240;}bool yellow()const{return in&&r>=180&&g>=120&&b<=180;}bool black()const{return in&&r<=12&&g<=12&&b<=12;}};
Pixel pixel(const View&f,Point p,std::uint64_t&n){++n;int x=static_cast<int>(std::llround(p.x)),y=static_cast<int>(std::llround(p.y));if(x<0||y<0||x>=f.width||y>=f.height)return {};auto i=static_cast<std::size_t>(y)*f.stride+x*3;return {f.rgb[i],f.rgb[i+1],f.rgb[i+2],true};}
bool masked(Point p,std::span<const Line>lines){for(const auto&l:lines)if(std::abs(dot(sub(p,l.center),v(l.angle)))<=2.8&&std::abs(dot(sub(p,l.center),u(l.angle)))<=l.length/2)return true;return false;}
Point pixel_center(Point p){return {double(std::llround(p.x)),double(std::llround(p.y))};}
std::uint64_t signature(const Descriptor&d){std::uint64_t h=seed;const double a[]={d.q.front.x,d.q.front.y,d.q.width,d.q.angle,d.measured_depth};for(double n:a){std::array<std::uint8_t,8>b{};std::memcpy(b.data(),&n,8);for(auto c:b)h=hash(h,c);}for(bool x:{d.front_end,d.rear_end,d.left,d.right})h=hash(h,x);return h;}
}
std::string_view name(Support s){switch(s){case Support::supported:return "supported";case Support::occluded:return "occluded";case Support::invalid:return "invalid";default:return "absent";}}
bool capacity_valid(std::size_t r,std::size_t l,std::size_t m,std::uint64_t p){return r<=128&&l<=16&&m<=1048576&&p<=budget;}
Observation Candidate::extract(const View&f,std::span<const Query>queries,std::span<const Line>lines)const{
 Observation out;out.count=std::min<std::size_t>(queries.size(),128);
 auto invalid=[&](std::string_view reason){out.invalid=true;out.reason=reason;for(std::size_t i=0;i<out.count;++i)out.parts[i].body=out.parts[i].contact=Support::invalid;return out;};
 if(!f.key.source_valid||f.width<=0||f.height<=0||f.width>1280||f.height>720||f.stride<f.width*3||f.rgb.size()!=static_cast<std::size_t>(f.stride)*f.height)return invalid("frame/source invalid");
 if(!capacity_valid(queries.size(),lines.size(),metadata_bytes(),0))return invalid("capacity overflow");
 for(const auto&l:lines)if(!std::isfinite(l.angle)||!std::isfinite(l.center.x)||!std::isfinite(l.center.y)||!std::isfinite(l.length)||l.length<=0)return invalid("nonfinite line");
 // One borrowed current RGB frame only. Signature equality is approximate, not full past-byte equality.
 out.rgb_signature=seed;for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){auto i=static_cast<std::size_t>(y)*f.stride+x*3;for(int c=0;c<3;++c)out.rgb_signature=hash(out.rgb_signature,f.rgb[i+c]);++out.probes;}
 for(std::size_t i=0;i<queries.size();++i){auto&q=queries[i];if(!finite(q))return invalid("nonfinite/ROI bound");auto&d=out.parts[i];d.q=q;d.key=f.key;d.rgb_signature=out.rgb_signature;auto tu=u(q.angle),nv=v(q.angle);std::uint64_t start=out.probes;
  if(q.tap){auto p1=pixel(f,add(q.front,mul(nv,-3)),out.probes),p2=pixel(f,add(q.front,mul(nv,3)),out.probes);d.left=d.right=p1.blue()&&p2.blue();d.body=Support::absent;d.front_end=d.left;}
  else{
   int first=-1,last=-1,both=0;bool someLeft=false,someRight=false,railLeft=false,railRight=false;int breaks=0;
   int leftRun=0,rightRun=0;Point leftPrevious{},rightPrevious{};
   // A rail needs a short current white segment, not a measured-line crossing
   // or one white dot. All witnesses also need their same-depth blue interior.
   auto rail=[&](Point p,const Pixel&sample,bool blue,int&run,Point&previous,bool&seen){
    p=pixel_center(p);
    if(!sample.white()||!blue||masked(p,lines)){run=0;return;}
    if(!run||p.x!=previous.x||p.y!=previous.y)++run;
    previous=p;seen|=run>=2;
   };
   for(int k=0;k<=static_cast<int>(std::ceil(q.depth));++k){auto p=add(q.front,mul(nv,-k));auto a=pixel(f,add(p,mul(tu,-.35*q.width)),out.probes),b=pixel(f,add(p,mul(tu,.35*q.width)),out.probes);auto ra=pixel(f,add(p,mul(tu,-q.width/2)),out.probes),rb=pixel(f,add(p,mul(tu,q.width/2)),out.probes);auto mid=pixel(f,p,out.probes);
    someLeft|=a.blue();someRight|=b.blue();
    rail(add(p,mul(tu,-q.width/2)),ra,a.blue(),leftRun,leftPrevious,railLeft);
    rail(add(p,mul(tu,q.width/2)),rb,b.blue(),rightRun,rightPrevious,railRight);
    d.effect|=a.yellow()||b.yellow()||mid.yellow();
    bool blue=a.blue()&&b.blue();bool line=(a.white()&&masked(add(p,mul(tu,-.35*q.width)),lines))||(b.white()&&masked(add(p,mul(tu,.35*q.width)),lines));
    if(blue){if(first<0)first=k;last=k;both++;breaks=0;}else if(first>=0&&!line){if(++breaks>=12)break;}
    if(out.probes>budget)return invalid("probe overflow");
   }
   d.left=someLeft&&railLeft;d.right=someRight&&railRight;d.body=both>=8&&d.left&&d.right?Support::supported:Support::absent;
   // Filled union without rails still reports visible body, but never independent entry.
   if(both>=8&&!railLeft&&!railRight)d.body=Support::supported;
   d.measured_depth=first>=0?last-first:0;d.rear=add(q.front,mul(nv,-q.depth));
   // Within four current pixels, require separate bilateral blue and black
   // witnesses. Actual blue/black pixels remain evidence even inside the
   // conservative geometric line mask; white never supplies either color.
   // Search both sides: a fixed outer +4 can hit the line just as inner -4 can.
   auto endpoint=[&](Point e,int direction){
    bool inside=false,outside=false;
    for(int k=1;k<=4;++k){
     auto before=add(e,mul(nv,-direction*k)),after=add(e,mul(nv,direction*k));
     auto blp=add(before,mul(tu,-.35*q.width)),brp=add(before,mul(tu,.35*q.width));
     auto alp=add(after,mul(tu,-.35*q.width)),arp=add(after,mul(tu,.35*q.width));
     auto bl=pixel(f,blp,out.probes),br=pixel(f,brp,out.probes);
     auto al=pixel(f,alp,out.probes),ar=pixel(f,arp,out.probes);
     inside|=bl.blue()&&br.blue();
     outside|=al.black()&&ar.black();
    }
    return inside&&outside&&d.left&&d.right;
   };
   d.front_end=endpoint(q.front,1);d.rear_end=endpoint(d.rear,-1);
  }
  std::size_t eligible=0,geometric_body_lines=0;Support best=Support::absent;
  for(const auto&l:lines){auto ln=v(l.angle),lu=u(l.angle);double denom=dot(nv,ln);Point hit;
   if(q.tap){hit=q.front;if(std::abs(dot(sub(hit,l.center),ln))>4)continue;}else{if(std::abs(denom)<1e-6)continue;double k=-dot(sub(q.front,l.center),ln)/denom;hit=add(q.front,mul(nv,k));if(k>4||k<-q.depth-4)continue;}
   if(std::abs(dot(sub(hit,l.center),lu))>l.length/2||std::abs(dot(sub(hit,q.front),tu))>q.width/2)continue;
   // A second measured line cannot disappear from association just because
   // the first line occludes its blue support samples. Keep body intersections
   // competing; this supplies no pixel support or physical-owner identity.
   const double body_k=dot(sub(hit,q.front),nv);
   if(!q.tap&&body_k<=0&&body_k>=-q.depth)++geometric_body_lines;
   const auto sampled=sample_current_contact(f,q,l,hit,out.probes);
   const bool supported=sampled.supported,yellow=sampled.yellow;
   Support s=supported&&(q.tap||d.body==Support::supported)?Support::supported:(yellow?Support::occluded:Support::absent);if(s==Support::supported){eligible++;d.hit=hit;d.line_id=l.id;}if(s==Support::supported||best==Support::absent)best=s;
  }
  d.contact=best;d.line_unique=eligible==1&&(q.tap||geometric_body_lines<=1);d.probes=static_cast<std::uint32_t>(out.probes-start);d.signature=signature(d);if(out.probes>budget)return invalid("probe overflow");
 }
 observe_relation_context(out,lines);
 return out;
}
bool Candidate::retains(Ns t)const{for(std::size_t i=0;i<size_;++i)if(history_[i].key.capture==t)return true;return false;}
static_assert(sizeof(Descriptor)<=256);static_assert(Candidate::metadata_bytes()<=1048576);
}
