#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bcc::native {

struct Vec3 { float x = 0, y = 0, z = 0; };
struct Angles { float yaw = 0, pitch = 0; };
struct PlayerSnapshot {
    Vec3 position;
    Angles view;
    int dimension = 0;
    bool valid = false;
};
struct EntitySnapshot {
    std::uint64_t runtimeId = 0;
    Vec3 position;
    Vec3 velocity;
    bool isPlayer = false;
    bool alive = false;
    std::string displayName;
};
struct CameraSnapshot {
    Vec3 origin;
    Angles view;
    float verticalFovDegrees = 70.f;
    bool valid = false;
};
struct WorldSnapshot {
    PlayerSnapshot player;
    CameraSnapshot camera;
    std::vector<EntitySnapshot> entities;
    std::uint64_t sequence = 0;
};

// Interface versionnée : aucun accès mémoire ni offset supposé.
// Les modules ne doivent utiliser que des snapshots explicitement valides.
class IBedrockBridge {
public:
    virtual ~IBedrockBridge() = default;
    virtual bool connected() const noexcept = 0;
    virtual std::optional<WorldSnapshot> readSnapshot() = 0;
    virtual const char* providerName() const noexcept = 0;
};
} // namespace bcc::native
