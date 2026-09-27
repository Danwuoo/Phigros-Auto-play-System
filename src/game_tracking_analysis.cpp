#include "pas/game_tracking.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#define NOMINMAX
#include <windows.h>

namespace pas {
namespace {
using json=nlohmann::json;
struct Row {CandidateBatch batch;std::string clip;json truth;bool labeled=false;};
std::vector<Row> load_bank(const std::filesystem::path& path){
 if(std::filesystem::file_size(path)>64ULL*1024*1024)throw std::invalid_argument("tracking bank exceeds 64MiB");
 std::ifstream file(path);const auto j=json::parse(file);
 if(j.at("schema")!=1||!j.at("batches").is_array()||j.at("batches").empty()||j.at("batches").size()>2048)throw std::invalid_argument("tracking bank schema/capacity");
 std::vector<Row> rows;for(const auto& row:j.at("batches")){
  Row r{parse_candidate_batch(row.at("batch")),row.at("clip_id"),row.value("truth",json::array()),row.contains("truth")};
  if(r.clip.empty()||r.clip.size()>128||!r.truth.is_array()||r.truth.size()>128)throw std::invalid_argument("tracking truth/clip capacity");
  std::set<std::uint64_t> ids;for(const auto& t:r.truth){const auto id=t.at("candidate_id").get<std::uint64_t>();
   if(!ids.insert(id).second||std::none_of(r.batch.candidates.begin(),r.batch.candidates.end(),[&](const auto& c){return c.candidate_id==id;}))throw std::invalid_argument("truth candidate absent/duplicate");
   (void)t.at("instance_id").get<std::uint64_t>();}
  r.labeled=r.labeled&&r.truth.size()==r.batch.candidates.size();
  rows.push_back(std::move(r));
 }return rows;
}
json correctness(const std::vector<Row>& rows,const std::string& method,bool reupdate){
 GameTrackingComparison tracker(method,reupdate);FakeClock clock;FakeTouchBackend backend(clock);
 auto owner=std::make_unique<GamePlanOwner>(clock,backend,2,GameActionOptions{15,35'000'000,30'000'000});
 std::string clip;std::map<std::uint64_t,std::uint64_t> last_track,birth_truth,intent_truth;
 std::map<std::string,std::size_t> downs;std::set<std::uint64_t> prior_visible,ever_visible;
 std::size_t labeled=0,unknown=0,observations=0,identity_switch=0,fragment=0,false_merge=0,false_continuation=0,ambiguous=0,predictions=0;
 std::vector<double> crossing_errors;json per_clip=json::object(),per_clip_metrics=json::object();
 std::size_t receipts_seen=0,total_down=0,negative_down=0,unknown_down=0;
 for(const auto& row:rows){
  const auto switches_before=identity_switch,fragments_before=fragment,merges_before=false_merge,continuations_before=false_continuation,ambiguous_before=ambiguous;
  if(clip!=row.clip){owner->stop();owner=std::make_unique<GamePlanOwner>(clock,backend,2,GameActionOptions{15,35'000'000,30'000'000});tracker.reset();last_track.clear();birth_truth.clear();prior_visible.clear();ever_visible.clear();intent_truth.clear();clip=row.clip;}
  clock.set(row.batch.context.capture_ns);const auto result=tracker.update(row.batch);owner->accept(result.executable);owner->poll();
  std::map<std::uint64_t,std::uint64_t> candidate_truth;for(const auto& t:row.truth)candidate_truth[t.at("candidate_id")]=t.at("instance_id");
  if(row.labeled)++labeled;else ++unknown;
  std::map<std::uint64_t,std::uint64_t> current_birth;
  std::set<std::uint64_t> visible;
  for(const auto& o:result.observations){if(o.reason=="association_ambiguous"||o.reason=="birth_identity_competition"||o.reason=="retired_birth_ambiguous"||o.reason=="birth_candidate_competition")++ambiguous;
   if(o.prediction_only)++predictions;
   if(!o.prediction_only&&o.identity_supported){++observations;
    if(row.labeled&&candidate_truth.contains(o.candidate_id)){
     const auto id=candidate_truth.at(o.candidate_id);current_birth[o.birth_id]=id;if(!id){++false_continuation;continue;}
     visible.insert(id);
     if(last_track.contains(id)&&last_track[id]!=o.track_id)++identity_switch;
     if(ever_visible.contains(id)&&!prior_visible.contains(id))++fragment;
     if(birth_truth.contains(o.birth_id)&&birth_truth[o.birth_id]!=id)++false_merge;
     birth_truth[o.birth_id]=id;last_track[id]=o.track_id;ever_visible.insert(id);
    }
   }
  }
  for(const auto& t:result.executable.targets)if(t.crossing_ns)for(const auto& truth:row.truth)
   if(truth.contains("crossing_ns")&&current_birth.contains(t.note_id)&&current_birth[t.note_id]==truth.at("instance_id").get<std::uint64_t>())crossing_errors.push_back((*t.crossing_ns-truth.at("crossing_ns").get<Nanoseconds>())/1e6);
  for(const auto& p:owner->take_accepted_plans())if(current_birth.contains(p.note_id))intent_truth[p.intent_id]=current_birth[p.note_id];
  owner->take_coverage_updates();owner->take_plan_cancellations();
  for(;receipts_seen<backend.receipts().size();++receipts_seen){const auto& receipt=backend.receipts()[receipts_seen];if(receipt.command.phase==Phase::down){++total_down;
   if(!intent_truth.contains(receipt.command.intent_id))++unknown_down;else if(!intent_truth[receipt.command.intent_id])++negative_down;else ++downs[clip+"/"+std::to_string(intent_truth[receipt.command.intent_id])];}}
  auto& metrics=per_clip_metrics[clip];if(metrics.is_null())metrics={{"identity_switches",0},{"fragments",0},{"false_merges",0},{"false_continuations",0},{"ambiguous_rejections",0}};
  metrics["identity_switches"]=metrics.at("identity_switches").get<std::size_t>()+identity_switch-switches_before;
  metrics["fragments"]=metrics.at("fragments").get<std::size_t>()+fragment-fragments_before;
  metrics["false_merges"]=metrics.at("false_merges").get<std::size_t>()+false_merge-merges_before;
  metrics["false_continuations"]=metrics.at("false_continuations").get<std::size_t>()+false_continuation-continuations_before;
  metrics["ambiguous_rejections"]=metrics.at("ambiguous_rejections").get<std::size_t>()+ambiguous-ambiguous_before;
  prior_visible=visible;per_clip[clip]=per_clip.value(clip,std::size_t{0})+1;
 }
 owner->stop();std::size_t duplicate_down=0;for(const auto& [id,n]:downs){(void)id;if(n>1)duplicate_down+=n-1;}
 const bool complete=labeled==rows.size();
 return {{"unique_frames",rows.size()},{"unique_clip_frames",per_clip},{"labeled_frames",labeled},{"unknown_frames",unknown},
  {"current_identity_observations",observations},{"identity_switches",complete?json(identity_switch):json(nullptr)},
  {"track_fragments_reacquisition",complete?json(fragment):json(nullptr)},{"false_merges",complete?json(false_merge):json(nullptr)},
  {"false_continuations",complete?json(false_continuation):json(nullptr)},{"ambiguous_rejections",ambiguous},{"prediction_only_outputs",predictions},
  {"fake_duplicate_down",complete?json(duplicate_down):json(nullptr)},{"fake_instances_with_down",downs.size()},
  {"fake_down_calls",total_down},{"fake_known_negative_down",complete?json(negative_down):json(nullptr)},{"fake_unknown_truth_down_calls",unknown_down},
  {"fake_stop_contacts",backend.contacts().size()},{"synthetic_crossing_error_ms",distribution(crossing_errors)},
  {"per_clip_metrics",complete?per_clip_metrics:json(nullptr)},
  {"candidate_recall",nullptr},{"candidate_recall_reason","requires independent visible-instance annotation, not candidate truth alone"},
  {"game_judgment","unknown"},{"definitions","switch=changed track per labeled instance; fragment=observed after absent; false merge=birth assigned to different labeled instance; metrics exclude unknown truth"}};
}
}
json analyze_tracking_bank(const std::filesystem::path& path,const std::vector<std::string>& methods,int updates,bool reupdate){
 if(methods.empty()||methods.size()>3||updates<1||updates>100000)throw std::invalid_argument("tracking comparison methods/updates");
 std::set<std::string> unique;for(const auto& method:methods){if(!unique.insert(method).second)throw std::invalid_argument("duplicate method");GameTrackingComparison check(method,reupdate&&method=="oc_observation");}
 const auto rows=load_bank(path);json result={{"schema",1},{"bank_sha256",sha256_file(path)},{"offline_only",true},{"touch_backend","FakeTouchBackend"},{"real_input_created",false},
  {"comparison_scope","association isolation on shared candidates"},{"full_pipeline_comparison","pending"},{"default_method","legacy"},{"line_tracking","baseline selection unchanged"},
  {"performance_pass",nullptr},{"qpc_clock",true},{"compiler_msc_full_ver",_MSC_FULL_VER},{"logical_processors",GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)},
  {"performance_updates_are_replays",true},{"unique_frames",rows.size()},{"warmup_updates",500},{"methods",json::object()},{"batch_order",json::array()}};
 std::set<std::string> sources,clips;for(const auto& row:rows){sources.insert(row.batch.history_source);clips.insert(row.clip);}
 result["history_sources"]=sources;result["unique_clips"]=clips.size();
 for(const auto& method:methods)result["methods"][method]={{"observation_reupdate",reupdate&&method=="oc_observation"},{"correctness",correctness(rows,method,reupdate&&method=="oc_observation")},{"performance_batches",json::array()}};
 HostClock clock;result["qpc_frequency"]=clock.frequency();
 for(int batch=0;batch<3;++batch){json order=json::array();for(std::size_t k=0;k<methods.size();++k){const auto& method=methods[(k+batch)%methods.size()];order.push_back(method);
   GameTrackingComparison tracker(method,reupdate&&method=="oc_observation");std::string prior;std::vector<double> times;times.reserve(updates);
   for(int i=-500;i<updates;++i){const auto index=static_cast<std::size_t>(i+500)%rows.size();const auto& row=rows[index];if(index==0||prior!=row.clip)tracker.reset();prior=row.clip;
    const auto begin=clock.now_ns();const auto ignored=tracker.update(row.batch);const auto elapsed=clock.now_ns()-begin;(void)ignored;if(i>=0)times.push_back(elapsed/1e6);}
   result["methods"][method]["performance_batches"].push_back({{"batch",batch},{"tracker_update_ms",distribution(times)}});
  }result["batch_order"].push_back(order);}
 return result;
}
void write_tracking_challenge(const std::filesystem::path& path){
 if(std::filesystem::exists(path))throw std::invalid_argument("challenge output exists");json rows=json::array();
 for(int clip=0;clip<12;++clip)for(int i=0;i<12;++i){CandidateBatch b;b.history_source="synthetic_geometric_candidates";b.ui=GameUi::playing;b.playing_gate=true;
  const std::array<int,6> dt{{10,16,33,51,16,10}};Nanoseconds t=0;for(int j=0;j<=i;++j)t+=static_cast<Nanoseconds>(dt[j%6])*1'000'000;
  if(clip==8&&i>=5)t+=441'000'000;b.context={static_cast<std::uint64_t>(clip+1),1,1,static_cast<std::uint64_t>(i+1),t,1280,720,1};
  b.lines.push_back({{640,576+(clip==6?(i%2?1.56:0):clip==7?i*2.:0)},{1,0},1080,3,1});
  if(clip==9)b.lines.push_back({{715,360},{0,1},600,3,.5});json truth=json::array();
  const int count=clip==2||clip==3||clip==4?2:1;
  for(int n=0;n<count;++n){if((clip==1||clip==8)&&i==5)continue;TrackingCandidate c;c.candidate_id=n+1;c.note.kind=clip==4&&n==1?NoteKind::tap:NoteKind::hold;
   c.note.center={500+n*(clip==3?4.:80.),488+t/1e9*400};c.note.tangent={1,0};c.note.width=60;c.note.height=c.note.kind==NoteKind::hold?120:8;
   c.note.rails_geometry=c.left_rail=c.right_rail=c.body_visible=c.note.kind==NoteKind::hold;c.note.tail=Vec2{c.note.center.x,c.note.center.y-120};
   c.quality=clip==0&&i>=4&&i<=6?ObservationQuality::weak_current:ObservationQuality::strong_current;c.action_support=true;c.head_visible=true;
   if(clip==5)c.quality=ObservationQuality::weak_current;if(clip==10&&i>=5)c.head_visible=c.action_support=false;
   if(clip==11&&i>=5)c.note.kind=NoteKind::drag;b.candidates.push_back(c);
   json label={{"candidate_id",c.candidate_id},{"instance_id",clip==5?std::uint64_t{0}:static_cast<std::uint64_t>(clip*10+n+1)}};if(clip!=5&&clip!=6&&clip!=7)label["crossing_ns"]=220'000'000;truth.push_back(label);
  }
  rows.push_back({{"clip_id","synthetic-"+std::to_string(clip)},{"batch",candidate_batch_json(b)},{"truth",truth}});
 }
 if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());std::ofstream out(path);if(!(out<<json{{"schema",1},{"data_class","synthetic_geometric_regression_not_game_pixels"},{"batches",rows}}.dump(2)<<'\n'))throw std::runtime_error("challenge write");
}
} // namespace pas
