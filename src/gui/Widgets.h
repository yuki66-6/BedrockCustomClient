#pragma once

#include "core/Setting.h"

#include <imgui.h>

namespace bcc {
class ThemeManager;
class KeybindManager;
} // namespace bcc

// Widgets personnalisés de la ClickGUI (interrupteurs animés, sliders
// d'accent, sélecteur de touche...).
namespace bcc::ui {

void setTheme(ThemeManager* theme);
ThemeManager& theme();
void setKeybindManager(KeybindManager* keybinds);

// Valeur animée vers `target` (interpolation exponentielle indépendante du FPS).
float animate(ImGuiID id, float target, float speedMul = 1.f);

ImU32 colorU32(const ImVec4& c, float alphaMul = 1.f);

// Interrupteur pilule. Retourne true si l'utilisateur l'a basculé.
bool toggleSwitch(const char* strId, bool value, float height = 0.f);

// Slider fin à pleine largeur (piste + curseur). Double clic droit : défaut.
bool slider(const char* strId, float* value, float min, float max);

// Rendu générique d'un paramètre selon son type. Retourne true si modifié.
bool settingWidget(Setting& setting);
bool keybindButton(KeySetting& key, const char* label = nullptr);

void tooltip(const char* text);
void sectionTitle(const char* text);
void dimText(const char* fmt, ...);
// Bandeau d'avertissement pour une donnée non disponible.
void unavailableNote(const char* text);
// Place le prochain widget (largeur `width`) aligné à droite sur la ligne courante.
void sameLineRight(float width);

} // namespace bcc::ui
