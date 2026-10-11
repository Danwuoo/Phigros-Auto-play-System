#pragma once
#include "../../bvi_cold_v3/relation_policy.hpp"

namespace bvi::owner_cooperative::cold_verification {
// Compare every public observation field; object padding is not evidence.
inline bool observation_equal(const Observation& a,const Observation& b){
 if(a.count!=b.count||a.probes!=b.probes||a.rgb_signature!=b.rgb_signature||a.invalid!=b.invalid||a.context_invalid!=b.context_invalid||a.reason!=b.reason||a.aliases_valid!=b.aliases_valid||a.canonical!=b.canonical)return false;
 for(std::size_t i=0;i<a.parts.size();++i){
  if(!same_measurement(a.parts[i],b.parts[i])||a.parts[i].probes!=b.parts[i].probes)return false;
  const auto& x=a.relations[i];const auto& y=b.relations[i];
  if(x.alternatives!=y.alternatives||x.independent!=y.independent||x.span!=y.span||x.first_independent!=y.first_independent||x.last_independent!=y.last_independent||x.freshness!=y.freshness||x.usable!=y.usable||x.ambiguous!=y.ambiguous||x.invalid!=y.invalid||x.links!=y.links)return false;
 }
 return true;
}
}
