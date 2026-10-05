// Independent synthetic raster, conditional on supplied current ROI/all-lines.
// Unmodified BVI extract -> relate -> constrain, NOT full GameObserver or game.
#include "bvi.hpp"
#include <cassert>
#include <iostream>
#include <vector>
int main(){
 std::cout<<"case,frame,current_contact,descriptor_same_as_first,rgb_same_as_previous,independent,span_ns,usable,new_down_opportunity\n";
 for(bool moving_note:{false,true}){
  bvi::Candidate candidate;std::uint64_t first_signature=0,previous_rgb=0;
  const int line_y[]{520,510,504,500};
  for(int i=0;i<4;++i){
   std::vector<std::uint8_t> rgb(640*640*3,0);
   auto put=[&](int x,int y,int r,int g,int b){auto p=(y*640+x)*3;rgb[p]=r;rgb[p+1]=g;rgb[p+2]=b;};
   const int cx=320+(moving_note?i:0),cy=500;
   // Blue Tap core 79 x 9 px. Ground-truth geometry is declared here,
   // independent of candidate predicates/results.
   for(int y=cy-4;y<=cy+4;++y)for(int x=cx-39;x<=cx+39;++x)put(x,y,40,190,255);
   for(int x=20;x<=620;++x)put(x,line_y[i],255,255,255);
   bvi::Key key;key.capture=10'000'000+i*20'000'000;key.ready=key.capture;key.sequence=i+1;
   bvi::View view{rgb,640,640,1920,key};
   bvi::Query query{{double(cx),500},78,8,0,true};
   bvi::Line line{{320,double(line_y[i])},0,600,7};
   auto o=candidate.extract(view,std::span(&query,1),std::span(&line,1));
   assert(!o.invalid&&o.count==1);const auto&d=o.parts[0];
   assert(d.front_end&&d.left&&d.right);
   if(i==0)first_signature=d.signature;
   const bool same=d.signature==first_signature,rgb_same=i&&d.rgb_signature==previous_rgb;
   previous_rgb=d.rgb_signature;
   candidate.relate(o,key,key.capture);
   bvi::Guard g;g.now=key.capture;g.gate_deadline=g.plan_deadline=g.now+100'000'000;
   g.execution="never_executed";g.prefix=0;g.free_contacts=4;
   const auto c=bvi::constrain(o,g);const auto&r=o.relations[0];
   const bool contact=d.contact==bvi::Support::supported&&d.line_unique;
   std::cout<<(moving_note?"moving_tap_line_chase":"stationary_tap_line_chase")<<','<<i+1<<','
    <<contact<<','<<same<<','<<rgb_same<<','<<int(r.independent)<<','<<r.span<<','<<r.usable<<','<<c.down[0]<<'\n';
   assert(!rgb_same);assert(contact==(i>=2));
   if(!moving_note){assert(same);assert(r.independent==1);assert(!r.usable);assert(!c.down[0]);}
   else if(i>=2){assert(r.usable);assert(c.down[0]);}
  }
 }
}
