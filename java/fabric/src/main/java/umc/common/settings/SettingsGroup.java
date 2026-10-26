package umc.common.settings;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/** Conteneur ordonné de réglages (base des modules et du thème). */
public class SettingsGroup {
	private final List<Setting<?>> settings = new ArrayList<>();
	private boolean loading;

	protected <S extends Setting<?>> S add(S setting) {
		settings.add(setting);
		return setting;
	}

	public List<Setting<?>> settings() {
		return Collections.unmodifiableList(settings);
	}

	public Setting<?> find(String id) {
		for (Setting<?> s : settings)
			if (s.id().equals(id)) return s;
		return null;
	}

	/** Vrai pendant un chargement : les réglages dépendants ne doivent pas en modifier d'autres. */
	public boolean loadingSettings() {
		return loading;
	}

	public JsonObject settingsToJson() {
		JsonObject o = new JsonObject();
		for (Setting<?> s : settings)
			if (s.persistent()) o.add(s.id(), s.toJson());
		return o;
	}

	public List<String> settingsFromJson(JsonElement json, String context) {
		List<String> warnings = new ArrayList<>();
		if (json == null || !json.isJsonObject()) {
			warnings.add(context + " : section de réglages invalide (objet attendu)");
			return warnings;
		}
		loading = true;
		try {
			for (Map.Entry<String, JsonElement> e : json.getAsJsonObject().entrySet()) {
				Setting<?> s = find(e.getKey());
				if (s == null) {
					warnings.add(context + " : réglage inconnu « " + e.getKey() + " » ignoré");
				} else if (!s.fromJson(e.getValue())) {
					warnings.add(context + " : valeur invalide pour « " + e.getKey() + " », valeur conservée");
				}
			}
		} finally {
			loading = false;
		}
		return warnings;
	}

	public void resetSettings() {
		loading = true;
		try {
			for (Setting<?> s : settings) s.reset();
		} finally {
			loading = false;
		}
	}
}
