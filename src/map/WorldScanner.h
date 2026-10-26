#pragma once

#include "map/BedrockWorld.h"
#include "map/BlockColors.h"
#include "map/LevelDb.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace bcc::map {

constexpr int kRegionChunks = 32;                 // chunks par côté de région
constexpr int kRegionPixels = kRegionChunks * 16; // 1 pixel = 1 bloc

// Résultat d'un scan : images RGBA (0 = zone non explorée) par région.
const char* dimensionName(int dimension); // 0 Overworld, 1 Nether, 2 End

struct WorldMapResult {
    WorldInfo world;
    int dimension = 0;
    std::map<std::pair<int, int>, std::vector<uint32_t>> regions;
    int chunks = 0;
    int minBlockX = 0, maxBlockX = 0, minBlockZ = 0, maxBlockZ = 0;
    bool hasPlayer = false;
    PlayerPos player;
    uint64_t columnsTotal = 0, columnsResolved = 0;
    ldb::ScanStats stats;
    double seconds = 0;
    std::string error;
};

// Scan d'un monde en arrière-plan (lecture seule).
class WorldScanner {
public:
    ~WorldScanner() { cancel(); }

    void start(const WorldInfo& world, const BlockColors& colors, int dimension = 0);
    void cancel();
    bool running() const { return running_; }
    float progress() const { return progress_; }
    std::string stage() const;
    // Récupère le résultat une fois terminé (nullptr sinon).
    std::unique_ptr<WorldMapResult> take();

    // Exécution synchrone (tests).
    static std::unique_ptr<WorldMapResult> scan(const WorldInfo& world, const BlockColors& colors, int dimension = 0,
                                                const std::atomic<bool>* cancel = nullptr,
                                                const std::function<void(float, const char*)>& progress = {});

private:
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<float> progress_{0.f};
    mutable std::mutex mutex_;
    std::string stage_;
    std::unique_ptr<WorldMapResult> result_;
};

} // namespace bcc::map
