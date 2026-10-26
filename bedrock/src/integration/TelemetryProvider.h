#pragma once

#include "integration/GameIntegration.h"
#include "integration/InputState.h"
#include "integration/ServerPinger.h"

namespace bcc {

class TextureManager;

// Point d'accès unique des modules aux données. Chaque donnée est soit réelle
// (entrées, fenêtre du jeu, ping public, FPS de l'overlay), soit explicitement
// indisponible — jamais simulée.
class TelemetryProvider {
public:
    InputState& input() { return input_; }
    const GameState& game() const { return game_; }
    void setGameState(const GameState& g) { game_ = g; }
    bool gameFocused() const { return game_.found && game_.foreground; }

    double now() const { return now_; }
    void setNow(double now) { now_ = now; }
    double sessionStart() const { return sessionStart_; }
    void setSessionStart(double t) { sessionStart_ = t; }

    float overlayFps() const { return overlayFps_; }
    void setOverlayFps(float fps) { overlayFps_ = fps; }

    ServerPinger* pinger() const { return pinger_; }
    void setPinger(ServerPinger* p) { pinger_ = p; }
    PingResult ping() const { return pinger_ ? pinger_->result() : PingResult{}; }

    TextureManager* textures() const { return textures_; }
    void setTextures(TextureManager* t) { textures_ = t; }

    // Raisons affichées pour les données inaccessibles depuis un overlay externe.
    static constexpr const char* kGameFpsUnavailable =
        "Le FPS réel de Minecraft n'est pas exposé aux autres applications. Le mesurer exigerait de s'injecter "
        "dans le jeu ou un traçage ETW (PresentMon) avec droits administrateur.";
    static constexpr const char* kPlayerDataUnavailable =
        "Position, vitesse, santé, armure et équipement ne sont accessibles qu'en lisant la mémoire du jeu : "
        "exclu par conception (non intrusif).";
    static constexpr const char* kEntityDistanceUnavailable =
        "La distance réelle aux autres joueurs nécessiterait les positions des entités, non exposées par le jeu.";

private:
    InputState input_;
    GameState game_;
    double now_ = 0;
    double sessionStart_ = 0;
    float overlayFps_ = 0;
    ServerPinger* pinger_ = nullptr;
    TextureManager* textures_ = nullptr;
};

} // namespace bcc
