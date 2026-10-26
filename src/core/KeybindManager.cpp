#include "core/KeybindManager.h"

#include "core/Events.h"
#include "core/ModuleManager.h"
#include "utils/Strings.h"

#include <windows.h>

#include <format>

namespace bcc {

KeybindManager::KeybindManager(EventBus& bus, ModuleManager& modules) : modules_(modules) {
    sub_ = Subscription(bus, bus.subscribe<KeyEvent>([this](const KeyEvent& e) { onKey(e); }));
}

void KeybindManager::beginCapture(KeySetting* target) {
    target_ = target;
    lastMessage_.clear();
}

void KeybindManager::cancelCapture() {
    if (!target_) return;
    target_ = nullptr;
    lastCaptureEnd_ = now_;
}

std::vector<Module*> KeybindManager::modulesBoundTo(int vk) const {
    std::vector<Module*> out;
    if (!vk) return out;
    for (auto& m : modules_.all())
        if (m->keybind().get() == vk) out.push_back(m.get());
    return out;
}

void KeybindManager::onKey(const KeyEvent& e) {
    if (!e.down || e.repeat) return;
    if (target_) {
        if (e.vk == VK_ESCAPE) {
            lastMessage_ = "Capture annulée";
        } else if (e.vk == VK_BACK || e.vk == VK_DELETE) {
            target_->set(0);
            lastMessage_ = "Raccourci effacé";
        } else if (e.vk == reservedVk_ && target_->id() != "menu_key") {
            lastMessage_ = std::format("{} est réservé à l'ouverture du menu", keyName(e.vk));
        } else {
            target_->set(e.vk);
            lastMessage_ = std::format("Raccourci : {}", keyName(e.vk));
        }
        target_ = nullptr;
        lastCaptureEnd_ = now_;
        return;
    }
    if (suspended_ || (requireGameFocus_ && !e.gameFocused)) return;
    for (Module* m : modulesBoundTo(e.vk)) m->toggle();
}

std::string KeybindManager::keyName(int vk) {
    if (vk <= 0) return "Aucune";
    switch (vk) {
    case VK_LBUTTON: return "Clic gauche";
    case VK_RBUTTON: return "Clic droit";
    case VK_MBUTTON: return "Clic molette";
    case VK_XBUTTON1: return "Souris 4";
    case VK_XBUTTON2: return "Souris 5";
    case VK_SPACE: return "Espace";
    case VK_LSHIFT: case VK_SHIFT: return "Maj gauche";
    case VK_RSHIFT: return "Maj droite";
    case VK_LCONTROL: case VK_CONTROL: return "Ctrl gauche";
    case VK_RCONTROL: return "Ctrl droit";
    case VK_LMENU: case VK_MENU: return "Alt";
    case VK_RMENU: return "Alt Gr";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Suppr";
    case VK_HOME: return "Début";
    case VK_END: return "Fin";
    case VK_PRIOR: return "Page préc.";
    case VK_NEXT: return "Page suiv.";
    case VK_LEFT: return "Gauche";
    case VK_RIGHT: return "Droite";
    case VK_UP: return "Haut";
    case VK_DOWN: return "Bas";
    case VK_RETURN: return "Entrée";
    case VK_TAB: return "Tab";
    case VK_ESCAPE: return "Échap";
    case VK_BACK: return "Retour arr.";
    default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F24) return std::format("F{}", vk - VK_F1 + 1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return std::format("Pavé {}", vk - VK_NUMPAD0);
    // Nom localisé selon la disposition du clavier (AZERTY, QWERTY...).
    UINT scan = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    wchar_t name[64] = {};
    if (scan && GetKeyNameTextW(static_cast<LONG>(scan << 16), name, 64) > 0) return toUtf8(name);
    return std::format("VK {}", vk);
}

} // namespace bcc
