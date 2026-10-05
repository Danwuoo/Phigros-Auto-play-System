// Author self-checks, separate from the frozen suite and independent challenge.
// Geometry is used only to render pixels and supply the already-legal ROI API.
#include "bvi.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
using namespace bvi;
using J=nlohmann::json;
namespace {
constexpr int W=640,H=640;
using RGB=std::array<std::uint8_t,3>;
constexpr RGB blue{40,190,255},white{255,255,255},black{0,0,0},yellow{255,220,0};
struct Canvas {
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(W*H*3);
 void at(int x,int y,RGB c){if(x<0||y<0||x>=W||y>=H)return;auto i=static_cast<std::size_t>(y*W+x)*3;std::copy(c.begin(),c.end(),bytes.begin()+i);}
 void rect(int x1,int y1,int x2,int y2,RGB c){for(int y=y1;y<=y2;++y)for(int x=x1;x<=x2;++x)at(x,y,c);}
 void hold(const Query&q,bool rails){
  const double ca=std::cos(q.angle),sa=std::sin(q.angle);
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){double dx=x-q.front.x,dy=y-q.front.y,s=dx*ca+dy*sa,n=-dx*sa+dy*ca;if(n>=-q.depth&&n<=0){if(std::abs(s)<=q.width/2)at(x,y,blue);if(rails&&std::abs(std::abs(s)-q.width/2)<=1.5)at(x,y,white);}}
 }
 void line(const Line&l){const double ca=std::cos(l.angle),sa=std::sin(l.angle);for(int y=0;y<H;++y)for(int x=0;x<W;++x){double dx=x-l.center.x,dy=y-l.center.y;if(std::abs(-dx*sa+dy*ca)<=2&&std::abs(dx*ca+dy*sa)<=l.length/2)at(x,y,white);}}
};
J checks=J::array();int failures=0;std::uint64_t maxProbes=0;
void check(std::string name,const J&want,const J&got){const bool pass=want==got;checks.push_back({{"name",name},{"expected",want},{"actual",got},{"pass",pass}});failures+=!pass;}
Key key(Ns t,std::uint64_t seq){Key k;k.capture=k.ready=t;k.sequence=seq;return k;}
Observation extract(Candidate&c,const Canvas&p,const std::vector<Query>&q,const std::vector<Line>&l,Key k){auto o=c.extract({p.bytes,W,H,W*3,k},q,l);maxProbes=std::max(maxProbes,o.probes);return o;}
Guard known(Ns now){return {now,200000000,200000000,30000000,"known_down",1,4,0,4,false};}
Query body(double front=504,double depth=200){return {{320,front},140,depth,0,false};}
Line line(){return {{320,500},0,640,7};}
void observation_case(const std::string&name,Query q,Canvas p,std::vector<Line> ls,bool expectedRails,bool expectedCap,bool checkMove=false,bool expectedMove=false){
 Candidate c;auto k=key(50000000,1);auto o=extract(c,p,{q},ls,k);c.relate(o,k,51000000);auto g=constrain(o,known(51000000));
 check(name+"/left",expectedRails,o.parts[0].left);check(name+"/right",expectedRails,o.parts[0].right);check(name+"/cap",expectedCap,o.parts[0].front_end);check(name+"/valid",false,o.invalid);
 if(checkMove){check(name+"/move",expectedMove,g.move);check(name+"/same-contact",4,g.contact_id);}
}
Canvas base(Query q,bool rails){Canvas p;p.hold(q,rails);p.line(line());return p;}
}
int main(){
 for(double y:{496.,500.,504.}){auto q=body(y);observation_case("visible-end-"+std::to_string(int(y)),q,base(q,true),{line()},true,true,true,true);}
 auto q=body();observation_case("white-line-no-rails",q,base(q,false),{line()},false,false,true,false);
 for(int n:{1,2}){auto p=base(q,false);for(int row=0;row<n;++row){p.at(250,480+row,white);p.at(390,480+row,white);}observation_case("rail-pixels-"+std::to_string(n),q,p,{line()},n==2,n==2,true,n==2);}
 {auto p=base(q,false);for(int row:{480,482}){p.at(250,row,white);p.at(390,row,white);}observation_case("two-isolated-rail-pixels",q,p,{line()},false,false,true,false);}
 {auto s=body(504,12);observation_case("short-body-long-rails",s,base(s,true),{line()},true,true,true,true);auto p=base(s,false);for(int y:{493,494}){p.at(250,y,white);p.at(390,y,white);}observation_case("short-body-two-pixel-rails",s,p,{line()},true,true,true,true);}
 {auto p=base(q,true);p.rect(265,500,275,503,black);observation_case("single-inner-flank",q,p,{line()},true,false);}
 {auto p=base(q,true);p.rect(265,500,375,503,yellow);observation_case("opaque-inner-band",q,p,{line()},true,false);}
 {auto p=base(q,true);p.rect(265,505,375,508,blue);observation_case("no-outer-black",q,p,{line()},true,false);}
 {auto p=base(q,true);Line second{{320,503},0,640,8};p.line(second);observation_case("all-inner-witnesses-masked",q,p,{line(),second},true,false);}
 Candidate history;Observation last;
 for(int step=0;step<3;++step){auto qn=q;qn.front.x=300+4*step;auto p=base(qn,true);auto k=key(10000000+20000000*step,step+1);last=extract(history,p,{qn},{line()},k);history.relate(last,k,k.capture+1000000);}
 check("new-entry/independent",3,last.relations[0].independent);check("new-entry/usable",true,last.relations[0].usable);
 auto g=known(51000000);g.execution="never_executed";g.prefix=0;g.attachment_query=-1;auto c=constrain(last,g);check("new-entry/down",true,c.down[0]);check("new-entry/not-move",false,c.move);
 g.execution="unknown_down";g.prefix=1;g.receipt_unknown=true;c=constrain(last,g);check("unknown/no-down",false,c.down[0]);check("unknown/no-move",false,c.move);check("unknown/release",true,c.release);
 g.execution="completed_up";g.prefix=2;c=constrain(last,g);check("completed/no-down",false,c.down[0]);check("completed/no-move",false,c.move);check("completed/persists",true,c.completed);
 g=known(51000000);g.plan_deadline=g.now;c=constrain(last,g);check("deadline/no-move",false,c.move);check("deadline/release",true,c.release);
 for(bool changeBackground:{false,true}){Candidate c;Observation o;for(int step=0;step<3;++step){auto p=base(q,true);if(changeBackground)p.at(1,1,{std::uint8_t(step+1),0,0});auto k=key(10000000+20000000*step,step+1);o=extract(c,p,{q},{line()},k);c.relate(o,k,k.capture+1000000);}check(std::string(changeBackground?"new-background":"duplicate-rgb")+"/independent",1,o.relations[0].independent);check(std::string(changeBackground?"new-background":"duplicate-rgb")+"/current-move",true,constrain(o,known(51000000)).move);}
 for(bool currentRails:{false,true}){
  Candidate c;Observation o;
  for(int step=0;step<3;++step){std::vector<Query>qs;if(step<2){qs={body(500,60-20*step),body(420+20*step,120)};}else qs={body(500,180)};Canvas p;for(auto shape:qs)p.hold(shape,step<2||currentRails);p.line(line());auto k=key(10000000+20000000*step,step+1);o=extract(c,p,qs,{line()},k);c.relate(o,k,k.capture+1000000);}
  if(!currentRails){check("original-union/ambiguous",true,o.relations[0].ambiguous);check("original-union/no-move",false,constrain(o,known(51000000)).move);}
  else {checks.push_back({{"name","true-cap-union/decision-only"},{"status","unresolved-relation-policy"},{"front_end",o.parts[0].front_end},{"ambiguous",o.relations[0].ambiguous},{"move",constrain(o,known(51000000)).move}});}
 }
 J out={{"schema","bvi.cold.v2.author-selfcheck.v1"},{"checks",checks},{"failed_assertions",failures},{"probes_max",maxProbes},{"metadata_bytes",Candidate::metadata_bytes()},{"touch_backend",false},{"physical_human_gold",0},{"adopted",false}};
 std::cout<<out.dump(2)<<'\n';return failures?1:0;
}
