#include "pas/game_session.hpp"
#include "pas/result_ui_labels.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace pas {
const char* name(PlaySessionState s) {
    switch(s) {
    case PlaySessionState::standby:return "STANDBY";
    case PlaySessionState::starting:return "STARTING";
    case PlaySessionState::playing:return "PLAYING";
    case PlaySessionState::result:return "RESULT";
    case PlaySessionState::fault:return "FAULT";
    case PlaySessionState::stopped:return "STOPPED";
    } return "FAULT";
}
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
PlaySessionLifecycle::PlaySessionLifecycle(Nanoseconds watchdog):watchdog_(watchdog) {
    if(watchdog<0||watchdog>3'600'000'000'000LL)throw std::invalid_argument("invalid round watchdog");
}
SessionStatus PlaySessionLifecycle::stop(const std::string& reason,bool fault) {
    status_.ended=status_.active;status_.active=false;status_.new_round=false;
    status_.state=fault?PlaySessionState::fault:PlaySessionState::stopped;status_.reason=reason;
    return status_;
}
SessionStatus PlaySessionLifecycle::observe(const SceneContext& c,const SessionUiEvidence& e,bool gate,Nanoseconds now) {
    status_.new_round=false;status_.ended=false;
    if(status_.state==PlaySessionState::fault||status_.state==PlaySessionState::stopped)return status_;
    const bool prior=previous_.frame!=0;
    if(prior&&(c.generation!=previous_.generation||c.geometry!=previous_.geometry||
        c.width!=previous_.width||c.height!=previous_.height||c.rotation!=previous_.rotation))
        return stop("geometry_or_generation_changed",true);
    const auto gap=c.capture_ns-previous_.capture_ns;
    const bool fresh=c.capture_ns<=now&&now-c.capture_ns<100'000'000&&
        (!prior||(c.frame>previous_.frame&&gap>0));
    if(!fresh) {hud_count_=result_count_=0;return status_;}
    if(prior&&gap>250'000'000)hud_count_=result_count_=0;
    previous_=c;
    if(status_.active&&watchdog_&&now-start_>=watchdog_)return stop("round_watchdog_aborted",true);
    if(status_.state==PlaySessionState::result)status_.state=PlaySessionState::standby;
    if(!status_.active) {
        if(e.result){left_result_=false;rearm_=false;}
        if(e.menu) {rearm_=true;left_result_=false;}
        // Manual retry can leave a result through a non-HUD transition.
        if(!e.result&&!e.hud&&!e.menu)left_result_=true;
        if(left_result_&&e.hud)rearm_=true;
        if(!rearm_||!e.hud) {hud_count_=0;return status_;}
        if(status_.round==std::numeric_limits<std::uint64_t>::max())return stop("round_id_exhausted",true);
        ++status_.round;status_.active=true;status_.new_round=true;
        status_.state=PlaySessionState::starting;status_.reason="current_playing_HUD_after_manual_transition";
        start_=c.capture_ns;hud_count_=0;result_count_=0;rearm_=false;left_result_=false;
    }
    if(e.hud) {
        if(!hud_count_)hud_since_=c.capture_ns;
        hud_count_=std::min(3,hud_count_+1);
    } else hud_count_=0;
    if(status_.state==PlaySessionState::starting&&gate&&hud_count_>=3&&c.capture_ns-hud_since_>=20'000'000) {
        status_.state=PlaySessionState::playing;status_.reason="fresh_independent_HUD_and_frozen_observer_gate";
    }
    if(e.result) {
        if(!result_count_)result_since_=c.capture_ns;
        result_count_=std::min(3,result_count_+1);
        if(result_count_>=3&&c.capture_ns-result_since_>=60'000'000) {
            status_.state=PlaySessionState::result;status_.active=false;status_.ended=true;
            status_.reason="six_current_result_labels_confirmed_over_60ms";
            rearm_=false;left_result_=false;hud_count_=0;
        }
    } else result_count_=0;
    return status_;
}
SessionGameOwner::SessionGameOwner(const Clock& c,TouchBackend& b,int contacts,GameActionOptions options)
    :clock_(c),backend_(b),contacts_(contacts),options_(options) {}
SessionObservation SessionPerception::process(const Frame& f) {
    SessionObservation p;p.scene=observer_.process(f);
    p.evidence=session_ui_pixels(f,p.scene);
    p.status=lifecycle_.observe(p.scene.context,p.evidence,p.scene.playing_gate,clock_.now_ns());
    if(p.status.new_round) {
        observer_.reset();p.scene.playing_gate=false;p.scene.targets.clear();p.scene.lines.clear();
    }
    p.scene.context.epoch=std::max(std::uint64_t{1},p.status.round);
    p.allow_down=p.status.state==PlaySessionState::playing&&p.status.active&&p.scene.playing_gate&&
        p.scene.capacity_valid&&!p.evidence.result&&f.source_valid&&f.capture_complete_ns<=clock_.now_ns()&&
        clock_.now_ns()-f.capture_complete_ns<100'000'000;
    return p;
}
void SessionGameOwner::start(std::uint64_t round) {
    if(round<=round_||owner_)throw std::logic_error("round owner must finish before a new monotonic round");
    round_=round;owner_=std::make_unique<GamePlanOwner>(clock_,backend_,contacts_,options_);
}
std::vector<TouchReceipt> SessionGameOwner::accept(const DecisionSnapshot& s,bool allow) {
    if(!owner_)return {};
    if(allow)return owner_->accept(s);
    auto gated=s;gated.playing_gate=false;return owner_->accept(gated);
}
std::vector<TouchReceipt> SessionGameOwner::poll() {return owner_?owner_->poll():std::vector<TouchReceipt>{};}
ReleaseReport SessionGameOwner::suspend(const std::string& reason) {
    if(!owner_)return {};
    owner_->scheduler().cancel(reason);
    if(!owner_->scheduler().fault().empty())throw std::runtime_error(owner_->scheduler().fault());
    return owner_->scheduler().last_release();
}
ReleaseReport SessionGameOwner::finish() {
    if(!owner_)return {};
    // Finalization has no note dispatch. Stop linearizes once and keeps the
    // first release report rather than overwriting it with a second empty cancel.
    owner_->scheduler().request_stop();owner_->scheduler().cancel("round_finish");
    const auto report=owner_->scheduler().last_release();
    if(!owner_->scheduler().fault().empty())throw std::runtime_error(owner_->scheduler().fault());
    owner_.reset();return report;
}
}
