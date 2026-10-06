#include "current.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace pas::current_rails {
namespace {
constexpr Nanoseconds ms=1'000'000;
double dot(Vec2 a,Vec2 b){return a.x*b.x+a.y*b.y;}
Vec2 sub(Vec2 a,Vec2 b){return {a.x-b.x,a.y-b.y};}
Vec2 add(Vec2 a,Vec2 b,double k){return {a.x+b.x*k,a.y+b.y*k};}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
using RGB=std::array<int,3>;
bool white(RGB p){return p[0]>=240&&p[1]>=240&&p[2]>=240;}
bool yellow(RGB p){return p[0]>=180&&p[1]>=120&&p[2]<=180;}
bool cyan(RGB p){return p[1]>=130&&p[2]>=180&&p[1]-p[0]>=12&&p[2]-p[1]>=6;}
int contrast(RGB a,RGB b){return a[0]-b[0]+a[1]-b[1]+a[2]-b[2];}
bool same(const NoteCandidate& a,const NoteCandidate& b){return a.kind==b.kind&&a.center.x==b.center.x&&a.center.y==b.center.y&&a.width==b.width&&a.height==b.height&&a.tangent.x==b.tangent.x&&a.tangent.y==b.tangent.y&&a.held_body_patch==b.held_body_patch&&bool(a.tail)==bool(b.tail)&&(!a.tail||(a.tail->x==b.tail->x&&a.tail->y==b.tail->y));}
struct Reader {
 const Frame& f;std::uint64_t& probes;
 std::optional<RGB> operator()(Vec2 p){
  if(!finite(p)||p.x<-.49||p.y<-.49||p.x>f.width-.51||p.y>f.height-.51)return {};
  const auto x=int(std::lround(p.x)),y=int(std::lround(p.y));
  if(x<0||y<0||x>=f.width||y>=f.height)return {};
  ++probes;const auto i=std::size_t(y)*f.stride+std::size_t(x)*3;
  return RGB{f.rgb[i],f.rgb[i+1],f.rgb[i+2]};
 }
};
bool line_mask(Vec2 p,const CandidateBatch& b){for(const auto& l:b.lines){const Vec2 n{-l.tangent.y,l.tangent.x};if(std::abs(dot(sub(p,l.center),n))<=2.8&&std::abs(dot(sub(p,l.center),l.tangent))<=l.length/2)return true;}return false;}
int body_rows(Reader& read,const NoteCandidate& note,Vec2 hit,const CandidateBatch& b,bool& effect){
 const auto u=note.tangent;const Vec2 n{-u.y,u.x};int good=0;
 for(int k=-16;k<=16;++k){
  const auto c=add(hit,n,k);bool complete=true,inside=true,rails=true;
  for(double s:{-.3,0.,.3}){
   const auto p=add(c,u,s*note.width);const auto a=read(p);
   const auto out=read(add(c,u,(s<0?-1.:1.)*(note.width/2+6)));
   if(!a||!out){complete=false;continue;}effect|=yellow(*a);
   // A masked white judgment row supplies no body evidence.
   const bool neutral=(*a)[0]>=60&&(*a)[1]>=(*a)[0]-5&&(*a)[2]>=(*a)[1]-5;
   inside&=!white(*a)&&!yellow(*a)&&(cyan(*a)||neutral)&&contrast(*a,*out)>=40&&!line_mask(p,b);
  }
  for(int sign:{-1,1}){bool found=false;for(int d=-2;d<=2;++d){const auto p=add(c,u,sign*note.width/2+d);const auto a=read(p);found|=a&&white(*a)&&!line_mask(p,b);}rails&=found;}
  good+=complete&&inside&&rails;
 }
 return good;
}
}
Evaluation Hook::evaluate(const Frame& f,const CandidateBatch& b,const DecisionSnapshot& scene,
 const current_execution::ExecutionLedger& ledger,Nanoseconds now){
 Evaluation out;out.filtered=scene;out.filtered.targets.clear();out.raw=b.candidates.size();
 const SceneContext expected{f.epoch,f.generation,f.geometry_version,f.sequence,f.capture_complete_ns,f.width,f.height,f.source_rotation};
 auto invalid=[&](){out.filtered.playing_gate=false;out.filtered.capacity_valid=false;return out;};
 if(!f.source_valid||!b.source_valid||!b.capacity_valid||!scene.capacity_valid||!ledger.valid()||!ledger.context_matches(expected)||b.context!=expected||scene.context!=expected||
  !f.epoch||!f.generation||!f.geometry_version||!f.sequence||f.width!=1280||f.height!=720||f.stride!=3840||f.source_rotation!=1||f.rgb.size()!=1280*720*3||b.candidates.size()>128||b.lines.size()>16||scene.targets.size()>128||f.capture_complete_ns<0||f.capture_complete_ns>std::numeric_limits<Nanoseconds>::max()-100*ms||f.capture_complete_ns>f.pixels_ready_ns||f.pixels_ready_ns>now||now-f.capture_complete_ns>=100*ms)return invalid();
 for(const auto& l:b.lines)if(l.observed_ns!=f.capture_complete_ns||!finite(l.center)||!finite(l.tangent)||std::abs(std::hypot(l.tangent.x,l.tangent.y)-1)>.001||!std::isfinite(l.length)||l.length<=0)return invalid();
 if(scene.lines.size()!=b.lines.size())return invalid();
 for(std::size_t i=0;i<b.lines.size();++i){const auto&a=b.lines[i];const auto&z=scene.lines[i];if(a.track_id!=z.track_id||a.observed_ns!=z.observed_ns||a.center.x!=z.center.x||a.center.y!=z.center.y||a.tangent.x!=z.tangent.x||a.tangent.y!=z.tangent.y||a.length!=z.length||a.association_valid!=z.association_valid)return invalid();}
 for(std::size_t i=0;i<b.candidates.size();++i){if(!b.candidates[i].candidate_id)return invalid();for(std::size_t j=0;j<i;++j)if(b.candidates[i].candidate_id==b.candidates[j].candidate_id)return invalid();}
 if(context_.frame&&(expected.epoch!=context_.epoch||expected.generation!=context_.generation||expected.geometry!=context_.geometry||expected.frame<=context_.frame||expected.capture_ns<=context_.capture_ns))return invalid();
 context_=expected;out.valid=true;
 // The original Tap contract is retained; its historical RGB novelty and
 // relation logic are never replaced by a permissive Hold colour predicate.
 auto taps=current_execution::evaluate(tap_,f,b,scene,ledger,now);
 for(auto& t:taps.filtered.targets)if(t.note.kind==NoteKind::tap){out.filtered.targets.push_back(t);++out.accepted;}
 // Drag/Flick keep the original formal observer/owner contract. This adapter
 // makes no new BVI claim about them; completed/retired IDs still cannot revive.
 for(const auto&t:scene.targets)if((t.note.kind==NoteKind::drag||t.note.kind==NoteKind::flick)&&ledger.permits_current_target(t.note_id)){out.filtered.targets.push_back(t);++out.accepted;}
 Reader read{f,out.probes};
 for(const auto& c:b.candidates){
  if(c.note.kind!=NoteKind::hold)continue;
  auto& s=out.support[out.count++];s.candidate=c.candidate_id;
  const auto& n=c.note;
  if(!finite(n.center)||!finite(n.tangent)||!std::isfinite(n.width)||n.width<16||n.width>512||std::abs(std::hypot(n.tangent.x,n.tangent.y)-1)>.001||c.quality==ObservationQuality::rejected){++out.unknown;continue;}
  const GameTarget* target=nullptr;int matches=0;
  for(const auto& t:scene.targets)if(same(n,t.note)){target=&t;++matches;}
  if(matches!=1||!target||!target->note_id){s.reason="identity_unmapped";++out.unknown;continue;}
  bool neighbour=false;for(const auto& other:b.candidates){if(&other==&c)continue;
   if(std::abs(dot(sub(other.note.center,n.center),n.tangent))<(n.width+other.note.width)/2&&
      std::abs(dot(sub(other.note.center,n.center),Vec2{-n.tangent.y,n.tangent.x}))<16)neighbour=true;}
  if(neighbour){s.reason="neighbour_or_duplicate_current_front";++out.unknown;continue;}
  s.note=target->note_id;const auto* attachment=ledger.find(s.note);
  if(ledger.retired(s.note)){s.reason="retired_identity_no_rebirth";continue;}
  s.active=attachment&&attachment->state==current_execution::Execution::active;
  if(attachment&&attachment->state!=current_execution::Execution::active&&attachment->state!=current_execution::Execution::pending){s.reason="terminal_execution_identity";continue;}
  const Vec2 normal{-n.tangent.y,n.tangent.x};double depth=0;Vec2 front=n.center;
  if(n.tail){depth=dot(sub(n.center,*n.tail),normal);if(!std::isfinite(depth)||std::abs(depth)>4095||std::abs(dot(sub(n.center,*n.tail),n.tangent))>2){++out.unknown;continue;}}
  auto measured=hold_front::measure(f,b.context,c);out.probes+=measured.probes;
  if(measured.boundary){
   s.terminal=std::all_of(measured.lanes.begin(),measured.lanes.end(),[](const auto& l){return cyan({l.inside.rgb[0],l.inside.rgb[1],l.inside.rgb[2]});});
   if(s.terminal)front=*measured.boundary;
  }
  s.front=front;
  Track* track=nullptr;
  if(s.terminal&&!n.held_body_patch){
   const auto prior_line=std::find_if(b.lines.begin(),b.lines.end(),[&](const auto& l){return l.track_id==target->line_id&&l.association_valid;});
   if(prior_line!=b.lines.end()&&prior_line->track_id){
    for(auto& h:tracks_)if(h.note==s.note){track=&h;break;}
    if(!track)for(auto& h:tracks_)if(!h.note||(h.count&&f.capture_complete_ns-h.points[h.count-1].time>90*ms)){track=&h;*track=Track{};track->note=s.note;break;}
    if(track){
     if(track->line!=prior_line->track_id){track->line=prior_line->track_id;track->count=0;}
     while(track->count&&f.capture_complete_ns-track->points[0].time>90*ms){std::move(track->points.begin()+1,track->points.begin()+track->count,track->points.begin());--track->count;}
     if(track->count==6){std::move(track->points.begin()+1,track->points.end(),track->points.begin());--track->count;}
     const Vec2 ln{-prior_line->tangent.y,prior_line->tangent.x};
     if(!track->count||f.capture_complete_ns-track->points[track->count-1].time>=10*ms)track->points[track->count++]={f.capture_complete_ns,dot(sub(front,prior_line->center),ln),front};
     s.samples=track->count;
    }
   }
  }
  // A body patch uses its current interior region; it never invents a head.
  int competitors=0;const LineCandidate* selected=nullptr;
  for(const auto& l:b.lines){
   const Vec2 ln{-l.tangent.y,l.tangent.x};const double denominator=dot(normal,ln);
   if(std::abs(denominator)<1e-6)continue;
   const double k=-dot(sub(front,l.center),ln)/denominator;const auto hit=add(front,normal,k);
   const double body_depth=n.tail?dot(sub(front,*n.tail),normal):0;
   const bool in_body=n.held_body_patch?std::abs(k)<=16:(body_depth>=0?k<=4&&k>=-body_depth-4:k>=-4&&k<=-body_depth+4);
   if(!in_body||std::abs(dot(sub(hit,l.center),l.tangent))>l.length/2)continue;
   ++competitors;selected=&l;s.hit=hit;
  }
  s.unique=competitors==1&&selected&&selected->association_valid&&selected->track_id&&selected->track_id==target->line_id&&!target->line_projection_only;
  if(!s.unique){s.reason="all_lines_compete_or_invalid";++out.unknown;continue;}s.line=selected->track_id;
  s.rows=body_rows(read,n,s.hit,b,s.effect);s.body=s.rows>=6&&!s.effect;
  if(!s.body){s.reason=s.effect?"effect_not_current_body":"body_support_insufficient";++out.unknown;continue;}
  auto t=*target;t.hit=s.hit;t.evidence_ns=f.capture_complete_ns;t.expires_ns=f.capture_complete_ns+100*ms;t.note.rails_geometry=true;
  if(s.active){
   t.note.held_body_evidence=true;t.note.head_on_line=false;
   t.samples=std::max(1,t.samples);
   // Tail remains measured only if the opposite terminal is supported now.
   if(n.tail&&!n.held_body_patch){auto tail=c;tail.note.center=*n.tail;tail.note.tail=front;
    const auto rear=hold_front::measure(f,b.context,tail);out.probes+=rear.probes;
    if(rear.boundary){t.note.tail=*rear.boundary;s.tail=true;}else {t.note.tail.reset();t.tail_crossing_ns.reset();}}
   else {t.note.tail.reset();t.tail_crossing_ns.reset();}
   s.accepted=true;s.reason="own_down_current_body_move";
  }else{
   if(!s.terminal||n.held_body_patch||!c.head_visible){s.reason="body_or_proposed_terminal_without_entry";continue;}
   if(!track){s.reason="bounded_history_capacity";continue;}
   const Vec2 ln{-selected->tangent.y,selected->tangent.x};const double distance=dot(sub(front,selected->center),ln);
   s.samples=track->count;
   if(track->count<3||track->points[track->count-1].time-track->points[0].time<30*ms){s.reason="current_relation_warming";continue;}
   const auto& first=track->points[0];const auto& last=track->points[track->count-1];
   const double velocity=(last.distance-first.distance)/((last.time-first.time)/1e9);
   bool reversal=false;for(int i=1;i<track->count;++i)if((track->points[i].distance-track->points[i-1].distance)*velocity<-.5)reversal=true;
   if(reversal||std::abs(distance)>4){s.reason="front_not_current_overlap";continue;}
   // No future root is borrowed from the reconstructed observer centre.
   t.note.center=front;t.note.head_on_line=true;t.note.held_body_evidence=false;
   t.crossing_ns=f.capture_complete_ns;t.uncertainty_ns=2*ms;t.samples=track->count;t.history_span_ns=last.time-first.time;
   t.current_contact=GameTarget::CurrentContact::hold_front_rails;
   t.reason="current_rails_v1_measured_front_overlap";s.accepted=true;s.reason="measured_front_relation_entry";
  }
  if(s.accepted){out.filtered.targets.push_back(std::move(t));++out.accepted;}
 }
 return out;
}
static_assert(Hook::storage_bytes()<1048576);
}
