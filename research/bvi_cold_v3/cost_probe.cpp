// Bounded synthetic function timing; excludes renderer, adapter, owner, RPC and game.
#ifndef BVI_HEADER
#define BVI_HEADER "bvi.hpp"
#endif
#include BVI_HEADER
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace bvi;
using J=nlohmann::json;
int main(int argc,char**argv){
 if(argc!=2||std::filesystem::exists(argv[1]))return 64;
 J result={{"schema","bvi.synthetic-function-cost.v1"},{"clock","std::chrono::steady_clock; not Windows QPC"},{"scope","extract+relate+constrain; no renderer/adapter/owner/RPC/journal/device"},{"timing_qualification",false},{"metadata_bytes",Candidate::metadata_bytes()},{"cases",J::array()}};
 constexpr int W=1280,H=720,N=256,Warm=16;
 for(const std::string scenario:{"single_hold","128_exact_aliases_16_lines","128_distinct_16_lines","invalid_source"}){
  std::vector<std::uint8_t>rgb(W*H*3);std::vector<Query>qs;std::vector<Line>ls;
  const int count=scenario.starts_with("128")?128:1;
  for(int i=0;i<count;++i)qs.push_back({{scenario=="128_distinct_16_lines"?double(40+(i%16)*76):640.,scenario=="128_distinct_16_lines"?double(90+(i/16)*76):500.},scenario=="128_distinct_16_lines"?40.:140.,scenario=="128_distinct_16_lines"?48.:160.,0,false});
  for(const auto&q:qs)for(int y=std::max(0,int(q.front.y-q.depth));y<=int(q.front.y)&&y<H;++y)for(int x=std::max(0,int(q.front.x-q.width/2));x<=int(q.front.x+q.width/2)&&x<W;++x){auto p=(y*W+x)*3;bool rail=std::abs(std::abs(x-q.front.x)-q.width/2)<=1.5;rgb[p]=rail?255:40;rgb[p+1]=rail?255:190;rgb[p+2]=255;}
  for(int i=0;i<(count==128?16:1);++i){double y=i?15.+i*40.:480.;ls.push_back({{640,y},0,1270,std::uint64_t(i+1)});for(int yy=std::max(0,int(y)-2);yy<=int(y)+2&&yy<H;++yy)for(int x=5;x<1275;++x){auto p=(yy*W+x)*3;rgb[p]=rgb[p+1]=rgb[p+2]=255;}}
  Candidate c;std::vector<long long>samples;std::size_t rejected=0,moves=0,downs=0;std::uint64_t maxProbes=0;
  for(int i=0;i<N+Warm;++i){Key key;key.capture=key.ready=100000000+std::int64_t(i)*20000000;key.sequence=i+1;key.source_valid=scenario!="invalid_source";Guard g;g.execution="never_executed";g.prefix=0;g.free_contacts=5;g.now=key.capture+1000000;g.gate_deadline=g.plan_deadline=g.now+50000000;
   const auto start=std::chrono::steady_clock::now();auto o=c.extract({rgb,W,H,W*3,key},qs,ls);c.relate(o,key,g.now);auto action=constrain(o,g);const auto end=std::chrono::steady_clock::now();
   if(i<Warm)continue;samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count());std::size_t d=0;for(auto b:action.down)d+=b;downs+=d;moves+=action.move;rejected+=!action.move&&!d;maxProbes=std::max(maxProbes,o.probes);
  }
  auto sorted=samples;std::sort(sorted.begin(),sorted.end());auto q=[&](double p){return sorted[std::size_t(std::ceil(p*sorted.size()))-1];};
  result["cases"].push_back({{"scenario",scenario},{"n",N},{"warmup",Warm},{"queries",qs.size()},{"lines",ls.size()},{"p50_ns",q(.50)},{"p95_ns",q(.95)},{"p99_ns",q(.99)},{"max_ns",sorted.back()},{"jitter_max_minus_p50_ns",sorted.back()-q(.50)},{"all_rejected_observations",rejected},{"move_proposals",moves},{"down_proposals",downs},{"max_counted_probes",maxProbes},{"raw_ns",samples}});
 }
 std::ofstream f(argv[1]);if(!f)return 2;f<<result.dump(2)<<'\n';return f?0:2;
}
