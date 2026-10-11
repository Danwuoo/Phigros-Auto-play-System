#pragma once
#include "pas/candidate_admission.hpp"
#include "pas/core.hpp"
#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>
#include <vector>

namespace pas {
struct CandidateReceiptCollectionRequest final {
    CandidateAdmissionRequest inputs;
    std::filesystem::path output;
    std::string game_package;
};
// A completed cold bundle is only permission to obtain the separately scoped
// qualification receipts. It cannot be converted to normal live admission.
class CandidateCollectionInputs final {
public:
    CandidateCollectionInputs()=default;
    const nlohmann::json& documents()const{return documents_;}
    const nlohmann::json& context()const{return context_;}
    void verify_unchanged()const{if(!check_)throw std::runtime_error("current candidate collection NOT_READY: cold input missing");check_();}
private:
    nlohmann::json documents_,context_;
    std::function<void()> check_;
    CandidateCollectionInputs(nlohmann::json d,nlohmann::json c,std::function<void()> f):documents_(std::move(d)),context_(std::move(c)),check_(std::move(f)){}
    friend CandidateCollectionInputs prepare_candidate_collection(const CandidateAdmissionRequest&);
};
CandidateCollectionInputs prepare_candidate_collection(const CandidateAdmissionRequest&);
class CandidatePhysicalPermit final {
public:CandidatePhysicalPermit()=default;
private:
    std::function<void()> check_;
    std::filesystem::path config_,capability_;
    CandidatePhysicalPermit(std::function<void()> check,std::filesystem::path config,std::filesystem::path cap):check_(std::move(check)),config_(std::move(config)),capability_(std::move(cap)){}
    void require(const std::filesystem::path& config,const std::filesystem::path& cap)const{
        if(!check_||config!=config_||cap!=capability_)throw std::runtime_error("current candidate physical qualification NOT_READY: actual receipt-bound permit required");check_();
    }
    friend void collect_candidate_physical_receipt(const CandidateReceiptCollectionRequest&);
    template<class F>friend void enter_candidate_physical_qualification(const CandidatePhysicalPermit&,const std::filesystem::path&,const std::filesystem::path&,F&&);
};
template<class F>void enter_candidate_physical_qualification(const CandidatePhysicalPermit& permit,const std::filesystem::path& config,const std::filesystem::path& cap,F&& f){permit.require(config,cap);std::forward<F>(f)();}

struct CandidatePhysicalFrame final {
    std::uint64_t frame=0;
    std::array<std::uint64_t,6> opportunities{}; // Tap, Hold entry/body/tail, held rotation, late alignment.
    std::uint64_t owner_unknown=0;
    bool same_current_frame=false;
};
inline bool candidate_none_result_confirmed(bool none,bool ended,bool result_state,bool saved_image){return none?(ended&&result_state):saved_image;}
class CandidatePhysicalAggregate final {
    mutable std::mutex mutex_;
    std::array<std::uint64_t,6> opportunities_{};
    std::uint64_t frames_=0,last_frame_=0,unknown_=0,failed_=0,receipts_=0;
    bool released_=false,result_=false,release_recorded_=false;
public:
    void frame(const CandidatePhysicalFrame& v){std::lock_guard lock(mutex_);if(!v.frame||v.frame<=last_frame_||!v.same_current_frame){++unknown_;return;}++frames_;last_frame_=v.frame;unknown_+=v.owner_unknown;for(std::size_t i=0;i<6;++i)opportunities_[i]+=v.opportunities[i];}
    void receipt(bool success){std::lock_guard lock(mutex_);++receipts_;if(!success)++failed_;}
    void release(bool verified,bool result){std::lock_guard lock(mutex_);if(!release_recorded_){released_=verified;result_=result;release_recorded_=true;}if(!verified)++unknown_;}
    nlohmann::json report()const{std::lock_guard lock(mutex_);nlohmann::json rows=nlohmann::json::array();const char* names[]={"tap","hold_entry","hold_body","hold_tail","held_rotation","late_alignment"};for(std::size_t i=0;i<6;++i)rows.push_back({{"phase",names[i]},{"owner_opportunities",opportunities_[i]}});return {{"schema","pas.current-physical-owner-aggregate.v1"},{"frames",frames_},{"receipts",receipts_},{"owner_unknown",unknown_},{"failed_injection",failed_},{"release_verified",released_},{"result_confirmed",result_},{"phases",rows},{"physical_gold",false},{"qualification_issued",false}};}
};

// These are the existing capture bench windows, not three artificial songs.
inline constexpr std::int64_t candidate_capture_ready_ns=15'000'000'000LL;
inline constexpr std::int64_t candidate_capture_warmup_ns=10'000'000'000LL;
inline constexpr std::int64_t candidate_capture_measurement_ns=60'000'000'000LL;
inline bool candidate_capture_metrics_valid(const nlohmann::json& p){try{
    const auto count=[](const auto& x){return (x.is_number_integer()||x.is_number_unsigned())&&x.template get<std::int64_t>()>=0;};
    for(const auto* key:{"attempted_notifications","accepted_frames","inactive_notifications","invalid_notifications","relative_stale_drops","source_gaps","failed","unknown","ready_budget_ns","warmup_ns","measurement_ns"})if(!count(p.at(key)))return false;
    if(p.at("failed")!=0||p.at("unknown")!=0||p.at("invalid_notifications")!=0||p.at("accepted_frames")==0||p.at("attempted_notifications").get<std::uint64_t>()!=p.at("accepted_frames").get<std::uint64_t>()+p.at("inactive_notifications").get<std::uint64_t>()+p.at("invalid_notifications").get<std::uint64_t>()+p.at("relative_stale_drops").get<std::uint64_t>())return false;
    if(!p.at("window_complete").is_boolean()||!p.at("window_complete").get<bool>()||p.at("ready_budget_ns")!=candidate_capture_ready_ns||p.at("warmup_ns")!=candidate_capture_warmup_ns||p.at("measurement_ns")!=candidate_capture_measurement_ns||p.at("source_absolute_age")!=nullptr||p.at("performance_pass")!=nullptr)return false;
    for(const auto* key:{"ready_start_ns","first_frame_ns","measurement_start_ns","measurement_end_ns"})if(!count(p.at(key)))return false;
    if(p.at("first_frame_ns").get<std::int64_t>()<p.at("ready_start_ns").get<std::int64_t>()||p.at("first_frame_ns").get<std::int64_t>()-p.at("ready_start_ns").get<std::int64_t>()>candidate_capture_ready_ns)return false;
    if(p.at("measurement_start_ns").get<std::int64_t>()-p.at("first_frame_ns").get<std::int64_t>()!=candidate_capture_warmup_ns||p.at("measurement_end_ns").get<std::int64_t>()-p.at("measurement_start_ns").get<std::int64_t>()<candidate_capture_measurement_ns)return false;
    for(const auto* key:{"full_rpc_read_ms","measurement_rpc_read_ms","measurement_copy_ms","measurement_host_residence_ms"}){const auto& d=p.at(key);if(!count(d.at("n"))||d.at("n")==0||d.at("n").get<std::uint64_t>()>100000)return false;double previous=0;for(const auto* q:{"p50","p95","p99","max"}){if(!d.at(q).is_number())return false;const auto v=d.at(q).get<double>();if(!std::isfinite(v)||v<previous)return false;previous=v;}}
    return p.at("full_rpc_read_ms").at("n")==p.at("accepted_frames")&&p.at("measurement_rpc_read_ms").at("n").get<std::uint64_t>()<=p.at("accepted_frames").get<std::uint64_t>()&&p.at("measurement_rpc_read_ms").at("n")==p.at("measurement_copy_ms").at("n")&&p.at("measurement_copy_ms").at("n")==p.at("measurement_host_residence_ms").at("n");
}catch(...){return false;}}

inline bool candidate_collection_review_mode(const nlohmann::json& input){
    const bool owner=input.contains("physical_owner_observation"),review=input.contains("physical_review");
    if(owner!=review)throw std::runtime_error("physical review NOT_READY: owner observation and independent review must both be pinned");
    return owner;
}
// A native launch of pas.exe is evidence for this collector only when the
// complete command and frozen collection input join the current prerequisites.
// This file-only shape check cannot create a permit or an admission.
inline bool candidate_collection_protocol_valid(const nlohmann::json& argv,const std::string& role,
    const nlohmann::json& frozen_input,const nlohmann::json& prior,const nlohmann::json& current,
    const nlohmann::json& output,const std::string& workspace,const std::string& game_package,
    const nlohmann::json& owner_pin=nlohmann::json{},const nlohmann::json& review_pin=nlohmann::json{}){try{
    using J=nlohmann::json;
    const auto path=[](const J& v){if(!v.is_string()||v.template get<std::string>().empty())throw std::invalid_argument("path");auto s=std::filesystem::path(v.template get<std::string>()).lexically_normal().generic_string();std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;};
    const auto pin=[&](const J& a,const J& b){return a.is_object()&&b.is_object()&&path(a.at("path"))==path(b.at("path"))&&a.at("bytes").is_number_integer()&&b.at("bytes").is_number_integer()&&a.at("bytes")==b.at("bytes")&&a.at("sha256").is_string()&&a.at("sha256")==b.at("sha256");};
    const char* command=role=="device"?"candidate-collect-device":role=="capture_rpc"?"candidate-collect-capture-rpc":(role=="physical_owner"||role=="physical")?"candidate-collect-physical":nullptr;
    if(!command||!argv.is_array()||argv.size()!=15||argv[0]!=command)return false;
    const char* flags[]={"--admission-input","--admission-sha256","--workspace","--config","--capability","--output","--game-package"};
    for(std::size_t i=0;i<7;++i)if(argv[1+i*2]!=flags[i]||!argv[2+i*2].is_string())return false;
    if(path(argv[2])!=path(frozen_input.at("path"))||argv[4]!=frozen_input.at("sha256")||path(argv[6])!=path(workspace)||path(argv[8])!=path(current.at("runtime_profile").at("path"))||path(argv[10])!=path(current.at("capability").at("path"))||path(argv[12])!=path(output.at("path"))||argv[14]!=game_package)return false;
    if(prior.at("schema")!="pas.current-candidate-collection-input.v1"||prior.at("evidence_kind")!="actual"||prior.at("source_commit")!=current.at("source_commit"))return false;
    if(prior.contains("game_package")&&prior.at("game_package")!=game_package)return false;
    for(const auto* k:{"candidate_binary","source_manifest","production_build","cold_preflight_input","runtime_profile","capability","production_cost","authorization"})if(!pin(prior.at(k),current.at(k)))return false;
    const auto predecessor=[&](const char* k){return pin(prior.at(k),current.at(k))&&prior.at("qualification_processes").at(k)==current.at("qualification_processes").at(k);};
    if(role!="device"&&!predecessor("device"))return false;
    if((role=="physical_owner"||role=="physical")&&!predecessor("capture_rpc"))return false;
    const bool review=candidate_collection_review_mode(prior);
    if(role=="physical")return review&&pin(prior.at("physical_owner_observation"),owner_pin)&&pin(prior.at("physical_review"),review_pin)&&prior.at("qualification_processes").at("physical_owner")==current.at("qualification_processes").at("physical_owner");
    return !review;
}catch(...){return false;}}
inline bool candidate_fixture_mapping_valid(const nlohmann::json& f){try{if(f.at("schema")!="pas.current-fixture-mapping-aggregate.v1"||!f.at("pass").is_boolean()||!f.at("pass").get<bool>()||f.at("failed")!=0||f.at("unknown")!=0||f.at("final_active_contacts")!=0||!f.at("release_verified").is_boolean()||!f.at("release_verified").get<bool>()||f.at("max_contacts_verified")!=5||f.at("path_tolerance_px")!=4||!f.at("cases").is_array()||f.at("cases").size()!=6)return false;for(const auto* key:{"failed","unknown","final_active_contacts","max_contacts_verified","path_tolerance_px"})if(!f.at(key).is_number_integer())return false;const char* names[]={"corner-0","corner-1","corner-2","corner-3","hold_move","five"};for(std::size_t i=0;i<6;++i){const auto& row=f.at("cases")[i];const int attempts=i<4?2:(i==4?3:11);if(row.at("case")!=names[i]||!row.at("attempts").is_number_integer()||!row.at("verified").is_number_integer()||row.at("attempts")!=attempts||row.at("verified")!=attempts||!row.at("pass").is_boolean()||!row.at("pass").get<bool>())return false;}return true;}catch(...){return false;}}
// This verifies a typed independent review of the completed actual observation.
// It never treats the candidate's opportunity counters as semantic gold.
inline bool candidate_physical_review_valid(const nlohmann::json& owner,const nlohmann::json& review,const nlohmann::json& owner_pin){try{
    const auto yes=[](const auto& j,const char* k){return j.at(k).is_boolean()&&j.at(k).template get<bool>();};
    const auto count=[](const auto& v){return (v.is_number_integer()||v.is_number_unsigned())&&v.template get<std::int64_t>()>=0;};
    if(owner.at("schema")!="pas.current-physical-owner-observation.v1"||owner.at("evidence_kind")!="actual"||review.at("schema")!="pas.current-physical-independent-review.v1"||review.at("evidence_kind")!="actual"||review.at("owner_observation")!=owner_pin)return false;
    for(const auto* k:{"source_commit","candidate_binary_sha256","profile_sha256","capability_sha256","device_receipt","capture_rpc_receipt"})if(owner.at(k)!=review.at(k))return false;
    if(!yes(review,"reviewed_after_owner")||!yes(review,"independent_of_candidate")||!review.at("candidate_predictions_used_as_gold").is_boolean()||yes(review,"candidate_predictions_used_as_gold")||review.at("review_basis")!="current_live_pixels"||!review.at("reviewer").is_string()||review.at("reviewer").get<std::string>().empty())return false;
    const auto& aggregate=owner.at("owner_aggregate");if(aggregate.at("schema")!="pas.current-physical-owner-aggregate.v1"||!count(aggregate.at("frames"))||aggregate.at("frames")==0||aggregate.at("failed_injection")!=0||aggregate.at("owner_unknown")!=0||!yes(aggregate,"release_verified")||!yes(aggregate,"result_confirmed")||yes(aggregate,"physical_gold")||yes(aggregate,"qualification_issued"))return false;
    for(const auto* key:{"failed_injection","owner_unknown","receipts"})if(!count(aggregate.at(key)))return false;
    if(!aggregate.at("physical_gold").is_boolean()||!aggregate.at("qualification_issued").is_boolean()||!candidate_fixture_mapping_valid(owner.at("fixture_mapping")))return false;
    const char* names[]={"tap","hold_entry","hold_body","hold_tail","held_rotation","late_alignment"};
    if(review.at("phases").size()!=6||aggregate.at("phases").size()!=6)return false;
    for(std::size_t i=0;i<6;++i){const auto& row=review.at("phases")[i];const auto& observed=aggregate.at("phases")[i];if(row.at("phase")!=names[i]||observed.at("phase")!=names[i]||!count(row.at("opportunities"))||row.at("opportunities")==0||!count(row.at("passed"))||row.at("passed")!=row.at("opportunities")||!count(row.at("failed"))||row.at("failed")!=0||!count(row.at("unknown"))||row.at("unknown")!=0)return false;}
    // The old 1519 case is an identity/association obligation, not a demand
    // for an ordinal, song, contact ID, or replay of the historical inputs.
    const auto& identity=review.at("identity_1519");for(const auto* key:{"opportunities","passed","failed","unknown"})if(!count(identity.at(key)))return false;if(identity.at("state")!="verified_current_identity_and_association"||identity.at("opportunities")==0||identity.at("passed")!=identity.at("opportunities")||identity.at("failed")!=0||identity.at("unknown")!=0||!yes(identity,"historical_evidence_unchanged"))return false;
    return true;
}catch(...){return false;}}

void collect_candidate_device_receipt(const CandidateReceiptCollectionRequest&);
void collect_candidate_capture_rpc_receipt(const CandidateReceiptCollectionRequest&);
void collect_candidate_physical_receipt(const CandidateReceiptCollectionRequest&);
void run_current_candidate_physical_qualification(const CandidatePhysicalPermit&,
    const std::string& config,const std::string& capability,CandidatePhysicalAggregate&);
}
