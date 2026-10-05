// Typed source-level probe, not RGB recognition, real-game input or an owner test.
// Compiles unmodified research/x10d_o_bvi_r1/bvi.cpp at commit 74e5443.
#include "bvi.hpp"
#include <cassert>
#include <iostream>
#include <string_view>
int main(){
 std::cout<<"case,frame,independent,span_ns,usable,new_down_opportunity,active_move\n";
 for(bool moving_endpoint:{false,true}){
  bvi::Candidate candidate;
  for(int i=0;i<4;++i){
   const bvi::Ns t=10'000'000+i*20'000'000;
   bvi::Key key;key.capture=t;key.ready=t;key.sequence=i+1;
   bvi::Observation o;o.count=1;auto&d=o.parts[0];
   d.key=key;d.q.front={320.0+(moving_endpoint?i:0),500};d.q.width=140;d.q.depth=100;
   d.body=d.contact=bvi::Support::supported;d.front_end=d.rear_end=true;d.left=d.right=true;
   d.line_unique=true;d.line_id=7;d.hit={320,500.0-i};d.measured_depth=100;
   // A changing current frame (for instance a moving measured line), but
   // exactly unchanged endpoint descriptor in the stationary case.
   d.rgb_signature=100+i;d.signature=200+(moving_endpoint?i:0);
   candidate.relate(o,key,t);
   bvi::Guard g;g.now=t;g.gate_deadline=g.plan_deadline=t+100'000'000;
   g.last_contact=t;g.execution="never_executed";g.prefix=0;g.free_contacts=4;
   auto fresh=bvi::constrain(o,g);
   g.execution="known_down";g.prefix=1;g.attachment_query=0;
   auto active=bvi::constrain(o,g);
   const auto&r=o.relations[0];
   std::cout<<(moving_endpoint?"moving_endpoint":"stationary_endpoint")<<','<<i+1<<','
    <<int(r.independent)<<','<<r.span<<','<<r.usable<<','<<fresh.down[0]<<','<<active.move<<'\n';
   assert(active.move);
   if(!moving_endpoint){assert(r.independent==1);assert(!r.usable);assert(!fresh.down[0]);}
   else if(i>=2){assert(r.usable);assert(fresh.down[0]);}
  }
 }
}
