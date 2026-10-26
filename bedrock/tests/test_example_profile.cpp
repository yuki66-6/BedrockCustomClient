#include "TestFramework.h"

#include "core/ClientSettings.h"
#include "core/ConfigManager.h"
#include "core/ModuleManager.h"
#include "gui/HudManager.h"
#include "gui/ThemeManager.h"
#include "modules/ModuleRegistry.h"

using namespace bcc;

TEST_CASE("Profil d'exemple livré : chargé sans aucun avertissement") {
    auto example = std::filesystem::path(__FILE__).parent_path().parent_path() / "configs" / "example-profile.json";
    REQUIRE(std::filesystem::exists(example));

    bcctest::TempDir dir;
    ModuleManager modules;
    registerAllModules(modules);
    ThemeManager theme;
    HudManager hud(modules);
    ClientSettings client;
    ConfigManager cfg(dir.path());
    cfg.addSection({"modules", [&] { return modules.toJson(); }, [&](const Json& j) { return modules.fromJson(j); },
                    [&] { modules.resetAll(); }});
    cfg.addSection({"theme", [&] { return theme.settingsToJson(); },
                    [&](const Json& j) { return theme.settingsFromJson(j, "thème"); }, [&] { theme.resetSettings(); }});
    cfg.addSection({"hud", [&] { return hud.toJson(); }, [&](const Json& j) { return hud.fromJson(j); },
                    [&] { hud.resetAll(); }});
    cfg.addSection({"client", [&] { return client.settingsToJson(); },
                    [&](const Json& j) { return client.settingsFromJson(j, "client"); }, [&] { client.resetSettings(); }});

    auto r = cfg.importFrom(example, "Exemple");
    CHECK(r.ok);
    for (auto& w : cfg.lastWarnings()) std::printf("    avertissement : %s\n", w.c_str());
    CHECK(cfg.lastWarnings().empty());
    CHECK(cfg.activeProfile() == "Exemple");
    CHECK(modules.find("crosshair")->enabled());
    CHECK(static_cast<EnumSetting*>(modules.find("crosshair")->findSetting("style"))->current() == "Croix + point");
    CHECK(static_cast<StringSetting*>(modules.find("ping")->findSetting("host"))->get() == "geo.hivebedrock.network");
    CHECK(client.menuKey->get() == 0x2D); // Insert
}
