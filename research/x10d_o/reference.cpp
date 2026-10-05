#include "contract.hpp"
namespace pas::x10d_o {
Claim resolve(const Frame&,const NoteCandidate& n,const LineCandidate&,std::span<const Anchor> a){
    // Deliberately exposes the old geometric-claim assumption for red tests.
    // This adapter is NOT an added C36h runtime policy or original function.
    Claim c;if(n.kind==NoteKind::hold&&n.rails_geometry&&(n.head_on_line||n.held_body_evidence)&&!a.empty()){
      c.pixel_support=true;c.measured_depth=n.height;c.owner=a.front().id;c.reason="reference_geometry_assumption";
    }return c;
}
}
