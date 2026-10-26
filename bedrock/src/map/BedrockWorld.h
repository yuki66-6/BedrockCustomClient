#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Format de sauvegarde des mondes Minecraft Bedrock (lecture seule).
namespace bcc::map {

struct WorldInfo {
    std::filesystem::path dir;  // dossier du monde (contient db/ et level.dat)
    std::string folderId;
    std::string name;
    int64_t lastPlayed = 0;     // horodatage Unix (secondes)
    uint64_t sizeBytes = 0;
    bool hasSpawn = false;
    int spawnX = 0, spawnY = 0, spawnZ = 0;
};

// Dossiers « minecraftWorlds » de l'installation actuelle (GDK) et de l'ancienne (UWP).
std::vector<std::filesystem::path> worldRoots();
std::vector<WorldInfo> listWorlds();
bool readWorldInfo(const std::filesystem::path& dir, WorldInfo& out);

// Clé de chunk : x, z (int32), dimension optionnelle, tag, index de sous-chunk.
struct ChunkKey {
    int32_t x = 0, z = 0, dimension = 0;
    uint8_t tag = 0;
    int8_t subY = 0;
};
bool parseChunkKey(std::string_view key, ChunkKey& out);

enum ChunkTag : uint8_t {
    TagData3D = 43,   // carte des hauteurs + biomes 3D (1.18+)
    TagVersion = 44,
    TagData2D = 45,   // ancienne carte des hauteurs + biomes 2D
    TagSubChunk = 47,
};

// Couche 0 d'un sous-chunk 16x16x16 avec sa palette de blocs.
struct SubChunk {
    int8_t y = 0;
    int bitsPerBlock = 0;
    std::vector<uint32_t> words;
    std::vector<std::string> palette; // noms (« minecraft:stone »...)
    // Index de palette du bloc local (x, y, z) dans [0,16).
    uint32_t paletteIndex(int x, int ly, int z) const;
    const std::string& blockName(int x, int ly, int z) const;
};
bool decodeSubChunk(std::string_view value, int8_t keyY, SubChunk& out);

// Carte des hauteurs (256 valeurs) depuis Data3D/Data2D. Index = (z << 4) | x.
bool decodeHeightMap(std::string_view value, int16_t out[256]);

struct PlayerPos {
    float x = 0, y = 0, z = 0;
    int dimension = 0;
};
std::optional<PlayerPos> decodeLocalPlayer(std::string_view value);

} // namespace bcc::map
