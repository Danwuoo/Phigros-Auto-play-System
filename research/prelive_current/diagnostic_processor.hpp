#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pas::diagnostic_phase {
struct ProcessorSample {
 std::uint16_t group=0;std::uint8_t number=0;bool available=false;
 static ProcessorSample read() noexcept {
  ProcessorSample value;
#ifdef _WIN32
  PROCESSOR_NUMBER processor{};GetCurrentProcessorNumberEx(&processor);
  value={processor.Group,processor.Number,true};
#endif
  return value;
 }
 nlohmann::json json() const {
  if(!available)return nullptr;
  return {{"group",group},{"number",number}};
 }
};
struct LogicalProcessor {
 std::uint16_t group=0,core_ordinal=0;std::uint8_t number=0,efficiency_class=0;
};
enum class TopologyStatus : std::uint8_t {
 not_windows,complete,query_failed,byte_bound,read_failed,malformed,logical_processor_bound
};
inline const char* topology_status_name(TopologyStatus status) noexcept {
 switch(status) {
 case TopologyStatus::not_windows:return "not_windows";
 case TopologyStatus::complete:return "complete";
 case TopologyStatus::query_failed:return "query_failed";
 case TopologyStatus::byte_bound:return "byte_bound";
 case TopologyStatus::read_failed:return "read_failed";
 case TopologyStatus::malformed:return "malformed";
 case TopologyStatus::logical_processor_bound:return "logical_processor_bound";
 }
 return "unknown";
}
// Diagnostic-only OS facts. Never used for affinity, CPU sets or a policy input.
// An incomplete topology keeps its status/error; no guessed efficiency class.
struct ProcessorTopology {
 static constexpr std::size_t byte_cap=65536,logical_processor_cap=256;
 std::array<LogicalProcessor,logical_processor_cap> rows{};
 std::size_t count=0;std::uint32_t query_bytes=0,returned_bytes=0,win32_error=0;
 TopologyStatus status=TopologyStatus::not_windows;
 bool complete() const noexcept{return status==TopologyStatus::complete;}
#ifdef _WIN32
 static ProcessorTopology decode(std::span<const std::byte> bytes) noexcept {
  ProcessorTopology topology;topology.returned_bytes=static_cast<std::uint32_t>(bytes.size());
  topology.status=TopologyStatus::malformed;
  if(bytes.empty())return topology;
  if(bytes.size()>byte_cap){topology.status=TopologyStatus::byte_bound;return topology;}
  constexpr auto size_offset=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Size);
  constexpr auto masks_offset=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Processor.GroupMask);
  constexpr auto count_offset=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Processor.GroupCount);
  constexpr auto class_offset=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Processor.EfficiencyClass);
  std::size_t offset=0;std::uint16_t core=0;
  while(offset<bytes.size()) {
   if(bytes.size()-offset<size_offset+sizeof(DWORD))return topology;
   LOGICAL_PROCESSOR_RELATIONSHIP relation{};DWORD size=0;
   std::memcpy(&relation,bytes.data()+offset,sizeof(relation));
   std::memcpy(&size,bytes.data()+offset+size_offset,sizeof(size));
   if(size<masks_offset||size>bytes.size()-offset||relation!=RelationProcessorCore)return topology;
   WORD groups=0;BYTE efficiency=0;
   std::memcpy(&groups,bytes.data()+offset+count_offset,sizeof(groups));
   std::memcpy(&efficiency,bytes.data()+offset+class_offset,sizeof(efficiency));
   if(!groups||groups>(size-masks_offset)/sizeof(GROUP_AFFINITY))return topology;
   const auto before=topology.count;
   for(std::size_t group=0;group<groups;++group) {
    GROUP_AFFINITY affinity{};
    std::memcpy(&affinity,bytes.data()+offset+masks_offset+group*sizeof(affinity),sizeof(affinity));
    for(std::size_t number=0;number<sizeof(KAFFINITY)*8;++number) {
     if(!(affinity.Mask&(KAFFINITY{1}<<number)))continue;
     for(std::size_t previous=0;previous<topology.count;++previous)
      if(topology.rows[previous].group==affinity.Group&&topology.rows[previous].number==number)return topology;
     if(topology.count==logical_processor_cap){topology.status=TopologyStatus::logical_processor_bound;return topology;}
     topology.rows[topology.count++]={affinity.Group,core,static_cast<std::uint8_t>(number),efficiency};
    }
   }
   if(topology.count==before)return topology;
   ++core;offset+=size;
  }
  topology.status=TopologyStatus::complete;return topology;
 }
#endif
 static ProcessorTopology read() noexcept {
  ProcessorTopology topology;
#ifdef _WIN32
  DWORD required=0;
  const auto probe=GetLogicalProcessorInformationEx(RelationProcessorCore,nullptr,&required);
  const auto error=probe?ERROR_SUCCESS:GetLastError();topology.query_bytes=required;
  if(probe||error!=ERROR_INSUFFICIENT_BUFFER){topology.status=TopologyStatus::query_failed;topology.win32_error=error;return topology;}
  if(!required||required>byte_cap){topology.status=TopologyStatus::byte_bound;return topology;}
  alignas(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) std::array<std::byte,byte_cap> buffer{};
  DWORD available=static_cast<DWORD>(buffer.size());
  if(!GetLogicalProcessorInformationEx(RelationProcessorCore,
    reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),&available)) {
   topology.status=TopologyStatus::read_failed;topology.win32_error=GetLastError();topology.returned_bytes=available;return topology;
  }
  if(available>byte_cap){topology.status=TopologyStatus::byte_bound;topology.returned_bytes=available;return topology;}
  topology=decode(std::span<const std::byte>(buffer.data(),available));topology.query_bytes=required;
#endif
  return topology;
 }
 std::optional<std::uint8_t> efficiency_class(ProcessorSample processor) const noexcept {
  if(!complete()||!processor.available)return {};
  for(std::size_t i=0;i<count;++i)if(rows[i].group==processor.group&&rows[i].number==processor.number)return rows[i].efficiency_class;
  return {};
 }
 nlohmann::json json() const {
  auto entries=nlohmann::json::array();
  for(std::size_t i=0;i<count;++i){const auto& row=rows[i];entries.push_back({{"group",row.group},{"number",row.number},{"core_ordinal",row.core_ordinal},{"efficiency_class",row.efficiency_class}});}
  return {{"schema","pas.diagnostic-processor-topology.v1"},{"status",topology_status_name(status)},{"complete",complete()},
   {"query_bytes",query_bytes},{"returned_bytes",returned_bytes},{"win32_error",win32_error},
   {"byte_cap",byte_cap},{"logical_processor_cap",logical_processor_cap},{"logical_processors",entries},
   {"efficiency_class_domain","raw Windows RelationProcessorCore EfficiencyClass; no P/E naming or frequency measurement"},
   {"endpoint_domain","GetCurrentProcessorNumberEx raw group/number; equal endpoints do not exclude intermediate migration"},
   {"cost_qualification",false},{"live_ready",false},{"device_operations",0}};
 }
};
// Call once before diagnostic measurement; initialization makes at most one
// size probe and one bounded read per process. No refresh, retries or tracing.
inline const ProcessorTopology& processor_topology() noexcept {
 static const ProcessorTopology topology=ProcessorTopology::read();return topology;
}
}
