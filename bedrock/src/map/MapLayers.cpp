#include "map/MapLayers.h"

#include "gui/Icons.h"
#include "map/MapExtensions.h"
#include "map/WorldScanner.h"

#include <cmath>
#include <format>

namespace bcc::map {
namespace {

ImU32 fromRgba(uint32_t c, float alphaMul = 1.f) {
    return IM_COL32(c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff, static_cast<int>(((c >> 24) & 0xff) * alphaMul));
}

void label(ImDrawList* dl, ImVec2 p, const char* text, ImU32 color, float fontScale = 0.85f) {
    float fs = ImGui::GetFontSize() * fontScale;
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.f, text);
    ImVec2 a{p.x - ts.x * 0.5f - 5.f, p.y}, b{p.x + ts.x * 0.5f + 5.f, p.y + ts.y + 3.f};
    dl->AddRectFilled(a, b, IM_COL32(8, 12, 22, 200), 4.f);
    dl->AddText(ImGui::GetFont(), fs, {a.x + 5.f, a.y + 1.5f}, color, text);
}

// Grille : chunks (16 blocs) quand on zoome, régions (512 blocs) sinon, et axes.
class GridLayer : public MapLayer {
public:
    const char* id() const override { return "grid"; }
    const char* name() const override { return "Grille"; }
    void draw(MapDrawContext& c) override {
        float step = c.zoom >= 1.5f ? 16.f : (c.zoom >= 0.18f ? 128.f : 512.f);
        ImVec2 vmin = c.viewMin(), vmax = c.viewMax();
        ImU32 thin = IM_COL32(255, 255, 255, c.zoom >= 1.5f ? 18 : 24);
        for (float x = std::floor(vmin.x / step) * step; x <= vmax.x; x += step) {
            float sx = c.worldToScreen(x, 0).x;
            c.drawList->AddLine({sx, c.canvasMin.y}, {sx, c.canvasMax.y}, x == 0 ? IM_COL32(255, 80, 80, 90) : thin);
        }
        for (float z = std::floor(vmin.y / step) * step; z <= vmax.y; z += step) {
            float sy = c.worldToScreen(0, z).y;
            c.drawList->AddLine({c.canvasMin.x, sy}, {c.canvasMax.x, sy}, z == 0 ? IM_COL32(80, 140, 255, 90) : thin);
        }
    }
};

class AreasLayer : public MapLayer {
public:
    const char* id() const override { return "areas"; }
    const char* name() const override { return "Zones (extensions)"; }
    void draw(MapDrawContext& c) override {
        if (!c.extensions) return;
        for (auto& a : c.extensions->areas()) {
            if (a.dimension != c.dimension) continue;
            ImVec2 p0 = c.worldToScreen(static_cast<float>(std::min(a.x1, a.x2)), static_cast<float>(std::min(a.z1, a.z2)));
            ImVec2 p1 = c.worldToScreen(static_cast<float>(std::max(a.x1, a.x2) + 1), static_cast<float>(std::max(a.z1, a.z2) + 1));
            c.drawList->AddRectFilled(p0, p1, fromRgba(a.color, 0.6f));
            c.drawList->AddRect(p0, p1, fromRgba(a.color | 0xff000000u), 0.f, 1.5f);
            if (p1.x - p0.x > 40.f) label(c.drawList, {(p0.x + p1.x) * 0.5f, p0.y + 4.f}, a.label.c_str(), IM_COL32_WHITE);
        }
    }
};

class MarkersLayer : public MapLayer {
public:
    const char* id() const override { return "markers"; }
    const char* name() const override { return "Repères"; }
    void draw(MapDrawContext& c) override {
        float s = 20.f * c.uiScale;
        std::vector<const MapMarker*> all;
        if (c.extensions)
            for (auto& m : c.extensions->markers()) all.push_back(&m);
        if (c.userMarkers)
            for (auto& m : *c.userMarkers) all.push_back(&m);
        for (const MapMarker* mp : all) {
            const MapMarker& m = *mp;
            if (m.dimension != c.dimension) continue;
            ImVec2 p = c.worldToScreen(static_cast<float>(m.x) + 0.5f, static_cast<float>(m.z) + 0.5f);
            if (p.x < c.canvasMin.x - s || p.x > c.canvasMax.x + s || p.y < c.canvasMin.y - s || p.y > c.canvasMax.y + s) continue;
            c.drawList->AddCircleFilled(p, s * 0.62f, IM_COL32(8, 12, 22, 210));
            ui::drawIcon(c.drawList, m.icon, p, s * 0.75f, fromRgba(m.color | 0xff000000u));
            bool hover = c.mouseInside && std::fabs(c.worldToScreen(c.mouseWorld.x, c.mouseWorld.y).x - p.x) < s * 0.6f &&
                         std::fabs(c.worldToScreen(c.mouseWorld.x, c.mouseWorld.y).y - p.y) < s * 0.6f;
            if (c.zoom >= 0.5f || hover) {
                std::string text = hover ? std::format("{} ({}, {})", m.label, m.x, m.z) : m.label;
                label(c.drawList, {p.x, p.y + s * 0.65f}, text.c_str(), IM_COL32_WHITE);
            }
        }
    }
};

class SpawnLayer : public MapLayer {
public:
    const char* id() const override { return "spawn"; }
    const char* name() const override { return "Point d'apparition"; }
    void draw(MapDrawContext& c) override {
        if (!c.world || !c.world->world.hasSpawn || c.dimension != 0) return;
        ImVec2 p = c.worldToScreen(static_cast<float>(c.world->world.spawnX) + 0.5f, static_cast<float>(c.world->world.spawnZ) + 0.5f);
        float s = 18.f * c.uiScale;
        c.drawList->AddCircleFilled(p, s * 0.6f, IM_COL32(8, 12, 22, 200));
        ui::drawIcon(c.drawList, Icon::Flag, p, s * 0.7f, IM_COL32(255, 215, 90, 255));
        label(c.drawList, {p.x, p.y + s * 0.65f}, "Apparition", IM_COL32(255, 215, 90, 255));
    }
};

class PlayerLayer : public MapLayer {
public:
    const char* id() const override { return "player"; }
    const char* name() const override { return "Joueur (dernière sauvegarde)"; }
    void draw(MapDrawContext& c) override {
        if (!c.world || !c.world->hasPlayer || c.world->player.dimension != c.dimension) return;
        ImVec2 p = c.worldToScreen(c.world->player.x, c.world->player.z);
        float t = static_cast<float>(ImGui::GetTime());
        float pulse = 0.5f + 0.5f * std::sin(t * 3.f);
        float s = 9.f * c.uiScale;
        c.drawList->AddCircleFilled(p, s * (1.8f + pulse), IM_COL32(70, 140, 255, static_cast<int>(60 * (1.f - pulse) + 20)));
        c.drawList->AddCircleFilled(p, s, IM_COL32(255, 255, 255, 255));
        c.drawList->AddCircleFilled(p, s * 0.7f, IM_COL32(70, 140, 255, 255));
        label(c.drawList, {p.x, p.y + s * 1.4f}, "Toi (sauvegarde)", IM_COL32(170, 205, 255, 255));
    }
};

} // namespace

void registerDefaultMapLayers(std::vector<std::unique_ptr<MapLayer>>& layers) {
    layers.push_back(std::make_unique<GridLayer>());
    layers.push_back(std::make_unique<AreasLayer>());
    layers.push_back(std::make_unique<SpawnLayer>());
    layers.push_back(std::make_unique<MarkersLayer>());
    layers.push_back(std::make_unique<PlayerLayer>());
}

} // namespace bcc::map
