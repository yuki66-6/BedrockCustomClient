#include "gui/ClickGUI.h"

#include "core/ClientSettings.h"
#include "core/ConfigManager.h"
#include "core/KeybindManager.h"
#include "core/ModuleDefinitions.h"
#include "integration/GameAdapter.h"
#include "utils/Paths.h"
#include "core/ModuleManager.h"
#include "gui/HudManager.h"
#include "gui/MapPage.h"
#include "gui/Notifications.h"
#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "utils/FileDialogs.h"
#include "utils/PerfStats.h"
#include "utils/Strings.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <format>

namespace bcc {
namespace {

struct PageInfo {
    Icon icon;
    const char* label;
    const char* title;
    const char* subtitle;
};

constexpr PageInfo kPages[] = {
    {Icon::Sword, "Combat", "Modules Combat", "Infos de combat en direct : CPS, portée simulée, HUD."},
    {Icon::Run, "Movement", "Modules Movement", "Visualise tes touches, ton sprint et tes sauts."},
    {Icon::Monitor, "Render", "Modules Render", "Réticule, FPS, ping et apparence du HUD."},
    {Icon::Person, "Player", "Modules Player", "Statistiques de session et informations disponibles."},
    {Icon::Map, "Carte", "Carte du monde", "Tes mondes vus du dessus, dévoilés au fil de l'exploration."},
    {Icon::Gear, "Settings", "Réglages", "Thème, raccourcis, HUD et options générales."},
    {Icon::Folder, "Configs", "Profils", "Crée, charge, exporte et partage tes réglages."},
};

float easeOutCubic(float t) { return 1.f - std::pow(1.f - t, 3.f); }

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

ClickGUI::ClickGUI(GuiServices services) : s_(std::move(services)) {}

void ClickGUI::setOpen(bool open) {
    open_ = open;
    if (!open && s_.hud) s_.hud->setEditMode(false);
}

Json ClickGUI::toJson() const {
    Json j = {{"page", static_cast<int>(page_)}};
    if (posFrac_.x >= 0) j["window"] = {{"x", posFrac_.x}, {"y", posFrac_.y}};
    return j;
}

void ClickGUI::fromJson(const Json& j) {
    if (!j.is_object()) return;
    int page = j.value("page", 0);
    if (page >= 0 && page < PageCount) page_ = static_cast<Page>(page);
    posFrac_ = {-1, -1};
    if (auto it = j.find("window"); it != j.end() && it->is_object()) {
        float x = it->value("x", -1.f), y = it->value("y", -1.f);
        if (std::isfinite(x) && std::isfinite(y) && x >= 0 && y >= 0) posFrac_ = {std::min(x, 0.95f), std::min(y, 0.95f)};
    }
}

void ClickGUI::resetLayout() {
    posFrac_ = {-1, -1};
    Setting::bumpRevision();
}

bool ClickGUI::matchesSearch(const Module& module) const {
    if (filter_ == 1 && !module.enabled()) return false;
    if (!search_[0]) return true;
    std::string q = lower(search_);
    return lower(module.name()).find(q) != std::string::npos || lower(module.id()).find(q) != std::string::npos ||
           lower(module.description()).find(q) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Dessin principal
// ---------------------------------------------------------------------------

void ClickGUI::draw(double now) {
    float dt = lastTime_ > 0 ? static_cast<float>(now - lastTime_) : 0.f;
    dt = std::clamp(dt, 0.f, 0.1f);
    lastTime_ = now;
    ThemeManager& theme = ui::theme();
    bool anim = theme.animationsEnabled();
    float speed = anim ? theme.animationSpeed() * 0.45f : 1000.f;
    progress_ = std::clamp(progress_ + (open_ ? 1.f : -1.f) * dt * speed, 0.f, 1.f);
    perspActive_ = false;
    mainWindow_ = nullptr;
    if (!open_ && progress_ <= 0.f) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiIO& io = ImGui::GetIO();
    float e = easeOutCubic(progress_);
    if (theme.dimBackground())
        ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y},
                                                      IM_COL32(0, 0, 0, static_cast<int>(100 * e)));

    if (s_.hud && s_.hud->editMode()) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
        drawHudEditBar();
        ImGui::PopStyleVar();
        return;
    }

    float sc = theme.uiScale();
    ImVec2 size{std::clamp(vp->Size.x * 0.60f, std::min(760.f * sc, vp->Size.x - 20.f), std::min(1160.f * sc, vp->Size.x - 20.f)),
                std::clamp(vp->Size.y * 0.76f, std::min(520.f * sc, vp->Size.y - 20.f), std::min(780.f * sc, vp->Size.y - 20.f))};
    bool mapFull = page_ == PageMap && s_.map && s_.map->fullscreen();
    if (mapFull) size = {vp->Size.x * 0.94f, vp->Size.y * 0.92f};
    ImVec2 frac = mapFull ? ImVec2{0.03f, 0.04f}
                          : (posFrac_.x >= 0 ? posFrac_ : ImVec2{0.045f, (1.f - size.y / vp->Size.y) * 0.5f});
    ImVec2 pos{vp->Pos.x + frac.x * vp->Size.x, vp->Pos.y + frac.y * vp->Size.y};
    pos.x = std::clamp(pos.x, vp->Pos.x - size.x * 0.6f, vp->Pos.x + vp->Size.x - size.x * 0.4f);
    pos.y = std::clamp(pos.y, vp->Pos.y, vp->Pos.y + vp->Size.y - 60.f * sc);

    // Perspective dynamique : inclinaison de base + parallaxe souris + pivot d'ouverture.
    float tilt = theme.tilt();
    float targetYaw = 0.16f * tilt, targetPitch = -0.03f * tilt;
    if (theme.parallax() && io.MousePos.x > -1e6f) {
        float mx = std::clamp((io.MousePos.x - vp->Pos.x) / vp->Size.x * 2.f - 1.f, -1.f, 1.f);
        float my = std::clamp((io.MousePos.y - vp->Pos.y) / vp->Size.y * 2.f - 1.f, -1.f, 1.f);
        targetYaw += mx * 0.06f;
        targetPitch -= my * 0.04f;
    }
    float k = anim ? 1.f - std::exp(-5.f * dt) : 1.f;
    yaw_ += (targetYaw - yaw_) * k;
    pitch_ += (targetPitch - pitch_) * k;
    persp_.center = {pos.x + size.x * 0.5f, pos.y + size.y * 0.5f};
    persp_.yaw = yaw_ + (anim ? (1.f - e) * 0.42f : 0.f);
    persp_.pitch = pitch_;
    persp_.scale = anim ? 0.93f + 0.07f * e : 1.f;
    persp_.focal = 1.7f * std::max(vp->Size.x, vp->Size.y);
    winMin_ = pos;
    winMax_ = {pos.x + size.x, pos.y + size.y};

    if (page_ != shownPage_) {
        shownPage_ = page_;
        pageChangeTime_ = now;
    }
    float pageT = anim ? static_cast<float>(std::clamp((now - pageChangeTime_) / 0.22, 0.0, 1.0)) : 1.f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("##bcc_main", nullptr, flags)) {
        mainWindow_ = ImGui::GetCurrentWindow();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        drawBackground(dl, pos, winMax_);
        float headerH = 74.f * sc, sidebarW = 196.f * sc;
        drawHeader(pos, {size.x, headerH});
        drawSidebar({pos.x, pos.y + headerH}, {sidebarW, size.y - headerH});

        // Contenu de la page, avec fondu/glissement au changement de page.
        float pad = 16.f * sc;
        ImGui::SetCursorScreenPos({pos.x + sidebarW + pad, pos.y + headerH + pad * 0.75f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2.f * sc, 2.f * sc});
        ImGui::BeginChild("##content", {size.x - sidebarW - pad * 2.f, size.y - headerH - pad * 1.5f},
                          ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleVar();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (0.2f + 0.8f * pageT));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.f - easeOutCubic(pageT)) * 12.f * sc);
        if (page_ <= PagePlayer) drawModulePage(page_);
        else if (page_ == PageMap) drawMapPage();
        else if (page_ == PageSettings) drawSettingsPage();
        else drawConfigsPage();
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    perspActive_ = mainWindow_ && !persp_.identity();
}

void ClickGUI::drawBackground(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    float r = theme.rounding() * 1.6f + 4.f;
    float alpha = ImGui::GetStyle().Alpha;
    dl->PushClipRectFullScreen();
    // Ombre portée douce + liseré lumineux d'accent.
    for (int i = 1; i <= 8; ++i) {
        float g = static_cast<float>(i) * 2.5f * sc;
        dl->AddRectFilled({p0.x - g, p0.y - g + 6.f * sc}, {p1.x + g, p1.y + g + 6.f * sc},
                          IM_COL32(0, 0, 0, static_cast<int>(16.f * (1.f - static_cast<float>(i) / 9.f) * alpha)), r + g);
    }
    for (int i = 1; i <= 3; ++i) {
        float g = static_cast<float>(i) * 1.5f;
        dl->AddRect({p0.x - g, p0.y - g}, {p1.x + g, p1.y + g}, theme.accentU32(0.10f / static_cast<float>(i) * alpha), r + g,
                    1.5f);
    }
    dl->PopClipRect();

    // Corps « verre » : fond translucide bleu nuit + reflet en haut.
    dl->AddRectFilled(p0, p1, ui::colorU32(theme.panelBg()), r);
    int sheen = static_cast<int>(10 * alpha);
    dl->AddRectFilledMultiColor({p0.x + r, p0.y + 1.f}, {p1.x - r, p0.y + 120.f * sc}, IM_COL32(255, 255, 255, sheen),
                                IM_COL32(255, 255, 255, sheen), IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    // Lueur d'accent diffuse dans le coin supérieur gauche.
    ImVec4 a = theme.accent();
    for (int i = 0; i < 6; ++i)
        dl->AddCircleFilled({p0.x + 60.f * sc, p0.y + 38.f * sc}, (90.f - static_cast<float>(i) * 14.f) * sc,
                            ui::colorU32(a, 0.018f), 48);
    dl->AddRect(p0, p1, ui::colorU32({1, 1, 1, 0.09f}), r, 1.f);
}

void ClickGUI::drawHeader(ImVec2 p0, ImVec2 size) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    float base = ImGui::GetFontSize();
    float alpha = ImGui::GetStyle().Alpha;
    double t = ImGui::GetTime();

    // Zone de déplacement (à gauche des contrôles).
    float controlsW = 330.f * sc;
    ImGui::SetCursorScreenPos(p0);
    ImGui::InvisibleButton("##drag", {std::max(10.f, size.x - controlsW), size.y});
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f)) {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 d = ImGui::GetIO().MouseDelta;
        posFrac_ = {(winMin_.x - vp->Pos.x + d.x) / vp->Size.x, (winMin_.y - vp->Pos.y + d.y) / vp->Size.y};
        Setting::bumpRevision();
    }
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    // Logo (flocon) avec halo pulsé.
    ImVec2 logo{p0.x + 40.f * sc, p0.y + size.y * 0.5f};
    float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(t * 2.0));
    dl->AddCircleFilled(logo, (22.f + 3.f * pulse) * sc, theme.accentU32(0.10f * alpha));
    ui::drawIcon(dl, Icon::Logo, logo, 34.f * sc, theme.accentU32(alpha));

    // Titre + sous-titre.
    float titleSize = base * 1.55f;
    ImVec2 tp{p0.x + 72.f * sc, p0.y + size.y * 0.5f - titleSize * 0.72f};
    dl->AddText(font, titleSize, tp, ui::colorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), "Bedrock");
    float w1 = font->CalcTextSizeA(titleSize, FLT_MAX, 0.f, "Bedrock ").x;
    dl->AddText(font, titleSize, {tp.x + w1, tp.y}, theme.accentU32(alpha), "Client");
    dl->AddText(font, base * 0.74f, {tp.x + 1.f, tp.y + titleSize * 1.05f}, ui::colorU32(theme.textDim(), 0.9f),
                "OVERLAY PVP  ·  SANS INJECTION");

    // Pastille d'état du jeu (donnée réelle : fenêtre Minecraft détectée ou non).
    bool detected = s_.gameDetected && s_.gameDetected();
    const char* status = detected ? "Minecraft détecté" : "Minecraft non détecté";
    float ss = base * 0.82f;
    ImVec2 st = font->CalcTextSizeA(ss, FLT_MAX, 0.f, status);
    ImVec2 pillMin{p0.x + size.x - controlsW + 10.f * sc, p0.y + size.y * 0.5f - st.y * 0.5f - 5.f * sc};
    ImVec2 pillMax{pillMin.x + st.x + 30.f * sc, pillMin.y + st.y + 10.f * sc};
    ImVec4 statusCol = detected ? ImVec4{0.30f, 0.86f, 0.52f, 1.f} : ImVec4{0.62f, 0.64f, 0.70f, 1.f};
    dl->AddRectFilled(pillMin, pillMax, ui::colorU32(statusCol, 0.12f), 99.f);
    dl->AddRect(pillMin, pillMax, ui::colorU32(statusCol, 0.30f), 99.f);
    ImVec2 dot{pillMin.x + 12.f * sc, (pillMin.y + pillMax.y) * 0.5f};
    if (detected) dl->AddCircleFilled(dot, (5.f + 2.f * pulse) * sc, ui::colorU32(statusCol, 0.25f));
    dl->AddCircleFilled(dot, 3.5f * sc, ui::colorU32(statusCol));
    dl->AddText(font, ss, {pillMin.x + 22.f * sc, pillMin.y + 5.f * sc}, ui::colorU32(statusCol), status);

    // Version.
    std::string ver = "v" BCC_VERSION;
    float vs = base * 0.82f;
    ImVec2 vsz = font->CalcTextSizeA(vs, FLT_MAX, 0.f, ver.c_str());
    float vx = p0.x + size.x - 60.f * sc - vsz.x;
    dl->AddText(font, vs, {vx, p0.y + size.y * 0.5f - vsz.y}, ui::colorU32(theme.textDim()), ver.c_str());
    dl->AddText(font, vs * 0.92f, {vx - 18.f * sc, p0.y + size.y * 0.5f + 1.f}, theme.accentU32(0.75f * alpha), "Overlay");

    // Bouton fermer.
    float cb = 26.f * sc;
    ImVec2 cp{p0.x + size.x - 16.f * sc - cb, p0.y + size.y * 0.5f - cb * 0.5f};
    ImGui::SetCursorScreenPos(cp);
    if (ImGui::InvisibleButton("##close", {cb, cb}) && s_.requestClose) s_.requestClose();
    float hov = ui::animate(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.f : 0.f);
    if (hov > 0.01f) dl->AddRectFilled(cp, {cp.x + cb, cp.y + cb}, ui::colorU32({0.95f, 0.3f, 0.35f, 0.25f * hov}), 6.f * sc);
    float x = 7.f * sc;
    ImU32 xc = ui::colorU32({0.85f + 0.15f * hov, 0.87f, 0.92f, 0.8f + 0.2f * hov});
    dl->AddLine({cp.x + x, cp.y + x}, {cp.x + cb - x, cp.y + cb - x}, xc, 1.6f * sc);
    dl->AddLine({cp.x + cb - x, cp.y + x}, {cp.x + x, cp.y + cb - x}, xc, 1.6f * sc);
    ui::tooltip("Fermer le menu (Échap ou touche du menu)");

    // Ligne d'accent sous l'en-tête, avec un reflet qui défile.
    float ly = p0.y + size.y - 1.f;
    ImVec4 a = theme.accent();
    dl->AddRectFilledMultiColor({p0.x + 196.f * sc, ly}, {p0.x + size.x - 18.f * sc, ly + 1.5f}, ui::colorU32(a, 0.55f),
                                ui::colorU32(a, 0.0f), ui::colorU32(a, 0.0f), ui::colorU32(a, 0.55f));
    if (theme.animationsEnabled()) {
        float span = size.x - 214.f * sc;
        float sx = p0.x + 196.f * sc + static_cast<float>(std::fmod(t * 0.18, 1.0)) * span;
        dl->AddRectFilledMultiColor({sx - 40.f * sc, ly}, {sx, ly + 1.5f}, ui::colorU32(a, 0.f), ui::colorU32({1, 1, 1, 0.7f}),
                                    ui::colorU32({1, 1, 1, 0.7f}), ui::colorU32(a, 0.f));
    }
}

void ClickGUI::drawSidebar(ImVec2 p0, ImVec2 size) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    float alpha = ImGui::GetStyle().Alpha;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = theme.rounding() * 1.6f + 4.f;
    dl->AddRectFilled(p0, {p0.x + size.x, p0.y + size.y}, ui::colorU32({0.f, 0.f, 0.f, 0.18f}), r,
                      ImDrawFlags_RoundCornersBottomLeft);
    dl->AddLine({p0.x + size.x, p0.y + 10.f * sc}, {p0.x + size.x, p0.y + size.y - 10.f * sc}, ui::colorU32({1, 1, 1, 0.05f}));

    float x = p0.x + 12.f * sc, w = size.x - 24.f * sc;
    float itemH = 46.f * sc, y0 = p0.y + 14.f * sc;
    float target = y0 + static_cast<float>(page_) * itemH;
    float iy = ui::animate(ImGui::GetID("##side_indicator"), target, 1.3f);
    ImVec4 a = theme.accent();
    dl->AddRectFilledMultiColor({x, iy}, {x + w, iy + itemH - 6.f * sc}, ui::colorU32(a, 0.28f), ui::colorU32(a, 0.06f),
                                ui::colorU32(a, 0.06f), ui::colorU32(a, 0.28f));
    dl->AddRect({x, iy}, {x + w, iy + itemH - 6.f * sc}, ui::colorU32(a, 0.35f), theme.rounding());
    dl->AddRectFilled({x, iy + 8.f * sc}, {x + 3.f * sc, iy + itemH - 14.f * sc}, ui::colorU32(a), 2.f);

    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * 1.02f;
    for (int i = 0; i < PageCount; ++i) {
        float y = y0 + static_cast<float>(i) * itemH;
        ImGui::SetCursorScreenPos({x, y});
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##page", {w, itemH - 6.f * sc})) page_ = static_cast<Page>(i);
        float hov = ui::animate(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.f : 0.f);
        ImGui::PopID();
        bool sel = i == page_;
        ImVec4 dim = theme.textDim();
        ImVec4 col = sel ? ImVec4{1, 1, 1, 1}
                         : ImVec4{dim.x + (1 - dim.x) * 0.5f * hov, dim.y + (1 - dim.y) * 0.5f * hov,
                                  dim.z + (1 - dim.z) * 0.5f * hov, 1.f};
        float cy = y + (itemH - 6.f * sc) * 0.5f;
        float shift = 3.f * sc * hov;
        ui::drawIcon(dl, kPages[i].icon, {x + 24.f * sc + shift, cy}, 20.f * sc,
                     sel ? theme.accentU32(alpha) : ui::colorU32(col));
        dl->AddText(font, fs, {x + 46.f * sc + shift, cy - fs * 0.5f}, ui::colorU32(col), kPages[i].label);
    }

    // Décor : relief stylisé + devise.
    float by = p0.y + size.y;
    ImU32 m1 = ui::colorU32(a, 0.06f), m2 = ui::colorU32(a, 0.10f);
    dl->AddTriangleFilled({p0.x + 20.f * sc, by - 70.f * sc}, {p0.x + 80.f * sc, by - 150.f * sc}, {p0.x + 140.f * sc, by - 70.f * sc}, m1);
    dl->AddTriangleFilled({p0.x + 70.f * sc, by - 70.f * sc}, {p0.x + 120.f * sc, by - 125.f * sc}, {p0.x + 175.f * sc, by - 70.f * sc}, m2);
    float ts = ImGui::GetFontSize() * 0.72f;
    const char* lines[] = {"J O U E", "R È G L E", "D O M I N E"};
    for (int i = 0; i < 3; ++i)
        dl->AddText(font, ts, {p0.x + 22.f * sc, by - 62.f * sc + static_cast<float>(i) * ts * 1.35f},
                    ui::colorU32(theme.textDim(), 0.55f), lines[i]);
    float breathe = 0.6f + 0.4f * static_cast<float>(std::sin(ImGui::GetTime() * 1.5));
    dl->AddRectFilled({p0.x + 22.f * sc, by - 14.f * sc}, {p0.x + (22.f + 36.f * breathe) * sc, by - 12.f * sc},
                      ui::colorU32(a, 0.9f), 1.f);
}

void ClickGUI::drawPageTitle(Icon icon, const char* title, const char* subtitle) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ui::drawIcon(dl, icon, {p.x + 16.f * sc, p.y + 20.f * sc}, 26.f * sc, theme.accentU32(ImGui::GetStyle().Alpha));
    ImFont* font = ImGui::GetFont();
    float ts = ImGui::GetFontSize() * 1.32f;
    dl->AddText(font, ts, {p.x + 40.f * sc, p.y + 2.f * sc}, ui::colorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), title);
    dl->AddText(font, ImGui::GetFontSize() * 0.86f, {p.x + 41.f * sc, p.y + 4.f * sc + ts}, ui::colorU32(theme.textDim()),
                subtitle);
    ImGui::Dummy({1.f, 46.f * sc});
}

bool ClickGUI::pill(const char* label, bool selected) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 sz{ts.x + 26.f * sc, ImGui::GetFrameHeight()};
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, sz);
    float hov = ui::animate(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.f : 0.f);
    float sel = ui::animate(ImGui::GetItemID() + 7, selected ? 1.f : 0.f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 a = theme.accent();
    dl->AddRectFilled(p, {p.x + sz.x, p.y + sz.y}, ui::colorU32({1, 1, 1, 0.03f + 0.04f * hov}), 99.f);
    if (sel > 0.01f) {
        dl->AddRectFilled(p, {p.x + sz.x, p.y + sz.y}, ui::colorU32(a, 0.22f * sel), 99.f);
        dl->AddRect(p, {p.x + sz.x, p.y + sz.y}, ui::colorU32(a, 0.55f * sel), 99.f);
        float uw = (sz.x - 24.f * sc) * sel;
        dl->AddRectFilled({p.x + (sz.x - uw) * 0.5f, p.y + sz.y - 2.f}, {p.x + (sz.x + uw) * 0.5f, p.y + sz.y}, ui::colorU32(a));
    }
    ImVec4 tc = selected ? ImVec4{1, 1, 1, 1} : theme.textDim();
    dl->AddText({p.x + 13.f * sc, p.y + (sz.y - ts.y) * 0.5f}, ui::colorU32(tc), label);
    return clicked;
}

// ---------------------------------------------------------------------------
// Cartes
// ---------------------------------------------------------------------------

bool ClickGUI::beginCard(const char* id, float width) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {14.f * sc, 12.f * sc});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.rounding() + 3.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.085f, 0.110f, 0.170f, 0.62f});
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{1.f, 1.f, 1.f, 0.07f});
    return ImGui::BeginChild(id, {width, 0},
                             ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void ClickGUI::endCard(bool highlight) {
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ThemeManager& theme = ui::theme();
    float alpha = ImGui::GetStyle().Alpha;
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hovered = ImGui::IsMouseHoveringRect(a, b) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    float hov = ui::animate(ImGui::GetItemID() + 11, hovered ? 1.f : 0.f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = theme.rounding() + 3.f;
    if (highlight) dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, theme.accentU32(0.30f * alpha), r + 1, 1.2f);
    for (int i = 1; i <= 3 && hov > 0.01f; ++i) {
        float g = static_cast<float>(i) * 1.6f;
        dl->AddRect({a.x - g, a.y - g}, {b.x + g, b.y + g}, theme.accentU32((0.28f / static_cast<float>(i)) * hov * alpha),
                    r + g, 1.5f);
    }
}

void ClickGUI::drawModuleCard(Module& module, float width) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    std::string cardId = "##card_" + module.id();
    beginCard(cardId.c_str(), width);
    ImGui::PushID(module.id().c_str());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float avail = ImGui::GetContentRegionAvail().x;
    float iconBox = 38.f * sc;
    float toggleW = ImGui::GetFrameHeight() * 0.78f * 1.8f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float on = ui::animate(ImGui::GetID("##on"), module.enabled() ? 1.f : 0.f);

    // Icône dans une case teintée.
    ImVec4 a = theme.accent();
    dl->AddRectFilled(p, {p.x + iconBox, p.y + iconBox}, ui::colorU32(a, 0.10f + 0.10f * on), theme.rounding() + 2.f);
    dl->AddRect(p, {p.x + iconBox, p.y + iconBox}, ui::colorU32(a, 0.20f + 0.25f * on), theme.rounding() + 2.f);
    ui::drawIcon(dl, module.icon(), {p.x + iconBox * 0.5f, p.y + iconBox * 0.5f}, iconBox * 0.56f,
                 ui::colorU32({a.x + (1 - a.x) * 0.25f, a.y + (1 - a.y) * 0.25f, a.z + (1 - a.z) * 0.25f, 1.f}));

    // Nom + description.
    float textX = iconBox + 12.f * sc;
    ImGui::SetCursorScreenPos({p.x + textX, p.y - 1.f});
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.10f);
    ImGui::TextUnformatted(module.name().c_str());
    ImGui::PopFont();
    std::string limit = module.limitation();
    if (!limit.empty()) {
        ImGui::SameLine();
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.78f);
        ImGui::TextColored({1.f, 0.72f, 0.30f, 0.95f}, "LIMITÉ");
        ImGui::PopFont();
        ui::tooltip(limit.c_str());
    }
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.86f);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::max(40.f, avail - textX - toggleW - 12.f * sc));
    ImGui::TextColored(theme.textDim(), "%s", module.description().c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::EndGroup();
    float headerH = std::max(iconBox, ImGui::GetItemRectSize().y);

    ImGui::SetCursorScreenPos({p.x + avail - toggleW, p.y + (iconBox - ImGui::GetFrameHeight()) * 0.5f});
    if (ui::toggleSwitch("##enabled", module.enabled())) module.toggle();
    ui::tooltip(module.enabled() ? "Désactiver" : "Activer");

    ImGui::SetCursorScreenPos({p.x, p.y + headerH + 8.f * sc});
    ImGui::Dummy({avail, 0.f});
    module.drawCardPreview();

    // Réglages : les principaux, puis « Plus de réglages ».
    std::vector<Setting*> list;
    for (auto& s : module.settings())
        if (s.get() != &module.keybind() && s->visible()) list.push_back(s.get());
    bool& more = moreSettings_[module.id()];
    int primary = std::min(static_cast<int>(list.size()), module.primarySettingCount());
    for (int i = 0; i < static_cast<int>(list.size()); ++i)
        if (i < primary || more) ui::settingWidget(*list[static_cast<size_t>(i)]);
    ui::keybindButton(module.keybind(), "Raccourci");

    int hidden = static_cast<int>(list.size()) - primary;
    if (more) {
        if (!limit.empty()) ui::unavailableNote(limit.c_str());
        module.drawExtraSettings();
        if (ImGui::SmallButton("Réinitialiser le module")) module.resetToDefaults();
    }
    if (hidden > 0 || !limit.empty() || more) {
        std::string label = more ? "Moins de réglages" : (hidden > 0 ? std::format("Plus de réglages ({})", hidden) : "Détails");
        ImGui::PushStyleColor(ImGuiCol_Text, theme.accent(0.9f));
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.86f);
        if (ImGui::Selectable(label.c_str(), false)) more = !more;
        ImGui::PopFont();
        ImGui::PopStyleColor();
    }
    ImGui::PopID();
    endCard(module.enabled());
}

void ClickGUI::drawModulePage(int category) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    const PageInfo& info = kPages[category];
    ImVec2 top = ImGui::GetCursorPos();
    drawPageTitle(info.icon, info.title, info.subtitle);

    // Filtres et recherche, alignés à droite du titre si la place le permet.
    float searchW = 190.f * sc;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float pillsW = ImGui::CalcTextSize("Tous").x + ImGui::CalcTextSize("Activés").x + 52.f * sc + spacing;
    float right = ImGui::GetContentRegionAvail().x;
    ImVec2 back = ImGui::GetCursorPos();
    bool inline_ = right > searchW + pillsW + 380.f * sc;
    if (inline_) ImGui::SetCursorPos({top.x + right - searchW - pillsW - spacing, top.y + 6.f * sc});
    if (pill("Tous", filter_ == 0)) filter_ = 0;
    ImGui::SameLine();
    if (pill("Activés", filter_ == 1)) filter_ = 1;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(searchW, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##search", "Rechercher…", search_, sizeof(search_));
    if (inline_) ImGui::SetCursorPos(back);
    else ImGui::Spacing();

    std::vector<Module*> mods;
    bool any = false;
    if (s_.modules)
        for (Module* m : s_.modules->byCategory(static_cast<Category>(category))) {
            any = true;
            if (matchesSearch(*m)) mods.push_back(m);
        }
    if (mods.empty()) {
        ImGui::Spacing();
        ui::dimText(any ? "Aucun module ne correspond au filtre." : "Aucun module dans cette catégorie.");
        return;
    }

    // Grille « masonry » : chaque carte va dans la colonne la moins haute.
    float avail = ImGui::GetContentRegionAvail().x;
    int cols = avail > 1040.f * sc ? 3 : (avail > 600.f * sc ? 2 : 1);
    float gap = 12.f * sc;
    float colW = std::floor((avail - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols));
    ImVec2 origin = ImGui::GetCursorPos();
    std::vector<float> colY(static_cast<size_t>(cols), origin.y);
    for (Module* m : mods) {
        size_t c = static_cast<size_t>(std::min_element(colY.begin(), colY.end()) - colY.begin());
        ImGui::SetCursorPos({origin.x + static_cast<float>(c) * (colW + gap), colY[c]});
        drawModuleCard(*m, colW);
        colY[c] += ImGui::GetItemRectSize().y + gap;
    }
    ImGui::SetCursorPos({origin.x, *std::max_element(colY.begin(), colY.end())});
    ImGui::Dummy({1.f, 1.f});
}

// ---------------------------------------------------------------------------
// Carte
// ---------------------------------------------------------------------------

void ClickGUI::drawMapPage() {
    if (!s_.map) {
        drawPageTitle(Icon::Map, kPages[PageMap].title, kPages[PageMap].subtitle);
        ui::dimText("Carte indisponible.");
        return;
    }
    s_.map->draw(ImGui::GetTime(), s_.gameDetected && s_.gameDetected());
}

// ---------------------------------------------------------------------------
// Réglages
// ---------------------------------------------------------------------------

void ClickGUI::drawSettingsPage() {
    drawPageTitle(Icon::Gear, kPages[PageSettings].title, kPages[PageSettings].subtitle);
    const char* tabs[] = {"Thème", "Raccourcis", "Général", "HUD", "Performance", "Capacités", "Infos"};
    for (int i = 0; i < 7; ++i) {
        if (i) ImGui::SameLine();
        if (pill(tabs[i], settingsTab_ == i)) settingsTab_ = i;
    }
    ImGui::Spacing();
    float width = std::min(ImGui::GetContentRegionAvail().x, 640.f * ui::theme().uiScale());
    beginCard("##settings_card", width);
    switch (settingsTab_) {
    case 0: drawThemeTab(); break;
    case 1: drawKeybindsTab(); break;
    case 2: drawGeneralTab(); break;
    case 3: drawHudTab(); break;
    case 4: drawPerformanceTab(); break;
    case 5: drawCapabilitiesTab(); break;
    default: drawAboutTab(); break;
    }
    endCard();
}

void ClickGUI::drawThemeTab() {
    for (auto& setting : ui::theme().settings())
        if (setting->visible()) ui::settingWidget(*setting);
    ImGui::Spacing();
    if (ImGui::Button("Réinitialiser le thème")) ui::theme().resetSettings();
    ImGui::SameLine();
    if (ImGui::Button("Recentrer le menu")) resetLayout();
}

void ClickGUI::drawKeybindsTab() {
    if (s_.client)
        if (auto* key = dynamic_cast<KeySetting*>(s_.client->findSetting("menu_key"))) ui::keybindButton(*key, "Ouvrir le menu");
    if (!s_.modules) return;
    ui::sectionTitle("Modules");
    for (auto& m : s_.modules->all()) {
        ui::keybindButton(m->keybind(), m->name().c_str());
        if (s_.keybinds && m->keybind().get()) {
            auto bound = s_.keybinds->modulesBoundTo(m->keybind().get());
            if (bound.size() > 1) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.72f, 0.30f, 1.f});
                ImGui::TextWrapped("  Même touche que %zu autre(s) module(s)", bound.size() - 1);
                ImGui::PopStyleColor();
            }
        }
    }
    if (s_.keybinds && !s_.keybinds->lastMessage().empty()) ui::dimText("%s", s_.keybinds->lastMessage().c_str());
    ui::dimText("Les touches sont observées sans être bloquées : Minecraft les reçoit aussi.");
}

void ClickGUI::drawGeneralTab() {
    if (!s_.client) {
        ui::dimText("Réglages généraux indisponibles.");
        return;
    }
    for (auto& setting : s_.client->settings())
        if (setting->visible() && setting->type() != SettingType::Key) ui::settingWidget(*setting);
    ImGui::Spacing();
    if (s_.launchMinecraft) {
        if (ImGui::Button("Lancer Minecraft")) s_.launchMinecraft();
        ui::tooltip("Ouvre le jeu via son protocole officiel « minecraft: »");
        ImGui::SameLine();
    }
    if (ImGui::Button("Réinitialiser")) s_.client->resetSettings();
    if (s_.modules) {
        ImGui::SameLine();
        if (ImGui::Button("Tout désactiver"))
            for (auto& m : s_.modules->all()) m->setEnabled(false);
    }
}

void ClickGUI::drawHudTab() {
    if (!s_.hud) {
        ui::dimText("HUD indisponible.");
        return;
    }
    for (auto& setting : s_.hud->settings())
        if (setting->visible()) ui::settingWidget(*setting);
    ImGui::Spacing();
    if (ImGui::Button("Éditer les positions du HUD", {ImGui::GetContentRegionAvail().x, 0})) s_.hud->setEditMode(true);
    if (ImGui::Button("Replacer les éléments par défaut")) s_.hud->resetLayout();
    ui::dimText("En mode édition : glisse un élément pour le déplacer, molette pour changer sa taille.");
}

void ClickGUI::drawPerformanceTab() {
    if (!s_.perf) return;
    PerfStats& p = *s_.perf;
    ImGui::Text("FPS de l'overlay : %.0f", p.fps());
    ImGui::Text("Temps CPU / image : moy. %.2f ms · p99 %.2f ms", p.avgFrameMs(), p.p99FrameMs());
    ImGui::Text("CPU : %.1f %% d'un cœur (%.2f %% du total)", p.cpuPercent(), p.cpuPercentAllCores());
    ImGui::Text("Mémoire : %.1f Mo (privée %.1f Mo)", p.workingSetMb(), p.privateMb());
    ImGui::PlotLines("##frametimes", p.history().data(), static_cast<int>(p.history().size()),
                     static_cast<int>(p.historyOffset()), "ms CPU / image", 0.f, 8.f,
                     {ImGui::GetContentRegionAvail().x, 60.f * ui::theme().uiScale()});
    ui::dimText("Ces mesures concernent uniquement l'overlay, pas Minecraft.");
}

void ClickGUI::drawCapabilitiesTab() {
    if (!s_.adapter) {
        ui::dimText("Adaptateur de jeu indisponible.");
        return;
    }
    ImGui::TextWrapped("Ce que %s peut réellement savoir. Une donnée indisponible n'est jamais simulée.", s_.adapter->name().c_str());
    ImGui::Spacing();
    if (ImGui::BeginTable("##caps", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Capacité", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("État", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Provenance", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableHeadersRow();
        for (auto& c : s_.adapter->capabilities()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(capabilityLabel(c.capability));
            if (!c.reason.empty()) ui::tooltip(c.reason.c_str());
            ImGui::TableNextColumn();
            ImVec4 col = c.availability == Availability::Available ? ImVec4{0.4f, 0.9f, 0.55f, 1}
                         : c.availability == Availability::Unsupported ? ImVec4{0.95f, 0.45f, 0.45f, 1}
                                                                      : ImVec4{1.f, 0.75f, 0.35f, 1};
            ImGui::TextColored(col, "%s", availabilityLabel(c.availability));
            if (!c.reason.empty()) ui::tooltip(c.reason.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", dataSourceId(c.source));
        }
        ImGui::EndTable();
    }
    ui::dimText("Survole une ligne pour voir le motif. La version Java (mod Fabric) a accès à beaucoup plus de données.");
}

void ClickGUI::drawAboutTab() {
    ImGui::TextWrapped("Bedrock Custom Client v" BCC_VERSION " — overlay externe pour Minecraft Bedrock (Windows).");
    ui::dimText("Aucune injection, aucune lecture ni écriture de la mémoire du jeu, aucun fichier du jeu modifié. "
                "Les données affichées viennent du clavier/souris (Raw Input), de la fenêtre du jeu et, pour le ping, "
                "d'une requête publique au serveur choisi.");
    ui::dimText("Non disponibles depuis un overlay externe : FPS réel du jeu, position/vitesse du joueur, distance "
                "aux autres joueurs, santé, armure, équipement.");
}

// ---------------------------------------------------------------------------
// Profils
// ---------------------------------------------------------------------------

void ClickGUI::drawConfigsPage() {
    drawPageTitle(Icon::Folder, kPages[PageConfigs].title, kPages[PageConfigs].subtitle);
    if (!s_.configs) {
        ui::dimText("Gestion des profils indisponible.");
        return;
    }
    ConfigManager& cfg = *s_.configs;
    float sc = ui::theme().uiScale();
    auto report = [&](const OpResult& r) {
        configStatus_ = r.message;
        configStatusOk_ = r.ok;
        if (s_.notify) {
            if (r.ok) s_.notify->success(r.message);
            else s_.notify->error(r.message);
        }
    };
    float avail = ImGui::GetContentRegionAvail().x;
    bool twoCols = avail > 640.f * sc;
    float gap = 12.f * sc;
    float leftW = twoCols ? std::floor((avail - gap) * 0.5f) : avail;
    ImVec2 origin = ImGui::GetCursorPos();

    beginCard("##profiles", leftW);
    ui::sectionTitle("Profils");
    auto profiles = cfg.listProfiles();
    if (selectedProfile_.empty()) selectedProfile_ = cfg.activeProfile();
    for (auto& name : profiles) {
        bool active = name == cfg.activeProfile();
        std::string label = name + (active ? "   (actif)" : "");
        if (ImGui::Selectable(label.c_str(), selectedProfile_ == name, ImGuiSelectableFlags_AllowDoubleClick)) {
            selectedProfile_ = name;
            pendingDelete_.clear();
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !active) report(cfg.load(name));
        }
    }
    ui::dimText("Double-clic pour charger. Profil actif : %s%s", cfg.activeProfile().c_str(),
                cfg.dirty() ? " (modifications en attente)" : "");
    endCard();
    float leftH = ImGui::GetItemRectSize().y;

    if (twoCols) ImGui::SetCursorPos({origin.x + leftW + gap, origin.y});
    beginCard("##profile_actions", twoCols ? avail - leftW - gap : avail);
    ui::sectionTitle("Actions");
    float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    bool canUse = !selectedProfile_.empty() && cfg.profileExists(selectedProfile_);
    ImGui::BeginDisabled(!canUse || selectedProfile_ == cfg.activeProfile());
    if (ImGui::Button("Charger", {bw, 0})) report(cfg.load(selectedProfile_));
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Enregistrer", {bw, 0})) report(cfg.save());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##profile_name", "Nom du nouveau profil", profileName_, sizeof(profileName_));
    std::string newName = profileName_;
    std::string why;
    bool validName = ConfigManager::isValidProfileName(newName, &why);
    ImGui::BeginDisabled(!validName);
    if (ImGui::Button("Créer (défauts)", {bw, 0})) {
        report(cfg.create(newName));
        if (configStatusOk_) {
            profileName_[0] = 0;
            selectedProfile_ = cfg.activeProfile();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Dupliquer l'actuel", {bw, 0})) {
        report(cfg.duplicate(newName));
        if (configStatusOk_) {
            profileName_[0] = 0;
            selectedProfile_ = cfg.activeProfile();
        }
    }
    ImGui::EndDisabled();
    if (!newName.empty() && !validName) ImGui::TextColored({1.f, 0.6f, 0.4f, 1.f}, "%s", why.c_str());

    if (ImGui::Button("Importer…", {bw, 0})) {
        if (auto file = dialogs::openJson(s_.ownerHwnd)) {
            report(cfg.importFrom(*file));
            selectedProfile_ = cfg.activeProfile();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Exporter…", {bw, 0})) {
        if (auto file = dialogs::saveJson(s_.ownerHwnd, toWide(cfg.activeProfile()) + L".json")) report(cfg.exportTo(*file));
    }

    ImGui::BeginDisabled(!canUse);
    if (pendingDelete_ != selectedProfile_) {
        if (ImGui::Button("Supprimer…", {ImGui::GetContentRegionAvail().x, 0})) pendingDelete_ = selectedProfile_;
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.70f, 0.20f, 0.24f, 0.9f});
        if (ImGui::Button(std::format("Confirmer la suppression de « {} »", selectedProfile_).c_str(),
                          {ImGui::GetContentRegionAvail().x, 0})) {
            report(cfg.remove(selectedProfile_));
            pendingDelete_.clear();
            selectedProfile_ = cfg.activeProfile();
        }
        ImGui::PopStyleColor();
    }
    ImGui::EndDisabled();
    if (s_.openConfigsFolder && ImGui::SmallButton("Ouvrir le dossier des profils")) s_.openConfigsFolder();
    if (!configStatus_.empty())
        ImGui::TextColored(configStatusOk_ ? ImVec4{0.4f, 0.9f, 0.6f, 1.f} : ImVec4{1.f, 0.5f, 0.45f, 1.f}, "%s",
                           configStatus_.c_str());
    ui::dimText("Les profils supprimés sont déplacés dans le dossier .trash (récupérables).");
    endCard();
    float rightH0 = ImGui::GetItemRectSize().y;
    if (twoCols) ImGui::SetCursorPos({origin.x, origin.y + std::max(leftH, rightH0) + gap});
    beginCard("##builtin_profiles", avail);
    ui::sectionTitle("Profils prédéfinis (communs Java / Bedrock)");
    for (auto& b : ConfigManager::listBuiltinProfiles(paths::sharedDir() / L"profiles")) {
        ImGui::PushID(b.name.c_str());
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(b.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", b.description.c_str());
        ui::sameLineRight(90.f * sc);
        if (ImGui::Button("Utiliser", {90.f * sc, 0})) {
            report(cfg.useBuiltin(b));
            selectedProfile_ = cfg.activeProfile();
        }
        ImGui::PopID();
    }
    ui::dimText("Crée une copie modifiable. Les modules propres à la version Java sont ignorés ici.");
    endCard();
    ImGui::Dummy({1, 1});
}

void ClickGUI::drawHudEditBar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    float w = std::min(vp->Size.x - 20.f, 660.f * sc);
    ImGui::SetNextWindowPos({vp->Pos.x + (vp->Size.x - w) * 0.5f, vp->Pos.y + 14.f * sc});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme.panelBg());
    ImGui::PushStyleColor(ImGuiCol_Border, theme.accent(0.6f));
    if (ImGui::Begin("##hud_edit", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(theme.accent(), "Mode édition du HUD");
        ImGui::SameLine();
        ImGui::TextDisabled("glisse les éléments · molette = taille");
        ui::sameLineRight(200.f * sc);
        if (ImGui::Button("Réinitialiser", {95.f * sc, 0})) s_.hud->resetLayout();
        ImGui::SameLine();
        if (ImGui::Button("Terminer", {95.f * sc, 0})) s_.hud->setEditMode(false);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

// ---------------------------------------------------------------------------
// Perspective
// ---------------------------------------------------------------------------

void ClickGUI::applyPerspective() {
    if (!perspActive_ || !mainWindow_) return;
    ImGuiContext& g = *GImGui;
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImVec2 m = ImGui::GetIO().MousePos;
    bool mouseInMenu = m.x >= winMin_.x && m.y >= winMin_.y && m.x <= winMax_.x && m.y <= winMax_.y;
    for (ImGuiWindow* w : g.Windows) {
        if (!w->Active) continue;
        bool mine = w == mainWindow_ || w->RootWindow == mainWindow_ || w->RootWindowPopupTree == mainWindow_;
        bool tip = (w->Flags & ImGuiWindowFlags_Tooltip) && mouseInMenu;
        if (!mine && !tip) continue;
        ImDrawList* dl = w->DrawList;
        for (ImDrawVert& v : dl->VtxBuffer) v.pos = persp_.forward(v.pos);
        for (ImDrawCmd& cmd : dl->CmdBuffer) {
            ImVec4& c = cmd.ClipRect;
            ImVec2 pts[4] = {persp_.forward({c.x, c.y}), persp_.forward({c.z, c.y}), persp_.forward({c.z, c.w}),
                             persp_.forward({c.x, c.w})};
            float x0 = pts[0].x, y0 = pts[0].y, x1 = pts[0].x, y1 = pts[0].y;
            for (auto& pt : pts) {
                x0 = std::min(x0, pt.x);
                y0 = std::min(y0, pt.y);
                x1 = std::max(x1, pt.x);
                y1 = std::max(y1, pt.y);
            }
            c = {std::max(0.f, std::floor(x0)), std::max(0.f, std::floor(y0)), std::min(disp.x, std::ceil(x1)),
                 std::min(disp.y, std::ceil(y1))};
        }
    }
}

bool ClickGUI::remapMouse(ImVec2 real, ImVec2* out) const {
    if (!perspActive_) return false;
    ImVec2 flat;
    if (!persp_.inverse(real, &flat)) return false;
    float margin = 12.f;
    bool inside = flat.x >= winMin_.x - margin && flat.y >= winMin_.y - margin && flat.x <= winMax_.x + margin &&
                  flat.y <= winMax_.y + margin;
    bool popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    bool dragging = ImGui::GetActiveID() != 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (!inside && !popup && !dragging) return false;
    *out = flat;
    return true;
}

} // namespace bcc
