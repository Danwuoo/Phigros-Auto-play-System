#pragma once
#include "pas/game_session.hpp"
#include "pas/result_ui_labels.hpp"
#include <algorithm>
namespace pas::result_ui_reference {
SessionUiEvidence session_ui_pixels(const Frame& f,const DecisionSnapshot& s) {
    SessionUiEvidence e;
    if(!f.source_valid||f.width!=1280||f.height!=720||f.stride!=3840||f.rgb.size()!=1280*720*3)
        return e;
    // The frozen observer already measures current pause bars and score glyphs.
    // Read its diagnostic counts, never its note list or absence of notes.
    const auto count=[&](const char* key) {
        const auto at=s.ui_basis.find(key); if(at==std::string::npos)return 0;
        const auto begin=at+std::char_traits<char>::length(key);
        int n=0;for(auto i=begin;i<s.ui_basis.size()&&s.ui_basis[i]>='0'&&s.ui_basis[i]<='9';++i)
            n=std::min(100,n*10+s.ui_basis[i]-'0');
        return n;
    };
    e.hud=count("pause_bars=")>=2&&count("score_glyphs=")>=4;
    e.menu=s.ui==GameUi::menu&&s.capacity_valid;
    if(e.hud||e.menu)return e;
    double weakest=1;
    for(const auto& label:result_ui::labels) {
        double best=0;
        // Bounded translation tolerance only; this profile is 1280x720.
        for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx) {
            int intersection=0,united=0;
            for(int y=0;y<label.height;++y)for(int x=0;x<label.width;++x) {
                const auto* p=f.rgb.data()+static_cast<std::size_t>(label.y+y+dy)*f.stride+(label.x+x+dx)*3;
                const bool a=label.bits[static_cast<std::size_t>(y)*label.width+x]=='#';
                const int lo=std::min({p[0],p[1],p[2]}),hi=std::max({p[0],p[1],p[2]});
                const bool b=lo>90&&hi-lo<35;
                intersection+=a&&b;united+=a||b;
            }
            best=std::max(best,united?static_cast<double>(intersection)/united:0);
        }
        if(best>=.72)++e.result_labels;
        weakest=std::min(weakest,best);
    }
    e.result_similarity=weakest;
    e.result=e.result_labels==6&&!e.hud&&!e.menu;
    return e;
}
}
