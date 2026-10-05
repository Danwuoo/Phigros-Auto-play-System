#define main independent_review_main
#include "../../../../../../research/bvi_cold_v2/independent_tests.cpp"
#undef main
int main(){using namespace review;auto q=hold(320,504,100,.25);auto lc=world(q,0,-4);auto l=line(lc.x,lc.y,.25);auto f=standard(q,true,{l});auto rgb=render(f);json out=json::array();for(int k=1;k<=4;++k)for(int flank:{-1,1})for(int side:{-1,1}){auto p=world(q,49*flank,k*side);int x=int(std::llround(p.x)),y=int(std::llround(p.y));auto ix=(static_cast<std::size_t>(y)*W+x)*3;double d=-(x-l.center.x)*std::sin(l.angle)+(y-l.center.y)*std::cos(l.angle);out.push_back({{"normal_offset",k*side},{"tangent_offset",49*flank},{"pixel",{x,y}},{"rgb",{rgb[ix],rgb[ix+1],rgb[ix+2]}},{"line_normal_distance",d},{"inside_2_8_mask",std::abs(d)<=2.8}});}std::cout<<out.dump(2)<<'\n';}
