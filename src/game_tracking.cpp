#include "pas/game_tracking.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace pas {
namespace {
constexpr Nanoseconds minimum_fit_span_ns=30'000'000,history_bucket_ns=10'000'000;
double distance(Vec2 a,Vec2 b) {return std::hypot(a.x-b.x,a.y-b.y);}
double normal_distance(Vec2 p,const LineCandidate& l) {return (p.x-l.center.x)*(-l.tangent.y)+(p.y-l.center.y)*l.tangent.x;}
}
void track_legacy_batch(DecisionSnapshot& out,const std::vector<NoteCandidate>& notes,
 const std::vector<std::optional<NoteCandidate>>& shortened_holds,
 std::vector<GameTrackHistory>& tracks,std::uint64_t& next_id,const std::vector<int>* forced_assignment,std::vector<std::size_t>* source_indices) {
 const auto now=out.context.capture_ns;
 if(notes.size()>128||tracks.size()>128||shortened_holds.size()!=notes.size())
     throw std::invalid_argument("tracking projection capacity/shape");
 if(forced_assignment) {
     if(forced_assignment->size()!=notes.size())throw std::invalid_argument("forced assignment shape");
     std::set<int> used;
     for(std::size_t i=0;i<notes.size();++i) {
         const int index=(*forced_assignment)[i];
         if(index<0||static_cast<std::size_t>(index)>=tracks.size()||!used.insert(index).second)
             throw std::invalid_argument("forced assignment index/duplicate");
         if(tracks[index].kind!=notes[i].kind&&
             !(tracks[index].kind==NoteKind::hold&&notes[i].kind==NoteKind::tap&&shortened_holds[i]))
             throw std::invalid_argument("forced assignment type");
     }
 }
    struct Pair { std::size_t note, track; double cost; };
    std::vector<Pair> pairs; pairs.reserve(notes.size()*tracks.size());
    std::set<std::uint64_t> rail_claims;
    for(const auto& note:notes) if(note.recent_identity) rail_claims.insert(note.recent_identity);
    for(std::size_t ni=0;!forced_assignment&&ni<notes.size();++ni) {
        const auto& n=notes[ni];
        for(std::size_t ti=0;ti<tracks.size();++ti) {
            const auto& t=tracks[ti];
            if(n.recent_identity) {
                if(n.recent_identity==t.id) pairs.push_back({ni,ti,0});
                continue;
            }
            if(rail_claims.contains(t.id)) continue;
            Vec2 point=n.center;
            if(t.kind!=n.kind) {
                if(t.kind!=NoteKind::hold||!shortened_holds[ni]||
                   distance(shortened_holds[ni]->center,t.last)>=40) continue;
                point=shortened_holds[ni]->center;
            }
            Vec2 expected=t.last;
            if(t.points.size()>=2&&t.points.back().t-t.points.front().t>=minimum_fit_span_ns) {
                const auto& a=t.points[t.points.size()>3?t.points.size()-4:0]; const auto& b=t.points.back();
                const double dt=(b.t-a.t)/1e9;
                if(dt>0) {const double future=(now-b.t)/1e9;
                    expected.x+=(b.p.x-a.p.x)/dt*future; expected.y+=(b.p.y-a.p.y)/dt*future;}
            }
            const double d=distance(expected,point);
            if(d<out.context.width*.08) pairs.push_back({ni,ti,d});
        }
    }
    std::sort(pairs.begin(),pairs.end(),[](const Pair& a,const Pair& b) {
        if(a.cost!=b.cost) return a.cost<b.cost;
        if(a.note!=b.note) return a.note<b.note; return a.track<b.track;
    });
    std::vector<int> assigned=forced_assignment?*forced_assignment:std::vector<int>(notes.size(),-1);
    std::vector<bool> track_used(tracks.size()), uncertain(notes.size());
    for(const auto& pair:pairs) {
        if(forced_assignment) break;
        if(assigned[pair.note]>=0||track_used[pair.track]) continue;
        assigned[pair.note]=static_cast<int>(pair.track); track_used[pair.track]=true;
        for(const auto& alternative:pairs) {
            if(alternative.cost>pair.cost+8) break;
            if((alternative.note==pair.note&&alternative.track!=pair.track)||
               (alternative.track==pair.track&&alternative.note!=pair.note)) uncertain[pair.note]=true;
        }
    }
    for(std::size_t ni=0;ni<notes.size();++ni) {
        const auto& n=notes[ni];
        GameTrackHistory* match=assigned[ni]>=0?&tracks[static_cast<std::size_t>(assigned[ni])]:nullptr;
        const bool ambiguous=uncertain[ni];
        if(!match) {
            if(tracks.size()==128) {out.capacity_valid=false; continue;}
            tracks.push_back({++next_id,0,n.kind,n.center,now,{},n}); match=&tracks.back();
        }
        match->observed=now;
        GameTarget target; target.note_id=match->id; target.revision=++match->revision;
        target.note=n;
        if(match->kind==NoteKind::hold&&n.kind==NoteKind::tap) {
            target.note=*shortened_holds[ni];
        }
        match->last=target.note.center;
        match->appearance=target.note;
        const bool held_anchor=match->rail_anchor&&match->rail_anchor->head_on_line&&now-match->rail_observed<=90'000'000;
        if(target.note.rails_geometry&&!ambiguous&&(!held_anchor||target.note.head_on_line)) {
            match->rail_anchor=target.note;match->rail_observed=now;match->rail_frame=out.context.frame;
        }
        target.evidence_ns=now; target.expires_ns=now+100'000'000;
        target.reason=ambiguous?"association_ambiguous":"line_unobservable";
        const LineCandidate* selected=nullptr;
        for(const auto& line:out.lines) {
            if(!line.association_valid)continue;
            if(std::abs(line.tangent.x*n.tangent.x+line.tangent.y*n.tangent.y)<.95||line.length<out.context.width*.32) continue;
            if(std::abs((n.center.x-line.center.x)*line.tangent.x+
                        (n.center.y-line.center.y)*line.tangent.y)>line.length/2+n.width) continue;
            if(!match->points.empty()&&line.track_id&&match->points.back().line.track_id==line.track_id) {
                selected=&line;break;
            }
            if(!selected||line.confidence>selected->confidence||
               (line.confidence==selected->confidence&&line.length>selected->length)) selected=&line;
        }
        if(!selected&&out.lines.size()==1&&out.lines.front().association_valid) selected=&out.lines.front();
        if(selected&&!ambiguous) {
            const auto& l=*selected;
            const auto latest_distance=normal_distance(target.note.center,l);
            target.line_id=l.track_id;target.distance=latest_distance;
            target.hit={target.note.center.x+latest_distance*l.tangent.y,
                        target.note.center.y-latest_distance*l.tangent.x};
            if(!match->points.empty()) {
                const auto& prior=match->points.back();const double dt=(now-prior.t)/1e9;
                if(dt>0&&(!l.track_id||prior.line.track_id==l.track_id)) {
                    const double old_d=normal_distance(prior.p,prior.line);
                    const Vec2 old_hit{prior.p.x+old_d*prior.line.tangent.y,prior.p.y-old_d*prior.line.tangent.x};
                    target.hit_velocity={(target.hit.x-old_hit.x)/dt,(target.hit.y-old_hit.y)/dt};
                } else match->points.clear();
            }
            const GameTrackPoint point{now,target.note.center,l,target.note.tail,target.note.rails_geometry};
            // A delivery burst must not evict the complete temporal baseline.
            // Keep the newest point in each bounded, host-clock 10 ms bucket.
            if(!match->points.empty()&&now-match->point_bucket_ns<history_bucket_ns)
                match->points.back()=point;
            else {match->points.push_back(point);match->point_bucket_ns=now;}
            while(match->points.size()>6 || (!match->points.empty()&&now-match->points.front().t>90'000'000))
                match->points.pop_front();
            target.samples=static_cast<int>(match->points.size());
            target.history_span_ns=match->points.back().t-match->points.front().t;
            target.reason="insufficient_history";
            if(match->points.size()>=3&&target.history_span_ns<minimum_fit_span_ns)
                target.reason="insufficient_temporal_span";
            if(match->points.size()>=3&&target.history_span_ns>=minimum_fit_span_ns) {
                double mt=0,md=0,denom=0,numerator=0;
                for(const auto& p:match->points) {mt+=(p.t-now)/1e9; md+=normal_distance(p.p,p.line);}
                mt/=match->points.size(); md/=match->points.size();
                for(const auto& p:match->points) {const double t=(p.t-now)/1e9-mt;
                    denom+=t*t; numerator+=t*(normal_distance(p.p,p.line)-md);}
                if(denom>0) {
                    const double v=numerator/denom, d=md-v*mt; double residual=0;
                    for(const auto& p:match->points) {
                        const double error=normal_distance(p.p,p.line)-(d+v*(p.t-now)/1e9);
                        residual+=error*error;
                    }
                    residual=std::sqrt(residual/match->points.size());
                    target.distance=d; target.velocity=v; target.residual=residual;
                    const double tau=std::abs(v)>5?-d/v:-1;
                    target.reason=std::abs(v)<=5?"relative_velocity_small":
                        tau<0?"root_past":tau>.35?"outside_short_horizon":
                        residual>8?"nonlinear_or_mismatch":"prediction_observe_only";
                    if(tau>=-.04&&tau<=.35&&residual<=8&&std::abs(v)>5) {
                        target.reason="prediction_observe_only";
                        target.crossing_ns=now+static_cast<Nanoseconds>(std::llround(tau*1e9));
                        target.uncertainty_ns=static_cast<Nanoseconds>(std::llround(
                            std::max(2.0,residual)/std::abs(v)*1e9));
                        // Timing uses the fitted distance. The hit point stays
                        // on the independently observed current line; fit
                        // residual must not displace it in the normal axis.
                    }
                    if(target.note.kind==NoteKind::hold&&target.note.tail) {
                        double tt=0,td=0,tail_den=0,tail_num=0; int count=0;
                        for(const auto& p:match->points) if(p.tail) {tt+=(p.t-now)/1e9;
                            td+=normal_distance(*p.tail,p.line); ++count;}
                        if(count>=3) {
                            tt/=count; td/=count;
                            for(const auto& p:match->points) if(p.tail) {
                                const double x=(p.t-now)/1e9-tt;
                                tail_den+=x*x; tail_num+=x*(normal_distance(*p.tail,p.line)-td);
                            }
                            const double tail_v=tail_den>0?tail_num/tail_den:0;
                            const double tail_d=td-tail_v*tt;
                            const double tail_tau=std::abs(tail_v)>5?-tail_d/tail_v:-1;
                            if(tail_tau>=-.05&&tail_tau<3)
                                target.tail_crossing_ns=now+static_cast<Nanoseconds>(std::llround(tail_tau*1e9));
                        }
                    }
                }
            }
        } else if(out.lines.size()>1) target.reason="multiple_line_association_unvalidated";
        out.targets.push_back(std::move(target));if(source_indices)source_indices->push_back(ni);
    }
}
CandidateBatch make_candidate_batch(const DecisionSnapshot& s,const Frame& f,
 const std::vector<NoteCandidate>& notes,const std::vector<std::optional<NoteCandidate>>& shortened,
 const std::vector<GameTrackHistory>& history) {
 CandidateBatch b;b.extractor_version=31;b.context=s.context;b.ui=s.ui;b.playing_gate=s.playing_gate;
 b.capacity_valid=s.capacity_valid;b.source_valid=f.source_valid;
 b.extraction_start_ns=s.recognition_start_ns;b.lines=s.lines;
 for(std::size_t i=0;i<notes.size();++i) {
  TrackingCandidate c;c.candidate_id=i+1;c.note=notes[i];c.shortened_hold=shortened[i];
  c.body_visible=c.note.kind==NoteKind::hold;c.left_rail=c.right_rail=c.note.rails_geometry;
  c.origin=c.note.recent_identity?"current_rails_recent_anchor":c.note.direct_rails_evidence?"current_reconstructed_front":"color_core";
  c.quality=c.note.kind==NoteKind::ambiguous?ObservationQuality::rejected:
      c.note.recent_identity?ObservationQuality::weak_current:ObservationQuality::strong_current;
  c.action_support=c.quality==ObservationQuality::strong_current;
  if(c.note.recent_identity) for(const auto& h:history) if(h.id==c.note.recent_identity) {
   c.hint_source_frame=h.rail_frame;c.hint_age_ns=f.capture_complete_ns-h.rail_observed;
   c.action_support=c.note.rails_geometry&&c.note.head_on_line&&c.hint_age_ns>=0&&c.hint_age_ns<=90'000'000;
  }
  b.candidates.push_back(std::move(c));
 }
 return b;
}
namespace {
using json=nlohmann::json;
json vec(Vec2 p){return json::array({p.x,p.y});}
Vec2 point(const json& j){if(!j.is_array()||j.size()!=2)throw std::invalid_argument("point shape");Vec2 p{j[0].get<double>(),j[1].get<double>()};if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::invalid_argument("nonfinite point");return p;}
json note_json(const NoteCandidate& n){return {{"kind",name(n.kind)},{"center",vec(n.center)},{"tangent",vec(n.tangent)},
 {"width",n.width},{"height",n.height},{"confidence_diagnostic_only",n.confidence},{"tail",n.tail?vec(*n.tail):json(nullptr)},
 {"outline",n.outline_evidence},{"rails",n.rails_geometry},{"head_on_line",n.head_on_line},{"direct_rails",n.direct_rails_evidence},{"extractor_hint_id",n.recent_identity}};}
NoteCandidate parse_note(const json& j){NoteCandidate n;const auto k=j.at("kind").get<std::string>();bool found=false;
 for(auto kind:{NoteKind::tap,NoteKind::hold,NoteKind::drag,NoteKind::flick,NoteKind::ambiguous})if(k==name(kind)){n.kind=kind;found=true;}
 if(!found)throw std::invalid_argument("note kind");n.center=point(j.at("center"));n.tangent=point(j.at("tangent"));
 n.width=j.at("width");n.height=j.at("height");n.confidence=j.value("confidence_diagnostic_only",0.0);
 if(!std::isfinite(n.width)||!std::isfinite(n.height)||n.width<=0||n.height<=0||n.width>8192||n.height>8192||std::abs(std::hypot(n.tangent.x,n.tangent.y)-1)>.01)throw std::invalid_argument("note geometry");
 if(j.contains("tail")&&!j["tail"].is_null())n.tail=point(j["tail"]);
 n.outline_evidence=j.value("outline",false);n.rails_geometry=j.value("rails",false);n.head_on_line=j.value("head_on_line",false);
 n.direct_rails_evidence=j.value("direct_rails",false);n.recent_identity=j.value("extractor_hint_id",std::uint64_t{0});return n;}
}
json candidate_batch_json(const CandidateBatch& b){json cs=json::array(),ls=json::array();
 for(const auto& c:b.candidates)cs.push_back({{"candidate_id",c.candidate_id},{"note",note_json(c.note)},
  {"shortened_hold",c.shortened_hold?note_json(*c.shortened_hold):json(nullptr)},
  {"quality",c.quality==ObservationQuality::strong_current?"strong_current":c.quality==ObservationQuality::weak_current?"weak_current":"rejected"},
  {"head_visible",c.head_visible},{"body_visible",c.body_visible},{"left_rail",c.left_rail},{"right_rail",c.right_rail},
  {"action_support",c.action_support},{"origin",c.origin},{"hint_source_frame",c.hint_source_frame},{"hint_age_ns",c.hint_age_ns}});
 for(const auto& l:b.lines)ls.push_back({{"center",vec(l.center)},{"tangent",vec(l.tangent)},{"length",l.length},{"thickness",l.thickness},{"confidence",l.confidence},
   {"line_id",l.track_id},{"observed_ns",l.observed_ns},{"velocity",vec(l.velocity)},
   {"angular_velocity",l.angular_velocity},{"association_valid",l.association_valid}});
 return {{"schema",1},{"extractor_version",b.extractor_version},{"quality_version",b.quality_version},{"history_source",b.history_source},
 {"context",{{"epoch",b.context.epoch},{"generation",b.context.generation},{"geometry",b.context.geometry},{"frame",b.context.frame},{"capture_ns",b.context.capture_ns},{"width",b.context.width},{"height",b.context.height},{"rotation",b.context.rotation}}},
 {"extraction_start_ns",b.extraction_start_ns},{"extraction_end_ns",b.extraction_end_ns},{"ui",name(b.ui)},
 {"playing_gate",b.playing_gate},{"capacity_valid",b.capacity_valid},{"source_valid",b.source_valid},{"lines",ls},{"candidates",cs}};
}
CandidateBatch parse_candidate_batch(const json& j){CandidateBatch b;if(j.at("schema")!=1)throw std::invalid_argument("candidate schema");
 const auto& c=j.at("context");b.context={c.at("epoch"),c.at("generation"),c.at("geometry"),c.at("frame"),c.at("capture_ns"),c.at("width"),c.at("height"),c.at("rotation")};
 if(b.context.width<2||b.context.height<2||b.context.width>4096||b.context.height>4096||b.context.capture_ns<0||b.context.rotation<0||b.context.rotation>3)throw std::invalid_argument("candidate context");
 b.extractor_version=j.at("extractor_version");b.quality_version=j.at("quality_version");b.history_source=j.at("history_source");
 if((b.extractor_version<29||b.extractor_version>31)||b.quality_version!=1||b.history_source.empty()||b.history_source.size()>128)throw std::invalid_argument("candidate extractor/quality/source version");
 b.extraction_start_ns=j.at("extraction_start_ns");b.extraction_end_ns=j.at("extraction_end_ns");
 if(b.extraction_start_ns<0||b.extraction_end_ns<b.extraction_start_ns)throw std::invalid_argument("extraction time order");
 const auto ui=j.at("ui").get<std::string>();bool found=false;for(auto u:{GameUi::unknown,GameUi::menu,GameUi::loading,GameUi::playing,GameUi::paused,GameUi::result})if(ui==name(u)){b.ui=u;found=true;}
 if(!found)throw std::invalid_argument("candidate UI");b.playing_gate=j.at("playing_gate");b.capacity_valid=j.at("capacity_valid");b.source_valid=j.at("source_valid");
 if(j.at("lines").size()>16||j.at("candidates").size()>128)throw std::invalid_argument("candidate capacity");
 for(const auto& l:j.at("lines")){LineCandidate line{point(l.at("center")),point(l.at("tangent")),l.at("length"),l.at("thickness"),l.at("confidence")};
  line.track_id=l.value("line_id",std::uint64_t{0});line.observed_ns=l.value("observed_ns",Nanoseconds{0});
  if(l.contains("velocity"))line.velocity=point(l.at("velocity"));
  line.angular_velocity=l.value("angular_velocity",0.0);line.association_valid=l.value("association_valid",true);
  if(line.observed_ns<0||!std::isfinite(line.angular_velocity))throw std::invalid_argument("line motion metadata");
  if(!std::isfinite(line.length)||line.length<=0||!std::isfinite(line.thickness)||line.thickness<=0||!std::isfinite(line.confidence)||std::abs(std::hypot(line.tangent.x,line.tangent.y)-1)>.01)throw std::invalid_argument("line geometry");b.lines.push_back(line);}
 std::set<std::uint64_t> ids;for(const auto& v:j.at("candidates")){TrackingCandidate t;t.candidate_id=v.at("candidate_id");
  if(!t.candidate_id||!ids.insert(t.candidate_id).second)throw std::invalid_argument("duplicate candidate id");t.note=parse_note(v.at("note"));
  if(!v.at("shortened_hold").is_null())t.shortened_hold=parse_note(v["shortened_hold"]);
  const auto q=v.at("quality");if(q=="strong_current")t.quality=ObservationQuality::strong_current;else if(q=="weak_current")t.quality=ObservationQuality::weak_current;else if(q!="rejected")throw std::invalid_argument("candidate quality");
  t.head_visible=v.at("head_visible");t.body_visible=v.at("body_visible");t.left_rail=v.at("left_rail");t.right_rail=v.at("right_rail");t.action_support=v.at("action_support");t.origin=v.at("origin");
  if(t.origin.empty()||t.origin.size()>128)throw std::invalid_argument("candidate origin capacity");
  t.hint_source_frame=v.at("hint_source_frame");t.hint_age_ns=v.at("hint_age_ns");if(t.hint_age_ns<0)throw std::invalid_argument("negative hint age");b.candidates.push_back(t);}
 return b;
}
} // namespace pas
