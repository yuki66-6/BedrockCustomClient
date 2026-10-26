#include "core/ModuleDefinitions.h"

#include "core/Json.h"

#include <fstream>
#include <sstream>

namespace bcc {
namespace {

std::string localized(const Json& j, const char* key) {
    auto it = j.find(key);
    if (it == j.end()) return {};
    if (it->is_string()) return it->get<std::string>();
    if (it->is_object()) return it->value("fr", it->value("en", std::string()));
    return {};
}

ModuleEditionStatus readStatus(const Json& editions, const char* edition) {
    ModuleEditionStatus s;
    auto it = editions.find(edition);
    if (it == editions.end() || !it->is_object()) return s;
    s.status = it->value("status", std::string("planned"));
    s.versions = it->value("versions", std::string());
    s.note = it->value("note", std::string());
    s.providedBy = it->value("providedBy", std::string("module"));
    return s;
}

} // namespace

bool ModuleDefinitions::load(const std::filesystem::path& file, std::string* error) {
    std::ifstream f(file, std::ios::binary);
    if (!f) {
        if (error) *error = "fichier introuvable";
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    Json j = Json::parse(ss.str(), nullptr, false, true);
    if (j.is_discarded() || !j.is_object() || j.value("format", std::string()) != "umc-modules" || !j.contains("modules") ||
        !j["modules"].is_array()) {
        if (error) *error = "format umc-modules invalide";
        return false;
    }
    std::vector<ModuleDefinition> out;
    for (auto& m : j["modules"]) {
        if (!m.is_object() || !m.contains("id") || !m["id"].is_string()) continue;
        ModuleDefinition d;
        d.id = m["id"].get<std::string>();
        d.category = m.value("category", std::string());
        d.name = localized(m, "name");
        d.description = localized(m, "description");
        if (auto rc = m.find("requiredCapabilities"); rc != m.end() && rc->is_array())
            for (auto& c : *rc)
                if (c.is_string()) d.requiredCapabilities.push_back(c.get<std::string>());
        if (auto ed = m.find("editions"); ed != m.end() && ed->is_object()) {
            d.java = readStatus(*ed, "java");
            d.bedrock = readStatus(*ed, "bedrock");
        }
        out.push_back(std::move(d));
    }
    modules_ = std::move(out);
    return true;
}

const ModuleDefinition* ModuleDefinitions::find(std::string_view id) const {
    for (auto& m : modules_)
        if (m.id == id) return &m;
    return nullptr;
}

ModuleDefinitions& ModuleDefinitions::global() {
    static ModuleDefinitions instance;
    return instance;
}

} // namespace bcc
