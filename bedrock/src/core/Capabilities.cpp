#include "core/Capabilities.h"

namespace bcc {
namespace {

struct Entry {
    const char* id;
    const char* label;
};

constexpr Entry kCapabilities[] = {
    {"keyboard_input", "Clavier"},
    {"mouse_input", "Souris et CPS"},
    {"game_window", "Fenêtre du jeu"},
    {"game_fps", "FPS du jeu"},
    {"overlay_fps", "FPS de l'overlay"},
    {"server_latency", "Latence serveur"},
    {"player_position", "Position du joueur"},
    {"player_rotation", "Orientation du joueur"},
    {"player_velocity", "Vitesse du joueur"},
    {"player_health", "Santé"},
    {"player_hunger", "Faim"},
    {"player_armor", "Armure et durabilité"},
    {"inventory", "Inventaire"},
    {"potion_effects", "Effets de potion actifs"},
    {"dimension", "Dimension actuelle"},
    {"world_identity", "Identité du monde / serveur"},
    {"world_seed", "Seed du monde"},
    {"saved_world_map", "Carte depuis les sauvegardes"},
    {"death_events", "Événements de mort"},
    {"in_world_render", "Rendu dans le monde"},
    {"fov_control", "Contrôle du champ de vision"},
    {"movement_toggles", "Bascule sprint / accroupi"},
    {"chat_commands", "Commandes de chat autorisées"},
};
static_assert(std::size(kCapabilities) == static_cast<size_t>(Capability::Count), "liste de capacités incomplète");

} // namespace

const char* capabilityId(Capability c) {
    size_t i = static_cast<size_t>(c);
    return i < std::size(kCapabilities) ? kCapabilities[i].id : "?";
}

const char* capabilityLabel(Capability c) {
    size_t i = static_cast<size_t>(c);
    return i < std::size(kCapabilities) ? kCapabilities[i].label : "?";
}

bool capabilityFromId(std::string_view id, Capability& out) {
    for (size_t i = 0; i < std::size(kCapabilities); ++i)
        if (id == kCapabilities[i].id) {
            out = static_cast<Capability>(i);
            return true;
        }
    return false;
}

std::vector<Capability> allCapabilities() {
    std::vector<Capability> out;
    for (size_t i = 0; i < static_cast<size_t>(Capability::Count); ++i) out.push_back(static_cast<Capability>(i));
    return out;
}

const char* availabilityId(Availability a) {
    switch (a) {
    case Availability::Available: return "available";
    case Availability::Stale: return "stale";
    case Availability::Unavailable: return "unavailable";
    case Availability::Unsupported: return "unsupported";
    case Availability::PermissionRequired: return "permission_required";
    }
    return "?";
}

const char* availabilityLabel(Availability a) {
    switch (a) {
    case Availability::Available: return "Disponible";
    case Availability::Stale: return "Périmé";
    case Availability::Unavailable: return "Indisponible";
    case Availability::Unsupported: return "Non pris en charge";
    case Availability::PermissionRequired: return "Permission requise";
    }
    return "?";
}

const char* dataSourceId(DataSource s) {
    switch (s) {
    case DataSource::GameApi: return "game_api";
    case DataSource::RawInput: return "raw_input";
    case DataSource::WindowManager: return "window_manager";
    case DataSource::ServerQuery: return "server_query";
    case DataSource::SaveFile: return "save_file";
    case DataSource::Overlay: return "overlay";
    case DataSource::Simulation: return "simulation";
    case DataSource::User: return "user";
    case DataSource::None: return "none";
    }
    return "?";
}

} // namespace bcc
