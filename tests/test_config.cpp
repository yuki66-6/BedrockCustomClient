#include "TestFramework.h"

#include "core/ConfigManager.h"
#include "core/ModuleManager.h"

#include <fstream>

using namespace bcc;

namespace {

class CfgModule : public Module {
public:
    explicit CfgModule(std::string id) : Module(std::move(id), "Cfg", "", Category::Render) {
        value = add<FloatSetting>("value", "Valeur", "", 1.f, 0.f, 10.f);
    }
    FloatSetting* value;
};

struct Fixture {
    bcctest::TempDir dir;
    ModuleManager modules;
    ConfigManager cfg{dir.path()};
    CfgModule* mod;
    Fixture() {
        mod = modules.emplace<CfgModule>("cfg_mod");
        cfg.addSection({"modules", [this] { return modules.toJson(); },
                        [this](const Json& j) { return modules.fromJson(j); }, [this] { modules.resetAll(); }});
    }
};

void writeText(const std::filesystem::path& p, const std::string& s) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}

} // namespace

TEST_CASE("Config : noms de profils") {
    CHECK(ConfigManager::isValidProfileName("PvP Hive"));
    CHECK(ConfigManager::isValidProfileName("défaut-2"));
    CHECK(!ConfigManager::isValidProfileName(""));
    CHECK(!ConfigManager::isValidProfileName("a/b"));
    CHECK(!ConfigManager::isValidProfileName("con"));
    CHECK(!ConfigManager::isValidProfileName(" espace"));
    CHECK(!ConfigManager::isValidProfileName("fin."));
    CHECK(!ConfigManager::isValidProfileName(std::string(40, 'x')));
}

TEST_CASE("Config : démarrage, sauvegarde et rechargement") {
    Fixture f;
    auto r = f.cfg.loadStartupProfile();
    CHECK(r.ok);
    CHECK(f.cfg.activeProfile() == "default");
    CHECK(std::filesystem::exists(f.cfg.profilePath("default")));

    f.mod->value->set(7.f);
    f.mod->setEnabled(true);
    CHECK(f.cfg.dirty());
    CHECK(f.cfg.save().ok);
    CHECK(!f.cfg.dirty());

    f.mod->value->set(2.f);
    CHECK(f.cfg.load("default").ok);
    CHECK(f.mod->value->get() == 7.f);
    CHECK(f.mod->enabled());
    CHECK(!f.cfg.dirty());

    // Un second gestionnaire retrouve le profil actif via client.json.
    ModuleManager mm2;
    auto* m2 = mm2.emplace<CfgModule>("cfg_mod");
    ConfigManager cfg2(f.dir.path());
    cfg2.addSection({"modules", [&] { return mm2.toJson(); }, [&](const Json& j) { return mm2.fromJson(j); },
                     [&] { mm2.resetAll(); }});
    CHECK(cfg2.loadStartupProfile().ok);
    CHECK(m2->value->get() == 7.f);
}

TEST_CASE("Config : créer, dupliquer, supprimer vers la corbeille") {
    Fixture f;
    f.cfg.loadStartupProfile();
    f.mod->value->set(5.f);
    CHECK(f.cfg.duplicate("copie").ok);
    CHECK(f.cfg.activeProfile() == "copie");
    CHECK(f.mod->value->get() == 5.f);
    CHECK(!f.cfg.duplicate("copie").ok); // existe déjà

    CHECK(f.cfg.create("vierge").ok);
    CHECK(f.mod->value->get() == 1.f); // valeurs par défaut
    CHECK(f.cfg.listProfiles().size() == 3);

    CHECK(f.cfg.remove("vierge").ok);
    CHECK(f.cfg.listProfiles().size() == 2);
    CHECK(f.cfg.activeProfile() != "vierge");
    bool inTrash = false;
    for (auto& e : std::filesystem::directory_iterator(f.dir.path() / ".trash"))
        inTrash |= e.path().filename().string().rfind("vierge-", 0) == 0;
    CHECK(inTrash);
    CHECK(!f.cfg.remove("inexistant").ok);
}

TEST_CASE("Config : fichier corrompu mis de côté") {
    Fixture f;
    f.cfg.loadStartupProfile();
    writeText(f.cfg.profilePath("casse"), "{ ceci n'est pas du json");
    f.mod->value->set(9.f);
    auto r = f.cfg.load("casse");
    CHECK(!r.ok);
    CHECK(f.mod->value->get() == 1.f); // réinitialisé
    CHECK(std::filesystem::exists(f.cfg.profilePath("casse"))); // recréé propre
    bool backup = false;
    for (auto& e : std::filesystem::directory_iterator(f.dir.path() / "profiles"))
        backup |= e.path().filename().string().find(".corrupt-") != std::string::npos;
    CHECK(backup);
    // Les sauvegardes corrompues n'apparaissent pas comme profils.
    for (auto& p : f.cfg.listProfiles()) CHECK(p.find("corrupt") == std::string::npos);
}

TEST_CASE("Config : import / export") {
    Fixture f;
    f.cfg.loadStartupProfile();
    f.mod->value->set(3.5f);
    auto out = f.dir.path() / "export" / "mon profil.json";
    std::filesystem::create_directories(out.parent_path());
    CHECK(f.cfg.exportTo(out).ok);
    f.mod->value->set(1.f);
    CHECK(f.cfg.importFrom(out).ok);
    CHECK(f.cfg.activeProfile() == "mon profil");
    CHECK(f.mod->value->get() == 3.5f);
    CHECK(f.cfg.importFrom(out).ok); // nom déjà pris : suffixe automatique
    CHECK(f.cfg.activeProfile() == "mon profil (2)");

    writeText(f.dir.path() / "autre.json", R"({"pas":"un profil"})");
    CHECK(!f.cfg.importFrom(f.dir.path() / "autre.json").ok);
}

TEST_CASE("Config : sauvegarde automatique avec délai") {
    Fixture f;
    f.cfg.loadStartupProfile();
    f.cfg.setAutosave(true, 1.0);
    f.cfg.update(10.0);
    f.mod->value->set(4.f);
    f.cfg.update(10.2); // changement détecté
    CHECK(f.cfg.dirty());
    f.cfg.update(10.8); // délai non écoulé
    CHECK(f.cfg.dirty());
    f.cfg.update(11.3);
    CHECK(!f.cfg.dirty());
    f.cfg.setAutosave(false, 1.0);
    f.mod->value->set(6.f);
    f.cfg.update(20.0);
    f.cfg.update(30.0);
    CHECK(f.cfg.dirty());
}

TEST_CASE("Config : sections inconnues et versions futures tolérées") {
    Fixture f;
    f.cfg.loadStartupProfile();
    writeText(f.cfg.profilePath("futur"),
              R"({"format": 99, "sections": {"modules": {"cfg_mod": {"enabled": true, "settings": {"value": 8}}}, "inconnue": {}}})");
    auto r = f.cfg.load("futur");
    CHECK(r.ok);
    CHECK(f.mod->value->get() == 8.f);
    CHECK(!f.cfg.lastWarnings().empty());
}
