#include "core/ClientSettings.h"

#include <windows.h>

namespace bcc {

ClientSettings::ClientSettings() {
    menuKey = add<KeySetting>("menu_key", "Touche du menu", "Ouvre/ferme la ClickGUI", VK_RSHIFT);
    hudOnlyWhenGameFocused = add<BoolSetting>(
        "hud_only_when_game_focused", "HUD seulement en jeu",
        "Masque le HUD quand Minecraft n'est pas la fenêtre active", true);
    followGameWindow = add<BoolSetting>("follow_game_window", "Suivre la fenêtre du jeu",
                                        "Aligne l'overlay sur la zone cliente de Minecraft", true);
    keybindsRequireGameFocus = add<BoolSetting>("keybinds_require_game_focus", "Raccourcis seulement en jeu",
                                                "Les raccourcis des modules n'agissent que si Minecraft est actif",
                                                true);
    fpsLimit = add<IntSetting>("fps_limit", "Limite de FPS de l'overlay",
                               "0 = synchronisé sur l'écran (V-Sync). Réduit l'usage CPU/GPU.", 0, 0, 240,
                               "%d FPS");
    autosave = add<BoolSetting>("autosave", "Sauvegarde automatique", "Enregistre le profil après chaque modification",
                                true);
    autosaveDelay = add<FloatSetting>("autosave_delay", "Délai de sauvegarde", "Attente après la dernière modification",
                                      1.5f, 0.5f, 10.f, "%.1f s");
    autosaveDelay->setVisibleIf([this] { return autosave->get(); });
    startupNotification = add<BoolSetting>("startup_notification", "Notification au démarrage",
                                           "Affiche un rappel de la touche du menu au lancement", true);
}

} // namespace bcc
