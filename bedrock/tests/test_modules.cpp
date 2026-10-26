#include "GuiHarness.h"
#include "TestFramework.h"

#include "core/ClientSettings.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/KeybindManager.h"
#include "core/ModuleManager.h"
#include "gui/ClickGUI.h"
#include "gui/ThemeManager.h"
#include "gui/Widgets.h"

#include <windows.h>

using namespace bcc;

namespace {

class DummyModule : public Module {
public:
    DummyModule(std::string id, Category c = Category::Combat) : Module(std::move(id), "Dummy", "Module de test", c) {
        level = add<FloatSetting>("level", "Niveau", "", 1.f, 0.f, 5.f);
    }
    int inits = 0, enables = 0, disables = 0, shutdowns = 0, updates = 0;
    FloatSetting* level;

protected:
    void onInit(ModuleContext&) override { ++inits; }
    void onEnable() override { ++enables; }
    void onDisable() override { ++disables; }
    void onShutdown() override { ++shutdowns; }

public:
    void onUpdate(double, double) override { ++updates; }
};

KeyEvent key(int vk, bool down = true, bool game = true) {
    KeyEvent e;
    e.vk = vk;
    e.down = down;
    e.gameFocused = game;
    return e;
}

} // namespace

TEST_CASE("EventBus : abonnement, publication, désabonnement") {
    EventBus bus;
    int a = 0, b = 0;
    auto t1 = bus.subscribe<KeyEvent>([&](const KeyEvent& e) { a += e.vk; });
    {
        Subscription s(bus, bus.subscribe<KeyEvent>([&](const KeyEvent&) { ++b; }));
        bus.publish(key(5));
        CHECK(a == 5);
        CHECK(b == 1);
    }
    bus.publish(key(2)); // l'abonnement RAII a été libéré
    CHECK(a == 7);
    CHECK(b == 1);
    bus.unsubscribe(t1);
    bus.publish(key(2));
    CHECK(a == 7);
    CHECK(bus.subscriberCount() == 0);

    // Désabonnement pendant la publication : pas de plantage, pas d'appel ultérieur.
    EventBus::Token self = 0;
    int calls = 0;
    self = bus.subscribe<MenuToggledEvent>([&](const MenuToggledEvent&) {
        ++calls;
        bus.unsubscribe(self);
    });
    bus.publish(MenuToggledEvent{true});
    bus.publish(MenuToggledEvent{false});
    CHECK(calls == 1);
}

TEST_CASE("ModuleManager : enregistrement et identifiants") {
    EventBus bus;
    ModuleManager mm(&bus);
    CHECK(mm.emplace<DummyModule>("cps") != nullptr);
    std::string err;
    CHECK(!mm.add(std::make_unique<DummyModule>("cps"), &err)); // doublon
    CHECK(!err.empty());
    CHECK(!mm.add(std::make_unique<DummyModule>("Bad Id")));
    CHECK(!mm.add(std::make_unique<DummyModule>("")));
    CHECK(mm.emplace<DummyModule>("crosshair", Category::Render) != nullptr);
    CHECK(mm.all().size() == 2);
    CHECK(mm.byCategory(Category::Render).size() == 1);
    CHECK(mm.find("crosshair") != nullptr);
    CHECK(mm.get<DummyModule>("cps") != nullptr);
    CHECK(mm.find("nope") == nullptr);
}

TEST_CASE("Module : cycle de vie et événements de bascule") {
    EventBus bus;
    ModuleManager mm(&bus);
    auto* m = mm.emplace<DummyModule>("m");
    int toggles = 0;
    bool lastState = false;
    Subscription s(bus, bus.subscribe<ModuleToggledEvent>([&](const ModuleToggledEvent& e) {
        ++toggles;
        lastState = e.enabled;
    }));

    m->setEnabled(true); // avant init : pas d'onEnable
    CHECK(m->enables == 0);
    ModuleContext ctx{&bus, nullptr, nullptr};
    mm.initAll(ctx);
    CHECK(m->inits == 1);
    CHECK(m->enables == 1); // activé à l'init car déjà « enabled »
    m->toggle();
    CHECK(m->disables == 1);
    CHECK(!m->enabled());
    CHECK(toggles == 2);
    CHECK(!lastState);
    mm.updateAll(1.0, 0.016); // désactivé : pas de mise à jour
    CHECK(m->updates == 0);
    m->setEnabled(true);
    mm.updateAll(1.0, 0.016);
    CHECK(m->updates == 1);
    mm.shutdownAll();
    CHECK(m->shutdowns == 1);
    CHECK(m->disables == 2);
}

TEST_CASE("Module : sérialisation de l'état et des réglages") {
    ModuleManager a;
    auto* ma = a.emplace<DummyModule>("m");
    ma->setEnabled(true);
    ma->level->set(4.f);
    ma->keybind().set('R');
    Json j = a.toJson();

    ModuleManager b;
    auto* mb = b.emplace<DummyModule>("m");
    j["inconnu"] = Json::object();
    auto warnings = b.fromJson(j);
    CHECK(warnings.size() == 1);
    CHECK(mb->enabled());
    CHECK(mb->level->get() == 4.f);
    CHECK(mb->keybind().get() == 'R');
    mb->resetToDefaults();
    CHECK(!mb->enabled());
    CHECK(mb->level->get() == 1.f);
    CHECK(mb->keybind().get() == 0);
}

TEST_CASE("KeybindManager : bascule, répétition, focus, suspension") {
    EventBus bus;
    ModuleManager mm(&bus);
    auto* m = mm.emplace<DummyModule>("m");
    m->keybind().set('R');
    KeybindManager kb(bus, mm);

    bus.publish(key('R'));
    CHECK(m->enabled());
    KeyEvent rep = key('R');
    rep.repeat = true;
    bus.publish(rep); // auto-répétition ignorée
    CHECK(m->enabled());
    bus.publish(key('R', false)); // relâchement ignoré
    CHECK(m->enabled());
    bus.publish(key('R', true, false)); // jeu sans focus
    CHECK(m->enabled());
    kb.setRequireGameFocus(false);
    bus.publish(key('R', true, false));
    CHECK(!m->enabled());
    kb.setSuspended(true); // menu ouvert
    bus.publish(key('R'));
    CHECK(!m->enabled());
}

TEST_CASE("KeybindManager : capture d'une touche") {
    EventBus bus;
    ModuleManager mm(&bus);
    auto* m = mm.emplace<DummyModule>("m");
    KeybindManager kb(bus, mm);
    kb.setReservedKey(VK_INSERT);

    kb.beginCapture(&m->keybind());
    CHECK(kb.capturing());
    bus.publish(key('G'));
    CHECK(!kb.capturing());
    CHECK(m->keybind().get() == 'G');
    CHECK(!m->enabled()); // la touche capturée ne déclenche pas le module

    kb.beginCapture(&m->keybind());
    bus.publish(key(VK_INSERT)); // réservée au menu
    CHECK(m->keybind().get() == 'G');

    kb.beginCapture(&m->keybind());
    bus.publish(key(VK_ESCAPE));
    CHECK(m->keybind().get() == 'G');

    kb.beginCapture(&m->keybind());
    bus.publish(key(VK_BACK));
    CHECK(m->keybind().get() == 0);
    CHECK(KeybindManager::keyName(VK_INSERT) == "Insert");
    CHECK(KeybindManager::keyName(0) == "Aucune");
    CHECK(!KeybindManager::keyName('A').empty());
}

TEST_CASE("GUI : panneaux de modules, recherche et onglets") {
    bcctest::GuiHarness h;
    REQUIRE(h.ok());
    EventBus bus;
    ModuleManager mm(&bus);
    for (int i = 0; i < 4; ++i) mm.emplace<DummyModule>("mod_" + std::to_string(i), static_cast<Category>(i));
    mm.find("mod_0")->setEnabled(true);
    KeybindManager kb(bus, mm);
    ui::setKeybindManager(&kb);
    ClientSettings client;
    GuiServices s;
    s.theme = &h.theme();
    s.modules = &mm;
    s.keybinds = &kb;
    s.client = &client;
    ClickGUI gui(s);
    gui.setOpen(true);
    h.run(60, [&](double now) { gui.draw(now); });
    CHECK(h.capture("gui_modules") > 50000);
    // La disposition des panneaux est sérialisable et relisible.
    Json layout = gui.toJson();
    ClickGUI other(s);
    other.fromJson(layout);
    CHECK(other.toJson() == layout);
    ui::setKeybindManager(nullptr);
}
