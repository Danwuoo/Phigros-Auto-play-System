#pragma once
// Offline RGB workload only. It cannot supply targets, plans, contacts or clocks.
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <stdexcept>
#include <vector>

namespace pas::current_rails::cost_workload {
using Images = std::array<std::vector<std::uint8_t>, 12>;
inline void box(std::vector<std::uint8_t>& pixels, int x0, int y0, int x1, int y1,
                std::array<std::uint8_t, 3> color) {
    for (int y = std::max(0, y0); y < std::min(720, y1); ++y)
        for (int x = std::max(0, x0); x < std::min(1280, x1); ++x)
            std::copy(color.begin(), color.end(), pixels.begin() + std::size_t(y) * 3840 + x * 3);
}
// The seven historical scene names retain their exact original RGB bytes.
// "hold" is intentionally not a shared A/B action-positive workload: its cyan
// fill and dark30 exterior do not satisfy the frozen v3 blue/black witnesses.
// No new scene is admitted to the cost pipeline by this diagnostic header.
inline Images stimulus(std::string_view scene) {
    Images images;
    for (int k = 0; k < 12; ++k) {
        auto& p = images[k];
        p.assign(1280 * 720 * 3, 30);
        box(p, 20, 20, 26, 42, {255,255,255});
        box(p, 34, 20, 40, 42, {255,255,255});
        for (int d = 0; d < 6; ++d)
            box(p, 1020+d*24, 20, 1032+d*24, 40, {255,255,255});
        if (scene == "dense") {
            for (int row = 0; row < 16; ++row) {
                const int y = 150+row*31;
                box(p, 40, y-1, 1240, y+2, {255,255,255});
                for (int col = 0; col < 8; ++col) {
                    const int x = 190+col*130;
                    box(p, x-39, y-12, x+39, y-3, {40,190,255});
                }
            }
        } else {
            box(p, 115, 499, 1165, 502, {255,255,255});
            const int y = 440+k*8;
            if (scene == "hold") {
                box(p, 350, y-100, 451, y+1, {155,233,255});
                box(p, 350, y-100, 352, y+1, {255,255,255});
                box(p, 449, y-100, 451, y+1, {255,255,255});
            } else {
                box(p, 491, y-4, 569, y+5, {40,190,255});
            }
        }
    }
    return images;
}

enum class Intervention { passage, stationary_body, leave, absence, effect };
inline std::string_view name(Intervention intervention) {
    switch(intervention) {
    case Intervention::stationary_body: return "stationary_body";
    case Intervention::leave: return "leave";
    case Intervention::absence: return "absence";
    case Intervention::effect: return "effect";
    default: return "passage";
    }
}
// Functional diagnosis only: all 21 declared approach phases must be retained.
// A single successful entry phase is NOT a representative Hold workload. In
// particular, safety Up after missing evidence is not measured tail completion.
inline Images entry_release_probe(int head_at_frame5, Intervention intervention) {
    if(head_at_frame5<490||head_at_frame5>510)throw std::invalid_argument("declared head sweep 490..510 only");
    Images images;
    for(int k=0;k<12;++k) {
        auto& p=images[k];p.assign(1280*720*3,0);
        box(p,20,20,26,42,{255,255,255});box(p,34,20,40,42,{255,255,255});
        for(int d=0;d<6;++d)box(p,1020+d*24,20,1032+d*24,40,{255,255,255});
        box(p,115,499,1165,502,{255,255,255});
        const bool after_entry=k>=5;
        const int head=after_entry&&intervention==Intervention::stationary_body?
            head_at_frame5:head_at_frame5+(k-4)*16;
        const int x=after_entry&&intervention==Intervention::leave?700:400;
        if(after_entry&&intervention==Intervention::absence)continue;
        const auto color=after_entry&&intervention==Intervention::effect?
            std::array<std::uint8_t,3>{230,190,70}:std::array<std::uint8_t,3>{40,190,255};
        box(p,x-50,head-100,x+51,head+1,color);
        box(p,x-50,head-100,x-48,head+1,{255,255,255});
        box(p,x+49,head-100,x+51,head+1,{255,255,255});
    }
    return images;
}
} // namespace pas::current_rails::cost_workload
