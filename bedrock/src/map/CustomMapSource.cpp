#include "map/CustomMapSource.h"

namespace bcc::map {

std::vector<WorldInfo> CustomMapSource::listWorlds() {
    std::vector<WorldInfo> worlds;
    // TODO (toi) : ajoute ici tes mondes. Exemple avec un dossier de mondes de
    // Realm téléchargés :
    //
    //   for (auto& dir : std::filesystem::directory_iterator(L"D:\\MesRealms")) {
    //       WorldInfo info;
    //       if (readWorldInfo(dir.path(), info)) worlds.push_back(info);
    //   }
    return worlds;
}

std::string CustomMapSource::emptyMessage() const {
    return "Source à compléter : ouvre src/map/CustomMapSource.cpp (voir docs/EXTENDING.md).";
}

// Exemple de carte fournie entièrement par toi (décommente aussi la déclaration) :
//
// std::unique_ptr<WorldMapResult> CustomMapSource::buildMap(const WorldInfo& world, int dimension,
//                                                           const BlockColors& colors) {
//     auto r = std::make_unique<WorldMapResult>();
//     r->world = world;
//     r->dimension = dimension;
//     auto& pixels = r->regions[{0, 0}];                  // région (0,0) = blocs 0..511
//     pixels.assign(kRegionPixels * kRegionPixels, 0u);   // 0 = non découvert
//     pixels[10 * kRegionPixels + 20] = BlockColors::rgba(255, 0, 0); // bloc (x=20, z=10) en rouge
//     r->chunks = 1;
//     r->maxBlockX = r->maxBlockZ = kRegionPixels;
//     return r;
// }

} // namespace bcc::map
