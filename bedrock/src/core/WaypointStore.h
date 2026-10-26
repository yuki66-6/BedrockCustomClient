#pragma once

#include "core/ConfigManager.h"
#include "core/Json.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace bcc {

struct Waypoint {
    std::string id;
    std::string name;
    int x = 0, y = 64, z = 0;
    std::string dimension = "overworld";
    std::string category = "other";
    std::string color = "#458AFF";
    std::string icon = "pin";
    bool visible = true;
    long long createdAt = 0;
    std::string source = "user";
};

struct WaypointWorld {
    std::string name;
    std::string edition = "bedrock";
    std::string kind = "singleplayer";
    std::vector<Waypoint> waypoints;
};

struct WaypointCategory {
    std::string id, name, color;
};

// Waypoints au format commun Java/Bedrock (shared/schemas/waypoints.schema.json),
// classés par monde/serveur puis par dimension.
class WaypointStore {
public:
    static constexpr const char* kGlobalWorld = "bedrock:global"; // visibles dans tous les mondes Bedrock

    explicit WaypointStore(std::filesystem::path file) : file_(std::move(file)) {}

    OpResult load();
    OpResult save() const;

    const std::map<std::string, WaypointWorld>& worlds() const { return worlds_; }
    const std::vector<WaypointCategory>& categories() const { return categories_; }
    std::vector<Waypoint> forWorld(const std::string& worldKey, bool includeGlobal = true) const;

    Waypoint& add(const std::string& worldKey, const std::string& worldName, Waypoint wp);
    bool remove(const std::string& worldKey, const std::string& id);

    OpResult exportTo(const std::filesystem::path& file) const;
    // Fusionne un fichier umc-waypoints ; ignore les identifiants déjà présents.
    OpResult importFrom(const std::filesystem::path& file, int* added = nullptr);
    // Ancien fichier de repères de la carte (map-extensions/waypoints.json, v1.0).
    int migrateLegacyMarkers(const std::filesystem::path& legacyFile);

    static Json toJson(const std::map<std::string, WaypointWorld>& worlds, const std::vector<WaypointCategory>& categories);
    static bool fromJson(const Json& j, std::map<std::string, WaypointWorld>& worlds, std::vector<WaypointCategory>& categories,
                         std::string* error = nullptr);
    static std::string newId();
    // « overworld » ↔ « minecraft:overworld », « nether » ↔ « minecraft:the_nether »…
    static std::string normalizeDimension(const std::string& d);
    static int dimensionIndex(const std::string& d); // 0, 1, 2 (-1 inconnu)

    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path file_;
    std::map<std::string, WaypointWorld> worlds_;
    std::vector<WaypointCategory> categories_;
};

} // namespace bcc
