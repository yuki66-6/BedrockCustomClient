#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bcc::raknet {

// « Unconnected ping » RakNet : la requête publique qu'utilisent les listes de
// serveurs Bedrock pour afficher MOTD, joueurs et latence. Aucune connexion au
// jeu n'est établie.
std::vector<uint8_t> buildUnconnectedPing(int64_t timeMs, uint64_t clientGuid);

struct PongInfo {
    int64_t timeMs = 0;
    uint64_t serverGuid = 0;
    std::string raw;  // « MCPE;MOTD;protocole;version;joueurs;max;... »
    std::string motd;
    std::string version;
    int players = -1;
    int maxPlayers = -1;
};

bool parseUnconnectedPong(const uint8_t* data, size_t size, PongInfo& out);

} // namespace bcc::raknet
