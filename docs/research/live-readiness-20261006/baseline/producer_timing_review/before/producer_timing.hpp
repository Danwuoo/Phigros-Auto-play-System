#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
namespace pas::cost_evidence {
struct ProducerTiming {
 std::vector<double> render_ms,lateness_ms,render_to_owner_ms;
 std::uint64_t overruns=0,count=0;std::int64_t first_due=0;
 static double delta(std::int64_t end,std::int64_t start){
  if((start>0&&end<std::numeric_limits<std::int64_t>::min()+start)||(start<0&&end>std::numeric_limits<std::int64_t>::max()+start))throw std::runtime_error("producer timestamp difference overflow");
  return (end-start)/1e6;
 }
 void observe(const nlohmann::json& row,std::int64_t cadence_ms){
  if((cadence_ms!=8&&cadence_ms!=20)||count>=10000)throw std::runtime_error("producer cadence/count bound");
  const auto due=row.at("producer_due_ns").get<std::int64_t>(),start=row.at("render_start_ns").get<std::int64_t>(),end=row.at("render_end_ns").get<std::int64_t>();
  const auto cadence=cadence_ms*1000000;
  if(due<=0||start<=0||end<start||end!=row.at("capture").get<std::int64_t>()||due>std::numeric_limits<std::int64_t>::max()-cadence)throw std::runtime_error("producer current render order");
  if(count==0)first_due=due;
  const auto offset=std::int64_t(count)*cadence;
  if(first_due>std::numeric_limits<std::int64_t>::max()-offset||due!=first_due+offset)throw std::runtime_error("producer fixed cadence sequence");
  ++count;render_ms.push_back(delta(end,start));lateness_ms.push_back(delta(start,due));overruns+=end>due+cadence;
  if(row.at("owned").get<bool>()){const auto owner=row.at("owner_complete").get<std::int64_t>();if(owner<end)throw std::runtime_error("producer owner order");render_to_owner_ms.push_back(delta(owner,start));}
 }
};
}
