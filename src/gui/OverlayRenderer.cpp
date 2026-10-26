#include "gui/OverlayRenderer.h"

#include "utils/Bitmap.h"
#include "utils/Log.h"
#include "utils/Strings.h"

#include <algorithm>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace bcc {

bool OverlayRenderer::fail(const char* what, HRESULT hr) {
    lastError_ = std::format("{} : {}", what, win32ErrorMessage(static_cast<unsigned long>(hr)));
    log::error("Rendu : {}", lastError_);
    return false;
}

bool OverlayRenderer::init(HWND hwnd, UINT width, UINT height) {
    width_ = std::max(1u, width);
    height_ = std::max(1u, height);

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // requis par DirectComposition
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                        D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels,
                                   static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &device_, &got, &context_);
    if (FAILED(hr)) {
        log::warn("Périphérique D3D11 matériel indisponible ({}), repli sur WARP", win32ErrorMessage(hr));
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels,
                               static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &device_, &got, &context_);
        if (FAILED(hr)) return fail("D3D11CreateDevice", hr);
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(hr = device_.As(&dxgiDevice))) return fail("IDXGIDevice", hr);
    ComPtr<IDXGIAdapter> adapter;
    if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
        DXGI_ADAPTER_DESC desc{};
        adapter->GetDesc(&desc);
        adapterName_ = toUtf8(desc.Description);
    }
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return fail("CreateDXGIFactory2", hr);

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = width_;
    sd.Height = height_;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(hr = factory->CreateSwapChainForComposition(device_.Get(), &sd, nullptr, &sc1)))
        return fail("CreateSwapChainForComposition", hr);
    if (FAILED(hr = sc1.As(&swapChain_))) return fail("IDXGISwapChain2", hr);
    swapChain_->SetMaximumFrameLatency(1);
    frameWaitable_ = swapChain_->GetFrameLatencyWaitableObject();

    if (FAILED(hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&dcompDevice_))))
        return fail("DCompositionCreateDevice", hr);
    if (FAILED(hr = dcompDevice_->CreateTargetForHwnd(hwnd, TRUE, &dcompTarget_)))
        return fail("CreateTargetForHwnd", hr);
    if (FAILED(hr = dcompDevice_->CreateVisual(&dcompVisual_))) return fail("CreateVisual", hr);
    dcompVisual_->SetContent(swapChain_.Get());
    dcompTarget_->SetRoot(dcompVisual_.Get());
    if (FAILED(hr = dcompDevice_->Commit())) return fail("IDCompositionDevice::Commit", hr);

    if (!createRenderTarget()) return false;
    log::info("Rendu initialisé : {} ({}x{}, niveau 0x{:X})", adapterName_, width_, height_, static_cast<int>(got));
    return true;
}

bool OverlayRenderer::createRenderTarget() {
    ComPtr<ID3D11Texture2D> back;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&back));
    if (FAILED(hr)) return fail("GetBuffer", hr);
    hr = device_->CreateRenderTargetView(back.Get(), nullptr, &rtv_);
    if (FAILED(hr)) return fail("CreateRenderTargetView", hr);
    return true;
}

void OverlayRenderer::shutdown() {
    if (context_) context_->ClearState();
    rtv_.Reset();
    if (frameWaitable_) {
        CloseHandle(frameWaitable_);
        frameWaitable_ = nullptr;
    }
    dcompVisual_.Reset();
    dcompTarget_.Reset();
    dcompDevice_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
}

void OverlayRenderer::resize(UINT width, UINT height) {
    width = std::max(1u, width);
    height = std::max(1u, height);
    if (!swapChain_ || (width == width_ && height == height_)) return;
    rtv_.Reset();
    context_->ClearState();
    context_->Flush();
    HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN,
                                           DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
    if (FAILED(hr)) {
        fail("ResizeBuffers", hr);
        return;
    }
    width_ = width;
    height_ = height;
    createRenderTarget();
}

bool OverlayRenderer::beginFrame() {
    if (!rtv_) return false;
    const float clear[4] = {0, 0, 0, 0};
    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(rtv_.Get(), clear);
    D3D11_VIEWPORT vp{0, 0, static_cast<float>(width_), static_cast<float>(height_), 0, 1};
    context_->RSSetViewports(1, &vp);
    return true;
}

bool OverlayRenderer::present(bool vsync) {
    HRESULT hr = swapChain_->Present(vsync ? 1 : 0, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        fail("Present (périphérique perdu)", device_->GetDeviceRemovedReason());
        return false;
    }
    return true;
}

namespace {

struct Readback {
    std::vector<unsigned char> pixels; // BGRA prémultiplié
    UINT w = 0, h = 0;
};

bool readBack(ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain2* sc, Readback& out) {
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    D3D11_TEXTURE2D_DESC desc;
    back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) return false;
    ctx->CopyResource(staging.Get(), back.Get());
    D3D11_MAPPED_SUBRESOURCE map;
    if (FAILED(ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map))) return false;
    out.w = desc.Width;
    out.h = desc.Height;
    out.pixels.resize(static_cast<size_t>(out.w) * out.h * 4);
    for (UINT y = 0; y < out.h; ++y)
        memcpy(&out.pixels[static_cast<size_t>(y) * out.w * 4], static_cast<unsigned char*>(map.pData) + y * map.RowPitch,
               static_cast<size_t>(out.w) * 4);
    ctx->Unmap(staging.Get(), 0);
    return true;
}

} // namespace

bool OverlayRenderer::readPixel(UINT x, UINT y, unsigned char outBgra[4]) {
    Readback rb;
    if (!readBack(device_.Get(), context_.Get(), swapChain_.Get(), rb) || x >= rb.w || y >= rb.h) return false;
    memcpy(outBgra, &rb.pixels[(static_cast<size_t>(y) * rb.w + x) * 4], 4);
    return true;
}

bool OverlayRenderer::captureBackBuffer(const std::filesystem::path& file, unsigned* outOpaquePixels) {
    Readback rb;
    if (!readBack(device_.Get(), context_.Get(), swapChain_.Get(), rb)) return false;

    return writePremultipliedBmp(file, rb.pixels.data(), rb.w, rb.h, rb.w * 4, outOpaquePixels);
}

} // namespace bcc
