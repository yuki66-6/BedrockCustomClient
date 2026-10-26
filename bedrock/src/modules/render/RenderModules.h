#pragma once

#include "modules/Module.h"

#include <string>
#include <vector>

namespace bcc {

// Réticule personnalisé dessiné au centre de la fenêtre du jeu. Styles
// vectoriels ou image fournie par l'utilisateur (dossier « crosshairs »).
class CrosshairModule : public Module {
public:
    CrosshairModule();
    Icon icon() const override { return Icon::Crosshair; }
    int primarySettingCount() const override { return 4; }
    void drawOverlay(OverlayContext& ctx) override;
    void drawCardPreview() override;
    void drawExtraSettings() override;
    std::string limitation() const override;

    static constexpr const char* kFolder = "crosshairs";

private:
    void drawAt(ImDrawList* dl, ImVec2 center, float scale, float spread, TelemetryProvider* telemetry);
    void refreshImages();

    EnumSetting* style_;
    FloatSetting* size_;
    FloatSetting* gap_;
    ColorSetting* color_;
    FloatSetting* thickness_;
    BoolSetting* outline_;
    ColorSetting* outlineColor_;
    BoolSetting* dynamic_;
    StringSetting* image_;
    FloatSetting* imageScale_;
    std::vector<std::string> images_;
    bool imagesScanned_ = false;
    float spread_ = 0.f;
};

// FPS de l'overlay (mesuré) — le FPS de Minecraft n'est pas accessible.
class FpsModule : public Module {
public:
    FpsModule();
    Icon icon() const override { return Icon::Monitor; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.012f, 0.50f}; }
    void drawHud(HudContext& ctx) override;
    std::string limitation() const override;

private:
    BoolSetting* showLabel_;
    ColorSetting* color_;
};

// Latence vers un serveur Bedrock via la requête publique de liste de serveurs.
class PingModule : public Module {
public:
    PingModule();
    Icon icon() const override { return Icon::Signal; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.012f, 0.68f}; }
    void onUpdate(double now, double dt) override;
    void drawHud(HudContext& ctx) override;
    void drawExtraSettings() override;
    std::string limitation() const override;

protected:
    void onDisable() override;

private:
    StringSetting* host_;
    IntSetting* port_;
    FloatSetting* interval_;
    BoolSetting* showMotd_;
    BoolSetting* showPlayers_;
};

// Barre compacte « CPS | ms | FPS » (coin supérieur droit).
class InfoBarModule : public Module {
public:
    InfoBarModule();
    Icon icon() const override { return Icon::Chart; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.80f, 0.015f}; }
    void drawHud(HudContext& ctx) override;

private:
    BoolSetting* showCps_;
    BoolSetting* showPing_;
    BoolSetting* showFps_;
    BoolSetting* showClock_;
};

} // namespace bcc
