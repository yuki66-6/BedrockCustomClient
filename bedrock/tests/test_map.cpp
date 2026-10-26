#include "TestFramework.h"

#include "map/BedrockWorld.h"
#include "map/BlockColors.h"
#include "map/LevelDb.h"
#include "map/Nbt.h"
#include "map/WorldScanner.h"
#include "utils/Bitmap.h"

#include <zlib.h>

#include <cstdlib>
#include <cstring>
#include <map>

using namespace bcc;

namespace {

void putVarint(std::string& s, uint64_t v) {
    while (v >= 0x80) {
        s.push_back(static_cast<char>((v & 0x7f) | 0x80));
        v >>= 7;
    }
    s.push_back(static_cast<char>(v));
}
void put32(std::string& s, uint32_t v) { s.append(reinterpret_cast<const char*>(&v), 4); }
void put64(std::string& s, uint64_t v) { s.append(reinterpret_cast<const char*>(&v), 8); }
void put16(std::string& s, uint16_t v) { s.append(reinterpret_cast<const char*>(&v), 2); }

std::string rawDeflate(const std::string& in) {
    z_stream zs{};
    deflateInit2(&zs, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    std::string out(in.size() + 64, '\0');
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    zs.avail_in = static_cast<uInt>(in.size());
    zs.next_out = reinterpret_cast<Bytef*>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    deflate(&zs, Z_FINISH);
    out.resize(zs.total_out);
    deflateEnd(&zs);
    return out;
}

// Construit une table SSTable minimale (un bloc de données) au format LevelDB.
std::string buildTable(const std::map<std::string, std::pair<uint64_t, std::string>>& entries, bool compress) {
    std::string block;
    for (auto& [key, sv] : entries) {
        putVarint(block, 0);
        putVarint(block, key.size() + 8);
        putVarint(block, sv.second.size());
        block += key;
        put64(block, (sv.first << 8) | 1);
        block += sv.second;
    }
    put32(block, 0);
    put32(block, 1);
    std::string file;
    std::string stored = compress ? rawDeflate(block) : block;
    uint64_t dataOff = 0, dataSize = stored.size();
    file += stored;
    file.push_back(compress ? 4 : 0);
    put32(file, 0); // CRC non vérifié
    std::string meta;
    put32(meta, 0);
    put32(meta, 1);
    uint64_t metaOff = file.size(), metaSize = meta.size();
    file += meta;
    file.push_back(0);
    put32(file, 0);
    std::string index;
    std::string handle;
    putVarint(handle, dataOff);
    putVarint(handle, dataSize);
    putVarint(index, 0);
    putVarint(index, 9);
    putVarint(index, handle.size());
    index += std::string(9, '\xff');
    index += handle;
    put32(index, 0);
    put32(index, 1);
    uint64_t indexOff = file.size(), indexSize = index.size();
    file += index;
    file.push_back(0);
    put32(file, 0);
    std::string footer;
    putVarint(footer, metaOff);
    putVarint(footer, metaSize);
    putVarint(footer, indexOff);
    putVarint(footer, indexSize);
    footer.resize(40, '\0');
    put64(footer, 0xdb4775248b80fb57ull);
    return file + footer;
}

std::string chunkKey(int32_t x, int32_t z, uint8_t tag, int sub = -100) {
    std::string k;
    put32(k, static_cast<uint32_t>(x));
    put32(k, static_cast<uint32_t>(z));
    k.push_back(static_cast<char>(tag));
    if (sub != -100) k.push_back(static_cast<char>(sub));
    return k;
}

void nbtString(std::string& s, const char* name, const std::string& value) {
    s.push_back(nbt::String);
    put16(s, static_cast<uint16_t>(std::strlen(name)));
    s += name;
    put16(s, static_cast<uint16_t>(value.size()));
    s += value;
}

// Sous-chunk v9 : 2 bits/bloc, palette [air, grass_block], herbe à y local 4.
std::string buildSubChunk(int8_t y) {
    std::string s;
    s.push_back(9);
    s.push_back(1);
    s.push_back(static_cast<char>(y));
    s.push_back(static_cast<char>(1 << 1)); // 1 bit par bloc
    std::vector<uint32_t> words(128, 0);
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            int idx = (x << 8) | (z << 4) | 4;
            words[static_cast<size_t>(idx / 32)] |= 1u << (idx % 32);
        }
    s.append(reinterpret_cast<const char*>(words.data()), words.size() * 4);
    put32(s, 2);
    for (const char* name : {"minecraft:air", "minecraft:grass_block"}) {
        s.push_back(nbt::Compound);
        put16(s, 0);
        nbtString(s, "name", name);
        s.push_back(nbt::End);
    }
    return s;
}

} // namespace

TEST_CASE("LevelDB : varints") {
    std::string s;
    putVarint(s, 300);
    putVarint(s, 1ull << 40);
    std::string_view in(s);
    uint64_t a, b;
    CHECK(ldb::getVarint64(in, a));
    CHECK(ldb::getVarint64(in, b));
    CHECK(a == 300);
    CHECK(b == (1ull << 40));
    CHECK(in.empty());
}

TEST_CASE("LevelDB : table brute et compressée (deflate Mojang)") {
    std::map<std::string, std::pair<uint64_t, std::string>> entries = {
        {"alpha", {5, "un"}}, {"beta", {7, "deux"}}, {"gamma", {9, std::string(500, 'x')}}};
    for (bool compress : {false, true}) {
        std::string table = buildTable(entries, compress);
        std::map<std::string, std::string> got;
        std::string err;
        CHECK(ldb::scanTable(table, [&](const ldb::Entry& e) {
            got[std::string(e.key)] = std::string(e.value);
            CHECK(e.sequence == entries.at(std::string(e.key)).first);
        }, &err));
        CHECK(got.size() == 3);
        CHECK(got["gamma"].size() == 500);
    }
    std::string bad = buildTable(entries, false);
    bad[bad.size() - 1] ^= 1;
    CHECK(!ldb::scanTable(bad, [](const ldb::Entry&) {}));
}

TEST_CASE("LevelDB : journal (WriteBatch, suppression)") {
    std::string batch;
    put64(batch, 100);
    put32(batch, 2);
    batch.push_back(1);
    putVarint(batch, 3);
    batch += "cle";
    putVarint(batch, 6);
    batch += "valeur";
    batch.push_back(0);
    putVarint(batch, 5);
    batch += "autre";
    std::string log;
    put32(log, 0);
    put16(log, static_cast<uint16_t>(batch.size()));
    log.push_back(1); // FULL
    log += batch;
    std::vector<ldb::Entry> got;
    std::vector<std::string> keys;
    CHECK(ldb::scanLog(log, [&](const ldb::Entry& e) {
        got.push_back(e);
        keys.emplace_back(e.key);
    }));
    REQUIRE(got.size() == 2);
    CHECK(keys[0] == "cle");
    CHECK(got[0].sequence == 100);
    CHECK(got[1].type == ldb::EntryType::Deletion);
    CHECK(got[1].sequence == 101);
}

TEST_CASE("Bedrock : clés de chunk, sous-chunk et NBT") {
    map::ChunkKey k;
    CHECK(map::parseChunkKey(chunkKey(-3, 7, map::TagSubChunk, -2), k));
    CHECK(k.x == -3);
    CHECK(k.z == 7);
    CHECK(k.subY == -2);
    CHECK(map::parseChunkKey(chunkKey(1, 2, map::TagData3D), k));
    CHECK(k.tag == map::TagData3D);
    CHECK(!map::parseChunkKey("~local_player", k));

    map::SubChunk sc;
    REQUIRE(map::decodeSubChunk(buildSubChunk(4), 4, sc));
    CHECK(sc.y == 4);
    CHECK(sc.palette.size() == 2);
    CHECK(sc.blockName(3, 4, 9) == "minecraft:grass_block");
    CHECK(sc.blockName(3, 5, 9) == "minecraft:air");

    map::BlockColors colors;
    CHECK(colors.isAir("minecraft:air"));
    CHECK(colors.color("minecraft:water") == map::BlockColors::rgba(52, 92, 212));
    CHECK(colors.color("monmod:custom_leaves") == map::BlockColors::rgba(70, 120, 40)); // heuristique
    colors.setOverride("monmod:ruby", map::BlockColors::rgba(200, 10, 50));
    CHECK(colors.color("monmod:ruby") == map::BlockColors::rgba(200, 10, 50));
    uint32_t c;
    CHECK(map::BlockColors::parseHex("#FF000080", c));
    CHECK(c == map::BlockColors::rgba(255, 0, 0, 128));
    CHECK(!map::BlockColors::parseHex("rouge", c));
}

TEST_CASE("Carte : monde synthétique complet (hauteurs + blocs + mode découverte)") {
    bcctest::TempDir dir;
    std::filesystem::create_directories(dir.path() / "db");
    // Chunk (0,0) : hauteur 72 (y monde 8 → sommet à y = 4 + 16*0 ... ) ; sous-chunk y=0 avec herbe à y=4.
    std::string heights(512, '\0');
    for (int i = 0; i < 256; ++i) {
        int16_t h = 64 + 5; // sommet y=4 → première case d'air y=5 → stocké 5 - (-64)
        std::memcpy(heights.data() + i * 2, &h, 2);
    }
    std::map<std::string, std::pair<uint64_t, std::string>> entries = {
        {chunkKey(0, 0, map::TagData3D), {1, heights}},
        {chunkKey(0, 0, map::TagSubChunk, 0), {2, buildSubChunk(0)}},
        {chunkKey(1, 0, map::TagData3D), {3, heights}}, // chunk exploré sans sous-chunk
    };
    std::string table = buildTable(entries, true);
    FILE* f = nullptr;
    REQUIRE(_wfopen_s(&f, (dir.path() / "db" / "000005.ldb").c_str(), L"wb") == 0);
    fwrite(table.data(), 1, table.size(), f);
    fclose(f);

    map::WorldInfo info;
    REQUIRE(map::readWorldInfo(dir.path(), info));
    auto r = map::WorldScanner::scan(info, map::BlockColors());
    REQUIRE(r != nullptr);
    CHECK(r->chunks == 2);
    CHECK(r->columnsTotal == 512);
    CHECK(r->columnsResolved == 256);
    REQUIRE(r->regions.size() == 1);
    const auto& px = r->regions.begin()->second;
    CHECK((px[0] & 0xff000000u) != 0);              // chunk exploré
    CHECK((px[40] & 0xff000000u) == 0);             // x = 40 : chunk 2 jamais exploré → transparent
    CHECK((px[0] & 0xff) == 106);                   // herbe
}

TEST_CASE("Carte : scan d'une copie d'un vrai monde (si BCC_TEST_WORLD est défini)") {
    char* path = nullptr;
    size_t len = 0;
    if (_dupenv_s(&path, &len, "BCC_TEST_WORLD") != 0 || !path) return; // test facultatif
    std::filesystem::path world = path;
    free(path);
    map::WorldInfo info;
    REQUIRE(map::readWorldInfo(world, info));
    for (int dim = 1; dim <= 2; ++dim) {
        auto d = map::WorldScanner::scan(info, map::BlockColors(), dim);
        REQUIRE(d != nullptr);
        std::printf("    %s : %d chunks, %llu/%llu colonnes résolues\n", map::dimensionName(dim), d->chunks,
                    static_cast<unsigned long long>(d->columnsResolved), static_cast<unsigned long long>(d->columnsTotal));
    }
    auto r = map::WorldScanner::scan(info, map::BlockColors());
    REQUIRE(r != nullptr);
    std::printf("    monde « %s » : %d chunks, %zu régions, %llu/%llu colonnes résolues, %d fichiers (%d en échec), %.2f s\n",
                info.name.c_str(), r->chunks, r->regions.size(), static_cast<unsigned long long>(r->columnsResolved),
                static_cast<unsigned long long>(r->columnsTotal), r->stats.files, r->stats.failedFiles, r->seconds);
    CHECK(r->chunks > 0);
    CHECK(r->stats.failedFiles == 0);
    CHECK(r->columnsResolved * 10 > r->columnsTotal * 7); // au moins 70 % des colonnes résolues
    // Image de la région du joueur (ou de la première) pour contrôle visuel.
    char* out = nullptr;
    if (_dupenv_s(&out, &len, "BCC_TEST_OUTPUT") == 0 && out) {
        auto& [key, pixels] = *r->regions.begin();
        std::vector<uint8_t> bgra(pixels.size() * 4);
        for (size_t i = 0; i < pixels.size(); ++i) {
            uint32_t c = pixels[i];
            bgra[i * 4 + 0] = static_cast<uint8_t>(c >> 16);
            bgra[i * 4 + 1] = static_cast<uint8_t>(c >> 8);
            bgra[i * 4 + 2] = static_cast<uint8_t>(c);
            bgra[i * 4 + 3] = static_cast<uint8_t>(c >> 24);
        }
        writePremultipliedBmp(std::filesystem::path(out) / "real_world_region.bmp", bgra.data(), map::kRegionPixels,
                              map::kRegionPixels, map::kRegionPixels * 4);
        free(out);
    }
}
