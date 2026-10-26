#include "modules/HudDraw.h"

#include "core/KeybindManager.h"

#include <windows.h>

#include <cmath>
#include <format>

namespace bcc::hud {

void line(const char* label, const std::string& value, const ImVec4& valueColor) {
    ImGui::TextColored({0.72f, 0.75f, 0.82f, 1.f}, "%s", label);
    ImGui::SameLine();
    ImGui::TextColored(valueColor, "%s", value.c_str());
}

void unavailable(const char* label, const char* reason) {
    ImGui::TextColored({0.72f, 0.75f, 0.82f, 1.f}, "%s", label);
    ImGui::SameLine();
    ImGui::TextColored({1.f, 0.72f, 0.30f, 1.f}, "N/D");
    if (reason && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", reason);
}

void centeredText(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, const char* text, float fontSize) {
    float fs = fontSize > 0 ? fontSize : ImGui::GetFontSize();
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.f, text);
    dl->AddText(ImGui::GetFont(), fs, {(min.x + max.x - ts.x) * 0.5f, (min.y + max.y - ts.y) * 0.5f}, color, text);
}

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

float approach(float current, float target, float dt, float speed) {
    return current + (target - current) * (1.f - std::exp(-speed * dt));
}

std::string formatDuration(double seconds) {
    long long s = static_cast<long long>(seconds);
    return std::format("{:02}:{:02}:{:02}", s / 3600, (s / 60) % 60, s % 60);
}

std::string shortKeyName(int vk) {
    switch (vk) {
    case 0: return "-";
    case VK_SPACE: return "Espace";
    case VK_LSHIFT: case VK_RSHIFT: return "Maj";
    case VK_LCONTROL: case VK_RCONTROL: return "Ctrl";
    default: break;
    }
    std::string n = KeybindManager::keyName(vk);
    return n.size() <= 6 ? n : n.substr(0, 6);
}

int layoutKey(unsigned scanCode) {
    UINT vk = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK);
    return vk ? static_cast<int>(vk) : 0;
}

} // namespace bcc::hud
