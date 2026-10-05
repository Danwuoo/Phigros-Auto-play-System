#include "bvi.hpp"
// Isolated cold v2 candidate, derived from research/x10d_o_bvi_build/bvi.cpp
// at acb27fdb095b82253ef9388eab52948a59663d02. The frozen donor stays unchanged.
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
double distance(const Descriptor&a,const Descriptor&b){if(std::abs(a.q.width-b.q.width)>4||std::abs(std::remainder(a.q.angle-b.q.angle,3.141592653589793))>0.6)return std::numeric_limits<double>::infinity();return std::hypot(a.q.front.x-b.q.front.x,a.q.front.y-b.q.front.y);}
bool contains(const Descriptor&a,const Descriptor&b){auto diff=sub(b.q.front,a.q.front);double s=dot(diff,u(a.q.angle)),d=dot(diff,v(a.q.angle));return std::abs(s)<=4&&d<=4&&d>=-a.measured_depth-4&&distance(a,b)<=128&&a.measured_depth>=b.measured_depth-4;}
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
   // witnesses. White line masks can remove a witness, never supply its color.
   // Search both sides: a fixed outer +4 can hit the line just as inner -4 can.
   auto endpoint=[&](Point e,int direction){
    bool inside=false,outside=false;
    for(int k=1;k<=4;++k){
     auto before=add(e,mul(nv,-direction*k)),after=add(e,mul(nv,direction*k));
     auto blp=add(before,mul(tu,-.35*q.width)),brp=add(before,mul(tu,.35*q.width));
     auto alp=add(after,mul(tu,-.35*q.width)),arp=add(after,mul(tu,.35*q.width));
     auto bl=pixel(f,blp,out.probes),br=pixel(f,brp,out.probes);
     auto al=pixel(f,alp,out.probes),ar=pixel(f,arp,out.probes);
     inside|=bl.blue()&&br.blue()&&!masked(pixel_center(blp),lines)&&!masked(pixel_center(brp),lines);
     outside|=al.black()&&ar.black()&&!masked(pixel_center(alp),lines)&&!masked(pixel_center(arp),lines);
    }
    return inside&&outside&&d.left&&d.right;
   };
   d.front_end=endpoint(q.front,1);d.rear_end=endpoint(d.rear,-1);
  }
  std::size_t eligible=0;Support best=Support::absent;
  for(const auto&l:lines){auto ln=v(l.angle),lu=u(l.angle);double denom=dot(nv,ln);Point hit;
   if(q.tap){hit=q.front;if(std::abs(dot(sub(hit,l.center),ln))>4)continue;}else{if(std::abs(denom)<1e-6)continue;double k=-dot(sub(q.front,l.center),ln)/denom;hit=add(q.front,mul(nv,k));if(k>4||k<-q.depth-4)continue;}
   if(std::abs(dot(sub(hit,l.center),lu))>l.length/2||std::abs(dot(sub(hit,q.front),tu))>q.width/2)continue;
   bool supported=false,yellow=false;
   for(int sign:{-1,1}){bool ok=true;for(int delta:q.tap?std::array<int,2>{3,3}:std::array<int,2>{4,6}){auto cp=add(hit,mul(ln,sign*delta));auto a=pixel(f,add(cp,mul(tu,-.35*q.width)),out.probes),b=pixel(f,add(cp,mul(tu,.35*q.width)),out.probes);yellow|=a.yellow()||b.yellow();ok&=a.blue()&&b.blue();}supported|=ok;}
   Support s=supported&&(q.tap||d.body==Support::supported)?Support::supported:(yellow?Support::occluded:Support::absent);if(s==Support::supported){eligible++;d.hit=hit;d.line_id=l.id;}if(s==Support::supported||best==Support::absent)best=s;
  }
  d.contact=best;d.line_unique=eligible==1;d.probes=static_cast<std::uint32_t>(out.probes-start);d.signature=signature(d);if(out.probes>budget)return invalid("probe overflow");
 }
 return out;
}
bool Candidate::retains(Ns t)const{for(std::size_t i=0;i<size_;++i)if(history_[i].key.capture==t)return true;return false;}
void Candidate::relate(Observation&out,Key key,Ns now){
 bool context=initialized_&&!(last_.context==key.context);bool clock=key.capture>now||(initialized_&&(key.capture<=last_.capture||key.sequence<=last_.sequence));bool stale=now-key.capture>=100000000;bool gap=initialized_&&key.capture-last_.capture>40000000;
 if(context||clock||gap||stale||out.invalid){size_=0;out.context_invalid=context||clock||stale;out.reason=context?"context changed":clock?"clock/key invalid":stale?"source deadline":gap?"adjacent gap reset":out.reason;}
 while(size_&&key.capture-history_[0].key.capture>90000000){for(std::size_t i=1;i<size_;++i)history_[i-1]=history_[i];--size_;}
 // Reserve current's slot before any causal output: at most five past + current.
 if(size_==6){for(std::size_t i=1;i<size_;++i)history_[i-1]=history_[i];--size_;}
 for(std::size_t i=0;i<out.count;++i){auto&d=out.parts[i];auto&r=out.relations[i];r.invalid=out.invalid||out.context_invalid;
  r={};r.invalid=out.invalid||out.context_invalid;r.freshness=now-key.capture;
  std::array<std::pair<std::uint64_t,std::uint64_t>,6> seen{};std::size_t seenCount=0,maxLinks=0;
  auto endpoint=[&](const Descriptor&p,Ns capture){
   if(!p.front_end)return;
   bool duplicate=false;for(std::size_t j=0;j<seenCount;++j)duplicate|=p.rgb_signature==seen[j].first||p.signature==seen[j].second;
   // Remember every eligible endpoint in this effective window, including duplicates.
   seen[seenCount++]={p.rgb_signature,p.signature};
   if(!duplicate){if(!r.independent)r.first_independent=capture;++r.independent;r.last_independent=capture;}
  };
  for(std::size_t h=0;h<size_;++h){const auto&s=history_[h];std::array<std::pair<double,std::size_t>,128>cost{};std::size_t num=0,contained=0;
   for(std::size_t j=0;j<s.count;++j){double c=distance(d,s.parts[j]);if(c<=128)cost[num++]={c,j};if(contains(d,s.parts[j])&&s.parts[j].front_end)contained++;}
   std::sort(cost.begin(),cost.begin()+num);if(contained>=2&&!d.front_end){r.ambiguous=true;maxLinks=2;}
   if(num){bool tie=num>1&&cost[1].first-cost[0].first<=4;if(tie){r.ambiguous=true;maxLinks=std::max<std::size_t>(maxLinks,2);}else{maxLinks=std::max<std::size_t>(maxLinks,1);endpoint(s.parts[cost[0].second],s.key.capture);}}
  }
  endpoint(d,key.capture);
  r.span=r.independent?r.last_independent-r.first_independent:0;r.alternatives=static_cast<std::uint8_t>(maxLinks);r.usable=r.independent>=3&&r.span>=30000000&&!r.ambiguous&&!r.invalid;r.links={0,1};
 }
 if(!out.invalid&&!out.context_invalid){auto&s=history_[size_++];s.count=out.count;s.key=key;s.parts=out.parts;}
 initialized_=true;last_=key;
}
Constraints constrain(const Observation&out,const Guard&g){
 Constraints c;c.contact_id=g.contact_id;c.completed=g.execution=="completed_up";bool unknown=g.execution=="unknown_down";bool active=g.execution=="known_down";bool never=g.execution=="never_executed";bool consistent=(never&&g.prefix==0)||(active&&g.prefix>0)||(unknown&&g.prefix>0&&g.receipt_unknown)||(c.completed&&g.prefix>=2);
 c.invalid=!consistent||out.invalid||out.context_invalid||g.now>=g.gate_deadline||g.now>=g.plan_deadline;c.release=unknown||c.invalid;
 if(c.invalid||unknown||c.completed)return c;
 for(std::size_t i=0;i<out.count;++i){const auto&d=out.parts[i];const auto&r=out.relations[i];bool current=d.contact==Support::supported&&d.line_unique;bool attached=active&&static_cast<int>(i)==g.attachment_query;
  // A consistent existing receipt is distinct from confirmation for a new Down.
  bool bodyNow=d.q.tap||(d.body==Support::supported&&d.left&&d.right);
  if(attached&&current&&bodyNow&&!r.invalid&&!r.ambiguous){c.move=c.refresh=true;c.hit=d.hit;}
  if(!attached&&current&&d.front_end&&r.usable&&g.free_contacts>0)c.down[i]=true;
 }
 if(active&&!c.refresh&&g.now-g.last_contact>=60000000)c.release=true;return c;
}
static_assert(sizeof(Descriptor)<=256);static_assert(Candidate::metadata_bytes()<=1048576);
}
