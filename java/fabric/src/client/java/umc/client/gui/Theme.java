package umc.client.gui;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import umc.common.settings.Settings;
import umc.common.settings.SettingsGroup;

import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Thème du menu et du HUD (section « theme » commune avec Bedrock). Les réglages
 * propres à l'overlay Bedrock (inclinaison 3D, police…) sont ignorés sans avertissement.
 */
public final class Theme extends SettingsGroup {
	private static final Set<String> BEDROCK_ONLY = Set.of("preset", "rounding", "ui_scale", "font_size", "tilt", "parallax");

	public final Settings.Color accent = add(new Settings.Color("accent", "Couleur d'accent", "Couleur principale de l'interface", 0xFF458AFF));
	public final Settings.Float panelAlpha = add(new Settings.Float("panel_alpha", "Opacité des panneaux", "Transparence du menu", 0.94f, 0.5f, 1f, "%.2f"));
	public final Settings.Bool animations = add(new Settings.Bool("animations", "Animations", "Ouverture, survols et interrupteurs animés", true));
	public final Settings.Float animSpeed = add(new Settings.Float("anim_speed", "Vitesse des animations", "Plus élevé = plus rapide", 12f, 4f, 30f, "%.0f"));
	public final Settings.Float hudBgAlpha = add(new Settings.Float("hud_bg_alpha", "Fond du HUD", "Opacité du fond des éléments HUD", 0.55f, 0f, 1f, "%.2f"));
	public final Settings.Bool dimBackground = add(new Settings.Bool("dim_background", "Assombrir le fond", "Voile sombre derrière le menu ouvert", true));
	public final Settings.Bool textShadow = add(new Settings.Bool("text_shadow", "Ombre du texte", "Ombre portée sous le texte du HUD", true));

	public int accent() {
		return accent.argb() | 0xFF000000;
	}

	public int panel() {
		return Draw.withAlpha(0xFF15171E, panelAlpha.value());
	}

	public int sidebar() {
		return Draw.withAlpha(0xFF0F1116, panelAlpha.value());
	}

	public int card() {
		return Draw.withAlpha(0xFF1E212B, panelAlpha.value());
	}

	public int cardHover() {
		return Draw.withAlpha(0xFF272B37, panelAlpha.value());
	}

	public int border() {
		return 0x33FFFFFF;
	}

	public int text() {
		return 0xFFE8EAF0;
	}

	public int textDim() {
		return 0xFF9EA3B3;
	}

	public int good() {
		return 0xFF4DDB85;
	}

	public int warn() {
		return 0xFFFFB84D;
	}

	public int bad() {
		return 0xFFF2525A;
	}

	public int hudBg() {
		return Draw.withAlpha(0xFF000000, hudBgAlpha.value());
	}

	/** Facteur de lissage par image pour les animations (1 = instantané). */
	public float lerpFactor(float deltaSeconds) {
		if (!animations.on()) return 1f;
		return Math.min(1f, deltaSeconds * animSpeed.value());
	}

	public List<String> load(JsonElement json) {
		if (json != null && json.isJsonObject()) {
			JsonObject filtered = new JsonObject();
			for (Map.Entry<String, JsonElement> e : json.getAsJsonObject().entrySet())
				if (!BEDROCK_ONLY.contains(e.getKey())) filtered.add(e.getKey(), e.getValue());
			json = filtered;
		}
		return settingsFromJson(json, "thème");
	}
}
