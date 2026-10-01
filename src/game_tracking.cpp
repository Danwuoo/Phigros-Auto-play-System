#include "pas/game_tracking.hpp"
#include "pas/strategy_version.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace pas {
namespace {
constexpr Nanoseconds minimum_fit_span_ns=30'000'000,history_bucket_ns=10'000'000;
double distance(Vec2 a,Vec2 b) {return std::hypot(a.x-b.x,a.y-b.y);}
double normal_distance(Vec2 p,const LineCandidate& l) {return (p.x-l.center.x)*(-l.tangent.y)+(p.y-l.center.y)*l.tangent.x;}
bool project_recent_confirmed_line(GameTarget& target,const GameTrackHistory& track,
 const DecisionSnapshot& scene,Nanoseconds now) {
    if(!track.confirmed_line_id||track.points.size()<3||
       track.points.back().t-track.points.front().t<minimum_fit_span_ns||
       (target.note.kind!=NoteKind::tap&&target.note.kind!=NoteKind::flick)||
       target.note.held_body_evidence||
       target.note.held_body_patch||target.note.confidence<.5)return false;
    const auto& last=track.points.back();
    const Nanoseconds gap=now-last.t;
    if(gap<=0||gap>40'000'000||last.line.track_id!=track.confirmed_line_id||
       !last.line.motion_valid||last.line.length<scene.context.width*.24||
       !std::all_of(track.points.begin(),track.points.end(),[&](const auto& point){
           return point.line.track_id==track.confirmed_line_id;
       }))return false;
    const double dt=gap/1e9,angle=last.line.angular_velocity*dt;
    LineCandidate projected=last.line;
    projected.center.x+=last.line.velocity.x*dt;
    projected.center.y+=last.line.velocity.y*dt;
    projected.tangent={last.line.tangent.x*std::cos(angle)-last.line.tangent.y*std::sin(angle),
                       last.line.tangent.x*std::sin(angle)+last.line.tangent.y*std::cos(angle)};
    const double d=normal_distance(target.note.center,projected);
    const Vec2 hit{target.note.center.x+d*projected.tangent.y,
                   target.note.center.y-d*projected.tangent.x};
    const double along=(hit.x-projected.center.x)*projected.tangent.x+
                       (hit.y-projected.center.y)*projected.tangent.y;
    const double prior_along=(hit.x-last.line.center.x)*last.line.tangent.x+
                             (hit.y-last.line.center.y)*last.line.tangent.y;
    const double half=last.line.length*.5-4;
    if(half<=0||std::abs(along)>half||std::abs(prior_along)>half)return false;
    // A current ridge at this Note or its old-line hit makes the replacement
    // ambiguous. An unrelated ridge elsewhere does not erase the old ID.
    for(const auto& line:scene.lines) if(line.association_valid&&
       line.length>=scene.context.width*.24) {
        if(line.track_id==track.confirmed_line_id)return false;
        const double note_along=std::abs((target.note.center.x-line.center.x)*line.tangent.x+
                                         (target.note.center.y-line.center.y)*line.tangent.y);
        const double hit_along=std::abs((hit.x-line.center.x)*line.tangent.x+
                                        (hit.y-line.center.y)*line.tangent.y);
        if((note_along<=line.length*.5+target.note.width&&
            std::abs(normal_distance(target.note.center,line))<=32)||
           (hit_along<=line.length*.5+target.note.width&&
            std::abs(normal_distance(hit,line))<=32))return false;
    }
    const auto& first=track.points.front();
    const double span=(last.t-first.t)/1e9;
    const double first_d=normal_distance(first.p,first.line);
    const double last_d=normal_distance(last.p,last.line);
    const double prior_v=(last_d-first_d)/span;
    const double current_v=(d-last_d)/dt;
    if(!std::isfinite(prior_v)||!std::isfinite(current_v)||
       std::abs(prior_v)<75||std::abs(current_v)<75||
       prior_v*current_v<=0||last_d*prior_v>=0)return false;
    double residual=0;
    for(const auto& point:track.points) {
        const double expected=first_d+prior_v*(point.t-first.t)/1e9;
        residual=std::max(residual,std::abs(normal_distance(point.p,point.line)-expected));
    }
    const double line_error=2+last.line.motion_residual+
        last.line.angular_residual*std::abs(along)+
        dt*(std::hypot(last.line.velocity.x,last.line.velocity.y)*.05+
            std::abs(last.line.angular_velocity)*std::abs(along)*.05);
    const double mismatch=std::abs(d-(last_d+prior_v*dt));
    const double error=std::max(residual,line_error+mismatch);
    const double limit=std::clamp(target.note.width*.125,8.0,16.0);
    const double tau=-d/current_v;
    if(!std::isfinite(error)||error>limit||!std::isfinite(tau)||
       tau<-.04||tau>.35)return false;
    target.line_id=track.confirmed_line_id;
    target.line_projection_only=true;
    target.last_line_observed_ns=last.t;
    target.projected_line_along_px=along;
    target.projected_line_half_length_px=half;
    target.hit=hit;target.distance=d;target.velocity=current_v;target.residual=residual;
    target.prediction_error_px=error;target.fit_residual_limit_px=limit;
    target.samples=static_cast<int>(track.points.size());
    target.history_span_ns=now-first.t;
    target.crossing_ns=now+static_cast<Nanoseconds>(std::llround(tau*1e9));
    target.uncertainty_ns=static_cast<Nanoseconds>(std::llround(error/std::abs(current_v)*1e9));
    target.reason="recent_confirmed_line_projection";
    return true;
}
bool core_contains(const NoteCandidate& n,Vec2 p) {
 const Vec2 d{p.x-n.center.x,p.y-n.center.y};
 return std::abs(std::hypot(n.tangent.x,n.tangent.y)-1)<=.01&&
     std::abs(d.x*n.tangent.x+d.y*n.tangent.y)<=n.width*.5+2&&
     std::abs(-d.x*n.tangent.y+d.y*n.tangent.x)<=n.height*.5+2;
}
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
    }
    // A near edge to an already occupied track is not, by itself, an
    // alternative assignment. In the isolated two-note exchange, compare the
    // cost of BOTH edges. Larger components remain conservatively ambiguous.
    if(!forced_assignment) {
        const double unavailable=1e9;
        std::vector<std::vector<double>> costs(notes.size(),std::vector<double>(tracks.size(),unavailable));
        std::vector<int> owner(tracks.size(),-1);
        for(const auto& pair:pairs) costs[pair.note][pair.track]=pair.cost;
        for(std::size_t ni=0;ni<assigned.size();++ni)
            if(assigned[ni]>=0) owner[static_cast<std::size_t>(assigned[ni])]=static_cast<int>(ni);
        for(std::size_t ni=0;ni<assigned.size();++ni) {
            if(assigned[ni]<0) continue;
            const auto ti=static_cast<std::size_t>(assigned[ni]);
            const double selected=costs[ni][ti];
            for(const auto& alternative:pairs) {
                if(alternative.cost>selected+8) break;
                std::size_t nj=ni,tj=ti;
                if(alternative.note==ni&&alternative.track!=ti) tj=alternative.track;
                else if(alternative.track==ti&&alternative.note!=ni) nj=alternative.note;
                else continue;
                const int displaced=nj==ni?owner[tj]:assigned[nj];
                if(displaced<0) {uncertain[ni]=true;break;}
                const auto other_note=nj==ni?static_cast<std::size_t>(displaced):nj;
                const auto other_track=nj==ni?tj:static_cast<std::size_t>(displaced);
                // More than two involved tracks may permit an augmenting
                // chain. Do not claim unique identity without solving it.
                bool larger_component=false;
                for(std::size_t k=0;k<tracks.size();++k) {
                    if(k!=ti&&k!=other_track&&
                       (costs[ni][k]<unavailable||costs[other_note][k]<unavailable)) {
                        larger_component=true;break;
                    }
                }
                if(larger_component) {uncertain[ni]=true;break;}
                const double swap=costs[ni][other_track]+costs[other_note][ti];
                const double original=selected+costs[other_note][other_track];
                if(swap<=original+8) {uncertain[ni]=true;break;}
            }
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
        const auto prior_note_center=match->last;
        const auto prior_note_observed=match->observed;
        const auto prior_note_revision=match->revision;
        // A confirmed relation is bounded by its last real line measurement.
        // Once that window ends, a newly visible line must earn a fresh fit;
        // an expired ID cannot block every later line for this Note forever.
        if(match->confirmed_line_id&&!match->points.empty()&&
           now-match->points.back().t>90'000'000) {
            match->points.clear();match->confirmed_line_id=0;
            match->replacement_line_id=0;match->replacement_observations=0;
        }
        match->observed=now;
        GameTarget target; target.note_id=match->id; target.revision=++match->revision;
        target.note=n;
        if(match->kind==NoteKind::hold&&n.kind==NoteKind::tap) {
            target.note=*shortened_holds[ni];
        }
        match->last=target.note.center;
        match->appearance=target.note;
        const bool held_anchor=match->rail_anchor&&
            (match->rail_anchor->head_on_line||match->rail_anchor->held_body_evidence)&&now-match->rail_observed<=90'000'000;
        if(target.note.rails_geometry&&!ambiguous&&(!held_anchor||target.note.head_on_line||target.note.held_body_evidence)) {
            match->rail_anchor=target.note;match->rail_observed=now;match->rail_frame=out.context.frame;
        }
        target.evidence_ns=now; target.expires_ns=now+100'000'000;
        target.reason=ambiguous?"association_ambiguous":"line_unobservable";
        const LineCandidate* selected=nullptr;
        double best_score=1e9,second_score=1e9;
        for(const auto& line:out.lines) {
            if(!line.association_valid)continue;
            if(line.length<out.context.width*.24)continue;
            const double along=std::abs((n.center.x-line.center.x)*line.tangent.x+
                                        (n.center.y-line.center.y)*line.tangent.y);
            if(along>line.length*.5+n.width+24)continue;
            const double across=std::abs(normal_distance(n.center,line));
            const double alignment=std::abs(line.tangent.x*n.tangent.x+line.tangent.y*n.tangent.y);
            const bool same_recent=!match->points.empty()&&line.track_id&&
                match->points.back().line.track_id==line.track_id;
            double relative_trend=0;
            if(!match->points.empty()) {
                const auto& prior=match->points.back();
                const double dt=(now-prior.t)/1e9;
                if(dt>0&&dt<=.09) {
                    LineCandidate previous_line=line;
                    if(same_recent)previous_line=prior.line;
                    else if(line.motion_valid) {
                        previous_line.center.x-=line.velocity.x*dt;
                        previous_line.center.y-=line.velocity.y*dt;
                        const double a=-line.angular_velocity*dt;
                        previous_line.tangent={line.tangent.x*std::cos(a)-line.tangent.y*std::sin(a),
                                               line.tangent.x*std::sin(a)+line.tangent.y*std::cos(a)};
                    }
                    // A weak bounded cue: approach/recession in this line's
                    // own local frame, not the note's drawn orientation.
                    relative_trend=std::clamp((across-std::abs(normal_distance(prior.p,previous_line)))*.2,
                                              -6.0,6.0);
                }
            }
            // Appearance orientation is only a weak CURRENT cue. A distant
            // note can align as it approaches; relative position and the
            // bounded relation history carry the association instead.
            // Paired CURRENT rails carry a stronger local geometry cue than
            // a color core. This is a soft cost, not a far-angle exclusion:
            // an embedded orthogonal decoration cannot win on distance alone.
            const double orientation_weight=n.rails_geometry&&n.direct_rails_evidence?
                40:(across<32?12:3);
            const double score=across+std::max(0.0,along-line.length*.5)*2+
                (1-alignment)*orientation_weight+(1-line.confidence)*4+relative_trend-
                (same_recent?120:0);
            if(score<best_score) {second_score=best_score;best_score=score;selected=&line;}
            else second_score=std::min(second_score,score);
        }
        // A crossing line can momentarily be nearer than the line this Note
        // has approached over several current frames. Keep the confirmed
        // relation only while that same line is visible now and the measured
        // relative distance continues to close. This cannot create a new
        // relation or carry a line through a frame with no current pixels.
        bool preserve_confirmed=false;
        if(match->confirmed_line_id&&!match->points.empty()&&
           now-match->points.back().t<=90'000'000&&
           match->points.back().line.track_id==match->confirmed_line_id) {
            const auto established=std::find_if(out.lines.begin(),out.lines.end(),
                [&](const LineCandidate& line){
                    return line.track_id==match->confirmed_line_id&&line.association_valid&&
                        line.length>=out.context.width*.24;
                });
            if(established!=out.lines.end()) {
                const double along=std::abs((n.center.x-established->center.x)*established->tangent.x+
                                            (n.center.y-established->center.y)*established->tangent.y);
                const double current_distance=std::abs(normal_distance(n.center,*established));
                const double prior_distance=std::abs(normal_distance(match->points.back().p,
                                                                     match->points.back().line));
                if(along<=established->length*.5+n.width+24&&
                   current_distance<=prior_distance+std::max(12.0,n.width*.10)) {
                    selected=&*established;
                    preserve_confirmed=true;
                }
            }
        }
        const bool relation_ambiguous=!preserve_confirmed&&second_score-best_score<8;
        if(relation_ambiguous)selected=nullptr;
        // A sustained relation cannot jump to an unrelated surviving line.
        // A new ID may reconnect only where its current ridge is the same
        // local line as the last measured one, on two fresh frames. Start
        // its motion fit anew; the old root is never transferred.
        bool relation_conflict=false;
        if(selected&&match->confirmed_line_id&&
           selected->track_id!=match->confirmed_line_id) {
            const bool old_visible=std::any_of(out.lines.begin(),out.lines.end(),[&](const auto& line){
                return line.association_valid&&line.track_id==match->confirmed_line_id;
            });
            bool local_continuation=false;
            if(!old_visible&&!match->points.empty()&&
               now-match->points.back().t<=90'000'000) {
                const auto& prior=match->points.back();
                const double prior_d=normal_distance(prior.p,prior.line);
                const Vec2 prior_hit{prior.p.x+prior_d*prior.line.tangent.y,
                                     prior.p.y-prior_d*prior.line.tangent.x};
                const double alignment=std::abs(prior.line.tangent.x*selected->tangent.x+
                                                prior.line.tangent.y*selected->tangent.y);
                local_continuation=alignment>=.97&&
                    std::abs(normal_distance(prior_hit,*selected))<=36;
            }
            if(local_continuation) {
                if(match->replacement_line_id!=selected->track_id||
                   now-match->replacement_first_ns>60'000'000) {
                    match->replacement_line_id=selected->track_id;
                    match->replacement_first_ns=now;
                    match->replacement_observations=1;
                } else ++match->replacement_observations;
                const bool current_contact_support=
                    (n.kind==NoteKind::drag||
                     (n.kind==NoteKind::hold&&n.held_body_evidence&&n.rails_geometry))&&
                    std::abs(normal_distance(n.center,*selected))<=40;
                if(current_contact_support||
                   (match->replacement_observations>=2&&
                    now-match->replacement_first_ns>=12'000'000)) {
                    match->confirmed_line_id=selected->track_id;
                    match->points.clear();
                    match->replacement_line_id=0;
                    match->replacement_observations=0;
                } else relation_conflict=true;
            } else {
                match->replacement_line_id=0;
                match->replacement_observations=0;
                relation_conflict=true;
            }
            if(relation_conflict)selected=nullptr;
        } else if(!selected||selected->track_id==match->confirmed_line_id) {
            match->replacement_line_id=0;
            match->replacement_observations=0;
        }
        if(target.note.held_body_evidence&&selected&&!match->points.empty()&&
           match->points.back().line.track_id!=selected->track_id)selected=nullptr;
        if(selected&&!ambiguous) {
            const auto& l=*selected;
            const bool first_line_sample=match->points.empty();
            // A line is unoriented: u and -u describe identical current
            // pixels. Keep the local normal continuous for this Note's
            // measured history so a detector sign flip is not a reversal.
            LineCandidate local_line=l;
            if(!match->points.empty()&&l.track_id&&
               match->points.back().line.track_id==l.track_id&&
               l.tangent.x*match->points.back().line.tangent.x+
                   l.tangent.y*match->points.back().line.tangent.y<0) {
                local_line.tangent.x=-local_line.tangent.x;
                local_line.tangent.y=-local_line.tangent.y;
            }
            const auto latest_distance=normal_distance(target.note.center,local_line);
            target.line_id=l.track_id;target.distance=latest_distance;
            target.hit={target.note.center.x+latest_distance*local_line.tangent.y,
                        target.note.center.y-latest_distance*local_line.tangent.x};
            const bool front_is_touch=target.note.held_body_evidence&&!target.note.head_on_line;
            if(front_is_touch)target.hit=target.note.center;
            if(!match->points.empty()) {
                const auto& prior=match->points.back();const double dt=(now-prior.t)/1e9;
                if(dt>0&&(!l.track_id||prior.line.track_id==l.track_id)) {
                    const double old_d=normal_distance(prior.p,prior.line);
                    const Vec2 old_hit=prior.front_is_touch?prior.p:
                        Vec2{prior.p.x+old_d*prior.line.tangent.y,prior.p.y-old_d*prior.line.tangent.x};
                    target.hit_velocity={(target.hit.x-old_hit.x)/dt,(target.hit.y-old_hit.y)/dt};
                } else match->points.clear();
            }
            const GameTrackPoint point{now,target.note.center,local_line,target.note.tail,target.note.rails_geometry,front_is_touch};
            // A delivery burst must not evict the complete temporal baseline.
            // Keep the newest point in each bounded, host-clock 10 ms bucket.
            if(!match->points.empty()&&now-match->point_bucket_ns<history_bucket_ns)
                match->points.back()=point;
            else {match->points.push_back(point);match->point_bucket_ns=now;}
            while(match->points.size()>6 || (!match->points.empty()&&now-match->points.front().t>90'000'000))
                match->points.pop_front();
            if(!match->confirmed_line_id&&l.track_id&&match->points.size()>=3&&
               match->points.back().t-match->points.front().t>=minimum_fit_span_ns&&
               std::all_of(match->points.begin(),match->points.end(),[&](const auto& point){
                   return point.line.track_id==l.track_id;
               }))match->confirmed_line_id=l.track_id;
            bool segment_reset=false;
            if(match->points.size()>=3) {
                const auto& a=match->points[match->points.size()-3];
                const auto& b=match->points[match->points.size()-2];
                const auto& c=match->points.back();
                const double dt0=(b.t-a.t)/1e9,dt1=(c.t-b.t)/1e9;
                if(dt0>0&&dt1>0) {
                    const double d0=normal_distance(a.p,a.line),d1=normal_distance(b.p,b.line),
                        d2=normal_distance(c.p,c.line);
                    const double v0=(d1-d0)/dt0,v1=(d2-d1)/dt1;
                    const bool reversal=v0*v1<0&&std::abs(v0)>75&&std::abs(v1)>75&&
                        std::abs(d2-d1)>std::max(24.0,target.note.width*.2);
                    const bool jump=std::abs(d2-d1)>48&&
                        std::abs(d2-d1-v0*dt1)>std::max(24.0,std::abs(v0)*dt1);
                    if(reversal||jump) {
                        // The current sample begins a fresh measured segment.
                        // Do not average a turn or teleport into a plausible
                        // fast crossing. A new root needs its own 30 ms span.
                        match->points.clear();match->points.push_back(point);
                        match->point_bucket_ns=now;segment_reset=true;
                    }
                }
            }
            target.samples=static_cast<int>(match->points.size());
            target.history_span_ns=match->points.back().t-match->points.front().t;
            target.reason=segment_reset?"motion_discontinuity":"insufficient_history";
            if(!segment_reset&&target.samples==1&&match->revision==1&&
               std::abs(latest_distance)<=8&&target.note.kind!=NoteKind::drag)
                target.reason="near_line_appearance_unqualified";
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
                    // Spatial error is not a fixed timing error: 9 px at
                    // 750 px/s is 12 ms, at 100 px/s it is 90 ms. Keep a
                    // bounded geometry gate separate from timing uncertainty.
                    const double spatial_limit=std::clamp(target.note.width*.125,8.0,16.0);
                    const double prediction_error=std::max({2.0,residual,std::abs(latest_distance-d)});
                    target.prediction_error_px=prediction_error;target.fit_residual_limit_px=spatial_limit;
                    const double tau=std::abs(v)>5?-d/v:-1;
                    target.reason=std::abs(v)<=5?"relative_velocity_small":
                        tau<0?"root_past":tau>.35?"outside_short_horizon":
                        residual>spatial_limit||std::abs(latest_distance-d)>spatial_limit?
                            "nonlinear_or_mismatch":"prediction_observe_only";
                    if(tau>=-.04&&tau<=.35&&residual<=spatial_limit&&
                       std::abs(latest_distance-d)<=spatial_limit&&std::abs(v)>5) {
                        target.reason="prediction_observe_only";
                        target.crossing_ns=now+static_cast<Nanoseconds>(std::llround(tau*1e9));
                        target.uncertainty_ns=static_cast<Nanoseconds>(std::llround(
                            prediction_error/std::abs(v)*1e9));
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
            // A newly visible line can coincide with a Tap already tracked
            // through line-free frames. The current head and current ridge
            // must actually overlap; the bounded prior note motion only
            // qualifies that observation, never supplies a hidden line.
            if(first_line_sample&&prior_note_revision>=2&&
               target.note.kind==NoteKind::tap&&!target.note.outline_evidence&&
               prior_note_observed<now&&now-prior_note_observed<=40'000'000&&
               l.observed_ns==now&&!target.note.rails_geometry&&
               l.confidence>=.8&&l.length>=out.context.width*.5&&
               target.note.confidence>=.5&&target.note.height>=4&&
               core_contains(target.note,target.hit)&&
               std::abs(latest_distance)<=std::min(8.0,target.note.height*.5+4)) {
                const double prior_distance=normal_distance(prior_note_center,l);
                const double approach=latest_distance-prior_distance;
                const auto local_lines=std::count_if(out.lines.begin(),out.lines.end(),[&](const LineCandidate& line) {
                    return line.observed_ns==now&&line.confidence>=.5&&
                        line.length>=out.context.width*.32&&
                        std::abs(normal_distance(target.note.center,line))<=std::max(8.0,target.note.height*.5+4)&&
                        std::abs((target.note.center.x-line.center.x)*line.tangent.x+
                                 (target.note.center.y-line.center.y)*line.tangent.y)<=line.length*.5+target.note.width*.5;
                });
                if(local_lines==1&&std::abs(approach)>=3&&std::abs(prior_distance)>=3&&
                   std::abs(latest_distance)<=std::abs(prior_distance)+2&&
                   prior_distance*approach<0&&
                   std::abs(prior_distance)<=std::max(32.0,target.note.width*.25))
                    target.reason="current_tap_overlap";
            }
        } else {
            if(relation_conflict)target.reason="confirmed_line_relation_conflict";
            else if(out.lines.size()>1) target.reason="multiple_line_association_unvalidated";
            if(!ambiguous)project_recent_confirmed_line(target,*match,out,now);
        }
        if(target.note.held_body_patch) {
            target.crossing_ns.reset();target.tail_crossing_ns.reset();
            if(selected&&!ambiguous)target.reason="held_body_touch_only";
        }
        out.targets.push_back(std::move(target));if(source_indices)source_indices->push_back(ni);
    }
}
CandidateBatch make_candidate_batch(const DecisionSnapshot& s,const Frame& f,
 const std::vector<NoteCandidate>& notes,const std::vector<std::optional<NoteCandidate>>& shortened,
 const std::vector<GameTrackHistory>& history) {
 CandidateBatch b;b.extractor_version=game_observer_version;b.context=s.context;b.ui=s.ui;b.playing_gate=s.playing_gate;
 b.capacity_valid=s.capacity_valid;b.source_valid=f.source_valid;
 b.extraction_start_ns=s.recognition_start_ns;b.lines=s.lines;
 for(std::size_t i=0;i<notes.size();++i) {
  TrackingCandidate c;c.candidate_id=i+1;c.note=notes[i];c.shortened_hold=shortened[i];
  c.body_visible=c.note.kind==NoteKind::hold;c.left_rail=c.right_rail=c.note.rails_geometry;
  c.head_visible=!c.note.held_body_patch;
  c.origin=c.note.held_body_patch?"current_body_patch_recent_anchor":
      c.note.recent_identity?"current_rails_recent_anchor":c.note.direct_rails_evidence?"current_reconstructed_front":"color_core";
  c.quality=c.note.kind==NoteKind::ambiguous?ObservationQuality::rejected:
      c.note.recent_identity?ObservationQuality::weak_current:ObservationQuality::strong_current;
  c.action_support=c.quality==ObservationQuality::strong_current;
  if(c.note.recent_identity) for(const auto& h:history) if(h.id==c.note.recent_identity) {
   c.hint_source_frame=h.rail_frame;c.hint_age_ns=f.capture_complete_ns-h.rail_observed;
   c.action_support=c.note.rails_geometry&&(c.note.head_on_line||c.note.held_body_evidence)&&c.hint_age_ns>=0&&c.hint_age_ns<=90'000'000;
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
 {"outline",n.outline_evidence},{"rails",n.rails_geometry},{"head_on_line",n.head_on_line},{"held_body_evidence",n.held_body_evidence},{"held_body_patch",n.held_body_patch},{"direct_rails",n.direct_rails_evidence},{"extractor_hint_id",n.recent_identity}};}
NoteCandidate parse_note(const json& j){NoteCandidate n;const auto k=j.at("kind").get<std::string>();bool found=false;
 for(auto kind:{NoteKind::tap,NoteKind::hold,NoteKind::drag,NoteKind::flick,NoteKind::ambiguous})if(k==name(kind)){n.kind=kind;found=true;}
 if(!found)throw std::invalid_argument("note kind");n.center=point(j.at("center"));n.tangent=point(j.at("tangent"));
 n.width=j.at("width");n.height=j.at("height");n.confidence=j.value("confidence_diagnostic_only",0.0);
 if(!std::isfinite(n.width)||!std::isfinite(n.height)||n.width<=0||n.height<=0||n.width>8192||n.height>8192||std::abs(std::hypot(n.tangent.x,n.tangent.y)-1)>.01)throw std::invalid_argument("note geometry");
 if(j.contains("tail")&&!j["tail"].is_null())n.tail=point(j["tail"]);
 n.outline_evidence=j.value("outline",false);n.rails_geometry=j.value("rails",false);n.head_on_line=j.value("head_on_line",false);
 n.held_body_evidence=j.value("held_body_evidence",false);
 n.held_body_patch=j.value("held_body_patch",false);
 if(n.held_body_patch&&(!n.held_body_evidence||n.head_on_line||n.tail))throw std::invalid_argument("held body patch contract");
 if(n.held_body_evidence&&(!n.outline_evidence||!n.rails_geometry||n.kind!=NoteKind::hold))throw std::invalid_argument("held body evidence contract");
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
   {"angular_velocity",l.angular_velocity},{"association_valid",l.association_valid},
   {"motion_valid",l.motion_valid},{"motion_samples",l.motion_samples},{"motion_span_ns",l.motion_span_ns},
   {"motion_residual_px",l.motion_residual},{"angular_residual_rad",l.angular_residual}});
 return {{"schema",1},{"extractor_version",b.extractor_version},{"quality_version",b.quality_version},{"history_source",b.history_source},
 {"context",{{"epoch",b.context.epoch},{"generation",b.context.generation},{"geometry",b.context.geometry},{"frame",b.context.frame},{"capture_ns",b.context.capture_ns},{"width",b.context.width},{"height",b.context.height},{"rotation",b.context.rotation}}},
 {"extraction_start_ns",b.extraction_start_ns},{"extraction_end_ns",b.extraction_end_ns},{"ui",name(b.ui)},
 {"playing_gate",b.playing_gate},{"capacity_valid",b.capacity_valid},{"source_valid",b.source_valid},{"lines",ls},{"candidates",cs}};
}
CandidateBatch parse_candidate_batch(const json& j){CandidateBatch b;if(j.at("schema")!=1)throw std::invalid_argument("candidate schema");
 const auto& c=j.at("context");b.context={c.at("epoch"),c.at("generation"),c.at("geometry"),c.at("frame"),c.at("capture_ns"),c.at("width"),c.at("height"),c.at("rotation")};
 if(b.context.width<2||b.context.height<2||b.context.width>4096||b.context.height>4096||b.context.capture_ns<0||b.context.rotation<0||b.context.rotation>3)throw std::invalid_argument("candidate context");
 b.extractor_version=j.at("extractor_version");b.quality_version=j.at("quality_version");b.history_source=j.at("history_source");
 if((b.extractor_version<29||b.extractor_version>game_observer_version)||b.quality_version!=1||b.history_source.empty()||b.history_source.size()>128)throw std::invalid_argument("candidate extractor/quality/source version");
 b.extraction_start_ns=j.at("extraction_start_ns");b.extraction_end_ns=j.at("extraction_end_ns");
 if(b.extraction_start_ns<0||b.extraction_end_ns<b.extraction_start_ns)throw std::invalid_argument("extraction time order");
 const auto ui=j.at("ui").get<std::string>();bool found=false;for(auto u:{GameUi::unknown,GameUi::menu,GameUi::loading,GameUi::playing,GameUi::paused,GameUi::result})if(ui==name(u)){b.ui=u;found=true;}
 if(!found)throw std::invalid_argument("candidate UI");b.playing_gate=j.at("playing_gate");b.capacity_valid=j.at("capacity_valid");b.source_valid=j.at("source_valid");
 if(j.at("lines").size()>16||j.at("candidates").size()>128)throw std::invalid_argument("candidate capacity");
 for(const auto& l:j.at("lines")){LineCandidate line{point(l.at("center")),point(l.at("tangent")),l.at("length"),l.at("thickness"),l.at("confidence")};
  line.track_id=l.value("line_id",std::uint64_t{0});line.observed_ns=l.value("observed_ns",Nanoseconds{0});
  if(l.contains("velocity"))line.velocity=point(l.at("velocity"));
  line.angular_velocity=l.value("angular_velocity",0.0);line.association_valid=l.value("association_valid",true);
  line.motion_valid=l.value("motion_valid",false);line.motion_samples=l.value("motion_samples",0);
  line.motion_span_ns=l.value("motion_span_ns",Nanoseconds{0});
  line.motion_residual=l.value("motion_residual_px",0.0);line.angular_residual=l.value("angular_residual_rad",0.0);
  if(line.motion_samples<0||line.motion_samples>6||line.motion_span_ns<0||line.motion_span_ns>90'000'000||
     !std::isfinite(line.motion_residual)||!std::isfinite(line.angular_residual)||line.motion_residual<0||line.angular_residual<0||
     (line.motion_valid&&(line.motion_samples<3||line.motion_span_ns<30'000'000||line.motion_residual>2||line.angular_residual>.015)))
      throw std::invalid_argument("line motion fit metadata");
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
