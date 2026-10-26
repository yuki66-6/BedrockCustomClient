#pragma once

#include "core/Setting.h"
#include "gui/Icons.h"

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace bcc {

class EventBus;
class TelemetryProvider;
class Notifications;

enum class Category { Combat = 0, Movement, Render, Player, Count };
const char* categoryName(Category category);

// Services mis à disposition des modules lors de leur initialisation.
// Les pointeurs peuvent être nuls (tests) : les modules doivent le tolérer.
struct ModuleContext {
    EventBus* bus = nullptr;
    TelemetryProvider* telemetry = nullptr;
    Notifications* notify = nullptr;
};

// Contexte de dessin d'un élément HUD (appelé à l'intérieur d'une fenêtre ImGui
// déjà positionnée par le HudManager).
struct HudContext {
    TelemetryProvider* telemetry = nullptr;
    double now = 0;
    float scale = 1.f;
    bool editMode = false;
};

// Contexte de dessin libre plein écran (réticule...).
struct OverlayContext {
    ImDrawList* drawList = nullptr;
    ImVec2 areaMin{0, 0}, areaMax{0, 0}; // zone cliente du jeu, coordonnées overlay
    TelemetryProvider* telemetry = nullptr;
    double now = 0;
    float dt = 0;
};

// Base de tous les modules : identifiant unique, nom, description, catégorie,
// état, raccourci, paramètres et cycle de vie.
class Module : public SettingsGroup {
public:
    Module(std::string id, std::string name, std::string description, Category category, bool enabledByDefault = false);
    ~Module() override = default;

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    Category category() const { return category_; }

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled);
    void toggle() { setEnabled(!enabled_); }
    KeySetting& keybind() { return *keybind_; }
    const KeySetting& keybind() const { return *keybind_; }

    // Cycle de vie (appelé par le ModuleManager).
    void init(ModuleContext& ctx);
    void shutdown();
    bool initialized() const { return initialized_; }

    virtual void onUpdate(double now, double dt) {}

    // HUD déplaçable (fenêtre gérée par le HudManager).
    virtual bool hasHud() const { return false; }
    virtual ImVec2 defaultHudPosition() const { return {0.02f, 0.30f}; } // fraction de l'écran
    virtual void drawHud(HudContext& ctx) {}
    // Dessin libre sur toute la zone de jeu.
    virtual void drawOverlay(OverlayContext& ctx) {}
    // Aperçu visuel affiché en permanence sur la carte du module.
    virtual void drawCardPreview() {}
    // Contenu spécifique sous les réglages dans la ClickGUI.
    virtual void drawExtraSettings() {}
    virtual Icon icon() const { return Icon::Generic; }
    // Nombre de réglages affichés sur la carte avant « Plus de réglages ».
    virtual int primarySettingCount() const { return 3; }
    // Limitation technique à afficher à l'utilisateur (vide si aucune).
    virtual std::string limitation() const { return {}; }

    Json toJson() const;
    std::vector<std::string> fromJson(const Json& j);
    void resetToDefaults();

    // Notifié à chaque changement d'état (installé par le ModuleManager).
    void setToggleListener(std::function<void(Module&)> listener) { toggleListener_ = std::move(listener); }

protected:
    virtual void onInit(ModuleContext& ctx) {}
    virtual void onShutdown() {}
    virtual void onEnable() {}
    virtual void onDisable() {}
    ModuleContext* context() const { return ctx_; }

private:
    std::string id_, name_, description_;
    Category category_;
    bool enabled_ = false;
    bool enabledByDefault_ = false;
    bool initialized_ = false;
    KeySetting* keybind_ = nullptr;
    ModuleContext* ctx_ = nullptr;
    std::function<void(Module&)> toggleListener_;
};

} // namespace bcc
