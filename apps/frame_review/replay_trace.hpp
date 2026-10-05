#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>
#include <array>
#include <memory>

// Included only in isolated offline exports. No policy consumes these records.
namespace pas {
inline thread_local bool x1_trace_enabled=false;
inline constexpr std::size_t x1_trace_capacity=16*1024*1024-64;
inline thread_local std::unique_ptr<std::array<char,x1_trace_capacity>> x1_trace_storage;
inline thread_local std::size_t x1_trace_count=0;
inline thread_local std::size_t x1_trace_bytes=0,x1_trace_peak=0;
inline void x1_emit(nlohmann::json record) {
    if(!x1_trace_enabled)return;
    const auto text=record.dump();const auto bytes=text.size()+1;
    if(x1_trace_count>=100000||x1_trace_bytes+bytes>x1_trace_capacity)
        throw std::runtime_error("trace_capacity");
    if(!x1_trace_storage)x1_trace_storage=std::make_unique<std::array<char,x1_trace_capacity>>();
    std::copy(text.begin(),text.end(),x1_trace_storage->begin()+x1_trace_bytes);
    (*x1_trace_storage)[x1_trace_bytes+text.size()]='\n';++x1_trace_count;
    x1_trace_bytes+=bytes;x1_trace_peak=std::max(x1_trace_peak,x1_trace_bytes);
}
inline nlohmann::json x1_drain() {
    nlohmann::json records=nlohmann::json::array();std::size_t begin=0;
    for(std::size_t end=0;end<x1_trace_bytes;++end)if((*x1_trace_storage)[end]=='\n') {
        records.push_back(nlohmann::json::parse(x1_trace_storage->data()+begin,x1_trace_storage->data()+end));begin=end+1;
    }
    x1_trace_count=0;x1_trace_bytes=0;return records;
}
}
