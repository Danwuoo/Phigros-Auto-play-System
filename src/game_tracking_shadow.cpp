#include "pas/game_tracking_shadow.hpp"
#include "pas/analysis.hpp"
#include <fstream>
#include <stdexcept>

namespace pas {
TrackingShadow::TrackingShadow(const std::string& method):method_(method){
 if(method!="byte_association"&&method!="oc_observation")throw std::invalid_argument("shadow requires byte_association or oc_observation");
 updates_.reserve(100000);residency_.reserve(100000);
 worker_=std::jthread([this]{try{GameTrackingComparison tracker(method_);HostClock clock;
  for(;;){std::optional<CandidateBatch> value;
   {std::unique_lock lock(mutex_);cv_.wait(lock,[&]{return stopping_||slot_.has_value();});if(stopping_)break;value=std::move(slot_);slot_.reset();}
   const auto begin=clock.now_ns();const auto result=tracker.update(*value);const auto end=clock.now_ns();
   {std::lock_guard lock(mutex_);++processed_;if(updates_.size()<100000){updates_.push_back((end-begin)/1e6);residency_.push_back((end-value->context.capture_ns)/1e6);}
    for(const auto& o:result.observations){predictions_+=o.prediction_only;action_evidence_+=o.action_evidence_valid;}}
  }}catch(const std::exception& e){std::lock_guard lock(mutex_);fault_=e.what();stopping_=true;slot_.reset();}});
}
TrackingShadow::~TrackingShadow(){stop();}
void TrackingShadow::submit(const CandidateBatch& b){std::lock_guard lock(mutex_);if(stopping_)return;++submitted_;if(slot_)++skipped_;slot_=b;cv_.notify_one();}
void TrackingShadow::stop(){{std::lock_guard lock(mutex_);if(slot_){++skipped_;slot_.reset();}stopping_=true;}cv_.notify_all();if(worker_.joinable())worker_.join();}
nlohmann::json TrackingShadow::stats() const{std::lock_guard lock(mutex_);return {{"method",method_},{"mailbox_capacity",1},{"submitted",submitted_},{"processed",processed_},{"skipped",skipped_},
 {"tracker_update_ms",distribution(updates_)},{"capture_to_shadow_completion_ms",distribution(residency_)},{"prediction_only_outputs",predictions_},{"action_qualified_diagnostic_outputs",action_evidence_},
 {"fault",fault_},{"backend_created",false},{"full_pipeline_comparison","pending"},{"capture_lease_retained",false}};}
TrackingBankRecorder::TrackingBankRecorder(std::size_t budget):budget_(budget){
 charged_=2048*sizeof(CandidateBatch);if(budget<charged_||budget>16*1024*1024)throw std::invalid_argument("bank memory budget");batches_.reserve(2048);
}
void TrackingBankRecorder::append(const CandidateBatch& b){
 if(b.candidates.size()>128||b.lines.size()>16||b.history_source.size()>128){++dropped_;return;}
 std::size_t bytes=b.lines.size()*sizeof(LineCandidate)+b.candidates.size()*sizeof(TrackingCandidate)+256;
 for(const auto& c:b.candidates){if(c.origin.size()>128){++dropped_;return;}bytes+=256;}
 if(batches_.size()==2048||bytes>budget_-charged_){++dropped_;return;}batches_.push_back(b);charged_+=bytes;
}
nlohmann::json TrackingBankRecorder::stats() const{return {{"retained_batches",batches_.size()},{"dropped_batches",dropped_},{"charged_bytes_upper",charged_},{"memory_budget_bytes",budget_},{"capacity",2048},
 {"truth","unknown"},{"history_source","baseline_guided"},{"retention","first bounded batches; subsequent batches dropped"}};}
void TrackingBankRecorder::save(const std::filesystem::path& path,const nlohmann::json& provenance) const{
 if(std::filesystem::exists(path))throw std::runtime_error("bank output exists");std::ofstream out(path);if(!out)throw std::runtime_error("bank write");
 // Stream after input stops, avoiding a second in-memory JSON tree of the bank.
 out<<"{\"schema\":1,\"provenance\":"<<provenance.dump()<<",\"stats\":"<<stats().dump()<<",\"batches\":[";
 bool first=true;for(const auto& b:batches_){if(!first)out<<',';first=false;const auto& c=b.context;
  out<<nlohmann::json{{"clip_id","context-"+std::to_string(c.epoch)+"-"+std::to_string(c.generation)+"-"+std::to_string(c.geometry)},
   {"batch",candidate_batch_json(b)}}.dump();}out<<"]}\n";if(!out)throw std::runtime_error("bank write failed");
}
} // namespace pas
