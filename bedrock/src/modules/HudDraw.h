#pragma once

#include "core/Setting.h"

#include <imgui.h>

#include <string>

// Petits utilitaires de dessin partagés par les éléments HUD des modules.
namespace bcc::hud {

inline ImVec4 col(const Color& c) { return {c.r, c.g, c.b, c.a}; }

// « libellé  valeur » sur une ligne, valeur colorée.
void line(const char* label, const std::string& value, const ImVec4& valueColor = {1, 1, 1, 1});
// Valeur indisponible : « libellé  N/D » en orange, raison en infobulle (mode édition).
void unavailable(const char* label, const char* reason);
// Texte centré dans un rectangle.
void centeredText(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, const char* text, float fontSize = 0.f);
ImVec4 mix(const ImVec4& a, const ImVec4& b, float t);
// Interpolation exponentielle indépendante du FPS.
float approach(float current, float target, float dt, float speed = 14.f);
std::string formatDuration(double seconds);
// Libellé court d'une touche pour les keystrokes (« Z », « Espace »...).
std::string shortKeyName(int vk);
// Touche physique WASD/ZQSD selon la disposition active (codes de balayage).
int layoutKey(unsigned scanCode);

} // namespace bcc::hud
