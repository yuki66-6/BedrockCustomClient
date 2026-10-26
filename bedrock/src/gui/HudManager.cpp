#include "gui/HudManager.h"

#include "core/ModuleManager.h"
#include "gui/ThemeManager.h"
#include "gui/Widgets.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace bcc {

HudManager::HudManager(ModuleManager& modules) : modules_(modules) {
    scale_ = add<FloatSetting>("scale", "Taille du HUD", "Échelle globale des éléments HUD", 1.f, 0.5f, 2.5f, "x%.2f");
    background_ = add<BoolSetting>("background", "Fond des éléments", "Affiche un fond translucide sous le HUD", true);
    snap_ = add<BoolSetting>("snap", "Aimanter aux bords", "Colle les éléments aux bords de l'écran pendant le déplacement",
                             true);
}

void HudManager::setEditMode(bool edit) {
    editMode_ = edit;
    if (!edit) dragging_.clear();
}

void HudManager::resetLayout() {
    layouts_.clear();
    Setting::bumpRevision();
}

void HudManager::drawOverlays(TelemetryProvider* telemetry, ImVec2 areaMin, ImVec2 areaSize, double now, float dt) {
    OverlayContext ctx;
    ctx.drawList = ImGui::GetBackgroundDrawList();
    ctx.areaMin = areaMin;
    ctx.areaMax = {areaMin.x + areaSize.x, areaMin.y + areaSize.y};
    ctx.telemetry = telemetry;
    ctx.now = now;
    ctx.dt = dt;
    for (auto& m : modules_.all())
        if (m->enabled()) m->drawOverlay(ctx);
}

void HudManager::draw(TelemetryProvider* telemetry, ImVec2 areaMin, ImVec2 areaSize, double now, float) {
    ThemeManager& theme = ui::theme();
    ImGuiIO& io = ImGui::GetIO();
    if (!io.MouseDown[0]) dragging_.clear();

    for (auto& mod : modules_.all()) {
        Module& m = *mod;
        if (!m.enabled() || !m.hasHud()) continue;
        Layout& L = layouts_[m.id()];
        ImVec2 frac = L.x < 0 ? m.defaultHudPosition() : ImVec2{L.x, L.y};
        ImVec2 pos{areaMin.x + frac.x * areaSize.x, areaMin.y + frac.y * areaSize.y};
        float s = L.scale * scale_->get() * theme.uiScale();

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse;
        if (!editMode_) flags |= ImGuiWindowFlags_NoInputs;
        float alpha = background_->get() ? theme.hudBackgroundAlpha() : 0.f;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.f * s, 6.f * s});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme.rounding() * s);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, editMode_ ? 1.f : 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {4, 4});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.035f, 0.045f, 0.075f, alpha});
        ImGui::PushStyleColor(ImGuiCol_Border, theme.accent(0.8f));
        std::string name = "##hud_" + m.id();
        if (ImGui::Begin(name.c_str(), nullptr, flags)) {
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * L.scale * scale_->get());
            HudContext ctx{telemetry, now, s, editMode_};
            m.drawHud(ctx);
            ImGui::PopFont();

            ImVec2 size = ImGui::GetWindowSize();
            if (editMode_) {
                bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) dragging_ = m.id();
                if (dragging_ == m.id() && io.MouseDown[0]) {
                    pos.x += io.MouseDelta.x;
                    pos.y += io.MouseDelta.y;
                    if (snap_->get()) {
                        float snapPx = 12.f * theme.uiScale();
                        if (pos.x - areaMin.x < snapPx) pos.x = areaMin.x;
                        if (areaMin.x + areaSize.x - (pos.x + size.x) < snapPx) pos.x = areaMin.x + areaSize.x - size.x;
                        if (pos.y - areaMin.y < snapPx) pos.y = areaMin.y;
                        if (areaMin.y + areaSize.y - (pos.y + size.y) < snapPx) pos.y = areaMin.y + areaSize.y - size.y;
                    }
                    Setting::bumpRevision();
                }
                if (hovered && io.MouseWheel != 0.f) {
                    L.scale = std::clamp(L.scale + io.MouseWheel * 0.1f, 0.5f, 3.f);
                    Setting::bumpRevision();
                }
                // Étiquette au-dessus de l'élément en mode édition.
                std::string label = std::format("{}  x{:.1f}", m.name(), L.scale);
                ImGui::GetForegroundDrawList()->AddText({pos.x, pos.y - ImGui::GetFontSize() - 2.f},
                                                        theme.accentU32(hovered ? 1.f : 0.7f), label.c_str());
            }
            // Garde l'élément dans la zone de jeu et mémorise sa position.
            pos.x = std::clamp(pos.x, areaMin.x, std::max(areaMin.x, areaMin.x + areaSize.x - size.x));
            pos.y = std::clamp(pos.y, areaMin.y, std::max(areaMin.y, areaMin.y + areaSize.y - size.y));
            if (L.x >= 0 || dragging_ == m.id()) {
                L.x = (pos.x - areaMin.x) / std::max(1.f, areaSize.x);
                L.y = (pos.y - areaMin.y) / std::max(1.f, areaSize.y);
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
    }
}

Json HudManager::toJson() const {
    Json layout = Json::object();
    for (auto& [id, L] : layouts_) {
        if (L.x < 0 && L.scale == 1.f) continue;
        layout[id] = {{"x", L.x}, {"y", L.y}, {"scale", L.scale}};
    }
    return {{"settings", settingsToJson()}, {"layout", layout}};
}

std::vector<std::string> HudManager::fromJson(const Json& j) {
    std::vector<std::string> warnings;
    if (!j.is_object()) return {"section hud invalide"};
    if (auto it = j.find("settings"); it != j.end()) warnings = settingsFromJson(*it, "hud");
    layouts_.clear();
    if (auto it = j.find("layout"); it != j.end() && it->is_object()) {
        for (auto& [id, v] : it->items()) {
            if (!v.is_object()) continue;
            Layout L;
            L.x = std::clamp(v.value("x", -1.f), -1.f, 1.f);
            L.y = std::clamp(v.value("y", -1.f), -1.f, 1.f);
            L.scale = std::clamp(v.value("scale", 1.f), 0.5f, 3.f);
            if (!std::isfinite(L.x) || !std::isfinite(L.y)) continue;
            layouts_[id] = L;
        }
    }
    return warnings;
}

void HudManager::resetAll() {
    resetSettings();
    layouts_.clear();
}

} // namespace bcc
