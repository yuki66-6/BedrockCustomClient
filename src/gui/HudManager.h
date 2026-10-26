#pragma once

#include "core/Setting.h"

#include <imgui.h>

#include <map>
#include <string>

namespace bcc {

class ModuleManager;
class TelemetryProvider;

// Affiche les éléments HUD des modules actifs. En mode édition (menu ouvert),
// les éléments se déplacent à la souris et se redimensionnent à la molette ;
// positions et tailles sont enregistrées dans le profil (fractions de l'écran).
class HudManager : public SettingsGroup {
public:
    explicit HudManager(ModuleManager& modules);

    void draw(TelemetryProvider* telemetry, ImVec2 areaMin, ImVec2 areaSize, double now, float dt);
    // Dessin libre des modules (réticule...), sous les fenêtres HUD.
    void drawOverlays(TelemetryProvider* telemetry, ImVec2 areaMin, ImVec2 areaSize, double now, float dt);

    bool editMode() const { return editMode_; }
    void setEditMode(bool edit);
    void resetLayout();
    float globalScale() const { return scale_->get(); }

    Json toJson() const;
    std::vector<std::string> fromJson(const Json& j);
    void resetAll();

private:
    struct Layout {
        float x = -1, y = -1; // fraction de la zone de jeu ; négatif = position par défaut du module
        float scale = 1.f;
    };

    ModuleManager& modules_;
    FloatSetting* scale_;
    BoolSetting* background_;
    BoolSetting* snap_;
    std::map<std::string, Layout> layouts_;
    bool editMode_ = false;
    std::string dragging_;
};

} // namespace bcc
