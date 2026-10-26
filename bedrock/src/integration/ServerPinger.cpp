#include "integration/ServerPinger.h"

#include "integration/RakNetPing.h"
#include "utils/Log.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <chrono>
#include <random>

namespace bcc {
namespace {
double steadyNow() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

ServerPinger::ServerPinger() {
    WSADATA wsa;
    wsaOk_ = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    thread_ = std::thread([this] { run(); });
}

ServerPinger::~ServerPinger() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    if (wsaOk_) WSACleanup();
}

void ServerPinger::setTarget(const std::string& host, int port, double intervalSeconds, bool enabled) {
    std::lock_guard lock(mutex_);
    if (host == host_ && port == port_ && enabled == enabled_ && intervalSeconds == interval_) return;
    if (host != host_ || port != port_) {
        result_ = {};
        resultTime_ = 0;
    }
    host_ = host;
    port_ = port;
    interval_ = std::max(1.0, intervalSeconds);
    enabled_ = enabled;
    changed_ = true;
    cv_.notify_all();
}

PingResult ServerPinger::result() const {
    std::lock_guard lock(mutex_);
    PingResult r = result_;
    r.ageSeconds = resultTime_ > 0 ? steadyNow() - resultTime_ : 0;
    return r;
}

void ServerPinger::run() {
    std::unique_lock lock(mutex_);
    while (!stop_) {
        if (!enabled_ || host_.empty() || !wsaOk_) {
            cv_.wait(lock, [this] { return stop_ || changed_; });
            changed_ = false;
            continue;
        }
        std::string host = host_;
        int port = port_;
        double interval = interval_;
        changed_ = false;
        lock.unlock();
        PingResult r;
        pingOnce(host, port, r);
        lock.lock();
        if (host == host_ && port == port_) {
            // On garde la dernière mesure valide si cette tentative a échoué.
            if (r.valid || !result_.valid) {
                result_ = r;
                resultTime_ = steadyNow();
            } else {
                result_.error = r.error;
            }
        }
        cv_.wait_for(lock, std::chrono::duration<double>(interval), [this] { return stop_ || changed_; });
    }
}

bool ServerPinger::pingOnce(const std::string& host, int port, PingResult& out) {
    out.host = host;
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        out.error = "adresse introuvable";
        return false;
    }
    SOCKET s = socket(res->ai_family, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        freeaddrinfo(res);
        out.error = "socket indisponible";
        return false;
    }
    static std::mt19937_64 rng{std::random_device{}()};
    auto start = std::chrono::steady_clock::now();
    int64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    auto packet = raknet::buildUnconnectedPing(t, rng());
    sendto(s, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0, res->ai_addr,
           static_cast<int>(res->ai_addrlen));
    freeaddrinfo(res);

    bool ok = false;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv{2, 0};
    if (select(0, &set, nullptr, nullptr, &tv) == 1) {
        uint8_t buf[2048];
        int n = recv(s, reinterpret_cast<char*>(buf), sizeof(buf), 0);
        raknet::PongInfo pong;
        if (n > 0 && raknet::parseUnconnectedPong(buf, static_cast<size_t>(n), pong)) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            out.valid = true;
            out.ms = static_cast<int>(ms.count());
            out.motd = pong.motd;
            out.version = pong.version;
            out.players = pong.players;
            out.maxPlayers = pong.maxPlayers;
            ok = true;
        } else {
            out.error = "réponse invalide";
        }
    } else {
        out.error = "pas de réponse (2 s)";
    }
    closesocket(s);
    return ok;
}

} // namespace bcc
