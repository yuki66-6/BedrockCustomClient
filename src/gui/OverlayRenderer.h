#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

namespace bcc {

// Rendu D3D11 vers une swapchain DirectComposition à alpha prémultiplié :
// l'overlay est réellement transparent (pas de clé couleur), et le compositeur
// de Windows (DWM) l'assemble au-dessus de la fenêtre du jeu. Aucune
// interaction avec le processus Minecraft.
class OverlayRenderer {
public:
    ~OverlayRenderer() { shutdown(); }

    bool init(HWND hwnd, UINT width, UINT height);
    void shutdown();

    void resize(UINT width, UINT height);
    // Efface le back buffer en transparent. false si le périphérique est perdu.
    bool beginFrame();
    // Présente l'image. Retourne false si le périphérique a été perdu.
    bool present(bool vsync);

    // Copie le back buffer courant (avant present) dans un BMP composité sur
    // un fond gris : utilisé par l'autotest pour vérifier le rendu.
    bool captureBackBuffer(const std::filesystem::path& file, unsigned* outOpaquePixels = nullptr);
    // Couleur d'un pixel du back buffer (prémultiplié, BGRA) — autotest.
    bool readPixel(UINT x, UINT y, unsigned char outBgra[4]);

    HANDLE frameWaitable() const { return frameWaitable_; }
    ID3D11Device* device() const { return device_.Get(); }
    ID3D11DeviceContext* context() const { return context_.Get(); }
    UINT width() const { return width_; }
    UINT height() const { return height_; }
    const std::string& adapterName() const { return adapterName_; }
    const std::string& lastError() const { return lastError_; }

private:
    bool createRenderTarget();
    bool fail(const char* what, HRESULT hr);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain2> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> dcompTarget_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> dcompVisual_;
    HANDLE frameWaitable_ = nullptr;
    UINT width_ = 0, height_ = 0;
    std::string adapterName_;
    std::string lastError_;
};

} // namespace bcc
