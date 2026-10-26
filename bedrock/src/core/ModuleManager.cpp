#include "core/ModuleManager.h"

#include "core/EventBus.h"
#include "core/Events.h"
#include "core/ModuleDefinitions.h"
#include "utils/Log.h"

#include <format>

namespace bcc {

ModuleManager::~ModuleManager() { shutdownAll(); }

bool ModuleManager::isValidId(std::string_view id) {
    if (id.empty() || id.size() > 48) return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}

bool ModuleManager::add(std::unique_ptr<Module> module, std::string* error) {
    auto fail = [&](std::string msg) {
        log::error("Enregistrement de module refusé : {}", msg);
        if (error) *error = std::move(msg);
        return false;
    };
    if (!module) return fail("module nul");
    if (!isValidId(module->id())) return fail(std::format("identifiant invalide « {} » (a-z, 0-9, _)", module->id()));
    if (find(module->id())) return fail(std::format("identifiant déjà utilisé « {} »", module->id()));
    module->setToggleListener([this](Module& m) {
        log::info("Module {} {}", m.id(), m.enabled() ? "activé" : "désactivé");
        if (bus_) bus_->publish(ModuleToggledEvent{&m, m.enabled()});
    });
    modules_.push_back(std::move(module));
    return true;
}

Module* ModuleManager::find(std::string_view id) const {
    for (auto& m : modules_)
        if (m->id() == id) return m.get();
    return nullptr;
}

std::vector<Module*> ModuleManager::byCategory(Category category) const {
    std::vector<Module*> out;
    for (auto& m : modules_)
        if (m->category() == category) out.push_back(m.get());
    return out;
}

size_t ModuleManager::enabledCount() const {
    size_t n = 0;
    for (auto& m : modules_) n += m->enabled() ? 1 : 0;
    return n;
}

void ModuleManager::initAll(ModuleContext& ctx) {
    for (auto& m : modules_) m->init(ctx);
}

void ModuleManager::shutdownAll() {
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) (*it)->shutdown();
}

void ModuleManager::updateAll(double now, double dt) {
    for (auto& m : modules_)
        if (m->enabled()) m->onUpdate(now, dt);
}

Json ModuleManager::toJson() const {
    Json j = Json::object();
    for (auto& m : modules_) j[m->id()] = m->toJson();
    return j;
}

std::vector<std::string> ModuleManager::fromJson(const Json& j) {
    std::vector<std::string> warnings;
    if (!j.is_object()) {
        warnings.push_back("section modules invalide");
        return warnings;
    }
    for (auto& [id, value] : j.items()) {
        Module* m = find(id);
        if (!m) {
            // Module d'un profil commun destiné à l'autre édition : ignoré sans avertissement.
            if (ModuleDefinitions::global().find(id)) log::debug("Module {} non disponible dans l'édition Bedrock", id);
            else warnings.push_back(std::format("module inconnu « {} » ignoré", id));
            continue;
        }
        auto w = m->fromJson(value);
        warnings.insert(warnings.end(), w.begin(), w.end());
    }
    return warnings;
}

void ModuleManager::resetAll() {
    for (auto& m : modules_) m->resetToDefaults();
}

} // namespace bcc
