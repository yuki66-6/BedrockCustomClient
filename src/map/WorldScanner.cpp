#include "map/WorldScanner.h"

#include "utils/Log.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <unordered_map>
#include <unordered_set>

namespace bcc::map {
namespace {

// Hauteur stockée = hauteur monde - (-64) : le sommet visible est juste en dessous.
constexpr int kHeightOffset = -64;
constexpr int kSearchDepth = 4;
constexpr int kNetherCeiling = 120; // au-dessus : plafond de bedrock du Nether

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

uint64_t chunkId(int32_t x, int32_t z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(z);
}
uint64_t subId(int32_t x, int32_t z, int8_t y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x) & 0xFFFFFFu) << 40) |
           (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0xFFFFFFu) << 16) | static_cast<uint8_t>(y);
}

struct Heights {
    uint64_t seq = 0;
    bool deleted = false;
    int16_t h[256];
};
struct Versioned {
    uint64_t seq = 0;
    bool deleted = false;
};
struct Columns {
    int16_t y[256];
    uint32_t color[256];
    Columns() {
        std::fill(std::begin(y), std::end(y), static_cast<int16_t>(INT16_MIN));
        std::fill(std::begin(color), std::end(color), 0u);
    }
};

uint32_t shade(uint32_t c, float k) {
    auto ch = [&](int shift) {
        float v = static_cast<float>((c >> shift) & 0xff) * k;
        return static_cast<uint32_t>(std::clamp(v, 0.f, 255.f)) << shift;
    };
    return ch(0) | ch(8) | ch(16) | (c & 0xff000000u);
}

} // namespace

const char* dimensionName(int dimension) {
    switch (dimension) {
    case 1: return "Nether";
    case 2: return "End";
    default: return "Overworld";
    }
}

std::unique_ptr<WorldMapResult> WorldScanner::scan(const WorldInfo& world, const BlockColors& colors, int dimension,
                                                   const std::atomic<bool>* cancel,
                                                   const std::function<void(float, const char*)>& progress) {
    auto t0 = std::chrono::steady_clock::now();
    auto result = std::make_unique<WorldMapResult>();
    result->world = world;
    result->dimension = dimension;
    auto report = [&](float p, const char* s) {
        if (progress) progress(p, s);
    };
    std::filesystem::path db = world.dir / L"db";
    bool nether = dimension == 1;

    // Passe 1 : versions les plus récentes des clés, cartes des hauteurs, joueur local.
    std::unordered_map<uint64_t, Heights> heights;
    std::unordered_map<uint64_t, Versioned> subs;
    std::unordered_set<uint64_t> explored; // chunks présents dans la sauvegarde = zones découvertes
    uint64_t playerSeq = 0;
    std::string playerValue;
    result->stats = ldb::scanDatabase(
        db,
        [&](const ldb::Entry& e) {
            ChunkKey k;
            if (parseChunkKey(e.key, k)) {
                if (k.dimension != dimension) return;
                if (k.tag == TagData3D || k.tag == TagData2D) {
                    Heights& h = heights[chunkId(k.x, k.z)];
                    if (e.sequence >= h.seq) {
                        h.seq = e.sequence;
                        h.deleted = e.type == ldb::EntryType::Deletion || !decodeHeightMap(e.value, h.h);
                    }
                } else if (k.tag == TagSubChunk) {
                    Versioned& v = subs[subId(k.x, k.z, k.subY)];
                    if (e.sequence >= v.seq) {
                        v.seq = e.sequence;
                        v.deleted = e.type == ldb::EntryType::Deletion;
                    }
                    if (e.type == ldb::EntryType::Value) explored.insert(chunkId(k.x, k.z));
                }
            } else if (e.key == "~local_player" && e.sequence >= playerSeq && e.type == ldb::EntryType::Value) {
                playerSeq = e.sequence;
                playerValue.assign(e.value.data(), e.value.size());
            }
        },
        cancel, [&](float p) { report(p * 0.45f, "Lecture des hauteurs"); });
    if (cancel && *cancel) return nullptr;
    std::erase_if(heights, [](const auto& kv) { return kv.second.deleted; });
    for (auto& [id, h] : heights) explored.insert(id);
    if (auto p = decodeLocalPlayer(playerValue)) {
        result->hasPlayer = true;
        result->player = *p;
    }

    // Passe 2 : bloc visible de chaque colonne.
    //  - Overworld / End : sommet donné par la carte des hauteurs ;
    //  - Nether : coupe sous le plafond (plus haut sol surmonté d'air sous y = 120).
    std::unordered_map<uint64_t, Columns> columns;
    columns.reserve(explored.size());
    SubChunk sc;
    ldb::scanDatabase(
        db,
        [&](const ldb::Entry& e) {
            if (e.type != ldb::EntryType::Value) return;
            ChunkKey k;
            if (!parseChunkKey(e.key, k) || k.dimension != dimension || k.tag != TagSubChunk) return;
            auto sv = subs.find(subId(k.x, k.z, k.subY));
            if (sv == subs.end() || sv->second.seq != e.sequence || sv->second.deleted) return;
            if (nether) {
                if (k.subY * 16 > kNetherCeiling || !decodeSubChunk(e.value, k.subY, sc)) return;
                int yMin = sc.y * 16;
                Columns& col = columns[chunkId(k.x, k.z)];
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        int i = (z << 4) | x;
                        for (int ly = std::min(15, kNetherCeiling - yMin); ly >= 1; --ly) {
                            int y = yMin + ly - 1;
                            if (y <= col.y[i]) break;
                            if (!colors.isAir(sc.blockName(x, ly, z))) continue;
                            const std::string& below = sc.blockName(x, ly - 1, z);
                            if (colors.isAir(below)) continue;
                            col.y[i] = static_cast<int16_t>(y);
                            col.color[i] = colors.color(below);
                            break;
                        }
                    }
                return;
            }
            auto hit = heights.find(chunkId(k.x, k.z));
            if (hit == heights.end()) return;
            const int16_t* h = hit->second.h;
            int yMin = k.subY * 16, yMax = yMin + 15;
            bool needed = false;
            for (int i = 0; i < 256 && !needed; ++i) {
                int top = h[i] + kHeightOffset;
                needed = h[i] > 0 && top - kSearchDepth <= yMax && top >= yMin;
            }
            if (!needed || !decodeSubChunk(e.value, k.subY, sc)) return;
            yMin = sc.y * 16;
            Columns& col = columns[chunkId(k.x, k.z)];
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    int i = (z << 4) | x;
                    if (h[i] <= 0) continue; // colonne vide (vide de l'End)
                    int top = h[i] + kHeightOffset;
                    for (int y = top; y >= top - kSearchDepth; --y) {
                        if (y < yMin || y > yMin + 15 || y <= col.y[i]) continue;
                        const std::string& name = sc.blockName(x, y - yMin, z);
                        if (colors.isAir(name)) continue;
                        col.y[i] = static_cast<int16_t>(y);
                        col.color[i] = colors.color(name);
                        break;
                    }
                }
        },
        cancel, [&](float p) { report(0.45f + p * 0.5f, "Lecture des blocs"); });
    if (cancel && *cancel) return nullptr;

    // Assemblage des régions, avec relief ombré (comparaison avec la colonne au nord).
    report(0.96f, "Assemblage de la carte");
    auto heightAt = [&](uint64_t id, const Columns* col, int i) -> int {
        if (!nether) {
            auto it = heights.find(id);
            if (it != heights.end()) return it->second.h[i];
        }
        return col ? col->y[i] : 0;
    };
    bool first = true;
    for (uint64_t id : explored) {
        int32_t cx = static_cast<int32_t>(id >> 32), cz = static_cast<int32_t>(id & 0xffffffffu);
        int rx = floorDiv(cx, kRegionChunks), rz = floorDiv(cz, kRegionChunks);
        auto& pixels = result->regions[{rx, rz}];
        if (pixels.empty()) pixels.assign(static_cast<size_t>(kRegionPixels) * kRegionPixels, 0u);
        auto colIt = columns.find(id);
        const Columns* col = colIt != columns.end() ? &colIt->second : nullptr;
        uint64_t northId = chunkId(cx, cz - 1);
        auto northCol = columns.find(northId);
        const Columns* ncol = northCol != columns.end() ? &northCol->second : nullptr;
        int ox = (cx - rx * kRegionChunks) * 16, oz = (cz - rz * kRegionChunks) * 16;
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                int i = (z << 4) | x;
                ++result->columnsTotal;
                uint32_t c;
                if (col && col->y[i] != INT16_MIN) {
                    c = col->color[i];
                    ++result->columnsResolved;
                } else if (dimension == 2) {
                    c = BlockColors::rgba(12, 10, 20); // vide de l'End (exploré)
                } else {
                    c = BlockColors::rgba(96, 118, 86); // exploré, bloc non résolu
                }
                int hHere = heightAt(id, col, i);
                // Bord nord d'une zone découverte : pas de voisin connu → pas d'ombrage.
                bool northKnown = explored.count(northId) != 0;
                int hNorth = z > 0 ? heightAt(id, col, i - 16) : (northKnown ? heightAt(northId, ncol, 240 + x) : hHere);
                int d = hHere - hNorth;
                float k = d > 0 ? 1.12f : (d < 0 ? 0.82f : 1.f);
                c = shade(c, k) | 0xff000000u;
                pixels[static_cast<size_t>(oz + z) * kRegionPixels + static_cast<size_t>(ox + x)] = c;
            }
        ++result->chunks;
        int bx = cx * 16, bz = cz * 16;
        if (first) {
            result->minBlockX = bx;
            result->maxBlockX = bx + 16;
            result->minBlockZ = bz;
            result->maxBlockZ = bz + 16;
            first = false;
        } else {
            result->minBlockX = std::min(result->minBlockX, bx);
            result->maxBlockX = std::max(result->maxBlockX, bx + 16);
            result->minBlockZ = std::min(result->minBlockZ, bz);
            result->maxBlockZ = std::max(result->maxBlockZ, bz + 16);
        }
    }
    result->seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!result->chunks) result->error = std::string("Aucune zone explorée dans la dimension ") + dimensionName(dimension) + ".";
    report(1.f, "Terminé");
    log::info("Carte « {} » ({}) : {} chunks, {} régions, {}/{} colonnes résolues, {} fichiers ({} en échec), {:.2f} s",
              world.name, dimensionName(dimension), result->chunks, result->regions.size(), result->columnsResolved,
              result->columnsTotal, result->stats.files, result->stats.failedFiles, result->seconds);
    return result;
}

void WorldScanner::start(const WorldInfo& world, const BlockColors& colors, int dimension) {
    cancel();
    cancel_ = false;
    running_ = true;
    progress_ = 0.f;
    {
        std::lock_guard lock(mutex_);
        result_.reset();
        stage_ = "Démarrage";
    }
    thread_ = std::thread([this, world, colors, dimension] {
        auto r = scan(world, colors, dimension, &cancel_, [this](float p, const char* s) {
            progress_ = p;
            std::lock_guard lock(mutex_);
            stage_ = s;
        });
        {
            std::lock_guard lock(mutex_);
            result_ = std::move(r);
        }
        running_ = false;
    });
}

void WorldScanner::cancel() {
    cancel_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
}

std::string WorldScanner::stage() const {
    std::lock_guard lock(mutex_);
    return stage_;
}

std::unique_ptr<WorldMapResult> WorldScanner::take() {
    if (running_) return nullptr;
    if (thread_.joinable()) thread_.join();
    std::lock_guard lock(mutex_);
    return std::move(result_);
}

} // namespace bcc::map
