#pragma once
#include "bvi.hpp"
#include <functional>

namespace bvi::owner_cooperative {
// Derivative of the frozen v3 extractor. Poll only the existing sole owner;
// all RGB bytes, probes and current-pixel witnesses retain the donor contract.
Observation extract(const Candidate&, const View&, std::span<const Query>,
                    std::span<const Line>, const std::function<bool()>& poll);
}
