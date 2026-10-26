#pragma once

#include <windows.h>

#include <string>

namespace bcc {

// Ce que l'on sait de la fenêtre du jeu (via les API publiques de fenêtrage
// Windows uniquement : aucune lecture du processus Minecraft).
struct GameState {
    bool found = false;
    bool foreground = false;
    bool minimized = false;
    HWND hwnd = nullptr;
    DWORD pid = 0;
    RECT clientRect{}; // zone cliente en coordonnées écran
    std::string detection; // « exe », « titre » ou vide
    int width() const { return clientRect.right - clientRect.left; }
    int height() const { return clientRect.bottom - clientRect.top; }
};

// Localise la fenêtre « Minecraft for Windows » (processus Minecraft.Windows.exe).
class GameIntegration {
public:
    explicit GameIntegration(HWND overlay) : overlay_(overlay) {}

    // Met à jour l'état ; les recherches complètes sont espacées (coût faible).
    // Retourne true si la zone cliente ou la présence du jeu a changé.
    bool poll(double now);
    const GameState& state() const { return state_; }
    // Appel rapide (à chaque événement d'entrée).
    bool isGameForeground() const;

    static bool isMinecraftExe(const std::wstring& path);

private:
    HWND find();
    bool validate(HWND hwnd) const;

    HWND overlay_;
    GameState state_;
    double lastSearch_ = -100;
};

} // namespace bcc
