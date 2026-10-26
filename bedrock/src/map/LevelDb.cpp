#include "map/LevelDb.h"

#include <windows.h>
#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <format>

namespace bcc::ldb {
namespace {

constexpr uint64_t kTableMagic = 0xdb4775248b80fb57ull;
constexpr size_t kFooterSize = 48;
constexpr size_t kLogBlockSize = 32768;

uint32_t fixed32(const char* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
uint64_t fixed64(const char* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}

struct BlockHandle {
    uint64_t offset = 0, size = 0;
};

bool getHandle(std::string_view& in, BlockHandle& h) { return getVarint64(in, h.offset) && getVarint64(in, h.size); }

// Lit le bloc décrit par `h` (contenu + trailer de 5 octets : compression, CRC).
bool readBlock(std::string_view file, const BlockHandle& h, std::string& out) {
    if (h.offset + h.size + 5 > file.size()) return false;
    std::string_view raw = file.substr(static_cast<size_t>(h.offset), static_cast<size_t>(h.size));
    uint8_t compression = static_cast<uint8_t>(file[static_cast<size_t>(h.offset + h.size)]);
    return decompressBlock(compression, raw, out);
}

// Parcourt les entrées d'un bloc (clés à préfixe partagé + tableau de restarts).
template <class F>
bool forEachBlockEntry(std::string_view block, F&& f) {
    if (block.size() < 4) return false;
    uint32_t numRestarts = fixed32(block.data() + block.size() - 4);
    size_t restartsSize = (static_cast<size_t>(numRestarts) + 1) * 4;
    if (restartsSize > block.size()) return false;
    std::string_view in = block.substr(0, block.size() - restartsSize);
    std::string key;
    while (!in.empty()) {
        uint32_t shared, nonShared, valueLen;
        if (!getVarint32(in, shared) || !getVarint32(in, nonShared) || !getVarint32(in, valueLen)) return false;
        if (shared > key.size() || static_cast<size_t>(nonShared) + valueLen > in.size()) return false;
        key.resize(shared);
        key.append(in.data(), nonShared);
        std::string_view value = in.substr(nonShared, valueLen);
        in.remove_prefix(static_cast<size_t>(nonShared) + valueLen);
        f(std::string_view(key), value);
    }
    return true;
}

// Les journaux contiennent des WriteBatch : séquence, compteur, puis opérations.
bool scanWriteBatch(std::string_view batch, const Visitor& visit) {
    if (batch.size() < 12) return false;
    uint64_t seq = fixed64(batch.data());
    uint32_t count = fixed32(batch.data() + 8);
    std::string_view in = batch.substr(12);
    for (uint32_t i = 0; i < count && !in.empty(); ++i) {
        uint8_t tag = static_cast<uint8_t>(in[0]);
        in.remove_prefix(1);
        uint32_t klen;
        if (!getVarint32(in, klen) || klen > in.size()) return false;
        Entry e;
        e.key = in.substr(0, klen);
        in.remove_prefix(klen);
        e.sequence = seq + i;
        if (tag == 1) {
            uint32_t vlen;
            if (!getVarint32(in, vlen) || vlen > in.size()) return false;
            e.value = in.substr(0, vlen);
            in.remove_prefix(vlen);
            e.type = EntryType::Value;
        } else if (tag == 0) {
            e.type = EntryType::Deletion;
        } else {
            return false;
        }
        visit(e);
    }
    return true;
}

} // namespace

bool getVarint32(std::string_view& in, uint32_t& v) {
    uint64_t v64;
    if (!getVarint64(in, v64) || v64 > 0xffffffffull) return false;
    v = static_cast<uint32_t>(v64);
    return true;
}

bool getVarint64(std::string_view& in, uint64_t& v) {
    v = 0;
    for (int shift = 0; shift <= 63 && !in.empty(); shift += 7) {
        uint8_t b = static_cast<uint8_t>(in[0]);
        in.remove_prefix(1);
        v |= static_cast<uint64_t>(b & 0x7f) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

bool readFileShared(const std::filesystem::path& file, std::string& out) {
    HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(h, &size) && size.QuadPart < (1ll << 31);
    if (ok) {
        out.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        size_t done = 0;
        while (ok && done < out.size()) {
            DWORD chunk = static_cast<DWORD>(std::min<size_t>(out.size() - done, 1u << 24));
            ok = ReadFile(h, out.data() + done, chunk, &read, nullptr) && read > 0;
            done += read;
        }
        out.resize(done);
    }
    CloseHandle(h);
    return ok;
}

bool decompressBlock(uint8_t compression, std::string_view in, std::string& out) {
    if (compression == 0) {
        out.assign(in.data(), in.size());
        return true;
    }
    if (compression != 2 && compression != 4) return false; // 1 = snappy : non utilisé par Bedrock
    z_stream zs{};
    // 2 = zlib (en-tête), 4 = deflate brut (variante Mojang).
    if (inflateInit2(&zs, compression == 2 ? 15 : -15) != Z_OK) return false;
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    zs.avail_in = static_cast<uInt>(in.size());
    out.resize(std::max<size_t>(in.size() * 4, 4096));
    size_t produced = 0;
    int r = Z_OK;
    while (r == Z_OK) {
        if (produced == out.size()) out.resize(out.size() * 2);
        zs.next_out = reinterpret_cast<Bytef*>(out.data() + produced);
        zs.avail_out = static_cast<uInt>(out.size() - produced);
        r = inflate(&zs, Z_NO_FLUSH);
        produced = out.size() - zs.avail_out;
        if (r == Z_BUF_ERROR && zs.avail_in == 0) break;
    }
    inflateEnd(&zs);
    out.resize(produced);
    return r == Z_STREAM_END || (r == Z_BUF_ERROR && produced > 0);
}

bool scanTable(std::string_view data, const Visitor& visit, std::string* error) {
    auto fail = [&](const char* msg) {
        if (error) *error = msg;
        return false;
    };
    if (data.size() < kFooterSize) return fail("table trop courte");
    std::string_view footer = data.substr(data.size() - kFooterSize);
    if (fixed64(footer.data() + 40) != kTableMagic) return fail("signature de table invalide");
    BlockHandle metaindex, index;
    std::string_view f = footer;
    if (!getHandle(f, metaindex) || !getHandle(f, index)) return fail("pied de table invalide");
    std::string indexBlock;
    if (!readBlock(data, index, indexBlock)) return fail("bloc d'index illisible");
    std::string dataBlock;
    bool ok = true;
    forEachBlockEntry(indexBlock, [&](std::string_view, std::string_view handleBytes) {
        BlockHandle h;
        if (!getHandle(handleBytes, h) || !readBlock(data, h, dataBlock)) {
            ok = false;
            return;
        }
        forEachBlockEntry(dataBlock, [&](std::string_view ikey, std::string_view value) {
            if (ikey.size() < 8) return;
            uint64_t trailer = fixed64(ikey.data() + ikey.size() - 8);
            Entry e;
            e.key = ikey.substr(0, ikey.size() - 8);
            e.sequence = trailer >> 8;
            e.type = (trailer & 0xff) == 1 ? EntryType::Value : EntryType::Deletion;
            if (e.type == EntryType::Value) e.value = value;
            visit(e);
        });
    });
    if (!ok) return fail("bloc de données illisible");
    return true;
}

bool scanLog(std::string_view data, const Visitor& visit, std::string* error) {
    std::string record;
    bool inFragment = false;
    size_t pos = 0;
    while (pos + 7 <= data.size()) {
        size_t blockLeft = kLogBlockSize - (pos % kLogBlockSize);
        if (blockLeft < 7) { // remplissage de fin de bloc
            pos += blockLeft;
            continue;
        }
        uint16_t len = static_cast<uint16_t>(static_cast<uint8_t>(data[pos + 4]) | (static_cast<uint8_t>(data[pos + 5]) << 8));
        uint8_t type = static_cast<uint8_t>(data[pos + 6]);
        if (type == 0 && len == 0) { // zone pré-allouée vide
            pos += blockLeft;
            continue;
        }
        if (pos + 7 + len > data.size()) break; // fin tronquée (écriture en cours)
        std::string_view payload = data.substr(pos + 7, len);
        pos += 7 + len;
        switch (type) {
        case 1: // FULL
            scanWriteBatch(payload, visit);
            inFragment = false;
            break;
        case 2: // FIRST
            record.assign(payload.data(), payload.size());
            inFragment = true;
            break;
        case 3: // MIDDLE
            if (inFragment) record.append(payload.data(), payload.size());
            break;
        case 4: // LAST
            if (inFragment) {
                record.append(payload.data(), payload.size());
                scanWriteBatch(record, visit);
            }
            inFragment = false;
            break;
        default:
            if (error) *error = "type d'enregistrement inconnu";
            return false;
        }
    }
    return true;
}

ScanStats scanDatabase(const std::filesystem::path& dbDir, const Visitor& visit, const std::atomic<bool>* cancel,
                       const std::function<void(float)>& progress) {
    ScanStats stats;
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dbDir, ec)) {
        auto ext = e.path().extension().wstring();
        if (e.is_regular_file() && (ext == L".ldb" || ext == L".sst" || ext == L".log")) files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    std::string content;
    for (size_t i = 0; i < files.size(); ++i) {
        if (cancel && *cancel) break;
        if (progress) progress(static_cast<float>(i) / static_cast<float>(std::max<size_t>(1, files.size())));
        if (!readFileShared(files[i], content)) {
            ++stats.failedFiles;
            continue;
        }
        stats.bytes += content.size();
        auto counting = [&](const Entry& e) {
            ++stats.entries;
            visit(e);
        };
        bool ok = files[i].extension() == L".log" ? scanLog(content, counting) : scanTable(content, counting);
        ++stats.files;
        if (!ok) ++stats.failedFiles;
    }
    if (progress) progress(1.f);
    return stats;
}

} // namespace bcc::ldb
