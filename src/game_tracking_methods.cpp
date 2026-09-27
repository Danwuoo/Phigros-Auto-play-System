#include "pas/game_tracking.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace pas {
namespace {
constexpr double unmatched_cost=200,ambiguity_px=8;
bool compatible_context(const SceneContext& a,const SceneContext& b) {
 return a.epoch==b.epoch&&a.generation==b.generation&&a.geometry==b.geometry&&a.width==b.width&&a.height==b.height&&a.rotation==b.rotation;
}
double dist(Vec2 a,Vec2 b){return std::hypot(a.x-b.x,a.y-b.y);}
// Independent shortest-augmenting-path assignment, square <=128. Forbidden
// edges carry unmatched cost; rejected assignments never become observations.
std::vector<int> assignment(const std::array<double,128*128>& cost,std::size_t rows,std::size_t cols){
 const std::size_t n=std::max(rows,cols);std::array<double,129> u{},v{},minimum{};
 std::array<std::size_t,129> p{},way{};std::array<bool,129> used{};
 for(std::size_t i=1;i<=n;++i){p[0]=i;std::size_t j0=0;minimum.fill(std::numeric_limits<double>::infinity());used.fill(false);
  do{used[j0]=true;const auto i0=p[j0];double delta=std::numeric_limits<double>::infinity();std::size_t j1=0;
   for(std::size_t j=1;j<=n;++j)if(!used[j]){const double c=i0<=rows&&j<=cols?cost[(i0-1)*128+j-1]:unmatched_cost;
    const double current=c-u[i0]-v[j];if(current<minimum[j]){minimum[j]=current;way[j]=j0;}
    if(minimum[j]<delta){delta=minimum[j];j1=j;}}
   for(std::size_t j=0;j<=n;++j)if(used[j]){u[p[j]]+=delta;v[j]-=delta;}else minimum[j]-=delta;
   j0=j1;
  }while(p[j0]!=0);
  do{const auto j1=way[j0];p[j0]=p[j1];j0=j1;}while(j0);
 }
 std::vector<int> result(rows,-1);for(std::size_t j=1;j<=cols;++j)if(p[j]&&p[j]<=rows&&cost[(p[j]-1)*128+j-1]<unmatched_cost)result[p[j]-1]=static_cast<int>(j-1);
 return result;
}
template<class Axis> void predict_axis(Axis& a,double dt){
 a.p+=dt*a.v;const double q=160000,dt2=dt*dt;
 a.p00+=2*dt*a.p01+dt2*a.p11+q*dt2*dt2/4;
 a.p01+=dt*a.p11+q*dt2*dt/2;a.p11+=q*dt2;
}
template<class Axis> void correct_axis(Axis& a,double z,double noise){
 const double den=a.p00+noise,k0=a.p00/den,k1=a.p01/den,e=z-a.p;
 const double old00=a.p00,old01=a.p01;a.p+=k0*e;a.v+=k1*e;
 a.p00=std::max(.001,(1-k0)*old00);a.p01=(1-k0)*old01;a.p11=std::max(.001,a.p11-k1*old01);
}
bool valid_note(const NoteCandidate& n){return std::isfinite(n.center.x)&&std::isfinite(n.center.y)&&std::isfinite(n.width)&&std::isfinite(n.height)&&n.width>0&&n.height>0&&n.width<=8192&&n.height<=8192&&std::isfinite(n.tangent.x)&&std::isfinite(n.tangent.y)&&std::abs(std::hypot(n.tangent.x,n.tangent.y)-1)<.01&&(!n.tail||(std::isfinite(n.tail->x)&&std::isfinite(n.tail->y)));}
}
GameTrackingComparison::GameTrackingComparison(std::string method,bool reupdate):method_(std::move(method)),reupdate_(reupdate){
 if(method_!="legacy"&&method_!="byte_association"&&method_!="oc_observation")throw std::invalid_argument("unknown tracking method");
 if(reupdate_&&method_!="oc_observation")throw std::invalid_argument("observation reupdate requires oc_observation");
}
void GameTrackingComparison::reset(){states_.clear();baseline_.clear();retired_.clear();previous_={};retirement_overflow_=false;}
TrackingResult GameTrackingComparison::update(const CandidateBatch& b){
 TrackingResult r;r.executable.context=b.context;r.executable.sequence=b.context.frame;r.executable.ui=b.ui;
 r.executable.playing_gate=b.playing_gate&&b.source_valid;r.executable.lines=b.lines;r.executable.capacity_valid=b.capacity_valid;
 if(b.context.width<2||b.context.height<2||b.context.capture_ns<0)throw std::invalid_argument("tracking context");
 if(b.candidates.size()>128||b.lines.size()>16||(!b.capacity_valid&&method_!="legacy")){
  if(previous_.frame&&!compatible_context(previous_,b.context))reset();
  for(const auto& s:states_)if(s.history.kind==NoteKind::hold){bool present=false;for(const auto& old:retired_)present|=old.birth_id==s.birth_id;
   if(!present){if(retired_.size()==128)retirement_overflow_=true;else retired_.push_back({s.birth_id,s.history.appearance,s.history.observed});}}
  states_.clear();baseline_.clear();previous_=b.context;r.capacity_valid=r.executable.capacity_valid=false;r.executable.playing_gate=false;r.reset_reason="capacity";return r;}
 for(const auto& line:b.lines)if(!std::isfinite(line.center.x)||!std::isfinite(line.center.y)||!std::isfinite(line.length)||line.length<=0||!std::isfinite(line.thickness)||line.thickness<=0||!std::isfinite(line.confidence)||!std::isfinite(line.tangent.x)||!std::isfinite(line.tangent.y)||std::abs(std::hypot(line.tangent.x,line.tangent.y)-1)>.01)throw std::invalid_argument("invalid tracking line");
 std::set<std::uint64_t> candidate_ids;
 for(const auto& c:b.candidates)if(!valid_note(c.note)||!c.candidate_id||c.hint_age_ns<0||!candidate_ids.insert(c.candidate_id).second||(c.shortened_hold&&!valid_note(*c.shortened_hold)))throw std::invalid_argument("invalid tracking candidate");
 const auto now=b.context.capture_ns;
 const auto retire=[&](const State& s){
  if(s.history.kind!=NoteKind::hold)return;
  for(const auto& old:retired_)if(old.birth_id==s.birth_id)return;
  if(retired_.size()==128){retirement_overflow_=true;return;}
  retired_.push_back({s.birth_id,s.history.appearance,s.history.observed});
 };
 if(previous_.frame&&(!compatible_context(previous_,b.context)||b.context.frame<=previous_.frame||now<=previous_.capture_ns||now-previous_.capture_ns>=100'000'000)){
  r.reset_reason=!compatible_context(previous_,b.context)?"context":"time_discontinuity";
  // Remember retired births across source gaps in the SAME context only.
  if(compatible_context(previous_,b.context)){for(const auto& s:states_)retire(s);states_.clear();baseline_.clear();}
  else reset();
 }
 previous_=b.context;
 if(!b.source_valid){for(const auto& s:states_)retire(s);states_.clear();baseline_.clear();r.executable.playing_gate=false;r.reset_reason="source_invalid";if(method_!="legacy")return r;}
 if(method_=="legacy"){
  std::erase_if(baseline_,[&](const auto& h){return now-h.observed>=100'000'000;});
  std::vector<NoteCandidate> notes;std::vector<std::optional<NoteCandidate>> short_holds;
  for(const auto& c:b.candidates){notes.push_back(c.note);short_holds.push_back(c.shortened_hold);}
  std::vector<std::size_t> source_indices;track_legacy_batch(r.executable,notes,short_holds,baseline_,next_id_,nullptr,&source_indices);
  if(!r.executable.capacity_valid){r.capacity_valid=false;r.executable.playing_gate=false;r.executable.ui=GameUi::unknown;}
  for(std::size_t i=0;i<r.executable.targets.size();++i){const auto& t=r.executable.targets[i];const auto& c=b.candidates[source_indices[i]];
   TrackedObservation o;o.track_id=o.birth_id=t.note_id;o.revision=t.revision;o.candidate_id=c.candidate_id;o.state="observed";o.stage="legacy_greedy";
   o.estimated=t.note.center;o.observed_ns=now;o.supported_ns=now;o.identity_supported=t.reason!="association_ambiguous";o.prediction_only=false;
   o.action_evidence_valid=c.action_support&&o.identity_supported&&c.head_visible&&b.playing_gate;
   o.head_visible=c.head_visible;o.body_visible=c.body_visible;o.left_rail=c.left_rail;o.right_rail=c.right_rail;o.tail=c.note.tail;o.reason=t.reason;r.observations.push_back(o);
  }
  // Baseline executable preserves legacy semantics for a meaningful oracle.
  return r;
 }
 std::erase_if(states_,[&](const auto& s){if(now-s.history.observed<100'000'000)return false;
  retire(s);return true;});
 if(retirement_overflow_){r.capacity_valid=r.executable.capacity_valid=false;r.executable.playing_gate=false;r.reset_reason="retirement_guard_capacity";}
 for(auto& s:states_){const double dt=(now-s.predicted_ns)/1e9;if(dt>0){predict_axis(s.x,dt);predict_axis(s.y,dt);s.predicted_ns=now;}}
 const auto initial=states_.size();std::vector<int> matched(initial,-1);std::vector<bool> used(b.candidates.size()),ambiguous(initial);
 std::vector<Nanoseconds> prior_supported;for(const auto& s:states_)prior_supported.push_back(s.supported_ns);
 const auto line_supported=[&](const NoteCandidate& n){return std::any_of(b.lines.begin(),b.lines.end(),[&](const auto& line){return line.length>=b.context.width*.32&&std::abs(line.tangent.x*n.tangent.x+line.tangent.y*n.tangent.y)>=.95&&std::abs(n.center.x-line.center.x)<=line.length/2+n.width;});};
 std::vector<double> costs(initial),margins(initial);std::vector<std::string> stages(initial);
 const auto observed_note=[&](std::size_t ti,std::size_t ci)->const NoteCandidate&{
  const auto& c=b.candidates[ci];const auto& h=states_[ti].history;
  if(h.kind==NoteKind::hold&&c.note.kind==NoteKind::tap&&c.shortened_hold&&c.shortened_hold->kind==NoteKind::hold&&dist(c.shortened_hold->center,h.last)<40)return *c.shortened_hold;
  return c.note;
 };
 const auto cost_for=[&](std::size_t ti,std::size_t ci,bool last_observation){
  const auto& s=states_[ti];const auto& n=observed_note(ti,ci);const auto& h=s.history;
  if(n.kind!=h.kind||n.kind==NoteKind::ambiguous||std::abs(n.tangent.x*h.appearance.tangent.x+n.tangent.y*h.appearance.tangent.y)<.95)return unmatched_cost;
  if(std::abs(n.width-h.appearance.width)>std::max(20.,h.appearance.width*.25))return unmatched_cost;
  const Vec2 expected=last_observation?h.last:Vec2{s.x.p,s.y.p};const double d=dist(expected,n.center);
  if(d>=b.context.width*.08)return unmatched_cost;
  double cost=d+std::abs(n.width-h.appearance.width)*.25;
  if(method_=="oc_observation"&&h.points.size()>=2&&h.points.back().t-h.points.front().t>=30'000'000){
   const auto a=h.points.front().p,z=h.points.back().p;const Vec2 v{z.x-a.x,z.y-a.y},delta{n.center.x-z.x,n.center.y-z.y};
   const double vn=std::hypot(v.x,v.y),dn=std::hypot(delta.x,delta.y);
   if(vn>5&&dn>2)cost+=12*(1-std::clamp((v.x*delta.x+v.y*delta.y)/(vn*dn),-1.,1.));
  }
  return cost;
 };
 const auto stage=[&](ObservationQuality quality,const std::string& label,bool last_observation){
  std::vector<std::size_t> ts,cs;for(std::size_t i=0;i<initial;++i)if(matched[i]<0&&!ambiguous[i])ts.push_back(i);
  for(std::size_t i=0;i<b.candidates.size();++i)if(!used[i]&&b.candidates[i].quality==quality)cs.push_back(i);
  if(ts.empty()||cs.empty())return;
  std::array<double,128*128> matrix;matrix.fill(unmatched_cost);
  for(std::size_t i=0;i<ts.size();++i)for(std::size_t j=0;j<cs.size();++j)matrix[i*128+j]=cost_for(ts[i],cs[j],last_observation);
  const auto chosen=assignment(matrix,ts.size(),cs.size());
  for(std::size_t i=0;i<chosen.size();++i)if(chosen[i]>=0){const auto j=static_cast<std::size_t>(chosen[i]);double second=unmatched_cost;
   for(std::size_t k=0;k<cs.size();++k)if(k!=j)second=std::min(second,matrix[i*128+k]);
   for(std::size_t k=0;k<ts.size();++k)if(k!=i)second=std::min(second,matrix[k*128+j]);
   const auto ti=ts[i],ci=cs[j];costs[ti]=matrix[i*128+j];margins[ti]=second-costs[ti];stages[ti]=label;
   // Reserve ambiguous competing observations; never hide them as births.
   if(margins[ti]<=ambiguity_px){ambiguous[ti]=true;used[ci]=true;continue;}
   matched[ti]=static_cast<int>(ci);used[ci]=true;
  }
 };
 stage(ObservationQuality::strong_current,"strong_assignment",false);
 if(method_=="oc_observation")stage(ObservationQuality::strong_current,"last_observation_recovery",true);
 stage(ObservationQuality::weak_current,"weak_continuation",false);
 std::vector<bool> birth_competition(b.candidates.size());
 for(std::size_t i=0;i<b.candidates.size();++i)if(!used[i]&&b.candidates[i].quality==ObservationQuality::strong_current)
  for(std::size_t j=i+1;j<b.candidates.size();++j)if(!used[j]&&b.candidates[j].quality==ObservationQuality::strong_current){const auto& a=b.candidates[i].note;const auto& z=b.candidates[j].note;
   if(a.kind==z.kind&&dist(a.center,z.center)<=8&&std::abs(a.width-z.width)<=8&&std::abs(a.tangent.x*z.tangent.x+a.tangent.y*z.tangent.y)>=.95)birth_competition[i]=birth_competition[j]=true;}
 std::vector<GameTrackHistory> histories;for(const auto& s:states_)histories.push_back(s.history);
 std::vector<NoteCandidate> executable_notes;std::vector<std::optional<NoteCandidate>> executable_shortened;std::vector<int> forced;
 for(std::size_t i=0;i<initial;++i){auto& s=states_[i];TrackedObservation o;o.track_id=s.history.id;o.birth_id=s.birth_id;o.revision=s.history.revision+1;
  o.observed_ns=s.history.observed;o.supported_ns=s.supported_ns;o.estimated={s.x.p,s.y.p};o.velocity={s.x.v,s.y.v};o.uncertainty_px=std::sqrt(std::max(s.x.p00,s.y.p00));
  o.cost=costs[i];o.competition_margin=margins[i];o.stage=stages[i];o.state="lost";o.reason=ambiguous[i]?"association_ambiguous":"prediction_only";
  if(matched[i]>=0){const auto ci=static_cast<std::size_t>(matched[i]);const auto& c=b.candidates[ci];const auto& n=observed_note(i,ci);o.candidate_id=c.candidate_id;o.prediction_only=false;o.identity_supported=true;o.state="observed";
   const auto prior_observed=s.history.observed;
   if(reupdate_&&c.head_visible&&now-prior_observed<=90'000'000&&s.history.points.size()>=2&&now-prior_observed>30'000'000){
    // Bounded ORU-style interpolation corrects only internal filter state.
    s.x.p=s.history.last.x;s.y.p=s.history.last.y;
    const auto old=s.history.last;constexpr std::size_t steps=3;const double dt=(now-prior_observed)/1e9/steps;
    for(std::size_t k=1;k<=steps;++k){predict_axis(s.x,dt);predict_axis(s.y,dt);
     correct_axis(s.x,old.x+(n.center.x-old.x)*k/steps,16);correct_axis(s.y,old.y+(n.center.y-old.y)*k/steps,16);}o.synthetic_updates=steps;
   }
   if(c.head_visible){correct_axis(s.x,n.center.x,4);correct_axis(s.y,n.center.y,4);}
   s.history.observed=now;s.history.appearance=n;s.history.last=c.head_visible?n.center:Vec2{s.x.p,s.y.p};++s.history.revision;
   o.observed_ns=now;o.estimated={s.x.p,s.y.p};o.reason="current_identity_only";
   o.head_visible=c.head_visible;o.body_visible=c.body_visible;o.left_rail=c.left_rail;o.right_rail=c.right_rail;o.tail=c.head_visible?n.tail:std::nullopt;
   o.action_evidence_valid=c.action_support&&c.head_visible&&b.playing_gate&&line_supported(n)&&
     (c.quality==ObservationQuality::strong_current||(c.body_visible&&c.left_rail&&c.right_rail&&c.note.rails_geometry&&(c.note.head_on_line||c.note.held_body_evidence)&&c.hint_source_frame>0&&c.hint_source_frame<b.context.frame&&c.hint_age_ns<=90'000'000));
   if(o.action_evidence_valid){s.supported_ns=now;o.supported_ns=now;auto note=n;note.recent_identity=0;executable_notes.push_back(note);executable_shortened.push_back(c.shortened_hold);forced.push_back(static_cast<int>(i));}
  }
  r.observations.push_back(o);
 }
 for(std::size_t ci=0;ci<b.candidates.size();++ci)if(!used[ci]&&b.candidates[ci].quality==ObservationQuality::strong_current){
  const auto& c=b.candidates[ci];bool competitor=false;for(std::size_t ti=0;ti<initial;++ti)if(cost_for(ti,ci,false)<unmatched_cost||cost_for(ti,ci,true)<unmatched_cost)competitor=true;
  if(competitor)continue;
  if(birth_competition[ci]){TrackedObservation o;o.candidate_id=c.candidate_id;o.state="tentative";o.stage="strong_birth_rejected";o.reason="birth_candidate_competition";o.prediction_only=false;o.estimated=c.note.center;o.observed_ns=now;r.observations.push_back(o);continue;}
  if(states_.size()==128){r.capacity_valid=r.executable.capacity_valid=false;r.executable.playing_gate=false;break;}
  State s;s.history.id=++next_id_;s.birth_id=s.history.id;s.history.kind=c.note.kind;s.history.last=c.note.center;s.history.observed=now;s.history.appearance=c.note;
  s.x.p=c.note.center.x;s.y.p=c.note.center.y;s.predicted_ns=now;
  // Spatial retirement guard binds re-born Hold diagnostics to the original
  // execution birth, so an expired contact cannot be replayed under a new ID.
  bool retired_ambiguous=false;std::uint64_t retired_birth=0;
  if(c.note.kind==NoteKind::hold)for(const auto& old:retired_)if(old.appearance.kind==c.note.kind&&
     (now-old.observed<250'000'000||c.note.head_on_line)&&dist(old.appearance.center,c.note.center)<40&&std::abs(old.appearance.width-c.note.width)<20){
   if(retired_birth&&retired_birth!=old.birth_id)retired_ambiguous=true;retired_birth=old.birth_id;}
  if(retired_birth)s.birth_id=retired_birth;
  if(c.note.kind==NoteKind::tap&&c.shortened_hold)for(const auto& old:retired_)if(dist(old.appearance.center,c.shortened_hold->center)<40&&std::abs(old.appearance.width-c.shortened_hold->width)<20)retired_ambiguous=true;
  for(const auto& active:states_)if(active.birth_id==s.birth_id)retired_ambiguous=true;
  if(retirement_overflow_&&c.note.kind==NoteKind::hold)retired_ambiguous=true;
  const auto index=states_.size();states_.push_back(s);histories.push_back(s.history);
  TrackedObservation o;o.track_id=s.history.id;o.birth_id=s.birth_id;o.candidate_id=c.candidate_id;o.state="tentative";o.stage="strong_birth";o.observed_ns=now;o.prediction_only=false;o.identity_supported=!retired_ambiguous;o.estimated=c.note.center;
  o.head_visible=c.head_visible;o.body_visible=c.body_visible;o.left_rail=c.left_rail;o.right_rail=c.right_rail;o.tail=c.note.tail;
  o.action_evidence_valid=c.action_support&&c.head_visible&&b.playing_gate&&line_supported(c.note)&&!retired_ambiguous;
  if(o.action_evidence_valid){states_.back().supported_ns=now;o.supported_ns=now;auto note=c.note;note.recent_identity=0;executable_notes.push_back(note);executable_shortened.push_back(c.shortened_hold);forced.push_back(static_cast<int>(index));}
  o.reason=retired_ambiguous?"retired_birth_ambiguous":"new_current_track";r.observations.push_back(o);
 }
 // Resolve execution birth competition BEFORE adding points to the fitter.
 std::map<std::uint64_t,std::size_t> births;
 for(const auto& o:r.observations)if(o.birth_id&&!o.prediction_only)++births[o.birth_id];
 for(std::size_t i=forced.size();i>0;--i){const auto index=static_cast<std::size_t>(forced[i-1]);if(births[states_[index].birth_id]>1){
  states_[index].supported_ns=index<prior_supported.size()?prior_supported[index]:0;
  forced.erase(forced.begin()+static_cast<std::ptrdiff_t>(i-1));executable_notes.erase(executable_notes.begin()+static_cast<std::ptrdiff_t>(i-1));executable_shortened.erase(executable_shortened.begin()+static_cast<std::ptrdiff_t>(i-1));}}
 track_legacy_batch(r.executable,executable_notes,executable_shortened,histories,next_id_,&forced);
 for(std::size_t i=0;i<states_.size();++i){auto& s=states_[i];s.history.points=histories[i].points;s.history.point_bucket_ns=histories[i].point_bucket_ns;
  s.history.rail_anchor=histories[i].rail_anchor;s.history.rail_observed=histories[i].rail_observed;s.history.rail_frame=histories[i].rail_frame;
  s.history.revision=std::max(s.history.revision,histories[i].revision);
 }
 // A birth is an execution identity. Never publish two action candidates for it.
 for(auto& o:r.observations){o.revision=0;for(const auto& s:states_)if(s.history.id==o.track_id)o.revision=s.history.revision;
  if(o.birth_id&&births[o.birth_id]>1){o.action_evidence_valid=false;o.identity_supported=false;o.reason="birth_identity_competition";for(const auto& s:states_)if(s.history.id==o.track_id)o.supported_ns=s.supported_ns;}}
 std::erase_if(r.executable.targets,[&](const auto& t){for(const auto& s:states_)if(t.note_id==s.history.id)return births[s.birth_id]>1;return false;});
 for(auto& t:r.executable.targets){for(const auto& s:states_)if(t.note_id==s.history.id){t.note_id=s.birth_id;break;}}
 for(auto& o:r.observations)for(const auto& t:r.executable.targets)if(t.note_id==o.birth_id){for(std::size_t i=0;i<b.lines.size();++i){const auto& line=b.lines[i];const auto offset=std::abs((t.hit.x-line.center.x)*(-line.tangent.y)+(t.hit.y-line.center.y)*line.tangent.x);if(offset<.01&&std::abs(line.tangent.x*t.note.tangent.x+line.tangent.y*t.note.tangent.y)>=.95){o.line_id=i+1;break;}}}
 if(!r.capacity_valid){r.executable.targets.clear();r.executable.playing_gate=false;}
 return r;
}
nlohmann::json tracked_json(const TrackingResult& r){using json=nlohmann::json;json rows=json::array();for(const auto& o:r.observations)rows.push_back({
 {"track_id",o.track_id},{"birth_id",o.birth_id},{"revision",o.revision},{"candidate_id",o.candidate_id},{"state",o.state},{"stage",o.stage},{"cost",o.cost},
 {"competition_margin",o.competition_margin},{"line_id",o.line_id},{"observed_ns",o.observed_ns},{"supported_ns",o.supported_ns},{"estimated",{o.estimated.x,o.estimated.y}},{"velocity",{o.velocity.x,o.velocity.y}},
 {"uncertainty_px",o.uncertainty_px},{"prediction_only",o.prediction_only},{"identity_supported",o.identity_supported},{"action_evidence_valid",o.action_evidence_valid},
 {"head_visible",o.head_visible},{"body_visible",o.body_visible},{"left_rail",o.left_rail},{"right_rail",o.right_rail},{"tail",o.tail?json::array({o.tail->x,o.tail->y}):json(nullptr)},
 {"synthetic_filter_updates",o.synthetic_updates},{"reason",o.reason}});
 return {{"schema",1},{"capacity_valid",r.capacity_valid},{"reset_reason",r.reset_reason},{"observations",rows},{"executable",decision_json(r.executable)}};
}
} // namespace pas
