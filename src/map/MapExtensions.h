#pragma once

#include "gui/Icons.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace bcc::map {

class BlockColors;

struct MapMarker {
    std::string label;
    int x = 0, z = 0;
    uint32_t color = 0xFFFF8A45; // RGBA (R en poids faible)
    Icon icon = Icon::Pin;
    int dimension = 0;
    std::string source; // fichier d'origine
};

struct MapArea {
    std::string label;
    int x1 = 0, z1 = 0, x2 = 0, z2 = 0;
    uint32_t color = 0x60FFD040;
    int dimension = 0;
    std::string source;
};

// Extensions de carte : fichiers JSON du dossier « map-extensions »
// (repères, zones, couleurs de blocs d'add-ons/mods). Voir docs/EXTENDING.md.
class MapExtensions {
public:
    static std::filesystem::path folder();
    static constexpr const char* kWaypointsFile = "waypoints.json";

    // Charge tous les fichiers ; applique les couleurs de blocs à `colors`.
    // Retourne la liste des erreurs (fichier : message).
    std::vector<std::string> load(BlockColors& colors, const std::filesystem::path& dir = folder());

    const std::vector<MapMarker>& markers() const { return markers_; }
    const std::vector<MapArea>& areas() const { return areas_; }
    const std::vector<std::string>& files() const { return files_; }

    // Repères ajoutés depuis la carte (enregistrés dans waypoints.json).
    bool addWaypoint(const std::string& label, int x, int z, int dimension, Icon icon, uint32_t color,
                     std::string* error = nullptr, const std::filesystem::path& dir = folder());
    bool removeWaypoint(const std::string& label, int x, int z, std::string* error = nullptr,
                        const std::filesystem::path& dir = folder());

    static Icon iconFromName(const std::string& name);
    static const char* iconName(Icon icon);
    static std::string colorToHex(uint32_t rgba);

private:
    std::vector<MapMarker> markers_;
    std::vector<MapArea> areas_;
    std::vector<std::string> files_;
};

} // namespace bcc::map
