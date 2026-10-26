#pragma once

#include "integration/GameAdapter.h"

namespace bcc {

class TelemetryProvider;

// Adaptateur Bedrock : overlay externe. Expose uniquement ce qui est réellement
// accessible sans injection (entrées, fenêtre, ping public, sauvegardes).
class BedrockOverlayAdapter : public GameAdapter {
public:
    explicit BedrockOverlayAdapter(TelemetryProvider& telemetry) : t_(telemetry) {}

    Edition edition() const override { return Edition::Bedrock; }
    std::string name() const override { return "Minecraft Bedrock (overlay)"; }
    CapabilityInfo capability(Capability c) const override;

    DataPoint<int> clicksPerSecond(bool left) const override;
    DataPoint<float> gameFps() const override;
    DataPoint<float> overlayFps() const override;
    DataPoint<int> latencyMs() const override;
    DataPoint<Vec3d> playerPosition() const override;
    DataPoint<std::string> worldIdentity() const override;
    DataPoint<long long> worldSeed() const override;

private:
    TelemetryProvider& t_;
};

} // namespace bcc
