#pragma once
#include "../chart_calibrate/opening_detector.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rhythm::experimental {
// Explicit normal-Level classic Hold/HL skin candidate. This does not change
// the archived Tap cyan predicate, establish image completeness or identify a
// native note. White/gold tail caps and a light body alone are never heads.
// Horizontal only; there is no rotated-head or arbitrary-tint qualification.
inline calibration::OpeningDetection detect_classic_hold_head_centers(
        int width,int height,int stride,std::span<const std::uint8_t> rgb) {
    calibration::OpeningDetection result;
    if(width!=1280||height!=720||stride<3840||stride>16384||rgb.size()!=static_cast<std::size_t>(stride)*720)return result;
    result.reason="no_supported_classic_hold_head";
    const auto pixel=[&](int x,int y){
        const auto at=static_cast<std::size_t>(y)*static_cast<std::size_t>(stride)+static_cast<std::size_t>(x)*3;
        return std::array<int,3>{rgb[at],rgb[at+1],rgb[at+2]};
    };
    const auto light=[&](int x,int y){const auto c=pixel(x,y);
        return c[0]>=80&&c[0]<=210&&c[1]>=160&&c[2]>=185&&c[1]-c[0]>=25&&c[2]-c[0]>=35;
    };
    const auto white=[&](int x,int y){const auto c=pixel(x,y);
        return std::min({c[0],c[1],c[2]})>=210&&std::max({c[0],c[1],c[2]})-std::min({c[0],c[1],c[2]})<=30;
    };
    const auto gap=[&](int x,int y){const auto c=pixel(x,y);
        const bool dark=std::max({c[0],c[1],c[2]})<=130;
        const bool gold=c[0]>=140&&c[1]>=120&&c[2]<=130&&std::min(c[0],c[1])-c[2]>=60;
        return dark||gold;
    };
    const auto transition=[&](int x,int y){return !light(x,y)&&!white(x,y);};
    const auto side=[&](int x,int step,int y,double& center){
        int gaps=0,whites=0,cores=0,soft=0;
        while(x>=0&&x<1280&&transition(x,y)&&gaps<10){if(gap(x,y))++cores;else if(++soft>2)return false;++gaps;x+=step;}
        if(!gaps||!cores||x<0||x>=1280)return false;
        const auto first=x;
        while(x>=0&&x<1280&&white(x,y)&&whites<10){++whites;x+=step;}
        center=(first+x-step)/2.;
        return whites>0&&x>0&&x<1279&&transition(x,y);
    };
    struct Row {int y=0,a=0,b=0;double left_white=0,right_white=0;};
    struct Band {int last=0,last_x2=0;std::vector<Row> rows;};
    std::vector<Band> bands;bands.reserve(32);std::size_t runs=0;
    const auto fail=[&](const char* why){result.valid=false;result.reason=why;result.centers.clear();result.metrics.clear();result.groups=bands.size();return result;};
    for(int y=30;y<570;++y) {
        int x=0;
        while(x<1280) {
            while(x<1280&&!light(x,y))++x;if(x==1280)break;
            const auto begin=x;while(x<1280&&light(x,y))++x;
            if(++runs>16384)return fail("classic_hold_head_run_budget_exhausted");
            const auto length=x-begin;
            if(length<70||length>180)continue;
            double left_white=0,right_white=0;
            if(!side(begin-1,-1,y,left_white)||!side(x,1,y,right_white))continue;
            if(++result.qualifying_runs>4096)return fail("classic_hold_head_qualified_row_budget_exhausted");
            const auto x2=begin+x-1;std::size_t selected=bands.size(),compatible=0;
            for(std::size_t i=0;i<bands.size();++i)if(bands[i].last==y-1&&std::abs(bands[i].last_x2-x2)<=4){selected=i;++compatible;}
            if(compatible>1)return fail("classic_hold_head_band_ownership_ambiguous");
            if(!compatible) {
                if(bands.size()>=128)return fail("classic_hold_head_band_budget_exhausted");
                bands.push_back({y,x2,{}});selected=bands.size()-1;
            }
            auto& band=bands[selected];band.last=y;band.last_x2=x2;
            band.rows.push_back({y,begin,x-1,left_white,right_white});
        }
    }
    result.groups=bands.size();
    for(const auto& band:bands) {
      for(const bool body_above:{true,false}) {
        if(band.rows.size()<2)continue;
        const auto& terminal=body_above?band.rows.back():band.rows.front();
        // Normal: 847..895 cyan pixels per50px head. HL: approximately
        // 821..868 per50px. The gold skirt is not part of the cyan head.
        const auto count=static_cast<std::size_t>(std::lround((terminal.b-terminal.a+1)/17.));
        if(count<2||count>18||band.rows.size()<count)continue;
        const auto first_index=body_above?band.rows.size()-count:0;
        const auto& first=band.rows[first_index];const auto& last=band.rows[first_index+count-1];
        const auto& body_edge=body_above?first:last;const auto& outer_edge=body_above?last:first;
        const auto taper_left=outer_edge.left_white-body_edge.left_white;
        const auto taper_right=body_edge.right_white-outer_edge.right_white;
        if(taper_left<.75||taper_right<.75||taper_left>static_cast<double>(count)*1.5||taper_right>static_cast<double>(count)*1.5)continue;
        // A tapered body is not a terminal cap. Its preceding qualified rail
        // trajectory must stop tapering before the short cap, when present.
        if(band.rows.size()>=count+3) {
            const auto& a=body_above?band.rows[first_index-3]:band.rows[count];
            const auto& b=body_above?band.rows[first_index-1]:band.rows[count+2];
            if(std::abs(a.left_white-b.left_white)>.75||std::abs(a.right_white-b.right_white)>.75)continue;
        }
        double x=0;int min_width=1280,max_width=0;bool coherent=true;
        for(std::size_t i=first_index;i<first_index+count;++i) {
            const auto& row=band.rows[i];x+=(row.a+row.b)/2.;min_width=std::min(min_width,row.b-row.a+1);max_width=std::max(max_width,row.b-row.a+1);
            if(i>first_index) {
                const auto& previous=band.rows[i-1];const auto sign=body_above?1.:-1.;
                if((row.left_white-previous.left_white)*sign<-.5||(previous.right_white-row.right_white)*sign<-.5||
                   std::abs(row.left_white-previous.left_white)>2||std::abs(row.right_white-previous.right_white)>2)coherent=false;
            }
        }
        if(!coherent)continue;x/=static_cast<double>(count);
        if(max_width-min_width>std::max(6,max_width/8))continue;
        if(first.y<=30||last.y>=569)return fail("clipped_classic_hold_head");
        int above=0,below=0;
        for(const int distance:{2,4,6})for(const double fraction:{-.3,0.,.3}) {
            const auto col=static_cast<int>(std::lround(x+fraction*min_width));
            if(col<1||col>=1279)return fail("clipped_classic_hold_head");
            // Body support can continue beyond the head-search ROI; that is
            // not clipping of a complete head. These probes stay in the image.
            above+=light(col,first.y-distance)?1:0;below+=light(col,last.y+distance)?1:0;
        }
        if(above>=8&&below>=8)return fail("classic_hold_head_body_side_ambiguous");
        if(body_above?!(above>=8&&below<=1):!(below>=8&&above<=1))continue;
        const auto anchor_y=body_above?static_cast<double>(first.y)-.5:static_cast<double>(last.y)+.5;
        if(result.centers.size()>=8)return fail("too_many_classic_hold_head_centers");
        // Do not merge nearby heads: each complete image band has its own ID.
        result.centers.push_back({"classic-hold-head-"+std::to_string(result.centers.size()),x,anchor_y});
        result.metrics.push_back({x,anchor_y,static_cast<double>(max_width),count});
      }
    }
    if(!result.centers.empty()){result.valid=true;result.reason="horizontal_classic_hold_head_light_cyan_chevrons;completeness_unverified";}
    return result;
}
} // namespace rhythm::experimental
