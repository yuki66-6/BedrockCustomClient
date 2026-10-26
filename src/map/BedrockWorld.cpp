#include "map/BedrockWorld.h"

#include "map/LevelDb.h"
#include "map/Nbt.h"
#include "utils/Strings.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace bcc::map {
namespace {

std::filesystem::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::filesystem::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) out = p;
    if (p) CoTaskMemFree(p);
    return out;
}

} // namespace

std::vector<std::filesystem::path> worldRoots() {
    std::vector<std::filesystem::path> roots;
    std::error_code ec;
    // Installation GDK (2023+) : %APPDATA%\Minecraft Bedrock\Users\<id>\games\com.mojang\minecraftWorlds
    auto users = knownFolder(FOLDERID_RoamingAppData) / L"Minecraft Bedrock" / L"Users";
    for (auto& u : std::filesystem::directory_iterator(users, ec)) {
        auto w = u.path() / L"games" / L"com.mojang" / L"minecraftWorlds";
        if (std::filesystem::is_directory(w, ec)) roots.push_back(w);
    }
    // Ancienne installation UWP.
    auto uwp = knownFolder(FOLDERID_LocalAppData) / L"Packages" / L"Microsoft.MinecraftUWP_8wekyb3d8bbwe" / L"LocalState" /
               L"games" / L"com.mojang" / L"minecraftWorlds";
    if (std::filesystem::is_directory(uwp, ec)) roots.push_back(uwp);
    return roots;
}

bool readWorldInfo(const std::filesystem::path& dir, WorldInfo& out) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir / L"db", ec)) return false;
    out.dir = dir;
    out.folderId = toUtf8(dir.filename().wstring());
    std::string text;
    if (ldb::readFileShared(dir / L"levelname.txt", text)) {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        out.name = text;
    }
    std::string levelDat;
    if (ldb::readFileShared(dir / L"level.dat", levelDat) && levelDat.size() > 8) {
        std::string_view in(levelDat);
        in.remove_prefix(8); // en-tête : version + longueur
        std::string rootName;
        nbt::Value root;
        if (nbt::readNamed(in, rootName, root)) {
            if (out.name.empty()) out.name = root.getString("LevelName");
            out.lastPlayed = root.getInt("LastPlayed");
            if (root.find("SpawnX")) {
                out.hasSpawn = true;
                out.spawnX = static_cast<int>(root.getInt("SpawnX"));
                out.spawnY = static_cast<int>(root.getInt("SpawnY"));
                out.spawnZ = static_cast<int>(root.getInt("SpawnZ"));
            }
        }
    }
    if (out.name.empty()) out.name = out.folderId;
    for (auto& f : std::filesystem::directory_iterator(dir / L"db", ec))
        if (f.is_regular_file(ec)) out.sizeBytes += f.file_size(ec);
    return true;
}

std::vector<WorldInfo> listWorlds() {
    std::vector<WorldInfo> worlds;
    std::error_code ec;
    for (auto& root : worldRoots())
        for (auto& d : std::filesystem::directory_iterator(root, ec)) {
            WorldInfo info;
            if (d.is_directory(ec) && readWorldInfo(d.path(), info)) worlds.push_back(std::move(info));
        }
    std::sort(worlds.begin(), worlds.end(), [](const WorldInfo& a, const WorldInfo& b) { return a.lastPlayed > b.lastPlayed; });
    return worlds;
}

bool parseChunkKey(std::string_view key, ChunkKey& out) {
    auto i32 = [&](size_t off) {
        int32_t v;
        std::memcpy(&v, key.data() + off, 4);
        return v;
    };
    switch (key.size()) {
    case 9:
    case 10:
        out.x = i32(0);
        out.z = i32(4);
        out.dimension = 0;
        out.tag = static_cast<uint8_t>(key[8]);
        out.subY = key.size() == 10 ? static_cast<int8_t>(key[9]) : 0;
        break;
    case 13:
    case 14:
        out.x = i32(0);
        out.z = i32(4);
        out.dimension = i32(8);
        out.tag = static_cast<uint8_t>(key[12]);
        out.subY = key.size() == 14 ? static_cast<int8_t>(key[13]) : 0;
        break;
    default: return false;
    }
    // Tags de chunk connus : 33 à 65, et 118 (ancienne version). Dimensions : 0, 1, 2.
    if (!((out.tag >= 33 && out.tag <= 65) || out.tag == 118) || out.dimension < 0 || out.dimension > 2) return false;
    if (out.tag == TagSubChunk) return key.size() == 10 || key.size() == 14;
    return key.size() == 9 || key.size() == 13;
}

uint32_t SubChunk::paletteIndex(int x, int ly, int z) const {
    if (bitsPerBlock == 0) return 0;
    int idx = (x << 8) | (z << 4) | ly;
    int perWord = 32 / bitsPerBlock;
    size_t word = static_cast<size_t>(idx / perWord);
    if (word >= words.size()) return 0;
    int shift = (idx % perWord) * bitsPerBlock;
    return (words[word] >> shift) & ((1u << bitsPerBlock) - 1u);
}

const std::string& SubChunk::blockName(int x, int ly, int z) const {
    static const std::string empty;
    uint32_t i = paletteIndex(x, ly, z);
    return i < palette.size() ? palette[i] : empty;
}

bool decodeSubChunk(std::string_view v, int8_t keyY, SubChunk& out) {
    if (v.size() < 2) return false;
    uint8_t version = static_cast<uint8_t>(v[0]);
    v.remove_prefix(1);
    out.y = keyY;
    if (version == 8 || version == 9) {
        uint8_t storages = static_cast<uint8_t>(v[0]);
        v.remove_prefix(1);
        if (version == 9) {
            if (v.empty()) return false;
            out.y = static_cast<int8_t>(v[0]);
            v.remove_prefix(1);
        }
        if (storages == 0) {
            out.bitsPerBlock = 0;
            out.palette = {"minecraft:air"};
            return true;
        }
    } else if (version != 1) {
        return false; // formats antérieurs à 1.2.13 non pris en charge
    }
    if (v.empty()) return false;
    uint8_t header = static_cast<uint8_t>(v[0]);
    v.remove_prefix(1);
    int bits = header >> 1;
    if (bits > 16 || (header & 1)) return false; // palette « réseau » : jamais sur disque
    out.bitsPerBlock = bits;
    size_t wordCount = 0;
    if (bits > 0) {
        int perWord = 32 / bits;
        wordCount = static_cast<size_t>((4096 + perWord - 1) / perWord);
    }
    if (v.size() < wordCount * 4) return false;
    out.words.resize(wordCount);
    if (wordCount) std::memcpy(out.words.data(), v.data(), wordCount * 4);
    v.remove_prefix(wordCount * 4);
    int32_t paletteSize = 1;
    if (v.size() < 4) return false;
    std::memcpy(&paletteSize, v.data(), 4);
    v.remove_prefix(4);
    if (paletteSize <= 0 || paletteSize > 4096) return false;
    out.palette.clear();
    out.palette.reserve(static_cast<size_t>(paletteSize));
    for (int32_t i = 0; i < paletteSize; ++i) {
        std::string name;
        nbt::Value entry;
        if (!nbt::readNamed(v, name, entry)) return false;
        out.palette.push_back(entry.getString("name"));
    }
    return true;
}

bool decodeHeightMap(std::string_view value, int16_t out[256]) {
    if (value.size() < 512) return false;
    std::memcpy(out, value.data(), 512);
    return true;
}

std::optional<PlayerPos> decodeLocalPlayer(std::string_view value) {
    std::string name;
    nbt::Value root;
    if (!nbt::readNamed(value, name, root)) return std::nullopt;
    const nbt::Value* pos = root.find("Pos");
    if (!pos || pos->list.size() < 3) return std::nullopt;
    PlayerPos p;
    p.x = static_cast<float>(pos->list[0].d);
    p.y = static_cast<float>(pos->list[1].d);
    p.z = static_cast<float>(pos->list[2].d);
    p.dimension = static_cast<int>(root.getInt("DimensionId"));
    return p;
}

} // namespace bcc::map
