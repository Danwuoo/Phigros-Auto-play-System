#pragma once
#include "bvi.hpp"
namespace bvi {
// Requires legal, current, measured lines; it does not authenticate their pixel source.
void observe_relation_context(Observation&,std::span<const Line>);
// Exact current measurement equivalence, never physical ownership or near-ROI merging.
bool same_measurement(const Descriptor&,const Descriptor&);
void canonicalize_measurements(Observation&);
}
