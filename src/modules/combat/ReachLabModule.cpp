#include "modules/combat/ReachLabModule.h"

#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "modules/HudDraw.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace bcc {

ReachLabModule::ReachLabModule()
    : Module("reach_lab", "Reach Lab", "Simulateur local de portée. Ne modifie pas la portée en jeu.", Category::Combat) {
    reach_ = add<FloatSetting>("sim_reach", "Portée (sim.)", "Portée configurée dans le simulateur", 3.f, 0.5f, 8.f,
                               "%.2f");
    distance_ = add<FloatSetting>("target_distance", "Distance cible", "Distance pieds à pieds de la cible simulée",
                                  2.8f, 0.5f, 8.f, "%.2f");
    preset_ = add<EnumSetting>("preset", "Preset", "Préréglages de portée (A, B, C sont personnalisables)",
                               std::vector<std::string>{"Vanilla 3.00", "Preset A", "Preset B", "Preset C", "Libre"}, 0);
    expand_ = add<FloatSetting>("hitbox_expand", "Marge hitbox", "Agrandissement simulé de la hitbox (chaque côté)", 0.f,
                                0.f, 0.5f, "%.2f");
    jitter_ = add<FloatSetting>("aim_jitter", "Imprécision visée", "Écart-type de visée pour le test de coups", 1.5f, 0.f,
                                6.f, "%.1f°");
    showVanilla_ = add<BoolSetting>("show_vanilla", "Cercle vanilla", "Affiche la portée de référence 3.00", true);
    presetA_ = add<FloatSetting>("preset_a", "Valeur preset A", "", 3.2f, 0.5f, 8.f, "%.2f");
    presetB_ = add<FloatSetting>("preset_b", "Valeur preset B", "", 3.5f, 0.5f, 8.f, "%.2f");
    presetC_ = add<FloatSetting>("preset_c", "Valeur preset C", "", 4.0f, 0.5f, 8.f, "%.2f");
    for (auto* p : {presetA_, presetB_, presetC_}) p->setVisibleIf([] { return false; }); // édités via les boutons

    preset_->setOnChange([this] {
        lastHitRate_ = -1.f;
        if (!applyingPreset_ && !loadingSettings()) applyPreset(preset_->get());
    });
    reach_->setOnChange([this] {
        lastHitRate_ = -1.f;
        if (!applyingPreset_ && !loadingSettings()) {
            applyingPreset_ = true;
            preset_->set(4); // valeur saisie à la main : « Libre »
            applyingPreset_ = false;
        }
    });
    for (auto* s : {distance_, expand_, jitter_}) s->setOnChange([this] { lastHitRate_ = -1.f; });
}

std::string ReachLabModule::limitation() const {
    return "Simulateur uniquement : la portée réelle est décidée par le jeu et le serveur (The Hive valide les coups "
           "côté serveur). La distance aux vrais joueurs n'est pas accessible depuis un overlay.";
}

void ReachLabModule::applyPreset(int index) {
    float values[] = {kVanillaReach, presetA_->get(), presetB_->get(), presetC_->get()};
    if (index < 0 || index > 3) return;
    applyingPreset_ = true;
    reach_->set(values[index]);
    applyingPreset_ = false;
}

sim::ReachConfig ReachLabModule::config() const {
    sim::ReachConfig c;
    c.reach = reach_->get();
    c.hitboxExpand = expand_->get();
    return c;
}

void ReachLabModule::drawTopView(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    ImVec4 a = ui::theme().accent();
    float alpha = ImGui::GetStyle().Alpha;
    dl->AddRectFilled(p, {p.x + size.x, p.y + size.y}, ui::colorU32({0.02f, 0.03f, 0.06f, 0.55f}), 6.f);
    dl->PushClipRect(p, {p.x + size.x, p.y + size.y}, true);
    struct ClipGuard {
        ImDrawList* dl;
        ~ClipGuard() { dl->PopClipRect(); }
    } guard{dl};

    // Échelle : 8,5 blocs sur la largeur ; l'attaquant est à gauche.
    float ppb = (size.x - 24.f) / 8.5f;
    ImVec2 me{p.x + 14.f, p.y + size.y * 0.5f};
    for (int b = 1; b <= 8; ++b) {
        float x = me.x + static_cast<float>(b) * ppb;
        dl->AddLine({x, p.y + 4.f}, {x, p.y + size.y - 4.f}, ui::colorU32({1, 1, 1, b == 3 ? 0.10f : 0.04f}));
        if (b % 2 == 0)
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.7f, {x + 2.f, p.y + size.y - ImGui::GetFontSize() * 0.8f},
                        ui::colorU32({1, 1, 1, 0.3f}), std::format("{}", b).c_str());
    }
    sim::ReachConfig cfg = config();
    sim::ReachResult res = sim::evaluateAtDistance(cfg, distance_->get());
    // Portées : arc vanilla (gris) et arc simulé (accent), mesurées depuis l'œil.
    auto arc = [&](float r, ImU32 c, float th) {
        dl->PathArcTo(me, r * ppb, -0.9f, 0.9f, 40);
        dl->PathStroke(c, 0, th);
    };
    if (showVanilla_->get()) arc(kVanillaReach, ui::colorU32({0.8f, 0.8f, 0.85f, 0.45f}), 1.5f);
    arc(cfg.reach, ui::colorU32(a, 0.9f), 2.f);
    // Cible (hitbox vue de dessus).
    float hw = (cfg.targetWidth * 0.5f + cfg.hitboxExpand) * ppb;
    float tx = me.x + distance_->get() * ppb;
    ImU32 tc = res.inReach ? IM_COL32(80, 220, 130, static_cast<int>(230 * alpha)) : IM_COL32(240, 90, 90, static_cast<int>(230 * alpha));
    dl->AddRectFilled({tx - hw, me.y - hw}, {tx + hw, me.y + hw}, (tc & 0x00FFFFFF) | (static_cast<ImU32>(60 * alpha) << 24), 2.f);
    dl->AddRect({tx - hw, me.y - hw}, {tx + hw, me.y + hw}, tc, 2.f, 1.5f);
    // Rayon de visée jusqu'à la portée simulée.
    float rayEnd = me.x + std::min(cfg.reach, 8.4f) * ppb;
    dl->AddLine(me, {rayEnd, me.y}, ui::colorU32(a, 0.5f), 1.f);
    if (res.rayHits) dl->AddCircleFilled({me.x + res.rayDistance * ppb, me.y}, 3.f, tc);
    dl->AddCircleFilled(me, 5.f, ui::colorU32({1, 1, 1, 0.95f}));
}

void ReachLabModule::drawCardPreview() {
    drawTopView({ImGui::GetContentRegionAvail().x, 84.f * ui::theme().uiScale()});
    sim::ReachResult r = sim::evaluateAtDistance(config(), distance_->get());
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.86f);
    ImGui::TextColored(r.inReach ? ImVec4{0.4f, 0.9f, 0.55f, 1} : ImVec4{1.f, 0.45f, 0.45f, 1}, "%s",
                       r.inReach ? "TOUCHÉ (simulation)" : "HORS DE PORTÉE (simulation)");
    ImGui::SameLine();
    ImGui::TextDisabled("œil→hitbox %.2f · pieds %.2f", r.eyeToHitbox, r.feetDistance);
    ImGui::PopFont();
}

void ReachLabModule::drawExtraSettings() {
    sim::ReachConfig cfg = config();
    sim::ReachResult r = sim::evaluateAtDistance(cfg, distance_->get());
    ui::sectionTitle("Trois portées à ne pas confondre");
    ImGui::TextWrapped("1. Distance affichée : %.2f blocs œil→hitbox (%.2f pieds à pieds)", r.eyeToHitbox,
                       r.feetDistance);
    ImGui::TextWrapped("2. Portée du simulateur : %.2f blocs → %s", cfg.reach, r.inReach ? "touche" : "rate");
    ImGui::TextWrapped("3. Portée autorisée par le serveur : inconnue (imposée et vérifiée par le serveur ; "
                       "référence vanilla %.2f)", kVanillaReach);
    ImGui::TextDisabled("Touche à visée parfaite jusqu'à %.2f blocs (pieds à pieds).", sim::maxHitDistance(cfg));

    if (lastHitRate_ < 0.f) {
        lastHitRate_ = sim::hitRate(cfg, distance_->get(), jitter_->get(), 1000, 42);
        sim::ReachConfig vanilla = cfg;
        vanilla.reach = kVanillaReach;
        lastVanillaRate_ = sim::hitRate(vanilla, distance_->get(), jitter_->get(), 1000, 42);
    }
    ImGui::Text("Test de 1000 coups (imprécision %.1f°) : %.0f %% simulés, %.0f %% en vanilla", jitter_->get(),
                lastHitRate_ * 100.f, lastVanillaRate_ * 100.f);

    ui::sectionTitle("Presets");
    ImGui::TextDisabled("A %.2f · B %.2f · C %.2f", presetA_->get(), presetB_->get(), presetC_->get());
    float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.f) / 3.f;
    FloatSetting* slots[] = {presetA_, presetB_, presetC_};
    const char* labels[] = {"Enregistrer → A", "Enregistrer → B", "Enregistrer → C"};
    for (int i = 0; i < 3; ++i) {
        if (i) ImGui::SameLine();
        if (ImGui::Button(labels[i], {w, 0})) {
            slots[i]->set(reach_->get());
            applyingPreset_ = true;
            preset_->set(i + 1);
            applyingPreset_ = false;
        }
    }
}

void ReachLabModule::drawHud(HudContext&) {
    sim::ReachResult r = sim::evaluateAtDistance(config(), distance_->get());
    ImGui::TextColored({0.55f, 0.75f, 1.f, 1.f}, "REACH LAB (simulation)");
    hud::line("Portée sim.", std::format("{:.2f}", reach_->get()));
    hud::line("Cible", std::format("{:.2f} → {}", r.eyeToHitbox, r.inReach ? "touche" : "rate"),
              r.inReach ? ImVec4{0.4f, 0.9f, 0.55f, 1} : ImVec4{1.f, 0.45f, 0.45f, 1});
    hud::line("Serveur", "décide seul", {0.7f, 0.7f, 0.75f, 1});
}

} // namespace bcc
