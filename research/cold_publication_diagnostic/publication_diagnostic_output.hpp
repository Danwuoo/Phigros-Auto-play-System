#pragma once
#include "publication_diagnostic.hpp"
#include "evidence_hash.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// Post-join output only. The destructor is declared before the producer and
// its runtime/writer objects, so stack unwinding joins/stops them first.
namespace cold::publication_diag {
using Json=nlohmann::json;
inline constexpr std::size_t segment_rows=2048,max_segments=5,row_bytes=1536,file_bytes=4*1024*1024;
static_assert(segment_rows*row_bytes+4096<file_bytes);
inline Json strings_json(const StringFacts& s){return Json::array({s.before_size,s.before_capacity,s.after_size,s.after_capacity,s.before_available,s.assignment_returned,s.data_pointer_changed});}
inline Json row_json(const Attempt& a){
 const auto& t=a.wait;const auto& g=a.gap;
 return Json::array({a.attempt,a.attempt>=257,a.due_ns,a.entered,a.completed,a.exception_seen,a.invalid_clock,a.published_ok,
  static_cast<unsigned>(a.publish_outcome),a.present_mask,a.read_failure_mask,a.stamps,
  Json::array({t.first_qpc_ns,t.last_qpc_ns,t.qpc_reads,t.kernel_calls,t.spin_enter_ns,t.spin_return_ns,t.spin_poll_count,t.spin_pause_calls,
   static_cast<unsigned>(t.phase),static_cast<unsigned>(t.failure_phase),static_cast<unsigned>(t.failure),t.entered,t.completed,t.spin_entered,t.spin_completed}),
  Json::array({g.max_gap_ns,g.gap_begin_ns,g.gap_end_ns,g.end_sample_index,g.sample_count,g.interval_count,g.available,g.invalid}),
  strings_json(a.frame_backend),strings_json(a.slot_backend),strings_json(a.slot_format)});
}
class Output final {
 std::filesystem::path root_;
 const std::vector<Attempt>& rows_;
 std::string mode_,scene_;
 int requested_,cadence_;
 bool saved_=false;
 Json result_;
 static Json file(const std::filesystem::path& path,const Json& value){
  if(std::filesystem::exists(path)||std::filesystem::is_symlink(std::filesystem::symlink_status(path)))throw std::runtime_error("fresh publication diagnostic metadata");
  const auto bytes=value.dump();if(bytes.size()+1>file_bytes)throw std::runtime_error("publication diagnostic metadata 4MiB cap");
  {std::ofstream out(path,std::ios::binary);out<<bytes<<'\n';out.flush();if(!out)throw std::runtime_error("publication diagnostic metadata write");}
  return {{"path",std::filesystem::absolute(path).string()},{"bytes",std::filesystem::file_size(path)},
   {"sha256",pas::evidence::sha256(path,file_bytes)}};
 }
public:
 Output(const std::string& report,const std::vector<Attempt>& rows,std::string mode,std::string scene,int requested,int cadence)
  :root_(report+".publication-diagnostic"),rows_(rows),mode_(std::move(mode)),scene_(std::move(scene)),requested_(requested),cadence_(cadence){
  if(requested<1000||requested>10000||rows.size()!=std::size_t(requested)+slot_extra)throw std::runtime_error("original requested bound and fixed diagnostic slots");
  // This fresh directory is inside the original report ledger. Its bytes must
  // be declared by the future runner's capacity reservation, not ignored.
  if(!std::filesystem::create_directory(root_))throw std::runtime_error("fresh publication diagnostic directory");
 }
 Json save(const char* completion){
  if(saved_)return result_;
  Json segments=Json::array();std::size_t entered=0,completed=0,partial=0,invalid=0,read_failed=0;
  for(std::size_t begin=0;begin<rows_.size();begin+=segment_rows){
   const auto end=std::min(rows_.size(),begin+segment_rows);Json rows=Json::array();
   for(std::size_t i=begin;i<end;++i){const auto& a=rows_[i];
    if(a.wait.kernel_calls>pas::current_rails::producer_wait_kernel_cap||a.wait.spin_poll_count>pas::current_rails::producer_active_wait_poll_cap||
       a.gap.sample_count>cold::poll_gap::sample_cap||a.gap.interval_count>cold::poll_gap::interval_cap)throw std::runtime_error("unchanged bounded wait diagnostic counters");
    auto row=row_json(a);if(row.dump().size()>row_bytes)throw std::runtime_error("combined publication diagnostic numeric row cap");
    rows.push_back(std::move(row));entered+=a.entered;completed+=a.completed;partial+=a.entered&&!a.completed;invalid+=a.invalid_clock;read_failed+=a.read_failure_mask!=0;
   }
   const auto number=segments.size();if(number>=max_segments)throw std::runtime_error("publication diagnostic 5 segment cap");
   const auto pin=file(root_/("segment-"+std::to_string(number)+".json"),{{"schema","pas.cold-publication-diagnostic-segment.v1"},
    {"segment",number},{"first_slot",begin+1},{"last_slot",end},{"rows",rows}});
   segments.push_back({{"first_slot",begin+1},{"last_slot",end},{"rows",end-begin},{"file",pin}});
  }
  Json summary={{"slots",rows_.size()},{"entered",entered},{"completed",completed},{"partial",partial},
   {"invalid_clock_slots",invalid},{"diagnostic_read_failure_slots",read_failed},{"unused_slots",rows_.size()-entered}};
  const Json index={{"schema","pas.cold-publication-diagnostic.v1"},{"status",completion},{"mode",mode_},{"scene",scene_},
   {"requested",requested_},{"warmup",256},{"measurement_attempts",requested_-256},{"cadence_ms",cadence_},
   {"point_names",point_names},{"attempt_columns",{"attempt","measurement","due_ns","entered","completed","exception_seen","invalid_clock","published_ok",
    "publish_outcome","present_mask","read_failure_mask","point_ns","wait","poll_gap","frame_backend","slot_backend","slot_format"}},
   {"wait_columns",{"first_qpc_ns","last_qpc_ns","qpc_reads","kernel_calls","spin_enter_ns","spin_return_ns","spin_poll_count","spin_pause_calls",
    "phase","failure_phase","failure","entered","completed","spin_entered","spin_completed"}},
   {"poll_gap_columns",{"max_gap_ns","begin_ns","end_ns","end_sample_index","sample_count","interval_count","available","invalid"}},
   {"string_columns",{"before_size","before_capacity","after_size","after_capacity","before_available","assignment_returned","data_pointer_changed"}},
   {"publish_outcomes",{"not_entered","entered","closed","pool_drop","returned_true"}},
   {"clock_domain","actual HostClock converted QPC ns; not raw QPC ticks"},{"wait_policy","full_period_active_qpc"},
   {"horizon_ns",pas::current_rails::producer_active_wait_horizon_ns},{"poll_cap",pas::current_rails::producer_active_wait_poll_cap},
   {"wait_clock_forwarding","each original wait read is forwarded once; no extra per-poll QPC or OS API"},
   {"point_clock_failure","extra diagnostic read is caught, marks unavailable/invalid and never replaces the original business exception"},
   {"gap_validity","available && !invalid; only adjacent original wait reads; not CPU residency or a scheduler/root-cause verdict"},
   {"string_facts","capacity and address-change facts locate storage changes, not an allocation duration or allocator root cause"},
   {"slot_bytes",sizeof(Attempt)},{"slot_byte_cap",slot_byte_bound},{"row_byte_cap",row_bytes},{"segment_rows",segment_rows},
   {"segment_cap",max_segments},{"file_byte_cap",file_bytes},{"summary",summary},{"segments",segments},
   {"diagnostic_build_identity",{{"status","FRESH_DIAGNOSTIC_ONLY"},{"source_commit",PAS_PUBLICATION_DIAGNOSTIC_SOURCE_COMMIT},{"source_dirty",bool(PAS_PUBLICATION_DIAGNOSTIC_SOURCE_DIRTY)},
    {"base_compiled_source_commit","3ee69edc85acb6b9b968424374a376f351d0e2dd"},{"new_binary_bound_by_campaign_and_owned_Native8",true}}},
   {"instrumentation_overhead_measured",false},{"diagnostic_only",true},{"qualification_issued",false},{"cost_qualification",false},
   {"normal_qualification",false},{"AA_qualification",false},{"noise_qualification",false},{"ABBA_qualification",false},{"live_ready",false},{"device_operations",0}};
  result_={{"index",file(root_/"index.json",index)},{"summary",summary},{"diagnostic_only",true},{"qualification_issued",false}};saved_=true;return result_;
 }
 ~Output(){if(!saved_)try{save("joined_unwind_or_partial");}catch(const std::exception& e){std::cerr<<"partial publication diagnostic save failed: "<<e.what()<<'\n';}}
};
}
