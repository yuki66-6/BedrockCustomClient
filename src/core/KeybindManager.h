#pragma once

#include "core/EventBus.h"

#include <string>
#include <vector>

namespace bcc {

class ModuleManager;
class Module;
class KeySetting;
struct KeyEvent;

// Raccourcis des modules et capture d'une nouvelle touche depuis la ClickGUI.
// Les touches sont observées (Raw Input) sans jamais être bloquées pour le jeu.
class KeybindManager {
public:
    KeybindManager(EventBus& bus, ModuleManager& modules);

    // Capture : la prochaine touche pressée est affectée à `target`.
    // Échap annule, Retour arrière/Suppr effacent le raccourci.
    void beginCapture(KeySetting* target);
    void cancelCapture();
    bool capturing() const { return target_ != nullptr; }
    const KeySetting* captureTarget() const { return target_; }
    // Vrai pendant un court instant après une capture (évite qu'Échap ferme le menu).
    bool recentlyCaptured(double now) const { return now - lastCaptureEnd_ < 0.25; }

    // Les raccourcis des modules ne s'appliquent que si le jeu a le focus
    // (sauf réglage contraire) et jamais pendant que le menu est ouvert.
    void setSuspended(bool suspended) { suspended_ = suspended; }
    void setRequireGameFocus(bool require) { requireGameFocus_ = require; }
    void setReservedKey(int vk) { reservedVk_ = vk; } // touche du menu
    void setClock(double now) { now_ = now; }

    std::vector<Module*> modulesBoundTo(int vk) const;
    std::string lastMessage() const { return lastMessage_; }

    static std::string keyName(int vk);

private:
    void onKey(const KeyEvent& e);

    ModuleManager& modules_;
    Subscription sub_;
    KeySetting* target_ = nullptr;
    bool suspended_ = false;
    bool requireGameFocus_ = true;
    int reservedVk_ = 0;
    double now_ = 0;
    double lastCaptureEnd_ = -10;
    std::string lastMessage_;
};

} // namespace bcc
