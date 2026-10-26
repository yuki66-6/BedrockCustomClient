#pragma once

#include <imgui.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace bcc {

struct Texture {
    ImTextureID id = 0; // ID3D11ShaderResourceView* pour le backend DX11 d'ImGui
    int width = 0, height = 0;
    explicit operator bool() const { return id != 0; }
};

// Images fournies par l'utilisateur (réticules, et futurs éléments visuels).
// Formats : PNG, JPG, BMP, GIF (1re image), décodés par WIC (intégré à Windows).
class TextureManager {
public:
    ~TextureManager();

    void setDevice(ID3D11Device* device); // nullptr : mode sans GPU (tests)
    // Dossier racine des ressources utilisateur (ex. <données>/crosshairs).
    static std::filesystem::path folder(const std::string& name);
    // Fichiers image d'un dossier (noms seuls, triés).
    static std::vector<std::string> listImages(const std::filesystem::path& dir);

    // Charge (ou renvoie depuis le cache) l'image. Texture vide en cas d'erreur.
    Texture get(const std::filesystem::path& file);
    const std::string& lastError() const { return lastError_; }
    void clear(); // libère tout (rechargement depuis le disque au prochain get)

    // Texture créée depuis des pixels RGBA en mémoire (carte du monde...).
    Texture createFromPixels(const std::string& key, const uint32_t* rgba, int width, int height);
    void release(const std::string& key);
    void releasePrefix(const std::string& prefix);

    // Décodage seul (sans GPU) : utilisé par les tests.
    static bool decode(const std::filesystem::path& file, std::vector<unsigned char>& rgba, int& w, int& h,
                       std::string* error = nullptr);

private:
    ID3D11Device* device_ = nullptr;
    std::map<std::wstring, Texture> cache_;
    std::map<std::wstring, std::string> failures_;
    std::map<std::string, Texture> dynamic_;
    std::string lastError_;
};

} // namespace bcc
