#pragma once

#include <windows.h>

namespace bcc {

// Icône de zone de notification : l'overlay n'a pas de bouton dans la barre
// des tâches, c'est donc le moyen d'ouvrir le menu ou de quitter.
class TrayIcon {
public:
    static constexpr UINT kCallbackMessage = WM_APP + 1;
    enum Command : UINT { CmdOpenMenu = 1001, CmdLaunchMinecraft, CmdOpenConfigs, CmdQuit };

    bool create(HWND owner, const wchar_t* tooltip);
    void destroy();
    // À appeler sur kCallbackMessage. Retourne la commande choisie (0 sinon).
    UINT handleMessage(LPARAM lParam);
    // Recrée l'icône si l'Explorateur a redémarré (message TaskbarCreated).
    void handleTaskbarCreated(UINT msg);

private:
    HWND owner_ = nullptr;
    bool created_ = false;
    UINT taskbarCreatedMsg_ = 0;
    wchar_t tooltip_[128] = {};
};

} // namespace bcc
