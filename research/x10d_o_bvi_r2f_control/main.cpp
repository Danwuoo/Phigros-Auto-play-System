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
Guard guard(const J&g){std::string_view e=g["execution"].get_ref<const std::string&>();return {g["now_ns"],g["gate_deadline_ns"],g["plan_deadline_ns"],g["last_contact_ns"],e,g["prefix"],g["contact_id"],g["attachment_query"],g["free_contacts"],g.value("receipt_unknown",e=="unknown_down")};}
J observation(const Observation&o,const Candidate&c,const Constraints&g){J j={{"count",o.count},{"invalid",o.invalid},{"context_invalid",o.context_invalid},{"reason",o.reason},{"probes",o.probes},{"samples",c.samples()},{"window_span_ns",c.window_span()},{"physical","unknown"},{"exclusive_owner",nullptr},{"move",g.move},{"refresh",g.refresh},{"release",g.release},{"contact_id",g.contact_id},{"lifecycle_invalid",g.invalid},{"completed_persists",g.completed},{"completed_from_geometry",false},{"geometry_only_completion",false},{"parts",J::array()}};
 for(std::size_t i=0;i<o.count;++i){const auto&d=o.parts[i];const auto&r=o.relations[i];j["parts"].push_back({{"front",{d.q.front.x,d.q.front.y}},{"body",d.body==Support::supported},{"contact",name(d.contact)},{"cap",d.front_end},{"rear_cap",d.rear_end},{"depth",d.measured_depth},{"rear",{d.rear.x,d.rear.y}},{"effect",d.effect},{"association",d.line_unique?"unique":"ambiguous"},{"line_id",d.line_id},{"hit",{d.hit.x,d.hit.y}},{"relation",r.invalid?"invalid":r.ambiguous?"ambiguous":r.alternatives?"continued":"unconfirmed"},{"alternatives",r.alternatives},{"independent",r.independent},{"first_independent_ns",r.first_independent},{"last_independent_ns",r.last_independent},{"freshness_ns",r.freshness},{"span_ns",r.span},{"usable",r.usable},{"down",g.down[i]}});}
 return j;
}
Observation typed(const J&f){Observation o;auto k=key(f);auto ls=lines(f);o.count=std::min<std::size_t>(f["descriptors"].size(),128);
 for(std::size_t i=0;i<o.count;++i){const auto&t=f["descriptors"][i];auto&d=o.parts[i];d.q=query(t["query"]);d.key=k;d.body=t["body"].get<bool>()?Support::supported:Support::absent;d.front_end=t["front_end"];d.rear_end=t["rear_end"];d.measured_depth=number(t["measured_depth"]);d.signature=std::stoull(t["descriptor_signature"].get<std::string>());d.rgb_signature=std::stoull(t["rgb_signature"].get<std::string>());if(!t.contains("rails_provenance")||t["rails_provenance"].get<std::string>().empty())throw std::runtime_error("typed rails provenance missing");d.left=t.at("left");d.right=t.at("right");d.rear={d.q.front.x+std::sin(d.q.angle)*d.q.depth,d.q.front.y-std::cos(d.q.angle)*d.q.depth};std::string mode=t["contact_measurement"];if(mode=="occluded"){d.contact=Support::occluded;continue;}if(mode=="absent"){d.contact=Support::absent;continue;}std::size_t n=0;
  for(const auto&l:ls){auto normal=nv(l.angle),u=tu(l.angle),v=nv(d.q.angle);double den=dot(v,normal);if(std::abs(den)<1e-6)continue;double off=-dot(sub(d.q.front,l.center),normal)/den;Point hit{d.q.front.x+v.x*off,d.q.front.y+v.y*off};if((d.q.tap?std::abs(off)<=4:(off<=4&&off>=-d.q.depth))&&std::abs(dot(sub(hit,l.center),u))<=l.length/2&&(d.body==Support::supported||d.q.tap)){n++;d.hit=hit;d.line_id=l.id;}}
  d.line_unique=n==1;d.contact=n?Support::supported:Support::absent;
 }
 if(!k.source_valid||!capacity_valid(f["descriptors"].size(),ls.size(),Candidate::metadata_bytes(),0))o.invalid=true;
 if(f.contains("measured_frame_byte_count")&&f["measured_frame_byte_count"]!=f["expected_frame_byte_count"])o.invalid=true;
 if(f.contains("declared_usage")&&!f["declared_usage"].is_null()){auto b=f["declared_usage"];o.invalid|=!capacity_valid(b.value("regions",1),b.value("lines",1),b.value("metadata",0),b.value("probes",0));}
 for(const auto&t:f["descriptors"])if(!std::isfinite(number(t["query"]["width"])))o.invalid=true;
 return o;
}
J normalized(J j){J parts=J::array();for(auto p:j["parts"]){p.erase("line_id");parts.push_back(p);}std::sort(parts.begin(),parts.end(),[](const J&a,const J&b){return a["front"]<b["front"];});return {{"parts",parts},{"move",j["move"]},{"refresh",j["refresh"]},{"release",j["release"]},{"contact_id",j["contact_id"]},{"invalid",j["lifecycle_invalid"]},{"physical",j["physical"]}};}
J read(const std::filesystem::path&p){std::ifstream in(p);if(!in)throw std::runtime_error("input missing");return J::parse(in);}
void save(const std::filesystem::path&p,const J&j){auto reservation=read(p);if(reservation.value("attempt",std::string{})!="bvi-r2f-20261005-03"||reservation.value("pending",false)!=true)throw std::runtime_error("report reservation invalid");auto s=j.dump(2)+"\n";if(s.size()>4194304)throw std::runtime_error("report bound");std::ofstream out(p,std::ios::trunc);out<<s;out.flush();if(!out)throw std::runtime_error("report write");}
}
#include "driver.inc"
