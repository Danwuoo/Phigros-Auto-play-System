#include "front.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
using namespace pas;
using namespace pas::hold_front;
using J=nlohmann::json;
namespace {
J checks=J::array();int failures=0;
void check(const std::string& id,bool ok){checks.push_back({{"id",id},{"pass",ok}});failures+=!ok;}
SceneContext ctx(const Frame& f){return {f.epoch,f.generation,f.geometry_version,f.sequence,f.capture_complete_ns,f.width,f.height,f.source_rotation};}
Frame blank(){Frame f;f.width=1280;f.height=720;f.stride=3840;f.epoch=f.generation=f.geometry_version=f.sequence=1;
    f.source_valid=true;f.source_rotation=1;f.capture_complete_ns=1;f.pixels_ready_ns=2;f.rgb.resize(1280*720*3,35);return f;}
TrackingCandidate roi(double angle=0){TrackingCandidate c;c.candidate_id=1;c.origin="current_reconstructed_front";
    c.quality=ObservationQuality::strong_current;c.note.kind=NoteKind::hold;c.note.center={640,360};c.note.width=120;
    c.note.tangent={std::cos(angle),std::sin(angle)};c.note.tail=Vec2{640+100*std::sin(angle),360-100*std::cos(angle)};return c;}
// Controlled local raster isolates geometry only, not Phigros semantic gold.
void paint(Frame& f,const TrackingCandidate& c,int edge,bool rails=true,int begin=-80){
    const auto u=c.note.tangent;const Vec2 v{-u.y,u.x};
    for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){
        const double dx=x-c.note.center.x,dy=y-c.note.center.y,a=dx*u.x+dy*u.y,n=dx*v.x+dy*v.y;
        if(n<begin||n>edge||std::abs(a)>c.note.width/2+1)continue;
        const std::array<std::uint8_t,3> rgb=rails&&std::abs(std::abs(a)-c.note.width/2)<1.4?
            std::array<std::uint8_t,3>{255,255,255}:std::array<std::uint8_t,3>{155,233,255};
        const auto at=std::size_t(y)*f.stride+x*3;std::copy(rgb.begin(),rgb.end(),f.rgb.begin()+at);
    }
}
void run(){
    for(int edge:{-12,0,9,16}){auto f=blank();auto c=roi();paint(f,c,edge);const auto r=measure(f,ctx(f),c);
        const auto p=consume(f,ctx(f),c,r);const auto label="offset_"+std::to_string(edge);
        check(label+"_measured_interval",r.boundary&&r.offset_low==edge&&r.offset_high==edge+1&&std::abs(r.boundary->y-(360+edge+.5))<.01);
        check(label+"_typed_no_ownership",p&&!p->action_authorized&&!p->head_role_confirmed);
        check(label+"_bounded",r.probes==probes_per_candidate&&r.left_rail_rows>=6&&r.right_rail_rows>=6);
    }
    for(double angle:{.45,1.5707963267948966,2.8,-.45}){auto f=blank();auto c=roi(angle);paint(f,c,7);
        auto r=measure(f,ctx(f),c);check("rotation_"+std::to_string(angle),r.boundary&&std::abs(r.offset_low-7)<2&&std::abs(r.offset_high-8)<2);
        if(r.boundary)check("rotation_normal_"+std::to_string(angle),std::abs(r.normal.x+std::sin(angle))<1e-9&&std::abs(r.normal.y-std::cos(angle))<1e-9);
    }
    auto f=blank();auto c=roi();paint(f,c,9);const auto good=measure(f,ctx(f),c);
    auto changed=good;changed.boundary->y+=1;check("edited_front_rejected",!consume(f,ctx(f),c,changed));
    changed=good;changed.lanes[0].inside.rgb[0]^=1;check("edited_support_rejected",!consume(f,ctx(f),c,changed));
    changed=good;changed.context.frame=2;check("typed_frame_rejected",!consume(f,ctx(f),c,changed));
    changed=good;changed.origin[0]='X';check("typed_origin_rejected",!consume(f,ctx(f),c,changed));
    auto other=ctx(f);++other.epoch;check("epoch_rejected",measure(f,other,c).reason==Reason::context_mismatch&&!consume(f,other,c,good));
    other=ctx(f);++other.generation;check("generation_rejected",!consume(f,other,c,good));
    other=ctx(f);++other.geometry;check("geometry_context_rejected",!consume(f,other,c,good));
    f.rgb[std::size_t(good.lanes[0].inside.y)*f.stride+good.lanes[0].inside.x*3]^=1;
    check("changed_current_pixels_rejected",!consume(f,ctx(f),c,good));
    for(int mode=0;mode<9;++mode){auto bad=blank();auto q=roi();
        if(mode==0)q.note.center.x=std::numeric_limits<double>::quiet_NaN();
        if(mode==1)q.note.width=std::numeric_limits<double>::infinity();
        if(mode==2)q.note.center.y=1e200;if(mode==3)q.note.width=513;
        if(mode==4)q.note.tangent.x=0;if(mode==5)q.note.tail->y=std::numeric_limits<double>::infinity();
        if(mode==6)bad.rgb.pop_back();if(mode==7)bad.stride=1;if(mode==8)bad.height=721;
        auto r=measure(bad,ctx(bad),q);check("malformed_"+std::to_string(mode),!r.boundary&&r.probes==0&&!consume(bad,ctx(bad),q,r));}
    f=blank();c=roi();paint(f,c,9);c.note.held_body_patch=true;
    auto body=measure(f,ctx(f),c);check("body_cannot_be_front",body.kind==Kind::body_interior&&body.probes==0&&!consume(f,ctx(f),c,body));
    c=roi();c.note.tail.reset();check("missing_tail_unknown",measure(f,ctx(f),c).reason==Reason::normal_unknown);
    c=roi();c.note.tail->x+=8;check("incompatible_tail_normal_unknown",measure(f,ctx(f),c).reason==Reason::normal_unknown);
    c=roi();c.note.center={10,10};c.note.tail=Vec2{10,-90};f=blank();paint(f,c,9);
    check("clip_does_not_invent_end",measure(f,ctx(f),c).kind==Kind::clipped);
    f=blank();c=roi();paint(f,c,9,false);check("no_rails_unknown",!measure(f,ctx(f),c).boundary);
    f=blank();paint(f,c,80);check("body_interior_no_end",!measure(f,ctx(f),c).boundary);
    f=blank();paint(f,c,-10);paint(f,c,18,true,0);auto multi=measure(f,ctx(f),c);
    check("multiple_ends_unknown",multi.reason==Reason::multiple_terminals&&multi.terminal_clusters==2);
    f=blank();paint(f,c,9);for(int y=355;y<=372;++y)for(int x=631;x<=649;++x){const auto at=std::size_t(y)*f.stride+x*3;
        f.rgb[at]=f.rgb[at+1]=f.rgb[at+2]=180;}
    check("occluded_inner_lane_abstains",!measure(f,ctx(f),c).boundary);
    f=blank();paint(f,c,9);auto sign=roi();sign.note.tangent={-1,0};
    const auto normal_flipped=measure(f,ctx(f),sign);check("mod_pi_normal_from_geometry",normal_flipped.boundary&&normal_flipped.normal.y==1);
    auto a=roi(),b=roi();a.note.center.x=300;a.note.tail->x=300;b.note.center.x=950;b.note.tail->x=950;b.candidate_id=2;
    f=blank();paint(f,a,3);paint(f,b,16);CandidateBatch batch;batch.context=ctx(f);batch.candidates={a,b};
    auto produced=produce(f,batch);check("distinct_neighbor_not_merged",produced.count==2&&produced.results[0].boundary&&produced.results[1].boundary&&
        produced.results[0].boundary->x==300&&produced.results[1].boundary->x==950);
    std::swap(batch.candidates[0],batch.candidates[1]);auto swapped=produce(f,batch);
    check("permutation_keeps_evidence",equivalent(produced.results[0],swapped.results[1])&&equivalent(produced.results[1],swapped.results[0]));
    batch.candidates={a,a};batch.candidates[1].candidate_id=2;auto duplicate=produce(f,batch);
    check("duplicate_is_not_physical_identity",duplicate.count==2&&duplicate.results[0].boundary&&duplicate.results[1].boundary&&
        duplicate.results[0].boundary->x==duplicate.results[1].boundary->x);
    batch.candidates.assign(128,a);auto full=produce(f,batch);check("exact_capacity_complete",full.valid&&full.count==128&&full.probes<=128*probes_per_candidate);
    batch.candidates.push_back(a);auto over=produce(f,batch);check("overflow_invalid_not_truncated",!over.valid&&over.count==0&&over.probes==0);
    batch.candidates={a};batch.lines.resize(17);check("complete_line_overflow",!produce(f,batch).valid);
    batch.lines.clear();batch.capacity_valid=false;check("declared_invalid_batch",!produce(f,batch).valid);
    c=roi();c.quality=ObservationQuality::rejected;check("rejected_proposal_abstains",measure(f,ctx(f),c).probes==0);
    c=roi();c.head_visible=false;check("invisible_head_abstains",measure(f,ctx(f),c).probes==0);
    c=roi();c.note.kind=NoteKind::tap;check("tap_identity_stays_unsupported",measure(f,ctx(f),c).kind==Kind::unsupported);
}
}
int main(int argc,char** argv){if(argc!=2||std::filesystem::exists(argv[1]))return 2;
    run();std::ofstream file(argv[1]);file<<J({{"schema","pas.current-hold-front-contract.v1"},{"assertions",checks.size()},
        {"failed_assertions",failures},{"checks",checks},{"physical_gold",0},{"device_commands",0},
        {"scope","current RGB geometry and no-action typed seam; ownership separately verified by unchanged 113-prefix suite"}}).dump(2)<<'\n';
    std::cout<<"front checks="<<checks.size()<<" failed="<<failures<<'\n';return file?(failures?1:0):2;}
