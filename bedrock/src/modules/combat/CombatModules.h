#pragma once

#include "modules/Module.h"

namespace bcc {

// Clics par seconde (gauche/droite), mesurés sur les vrais clics souris.
class CpsModule : public Module {
public:
    CpsModule();
    Icon icon() const override { return Icon::Chart; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.012f, 0.30f}; }
    void onUpdate(double now, double dt) override;
    void drawHud(HudContext& ctx) override;

private:
    EnumSetting* mode_;
    BoolSetting* onlyInGame_;
    BoolSetting* showPeak_;
    ColorSetting* color_;
};

// Résumé des paramètres de combat disponibles (CPS, ping, Reach Lab simulé).
class CombatHudModule : public Module {
public:
    CombatHudModule();
    Icon icon() const override { return Icon::Shield; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.012f, 0.40f}; }
    void drawHud(HudContext& ctx) override;
    std::string limitation() const override;
    // Source de la portée simulée (module Reach Lab), affichée comme telle.
    void setSimReachSource(std::function<float()> source) { simReach_ = std::move(source); }

private:
    std::function<float()> simReach_;
    BoolSetting* showCps_;
    BoolSetting* showPing_;
    BoolSetting* showReach_;
    BoolSetting* showUnavailable_;
};

} // namespace bcc
