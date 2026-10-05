// C-V3-CONTACT-1 frozen synthetic controls. No renderer labels reach sampler.
// CONTACT_BASELINE tests a literal copy of the donor contact subpredicate.
// CONTACT_INTEGRATION additionally exercises the integrated candidate/constraints.
#include "contact_policy.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace bvi;
using J=nlohmann::json;
namespace {
constexpr int W=640,H=640;
constexpr std::uint64_t Budget=6291456;
constexpr double Pi=3.14159265358979323846;
Point u(double a){return {std::cos(a),std::sin(a)};}
Point v(double a){return {-std::sin(a),std::cos(a)};}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y};}
Point mul(Point a,double k){return {a.x*k,a.y*k};}
Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
Point world(const Query&q,double s,double d){return add(q.front,add(mul(u(q.angle),s),mul(v(q.angle),d)));}
Point hit(const Query&q,const Line&l){const auto vn=v(q.angle),ln=v(l.angle);const auto den=dot(vn,ln);if(std::abs(den)<1e-6)return q.front;return add(q.front,mul(vn,-dot(sub(q.front,l.center),ln)/den));}
Query hold(double x=320,double y=520,double depth=100,double angle=0){return {{x,y},140,depth,angle,false};}
Line line(double x=320,double y=500,double angle=0,double length=500,std::uint64_t id=1){return {{x,y},angle,length,id};}
struct Pixel {int r{},g{},b{};bool in{};bool blue()const{return in&&r>=20&&r<=100&&g>=130&&g<=230&&b>=180&&b<=255&&b>=g+20;}bool yellow()const{return in&&r>=180&&g>=120&&b<=180;}};
Pixel pixel(const View&f,Point p,std::uint64_t&n){++n;if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<=-.5||p.y<=-.5||p.x>=f.width-.5||p.y>=f.height-.5)return {};const auto x=static_cast<int>(std::llround(p.x)),y=static_cast<int>(std::llround(p.y));auto i=static_cast<std::size_t>(y)*f.stride+x*3;return {f.rgb[i],f.rgb[i+1],f.rgb[i+2],true};}
ContactSample old_sample(const View&f,const Query&q,const Line&l,Point h,std::uint64_t&n){
 ContactSample out;const auto ln=v(l.angle),tu=u(q.angle);
 for(int sign:{-1,1}){bool ok=true;for(int delta:q.tap?std::array<int,2>{3,3}:std::array<int,2>{4,6}){const auto cp=add(h,mul(ln,sign*delta));const auto a=pixel(f,add(cp,mul(tu,-.35*q.width)),n),b=pixel(f,add(cp,mul(tu,.35*q.width)),n);out.yellow|=a.yellow()||b.yellow();ok&=a.blue()&&b.blue();}out.supported|=ok;}
 return out;
}
ContactSample selected(const View&f,const Query&q,const Line&l,Point h,std::uint64_t&n){
#ifdef CONTACT_BASELINE
 return old_sample(f,q,l,h,n);
#else
 return sample_current_contact(f,q,l,h,n);
#endif
}
struct Test {
 std::string id,kind;
 Query q=hold();Line l=line();
 std::string pixels="render";
 bool expect=false,yellow=false,rails=true;
 Point override_hit{};bool replace_hit=false;
 std::uint64_t start_probes=0;
};
std::vector<std::uint8_t> render(const Test&t,const std::vector<Line>&ls={}){
 std::vector<std::uint8_t>rgb(static_cast<std::size_t>(W)*H*3);
 const auto lines=ls.empty()?std::vector<Line>{t.l}:ls;
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){const Point p{double(x),double(y)};const auto d=sub(p,t.q.front);const double s=dot(d,u(t.q.angle)),n=dot(d,v(t.q.angle));
  std::array<int,3>c{};const bool body=std::abs(s)<=t.q.width/2&&(t.q.tap?std::abs(n)<=t.q.depth/2:n>=-t.q.depth&&n<=0);
  if(body)c={40,190,255};
  if(!t.q.tap&&t.rails&&n>=-t.q.depth&&n<=0&&std::abs(std::abs(s)-t.q.width/2)<=1.5)c={255,255,255};
  for(const auto&l:lines)if(std::abs(dot(sub(p,l.center),v(l.angle)))<=2&&std::abs(dot(sub(p,l.center),u(l.angle)))<=l.length/2)c={255,255,255};
  const double ln=dot(sub(p,t.l.center),v(t.l.angle));
  if(t.pixels=="black")c={0,0,0};
  if(t.pixels=="white")c={255,255,255};
  if(t.pixels=="yellow")c={255,225,80};
  if(t.pixels=="blue")c={40,190,255};
  if(t.pixels=="left_missing"&&s<0)c={0,0,0};
  if(t.pixels=="mixed_sides"&&((s<0&&ln<0)||(s>0&&ln>0)))c={0,0,0};
  if(t.pixels=="delta6_missing"&&std::abs(ln)>5)c={0,0,0};
  if(t.pixels=="white_band"&&std::abs(ln)<=8)c={255,255,255};
  if(t.pixels=="yellow_band"&&std::abs(ln)<=8)c={255,225,80};
  if(t.pixels=="alpha89")c={115,202,194};
  const auto at=(static_cast<std::size_t>(y)*W+x)*3;for(int z=0;z<3;++z)rgb[at+z]=static_cast<std::uint8_t>(c[z]);
 }
 return rgb;
}
std::vector<Test> controls(){
 std::vector<Test>a;
 auto addcase=[&](std::string id,std::string kind,Query q,Line l,bool supported,std::string pixels="render",bool yellow=false)->Test&{a.push_back({std::move(id),std::move(kind),q,l,std::move(pixels),supported,yellow});return a.back();};
 addcase("P01_axis_aligned","legal_current_support",hold(),line(),true);
 {auto q=hold(320,520,150,.6);auto h=world(q,0,-30);addcase("P02_co_rotation","legal_current_support",q,line(h.x,h.y,.6),true);}
 addcase("P03_frozen_V10_geometry","legal_current_support",hold(316,505,200,.25),line(320,500,.4),true);
 addcase("P04_mirrored_oblique","legal_current_support",hold(324,505,200,-.25),line(320,500,-.4),true);
 addcase("P05_translated_oblique","legal_current_support",hold(263,452,200,.25),line(267,447,.4),true);
 {auto q=hold(320,520,180,.4);auto h=world(q,0,-75);addcase("P06_deep_oblique","legal_current_support",q,line(h.x,h.y,.8),true);}
 addcase("P07_short_visible_body16","legal_current_support",hold(320,504,16),line(),true);
 {auto q=hold(320,520,240,.2);auto h=world(q,0,-110);addcase("P08_large_obliqueness","legal_current_support",q,line(h.x,h.y,.9),true);}
 for(const auto&mode:std::vector<std::string>{"black","white","yellow","left_missing","mixed_sides","delta6_missing","white_band","yellow_band","alpha89"})addcase("N_pixel_"+mode,"missing_current_support",hold(),line(),false,mode,mode=="yellow"||mode=="yellow_band");
 addcase("N10_short_body2","missing_current_support",hold(320,501,2),line(),false);
 addcase("N11_parallel","bounded_geometry",hold(),line(320,500,Pi/2),false,"blue");
 addcase("N12_near_parallel","bounded_geometry",hold(320,520,200),line(320,480,Pi/2-.001),false,"blue");
 addcase("N13_clipped_flank","bounded_geometry",hold(20,520),line(20,500),false,"blue");
 addcase("N14_hit_outside_body","bounded_geometry",hold(320,490),line(),false,"blue");
 addcase("N15_hit_outside_segment","bounded_geometry",hold(),line(0,500,0,50),false,"blue");
 addcase("N16_flanks_outside_segment","bounded_geometry",hold(),line(320,500,0,40),false,"blue");
 auto&t17=addcase("N17_hit_off_line","bounded_geometry",hold(),line(),false,"blue");t17.replace_hit=true;t17.override_hit={320,498};
 auto&t18=addcase("N18_hit_off_axis","bounded_geometry",hold(),line(),false,"blue");t18.replace_hit=true;t18.override_hit={325,500};
 auto&t19=addcase("N19_budget_exhaustion","bounded_resource",hold(),line(),false);t19.start_probes=Budget-2;
 return a;
}
struct Recorder {J rows=J::array();int assertions=0,failures=0;void check(J&r,std::string field,J expected,J actual){bool pass=expected==actual;r["checks"].push_back({{"field",field},{"expected",expected},{"actual",actual},{"pass",pass}});++assertions;if(!pass)++failures;}void push(J r){rows.push_back(std::move(r));}};
J expectations(){J a=J::array();for(const auto&t:controls())a.push_back({{"id",t.id},{"class",t.kind},{"supported",t.expect},{"yellow",t.yellow},{"max_probes",8}});return {{"schema","C-V3-CONTACT-1-controls"},{"frozen_before_implementation",true},{"controls",a},{"tap_parity","8 valid-input variants, old literal subpredicate equality"},{"integration","unique-oblique, no-rails, unknown-down, completed-up, two-lines in both orders"},{"invalid_inputs","invalid span/source/dimensions/stride, nonfinite query/line/hit, oversized query; fail-closed without read"},{"V04","original yellow predicate remains false on RGB(115,202,194); original oracle unchanged"}};}
void direct_tests(Recorder&r){for(const auto&t:controls()){auto rgb=render(t);View f{rgb,W,H,W*3,{}};std::uint64_t probes=t.start_probes;const auto h=t.replace_hit?t.override_hit:hit(t.q,t.l);const auto got=selected(f,t.q,t.l,h,probes);J row={{"id",t.id},{"class",t.kind},{"checks",J::array()},{"actual",{{"supported",got.supported},{"yellow",got.yellow},{"probes",probes-t.start_probes},{"hit",{h.x,h.y}}}}};r.check(row,"supported",t.expect,got.supported);r.check(row,"yellow",t.yellow,got.yellow);r.check(row,"max8",true,probes-t.start_probes<=8);if(t.start_probes)r.check(row,"budget_overflow_reported",Budget+1,probes);r.push(std::move(row));}}
void tap_tests(Recorder&r){for(int k=0;k<8;++k){Test t;t.q={{320,400},140,8,.1*k,true};t.l=line(320,400,.12*k);if(k==4)t.pixels="black";if(k==5)t.pixels="white";if(k==6)t.pixels="yellow";if(k==7)t.pixels="alpha89";auto rgb=render(t);View f{rgb,W,H,W*3,{}};std::uint64_t oldn=0,n=0;auto old=old_sample(f,t.q,t.l,t.q.front,oldn),got=selected(f,t.q,t.l,t.q.front,n);J row={{"id","TAP-parity-"+std::to_string(k)},{"class","tap_unchanged"},{"checks",J::array()}};r.check(row,"supported",old.supported,got.supported);r.check(row,"yellow",old.yellow,got.yellow);r.check(row,"probes",oldn,n);r.push(std::move(row));}}
void invalid_tests(Recorder&r){
#ifndef CONTACT_BASELINE
 const std::vector<std::string>modes={"short_span","bad_source","negative_width","too_large_height","bad_stride","nan_query","inf_line","nan_hit","huge_hit","huge_query","oversized_width"};
 for(const auto&m:modes){Test t;auto rgb=render(t);View f{rgb,W,H,W*3,{}};auto q=t.q;auto l=t.l;auto h=hit(q,l);if(m=="short_span")f.rgb=f.rgb.first(1);if(m=="bad_source")f.key.source_valid=false;if(m=="negative_width")f.width=-1;if(m=="too_large_height")f.height=721;if(m=="bad_stride")f.stride=1;if(m=="nan_query")q.angle=std::numeric_limits<double>::quiet_NaN();if(m=="inf_line")l.center.x=std::numeric_limits<double>::infinity();if(m=="nan_hit")h.x=std::numeric_limits<double>::quiet_NaN();if(m=="huge_hit")h.x=1e308;if(m=="huge_query")q.front={1e308,1e308};if(m=="oversized_width")q.width=4097;std::uint64_t n=0;auto got=selected(f,q,l,h,n);J row={{"id","INVALID-"+m},{"class","invalid_input"},{"checks",J::array()}};r.check(row,"supported",false,got.supported);r.check(row,"yellow",false,got.yellow);r.check(row,"no_read",0,n);r.push(std::move(row));}
#else
 (void)r; // Unsafe donor preconditions are deliberately not violated.
#endif
}
#ifdef CONTACT_INTEGRATION
void integration_tests(Recorder&r){for(const auto&mode:std::vector<std::string>{"unique_oblique","no_rails","unknown_down","completed_up","two_lines","two_lines_reverse"}){Test t;t.q=hold(316,505,200,.25);t.l=line(320,500,.4);t.rails=mode!="no_rails";std::vector<Line>ls{t.l};if(mode.starts_with("two_lines")){t.q=hold(320,550,240,.25);ls.push_back(line(320,470,.4,500,2));if(mode=="two_lines_reverse")std::reverse(ls.begin(),ls.end());}auto rgb=render(t,ls);Key k;k.capture=k.ready=100000000;k.sequence=1;Candidate c;const std::array<Query,1>qs{t.q};auto o=c.extract({rgb,W,H,W*3,k},qs,ls);c.relate(o,k,101000000);Guard g;g.now=101000000;g.gate_deadline=g.plan_deadline=200000000;g.last_contact=100000000;g.execution=mode=="unknown_down"?"unknown_down":mode=="completed_up"?"completed_up":"known_down";g.prefix=mode=="completed_up"?2:1;g.receipt_unknown=mode=="unknown_down";g.contact_id=3;g.attachment_query=0;auto action=constrain(o,g);J row={{"id","INTEGRATION-"+mode},{"class","action_gate"},{"checks",J::array()},{"actual",{{"contact",name(o.parts[0].contact)},{"body",name(o.parts[0].body)},{"line_unique",o.parts[0].line_unique},{"move",action.move},{"refresh",action.refresh},{"probes",o.probes}}}};const bool good=mode=="unique_oblique";r.check(row,"move",good,action.move);r.check(row,"refresh",good,action.refresh);r.check(row,"down_false",false,action.down[0]);r.check(row,"contact_id",3,action.contact_id);if(mode.starts_with("two_lines"))r.check(row,"line_unique",false,o.parts[0].line_unique);if(good){r.check(row,"supported",true,o.parts[0].contact==Support::supported);r.check(row,"line_unique",true,o.parts[0].line_unique);}r.push(std::move(row));}}
#endif
J run(){Recorder r;direct_tests(r);tap_tests(r);invalid_tests(r);
#ifdef CONTACT_INTEGRATION
 integration_tests(r);
#endif
 J den=J::object();for(const auto&row:r.rows){const auto kind=row["class"].get<std::string>();if(!den.contains(kind))den[kind]={{"cases",0},{"failed_cases",0}};den[kind]["cases"]=den[kind]["cases"].get<int>()+1;bool bad=false;for(const auto&c:row["checks"])bad|=!c["pass"].get<bool>();if(bad)den[kind]["failed_cases"]=den[kind]["failed_cases"].get<int>()+1;}return {{"schema","contact-policy-result-v1"},{"contract","C-V3-CONTACT-1"},{"physical_human_gold",0},{"touch_backend",false},{"assertions",r.assertions},{"failed_assertions",r.failures},{"cases",r.rows},{"denominators",den}};}
}
int main(int argc,char**argv){if(argc==2&&std::string(argv[1])=="--freeze"){std::cout<<expectations().dump(2)<<'\n';return 0;}auto j=run();if(argc==2){std::ofstream out(argv[1]);if(!out)return 2;out<<j.dump(2)<<'\n';if(!out)return 2;}else std::cout<<j.dump(2)<<'\n';return j["failed_assertions"].get<int>()?1:0;}
