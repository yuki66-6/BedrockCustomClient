#pragma once

#include "core/Json.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace bcc {

struct OpResult {
    bool ok = true;
    std::string message;
};

// Section d'un profil (modules, thème, HUD...). Chaque sous-système fournit
// sa sérialisation ; le ConfigManager assemble le document JSON.
struct ConfigSection {
    std::string key;
    std::function<Json()> save;
    std::function<std::vector<std::string>(const Json&)> load; // retourne les avertissements
    std::function<void()> reset;
};

// Profils JSON dans <dossier>/profiles/<nom>.json :
//  - écriture atomique (fichier temporaire + remplacement) ;
//  - fichier corrompu mis de côté (.corrupt-<date>.json) puis valeurs par défaut ;
//  - suppression = déplacement dans <dossier>/.trash (récupérable) ;
//  - sauvegarde automatique après un délai sans modification.
class ConfigManager {
public:
    static constexpr int kFormatVersion = 1;

    explicit ConfigManager(std::filesystem::path dir);

    void addSection(ConfigSection section);

    static bool isValidProfileName(std::string_view name, std::string* why = nullptr);

    std::vector<std::string> listProfiles() const;
    const std::string& activeProfile() const { return active_; }
    bool profileExists(std::string_view name) const;

    // Charge le dernier profil actif (client.json) ou crée « default ».
    OpResult loadStartupProfile();
    OpResult save();                          // profil actif
    OpResult load(const std::string& name);   // devient actif
    OpResult create(const std::string& name); // nouveau profil aux valeurs par défaut, devient actif
    OpResult duplicate(const std::string& name); // copie de l'état courant, devient actif
    OpResult remove(const std::string& name); // vers la corbeille
    OpResult exportTo(const std::filesystem::path& file) const;
    OpResult importFrom(const std::filesystem::path& file, std::string name = {});

    // Sauvegarde automatique : à appeler régulièrement avec l'heure courante.
    void setAutosave(bool enabled, double delaySeconds);
    void update(double now);
    bool dirty() const;

    const std::vector<std::string>& lastWarnings() const { return warnings_; }
    const std::filesystem::path& directory() const { return dir_; }
    std::filesystem::path profilePath(std::string_view name) const;

private:
    Json buildDocument() const;
    std::vector<std::string> applyDocument(const Json& doc);
    void resetAll();
    OpResult readJson(const std::filesystem::path& file, Json& out) const;
    OpResult writeAtomic(const std::filesystem::path& file, const std::string& text) const;
    void writeClientState() const;
    void markClean();

    std::filesystem::path dir_;
    std::vector<ConfigSection> sections_;
    std::string active_ = "default";
    std::vector<std::string> warnings_;

    bool autosave_ = true;
    double autosaveDelay_ = 1.5;
    uint64_t savedRevision_ = 0;
    uint64_t seenRevision_ = 0;
    double lastChange_ = 0;
};

} // namespace bcc
