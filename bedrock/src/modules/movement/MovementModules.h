#pragma once

#include "core/EventBus.h"
#include "modules/Module.h"

#include <array>
#include <cstdint>

namespace bcc {

// Touches de déplacement + clics, avec animation d'appui.
class KeystrokesModule : public Module {
public:
    KeystrokesModule();
    Icon icon() const override { return Icon::Keyboard; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.008f, 0.015f}; }
    void drawHud(HudContext& ctx) override;
    void drawCardPreview() override;

    // Touches avant/gauche/arrière/droite selon la disposition (ZQSD en AZERTY).
    static std::array<int, 4> detectMovementKeys();

private:
    void draw(TelemetryProvider* telemetry, double now, bool preview);
    void applyLayout(int layout);

    EnumSetting* style_;
    BoolSetting* showMouse_;
    BoolSetting* showSpace_;
    BoolSetting* showCps_;
    EnumSetting* layout_;
    KeySetting* forward_;
    KeySetting* left_;
    KeySetting* back_;
    KeySetting* right_;
    KeySetting* jump_;
    ColorSetting* pressed_;
    std::array<float, 8> anim_{};
    double lastNow_ = 0;
};

// Indicateur de sprint déduit du clavier (touche sprint + avancer).
class SprintModule : public Module {
public:
    SprintModule();
    Icon icon() const override { return Icon::Run; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.44f, 0.86f}; }
    void drawHud(HudContext& ctx) override;
    std::string limitation() const override;

protected:
    void onInit(ModuleContext& ctx) override;
    void onShutdown() override { sub_.reset(); }

private:
    EnumSetting* mode_;
    KeySetting* sprintKey_;
    KeySetting* forwardKey_;
    StringSetting* text_;
    ColorSetting* color_;
    bool toggled_ = false;
    Subscription sub_;
};

// Compteur d'appuis sur la touche Saut pendant que le jeu a le focus.
class JumpCounterModule : public Module {
public:
    JumpCounterModule();
    Icon icon() const override { return Icon::Jump; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.012f, 0.56f}; }
    void drawHud(HudContext& ctx) override;
    std::string limitation() const override;
    uint64_t count() const { return count_; }

protected:
    void onInit(ModuleContext& ctx) override;
    void onShutdown() override { sub_.reset(); }

private:
    Subscription sub_;
    KeySetting* jumpKey_;
    BoolSetting* showSession_;
    uint64_t count_ = 0;
};

} // namespace bcc
