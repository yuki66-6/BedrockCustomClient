#pragma once

#include "core/DataPoint.h"

#include <string>
#include <string_view>
#include <vector>

namespace bcc {

// Capacités communes Java/Bedrock (ordre et identifiants : shared/schemas/capabilities.json).
enum class Capability {
    KeyboardInput,
    MouseInput,
    GameWindow,
    GameFps,
    OverlayFps,
    ServerLatency,
    PlayerPosition,
    PlayerRotation,
    PlayerVelocity,
    PlayerHealth,
    PlayerHunger,
    PlayerArmor,
    Inventory,
    PotionEffects,
    Dimension,
    WorldIdentity,
    WorldSeed,
    SavedWorldMap,
    DeathEvents,
    InWorldRender,
    FovControl,
    MovementToggles,
    ChatCommands,
    Count
};

const char* capabilityId(Capability c);
bool capabilityFromId(std::string_view id, Capability& out);
const char* capabilityLabel(Capability c); // libellé français
std::vector<Capability> allCapabilities();

struct CapabilityInfo {
    Capability capability = Capability::KeyboardInput;
    Availability availability = Availability::Unsupported;
    DataSource source = DataSource::None;
    std::string reason; // explication si non disponible, ou précision
};

} // namespace bcc
