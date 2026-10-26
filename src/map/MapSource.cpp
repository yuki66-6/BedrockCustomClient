#include "map/MapSource.h"

#include "map/CustomMapSource.h"
#include "map/WorldScanner.h"

#include <windows.h>

#include <chrono>

namespace bcc::map {

std::unique_ptr<WorldMapResult> MapSource::buildMap(const WorldInfo& world, int dimension, const BlockColors& colors) {
    return WorldScanner::scan(world, colors, dimension);
}

std::vector<WorldInfo> BedrockSaveSource::listWorlds_() { return map::listWorlds(); }

std::string BedrockSaveSource::emptyMessage() const {
    return "Aucun monde solo trouvé dans %APPDATA%\\Minecraft Bedrock. Pour un Realm : Paramètres du Realm → "
           "Télécharger le monde, puis il apparaîtra ici.";
}

int64_t lastDbWrite(const WorldInfo& world) {
    int64_t newest = 0;
    std::error_code ec;
    for (auto& f : std::filesystem::directory_iterator(world.dir / L"db", ec)) {
        auto t = f.last_write_time(ec);
        if (ec) continue;
        auto sys = std::chrono::clock_cast<std::chrono::system_clock>(t);
        newest = std::max<int64_t>(newest, std::chrono::duration_cast<std::chrono::seconds>(sys.time_since_epoch()).count());
    }
    return newest;
}

// Monde « actuel » : sans toucher aux fichiers du jeu (aucun verrou sondé), on
// prend celui dont la base a été écrite le plus récemment — pendant une partie,
// Minecraft écrit régulièrement dans le dossier db du monde ouvert.
int BedrockSaveSource::currentWorldIndex(const std::vector<WorldInfo>& worlds, bool gameRunning) {
    int best = -1;
    int64_t bestTime = 0;
    for (size_t i = 0; i < worlds.size(); ++i) {
        int64_t t = lastDbWrite(worlds[i]);
        if (t > bestTime) {
            bestTime = t;
            best = static_cast<int>(i);
        }
    }
    (void)gameRunning; // même règle avec ou sans jeu lancé : le dernier monde écrit
    return best;
}

std::vector<std::unique_ptr<MapSource>> createMapSources() {
    std::vector<std::unique_ptr<MapSource>> sources;
    sources.push_back(std::make_unique<BedrockSaveSource>());
    sources.push_back(std::make_unique<CustomMapSource>());
    return sources;
}

} // namespace bcc::map
