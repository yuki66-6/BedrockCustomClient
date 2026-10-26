#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace bcc {

struct PingResult {
    bool valid = false;  // une réponse a été reçue
    int ms = -1;
    std::string host;
    std::string motd;
    std::string version;
    int players = -1, maxPlayers = -1;
    std::string error;   // dernière erreur (résolution, délai dépassé...)
    double ageSeconds = 0; // âge de la mesure
};

// Mesure périodique de la latence vers un serveur Bedrock (ping RakNet public),
// sur un thread dédié qui dort entre deux mesures (aucune attente active).
class ServerPinger {
public:
    ServerPinger();
    ~ServerPinger();

    void setTarget(const std::string& host, int port, double intervalSeconds, bool enabled);
    PingResult result() const;

private:
    void run();
    bool pingOnce(const std::string& host, int port, PingResult& out);

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::string host_;
    int port_ = 19132;
    double interval_ = 3.0;
    bool enabled_ = false;
    bool changed_ = false;
    bool stop_ = false;
    PingResult result_;
    double resultTime_ = 0;
    bool wsaOk_ = false;
    std::thread thread_;
};

} // namespace bcc
