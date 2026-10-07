#pragma once
#include "pas/core.hpp"
#include <array>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pas::diagnostic_phase {
struct ThreadSample {
 std::optional<std::uint64_t> cpu_ns,cycles;
 static ThreadSample read() {
  ThreadSample s;
#ifdef _WIN32
  FILETIME created{},ended{},kernel{},user{};
  if(GetThreadTimes(GetCurrentThread(),&created,&ended,&kernel,&user)) {
   const auto ticks=[](FILETIME t){return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
   s.cpu_ns=(ticks(kernel)+ticks(user))*100;
  }
  ULONG64 c=0;if(QueryThreadCycleTime(GetCurrentThread(),&c))s.cycles=c;
#endif
  return s;
 }
};
struct Row {std::string_view name;Nanoseconds wall_ns=0;std::optional<std::uint64_t> cpu_ns,cycles;};
// Explicitly diagnostic-only. Fixed nine scalar rows, no pixels, leases, plan
// history, policy inputs, CPU-to-wall conversion, or cost qualification.
class Profile final {
 std::array<Row,9> rows_{};std::size_t count_=0;std::uint64_t frame_=0;
 static std::optional<std::uint64_t> delta(std::optional<std::uint64_t> a,std::optional<std::uint64_t> b){if(!a||!b||*b<*a)return {};return *b-*a;}
public:
 bool enabled=false;
 void begin_frame(std::uint64_t frame){if(frame_!=frame){frame_=frame;count_=0;}}
 template<class F> void run(std::string_view name,F&& work){
  if(!enabled){work();return;}if(count_==rows_.size())throw std::runtime_error("diagnostic phase bound");
  HostClock wall;const auto start=wall.now_ns();const auto a=ThreadSample::read();work();const auto b=ThreadSample::read();
  rows_[count_++]={name,wall.now_ns()-start,delta(a.cpu_ns,b.cpu_ns),delta(a.cycles,b.cycles)};
 }
 nlohmann::json json() const {
  auto rows=nlohmann::json::array();for(std::size_t i=0;i<count_;++i){const auto&r=rows_[i];rows.push_back({{"phase",r.name},{"wall_qpc_ns",r.wall_ns},{"thread_kernel_user_ns",r.cpu_ns?nlohmann::json(*r.cpu_ns):nlohmann::json(nullptr)},{"thread_cycles",r.cycles?nlohmann::json(*r.cycles):nlohmann::json(nullptr)}});}
  return {{"schema","pas.diagnostic-phase-profile.v1"},{"enabled",enabled},{"source_frame",frame_},{"rows",rows},{"row_cap",9},{"cpu_domain","Windows GetThreadTimes kernel+user, 100ns units; quantization not measured"},{"cycles_domain","Windows QueryThreadCycleTime; cycles are not nanoseconds"},{"cost_qualification",false},{"device_operations",0}};
 }
};
}
