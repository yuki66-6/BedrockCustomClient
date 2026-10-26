#pragma once

#include "core/Setting.h"

namespace bcc {

// Réglages généraux du client (onglet Settings > Général / Raccourcis).
class ClientSettings : public SettingsGroup {
public:
    ClientSettings();

    KeySetting* menuKey;
    BoolSetting* hudOnlyWhenGameFocused;
    BoolSetting* followGameWindow;
    BoolSetting* keybindsRequireGameFocus;
    IntSetting* fpsLimit;
    BoolSetting* autosave;
    FloatSetting* autosaveDelay;
    BoolSetting* startupNotification;
};

} // namespace bcc
