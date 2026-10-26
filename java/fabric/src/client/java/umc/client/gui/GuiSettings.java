package umc.client.gui;

import umc.common.settings.Settings;
import umc.common.settings.SettingsGroup;

/** Réglages du menu propres au mod Java (section « java/gui » du profil). */
public final class GuiSettings extends SettingsGroup {
	public final Settings.Int width = add(new Settings.Int("width", "Largeur du menu", "Largeur de la fenêtre (pixels GUI)", 440, 300, 1200, "%d"));
	public final Settings.Int height = add(new Settings.Int("height", "Hauteur du menu", "Hauteur de la fenêtre (pixels GUI)", 270, 190, 800, "%d"));
	public final Settings.Bool pauseGame = add(new Settings.Bool("pause_game", "Mettre le jeu en pause", "En solo, ouvrir le menu met la partie en pause", false));
	public final Settings.Bool autosave = add(new Settings.Bool("autosave", "Sauvegarde automatique", "Enregistre le profil 1,5 s après chaque modification", true));
	public final Settings.Bool rememberPage = add(new Settings.Bool("remember_page", "Rouvrir la dernière page", "Le menu se rouvre sur la page quittée", true));
}
