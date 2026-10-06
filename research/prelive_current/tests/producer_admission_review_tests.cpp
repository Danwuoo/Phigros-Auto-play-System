// Independent synthetic raw-evidence tests. These do not run the pipeline,
// Windows processes, or a physical device and cannot qualify real cost.
#define main reviewed_cost_analysis_entry
#include "../cost_analysis.cpp"
#undef main
#include "../producer_timing.hpp"
#include <sstream>
int raw_audit_entry(int,char**);
namespace {
J dist(std::vector<double> v){if(v.empty())return {{"n",0}};std::sort(v.begin(),v.end());auto q=[&](double x){double t=(v.size()-1)*x;auto a=std::size_t(t),b=std::min(a+1,v.size()-1);return v[a]+(v[b]-v[a])*(t-a);};return {{"n",v.size()},{"p50",q(.5)},{"p95",q(.95)},{"p99",q(.99)},{"max",v.back()},{"jitter_p95_minus_p5",q(.95)-q(.05)}};}
void save_json(const fs::path& p,const J& j){std::ofstream f(p,std::ios::binary);f<<j.dump(2)<<'\n';if(!f)throw std::runtime_error("fixture write");}
J make_case(const fs::path& root,const std::string& name,const std::string& mutation,const fs::path& self){
 constexpr std::int64_t epoch=1000000000,cadence=20000000;constexpr int n=2560;const auto p=root/(name+".json"),audit_path=root/(name+"-audit.json");J attempts=J::array(),events=J::array();
 const J provenance={{"schema","synthetic-producer-admission-review-only"},{"case",name}};
 events.push_back({{"event","current_chain_provenance"},{"provenance",provenance}});
 events.push_back({{"event","producer_schedule"},{"epoch_ns",epoch},{"cadence_ms",20},{"requested",n},{"generator","closed_reset_v2_streamed_all_offsets"}});
 pas::cost_evidence::ProducerTiming timing;std::vector<double> total,owner_time,publish,mt,mo,mh,mr,publication_times;std::uint64_t publication_overruns=0;
 for(int i=1;i<=n;++i){const auto due=epoch+std::int64_t(i-1)*cadence,start=due+1000000;auto end=due+3000000;
  if((mutation=="honest_overrun"||mutation=="hidden_overrun")&&i==777)end=due+21000000;
  const auto published=end+500000,owner=end+2000000,complete=owner+1000000;auto publication_return=end+750000;
  if((mutation=="honest_publication_overrun"||mutation=="hidden_publication_overrun")&&i==777)publication_return=due+21000000;
  publication_times.push_back(double(publication_return-start)/1000000);publication_overruns+=publication_return>due+cadence;
  J row={{"attempt",i},{"producer_due_ns",due},{"render_start_ns",start},{"render_end_ns",end},{"publication_return_ns",publication_return},{"capture",end},{"published_ok",true},{"consumed",true},{"owned",true},{"published",published},{"owner_complete",owner},{"journal_enqueue_complete",complete},{"observer_ns",600000},{"hook_ns",200000},{"owner_ns",200000},{"writer_ok",true}};
  timing.observe(row,20);attempts.push_back(row);total.push_back(3);owner_time.push_back(2);publish.push_back(.5);if(i>256){mt.push_back(2);mo.push_back(.2);mh.push_back(.2);mr.push_back(.6);}
  if(i==257){events.push_back({{"event","own_accepted_plan"},{"epoch",1},{"generation",1},{"geometry",1},{"note",1},{"intent",1},{"revision",1},{"source_frame",i},{"evidence",end},{"valid_until",end+100000000},{"prefix_offset",0},{"basis","synthetic_admission_fixture"},{"steps",J::array({{{"phase",0},{"x",450},{"y",500},{"due",end+100000}},{{"phase",2},{"x",450},{"y",500},{"due",end+200000}}})}});
   for(int phase:{0,2}){const auto scheduled=end+(phase==0?100000:200000);events.push_back({{"event","own_injection_receipt"},{"intent",1},{"source_frame",i},{"contact",0},{"x",450},{"y",500},{"phase",phase},{"scheduled",scheduled},{"injection_start",scheduled+50000},{"injection_return",scheduled+60000},{"success",true},{"reason","synthetic test double"}});}}
  events.push_back({{"event","complete_current_chain"},{"frame",i},{"capture",end}});
 }
 const auto finish=epoch+std::int64_t(n)*cadence;events.push_back({{"event","release_receipt"},{"requested",J::array()},{"failed",J::array()},{"unknown",J::array()},{"start",finish},{"return",finish+1000}});
 J r={{"schema","pas.prelive-complete-chain-cost.v1"},{"unit_test_double",true},{"mode","A"},{"scene","hold-closed-reset-v2"},{"requested",n},{"attempts",n},{"warmup_attempts",256},{"cadence_ms",20},{"consumed",n},{"owner_seen",n},{"owner_seen_fraction",1},{"published",n},{"pool_drops",0},{"consumer_skips",0},{"unconsumed",0},{"writer_ok",n},{"writer_drops",0},{"writer_fault",false},{"journal_admission_failed",false},{"safe",true},{"supervisor_stopped",false},{"injection_failed",0},{"release_verified",true},{"own_down",1},{"own_move",0},{"own_up",1},{"receipt_count",2},{"release_calls",1},{"capture_to_owner_journal_ms",dist(total)},{"capture_to_owner_ms",dist(owner_time)},{"capture_to_publish_ms",dist(publish)},{"measurement",{{"attempts",2304},{"owner_seen",2304},{"owner_seen_fraction",1},{"capture_to_owner_ms",dist(mt)},{"observer_ms",dist(mr)},{"hook_ms",dist(mh)},{"owner_ms",dist(mo)}}},{"injection_ms",dist({.01,.01})},{"all_phase_lateness_ms",dist({.05,.05})},{"evidence_to_injection_ms",dist({.15,.25})},{"phase_lateness_ms",{{"down",dist({.05})},{"move",dist({})},{"up",dist({.05})}}},{"final_release",{{"requested",J::array()},{"failed",J::array()},{"unknown",J::array()},{"start",finish},{"return",finish+1000}}},{"clock_domain","host_QPC"},{"source_absolute_age",nullptr},{"device_endpoints",0},{"rss_peak_os",64*MiB},{"provenance",provenance},{"producer_epoch_ns",epoch},{"producer_render_ms",dist(timing.render_ms)},{"producer_schedule_lateness_ms",dist(timing.lateness_ms)},{"render_start_to_owner_ms",dist(timing.render_to_owner_ms)},{"producer_deadline_overruns",timing.overruns},{"render_start_to_publication_return_ms",dist(publication_times)},{"publication_deadline_overruns",publication_overruns},{"renderer_storage_bytes",2764800},{"generator","closed_reset_v2_streamed_all_offsets"}};
 if(mutation=="summary_tamper")r["producer_render_ms"]["p99"]=999;
 if(mutation=="hidden_overrun")r["producer_deadline_overruns"]=0;
 if(mutation=="hidden_publication_overrun")r["publication_deadline_overruns"]=0;
 if(mutation=="publication_summary_tamper")r["render_start_to_publication_return_ms"]["p99"]=999;
 if(mutation=="missing_publication_return")attempts[776].erase("publication_return_ns");
 if(mutation=="missing_raw_field")attempts[776].erase("render_start_ns");
 if(mutation=="wrong_due_step")attempts[776]["producer_due_ns"]=attempts[776]["producer_due_ns"].get<std::int64_t>()+1;
 if(mutation=="future_due_shift")for(auto& row:attempts)row["producer_due_ns"]=row["producer_due_ns"].get<std::int64_t>()+1000000000;
 if(mutation=="schedule_anchor_tamper")events[1]["epoch_ns"]=epoch+1;
 if(mutation=="missing_schedule_event")events.erase(1);
 if(mutation=="missing_report_fields")for(const auto* key:{"producer_epoch_ns","producer_render_ms","producer_schedule_lateness_ms","render_start_to_owner_ms","producer_deadline_overruns","publication_deadline_overruns","render_start_to_publication_return_ms"})r.erase(key);
 const auto journal=p.string()+".journal.jsonl";{std::ofstream f(journal,std::ios::binary);for(const auto& e:events)f<<e.dump()<<'\n';if(!f)throw std::runtime_error("fixture Journal write");}
 r["journal_records"]=events.size();r["journal_admitted_bytes"]=fs::file_size(journal)+1024;save_json(p,r);save_json(p.string()+".attempts.json",attempts);
 std::string a0=self.string(),a1=p.string(),a2=audit_path.string();char* args[]={a0.data(),a1.data(),a2.data()};std::ostringstream errors;auto* old=std::cerr.rdbuf(errors.rdbuf());const auto rc=raw_audit_entry(3,args);std::cerr.rdbuf(old);{std::ofstream f(root/(name+".audit.stderr"));f<<errors.str();}
 J result={{"case",name},{"mutation",mutation},{"audit_exit",rc},{"audit_stderr",errors.str()},{"raw_report_sha256",pas::evidence::sha256(p)},{"raw_attempts_sha256",pas::evidence::sha256(p.string()+".attempts.json")},{"raw_journal_sha256",pas::evidence::sha256(journal)},{"integrity",false},{"producer_metrics_verified",false},{"numeric_cost_gate",false},{"producer_admission_component",false}};
 if(rc==0){auto a=read(audit_path);result["audit_errors"]=a["errors"];result["integrity"]=a["integrity"];result["producer_metrics_verified"]=a["producer_metrics_verified"];r["_audited_measurement"]={{"phase_counts",a["measurement_phase_counts"]},{"lateness",a["measurement_all_phase_lateness_ms"]}};auto v=validity(r);result["numeric_cost_gate"]=v["pass"];result["cost_failures"]=v["failures"];result["producer_admission_component"]=a["integrity"].get<bool>()&&a["producer_metrics_verified"].get<bool>()&&v["pass"].get<bool>();}
 return result;
}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("fresh output root required");const fs::path root=fs::absolute(argv[1]);if(fs::exists(root))throw std::runtime_error("fresh root required");fs::create_directories(root);J checks=J::array();
 for(const auto* mutation:{"none","summary_tamper","missing_raw_field","wrong_due_step","honest_overrun","hidden_overrun","honest_publication_overrun","hidden_publication_overrun","publication_summary_tamper","missing_publication_return","future_due_shift","schedule_anchor_tamper","missing_schedule_event","missing_report_fields"}){auto r=make_case(root,mutation,mutation,fs::canonical(argv[0]));bool pass=false;const auto error=r.value("audit_stderr","");
  if(std::string_view(mutation)=="none")pass=r["producer_admission_component"].get<bool>();
  else if(std::string_view(mutation)=="missing_raw_field")pass=r["audit_exit"]==2&&error.find("render_start_ns")!=std::string::npos;
  else if(std::string_view(mutation)=="missing_publication_return")pass=r["audit_exit"]==2&&error.find("publication_return_ns")!=std::string::npos;
  else if(std::string_view(mutation)=="wrong_due_step")pass=r["audit_exit"]==2&&error.find("producer fixed cadence sequence")!=std::string::npos;
  else if(std::string_view(mutation)=="future_due_shift")pass=r["audit_exit"]==2&&error.find("producer current render order")!=std::string::npos;
  else if(std::string_view(mutation)=="honest_overrun"||std::string_view(mutation)=="honest_publication_overrun")pass=r["integrity"].get<bool>()&&r["producer_metrics_verified"].get<bool>()&&!r["numeric_cost_gate"].get<bool>();
  else {const std::map<std::string,std::string> expected{{"summary_tamper","distribution:producer_render_ms"},{"hidden_overrun","producer_overrun_count"},{"hidden_publication_overrun","publication_overrun_count"},{"publication_summary_tamper","distribution:render_start_to_publication_return_ms"},{"schedule_anchor_tamper","producer_schedule_anchor_join"},{"missing_schedule_event","producer_schedule_anchor_join"},{"missing_report_fields","producer_schedule_without_raw_contract"}};const auto wanted=expected.at(mutation);r["expected_error"]=wanted;const auto errors=r.value("audit_errors",J::array());pass=r["audit_exit"]==0&&!r["integrity"].get<bool>()&&!r["producer_metrics_verified"].get<bool>()&&!r["producer_admission_component"].get<bool>()&&std::find(errors.begin(),errors.end(),wanted)!=errors.end();}
  r["pass"]=pass;checks.push_back(r);if(std::string_view(mutation)=="none"&&!pass){save_json(root/"review_result.json",{{"pass",false},{"checks",checks}});throw std::runtime_error("valid raw baseline must pass before mutations");}}
 bool pass=true;for(const auto& c:checks)pass=pass&&c["pass"].get<bool>();J result={{"schema","pas.producer-admission-independent-review.v1"},{"pass",pass},{"checks",checks},{"scope","Synthetic raw rows and Journal fed through real C++ auditor and numeric analyzer validity. Campaign/process qualification is covered separately; this is not measured Windows pipeline cost."},{"actual_cost_ready",false},{"live_ready",false}};save_json(root/"review_result.json",result);std::cout<<result.dump(2)<<'\n';return pass?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
