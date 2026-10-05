// Independent synthetic RGB review. Expectations frozen separately before candidate review.
// Deliberately does not include candidate internals, canonical renderer, or oracle adapters.
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

namespace review {
using nlohmann::json;
using namespace bvi;
constexpr int W=640,H=640;
constexpr Ns BASE=1000000000;
struct Body {Query q;bool left=true,right=true;};
struct Dot {Point p;double radius=0.6;};
struct Frame {std::vector<Body> bodies;std::vector<Query> queries;std::vector<Line> lines;std::vector<Dot> dots;int background_tag=0;};
Point local(Point p,const Query&q) {const double dx=p.x-q.front.x,dy=p.y-q.front.y;return {dx*std::cos(q.angle)+dy*std::sin(q.angle),-dx*std::sin(q.angle)+dy*std::cos(q.angle)};}
Point world(const Query&q,double s,double d){return {q.front.x+s*std::cos(q.angle)-d*std::sin(q.angle),q.front.y+s*std::sin(q.angle)+d*std::cos(q.angle)};}
std::vector<std::uint8_t> render(const Frame&f){
 std::vector<std::uint8_t> rgb(static_cast<std::size_t>(W)*H*3);
 auto put=[&](int x,int y,std::array<std::uint8_t,3>c){const auto at=(static_cast<std::size_t>(y)*W+x)*3;for(int k=0;k<3;++k)rgb[at+k]=c[k];};
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){const Point p{double(x),double(y)};
  for(const auto&b:f.bodies){auto a=local(p,b.q);const bool inside=b.q.tap?std::abs(a.y)<=4:(a.y>=-b.q.depth&&a.y<=0);if(std::abs(a.x)<=b.q.width/2&&inside)put(x,y,{40,190,255});}
  for(const auto&b:f.bodies){if(b.q.tap)continue;auto a=local(p,b.q);if(a.y>=-b.q.depth&&a.y<=0&&((b.left&&std::abs(a.x+b.q.width/2)<=1.5)||(b.right&&std::abs(a.x-b.q.width/2)<=1.5)))put(x,y,{255,255,255});}
  for(const auto&l:f.lines){const double dx=p.x-l.center.x,dy=p.y-l.center.y;const double s=dx*std::cos(l.angle)+dy*std::sin(l.angle),d=-dx*std::sin(l.angle)+dy*std::cos(l.angle);if(std::abs(d)<=2&&std::abs(s)<=l.length/2)put(x,y,{255,255,255});}
  for(const auto&z:f.dots)if(std::abs(p.x-z.p.x)<=z.radius&&std::abs(p.y-z.p.y)<=z.radius)put(x,y,{255,255,255});
 }
 if(f.background_tag)put(10,10,{static_cast<std::uint8_t>(f.background_tag),17,19});
 return rgb;
}
Query hold(double x=320,double y=504,double depth=100,double angle=0){return {{x,y},140,depth,angle,false};}
Line line(double x=320,double y=500,double angle=0,double length=500,std::uint64_t id=1){return {{x,y},angle,length,id};}
Frame standard(Query q=hold(),bool rails=true,std::vector<Line>ls={line()}){return {{{q,rails,rails}},{q},std::move(ls),{},0};}
struct Result {Observation o;Constraints c;std::size_t samples{};};
Guard guard(Ns now,std::string_view execution,int attachment=0){Guard g;g.now=now;g.gate_deadline=now+200000000;g.plan_deadline=now+200000000;g.last_contact=now;g.execution=execution;g.prefix=execution=="never_executed"?0:execution=="completed_up"?2:1;g.contact_id=3;g.attachment_query=attachment;g.receipt_unknown=execution=="unknown_down";return g;}
Result execute(const std::vector<Frame>&fs,std::string_view execution="known_down",int attachment=0){Candidate candidate;Result r;for(std::size_t i=0;i<fs.size();++i){auto rgb=render(fs[i]);Key key;key.capture=BASE+static_cast<Ns>(i)*20000000;key.ready=key.capture;key.sequence=i+1;View view{rgb,W,H,W*3,key};r.o=candidate.extract(view,fs[i].queries,fs[i].lines);candidate.relate(r.o,key,key.capture+1000000);r.c=constrain(r.o,guard(key.capture+1000000,execution,attachment));r.samples=candidate.samples();}return r;}
json describe(const Result&r){json parts=json::array();for(std::size_t i=0;i<r.o.count;++i){const auto&d=r.o.parts[i];const auto&t=r.o.relations[i];parts.push_back({{"front",{d.q.front.x,d.q.front.y}},{"body",name(d.body)},{"contact",name(d.contact)},{"left",d.left},{"right",d.right},{"front_end",d.front_end},{"rear_end",d.rear_end},{"line_unique",d.line_unique},{"line_id",d.line_id},{"hit",{d.hit.x,d.hit.y}},{"measured_depth",d.measured_depth},{"independent",t.independent},{"span_ns",t.span},{"ambiguous",t.ambiguous},{"alternatives",t.alternatives},{"usable",t.usable},{"down",r.c.down[i]}});}return {{"parts",parts},{"move",r.c.move},{"refresh",r.c.refresh},{"release",r.c.release},{"completed",r.c.completed},{"invalid",r.c.invalid},{"contact_id",r.c.contact_id},{"probes",r.o.probes},{"samples",r.samples}};}
struct Suite {json cases=json::array();int assertions=0,failures=0;std::map<std::string,std::array<int,3>>totals;
 void add(const std::string&id,const std::string&kind,const Result&r,const std::vector<std::pair<std::string,std::pair<bool,bool>>>&checks){json rows=json::array();bool pass=true;for(auto&[name,values]:checks){bool ok=values.first==values.second;rows.push_back({{"field",name},{"actual",values.first},{"expected",values.second},{"pass",ok}});++assertions;if(!ok){++failures;pass=false;}}cases.push_back({{"id",id},{"class",kind},{"checks",rows},{"pass",checks.empty()?json(nullptr):json(pass)},{"actual",describe(r)}});auto&t=totals[kind];++t[0];if(!checks.empty()){++t[1];if(pass)++t[2];}}
};
using Check=std::pair<std::string,std::pair<bool,bool>>;
Check check(std::string name,bool actual,bool expected){return {std::move(name),{actual,expected}};}
void positive(Suite&s,const std::string&id,Frame f){auto r=execute({f});s.add(id,"legal_action_opportunity",r,{check("bilateral_rails",r.o.parts[0].left&&r.o.parts[0].right,true),check("front_end",r.o.parts[0].front_end,true),check("move",r.c.move,true)});}
void negative(Suite&s,const std::string&id,Frame f,bool rails=true){auto r=execute({f});std::vector<Check>c{check("front_end",r.o.parts[0].front_end,false),check("move",r.c.move,false)};if(rails)c.push_back(check("bilateral_rails",r.o.parts[0].left&&r.o.parts[0].right,false));s.add(id,"safe_denial",r,c);}
bool sameEligibility(const Result&a,const Result&b){if(a.c.move!=b.c.move||a.c.refresh!=b.c.refresh||a.c.release!=b.c.release||a.o.count!=b.o.count)return false;std::vector<std::array<double,6>>aa,bb;auto get=[](const Result&r,std::vector<std::array<double,6>>&v){for(std::size_t i=0;i<r.o.count;++i)v.push_back({r.o.parts[i].q.front.x,r.o.parts[i].q.front.y,double(r.c.down[i]),double(r.o.parts[i].line_unique),double(r.o.relations[i].ambiguous),double(r.o.parts[i].front_end)});std::sort(v.begin(),v.end());};get(a,aa);get(b,bb);return aa==bb;}
json run(){Suite s;
 positive(s,"P01_true_rails_front_504",standard());
 positive(s,"P02_true_rails_front_500",standard(hold(320,500)));
 {auto r=execute({standard(hold(),true,{})});s.add("P03_no_overlay","observed_geometry",r,{check("bilateral_rails",r.o.parts[0].left&&r.o.parts[0].right,true),check("front_end",r.o.parts[0].front_end,true),check("rear_end",r.o.parts[0].rear_end,true)});}
 positive(s,"P04_translated_true",standard(hold(267,443),true,{line(267,439)}));
 for(auto [id,a]:std::vector<std::pair<std::string,double>>{{"P05_rotate_together_025",.25},{"P06_rotate_together_060",.60}}){auto q=hold(320,504,100,a);Point lc=world(q,0,-4);positive(s,id,standard(q,true,{line(lc.x,lc.y,a)}));}
 positive(s,"P07_short_visible_depth_16",standard(hold(320,504,16)));
 positive(s,"P08_partial_line_overlay",standard(hold(),true,{line(320,500,0,100)}));
 negative(s,"N01_no_rails_crossing_line",standard(hold(),false,{line(320,450)}));
 negative(s,"N02_no_rails_front_line",standard(hold(320,500),false));
 negative(s,"N03_no_rails_two_crossing_lines",standard(hold(),false,{line(320,450),line(320,500,0,500,2)}));
 {auto f=standard(hold(),false);f.dots={{world(f.queries[0],-70,-30),.6},{world(f.queries[0],70,-30),.6}};negative(s,"N04_isolated_bilateral_dots",f);}
 {auto f=standard();f.bodies[0].right=false;negative(s,"N05_unilateral_rail",f);}
 negative(s,"N06_fully_white_hidden_body",standard(hold(320,501,2)),false);
 for(auto [id,exec]:std::vector<std::pair<std::string,std::string_view>>{{"N07_unknown_down","unknown_down"},{"N08_completed_up","completed_up"}}){auto r=execute({standard(hold(320,490)),standard(hold(320,500)),standard(hold(320,504))},exec);s.add(id,"safe_denial",r,{check("move",r.c.move,false),check("down",r.c.down[0],false),check(exec=="unknown_down"?"release":"completed",exec=="unknown_down"?r.c.release:r.c.completed,true)});}
 {Frame before;for(auto q:{hold(320,410,60),hold(320,500,60)}){before.bodies.push_back({q,true,true});before.queries.push_back(q);}before.lines={line()};auto after=standard(hold(320,500,150));auto r=execute({before,after});s.add("N09_two_holds_union","safe_denial",r,{check("ambiguous",r.o.relations[0].ambiguous,true),check("move",r.c.move,false),check("down",r.c.down[0],false)});}
 negative(s,"N10_no_rails_translated",standard(hold(267,443),false,{line(267,439)}));
 {auto q=hold(320,504,100,.25);auto lc=world(q,0,-4);negative(s,"N11_no_rails_rotated",standard(q,false,{line(lc.x,lc.y,.25)}));}
 {auto r=execute({standard(hold(),true,{})});s.add("N12_unique_line_missing","safe_denial",r,{check("move",r.c.move,false),check("down",r.c.down[0],false)});}
 {Frame f;for(auto q:{hold(190,504),hold(450,504)}){f.bodies.push_back({q,true,true});f.queries.push_back(q);}f.lines={line()};auto a=execute({f});std::reverse(f.queries.begin(),f.queries.end());auto b=execute({f},"known_down",1);s.add("M01_region_order","metamorphic",b,{check("eligibility_invariant",sameEligibility(a,b),true)});}
 {auto f=standard(hold(320,530),true,{line(320,470),line(320,500,0,500,2)});auto a=execute({f});std::reverse(f.lines.begin(),f.lines.end());auto b=execute({f});s.add("M02_line_order_ambiguous","metamorphic",b,{check("eligibility_invariant",sameEligibility(a,b),true),check("move",b.c.move,false)});s.cases.back()["diagnostic_hit_equal"]=a.o.parts[0].hit.x==b.o.parts[0].hit.x&&a.o.parts[0].hit.y==b.o.parts[0].hit.y;}
 {auto f=standard(hold(),true,{line(),line(320,350,0,500,2)});auto a=execute({f});std::reverse(f.lines.begin(),f.lines.end());auto b=execute({f});s.add("M03_line_order_unique","metamorphic",b,{check("eligibility_invariant",sameEligibility(a,b),true),check("move",b.c.move,true)});}
 {auto f=standard();auto r=execute({f,f,f});s.add("S01_stationary_known_down","legal_action_opportunity",r,{check("move",r.c.move,true),check("independent_eq_1",r.o.relations[0].independent==1,true)});}
 {auto f=standard();auto r=execute({f,f,f},"never_executed");s.add("S02_stationary_never_executed","safe_denial",r,{check("down",r.c.down[0],false),check("independent_eq_1",r.o.relations[0].independent==1,true)});}
 {auto a=standard(),b=a,c=a;b.background_tag=1;c.background_tag=2;auto r=execute({a,b,c},"never_executed");s.add("S03_background_only_change","safe_denial",r,{check("down",r.c.down[0],false),check("independent_eq_1",r.o.relations[0].independent==1,true)});}
 {auto r=execute({standard(hold(320,490)),standard(hold(320,500)),standard(hold(320,504))},"never_executed");s.add("S04_three_distinct_endpoints","legal_action_opportunity",r,{check("down",r.c.down[0],true),check("independent_eq_3",r.o.relations[0].independent==3,true)});}
 {Query q=hold(320,500,8);q.tap=true;auto a=standard(q,false,{line(320,470)}),b=standard(q,false,{line(320,485)}),c=standard(q,false,{line(320,500)});s.add("D01_stationary_tap_moving_line","decision_only",execute({a,b,c},"never_executed"),{});}
 s.add("D02_short_visible_depth_6","decision_only",execute({standard(hold(320,504,6))}),{});
 s.add("D03_oblique_contact","decision_only",execute({standard(hold(316,505,200,.25),true,{line(320,500,.4)})}),{});
 {auto f=standard();auto a=execute({f}),b=execute({f});s.add("D04_identical_observable_worlds","decision_only",b,{});s.cases.back()["observational_output_equal"]=describe(a)==describe(b);s.cases.back()["physical_owner"]=nullptr;}
 // Addendum controls are separately frozen and never alter the original 31 cases.
 {auto r=execute({standard(hold(320,500)),standard()});s.add("G01_single_legal_continuation","merge_action_control",r,{check("ambiguous",r.o.relations[0].ambiguous,false),check("move",r.c.move,true)});}
 {Frame a,b;for(auto q:{hold(320,410,60),hold(320,500,60)}){a.bodies.push_back({q,true,true});a.queries.push_back(q);}for(auto q:{hold(320,414,60),hold(320,504,60)}){b.bodies.push_back({q,true,true});b.queries.push_back(q);}a.lines=b.lines={line(320,400),line(320,500,0,500,2)};auto r=execute({a,b},"known_down",1);s.add("G02_two_holds_still_separate","merge_action_control",r,{check("any_ambiguous",r.o.relations[0].ambiguous||r.o.relations[1].ambiguous,false),check("move",r.c.move,true)});}
 {Frame before;for(auto q:{hold(320,410,60),hold(320,500,60)}){before.bodies.push_back({q,true,true});before.queries.push_back(q);}before.lines={line()};auto r=execute({before,standard(hold(320,504,80))});s.add("G03_union_contains_one_prior_endpoint","merge_action_control",r,{check("ambiguous",r.o.relations[0].ambiguous,false),check("move",r.c.move,true)});}
 {auto before=standard(hold(320,500));before.queries.push_back(before.queries[0]);auto r=execute({before,standard()});s.add("G04_duplicate_prior_ROI","duplicate_representation_control",r,{check("ambiguous",r.o.relations[0].ambiguous,false),check("move",r.c.move,true)});}
 json totals=json::object();for(auto&[kind,t]:s.totals)totals[kind]={{"cases",t[0]},{"scored_cases",t[1]},{"passed_cases",t[2]},{"failed_cases",t[1]-t[2]}};
 return {{"schema","bvi-independent-review-result-v1"},{"renderer","independent C++20 integer-center rasterizer; no candidate/oracle renderer helpers"},{"expectation_file","expectations-frozen.json"},{"case_count",s.cases.size()},{"assertions",s.assertions},{"failed_assertions",s.failures},{"denominators",totals},{"cases",s.cases}};
}
}
int main(int argc,char**argv){auto report=review::run();if(argc==2){std::ofstream out(argv[1]);if(!out)return 2;out<<report.dump(2)<<'\n';if(!out)return 2;}else std::cout<<report.dump(2)<<'\n';return report["failed_assertions"].get<int>()?1:0;}
