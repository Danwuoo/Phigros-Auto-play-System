#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rhythm::experimental {
// Pixel classification only. A duration*width ratio would be an additional
// unqualified game/rendering hypothesis, never a song clock or input lease.
struct ProgressBarOptions {
    int row_begin=0,row_end=7; // Half-open, inside the first eight actual rows.
    int maximum_transition_columns=2;
};
enum class ProgressBarRowStatus {
    not_measured,empty_dark,prefix,full_width,no_left_prefix,
    trailing_bright_segment,unsupported_transition,unsupported_trailing_pixels
};
inline const char* progress_bar_row_status_name(ProgressBarRowStatus s) {
    switch(s) {
    case ProgressBarRowStatus::not_measured:return "not_measured";
    case ProgressBarRowStatus::empty_dark:return "empty_dark";
    case ProgressBarRowStatus::prefix:return "prefix";
    case ProgressBarRowStatus::full_width:return "full_width_unknown";
    case ProgressBarRowStatus::no_left_prefix:return "no_left_prefix";
    case ProgressBarRowStatus::trailing_bright_segment:return "trailing_bright_segment";
    case ProgressBarRowStatus::unsupported_transition:return "unsupported_transition";
    case ProgressBarRowStatus::unsupported_trailing_pixels:return "unsupported_trailing_pixels";
    }
    return "invalid_row_status";
}
struct ProgressBarRowMetadata {
    int y=-1,bright_prefix_columns=0,transition_columns=0,bright_runs=0;
    // Raw bounds remain available for diagnosing a rejected left boundary.
    // They do not turn a disconnected/unsupported row into a valid bar.
    int first_bright_column=-1,last_bright_column=-1;
    int endpoint_lower_column=-1,endpoint_upper_column=-1;
    int minimum_channel=255,maximum_channel=0;
    bool definite_dark_after_prefix=false;
    ProgressBarRowStatus status=ProgressBarRowStatus::not_measured;
};
struct ProgressBarMetadata {
    static constexpr bool pixels_retained=false,input_permission=false,diagnostic_only=true;
    bool valid=false;
    const char* reason="not_measured";
    std::size_t pixel_reads=0,row_count=0,candidate_rows=0;
    int first_candidate_row=-1,bar_height=0;
    // Hull of all per-row endpoint intervals, never an average or intersection.
    // This may remain populated when incompatible rows make valid=false.
    int endpoint_lower_column=-1,endpoint_upper_column=-1;
    std::array<ProgressBarRowMetadata,8> rows{};
};
inline ProgressBarMetadata measure_progress_bar_metadata(
        int width,int height,int stride,std::span<const std::uint8_t> rgb,
        ProgressBarOptions options={}) {
    ProgressBarMetadata out;
    if(width!=1280||height!=720||stride<3840||stride>16384||
       rgb.size()!=static_cast<std::size_t>(stride)*720) {
        out.reason="unsupported_geometry";return out;
    }
    if(options.row_begin<0||options.row_begin>=options.row_end||options.row_end>8||
       options.maximum_transition_columns<0||options.maximum_transition_columns>4) {
        out.reason="unsupported_options";return out;
    }
    bool unsupported=false;int last_candidate=-1,common_lower=0,common_upper=1280;
    for(int y=options.row_begin;y<options.row_end;++y) {
        auto& row=out.rows[out.row_count++];row.y=y;
        int mode=0; // Bright prefix, optional transition, definite dark suffix.
        bool previous_bright=false,trailing_bright=false,trailing_other=false,any_other=false;
        for(int x=0;x<1280;++x) {
            const auto at=static_cast<std::size_t>(y)*static_cast<std::size_t>(stride)+static_cast<std::size_t>(x)*3;
            const int lo=std::min({rgb[at],rgb[at+1],rgb[at+2]});
            const int hi=std::max({rgb[at],rgb[at+1],rgb[at+2]});++out.pixel_reads;
            row.minimum_channel=std::min(row.minimum_channel,lo);row.maximum_channel=std::max(row.maximum_channel,hi);
            const bool bright=lo>=224&&hi-lo<=16,dark=hi<=160;
            if(bright){if(row.first_bright_column<0)row.first_bright_column=x;row.last_bright_column=x;}
            if(bright&&!previous_bright)++row.bright_runs;previous_bright=bright;
            if(mode==0&&bright){++row.bright_prefix_columns;continue;}
            if(mode==0)mode=dark?2:1;
            if(mode==1) {
                if(bright)trailing_bright=true;
                else if(dark){mode=2;row.definite_dark_after_prefix=true;}
                else {++row.transition_columns;any_other=true;}
            } else {
                row.definite_dark_after_prefix=true;
                if(bright)trailing_bright=true;
                else if(!dark){trailing_other=true;any_other=true;}
            }
        }
        if(row.bright_prefix_columns==1280)row.status=ProgressBarRowStatus::full_width;
        else if(row.bright_prefix_columns==0)row.status=
            row.bright_runs==0&&!any_other?ProgressBarRowStatus::empty_dark:ProgressBarRowStatus::no_left_prefix;
        else if(trailing_bright)row.status=ProgressBarRowStatus::trailing_bright_segment;
        else if(trailing_other)row.status=ProgressBarRowStatus::unsupported_trailing_pixels;
        else if(!row.definite_dark_after_prefix||row.transition_columns>options.maximum_transition_columns)
            row.status=ProgressBarRowStatus::unsupported_transition;
        else {
            row.status=ProgressBarRowStatus::prefix;
            row.endpoint_lower_column=row.bright_prefix_columns;
            row.endpoint_upper_column=row.bright_prefix_columns+row.transition_columns;
            if(!out.candidate_rows){out.first_candidate_row=y;out.endpoint_lower_column=row.endpoint_lower_column;
                out.endpoint_upper_column=row.endpoint_upper_column;}
            else {out.endpoint_lower_column=std::min(out.endpoint_lower_column,row.endpoint_lower_column);
                out.endpoint_upper_column=std::max(out.endpoint_upper_column,row.endpoint_upper_column);}
            ++out.candidate_rows;last_candidate=y;
            common_lower=std::max(common_lower,row.endpoint_lower_column);
            common_upper=std::min(common_upper,row.endpoint_upper_column);
        }
        if(row.status!=ProgressBarRowStatus::prefix&&row.status!=ProgressBarRowStatus::empty_dark)unsupported=true;
    }
    if(!out.candidate_rows){out.reason=unsupported?"unsupported_rows":"no_progress_prefix";return out;}
    out.bar_height=last_candidate-out.first_candidate_row+1;
    if(unsupported){out.reason="unsupported_rows";return out;}
    if(out.candidate_rows>6||out.bar_height!=static_cast<int>(out.candidate_rows)) {
        out.reason="unsupported_bar_height_or_row_gap";return out;
    }
    if(common_lower>common_upper){out.reason="contradictory_row_endpoints";return out;}
    out.valid=true;out.reason="bounded_neutral_prefix_metadata_only";return out;
}
} // namespace rhythm::experimental
