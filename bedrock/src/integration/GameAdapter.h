#pragma once

#include "core/Capabilities.h"
#include "core/DataPoint.h"

#include <string>
#include <vector>

namespace bcc {

enum class Edition { Java, Bedrock };

struct Vec3d {
    double x = 0, y = 0, z = 0;
};

// Interface commune aux intégrations de jeu (même contrat que GameAdapter côté
// Java : java/fabric/.../telemetry/GameAdapter.java). Toute donnée est un
// DataPoint : jamais de valeur inventée, toujours un motif si indisponible.
class GameAdapter {
public:
    virtual ~GameAdapter() = default;

    virtual Edition edition() const = 0;
    virtual std::string name() const = 0;
    virtual CapabilityInfo capability(Capability c) const = 0;
    std::vector<CapabilityInfo> capabilities() const;
    bool supports(Capability c) const { return capability(c).availability != Availability::Unsupported; }

    virtual DataPoint<int> clicksPerSecond(bool left) const = 0;
    virtual DataPoint<float> gameFps() const = 0;
    virtual DataPoint<float> overlayFps() const = 0;
    virtual DataPoint<int> latencyMs() const = 0;
    virtual DataPoint<Vec3d> playerPosition() const = 0;
    virtual DataPoint<std::string> worldIdentity() const = 0;
    virtual DataPoint<long long> worldSeed() const = 0;
};

} // namespace bcc
