package umc.client.module;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import umc.common.profile.ProfileStore;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.function.Predicate;

/** Registre des modules : enregistrement, recherche, (dé)sérialisation dans les profils. */
public final class ModuleManager {
	private final List<Module> modules = new ArrayList<>();

	public <M extends Module> M register(M module) {
		if (find(module.id()) != null) throw new IllegalArgumentException("identifiant de module en double : " + module.id());
		if (!module.id().matches("[a-z0-9_]{1,48}")) throw new IllegalArgumentException("identifiant invalide : " + module.id());
		modules.add(module);
		return module;
	}

	public List<Module> all() {
		return Collections.unmodifiableList(modules);
	}

	public Module find(String id) {
		for (Module m : modules)
			if (m.id().equals(id)) return m;
		return null;
	}

	@SuppressWarnings("unchecked")
	public <M extends Module> M get(Class<M> type) {
		for (Module m : modules)
			if (type.isInstance(m)) return (M) m;
		return null;
	}

	public List<Module> byCategory(Category c) {
		List<Module> out = new ArrayList<>();
		for (Module m : modules)
			if (m.category() == c) out.add(m);
		return out;
	}

	/** Section « modules » du profil : état, favori et réglages (communs avec Bedrock). */
	public JsonObject toJson() {
		JsonObject o = new JsonObject();
		for (Module m : modules) {
			JsonObject mo = new JsonObject();
			mo.addProperty("enabled", m.enabled());
			if (m.favorite()) mo.addProperty("favorite", true);
			mo.add("settings", m.settingsToJson());
			o.add(m.id(), mo);
		}
		return o;
	}

	public List<String> fromJson(JsonElement json, Predicate<String> knownElsewhere) {
		List<String> warnings = new ArrayList<>();
		if (json == null || !json.isJsonObject()) {
			warnings.add("section modules invalide");
			return warnings;
		}
		for (Map.Entry<String, JsonElement> e : json.getAsJsonObject().entrySet()) {
			Module m = find(e.getKey());
			if (m == null) {
				if (!knownElsewhere.test(e.getKey())) warnings.add("module inconnu « " + e.getKey() + " » ignoré");
				continue;
			}
			if (!e.getValue().isJsonObject()) continue;
			JsonObject mo = e.getValue().getAsJsonObject();
			if (mo.has("enabled") && mo.get("enabled").isJsonPrimitive()) m.setEnabled(mo.get("enabled").getAsBoolean());
			if (mo.has("favorite")) m.setFavorite(mo.get("favorite").getAsBoolean());
			if (mo.has("settings")) warnings.addAll(m.settingsFromJson(mo.get("settings"), "module " + m.id()));
		}
		return warnings;
	}

	/** Raccourcis : propres à Java (codes GLFW), rangés dans la section « java/keybinds ». */
	public JsonObject keybindsToJson() {
		JsonObject o = new JsonObject();
		for (Module m : modules)
			if (m.keybind() != Module.NO_KEY) o.addProperty(m.id(), m.keybind());
		return o;
	}

	public List<String> keybindsFromJson(JsonElement json) {
		List<String> w = new ArrayList<>();
		if (json == null || !json.isJsonObject()) return w;
		for (Map.Entry<String, JsonElement> e : json.getAsJsonObject().entrySet()) {
			Module m = find(e.getKey());
			if (m != null && e.getValue().isJsonPrimitive()) m.setKeybind(e.getValue().getAsInt());
		}
		return w;
	}

	public void resetAll() {
		for (Module m : modules) m.resetToDefaults();
	}

	public static boolean isProfile(JsonElement e) {
		return ProfileStore.isProfileDocument(e);
	}
}
