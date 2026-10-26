#include "modules/render/RenderModules.h"

#include "gui/Icons.h"
#include "gui/TextureManager.h"
#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "integration/TelemetryProvider.h"
#include "modules/HudDraw.h"
#include "utils/Strings.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <format>

namespace bcc {

// ---------------------------------------------------------------------------
// Crosshair
// ---------------------------------------------------------------------------

CrosshairModule::CrosshairModule()
    : Module("crosshair", "Crosshair", "Réticule personnalisé : styles ou ta propre image PNG.", Category::Render) {
    style_ = add<EnumSetting>("style", "Type", "Forme du réticule",
                              std::vector<std::string>{"Croix", "Point", "Croix + point", "Cercle", "T", "Image"}, 0);
    size_ = add<FloatSetting>("size", "Taille", "Longueur des branches (px)", 7.f, 1.f, 40.f, "%.0f");
    gap_ = add<FloatSetting>("gap", "Écart", "Espace au centre (px)", 3.f, 0.f, 20.f, "%.0f");
    color_ = add<ColorSetting>("color", "Couleur", "Couleur du réticule", Color{0.35f, 0.65f, 1.f, 1.f});
    thickness_ = add<FloatSetting>("thickness", "Épaisseur", "Épaisseur des traits (px)", 2.f, 1.f, 8.f, "%.1f");
    outline_ = add<BoolSetting>("outline", "Contour", "Contour sombre pour la lisibilité", true);
    outlineColor_ = add<ColorSetting>("outline_color", "Couleur du contour", "", Color{0, 0, 0, 0.75f});
    outlineColor_->setVisibleIf([this] { return outline_->get(); });
    dynamic_ = add<BoolSetting>("dynamic", "Dynamique", "S'écarte brièvement à chaque clic gauche", false);
    image_ = add<StringSetting>("image", "Image", "Fichier du dossier crosshairs", "", 128);
    image_->setVisibleIf([] { return false; }); // choisi via la liste ci-dessous
    imageScale_ = add<FloatSetting>("image_scale", "Échelle de l'image", "", 1.f, 0.1f, 4.f, "x%.2f");
    imageScale_->setVisibleIf([this] { return style_->get() == 5; });
    for (Setting* s : std::initializer_list<Setting*>{size_, gap_, thickness_, outline_, dynamic_})
        s->setVisibleIf([this] { return style_->get() != 5; });
}

std::string CrosshairModule::limitation() const {
    return "Le réticule de Minecraft reste affiché : masque-le avec un pack de ressources si tu veux uniquement celui-ci.";
}

void CrosshairModule::refreshImages() {
    auto dir = TextureManager::folder(kFolder);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    images_ = TextureManager::listImages(dir);
    imagesScanned_ = true;
}

void CrosshairModule::drawAt(ImDrawList* dl, ImVec2 c, float scale, float spread, TelemetryProvider* telemetry) {
    float alpha = ImGui::GetStyle().Alpha;
    Color cc = color_->get();
    ImU32 col = ImGui::ColorConvertFloat4ToU32({cc.r, cc.g, cc.b, cc.a * alpha});
    Color oc = outlineColor_->get();
    ImU32 ocol = ImGui::ColorConvertFloat4ToU32({oc.r, oc.g, oc.b, oc.a * alpha});
    int style = style_->get();

    if (style == 5) { // image utilisateur
        TextureManager* tm = telemetry ? telemetry->textures() : nullptr;
        Texture tex;
        if (tm && !image_->get().empty()) tex = tm->get(TextureManager::folder(kFolder) / toWide(image_->get()));
        if (tex) {
            float w = static_cast<float>(tex.width) * imageScale_->get() * scale;
            float h = static_cast<float>(tex.height) * imageScale_->get() * scale;
            dl->AddImage(ImTextureRef(tex.id), {c.x - w * 0.5f, c.y - h * 0.5f}, {c.x + w * 0.5f, c.y + h * 0.5f}, {0, 0},
                         {1, 1}, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)));
        } else {
            dl->AddText({c.x - 10.f, c.y - 8.f}, col, "?");
        }
        return;
    }

    float len = size_->get() * scale, gap = (gap_->get() + spread) * scale, th = thickness_->get() * scale;
    auto bar = [&](ImVec2 a, ImVec2 b) {
        if (outline_->get()) {
            float o = std::max(1.f, scale);
            dl->AddRectFilled({std::min(a.x, b.x) - o, std::min(a.y, b.y) - o}, {std::max(a.x, b.x) + o, std::max(a.y, b.y) + o}, ocol);
        }
        dl->AddRectFilled({std::min(a.x, b.x), std::min(a.y, b.y)}, {std::max(a.x, b.x), std::max(a.y, b.y)}, col);
    };
    float h = th * 0.5f;
    auto cross = [&](bool top) {
        if (top) bar({c.x - h, c.y - gap - len}, {c.x + h, c.y - gap});
        bar({c.x - h, c.y + gap}, {c.x + h, c.y + gap + len});
        bar({c.x - gap - len, c.y - h}, {c.x - gap, c.y + h});
        bar({c.x + gap, c.y - h}, {c.x + gap + len, c.y + h});
    };
    auto dot = [&] {
        float r = std::max(1.f, th);
        if (outline_->get()) dl->AddCircleFilled(c, r + std::max(1.f, scale), ocol);
        dl->AddCircleFilled(c, r, col);
    };
    switch (style) {
    case 0: cross(true); break;
    case 1: dot(); break;
    case 2: cross(true); dot(); break;
    case 3:
        if (outline_->get()) dl->AddCircle(c, len + gap, ocol, 0, th + 2.f * std::max(1.f, scale));
        dl->AddCircle(c, len + gap, col, 0, th);
        dot();
        break;
    case 4: cross(false); break;
    default: break;
    }
}

void CrosshairModule::drawOverlay(OverlayContext& ctx) {
    float target = 0.f;
    if (dynamic_->get() && ctx.telemetry && ctx.telemetry->input().mouseDown(MouseButton::Left)) target = 4.f;
    spread_ = hud::approach(spread_, target, ctx.dt, 18.f);
    ImVec2 c{std::floor((ctx.areaMin.x + ctx.areaMax.x) * 0.5f), std::floor((ctx.areaMin.y + ctx.areaMax.y) * 0.5f)};
    drawAt(ctx.drawList, c, 1.f, spread_, ctx.telemetry);
}

void CrosshairModule::drawCardPreview() {
    float sc = ui::theme().uiScale();
    float w = ImGui::GetContentRegionAvail().x, h = 64.f * sc;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy({w, h});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Fond « ciel » pour juger le contraste du réticule.
    dl->AddRectFilledMultiColor(p, {p.x + w, p.y + h}, IM_COL32(70, 110, 170, 150), IM_COL32(70, 110, 170, 150),
                                IM_COL32(40, 70, 40, 150), IM_COL32(40, 70, 40, 150));
    float spread = dynamic_->get() ? 2.f + 2.f * static_cast<float>(std::sin(ImGui::GetTime() * 4.0)) : 0.f;
    drawAt(dl, {std::floor(p.x + w * 0.5f), std::floor(p.y + h * 0.5f)}, 1.5f, spread,
           context() ? context()->telemetry : nullptr);
}

void CrosshairModule::drawExtraSettings() {
    ui::sectionTitle("Images personnalisées");
    if (!imagesScanned_) refreshImages();
    const std::string& current = image_->get();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(w);
    if (ImGui::BeginCombo("##images", current.empty() ? "Aucune image" : current.c_str())) {
        for (auto& name : images_)
            if (ImGui::Selectable(name.c_str(), name == current)) {
                image_->set(name);
                style_->set(5);
            }
        if (images_.empty()) ImGui::TextDisabled("Aucun fichier PNG/JPG/BMP dans le dossier.");
        ImGui::EndCombo();
    }
    float bw = (w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Ouvrir le dossier", {bw, 0})) {
        refreshImages();
        ShellExecuteW(nullptr, L"open", TextureManager::folder(kFolder).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::Button("Rafraîchir", {bw, 0})) {
        refreshImages();
        if (context() && context()->telemetry && context()->telemetry->textures()) context()->telemetry->textures()->clear();
    }
    TextureManager* tm = context() && context()->telemetry ? context()->telemetry->textures() : nullptr;
    if (tm && !tm->lastError().empty()) ui::unavailableNote(tm->lastError().c_str());
    ui::dimText("Dépose tes images (PNG transparent conseillé, 16 à 256 px) dans le dossier « crosshairs », "
                "clique Rafraîchir puis choisis-la. Le style passe automatiquement sur « Image ».");
}

// ---------------------------------------------------------------------------
// FPS
// ---------------------------------------------------------------------------

FpsModule::FpsModule()
    : Module("fps", "FPS Counter", "Images par seconde de l'overlay (mesurées).", Category::Render) {
    showLabel_ = add<BoolSetting>("show_label", "Préciser « overlay »", "Rappelle que ce n'est pas le FPS du jeu", true);
    color_ = add<ColorSetting>("color", "Couleur", "", Color{1, 1, 1, 1});
}

std::string FpsModule::limitation() const { return TelemetryProvider::kGameFpsUnavailable; }

void FpsModule::drawHud(HudContext& ctx) {
    float fps = ctx.telemetry ? ctx.telemetry->overlayFps() : 0.f;
    ImGui::TextColored(hud::col(color_->get()), "%.0f FPS", fps);
    if (showLabel_->get()) {
        ImGui::SameLine();
        ImGui::TextDisabled("overlay");
    }
}

// ---------------------------------------------------------------------------
// Ping
// ---------------------------------------------------------------------------

PingModule::PingModule()
    : Module("ping", "Ping", "Latence vers ton serveur (requête publique de liste de serveurs).", Category::Render) {
    host_ = add<StringSetting>("host", "Serveur", "Adresse du serveur Bedrock (ex. geo.hivebedrock.network)", "", 128);
    port_ = add<IntSetting>("port", "Port", "Port UDP du serveur", 19132, 1, 65535);
    showMotd_ = add<BoolSetting>("show_motd", "Afficher le MOTD", "Nom affiché par le serveur", false);
    showPlayers_ = add<BoolSetting>("show_players", "Joueurs en ligne", "Nombre annoncé par le serveur", false);
    interval_ = add<FloatSetting>("interval", "Intervalle", "Temps entre deux mesures", 3.f, 1.f, 30.f, "%.0f s");
}

std::string PingModule::limitation() const {
    return "Mesure le temps de réponse du serveur à une requête publique (comme la liste des serveurs) : proche de "
           "la latence réseau, mais ce n'est pas le ping interne du jeu.";
}

void PingModule::onUpdate(double, double) {
    if (context() && context()->telemetry && context()->telemetry->pinger())
        context()->telemetry->pinger()->setTarget(host_->get(), port_->get(), interval_->get(), true);
}

void PingModule::onDisable() {
    if (context() && context()->telemetry && context()->telemetry->pinger())
        context()->telemetry->pinger()->setTarget(host_->get(), port_->get(), interval_->get(), false);
}

void PingModule::drawExtraSettings() {
    float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("The Hive", {bw, 0})) {
        host_->set("geo.hivebedrock.network");
        port_->set(19132);
    }
    ImGui::SameLine();
    if (ImGui::Button("Effacer", {bw, 0})) host_->set("");
    if (host_->get().empty()) ui::dimText("Indique l'adresse du serveur sur lequel tu joues.");
}

void PingModule::drawHud(HudContext& ctx) {
    if (host_->get().empty()) {
        ImGui::TextDisabled("Ping : serveur non configuré");
        return;
    }
    PingResult p = ctx.telemetry ? ctx.telemetry->ping() : PingResult{};
    if (p.valid) {
        ImVec4 c = p.ms < 60 ? ImVec4{0.4f, 0.9f, 0.5f, 1} : (p.ms < 120 ? ImVec4{1, 0.8f, 0.4f, 1} : ImVec4{1, 0.45f, 0.45f, 1});
        hud::line("Ping", std::format("{} ms", p.ms), c);
        if (showPlayers_->get() && p.players >= 0) hud::line("Joueurs", std::format("{}", p.players));
        if (showMotd_->get() && !p.motd.empty()) ImGui::TextDisabled("%s", p.motd.c_str());
    } else {
        hud::line("Ping", p.error.empty() ? "mesure…" : p.error, {0.7f, 0.7f, 0.75f, 1});
    }
}

// ---------------------------------------------------------------------------
// Barre d'infos
// ---------------------------------------------------------------------------

InfoBarModule::InfoBarModule()
    : Module("info_bar", "Info Bar", "Barre compacte : CPS, ping, FPS de l'overlay, heure.", Category::Render, true) {
    showCps_ = add<BoolSetting>("show_cps", "CPS", "", true);
    showPing_ = add<BoolSetting>("show_ping", "Ping", "Nécessite le module Ping configuré", true);
    showFps_ = add<BoolSetting>("show_fps", "FPS overlay", "FPS de l'overlay (pas du jeu)", false);
    showClock_ = add<BoolSetting>("show_clock", "Heure", "", false);
}

void InfoBarModule::drawHud(HudContext& ctx) {
    TelemetryProvider* t = ctx.telemetry;
    std::vector<std::string> parts;
    PingResult ping = t ? t->ping() : PingResult{};
    if (showCps_->get())
        parts.push_back(std::format("{} CPS", t ? t->input().cps(MouseButton::Left, ctx.now) : 0));
    if (showPing_->get()) parts.push_back(ping.valid ? std::format("{} ms", ping.ms) : "— ms");
    if (showFps_->get()) parts.push_back(std::format("{:.0f} FPS ovl", t ? t->overlayFps() : 0.f));
    if (showClock_->get()) {
        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        parts.push_back(std::format("{:02}:{:02}", local.tm_hour, local.tm_min));
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) {
            ImGui::SameLine();
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine({p.x, p.y + 2.f}, {p.x, p.y + ImGui::GetFontSize() - 2.f}, IM_COL32(255, 255, 255, 60));
            ImGui::Dummy({4.f, 1.f});
            ImGui::SameLine();
        }
        ImGui::TextUnformatted(parts[i].c_str());
    }
    if (showPing_->get()) {
        ImGui::SameLine();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float fs = ImGui::GetFontSize();
        ImVec4 c = !ping.valid ? ImVec4{0.5f, 0.5f, 0.55f, 1} : (ping.ms < 60 ? ImVec4{0.35f, 0.9f, 0.5f, 1} : (ping.ms < 120 ? ImVec4{1, 0.8f, 0.4f, 1} : ImVec4{1, 0.45f, 0.45f, 1}));
        ui::drawIcon(dl, Icon::Signal, {p.x + fs * 0.5f, p.y + fs * 0.5f}, fs * 0.9f, ImGui::ColorConvertFloat4ToU32(c));
        ImGui::Dummy({fs, fs});
    }
}

} // namespace bcc
