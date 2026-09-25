#pragma once

#include "pas/core.hpp"
#include <optional>
#include <string>
#include <vector>

namespace pas {

struct PointerSample {
    std::uint32_t sequence = 0;
    int phase = 0; // 1 down, 2 move, 3 up, 4 cancel
    int android_id = 0;
    int x = 0;
    int y = 0;
};

struct TouchEvidence {
    std::uint32_t sequence = 0;
    std::uint32_t overflow_total = 0;
    std::uint32_t active = 0;
    std::uint32_t downs = 0, ups = 0, moves = 0, cancels = 0;
    std::vector<PointerSample> samples;
    bool complete = false;
    std::string error;
};

TouchEvidence decode_touch_evidence(const Frame& frame, std::uint32_t since_sequence);

struct ExpectedPointer {
    int phase;
    int android_id;
    int x;
    int y;
};

struct PathCheck {
    bool passed = false;
    std::string reason;
};

PathCheck verify_touch_path(const TouchEvidence& evidence,
                            const std::vector<ExpectedPointer>& expected,
                            int tolerance_px, int final_active,
                            const std::vector<int>& stationary_ids = {});

} // namespace pas
