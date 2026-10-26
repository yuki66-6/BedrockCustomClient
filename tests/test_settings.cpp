#include "TestFramework.h"

#include "core/Setting.h"

using namespace bcc;

TEST_CASE("Setting : bornes et révision") {
    FloatSetting f("reach", "Portée", "", 3.f, 0.f, 6.f);
    uint64_t rev = Setting::revision();
    f.set(10.f);
    CHECK(f.get() == 6.f);
    CHECK(Setting::revision() > rev);
    rev = Setting::revision();
    f.set(6.f); // aucune modification : pas de nouvelle révision
    CHECK(Setting::revision() == rev);
    f.set(std::nanf(""));
    CHECK(f.get() == 6.f);
    f.reset();
    CHECK(f.get() == 3.f);

    IntSetting i("n", "N", "", 5, 1, 10);
    i.set(-4);
    CHECK(i.get() == 1);
}

TEST_CASE("Setting : sérialisation JSON aller-retour") {
    SettingsGroup a;
    a.add<BoolSetting>("b", "B", "", false)->set(true);
    a.add<IntSetting>("i", "I", "", 1, 0, 100)->set(42);
    a.add<FloatSetting>("f", "F", "", 1.f, 0.f, 10.f)->set(2.5f);
    a.add<EnumSetting>("e", "E", "", std::vector<std::string>{"Croix", "Point", "Cercle"}, 0)->set(2);
    a.add<ColorSetting>("c", "C", "", Color{1, 0, 0, 1})->set({0.f, 0.5f, 1.f, 0.25f});
    a.add<KeySetting>("k", "K", "", 0)->set(0x52);
    a.add<StringSetting>("s", "S", "", "")->set("geo.hivebedrock.network");
    a.add<ActionSetting>("act", "Action", "", [] {});
    Json j = a.settingsToJson();
    CHECK(!j.contains("act")); // les actions ne sont pas persistées
    CHECK(j["e"] == "Cercle");

    SettingsGroup b;
    auto* bb = b.add<BoolSetting>("b", "B", "", false);
    auto* bi = b.add<IntSetting>("i", "I", "", 1, 0, 100);
    auto* bf = b.add<FloatSetting>("f", "F", "", 1.f, 0.f, 10.f);
    auto* be = b.add<EnumSetting>("e", "E", "", std::vector<std::string>{"Croix", "Point", "Cercle"}, 0);
    auto* bc = b.add<ColorSetting>("c", "C", "", Color{1, 0, 0, 1});
    auto* bk = b.add<KeySetting>("k", "K", "", 0);
    auto* bs = b.add<StringSetting>("s", "S", "", "");
    auto warnings = b.settingsFromJson(j, "test");
    CHECK(warnings.empty());
    CHECK(bb->get());
    CHECK(bi->get() == 42);
    CHECK(bf->get() == 2.5f);
    CHECK(be->get() == 2);
    CHECK((bc->get() == Color{0.f, 0.5f, 1.f, 0.25f}));
    CHECK(bk->get() == 0x52);
    CHECK(bs->get() == "geo.hivebedrock.network");
}

TEST_CASE("Setting : valeurs JSON invalides ignorées avec avertissement") {
    SettingsGroup g;
    auto* f = g.add<FloatSetting>("f", "F", "", 1.f, 0.f, 10.f);
    auto* e = g.add<EnumSetting>("e", "E", "", std::vector<std::string>{"A", "B"}, 1);
    auto* c = g.add<ColorSetting>("c", "C", "", Color{1, 1, 1, 1});
    Json j = {{"f", "pas un nombre"}, {"e", "Z"}, {"c", {1, 2}}, {"inconnu", 3}};
    auto warnings = g.settingsFromJson(j, "test");
    CHECK(warnings.size() == 4);
    CHECK(f->get() == 1.f);
    CHECK(e->get() == 1);
    CHECK((c->get() == Color{1, 1, 1, 1}));
    CHECK(g.settingsFromJson(Json::array(), "test").size() == 1);
}

TEST_CASE("Setting : visibilité conditionnelle et callback") {
    BoolSetting mode("m", "M", "", false);
    FloatSetting dep("d", "D", "", 1.f, 0.f, 2.f);
    dep.setVisibleIf([&] { return mode.get(); });
    int calls = 0;
    mode.setOnChange([&] { ++calls; });
    CHECK(!dep.visible());
    mode.set(true);
    CHECK(dep.visible());
    CHECK(calls == 1);
}
