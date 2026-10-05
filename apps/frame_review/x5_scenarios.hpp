#pragma once
#include "x5_rule.hpp"

namespace pas::x5 {
inline Input approach_fixture(){Input in;in.now=40'000'000;in.frame_extent=1280;in.width=128;in.height=8;in.center={400,440};in.tangent={0,1};in.appearance_known=true;in.strong_current=true;in.identity=Identity::unique;in.confirmation=Confirmation::unconfirmed;
    in.line_count=2;in.lines[0]={{640,576},{1,0},1280,.9,true,true};in.lines[1]={{900,360},{0,1},720,.9,true,true};in.sample_count=3;
    for(std::size_t k=0;k<3;++k){in.samples[k].ns=static_cast<std::int64_t>(k)*20'000'000;in.samples[k].note={400,400+20.*k};in.samples[k].lines[0]=in.lines[0];in.samples[k].lines[1]=in.lines[1];}return in;
}
// Two legal future continuations have exactly the same observable prefix.
// A: keep descending, then align with/strike y576. B: turn right after this
// prefix and align with/strike x900; y576 is a crossing decoration. These
// labels are synthetic scenario definitions, never inferred from the rule.
// Neither future nor physical role is present in Input.
inline Input indistinguishable_late_turn_prefix(){return approach_fixture();}
}
