#pragma once
#include "progress_bar_metadata.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rhythm::experimental {
inline constexpr std::size_t numeric_hud_raw_limit=64,numeric_hud_max_rows=7200;
inline constexpr std::int64_t numeric_hud_interval_ns=100'000'000;

// Fixed diagnostic text storage. Non-ASCII OCR is explicitly marked unknown;
// never reinterpret a letter, punctuation mark, or clipped token as a digit.
struct NumericHudRawText {
    std::array<char,numeric_hud_raw_limit> bytes{};
    std::size_t size=0;
    bool truncated=false,non_ascii=false;
    void assign(std::string_view text) noexcept {
        size=text.size()<bytes.size()?text.size():bytes.size();truncated=text.size()>bytes.size();non_ascii=false;
        for(std::size_t i=0;i<size;++i){const auto c=static_cast<unsigned char>(text[i]);
            if(c>127){bytes[i]='?';non_ascii=true;}else bytes[i]=static_cast<char>(c);}
    }
    void assign(std::wstring_view text) noexcept {
        size=text.size()<bytes.size()?text.size():bytes.size();truncated=text.size()>bytes.size();non_ascii=false;
        for(std::size_t i=0;i<size;++i){const auto c=static_cast<std::uint32_t>(text[i]);
            if(c>127){bytes[i]='?';non_ascii=true;}else bytes[i]=static_cast<char>(c);}
    }
    std::string_view view() const noexcept {return {bytes.data(),size};}
};
enum class NumericHudParseStatus {valid,empty,raw_too_long,non_ascii,non_numeric,wrong_length,out_of_range,ambiguous,invalid_limit};
inline const char* numeric_hud_parse_name(NumericHudParseStatus s) noexcept {
    switch(s){
    case NumericHudParseStatus::valid:return "valid";case NumericHudParseStatus::empty:return "empty";
    case NumericHudParseStatus::raw_too_long:return "raw_too_long";case NumericHudParseStatus::non_ascii:return "non_ascii";
    case NumericHudParseStatus::non_numeric:return "non_numeric";case NumericHudParseStatus::wrong_length:return "wrong_length";
    case NumericHudParseStatus::out_of_range:return "out_of_range";case NumericHudParseStatus::ambiguous:return "ambiguous";
    case NumericHudParseStatus::invalid_limit:return "invalid_limit";}
    return "unknown";
}
struct NumericHudParsedValue {std::optional<std::uint32_t> value;NumericHudParseStatus status=NumericHudParseStatus::empty;};
inline bool numeric_hud_space(char c) noexcept {return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f'||c=='\v';}
inline std::string_view numeric_hud_trim(std::string_view text) noexcept {
    while(!text.empty()&&numeric_hud_space(text.front()))text.remove_prefix(1);
    while(!text.empty()&&numeric_hud_space(text.back()))text.remove_suffix(1);
    return text;
}
inline NumericHudParsedValue parse_numeric_hud_score(std::string_view raw) noexcept {
    if(raw.size()>numeric_hud_raw_limit)return {{},NumericHudParseStatus::raw_too_long};
    const auto text=numeric_hud_trim(raw);
    if(text.empty())return {{},NumericHudParseStatus::empty};
    for(const unsigned char c:text)if(c>127)return {{},NumericHudParseStatus::non_ascii};
    for(const char c:text)if(c<'0'||c>'9')return {{},NumericHudParseStatus::non_numeric};
    if(text.size()!=7)return {{},NumericHudParseStatus::wrong_length};
    std::uint32_t number=0;for(const char c:text)number=number*10u+static_cast<std::uint32_t>(c-'0');
    if(number>1'000'000)return {{},NumericHudParseStatus::out_of_range};
    return {number,NumericHudParseStatus::valid};
}
inline bool numeric_hud_combo_label(std::string_view token) noexcept {
    constexpr std::string_view label="COMBO";if(token.size()!=label.size())return false;
    for(std::size_t i=0;i<token.size();++i){char c=token[i];if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
        if(c!=label[i])return false;}
    return true;
}
inline NumericHudParsedValue parse_numeric_hud_combo(std::string_view raw,std::size_t maximum_combo) noexcept {
    if(!maximum_combo||maximum_combo>200'000)return {{},NumericHudParseStatus::invalid_limit};
    if(raw.size()>numeric_hud_raw_limit)return {{},NumericHudParseStatus::raw_too_long};
    auto text=numeric_hud_trim(raw);if(text.empty())return {{},NumericHudParseStatus::empty};
    for(const unsigned char c:text)if(c>127)return {{},NumericHudParseStatus::non_ascii};
    std::optional<std::uint32_t> value;bool label_seen=false;
    while(!text.empty()){
        const auto end=text.find_first_of(" \t\r\n\f\v");const auto token=text.substr(0,end);
        if(numeric_hud_combo_label(token)){
            if(label_seen)return {{},NumericHudParseStatus::ambiguous};label_seen=true;
        }else{
            if(value)return {{},NumericHudParseStatus::ambiguous};
            for(const char c:token)if(c<'0'||c>'9')return {{},NumericHudParseStatus::non_numeric};
            if(token.empty()||token.size()>6)return {{},NumericHudParseStatus::wrong_length};
            std::uint32_t number=0;for(const char c:token)number=number*10u+static_cast<std::uint32_t>(c-'0');
            if(!number||number>maximum_combo)return {{},NumericHudParseStatus::out_of_range};value=number;
        }
        if(end==std::string_view::npos)break;text=numeric_hud_trim(text.substr(end));
    }
    if(!value)return {{},NumericHudParseStatus::empty};
    return {value,NumericHudParseStatus::valid};
}
inline NumericHudParsedValue parse_numeric_hud_score(const NumericHudRawText& raw) noexcept {
    if(raw.truncated)return {{},NumericHudParseStatus::raw_too_long};
    if(raw.non_ascii)return {{},NumericHudParseStatus::non_ascii};return parse_numeric_hud_score(raw.view());
}
inline NumericHudParsedValue parse_numeric_hud_combo(const NumericHudRawText& raw,std::size_t maximum_combo) noexcept {
    if(raw.truncated)return {{},NumericHudParseStatus::raw_too_long};
    if(raw.non_ascii)return {{},NumericHudParseStatus::non_ascii};return parse_numeric_hud_combo(raw.view(),maximum_combo);
}

struct NumericHudFrame {
    std::uint64_t sequence=0,epoch=0,generation=0,geometry_version=0;
    std::optional<std::uint64_t> source_sequence;
    std::optional<std::int64_t> source_timestamp_us;
    std::int64_t received_ns=-1,processed_ns=-1;
};
struct NumericHudSubmissionStats {
    std::size_t submit_calls=0,submitted=0,invalid_metadata=0,invalid_geometry=0,sequence_rejects=0,
        timestamp_rejects=0,interval_skips=0,busy_drops=0,unavailable_drops=0,capacity_drops=0;
};
enum class NumericHudSubmitOutcome {submitted,invalid_metadata,invalid_geometry,sequence_reject,timestamp_reject,
    interval_skip,busy,unavailable,capacity};

// Producer-only metadata/cadence budget: no pixel storage, clock reads, images,
// OCR or permission semantics. Submitted provenance is never timestamp-renewed.
class NumericHudMetadataBudget {
    std::size_t capacity_;
    NumericHudSubmissionStats stats_;
    std::uint64_t last_sequence_=0;
    std::int64_t last_received_=-1,last_attempt_received_=-1;
public:
    explicit NumericHudMetadataBudget(std::size_t capacity=numeric_hud_max_rows) noexcept
        :capacity_(capacity>numeric_hud_max_rows?numeric_hud_max_rows:capacity){}
    NumericHudSubmitOutcome observe(const NumericHudFrame& f,std::uint32_t width,std::uint32_t height,
            std::uint32_t stride,std::size_t bytes,bool idle,bool available) noexcept {
        ++stats_.submit_calls;
        if(!f.sequence||!f.epoch||!f.generation||!f.geometry_version||f.received_ns<0||f.processed_ns<f.received_ns||
                (f.source_timestamp_us&&*f.source_timestamp_us<0)){
            ++stats_.invalid_metadata;return NumericHudSubmitOutcome::invalid_metadata;}
        if(width!=1280||height!=720||stride<3840||stride>4096||bytes!=static_cast<std::size_t>(stride)*720u){
            ++stats_.invalid_geometry;return NumericHudSubmitOutcome::invalid_geometry;}
        if(f.sequence<=last_sequence_){++stats_.sequence_rejects;return NumericHudSubmitOutcome::sequence_reject;}
        if(f.received_ns<=last_received_){++stats_.timestamp_rejects;return NumericHudSubmitOutcome::timestamp_reject;}
        last_sequence_=f.sequence;last_received_=f.received_ns;
        if(last_attempt_received_>=0&&f.received_ns-last_attempt_received_<numeric_hud_interval_ns){
            ++stats_.interval_skips;return NumericHudSubmitOutcome::interval_skip;}
        last_attempt_received_=f.received_ns;
        if(stats_.submitted>=capacity_){++stats_.capacity_drops;return NumericHudSubmitOutcome::capacity;}
        if(!available){++stats_.unavailable_drops;return NumericHudSubmitOutcome::unavailable;}
        if(!idle){++stats_.busy_drops;return NumericHudSubmitOutcome::busy;}
        ++stats_.submitted;return NumericHudSubmitOutcome::submitted;
    }
    const NumericHudSubmissionStats& stats() const noexcept{return stats_;}
};
enum class NumericHudOcrStatus {not_attempted,no_ink,recognized,timeout,cancelled,error};
inline const char* numeric_hud_ocr_name(NumericHudOcrStatus status) noexcept {
    switch(status){case NumericHudOcrStatus::not_attempted:return "not_attempted";case NumericHudOcrStatus::no_ink:return "no_ink";
    case NumericHudOcrStatus::recognized:return "recognized";case NumericHudOcrStatus::timeout:return "timeout";
    case NumericHudOcrStatus::cancelled:return "cancelled";case NumericHudOcrStatus::error:return "error";}return "unknown";
}
struct NumericHudRoiResult {
    std::optional<std::uint32_t> value;
    NumericHudRawText raw;
    NumericHudParseStatus parse_status=NumericHudParseStatus::empty;
    NumericHudOcrStatus ocr_status=NumericHudOcrStatus::not_attempted;
    std::array<int,4> crop{};
    int scale=2;
    bool inverted=false;
    std::size_t attempts=0;
    std::int64_t ocr_elapsed_us=0;
};
struct NumericHudRow {
    std::size_t sample_index=0;
    NumericHudFrame frame;
    NumericHudRoiResult score,combo;
    ProgressBarMetadata progress_bar;
    std::int64_t processing_elapsed_us=0;
};
struct NumericHudObserverSummary {
    bool available=false,finished=false;
    std::string status,engine_language;
    NumericHudRawText error;
    std::size_t maximum_combo=0,completed=0,score_values=0,combo_values=0,ocr_timeouts=0,ocr_errors=0,cancelled_rows=0;
    NumericHudSubmissionStats submission;
};

template<class Json> inline void to_json(Json& j,const NumericHudFrame& f){
    j={{"sequence",f.sequence},{"epoch",f.epoch},{"generation",f.generation},{"geometry_version",f.geometry_version},
        {"received_ns",f.received_ns},{"processed_ns",f.processed_ns},
        {"source_sequence",f.source_sequence?Json(*f.source_sequence):Json(nullptr)},
        {"source_timestamp_us",f.source_timestamp_us?Json(*f.source_timestamp_us):Json(nullptr)}};
}
template<class Json> inline void to_json(Json& j,const NumericHudSubmissionStats& s){
    j={{"submit_calls",s.submit_calls},{"submitted",s.submitted},{"invalid_metadata",s.invalid_metadata},
        {"invalid_geometry",s.invalid_geometry},{"sequence_rejects",s.sequence_rejects},{"timestamp_rejects",s.timestamp_rejects},
        {"interval_skips",s.interval_skips},{"busy_drops",s.busy_drops},{"unavailable_drops",s.unavailable_drops},
        {"capacity_drops",s.capacity_drops}};
}
template<class Json> inline void to_json(Json& j,const NumericHudRoiResult& r){
    j={{"value",r.value?Json(*r.value):Json(nullptr)},{"raw",std::string(r.raw.view())},
        {"raw_truncated",r.raw.truncated},{"raw_non_ascii",r.raw.non_ascii},{"parse_status",numeric_hud_parse_name(r.parse_status)},
        {"ocr_status",numeric_hud_ocr_name(r.ocr_status)},{"crop_xywh",r.crop},{"scale",r.scale},{"inverted",r.inverted},
        {"attempts",r.attempts},{"ocr_elapsed_us",r.ocr_elapsed_us}};
}
template<class Json> inline void to_json(Json& j,const NumericHudRow& r){
    j={{"sample_index",r.sample_index},{"frame",r.frame},{"score",r.score},{"combo",r.combo},
        {"processing_elapsed_us",r.processing_elapsed_us}};
    Json rows=Json::array();
    for(std::size_t i=0;i<r.progress_bar.row_count;++i){const auto& row=r.progress_bar.rows[i];
        rows.push_back({{"y",row.y},{"bright_prefix_columns",row.bright_prefix_columns},
            {"transition_columns",row.transition_columns},{"bright_runs",row.bright_runs},
            {"first_bright_column",row.first_bright_column},{"last_bright_column",row.last_bright_column},
            {"endpoint_lower_column",row.endpoint_lower_column},{"endpoint_upper_column",row.endpoint_upper_column},
            {"minimum_channel",row.minimum_channel},{"maximum_channel",row.maximum_channel},
            {"definite_dark_after_prefix",row.definite_dark_after_prefix},
            {"status",progress_bar_row_status_name(row.status)}});
    }
    j["progress_bar"]={{"valid",r.progress_bar.valid},{"reason",r.progress_bar.reason},
        {"pixel_reads",r.progress_bar.pixel_reads},{"row_count",r.progress_bar.row_count},
        {"candidate_rows",r.progress_bar.candidate_rows},{"first_candidate_row",r.progress_bar.first_candidate_row},
        {"bar_height",r.progress_bar.bar_height},{"endpoint_lower_column",r.progress_bar.endpoint_lower_column},
        {"endpoint_upper_column",r.progress_bar.endpoint_upper_column},{"rows",std::move(rows)},
        {"diagnostic_only",true},{"pixels_retained",false},{"input_permission",false},
        {"scope","Original-frame top-row pixel classification only; no song-time, phase or clock qualification"}};
}
template<class Json> inline void to_json(Json& j,const NumericHudObserverSummary& s){
    j={{"available",s.available},{"finished",s.finished},{"status",s.status},{"engine_language",s.engine_language},
        {"error",std::string(s.error.view())},{"maximum_combo",s.maximum_combo},{"completed",s.completed},
        {"score_values",s.score_values},{"combo_values",s.combo_values},{"ocr_timeouts",s.ocr_timeouts},
        {"ocr_errors",s.ocr_errors},{"cancelled_rows",s.cancelled_rows},{"submission",s.submission},
        {"capacity",numeric_hud_max_rows},{"minimum_receipt_interval_ns",numeric_hud_interval_ns},
        {"score_copy_bytes",7200},{"combo_copy_bytes",11520},{"pixels_retained",false},{"input_authority",false}};
}
} // namespace rhythm::experimental
