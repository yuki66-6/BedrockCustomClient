#include "integration/RakNetPing.h"

#include <cstring>
#include <sstream>

namespace bcc::raknet {
namespace {

constexpr uint8_t kMagic[16] = {0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xfe, 0xfe,
                                0xfd, 0xfd, 0xfd, 0xfd, 0x12, 0x34, 0x56, 0x78};
constexpr uint8_t kPing = 0x01, kPong = 0x1c;

void putBE64(std::vector<uint8_t>& v, uint64_t x) {
    for (int i = 7; i >= 0; --i) v.push_back(static_cast<uint8_t>(x >> (i * 8)));
}

uint64_t getBE64(const uint8_t* p) {
    uint64_t x = 0;
    for (int i = 0; i < 8; ++i) x = (x << 8) | p[i];
    return x;
}

int toInt(const std::string& s) {
    try {
        size_t used = 0;
        int v = std::stoi(s, &used);
        return used ? v : -1;
    } catch (...) {
        return -1;
    }
}

} // namespace

std::vector<uint8_t> buildUnconnectedPing(int64_t timeMs, uint64_t clientGuid) {
    std::vector<uint8_t> v;
    v.reserve(33);
    v.push_back(kPing);
    putBE64(v, static_cast<uint64_t>(timeMs));
    v.insert(v.end(), std::begin(kMagic), std::end(kMagic));
    putBE64(v, clientGuid);
    return v;
}

bool parseUnconnectedPong(const uint8_t* data, size_t size, PongInfo& out) {
    if (size < 35 || data[0] != kPong) return false;
    if (std::memcmp(data + 17, kMagic, 16) != 0) return false;
    size_t len = (static_cast<size_t>(data[33]) << 8) | data[34];
    if (35 + len > size) return false;
    out.timeMs = static_cast<int64_t>(getBE64(data + 1));
    out.serverGuid = getBE64(data + 9);
    out.raw.assign(reinterpret_cast<const char*>(data + 35), len);

    std::vector<std::string> fields;
    std::stringstream ss(out.raw);
    std::string f;
    while (std::getline(ss, f, ';')) fields.push_back(f);
    if (fields.size() > 1) out.motd = fields[1];
    if (fields.size() > 3) out.version = fields[3];
    if (fields.size() > 4) out.players = toInt(fields[4]);
    if (fields.size() > 5) out.maxPlayers = toInt(fields[5]);
    return true;
}

} // namespace bcc::raknet
