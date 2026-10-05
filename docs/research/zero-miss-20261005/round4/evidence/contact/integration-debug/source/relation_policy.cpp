#include "relation_policy.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace bvi {
namespace {
constexpr double pi=3.141592653589793;
Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}
Point u(double a){return {std::cos(a),std::sin(a)};}
Point v(double a){return {-std::sin(a),std::cos(a)};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
bool eq(Point a,Point b){return a.x==b.x&&a.y==b.y;}
bool same_query(const Query&a,const Query&b){return eq(a.front,b.front)&&a.width==b.width&&a.depth==b.depth&&a.angle==b.angle&&a.tap==b.tap;}
bool same_line(const RelationLine&a,const RelationLine&b){return a.state==b.state&&eq(a.center,b.center)&&a.angle==b.angle&&a.length==b.length&&a.separation==b.separation&&a.id==b.id;}
bool same_key(const Key&a,const Key&b){return a.context==b.context&&a.capture==b.capture&&a.ready==b.ready&&a.sequence==b.sequence&&a.source_valid==b.source_valid;}
double distance(const Descriptor&a,const Descriptor&b){if(std::abs(a.q.width-b.q.width)>4||std::abs(std::remainder(a.q.angle-b.q.angle,pi))>.6||a.q.tap!=b.q.tap)return std::numeric_limits<double>::infinity();return std::hypot(a.q.front.x-b.q.front.x,a.q.front.y-b.q.front.y);}
bool contains(const Descriptor&a,const Descriptor&b){auto diff=sub(b.q.front,a.q.front);double s=dot(diff,u(a.q.angle)),d=dot(diff,v(a.q.angle));return std::abs(s)<=4&&d<=4&&d>=-a.measured_depth-4&&distance(a,b)<=128&&a.measured_depth>=b.measured_depth-4;}
using Aliases=std::array<std::uint8_t,128>;
Aliases aliases(std::span<const Descriptor>parts){Aliases result{};for(std::size_t i=0;i<parts.size();++i){result[i]=static_cast<std::uint8_t>(i);for(std::size_t j=0;j<i;++j)if(same_measurement(parts[i],parts[j])){result[i]=result[j];break;}}return result;}
struct Choice{std::uint8_t index{255};bool ambiguous{};std::uint8_t alternatives{};};
Choice choose(const Descriptor&d,const Sample&s,const Aliases&a){double first=std::numeric_limits<double>::infinity(),second=first;std::size_t nearest=0,contained=0;for(std::size_t j=0;j<s.count;++j){if(a[j]!=j)continue;double cost=distance(d,s.parts[j]);if(cost<=128){if(cost<first){second=first;first=cost;nearest=j;}else if(cost<second)second=cost;}if(contains(d,s.parts[j])&&s.parts[j].front_end)++contained;}Choice out;out.ambiguous=contained>=2||(std::isfinite(second)&&second-first<=4);out.alternatives=out.ambiguous?2:std::isfinite(first)?1:0;if(std::isfinite(first)&&!(std::isfinite(second)&&second-first<=4))out.index=static_cast<std::uint8_t>(nearest);return out;}
bool same_stationary_line(const RelationLine&a,const RelationLine&b){return a.state==RelationLineState::unique&&b.state==RelationLineState::unique&&a.id==b.id&&std::abs(std::remainder(a.angle-b.angle,pi))<=.6&&std::hypot(a.center.x-b.center.x,a.center.y-b.center.y)<=128;}
}
bool same_measurement(const Descriptor&a,const Descriptor&b){return same_query(a.q,b.q)&&same_key(a.key,b.key)&&a.body==b.body&&a.contact==b.contact&&a.front_end==b.front_end&&a.rear_end==b.rear_end&&a.effect==b.effect&&a.left==b.left&&a.right==b.right&&a.line_unique==b.line_unique&&a.measured_depth==b.measured_depth&&eq(a.rear,b.rear)&&eq(a.hit,b.hit)&&a.line_id==b.line_id&&a.signature==b.signature&&a.rgb_signature==b.rgb_signature&&same_line(a.approach,b.approach);}
void canonicalize_measurements(Observation&out){out.aliases_valid=false;if(out.count>128)return;out.canonical=aliases({out.parts.data(),out.count});out.aliases_valid=true;}
void observe_relation_context(Observation&out,std::span<const Line>lines){
 for(std::size_t i=0;i<std::min<std::size_t>(out.count,128);++i){auto&d=out.parts[i];d.approach={};if(out.invalid)continue;std::size_t count=0;RelationLine selected;const auto nv=v(d.q.angle);
  for(const auto&l:lines){if(!std::isfinite(l.center.x)||!std::isfinite(l.center.y)||!std::isfinite(l.angle)||!std::isfinite(l.length)||l.length<=0)continue;const auto ln=v(l.angle);const double den=dot(nv,ln);if(std::abs(den)<1e-6)continue;const double sep=dot(sub(d.q.front,l.center),ln),k=-sep/den;if(!std::isfinite(k)||std::abs(k)>128)continue;const Point hit{d.q.front.x+k*nv.x,d.q.front.y+k*nv.y};if(std::abs(dot(sub(hit,l.center),u(l.angle)))>l.length/2)continue;++count;selected={l.center,l.angle,l.length,sep,l.id,RelationLineState::unique};
  }
  if(count==1)d.approach=selected;else if(count>1)d.approach.state=RelationLineState::multiple;
 }
 canonicalize_measurements(out);
}
void Candidate::relate(Observation&out,Key key,Ns now){
 if(out.count>128){out.count=128;out.invalid=true;out.reason="relation capacity overflow";}
 if(!key.source_valid){out.invalid=true;out.reason="source invalid";}
 const bool context=initialized_&&!(last_.context==key.context);const bool clock=key.capture>now||(initialized_&&(key.capture<=last_.capture||key.sequence<=last_.sequence));const bool stale=now-key.capture>=100000000;const bool gap=initialized_&&key.capture-last_.capture>40000000;
 if(context||clock||gap||stale||out.invalid){size_=0;out.context_invalid=context||clock||stale;out.reason=context?"context changed":clock?"clock/key invalid":stale?"source deadline":gap?"adjacent gap reset":out.reason;}
 while(size_&&key.capture-history_[0].key.capture>90000000){for(std::size_t i=1;i<size_;++i)history_[i-1]=history_[i];--size_;}
 if(size_==6){for(std::size_t i=1;i<size_;++i)history_[i-1]=history_[i];--size_;}
 canonicalize_measurements(out);
 std::array<Aliases,6> pastAliases{};std::array<std::array<Choice,128>,6> choices{};
 for(std::size_t h=0;h<size_;++h){auto&s=history_[h];pastAliases[h]=aliases({s.parts.data(),s.count});std::array<std::uint8_t,128> users{};for(std::size_t i=0;i<out.count;++i)if(out.canonical[i]==i){auto&c=choices[h][i];c=choose(out.parts[i],s,pastAliases[h]);if(c.index!=255&&out.parts[i].front_end)++users[c.index];}for(std::size_t i=0;i<out.count;++i)if(out.canonical[i]==i){auto&c=choices[h][i];if(c.index!=255&&users[c.index]>1){c.ambiguous=true;c.alternatives=2;}}}
 for(std::size_t i=0;i<out.count;++i){if(out.canonical[i]!=i)continue;auto&d=out.parts[i];auto&r=out.relations[i];r={};r.invalid=out.invalid||out.context_invalid;r.freshness=now-key.capture;
  std::array<const Descriptor*,6> seen{};std::size_t seenCount=0;std::uint8_t stationaryCount=0;Ns stationaryFirst=0,stationaryLast=0;const Descriptor*previous=nullptr;const Descriptor*lastObserved=nullptr;std::array<std::uint64_t,6> stationaryRgb{};std::size_t rgbCount=0;
  auto resetStationary=[&](){stationaryCount=0;stationaryFirst=stationaryLast=0;previous=lastObserved=nullptr;rgbCount=0;};
  auto endpoint=[&](const Descriptor&p,Ns capture){
   if(!p.front_end){resetStationary();return;}
   bool duplicate=false;for(std::size_t j=0;j<seenCount;++j)duplicate|=p.rgb_signature==seen[j]->rgb_signature||p.signature==seen[j]->signature||same_query(p.q,seen[j]->q);seen[seenCount++]=&p;if(!duplicate){if(!r.independent)r.first_independent=capture;++r.independent;r.last_independent=capture;}
   if(!same_query(p.q,d.q)||p.approach.state!=RelationLineState::unique){resetStationary();return;}
   if(lastObserved){const auto&a=lastObserved->approach;const auto&b=p.approach;const bool crossed=a.separation*b.separation<0;const bool receded=std::abs(b.separation)>std::abs(a.separation)+1e-9;
    if(!same_stationary_line(a,b)||crossed||receded){resetStationary();}
   }
   if(!previous){previous=lastObserved=&p;stationaryCount=1;stationaryFirst=stationaryLast=capture;stationaryRgb[rgbCount++]=p.rgb_signature;return;}
   lastObserved=&p;bool repeat=false;for(std::size_t j=0;j<rgbCount;++j)repeat|=p.rgb_signature==stationaryRgb[j];stationaryRgb[rgbCount++]=p.rgb_signature;
   const double progress=std::abs(previous->approach.separation)-std::abs(p.approach.separation);
   if(!repeat&&progress>=1.0){++stationaryCount;stationaryLast=capture;previous=&p;}
  };
  for(std::size_t j=0;j<out.count;++j)if(i!=j&&out.canonical[j]==j&&d.front_end&&out.parts[j].front_end&&distance(d,out.parts[j])<=4){r.ambiguous=true;r.alternatives=2;}
  for(std::size_t h=0;h<size_;++h){const auto&c=choices[h][i];r.ambiguous|=c.ambiguous;r.alternatives=std::max(r.alternatives,c.alternatives);if(c.index!=255&&!c.ambiguous)endpoint(history_[h].parts[c.index],history_[h].key.capture);else resetStationary();}
  endpoint(d,key.capture);r.span=r.independent?r.last_independent-r.first_independent:0;
  const Ns stationarySpan=stationaryCount?stationaryLast-stationaryFirst:0;
  // Preserve both 3 observations and 30 ms. A stationary path is separately earned.
  if(stationaryCount>=3&&stationarySpan>=30000000&&(r.independent<3||r.span<30000000)){r.independent=stationaryCount;r.first_independent=stationaryFirst;r.last_independent=stationaryLast;r.span=stationarySpan;}
  r.usable=r.independent>=3&&r.span>=30000000&&!r.ambiguous&&!r.invalid;r.links={0,1};
 }
 for(std::size_t i=0;i<out.count;++i)if(out.canonical[i]!=i)out.relations[i]=out.relations[out.canonical[i]];
 if(!out.invalid&&!out.context_invalid){auto&s=history_[size_++];s.count=out.count;s.key=key;s.parts=out.parts;}
 initialized_=true;last_=key;
}
static_assert(sizeof(Descriptor)<=256);static_assert(Candidate::metadata_bytes()<=1048576);
}
