// Isolated arithmetic probes of formulas inspected at commit
// 74e54437d4a3ad2b2bd1a3b09312211a92f2359e. This is NOT a production
// observer/tracker build, RGB test, game replay, or timing benchmark.
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <tuple>
#include <vector>
struct Point { double arrival_ms, distance_px; };
struct Fit { double speed_px_ms, root_ms, rms_px, uncertainty_ms; };
Fit fit(std::vector<Point> p) {
    const double now=p.back().arrival_ms;
    while (p.size()>6 || now-p.front().arrival_ms>90.0) p.erase(p.begin());
    double mt=0,md=0,den=0,num=0;
    for(auto q:p){mt+=q.arrival_ms-now;md+=q.distance_px;}
    mt/=p.size();md/=p.size();
    for(auto q:p){double t=q.arrival_ms-now-mt;den+=t*t;num+=t*(q.distance_px-md);}
    const double v=num/den,d=md-v*mt;double sq=0;
    for(auto q:p){double err=q.distance_px-(d+v*(q.arrival_ms-now));sq+=err*err;}
    const double residual=std::sqrt(sq/p.size());
    const double error=std::max({2.0,residual,std::abs(p.back().distance_px-d)});
    return {v,now-d/v,residual,error/std::abs(v)};
}
int main(){
 std::cout<<std::fixed<<std::setprecision(6);
 std::cout<<"kind,case,arrival_ms,measured_distance_px,fit_speed_px_s,predicted_root_ms,true_root_ms,bias_ms,residual_px,uncertainty_ms,relative_lag_ms\n";
 for(auto [name,offset,slope]:std::vector<std::tuple<std::string_view,double,double>>{
  {"constant_age_0",0,0},{"constant_age_20",20,0},{"affine_age_20_plus_half_source",20,.5}}){
   std::vector<Point> p;for(int i=0;i<6;++i){double s=20.0*i;p.push_back({s+offset+slope*s,.8*(s-150)});}
   auto f=fit(p);std::cout<<"time,"<<name<<','<<p.back().arrival_ms<<','<<p.back().distance_px<<','<<f.speed_px_ms*1000<<','<<f.root_ms<<",150.000000,"<<f.root_ms-150<<','<<f.rms_px<<','<<f.uncertainty_ms<<','<<slope*100<<'\n';
 }
 std::cout<<"kind,case,step,prior_abs_distance_px,current_abs_distance_px,allowed_increment_px,preserve_predicate\n";
 // src/game_tracking.cpp:299-300. All other conditions are assumed true.
 for(double increment:{12.0,12.8,12.800001,49.0}){
  double prior=100;for(int i=1;i<=6;++i){double current=prior+increment,allowed=std::max(12.0,128*.10);
   bool ok=current<=prior+allowed;std::cout<<"preserve,width128_increment"<<increment<<','<<i<<','<<prior<<','<<current<<','<<allowed<<','<<(ok?1:0)<<'\n';prior=current;}
 }
}
