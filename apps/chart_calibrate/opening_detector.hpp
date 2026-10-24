#pragma once
#include "rhythm/visual_candidates.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rhythm::calibration {
struct OpeningCenterMetrics { double x=0,y=0,width=0;std::size_t rows=0; };
struct OpeningDetection {
    bool valid=false;
    // A run detector cannot establish complete note coverage or no occlusion.
    bool centers_complete_unoccluded=false;
    std::string reason="invalid_rgb_profile";
    std::vector<rhythm::VisualCenter> centers;
    std::vector<OpeningCenterMetrics> metrics;
    std::size_t qualifying_runs=0,groups=0,ignored_short_runs=0,ignored_long_runs=0;
};

// Frozen research detector: RGB888, 1280x720 normalized image, rows [30,570),
// R<80, G>140, B>185, horizontal runs [70,180], >=2 adjacent rows per group,
// neighbouring-row centre change <=2px. No OCR, note type or completeness claim.
// Padding is accepted, but the exact bounded payload must be supplied.
inline OpeningDetection detect_opening_centers(int width,int height,int stride,
                                               std::span<const std::uint8_t> rgb) {
    OpeningDetection result;
    if(width!=1280||height!=720||stride<width*3||stride>16384||
       rgb.size()!=static_cast<std::size_t>(stride)*static_cast<std::size_t>(height))return result;
    struct Group {
        int last_y=0,last_x2=0;
        std::int64_t sum_x2=0,sum_y=0;
        std::vector<int> widths;
    };
    std::vector<Group> groups;
    const auto fail=[&](const char* reason) {
        result.valid=false;result.reason=reason;result.centers.clear();result.metrics.clear();
        result.groups=groups.size();return result;
    };
    const auto cyan=[&](int x,int y) {
        const auto at=static_cast<std::size_t>(y)*static_cast<std::size_t>(stride)+static_cast<std::size_t>(x)*3;
        return rgb[at]<80&&rgb[at+1]>140&&rgb[at+2]>185;
    };
    for(int y=30;y<570;++y) {
        int x=0;
        while(x<width) {
            while(x<width&&!cyan(x,y))++x;
            if(x==width)break;
            const int begin=x;
            while(x<width&&cyan(x,y))++x;
            const int length=x-begin;
            if(length<70){++result.ignored_short_runs;continue;}
            if(length>180){++result.ignored_long_runs;continue;}
            if(++result.qualifying_runs>4096)return fail("qualifying_run_budget_exhausted");
            const int center_x2=begin+x-1;
            std::size_t selected=groups.size(),compatible=0;
            for(std::size_t i=0;i<groups.size();++i)if(groups[i].last_y>=y-1&&
                std::abs(groups[i].last_x2-center_x2)<=4) {selected=i;++compatible;}
            if(compatible>1)return fail("ambiguous_run_grouping");
            if(!compatible) {
                if(groups.size()>=128)return fail("run_group_budget_exhausted");
                groups.emplace_back();selected=groups.size()-1;
            }
            auto& group=groups[selected];
            if(group.widths.size()>=540)return fail("group_row_budget_exhausted");
            group.last_y=y;group.last_x2=center_x2;group.sum_x2+=center_x2;group.sum_y+=y;group.widths.push_back(length);
        }
    }
    result.groups=groups.size();
    for(auto& group:groups)if(group.widths.size()>=2) {
        if(result.centers.size()>=8)return fail("too_many_centers");
        const auto rows=group.widths.size();
        const double x=static_cast<double>(group.sum_x2)/(2*static_cast<double>(rows));
        const double y=static_cast<double>(group.sum_y)/static_cast<double>(rows);
        std::sort(group.widths.begin(),group.widths.end());
        const auto mid=rows/2;
        const double median=rows%2?static_cast<double>(group.widths[mid]):
            (static_cast<double>(group.widths[mid-1])+group.widths[mid])/2;
        result.centers.push_back({"cyan-"+std::to_string(result.centers.size()),x,y});
        result.metrics.push_back({x,y,median,rows});
    }
    if(result.centers.empty())return fail("no_supported_cyan_centers");
    result.valid=true;result.reason="bounded_cyan_run_centers;completeness_unverified";return result;
}
} // namespace rhythm::calibration
