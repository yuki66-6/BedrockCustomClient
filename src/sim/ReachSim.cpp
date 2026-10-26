#include "sim/ReachSim.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace bcc::sim {
namespace {
constexpr float kDegToRad = 3.14159265358979f / 180.f;
float axis(const Vec3& v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
} // namespace

bool rayAabb(const Vec3& origin, const Vec3& dir, const Aabb& box, float& tHit) {
    float tmin = 0.f, tmax = std::numeric_limits<float>::max();
    for (int i = 0; i < 3; ++i) {
        float o = axis(origin, i), d = axis(dir, i), lo = axis(box.min, i), hi = axis(box.max, i);
        if (std::fabs(d) < 1e-8f) {
            if (o < lo || o > hi) return false;
            continue;
        }
        float t1 = (lo - o) / d, t2 = (hi - o) / d;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    tHit = tmin;
    return true;
}

float distanceToAabb(const Vec3& p, const Aabb& box) {
    float dx = std::max({box.min.x - p.x, 0.f, p.x - box.max.x});
    float dy = std::max({box.min.y - p.y, 0.f, p.y - box.max.y});
    float dz = std::max({box.min.z - p.z, 0.f, p.z - box.max.z});
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vec3 lookDirection(float yawDeg, float pitchDeg) {
    float yaw = yawDeg * kDegToRad, pitch = pitchDeg * kDegToRad;
    return {-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
}

Aabb targetBox(const ReachConfig& cfg, const Vec3& feet) {
    float hw = cfg.targetWidth * 0.5f + cfg.hitboxExpand;
    return {{feet.x - hw, feet.y - cfg.hitboxExpand, feet.z - hw},
            {feet.x + hw, feet.y + cfg.targetHeight + cfg.hitboxExpand, feet.z + hw}};
}

ReachResult evaluate(const ReachConfig& cfg, const Vec3& attackerFeet, float yawDeg, float pitchDeg,
                     const Vec3& targetFeet) {
    ReachResult r;
    Vec3 eye{attackerFeet.x, attackerFeet.y + cfg.eyeHeight, attackerFeet.z};
    Aabb box = targetBox(cfg, targetFeet);
    float dx = targetFeet.x - attackerFeet.x, dz = targetFeet.z - attackerFeet.z;
    r.feetDistance = std::sqrt(dx * dx + dz * dz);
    r.eyeToHitbox = distanceToAabb(eye, box);
    float t = 0;
    r.rayHits = rayAabb(eye, lookDirection(yawDeg, pitchDeg), box, t);
    r.rayDistance = r.rayHits ? t : 0.f;
    r.inReach = r.rayHits && t <= cfg.reach;
    return r;
}

namespace {
// Angles (yaw, pitch) visant le torse d'une cible placée sur +Z.
void aimAt(const ReachConfig& cfg, float distance, float& yaw, float& pitch) {
    float dy = cfg.targetHeight * 0.6f - cfg.eyeHeight;
    yaw = 0.f;
    pitch = -std::atan2(dy, distance) / kDegToRad;
}
} // namespace

ReachResult evaluateAtDistance(const ReachConfig& cfg, float distance) {
    float yaw, pitch;
    aimAt(cfg, distance, yaw, pitch);
    return evaluate(cfg, {0, 0, 0}, yaw, pitch, {0, 0, distance});
}

float hitRate(const ReachConfig& cfg, float distance, float jitterDeg, int samples, uint32_t seed) {
    if (samples <= 0) return 0.f;
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(0.f, std::max(0.f, jitterDeg));
    float yaw, pitch;
    aimAt(cfg, distance, yaw, pitch);
    int hits = 0;
    for (int i = 0; i < samples; ++i) {
        float jy = jitterDeg > 0 ? noise(rng) : 0.f, jp = jitterDeg > 0 ? noise(rng) : 0.f;
        hits += evaluate(cfg, {0, 0, 0}, yaw + jy, pitch + jp, {0, 0, distance}).inReach ? 1 : 0;
    }
    return static_cast<float>(hits) / static_cast<float>(samples);
}

float maxHitDistance(const ReachConfig& cfg) {
    float lo = 0.f, hi = cfg.reach + cfg.targetWidth + 2.f;
    for (int i = 0; i < 40; ++i) {
        float mid = (lo + hi) * 0.5f;
        if (evaluateAtDistance(cfg, mid).inReach) lo = mid;
        else hi = mid;
    }
    return lo;
}

} // namespace bcc::sim
