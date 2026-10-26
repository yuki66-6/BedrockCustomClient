#pragma once

#include <windows.h>

#include <functional>

namespace bcc {

class EventBus;
class InputState;

// Lecture passive du clavier et de la souris via Raw Input (RIDEV_INPUTSINK) :
// documenté par Microsoft, aucun hook, aucune interception — Minecraft reçoit
// toujours les mêmes entrées. Les événements sont publiés sur l'EventBus.
class InputMonitor {
public:
    InputMonitor(EventBus& bus, InputState& state) : bus_(bus), state_(state) {}

    bool registerDevices(HWND target);
    void unregisterDevices();
    // À appeler sur WM_INPUT.
    void handleRawInput(LPARAM lParam, double now);

    // Fournit l'état « Minecraft au premier plan » / « overlay au premier plan ».
    void setFocusQuery(std::function<bool()> gameFocused, std::function<bool()> overlayFocused) {
        gameFocused_ = std::move(gameFocused);
        overlayFocused_ = std::move(overlayFocused);
    }
    unsigned long long eventsProcessed() const { return events_; }

    // Normalise les touches génériques (Maj/Ctrl/Alt) en gauche/droite.
    static int normalizeVk(int vk, unsigned short makeCode, bool e0);

private:
    EventBus& bus_;
    InputState& state_;
    std::function<bool()> gameFocused_, overlayFocused_;
    bool registered_ = false;
    unsigned long long events_ = 0;
};

} // namespace bcc
