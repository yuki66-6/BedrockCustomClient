#pragma once

#include "core/Events.h"

#include <array>
#include <cstdint>
#include <deque>

namespace bcc {

// Clics par seconde sur une fenêtre glissante d'une seconde.
class CpsCounter {
public:
    void add(double t);
    int count(double now); // purge les clics de plus d'une seconde
    int peak() const { return peak_; }
    uint64_t total() const { return total_; }
    void reset();

private:
    std::deque<double> clicks_;
    int peak_ = 0;
    uint64_t total_ = 0;
};

// État clavier/souris observé (Raw Input). Ne contient que des données réelles
// issues des périphériques de l'utilisateur.
class InputState {
public:
    // Retourne true s'il s'agit d'une auto-répétition (touche déjà enfoncée).
    bool onKey(int vk, bool down, double t, bool gameFocused);
    void onMouseButton(MouseButton button, bool down, double t, bool gameFocused);

    bool keyDown(int vk) const { return vk > 0 && vk < 256 && keys_[static_cast<size_t>(vk)]; }
    bool mouseDown(MouseButton b) const { return mouse_[static_cast<size_t>(b)]; }
    int cps(MouseButton b, double now) { return counter(b).count(now); }
    CpsCounter& counter(MouseButton b) { return b == MouseButton::Right ? right_ : left_; }
    uint64_t keyPresses() const { return keyPresses_; }

    // Si vrai, seuls les clics faits pendant que Minecraft a le focus comptent.
    void setCountOnlyInGame(bool v) { countOnlyInGame_ = v; }
    void releaseAll(); // perte de focus : évite les touches « collées »
    void resetStats();

private:
    std::array<bool, 256> keys_{};
    std::array<bool, 5> mouse_{};
    CpsCounter left_, right_;
    uint64_t keyPresses_ = 0;
    bool countOnlyInGame_ = true;
};

} // namespace bcc
