package umc.client.module;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.common.settings.Setting;
import umc.common.settings.SettingsGroup;

/**
 * Base des modules du mod Fabric : identifiant commun avec Bedrock, réglages,
 * état, raccourci (code GLFW, -1 = aucun), cycle de vie et HUD optionnel.
 */
public abstract class Module extends SettingsGroup {
	public static final int NO_KEY = -1;

	private final String id;
	private final String name;
	private final String description;
	private final Category category;
	private final boolean enabledByDefault;
	private boolean enabled;
	private int keybind = NO_KEY;
	private boolean favorite;
	private String lastError = "";

	protected Module(String id, String name, String description, Category category, boolean enabledByDefault) {
		this.id = id;
		this.name = name;
		this.description = description;
		this.category = category;
		this.enabledByDefault = enabledByDefault;
		this.enabled = enabledByDefault;
	}

	public String id() { return id; }
	public String name() { return name; }
	public String description() { return description; }
	public Category category() { return category; }
	public boolean enabled() { return enabled; }
	public int keybind() { return keybind; }
	public boolean favorite() { return favorite; }
	/** Dernière erreur d'exécution (affichée sur la carte du module). */
	public String lastError() { return lastError; }

	public void setEnabled(boolean value) {
		if (value == enabled) return;
		enabled = value;
		try {
			if (enabled) onEnable();
			else onDisable();
			lastError = "";
		} catch (RuntimeException e) {
			reportError(e);
		}
		Setting.bumpRevision();
	}

	public void toggle() { setEnabled(!enabled); }

	public void setKeybind(int key) {
		if (key == keybind) return;
		keybind = key;
		Setting.bumpRevision();
	}

	public void setFavorite(boolean value) {
		if (value == favorite) return;
		favorite = value;
		Setting.bumpRevision();
	}

	public void resetToDefaults() {
		resetSettings();
		setEnabled(enabledByDefault);
		keybind = NO_KEY;
	}

	/** Appelé à chaque tick client (20/s) si le module est actif, avec un monde chargé. */
	public final void tick(Minecraft mc) {
		try {
			onTick(mc);
		} catch (RuntimeException e) {
			reportError(e);
		}
	}

	/** Enregistre une erreur d'exécution sans interrompre le jeu. */
	public void reportError(Throwable e) {
		lastError = e.getClass().getSimpleName() + " : " + e.getMessage();
	}

	protected void onEnable() {}
	protected void onDisable() {}
	protected void onTick(Minecraft mc) {}

	/** Élément HUD déplaçable. */
	public boolean hasHud() { return false; }
	/** Position par défaut (fraction de l'écran). */
	public float defaultX() { return 0.01f; }
	public float defaultY() { return 0.3f; }
	/** Dessine le HUD à l'origine (0,0) ; retourne la taille {largeur, hauteur} en pixels GUI. */
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) { return new int[] {0, 0}; }

	/** Limitation technique affichée à l'utilisateur (vide si aucune). */
	public String limitation() { return ""; }
}
