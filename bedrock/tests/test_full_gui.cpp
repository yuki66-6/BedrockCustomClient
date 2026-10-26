#include "GuiHarness.h"
#include "TestFramework.h"

#include "core/ClientSettings.h"
#include "core/ConfigManager.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/KeybindManager.h"
#include "core/ModuleManager.h"
#include "gui/ClickGUI.h"
#include "gui/HudManager.h"
#include "gui/Notifications.h"
#include "gui/TextureManager.h"
#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "integration/TelemetryProvider.h"
#include "modules/ModuleRegistry.h"
#include "modules/movement/MovementModules.h"
#include "utils/PerfStats.h"

#include <windows.h>

using namespace bcc;

namespace {

// Client complet (sans fenêtre) : tous les modules, HUD, profils, ClickGUI.
struct FullClient {
    bcctest::GuiHarness h{1600, 900};
    bcctest::TempDir dir;
    EventBus bus;
    TelemetryProvider telemetry;
    ModuleManager modules{&bus};
    ClientSettings client;
    Notifications notes;
    PerfStats perf;
    TextureManager textures;
    std::unique_ptr<KeybindManager> keybinds;
    std::unique_ptr<HudManager> hud;
    std::unique_ptr<ConfigManager> configs;
    std::unique_ptr<ClickGUI> gui;
    ModuleContext ctx;

    FullClient() {
        registerAllModules(modules);
        keybinds = std::make_unique<KeybindManager>(bus, modules);
        hud = std::make_unique<HudManager>(modules);
        configs = std::make_unique<ConfigManager>(dir.path());
        textures.setDevice(h.device());
        telemetry.setTextures(&textures);
        ui::setKeybindManager(keybinds.get());
        GuiServices s;
        s.theme = &h.theme();
        s.notify = &notes;
        s.perf = &perf;
        s.modules = &modules;
        s.keybinds = keybinds.get();
        s.hud = hud.get();
        s.client = &client;
        s.configs = configs.get();
        s.telemetry = &telemetry;
        s.gameDetected = [] { return true; };
        gui = std::make_unique<ClickGUI>(s);
        configs->addSection({"modules", [this] { return modules.toJson(); }, [this](const Json& j) { return modules.fromJson(j); },
                             [this] { modules.resetAll(); }});
        configs->addSection({"hud", [this] { return hud->toJson(); }, [this](const Json& j) { return hud->fromJson(j); },
                             [this] { hud->resetAll(); }});
        configs->loadStartupProfile();
        ctx = {&bus, &telemetry, &notes};
        modules.initAll(ctx);
    }
    ~FullClient() {
        modules.shutdownAll();
        ui::setKeybindManager(nullptr);
    }
    void frame(double now) {
        telemetry.setNow(now);
        modules.updateAll(now, 1.0 / 60.0);
        ImVec2 area = ImGui::GetIO().DisplaySize;
        hud->drawOverlays(&telemetry, {0, 0}, area, now, 1.f / 60.f);
        hud->draw(&telemetry, {0, 0}, area, now, 1.f / 60.f);
        gui->draw(now);
        notes.draw(now);
    }
};

} // namespace

TEST_CASE("Client complet : 12 modules enregistrés, identifiants uniques") {
    ModuleManager mm;
    registerAllModules(mm);
    CHECK(mm.all().size() == 12);
    for (int c = 0; c < static_cast<int>(Category::Count); ++c) CHECK(!mm.byCategory(static_cast<Category>(c)).empty());
    for (const char* id : {"reach_lab", "cps", "combat_hud", "keystrokes", "sprint", "jumps", "crosshair",
                           "info_bar", "ping", "fps", "session", "player_info"})
        CHECK(mm.find(id) != nullptr);
}

TEST_CASE("Client complet : toutes les pages et tous les modules activés se rendent") {
    FullClient fc;
    REQUIRE(fc.h.ok());
    for (auto& m : fc.modules.all()) m->setEnabled(true);
    // Simule quelques entrées réelles pour les keystrokes et le CPS.
    fc.telemetry.input().onKey(KeystrokesModule::detectMovementKeys()[0], true, 1.0, true);
    fc.telemetry.input().onMouseButton(MouseButton::Left, true, 1.0, true);

    fc.gui->setOpen(true);
    for (int page = 0; page < ClickGUI::PageCount; ++page) {
        fc.gui->setPage(static_cast<ClickGUI::Page>(page));
        fc.h.run(25, [&](double now) { fc.frame(now); }, [&] { fc.gui->applyPerspective(); });
        CHECK(fc.h.capture(("full_page_" + std::to_string(page)).c_str()) > 40000);
    }
    // Mode édition du HUD.
    fc.hud->setEditMode(true);
    fc.h.run(10, [&](double now) { fc.frame(now); });
    CHECK(fc.h.capture("full_hud_edit") > 5000);
    fc.hud->setEditMode(false);

    // Menu fermé : seuls le HUD et le réticule restent.
    fc.gui->setOpen(false);
    fc.h.run(60, [&](double now) { fc.frame(now); });
    unsigned hudPixels = fc.h.capture("full_hud_only");
    CHECK(hudPixels > 1000);
    CHECK(hudPixels < 400000);
}

TEST_CASE("Client complet : profil enregistré puis relu à l'identique") {
    FullClient fc;
    fc.modules.find("crosshair")->setEnabled(true);
    static_cast<FloatSetting*>(fc.modules.find("reach_lab")->findSetting("sim_reach"))->set(3.75f);
    static_cast<EnumSetting*>(fc.modules.find("reach_lab")->findSetting("preset"))->set(2); // preset B → 3.5
    CHECK(static_cast<FloatSetting*>(fc.modules.find("reach_lab")->findSetting("sim_reach"))->get() == 3.5f);
    REQUIRE(fc.configs->save().ok);
    Json saved = fc.modules.toJson();
    fc.modules.resetAll();
    REQUIRE(fc.configs->load("default").ok);
    CHECK(fc.modules.toJson() == saved); // le preset reste « Preset B » après rechargement
}
