#include "relation_policy.hpp"
#include <algorithm>
namespace bvi {
Constraints constrain(const Observation&out,const Guard&g){
 Constraints c;c.contact_id=g.contact_id;c.completed=g.execution=="completed_up";const bool unknown=g.execution=="unknown_down",active=g.execution=="known_down",never=g.execution=="never_executed";const bool consistent=(never&&g.prefix==0)||(active&&g.prefix>0)||(unknown&&g.prefix>0&&g.receipt_unknown)||(c.completed&&g.prefix>=2);
 c.invalid=!consistent||out.invalid||out.context_invalid||out.count>128||g.now>=g.gate_deadline||g.now>=g.plan_deadline;c.release=unknown||c.invalid;if(c.invalid||unknown||c.completed)return c;
 auto canonical=[&](std::size_t i){if(out.aliases_valid)return static_cast<std::size_t>(out.canonical[i]);for(std::size_t j=0;j<i;++j)if(same_measurement(out.parts[i],out.parts[j]))return j;return i;};
 const bool validAttachment=active&&g.attachment_query>=0&&static_cast<std::size_t>(g.attachment_query)<out.count;const auto attachment=validAttachment?canonical(static_cast<std::size_t>(g.attachment_query)):std::size_t(128);int free=std::max(0,g.free_contacts);
 for(std::size_t i=0;i<out.count;++i){const auto&d=out.parts[i];const auto&r=out.relations[i];const bool current=d.contact==Support::supported&&d.line_unique;const bool attached=validAttachment&&static_cast<int>(i)==g.attachment_query;const bool attachedGroup=validAttachment&&canonical(i)==attachment;const bool bodyNow=d.q.tap||(d.body==Support::supported&&d.left&&d.right);
  if(attached&&current&&bodyNow&&!r.invalid&&!r.ambiguous){c.move=c.refresh=true;c.hit=d.hit;}
  if(canonical(i)==i&&!attachedGroup&&current&&bodyNow&&d.front_end&&r.usable&&free>0){c.down[i]=true;--free;}
 }
 if(active&&!c.refresh&&g.now-g.last_contact>=60000000)c.release=true;
 return c;
}
}
