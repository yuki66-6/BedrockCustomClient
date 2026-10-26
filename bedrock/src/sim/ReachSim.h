#pragma once

#include <cstdint>

// Simulateur local de portée d'attaque (Reach Lab). Purement géométrique :
// il ne communique avec aucun jeu ni serveur et ne modifie rien en jeu.
namespace bcc::sim {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Aabb {
    Vec3 min, max;
};

// Rayon / boîte (méthode des « slabs »). Retourne true et la distance d'entrée
// `tHit` (≥ 0) si le rayon touche la boîte.
bool rayAabb(const Vec3& origin, const Vec3& dir, const Aabb& box, float& tHit);
// Distance du point à la boîte (0 si à l'intérieur).
float distanceToAabb(const Vec3& p, const Aabb& box);
// Direction de visée façon Minecraft : yaw 0 = +Z, pitch positif = vers le bas.
Vec3 lookDirection(float yawDeg, float pitchDeg);

struct ReachConfig {
    float reach = 3.0f;        // portée configurée dans le simulateur (blocs)
    float hitboxExpand = 0.0f; // marge ajoutée à chaque côté de la hitbox
    float targetWidth = 0.6f;  // hitbox d'un joueur
    float targetHeight = 1.8f;
    float eyeHeight = 1.62f;
};

struct ReachResult {
    float feetDistance = 0;  // distance horizontale pieds → pieds (ce qu'on « voit » au sol)
    float eyeToHitbox = 0;   // distance œil → point le plus proche de la hitbox
    bool rayHits = false;    // le regard croise la hitbox
    float rayDistance = 0;   // distance de l'impact le long du regard
    bool inReach = false;    // touche avec la portée configurée
};

Aabb targetBox(const ReachConfig& cfg, const Vec3& feet);
ReachResult evaluate(const ReachConfig& cfg, const Vec3& attackerFeet, float yawDeg, float pitchDeg,
                     const Vec3& targetFeet);

// Visée parfaite vers le torse d'une cible située à `distance` blocs (pieds à pieds).
ReachResult evaluateAtDistance(const ReachConfig& cfg, float distance);

// Taux de coups au but (0..1) pour une cible à `distance`, avec une imprécision
// de visée gaussienne d'écart-type `jitterDeg`. Déterministe pour une graine donnée.
float hitRate(const ReachConfig& cfg, float distance, float jitterDeg, int samples, uint32_t seed);

// Distance maximale (pieds à pieds) à laquelle une visée parfaite touche encore.
float maxHitDistance(const ReachConfig& cfg);

} // namespace bcc::sim
