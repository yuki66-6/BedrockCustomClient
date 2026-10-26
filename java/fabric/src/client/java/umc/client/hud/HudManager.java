package umc.client.hud;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.gui.Theme;
import umc.client.module.Module;
import umc.client.module.ModuleManager;
import umc.client.telemetry.GameAdapter;
import umc.common.settings.Setting;
import umc.common.settings.Settings;
import umc.common.settings.SettingsGroup;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Position et échelle des éléments HUD (section « hud », même format que Bedrock :
 * {@code layout: {id: {x, y, scale}}} avec x, y en fraction de l'écran, -1 = défaut).
 */
public final class HudManager extends SettingsGroup {
	public final Settings.Float scale = add(new Settings.Float("scale", "Taille du HUD", "Échelle globale des éléments HUD", 1f, 0.5f, 2.5f, "x%.2f"));
	public final Settings.Bool background = add(new Settings.Bool("background", "Fond des éléments", "Affiche un fond translucide sous le HUD", true));
	public final Settings.Bool snap = add(new Settings.Bool("snap", "Aimanter aux bords", "Colle les éléments aux bords de l'écran pendant le déplacement", true));

	public static final class Layout {
		public float x = -1, y = -1, scale = 1;
	}

	/** Rectangle occupé par un élément à la dernière image (pixels GUI). */
	public record Box(Module module, int x, int y, int w, int h) {
	}

	private final ModuleManager modules;
	private final Theme theme;
	private final GameAdapter adapter;
	private final Map<String, Layout> layouts = new LinkedHashMap<>();
	private final Map<String, int[]> sizes = new HashMap<>();
	private final List<Box> boxes = new ArrayList<>();

	public HudManager(ModuleManager modules, Theme theme, GameAdapter adapter) {
		this.modules = modules;
		this.theme = theme;
		this.adapter = adapter;
	}

	public List<Box> boxes() {
		return boxes;
	}

	public Layout layout(String id) {
		return layouts.computeIfAbsent(id, k -> new Layout());
	}

	public void resetLayout(String id) {
		layouts.remove(id);
		Setting.bumpRevision();
	}

	public float scaleOf(Module m) {
		Layout l = layouts.get(m.id());
		return scale.value() * (l == null ? 1f : l.scale);
	}

	/** Position du coin supérieur gauche, bornée à l'écran. */
	public int[] positionOf(Module m, int screenW, int screenH) {
		Layout l = layouts.get(m.id());
		float fx = l != null && l.x >= 0 ? l.x : m.defaultX();
		float fy = l != null && l.y >= 0 ? l.y : m.defaultY();
		int[] size = sizes.getOrDefault(m.id(), new int[] {40, 12});
		float s = scaleOf(m);
		int w = Math.round(size[0] * s), h = Math.round(size[1] * s);
		int x = Math.clamp(Math.round(fx * screenW), 0, Math.max(0, screenW - w));
		int y = Math.clamp(Math.round(fy * screenH), 0, Math.max(0, screenH - h));
		return new int[] {x, y, w, h};
	}

	public void render(GuiGraphicsExtractor g, boolean editing) {
		Minecraft mc = Minecraft.getInstance();
		boxes.clear();
		if (mc.player == null && !editing) return;
		HudContext ctx = new HudContext(mc, mc.font, theme, adapter, editing, background.on());
		int sw = g.guiWidth(), sh = g.guiHeight();
		for (Module m : modules.all()) {
			if (!m.hasHud() || !m.enabled()) continue;
			int[] pos = positionOf(m, sw, sh);
			float s = scaleOf(m);
			g.pose().pushMatrix();
			g.pose().translate(pos[0], pos[1]);
			g.pose().scale(s, s);
			try {
				int[] size = m.renderHud(g, ctx);
				if (size[0] > 0 && size[1] > 0) sizes.put(m.id(), size);
			} catch (RuntimeException e) {
				m.reportError(e);
			} finally {
				g.pose().popMatrix();
			}
			int[] size = sizes.getOrDefault(m.id(), new int[] {40, 12});
			boxes.add(new Box(m, pos[0], pos[1], Math.round(size[0] * s), Math.round(size[1] * s)));
		}
	}

	public JsonObject toJson() {
		JsonObject layout = new JsonObject();
		for (Map.Entry<String, Layout> e : layouts.entrySet()) {
			Layout l = e.getValue();
			if (l.x < 0 && l.scale == 1f) continue;
			JsonObject o = new JsonObject();
			o.addProperty("x", l.x);
			o.addProperty("y", l.y);
			o.addProperty("scale", l.scale);
			layout.add(e.getKey(), o);
		}
		JsonObject o = new JsonObject();
		o.add("settings", settingsToJson());
		o.add("layout", layout);
		return o;
	}

	public List<String> fromJson(JsonElement json) {
		List<String> warnings = new ArrayList<>();
		if (json == null || !json.isJsonObject()) {
			warnings.add("section hud invalide");
			return warnings;
		}
		JsonObject o = json.getAsJsonObject();
		if (o.has("settings")) warnings.addAll(settingsFromJson(o.get("settings"), "hud"));
		layouts.clear();
		if (o.has("layout") && o.get("layout").isJsonObject()) {
			for (Map.Entry<String, JsonElement> e : o.getAsJsonObject("layout").entrySet()) {
				if (!e.getValue().isJsonObject()) continue;
				JsonObject v = e.getValue().getAsJsonObject();
				Layout l = new Layout();
				l.x = num(v, "x", -1, -1, 1);
				l.y = num(v, "y", -1, -1, 1);
				l.scale = num(v, "scale", 1, 0.5f, 3f);
				layouts.put(e.getKey(), l);
			}
		}
		return warnings;
	}

	private static float num(JsonObject o, String key, float def, float min, float max) {
		if (!o.has(key) || !o.get(key).isJsonPrimitive() || !o.get(key).getAsJsonPrimitive().isNumber()) return def;
		float f = o.get(key).getAsFloat();
		return Float.isFinite(f) ? Math.clamp(f, min, max) : def;
	}

	public void resetAll() {
		resetSettings();
		layouts.clear();
	}
}
