#pragma once

#include "map/BedrockWorld.h"
#include "map/BlockColors.h"

#include <memory>
#include <string>
#include <vector>

namespace bcc::map {

struct WorldMapResult;

// Source de mondes pour la page Carte. La source par défaut lit les sauvegardes
// locales ; d'autres sources peuvent être ajoutées (voir CustomMapSource).
class MapSource {
public:
    virtual ~MapSource() = default;
    virtual const char* name() const = 0;
    virtual std::vector<WorldInfo> listWorlds() = 0;
    // Index du monde « actuel » dans la liste (-1 si inconnu).
    virtual int currentWorldIndex(const std::vector<WorldInfo>& worlds, bool gameRunning) { return -1; }
    // Message affiché quand la source n'a aucun monde à proposer.
    virtual std::string emptyMessage() const { return "Aucun monde trouvé."; }
    // Construit la carte d'un monde. Par défaut : lecture d'un dossier au format
    // Bedrock (db/ LevelDB). Une source peut fournir ses propres données.
    virtual std::unique_ptr<WorldMapResult> buildMap(const WorldInfo& world, int dimension, const BlockColors& colors);
};

// Mondes solo de cette installation de Minecraft (sauvegardes locales, lecture seule).
class BedrockSaveSource : public MapSource {
public:
    const char* name() const override { return "Mondes locaux"; }
    std::vector<WorldInfo> listWorlds() override { return listWorlds_(); }
    int currentWorldIndex(const std::vector<WorldInfo>& worlds, bool gameRunning) override;
    std::string emptyMessage() const override;

private:
    static std::vector<WorldInfo> listWorlds_();
};

// Dernière modification d'un fichier du dossier db (Minecraft y écrit pendant la partie).
int64_t lastDbWrite(const WorldInfo& world);

// Toutes les sources disponibles (locale + la tienne, à compléter).
std::vector<std::unique_ptr<MapSource>> createMapSources();

} // namespace bcc::map
