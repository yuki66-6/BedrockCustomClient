#include "gui/Widgets.h"

#include "core/KeybindManager.h"
#include "gui/ThemeManager.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace bcc::ui {
namespace {
ThemeManager* g_theme = nullptr;
KeybindManager* g_keybinds = nullptr;

float scale() { return g_theme ? g_theme->uiScale() : 1.f; }
ImVec4 accentColor() { return g_theme ? g_theme->accent() : ImVec4{0.27f, 0.54f, 1.f, 1.f}; }
} // namespace

void setTheme(ThemeManager* theme) { g_theme = theme; }
ThemeManager& theme() { return *g_theme; }
void setKeybindManager(KeybindManager* keybinds) { g_keybinds = keybinds; }

float animate(ImGuiID id, float target, float speedMul) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* value = storage->GetFloatRef(id, target);
    if (!g_theme || !g_theme->animationsEnabled()) {
        *value = target;
        return target;
    }
    float dt = ImGui::GetIO().DeltaTime;
    float k = 1.f - std::exp(-g_theme->animationSpeed() * speedMul * dt);
    *value += (target - *value) * k;
    if (std::fabs(target - *value) < 0.001f) *value = target;
    return *value;
}

ImU32 colorU32(const ImVec4& c, float alphaMul) {
    return ImGui::ColorConvertFloat4ToU32({c.x, c.y, c.z, c.w * alphaMul * ImGui::GetStyle().Alpha});
}

void sameLineRight(float width) {
    ImGui::SameLine();
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
}

bool toggleSwitch(const char* strId, bool value, float height) {
    float h = height > 0 ? height : ImGui::GetFrameHeight() * 0.78f;
    float w = h * 1.8f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float yOff = (ImGui::GetFrameHeight() - h) * 0.5f;
    ImGui::SetCursorScreenPos({p.x, p.y + yOff});
    bool clicked = ImGui::InvisibleButton(strId, {w, h});
    ImGuiID id = ImGui::GetItemID();
    bool hovered = ImGui::IsItemHovered();
    float t = animate(id, value ? 1.f : 0.f);
    float hov = animate(id + 1, hovered ? 1.f : 0.f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a{p.x, p.y + yOff}, b{p.x + w, p.y + yOff + h};
    ImVec4 off{0.20f, 0.22f, 0.27f, 1.f};
    ImVec4 on = accentColor();
    ImVec4 track{off.x + (on.x - off.x) * t + 0.04f * hov, off.y + (on.y - off.y) * t + 0.04f * hov,
                 off.z + (on.z - off.z) * t + 0.04f * hov, 1.f};
    if (t > 0.01f) dl->AddRectFilled({a.x - 2, a.y - 2}, {b.x + 2, b.y + 2}, colorU32(on, 0.18f * t), h); // halo
    dl->AddRectFilled(a, b, colorU32(track), h * 0.5f);
    float r = h * 0.5f - 2.5f;
    ImVec2 knob{a.x + h * 0.5f + (w - h) * t, a.y + h * 0.5f};
    dl->AddCircleFilled({knob.x, knob.y + 1.f}, r, colorU32({0, 0, 0, 0.25f}));
    dl->AddCircleFilled(knob, r, colorU32({1, 1, 1, 0.97f}));
    ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + yOff});
    return clicked;
}

namespace {

// Piste + curseur d'un slider de largeur `w`, à la position courante.
bool sliderTrack(const char* strId, float w, float* value, float min, float max, Setting* resettable) {
    float h = ImGui::GetFrameHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(strId, {std::max(10.f, w), h});
    ImGuiID id = ImGui::GetItemID();
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();
    float knobR = h * 0.22f;
    float x0 = p.x + knobR, x1 = p.x + w - knobR;
    bool changed = false;
    if (active && max > min) {
        float t = std::clamp((ImGui::GetIO().MousePos.x - x0) / std::max(1.f, x1 - x0), 0.f, 1.f);
        float v = min + t * (max - min);
        if (v != *value) {
            *value = v;
            changed = true;
        }
    }
    if (resettable && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) resettable->reset();
    float t = max > min ? std::clamp((*value - min) / (max - min), 0.f, 1.f) : 0.f;
    float shownT = active ? t : animate(id, t, 2.f);
    if (active) ImGui::GetStateStorage()->SetFloat(id, t);
    float hov = animate(id + 1, (hovered || active) ? 1.f : 0.f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cy = p.y + h * 0.5f, track = 1.5f + 0.8f * hov;
    ImVec4 a = accentColor();
    dl->AddRectFilled({x0, cy - track}, {x1, cy + track}, colorU32({0.20f, 0.22f, 0.27f, 1.f}), track);
    float kx = x0 + (x1 - x0) * shownT;
    dl->AddRectFilled({x0, cy - track}, {kx, cy + track}, colorU32(a), track);
    dl->AddCircleFilled({kx, cy}, knobR * (1.f + 0.9f * hov), colorU32(a, 0.22f));
    dl->AddCircleFilled({kx, cy}, knobR, colorU32(a));
    dl->AddCircleFilled({kx, cy}, knobR * 0.45f, colorU32({1, 1, 1, 0.95f}));
    return changed;
}

} // namespace

bool slider(const char* strId, float* value, float min, float max) {
    return sliderTrack(strId, ImGui::GetContentRegionAvail().x, value, min, max, nullptr);
}

void tooltip(const char* text) {
    if (text && *text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", text);
}

void sectionTitle(const char* text) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, accentColor());
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

void dimText(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", buf);
    ImGui::PopStyleColor();
}

void unavailableNote(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.72f, 0.30f, 1.f});
    ImGui::TextWrapped("N/D : %s", text);
    ImGui::PopStyleColor();
}

namespace {

// Ligne de réglage : libellé dans la colonne de gauche, contrôle à droite.
// Retourne la largeur disponible pour le contrôle (curseur déjà positionné).
float beginRow(const Setting& s, const char* label = nullptr) {
    float total = ImGui::GetContentRegionAvail().x;
    float labelW = std::floor(total * 0.36f);
    ImGui::AlignTextToFramePadding();
    float x0 = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(label ? label : s.name().c_str());
    tooltip(s.description().c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(x0 + labelW);
    return total - labelW;
}

// Case affichant la valeur numérique à droite d'un slider.
void valueBox(const char* text, float width) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight();
    float r = ImGui::GetStyle().FrameRounding;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + width, p.y + h}, colorU32({0.07f, 0.09f, 0.13f, 0.9f}), r);
    dl->AddRect(p, {p.x + width, p.y + h}, colorU32({1, 1, 1, 0.08f}), r);
    ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddText({p.x + (width - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f},
                colorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), text);
    ImGui::Dummy({width, h});
}

bool numericRow(Setting& s, float* v, float min, float max, const char* text) {
    float avail = beginRow(s);
    float boxW = std::max(ImGui::CalcTextSize(text).x + 14.f * scale(), 48.f * scale());
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    bool changed = sliderTrack("##slider", avail - boxW - spacing, v, min, max, &s);
    tooltip(s.description().c_str());
    ImGui::SameLine(0, spacing);
    valueBox(text, boxW);
    return changed;
}

} // namespace

bool keybindButton(KeySetting& key, const char* label) {
    ImGui::PushID(&key);
    float avail = beginRow(key, label);
    bool capturing = g_keybinds && g_keybinds->captureTarget() == &key;
    std::string text = capturing ? "Appuie sur une touche…" : KeybindManager::keyName(key.get());
    if (!capturing && key.get() == 0) text = "AUCUNE";
    float clearW = ImGui::GetFrameHeight();
    float btnW = std::min(avail - clearW - ImGui::GetStyle().ItemSpacing.x,
                          std::max(ImGui::CalcTextSize(text.c_str()).x + 24.f * scale(), 70.f * scale()));
    if (capturing) {
        float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.f);
        ImGui::PushStyleColor(ImGuiCol_Button, g_theme ? g_theme->accent(0.30f + 0.4f * pulse) : ImVec4{0.3f, 0.5f, 1, 1});
    }
    bool changed = false;
    if (ImGui::Button(text.c_str(), {btnW, 0}) && g_keybinds) {
        if (capturing) g_keybinds->cancelCapture();
        else g_keybinds->beginCapture(&key);
    }
    if (capturing) ImGui::PopStyleColor();
    tooltip("Clic : choisir une touche · Échap : annuler · Retour arrière : effacer");
    sameLineRight(clearW);
    if (ImGui::Button("x", {clearW, 0})) {
        if (capturing && g_keybinds) g_keybinds->cancelCapture();
        key.set(0);
        changed = true;
    }
    tooltip("Effacer le raccourci");
    ImGui::PopID();
    return changed;
}

bool settingWidget(Setting& setting) {
    ImGui::PushID(setting.id().c_str());
    bool changed = false;
    switch (setting.type()) {
    case SettingType::Bool: {
        auto& s = static_cast<BoolSetting&>(setting);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(s.name().c_str());
        tooltip(s.description().c_str());
        sameLineRight(ImGui::GetFrameHeight() * 0.78f * 1.8f);
        if (toggleSwitch("##sw", s.get())) {
            s.set(!s.get());
            changed = true;
        }
        break;
    }
    case SettingType::Int: {
        auto& s = static_cast<IntSetting&>(setting);
        char text[64];
        snprintf(text, sizeof(text), s.format().c_str(), s.get());
        float v = static_cast<float>(s.get());
        if (numericRow(s, &v, static_cast<float>(s.min()), static_cast<float>(s.max()), text)) {
            s.set(static_cast<int>(std::lround(v)));
            changed = true;
        }
        break;
    }
    case SettingType::Float: {
        auto& s = static_cast<FloatSetting&>(setting);
        char text[64];
        snprintf(text, sizeof(text), s.format().c_str(), s.get());
        float v = s.get();
        if (numericRow(s, &v, s.min(), s.max(), text)) {
            s.set(v);
            changed = true;
        }
        break;
    }
    case SettingType::Enum: {
        auto& s = static_cast<EnumSetting&>(setting);
        float avail = beginRow(s);
        ImGui::SetNextItemWidth(avail);
        if (ImGui::BeginCombo("##v", s.current().c_str())) {
            for (int i = 0; i < static_cast<int>(s.options().size()); ++i) {
                bool selected = i == s.get();
                if (ImGui::Selectable(s.options()[static_cast<size_t>(i)].c_str(), selected)) {
                    s.set(i);
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        break;
    }
    case SettingType::Color: {
        auto& s = static_cast<ColorSetting&>(setting);
        float avail = beginRow(s);
        ImVec4 col{s.get().r, s.get().g, s.get().b, s.get().a};
        if (ImGui::ColorButton("##swatch", col, ImGuiColorEditFlags_AlphaPreviewHalf,
                               {std::min(avail, 44.f * scale()), ImGui::GetFrameHeight()}))
            ImGui::OpenPopup("##picker");
        if (ImGui::BeginPopup("##picker")) {
            float c[4] = {col.x, col.y, col.z, col.w};
            if (ImGui::ColorPicker4("##pick", c,
                                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview |
                                        ImGuiColorEditFlags_AlphaPreviewHalf)) {
                s.set({c[0], c[1], c[2], c[3]});
                changed = true;
            }
            ImGui::EndPopup();
        }
        break;
    }
    case SettingType::Key: changed = keybindButton(static_cast<KeySetting&>(setting)); break;
    case SettingType::String: {
        auto& s = static_cast<StringSetting&>(setting);
        float avail = beginRow(s);
        std::string buf = s.get();
        buf.resize(s.maxLength() + 1, '\0');
        ImGui::SetNextItemWidth(avail);
        if (ImGui::InputText("##v", buf.data(), buf.size())) {
            s.set(std::string(buf.c_str()));
            changed = true;
        }
        break;
    }
    case SettingType::Action: {
        auto& s = static_cast<ActionSetting&>(setting);
        if (ImGui::Button(s.name().c_str(), {ImGui::GetContentRegionAvail().x, 0})) {
            s.trigger();
            changed = true;
        }
        tooltip(s.description().c_str());
        break;
    }
    }
    ImGui::PopID();
    return changed;
}

} // namespace bcc::ui
