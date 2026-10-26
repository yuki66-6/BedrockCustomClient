#include "modules/player/PlayerModules.h"

#include "gui/Widgets.h"
#include "integration/TelemetryProvider.h"
#include "modules/HudDraw.h"

#include <format>

namespace bcc {

SessionStatsModule::SessionStatsModule()
    : Module("session", "Session Stats", "Temps de jeu, clics et touches de la session en cours.", Category::Player) {
    showClicks_ = add<BoolSetting>("show_clicks", "Clics", "Total des clics et pic de CPS", true);
    showKeys_ = add<BoolSetting>("show_keys", "Touches", "Nombre d'appuis clavier", false);
    add<ActionSetting>("reset", "Nouvelle session", "Remet les statistiques à zéro", [this] {
        inGameSeconds_ = 0;
        resetTime_ = telemetry_ ? telemetry_->now() : -1;
        if (telemetry_) telemetry_->input().resetStats();
    });
}

void SessionStatsModule::onUpdate(double now, double dt) {
    if (context() && context()->telemetry) {
        telemetry_ = context()->telemetry;
        if (resetTime_ < 0) resetTime_ = telemetry_->sessionStart() > 0 ? telemetry_->sessionStart() : now;
        if (telemetry_->gameFocused()) inGameSeconds_ += dt;
    }
}

void SessionStatsModule::drawHud(HudContext& ctx) {
    ImGui::TextColored({0.55f, 0.75f, 1.f, 1.f}, "SESSION");
    double start = resetTime_ >= 0 ? resetTime_ : ctx.now;
    hud::line("Durée", hud::formatDuration(ctx.now - start));
    hud::line("En jeu", hud::formatDuration(inGameSeconds_));
    if (showClicks_->get() && ctx.telemetry) {
        InputState& in = ctx.telemetry->input();
        hud::line("Clics", std::format("{} | {}", in.counter(MouseButton::Left).total(), in.counter(MouseButton::Right).total()));
        hud::line("Pic CPS", std::format("{} | {}", in.counter(MouseButton::Left).peak(), in.counter(MouseButton::Right).peak()));
    }
    if (showKeys_->get() && ctx.telemetry) hud::line("Touches", std::format("{}", ctx.telemetry->input().keyPresses()));
}

void SessionStatsModule::drawCardPreview() {
    if (!telemetry_) return;
    ImGui::TextDisabled("En jeu depuis le lancement : %s", hud::formatDuration(inGameSeconds_).c_str());
}

PlayerInfoModule::PlayerInfoModule()
    : Module("player_info", "Player Info", "Ce que l'overlay sait légalement de ta partie.", Category::Player) {
    showWindow_ = add<BoolSetting>("show_window", "Fenêtre du jeu", "Résolution et état de la fenêtre Minecraft", true);
    showUnavailable_ = add<BoolSetting>("show_unavailable", "Afficher les N/D", "Liste les données non accessibles", true);
}

std::string PlayerInfoModule::limitation() const { return TelemetryProvider::kPlayerDataUnavailable; }

void PlayerInfoModule::drawHud(HudContext& ctx) {
    ImGui::TextColored({0.55f, 0.75f, 1.f, 1.f}, "JOUEUR");
    if (showWindow_->get() && ctx.telemetry) {
        const GameState& g = ctx.telemetry->game();
        if (g.found) {
            hud::line("Minecraft", g.foreground ? "actif" : (g.minimized ? "réduit" : "arrière-plan"),
                      g.foreground ? ImVec4{0.4f, 0.9f, 0.55f, 1} : ImVec4{0.8f, 0.8f, 0.85f, 1});
            hud::line("Fenêtre", std::format("{}x{}", g.width(), g.height()));
        } else {
            hud::line("Minecraft", "non détecté", {0.7f, 0.7f, 0.75f, 1});
        }
    }
    if (showUnavailable_->get()) {
        hud::unavailable("Santé / armure", TelemetryProvider::kPlayerDataUnavailable);
        hud::unavailable("Équipement", TelemetryProvider::kPlayerDataUnavailable);
        hud::unavailable("Coordonnées", "Active « Afficher les coordonnées » dans les paramètres du monde Minecraft.");
    }
}

void PlayerInfoModule::drawCardPreview() {
    ui::dimText("Disponible : état et taille de la fenêtre du jeu. Non disponible : pseudo, santé, armure, équipement, "
                "coordonnées (lecture mémoire exclue).");
}

} // namespace bcc
