// C++20 independent v2 fixture/readout contract. Expected values precede execution.
// No legacy input/oracle rewriting, no device/backend, no authoring labels in extract.
#include "bvi.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using J=nlohmann::json;
using namespace bvi;
namespace {
constexpr int W=640,H=640,S=W*3;
constexpr Ns MS=1000000;
struct Recorder {
 J rows=J::array(); int total{},failed{};
 J row(const std::string&name,const std::string&category,const std::string&basis) {
  return {{"case",name},{"category",category},{"contract",basis},{"assertions",J::array()}};
 }
 void check(J&r,const std::string&f,const J&expected,const J&actual){
  const bool pass=expected==actual; r["assertions"].push_back({{"field",f},{"expected",expected},{"actual",actual},{"pass",pass}});
  ++total; if(!pass)++failed;
 }
 void save(J r){rows.push_back(std::move(r));}
};
Point u(double a){return {std::cos(a),std::sin(a)};}
Point v(double a){return {-std::sin(a),std::cos(a)};}
Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
Key key(Ns t=50*MS,std::uint64_t sequence=1){Key k;k.capture=k.ready=t;k.sequence=sequence;return k;}
Query query(double y=504){return {{320,y},140,200,0,false};}
Line line(double y=500){return {{320,y},0,640,7};}
Guard guard(Ns now=51*MS){return {now,200*MS,200*MS,30*MS,"known_down",1,1,0,4,false};}
void put(std::vector<std::uint8_t>&p,int x,int y,int r,int g,int b){
 if(x<0||x>=W||y<0||y>=H)return;
 const auto i=static_cast<std::size_t>(y)*S+x*3;
 p[i]=static_cast<std::uint8_t>(r);p[i+1]=static_cast<std::uint8_t>(g);p[i+2]=static_cast<std::uint8_t>(b);
}
// Renderer owns labels; only its RGB bytes plus legal geometry leave this function.
std::vector<std::uint8_t> render(Query q,const std::vector<Line>&ls,bool rails=true,const std::vector<int>&rail_depths={}){
 std::vector<std::uint8_t>p(S*H);const auto tu=u(q.angle),nv=v(q.angle);
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){const auto d=sub({double(x),double(y)},q.front);const auto along=dot(d,tu),depth=dot(d,nv);
  if(std::abs(along)<=q.width/2&&depth>=-q.depth&&depth<=0)put(p,x,y,40,190,255);
 }
 if(rails)for(int y=0;y<H;++y)for(int x=0;x<W;++x){const auto d=sub({double(x),double(y)},q.front);const auto along=dot(d,tu),depth=dot(d,nv);
  const bool selected=rail_depths.empty()||std::any_of(rail_depths.begin(),rail_depths.end(),[&](int k){return std::abs(depth+k)<.1;});
  if(selected&&std::abs(std::abs(along)-q.width/2)<=1.5&&depth>=-q.depth&&depth<=0)put(p,x,y,255,255,255);
 }
 for(const auto&l:ls)for(int y=0;y<H;++y)for(int x=0;x<W;++x){auto d=sub({double(x),double(y)},l.center);if(std::abs(dot(d,v(l.angle)))<=2&&std::abs(dot(d,u(l.angle)))<=l.length/2)put(p,x,y,255,255,255);}
 return p;
}
Observation raw_typed(Query q,const std::vector<Line>&ls,Key k){
 Observation o;o.count=1;auto&d=o.parts[0];d.q=q;d.key=k;d.body=Support::supported;d.left=d.right=d.front_end=true;d.measured_depth=q.depth;
 d.signature=k.sequence;d.rgb_signature=100+k.sequence;d.rear={q.front.x+std::sin(q.angle)*q.depth,q.front.y-std::cos(q.angle)*q.depth};
 std::size_t n=0;for(const auto&l:ls){const auto nv=v(q.angle),ln=v(l.angle);double den=dot(nv,ln);if(std::abs(den)<1e-6)continue;
  double off=-dot(sub(q.front,l.center),ln)/den;Point h{q.front.x+nv.x*off,q.front.y+nv.y*off};
  if(off<=4&&off>=-q.depth&&std::abs(dot(sub(h,l.center),u(l.angle)))<=l.length/2){++n;d.hit=h;d.line_id=l.id;}
 }
 d.line_unique=n==1;d.contact=n?Support::supported:Support::absent;return o;
}
struct TypedEnvelope {
 bool source_valid=true;std::vector<Query> queries{query()};std::vector<Line> lines{line()};
 std::size_t measured_bytes=S*H,expected_bytes=S*H,metadata=Candidate::metadata_bytes();std::uint64_t probes=0;
};
// This is a separately versioned typed schema adapter, not a claim that legacy typed() changed.
void validate_typed(Observation&o,const TypedEnvelope&e){
 if(!e.source_valid){o.invalid=true;o.reason="source";}
 else if(e.queries.size()>128){o.invalid=true;o.reason="regions";}
 else if(e.lines.size()>16){o.invalid=true;o.reason="lines";}
 else if(!capacity_valid(e.queries.size(),e.lines.size(),e.metadata,e.probes)){o.invalid=true;o.reason=e.metadata>1048576?"metadata_helper":"probes_helper";}
 else if(e.measured_bytes!=e.expected_bytes){o.invalid=true;o.reason="byte_count";}
 else for(const auto&q:e.queries)if(!std::isfinite(q.width)||!std::isfinite(q.front.x)||!std::isfinite(q.front.y)||!std::isfinite(q.angle)||!std::isfinite(q.depth)){
  o.invalid=true;o.reason="nonfinite_query";break;
 }
}
J readout(const Observation&o,const Constraints&c){
 J parts=J::array();for(std::size_t i=0;i<o.count;++i){const auto&d=o.parts[i];const auto&r=o.relations[i];
  const bool visible=!o.invalid&&d.body==Support::supported;
  const bool body=!o.invalid&&!o.context_invalid&&!r.invalid&&!r.ambiguous&&(d.q.tap||(d.body==Support::supported&&d.left&&d.right));
  const bool hit=body&&d.contact==Support::supported&&d.line_unique;
  parts.push_back({{"raw_body_claim",d.body==Support::supported},{"raw_contact",name(d.contact)},{"raw_hit",{d.hit.x,d.hit.y}},
   {"raw_line_id",d.line_id},{"line_unique",d.line_unique},{"visible_body_valid",visible},{"action_eligible_body",body},
   {"eligible_hit",hit?J::array({d.hit.x,d.hit.y}):J(nullptr)},{"relation_invalid",r.invalid},{"relation_ambiguous",r.ambiguous}});
 }
 return {{"invalid",o.invalid},{"context_invalid",o.context_invalid},{"reason",o.reason},{"parts",parts},{"move",c.move},{"refresh",c.refresh},
 {"down",c.down[0]},{"release",c.release},{"constraint_invalid",c.invalid},{"contact_id",c.contact_id},{"completed",c.completed}};
}
void no_action(Recorder&rec,J&r,const Constraints&c,bool release){rec.check(r,"move",false,c.move);rec.check(r,"refresh",false,c.refresh);rec.check(r,"down",false,c.down[0]);rec.check(r,"all_down_false",true,std::none_of(c.down.begin(),c.down.end(),[](bool x){return x;}));rec.check(r,"release",release,c.release);}
void capacity_tests(Recorder&rec){
 const std::vector<std::uint8_t> pixels(S*H);struct C{const char*name;std::size_t qs,ls;bool invalid;};
 for(const C t:std::vector<C>{{"API-129-queries",129,1,true},{"API-17-lines",1,17,true},{"API-128-queries",128,1,false},{"API-16-lines",1,16,false},{"API-128-queries-16-lines",128,16,false}}){
  Candidate c;std::vector<Query>qs(t.qs,query());std::vector<Line>ls(t.ls,line());auto k=key();auto o=c.extract({pixels,W,H,S,k},qs,ls);c.relate(o,k,51*MS);auto g=constrain(o,guard());
  auto r=rec.row(t.name,"actual_API_capacity","C-V2-CAPACITY");r["input"]={{"actual_queries",qs.size()},{"actual_lines",ls.size()},{"RGB_bytes",pixels.size()},{"declared_usage_passed_to_candidate",false}};r["actual"]={{"invalid",o.invalid},{"count",o.count},{"probes",o.probes}};
  rec.check(r,"invalid",t.invalid,o.invalid);rec.check(r,"count_bound",true,o.count<=128);rec.check(r,"actual_query_count",t.qs,qs.size());rec.check(r,"actual_line_count",t.ls,ls.size());
  if(t.invalid){rec.check(r,"effective_body",false,readout(o,g)["parts"][0]["visible_body_valid"]);bool all_invalid=true;for(std::size_t i=0;i<o.count;++i)all_invalid&=o.parts[i].body==Support::invalid&&o.parts[i].contact==Support::invalid;rec.check(r,"all_output_support_invalid",true,all_invalid);no_action(rec,r,g,true);}rec.save(r);
 }
 struct Hc{const char*name;std::size_t m;std::uint64_t p;bool valid;};
 for(const Hc t:std::vector<Hc>{{"HELPER-metadata-1MiB",1048576,0,true},{"HELPER-metadata-1MiB-plus1",1048577,0,false},{"HELPER-probes-limit",0,6291456,true},{"HELPER-probes-plus1",0,6291457,false}}){
  auto r=rec.row(t.name,"capacity_helper_only","C-V2-CAPACITY");r["input"]={{"metadata",t.m},{"probes",t.p},{"dynamic_extract_executed",false}};rec.check(r,"capacity_valid",t.valid,capacity_valid(1,1,t.m,t.p));rec.save(r);
 }
 auto r=rec.row("ABI-static-metadata","ABI_static_bound","C-V2-CAPACITY");r["metadata_bytes"]=Candidate::metadata_bytes();rec.check(r,"metadata_within_1MiB",true,Candidate::metadata_bytes()<=1048576);rec.save(r);
}
void typed_tests(Recorder&rec){
 const std::vector<std::string> modes={"source","regions-overflow","lines-overflow","metadata-overflow","probes-overflow","short-rgb","nan","infinity"};
 for(const auto&m:modes){TypedEnvelope e;if(m=="source")e.source_valid=false;if(m=="regions-overflow")e.queries.resize(129,query());if(m=="lines-overflow")e.lines.resize(17,line());
  if(m=="metadata-overflow")e.metadata=1048577;
  if(m=="probes-overflow")e.probes=6291457;
  if(m=="short-rgb")--e.measured_bytes;
  if(m=="nan")e.queries[0].width=std::numeric_limits<double>::quiet_NaN();
  if(m=="infinity")e.queries[0].width=std::numeric_limits<double>::infinity();
  Candidate c;auto k=key();auto o=raw_typed(query(),{line()},k);validate_typed(o,e);c.relate(o,k,51*MS);const auto g=constrain(o,guard());const auto a=readout(o,g);
  auto r=rec.row("TYPED-invalid-"+m,"typed_invalid_payload","C-V2-ELIGIBILITY");r["legacy_case"]="V16-"+m;r["input"]={{"actual_queries",e.queries.size()},{"actual_lines",e.lines.size()},{"measured_bytes",e.measured_bytes},{"expected_bytes",e.expected_bytes},{"metadata_helper",e.metadata},{"probe_helper",e.probes},{"dynamic_RGB_stress",false}};r["actual"]=a;
  rec.check(r,"invalid",true,o.invalid);rec.check(r,"raw_body_claim",true,a["parts"][0]["raw_body_claim"]);rec.check(r,"raw_contact","supported",a["parts"][0]["raw_contact"]);rec.check(r,"raw_hit",J::array({320.0,500.0}),a["parts"][0]["raw_hit"]);
  rec.check(r,"visible_body_valid",false,a["parts"][0]["visible_body_valid"]);rec.check(r,"action_eligible_body",false,a["parts"][0]["action_eligible_body"]);rec.check(r,"eligible_hit",nullptr,a["parts"][0]["eligible_hit"]);rec.check(r,"history_samples",0,c.samples());rec.check(r,"relation_invalid",true,o.relations[0].invalid);no_action(rec,r,g,true);rec.save(r);
 }
 for(const auto&mode:std::vector<std::string>{"valid","context","future","nonmonotonic","ordinary-gap","expired-plan","unknown-down","completed"}){
  Candidate c;const auto k0=key();auto first=raw_typed(query(),{line()},k0);c.relate(first,k0,51*MS);auto k=key(70*MS,2);Ns now=71*MS;
  if(mode=="context")k.context.epoch=2;
  if(mode=="future"){k.capture=k.ready=72*MS;now=71*MS;}
  if(mode=="nonmonotonic")k.capture=k.ready=50*MS;
  if(mode=="ordinary-gap"){k.capture=k.ready=91*MS;now=92*MS;}
  auto o=raw_typed(query(),{line()},k);TypedEnvelope e;validate_typed(o,e);c.relate(o,k,now);auto gg=guard(now);if(mode=="expired-plan")gg.plan_deadline=now;
  if(mode=="unknown-down"){gg.execution="unknown_down";gg.receipt_unknown=true;}if(mode=="completed"){gg.execution="completed_up";gg.prefix=2;}
  auto g=constrain(o,gg);const auto a=readout(o,g);auto r=rec.row("ELIGIBILITY-"+mode,"eligibility_positive_reset_and_receipt","C-V2-ELIGIBILITY");r["actual"]=a;r["history_samples"]=c.samples();
  const bool temporal_bad=mode=="context"||mode=="future"||mode=="nonmonotonic";const bool action_bad=temporal_bad||mode=="expired-plan"||mode=="unknown-down"||mode=="completed";
  rec.check(r,"raw_body_claim",true,a["parts"][0]["raw_body_claim"]);rec.check(r,"visible_body_valid",true,a["parts"][0]["visible_body_valid"]);rec.check(r,"context_invalid",temporal_bad,o.context_invalid);
  rec.check(r,"action_eligible_body",!temporal_bad,a["parts"][0]["action_eligible_body"]);rec.check(r,"eligible_hit",temporal_bad?J(nullptr):J::array({320.0,500.0}),a["parts"][0]["eligible_hit"]);
  rec.check(r,"move",!action_bad,g.move);rec.check(r,"refresh",!action_bad,g.refresh);rec.check(r,"down",false,g.down[0]);rec.check(r,"release",action_bad&&mode!="completed",g.release);
  if(mode=="ordinary-gap")rec.check(r,"gap_history_reset_then_current",1,c.samples());
  if(mode=="completed")rec.check(r,"completed_persists",true,g.completed);
  rec.save(r);
 }
}
void ambiguous_tests(Recorder&rec){
 Query q=query();q.front.x=308;const Line a=line(),b{{320,500},.04,640,8};J outputs=J::array();
 for(bool reverse:{false,true}){auto ls=std::vector<Line>{a,b};if(reverse)std::reverse(ls.begin(),ls.end());Candidate c;const auto k=key();auto o=raw_typed(q,ls,k);c.relate(o,k,51*MS);const auto g=constrain(o,guard());auto out=readout(o,g);outputs.push_back(out);
  auto r=rec.row(reverse?"AMBIGUOUS-reverse":"AMBIGUOUS-forward","ambiguous_projection","C-V2-AMBIGUOUS");r["actual"]=out;
  rec.check(r,"raw_body_claim",true,out["parts"][0]["raw_body_claim"]);rec.check(r,"raw_contact","supported",out["parts"][0]["raw_contact"]);rec.check(r,"line_unique",false,o.parts[0].line_unique);rec.check(r,"eligible_hit",nullptr,out["parts"][0]["eligible_hit"]);no_action(rec,r,g,false);rec.check(r,"contact_id",1,g.contact_id);rec.save(r);
 }
 auto r=rec.row("AMBIGUOUS-order-semantic-comparison","ambiguous_projection","C-V2-AMBIGUOUS");rec.check(r,"raw_hit_order_dependence_retained",true,outputs[0]["parts"][0]["raw_hit"]!=outputs[1]["parts"][0]["raw_hit"]);
 for(const auto*f:{"move","refresh","down","release","contact_id"})rec.check(r,std::string("same_")+f,outputs[0][f],outputs[1][f]);
 rec.check(r,"same_eligible_hit",outputs[0]["parts"][0]["eligible_hit"],outputs[1]["parts"][0]["eligible_hit"]);r["legacy_whole_output_equivalence_repaired"]=false;rec.save(r);
}
void visual_tests(Recorder&rec){
 for(const auto&mode:std::vector<std::string>{"long-rails","two-point-rails","one-point-rails","gapped-two-point-rails","no-rails-line-crossing","front-line-plus4","front-line-minus4","endpoint-white-band","endpoint-single-blue-side","endpoint-no-outer-black","endpoint-opaque-yellow","endpoint-clipping","rear-line-plus4","rear-line-minus4","co-rotation-visible-blue"}){
  auto q=query();std::vector<Line>ls{line()};std::vector<int>depths;bool rails=mode!="no-rails-line-crossing";
  if(mode=="two-point-rails"){depths.push_back(20);depths.push_back(21);}
  if(mode=="one-point-rails")depths.push_back(20);
  if(mode=="gapped-two-point-rails"){depths.push_back(20);depths.push_back(22);}
  if(mode=="front-line-minus4")q.front.y=496;
  if(mode=="endpoint-clipping")q.front.y=639;
  if(mode=="rear-line-plus4")ls={line(300)};
  if(mode=="rear-line-minus4")ls={line(308)};
  if(mode=="co-rotation-visible-blue"){q.angle=.25;const auto normal=v(q.angle);ls={{{q.front.x-4*normal.x,q.front.y-4*normal.y},q.angle,640,7}};}
  auto p=render(q,ls,rails,depths);
  if(mode=="endpoint-white-band")for(int y=500;y<=508;++y)for(int x=260;x<=380;++x)put(p,x,y,255,255,255);
  if(mode=="endpoint-single-blue-side")for(int y=500;y<504;++y)for(int x=270;x<=272;++x)put(p,x,y,0,0,0);
  if(mode=="endpoint-no-outer-black")for(int y=505;y<=508;++y)for(int x=260;x<=380;++x)put(p,x,y,40,190,255);
  if(mode=="endpoint-opaque-yellow")for(int y=500;y<=508;++y)for(int x=260;x<=380;++x)put(p,x,y,255,225,80);
  Candidate c;auto k=key();std::vector<Query>qs{q};auto o=c.extract({p,W,H,S,k},qs,ls);c.relate(o,k,51*MS);const auto g=constrain(o,guard());const auto&d=o.parts[0];
  auto r=rec.row("RGB-"+mode,"rails_and_endpoints","C-V2-RAIL/C-V2-ENDPOINT");r["actual"]=readout(o,g);r["actual"]["left"]=d.left;r["actual"]["right"]=d.right;r["actual"]["front_end"]=d.front_end;r["actual"]["rear_end"]=d.rear_end;r["actual"]["probes"]=o.probes;
  const bool expected_rails=mode!="one-point-rails"&&mode!="gapped-two-point-rails"&&mode!="no-rails-line-crossing";
  const bool expected_front=expected_rails&&mode!="endpoint-white-band"&&mode!="endpoint-single-blue-side"&&mode!="endpoint-no-outer-black"&&mode!="endpoint-opaque-yellow"&&mode!="endpoint-clipping";
  rec.check(r,"invalid",false,o.invalid);rec.check(r,"left",expected_rails,d.left);rec.check(r,"right",expected_rails,d.right);rec.check(r,"front_end",expected_front,d.front_end);
  if(mode=="rear-line-plus4"||mode=="rear-line-minus4"||mode=="long-rails")rec.check(r,"rear_end",true,d.rear_end);
  if(!expected_rails){rec.check(r,"move",false,g.move);rec.check(r,"refresh",false,g.refresh);}if(mode=="long-rails")rec.check(r,"move",true,g.move);rec.save(r);
 }
}
void merge_tests(Recorder&rec){
 for(const auto&mode:std::vector<std::string>{"union-with-cap","union-without-cap","single-continuation","two-separated-current"}){
  Candidate c;auto k0=key(10*MS,1);Query low{{320,490},140,40,0,false},high{{320,430},140,40,0,false};auto old=raw_typed(low,{line(480)},k0);
  if(mode!="single-continuation"){old.count=2;old.parts[1]=raw_typed(high,{line(420)},k0).parts[0];}c.relate(old,k0,11*MS);
  auto k=key(40*MS,2);Query current{{320,510},140,140,0,false};auto o=raw_typed(current,{line()},k);
  if(mode=="union-without-cap")o.parts[0].front_end=false;
  if(mode=="two-separated-current"){o=raw_typed(low,{line(480)},k);o.count=2;o.parts[1]=raw_typed(high,{line(420)},k).parts[0];}
  c.relate(o,k,41*MS);const auto g=constrain(o,guard(41*MS));const bool ambiguous=mode=="union-with-cap"||mode=="union-without-cap";
  auto r=rec.row("MERGE-"+mode,"merge_correspondence","C-V2-MERGE");r["actual"]=readout(o,g);rec.check(r,"relation_ambiguous",ambiguous,o.relations[0].ambiguous);rec.check(r,"alternatives",ambiguous?2:1,o.relations[0].alternatives);rec.check(r,"move",!ambiguous,g.move);rec.check(r,"refresh",!ambiguous,g.refresh);
  if(mode=="two-separated-current")rec.check(r,"second_relation_ambiguous",false,o.relations[1].ambiguous);
  rec.save(r);
 }
}
J change_map(const J&classification){
 if(classification.at("failures").size()!=54)throw std::runtime_error("legacy failures must remain 54");
 J rows=J::array();std::map<std::string,int>counts;
 for(const auto&f:classification.at("failures")){J r=f;const std::string cluster=f.at("cluster");++counts[cluster];r["legacy_expected_changed"]=false;r["legacy_fail_preserved"]=true;
  if(cluster=="C1"){r["v2_contract"]="C-V2-RAIL/C-V2-MERGE";r["disposition"]="same_original_expected_candidate_repair";r["new_controls"]={"RGB-no-rails-line-crossing","RGB-long-rails","RGB-two-point-rails","RGB-one-point-rails","MERGE-union-with-cap","MERGE-two-separated-current"};}
  else if(cluster=="C2"){r["v2_contract"]="C-V2-ENDPOINT";r["disposition"]="same_original_expected_candidate_repair";r["new_controls"]={"RGB-front-line-plus4","RGB-front-line-minus4","RGB-endpoint-white-band","RGB-endpoint-single-blue-side"};}
  else if(cluster=="C3"){r["v2_contract"]="C-V2-PENDING";r["disposition"]="pending_effect_definition_original_fail_retained";r["new_controls"]=J::array();}
  else if(cluster=="C4"){r["v2_contract"]="C-V2-PENDING";r["disposition"]="pending_rotation_visibility_original_fail_retained";r["new_controls"]=J::array();}
  else if(cluster=="C5"){r["v2_contract"]="C-V2-CAPACITY";r["disposition"]="original_condition_not_applied_new_condition_separate";const auto n=f.at("case").get<std::string>();
   if(n.find("regions")!=std::string::npos)r["new_controls"]={"API-129-queries","API-128-queries"};
   else if(n.find("lines")!=std::string::npos)r["new_controls"]={"API-17-lines","API-16-lines"};
   else if(n.find("metadata")!=std::string::npos)r["new_controls"]={"HELPER-metadata-1MiB","HELPER-metadata-1MiB-plus1","ABI-static-metadata"};
   else r["new_controls"]={"HELPER-probes-limit","HELPER-probes-plus1"};
   r["original_actual_API_queries"]=1;r["original_actual_API_lines"]=1;r["declared_usage_read_by_candidate"]=false;r["dynamic_probe_coverage"]=false;
  }else if(cluster=="C6"){r["v2_contract"]="C-V2-ELIGIBILITY";r["disposition"]="new_raw_vs_eligible_view_original_adapter_not_repaired";const auto n=f.at("case").get<std::string>();r["new_controls"]={"TYPED-invalid-"+n.substr(4),"ELIGIBILITY-valid","ELIGIBILITY-context","ELIGIBILITY-future","ELIGIBILITY-nonmonotonic","ELIGIBILITY-ordinary-gap"};}
  else if(cluster=="C7"){r["v2_contract"]="C-V2-AMBIGUOUS";r["disposition"]="new_null_eligible_projection_original_comparator_not_repaired";r["new_controls"]={"AMBIGUOUS-forward","AMBIGUOUS-reverse","AMBIGUOUS-order-semantic-comparison","ELIGIBILITY-valid"};}
  else throw std::runtime_error("unknown failure cluster");
  rows.push_back(std::move(r));
 }
 if(counts["C1"]!=5||counts["C2"]!=6||counts["C3"]!=2||counts["C4"]!=7||counts["C5"]!=24||counts["C6"]!=8||counts["C7"]!=2)throw std::runtime_error("classification count changed");
 return {{"schema","bvi.cold.contract-change-map.v2"},{"baseline","acb27fdb095b82253ef9388eab52948a59663d02"},{"source_classification","../round2/BVI_FAILURE_SEMANTICS_CLASSIFICATION.json"},{"legacy_suite_sha256","8fbc37e8db4afe1d8736b2f89c17f25dc91d2213581d241279c6ab2dbda38611"},
 {"note","This map preserves original expectations and 54 failed rows; v2 projections are separate cold consumers, not integrated owner fixes."},{"legacy_denominator",{{"layer_cases",356},{"assertions",3938},{"failures",54}}},{"mapped_failures",54},{"cluster_counts",counts},{"expected_changes",J::array()},{"rows",rows},
 {"contract_revisions",J::array({{{"id","C-V2-ENDPOINT-r2"},{"initial_contract_sha256","269b94f49feb003cb83e0d6c706bd1d8c8627d51b68372446574aaa9bc683c42"},{"reason","Independent P05 actual blue at distance 2.7826 was rejected by 2.8 geometric mask; actual blue/black are mutually exclusive with white."},{"change","Remove endpoint geometric mask veto; rail white still requires outside mask."},{"oracle_expectations_changed",false},{"new_control","RGB-co-rotation-visible-blue"}},{{"id","C-V2-MERGE-P2"},{"reason","N09 and MERGE-union-with-cap expose current cap suppressing unresolved prior alternatives; single and separated-current positives retained."},{"change","Only remove !front_end exemption; preserve contains and nearest tie; no new separation threshold."},{"oracle_expectations_changed",false}}})},
 {"open_decisions",J::array({{{"id","effect-transparent-color"},{"legacy_failures",2},{"status","pending_original_yellow_unchanged"}},{{"id","rotation-line-occlusion"},{"legacy_failures",7},{"status","pending_original_flank_unchanged"}},{{"id","dynamic-probe-exhaustion"},{"status","not_measured_helper_only"}},{{"id","stationary-new-entry-dedup"},{"status","not_changed_no_threshold_reduction"}},{{"id","duplicate-ROI-nearest-tie"},{"status","existing_conservative_rejection_deferred"}}})}};
}
J read(const std::filesystem::path&p){std::ifstream in(p);if(!in)throw std::runtime_error("input missing: "+p.string());return J::parse(in);}
void save_new(const std::filesystem::path&p,const J&j){if(std::filesystem::exists(p))throw std::runtime_error("refusing to overwrite: "+p.string());std::ofstream out(p);if(!out)throw std::runtime_error("cannot create output");out<<j.dump(2)<<'\n';if(!out)throw std::runtime_error("write failed");}
}
int main(int argc,char**argv){try{
 if(argc!=3&&argc!=4){std::cerr<<"usage: contract_fixture_v2 original-classification.json fresh-result.json [fresh-change-map.json]\n";return 2;}
 const auto map=change_map(read(argv[1]));Recorder rec;capacity_tests(rec);typed_tests(rec);ambiguous_tests(rec);visual_tests(rec);merge_tests(rec);
 J categories=J::object();for(const auto&r:rec.rows){auto&c=categories[r.at("category").get<std::string>()];if(c.is_null())c={{"cases",0},{"assertions",0},{"failed_assertions",0}};c["cases"]=c["cases"].get<int>()+1;for(const auto&a:r["assertions"]){c["assertions"]=c["assertions"].get<int>()+1;if(!a["pass"].get<bool>())c["failed_assertions"]=c["failed_assertions"].get<int>()+1;}}
 J report={{"schema","bvi.cold.contract-fixture.v2"},{"cases",rec.rows.size()},{"assertions",rec.total},{"failed_assertions",rec.failed},{"categories",categories},{"rows",rec.rows},
 {"legacy_expected_modified",false},{"legacy_invalid_adapter_repaired",false},{"legacy_ambiguous_comparator_repaired",false},{"touch_backend",false},{"physical_human_gold",0},{"dynamic_probe_exhaustion_measured",false},{"new_projection_in_formal_owner",false},
 {"pending_decisions",map["open_decisions"]},{"status",rec.failed?"v2_cold_fail":"v2_specified_controls_pass_pending_decisions_remain"}};
 save_new(argv[2],report);if(argc==4)save_new(argv[3],map);std::cout<<"v2 cases="<<rec.rows.size()<<" assertions="<<rec.total<<" failed="<<rec.failed<<" legacy_failures_preserved=54\n";return rec.failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
