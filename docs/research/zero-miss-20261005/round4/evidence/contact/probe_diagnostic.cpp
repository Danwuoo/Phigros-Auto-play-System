// Frozen renderer prologue copied verbatim from research/x10d_o_bvi_build/main.cpp.
#include "bvi.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
using J=nlohmann::json;using namespace bvi;
namespace {
double number(const J&j){if(j.is_number())return j.get<double>();return j=="nan"?std::numeric_limits<double>::quiet_NaN():std::numeric_limits<double>::infinity();}
Point point(const J&j){return {number(j[0]),number(j[1])};}
Query query(const J&j){return {point(j["front"]),number(j["width"]),number(j["depth"]),number(j["angle"]),j.value("kind","")=="tap"};}
Key key(const J&f){const auto&c=f["context"];Ns t=f["time_ns"];return {{c["epoch"],c["generation"],c["geometry"],c["rotation"]},t,t,f["sequence"],f["source_valid"]};}
std::vector<Line> lines(const J&f){std::vector<Line>a;for(const auto&l:f["lines"])a.push_back({point(l["center"]),number(l["angle"]),number(l["length"]),l["id"]});return a;}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}Point tu(double a){return {std::cos(a),std::sin(a)};}Point nv(double a){return {-std::sin(a),std::cos(a)};}
// Independent declarative renderer. It has no candidate header helpers, masks or predicted support.
std::vector<std::uint8_t> render(const J&f){int w=f["width"],h=f["height"],stride=f["stride"];std::vector<std::uint8_t>rgb(static_cast<std::size_t>(stride)*h);
 auto set=[&](int x,int y,int r,int g,int b){auto i=static_cast<std::size_t>(y)*stride+x*3;rgb[i]=static_cast<std::uint8_t>(r);rgb[i+1]=static_cast<std::uint8_t>(g);rgb[i+2]=static_cast<std::uint8_t>(b);};
 for(const auto&s:f["shapes"]){auto q=query(s);auto u=tu(q.angle),v=nv(q.angle);for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto d=sub({double(x),double(y)},q.front);double along=dot(d,u),normal=dot(d,v);bool inside=std::abs(along)<=q.width/2&&(q.tap?std::abs(normal)<=q.depth/2:(normal>=-q.depth&&normal<=0));if(inside)set(x,y,40,190,255);}}
 for(const auto&s:f["shapes"]){auto q=query(s);if(!s.value("rails",false)||q.tap)continue;auto u=tu(q.angle),v=nv(q.angle);for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto d=sub({double(x),double(y)},q.front);if(std::abs(std::abs(dot(d,u))-q.width/2)<=1.5&&dot(d,v)>=-q.depth&&dot(d,v)<=0)set(x,y,255,255,255);}}
 for(const auto&l:lines(f)){auto u=tu(l.angle),v=nv(l.angle);for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto d=sub({double(x),double(y)},l.center);if(std::abs(dot(d,v))<=2&&std::abs(dot(d,u))<=l.length/2)set(x,y,255,255,255);}}
 for(const auto&e:f["effects"]){std::vector<Point>poly;for(const auto&p:e["polygon"])poly.push_back(point(p));int a=e["rgba"][3];for(int y=0;y<h;++y)for(int x=0;x<w;++x){bool inside=false;for(std::size_t i=0,j=poly.size()-1;i<poly.size();j=i++){auto p=poly[i],q=poly[j];double cross=(x-p.x)*(q.y-p.y)-(y-p.y)*(q.x-p.x);if(std::abs(cross)<1e-9&&x>=std::min(p.x,q.x)&&x<=std::max(p.x,q.x)&&y>=std::min(p.y,q.y)&&y<=std::max(p.y,q.y)){inside=true;break;}if((p.y>y)!=(q.y>y)&&x<(q.x-p.x)*(y-p.y)/(q.y-p.y)+p.x)inside=!inside;}if(inside){auto k=static_cast<std::size_t>(y)*stride+x*3;for(int c=0;c<3;++c)rgb[k+c]=static_cast<std::uint8_t>((e["rgba"][c].get<int>()*a+rgb[k+c]*(255-a)+127)/255);}}}
 for(const auto&s:f["clear"])for(int y=std::max(0,s["y"][0].get<int>());y<std::min(h,s["y"][1].get<int>());++y)for(int x=std::max(0,s["x"][0].get<int>());x<std::min(w,s["x"][1].get<int>());++x)set(x,y,0,0,0);
 int delta=f.value("rgb_byte_delta",0);if(delta<0)rgb.resize(rgb.size()+delta);return rgb;
}
}
#include "contact_policy.hpp"
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream in(argv[1]);J root=J::parse(in);J report={{"schema","contact-probe-diagnostic-v1"},{"case","V10-rotation"},{"source","original normalized-execution.json with unchanged historical renderer"}};
for(const auto&c:root["cases"]){if(c["name"]!="V10-rotation")continue;const auto&f=c["frames"].back();auto rgb=render(f);auto q=query(f["queries"][0]);auto l=lines(f)[0];auto vn=nv(q.angle),un=tu(q.angle),ln=nv(l.angle);double den=dot(vn,ln);double depth=-dot(sub(q.front,l.center),ln)/den;Point hit{q.front.x+depth*vn.x,q.front.y+depth*vn.y};report["hit"]={hit.x,hit.y};report["before"]=J::array();report["after"]=J::array();
for(bool newer:{false,true})for(int sign:{-1,1})for(int delta:{4,6})for(int side:{-1,1}){Point anchor{hit.x+side*.35*q.width*un.x,hit.y+side*.35*q.width*un.y};if(newer){double shift=-dot(sub(anchor,l.center),ln)/den;anchor.x+=shift*vn.x;anchor.y+=shift*vn.y;}Point p{anchor.x+sign*delta*ln.x,anchor.y+sign*delta*ln.y};int x=static_cast<int>(std::llround(p.x)),y=static_cast<int>(std::llround(p.y));auto i=static_cast<std::size_t>(y)*f["stride"].get<int>()+x*3;int r=rgb[i],g=rgb[i+1],b=rgb[i+2];auto d=sub(p,q.front);auto pc=Point{double(x),double(y)};auto dc=sub(pc,q.front);bool inside=dot(d,vn)<=1e-6&&dot(d,vn)>=-q.depth-1e-6&&std::abs(dot(d,un))<=q.width/2+1e-6&&dot(dc,vn)<=1e-6&&dot(dc,vn)>=-q.depth-1e-6&&std::abs(dot(dc,un))<=q.width/2+1e-6;report[newer?"after":"before"].push_back({{"sign",sign},{"delta",delta},{"flank",side},{"point",{p.x,p.y}},{"pixel",{x,y}},{"rgb",{r,g,b}},{"body_continuous_and_pixel",inside},{"line_normal_distance",dot(sub(pc,l.center),ln)},{"blue_predicate",r>=20&&r<=100&&g>=130&&g<=230&&b>=180&&b<=255&&b>=g+20}});}
std::uint64_t probes=0;auto result=sample_current_contact({rgb,f["width"],f["height"],f["stride"],key(f)},q,l,hit,probes);report["candidate"]={{"supported",result.supported},{"yellow",result.yellow},{"probes",probes}};
}
report["V04_alpha89"]={{"source_rgba",{255,225,80,89}},{"background_rgb",{40,190,255}},{"observed_rgb",{(255*89+40*166+127)/255,(225*89+190*166+127)/255,(80*89+255*166+127)/255}},{"yellow_predicate_visible_v1",false},{"original_effect_oracle_unchanged",true}};std::cout<<report.dump(2)<<'\n';return report["candidate"]["supported"].get<bool>()?0:1;}
