#pragma once
#include "pas/core.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <stop_token>

namespace pas {
void run_bench_gpu_load(std::stop_token stop, const Clock& clock,
    const std::function<void(nlohmann::json)>& record);
nlohmann::json sample_gpu_engines();
}
