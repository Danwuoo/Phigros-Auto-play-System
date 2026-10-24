#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace pas_fixture_evidence {
enum class PointerAction { down, pointer_down, move, up, pointer_up, cancel, other };
struct PointerLedger {
    std::array<bool,32> active{};
    std::uint64_t errors=0;
    bool saw_pointer=false;
    std::size_t count() const {std::size_t result=0;for(bool item:active)if(item)++result;return result;}
    void observe(PointerAction action,std::size_t changed,const std::array<bool,32>& present) {
        if(changed>=active.size()) {++errors;return;}
        if(action==PointerAction::down) {
            std::array<bool,32> expected{};expected[changed]=true;
            if(count()!=0||present!=expected)++errors;
            active=expected;saw_pointer=true;
        } else if(action==PointerAction::pointer_down) {
            auto expected=active;expected[changed]=true;
            if(active[changed]||count()==0||present!=expected)++errors;
            active=expected;saw_pointer=true;
        } else if(action==PointerAction::up||action==PointerAction::pointer_up) {
            if(!active[changed]||present!=active)++errors;
            active[changed]=false;
            if((action==PointerAction::up&&count()!=0)||(action==PointerAction::pointer_up&&count()==0))++errors;
        } else if(action==PointerAction::move) {
            if(present!=active)++errors;
        } else if(action==PointerAction::cancel) {
            if(present!=active)++errors;
            active.fill(false);
        }
    }
};
} // namespace pas_fixture_evidence
