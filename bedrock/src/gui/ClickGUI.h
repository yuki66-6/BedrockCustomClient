#pragma once

#include "core/Json.h"
#include "gui/Icons.h"
#include "gui/Perspective.h"

#include <imgui.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

struct ImGuiWindow;

namespace bcc {

class ThemeManager;
class Notifications;
class PerfStats;
class ModuleManager;
class KeybindManager;
class ConfigManager;
class HudManager;
class SettingsGroup;
class TelemetryProvider;
class Module;
class MapPage;
class GameAdapter;

// Services utilisés par l'interface. Les pointeurs nuls désactivent la partie
// correspondante (tests, versions intermédiaires).
struct GuiServices {
    ThemeManager* theme = nullptr;
    Notifications* notify = nullptr;
    PerfStats* perf = nullptr;
    ModuleManager* modules = nullptr;
    KeybindManager* keybinds = nullptr;
    ConfigManager* configs = nullptr;
    HudManager* hud = nullptr;
    SettingsGroup* client = nullptr;
    TelemetryProvider* telemetry = nullptr;
    MapPage* map = nullptr;
    GameAdapter* adapter = nullptr;
    std::function<void()> requestQuit;
    std::function<void()> requestClose;      // fermer le menu (bouton ×)
    std::function<void()> openConfigsFolder;
    std::function<void()> launchMinecraft;
    std::function<bool()> gameDetected;
    void* ownerHwnd = nullptr; // propriétaire des boîtes de dialogue
};

// Menu principal : fenêtre unique (barre latérale + cartes de modules),
// effet verre, perspective 3D dynamique et animations.
class ClickGUI {
public:
    enum Page { PageCombat = 0, PageMovement, PageRender, PagePlayer, PageMap, PageSettings, PageConfigs, PageCount };

    explicit ClickGUI(GuiServices services);

    void setOpen(bool open);
    bool isOpen() const { return open_; }
    float openProgress() const { return progress_; }
    bool visible() const { return open_ || progress_ > 0.f; }
    void setPage(Page page) { page_ = page; }
    Page page() const { return page_; }

    void draw(double now);
    // À appeler après ImGui::Render() : applique la perspective aux listes de
    // dessin du menu (et de ses popups).
    void applyPerspective();
    // Convertit la position réelle de la souris en position « à plat » pour
    // ImGui. Retourne false si aucune conversion n'est nécessaire.
    bool remapMouse(ImVec2 real, ImVec2* out) const;

    Json toJson() const;
    void fromJson(const Json& j);
    void resetLayout();

private:
    void drawBackground(ImDrawList* dl, ImVec2 p0, ImVec2 p1);
    void drawHeader(ImVec2 p0, ImVec2 size);
    void drawSidebar(ImVec2 p0, ImVec2 size);
    void drawPageTitle(Icon icon, const char* title, const char* subtitle);
    bool pill(const char* label, bool selected);
    void drawModulePage(int category);
    void drawModuleCard(Module& module, float width);
    void drawSettingsPage();
    void drawMapPage();
    void drawConfigsPage();
    void drawHudEditBar();
    void drawThemeTab();
    void drawKeybindsTab();
    void drawGeneralTab();
    void drawHudTab();
    void drawPerformanceTab();
    void drawAboutTab();
    void drawCapabilitiesTab();
    bool beginCard(const char* id, float width);
    void endCard(bool highlight = false);
    bool matchesSearch(const Module& module) const;

    GuiServices s_;
    bool open_ = false;
    float progress_ = 0.f;
    double lastTime_ = 0;
    Page page_ = PageCombat;
    Page shownPage_ = PageCombat;
    double pageChangeTime_ = -10;
    int filter_ = 0;      // 0 = tous, 1 = activés
    int settingsTab_ = 0;
    char search_[64] = {};
    ImVec2 posFrac_{-1, -1}; // position normalisée de la fenêtre (négatif = défaut)
    std::map<std::string, bool> moreSettings_;

    // Perspective
    ImGuiWindow* mainWindow_ = nullptr;
    Perspective persp_;
    bool perspActive_ = false;
    float yaw_ = 0.f, pitch_ = 0.f;
    ImVec2 winMin_{0, 0}, winMax_{0, 0};

    // Page Configs
    char profileName_[64] = {};
    std::string selectedProfile_;
    std::string pendingDelete_;
    std::string configStatus_;
    bool configStatusOk_ = true;
};

} // namespace bcc
