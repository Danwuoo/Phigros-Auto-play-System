// R3 relation expectations frozen before implementation; same source links v2/v3.
#ifndef BVI_HEADER
#define BVI_HEADER "bvi.hpp"
#endif
#include BVI_HEADER
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace bvi;
using J=nlohmann::json;
namespace {
constexpr int W=640,H=640;constexpr Ns MS=1000000,BASE=1000*MS;
struct Frame {std::vector<Query> bodies,queries;std::vector<Line> lines;int background{};int replay{-1};};
Query q(double x=320,double y=500,bool tap=true,double depth=80){return {{x,y},80,depth,0,tap};}
Line line(double y=500,double x=320,double length=500,std::uint64_t id=7){return {{x,y},0,length,id};}
Frame frame(Query a=q(),std::vector<Line> ls={line()}){return {{a},{a},std::move(ls),0,-1};}
std::vector<std::uint8_t> render(const Frame&f){std::vector<std::uint8_t> p(W*H*3);auto put=[&](int x,int y,int r,int g,int b){auto i=(y*W+x)*3;p[i]=r;p[i+1]=g;p[i+2]=b;};
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){for(auto a:f.bodies){double dx=x-a.front.x,dy=y-a.front.y,s=dx*std::cos(a.angle)+dy*std::sin(a.angle),d=-dx*std::sin(a.angle)+dy*std::cos(a.angle);bool inside=a.tap?std::abs(d)<=4:d>=-a.depth&&d<=0;if(std::abs(s)<=a.width/2&&inside)put(x,y,40,190,255);if(!a.tap&&inside&&std::abs(std::abs(s)-a.width/2)<=1.5)put(x,y,255,255,255);}for(auto l:f.lines){double dx=x-l.center.x,dy=y-l.center.y,s=dx*std::cos(l.angle)+dy*std::sin(l.angle),d=-dx*std::sin(l.angle)+dy*std::cos(l.angle);if(std::abs(d)<=2&&std::abs(s)<=l.length/2)put(x,y,255,255,255);}}
 if(f.background)put(2,2,f.background,30,10);return p;
}
Guard guard(Ns now,std::string_view ex,int attachment=0){return {now,now+100*MS,now+100*MS,now,ex,ex=="never_executed"?0:ex=="completed_up"?2:1,2,attachment,4,ex=="unknown_down"};}
struct Result{Observation o;Constraints c;std::size_t samples{};};
Result run(std::vector<Frame>fs,std::string_view ex="never_executed",int attachment=0,Ns step=20*MS,std::string_view fault={}){Candidate candidate;Result r;std::vector<std::vector<std::uint8_t>> rendered;
 for(std::size_t i=0;i<fs.size();++i){rendered.push_back(render(fs[i]));auto&pixels=rendered[fs[i].replay>=0?static_cast<std::size_t>(fs[i].replay):i];Key k;k.capture=k.ready=BASE+step*i;k.sequence=i+1;Ns now=k.capture+MS;
 if(i+1==fs.size()){if(fault=="source")k.source_valid=false;if(fault=="context")k.context.epoch=2;if(fault=="future")k.capture=now+MS;if(fault=="sequence")k.sequence=1;}
 r.o=candidate.extract({pixels,W,H,W*3,k},fs[i].queries,fs[i].lines);candidate.relate(r.o,k,now);auto g=guard(now,ex,attachment);if(fault=="deadline")g.plan_deadline=now;if(fault=="gate")g.gate_deadline=now;if(fault=="no_contacts")g.free_contacts=0;r.c=constrain(r.o,g);r.samples=candidate.samples();}return r;
}
std::vector<Frame> approach(bool tap=true){return {frame(q(320,500,tap),{line(530)}),frame(q(320,500,tap),{line(515)}),frame(q(320,500,tap),{line(500)})};}
std::vector<Frame> moving(){return {frame(q(320,486)),frame(q(320,494)),frame(q(320,500))};}
int downs(const Result&r){return std::count(r.c.down.begin(),r.c.down.end(),true);}
J describe(const Result&r){J parts=J::array();for(std::size_t i=0;i<r.o.count;++i){auto&d=r.o.parts[i];auto&t=r.o.relations[i];parts.push_back({{"index",i},{"front",{d.q.front.x,d.q.front.y}},{"contact",name(d.contact)},{"front_end",d.front_end},{"line_unique",d.line_unique},{"independent",t.independent},{"span",t.span},{"ambiguous",t.ambiguous},{"usable",t.usable},{"down",r.c.down[i]}});}return {{"count",r.o.count},{"parts",parts},{"move",r.c.move},{"refresh",r.c.refresh},{"release",r.c.release},{"invalid",r.c.invalid},{"completed",r.c.completed},{"downs",downs(r)},{"samples",r.samples}};}
struct Suite{J cases=J::array();int assertions{},failed{};std::map<std::string,std::array<int,3>> groups;
 void add(std::string id,std::string kind,const Result&r,std::vector<std::pair<std::string,std::pair<J,J>>> checks){J out=J::array();int errors=0;for(auto&[name,v]:checks){bool pass=v.first==v.second;out.push_back({{"field",name},{"expected",v.first},{"actual",v.second},{"pass",pass}});++assertions;errors+=!pass;}failed+=errors;auto&g=groups[kind];++g[0];g[1]+=checks.size();g[2]+=errors;cases.push_back({{"id",id},{"kind",kind},{"assertions",out},{"failed",errors},{"actual",describe(r)}});}
};
using Check=std::pair<std::string,std::pair<J,J>>;
Check ck(std::string s,J e,J a){return {s,{e,a}};}
J tests(){Suite s;auto legal=[&](std::string id,Result r){s.add(id,"legal_opportunity",r,{ck("new_down_count",1,downs(r)),ck("usable",true,r.o.relations[0].usable),ck("three_independent",true,r.o.relations[0].independent>=3),ck("span_ge_30ms",true,r.o.relations[0].span>=30*MS)});};auto reject=[&](std::string id,Result r){s.add(id,"safe_rejection",r,{ck("new_down_count",0,downs(r))});};
 legal("P01_stationary_tap_line_approach",run(approach()));legal("P02_stationary_hold_line_approach",run(approach(false)));legal("P03_legacy_moving_note",run(moving()));
 {auto f=approach();for(auto&x:f)x.queries.push_back(x.queries[0]);legal("P04_exact_current_duplicates_one_down",run(f));auto r=run(f,"known_down",1);s.add("P05_noncanonical_attachment","legal_opportunity",r,{ck("new_down_count",0,downs(r)),ck("move",true,r.c.move),ck("refresh",true,r.c.refresh),ck("count_preserved",2,r.o.count)});}
 {auto f=moving();f[0].queries.push_back(f[0].queries[0]);f[1].queries.push_back(f[1].queries[0]);auto r=run(f,"known_down");s.add("P06_duplicate_prior_no_false_ambiguity","legal_opportunity",r,{ck("ambiguous",false,r.o.relations[0].ambiguous),ck("move",true,r.c.move),ck("refresh",true,r.c.refresh)});}
 {auto f=approach();for(auto&x:f){auto b=x.bodies[0];b.front.x=500;x.bodies.push_back(b);x.queries.push_back(b);}auto r=run(f);s.add("P07_distinct_separated_current","legal_opportunity",r,{ck("count_preserved",2,r.o.count),ck("new_down_count",2,downs(r)),ck("first_down",true,r.c.down[0]),ck("second_down",true,r.c.down[1])});}
 {auto f=approach();for(auto&x:f){auto b=x.bodies[0];b.front.x=500;x.bodies.push_back(b);x.queries.push_back(b);x.queries.push_back(x.queries[0]);std::swap(x.queries[0],x.queries[2]);}auto r=run(f,"known_down",2);s.add("P08_alias_attachment_other_group_down","legal_opportunity",r,{ck("count_preserved",3,r.o.count),ck("new_down_count",1,downs(r)),ck("other_group_down",true,r.c.down[1]),ck("move",true,r.c.move)});}
 {auto a=frame(),b=a,c=a;reject("N01_stationary_pair",run({a,b,c}));b.background=1;c.background=2;reject("N02_background_only_change",run({a,b,c}));b.lines[0].id=8;c.lines[0].id=9;reject("N03_line_id_only_change",run({a,b,c}));}
 {auto f=approach();f[1].replay=0;f[2].replay=0;reject("N04_replayed_rgb_fresh_timestamps",run(f));}
 {auto f=std::vector<Frame>{frame(),frame(),frame()};for(int i=0;i<3;++i){f[i].lines.push_back(line(100+10*i));f[i].background=i+1;}reject("N05_unrelated_line_motion",run(f));}
 {auto f=std::vector<Frame>{frame(),frame(),frame()};for(int i=0;i<3;++i){f[i].lines[0].center.x+=10*i;f[i].lines[0].length=300;f[i].background=i+1;}reject("N06_tangential_motion_only",run(f));}
 {auto f=std::vector<Frame>{frame(q(),{line(500)}),frame(q(),{line(515)}),frame(q(),{line(530)})};reject("N07_receding_line",run(f));}
 reject("N08_short_span",run(approach(),"never_executed",0,10*MS));{auto f=approach();f.erase(f.begin());reject("N09_two_samples",run(f));}
 {auto f=approach();for(auto&x:f)x.lines.push_back(line(450,320,500,8));reject("N10_multiple_relevant_lines",run(f));}
 {auto f=moving();for(auto&x:f){auto near=x.queries[0];near.front.x+=1;x.queries.push_back(near);}auto r=run(f);s.add("N11_near_current_not_alias","safe_rejection",r,{ck("count_preserved",2,r.o.count),ck("new_down_count",0,downs(r)),ck("first_ambiguous",true,r.o.relations[0].ambiguous),ck("second_ambiguous",true,r.o.relations[1].ambiguous)});}
 {Frame before;before.bodies=before.queries={q(320,410,false,60),q(320,500,false,60)};before.lines={line()};auto after=frame(q(320,500,false,150));auto r=run({before,after},"known_down");s.add("N12_distinct_prior_union","safe_rejection",r,{ck("ambiguous",true,r.o.relations[0].ambiguous),ck("move",false,r.c.move),ck("new_down_count",0,downs(r))});}
 {auto f=moving();auto near=f.back().queries[0];near.front.x+=1;f.back().queries.push_back(near);auto r=run(f);s.add("N17_new_current_competitor", "safe_rejection",r,{ck("new_down_count",0,downs(r)),ck("first_ambiguous",true,r.o.relations[0].ambiguous),ck("second_ambiguous",true,r.o.relations[1].ambiguous)});}
 {auto f=approach();f[1].lines[0].id=8;reject("N18_switched_measured_line",run(f));}
 {auto f=std::vector<Frame>{frame(q(),{line(530)}),frame(q(),{line(515)}),frame(q(),{line(497)})};reject("N19_overline_crossing",run(f));}
 for(auto ex:{"unknown_down","completed_up"}){auto f=approach();for(auto&x:f)x.queries.push_back(x.queries[0]);auto r=run(f,ex,1);s.add(std::string("N13_")+ex,"receipt_safety",r,{ck("move",false,r.c.move),ck("new_down_count",0,downs(r)),ck(ex==std::string("unknown_down")?"release":"completed",true,ex==std::string("unknown_down")?r.c.release:r.c.completed)});}
 for(auto fault:{"deadline","gate","source","context","future","sequence","no_contacts"})reject(std::string("N14_")+fault,run(approach(),"never_executed",0,20*MS,fault));
 {auto f=approach();f[1].lines.clear();reject("N15_missing_intermediate_witness",run(f));}
 {auto f=std::vector<Frame>{frame(q(),{line(530)}),frame(q(),{line(520)}),frame(q(),{line(525)}),frame(q(),{line(500)})};reject("N16_recession_breaks_stationary_chain",run(f));}
 {auto f=approach();for(auto&x:f)x.queries.resize(128,x.queries[0]);auto r=run(f);s.add("B01_exact_alias_capacity_128","bounded_capacity",r,{ck("count",128,r.o.count),ck("new_down_count",1,downs(r)),ck("metadata_le_1MiB",true,Candidate::metadata_bytes()<=1048576),ck("descriptor_le_256",true,sizeof(Descriptor)<=256)});}
 J groups=J::object();for(auto&[k,v]:s.groups)groups[k]={{"cases",v[0]},{"assertions",v[1]},{"failed",v[2]}};return {{"schema","bvi-v3-relation-tests-v1"},{"case_count",s.cases.size()},{"assertions",s.assertions},{"failed_assertions",s.failed},{"denominators",groups},{"descriptor_bytes",sizeof(Descriptor)},{"metadata_bytes",Candidate::metadata_bytes()},{"cases",s.cases}};
}
}
int main(int argc,char**argv){auto j=tests();if(argc==2){std::ofstream o(argv[1]);if(!o)return 2;o<<j.dump(2)<<'\n';if(!o)return 2;}else std::cout<<j.dump(2)<<'\n';return j["failed_assertions"].get<int>()?1:0;}
