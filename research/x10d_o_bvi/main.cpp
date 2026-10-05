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
J observation(const Observation&o,const Candidate&c,const Constraints&g){J j={{"count",o.count},{"invalid",o.invalid},{"context_invalid",o.context_invalid},{"reason",o.reason},{"probes",o.probes},{"samples",c.samples()},{"physical","unknown"},{"exclusive_owner",nullptr},{"move",g.move},{"refresh",g.refresh},{"release",g.release},{"lifecycle_invalid",g.invalid},{"completed_persists",g.completed},{"completed_from_geometry",false},{"geometry_only_completion",false},{"parts",J::array()}};
 for(std::size_t i=0;i<o.count;++i){const auto&d=o.parts[i];const auto&r=o.relations[i];j["parts"].push_back({{"front",{d.q.front.x,d.q.front.y}},{"body",d.body==Support::supported},{"contact",name(d.contact)},{"cap",d.front_end},{"rear_cap",d.rear_end},{"depth",d.measured_depth},{"rear",{d.rear.x,d.rear.y}},{"effect",d.effect},{"association",d.line_unique?"unique":"ambiguous"},{"line_id",d.line_id},{"hit",{d.hit.x,d.hit.y}},{"relation",r.invalid?"invalid":r.ambiguous?"ambiguous":r.alternatives?"continued":"unconfirmed"},{"alternatives",r.alternatives},{"independent",r.independent},{"span_ns",r.span},{"usable",r.usable},{"down",g.down[i]}});}
 return j;
}
Observation typed(const J&f){Observation o;auto k=key(f);auto ls=lines(f);o.count=std::min<std::size_t>(f["descriptors"].size(),128);
 for(std::size_t i=0;i<o.count;++i){const auto&t=f["descriptors"][i];auto&d=o.parts[i];d.q=query(t["query"]);d.key=k;d.body=t["body"].get<bool>()?Support::supported:Support::absent;d.front_end=t["front_end"];d.rear_end=t["rear_end"];d.measured_depth=number(t["measured_depth"]);d.signature=std::stoull(t["descriptor_signature"].get<std::string>());d.rgb_signature=std::stoull(t["rgb_signature"].get<std::string>());d.rear={d.q.front.x+std::sin(d.q.angle)*d.q.depth,d.q.front.y-std::cos(d.q.angle)*d.q.depth};std::string mode=t["contact_measurement"];if(mode=="occluded"){d.contact=Support::occluded;continue;}if(mode=="absent"){d.contact=Support::absent;continue;}std::size_t n=0;
  for(const auto&l:ls){auto normal=nv(l.angle),u=tu(l.angle),v=nv(d.q.angle);double den=dot(v,normal);if(std::abs(den)<1e-6)continue;double off=-dot(sub(d.q.front,l.center),normal)/den;Point hit{d.q.front.x+v.x*off,d.q.front.y+v.y*off};if((d.q.tap?std::abs(off)<=4:(off<=4&&off>=-d.q.depth))&&std::abs(dot(sub(hit,l.center),u))<=l.length/2&&(d.body==Support::supported||d.q.tap)){n++;d.hit=hit;d.line_id=l.id;}}
  d.line_unique=n==1;d.contact=n?Support::supported:Support::absent;
 }
 if(!k.source_valid||!capacity_valid(f["descriptors"].size(),ls.size(),Candidate::metadata_bytes(),0))o.invalid=true;
 if(f.contains("measured_frame_byte_count")&&f["measured_frame_byte_count"]!=f["expected_frame_byte_count"])o.invalid=true;
 if(f.contains("declared_usage")&&!f["declared_usage"].is_null()){auto b=f["declared_usage"];o.invalid|=!capacity_valid(b.value("regions",1),b.value("lines",1),b.value("metadata",0),b.value("probes",0));}
 for(const auto&t:f["descriptors"])if(!std::isfinite(number(t["query"]["width"])))o.invalid=true;
 return o;
}
J normalized(J j){J parts=J::array();for(auto p:j["parts"]){p.erase("line_id");parts.push_back(p);}std::sort(parts.begin(),parts.end(),[](const J&a,const J&b){return a["front"]<b["front"];});return {{"parts",parts},{"move",j["move"]},{"refresh",j["refresh"]},{"release",j["release"]},{"invalid",j["lifecycle_invalid"]},{"physical",j["physical"]}};}
J read(const std::filesystem::path&p){std::ifstream in(p);if(!in)throw std::runtime_error("input missing");return J::parse(in);}
void save(const std::filesystem::path&p,const J&j){if(std::filesystem::exists(p))throw std::runtime_error("output exists");auto s=j.dump(2)+"\n";if(s.size()>4194304)throw std::runtime_error("report bound");std::ofstream out(p);out<<s;if(!out)throw std::runtime_error("report write");}
}
int main(int argc,char**argv){try{if(argc!=6){std::cerr<<"usage: bvi_tests normalized.json oracle.json typed-inputs.json supplemental.json output.json\n";return 2;}auto spec=read(argv[1]),oracle=read(argv[2]),ti=read(argv[3]),extra=read(argv[4]);if(spec["expanded_cases"]!=69||oracle["expanded_cases"]!=69||ti["cases"].size()!=69)throw std::runtime_error("fixture denominator");
 J report={{"schema","bvi.result.v1"},{"family","BVI-1"},{"top_vectors",20},{"expanded_cases",69},{"metadata_bytes",Candidate::metadata_bytes()},{"sizeof_descriptor",sizeof(Descriptor)},{"no_past_rgb",true},{"touch_backend",false},{"physical_human_gold",0},{"layers",J::object()},{"unverified_oracle_fields",J::array()}};
 std::map<std::string,J>rgbResults,typedResults;int failures=0,totalAssertions=0;
 // Strong same-RGB controls compare complete bytes, without feeding private recipe lists to Candidate.
 report["renderer_controls"]=J::array();for(auto pair:std::array<std::pair<std::string,std::string>,3>{{{"V00-whole","V00-touching"},{"V01-whole","V01-touching"},{"V19-two-physical","V19-decorative-countermodel"}}}){const J*a=nullptr,*b=nullptr;for(const auto&c:spec["cases"]){if(c["name"]==pair.first)a=&c;if(c["name"]==pair.second)b=&c;}if(!a||!b)throw std::runtime_error("pair missing");bool equal=true;for(std::size_t i=0;i<(*a)["frames"].size();++i){auto x=render((*a)["frames"][i]),y=render((*b)["frames"][i]);equal&=x==y;equal&=(*a)["frames"][i]["queries"]==(*b)["frames"][i]["queries"];equal&=(*a)["frames"][i]["lines"]==(*b)["frames"][i]["lines"];equal&=(*a)["frames"][i]["context"]==(*b)["frames"][i]["context"];}report["renderer_controls"].push_back({{"a",pair.first},{"b",pair.second},{"full_rgb_byte_equality",equal},{"frames",(*a)["frames"].size()},{"pass",equal}});totalAssertions++;if(!equal)failures++;}
 for(std::string layer:{"rgb","typed","lifecycle"}){J rows=J::array();int assertions=0,failed=0,checkedCases=0;
  for(std::size_t ci=0;ci<69;++ci){auto c=spec["cases"][ci];std::string cname=c["name"];const auto&e=oracle["cases"][ci]["expected"];if(cname!=oracle["cases"][ci]["name"]||cname!=ti["cases"][ci]["name"])throw std::runtime_error("fixture mapping");Candidate candidate;Observation o;Constraints cg;J trajectory=J::array();const auto&fs=layer=="rgb"?c["frames"]:ti["cases"][ci]["frames"];int rearPast=0;J rearValues=J::array();
   for(std::size_t fi=0;fi<fs.size();++fi){const auto&f=fs[fi];auto k=key(f);if(layer=="rgb"){auto pixels=render(f);std::vector<Query>qs;for(const auto&q:f["queries"])qs.push_back(query(q));auto ls=lines(f);o=candidate.extract({pixels,f["width"],f["height"],f["stride"],k},qs,ls);}else o=typed(f);auto g=guard(c["guard"]);if(fi+1<fs.size())g.now=k.capture+1000000;candidate.relate(o,k,g.now);cg=constrain(o,g);if(o.count&&o.parts[0].rear_end){auto ls=lines(f);if(!ls.empty()&&dot(sub(o.parts[0].rear,ls[0].center),nv(ls[0].angle))>0)rearPast++;}rearValues.push_back(o.count?o.parts[0].rear.y:0);auto j=observation(o,candidate,cg);j["up_geometry_opportunity"]=rearPast>=2;trajectory.push_back(j);}
   auto actual=trajectory.back();if(c["guard"].contains("witness_ns")&&!c["guard"]["witness_ns"].is_null())actual["witness_retained"]=candidate.retains(c["guard"]["witness_ns"]);J checks=J::array();
   auto check=[&](std::string field,J want,J got){bool pass=want==got;checks.push_back({{"field",field},{"expected",want},{"actual",got},{"pass",pass}});assertions++;if(!pass){failed++;failures++;}};
   for(auto it=e.begin();it!=e.end();++it){std::string f=it.key();const auto&w=it.value();bool measured=f=="body"||f=="contact"||f=="cap"||f=="effect"||f=="association"||f=="physical"||f=="line_id";bool relation=f=="relation"||f=="independent"||f=="alternatives"||f=="usable"||f=="max_samples"||f=="max_window_ns"||f=="witness_retained";bool life=f=="move"||f=="down"||f=="refresh"||f=="release"||f=="lifecycle_invalid"||f=="completed_persists"||f=="completed_from_geometry"||f=="geometry_only_completion"||f=="up_at_second"||f=="up_at_third";
    bool geometryCheck=f=="hit_line_error_max"||f=="hit_body_required"||f=="current_body_preserved";
    if(layer=="rgb"&&!measured&&!geometryCheck&&f!="equal_to"&&f!="equivalent_to")continue;if(layer=="typed"&&!measured&&!relation&&!geometryCheck&&f!="qualification_invalid"&&f!="equal_to"&&f!="equivalent_to"&&f!="rear"&&f!="past_line_count")continue;if(layer=="lifecycle"&&!life&&f!="qualification_invalid"&&f!="equal_to"&&f!="equivalent_to")continue;
    if(f=="body"||f=="contact"||f=="cap"||f=="relation"||f=="down"){J got=J::array();for(std::size_t i=0;i<w.size();++i)got.push_back(i<actual["parts"].size()?actual["parts"][i][f]:J(nullptr));check(f,w,got);}
    else if(f=="effect"){bool effect=false;for(const auto&p:actual["parts"])effect|=p["effect"].get<bool>();check(f,w,effect);}
    else if(f=="current_body_preserved"){if(w==true)check(f,true,actual["parts"][0]["body"]);}
    else if(f=="hit_line_error_max"||f=="hit_body_required"){auto d=o.parts[0];auto ls=lines(fs.back());double error=1e9;for(auto l:ls)if(l.id==d.line_id)error=std::abs(dot(sub(d.hit,l.center),nv(l.angle)));auto delta=sub(d.hit,d.q.front);bool in=std::abs(dot(delta,tu(d.q.angle)))<=d.q.width/2&&dot(delta,nv(d.q.angle))<=0&&dot(delta,nv(d.q.angle))>=-d.q.depth;if(f=="hit_line_error_max")check(f,true,error<=w.get<double>());else check(f,w,in&&d.contact==Support::supported);}
    else if(f=="qualification_invalid")check(f,w,actual["invalid"].get<bool>()||actual["context_invalid"].get<bool>()||actual["lifecycle_invalid"].get<bool>());
    else if(f=="association"||f=="line_id"||f=="independent"||f=="alternatives"||f=="usable")check(f,w,actual["parts"].empty()?J(nullptr):actual["parts"][0][f]);
    else if(f=="max_samples")check(f,true,candidate.samples()<=w.get<std::size_t>());
    else if(f=="max_window_ns")check(f,true,candidate.window_span()<=w.get<Ns>());
    else if(f=="rear")check(f,w,rearValues);
    else if(f=="past_line_count")check(f,w,rearPast);
    else if(f=="up_at_second"||f=="up_at_third")check(f,w,trajectory[f=="up_at_second"?1:2]["up_geometry_opportunity"]);
    else if((f=="equal_to"||f=="equivalent_to")&&!w.is_null()){auto&bank=layer=="rgb"?rgbResults:typedResults;auto name=w.get<std::string>();if(!bank.contains(name))throw std::runtime_error("countermodel reference missing");check(f,normalized(bank[name]),normalized(actual));}
    else if(actual.contains(f))check(f,w,actual[f]);
    else report["unverified_oracle_fields"].push_back({{"case",cname},{"field",f},{"layer",layer}});
   }
   if(layer=="rgb")rgbResults[cname]=actual;else if(layer=="typed")typedResults[cname]=actual;
   if(checks.size())checkedCases++;rows.push_back({{"case",cname},{"top",c["id"]},{"assertions",checks},{"actual",actual},{"frames",fs.size()},{"status",checks.empty()?"recorded_no_layer_assertion":std::any_of(checks.begin(),checks.end(),[](const J&a){return !a["pass"].get<bool>();})?"failed":"passed"}});
  }
  totalAssertions+=assertions;report["layers"][layer]={{"cases_enumerated",69},{"cases_with_assertions",checkedCases},{"assertions",assertions},{"failed_assertions",failed},{"rows",rows}};
 }
 report["supplemental"]=J::array();for(const auto&t:extra["cases"]){std::string kind=t["kind"];J actual;bool pass=true;
  if(kind=="capacity"){bool valid=capacity_valid(t["regions"],t["lines"],t["metadata"],t["probes"]);actual={{"valid",valid}};pass=valid==t["valid"];}
  else if(kind=="actual-capacity"){Candidate c;Key k;k.capture=k.ready=50000000;k.sequence=1;std::vector<std::uint8_t>pixels(640*640*3);std::vector<Query>q(t["queries"].get<std::size_t>(),{{320,500},140,200,0,false});std::vector<Line>l(t["lines"].get<std::size_t>(),{{320,500},0,640,7});auto o=c.extract({pixels,640,640,1920,k},q,l);actual={{"invalid",o.invalid},{"input_queries",q.size()},{"input_lines",l.size()},{"output_count",o.count},{"probes",o.probes}};pass=o.invalid==t["invalid"];}
  else if(kind=="source"){Candidate c;Observation o;o.count=1;Key k;k.capture=k.ready=50000000;k.sequence=1;c.relate(o,k,k.capture+t["age"].get<Ns>());actual={{"invalid",o.context_invalid}};pass=o.context_invalid==t["invalid"];}
  else{Observation o;o.count=1;o.parts[0].contact=Support::supported;o.parts[0].line_unique=o.parts[0].front_end=true;o.relations[0].usable=true;Guard g{51000000,200000000,200000000,30000000,"known_down",1,1,0,4,false};std::string execution;
   if(kind=="lifecycle"){o.parts[0].contact=Support::absent;g.now=g.last_contact+t["age"].get<Ns>();}
   if(kind=="deadline"){g.plan_deadline=g.now+t["plan_offset"].get<Ns>();g.gate_deadline=g.now+t["gate_offset"].get<Ns>();}
   if(kind=="receipt"){execution=t["execution"];g.execution=execution;g.prefix=t["prefix"];g.receipt_unknown=t["receipt_unknown"];if(execution=="never_executed")g.attachment_query=-1;}
   auto c=constrain(o,g);actual={{"invalid",c.invalid},{"move",c.move},{"down",c.down[0]},{"release",c.release}};for(std::string f:{"invalid","move","down","release"})if(t.contains(f))pass&=actual[f]==t[f];
  }
  report["supplemental"].push_back({{"name",t["name"]},{"kind",kind},{"actual",actual},{"pass",pass}});totalAssertions++;if(!pass)failures++;
 }
 report["assertions"]=totalAssertions;report["failed_assertions"]=failures;report["status"]=failures?"negative_stop_family":"cold_contract_only";report["real_png_audit_allowed"]=failures==0&&report["unverified_oracle_fields"].empty();save(argv[5],report);std::cout<<"vectors=20 expanded=69 supplementary="<<extra["cases"].size()<<" assertions="<<totalAssertions<<" failed="<<failures<<" metadata="<<Candidate::metadata_bytes()<<" report="<<argv[5]<<"\n";return failures?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
