#include "pas/preview.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <array>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace pas {
namespace {
using Microsoft::WRL::ComPtr;

void check(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(result));
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}

constexpr const char* shader_source = R"(
Texture2D frameTexture : register(t0);
SamplerState nearestSampler : register(s0);
struct VertexOut { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
VertexOut vs_main(uint vertex : SV_VertexID) {
    float2 uv = float2((vertex << 1) & 2, vertex & 2);
    VertexOut output;
    output.position = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
    output.uv = uv;
    return output;
}
float4 ps_main(VertexOut input) : SV_TARGET {
    return frameTexture.Sample(nearestSampler, input.uv);
}
)";
}

struct PreviewWindow::Impl {
    int width, height;
    HWND window = nullptr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> pixel;
    ComPtr<ID3D11SamplerState> sampler;
    std::vector<std::uint8_t> bgra;

    Impl(int w, int h, bool benchmark_placement) : width(w), height(h) {
        if (w < 1 || h < 1 || static_cast<std::uint64_t>(w) * h * 3 > 16 * 1024 * 1024)
            throw std::invalid_argument("invalid preview geometry");
        static std::once_flag registered;
        std::call_once(registered, [] {
            WNDCLASSW type{};
            type.lpfnWndProc = window_proc;
            type.hInstance = GetModuleHandleW(nullptr);
            type.lpszClassName = L"PASNativePreview";
            type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            if (!RegisterClassW(&type)) throw std::runtime_error("preview class registration failed");
        });
        constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT rectangle{0, 0, benchmark_placement ? 400 : w, benchmark_placement ? 225 : h};
        if (!AdjustWindowRect(&rectangle, style, FALSE)) throw std::runtime_error("preview window sizing failed");
        window = CreateWindowExW(benchmark_placement ? WS_EX_NOACTIVATE : 0,
            L"PASNativePreview", L"PAS observe preview", style,
            benchmark_placement ? 1460 : CW_USEDEFAULT, benchmark_placement ? 125 : CW_USEDEFAULT,
            rectangle.right - rectangle.left,
            rectangle.bottom - rectangle.top, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!window) throw std::runtime_error("preview window creation failed");
        try {
            DXGI_SWAP_CHAIN_DESC chain{};
            chain.BufferDesc.Width = static_cast<UINT>(w);
            chain.BufferDesc.Height = static_cast<UINT>(h);
            chain.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            chain.SampleDesc.Count = 1;
            chain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            chain.BufferCount = 2;
            chain.OutputWindow = window;
            chain.Windowed = TRUE;
            chain.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            const std::array levels{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
            D3D_FEATURE_LEVEL selected{};
            check(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels.data(), static_cast<UINT>(levels.size()),
                D3D11_SDK_VERSION, &chain, &swap, &device, &selected, &context), "D3D11 device");
            ComPtr<ID3D11Texture2D> backbuffer;
            check(swap->GetBuffer(0, IID_PPV_ARGS(&backbuffer)), "D3D11 swap buffer");
            check(device->CreateRenderTargetView(backbuffer.Get(), nullptr, &target), "D3D11 render target");
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = static_cast<UINT>(w); desc.Height = static_cast<UINT>(h);
            desc.MipLevels = 1; desc.ArraySize = 1; desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            check(device->CreateTexture2D(&desc, nullptr, &texture), "D3D11 frame texture");
            check(device->CreateShaderResourceView(texture.Get(), nullptr, &view), "D3D11 frame view");
            ComPtr<ID3DBlob> vs, ps, errors;
            check(D3DCompile(shader_source, std::strlen(shader_source), nullptr, nullptr, nullptr,
                "vs_main", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, &errors), "D3D11 vertex shader");
            errors.Reset();
            check(D3DCompile(shader_source, std::strlen(shader_source), nullptr, nullptr, nullptr,
                "ps_main", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, &errors), "D3D11 pixel shader");
            check(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex),
                  "D3D11 vertex shader creation");
            check(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixel),
                  "D3D11 pixel shader creation");
            D3D11_SAMPLER_DESC sampling{};
            sampling.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampling.MaxLOD = D3D11_FLOAT32_MAX;
            check(device->CreateSamplerState(&sampling, &sampler), "D3D11 sampler");
            bgra.resize(static_cast<std::size_t>(w) * h * 4);
            ShowWindow(window, benchmark_placement ? SW_SHOWNOACTIVATE : SW_SHOWNORMAL);
        } catch (...) {
            DestroyWindow(window);
            window = nullptr;
            throw;
        }
    }

    ~Impl() {
        if (window && IsWindow(window)) DestroyWindow(window);
        MSG message{};
        while (PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {}
    }
};

PreviewWindow::PreviewWindow(int width, int height, bool benchmark_placement)
    : impl_(std::make_unique<Impl>(width, height, benchmark_placement)) {}
PreviewWindow::~PreviewWindow() = default;

bool PreviewWindow::pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) return false;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return IsWindow(impl_->window) != FALSE;
}

void PreviewWindow::draw(const Frame& frame) {
    auto& state = *impl_;
    if (!IsWindow(state.window)) return;
    if (frame.width != state.width || frame.height != state.height ||
        frame.stride != state.width * 3 ||
        frame.rgb.size() != static_cast<std::size_t>(state.width) * state.height * 3)
        throw std::invalid_argument("preview frame geometry changed");
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(state.width) * state.height; ++pixel) {
        state.bgra[4*pixel] = frame.rgb[3*pixel+2];
        state.bgra[4*pixel+1] = frame.rgb[3*pixel+1];
        state.bgra[4*pixel+2] = frame.rgb[3*pixel];
        state.bgra[4*pixel+3] = 255;
    }
    state.context->UpdateSubresource(state.texture.Get(), 0, nullptr, state.bgra.data(),
                                    static_cast<UINT>(state.width * 4), 0);
    const float black[4] = {0,0,0,1};
    state.context->ClearRenderTargetView(state.target.Get(), black);
    auto* target = state.target.Get();
    state.context->OMSetRenderTargets(1, &target, nullptr);
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(state.width);
    viewport.Height = static_cast<float>(state.height);
    viewport.MaxDepth = 1;
    state.context->RSSetViewports(1, &viewport);
    state.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    state.context->VSSetShader(state.vertex.Get(), nullptr, 0);
    state.context->PSSetShader(state.pixel.Get(), nullptr, 0);
    auto* view = state.view.Get();
    auto* sampler = state.sampler.Get();
    state.context->PSSetShaderResources(0, 1, &view);
    state.context->PSSetSamplers(0, 1, &sampler);
    state.context->Draw(3, 0);
    const auto result = state.swap->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    if (result != DXGI_ERROR_WAS_STILL_DRAWING) check(result, "D3D11 present");
}

} // namespace pas
