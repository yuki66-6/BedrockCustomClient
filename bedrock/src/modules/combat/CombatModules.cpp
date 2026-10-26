#include "modules/combat/CombatModules.h"

#include "integration/TelemetryProvider.h"
#include "modules/HudDraw.h"

#include <format>

namespace bcc {

CpsModule::CpsModule()
    : Module("cps", "CPS Counter", "Affiche tes clics par seconde (vrais clics souris).", Category::Combat) {
    mode_ = add<EnumSetting>("mode", "Affichage", "Boutons pris en compte",
                             std::vector<std::string>{"Les deux", "Gauche", "Droite"}, 0);
    showPeak_ = add<BoolSetting>("show_peak", "Afficher le pic", "Pic de CPS de la session", false);
    color_ = add<ColorSetting>("color", "Couleur", "Couleur du texte", Color{1, 1, 1, 1});
    onlyInGame_ = add<BoolSetting>("only_in_game", "Seulement en jeu",
                                   "Ne compte que les clics faits quand Minecraft est au premier plan", true);
}

void CpsModule::onUpdate(double, double) {
    if (context() && context()->telemetry) context()->telemetry->input().setCountOnlyInGame(onlyInGame_->get());
}

void CpsModule::drawHud(HudContext& ctx) {
    if (!ctx.telemetry) return;
    InputState& in = ctx.telemetry->input();
    int l = in.cps(MouseButton::Left, ctx.now), r = in.cps(MouseButton::Right, ctx.now);
    std::string text;
    switch (mode_->get()) {
    case 1: text = std::format("{} CPS", l); break;
    case 2: text = std::format("{} CPS", r); break;
    default: text = std::format("{} | {} CPS", l, r); break;
    }
    ImGui::TextColored(hud::col(color_->get()), "%s", text.c_str());
    if (showPeak_->get())
        ImGui::TextDisabled("pic %d | %d", in.counter(MouseButton::Left).peak(), in.counter(MouseButton::Right).peak());
}

CombatHudModule::CombatHudModule()
    : Module("combat_hud", "Combat HUD", "Paramètres de combat réellement disponibles, en un coup d'œil.",
             Category::Combat) {
    showCps_ = add<BoolSetting>("show_cps", "CPS", "Clics par seconde gauche/droite", true);
    showPing_ = add<BoolSetting>("show_ping", "Ping serveur", "Latence mesurée par le module Ping", true);
    showReach_ = add<BoolSetting>("show_reach", "Reach Lab (simulé)", "Portée configurée dans le simulateur", true);
    showUnavailable_ = add<BoolSetting>("show_unavailable", "Afficher les N/D",
                                        "Liste aussi les données que le jeu n'expose pas", false);
}

std::string CombatHudModule::limitation() const { return TelemetryProvider::kEntityDistanceUnavailable; }

void CombatHudModule::drawHud(HudContext& ctx) {
    if (!ctx.telemetry) return;
    TelemetryProvider& t = *ctx.telemetry;
    ImGui::TextColored({0.55f, 0.75f, 1.f, 1.f}, "COMBAT");
    if (showCps_->get())
        hud::line("CPS", std::format("{} | {}", t.input().cps(MouseButton::Left, ctx.now),
                                     t.input().cps(MouseButton::Right, ctx.now)));
    if (showPing_->get()) {
        PingResult p = t.ping();
        if (p.valid) hud::line("Ping", std::format("{} ms", p.ms), p.ms < 60 ? ImVec4{0.4f, 0.9f, 0.5f, 1} : ImVec4{1, 0.8f, 0.4f, 1});
        else hud::line("Ping", "—", {0.6f, 0.6f, 0.65f, 1});
    }
    if (showReach_->get()) {
        // Lu dans les réglages du module Reach Lab via le profil : affiché comme simulé.
        if (simReach_) hud::line("Reach (sim.)", std::format("{:.2f}", simReach_()), {0.6f, 0.75f, 1.f, 1});
    }
    hud::line("Portée serveur", "inconnue", {0.7f, 0.7f, 0.75f, 1});
    if (showUnavailable_->get()) {
        hud::unavailable("Distance cible", TelemetryProvider::kEntityDistanceUnavailable);
        hud::unavailable("FPS du jeu", TelemetryProvider::kGameFpsUnavailable);
    }
}

} // namespace bcc
