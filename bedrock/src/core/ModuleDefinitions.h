#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace bcc {

struct ModuleEditionStatus {
    std::string status = "planned"; // tested | implemented | partial | unsupported | planned
    std::string versions;
    std::string note;
    std::string providedBy = "module"; // module | map_page | launcher
    bool shown() const { return status != "planned" && status != "unsupported"; }
};

struct ModuleDefinition {
    std::string id;
    std::string category;
    std::string name;        // français
    std::string description; // français
    std::vector<std::string> requiredCapabilities;
    ModuleEditionStatus java;
    ModuleEditionStatus bedrock;
};

// Définitions communes des modules (shared/module-definitions/modules.json) :
// statut par édition, capacités requises. Sert à ignorer sans avertissement les
// modules d'un profil destinés à l'autre édition, et à afficher les statuts.
class ModuleDefinitions {
public:
    bool load(const std::filesystem::path& file, std::string* error = nullptr);
    bool loaded() const { return !modules_.empty(); }
    const ModuleDefinition* find(std::string_view id) const;
    const std::vector<ModuleDefinition>& all() const { return modules_; }

    static ModuleDefinitions& global();

private:
    std::vector<ModuleDefinition> modules_;
};

} // namespace bcc
