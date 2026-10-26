package umc.common.profile;

import com.google.gson.JsonElement;

import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;

/**
 * Section d'un profil v2 (« modules », « theme », « hud », « java/gui »…).
 * {@code v1Key} : nom de la section dans les anciens profils v1 de Bedrock (ou null).
 */
public record ProfileSection(String path, String v1Key, Supplier<JsonElement> save, Function<JsonElement, List<String>> load,
		Runnable reset) {
}
