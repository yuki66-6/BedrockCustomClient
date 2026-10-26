#include "GuiHarness.h"
#include "TestFramework.h"

#include "gui/ClickGUI.h"
#include "gui/MapPage.h"
#include "gui/Notifications.h"
#include "gui/TextureManager.h"
#include "gui/ThemeManager.h"
#include "map/MapExtensions.h"
#include "map/WorldScanner.h"
#include "utils/Paths.h"

#include <chrono>
#include <fstream>
#include <thread>

using namespace bcc;

TEST_CASE("Extensions de carte : chargement, couleurs de mods, repères") {
    bcctest::TempDir dir;
    std::ofstream(dir.path() / "mods.json") << R"({
        // commentaires acceptés
        "markers": [{"label": "Base", "x": 10, "z": -20, "icon": "home", "color": "#11223344"},
                    {"label": "Sans coordonnées"}],
        "areas": [{"label": "Ferme", "x1": 0, "z1": 0, "x2": 32, "z2": 16, "dimension": "nether"}],
        "blockColors": {"monmod:ruby": "#FF0000", "monmod:bad": "rouge"}
    })";
    std::ofstream(dir.path() / "casse.json") << "{ pas du json";
    map::BlockColors colors;
    map::MapExtensions ext;
    auto errors = ext.load(colors, dir.path());
    CHECK(errors.size() == 3); // repère invalide, couleur invalide, fichier invalide
    REQUIRE(ext.markers().size() == 1);
    CHECK(ext.markers()[0].icon == Icon::Home);
    CHECK(ext.markers()[0].color == map::BlockColors::rgba(0x11, 0x22, 0x33, 0x44));
    REQUIRE(ext.areas().size() == 1);
    CHECK(ext.areas()[0].dimension == 1);
    CHECK(colors.color("monmod:ruby") == map::BlockColors::rgba(255, 0, 0));

}

TEST_CASE("Page Carte : rendu, sources et lecture du monde actuel (lecture seule)") {
    bcctest::TempDir data;
    paths::setDataDir(data.path());
    bcctest::GuiHarness h;
    REQUIRE(h.ok());
    TextureManager textures;
    textures.setDevice(h.device());
    Notifications notes;
    MapPage page(&textures, &notes);
    GuiServices s;
    s.theme = &h.theme();
    s.map = &page;
    s.gameDetected = [] { return false; };
    ClickGUI gui(s);
    gui.setOpen(true);
    gui.setPage(ClickGUI::PageMap);
    h.run(10, [&](double now) { gui.draw(now); }, [&] { gui.applyPerspective(); });
    // Si des mondes existent sur ce PC, la page lit le plus récent : on attend la fin.
    for (int i = 0; i < 300 && page.busy(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    h.run(40, [&](double now) { gui.draw(now); }, [&] { gui.applyPerspective(); });
    CHECK(h.capture("map_page") > 40000);
    if (page.worldCount() > 0) {
        REQUIRE(page.currentMap() != nullptr);
        std::printf("    monde actuel détecté : « %s », %d chunks\n", page.currentMap()->world.name.c_str(),
                    page.currentMap()->chunks);
    }
    paths::setDataDir({});
}
