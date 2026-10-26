#include "GuiHarness.h"

#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "utils/Bitmap.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <cstdlib>
#include <vector>

namespace bcctest {

GuiHarness::GuiHarness(unsigned width, unsigned height) : w_(width), h_(height) {
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   levels, 2, D3D11_SDK_VERSION, &device_, nullptr, &ctx_);
    if (FAILED(hr))
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2,
                               D3D11_SDK_VERSION, &device_, nullptr, &ctx_);
    if (FAILED(hr)) return;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = w_;
    td.Height = h_;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &target_))) return;
    if (FAILED(device_->CreateRenderTargetView(target_.Get(), nullptr, &rtv_))) return;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {static_cast<float>(w_), static_cast<float>(h_)};
    theme_ = std::make_unique<bcc::ThemeManager>();
    bcc::ui::setTheme(theme_.get());
    bcc::ThemeManager::loadFonts();
    theme_->apply(1.f);
    ok_ = ImGui_ImplDX11_Init(device_.Get(), ctx_.Get());
}

GuiHarness::~GuiHarness() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
    }
    bcc::ui::setTheme(nullptr);
}

void GuiHarness::run(int frames, const std::function<void(double)>& draw, const std::function<void()>& afterRender) {
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < frames; ++i) {
        io.DeltaTime = 1.f / 60.f;
        time_ += 1.0 / 60.0;
        theme_->apply(1.f);
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        draw(time_);
        ImGui::Render();
        if (afterRender) afterRender();
        const float clear[4] = {0, 0, 0, 0};
        ctx_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
        ctx_->ClearRenderTargetView(rtv_.Get(), clear);
        D3D11_VIEWPORT vp{0, 0, static_cast<float>(w_), static_cast<float>(h_), 0, 1};
        ctx_->RSSetViewports(1, &vp);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}

unsigned GuiHarness::capture(const char* name) {
    D3D11_TEXTURE2D_DESC td;
    target_->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &staging))) return 0;
    ctx_->CopyResource(staging.Get(), target_.Get());
    D3D11_MAPPED_SUBRESOURCE map;
    if (FAILED(ctx_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map))) return 0;
    const auto* px = static_cast<const uint8_t*>(map.pData);
    unsigned opaque = 0;
    for (unsigned y = 0; y < h_; ++y)
        for (unsigned x = 0; x < w_; ++x)
            if (px[y * map.RowPitch + x * 4 + 3] > 8) ++opaque;
    char* dir = nullptr;
    size_t len = 0;
    if (_dupenv_s(&dir, &len, "BCC_TEST_OUTPUT") == 0 && dir) {
        std::filesystem::path out = std::filesystem::path(dir) / (std::string(name) + ".bmp");
        bcc::writePremultipliedBmp(out, px, w_, h_, map.RowPitch);
        free(dir);
    }
    ctx_->Unmap(staging.Get(), 0);
    return opaque;
}

} // namespace bcctest
