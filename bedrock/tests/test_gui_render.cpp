#include "GuiHarness.h"
#include "TestFramework.h"

#include "gui/ClickGUI.h"
#include "gui/Notifications.h"
#include "gui/Perspective.h"
#include "gui/ThemeManager.h"
#include "utils/PerfStats.h"

using namespace bcc;

TEST_CASE("GUI : la ClickGUI s'ouvre, s'anime et se rend hors écran") {
    bcctest::GuiHarness h;
    REQUIRE(h.ok());
    Notifications notes;
    PerfStats perf;
    GuiServices s;
    s.theme = &h.theme();
    s.notify = &notes;
    s.perf = &perf;
    ClickGUI gui(s);

    // Fermée : rien n'est dessiné.
    h.run(5, [&](double now) { gui.draw(now); });
    CHECK(h.capture("gui_closed") == 0);

    gui.setOpen(true);
    notes.info("Notification de test");
    h.run(90, [&](double now) {
        gui.draw(now);
        notes.draw(now);
    });
    CHECK(gui.openProgress() == 1.f);
    CHECK(h.capture("gui_open") > 50000);

    // Fermeture animée jusqu'à disparition complète.
    gui.setOpen(false);
    h.run(90, [&](double now) { gui.draw(now); });
    CHECK(!gui.visible());
}

TEST_CASE("Perspective : l'inverse retrouve le point d'origine") {
    Perspective p;
    p.center = {800, 450};
    p.yaw = 0.2f;
    p.pitch = -0.05f;
    p.scale = 0.95f;
    p.focal = 2700.f;
    for (float x = 300; x <= 1300; x += 250)
        for (float y = 150; y <= 750; y += 200) {
            ImVec2 screen = p.forward({x, y});
            ImVec2 back;
            REQUIRE(p.inverse(screen, &back));
            CHECK_NEAR(back.x, x, 0.05);
            CHECK_NEAR(back.y, y, 0.05);
        }
    // Inclinaison positive : le bord droit s'éloigne (apparaît plus petit).
    float left = p.forward({300, 750}).y - p.forward({300, 150}).y;
    float right = p.forward({1300, 750}).y - p.forward({1300, 150}).y;
    CHECK(right < left);
    Perspective flat;
    CHECK(flat.identity());
}

TEST_CASE("GUI : changement de thème et d'échelle sans erreur") {
    bcctest::GuiHarness h;
    REQUIRE(h.ok());
    GuiServices s;
    s.theme = &h.theme();
    ClickGUI gui(s);
    gui.setOpen(true);
    static_cast<FloatSetting*>(h.theme().findSetting("ui_scale"))->set(1.5f);
    static_cast<EnumSetting*>(h.theme().findSetting("preset"))->set(3);
    h.run(30, [&](double now) { gui.draw(now); });
    CHECK(h.theme().uiScale() == 1.5f);
    CHECK(static_cast<EnumSetting*>(h.theme().findSetting("preset"))->current() == "Vert");
    // Couleur modifiée à la main : le préréglage passe sur « Personnalisé ».
    static_cast<ColorSetting*>(h.theme().findSetting("accent"))->set({0.1f, 0.2f, 0.3f, 1.f});
    CHECK(static_cast<EnumSetting*>(h.theme().findSetting("preset"))->current() == "Personnalisé");
}
