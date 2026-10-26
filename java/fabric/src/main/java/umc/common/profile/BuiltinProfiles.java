package umc.common.profile;

import com.google.gson.JsonElement;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.List;

/** Profils prédéfinis communs (shared/profiles), embarqués dans le mod. */
public final class BuiltinProfiles {
	/** Fichiers de shared/profiles (sans extension) ; la cohérence est vérifiée par les tests. */
	public static final List<String> FILES = List.of("bedwars", "building", "crystal-pvp-training", "exploration", "performance", "survival");

	private BuiltinProfiles() {
	}

	/** Document du profil, ou null s'il est absent ou invalide. */
	public static JsonElement load(String file) {
		try (InputStream in = BuiltinProfiles.class.getResourceAsStream("/assets/umc/shared/profiles/" + file + ".json")) {
			if (in == null) return null;
			JsonElement doc = ProfileStore.parseLenient(new String(in.readAllBytes(), StandardCharsets.UTF_8));
			return ProfileStore.isProfileDocument(doc) ? doc : null;
		} catch (IOException e) {
			return null;
		}
	}

	/** Nom affiché (champ « name » du profil, sinon le nom du fichier). */
	public static String displayName(String file) {
		JsonElement doc = load(file);
		if (doc != null && doc.getAsJsonObject().has("name")) return doc.getAsJsonObject().get("name").getAsString();
		return file;
	}

	public static String description(String file) {
		JsonElement doc = load(file);
		if (doc != null && doc.getAsJsonObject().has("description")) return doc.getAsJsonObject().get("description").getAsString();
		return "";
	}
}
