#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#if defined(PAS_PRODUCER_ELIGIBILITY_DELTA) && PAS_PRODUCER_ELIGIBILITY_DELTA
#include "producer_eligibility_delta_identity.hpp"
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pas::current_rails {
enum class WorkerAffinityFailure : std::uint8_t {
 none,process_query,normal_process_required,group_query,single_active_group_required,
 process_mask_query,process_mask_invalid,topology_query,topology_byte_bound,
 topology_read,topology_malformed,topology_core_bound,topology_lp_bound,
 topology_group,topology_incomplete,two_highest_class_cores_required,
 requested_mask_invalid,requested_mask_not_in_process,thread_query,affinity_query,
 original_group,original_mask,affinity_set,affinity_readback_query,affinity_readback_mismatch,
 wrong_thread,rollback_set,rollback_readback_query,rollback_readback_mismatch,
 restore_set,restore_readback_query,restore_readback_mismatch,fallback_api_exception
};
inline const char* worker_affinity_failure_name(WorkerAffinityFailure value) noexcept {
#define PAS_AFFINITY_REASON(name) case WorkerAffinityFailure::name:return "worker_affinity_" #name
 switch(value) {
 case WorkerAffinityFailure::none:return "none";
 PAS_AFFINITY_REASON(process_query);PAS_AFFINITY_REASON(normal_process_required);
 PAS_AFFINITY_REASON(group_query);PAS_AFFINITY_REASON(single_active_group_required);
 PAS_AFFINITY_REASON(process_mask_query);PAS_AFFINITY_REASON(process_mask_invalid);
 PAS_AFFINITY_REASON(topology_query);PAS_AFFINITY_REASON(topology_byte_bound);
 PAS_AFFINITY_REASON(topology_read);PAS_AFFINITY_REASON(topology_malformed);
 PAS_AFFINITY_REASON(topology_core_bound);PAS_AFFINITY_REASON(topology_lp_bound);
 PAS_AFFINITY_REASON(topology_group);PAS_AFFINITY_REASON(topology_incomplete);
 PAS_AFFINITY_REASON(two_highest_class_cores_required);PAS_AFFINITY_REASON(requested_mask_invalid);
 PAS_AFFINITY_REASON(requested_mask_not_in_process);PAS_AFFINITY_REASON(thread_query);
 PAS_AFFINITY_REASON(affinity_query);PAS_AFFINITY_REASON(original_group);PAS_AFFINITY_REASON(original_mask);
 PAS_AFFINITY_REASON(affinity_set);PAS_AFFINITY_REASON(affinity_readback_query);
 PAS_AFFINITY_REASON(affinity_readback_mismatch);PAS_AFFINITY_REASON(wrong_thread);
 PAS_AFFINITY_REASON(rollback_set);PAS_AFFINITY_REASON(rollback_readback_query);
 PAS_AFFINITY_REASON(rollback_readback_mismatch);PAS_AFFINITY_REASON(restore_set);
 PAS_AFFINITY_REASON(restore_readback_query);PAS_AFFINITY_REASON(restore_readback_mismatch);
 PAS_AFFINITY_REASON(fallback_api_exception);
 }
#undef PAS_AFFINITY_REASON
 return "worker_affinity_unknown_failure";
}
struct WorkerGroupAffinity {
 std::uint16_t group=0;std::uint64_t mask=0;
 bool operator==(const WorkerGroupAffinity&) const=default;
};
struct WorkerCore {std::uint16_t group=0;std::uint8_t efficiency_class=0;std::uint64_t mask=0;};
struct WorkerCoreTopology {
 static constexpr std::size_t byte_cap=65536,core_cap=256,logical_processor_cap=256;
 std::array<WorkerCore,core_cap> cores{};std::size_t count=0;
 std::uint32_t query_bytes=0,returned_bytes=0,win32_error=0;
 WorkerAffinityFailure failure=WorkerAffinityFailure::topology_incomplete;
};
inline constexpr const char* producer_eligibility_policy_id=
 "highest_raw_efficiency_class_one_lp_per_core_excluding_consumer";
inline constexpr std::uint32_t producer_eligibility_policy_version=2;
struct PlacementPlan {
 std::uint16_t group=0,consumer_core=0,producer_core=0;
 std::uint8_t consumer_number=0,producer_number=0,efficiency_class=0;
 std::uint32_t active_groups=0,process_class=0,topology_bytes=0,topology_error=0;
 std::size_t topology_cores=0,topology_logical_processors=0;
 std::uint64_t process_mask=0,system_mask=0,consumer_mask=0,producer_mask=0;
 std::uint64_t consumer_core_mask=0,producer_anchor_mask=0;
 std::size_t producer_eligible_core_count=0;
 // One group is at most 64 LPs. These fixed maps retain the exact bounded OS
 // topology without copying the 64KiB topology buffer into each snapshot.
 std::array<std::uint8_t,64> topology_core_by_lp{},topology_class_by_lp{};
 bool topology_available=false;
 WorkerAffinityFailure failure=WorkerAffinityFailure::none;
 std::uint64_t physical_core_mask(std::size_t core) const noexcept {
  std::uint64_t mask=0;for(unsigned lp=0;lp<64;++lp)
   if((system_mask&(std::uint64_t{1}<<lp))&&topology_core_by_lp[lp]==core)mask|=std::uint64_t{1}<<lp;
  return mask;
 }
 bool valid() const noexcept {
  if(failure!=WorkerAffinityFailure::none||!topology_available||active_groups!=1||process_class!=0x20||group!=0||
     !process_mask||!system_mask||(process_mask&~system_mask)||
     !topology_cores||topology_cores>WorkerCoreTopology::core_cap||
     !topology_bytes||topology_bytes>WorkerCoreTopology::byte_cap||
     topology_logical_processors!=std::size_t(std::popcount(system_mask))||
     consumer_number>=64||producer_number>=64||consumer_core>=topology_cores||producer_core>=topology_cores||
     consumer_core==producer_core||consumer_mask!=(std::uint64_t{1}<<consumer_number)||
     producer_anchor_mask!=(std::uint64_t{1}<<producer_number)||
     ((consumer_mask|producer_anchor_mask)&~process_mask))return false;
  unsigned highest=0;for(unsigned lp=0;lp<64;++lp)if(system_mask&(std::uint64_t{1}<<lp)){
   if(topology_core_by_lp[lp]>=topology_cores)return false;
   if(topology_class_by_lp[lp]>highest)highest=topology_class_by_lp[lp];
  }
  std::uint64_t expected=0;std::size_t eligible=0,highest_count=0;
  unsigned last=0,previous=0;
  for(std::size_t core=0;core<topology_cores;++core){
   const auto mask=physical_core_mask(core);if(!mask)return false;
   const auto lp=static_cast<unsigned>(std::countr_zero(mask));const auto raw_class=topology_class_by_lp[lp];
   for(unsigned bit=0;bit<64;++bit)if((mask&(std::uint64_t{1}<<bit))&&topology_class_by_lp[bit]!=raw_class)return false;
   if(raw_class!=highest)continue;
   if(highest_count==0||lp>last){previous=last;last=lp;}
   else if(highest_count==1||lp>previous)previous=lp;
   ++highest_count;
   const auto bit=std::uint64_t{1}<<lp;
   if(core!=consumer_core&&(bit&process_mask)){expected|=bit;++eligible;}
  }
  return highest_count>=2&&efficiency_class==highest&&consumer_number==previous&&producer_number==last&&
   physical_core_mask(consumer_core)==consumer_core_mask&&std::countr_zero(consumer_core_mask)==consumer_number&&
   topology_core_by_lp[consumer_number]==consumer_core&&topology_core_by_lp[producer_number]==producer_core&&
   std::countr_zero(physical_core_mask(producer_core))==producer_number&&producer_mask==expected&&
   producer_mask&&(producer_mask&producer_anchor_mask)==producer_anchor_mask&&
   !(producer_mask&consumer_core_mask)&&producer_eligible_core_count==eligible;
 }
};
// Policy reads bounded OS facts, never a diagnostic JSON or gameplay history.
// Choose the last two distinct cores ordered by each core's lowest LP in the
// highest raw EfficiencyClass; consumer takes the lower and the higher remains
// the producer identity anchor. The producer can run on every available lowest
// LP of that highest class, excluding the entire consumer physical core. An
// unavailable lowest LP never selects its sibling or a lower class. The original
// consumer and anchor must still be in the process mask. Core numbers are OS
// snapshot ordinals, not persistent hardware identities.
template<class Api> PlacementPlan make_worker_placement(Api& api) noexcept {
 PlacementPlan plan;const auto reject=[&](WorkerAffinityFailure reason){plan.failure=reason;return plan;};
 plan.process_class=api.process_class();if(!plan.process_class)return reject(WorkerAffinityFailure::process_query);
 if(plan.process_class!=0x20)return reject(WorkerAffinityFailure::normal_process_required);
 plan.active_groups=api.active_groups();if(!plan.active_groups)return reject(WorkerAffinityFailure::group_query);
 if(plan.active_groups!=1)return reject(WorkerAffinityFailure::single_active_group_required);
 if(!api.process_masks(plan.process_mask,plan.system_mask))return reject(WorkerAffinityFailure::process_mask_query);
 if(!plan.process_mask||!plan.system_mask||(plan.process_mask&~plan.system_mask))return reject(WorkerAffinityFailure::process_mask_invalid);
 const auto topology=api.topology();plan.topology_bytes=topology.returned_bytes;plan.topology_error=topology.win32_error;
 plan.topology_cores=topology.count;if(topology.failure!=WorkerAffinityFailure::none)return reject(topology.failure);
 if(topology.count==0||topology.count>WorkerCoreTopology::core_cap)return reject(WorkerAffinityFailure::topology_core_bound);
 if(topology.returned_bytes==0||topology.returned_bytes>WorkerCoreTopology::byte_cap)return reject(WorkerAffinityFailure::topology_byte_bound);
 std::uint64_t all=0;unsigned highest=0;
 for(std::size_t i=0;i<topology.count;++i){const auto& core=topology.cores[i];
  if(core.group!=0)return reject(WorkerAffinityFailure::topology_group);
  if(!core.mask||(all&core.mask))return reject(WorkerAffinityFailure::topology_malformed);
  all|=core.mask;plan.topology_logical_processors+=std::popcount(core.mask);
  if(plan.topology_logical_processors>WorkerCoreTopology::logical_processor_cap)return reject(WorkerAffinityFailure::topology_lp_bound);
  if(core.efficiency_class>highest)highest=core.efficiency_class;
  for(unsigned lp=0;lp<64;++lp)if(core.mask&(std::uint64_t{1}<<lp)){
   plan.topology_core_by_lp[lp]=static_cast<std::uint8_t>(i);
   plan.topology_class_by_lp[lp]=core.efficiency_class;
  }
 }
 if(all!=plan.system_mask)return reject(WorkerAffinityFailure::topology_incomplete);
 plan.topology_available=true;
 std::array<std::size_t,2> selected{topology.count,topology.count};std::array<unsigned,2> numbers{0,0};
 for(std::size_t i=0;i<topology.count;++i){const auto& core=topology.cores[i];if(core.efficiency_class!=highest)continue;
  const auto number=static_cast<unsigned>(std::countr_zero(core.mask));
  if(selected[1]==topology.count||number>numbers[1]){numbers[0]=numbers[1];selected[0]=selected[1];numbers[1]=number;selected[1]=i;}
  else if(selected[0]==topology.count||number>numbers[0]){numbers[0]=number;selected[0]=i;}
 }
 if(selected[0]==topology.count)return reject(WorkerAffinityFailure::two_highest_class_cores_required);
 plan.efficiency_class=static_cast<std::uint8_t>(highest);
 plan.consumer_core=static_cast<std::uint16_t>(selected[0]);plan.producer_core=static_cast<std::uint16_t>(selected[1]);
 plan.consumer_number=static_cast<std::uint8_t>(numbers[0]);plan.producer_number=static_cast<std::uint8_t>(numbers[1]);
 plan.consumer_mask=std::uint64_t{1}<<numbers[0];plan.producer_anchor_mask=std::uint64_t{1}<<numbers[1];
 plan.consumer_core_mask=topology.cores[selected[0]].mask;
 if((plan.consumer_mask&plan.process_mask)!=plan.consumer_mask||(plan.producer_anchor_mask&plan.process_mask)!=plan.producer_anchor_mask)
  return reject(WorkerAffinityFailure::requested_mask_not_in_process);
 for(std::size_t i=0;i<topology.count;++i){const auto& core=topology.cores[i];
  if(i==selected[0]||core.efficiency_class!=highest)continue;
  const auto bit=std::uint64_t{1}<<std::countr_zero(core.mask);
  if(!(bit&plan.process_mask))continue;
  plan.producer_mask|=bit;++plan.producer_eligible_core_count;
 }
 return plan;
}
struct WorkerAffinitySnapshot {
 std::uint32_t thread_id=0,active_groups=0,process_class=0;
 std::uint64_t process_mask=0,system_mask=0;
 WorkerGroupAffinity previous{},requested{},applied_readback{},restored_readback{},rollback_readback{},fallback_readback{};
 bool previous_available=false,applied_available=false,restored_available=false,rollback_available=false,fallback_available=false;
 bool set_succeeded=false,applied=false,close_attempted=false,restored=false,rollback_attempted=false,rollback_restored=false,fallback_attempted=false;
 WorkerAffinityFailure failure=WorkerAffinityFailure::none,rollback_failure=WorkerAffinityFailure::none,fallback_failure=WorkerAffinityFailure::none;
 bool verified() const noexcept{return applied&&close_attempted&&restored&&previous_available&&applied_available&&restored_available&&
  failure==WorkerAffinityFailure::none&&rollback_failure==WorkerAffinityFailure::none&&fallback_failure==WorkerAffinityFailure::none&&
  active_groups==1&&process_class==0x20&&applied_readback==requested&&restored_readback==previous;}
};
class WorkerAffinityError final : public std::runtime_error {
 WorkerAffinityFailure reason_,rollback_;
public:
 explicit WorkerAffinityError(WorkerAffinityFailure reason,WorkerAffinityFailure rollback=WorkerAffinityFailure::none)
  :std::runtime_error(worker_affinity_failure_name(reason)),reason_(reason),rollback_(rollback){}
 WorkerAffinityFailure reason() const noexcept{return reason_;}
 WorkerAffinityFailure rollback_reason() const noexcept{return rollback_;}
};
// All Api operations are nonthrowing current-thread OS calls. A fresh external
// receipt survives constructor failure. Destructor fallback cannot verify a run.
template<class Api> class WorkerAffinityScope final {
 Api& api_;WorkerAffinitySnapshot& receipt_;bool active_=false;
 [[noreturn]] void reject(WorkerAffinityFailure reason){if(receipt_.failure==WorkerAffinityFailure::none)receipt_.failure=reason;throw WorkerAffinityError(reason,receipt_.rollback_failure);}
 void rollback() noexcept {
  receipt_.rollback_attempted=true;
  if(api_.thread()!=receipt_.thread_id){receipt_.rollback_failure=WorkerAffinityFailure::wrong_thread;return;}
  if(!api_.set(receipt_.previous)){receipt_.rollback_failure=WorkerAffinityFailure::rollback_set;return;}
  const auto value=api_.current();if(!value){receipt_.rollback_failure=WorkerAffinityFailure::rollback_readback_query;return;}
  receipt_.rollback_available=true;receipt_.rollback_readback=*value;
  if(*value!=receipt_.previous)receipt_.rollback_failure=WorkerAffinityFailure::rollback_readback_mismatch;else receipt_.rollback_restored=true;
 }
public:
 WorkerAffinityScope(Api& api,WorkerAffinitySnapshot& receipt,std::uint64_t mask):api_(api),receipt_(receipt) {
  receipt_={};receipt_.requested={0,mask};receipt_.process_class=api_.process_class();
  if(!receipt_.process_class)reject(WorkerAffinityFailure::process_query);
  if(receipt_.process_class!=0x20)reject(WorkerAffinityFailure::normal_process_required);
  receipt_.active_groups=api_.active_groups();if(!receipt_.active_groups)reject(WorkerAffinityFailure::group_query);
  if(receipt_.active_groups!=1)reject(WorkerAffinityFailure::single_active_group_required);
  if(!api_.process_masks(receipt_.process_mask,receipt_.system_mask))reject(WorkerAffinityFailure::process_mask_query);
  if(!receipt_.process_mask||!receipt_.system_mask||(receipt_.process_mask&~receipt_.system_mask))reject(WorkerAffinityFailure::process_mask_invalid);
  if(!mask)reject(WorkerAffinityFailure::requested_mask_invalid);
  if((mask&receipt_.process_mask)!=mask)reject(WorkerAffinityFailure::requested_mask_not_in_process);
  receipt_.thread_id=api_.thread();if(!receipt_.thread_id)reject(WorkerAffinityFailure::thread_query);
  const auto previous=api_.current();if(!previous)reject(WorkerAffinityFailure::affinity_query);
  receipt_.previous=*previous;receipt_.previous_available=true;
  if(previous->group!=0)reject(WorkerAffinityFailure::original_group);
  if(!previous->mask||(previous->mask&~receipt_.process_mask))reject(WorkerAffinityFailure::original_mask);
  if(api_.thread()!=receipt_.thread_id)reject(WorkerAffinityFailure::wrong_thread);
  if(!api_.set(receipt_.requested))reject(WorkerAffinityFailure::affinity_set);
  receipt_.set_succeeded=true;const auto applied=api_.current();
  if(applied){receipt_.applied_available=true;receipt_.applied_readback=*applied;}
  if(!applied||*applied!=receipt_.requested){const auto reason=applied?WorkerAffinityFailure::affinity_readback_mismatch:WorkerAffinityFailure::affinity_readback_query;receipt_.failure=reason;rollback();reject(reason);}
  receipt_.applied=true;active_=true;
 }
 WorkerAffinityScope(const WorkerAffinityScope&)=delete;WorkerAffinityScope& operator=(const WorkerAffinityScope&)=delete;
 ~WorkerAffinityScope() noexcept {
  if(!active_)return;receipt_.fallback_attempted=true;
  try {
   if(api_.thread()!=receipt_.thread_id){receipt_.fallback_failure=WorkerAffinityFailure::wrong_thread;return;}
   if(!api_.set(receipt_.previous)){receipt_.fallback_failure=WorkerAffinityFailure::restore_set;return;}
   const auto value=api_.current();if(!value){receipt_.fallback_failure=WorkerAffinityFailure::restore_readback_query;return;}
   receipt_.fallback_available=true;receipt_.fallback_readback=*value;
   if(*value!=receipt_.previous)receipt_.fallback_failure=WorkerAffinityFailure::restore_readback_mismatch;
  }catch(...){receipt_.fallback_failure=WorkerAffinityFailure::fallback_api_exception;}
 }
 void close() {
  if(!active_)return;receipt_.close_attempted=true;
  if(api_.thread()!=receipt_.thread_id)reject(WorkerAffinityFailure::wrong_thread);
  if(!api_.set(receipt_.previous))reject(WorkerAffinityFailure::restore_set);
  const auto value=api_.current();if(!value)reject(WorkerAffinityFailure::restore_readback_query);
  receipt_.restored_available=true;receipt_.restored_readback=*value;
  if(*value!=receipt_.previous)reject(WorkerAffinityFailure::restore_readback_mismatch);
  receipt_.restored=true;active_=false;
 }
};
#ifdef _WIN32
inline WorkerCoreTopology read_worker_core_topology() noexcept {
 WorkerCoreTopology topology;DWORD required=0;
 const auto probe=GetLogicalProcessorInformationEx(RelationProcessorCore,nullptr,&required);
 const auto probe_error=probe?ERROR_SUCCESS:GetLastError();topology.query_bytes=required;
 if(probe||probe_error!=ERROR_INSUFFICIENT_BUFFER){topology.failure=WorkerAffinityFailure::topology_query;topology.win32_error=probe_error;return topology;}
 if(!required||required>WorkerCoreTopology::byte_cap){topology.failure=WorkerAffinityFailure::topology_byte_bound;return topology;}
 alignas(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) std::array<std::byte,WorkerCoreTopology::byte_cap> buffer{};
 DWORD available=static_cast<DWORD>(buffer.size());
 if(!GetLogicalProcessorInformationEx(RelationProcessorCore,reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),&available)){topology.failure=WorkerAffinityFailure::topology_read;topology.win32_error=GetLastError();topology.returned_bytes=available;return topology;}
 topology.returned_bytes=available;if(!available||available>buffer.size()){topology.failure=WorkerAffinityFailure::topology_byte_bound;return topology;}
 constexpr auto size_offset=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Size),masks_offset=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Processor.GroupMask);
 constexpr auto count_offset=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Processor.GroupCount),class_offset=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Processor.EfficiencyClass);
 topology.failure=WorkerAffinityFailure::topology_malformed;std::size_t offset=0;
 while(offset<available) {
  if(available-offset<size_offset+sizeof(DWORD))return topology;
  LOGICAL_PROCESSOR_RELATIONSHIP relation{};DWORD size=0;WORD groups=0;BYTE efficiency=0;
  std::memcpy(&relation,buffer.data()+offset,sizeof(relation));std::memcpy(&size,buffer.data()+offset+size_offset,sizeof(size));
  if(size<masks_offset+sizeof(GROUP_AFFINITY)||size>available-offset||relation!=RelationProcessorCore)return topology;
  std::memcpy(&groups,buffer.data()+offset+count_offset,sizeof(groups));std::memcpy(&efficiency,buffer.data()+offset+class_offset,sizeof(efficiency));
  if(groups!=1){topology.failure=WorkerAffinityFailure::topology_group;return topology;}
  GROUP_AFFINITY affinity{};std::memcpy(&affinity,buffer.data()+offset+masks_offset,sizeof(affinity));
  if(topology.count==topology.cores.size()){topology.failure=WorkerAffinityFailure::topology_core_bound;return topology;}
  topology.cores[topology.count++]={affinity.Group,efficiency,static_cast<std::uint64_t>(affinity.Mask)};offset+=size;
 }
 topology.failure=WorkerAffinityFailure::none;return topology;
}
struct WindowsWorkerAffinityApi {
 std::uint32_t process_class() const noexcept{return GetPriorityClass(GetCurrentProcess());}
 std::uint32_t active_groups() const noexcept{return GetActiveProcessorGroupCount();}
 bool process_masks(std::uint64_t& process,std::uint64_t& system) const noexcept {DWORD_PTR p=0,s=0;if(!GetProcessAffinityMask(GetCurrentProcess(),&p,&s))return false;process=p;system=s;return true;}
 WorkerCoreTopology topology() const noexcept{return read_worker_core_topology();}
 std::uint32_t thread() const noexcept{return GetCurrentThreadId();}
 std::optional<WorkerGroupAffinity> current() const noexcept {GROUP_AFFINITY value{};if(!GetThreadGroupAffinity(GetCurrentThread(),&value))return {};return WorkerGroupAffinity{value.Group,static_cast<std::uint64_t>(value.Mask)};}
 bool set(WorkerGroupAffinity requested) const noexcept {GROUP_AFFINITY value{};value.Group=requested.group;value.Mask=static_cast<KAFFINITY>(requested.mask);return SetThreadGroupAffinity(GetCurrentThread(),&value,nullptr)!=0;}
 static const PlacementPlan& plan() noexcept {static const PlacementPlan value=[](){WindowsWorkerAffinityApi api;return make_worker_placement(api);}();return value;}
};
static_assert(sizeof(KAFFINITY)==sizeof(std::uint64_t),"cold affinity policy requires x64");
#endif
}
