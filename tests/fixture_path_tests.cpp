#include "../apps/touch_probe/fixture_path.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace pas;
unsigned checks=0;
void require(bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
TouchEvidence moved() {
    TouchEvidence result;result.complete=true;result.active=4;result.downs=4;result.moves=4;result.sequence=8;
    for(int id=0;id<4;++id)result.samples.push_back({static_cast<std::uint32_t>(id+1),1,id,256*(id+1),541});
    for(int id=0;id<4;++id)result.samples.push_back({static_cast<std::uint32_t>(id+5),2,id,256*(id+1),id?541:481});
    return result;
}
std::vector<ExpectedPointer> path(bool full=false) {
    std::vector<ExpectedPointer> result;
    for(int id=0;id<4;++id)result.push_back({1,id,256*(id+1),540});
    result.push_back({2,0,256,480});
    if(full)for(int id=0;id<4;++id)result.push_back({3,id,256*(id+1),id?540:480});
    return result;
}
fixture_probe::FixturePathCheck check(const TouchEvidence& evidence,bool full=false) {
    return fixture_probe::verify_fixture_path(evidence,path(full),3,full?0:4,{1,2,3});
}
}
int main() {
try {
    const auto raw=moved();require(!verify_touch_path(raw,path(),3,4,{1,2,3}).passed,"regression no longer models retained mismatch");
    const auto adapted=check(raw);require(adapted.path.passed&&adapted.redundant_stationary_moves==3,"four-pointer snapshot adaptation failed");
    require(raw.samples.size()==8&&raw.moves==4&&raw.samples.back().phase==2,"adapter mutated raw evidence");
    auto final=raw;final.active=0;final.ups=4;
    for(int id=0;id<4;++id)final.samples.push_back({static_cast<std::uint32_t>(9+id),3,id,256*(id+1),id?541:481});
    require(check(final,true).path.passed,"complete path rejected stable per-pointer MOVE snapshots");
    auto bad=raw;bad.samples[5].x+=4;require(!check(bad).path.passed,"stationary drift suppressed");
    bad=raw;bad.samples[1].x-=3;bad.samples[5].x+=3;require(!check(bad).path.passed,"opposite tolerance endpoint drift suppressed");
    bad=raw;bad.samples[5].android_id=19;require(!check(bad).path.passed,"unknown-ID MOVE suppressed");
    bad=raw;bad.samples[5].phase=3;require(!check(bad).path.passed,"wrong phase suppressed");
    bad=raw;bad.samples[4].x+=20;require(!check(bad).path.passed,"primary off-path MOVE suppressed");
    bad=raw;bad.samples.erase(bad.samples.begin()+4);require(!check(bad).path.passed,"missing primary target hidden by stationary MOVE");
    bad=raw;bad.samples[4].y=510;require(!check(bad).path.passed,"intermediate primary MOVE became target");
    bad=raw;bad.samples.insert(bad.samples.begin()+1,{2,2,1,512,540});require(!check(bad).path.passed,"MOVE before stationary DOWN suppressed");
    bad=final;bad.samples.push_back({13,2,1,512,541});require(!check(bad,true).path.passed,"released-ID MOVE suppressed");
    bad=final;bad.samples.insert(bad.samples.end()-1,{12,4,1,512,541});require(!check(bad,true).path.passed,"unexpected CANCEL hidden");
    bad=raw;bad.samples.push_back({9,2,0,256,481});require(!check(bad).path.passed,"extra primary MOVE hidden");
    bad=raw;bad.complete=false;require(!check(bad).path.passed,"incomplete evidence accepted");
    bad=raw;bad.samples.resize(65);require(!check(bad).path.passed,"visual-ring bound not enforced");
    require(!fixture_probe::verify_fixture_path(raw,path(),3,4,{1,1}).path.passed,"duplicate stationary ID accepted");
    require(!fixture_probe::verify_fixture_path(raw,path(),3,4,{19}).path.passed,"stationary ID without DOWN accepted");
    auto wrong_path=path();wrong_path.push_back({2,1,512,540});
    require(!fixture_probe::verify_fixture_path(raw,wrong_path,3,4,{1,2,3}).path.passed,"requested MOVE called stationary");
    std::cout<<checks<<" fixture path adapter checks passed; no device.\n";return 0;
}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
