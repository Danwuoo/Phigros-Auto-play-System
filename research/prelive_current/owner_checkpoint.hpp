#pragma once
#include "pas/core.hpp"
#include <functional>

namespace pas::current_execution {
struct OwnerCheckpoint {
    const Clock& clock;
    std::function<bool()> poll;
    bool check() const { return poll && poll(); }
    Nanoseconds now() const { return clock.now_ns(); }
};
}
