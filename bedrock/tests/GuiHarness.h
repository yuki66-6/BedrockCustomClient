#pragma once

// Rendu ImGui hors écran (texture D3D11, aucune fenêtre) pour tester l'interface
// sans rien afficher sur le bureau.

#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <functional>
#include <memory>

namespace bcc {
class ThemeManager;
}

namespace bcctest {

class GuiHarness {
public:
    GuiHarness(unsigned width = 1600, unsigned height = 900);
    ~GuiHarness();

    bool ok() const { return ok_; }
    bcc::ThemeManager& theme() { return *theme_; }
    ID3D11Device* device() const { return device_.Get(); }
    double time() const { return time_; }

    // Exécute `frames` images de 1/60 s en appelant `draw` entre NewFrame et Render.
    void run(int frames, const std::function<void(double now)>& draw,
             const std::function<void()>& afterRender = {});
    // Nombre de pixels non transparents de la dernière image ; écrit un BMP si
    // la variable d'environnement BCC_TEST_OUTPUT désigne un dossier.
    unsigned capture(const char* name);

private:
    bool ok_ = false;
    unsigned w_, h_;
    double time_ = 1.0;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> target_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    std::unique_ptr<bcc::ThemeManager> theme_;
};

} // namespace bcctest
