#pragma once
#include "bvi.hpp"

namespace bvi {
// Current RGB witnesses only. Caller retains body, unique-line and action gates.
struct ContactSample { bool supported{}, yellow{}; };
ContactSample sample_current_contact(const View&, const Query&, const Line&,
                                     Point hit, std::uint64_t& probes);
}
