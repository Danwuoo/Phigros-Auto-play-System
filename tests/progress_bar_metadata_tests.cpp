#include "../apps/chart_experiment/progress_bar_metadata.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace rhythm::experimental;
namespace {
std::size_t checks=0;
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
std::vector<std::uint8_t> image(){return std::vector<std::uint8_t>(1280u*720u*3u,30);}
void pixel(std::vector<std::uint8_t>& p,int x,int y,std::uint8_t v) {
    const auto at=(static_cast<std::size_t>(y)*1280+static_cast<std::size_t>(x))*3;
    p[at]=p[at+1]=p[at+2]=v;
}
void prefix(std::vector<std::uint8_t>& p,int columns,int top=0,int rows=3) {
    for(int y=top;y<top+rows;++y)for(int x=0;x<columns;++x)pixel(p,x,y,250);
}
ProgressBarMetadata measure(const std::vector<std::uint8_t>& p,ProgressBarOptions o={}) {
    return measure_progress_bar_metadata(1280,720,3840,p,o);
}
}
int main() {
try {
    auto p=image();prefix(p,467);auto r=measure(p);
    check(r.valid&&r.candidate_rows==3&&r.bar_height==3&&r.endpoint_lower_column==467&&r.endpoint_upper_column==467,"exact thin prefix not retained");
    check(r.row_count==7&&r.pixel_reads==1280u*7u&&!r.pixels_retained&&!r.input_permission&&r.diagnostic_only,"metadata budget or authority changed");
    check(r.rows[0].bright_runs==1&&r.rows[3].status==ProgressBarRowStatus::empty_dark,"per-row evidence missing");
    p=image();prefix(p,467,0,2);pixel(p,467,0,190);pixel(p,468,0,190);pixel(p,467,1,190);r=measure(p);
    check(r.valid&&r.rows[0].endpoint_lower_column==467&&r.rows[0].endpoint_upper_column==469&&r.endpoint_upper_column==469,"transition hull was averaged or tightened");
    p=image();prefix(p,100,0,1);prefix(p,102,1,1);r=measure(p);
    check(!r.valid&&std::string(r.reason)=="contradictory_row_endpoints"&&r.endpoint_lower_column==100&&r.endpoint_upper_column==102,"contradictory rows silently selected a best endpoint");
    p=image();prefix(p,200);pixel(p,100,0,30);r=measure(p);
    check(!r.valid&&r.rows[0].status==ProgressBarRowStatus::trailing_bright_segment&&r.rows[0].bright_runs==2,"prefix gap was repaired");
    p=image();prefix(p,200);pixel(p,900,0,250);r=measure(p);
    check(!r.valid&&r.rows[0].bright_runs==2,"extra trailing bright segment was erased");
    p=image();prefix(p,200);pixel(p,900,0,190);check(!measure(p).valid,"unsupported trailing pixels ignored");
    p=image();prefix(p,200);pixel(p,0,0,30);check(!measure(p).valid,"prefix need not begin at x0");
    check(!measure(image()).valid,"empty image generated a progress endpoint");
    p=image();prefix(p,1280);check(!measure(p).valid,"full-width bar became a false completed clock");
    p=image();prefix(p,200,0,7);check(!measure(p).valid,"bar taller than six rows accepted");
    p=image();prefix(p,200,0,1);prefix(p,200,2,1);check(!measure(p).valid,"disconnected vertical rows accepted");
    p=image();for(int y=0;y<3;++y)for(int x=0;x<1280;++x)pixel(p,x,y,static_cast<std::uint8_t>(255-x*225/1279));
    check(!measure(p).valid,"smooth header gradient mistaken for a sharp progress edge");
    p=image();prefix(p,1,7,1);r=measure(p,{7,8,2});check(r.valid&&r.pixel_reads==1280&&r.endpoint_lower_column==1,"bounded eighth row or left boundary rejected");
    p=image();prefix(p,1279,0,1);r=measure(p);check(r.valid&&r.endpoint_upper_column==1279,"rightmost non-full endpoint rejected");
    check(!measure_progress_bar_metadata(1279,720,3840,p).valid,"unsupported width accepted");
    check(!measure_progress_bar_metadata(1280,720,3839,p).valid,"short stride accepted");
    check(!measure_progress_bar_metadata(1280,720,16385,p).valid,"unbounded stride accepted");
    p.pop_back();r=measure(p);check(!r.valid&&r.pixel_reads==0,"truncated input read pixels");
    p=image();for(const auto o:{ProgressBarOptions{-1,7,2},ProgressBarOptions{0,9,2},ProgressBarOptions{2,2,2},ProgressBarOptions{0,7,5}})
        check(!measure(p,o).valid,"unsupported row/transition option accepted");
    r=measure(p,{0,8,2});check(r.pixel_reads<=1280u*8u&&r.row_count<=8,"fixed work/storage bounds exceeded");
    std::cout<<checks<<" progress metadata checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
