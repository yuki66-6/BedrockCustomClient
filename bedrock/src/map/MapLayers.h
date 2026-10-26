#pragma once

#include "map/MapExtensions.h"

#include <imgui.h>

#include <memory>
#include <vector>

namespace bcc::map {

struct WorldMapResult;
class MapExtensions;

// Contexte de dessin d'un calque de la page Carte.
struct MapDrawContext {
    ImDrawList* drawList = nullptr;
    ImVec2 canvasMin{0, 0}, canvasMax{0, 0};
    ImVec2 center{0, 0};   // coordonnées monde (x, z) au centre du canevas
    float zoom = 1.f;      // pixels par bloc
    float uiScale = 1.f;
    int dimension = 0;
    const WorldMapResult* world = nullptr;  // peut être nul (aucun monde chargé)
    const MapExtensions* extensions = nullptr;
    const std::vector<MapMarker>* userMarkers = nullptr; // waypoints du monde (format commun)
    ImVec2 mouseWorld{0, 0};
    bool mouseInside = false;

    ImVec2 worldToScreen(float x, float z) const {
        return {(canvasMin.x + canvasMax.x) * 0.5f + (x - center.x) * zoom,
                (canvasMin.y + canvasMax.y) * 0.5f + (z - center.y) * zoom};
    }
    ImVec2 screenToWorld(ImVec2 p) const {
        return {center.x + (p.x - (canvasMin.x + canvasMax.x) * 0.5f) / zoom,
                center.y + (p.y - (canvasMin.y + canvasMax.y) * 0.5f) / zoom};
    }
    ImVec2 viewMin() const { return screenToWorld(canvasMin); }
    ImVec2 viewMax() const { return screenToWorld(canvasMax); }
};

// Calque de carte : élément dessiné par-dessus les tuiles (grille, repères...).
class MapLayer {
public:
    virtual ~MapLayer() = default;
    virtual const char* id() const = 0;
    virtual const char* name() const = 0;
    virtual bool enabledByDefault() const { return true; }
    virtual void draw(MapDrawContext& ctx) = 0;
};

// Calques fournis par défaut. Pour en ajouter un : voir docs/EXTENDING.md.
void registerDefaultMapLayers(std::vector<std::unique_ptr<MapLayer>>& layers);

} // namespace bcc::map
