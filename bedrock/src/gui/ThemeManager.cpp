#include "gui/ThemeManager.h"

#include "utils/Log.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>

namespace bcc {
namespace {

struct Preset {
    const char* name;
    Color accent;
};

constexpr Preset kPresets[] = {
    {"Bleu", {0.27f, 0.54f, 1.00f, 1.f}},
    {"Cyan", {0.10f, 0.78f, 0.90f, 1.f}},
    {"Violet", {0.62f, 0.42f, 1.00f, 1.f}},
    {"Vert", {0.25f, 0.82f, 0.48f, 1.f}},
    {"Rouge", {0.95f, 0.30f, 0.34f, 1.f}},
    {"Orange", {1.00f, 0.58f, 0.18f, 1.f}},
};

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

} // namespace

ThemeManager::ThemeManager() {
    std::vector<std::string> names;
    for (auto& p : kPresets) names.emplace_back(p.name);
    names.emplace_back("Personnalisé");

    preset_ = add<EnumSetting>("preset", "Préréglage", "Couleur d'accent prédéfinie", names, 0);
    accent_ = add<ColorSetting>("accent", "Couleur d'accent", "Couleur principale de l'interface", kPresets[0].accent);
    panelAlpha_ = add<FloatSetting>("panel_alpha", "Opacité des panneaux", "Transparence des panneaux du menu", 0.94f,
                                    0.5f, 1.f, "%.2f");
    rounding_ = add<FloatSetting>("rounding", "Arrondi", "Rayon des coins (pixels)", 6.f, 0.f, 12.f, "%.0f px");
    uiScaleSetting_ = add<FloatSetting>("ui_scale", "Taille de l'interface", "Échelle globale du menu et du HUD",
                                        1.f, 0.75f, 2.f, "x%.2f");
    fontSize_ = add<FloatSetting>("font_size", "Taille du texte", "Taille de police de base", 15.f, 12.f, 22.f,
                                  "%.0f px");
    animations_ = add<BoolSetting>("animations", "Animations", "Transitions d'ouverture, survols, interrupteurs", true);
    animSpeed_ = add<FloatSetting>("anim_speed", "Vitesse des animations", "Plus élevé = plus rapide", 12.f, 4.f,
                                   30.f, "%.0f");
    animSpeed_->setVisibleIf([this] { return animations_->get(); });
    hudBgAlpha_ = add<FloatSetting>("hud_bg_alpha", "Fond du HUD", "Opacité du fond des éléments HUD", 0.55f, 0.f,
                                    1.f, "%.2f");
    dimBackground_ = add<BoolSetting>("dim_background", "Assombrir le fond", "Voile sombre derrière le menu ouvert",
                                      false);
    tilt_ = add<FloatSetting>("tilt", "Inclinaison 3D", "Perspective du menu (0 = à plat)", 0.55f, 0.f, 1.f, "%.2f");
    parallax_ = add<BoolSetting>("parallax", "Effet parallaxe", "Le menu pivote légèrement en suivant la souris", true);

    preset_->setOnChange([this] {
        dirty_ = true;
        if (!applyingPreset_ && !loadingSettings()) applyPreset(preset_->get());
    });
    accent_->setOnChange([this] {
        dirty_ = true;
        // Une couleur choisie à la main bascule le préréglage sur « Personnalisé ».
        if (!applyingPreset_ && !loadingSettings()) {
            applyingPreset_ = true;
            int custom = static_cast<int>(std::size(kPresets));
            int idx = preset_->get();
            if (idx >= custom || !(kPresets[idx].accent == accent_->get())) preset_->set(custom);
            applyingPreset_ = false;
        }
    });
    for (auto* s : {static_cast<Setting*>(panelAlpha_), static_cast<Setting*>(rounding_),
                    static_cast<Setting*>(uiScaleSetting_), static_cast<Setting*>(fontSize_)})
        s->setOnChange([this] { dirty_ = true; });
}

void ThemeManager::loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    wchar_t winDir[MAX_PATH];
    GetWindowsDirectoryW(winDir, MAX_PATH);
    auto font = std::filesystem::path(winDir) / L"Fonts" / L"segoeui.ttf";
    std::error_code ec;
    if (std::filesystem::exists(font, ec) && io.Fonts->AddFontFromFileTTF(font.string().c_str(), 16.f)) return;
    log::warn("Police Segoe UI introuvable, police intégrée utilisée");
    io.Fonts->AddFontDefault();
}

void ThemeManager::reconcilePreset() {
    applyingPreset_ = true;
    int custom = static_cast<int>(std::size(kPresets));
    int match = custom;
    for (int i = 0; i < custom; ++i)
        if (kPresets[i].accent == accent_->get()) match = i;
    preset_->set(match);
    applyingPreset_ = false;
    dirty_ = true;
}

void ThemeManager::applyPreset(int index) {
    if (index < 0 || index >= static_cast<int>(std::size(kPresets))) return;
    applyingPreset_ = true;
    accent_->set(kPresets[index].accent);
    applyingPreset_ = false;
}

ImVec4 ThemeManager::accent(float alphaMul) const {
    auto c = accent_->get();
    return {c.r, c.g, c.b, c.a * alphaMul};
}

ImVec4 ThemeManager::panelBg() const { return {0.040f, 0.052f, 0.085f, panelAlpha_->get()}; }
ImVec4 ThemeManager::headerBg() const { return {0.070f, 0.090f, 0.140f, panelAlpha_->get()}; }

void ThemeManager::apply(float dpiScale) {
    if (dpiScale != lastDpi_) dirty_ = true;
    if (!dirty_) return;
    dirty_ = false;
    lastDpi_ = dpiScale;
    scale_ = std::clamp(uiScaleSetting_->get() * dpiScale, 0.5f, 4.f);

    ImGuiStyle style; // repart du style par défaut pour éviter le cumul des échelles
    ImGui::StyleColorsDark(&style);
    float r = rounding_->get();
    style.WindowRounding = r + 2;
    style.ChildRounding = r;
    style.FrameRounding = r;
    style.PopupRounding = r;
    style.GrabRounding = r;
    style.TabRounding = r;
    style.ScrollbarRounding = 8;
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 0;
    style.PopupBorderSize = 1;
    style.WindowPadding = {10, 10};
    style.FramePadding = {8, 5};
    style.ItemSpacing = {8, 6};
    style.ItemInnerSpacing = {6, 4};
    style.ScrollbarSize = 10;
    style.GrabMinSize = 10;
    style.WindowTitleAlign = {0.5f, 0.5f};
    style.SeparatorTextBorderSize = 1;

    ImVec4 a = accent();
    ImVec4 bg = panelBg();
    ImVec4 frame{0.085f, 0.105f, 0.155f, 1.f};
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = {0.93f, 0.94f, 0.96f, 1.f};
    c[ImGuiCol_TextDisabled] = textDim();
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = {0.075f, 0.095f, 0.145f, 0.55f * bg.w};
    c[ImGuiCol_PopupBg] = {0.050f, 0.065f, 0.105f, 0.98f};
    c[ImGuiCol_Border] = {1.f, 1.f, 1.f, 0.07f};
    c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
    c[ImGuiCol_FrameBg] = frame;
    c[ImGuiCol_FrameBgHovered] = mix(frame, a, 0.18f);
    c[ImGuiCol_FrameBgActive] = mix(frame, a, 0.30f);
    c[ImGuiCol_TitleBg] = headerBg();
    c[ImGuiCol_TitleBgActive] = headerBg();
    c[ImGuiCol_TitleBgCollapsed] = headerBg();
    c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    c[ImGuiCol_ScrollbarGrab] = {1, 1, 1, 0.12f};
    c[ImGuiCol_ScrollbarGrabHovered] = {1, 1, 1, 0.20f};
    c[ImGuiCol_ScrollbarGrabActive] = a;
    c[ImGuiCol_CheckMark] = a;
    c[ImGuiCol_SliderGrab] = a;
    c[ImGuiCol_SliderGrabActive] = mix(a, {1, 1, 1, 1}, 0.25f);
    c[ImGuiCol_Button] = frame;
    c[ImGuiCol_ButtonHovered] = mix(frame, a, 0.35f);
    c[ImGuiCol_ButtonActive] = mix(frame, a, 0.60f);
    c[ImGuiCol_Header] = mix(frame, a, 0.25f);
    c[ImGuiCol_HeaderHovered] = mix(frame, a, 0.35f);
    c[ImGuiCol_HeaderActive] = mix(frame, a, 0.50f);
    c[ImGuiCol_Separator] = {1, 1, 1, 0.08f};
    c[ImGuiCol_SeparatorHovered] = a;
    c[ImGuiCol_SeparatorActive] = a;
    c[ImGuiCol_ResizeGrip] = {0, 0, 0, 0};
    c[ImGuiCol_ResizeGripHovered] = accent(0.6f);
    c[ImGuiCol_ResizeGripActive] = a;
    c[ImGuiCol_Tab] = frame;
    c[ImGuiCol_TabHovered] = mix(frame, a, 0.45f);
    c[ImGuiCol_TabSelected] = mix(frame, a, 0.35f);
    c[ImGuiCol_TabSelectedOverline] = a;
    c[ImGuiCol_TabDimmed] = frame;
    c[ImGuiCol_TabDimmedSelected] = mix(frame, a, 0.25f);
    c[ImGuiCol_PlotLines] = a;
    c[ImGuiCol_PlotHistogram] = a;
    c[ImGuiCol_TextSelectedBg] = accent(0.35f);
    c[ImGuiCol_NavCursor] = a;
    c[ImGuiCol_InputTextCursor] = a;
    c[ImGuiCol_ModalWindowDimBg] = {0, 0, 0, 0.45f};

    style.ScaleAllSizes(scale_);
    style.FontSizeBase = fontSize_->get();
    style.FontScaleMain = scale_;
    ImGui::GetStyle() = style;
}

} // namespace bcc
