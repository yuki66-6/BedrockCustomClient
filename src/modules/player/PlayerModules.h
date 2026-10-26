#pragma once

#include "modules/Module.h"

namespace bcc {

// Statistiques de session (temps, temps en jeu, clics, pic de CPS, touches).
class SessionStatsModule : public Module {
public:
    SessionStatsModule();
    Icon icon() const override { return Icon::Clock; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.83f, 0.30f}; }
    void onUpdate(double now, double dt) override;
    void drawHud(HudContext& ctx) override;
    void drawCardPreview() override;

private:
    BoolSetting* showClicks_;
    BoolSetting* showKeys_;
    double inGameSeconds_ = 0;
    double resetTime_ = -1;
    TelemetryProvider* telemetry_ = nullptr;
};

// Informations disponibles sur le joueur / la partie, et liste explicite de
// celles qui ne le sont pas.
class PlayerInfoModule : public Module {
public:
    PlayerInfoModule();
    Icon icon() const override { return Icon::Person; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.83f, 0.46f}; }
    void drawHud(HudContext& ctx) override;
    void drawCardPreview() override;
    std::string limitation() const override;

private:
    BoolSetting* showWindow_;
    BoolSetting* showUnavailable_;
};

} // namespace bcc
