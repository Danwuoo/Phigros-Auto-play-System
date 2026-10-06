#include "bvi.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
void box(std::vector<std::uint8_t>& p,int x0,int y0,int x1,int y1,std::array<std::uint8_t,3> c){for(int y=std::max(0,y0);y<std::min(720,y1);++y)for(int x=std::max(0,x0);x<std::min(1280,x1);++x)std::copy(c.begin(),c.end(),p.begin()+std::size_t(y)*3840+x*3);}
int main(){for(int variant=0;variant<4;++variant){auto b=std::make_unique<bvi::Candidate>();int allowed=0;for(int k=0;k<12;++k){std::vector<std::uint8_t> p(1280*720*3,variant>=2?0:30);box(p,115,499,1165,502,{255,255,255});int y=440+k*8;box(p,350,y-100,451,y+1,variant%2?std::array<std::uint8_t,3>{40,190,255}:std::array<std::uint8_t,3>{155,233,255});box(p,350,y-100,352,y+1,{255,255,255});box(p,449,y-100,451,y+1,{255,255,255});bvi::Key key{{1,1,1,1},(k+1)*20'000'000LL,(k+1)*20'000'000LL,std::uint64_t(k+1),true};const std::array<bvi::Query,1> q={bvi::Query{{400,double(y)},100,100,0,false}};const std::array<bvi::Line,1> l={bvi::Line{{640,500},0,1050,1}};auto o=b->extract({p,1280,720,3840,key},q,l);b->relate(o,key,key.capture);auto c=bvi::constrain(o,{key.capture,key.capture+100'000'000,key.capture+100'000'000,0,"never_executed",0,0,0,5,false});allowed+=c.down[0];std::cout<<"variant="<<variant<<" frame="<<k<<" body="<<bvi::name(o.parts[0].body)<<" front="<<o.parts[0].front_end<<" contact="<<bvi::name(o.parts[0].contact)<<" relation="<<o.relations[0].usable<<" down="<<c.down[0]<<'\n';}std::cout<<"variant="<<variant<<" allowed="<<allowed<<'\n';}}
