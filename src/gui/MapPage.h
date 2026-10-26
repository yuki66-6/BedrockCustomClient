#pragma once

#include "map/BlockColors.h"
#include "map/MapExtensions.h"
#include "map/MapLayers.h"
#include "map/MapSource.h"

#include <imgui.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace bcc {

class TextureManager;
class Notifications;
namespace map {
struct WorldMapResult;
}

// Page « Carte » : carte de tes mondes à partir de leurs sauvegardes (lecture
// seule), en mode découverte, avec dimensions, calques et extensions.
class MapPage {
public:
    MapPage(TextureManager* textures, Notifications* notify);
    ~MapPage();

    void draw(double now, bool gameRunning);
    bool fullscreen() const { return fullscreen_; }

    // Accès pour les tests.
    void selectWorld(int index) { requestedWorld_ = index; }
    bool busy() const { return jobRunning_; }
    const map::WorldMapResult* currentMap() const { return result_.get(); }
    void refreshNow() { forceScan_ = true; }
    size_t worldCount() const { return worlds_.size(); }

private:
    void refreshWorldList(bool gameRunning);
    void startScan();
    void finishScan();
    void uploadTextures();
    void drawToolbar(bool gameRunning);
    void drawCanvas(double now);
    void drawStatus();
    void drawWaypointPopup();
    void centerOnDefault();
    void reloadExtensions();

    TextureManager* textures_;
    Notifications* notify_;
    std::vector<std::unique_ptr<map::MapSource>> sources_;
    int sourceIndex_ = 0;
    std::vector<map::WorldInfo> worlds_;
    int worldChoice_ = -1;      // -1 = « monde actuel (auto) »
    int requestedWorld_ = -2;   // demande externe (tests)
    int activeWorld_ = -1;      // monde effectivement affiché
    int dimension_ = 0;
    double lastWorldList_ = -100;
    double lastScan_ = -100;
    bool autoRefresh_ = true;
    bool forceScan_ = true;
    bool fullscreen_ = false;

    // Travail en arrière-plan
    std::thread job_;
    std::atomic<bool> jobRunning_{false};
    std::atomic<float> jobProgress_{0.f};
    std::mutex jobMutex_;
    std::string jobStage_;
    std::unique_ptr<map::WorldMapResult> jobResult_;
    std::unique_ptr<map::WorldMapResult> result_;
    std::map<std::pair<int, int>, ImTextureID> tiles_;
    bool keepViewOnNextResult_ = false;

    // Vue
    ImVec2 center_{0, 0};
    float zoom_ = 1.f, targetZoom_ = 1.f;
    ImVec2 lastMouseWorld_{0, 0};
    bool mouseInside_ = false;

    // Calques et extensions
    map::BlockColors colors_;
    map::MapExtensions extensions_;
    std::vector<std::unique_ptr<map::MapLayer>> layers_;
    std::map<std::string, bool> layerEnabled_;
    std::vector<std::string> extensionErrors_;

    // Ajout de repère
    ImVec2 popupWorld_{0, 0};
    char waypointName_[48] = {};
    int waypointIcon_ = 0;
};

} // namespace bcc
