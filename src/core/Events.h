#pragma once

#include <string>

namespace bcc {

class Module;

// Touche du clavier observée (Raw Input, sans interception : le jeu la reçoit aussi).
struct KeyEvent {
    int vk = 0;
    bool down = false;
    bool repeat = false;       // auto-répétition d'une touche maintenue
    bool gameFocused = false;  // Minecraft était au premier plan
    bool overlayFocused = false;
};

enum class MouseButton { Left = 0, Right = 1, Middle = 2, X1 = 3, X2 = 4 };

struct MouseButtonEvent {
    MouseButton button = MouseButton::Left;
    bool down = false;
    bool gameFocused = false;
    double time = 0;
};

struct ModuleToggledEvent {
    Module* module = nullptr;
    bool enabled = false;
};

struct MenuToggledEvent {
    bool open = false;
};

struct ProfileLoadedEvent {
    std::string name;
};

} // namespace bcc
