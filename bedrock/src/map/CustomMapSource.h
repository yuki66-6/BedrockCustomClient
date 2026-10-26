#pragma once

#include "map/MapSource.h"

namespace bcc::map {

// ============================================================================
//  TA SOURCE DE CARTE — à compléter (voir docs/EXTENDING.md, section 7).
//
//  Elle apparaît dans la page Carte sous le nom renvoyé par name(). Deux façons
//  de l'utiliser :
//   1. listWorlds() renvoie des dossiers au format Bedrock (db/ + level.dat),
//      par exemple des mondes de Realm téléchargés rangés ailleurs : la carte
//      les lit automatiquement (buildMap par défaut).
//   2. buildMap() construit toi-même un WorldMapResult (régions de pixels,
//      position du joueur...) à partir de n'importe quelle donnée.
// ============================================================================
class CustomMapSource : public MapSource {
public:
    const char* name() const override { return "Source personnalisée"; }
    std::vector<WorldInfo> listWorlds() override;
    std::string emptyMessage() const override;
    // Décommente pour fournir tes propres données de carte :
    // std::unique_ptr<WorldMapResult> buildMap(const WorldInfo& world, int dimension,
    //                                          const BlockColors& colors) override;
};

} // namespace bcc::map
