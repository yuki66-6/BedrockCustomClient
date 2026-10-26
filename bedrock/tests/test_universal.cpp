#include "TestFramework.h"

#include "core/Capabilities.h"
#include "core/ClientSettings.h"
#include "core/ConfigManager.h"
#include "core/DataPoint.h"
#include "core/ModuleDefinitions.h"
#include "core/ModuleManager.h"
#include "core/WaypointStore.h"
#include "gui/HudManager.h"
#include "gui/ThemeManager.h"
#include "integration/BedrockOverlayAdapter.h"
#include "integration/TelemetryProvider.h"
#include "modules/ModuleRegistry.h"

#include <fstream>
#include <set>
#include <sstream>

using namespace bcc;

namespace {

std::filesystem::path sharedDir() { return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "shared"; }

Json readJson(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return Json::parse(ss.str(), nullptr, false, true);
}

// Client Bedrock complet pour les tests de profils (modules, thème, HUD, client).
struct ProfileFixture {
    bcctest::TempDir dir;
    ModuleManager modules;
    ThemeManager theme;
    ClientSettings client;
    std::unique_ptr<HudManager> hud;
    std::unique_ptr<ConfigManager> cfg;
    ProfileFixture() {
        registerAllModules(modules);
        hud = std::make_unique<HudManager>(modules);
        cfg = std::make_unique<ConfigManager>(dir.path());
        cfg->addSection({"modules", [this] { return modules.toJson(); }, [this](const Json& j) { return modules.fromJson(j); },
                         [this] { modules.resetAll(); }});
        cfg->addSection({"theme", [this] { return theme.settingsToJson(); },
                         [this](const Json& j) {
                             auto w = theme.settingsFromJson(j, "thème");
                             theme.reconcilePreset();
                             return w;
                         },
                         [this] { theme.resetSettings(); }});
        cfg->addSection({"hud", [this] { return hud->toJson(); }, [this](const Json& j) { return hud->fromJson(j); },
                         [this] { hud->resetAll(); }});
        cfg->addSection({"client", [this] { return client.settingsToJson(); },
                         [this](const Json& j) { return client.settingsFromJson(j, "client"); }, [this] { client.resetSettings(); },
                         "bedrock/client"});
    }
};

} // namespace

TEST_CASE("Universel : DataPoint (fraîcheur, motif)") {
    auto d = DataPoint<int>::available(42, DataSource::RawInput, 10.0, 1.0);
    CHECK(d.usable(10.5));
    CHECK(d.availabilityAt(11.5) == Availability::Stale);
    CHECK(!d.usable(11.5));
    auto forever = DataPoint<int>::available(1, DataSource::User, 0.0);
    CHECK(forever.usable(1e9));
    auto u = DataPoint<float>::missing(Availability::Unsupported, "raison");
    CHECK(!u.value.has_value());
    CHECK(u.reason == "raison");
    CHECK(std::string(availabilityId(Availability::PermissionRequired)) == "permission_required");
    CHECK(std::string(dataSourceId(DataSource::SaveFile)) == "save_file");
}

TEST_CASE("Universel : capacités C++ identiques à shared/schemas/capabilities.json") {
    Json j = readJson(sharedDir() / "schemas" / "capabilities.json");
    REQUIRE(j.is_object());
    std::set<std::string> fromJson, fromCpp;
    for (auto& c : j["capabilities"]) fromJson.insert(c["id"].get<std::string>());
    for (Capability c : allCapabilities()) {
        fromCpp.insert(capabilityId(c));
        Capability back;
        CHECK(capabilityFromId(capabilityId(c), back));
        CHECK(back == c);
    }
    CHECK(fromJson == fromCpp);
}

TEST_CASE("Universel : l'adaptateur Bedrock n'invente aucune donnée") {
    TelemetryProvider t;
    t.setNow(5.0);
    BedrockOverlayAdapter a(t);
    CHECK(a.edition() == Edition::Bedrock);
    CHECK(a.capabilities().size() == static_cast<size_t>(Capability::Count));
    for (Capability c : {Capability::PlayerPosition, Capability::PlayerHealth, Capability::Inventory, Capability::PotionEffects,
                         Capability::GameFps, Capability::FovControl}) {
        CapabilityInfo i = a.capability(c);
        CHECK(i.availability == Availability::Unsupported);
        CHECK(!i.reason.empty());
    }
    CHECK(a.capability(Capability::MouseInput).availability == Availability::Available);
    CHECK(!a.playerPosition().value.has_value());
    CHECK(!a.gameFps().value.has_value());
    CHECK(!a.worldSeed().value.has_value());
    CHECK(a.latencyMs().availability == Availability::Unavailable); // aucun serveur configuré
    t.input().onMouseButton(MouseButton::Left, true, 4.8, true);
    auto cps = a.clicksPerSecond(true);
    CHECK(cps.usable(5.0));
    CHECK(*cps.value == 1);
}

TEST_CASE("Universel : chaque module Bedrock a une définition partagée cohérente") {
    ModuleDefinitions defs;
    std::string err;
    REQUIRE(defs.load(sharedDir() / "module-definitions" / "modules.json", &err));
    ModuleManager mm;
    registerAllModules(mm);
    for (auto& m : mm.all()) {
        const ModuleDefinition* d = defs.find(m->id());
        if (!d) std::printf("    définition manquante : %s\n", m->id().c_str());
        REQUIRE(d != nullptr);
        CHECK(d->bedrock.shown()); // un module enregistré n'est jamais « planned » ni « unsupported »
    }
    // Inversement : tout module marqué disponible pour Bedrock existe réellement.
    for (auto& d : defs.all())
        if (d.bedrock.shown() && d.bedrock.providedBy == "module") {
            if (!mm.find(d.id)) std::printf("    module annoncé mais absent : %s\n", d.id.c_str());
            CHECK(mm.find(d.id) != nullptr);
        }
    // Capacités requises connues.
    for (auto& d : defs.all())
        for (auto& c : d.requiredCapabilities) {
            Capability cap;
            CHECK(capabilityFromId(c, cap));
        }
}

TEST_CASE("Universel : migration d'un profil v1 vers le format commun v2") {
    ProfileFixture f;
    REQUIRE(f.cfg->loadStartupProfile().ok);
    std::filesystem::create_directories(f.dir.path() / "profiles");
    std::ofstream(f.dir.path() / "profiles" / "ancien.json") << R"({"format": 1, "client": "BedrockCustomClient", "version": "1.0.0",
        "sections": {"modules": {"crosshair": {"enabled": true, "settings": {"size": 12}}},
                     "client": {"fps_limit": 90}}})";
    auto r = f.cfg->load("ancien");
    CHECK(r.ok);
    CHECK(f.modules.find("crosshair")->enabled());
    CHECK(f.client.fpsLimit->get() == 90);
    // Fichier réécrit en v2, copie de l'original conservée.
    Json v2 = readJson(f.dir.path() / "profiles" / "ancien.json");
    CHECK(v2.value("format", std::string()) == "umc-profile");
    CHECK(v2.value("formatVersion", 0) == 2);
    CHECK(v2["bedrock"]["client"]["fps_limit"] == 90);
    CHECK(v2["modules"]["crosshair"]["enabled"] == true);
    CHECK(std::filesystem::exists(f.dir.path() / "profiles" / ".backup-v1" / "ancien.json"));
}

TEST_CASE("Universel : profils prédéfinis communs chargés sans avertissement") {
    ProfileFixture f;
    REQUIRE(f.cfg->loadStartupProfile().ok);
    ModuleDefinitions::global().load(sharedDir() / "module-definitions" / "modules.json");
    auto builtins = ConfigManager::listBuiltinProfiles(sharedDir() / "profiles");
    CHECK(builtins.size() == 6);
    for (auto& b : builtins) {
        auto r = f.cfg->useBuiltin(b);
        CHECK(r.ok);
        for (auto& w : f.cfg->lastWarnings()) std::printf("    %s : %s\n", b.name.c_str(), w.c_str());
        CHECK(f.cfg->lastWarnings().empty()); // modules Java ignorés silencieusement
    }
    // Le profil « Performance » limite les FPS de l'overlay et coupe les animations.
    for (auto& b : builtins)
        if (b.name == "Performance") {
            f.cfg->useBuiltin(b);
            CHECK(f.client.fpsLimit->get() == 60);
            CHECK(!f.theme.animationsEnabled());
            CHECK(f.modules.find("fps")->enabled());
            CHECK(!f.modules.find("keystrokes")->enabled()); // désactivé explicitement par le profil
            CHECK(f.modules.find("info_bar")->enabled());
        }
}

TEST_CASE("Universel : waypoints au format commun (aller-retour, import, migration)") {
    bcctest::TempDir dir;
    WaypointStore store(dir.path() / "waypoints" / "umc-waypoints.json");
    CHECK(store.load().ok); // fichier absent : vide
    Waypoint wp;
    wp.name = "Base";
    wp.x = 10;
    wp.z = -20;
    wp.dimension = "minecraft:the_nether";
    auto& added = store.add("bedrock:sp:abc", "Mon monde", wp);
    CHECK(!added.id.empty());
    CHECK(added.dimension == "nether");
    CHECK(added.createdAt > 0);
    REQUIRE(store.save().ok);

    WaypointStore reloaded(store.file());
    REQUIRE(reloaded.load().ok);
    auto list = reloaded.forWorld("bedrock:sp:abc");
    REQUIRE(list.size() == 1);
    CHECK(list[0].name == "Base");
    CHECK(WaypointStore::dimensionIndex(list[0].dimension) == 1);

    // Le fichier d'exemple partagé (écrit pour Java) s'importe côté Bedrock.
    int count = 0;
    CHECK(reloaded.importFrom(sharedDir() / "waypoints" / "example-waypoints.json", &count).ok);
    CHECK(count == 1);
    CHECK(reloaded.importFrom(sharedDir() / "waypoints" / "example-waypoints.json", &count).ok);
    CHECK(count == 0); // pas de doublon

    // Migration de l'ancien fichier de repères de la carte (v1.0).
    auto legacy = dir.path() / "waypoints.json";
    std::ofstream(legacy) << R"({"name": "Mes repères", "markers": [{"label": "Mine", "x": 5, "z": 6, "icon": "flag"}]})";
    CHECK(reloaded.migrateLegacyMarkers(legacy) == 1);
    CHECK(!std::filesystem::exists(legacy));
    CHECK(reloaded.forWorld("bedrock:sp:autre").size() == 1); // global : visible dans tous les mondes Bedrock
    CHECK(reloaded.remove("bedrock:sp:abc", list[0].id));
    CHECK(!reloaded.importFrom(sharedDir() / "profiles" / "survival.json").ok); // mauvais format refusé
}
