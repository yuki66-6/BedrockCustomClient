package umc.common.modules;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import umc.common.profile.ProfileStore;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** Définitions communes des modules (shared/module-definitions/modules.json). */
public final class ModuleDefinitions {
	public record EditionStatus(String status, String versions, String note, String providedBy) {
		public boolean shown() {
			return !status.equals("planned") && !status.equals("unsupported");
		}
	}

	public record Definition(String id, String category, String name, String description, List<String> requiredCapabilities,
			EditionStatus java, EditionStatus bedrock) {
	}

	/** Identifiants des modules réellement enregistrés par le mod Fabric (vérifiés par les tests). */
	public static final List<String> JAVA_MODULE_IDS = List.of("keystrokes", "cps", "fps", "coordinates", "direction", "ping",
			"armor_hud", "potion_effects", "speed", "toggle_sprint", "toggle_sneak", "zoom", "waypoints", "death_waypoint",
			"crosshair", "totem_counter", "time_hud", "info_bar");

	private final Map<String, Definition> byId = new TreeMap<>();

	public static ModuleDefinitions parse(String json) {
		ModuleDefinitions d = new ModuleDefinitions();
		JsonElement el = ProfileStore.parseLenient(json);
		if (el == null || !el.isJsonObject() || !el.getAsJsonObject().has("modules")) return d;
		for (JsonElement m : el.getAsJsonObject().getAsJsonArray("modules")) {
			JsonObject o = m.getAsJsonObject();
			List<String> caps = new ArrayList<>();
			if (o.has("requiredCapabilities"))
				for (JsonElement c : o.getAsJsonArray("requiredCapabilities")) caps.add(c.getAsString());
			JsonObject ed = o.has("editions") ? o.getAsJsonObject("editions") : new JsonObject();
			d.byId.put(o.get("id").getAsString(), new Definition(o.get("id").getAsString(), str(o, "category"), localized(o, "name"),
					localized(o, "description"), caps, status(ed, "java"), status(ed, "bedrock")));
		}
		return d;
	}

	/** Lit les définitions embarquées dans le mod (copiées depuis shared/ à la compilation). */
	public static ModuleDefinitions fromClasspath() {
		try (InputStream in = ModuleDefinitions.class.getResourceAsStream("/assets/umc/shared/module-definitions/modules.json")) {
			if (in == null) return new ModuleDefinitions();
			return parse(new String(in.readAllBytes(), StandardCharsets.UTF_8));
		} catch (IOException e) {
			return new ModuleDefinitions();
		}
	}

	public Definition find(String id) {
		return byId.get(id);
	}

	public List<Definition> all() {
		return List.copyOf(byId.values());
	}

	private static String str(JsonObject o, String key) {
		return o.has(key) ? o.get(key).getAsString() : "";
	}

	private static String localized(JsonObject o, String key) {
		if (!o.has(key)) return "";
		JsonElement e = o.get(key);
		if (e.isJsonPrimitive()) return e.getAsString();
		JsonObject l = e.getAsJsonObject();
		return l.has("fr") ? l.get("fr").getAsString() : str(l, "en");
	}

	private static EditionStatus status(JsonObject editions, String edition) {
		if (!editions.has(edition)) return new EditionStatus("planned", "", "", "module");
		JsonObject o = editions.getAsJsonObject(edition);
		return new EditionStatus(o.has("status") ? o.get("status").getAsString() : "planned", str(o, "versions"), str(o, "note"),
				o.has("providedBy") ? o.get("providedBy").getAsString() : "module");
	}
}
