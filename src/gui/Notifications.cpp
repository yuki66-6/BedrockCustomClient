#include "gui/Notifications.h"

#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "utils/Log.h"

#include <imgui.h>

#include <algorithm>

namespace bcc {

void Notifications::push(Kind kind, std::string text, double durationSec) {
    switch (kind) {
    case Kind::Error: log::error("[notification] {}", text); break;
    case Kind::Warning: log::warn("[notification] {}", text); break;
    default: log::info("[notification] {}", text); break;
    }
    items_.push_back({kind, std::move(text), durationSec});
    while (items_.size() > 5) items_.pop_front();
}

void Notifications::draw(double now) {
    if (items_.empty()) return;
    std::erase_if(items_, [now](const Item& it) { return it.start >= 0 && now - it.start > it.duration; });

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();
    float pad = 10.f * ui::theme().uiScale();
    float width = 320.f * ui::theme().uiScale();
    float y = vp->Pos.y + vp->Size.y - pad;

    for (auto it = items_.rbegin(); it != items_.rend(); ++it) {
        if (it->start < 0) it->start = now;
        double age = now - it->start;
        float fadeIn = static_cast<float>(std::clamp(age / 0.18, 0.0, 1.0));
        float fadeOut = static_cast<float>(std::clamp((it->duration - age) / 0.35, 0.0, 1.0));
        float alpha = std::min(fadeIn, fadeOut);

        ImVec4 bar;
        switch (it->kind) {
        case Kind::Success: bar = {0.25f, 0.82f, 0.48f, 1}; break;
        case Kind::Warning: bar = {1.f, 0.70f, 0.25f, 1}; break;
        case Kind::Error: bar = {0.95f, 0.32f, 0.34f, 1}; break;
        default: bar = ui::theme().accent(); break;
        }

        ImVec2 textSize = ImGui::CalcTextSize(it->text.c_str(), nullptr, false, width - pad * 3);
        float h = textSize.y + pad * 1.6f;
        float x = vp->Pos.x + vp->Size.x - width - pad + (1.f - fadeIn) * 40.f;
        ImVec2 a{x, y - h}, b{x + width, y};
        dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32({0.08f, 0.085f, 0.10f, 0.95f * alpha}),
                          style.FrameRounding);
        dl->AddRectFilled(a, {a.x + 4.f, b.y}, ImGui::ColorConvertFloat4ToU32({bar.x, bar.y, bar.z, alpha}),
                          style.FrameRounding, ImDrawFlags_RoundCornersLeft);
        dl->AddText(nullptr, 0.f, {a.x + pad * 1.5f, a.y + pad * 0.8f},
                    ImGui::ColorConvertFloat4ToU32({0.93f, 0.94f, 0.96f, alpha}), it->text.c_str(), nullptr,
                    width - pad * 3);
        y -= h + pad * 0.6f;
    }
}

} // namespace bcc
