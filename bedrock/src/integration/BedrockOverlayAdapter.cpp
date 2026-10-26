#include "integration/BedrockOverlayAdapter.h"

#include "integration/TelemetryProvider.h"

namespace bcc {
namespace {

constexpr const char* kMemoryOnly =
    "Donnée présente uniquement dans la mémoire du jeu : un overlay externe non intrusif ne peut pas la lire.";

} // namespace

std::vector<CapabilityInfo> GameAdapter::capabilities() const {
    std::vector<CapabilityInfo> out;
    for (Capability c : allCapabilities()) out.push_back(capability(c));
    return out;
}

CapabilityInfo BedrockOverlayAdapter::capability(Capability c) const {
    CapabilityInfo i;
    i.capability = c;
    auto set = [&](Availability a, DataSource s, std::string reason = {}) {
        i.availability = a;
        i.source = s;
        i.reason = std::move(reason);
        return i;
    };
    const GameState& g = t_.game();
    switch (c) {
    case Capability::KeyboardInput:
    case Capability::MouseInput: return set(Availability::Available, DataSource::RawInput, "Raw Input, lecture passive");
    case Capability::GameWindow:
        return g.found ? set(Availability::Available, DataSource::WindowManager)
                       : set(Availability::Unavailable, DataSource::WindowManager, "Fenêtre Minecraft non détectée.");
    case Capability::OverlayFps: return set(Availability::Available, DataSource::Overlay);
    case Capability::ServerLatency: {
        PingResult p = t_.ping();
        if (p.valid) return set(Availability::Available, DataSource::ServerQuery, "Requête publique RakNet (pas le ping interne du jeu).");
        return set(Availability::Unavailable, DataSource::ServerQuery,
                   p.host.empty() ? "Configure un serveur dans le module Ping." : "Aucune réponse du serveur : " + p.error);
    }
    case Capability::SavedWorldMap:
        return set(Availability::Available, DataSource::SaveFile, "Mondes solo locaux, lecture seule, état de la dernière sauvegarde.");
    case Capability::WorldIdentity:
        return set(Availability::Unavailable, DataSource::SaveFile,
                   "Monde solo deviné d'après la dernière sauvegarde écrite ; serveur/Realm inconnus.");
    case Capability::WorldSeed:
        return set(Availability::Unavailable, DataSource::SaveFile,
                   "Lisible seulement dans level.dat d'un monde solo local ; jamais pour un serveur ou un Realm.");
    case Capability::GameFps:
        return set(Availability::Unsupported, DataSource::None,
                   "Non exposé aux autres applications (exigerait une injection ou ETW avec droits administrateur).");
    case Capability::InWorldRender:
        return set(Availability::Unsupported, DataSource::None, "Dessiner dans le monde exigerait de modifier le rendu du jeu.");
    case Capability::FovControl:
        return set(Availability::Unsupported, DataSource::None, "Réglage interne du jeu, non modifiable de l'extérieur.");
    case Capability::MovementToggles:
        return set(Availability::Unsupported, DataSource::None,
                   "Utilise les options « Basculer le sprint / l'accroupissement » de Minecraft Bedrock.");
    case Capability::ChatCommands:
        return set(Availability::Unsupported, DataSource::None, "Aucune interface autorisée pour envoyer des commandes depuis l'overlay.");
    case Capability::PlayerPosition:
    case Capability::PlayerRotation:
    case Capability::PlayerVelocity:
    case Capability::PlayerHealth:
    case Capability::PlayerHunger:
    case Capability::PlayerArmor:
    case Capability::Inventory:
    case Capability::PotionEffects:
    case Capability::Dimension:
    case Capability::DeathEvents: return set(Availability::Unsupported, DataSource::None, kMemoryOnly);
    default: break;
    }
    return set(Availability::Unsupported, DataSource::None, "Capacité inconnue.");
}

DataPoint<int> BedrockOverlayAdapter::clicksPerSecond(bool left) const {
    int v = t_.input().cps(left ? MouseButton::Left : MouseButton::Right, t_.now());
    return DataPoint<int>::available(v, DataSource::RawInput, t_.now(), 1.0);
}

DataPoint<float> BedrockOverlayAdapter::gameFps() const {
    return DataPoint<float>::missing(Availability::Unsupported, TelemetryProvider::kGameFpsUnavailable);
}

DataPoint<float> BedrockOverlayAdapter::overlayFps() const {
    return DataPoint<float>::available(t_.overlayFps(), DataSource::Overlay, t_.now(), 2.0);
}

DataPoint<int> BedrockOverlayAdapter::latencyMs() const {
    PingResult p = t_.ping();
    if (!p.valid) return DataPoint<int>::missing(Availability::Unavailable, p.host.empty() ? "Aucun serveur configuré." : p.error,
                                                 DataSource::ServerQuery);
    // L'âge de la mesure fixe l'horodatage ; au-delà de 15 s elle est considérée périmée.
    return DataPoint<int>::available(p.ms, DataSource::ServerQuery, t_.now() - p.ageSeconds, 15.0);
}

DataPoint<Vec3d> BedrockOverlayAdapter::playerPosition() const {
    return DataPoint<Vec3d>::missing(Availability::Unsupported, TelemetryProvider::kPlayerDataUnavailable);
}

DataPoint<std::string> BedrockOverlayAdapter::worldIdentity() const {
    return DataPoint<std::string>::missing(Availability::Unavailable, capability(Capability::WorldIdentity).reason,
                                           DataSource::SaveFile);
}

DataPoint<long long> BedrockOverlayAdapter::worldSeed() const {
    return DataPoint<long long>::missing(Availability::Unavailable, capability(Capability::WorldSeed).reason, DataSource::SaveFile);
}

} // namespace bcc
