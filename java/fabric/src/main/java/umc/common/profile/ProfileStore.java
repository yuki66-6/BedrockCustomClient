package umc.common.profile;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.gson.Strictness;
import com.google.gson.stream.JsonReader;
import umc.common.settings.Setting;

import java.io.IOException;
import java.io.StringReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.function.Predicate;
import java.util.stream.Stream;

/**
 * Profils au format commun umc-profile v2 (shared/schemas/profile.schema.json),
 * dans {@code <dossier>/profiles/<nom>.json}. Écriture atomique, fichier corrompu
 * mis de côté, suppression vers {@code .trash}, migration des profils v1 de
 * Bedrock, profils prédéfinis communs, sauvegarde automatique.
 */
public final class ProfileStore {
	public static final int FORMAT_VERSION = 2;
	private static final Gson GSON = new GsonBuilder().setPrettyPrinting().disableHtmlEscaping().create();
	private static final DateTimeFormatter STAMP = DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss");
	private static final Set<String> RESERVED = Set.of("CON", "PRN", "AUX", "NUL", "CLIENT", "COM1", "COM2", "COM3", "COM4",
			"COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9");

	private final Path dir;
	private final String edition;
	private final String clientVersion;
	private final List<ProfileSection> sections = new ArrayList<>();
	private String active = "default";
	private List<String> warnings = List.of();
	private Predicate<String> knownElsewhere = id -> false;

	private boolean autosave = true;
	private double autosaveDelay = 1.5;
	private long savedRevision;
	private long seenRevision;
	private double lastChange;

	public ProfileStore(Path dir, String edition, String clientVersion) {
		this.dir = dir;
		this.edition = edition;
		this.clientVersion = clientVersion;
	}

	public void addSection(ProfileSection section) {
		sections.add(section);
	}

	public String activeProfile() {
		return active;
	}

	public List<String> lastWarnings() {
		return warnings;
	}

	public Path directory() {
		return dir;
	}

	public Path profilePath(String name) {
		return dir.resolve("profiles").resolve(name + ".json");
	}

	// ---------------------------------------------------------------- noms

	public static String validateName(String name) {
		if (name == null || name.isEmpty()) return "Le nom est vide.";
		if (name.length() > 32) return "32 caractères maximum.";
		if (name.startsWith(" ") || name.endsWith(" ") || name.startsWith(".") || name.endsWith("."))
			return "Pas d'espace ni de point au début ou à la fin.";
		for (char c : name.toCharArray())
			if (c < 32 || "<>:\"/\\|?*".indexOf(c) >= 0) return "Caractères interdits : < > : \" / \\ | ? *";
		if (RESERVED.contains(name.toUpperCase())) return "Nom réservé par Windows.";
		return null;
	}

	public List<String> listProfiles() {
		List<String> out = new ArrayList<>();
		Path p = dir.resolve("profiles");
		if (!Files.isDirectory(p)) return out;
		try (Stream<Path> s = Files.list(p)) {
			s.filter(f -> f.getFileName().toString().endsWith(".json"))
					.map(f -> f.getFileName().toString().replaceFirst("\\.json$", ""))
					.filter(n -> !n.contains(".corrupt-"))
					.sorted()
					.forEach(out::add);
		} catch (IOException ignored) {
		}
		return out;
	}

	public boolean exists(String name) {
		return Files.isRegularFile(profilePath(name));
	}

	// ---------------------------------------------------------------- document

	public static boolean isProfileDocument(JsonElement doc) {
		if (doc == null || !doc.isJsonObject()) return false;
		JsonObject o = doc.getAsJsonObject();
		JsonElement f = o.get("format");
		if (f != null && f.isJsonPrimitive() && f.getAsJsonPrimitive().isString()) return "umc-profile".equals(f.getAsString());
		return o.has("sections") && o.get("sections").isJsonObject(); // v1 (Bedrock Custom Client 1.0)
	}

	public JsonObject buildDocument() {
		JsonObject doc = new JsonObject();
		doc.addProperty("format", "umc-profile");
		doc.addProperty("formatVersion", FORMAT_VERSION);
		doc.addProperty("name", active);
		doc.addProperty("edition", edition);
		doc.addProperty("clientVersion", clientVersion);
		for (ProfileSection s : sections) setPath(doc, s.path(), s.save().get());
		return doc;
	}

	/** Applique un document (v1 ou v2). Chaque section est réinitialisée avant application. */
	public List<String> applyDocument(JsonElement docEl, boolean[] migrated) {
		List<String> out = new ArrayList<>();
		if (migrated != null) migrated[0] = false;
		if (!isProfileDocument(docEl)) {
			out.add("document invalide (ni profil umc-profile v2, ni profil v1)");
			return out;
		}
		JsonObject doc = docEl.getAsJsonObject();
		boolean v1 = !doc.get("format").getAsJsonPrimitive().isString();
		if (v1 && migrated != null) migrated[0] = true;
		if (!v1 && doc.has("formatVersion") && doc.get("formatVersion").getAsInt() > FORMAT_VERSION)
			out.add("profil créé par une version plus récente (format " + doc.get("formatVersion").getAsInt() + ")");
		for (ProfileSection s : sections) {
			JsonElement value = v1 ? (s.v1Key() == null ? null : doc.getAsJsonObject("sections").get(s.v1Key())) : atPath(doc, s.path());
			s.reset().run();
			if (value != null) out.addAll(s.load().apply(value));
		}
		return out;
	}

	/** Les modules d'un profil commun qui n'existent pas dans cette édition sont ignorés sans avertissement. */
	public void setKnownElsewhere(Predicate<String> predicate) {
		knownElsewhere = predicate;
	}

	public boolean isKnownElsewhere(String moduleId) {
		return knownElsewhere.test(moduleId);
	}

	// ---------------------------------------------------------------- opérations

	public OpResult loadStartupProfile() {
		String name = "default";
		JsonElement state = readJson(dir.resolve("client.json"));
		if (state != null && state.isJsonObject() && state.getAsJsonObject().has("active_profile")) {
			String stored = state.getAsJsonObject().get("active_profile").getAsString();
			if (validateName(stored) == null && exists(stored)) name = stored;
		}
		if (!exists(name)) {
			active = name;
			for (ProfileSection s : sections) s.reset().run();
			OpResult r = save();
			return r.ok() ? OpResult.ok("Profil « " + name + " » créé") : r;
		}
		return load(name);
	}

	public OpResult save() {
		OpResult r = writeAtomic(profilePath(active), GSON.toJson(buildDocument()));
		if (!r.ok()) return r;
		markClean();
		writeAtomic(dir.resolve("client.json"), "{\n  \"active_profile\": " + GSON.toJson(active) + "\n}");
		return OpResult.ok("Profil « " + active + " » enregistré");
	}

	public OpResult load(String name) {
		String why = validateName(name);
		if (why != null) return OpResult.fail(why);
		Path file = profilePath(name);
		if (!Files.isRegularFile(file)) return OpResult.fail("Profil « " + name + " » introuvable");
		JsonElement doc = readJson(file);
		if (doc == null || !isProfileDocument(doc)) {
			Path backup = file.resolveSibling(name + ".corrupt-" + LocalDateTime.now().format(STAMP) + ".json");
			try {
				Files.move(file, backup);
			} catch (IOException ignored) {
			}
			for (ProfileSection s : sections) s.reset().run();
			active = name;
			save();
			return OpResult.fail("Profil « " + name + " » corrompu : sauvegardé sous " + backup.getFileName() + " et réinitialisé");
		}
		boolean[] migrated = {false};
		warnings = applyDocument(doc, migrated);
		active = name;
		if (migrated[0]) {
			try {
				Path backupDir = dir.resolve("profiles").resolve(".backup-v1");
				Files.createDirectories(backupDir);
				Files.copy(file, backupDir.resolve(file.getFileName()), StandardCopyOption.REPLACE_EXISTING);
			} catch (IOException ignored) {
			}
			save();
		}
		markClean();
		writeAtomic(dir.resolve("client.json"), "{\n  \"active_profile\": " + GSON.toJson(active) + "\n}");
		return OpResult.ok(warnings.isEmpty() ? "Profil « " + name + " » chargé"
				: "Profil « " + name + " » chargé (" + warnings.size() + " avertissement(s))");
	}

	public OpResult create(String name) {
		String why = validateName(name);
		if (why != null) return OpResult.fail(why);
		if (exists(name)) return OpResult.fail("Le profil « " + name + " » existe déjà");
		if (dirty()) save();
		for (ProfileSection s : sections) s.reset().run();
		active = name;
		OpResult r = save();
		return r.ok() ? OpResult.ok("Profil « " + name + " » créé (valeurs par défaut)") : r;
	}

	public OpResult duplicate(String name) {
		String why = validateName(name);
		if (why != null) return OpResult.fail(why);
		if (exists(name)) return OpResult.fail("Le profil « " + name + " » existe déjà");
		if (dirty()) save();
		String from = active;
		active = name;
		OpResult r = save();
		if (!r.ok()) active = from;
		return r.ok() ? OpResult.ok("Profil « " + name + " » créé à partir de « " + from + " »") : r;
	}

	public OpResult remove(String name) {
		if (!exists(name)) return OpResult.fail("Profil « " + name + " » introuvable");
		try {
			Path trash = dir.resolve(".trash");
			Files.createDirectories(trash);
			Files.move(profilePath(name), trash.resolve(name + "-" + LocalDateTime.now().format(STAMP) + ".json"));
		} catch (IOException e) {
			return OpResult.fail("Suppression impossible : " + e.getMessage());
		}
		String msg = "Profil « " + name + " » déplacé dans la corbeille (.trash)";
		if (name.equals(active)) {
			List<String> remaining = listProfiles();
			if (remaining.isEmpty()) {
				for (ProfileSection s : sections) s.reset().run();
				active = "default";
				save();
			} else {
				load(remaining.getFirst());
			}
			msg += " — profil actif : « " + active + " »";
		}
		return OpResult.ok(msg);
	}

	public OpResult exportTo(Path file) {
		OpResult r = writeAtomic(file, GSON.toJson(buildDocument()));
		return r.ok() ? OpResult.ok("Réglages exportés vers " + file.getFileName()) : r;
	}

	public OpResult importFrom(Path file, String name) {
		JsonElement doc = readJson(file);
		if (doc == null) return OpResult.fail("Import impossible : fichier illisible ou JSON invalide");
		if (!isProfileDocument(doc)) return OpResult.fail("Ce fichier n'est pas un profil Universal Minecraft Client");
		if (name == null || name.isEmpty()) name = file.getFileName().toString().replaceFirst("\\.json$", "");
		String why = validateName(name);
		if (why != null) return OpResult.fail(why);
		String base = name;
		for (int i = 2; exists(name); i++) name = base + " (" + i + ")";
		if (dirty()) save();
		warnings = applyDocument(doc, null);
		active = name;
		OpResult r = save();
		return r.ok() ? OpResult.ok("Profil « " + name + " » importé" + (warnings.isEmpty() ? "" : " (" + warnings.size() + " avertissement(s))"))
				: r;
	}

	/** Profil prédéfini (shared/profiles) : appliqué puis enregistré comme profil personnel modifiable. */
	public OpResult useBuiltin(String name, JsonElement doc) {
		if (!isProfileDocument(doc)) return OpResult.fail("Profil prédéfini invalide");
		String target = name;
		for (int i = 2; exists(target); i++) target = name + " (" + i + ")";
		if (dirty()) save();
		warnings = applyDocument(doc, null);
		active = target;
		OpResult r = save();
		return r.ok() ? OpResult.ok("Profil prédéfini « " + name + " » appliqué") : r;
	}

	// ---------------------------------------------------------------- sauvegarde auto

	public void setAutosave(boolean enabled, double delaySeconds) {
		autosave = enabled;
		autosaveDelay = delaySeconds;
	}

	public boolean dirty() {
		return Setting.revision() != savedRevision;
	}

	public void update(double now) {
		long rev = Setting.revision();
		if (rev != seenRevision) {
			seenRevision = rev;
			lastChange = now;
		}
		if (autosave && rev != savedRevision && now - lastChange >= autosaveDelay) save();
	}

	private void markClean() {
		savedRevision = seenRevision = Setting.revision();
	}

	// ---------------------------------------------------------------- utilitaires JSON

	public static JsonElement parseLenient(String text) {
		try {
			JsonReader reader = new JsonReader(new StringReader(text));
			reader.setStrictness(Strictness.LENIENT); // commentaires acceptés
			return JsonParser.parseReader(reader);
		} catch (RuntimeException e) {
			return null;
		}
	}

	public static JsonElement readJson(Path file) {
		try {
			if (!Files.isRegularFile(file)) return null;
			return parseLenient(Files.readString(file, StandardCharsets.UTF_8));
		} catch (IOException e) {
			return null;
		}
	}

	public static OpResult writeAtomic(Path file, String text) {
		try {
			Files.createDirectories(file.toAbsolutePath().getParent());
			Path tmp = file.resolveSibling(file.getFileName() + ".tmp");
			Files.writeString(tmp, text, StandardCharsets.UTF_8);
			try {
				Files.move(tmp, file, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
			} catch (AtomicMoveNotSupportedException e) {
				Files.move(tmp, file, StandardCopyOption.REPLACE_EXISTING);
			}
			return OpResult.ok("");
		} catch (IOException e) {
			return OpResult.fail("écriture impossible : " + e.getMessage());
		}
	}

	static JsonElement atPath(JsonObject doc, String path) {
		JsonElement cur = doc;
		for (String part : path.split("/")) {
			if (cur == null || !cur.isJsonObject()) return null;
			cur = cur.getAsJsonObject().get(part);
		}
		return cur;
	}

	static void setPath(JsonObject doc, String path, JsonElement value) {
		String[] parts = path.split("/");
		JsonObject cur = doc;
		for (int i = 0; i < parts.length - 1; i++) {
			JsonElement next = cur.get(parts[i]);
			if (next == null || !next.isJsonObject()) {
				next = new JsonObject();
				cur.add(parts[i], next);
			}
			cur = next.getAsJsonObject();
		}
		cur.add(parts[parts.length - 1], value);
	}
}
