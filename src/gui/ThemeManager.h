#pragma once

#include "core/Setting.h"

#include <imgui.h>

namespace bcc {

inline ImVec4 toImVec4(const Color& c) { return {c.r, c.g, c.b, c.a}; }
inline Color toColor(const ImVec4& v) { return {v.x, v.y, v.z, v.w}; }

// Thème sombre de la ClickGUI : couleurs, arrondis, échelle, animations.
class ThemeManager : public SettingsGroup {
public:
    ThemeManager();

    // Charge Segoe UI (police système) ou, à défaut, la police intégrée d'ImGui.
    static void loadFonts();

    // Reconstruit le style ImGui si un réglage (ou le DPI) a changé.
    void apply(float dpiScale);
    void forceRebuild() { dirty_ = true; }

    ImVec4 accent(float alphaMul = 1.f) const;
    ImU32 accentU32(float alphaMul = 1.f) const { return ImGui::ColorConvertFloat4ToU32(accent(alphaMul)); }
    ImVec4 panelBg() const;
    ImVec4 headerBg() const;
    ImVec4 textDim() const { return {0.62f, 0.64f, 0.70f, 1.f}; }
    float uiScale() const { return scale_; } // échelle effective (utilisateur x DPI)
    bool animationsEnabled() const { return animations_->get(); }
    float animationSpeed() const { return animSpeed_->get(); }
    float rounding() const { return rounding_->get(); }
    float hudBackgroundAlpha() const { return hudBgAlpha_->get(); }
    bool dimBackground() const { return dimBackground_->get(); }
    float tilt() const { return tilt_->get(); }
    bool parallax() const { return parallax_->get(); }

private:
    void applyPreset(int index);

    EnumSetting* preset_;
    ColorSetting* accent_;
    FloatSetting* panelAlpha_;
    FloatSetting* rounding_;
    FloatSetting* uiScaleSetting_;
    FloatSetting* fontSize_;
    BoolSetting* animations_;
    FloatSetting* animSpeed_;
    FloatSetting* hudBgAlpha_;
    BoolSetting* dimBackground_;
    FloatSetting* tilt_;
    BoolSetting* parallax_;

    bool dirty_ = true;
    float lastDpi_ = 0;
    float scale_ = 1;
    bool applyingPreset_ = false;
};

} // namespace bcc
