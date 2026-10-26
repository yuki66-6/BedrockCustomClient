#include "gui/TextureManager.h"

#include "utils/Log.h"
#include "utils/Paths.h"
#include "utils/Strings.h"

#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cwctype>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

namespace bcc {

TextureManager::~TextureManager() { clear(); }

void TextureManager::setDevice(ID3D11Device* device) {
    if (device != device_) clear();
    device_ = device;
}

std::filesystem::path TextureManager::folder(const std::string& name) { return paths::dataDir() / toWide(name); }

std::vector<std::string> TextureManager::listImages(const std::filesystem::path& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::wstring ext = e.path().extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".gif")
            out.push_back(toUtf8(e.path().filename().wstring()));
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool TextureManager::decode(const std::filesystem::path& file, std::vector<unsigned char>& rgba, int& w, int& h,
                            std::string* error) {
    auto fail = [&](const char* what, HRESULT hr) {
        if (error) *error = std::format("{} : {}", what, win32ErrorMessage(static_cast<unsigned long>(hr)));
        return false;
    };
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct CoGuard {
        HRESULT hr;
        ~CoGuard() { if (SUCCEEDED(hr)) CoUninitialize(); }
    } guard{co};

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return fail("WIC indisponible", hr);
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return fail("image illisible", hr);
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(hr = decoder->GetFrame(0, &frame))) return fail("image vide", hr);
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(hr = factory->CreateFormatConverter(&conv))) return fail("conversion", hr);
    hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                          WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return fail("conversion RGBA", hr);
    UINT uw = 0, uh = 0;
    conv->GetSize(&uw, &uh);
    if (!uw || !uh || uw > 4096 || uh > 4096) {
        if (error) *error = "dimensions non prises en charge (1 à 4096 px)";
        return false;
    }
    rgba.resize(static_cast<size_t>(uw) * uh * 4);
    if (FAILED(hr = conv->CopyPixels(nullptr, uw * 4, static_cast<UINT>(rgba.size()), rgba.data())))
        return fail("lecture des pixels", hr);
    w = static_cast<int>(uw);
    h = static_cast<int>(uh);
    return true;
}

Texture TextureManager::get(const std::filesystem::path& file) {
    auto key = file.wstring();
    if (auto it = cache_.find(key); it != cache_.end()) return it->second;
    if (auto it = failures_.find(key); it != failures_.end()) {
        lastError_ = it->second;
        return {};
    }
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;
    std::string err;
    if (!decode(file, rgba, w, h, &err)) {
        lastError_ = toUtf8(file.filename().wstring()) + " : " + err;
        failures_[key] = lastError_;
        log::warn("Image : {}", lastError_);
        return {};
    }
    if (!device_) {
        lastError_ = "pas de périphérique graphique";
        return {};
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{rgba.data(), static_cast<UINT>(w * 4), 0};
    ComPtr<ID3D11Texture2D> tex;
    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(device_->CreateTexture2D(&td, &init, &tex)) ||
        FAILED(device_->CreateShaderResourceView(tex.Get(), nullptr, &srv))) {
        lastError_ = "création de texture impossible";
        failures_[key] = lastError_;
        return {};
    }
    Texture t{static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(srv)), w, h};
    cache_[key] = t;
    lastError_.clear();
    log::info("Image chargée : {} ({}x{})", toUtf8(file.filename().wstring()), w, h);
    return t;
}

Texture TextureManager::createFromPixels(const std::string& key, const uint32_t* rgba, int width, int height) {
    release(key);
    if (!device_ || !rgba || width <= 0 || height <= 0) return {};
    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{rgba, static_cast<UINT>(width * 4), 0};
    ComPtr<ID3D11Texture2D> tex;
    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(device_->CreateTexture2D(&td, &init, &tex)) || FAILED(device_->CreateShaderResourceView(tex.Get(), nullptr, &srv)))
        return {};
    Texture t{static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(srv)), width, height};
    dynamic_[key] = t;
    return t;
}

void TextureManager::release(const std::string& key) {
    auto it = dynamic_.find(key);
    if (it == dynamic_.end()) return;
    if (it->second.id) reinterpret_cast<ID3D11ShaderResourceView*>(static_cast<uintptr_t>(it->second.id))->Release();
    dynamic_.erase(it);
}

void TextureManager::releasePrefix(const std::string& prefix) {
    for (auto it = dynamic_.begin(); it != dynamic_.end();) {
        if (it->first.rfind(prefix, 0) == 0) {
            if (it->second.id) reinterpret_cast<ID3D11ShaderResourceView*>(static_cast<uintptr_t>(it->second.id))->Release();
            it = dynamic_.erase(it);
        } else {
            ++it;
        }
    }
}

void TextureManager::clear() {
    for (auto& [k, t] : cache_)
        if (t.id) reinterpret_cast<ID3D11ShaderResourceView*>(static_cast<uintptr_t>(t.id))->Release();
    cache_.clear();
    failures_.clear();
    releasePrefix("");
}

} // namespace bcc
