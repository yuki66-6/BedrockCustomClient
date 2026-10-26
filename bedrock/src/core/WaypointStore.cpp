#include "core/WaypointStore.h"

#include "utils/Log.h"
#include "utils/Strings.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <random>
#include <sstream>

namespace bcc {
namespace {

std::vector<WaypointCategory> defaultCategories() {
    return {{"base", "Base", "#458AFF"},
            {"resource", "Ressources", "#4DDB85"},
            {"danger", "Danger", "#F2525A"},
            {"death", "Morts", "#9EA3B3"},
            {"other", "Autre", "#FFB84D"}};
}

bool readJson(const std::filesystem::path& file, Json& out, std::string& err) {
    std::ifstream f(file, std::ios::binary);
    if (!f) {
        err = "fichier illisible";
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    out = Json::parse(ss.str(), nullptr, false, true);
    if (out.is_discarded()) {
        err = "JSON invalide";
        return false;
    }
    return true;
}

OpResult writeAtomic(const std::filesystem::path& file, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    auto tmp = file;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return {false, "écriture impossible"};
        f << text;
    }
    if (!MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return {false, "remplacement impossible : " + win32ErrorMessage(GetLastError())};
    return {};
}

} // namespace

std::string WaypointStore::newId() {
    static std::mt19937_64 rng{std::random_device{}()};
    return std::format("{:016x}", rng());
}

std::string WaypointStore::normalizeDimension(const std::string& d) {
    if (d == "minecraft:overworld" || d == "overworld" || d == "0") return "overworld";
    if (d == "minecraft:the_nether" || d == "nether" || d == "the_nether" || d == "1") return "nether";
    if (d == "minecraft:the_end" || d == "end" || d == "the_end" || d == "2") return "end";
    return d;
}

int WaypointStore::dimensionIndex(const std::string& d) {
    std::string n = normalizeDimension(d);
    return n == "overworld" ? 0 : n == "nether" ? 1 : n == "end" ? 2 : -1;
}

Json WaypointStore::toJson(const std::map<std::string, WaypointWorld>& worlds, const std::vector<WaypointCategory>& categories) {
    Json cats = Json::array();
    for (auto& c : categories) cats.push_back({{"id", c.id}, {"name", c.name}, {"color", c.color}});
    Json ws = Json::object();
    for (auto& [key, w] : worlds) {
        Json list = Json::array();
        for (auto& p : w.waypoints)
            list.push_back({{"id", p.id}, {"name", p.name}, {"x", p.x}, {"y", p.y}, {"z", p.z}, {"dimension", p.dimension},
                            {"category", p.category}, {"color", p.color}, {"icon", p.icon}, {"visible", p.visible},
                            {"createdAt", p.createdAt}, {"source", p.source}});
        ws[key] = {{"name", w.name}, {"edition", w.edition}, {"kind", w.kind}, {"waypoints", list}};
    }
    return {{"format", "umc-waypoints"}, {"formatVersion", 1}, {"categories", cats}, {"worlds", ws}};
}

bool WaypointStore::fromJson(const Json& j, std::map<std::string, WaypointWorld>& worlds, std::vector<WaypointCategory>& categories,
                             std::string* error) {
    if (!j.is_object() || j.value("format", std::string()) != "umc-waypoints" || !j.contains("worlds") || !j["worlds"].is_object()) {
        if (error) *error = "ce n'est pas un fichier umc-waypoints";
        return false;
    }
    categories.clear();
    if (auto it = j.find("categories"); it != j.end() && it->is_array())
        for (auto& c : *it)
            if (c.is_object() && c.contains("id"))
                categories.push_back({c.value("id", std::string()), c.value("name", std::string()), c.value("color", std::string("#FFB84D"))});
    if (categories.empty()) categories = defaultCategories();
    worlds.clear();
    for (auto& [key, w] : j["worlds"].items()) {
        if (!w.is_object()) continue;
        WaypointWorld world;
        world.name = w.value("name", key);
        world.edition = w.value("edition", std::string("bedrock"));
        world.kind = w.value("kind", std::string("unknown"));
        if (auto list = w.find("waypoints"); list != w.end() && list->is_array()) {
            for (auto& p : *list) {
                if (!p.is_object() || !p.contains("x") || !p.contains("z") || !p["x"].is_number() || !p["z"].is_number()) continue;
                Waypoint wp;
                wp.id = p.value("id", newId());
                wp.name = p.value("name", std::string("Waypoint"));
                wp.x = p["x"].get<int>();
                wp.y = p.value("y", 64);
                wp.z = p["z"].get<int>();
                wp.dimension = normalizeDimension(p.value("dimension", std::string("overworld")));
                wp.category = p.value("category", std::string("other"));
                wp.color = p.value("color", std::string("#458AFF"));
                wp.icon = p.value("icon", std::string("pin"));
                wp.visible = p.value("visible", true);
                wp.createdAt = p.value("createdAt", 0LL);
                wp.source = p.value("source", std::string("user"));
                world.waypoints.push_back(std::move(wp));
            }
        }
        worlds[key] = std::move(world);
    }
    return true;
}

OpResult WaypointStore::load() {
    std::error_code ec;
    if (!std::filesystem::exists(file_, ec)) {
        worlds_.clear();
        categories_ = defaultCategories();
        return {true, "aucun waypoint"};
    }
    Json j;
    std::string err;
    if (!readJson(file_, j, err) || !fromJson(j, worlds_, categories_, &err)) {
        log::error("Waypoints {} : {}", toUtf8(file_.wstring()), err);
        categories_ = defaultCategories();
        return {false, "Waypoints illisibles : " + err};
    }
    return {};
}

OpResult WaypointStore::save() const { return writeAtomic(file_, toJson(worlds_, categories_).dump(2)); }

std::vector<Waypoint> WaypointStore::forWorld(const std::string& worldKey, bool includeGlobal) const {
    std::vector<Waypoint> out;
    if (auto it = worlds_.find(worldKey); it != worlds_.end()) out = it->second.waypoints;
    if (includeGlobal && worldKey != kGlobalWorld)
        if (auto it = worlds_.find(kGlobalWorld); it != worlds_.end())
            out.insert(out.end(), it->second.waypoints.begin(), it->second.waypoints.end());
    return out;
}

Waypoint& WaypointStore::add(const std::string& worldKey, const std::string& worldName, Waypoint wp) {
    WaypointWorld& w = worlds_[worldKey];
    if (w.name.empty()) w.name = worldName;
    if (wp.id.empty()) wp.id = newId();
    if (!wp.createdAt)
        wp.createdAt = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    wp.dimension = normalizeDimension(wp.dimension);
    w.waypoints.push_back(std::move(wp));
    return w.waypoints.back();
}

bool WaypointStore::remove(const std::string& worldKey, const std::string& id) {
    auto it = worlds_.find(worldKey);
    if (it == worlds_.end()) return false;
    auto& list = it->second.waypoints;
    auto n = std::erase_if(list, [&](const Waypoint& w) { return w.id == id; });
    return n > 0;
}

OpResult WaypointStore::exportTo(const std::filesystem::path& file) const {
    OpResult r = writeAtomic(file, toJson(worlds_, categories_).dump(2));
    if (r.ok) r.message = "Waypoints exportés vers " + toUtf8(file.filename().wstring());
    return r;
}

OpResult WaypointStore::importFrom(const std::filesystem::path& file, int* added) {
    Json j;
    std::string err;
    std::map<std::string, WaypointWorld> incoming;
    std::vector<WaypointCategory> cats;
    if (!readJson(file, j, err) || !fromJson(j, incoming, cats, &err)) return {false, "Import impossible : " + err};
    int count = 0;
    for (auto& [key, w] : incoming) {
        WaypointWorld& dst = worlds_[key];
        if (dst.name.empty()) {
            dst.name = w.name;
            dst.edition = w.edition;
            dst.kind = w.kind;
        }
        for (auto& wp : w.waypoints) {
            bool exists = std::any_of(dst.waypoints.begin(), dst.waypoints.end(), [&](const Waypoint& o) { return o.id == wp.id; });
            if (!exists) {
                Waypoint copy = wp;
                if (copy.source == "user") copy.source = "import";
                dst.waypoints.push_back(copy);
                ++count;
            }
        }
    }
    for (auto& c : cats)
        if (std::none_of(categories_.begin(), categories_.end(), [&](const WaypointCategory& o) { return o.id == c.id; }))
            categories_.push_back(c);
    if (added) *added = count;
    return {true, std::format("{} waypoint(s) importé(s)", count)};
}

int WaypointStore::migrateLegacyMarkers(const std::filesystem::path& legacyFile) {
    std::error_code ec;
    if (!std::filesystem::exists(legacyFile, ec)) return 0;
    Json j;
    std::string err;
    if (!readJson(legacyFile, j, err) || !j.is_object() || !j.contains("markers") || !j["markers"].is_array()) return 0;
    int count = 0;
    for (auto& m : j["markers"]) {
        if (!m.is_object() || !m.contains("x") || !m.contains("z")) continue;
        Waypoint wp;
        wp.name = m.value("label", std::string("Repère"));
        wp.x = m.value("x", 0);
        wp.z = m.value("z", 0);
        wp.dimension = normalizeDimension(m.value("dimension", std::string("overworld")));
        wp.icon = m.value("icon", std::string("pin"));
        wp.color = m.value("color", std::string("#458AFF"));
        wp.source = "import";
        add(kGlobalWorld, "Tous les mondes Bedrock", wp);
        ++count;
    }
    auto done = legacyFile;
    done += L".migrated";
    std::filesystem::rename(legacyFile, done, ec);
    log::info("{} repère(s) de l'ancienne carte migrés vers le format commun", count);
    return count;
}

} // namespace bcc
