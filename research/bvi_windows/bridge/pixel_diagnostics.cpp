#include "pixel_diagnostics.hpp"
#include <cmath>
#include <stdexcept>

namespace pas::bvi_offline::diagnostic {
using J=nlohmann::json;
J rgb_at(const Frame& f,double x,double y) {
    if(f.width<=0||f.width>1280||f.height<=0||f.height>720||
       f.stride<f.width*3||f.stride>3840||
       f.rgb.size()!=static_cast<std::size_t>(f.stride)*f.height||
       !std::isfinite(x)||!std::isfinite(y)||x<-1||x>f.width||y<-1||y>f.height)
        return nullptr;
    // Clip before integer conversion. NaN/huge coordinates cannot overflow
    // llround, and a malformed stride/storage can never index the RGB vector.
    const auto px=std::llround(x),py=std::llround(y);
    if(px<0||px>=f.width||py<0||py>=f.height)return nullptr;
    const auto offset=static_cast<std::size_t>(py)*f.stride+static_cast<std::size_t>(px)*3;
    return J::array({f.rgb[offset],f.rgb[offset+1],f.rgb[offset+2]});
}
J witness_review(const Frame& f,const bvi::Query& q) {
    if(!std::isfinite(q.front.x)||!std::isfinite(q.front.y)||
       std::abs(q.front.x)>8192||std::abs(q.front.y)>8192||
       !std::isfinite(q.angle)||!std::isfinite(q.width)||q.width<4||q.width>4096||
       !std::isfinite(q.depth)||q.depth<=0||q.depth>4095)
        throw std::invalid_argument("diagnostic query bound");
    J rows=J::array();
    const double ux=std::cos(q.angle),uy=std::sin(q.angle),nx=-uy,ny=ux;
    for(int normal=normal_first;normal<=normal_last;++normal) {
        J points=J::array();
        for(double along:{-q.width/2,-.35*q.width,0.,.35*q.width,q.width/2}) {
            const double x=q.front.x+along*ux+normal*nx,y=q.front.y+along*uy+normal*ny;
            const auto rgb=rgb_at(f,x,y);J failures=J::array();
            bool blue=false,white=false,black=false;
            if(rgb.is_null())failures.push_back("outside_frame");
            else {
                const int r=rgb[0],g=rgb[1],b=rgb[2];
                if(r<20||r>100)failures.push_back("R20..100");
                if(g<130||g>230)failures.push_back("G130..230");
                if(b<180||b>255)failures.push_back("B180..255");
                if(b<g+20)failures.push_back("B>=G+20");
                blue=failures.empty();white=r>=240&&g>=240&&b>=240;
                black=r<=12&&g<=12&&b<=12;
            }
            points.push_back({{"along",along},{"pixel",{std::llround(x),std::llround(y)}},
                {"rgb",rgb},{"v3_blue",blue},{"v3_blue_failures",failures},
                {"v3_white",white},{"v3_black",black}});
        }
        rows.push_back({{"normal",normal},{"points",points}});
    }
    return rows;
}
} // namespace pas::bvi_offline::diagnostic
