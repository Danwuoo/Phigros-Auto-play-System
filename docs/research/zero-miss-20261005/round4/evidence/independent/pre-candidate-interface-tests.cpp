// Independent cold-v3 behavior review. Expectations are frozen before v3 source review.
// No candidate internals, canonical renderer, canonical oracle, or fixture helpers.
#ifndef BVI_HEADER
#define BVI_HEADER "bvi.hpp"
#endif
#include BVI_HEADER
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>
namespace independent_v3 {
using namespace bvi;
using nlohmann::json;
constexpr int W=640,H=640;
constexpr Ns BASE=1000000000, STEP=20000000;
struct Body {Query q; bool left=true,right=true;};
struct Hole {Query q;double lo_s{},hi_s{},lo_d{},hi_d{};};
struct Frame {std::vector<Body> bodies;std::vector<Query> queries;std::vector<Line> lines;std::vector<Hole> holes;int background{};};
Point local(Point p,const Query&q){double x=p.x-q.front.x,y=p.y-q.front.y;return {x*std::cos(q.angle)+y*std::sin(q.angle),-x*std::sin(q.angle)+y*std::cos(q.angle)};}
Point world(const Query&q,double s,double d){return {q.front.x+s*std::cos(q.angle)-d*std::sin(q.angle),q.front.y+s*std::sin(q.angle)+d*std::cos(q.angle)};}
std::vector<std::uint8_t> render(const Frame&f){std::vector<std::uint8_t>rgb(static_cast<std::size_t>(W)*H*3);auto put=[&](int x,int y,std::array<std::uint8_t,3>c){auto a=(static_cast<std::size_t>(y)*W+x)*3;for(int k=0;k<3;++k)rgb[a+k]=c[k];};
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){Point p{double(x),double(y)};
  for(auto&b:f.bodies){auto v=local(p,b.q);bool inside=b.q.tap?std::abs(v.y)<=4:v.y>=-b.q.depth&&v.y<=0;if(inside&&std::abs(v.x)<=b.q.width/2)put(x,y,{40,190,255});}
  for(auto&b:f.bodies){if(b.q.tap)continue;auto v=local(p,b.q);if(v.y>=-b.q.depth&&v.y<=0&&((b.left&&std::abs(v.x+b.q.width/2)<=1.5)||(b.right&&std::abs(v.x-b.q.width/2)<=1.5)))put(x,y,{255,255,255});}
  for(auto&h:f.holes){auto v=local(p,h.q);if(v.x>=h.lo_s&&v.x<=h.hi_s&&v.y>=h.lo_d&&v.y<=h.hi_d)put(x,y,{0,0,0});}
  for(auto&l:f.lines){double dx=p.x-l.center.x,dy=p.y-l.center.y;double s=dx*std::cos(l.angle)+dy*std::sin(l.angle),d=-dx*std::sin(l.angle)+dy*std::cos(l.angle);if(std::abs(s)<=l.length/2&&std::abs(d)<=2)put(x,y,{255,255,255});}
 }
 if(f.background)put(10,10,{static_cast<std::uint8_t>(f.background),17,19});return rgb;
}
Query hold(double x=320,double y=504,double depth=100,double a=0,double width=140){return {{x,y},width,depth,a,false};}
Query tap(double x=320,double y=500,double a=0,double width=140){auto q=hold(x,y,8,a,width);q.tap=true;return q;}
Line line(double x=320,double y=500,double a=0,double len=500,std::uint64_t id=1){return {{x,y},a,len,id};}
Frame frame(Query q=hold(),std::vector<Line>lines={line()},bool rails=true){return {{{q,rails,rails}},{q},std::move(lines),{},0};}
std::vector<Frame> approach(Query q,std::uint64_t id=1){std::vector<Frame>fs;for(double d:{-30.,-15.,0.}){auto p=world(q,0,d);fs.push_back(frame(q,{line(p.x,p.y,q.angle,500,id)}));}return fs;}
struct Timing {bool replay=false,wrong_epoch=false,future=false,stale=false;};
struct Result {Observation o;Constraints c;std::size_t samples{};std::vector<json>trace;};
Guard guard(Ns now,std::string_view execution,int attachment){Guard g;g.now=now;g.gate_deadline=now+200000000;g.plan_deadline=now+200000000;g.last_contact=now;g.execution=execution;g.prefix=execution=="never_executed"?0:execution=="completed_up"?2:1;g.contact_id=3;g.attachment_query=attachment;g.receipt_unknown=execution=="unknown_down";return g;}
std::size_t downs(const Constraints&c){return std::count(c.down.begin(),c.down.end(),true);}
json describe(const Result&r){json parts=json::array();for(std::size_t i=0;i<r.o.count;++i){auto&d=r.o.parts[i];auto&t=r.o.relations[i];parts.push_back({{"query_index",i},{"front",{d.q.front.x,d.q.front.y}},{"body",name(d.body)},{"contact",name(d.contact)},{"front_end",d.front_end},{"rear_end",d.rear_end},{"left",d.left},{"right",d.right},{"line_unique",d.line_unique},{"line_id",d.line_id},{"hit",{d.hit.x,d.hit.y}},{"independent",t.independent},{"span_ns",t.span},{"alternatives",t.alternatives},{"ambiguous",t.ambiguous},{"usable",t.usable},{"down",r.c.down[i]}});}return {{"parts",parts},{"down_count",downs(r.c)},{"move",r.c.move},{"refresh",r.c.refresh},{"release",r.c.release},{"completed",r.c.completed},{"invalid",r.c.invalid},{"contact_id",r.c.contact_id},{"observation_invalid",r.o.invalid},{"reason",r.o.reason},{"samples",r.samples},{"probes",r.o.probes}};}
Result execute(const std::vector<Frame>&fs,std::string_view execution="never_executed",int attachment=0,Timing timing={}){Candidate candidate;Result r;for(std::size_t i=0;i<fs.size();++i){auto rgb=render(fs[i]);Key k;k.capture=BASE+static_cast<Ns>(i)*STEP;k.ready=k.capture;k.sequence=i+1;if(timing.replay){k.capture=BASE;k.ready=BASE;k.sequence=1;}if(timing.wrong_epoch&&i+1==fs.size())k.context.epoch=2;Ns now=BASE+static_cast<Ns>(i)*STEP+1000000;if(i+1==fs.size()&&timing.future)now=k.capture-1;if(i+1==fs.size()&&timing.stale)now=k.capture+101000000;View v{rgb,W,H,W*3,k};r.o=candidate.extract(v,fs[i].queries,fs[i].lines);
#ifdef BVI_V3_RELATION_CONTEXT
observe_relation_context(r.o,fs[i].lines);
#endif
candidate.relate(r.o,k,now);r.c=constrain(r.o,guard(now,execution,attachment));r.samples=candidate.samples();r.trace.push_back(describe(r));}return r;}
using Check=std::pair<std::string,std::pair<bool,bool>>;
Check check(std::string name,bool actual,bool expected=true){return {std::move(name),{actual,expected}};}
struct Suite {json cases=json::array();int assertions=0,failures=0;int legal=0,legal_success=0,unsafe=0,unsafe_actions=0;std::map<std::string,std::array<int,3>>totals;
 void add(std::string id,std::string kind,const Result&r,std::vector<Check>checks,int expected_down=-1,int expected_move=-1){json rows=json::array();bool pass=true;for(auto&[label,vs]:checks){bool ok=vs.first==vs.second;rows.push_back({{"field",label},{"actual",vs.first},{"expected",vs.second},{"pass",ok}});++assertions;if(!ok){++failures;pass=false;}}auto&t=totals[kind];++t[0];if(!checks.empty()){++t[1];if(pass)++t[2];}bool legal_action=expected_down>0||expected_move==1;bool action_ok=(expected_down<0||downs(r.c)==static_cast<std::size_t>(expected_down))&&(expected_move<0||r.c.move==bool(expected_move));if(legal_action){++legal;if(action_ok)++legal_success;}if(kind=="safe_denial"){++unsafe;if(downs(r.c)>0||r.c.move)++unsafe_actions;}cases.push_back({{"id",id},{"class",kind},{"checks",rows},{"pass",checks.empty()?json(nullptr):json(pass)},{"expected_down_count",expected_down<0?json(nullptr):json(expected_down)},{"expected_move",expected_move<0?json(nullptr):json(bool(expected_move))},{"action_pass",legal_action?json(action_ok):json(nullptr)},{"actual",describe(r)},{"trace",r.trace}});}
};
void down_case(Suite&s,std::string id,const std::vector<Frame>&fs,int count=1){auto r=execute(fs);s.add(id,"legal_action_opportunity",r,{check("expected_down_count",downs(r.c)==static_cast<std::size_t>(count)),check("not_invalid",!r.c.invalid)},count,0);}
void move_case(Suite&s,std::string id,const std::vector<Frame>&fs,int attachment=0){auto r=execute(fs,"known_down",attachment);s.add(id,"legal_action_opportunity",r,{check("move",r.c.move),check("refresh",r.c.refresh),check("same_contact",r.c.contact_id==3),check("no_down",downs(r.c)==0)},0,1);}
void denial(Suite&s,std::string id,const std::vector<Frame>&fs,std::string_view execution="never_executed",Timing timing={}){auto r=execute(fs,execution,0,timing);s.add(id,"safe_denial",r,{check("no_down",downs(r.c)==0),check("no_move",!r.c.move)},0,0);}
std::vector<Frame> moving_two_taps(){std::vector<Frame>fs;for(double y:{480.,490.,500.}){Frame f;for(double x:{299.,341.}){auto q=tap(x,y,0,32);f.bodies.push_back({q,false,false});f.queries.push_back(q);}f.lines={line()};fs.push_back(f);}return fs;}
json run(){Suite s;
 down_case(s,"A01_stationary_tap_line_approach",approach(tap()));
 down_case(s,"A02_stationary_hold_line_approach",approach(hold(320,500)));
 down_case(s,"A03_stationary_tap_rotated_approach",approach(tap(320,500,.35)));
 down_case(s,"A04_stationary_tap_translated_approach",approach(tap(273,427)));
 {auto before=frame(hold(320,500));before.queries.push_back(before.queries[0]);move_case(s,"R01_prior_duplicate_continuation",{before,frame()});}
 {auto fs=approach(tap());for(auto&f:fs)f.queries.push_back(f.queries[0]);down_case(s,"R02_current_duplicate_one_down",fs);}
 {auto f=frame();f.queries.push_back(f.queries[0]);move_case(s,"R03_alias_attachment_keeps_contact",{f,f,f},1);}
 {auto f=frame();auto other=hold(125,504,100,0,70);f.bodies.push_back({other,true,true});f.queries.insert(f.queries.begin(),other);f.queries.push_back(f.queries[1]);move_case(s,"R04_permuted_alias_attachment",{f,f,f},2);}
 {auto fs=approach(tap());fs[0].queries.push_back(fs[0].queries[0]);fs[1].queries.push_back(fs[1].queries[0]);down_case(s,"R05_prior_duplicates_stationary_down",fs);}
 down_case(s,"R06_two_separate_nearby_taps",moving_two_taps(),2);
 {Frame a,b;for(auto q:{hold(320,410,60),hold(320,500,60)}){a.bodies.push_back({q,true,true});a.queries.push_back(q);}for(auto q:{hold(320,414,60),hold(320,504,60)}){b.bodies.push_back({q,true,true});b.queries.push_back(q);}a.lines=b.lines={line(320,400),line(320,500,0,500,2)};move_case(s,"R07_two_holds_remain_separate",{a,b},1);}
 move_case(s,"O01_oblique_current_blue",{frame(hold(316,505,200,.25),{line(320,500,.4)})});
 move_case(s,"O02_oblique_translated",{frame(hold(266,435,200,.25),{line(270,430,.4)})});
 move_case(s,"O03_oblique_opposite_angle",{frame(hold(324,505,200,-.25),{line(320,500,-.4)})});
 move_case(s,"O04_oblique_deeper_intersection",{frame(hold(316,540,200,.25),{line(320,500,.4)})});
 {auto a=frame(tap()),b=a,c=a;b.background=1;c.background=2;denial(s,"N01_background_only",{a,b,c});}
 {std::vector<Frame>fs;for(double y:{200.,215.,230.})fs.push_back(frame(tap(),{line(),line(320,y,0,500,2)}));denial(s,"N02_irrelevant_line_moves",fs);}
 denial(s,"N03_same_source_replay",approach(tap()),"never_executed",{true,false,false,false});
 {std::vector<Frame>fs;int i=0;for(double y:{500.10,500.11,500.12}){auto f=frame(tap(),{line(320,y)});f.background=++i;fs.push_back(f);}denial(s,"N04_subpixel_line_noise_same_pixels",fs);}
 denial(s,"N05_wrong_epoch",approach(tap()),"never_executed",{false,true,false,false});
 denial(s,"N06_future_capture",approach(tap()),"never_executed",{false,false,true,false});
 denial(s,"N07_stale_capture",approach(tap()),"never_executed",{false,false,false,true});
 {auto fs=approach(tap());for(auto&f:fs){auto l=f.lines[0];l.id=2;l.center.y+=2;f.lines.push_back(l);}denial(s,"N08_two_current_lines_ambiguous",fs);}
 {auto fs=approach(tap());for(std::size_t i=0;i<fs.size();++i)fs[i].lines[0].id=i+1;denial(s,"N09_line_id_switch",fs);}
 denial(s,"N10_unknown_receipt_no_resurrection",approach(tap()),"unknown_down");
 denial(s,"N11_completed_no_resurrection",approach(tap()),"completed_up");
 {Frame before;for(auto q:{hold(320,410,60),hold(320,500,60)}){before.bodies.push_back({q,true,true});before.queries.push_back(q);}before.lines={line()};denial(s,"N12_two_holds_union",{before,frame(hold(320,500,150))},"known_down");}
 {Frame before;for(auto q:{hold(320,460,15,0,80),hold(320,480,15,0,80)}){before.bodies.push_back({q,true,true});before.queries.push_back(q);}before.lines={line()};denial(s,"N13_near_distinct_prior_regions_not_exact_duplicates",{before,frame(hold(320,500,70,0,80))},"known_down");}
 {auto f=frame(hold(316,505,200,.25),{line(320,500,.4)});f.bodies.clear();denial(s,"N14_white_line_only",{f},"known_down");}
 denial(s,"N15_oblique_no_rails",{frame(hold(316,505,200,.25),{line(320,500,.4)},false)},"known_down");
 denial(s,"N16_parallel_line_and_rails",{frame(hold(),{line(320,450,1.5707963267948966)})},"known_down");
 denial(s,"N17_frame_boundary_missing_witness",{frame(hold(20,505,200,.25),{line(24,500,.4)})},"known_down");
 denial(s,"N18_short_body_no_visible_contact",{frame(hold(320,501,2),{line()})},"known_down");
 {auto q=hold(316,505,200,.25);auto f=frame(q,{line(320,500,.4)});f.holes.push_back({q,1,67,-200,0});denial(s,"N19_missing_one_side_support",{f},"known_down");}
 {auto a=frame(hold(316,505,200,.25),{line(320,500,.4),line(320,200,0,500,2)});auto ra=execute({a},"known_down");std::reverse(a.lines.begin(),a.lines.end());auto rb=execute({a},"known_down");s.add("M01_line_input_order","metamorphic",rb,{check("move_invariant",ra.c.move==rb.c.move),check("down_invariant",downs(ra.c)==downs(rb.c)),check("contact_invariant",ra.c.contact_id==rb.c.contact_id)});}
 {auto a=moving_two_taps();auto ra=execute(a);for(auto&f:a)std::reverse(f.queries.begin(),f.queries.end());auto rb=execute(a);s.add("M02_distinct_query_order","metamorphic",rb,{check("down_count_invariant",downs(ra.c)==downs(rb.c)),check("two_down",downs(rb.c)==2)});}
 {auto f=frame();auto a=execute({f},"known_down"),b=execute({f},"known_down");s.add("D01_identical_observations_physical_owner_unknown","decision_only",b,{});s.cases.back()["identical_output"]=describe(a)==describe(b);s.cases.back()["physical_owner_gold"]=nullptr;}
 {auto f=frame();auto q=f.queries[0];q.front.x+=.1;f.queries.push_back(q);s.add("D02_near_duplicate_same_visible_body","decision_only",execute({f,frame()},"known_down"),{});}
 s.add("D03_short_six_pixel_body_unresolved","decision_only",execute({frame(hold(320,504,6))},"known_down"),{});
 json totals=json::object();for(auto&[kind,t]:s.totals)totals[kind]={{"cases",t[0]},{"scored_cases",t[1]},{"passed_cases",t[2]},{"failed_cases",t[1]-t[2]}};
 return {{"schema","bvi-cold-v3-independent-results-v1"},{"renderer","Independent integer pixel-center C++20 blue bodies, 3px white rails, 4px white lines"},{"case_count",s.cases.size()},{"assertions",s.assertions},{"failed_assertions",s.failures},{"legal_action_opportunities",s.legal},{"legal_actions_observed",s.legal_success},{"overrejections",s.legal-s.legal_success},{"safety_denial_opportunities",s.unsafe},{"unsafe_false_positives",s.unsafe_actions},{"denominators",totals},{"cases",s.cases}};
}
}
int main(int argc,char**argv){auto r=independent_v3::run();if(argc==2){std::ofstream out(argv[1]);if(!out)return 2;out<<r.dump(2)<<'\n';if(!out)return 2;}else std::cout<<r.dump(2)<<'\n';return r["failed_assertions"].get<int>()?1:0;}
