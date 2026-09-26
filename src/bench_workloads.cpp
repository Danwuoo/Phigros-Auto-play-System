#include "pas/bench_workloads.hpp"
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <wrl/client.h>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

namespace pas {
namespace {
void check(HRESULT status, const char* operation) {
    if (FAILED(status)) throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(status));
}
}
void run_bench_gpu_load(std::stop_token stop, const Clock& clock,
    const std::function<void(nlohmann::json)>& record) {
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context), "GPU load device");
    if (level < D3D_FEATURE_LEVEL_11_0) throw std::runtime_error("GPU load needs D3D11 compute");
    constexpr const char* source = R"(
RWTexture2D<float4> output : register(u0);
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID) {
    float4 v = float4(id.xy * 0.0001, 0.25, 0.75);
    [loop] for (uint i=0; i<128; ++i) v = frac(sin(v * 1.0001 + float(i) * 0.0001) * 100.01);
    output[id.xy] = v;
})";
    ComPtr<ID3DBlob> binary, errors;
    check(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "main", "cs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, &binary, &errors), "GPU load shader");
    ComPtr<ID3D11ComputeShader> shader;
    check(device->CreateComputeShader(binary->GetBufferPointer(), binary->GetBufferSize(), nullptr, &shader), "GPU load compute");
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = 512; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11UnorderedAccessView> view;
    check(device->CreateTexture2D(&desc, nullptr, &texture), "GPU load texture");
    check(device->CreateUnorderedAccessView(texture.Get(), nullptr, &view), "GPU load UAV");
    D3D11_QUERY_DESC query_desc{D3D11_QUERY_EVENT, 0};
    ComPtr<ID3D11Query> query;
    check(device->CreateQuery(&query_desc, &query), "GPU load completion query");
    context->CSSetShader(shader.Get(), nullptr, 0);
    auto* uav = view.Get(); context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    ComPtr<IDXGIDevice> dxgi; check(device.As(&dxgi), "GPU load DXGI");
    ComPtr<IDXGIAdapter> adapter; check(dxgi->GetAdapter(&adapter), "GPU load adapter");
    DXGI_ADAPTER_DESC adapter_desc{}; check(adapter->GetDesc(&adapter_desc), "GPU load adapter desc");
    record({{"event", "gpu_load_start"}, {"monotonic_ns", clock.now_ns()},
        {"adapter_luid_high", adapter_desc.AdapterLuid.HighPart},
        {"adapter_luid_low", adapter_desc.AdapterLuid.LowPart}, {"texture", {512,512}},
        {"iterations_per_pixel", 128}, {"max_inflight_dispatches", 1},
        {"completion_timeout_ms", 2000}, {"shader", source}});
    std::uint64_t dispatches = 0;
    while (!stop.stop_requested()) {
        context->Dispatch(64,64,1); context->End(query.Get()); context->Flush();
        const auto deadline = clock.now_ns() + 2'000'000'000;
        while (true) {
            const auto status = context->GetData(query.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
            check(status, "GPU load completion");
            if (status == S_OK) break;
            if (clock.now_ns() >= deadline) throw std::runtime_error("GPU load dispatch exceeded deadline");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ++dispatches;
    }
    record({{"event", "gpu_load_end"}, {"monotonic_ns", clock.now_ns()}, {"dispatches", dispatches}});
}

nlohmann::json sample_gpu_engines() {
    struct Query {
        PDH_HQUERY query = nullptr;
        PDH_HCOUNTER counter = nullptr;
        PDH_STATUS status;
        Query() {
            status = PdhOpenQueryW(nullptr, 0, &query);
            if (status == ERROR_SUCCESS)
                status = PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter);
            if (status == ERROR_SUCCESS) status = PdhCollectQueryData(query);
        }
        ~Query() { if (query) PdhCloseQuery(query); }
    };
    static thread_local Query state;
    if (state.status != ERROR_SUCCESS) return {{"available", false}, {"pdh_status", state.status}};
    auto status = PdhCollectQueryData(state.query);
    DWORD bytes = 0, count = 0;
    if (status == ERROR_SUCCESS)
        status = PdhGetFormattedCounterArrayW(state.counter, PDH_FMT_DOUBLE, &bytes, &count, nullptr);
    if (status != PDH_MORE_DATA || bytes > 4 * 1024 * 1024)
        return {{"available", false}, {"pdh_status", status}};
    std::vector<std::uint8_t> storage(bytes);
    auto* values = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(storage.data());
    status = PdhGetFormattedCounterArrayW(state.counter, PDH_FMT_DOUBLE, &bytes, &count, values);
    if (status != ERROR_SUCCESS || count > 4096) return {{"available", false}, {"pdh_status", status}};
    auto engines = nlohmann::json::array();
    for (DWORD i = 0; i < count; ++i) {
        const std::wstring name(values[i].szName);
        const auto& value = values[i].FmtValue;
        if (value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA) continue;
        // Instance names contain ASCII ids and engine types, never UI titles.
        engines.push_back({{"instance", std::string(name.begin(), name.end())}, {"percent", value.doubleValue}});
    }
    return {{"available", true}, {"scope", "all host engines; each engine independently normalized"}, {"engines", engines}};
}
}
