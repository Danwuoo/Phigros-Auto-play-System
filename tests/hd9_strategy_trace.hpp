#pragma once
#include "pas/game.hpp"
#include "pas/game_session.hpp"
#include <algorithm>

namespace pas::test {
inline Frame session_image(std::uint64_t sequence,Nanoseconds time) {
    Frame f;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;
    f.epoch=f.generation=f.geometry_version=1;f.sequence=sequence;f.capture_complete_ns=time;
    f.rgb.resize(1280*720*3);return f;
}
inline void box(Frame& f,int x,int y,int w,int h,std::array<std::uint8_t,3> color) {
    for(int py=y;py<y+h;++py)for(int px=x;px<x+w;++px)
        std::copy(color.begin(),color.end(),f.rgb.data()+static_cast<std::size_t>(py)*f.stride+px*3);
}
inline void session_hud(Frame& f) {
    box(f,20,20,6,22,{255,255,255});box(f,34,20,6,22,{255,255,255});
    for(int i=0;i<6;++i)box(f,1020+i*24,20,12,20,{255,255,255});
}
// Deterministic pixels + QPC. Golden produced against the separately rebuilt
// original 1636519 static library, before the session implementation build.
inline nlohmann::json hd9_trace(bool wrapped) {
    using nlohmann::json;json trace=json::array();
    for(int scenario=0;scenario<6;++scenario) {
        FakeClock clock;GameObserver observer(clock);FakeTouchBackend backend(clock);
        GamePlanOwner original(clock,backend,5,{15,35'000'000,30'000'000});
        SessionGameOwner session(clock,backend,5,{15,35'000'000,30'000'000});
        if(wrapped)session.start(1);
        for(int i=0;i<18;++i) {
            auto f=session_image(i+1,1'000'000'000+i*20'000'000);session_hud(f);
            box(f,100,540,1080,3,{255,255,255});
            const int head=340+i*14;
            if(scenario==0||scenario==4||scenario==5)box(f,380,head,144,8,{40,190,255});
            if(scenario==1) {
                box(f,420,head-140,144,140,{40,190,255});
                box(f,416,head-140,4,144,{245,245,245});box(f,564,head-140,4,144,{245,245,245});
            }
            if(scenario==2||scenario==5)box(f,720,head,144,8,{255,220,40});
            if(scenario==3||scenario==5)box(f,960,head-50,144,8,{255,65,95});
            if(scenario==4&&i==10)f=session_image(i+1,f.capture_complete_ns); // Short HUD loss, same epoch.
            clock.set(f.capture_complete_ns);const auto s=observer.process(f);
            auto receipts=wrapped?session.accept(s,true):original.accept(s);
            auto plans=wrapped?session.owner()->take_accepted_plans():original.take_accepted_plans();
            json pj=json::array();for(const auto& p:plans) {
                json steps=json::array();for(const auto& st:p.steps)steps.push_back({{"phase",static_cast<int>(st.phase)},{"x",st.x},{"y",st.y},{"due",st.due_ns}});
                pj.push_back({{"intent",p.intent_id},{"note",p.note_id},{"revision",p.revision},{"evidence",p.evidence_ns},
                    {"valid_until",p.valid_until_ns},{"steps",steps},{"basis",p.basis},{"prefix",p.prefix_offset}});
            }
            auto later=wrapped?session.poll():original.poll();receipts.insert(receipts.end(),later.begin(),later.end());
            json rj=json::array();for(const auto& r:receipts)rj.push_back({{"intent",r.command.intent_id},{"phase",static_cast<int>(r.command.phase)},
                {"x",r.command.x},{"y",r.command.y},{"scheduled",r.command.scheduled_ns},{"source",r.command.source_frame_sequence},{"success",r.success}});
            trace.push_back({{"scenario",scenario},{"decision",decision_json(s)},{"plans",pj},{"receipts",rj}});
        }
        if(wrapped)session.finish();else original.stop();
    }
    return trace;
}
}
