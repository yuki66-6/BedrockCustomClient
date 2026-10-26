#pragma once

#include "modules/Module.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcc {

class EventBus;

// Registre des modules : enregistrement, recherche, activation, cycle de vie
// et (dé)sérialisation de leur état.
class ModuleManager {
public:
    explicit ModuleManager(EventBus* bus = nullptr) : bus_(bus) {}
    ~ModuleManager();

    // Refuse un identifiant vide, invalide ou déjà utilisé.
    bool add(std::unique_ptr<Module> module, std::string* error = nullptr);

    template <class T, class... Args>
    T* emplace(Args&&... args) {
        auto m = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = m.get();
        return add(std::move(m)) ? raw : nullptr;
    }

    Module* find(std::string_view id) const;
    template <class T>
    T* get(std::string_view id) const { return dynamic_cast<T*>(find(id)); }

    const std::vector<std::unique_ptr<Module>>& all() const { return modules_; }
    std::vector<Module*> byCategory(Category category) const;
    size_t enabledCount() const;

    void initAll(ModuleContext& ctx);
    void shutdownAll();
    void updateAll(double now, double dt);

    Json toJson() const;
    std::vector<std::string> fromJson(const Json& j);
    void resetAll();

    static bool isValidId(std::string_view id);

private:
    EventBus* bus_;
    std::vector<std::unique_ptr<Module>> modules_;
};

} // namespace bcc
