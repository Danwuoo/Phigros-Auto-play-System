// C++20 cold harness; no backend, observer, scheduler, RPC or runtime DLL loading.
#include <set>
namespace {
const std::set<std::string> handlers={"body","contact","cap","association","physical","line_id","hit_line_error_max","hit_body_required","current_body_preserved","effect","relation","independent","alternatives","usable","max_samples","max_window_ns","witness_retained","rear","past_line_count","span_ns","first_independent_ns","last_independent_ns","freshness_ns","move","down","refresh","release","lifecycle_invalid","completed_persists","completed_from_geometry","geometry_only_completion","up_at_second","up_at_third","contact_id","receipt_prefix","last_contact_after_ns","qualification_invalid","equal_to","equivalent_to"};
bool applies(const J&coverage,const std::string&f,const std::string&layer){
 if(!coverage.at("handlers").contains(f))return false;
 const auto&a=coverage.at("handlers").at(f);return std::find(a.begin(),a.end(),J(layer))!=a.end();
}
J validate_expected(const J&e,const J&coverage){
 J errors=J::array();if(!e.is_object()){errors.push_back("expected is not object");return errors;}
 for(auto it=e.begin();it!=e.end();++it){const auto&f=it.key();
  if(f=="permutation"){
   const auto&values=coverage.at("metadata").at(f);
   if(!it.value().is_string()||std::find(values.begin(),values.end(),it.value())==values.end())errors.push_back("unknown permutation value");
  }else if(!handlers.contains(f)||!coverage.at("handlers").contains(f)||coverage.at("handlers").at(f).empty())errors.push_back("unknown or uncovered field: "+f);
  else for(const auto&layer:coverage.at("handlers").at(f))if(layer!="rgb"&&layer!="typed"&&layer!="lifecycle"&&layer!="e2e")errors.push_back("unknown layer: "+layer.get<std::string>());
 }
 return errors;
}
struct Run {Observation last;J frames=J::array();std::uint64_t probes_max{};};
Run execute(const J&c,const std::string&layer){
 Candidate candidate;Run run;J receipt=c.at("guard");const bool rgb=layer=="rgb"||layer=="e2e";
 const auto&fs=c.at(rgb?"frames":"typed_frames");int rearPast=0;
 if(fs.empty()||fs.size()>64)throw std::runtime_error("frame bound");
 for(std::size_t fi=0;fi<fs.size();++fi){
  const auto&f=fs.at(fi);const auto k=key(f);
  if(f.contains("guard_patch"))for(auto it=f["guard_patch"].begin();it!=f["guard_patch"].end();++it)receipt[it.key()]=it.value();
  J effective=receipt;
  if(c.value("id",std::string{})=="R1"||fi+1<fs.size())effective["now_ns"]=k.capture+1000000;
  auto g=guard(effective);Observation o;
  if(rgb){auto pixels=render(f);std::vector<Query>qs;for(const auto&q:f["queries"])qs.push_back(query(q));auto ls=lines(f);o=candidate.extract({pixels,f["width"],f["height"],f["stride"],k},qs,ls);}
  else o=typed(f);
  candidate.relate(o,k,g.now);auto constraints=constrain(o,g);
  if(o.count&&o.parts[0].rear_end){auto ls=lines(f);if(!ls.empty()&&dot(sub(o.parts[0].rear,ls[0].center),nv(ls[0].angle))>0)++rearPast;}
  auto j=observation(o,candidate,constraints);j["up_geometry_opportunity"]=rearPast>=2;j["past_line_count"]=rearPast;
  j["receipt_prefix"]=g.prefix;j["last_contact_before_ns"]=g.last_contact;j["expected_contact_id"]=g.contact_id;
  if(c.value("update_receipt",false)&&constraints.refresh)receipt["last_contact_ns"]=g.now;
  j["last_contact_after_ns"]=receipt["last_contact_ns"];
  if(c["guard"].contains("witness_ns")&&!c["guard"]["witness_ns"].is_null())j["witness_retained"]=candidate.retains(c["guard"]["witness_ns"]);
  if(o.count){const auto&d=o.parts[0];double error=1e9;for(const auto&l:lines(f))if(l.id==d.line_id)error=std::abs(dot(sub(d.hit,l.center),nv(l.angle)));auto delta=sub(d.hit,d.q.front);bool in=std::abs(dot(delta,tu(d.q.angle)))<=d.q.width/2&&dot(delta,nv(d.q.angle))<=0&&dot(delta,nv(d.q.angle))>=-d.q.depth;
   j["hit_line_error"]=error;j["hit_body"]=in&&d.contact==Support::supported;
  }
  run.probes_max=std::max(run.probes_max,o.probes);run.last=o;run.frames.push_back(j);
 }
 return run;
}
J field_value(const std::string&f,const J&w,const J&actual,const J&trajectory){
 if(f=="body"||f=="contact"||f=="cap"||f=="relation"||f=="down"){
  J a=J::array();for(std::size_t i=0;i<w.size();++i)a.push_back(i<actual["parts"].size()?actual["parts"][i].at(f):J(nullptr));return a;
 }
 if(f=="effect"){bool any=false;for(const auto&p:actual["parts"])any|=p.at(f).get<bool>();return any;}
 if(f=="current_body_preserved")return !actual["parts"].empty()&&actual["parts"][0]["body"].get<bool>();
 if(f=="hit_line_error_max")return actual.contains("hit_line_error")&&actual["hit_line_error"].get<double>()<=w.get<double>();
 if(f=="hit_body_required")return actual.value("hit_body",false);
 if(f=="qualification_invalid")return actual.at("invalid").get<bool>()||actual.at("context_invalid").get<bool>()||actual.at("lifecycle_invalid").get<bool>();
 if(f=="max_samples")return actual.at("samples").get<std::size_t>()<=w.get<std::size_t>();
 if(f=="max_window_ns")return actual.at("window_span_ns").get<Ns>()<=w.get<Ns>();
 if(f=="rear"){J a=J::array();for(const auto&j:trajectory)a.push_back(j["parts"].empty()?J(0):j["parts"][0]["rear"][1]);return a;}
 if(f=="up_at_second"||f=="up_at_third")return trajectory.at(f=="up_at_second"?1:2).at("up_geometry_opportunity");
 if(f=="association"||f=="line_id"||f=="independent"||f=="alternatives"||f=="usable"||f=="span_ns"||f=="first_independent_ns"||f=="last_independent_ns"||f=="freshness_ns")return actual["parts"].empty()?J(nullptr):actual["parts"][0].at(f);
 if(actual.contains(f))return actual.at(f);
 throw std::runtime_error("required field has no actual handler: "+f);
}
}
struct AssertionAccumulator {
 int failures{},assertions{};
 void check(J&rows,const std::string&field,const J&want,const J&got){const bool pass=want==got;rows.push_back({{"field",field},{"expected",want},{"actual",got},{"pass",pass}});++assertions;if(!pass)++failures;}
 int finalize(J&report){report["assertions"]=assertions;report["failed_assertions"]=failures;report["status"]=failures?"cold_fail_requires_classification":"cold_contract_only";report["adopted"]=false;return failures?1:0;}
};
bool rejects_wrong_contact(const J&report,int exit){if(exit!=1||report.value("status",std::string{})!="cold_fail_requires_classification"||report.value("failed_assertions",0)!=2)return false;const auto&rows=report.at("negative_rows");if(rows.size()!=2)return false;return rows[0]["field"]=="contact_id_transfer@0"&&rows[1]["field"]=="renaming_output_contact"&&rows[0]["expected"]==4&&rows[1]["expected"]==4&&rows[0]["actual"]==1&&rows[1]["actual"]==1&&rows[0]["pass"]==false&&rows[1]["pass"]==false;}
int main(int argc,char**argv){try{
 if(argc!=8&&argc!=9){std::cerr<<"usage: bvi_tests original-normalized original-oracle typed-r1 original-supplemental r1-cases coverage output [wrong-contact-only]\n";return 2;}
 auto spec=read(argv[1]),oracle=read(argv[2]),ti=read(argv[3]),extra=read(argv[4]),r1=read(argv[5]),coverage=read(argv[6]);
 if(spec.at("expanded_cases")!=69||oracle.at("expanded_cases")!=69||ti.at("cases").size()!=69||extra.at("cases").size()!=22||r1.at("cases").size()!=20)throw std::runtime_error("fixture denominator");
 J all=J::array();for(std::size_t ci=0;ci<69;++ci){auto c=spec["cases"][ci];if(c["name"]!=oracle["cases"][ci]["name"]||c["name"]!=ti["cases"][ci]["name"])throw std::runtime_error("fixture mapping");c["expected"]=oracle["cases"][ci]["expected"];c["typed_frames"]=ti["cases"][ci]["frames"];all.push_back(c);}for(const auto&c:r1["cases"])all.push_back(c);
 J report={{"schema","bvi.r1.result.v1"},{"family","BVI-1"},{"original_top_vectors",20},{"original_expanded_cases",69},{"original_supplemental_cases",22},{"new_cases",20},{"schema_controls",4},{"metadata_bytes",Candidate::metadata_bytes()},{"sizeof_candidate",sizeof(Candidate)},{"sizeof_observation",sizeof(Observation)},{"sizeof_descriptor",sizeof(Descriptor)},{"no_past_rgb",true},{"touch_backend",false},{"physical_human_gold",0},{"layers",J::object()},{"unverified_oracle_fields",J::array()},{"schema_errors",J::array()}};
 AssertionAccumulator aggregate;int&failures=aggregate.failures;int&assertions=aggregate.assertions;std::uint64_t probesMax=0;std::set<std::string>covered;
 auto check=[&](J&list,const std::string&field,const J&want,const J&got){aggregate.check(list,field,want,got);};
 auto negative=[&](){J n={{"schema","bvi.r2f.contact-negative.v1"},{"negative_rows",J::array()}};AssertionAccumulator a;
  const J*c=nullptr;for(const auto&item:all)if(item["expected"].value("permutation",std::string{})=="contact-id")c=&item;
  if(!c)throw std::runtime_error("contact permutation fixture missing");auto trajectory=execute(*c,"e2e").frames;
  J actual=trajectory.back();actual["contact_id"]=1; // Fault at the same output-to-assertion boundary, never fixture mutation.
  a.check(n["negative_rows"],"contact_id_transfer@0",actual["expected_contact_id"],actual["contact_id"]);
  a.check(n["negative_rows"],"renaming_output_contact",coverage["contact_mapping"]["to"],actual["contact_id"]);
  const int code=a.finalize(n);n["consumer_rejects"]=rejects_wrong_contact(n,code);n["expected_native_exit"]=1;return n;
 };
 if(argc==9){if(std::string_view(argv[8])!="wrong-contact-only")throw std::runtime_error("unknown mode");auto n=negative();save(argv[7],n);return n["failed_assertions"].get<int>()?1:0;}
 // Validate BEFORE any candidate call; no unknown expected field can vanish between layers.
 for(const auto&c:all){auto errors=validate_expected(c["expected"],coverage);for(const auto&e:errors)report["schema_errors"].push_back({{"case",c["name"]},{"error",e}});if(c.contains("expected_steps"))for(const auto&e:c["expected_steps"])for(const auto&error:validate_expected(e,coverage))report["schema_errors"].push_back({{"case",c["name"]},{"error",error}});}
 report["schema_negative_controls"]=J::array();for(const auto&t:r1["schema_controls"]){bool rejected=false;if(t.value("kind",std::string{})=="contact"){auto n=negative();rejected=rejects_wrong_contact(n,1);report["contact_negative_aggregate"]=n;}else{J copy=coverage;if(t.contains("remove_coverage"))copy["handlers"][t["remove_coverage"].get<std::string>()]=J::array();rejected=!validate_expected(t["expected"],copy).empty();}J checks=J::array();check(checks,t["name"],t["reject"],rejected);report["schema_negative_controls"].push_back({{"name",t["name"]},{"assertions",checks}});}
 report["contact_adapter_controls"]=J::array();{AssertionAccumulator a;J rows=J::array();a.check(rows,"contact_id_transfer@0",4,4);check(report["contact_adapter_controls"],"C01",0,a.finalize(report["contact_positive_control"]));auto n=negative();check(report["contact_adapter_controls"],"C02-C03",true,rejects_wrong_contact(n,1));J fake=n;fake["negative_rows"]=J::array();check(report["contact_adapter_controls"],"C04-missing-rows",false,rejects_wrong_contact(fake,1));check(report["contact_adapter_controls"],"C04-false-exit0",false,rejects_wrong_contact(n,0));}
 if(!report["schema_errors"].empty()){report["status"]="harness_schema_fail_closed";report["failed_assertions"]=failures+report["schema_errors"].size();report["candidate_executed"]=false;save(argv[7],report);return 2;}
 report["renderer_controls"]=J::array();for(auto pair:std::array<std::pair<std::string,std::string>,3>{{{"V00-whole","V00-touching"},{"V01-whole","V01-touching"},{"V19-two-physical","V19-decorative-countermodel"}}}){
  const J*a=nullptr,*b=nullptr;for(const auto&c:all){if(c["name"]==pair.first)a=&c;if(c["name"]==pair.second)b=&c;}if(!a||!b)throw std::runtime_error("pair missing");bool equal=(*a)["frames"].size()==(*b)["frames"].size();for(std::size_t i=0;i<(*a)["frames"].size()&&equal;++i){const auto&x=(*a)["frames"][i];const auto&y=(*b)["frames"][i];equal=render(x)==render(y)&&x["queries"]==y["queries"]&&x["lines"]==y["lines"]&&x["context"]==y["context"];}J checks=J::array();check(checks,"full_RGB_and_legal_input_equality",true,equal);report["renderer_controls"].push_back({{"a",pair.first},{"b",pair.second},{"assertions",checks}});
 }
 for(const std::string layer:{"rgb","typed","lifecycle","e2e"}){
  J rows=J::array();std::map<std::string,Run>bank;std::map<std::string,J>cases;
  for(const auto&c:all){const auto name=c.at("name").get<std::string>();bank.emplace(name,execute(c,layer));cases[name]=c;probesMax=std::max(probesMax,bank.at(name).probes_max);}
  int startAssertions=assertions,startFailures=failures,checked=0;
  for(const auto&c:all){const auto name=c.at("name").get<std::string>();const auto&run=bank.at(name);const auto&actual=run.frames.back();J checks=J::array();
   for(std::size_t fi=0;fi<run.frames.size();++fi)check(checks,"contact_id_transfer@"+std::to_string(fi),run.frames[fi]["expected_contact_id"],run.frames[fi]["contact_id"]);
   auto evaluate=[&](const J&e,const J&a,int step){
    for(auto it=e.begin();it!=e.end();++it){const auto f=it.key();const auto&w=it.value();std::string id=name+":"+std::to_string(step)+":"+f+":"+layer;
     if(f=="permutation"){covered.insert(name+":"+std::to_string(step)+":"+f+":metadata");continue;}
     if(!applies(coverage,f,layer))continue;
     J want=w,got;
     if(f=="equal_to"||f=="equivalent_to"){
      if(w.is_null())got=nullptr;
      else{const auto reference=w.get<std::string>();if(!bank.contains(reference))throw std::runtime_error("reference missing");J renamed=a;const auto&base=cases.at(reference);if(c["expected"].value("permutation",std::string{})=="contact-id"){
        const int from=coverage["contact_mapping"]["from"],to=coverage["contact_mapping"]["to"];
        check(checks,"renaming_base_contact",from,bank.at(reference).frames.back()["contact_id"]);check(checks,"renaming_output_contact",to,a["contact_id"]);
        if(base["guard"]["contact_id"]!=from||c["guard"]["contact_id"]!=to)throw std::runtime_error("undeclared contact mapping");
        if(renamed["contact_id"]==to)renamed["contact_id"]=from;
       }want=normalized(bank.at(reference).frames.back());got=normalized(renamed);}
     }else{got=field_value(f,w,a,run.frames);if(f=="max_samples"||f=="max_window_ns"||f=="hit_line_error_max")want=true;}
     check(checks,(step<0?f:f+"@"+std::to_string(step)),want,got);covered.insert(id);
    }
   };
   evaluate(c["expected"],actual,-1);
   if(c.contains("expected_steps")&&!c["expected_steps"].empty()){
    if(c["expected_steps"].size()!=run.frames.size())throw std::runtime_error("step oracle denominator");
    for(std::size_t fi=0;fi<run.frames.size();++fi)evaluate(c["expected_steps"][fi],run.frames[fi],static_cast<int>(fi));
   }
   if(!checks.empty())++checked;
   rows.push_back({{"case",name},{"original",c["id"]!="R1"},{"assertions",checks},{"actual",actual},{"trajectory",c.value("id",std::string{})=="R1"?run.frames:J(nullptr)},{"frames",run.frames.size()}});
  }
  report["layers"][layer]={{"cases_enumerated",all.size()},{"cases_with_assertions",checked},{"assertions",assertions-startAssertions},{"failed_assertions",failures-startFailures},{"rows",rows}};
 }
 // Every declared necessary field and applicable layer must have been checked.
 for(const auto&r:coverage["rows"]){const auto name=r["case"].get<std::string>(),f=r["field"].get<std::string>();const int step=r.value("step",-1);for(const auto&layer:r["layers"]){const auto id=name+":"+std::to_string(step)+":"+f+":"+layer.get<std::string>();if(!covered.contains(id))report["unverified_oracle_fields"].push_back(r);}}
 if(!report["unverified_oracle_fields"].empty())failures+=static_cast<int>(report["unverified_oracle_fields"].size());
 report["supplemental"]=J::array();for(const auto&t:extra["cases"]){const std::string kind=t["kind"];J actual;bool pass=true;
  if(kind=="capacity"){const bool valid=capacity_valid(t["regions"],t["lines"],t["metadata"],t["probes"]);actual={{"valid",valid}};pass=valid==t["valid"];}
  else if(kind=="actual-capacity"){Candidate c;Key k;k.capture=k.ready=50000000;k.sequence=1;std::vector<std::uint8_t>pixels(640*640*3);std::vector<Query>q(t["queries"].get<std::size_t>(),{{320,500},140,200,0,false});std::vector<Line>l(t["lines"].get<std::size_t>(),{{320,500},0,640,7});const auto o=c.extract({pixels,640,640,1920,k},q,l);actual={{"invalid",o.invalid},{"input_queries",q.size()},{"input_lines",l.size()},{"output_count",o.count},{"probes",o.probes}};pass=o.invalid==t["invalid"];probesMax=std::max(probesMax,o.probes);}
  else if(kind=="source"){Candidate c;Observation o;o.count=1;Key k;k.capture=k.ready=50000000;k.sequence=1;c.relate(o,k,k.capture+t["age"].get<Ns>());actual={{"invalid",o.context_invalid}};pass=o.context_invalid==t["invalid"];}
  else{Observation o;o.count=1;o.parts[0].body=Support::supported;o.parts[0].left=o.parts[0].right=true;o.parts[0].contact=Support::supported;o.parts[0].line_unique=o.parts[0].front_end=true;o.relations[0].usable=true;Guard g{51000000,200000000,200000000,30000000,"known_down",1,1,0,4,false};std::string execution;
   if(kind=="lifecycle"){o.parts[0].contact=Support::absent;g.now=g.last_contact+t["age"].get<Ns>();}
   if(kind=="deadline"){g.plan_deadline=g.now+t["plan_offset"].get<Ns>();g.gate_deadline=g.now+t["gate_offset"].get<Ns>();}
   if(kind=="receipt"){execution=t["execution"];g.execution=execution;g.prefix=t["prefix"];g.receipt_unknown=t["receipt_unknown"];if(execution=="never_executed")g.attachment_query=-1;}
   const auto c=constrain(o,g);actual={{"invalid",c.invalid},{"move",c.move},{"down",c.down[0]},{"release",c.release},{"contact_id",c.contact_id}};for(const std::string f:{"invalid","move","down","release"})if(t.contains(f))pass&=actual[f]==t[f];pass&=c.contact_id==g.contact_id;
  }
  J checks=J::array();check(checks,t["name"],true,pass);report["supplemental"].push_back({{"name",t["name"]},{"kind",kind},{"actual",actual},{"assertions",checks}});
 }
 report["probes_max"]=probesMax;report["assertions"]=assertions;report["failed_assertions"]=failures;report["status"]=failures?"cold_fail_requires_classification":"cold_contract_only";report["real_png_audit_allowed"]=false;report["effect_typed_applicable"]=false;
 const int exit=aggregate.finalize(report);save(argv[7],report);std::cout<<"original=69+22 new=20 schema_controls=4 layers=4 assertions="<<assertions<<" failed="<<failures<<" metadata="<<Candidate::metadata_bytes()<<" probes_max="<<probesMax<<"\n";return exit;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
