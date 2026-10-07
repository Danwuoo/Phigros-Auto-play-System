#include <nlohmann/json.hpp>
#include "../prelive_current/evidence_hash.hpp"
#include "../prelive_current/producer_timing.hpp"
#include "../../include/pas/lossless_event_stream.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <limits>
#include <optional>
#include <set>
#include <vector>
using J=nlohmann::json;namespace fs=std::filesystem;
namespace {
constexpr std::uint64_t stream_cap=1073741824ull+65536;
J read(const fs::path&p,std::size_t cap){if(fs::file_size(p)>cap)throw std::runtime_error("file bound");std::ifstream f(p);if(!f)throw std::runtime_error("input open");return J::parse(f);}
template<class F> std::size_t stream(const fs::path&p,F fn){if(fs::file_size(p)>stream_cap)throw std::runtime_error("stream bound");pas::LosslessEventReader f(p,p.extension()==".gz"?4294967296ull:stream_cap);std::string line;std::size_t n=0;while(f.next(line)){if(++n>65536||line.size()>262144||line.empty())throw std::runtime_error("row bound");fn(J::parse(line));}return n;}
J binding(const fs::path&p){return {{"path",fs::absolute(p).lexically_normal().string()},{"bytes",fs::file_size(p)},{"sha256",pas::evidence::sha256(p)}};}
J distribution(std::vector<double> v){if(v.empty())return {{"n",0}};std::sort(v.begin(),v.end());auto p=[&](double x){const double a=(v.size()-1)*x;const auto lo=std::size_t(a),hi=std::min(lo+1,v.size()-1);return v[lo]+(v[hi]-v[lo])*(a-lo);};return {{"n",v.size()},{"p50",p(.5)},{"p95",p(.95)},{"p99",p(.99)},{"max",v.back()},{"jitter_p95_minus_p5",p(.95)-p(.05)}};}
double milliseconds(std::int64_t end,std::int64_t start){if((start>0&&end<std::numeric_limits<std::int64_t>::min()+start)||(start<0&&end>std::numeric_limits<std::int64_t>::max()+start))throw std::runtime_error("timestamp difference overflow");return (end-start)/1e6;}
bool same_number(const J&a,const J&b){if(!a.is_number()||!b.is_number())return false;const double x=a.get<double>(),y=b.get<double>();return std::isfinite(x)&&std::isfinite(y)&&std::abs(x-y)<=1e-8+std::abs(y)*1e-10;}
using Key=std::pair<int,std::uint64_t>;
struct Step{int phase;double x,y;std::int64_t due;};
struct Plan{std::uint64_t note,revision,source,offset,epoch,generation,geometry;std::int64_t evidence,until;std::vector<Step>steps;};
struct Receipt{Key key;int contact,phase;std::uint64_t source,cursor;std::int64_t scheduled,start,end;bool success,coordinates;double x=0,y=0;};
struct State{std::uint64_t cursor=0;bool terminal=false,unknown=false;int contact=-1;};
struct Cancel{std::int64_t time=0,scheduled=0,end=0;std::uint64_t source=0;bool expired=false;};
}
int main(int argc,char**argv){try{
 if(argc!=3||fs::exists(argv[2]))throw std::runtime_error("fresh report");
 const fs::path p=argv[1];
 const auto verifier_binary=binding(argv[0]);J bindings={{"report",binding(p)}};const auto r=read(p,1024*1024);const auto schema=r.at("schema").get<std::string>();
 const bool pixel=schema=="pas.prelive-current-pixels.v1";if(!pixel&&schema!="pas.prelive-complete-chain-cost.v1")throw std::runtime_error("unsupported report schema");
 const auto encoding=r.value("journal_event_encoding",std::string("plain-jsonl-v1"));if(encoding!="plain-jsonl-v1"&&encoding!="gzip-jsonl-v1")throw std::runtime_error("journal encoding");if(encoding=="gzip-jsonl-v1"&&(pixel||r.at("requested")!=10000))throw std::runtime_error("gzip stress scope");
 const auto rows_path=p.string()+(pixel?".rows.jsonl":".attempts.json"),events_path=p.string()+(pixel?".events.jsonl":encoding=="gzip-jsonl-v1"?".journal.jsonl.gz":".journal.jsonl");
 bindings[pixel?"rows":"attempts"]=binding(rows_path);bindings[pixel?"events":"journal"]=binding(events_path);
 std::set<std::string> error_set;auto check=[&](bool b,const std::string&s){if(!b)error_set.insert(s);};
 const bool loss=!pixel&&(r.at("writer_fault").get<bool>()||r.at("writer_drops").get<std::int64_t>()!=0||r.at("journal_admission_failed").get<bool>());
 std::size_t consumed=0,owned=0,published=0,written=0,measurement_owned=0,consumer_skips=0,last_consumed=0;std::set<int>written_frames;std::map<int,std::int64_t>captures;std::map<Key,std::int64_t>source_frames;
 std::vector<double> total,owner_time,publish_time,mt,mo,mh,mr,pixel_compute;
 J recomputed=J::object();bool metrics_verified=true,all_input=false,producer_metrics_verified=false;pas::cost_evidence::ProducerTiming producer_timing;
 auto verify_distribution=[&](const J&actual,const std::string&field,const std::vector<double>&values){const auto expected=distribution(values);recomputed[field]=expected;bool ok=actual.is_object()&&actual.contains("n")&&actual.at("n")==expected.at("n");for(const auto&[key,value]:expected.items())if(key!="n")ok=ok&&actual.contains(key)&&same_number(actual.at(key),value);if(expected.at("n")==0)ok=ok&&actual.size()==1;check(ok,"distribution:"+field);metrics_verified&=ok;};
 if(!pixel){
  const auto a=read(rows_path,12*1024*1024);if(!a.is_array()||a.empty()||a.size()>10000)throw std::runtime_error("attempt count bound");
  check(a.size()==r.at("requested")&&a.size()==r.at("attempts"),"attempt_length_and_reported_count");all_input=a.size()==r.at("requested")&&a.size()==r.at("attempts");
  const auto warmup=r.at("warmup_attempts").get<std::size_t>();check(warmup==256&&warmup<a.size(),"warmup_contract");
  const bool producer_fields=r.contains("producer_render_ms");
  std::int64_t prior=0;for(std::size_t i=0;i<a.size();++i){const auto&s=a[i];if(producer_fields)producer_timing.observe(s,r.at("cadence_ms").get<std::int64_t>());check(s.at("attempt")==i+1,"attempt_sequence");const auto capture=s.at("capture").get<std::int64_t>();check(capture>0&&capture>=prior,"capture_order");prior=capture;captures[int(i+1)]=capture;source_frames[{0,i+1}]=capture;
   const bool pub=s.at("published_ok"),con=s.at("consumed"),own=s.at("owned"),write=s.at("writer_ok");published+=pub;consumed+=con;owned+=own;written+=write;if(write)written_frames.insert(int(i+1));check(!con||pub,"consume_unpublished");check(!own||con,"own_unconsumed");check(!write||con,"write_unconsumed");
   if(con){if(last_consumed)consumer_skips+=i+1-last_consumed-1;last_consumed=i+1;const auto complete=s.at("journal_enqueue_complete").get<std::int64_t>(),owner=s.at("owner_complete").get<std::int64_t>(),publish=s.at("published").get<std::int64_t>();check(complete>=owner&&owner>=publish&&publish>=capture,"chain_time_order");total.push_back(milliseconds(complete,capture));publish_time.push_back(milliseconds(publish,capture));}
   if(own){const auto observer=s.at("observer_ns").get<std::int64_t>(),hook=s.at("hook_ns").get<std::int64_t>(),owner=s.at("owner_ns").get<std::int64_t>(),finish=s.at("owner_complete").get<std::int64_t>();check(observer>=0&&hook>=0&&owner>=0,"compute_time_order");owner_time.push_back(milliseconds(finish,capture));if(i>=warmup){++measurement_owned;mt.push_back(milliseconds(finish,capture));mo.push_back(owner/1e6);mh.push_back(hook/1e6);mr.push_back(observer/1e6);}}
  }
  if(producer_fields){const auto before=error_set.size();verify_distribution(r.at("producer_render_ms"),"producer_render_ms",producer_timing.render_ms);verify_distribution(r.at("producer_schedule_lateness_ms"),"producer_schedule_lateness_ms",producer_timing.lateness_ms);verify_distribution(r.at("render_start_to_owner_ms"),"render_start_to_owner_ms",producer_timing.render_to_owner_ms);check(r.at("producer_deadline_overruns")==producer_timing.overruns,"producer_overrun_count");verify_distribution(r.at("render_start_to_publication_return_ms"),"render_start_to_publication_return_ms",producer_timing.render_to_publication_ms);check(r.at("publication_deadline_overruns")==producer_timing.publication_overruns,"publication_overrun_count");producer_metrics_verified=before==error_set.size();}
  check(consumed==r.at("consumed")&&owned==r.at("owner_seen")&&published==r.at("published"),"reported_counts");check(published+r.at("pool_drops").get<std::size_t>()==a.size(),"publish_denominator");check(written==r.at("writer_ok"),"writer_count");check(a.size()-consumed==r.at("unconsumed"),"unconsumed_count");check(consumer_skips==r.at("consumer_skips"),"consumer_skips_count");check(same_number(r.at("owner_seen_fraction"),double(owned)/a.size()),"full_coverage_fraction");
  const auto&m=r.at("measurement");check(m.at("attempts")==a.size()-warmup&&m.at("owner_seen")==measurement_owned&&same_number(m.at("owner_seen_fraction"),double(measurement_owned)/(a.size()-warmup)),"measurement_denominator");
  verify_distribution(r.at("capture_to_owner_journal_ms"),"capture_to_owner_journal_ms",total);verify_distribution(r.at("capture_to_owner_ms"),"capture_to_owner_ms",owner_time);verify_distribution(r.at("capture_to_publish_ms"),"capture_to_publish_ms",publish_time);
  verify_distribution(m.at("capture_to_owner_ms"),"measurement.capture_to_owner_ms",mt);verify_distribution(m.at("observer_ms"),"measurement.observer_ms",mr);verify_distribution(m.at("hook_ms"),"measurement.hook_ms",mh);verify_distribution(m.at("owner_ms"),"measurement.owner_ms",mo);
 }else{
  std::map<int,std::pair<std::uint64_t,std::int64_t>>prior;const auto n=stream(rows_path,[&](const J&s){++consumed;check(s.at("ordinal").get<int>()>0,"pixel_ordinal");const int window=s.at("window");const auto source=s.at("source_frame").get<std::uint64_t>();const auto capture=s.at("capture").get<std::int64_t>();auto&last=prior[window];check(source>last.first&&capture>=last.second&&s.at("pixels_ready")>=capture,"pixel_source_order");last={source,capture};source_frames[{window,source}]=capture;pixel_compute.push_back(s.at("complete_ms").get<double>());});
  check(n==r.at("processed"),"pixel_rows_count");all_input=r.at("processed")==r.at("frames");check(all_input,"pixel_input_incomplete");verify_distribution(r.at("compute_ms"),"compute_ms",pixel_compute);
 }
 std::map<Key,std::vector<Plan>>plans;std::map<Key,std::uint64_t>note_intents;std::map<Key,State>states;std::map<Key,std::vector<Cancel>>cancellations;std::vector<Receipt>receipts;std::array<std::optional<Key>,5>active{};std::map<std::string,int>basis;std::set<int>complete;
 std::size_t releases=0,failed=0,provenance_events=0,producer_schedule_events=0;bool producer_schedule_matches=true;bool provenance_matches=true;const J run_provenance=r.value("provenance",J(nullptr));int window=0;bool release_uncertainty=false,coordinate_complete=true,plan_complete=!loss;J release_unknown=J::array();
 auto no_active=[&](){return std::none_of(active.begin(),active.end(),[](const auto&x){return bool(x);});};
 const auto rows=stream(events_path,[&](const J&e){const auto type=e.value("event","");if(pixel&&e.value("window",0)!=window){check(no_active(),"window_reset_active");window=e.at("window");}
  if(type=="producer_schedule"){++producer_schedule_events;producer_schedule_matches=producer_schedule_matches&&r.contains("producer_epoch_ns")&&e.at("epoch_ns")==r.at("producer_epoch_ns")&&e.at("cadence_ms")==r.at("cadence_ms")&&e.at("requested")==r.at("requested")&&e.at("generator")==r.at("generator");}
  if(type=="current_chain_provenance"){++provenance_events;provenance_matches=provenance_matches&&e.contains("provenance")&&e.at("provenance")==run_provenance;}
  if(type=="own_accepted_plan"){
   const Key key{window,e.at("intent")};Plan a{e.at("note"),e.at("revision"),e.at("source_frame"),e.at("prefix_offset"),e.at("epoch"),e.at("generation"),e.at("geometry"),e.at("evidence"),e.at("valid_until"),{}};
   check(key.second&&a.note&&a.revision&&a.source&&a.epoch&&a.generation&&a.geometry,"plan_identity_context");check(source_frames.contains({window,a.source})&&source_frames[{window,a.source}]==a.evidence,"plan_source_evidence_join");const Key note_key{window,a.note};if(note_intents.contains(note_key))check(note_intents[note_key]==key.second,"note_identity_rebirth");else note_intents[note_key]=key.second;check(e.at("steps").is_array()&&e.at("steps").size()>=2&&e.at("steps").size()<=16,"plan_steps_bound");if(e.at("steps").size()>16)throw std::runtime_error("plan step bound");
   for(const auto&s:e.at("steps")){Step step{s.at("phase"),s.at("x"),s.at("y"),s.at("due")};check(step.phase>=0&&step.phase<=2&&std::isfinite(step.x)&&std::isfinite(step.y)&&step.x>=0&&step.x<1280&&step.y>=0&&step.y<720,"plan_step_geometry");if(!a.steps.empty())check(step.due>=a.steps.back().due,"plan_step_time_order");a.steps.push_back(step);}
   check(!a.steps.empty()&&a.steps.back().phase==2&&(a.offset||a.steps.front().phase==0),"plan_phase_order");for(std::size_t i=1;i+1<a.steps.size();++i)check(a.steps[i].phase==1,"plan_middle_phase");
   auto&versions=plans[key];if(!versions.empty()){const auto&old=versions.back();check(a.note==old.note&&a.epoch==old.epoch&&a.generation==old.generation&&a.geometry==old.geometry,"intent_note_context_changed");check(a.revision>old.revision&&a.source>=old.source&&a.evidence>=old.evidence&&a.offset>=old.offset,"plan_revision_order");}else check(a.offset==0,"initial_plan_prefix");versions.push_back(std::move(a));++basis[e.at("basis").get<std::string>()];
  }
  if(type=="game_contact_cancelled")cancellations[{window,e.at("intent_id")}].push_back({e.at("cancel_ns"),0,0,0,false});
  if(type=="game_contact_up"&&e.value("reason","")=="target_evidence_expired")cancellations[{window,e.at("intent_id")}].push_back({0,e.at("scheduled_ns"),e.at("injection_return_ns"),e.at("source_frame"),true});
  if(type=="own_injection_receipt"){
   if(receipts.size()>=20000)throw std::runtime_error("receipt bound");
   Receipt a{{window,e.at("intent")},e.at("contact"),e.at("phase"),e.at("source_frame"),0,e.at("scheduled"),e.at("injection_start"),e.at("injection_return"),e.at("success"),e.contains("x")&&e.contains("y")};
   if(a.coordinates){a.x=e.at("x");a.y=e.at("y");check(std::isfinite(a.x)&&std::isfinite(a.y)&&a.x>=0&&a.x<1280&&a.y>=0&&a.y<720,"receipt_coordinates");}else coordinate_complete=false;
   check(a.contact>=0&&a.contact<5&&a.phase>=0&&a.phase<=2,"contact_phase_bounds");check(a.end>=a.start&&a.start>=a.scheduled&&a.scheduled>=0,"receipt_time_order");if(a.contact<0||a.contact>=5||a.phase<0||a.phase>2)return;
   auto&state=states[a.key];a.cursor=state.cursor;
   if(!loss){check(!state.terminal&&!state.unknown,"terminal_intent_replayed");if(a.phase==0)check(!active[a.contact]&&state.contact==-1&&state.cursor==0,"contact_duplicate_down");else check(active[a.contact]&&*active[a.contact]==a.key&&state.contact==a.contact,"move_up_without_same_intent_down");}
   if(a.phase==0){active[a.contact]=a.key;state.contact=a.contact;}
   if(!a.success){++failed;state.unknown=true;}else{++state.cursor;if(a.phase==2){active[a.contact].reset();state.terminal=true;}}
   receipts.push_back(a);
  }
  if(type=="release_receipt"){
   ++releases;check(e.at("return")>=e.at("start"),"release_time_order");std::set<int>requested,failed_ids,unknown_ids;
   for(int id:e.at("requested"))check(id>=0&&id<5&&requested.insert(id).second,"release_requested_bounds_duplicate");
   if(!loss)for(int id=0;id<5;++id)if(active[id])check(requested.contains(id),"release_missing_active_contact");
   for(int id:e.at("failed"))check(requested.contains(id)&&failed_ids.insert(id).second,"release_failed_subset");
   for(int id:e.at("unknown"))check(requested.contains(id)&&!failed_ids.contains(id)&&unknown_ids.insert(id).second,"release_unknown_subset");
   if(!failed_ids.empty()||!unknown_ids.empty()){release_uncertainty=true;release_unknown.push_back(e);}
   for(int id:requested)if(id>=0&&id<5&&!failed_ids.contains(id)&&!unknown_ids.contains(id)){if(active[id])states[*active[id]].terminal=true;active[id].reset();}
  }
  if(type=="complete_current_chain"){const int id=e.at("frame");check(complete.insert(id).second,"complete_frame_duplicate");if(!pixel){check(captures.contains(id),"complete_frame_unknown");if(e.contains("capture"))check(e.at("capture")==captures[id],"complete_capture_join");}}
 });
 std::array<int,3>phases{},measurement_phases{};std::vector<double>measurement_late;const auto measurement_start=!pixel&&captures.contains(257)?captures.at(257):0;std::vector<double>injection,late,evidence_to_injection;std::array<std::vector<double>,3>phase_late;
 for(const auto&a:receipts){++phases[a.phase];injection.push_back(milliseconds(a.end,a.start));late.push_back(milliseconds(a.start,a.scheduled));phase_late[a.phase].push_back(late.back());if(!pixel&&measurement_start&&a.start>=measurement_start){++measurement_phases[a.phase];measurement_late.push_back(late.back());}
  if(!pixel){check(a.source<=10000&&captures.contains(int(a.source)),"receipt_source_attempt_missing");if(a.source<=10000&&captures.contains(int(a.source)))evidence_to_injection.push_back(milliseconds(a.start,captures[int(a.source)]));}
  bool match=false;const auto found=plans.find(a.key);if(found!=plans.end())for(const auto&plan:found->second){if(plan.source!=a.source||a.cursor<plan.offset||a.cursor-plan.offset>=plan.steps.size())continue;const auto&step=plan.steps[std::size_t(a.cursor-plan.offset)];if(step.phase==a.phase&&step.due==a.scheduled&&(!a.coordinates||(step.x==a.x&&step.y==a.y))){match=true;break;}}
  // Explicit cancellation/expiry Up is a safety command outside the planned
  // future suffix. It still requires the same intent, source and last point.
  if(!match&&a.phase==2&&a.cursor>0&&found!=plans.end())for(const auto&cancel:cancellations[a.key]){
   const bool cancellation=cancel.expired?(cancel.scheduled==a.scheduled&&cancel.end==a.end&&cancel.source==a.source):(cancel.time>0&&cancel.time<=a.scheduled&&a.scheduled-cancel.time<=100000000);
   if(!cancellation)continue;
   for(const auto&plan:found->second){if(plan.source!=a.source||a.cursor<=plan.offset||a.cursor-plan.offset>plan.steps.size())continue;const auto&last=plan.steps[std::size_t(a.cursor-plan.offset-1)];if(!a.coordinates||(last.x==a.x&&last.y==a.y)){match=true;break;}}
  }
  if(!match){plan_complete=false;if(!loss)check(false,"receipt_plan_step_source_cursor_join");}
 }
 if(!loss){check(phases[0]==r.at("own_down")&&phases[1]==r.at("own_move")&&phases[2]==r.at("own_up"),"receipt_phase_counts");if(!pixel){check(rows==r.at("journal_records"),"journal_rows_count");check(complete==written_frames,"complete_frame_join");check(failed==r.at("injection_failed"),"backend_failure_count");check(receipts.size()==r.at("receipt_count")&&releases==r.at("release_calls"),"receipt_release_counts");check(fs::file_size(events_path)<=r.at("journal_admitted_bytes"),"journal_physical_bound");
   verify_distribution(r.at("injection_ms"),"injection_ms",injection);verify_distribution(r.at("all_phase_lateness_ms"),"all_phase_lateness_ms",late);verify_distribution(r.at("evidence_to_injection_ms"),"evidence_to_injection_ms",evidence_to_injection);const std::array<std::string,3>names{"down","move","up"};for(std::size_t i=0;i<3;++i)verify_distribution(r.at("phase_lateness_ms").at(names[i]),"phase_lateness_ms."+names[i],phase_late[i]);
  }else if(r.contains("event_count"))check(rows==r.at("event_count"),"pixel_event_count");
 }else metrics_verified=false;
 if(r.at("release_verified").get<bool>()){check(no_active(),"final_contact_active");check(!release_uncertainty,"release_verified_overrides_uncertainty");}
 for(const auto&[name,b]:bindings.items())check(binding(b.at("path").get<std::string>())==b,"input_changed_during_audit:"+name);
 check(binding(verifier_binary.at("path").get<std::string>())==verifier_binary,"verifier_binary_changed");
 if(!pixel&&r.contains("producer_render_ms")){const bool joined=producer_schedule_events==1&&producer_schedule_matches&&r.at("producer_epoch_ns")==producer_timing.first_due;check(joined,"producer_schedule_anchor_join");producer_metrics_verified&=joined;}else if(producer_schedule_events){check(false,"producer_schedule_without_raw_contract");producer_metrics_verified=false;}
 const bool provenance_complete=!pixel&&run_provenance.is_object()&&provenance_events==1&&provenance_matches;
 if(!pixel&&(!run_provenance.is_null()||provenance_events))check(provenance_complete,"run_provenance_join");
 J errors=J::array();for(const auto&e:error_set)errors.push_back(e);
 const bool complete_prefix=!loss&&all_input&&plan_complete&&coordinate_complete&&!release_uncertainty&&error_set.empty();
 J out={{"schema","pas.prelive-independent-integrity.v2"},{"path",p.string()},{"input_bindings",bindings},{"verifier_binary",verifier_binary},{"integrity",errors.empty()},{"errors",errors},{"complete_prefix_evidence",complete_prefix},{"declared_writer_loss",loss},{"metrics_verified",metrics_verified},{"producer_metrics_verified",producer_metrics_verified},{"provenance_join_complete",provenance_complete},{"run_provenance",run_provenance},{"plan_join_complete",plan_complete},{"coordinate_join_complete",coordinate_complete},{"release_uncertainty_seen",release_uncertainty},{"recomputed_distributions",recomputed},{"event_rows",rows},{"receipts",receipts.size()},{"phases",phases},{"measurement_window_start_ns",measurement_start},{"measurement_phase_counts",measurement_phases},{"measurement_all_phase_lateness_ms",distribution(measurement_late)},{"release_calls",releases},{"unknown_release",release_unknown},{"plan_basis_rows",basis},{"all_input_processed",all_input},{"device_endpoints",0},{"game_adoption","unknown"}};
 std::ofstream f(argv[2]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("write");std::cout<<(errors.empty()?"integrity verified":"integrity failed")<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
