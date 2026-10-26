#include "modules/Module.h"

#include <format>

namespace bcc {

const char* categoryName(Category category) {
    switch (category) {
    case Category::Combat: return "Combat";
    case Category::Movement: return "Movement";
    case Category::Render: return "Render";
    case Category::Player: return "Player";
    default: return "?";
    }
}

Module::Module(std::string id, std::string name, std::string description, Category category, bool enabledByDefault)
    : id_(std::move(id)), name_(std::move(name)), description_(std::move(description)), category_(category),
      enabled_(enabledByDefault), enabledByDefault_(enabledByDefault) {
    keybind_ = add<KeySetting>("keybind", "Raccourci", "Touche pour activer/désactiver ce module en jeu", 0);
}

void Module::setEnabled(bool enabled) {
    if (enabled == enabled_) return;
    enabled_ = enabled;
    if (initialized_) {
        if (enabled_) onEnable();
        else onDisable();
    }
    Setting::bumpRevision();
    if (toggleListener_) toggleListener_(*this);
}

void Module::init(ModuleContext& ctx) {
    if (initialized_) return;
    ctx_ = &ctx;
    onInit(ctx);
    initialized_ = true;
    if (enabled_) onEnable();
}

void Module::shutdown() {
    if (!initialized_) return;
    if (enabled_) onDisable();
    onShutdown();
    initialized_ = false;
}

Json Module::toJson() const { return {{"enabled", enabled_}, {"settings", settingsToJson()}}; }

std::vector<std::string> Module::fromJson(const Json& j) {
    std::vector<std::string> warnings;
    if (!j.is_object()) {
        warnings.push_back(std::format("module {} : objet attendu", id_));
        return warnings;
    }
    if (auto it = j.find("enabled"); it != j.end()) {
        if (it->is_boolean()) setEnabled(it->get<bool>());
        else warnings.push_back(std::format("module {} : « enabled » invalide", id_));
    }
    if (auto it = j.find("settings"); it != j.end()) {
        auto w = settingsFromJson(*it, "module " + id_);
        warnings.insert(warnings.end(), w.begin(), w.end());
    }
    return warnings;
}

void Module::resetToDefaults() {
    resetSettings();
    setEnabled(enabledByDefault_);
}

} // namespace bcc
