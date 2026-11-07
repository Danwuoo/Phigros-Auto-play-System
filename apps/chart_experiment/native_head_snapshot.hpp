#pragma once
#include "rhythm/phigros_pose.hpp"
#include "rhythm/visual_candidates.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rhythm::experimental {
struct NativeHeadSnapshotWindow {SongNs begin_ns=0,end_ns=0,step_ns=5'000'000;};
struct NativeHeadSnapshotLimits {
    std::size_t maximum_sources=10000,maximum_samples=2601;
    std::size_t maximum_precompute_work=6'000'000,maximum_storage_bytes=8*1024*1024;
};
struct NativeHeadSnapshotSource {
    std::string source_note_id;
    std::size_t line_index=0,note_index=0;
    SongNs judgment_ns=0;
    NoteKind kind=NoteKind::tap;
    NoteSide side=NoteSide::above;
};
enum class NativeHeadVisibility {full_visible,partial,clipped};
struct NativeHeadSnapshotPrediction {
    std::uint16_t source_index=0;
    double x=0,y=0;
    NativeHeadVisibility visibility=NativeHeadVisibility::partial;
};
struct NativeHeadSnapshotSample {
    SongNs song_ns=0;
    std::size_t future_sources=0,outside_center_roi=0,possible_heads=0,full_visible_heads=0;
    std::size_t partial_heads=0,clipped_heads=0,head_count=0;
    bool complete=true,unobservable=true,coincident_sources=false;
    std::array<NativeHeadSnapshotPrediction,8> heads{};
};
struct NativeHeadSnapshotAssignment {
    std::uint16_t grid_index=0;
    std::uint8_t observed_count=0;
    bool partial_match=false;
    // Indexed by the original observation order. Every original source index
    // remains separate, including coincident native sources.
    std::array<std::uint16_t,8> source_indices{};
};
struct NativeHeadSnapshotMatchLimits {
    std::size_t maximum_work=262144,maximum_assignments=4096;
};
struct NativeHeadSnapshotMatch {
    static constexpr bool diagnostic_only=true,input_permission=false,pixels_retained=false;
    bool valid=false,source_ambiguity=false,budget_exhausted=false,local_domain_clipped=false;
    const char* reason="not_measured";
    SongNs local_begin_ns=0,local_end_ns=0;
    std::size_t work=0,examined_samples=0,incomplete_samples=0,unobservable_samples=0;
    std::size_t matched_grid_count=0;
    std::array<std::uint16_t,101> matched_grid_indices{};
    std::vector<NativeHeadSnapshotAssignment> assignments;
};

// Pure, before-READY precomputation. No frame acquisition, clock fitting,
// SyncLease, input permission or continuous-time interval claim. The supplied
// pose evaluator must belong to this original chart; it is borrowed only for
// construction and is not retained. Source chart/notes/times are never changed.
class NativeHeadSnapshotModel {
    struct Range {long double lower=0,upper=0;};
    static constexpr SongNs second=1'000'000'000,local_radius=250'000'000;
    NativeHeadSnapshotWindow window_;
    NativeHeadSnapshotLimits limits_;
    std::string chart_sha256_;
    std::vector<NativeHeadSnapshotSource> sources_;
    std::vector<NativeHeadSnapshotSample> samples_;
    std::size_t precompute_work_=0,storage_estimate_=0;
    static void require(bool condition,const char* reason){if(!condition)throw std::invalid_argument(reason);}
    static long double down(long double x){return std::nextafter(x,-std::numeric_limits<long double>::infinity());}
    static long double up(long double x){return std::nextafter(x,std::numeric_limits<long double>::infinity());}
    static Range point(long double x){return {x,x};}
    static Range add(Range a,Range b){return {down(a.lower+b.lower),up(a.upper+b.upper)};}
    static Range increment(double value,SongNs dt) {
        const auto lo=down(static_cast<long double>(dt)/second),hi=up(static_cast<long double>(dt)/second);
        const auto a=static_cast<long double>(value)*lo,b=static_cast<long double>(value)*hi;
        return {down(std::min(a,b)),up(std::max(a,b))};
    }
    void work(){if(++precompute_work_>limits_.maximum_precompute_work)throw std::invalid_argument("native_head_snapshot_precompute_work_budget");}
    // Same native-zero/provided-floor/v1 sentinel interpretation as the
    // existing adaptive opening floor_at. Ranges preserve arithmetic padding.
    Range floor_at(const ChartLine& line,SongNs at,SongNs native_zero,bool source_v1) {
        Range floor=point(0);SongNs cursor=native_zero;
        const bool zero_integral=source_v1&&std::none_of(line.speed_events.begin(),line.speed_events.end(),
            [](const auto& e){return e.floor_position.has_value();});
        std::optional<SongNs> previous_end;
        for(const auto& e:line.speed_events) {
            work();require(std::isfinite(e.value)&&std::abs(e.value)<=1e6&&e.time.end_ns>=e.time.start_ns&&
                (zero_integral?(previous_end?e.time.start_ns==*previous_end:e.time.start_ns<=native_zero):e.time.start_ns==cursor),
                "native_head_snapshot_speed_coverage_or_values");
            previous_end=e.time.end_ns;
            if(zero_integral&&e.time.end_ns<=native_zero)continue;
            require((zero_integral?std::max(e.time.start_ns,native_zero):e.time.start_ns)==cursor&&e.time.end_ns>=cursor,
                "native_head_snapshot_speed_coverage_or_values");
            if(e.floor_position){require(std::isfinite(*e.floor_position)&&std::abs(*e.floor_position)<=1e9,
                "native_head_snapshot_floor_value");floor=point(*e.floor_position);}
            if(at<cursor)return floor;
            if(e.time.end_ns==cursor)continue;
            if(at<e.time.end_ns)return add(floor,increment(e.value,at-cursor));
            floor=add(floor,increment(e.value,e.time.end_ns-cursor));cursor=e.time.end_ns;
        }
        throw std::invalid_argument("native_head_snapshot_speed_coverage_missing");
    }
    static bool hash(std::string_view s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return
        (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
public:
    static constexpr bool diagnostic_only=true,input_permission=false,pixels_retained=false,source_pixel_age_qualified=false;
    NativeHeadSnapshotModel(const SemanticChart& chart,const PhigrosPoseEvaluator& poses,
            NativeHeadSnapshotWindow window,NativeHeadSnapshotLimits limits={})
        :window_(window),limits_(limits),chart_sha256_(chart.source.chart_sha256) {
        require(chart.format_version==3&&(chart.source_format_version==1||chart.source_format_version==3)&&hash(chart_sha256_)&&
            std::isfinite(chart.offset_seconds)&&std::abs(chart.offset_seconds)<=3600&&!chart.lines.empty()&&chart.lines.size()<=64&&
            poses.line_count()==chart.lines.size()&&chart.note_count>0&&chart.note_count<=10000,"native_head_snapshot_chart_scope");
        require(window.begin_ns>=0&&window.end_ns>window.begin_ns&&window.end_ns<=600*second&&window.step_ns==5'000'000&&
            window.end_ns-window.begin_ns<=13*second&&(window.end_ns-window.begin_ns)%window.step_ns==0,
            "native_head_snapshot_window_scope");
        require(limits.maximum_sources>0&&limits.maximum_sources<=10000&&limits.maximum_samples>0&&limits.maximum_samples<=2601&&
            limits.maximum_precompute_work>0&&limits.maximum_precompute_work<=6'000'000&&limits.maximum_storage_bytes>0&&
            limits.maximum_storage_bytes<=8*1024*1024,"native_head_snapshot_limits");
        const auto count=static_cast<std::size_t>((window.end_ns-window.begin_ns)/window.step_ns)+1;
        require(count<=limits.maximum_samples,"native_head_snapshot_sample_budget");
        std::size_t actual_notes=0,cyan_sources=0;std::set<std::string_view> ids;
        for(std::size_t li=0;li<chart.lines.size();++li) {
            const auto& line=chart.lines[li];require(line.source_index==li&&std::isfinite(line.bpm)&&line.bpm>0&&
                !line.speed_events.empty()&&line.speed_events.size()<=4096,"native_head_snapshot_line_scope");
            for(const auto& note:line.notes) {
                require(++actual_notes<=10000,"native_head_snapshot_actual_note_budget");
                const auto pointer="/judgeLineList/"+std::to_string(li)+(note.side==NoteSide::above?"/notesAbove/":"/notesBelow/")+std::to_string(note.source_index);
                require((note.side==NoteSide::above||note.side==NoteSide::below)&&note.source.json_pointer==pointer&&
                    note.source.id==chart_sha256_+'#'+pointer&&note.source.id.size()<=512&&ids.insert(note.source.id).second&&
                    (note.kind==NoteKind::tap||note.kind==NoteKind::drag||note.kind==NoteKind::hold||note.kind==NoteKind::flick),
                    "native_head_snapshot_source_identity");
                if(note.kind==NoteKind::tap||note.kind==NoteKind::hold)++cyan_sources;
            }
        }
        require(actual_notes==chart.note_count&&cyan_sources<=limits.maximum_sources,"native_head_snapshot_source_count");
        require(cyan_sources==0||count<=limits.maximum_precompute_work/cyan_sources,"native_head_snapshot_prediction_budget");
        require(cyan_sources*sizeof(NativeHeadSnapshotSource)+count*sizeof(NativeHeadSnapshotSample)+chart_sha256_.capacity()+1<=
            limits.maximum_storage_bytes,"native_head_snapshot_storage_budget");
        sources_.reserve(cyan_sources);samples_.reserve(count);
        storage_estimate_=sources_.capacity()*sizeof(NativeHeadSnapshotSource)+samples_.capacity()*sizeof(NativeHeadSnapshotSample)+chart_sha256_.capacity()+1;
        require(storage_estimate_<=limits.maximum_storage_bytes,"native_head_snapshot_storage_budget");
        for(std::size_t li=0;li<chart.lines.size();++li)for(std::size_t ni=0;ni<chart.lines[li].notes.size();++ni) {
            const auto& n=chart.lines[li].notes[ni];if(n.kind!=NoteKind::tap&&n.kind!=NoteKind::hold)continue;
            require(n.start_ns>=0&&n.end_ns>=n.start_ns&&std::isfinite(n.position_x)&&std::abs(n.position_x)<=100&&
                std::isfinite(n.floor_position)&&std::abs(n.floor_position)<=1e9&&std::isfinite(n.speed)&&std::abs(n.speed)<=10000,
                "native_head_snapshot_note_values");
            require(n.source.id.size()+1+64<=limits.maximum_storage_bytes-storage_estimate_,"native_head_snapshot_storage_budget");
            sources_.push_back({n.source.id,li,ni,n.start_ns,n.kind,n.side});
            storage_estimate_+=sources_.back().source_note_id.capacity()+1+64; // Include conservative per-string allocation overhead.
            require(storage_estimate_<=limits.maximum_storage_bytes,"native_head_snapshot_storage_budget");
        }
        require(storage_estimate_<=limits.maximum_storage_bytes,"native_head_snapshot_storage_budget");
        const auto native_zero=phigros_time_ns(0,chart.lines.front().bpm,chart.offset_seconds);
        for(std::size_t gi=0;gi<count;++gi) {
            NativeHeadSnapshotSample sample;sample.song_ns=window.begin_ns+static_cast<SongNs>(gi)*window.step_ns;
            std::size_t last_line=chart.lines.size();Range floor;PhigrosLinePose pose;
            for(std::size_t si=0;si<sources_.size();++si) {
                work();const auto& source=sources_[si];if(source.judgment_ns<=sample.song_ns)continue;++sample.future_sources;
                const auto& line=chart.lines[source.line_index];const auto& n=line.notes[source.note_index];
                if(last_line!=source.line_index){last_line=source.line_index;floor=floor_at(line,sample.song_ns,native_zero,chart.source_format_version==1);
                    pose=poses.line_at(source.line_index,sample.song_ns);}
                const long double fp=floor.lower+(floor.upper-floor.lower)/2;
                const auto speed=n.kind==NoteKind::hold?1.:n.speed;
                const auto z=static_cast<double>((fp-n.floor_position)*(720.L/(2*.83175L))*speed*(n.side==NoteSide::above?1:-1));
                const auto base=phigros_line_point(pose,n.position_x,n.side).reference_pixels;
                const double x=base.x-z*pose.above_normal.x,y=base.y-z*pose.above_normal.y;
                require(std::isfinite(x)&&std::isfinite(y),"native_head_snapshot_nonfinite_prediction");
                // Independent forward-center geometry, not proof that pixels
                // are absent. The same +/-3px comparison domain is used below.
                if(x< -3||x>1283||y<27||y>573){++sample.outside_center_roi;continue;}
                ++sample.possible_heads;
                const auto visibility=x>=95&&x<=1184&&y>=125&&y<=474?NativeHeadVisibility::full_visible:
                    x<0||x>1279||y<30||y>569?NativeHeadVisibility::clipped:NativeHeadVisibility::partial;
                if(visibility==NativeHeadVisibility::full_visible)++sample.full_visible_heads;
                else if(visibility==NativeHeadVisibility::clipped)++sample.clipped_heads;else ++sample.partial_heads;
                if(sample.head_count<sample.heads.size())sample.heads[sample.head_count++]={static_cast<std::uint16_t>(si),x,y,visibility};
                else sample.complete=false;
            }
            sample.unobservable=sample.possible_heads==0;
            if(!sample.complete)sample.head_count=0; // No silently truncated candidate may be matched.
            for(std::size_t a=0;a<sample.head_count;++a)for(std::size_t b=a+1;b<sample.head_count;++b)
                if(std::hypot(sample.heads[a].x-sample.heads[b].x,sample.heads[a].y-sample.heads[b].y)<=1e-9)sample.coincident_sources=true;
            samples_.push_back(sample);
        }
    }
    const auto& sources()const{return sources_;}
    const auto& samples()const{return samples_;}
    const auto& window()const{return window_;}
    const auto& chart_sha256()const{return chart_sha256_;}
    std::size_t precompute_work()const{return precompute_work_;}
    std::size_t storage_bytes_estimate()const{return storage_estimate_;}
    NativeHeadSnapshotMatch match(std::span<const VisualCenter> observed,SongNs predicted_song_ns,
            NativeHeadSnapshotMatchLimits limits={})const {
        NativeHeadSnapshotMatch result;
        if(observed.empty()||observed.size()>8){result.reason="empty_or_unsupported_observed_centers";return result;}
        if(limits.maximum_work==0||limits.maximum_work>262144||limits.maximum_assignments==0||limits.maximum_assignments>4096) {
            result.reason="unsupported_match_limits";return result;
        }
        for(std::size_t i=0;i<observed.size();++i) {
            const auto& o=observed[i];if(o.id.empty()||o.id.size()>64||!std::isfinite(o.x)||!std::isfinite(o.y)||
                o.x<0||o.x>1279||o.y<30||o.y>569){result.reason="invalid_observed_center";return result;}
            for(std::size_t j=0;j<i;++j)if(observed[j].id==o.id){result.reason="duplicate_observation_id";return result;}
        }
        if(predicted_song_ns<window_.begin_ns||predicted_song_ns>window_.end_ns){result.reason="prediction_outside_explicit_domain";return result;}
        result.local_begin_ns=std::max(window_.begin_ns,predicted_song_ns-local_radius);
        result.local_end_ns=std::min(window_.end_ns,predicted_song_ns+local_radius);
        result.local_domain_clipped=result.local_begin_ns!=predicted_song_ns-local_radius||result.local_end_ns!=predicted_song_ns+local_radius;
        result.assignments.reserve(limits.maximum_assignments);
        const auto bump=[&](){return ++result.work<=limits.maximum_work;};
        for(std::size_t gi=0;gi<samples_.size();++gi) {
            const auto& s=samples_[gi];if(s.song_ns<result.local_begin_ns||s.song_ns>result.local_end_ns)continue;++result.examined_samples;
            if(!s.complete){++result.incomplete_samples;continue;}if(s.unobservable){++result.unobservable_samples;continue;}
            if(s.full_visible_heads>observed.size())continue;
            std::array<std::uint16_t,8> edges{};std::uint16_t required=0;
            for(std::size_t h=0;h<s.head_count;++h)if(s.heads[h].visibility==NativeHeadVisibility::full_visible)required|=static_cast<std::uint16_t>(1u<<h);
            bool missing=false;
            for(std::size_t oi=0;oi<observed.size();++oi) {
                for(std::size_t hi=0;hi<s.head_count;++hi) {
                    if(!bump()){result.budget_exhausted=true;break;}
                    if(std::hypot(observed[oi].x-s.heads[hi].x,observed[oi].y-s.heads[hi].y)<=3)
                        edges[oi]|=static_cast<std::uint16_t>(1u<<hi);
                }
                if(!edges[oi])missing=true;if(result.budget_exhausted)break;
            }
            if(result.budget_exhausted)break;if(missing)continue;
            NativeHeadSnapshotAssignment assignment;assignment.grid_index=static_cast<std::uint16_t>(gi);
            assignment.observed_count=static_cast<std::uint8_t>(observed.size());const auto previous=result.assignments.size();
            const auto visit=[&](auto&& self,std::size_t oi,std::uint16_t used,bool partial)->void {
                if(result.budget_exhausted)return;if(!bump()){result.budget_exhausted=true;return;}
                if(oi==observed.size()) {
                    if((used&required)!=required)return;
                    if(result.assignments.size()>=limits.maximum_assignments){result.budget_exhausted=true;return;}
                    assignment.partial_match=partial;result.assignments.push_back(assignment);return;
                }
                for(std::size_t hi=0;hi<s.head_count;++hi)if((edges[oi]&(1u<<hi))&&!(used&(1u<<hi))) {
                    assignment.source_indices[oi]=s.heads[hi].source_index;
                    self(self,oi+1,static_cast<std::uint16_t>(used|(1u<<hi)),partial||s.heads[hi].visibility!=NativeHeadVisibility::full_visible);
                }
            };
            visit(visit,0,0,false);if(result.budget_exhausted)break;
            if(result.assignments.size()>previous) {
                if(result.matched_grid_count>=result.matched_grid_indices.size()){result.budget_exhausted=true;break;}
                result.matched_grid_indices[result.matched_grid_count++]=static_cast<std::uint16_t>(gi);
                if(result.assignments.size()-previous>1||s.coincident_sources)result.source_ambiguity=true;
            }
        }
        if(result.budget_exhausted){result.assignments.clear();result.matched_grid_count=0;result.reason="match_budget_exhausted_unknown";return result;}
        result.valid=!result.assignments.empty();result.reason=result.valid?"conditional_sampled_grid_matches_only":"no_complete_local_grid_assignment_unknown";
        return result;
    }
};
} // namespace rhythm::experimental
