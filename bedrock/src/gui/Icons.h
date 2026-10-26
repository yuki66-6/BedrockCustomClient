#pragma once

#include <imgui.h>

namespace bcc {

// Icônes vectorielles dessinées à la volée (aucune police d'icônes requise).
enum class Icon {
    Generic,
    Logo,
    Sword,
    Target,
    Keyboard,
    Run,
    Speed,
    Jump,
    Crosshair,
    Monitor,
    Chart,
    Signal,
    Person,
    Clock,
    Shield,
    Gear,
    Folder,
    Ruler,
    Map,
    Star,
    Home,
    Flag,
    Pin,
    Skull,
};

namespace ui {
// Dessine `icon` centré sur `center`, dans un carré de côté `size`.
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 center, float size, ImU32 color);
} // namespace ui

} // namespace bcc
