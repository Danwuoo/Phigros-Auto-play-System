#include "../apps/chart_experiment/scene_opening_sync.hpp"
#include <iostream>

using namespace rhythm;
using namespace rhythm::experimental;

namespace {
constexpr SongNs ms=1'000'000,second=1'000*ms,origin=10*second;
constexpr auto horizontal=SceneOpeningAxis::horizontal,vertical=SceneOpeningAxis::vertical;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}

void add_line(SemanticChart& chart,SceneOpeningAxis axis,double a,double b,SongNs starts=0,double tilt=0){
    ChartLine line;line.source_index=chart.lines.size();line.bpm=120;
    MoveLineEvent move;move.time.start_ns=starts;move.time.end_ns=starts+2*second;
    move.start_x=axis==vertical?a/1280:.5;move.end_x=axis==vertical?b/1280:.5;
    move.start_y=axis==horizontal?1-a/720:.5;move.end_y=axis==horizontal?1-b/720:.5;
    line.move_events.push_back(move);
    NumericLineEvent angle;angle.time.start_ns=-2*second;angle.time.end_ns=10*second;
    angle.start=angle.end=(axis==vertical?90:0)+tilt;line.rotate_events.push_back(angle);
    if(starts){NumericLineEvent hidden;hidden.time.start_ns=-2*second;hidden.time.end_ns=starts;line.opacity_events.push_back(hidden);}
    NumericLineEvent shown;shown.time.start_ns=starts;shown.time.end_ns=starts+2*second;
    shown.start=shown.end=1;line.opacity_events.push_back(shown);
    NumericLineEvent after;after.time.start_ns=shown.time.end_ns;after.time.end_ns=10*second;
    line.opacity_events.push_back(after);
    chart.lines.push_back(std::move(line)); // No playable note required on this line.
}
SemanticChart chart_with_line(SceneOpeningAxis axis=horizontal,double a=50,double b=550,double tilt=0){
    SemanticChart chart;chart.format_version=3;add_line(chart,axis,a,b,0,tilt);return chart;
}
SceneOpeningSyncResult feed(SceneOpeningSync& model,unsigned seq,SongNs time,std::vector<SceneOpeningLine> lines,
                            SongNs maximum=5*second,SongNs delay=ms,std::uint64_t epoch=1,char hash='0'){
    const auto receipt=origin+time;
    if(hash=='0')hash="0123456789abcdef"[seq%16];
    const SceneOpeningFrame frame{seq,epoch,1,1,receipt,"line-frame-"+std::to_string(seq),std::string(64,hash),lines};
    return model.observe(frame,receipt+delay,{-second,maximum});
}
SceneOpeningSyncResult motion(SceneOpeningSync& model,SceneOpeningAxis axis=horizontal,SongNs maximum=5*second){
    check(!feed(model,1,800*ms,{{axis,250}},maximum).origin_host_time,"single frame provided origin");
    check(!feed(model,2,850*ms,{{axis,262.5}},maximum).origin_host_time,"two frames provided origin");
    return feed(model,3,900*ms,{{axis,275}},maximum);
}
}

int main() try {
    for(const auto axis:{horizontal,vertical}){
        SceneOpeningSync model(chart_with_line(axis));const auto ready=motion(model,axis);
        check(ready.song_time&&ready.origin_host_time&&ready.native_line_index==0&&
              ready.origin_host_time->lower<=origin&&origin<=ready.origin_host_time->upper&&
              ready.song_time->lower<=900*ms&&900*ms<=ready.song_time->upper&&
              ready.origin_host_time->upper-ready.origin_host_time->lower<=40*ms&&
              ready.supporting_evidence_ids==std::vector<std::string>{"line-frame-1","line-frame-2","line-frame-3"}&&
              !ready.input_permission&&!ready.pixels_retained&&!ready.timing_qualified,
              "note-free native axis movement lost unique receipt timing or gained permission");
    }
    {
        auto chart=chart_with_line();auto& moves=chart.lines[0].move_events;
        const auto original=moves[0];moves.clear();
        constexpr std::array<SongNs,5> cuts{0,800*ms,850*ms,900*ms,2*second};
        for(std::size_t i=1;i<cuts.size();++i){
            auto piece=original;piece.time.start_ns=cuts[i-1];piece.time.end_ns=cuts[i];
            piece.start_y=std::lerp(original.start_y,original.end_y,static_cast<double>(cuts[i-1])/(2*second));
            piece.end_y=std::lerp(original.start_y,original.end_y,static_cast<double>(cuts[i])/(2*second));
            moves.push_back(piece);
        }
        SceneOpeningSync segmented(chart);
        check(motion(segmented).origin_host_time.has_value(),"one actual track could not bridge adjacent native event pieces");
    }
    {
        auto chart=chart_with_line();add_line(chart,horizontal,50,550,3*second);
        SceneOpeningSync model(chart),earlier_window(chart);
        const auto ambiguous=motion(model);
        check(!ambiguous.origin_host_time&&std::string_view(ambiguous.reason)=="scene_opening_time_alias"&&
              ambiguous.candidate_tracks>=2,"late native motion alias was pruned");
        check(motion(earlier_window,horizontal,2*second).origin_host_time.has_value(),"caller-supplied smaller time window was ignored");
    }
    {
        auto chart=chart_with_line();for(const auto y:{250.,262.5,275.})add_line(chart,horizontal,y,y,3*second);
        SceneOpeningSync static_alias(chart);
        const auto ambiguous=motion(static_alias);
        check(!ambiguous.origin_host_time&&std::string_view(ambiguous.reason)=="scene_opening_static_line_alias",
              "distinct static lines with intermittent coverage became moving evidence");
        SceneOpeningSync static_set(chart);
        const std::vector<SceneOpeningLine> lines{{horizontal,250},{horizontal,262.5},{horizontal,275}};
        for(unsigned seq=1;seq<=3;++seq)
            check(!feed(static_set,seq,(750+seq*50)*ms,lines).origin_host_time,"static observation set generated motion by reassignment");
    }
    {
        SceneOpeningSync stationary(chart_with_line(horizontal,250,250)),tilted(chart_with_line(horizontal,50,550,.5));
        for(unsigned seq=1;seq<=3;++seq)
            check(!feed(stationary,seq,(750+seq*50)*ms,{{horizontal,250}}).origin_host_time,"static chart line yielded origin");
        check(!motion(tilted).origin_host_time,"near-axis approximation became positive proof");
    }
    {
        SceneOpeningSync model(chart_with_line()),bounded(chart_with_line(),2*second);
        const auto stale=feed(model,1,800*ms,{{horizontal,250}},5*second,100*ms);
        check(!stale.origin_host_time&&std::string_view(stale.reason)=="scene_opening_invalid_or_stale_frame","old actual frame became evidence");
        feed(model,2,850*ms,{{horizontal,262.5}},5*second,ms,1,'a');
        feed(model,3,900*ms,{{horizontal,275}},5*second,ms,1,'a');
        check(!feed(model,4,950*ms,{{horizontal,287.5}}).origin_host_time,"identical pixels became distinct support");
        check(!feed(model,5,1*second,{{horizontal,300}},5*second,ms,2).origin_host_time,"previous source epoch reused");
        check(!feed(model,6,1050*ms,{{horizontal,312.5}},5*second,ms,2).origin_host_time,"new epoch armed after two frames");
        check(feed(model,7,1100*ms,{{horizontal,325}},5*second,ms,2).origin_host_time.has_value(),"new epoch did not reacquire independently");
        const auto too_wide=feed(bounded,1,800*ms,{{horizontal,250}});
        check(!too_wide.origin_host_time&&std::string_view(too_wide.reason)=="scene_opening_song_window_outside_model",
              "out-of-model time window discarded aliases silently");
    }
    std::cout<<"scene opening synchronization contracts passed\n";
    return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
