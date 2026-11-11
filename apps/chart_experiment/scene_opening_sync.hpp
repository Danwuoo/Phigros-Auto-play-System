#pragma once
#include "rhythm/phigros_pose.hpp"
#include "rhythm/visual_candidates.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>

namespace rhythm::experimental {
enum class SceneOpeningAxis { horizontal, vertical };
inline constexpr std::size_t scene_opening_max_lines=24;

// Raster-normal coordinate in the normalized 1280x720 capture. A line has
// no chart identity, known endpoint, or known tangent-axis origin in pixels.
struct SceneOpeningLine {SceneOpeningAxis axis;double coordinate;};
struct SceneOpeningFrame {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    SongNs received_host_ns=-1;
    std::string evidence_id,frame_sha256;
    std::span<const SceneOpeningLine> lines;
};
struct SceneOpeningSyncResult {
    static constexpr bool input_permission=false,pixels_retained=false,timing_qualified=false;
    const char* reason="scene_opening_waiting_for_lines";
    std::optional<VisualTimeInterval> song_time,origin_host_time;
    std::optional<std::size_t> native_line_index;
    std::vector<std::string> supporting_evidence_ids;
    std::size_t candidate_tracks=0;
};

// Conditional rate-1 arrival evidence. The caller supplies the entire
// causally possible song window on each fresh receipt, including late aliases.
// This class has no acquisition, lead, arming, or input path.
class SceneOpeningSync {
    static constexpr SongNs ms=1'000'000,second=1'000'000'000,safe=(SongNs{1}<<53)-1;
    static constexpr SongNs minimum_song_ns=-2*second;
    static constexpr std::size_t max_source_events=524288,max_model_pieces=131072,max_matches=4096,max_work=262144,max_frames=12,max_evidence=2048;
    static constexpr double axis_error_degrees=1.0,pixel_error=3.0;
    struct Piece {
        VisualTimeInterval time;
        PhigrosLinePose first,last;
    };
    struct Match {
        std::size_t line,observation;
        VisualTimeInterval origin;
        bool exact_axis,model_motion,stationary;
    };
    struct Receipt {
        SongNs received;
        std::string evidence,hash;
        std::vector<SceneOpeningLine> observed;
        std::vector<Match> matches;
        std::vector<std::vector<VisualTimeInterval>> static_origins;
    };
    enum class Budget { work, angle_aliases, matches, tracks };
    struct Track {
        VisualTimeInterval origin;
        std::size_t old,middle,line;
        std::array<std::size_t,3> observations;
        bool positive;
    };
    SongNs maximum_song_ns_,last_received_=-1,last_now_=-1;
    std::uint64_t last_sequence_=0;
    std::tuple<std::uint64_t,std::uint64_t,std::uint64_t> context_{};
    std::vector<std::vector<Piece>> model_;
    std::deque<Receipt> history_;
    std::set<std::string> evidence_;

    static bool valid(VisualTimeInterval t){return t.lower<=t.upper;}
    static VisualTimeInterval intersect(VisualTimeInterval a,VisualTimeInterval b){
        return {std::max(a.lower,b.lower),std::min(a.upper,b.upper)};
    }
    static bool hash_text(const std::string& hash){
        return hash.size()==64&&std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
    }
    static bool evidence_text(const std::string& id){
        return !id.empty()&&id.size()<=128&&std::all_of(id.begin(),id.end(),[](unsigned char c){return c>=33&&c<=126;});
    }
    static long double at(const Piece& piece,double first,double last,SongNs time){
        if(piece.time.lower==piece.time.upper)return first;
        const auto fraction=static_cast<long double>(time-piece.time.lower)/(piece.time.upper-piece.time.lower);
        return std::lerp(static_cast<long double>(first),static_cast<long double>(last),fraction);
    }
    static VisualTimeInterval linear_clip(const Piece& piece,VisualTimeInterval in,double first,double last,
                                          long double low,long double high){
        if(!valid(in))return in;
        // Outward padding preserves numeric-boundary and integral-time aliases.
        constexpr long double numeric_pad=.000001L;
        low-=numeric_pad;high+=numeric_pad;
        if(first==last||piece.time.lower==piece.time.upper)return low<=first&&first<=high?in:VisualTimeInterval{1,0};
        const auto scale=static_cast<long double>(piece.time.upper-piece.time.lower)/(static_cast<long double>(last)-first);
        const auto a=(low-first)*scale,b=(high-first)*scale;
        const auto lower=std::max(static_cast<long double>(in.lower),piece.time.lower+std::min(a,b)-1);
        const auto upper=std::min(static_cast<long double>(in.upper),piece.time.lower+std::max(a,b)+1);
        if(lower>upper)return {1,0};
        return {static_cast<SongNs>(std::ceil(lower)),static_cast<SongNs>(std::floor(upper))};
    }
    static bool coordinates_valid(const SceneOpeningLine& line){
        if(line.axis!=SceneOpeningAxis::horizontal&&line.axis!=SceneOpeningAxis::vertical)return false;
        return std::isfinite(line.coordinate)&&line.coordinate>=0&&
            line.coordinate<(line.axis==SceneOpeningAxis::horizontal?720:1280);
    }
    static void bump(std::size_t& work){if(++work>max_work)throw Budget::work;}
    static std::vector<VisualTimeInterval> merged(std::vector<VisualTimeInterval> intervals){
        std::sort(intervals.begin(),intervals.end(),[](const auto& a,const auto& b){return a.lower<b.lower;});
        std::vector<VisualTimeInterval> out;
        for(const auto& interval:intervals){
            if(out.empty()||interval.lower>out.back().upper+1)out.push_back(interval);
            else out.back().upper=std::max(out.back().upper,interval.upper);
        }
        return out;
    }
    std::vector<Match> match(const SceneOpeningFrame& frame,VisualTimeInterval window,std::size_t& work) const {
        std::vector<Match> matches;
        for(std::size_t line=0;line<model_.size();++line){
            const auto& pieces=model_[line];
            auto p=std::lower_bound(pieces.begin(),pieces.end(),window.lower,
                [](const Piece& piece,SongNs time){return piece.time.upper<time;});
            for(;p!=pieces.end()&&p->time.lower<=window.upper;++p){
                const auto domain=linear_clip(*p,intersect(p->time,window),p->first.opacity,p->last.opacity,0,1e9);
                if(!valid(domain))continue;
                const auto a0=at(*p,p->first.rotation_degrees,p->last.rotation_degrees,domain.lower);
                const auto a1=at(*p,p->first.rotation_degrees,p->last.rotation_degrees,domain.upper);
                for(std::size_t observation=0;observation<frame.lines.size();++observation){
                    bump(work);
                    const auto& observed=frame.lines[observation];
                    const bool horizontal=observed.axis==SceneOpeningAxis::horizontal;
                    const double base=horizontal?0:90;
                    const auto first_turn=static_cast<int>(std::ceil((std::min(a0,a1)-base-axis_error_degrees-.000001L)/180));
                    const auto last_turn=static_cast<int>(std::floor((std::max(a0,a1)-base+axis_error_degrees+.000001L)/180));
                    if(last_turn-first_turn>32)throw Budget::angle_aliases;
                    const auto dimension=horizontal?720.:1280.;
                    const auto normal_first=dimension*(horizontal?p->first.origin.y:p->first.origin.x);
                    const auto normal_last=dimension*(horizontal?p->last.origin.y:p->last.origin.x);
                    const auto along_dimension=horizontal?1280.:720.;
                    const auto along_first=along_dimension*(horizontal?p->first.origin.x:p->first.origin.y);
                    const auto along_last=along_dimension*(horizontal?p->last.origin.x:p->last.origin.y);
                    const bool exact_axis=p->first.rotation_degrees==p->last.rotation_degrees&&
                        std::abs(std::remainder(p->first.rotation_degrees,180.))==base;
                    // A near-axis observation can come from any along-axis
                    // section of the viewport. Retain it as an alias, not proof.
                    constexpr double upper_tangent=0.017456;
                    const auto allowance=exact_axis?pixel_error:pixel_error+upper_tangent*std::max(
                        {std::abs(along_first),std::abs(along_last),std::abs(along_first-along_dimension),std::abs(along_last-along_dimension)});
                    for(auto turn=first_turn;turn<=last_turn;++turn){
                        bump(work);
                        const auto angle=base+180.*turn;
                        auto time=linear_clip(*p,domain,p->first.rotation_degrees,p->last.rotation_degrees,
                            angle-axis_error_degrees,angle+axis_error_degrees);
                        time=linear_clip(*p,time,normal_first,normal_last,observed.coordinate-allowance,observed.coordinate+allowance);
                        if(!valid(time))continue;
                        const bool visible=std::min(at(*p,p->first.opacity,p->last.opacity,time.lower),
                            at(*p,p->first.opacity,p->last.opacity,time.upper))>0;
                        if(matches.size()==max_matches)throw Budget::matches;
                        const bool motion=normal_first!=normal_last;
                        matches.push_back({line,observation,{frame.received_host_ns-time.upper,frame.received_host_ns-time.lower},
                            exact_axis&&visible,motion,!motion&&p->first.rotation_degrees==p->last.rotation_degrees});
                    }
                }
            }
        }
        return matches;
    }
    static auto first_for(const Receipt& receipt,std::size_t line){
        return std::lower_bound(receipt.matches.begin(),receipt.matches.end(),line,
            [](const Match& value,std::size_t id){return value.line<id;});
    }
    static bool distinct(const Receipt& older,const Receipt& later){
        return later.received-older.received>=20*ms&&older.hash!=later.hash;
    }
    static bool moved(const Receipt& earlier,const Receipt& later,const Match& a,const Match& b){
        const auto& old=earlier.observed[a.observation];const auto& current=later.observed[b.observation];
        if(std::abs(old.coordinate-current.coordinate)<=2*pixel_error+.01)return false;
        const auto contains_close_line=[](const auto& observations,const SceneOpeningLine& line){
            return std::any_of(observations.begin(),observations.end(),[&](const auto& other){return other.axis==line.axis&&
                std::abs(other.coordinate-line.coordinate)<=2*pixel_error;});
        };
        // Changing the assignment within an unchanged collection of static
        // lines cannot manufacture measured motion.
        return !contains_close_line(later.observed,old)&&!contains_close_line(earlier.observed,current);
    }
    static bool has_static_alias(const Receipt& first,const Receipt& middle,const Receipt& current,
                                 const Match& a,const Match& b,const Match& c,std::size_t& work){
        const auto& x=first.static_origins[a.observation];
        const auto& y=middle.static_origins[b.observation];
        const auto& z=current.static_origins[c.observation];
        // Independent static chart lines at different positions may explain a
        // changing observed set when the render/detector misses other lines.
        std::size_t i=0,j=0,k=0;
        while(i<x.size()&&j<y.size()&&k<z.size()){
            bump(work);
            if(valid(intersect(intersect(x[i],y[j]),z[k])))return true;
            const auto earliest_end=std::min({x[i].upper,y[j].upper,z[k].upper});
            if(x[i].upper==earliest_end)++i;
            if(y[j].upper==earliest_end)++j;
            if(z[k].upper==earliest_end)++k;
        }
        return false;
    }
    SceneOpeningSyncResult solve(const Receipt& current,std::size_t& work) const {
        SceneOpeningSyncResult result;
        result.reason="scene_opening_waiting_for_three_motion_frames";
        std::vector<Track> tracks;
        for(std::size_t mid=1;mid<history_.size();++mid){
            const auto& middle=history_[mid];if(!distinct(middle,current))continue;
            for(std::size_t old=0;old<mid;++old){
                const auto& earlier=history_[old];if(!distinct(earlier,middle)||!distinct(earlier,current))continue;
                for(const auto& latest:current.matches){
                    bump(work);
                    for(auto m=first_for(middle,latest.line);m!=middle.matches.end()&&m->line==latest.line;++m){
                        bump(work);if(middle.observed[m->observation].axis!=current.observed[latest.observation].axis)continue;
                        const auto middle_origin=intersect(latest.origin,m->origin);if(!valid(middle_origin))continue;
                        for(auto a=first_for(earlier,latest.line);a!=earlier.matches.end()&&a->line==latest.line;++a){
                            bump(work);if(earlier.observed[a->observation].axis!=current.observed[latest.observation].axis)continue;
                            const auto origin=intersect(middle_origin,a->origin);if(!valid(origin))continue;
                            if(!moved(earlier,middle,*a,*m)||!moved(middle,current,*m,latest))continue;
                            if(tracks.size()==max_matches)throw Budget::tracks;
                            const bool positive=a->exact_axis&&m->exact_axis&&latest.exact_axis&&
                                (a->model_motion||m->model_motion||latest.model_motion);
                            if(positive&&has_static_alias(earlier,middle,current,*a,*m,latest,work)){
                                result.reason="scene_opening_static_line_alias";return result;
                            }
                            tracks.push_back({origin,old,mid,latest.line,{a->observation,m->observation,latest.observation},positive});
                        }
                    }
                }
            }
        }
        result.candidate_tracks=tracks.size();if(tracks.empty())return result;
        std::sort(tracks.begin(),tracks.end(),[](const Track& a,const Track& b){return a.origin.lower<b.origin.lower;});
        auto origin=tracks.front().origin;
        for(const auto& track:tracks){
            if(track.origin.lower>origin.upper+1){result.reason="scene_opening_time_alias";return result;}
            origin.upper=std::max(origin.upper,track.origin.upper);
        }
        if(origin.upper-origin.lower>40*ms){result.reason="scene_opening_interval_too_wide";return result;}
        // Adjacent native event pieces may split one actual observed track.
        // Join them only within the same source, receipt and observation tuple.
        using Proof=std::tuple<std::size_t,std::size_t,std::size_t,std::array<std::size_t,3>>;
        std::map<Proof,std::vector<VisualTimeInterval>> proofs;
        for(const auto& track:tracks)if(track.positive)
            proofs[{track.old,track.middle,track.line,track.observations}].push_back(track.origin);
        if(origin.lower>=0)for(auto& [proof,intervals]:proofs){
            const auto coverage=merged(std::move(intervals));
            if(std::none_of(coverage.begin(),coverage.end(),[&](const auto& interval){return interval.lower<=origin.lower&&origin.upper<=interval.upper;}))continue;
            result.reason="scene_opening_unique_axis_motion";
            result.origin_host_time=origin;
            result.song_time=VisualTimeInterval{current.received-origin.upper,current.received-origin.lower};
            result.native_line_index=std::get<2>(proof);
            result.supporting_evidence_ids={history_[std::get<0>(proof)].evidence,history_[std::get<1>(proof)].evidence,current.evidence};
            return result;
        }
        result.reason="scene_opening_no_complete_axis_motion_proof";
        return result;
    }
public:
    explicit SceneOpeningSync(const SemanticChart& chart,SongNs maximum_song_ns=600*second)
        :maximum_song_ns_(maximum_song_ns){
        if(chart.lines.empty()||chart.lines.size()>256||maximum_song_ns<=0||maximum_song_ns>600*second)
            throw std::invalid_argument("scene_opening_chart_scope");
        std::size_t source_events=0,pieces=0;
        for(const auto& line:chart.lines){
            source_events+=line.move_events.size()+line.rotate_events.size()+line.opacity_events.size();
            if(source_events>max_source_events)throw std::invalid_argument("scene_opening_native_event_budget");
        }
        const PhigrosPoseEvaluator poses(chart);model_.resize(chart.lines.size());
        for(std::size_t line=0;line<chart.lines.size();++line){
            const auto& native=chart.lines[line];
            std::set<SongNs> cuts{minimum_song_ns,maximum_song_ns+1};
            const auto add=[&](const auto& events){for(const auto& event:events)for(const auto time:{event.time.start_ns,event.time.end_ns})
                if(time>minimum_song_ns&&time<=maximum_song_ns)cuts.insert(time);};
            add(native.move_events);add(native.rotate_events);add(native.opacity_events);
            auto start=cuts.begin();
            for(auto end=std::next(start);end!=cuts.end();++start,++end){
                const VisualTimeInterval time{*start,*end-1};
                const auto first=poses.line_at(line,time.lower),last=poses.line_at(line,time.upper);
                if(std::max(first.opacity,last.opacity)<=0)continue;
                for(const auto& pose:{first,last})if(std::abs(pose.origin.x)>1e6||std::abs(pose.origin.y)>1e6||
                    std::abs(pose.rotation_degrees)>1e9||std::abs(pose.opacity)>1e9)
                    throw std::invalid_argument("scene_opening_native_pose_bound");
                if(++pieces>max_model_pieces)throw std::invalid_argument("scene_opening_piece_budget");
                model_[line].push_back({time,first,last});
            }
        }
    }
    void reset(){history_.clear();evidence_.clear();context_={};last_sequence_=0;last_received_=last_now_=-1;}
    SceneOpeningSyncResult observe(const SceneOpeningFrame& frame,SongNs now,VisualTimeInterval possible_song_time){
        SceneOpeningSyncResult result;
        const auto reject=[&](const char* reason){history_.clear();result.reason=reason;return result;};
        if(!valid(possible_song_time)||possible_song_time.lower<minimum_song_ns||possible_song_time.upper>maximum_song_ns_)
            return reject("scene_opening_song_window_outside_model");
        if(!frame.sequence||!frame.epoch||!frame.generation||!frame.geometry_version||now<0||now>safe||now<last_now_||
            frame.received_host_ns<0||frame.received_host_ns>now||now-frame.received_host_ns>=100*ms||
            !evidence_text(frame.evidence_id)||!hash_text(frame.frame_sha256)||frame.lines.size()>scene_opening_max_lines||
            !std::all_of(frame.lines.begin(),frame.lines.end(),coordinates_valid))
            return reject("scene_opening_invalid_or_stale_frame");
        last_now_=now;
        const auto context=std::tuple{frame.epoch,frame.generation,frame.geometry_version};
        if(context_!=context){
            history_.clear();evidence_.clear();last_sequence_=0;last_received_=-1;context_=context;
        }
        if(frame.sequence<=last_sequence_||frame.received_host_ns<=last_received_||evidence_.contains(frame.evidence_id))
            return reject("scene_opening_replayed_frame");
        last_sequence_=frame.sequence;last_received_=frame.received_host_ns;
        if(evidence_.size()>=max_evidence)return reject("scene_opening_evidence_budget");
        evidence_.insert(frame.evidence_id);
        while(!history_.empty()&&frame.received_host_ns-history_.front().received>200*ms)history_.pop_front();
        if(frame.lines.empty())return result;
        if(!history_.empty()&&frame.received_host_ns-history_.back().received<20*ms)return result;
        if(history_.size()>=max_frames)return reject("scene_opening_history_budget");
        std::size_t work=0;
        try {
            Receipt current{frame.received_host_ns,frame.evidence_id,frame.frame_sha256,
                {frame.lines.begin(),frame.lines.end()},match(frame,possible_song_time,work),{}};
            if(current.matches.empty()){result.reason="scene_opening_no_chart_line_match";return result;}
            current.static_origins.resize(current.observed.size());
            for(const auto& match:current.matches)if(match.stationary)current.static_origins[match.observation].push_back(match.origin);
            for(auto& intervals:current.static_origins)intervals=merged(std::move(intervals));
            result=solve(current,work);history_.push_back(std::move(current));return result;
        } catch(Budget reason){
            if(reason==Budget::angle_aliases)return reject("scene_opening_angle_alias_budget");
            if(reason==Budget::matches)return reject("scene_opening_match_budget");
            if(reason==Budget::tracks)return reject("scene_opening_track_budget");
            return reject("scene_opening_work_budget");
        }
    }
};
} // namespace rhythm::experimental
