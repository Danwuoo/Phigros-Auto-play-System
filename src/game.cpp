#include "pas/game.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace pas {
const char* name(GameUi v) {
    switch(v) {
    case GameUi::menu: return "MENU"; case GameUi::loading: return "LOADING";
    case GameUi::playing: return "PLAYING"; case GameUi::paused: return "PAUSED";
    case GameUi::result: return "RESULT"; default: return "UNKNOWN";
    }
}
const char* name(NoteKind v) {
    switch(v) {
    case NoteKind::tap: return "tap"; case NoteKind::hold: return "hold";
    case NoteKind::drag: return "drag"; case NoteKind::flick: return "flick";
    default: return "ambiguous";
    }
}
namespace {
struct Component {
    int color = 0, count = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double sx = 0, sy = 0, xx = 0, yy = 0, xy = 0;
};
int classify(const std::uint8_t* p) {
    const int r=p[0], g=p[1], b=p[2];
    if (b > 165 && g > 115 && b > r + 35) return 1;
    if (r > 180 && g > 160 && b < g - 55) return 2;
    if (r > 190 && r > g + 45 && b > 75) return 3;
    if (r > 195 && g > 195 && b > 195 && std::max({r,g,b})-std::min({r,g,b}) < 35) return 4;
    return 0;
}
std::vector<Component> components(const Frame& f, bool& capacity) {
    constexpr int scale=2;
    const int w=(f.width+1)/2, h=(f.height+1)/2;
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(w)*h);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x)
        mask[static_cast<std::size_t>(y)*w+x]=static_cast<std::uint8_t>(classify(
            f.rgb.data()+static_cast<std::size_t>(y*scale)*f.stride+x*scale*3));
    std::vector<int> queue; queue.reserve(mask.size());
    std::vector<Component> output;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const int index=y*w+x, color=mask[index];
        if(!color) continue;
        mask[index]=0; queue.clear(); queue.push_back(index);
        Component c; c.color=color; c.x0=c.x1=x*scale; c.y0=c.y1=y*scale;
        for(std::size_t i=0;i<queue.size();++i) {
            const int q=queue[i], qx=q%w, qy=q/w, px=qx*scale, py=qy*scale;
            ++c.count; c.sx+=px; c.sy+=py; c.xx+=px*px; c.yy+=py*py; c.xy+=px*py;
            c.x0=std::min(c.x0,px); c.x1=std::max(c.x1,px);
            c.y0=std::min(c.y0,py); c.y1=std::max(c.y1,py);
            for(const auto [dx,dy] : {std::pair{-1,0},{1,0},{0,-1},{0,1}}) {
                const int nx=qx+dx, ny=qy+dy;
                if(nx<0||ny<0||nx>=w||ny>=h) continue;
                const int ni=ny*w+nx;
                if(mask[ni]==color) {mask[ni]=0; queue.push_back(ni);}
            }
        }
        if(c.count<3) continue;
        if(output.size()==2048) {capacity=false; return output;}
        output.push_back(c);
    }
    return output;
}
double distance(Vec2 a, Vec2 b) {return std::hypot(a.x-b.x,a.y-b.y);}
double normal_distance(Vec2 p, const LineCandidate& l) {
    return (p.x-l.center.x)*(-l.tangent.y)+(p.y-l.center.y)*l.tangent.x;
}
bool same_geometry(const SceneContext& a,const SceneContext& b) {
    return a.epoch==b.epoch&&a.generation==b.generation&&a.geometry==b.geometry&&
        a.width==b.width&&a.height==b.height&&a.rotation==b.rotation;
}
std::optional<Vec2> song_play_button(const Frame& f) {
    const auto white=[&](int x,int y) {
        if(x<0||y<0||x>=f.width||y>=f.height) return false;
        return classify(f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3)==4;
    };
    const auto fraction=[&](double x0,double y0,double x1,double y1) {
        int count=0,total=0;
        for(int y=static_cast<int>(y0*f.height);y<static_cast<int>(y1*f.height);y+=2)
            for(int x=static_cast<int>(x0*f.width);x<static_cast<int>(x1*f.width);x+=2) {
                ++total; if(white(x,y)) ++count;
            }
        return total?static_cast<double>(count)/total:0.0;
    };
    // Two independent selection-page structures, observed on the current
    // 1280x720 UI: selected difficulty badge and white Play trapezoid.
    // The title screen and settlement's Next button do not satisfy both.
    if(fraction(.35,.34,.43,.46)<.30 || fraction(.91,.82,.995,.94)<.65) return {};
    const int x0=static_cast<int>(f.width*.92),x1=static_cast<int>(f.width*.98),
              y0=static_cast<int>(f.height*.835),y1=static_cast<int>(f.height*.925);
    const int radius=std::max(6,static_cast<int>(f.height*.024));
    int count=0,minimum_x=x1,maximum_x=x0,minimum_y=y1,maximum_y=y0;
    double sx=0,sy=0;
    for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
        const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
        if(std::max({p[0],p[1],p[2]})>90) continue;
        const int surrounding=white(x-radius,y)+white(x+radius,y)+white(x,y-radius)+white(x,y+radius);
        if(surrounding<3) continue;
        ++count; sx+=x; sy+=y; minimum_x=std::min(minimum_x,x); maximum_x=std::max(maximum_x,x);
        minimum_y=std::min(minimum_y,y); maximum_y=std::max(maximum_y,y);
    }
    if(count<12 || count>f.width*f.height*.003 || maximum_x-minimum_x<f.width*.009 ||
       maximum_y-minimum_y<f.height*.016) return {};
    return Vec2{sx/count,sy/count};
}
}
void GameObserver::reset() {tracks_.clear(); playing_confirmations_=0; menu_confirmations_=0; previous_={};}

DecisionSnapshot GameObserver::process(const Frame& f) {
    if(f.width<2||f.height<2||f.stride!=f.width*3||
       f.rgb.size()!=static_cast<std::size_t>(f.stride)*f.height)
        throw std::invalid_argument("invalid game frame");
    DecisionSnapshot out; out.sequence=++sequence_;
    out.context={f.epoch,f.generation,f.geometry_version,f.sequence,f.capture_complete_ns,
                 f.width,f.height,f.source_rotation};
    out.recognition_start_ns=clock_.now_ns();
    if(!same_geometry(previous_,out.context)||f.capture_complete_ns<=previous_.capture_ns||
       f.capture_complete_ns-previous_.capture_ns>100'000'000) reset();
    const bool distinct=f.sequence>previous_.frame;
    previous_=out.context;
    const auto all=components(f,out.capacity_valid);
    std::vector<NoteCandidate> notes;
    int pause_bars=0, score_glyphs=0;
    // Live HD exposed a one-pixel horizontal line joined to Hold borders.
    // Component PCA alone then loses it. Scan every source row for long thin
    // visible runs; position is discovered from pixels, never a fixed Y.
    struct RowLine { int y, x0, x1; };
    std::vector<RowLine> rows;
    for(int y=static_cast<int>(f.height*.12);y<static_cast<int>(f.height*.95);++y) {
        int begin=-1, last=-1, gaps=0;
        const auto finish=[&] {
            if(begin>=0 && last-begin>f.width*.32 && rows.size()<static_cast<std::size_t>(f.height))
                rows.push_back({y,begin,last});
        };
        for(int x=0;x<f.width;++x) {
            const int color=classify(f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3);
            if(color==4||color==2) {if(begin<0) begin=x; last=x; gaps=0;}
            else if(begin>=0 && ++gaps>4) {finish(); begin=-1; last=-1; gaps=0;}
        }
        finish();
    }
    for(std::size_t i=0;i<rows.size();) {
        std::size_t j=i+1;
        int left=rows[i].x0, right=rows[i].x1;
        // Collinear support remains one geometric line even when a Hold
        // interrupts it. Group the whole thin band, not individual runs;
        // separate Y bands must retain separate association candidates.
        while(j<rows.size() && rows[j].y-rows[j-1].y<=2) {
            left=std::min(left,rows[j].x0); right=std::max(right,rows[j].x1); ++j;
        }
        const double thickness=rows[j-1].y-rows[i].y+1;
        if(thickness<=f.height*.018 && out.lines.size()<16)
            out.lines.push_back({{(left+right)/2.0,(rows[i].y+rows[j-1].y)/2.0},
                {1,0},static_cast<double>(right-left),thickness,.65});
        i=j;
    }
    for(const auto& c:all) {
        const double w=c.x1-c.x0+2.0,h=c.y1-c.y0+2.0;
        const Vec2 center{c.sx/c.count,c.sy/c.count};
        if(c.color==4 && c.x1<f.width*.12 && c.y1<f.height*.12 &&
           h>f.height*.013 && h>1.4*w && w<f.width*.02) ++pause_bars;
        if(c.color==4 && c.x0>f.width*.78 && c.y1<f.height*.12 &&
           h>f.height*.01 && h<f.height*.07 && w<f.width*.035) ++score_glyphs;
        const double xx=c.xx/c.count-center.x*center.x,
                     yy=c.yy/c.count-center.y*center.y, xy=c.xy/c.count-center.x*center.y;
        const double theta=.5*std::atan2(2*xy,xx-yy);
        const double spread=std::hypot(xx-yy,2*xy);
        const double major=std::sqrt(std::max(0.0,(xx+yy+spread)/2)),
                     minor=std::sqrt(std::max(0.0,(xx+yy-spread)/2));
        if((c.color==4||c.color==2) && major>f.width*.07 &&
           major>12*std::max(1.0,minor) && center.y>f.height*.12 &&
           center.y<f.height*.95) {
            int adjacent_note_samples=0;
            for(int k=-2;k<=2;++k) {
                const double along=k*major*.6;
                bool adjacent=false;
                for(const int side:{-1,1}) {
                    const int x=static_cast<int>(std::lround(center.x+along*std::cos(theta)-side*8*std::sin(theta))),
                              y=static_cast<int>(std::lround(center.y+along*std::sin(theta)+side*8*std::cos(theta)));
                    if(x>=0&&y>=0&&x<f.width&&y<f.height) {
                        const int color=classify(f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3);
                        adjacent=adjacent||color==1||color==3;
                    }
                }
                if(adjacent) ++adjacent_note_samples;
            }
            if(adjacent_note_samples>=3) continue; // Hold outline, not independent line evidence.
            const bool existing=std::any_of(out.lines.begin(),out.lines.end(),[&](const LineCandidate& l) {
                return std::abs(normal_distance(center,l))<8 && std::abs(std::sin(theta))<.03;
            });
            if(!existing) {
                if(out.lines.size()==16) {out.capacity_valid=false; continue;}
                out.lines.push_back({center,{std::cos(theta),std::sin(theta)},
                                     major*3.46,std::max(2.0,minor*3.46),.6});
            }
            continue;
        }
        if(c.color==4||center.y<f.height*.10 || w<f.width*.018||w>f.width*.20||
           h<4 || h>f.height*.75 || c.count*4<w*h*.13) continue;
        if(notes.size()==128) {out.capacity_valid=false; continue;}
        NoteKind kind=c.color==2?NoteKind::drag:c.color==3?NoteKind::flick:
            h>w*.65?NoteKind::hold:NoteKind::tap;
        notes.push_back({center,kind,w,h,.55});
    }
    // Independent HUD evidence: line/note presence never arms gameplay.
    // Non-playing classes require live evidence before templates are enabled.
    const bool hud=pause_bars>=2&&score_glyphs>=4;
    if(distinct) playing_confirmations_=hud?std::min(3,playing_confirmations_+1):0;
    out.playing_gate=f.source_valid&&out.capacity_valid&&playing_confirmations_>=3;
    out.ui=out.playing_gate?GameUi::playing:GameUi::unknown;
    out.ui_basis="development HUD: pause_bars="+std::to_string(pause_bars)+
        ";score_glyphs="+std::to_string(score_glyphs)+";confirmations="+
        std::to_string(playing_confirmations_)+";nonplaying_classes_unvalidated";
    const auto play=song_play_button(f);
    if(distinct) menu_confirmations_=!hud&&play?std::min(3,menu_confirmations_+1):0;
    if(!out.playing_gate&&menu_confirmations_>=3&&out.capacity_valid&&f.source_valid) {
        out.ui=GameUi::menu; out.play_button=play;
        out.ui_basis="song_selection_difficulty_badge_and_play_icon;confirmations="+
            std::to_string(menu_confirmations_);
    }
    const auto now=f.capture_complete_ns;
    std::erase_if(tracks_,[&](const History& t){return now-t.observed>=100'000'000;});
    // Merge complementary nested color ribbons only when one is distinctly
    // narrower and enclosed by the other. Equal-width overlapping candidates
    // retain separate identities; proximity alone is not merge evidence.
    for(std::size_t i=0;i<notes.size();++i) for(std::size_t j=i+1;j<notes.size();) {
        auto& a=notes[i]; const auto& b=notes[j];
        const double ratio=std::min(a.width,b.width)/std::max(a.width,b.width);
        if(a.kind==b.kind&&a.kind!=NoteKind::hold&&ratio>.6&&ratio<.93&&
           std::abs(a.center.x-b.center.x)<3&&std::abs(a.center.y-b.center.y)<10&&
           std::max(a.height,b.height)<20) {
            const double top=std::min(a.center.y-a.height/2,b.center.y-b.height/2),
                         bottom=std::max(a.center.y+a.height/2,b.center.y+b.height/2);
            a.center.y=(top+bottom)/2; a.height=bottom-top; a.width=std::max(a.width,b.width);
            notes.erase(notes.begin()+static_cast<std::ptrdiff_t>(j));
        } else ++j;
    }
    struct Pair { std::size_t note, track; double cost; };
    std::vector<Pair> pairs; pairs.reserve(notes.size()*tracks_.size());
    for(std::size_t ni=0;ni<notes.size();++ni) {
        const auto& n=notes[ni];
        for(std::size_t ti=0;ti<tracks_.size();++ti) {
            const auto& t=tracks_[ti]; if(t.kind!=n.kind) continue;
            Vec2 expected=t.last;
            if(t.points.size()>=2) {
                const auto& a=t.points[t.points.size()>3?t.points.size()-4:0]; const auto& b=t.points.back();
                const double dt=(b.t-a.t)/1e9;
                if(dt>0) {const double future=(now-b.t)/1e9;
                    expected.x+=(b.p.x-a.p.x)/dt*future; expected.y+=(b.p.y-a.p.y)/dt*future;}
            }
            const double d=distance(expected,n.center);
            if(d<f.width*.08) pairs.push_back({ni,ti,d});
        }
    }
    std::sort(pairs.begin(),pairs.end(),[](const Pair& a,const Pair& b) {
        if(a.cost!=b.cost) return a.cost<b.cost;
        if(a.note!=b.note) return a.note<b.note; return a.track<b.track;
    });
    std::vector<int> assigned(notes.size(),-1);
    std::vector<bool> track_used(tracks_.size()), uncertain(notes.size());
    for(const auto& pair:pairs) {
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
        History* match=assigned[ni]>=0?&tracks_[static_cast<std::size_t>(assigned[ni])]:nullptr;
        const bool ambiguous=uncertain[ni];
        if(!match) {
            if(tracks_.size()==128) {out.capacity_valid=false; continue;}
            tracks_.push_back({++next_id_,0,n.kind,n.center,now,{}}); match=&tracks_.back();
        }
        match->last=n.center; match->observed=now;
        GameTarget target; target.note_id=match->id; target.revision=++match->revision;
        target.note=n; target.evidence_ns=now; target.expires_ns=now+100'000'000;
        target.reason=ambiguous?"association_ambiguous":"line_unobservable";
        if(out.lines.size()==1&&!ambiguous) {
            const auto& l=out.lines.front();
            match->points.push_back({now,n.center,l});
            if(match->points.size()>8) match->points.pop_front();
            target.samples=static_cast<int>(match->points.size());
            target.reason="insufficient_history";
            if(match->points.size()>=3) {
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
                        residual>5?"nonlinear_or_mismatch":"prediction_observe_only";
                    if(tau>=0&&tau<=.35&&residual<=5) {
                        target.crossing_ns=now+static_cast<Nanoseconds>(std::llround(tau*1e9));
                        target.uncertainty_ns=static_cast<Nanoseconds>(std::llround(
                            std::max(2.0,residual)/std::abs(v)*1e9));
                        // Projection at the observed line is diagnostic only;
                        // rotating/moving-line hit regions require validation.
                        target.hit={n.center.x+d*l.tangent.y,n.center.y-d*l.tangent.x};
                    }
                }
            }
        } else if(out.lines.size()>1) target.reason="multiple_line_association_unvalidated";
        out.targets.push_back(std::move(target));
    }
    if(!out.capacity_valid) {out.playing_gate=false; out.ui=GameUi::unknown;}
    out.recognition_end_ns=clock_.now_ns();
    return out;
}

GamePlanOwner::GamePlanOwner(const Clock& clock, TouchBackend& backend, int contacts)
    : clock_(clock),scheduler_(clock,backend,contacts,128,16,350'000'000,100'000'000) {}
std::vector<TouchReceipt> GamePlanOwner::accept(const DecisionSnapshot& s) {
    std::vector<TouchReceipt> receipts;
    if(s.sequence<=last_snapshot_) {last_rejection_="snapshot_order"; return receipts;}
    if(s.context.epoch<epoch_ || (s.context.epoch==epoch_&&
       (s.context.generation<context_.generation || s.context.geometry<context_.geometry))) {
        last_rejection_="context_order"; return receipts;
    }
    last_snapshot_=s.sequence;
    if(!same_geometry(context_,s.context)) {
        scheduler_.cancel("scene_context_changed"); identities_.clear();
        epoch_=s.context.epoch; context_=s.context;
    }
    const bool fresh=s.context.capture_ns<=clock_.now_ns()&&
        clock_.now_ns()-s.context.capture_ns<100'000'000;
    if(!scheduler_.set_context(epoch_,s.context.generation,s.context.geometry,
        s.playing_gate&&s.capacity_valid&&fresh,s.context.capture_ns)) return receipts;
    if(!s.playing_gate||!s.capacity_valid||!fresh) {last_rejection_="gate_invalid"; return receipts;}
    std::set<std::uint64_t> visible;
    for(const auto& t:s.targets) visible.insert(t.note_id);
    for(auto& [id,identity]:identities_) if(!visible.contains(id)) {
        auto canceled=scheduler_.cancel_intent(identity.intent);
        receipts.insert(receipts.end(),canceled.begin(),canceled.end());
    }
    // Keep bounded retirement evidence beyond the observer's occlusion window.
    std::erase_if(identities_,[&](const auto& e) {
        return !visible.contains(e.first)&&clock_.now_ns()>=e.second.expires;
    });
    for(const auto& t:s.targets) {
        auto found=identities_.find(t.note_id);
        if(found==identities_.end()) {
            if(identities_.size()==128) {last_rejection_="identity_capacity"; scheduler_.cancel(last_rejection_); break;}
            found=identities_.emplace(t.note_id,Identity{0,0,t.expires_ns+250'000'000,false}).first;
        }
        auto& id=found->second; id.expires=t.expires_ns+250'000'000;
        if(id.submitted) {
            if(!t.crossing_ns||t.reason!="prediction_observe_only"||t.expires_ns<=clock_.now_ns()) {
                auto canceled=scheduler_.cancel_intent(id.intent);
                receipts.insert(receipts.end(),canceled.begin(),canceled.end());
            }
            continue; // First limited version: immutable Tap, no repeat.
        }
        if(!t.crossing_ns||t.reason!="prediction_observe_only"||t.note.kind!=NoteKind::tap||
           t.uncertainty_ns>20'000'000||t.expires_ns<=clock_.now_ns()) continue;
        const auto remaining=*t.crossing_ns-clock_.now_ns();
        if(remaining<0||remaining>40'000'000) continue; // Near-dispatch submission only.
        if(t.hit.x<0||t.hit.y<s.context.height*.12||t.hit.x>=s.context.width||t.hit.y>=s.context.height) {
            last_rejection_="unsafe_region"; continue;
        }
        id.intent=++next_intent_; id.revision=t.revision;
        ContactPlan plan{epoch_,id.intent,t.revision,t.evidence_ns,*t.crossing_ns+20'000'000,
            s.context.frame,"live_pixels_short_linear_fit",
            {{Phase::down,t.hit.x,t.hit.y,*t.crossing_ns},
             {Phase::up,t.hit.x,t.hit.y,*t.crossing_ns+15'000'000}}};
        plan.note_id=t.note_id; plan.generation=s.context.generation; plan.geometry_version=s.context.geometry;
        id.submitted=scheduler_.submit(std::move(plan));
        if(!id.submitted) last_rejection_="scheduler_rejected";
    }
    return receipts;
}
std::vector<TouchReceipt> GamePlanOwner::poll() {return scheduler_.run_due();}
void GamePlanOwner::stop() {scheduler_.request_stop(); scheduler_.run_due(); scheduler_.cancel("runtime_stop");}

std::optional<ContactPlan> PlayButtonPlanner::take(const DecisionSnapshot& s,Nanoseconds now) {
    if(attempted_ || s.ui!=GameUi::menu || !s.play_button || s.playing_gate || !s.capacity_valid ||
       !s.context.epoch || !s.context.frame || s.context.capture_ns>now ||
       now-s.context.capture_ns>=60'000'000) return {};
    const auto p=*s.play_button;
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<s.context.width*.88||p.x>=s.context.width||
       p.y<s.context.height*.78||p.y>=s.context.height*.96) return {};
    attempted_=true;
    ContactPlan plan{s.context.epoch,1,1,s.context.capture_ns,now+30'000'000,s.context.frame,
        "pixels_song_selection_PLAY",{{Phase::down,p.x,p.y,now},{Phase::up,p.x,p.y,now+20'000'000}}};
    plan.generation=s.context.generation; plan.geometry_version=s.context.geometry;
    return plan;
}

nlohmann::json decision_json(const DecisionSnapshot& s) {
    using nlohmann::json;
    json lines=json::array(),targets=json::array();
    for(const auto& l:s.lines) lines.push_back({{"x",l.center.x},{"y",l.center.y},
        {"ux",l.tangent.x},{"uy",l.tangent.y},{"length",l.length},{"confidence",l.confidence}});
    for(const auto& t:s.targets) targets.push_back({{"note_id",t.note_id},{"revision",t.revision},
        {"kind",name(t.note.kind)},{"x",t.note.center.x},{"y",t.note.center.y},
        {"width",t.note.width},{"height",t.note.height},{"evidence_ns",t.evidence_ns},
        {"expires_ns",t.expires_ns},{"crossing_ns",t.crossing_ns?json(*t.crossing_ns):json(nullptr)},
        {"uncertainty_ns",t.uncertainty_ns},{"hit_x",t.hit.x},{"hit_y",t.hit.y},
        {"relative_distance_px",t.distance},{"relative_velocity_px_s",t.velocity},
        {"residual_px",t.residual},{"samples",t.samples},{"reason",t.reason}});
    return {{"event","game_decision"},{"decision_schema",1},{"sequence",s.sequence},
        {"epoch",s.context.epoch},{"generation",s.context.generation},{"geometry_version",s.context.geometry},
        {"frame_sequence",s.context.frame},{"capture_complete_ns",s.context.capture_ns},
        {"recognition_start_ns",s.recognition_start_ns},{"recognition_end_ns",s.recognition_end_ns},
        {"ui",name(s.ui)},{"ui_basis",s.ui_basis},{"playing_gate",s.playing_gate},
        {"play_button",s.play_button?json{{"x",s.play_button->x},{"y",s.play_button->y}}:json(nullptr)},
        {"capacity_valid",s.capacity_valid},{"lines",lines},{"targets",targets},
        {"source_absolute_age",nullptr},{"per_note_feedback","unknown"}};
}

nlohmann::json analyze_game_jsonl(const std::filesystem::path& path) {
    using nlohmann::json;
    std::ifstream file(path); if(!file) throw std::runtime_error("cannot open game journal");
    std::uint64_t frames=0,predictions=0,dry=0,revokes=0,rejections=0,gate_frames=0,multi_line=0;
    std::map<std::string,std::uint64_t> kinds,reasons,uis;
    std::vector<double> processing,uncertainty,residual,intervals,down_skew;
    Nanoseconds previous=0,last_down_due=-1,last_down_start=0;
    const auto sample=[](std::vector<double>& values,double value) {if(values.size()<100'000) values.push_back(value);};
    const auto count=[](auto& counts,const std::string& key) {
        if(!counts.contains(key)&&counts.size()>=64) throw std::runtime_error("game analyzer key capacity");
        ++counts[key];
    };
    std::string line;
    while(std::getline(file,line)) {
        if(line.empty()) continue;
        const auto e=json::parse(line); const auto event=e.value("event","");
        if(event=="game_decision") {
            if(e.value("decision_schema",0)!=1) throw std::runtime_error("unknown game decision schema");
            ++frames; count(uis,e.at("ui").get<std::string>());
            if(e.at("playing_gate").get<bool>()) ++gate_frames;
            if(e.at("lines").size()>1) ++multi_line;
            const auto t=e.at("capture_complete_ns").get<Nanoseconds>();
            if(previous&&t>previous) sample(intervals,(t-previous)/1e6); previous=t;
            sample(processing,(e.at("recognition_end_ns").get<Nanoseconds>()-
                               e.at("recognition_start_ns").get<Nanoseconds>())/1e6);
            for(const auto& target:e.at("targets")) {
                count(kinds,target.at("kind").get<std::string>());
                count(reasons,target.at("reason").get<std::string>());
                if(!target.at("crossing_ns").is_null()) {
                    ++predictions; sample(uncertainty,target.at("uncertainty_ns").get<Nanoseconds>()/1e6);
                    sample(residual,target.at("residual_px").get<double>());
                }
            }
        } else if(event=="dry_touch_receipt") {
            ++dry;
            if(e.at("phase")==0) {
                const auto due=e.at("scheduled_ns").get<Nanoseconds>(),start=e.at("injection_start_ns").get<Nanoseconds>();
                if(due==last_down_due) sample(down_skew,(start-last_down_start)/1e6);
                last_down_due=due; last_down_start=start;
            }
        } else if(event=="runtime_revoke") ++revokes;
        else if(event=="scheduler_rejection") ++rejections;
    }
    return {{"schema_version",1},{"frames",frames},{"playing_gate_frames",gate_frames},
        {"ui_occurrences",uis},{"note_candidate_occurrences",kinds},{"target_reason_occurrences",reasons},
        {"prediction_occurrences",predictions},{"multi_line_frames",multi_line},
        {"dry_command_count",dry},{"runtime_revokes",revokes},{"scheduler_rejections",rejections},
        {"capture_interval_ms",distribution(intervals)},{"recognition_duration_ms",distribution(processing)},
        {"prediction_uncertainty_ms",distribution(uncertainty)},{"prediction_residual_px",distribution(residual)},
        {"dry_equal_deadline_down_skew_ms",distribution(down_skew)},
        {"distribution_sample_cap",100000},{"per_note_feedback","unknown"},{"gameplay_validated",false},
        {"raw_sha256",sha256_file(path)}};
}
void draw_game_overlay(Frame& f,const DecisionSnapshot& s) {
    const auto pixel=[&](int x,int y,std::array<std::uint8_t,3> color) {
        if(x>=0&&y>=0&&x<f.width&&y<f.height) {
            auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;
            std::copy(color.begin(),color.end(),p);
        }
    };
    const auto line=[&](Vec2 a,Vec2 b,std::array<std::uint8_t,3> color) {
        const int steps=static_cast<int>(std::max(std::abs(a.x-b.x),std::abs(a.y-b.y)))+1;
        for(int i=0;i<=steps;++i) {const double u=static_cast<double>(i)/steps;
            pixel(static_cast<int>(std::lround(a.x+(b.x-a.x)*u)),
                  static_cast<int>(std::lround(a.y+(b.y-a.y)*u)),color);}
    };
    for(const auto& l:s.lines) line({l.center.x-l.tangent.x*l.length/2,l.center.y-l.tangent.y*l.length/2},
        {l.center.x+l.tangent.x*l.length/2,l.center.y+l.tangent.y*l.length/2},{0,255,0});
    for(const auto& t:s.targets) {
        const auto p=t.note.center; const double w=t.note.width/2,h=t.note.height/2;
        const std::array<std::uint8_t,3> color=t.crossing_ns?std::array<std::uint8_t,3>{0,255,255}:
            std::array<std::uint8_t,3>{255,100,0};
        line({p.x-w,p.y-h},{p.x+w,p.y-h},color); line({p.x-w,p.y+h},{p.x+w,p.y+h},color);
        line({p.x-w,p.y-h},{p.x-w,p.y+h},color); line({p.x+w,p.y-h},{p.x+w,p.y+h},color);
        if(t.crossing_ns) {line({t.hit.x-6,t.hit.y},{t.hit.x+6,t.hit.y},color);
            line({t.hit.x,t.hit.y-6},{t.hit.x,t.hit.y+6},color);}
    }
}
} // namespace pas
