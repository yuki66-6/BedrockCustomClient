package umc.common.waypoint;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import umc.common.profile.OpResult;
import umc.common.profile.ProfileStore;

import java.nio.file.Files;
import java.nio.file.Path;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Waypoints classés par monde/serveur puis par dimension, au format commun
 * umc-waypoints (lisible par la version Bedrock et inversement).
 */
public final class WaypointStore {
	private static final Gson GSON = new GsonBuilder().setPrettyPrinting().disableHtmlEscaping().create();
	private static final SecureRandom RNG = new SecureRandom();

	public record Category(String id, String name, String color) {
	}

	public static final class World {
		public String name = "";
		public String edition = "java";
		public String kind = "unknown";
		public final List<Waypoint> waypoints = new ArrayList<>();
	}

	private final Path file;
	private final Map<String, World> worlds = new LinkedHashMap<>();
	private final List<Category> categories = new ArrayList<>(defaultCategories());

	public WaypointStore(Path file) {
		this.file = file;
	}

	public Path file() {
		return file;
	}

	public static List<Category> defaultCategories() {
		return List.of(new Category("base", "Base", "#458AFF"), new Category("resource", "Ressources", "#4DDB85"),
				new Category("danger", "Danger", "#F2525A"), new Category("death", "Morts", "#9EA3B3"),
				new Category("other", "Autre", "#FFB84D"));
	}

	public static String newId() {
		byte[] b = new byte[8];
		RNG.nextBytes(b);
		return HexFormat.of().formatHex(b);
	}

	public static String normalizeDimension(String d) {
		if (d == null) return "overworld";
		return switch (d) {
			case "minecraft:overworld", "overworld", "0" -> "overworld";
			case "minecraft:the_nether", "nether", "the_nether", "1" -> "nether";
			case "minecraft:the_end", "end", "the_end", "2" -> "end";
			default -> d;
		};
	}

	public Map<String, World> worlds() {
		return worlds;
	}

	public List<Category> categories() {
		return categories;
	}

	public List<Waypoint> forWorld(String worldKey) {
		World w = worlds.get(worldKey);
		return w == null ? List.of() : w.waypoints;
	}

	public List<Waypoint> forWorldAndDimension(String worldKey, String dimension) {
		String d = normalizeDimension(dimension);
		List<Waypoint> out = new ArrayList<>();
		for (Waypoint wp : forWorld(worldKey))
			if (wp.dimension.equals(d)) out.add(wp);
		return out;
	}

	public Waypoint add(String worldKey, String worldName, String kind, Waypoint wp) {
		World w = worlds.computeIfAbsent(worldKey, k -> new World());
		if (w.name.isEmpty()) {
			w.name = worldName;
			w.kind = kind;
		}
		if (wp.id == null || wp.id.isEmpty()) wp.id = newId();
		if (wp.createdAt == 0) wp.createdAt = System.currentTimeMillis() / 1000;
		wp.dimension = normalizeDimension(wp.dimension);
		w.waypoints.add(wp);
		return wp;
	}

	public boolean remove(String worldKey, String id) {
		World w = worlds.get(worldKey);
		return w != null && w.waypoints.removeIf(wp -> wp.id.equals(id));
	}

	// ---------------------------------------------------------------- JSON

	public JsonObject toJson() {
		JsonObject root = new JsonObject();
		root.addProperty("format", "umc-waypoints");
		root.addProperty("formatVersion", 1);
		JsonArray cats = new JsonArray();
		for (Category c : categories) {
			JsonObject o = new JsonObject();
			o.addProperty("id", c.id());
			o.addProperty("name", c.name());
			o.addProperty("color", c.color());
			cats.add(o);
		}
		root.add("categories", cats);
		JsonObject ws = new JsonObject();
		for (Map.Entry<String, World> e : worlds.entrySet()) {
			JsonObject w = new JsonObject();
			w.addProperty("name", e.getValue().name);
			w.addProperty("edition", e.getValue().edition);
			w.addProperty("kind", e.getValue().kind);
			JsonArray list = new JsonArray();
			for (Waypoint p : e.getValue().waypoints) list.add(GSON.toJsonTree(p));
			w.add("waypoints", list);
			ws.add(e.getKey(), w);
		}
		root.add("worlds", ws);
		return root;
	}

	/** Remplace le contenu par celui du document. @return null si OK, sinon le motif. */
	public String fromJson(JsonElement el) {
		Map<String, World> parsedWorlds = new LinkedHashMap<>();
		List<Category> parsedCats = new ArrayList<>();
		String err = parse(el, parsedWorlds, parsedCats);
		if (err != null) return err;
		worlds.clear();
		worlds.putAll(parsedWorlds);
		categories.clear();
		categories.addAll(parsedCats.isEmpty() ? defaultCategories() : parsedCats);
		return null;
	}

	private static String parse(JsonElement el, Map<String, World> outWorlds, List<Category> outCats) {
		if (el == null || !el.isJsonObject()) return "JSON invalide";
		JsonObject root = el.getAsJsonObject();
		if (!root.has("format") || !"umc-waypoints".equals(root.get("format").getAsString()) || !root.has("worlds")
				|| !root.get("worlds").isJsonObject())
			return "ce n'est pas un fichier umc-waypoints";
		if (root.has("categories") && root.get("categories").isJsonArray())
			for (JsonElement c : root.getAsJsonArray("categories"))
				if (c.isJsonObject() && c.getAsJsonObject().has("id")) {
					JsonObject o = c.getAsJsonObject();
					outCats.add(new Category(o.get("id").getAsString(), o.has("name") ? o.get("name").getAsString() : "",
							o.has("color") ? o.get("color").getAsString() : "#FFB84D"));
				}
		for (Map.Entry<String, JsonElement> e : root.getAsJsonObject("worlds").entrySet()) {
			if (!e.getValue().isJsonObject()) continue;
			JsonObject wo = e.getValue().getAsJsonObject();
			World w = new World();
			w.name = wo.has("name") ? wo.get("name").getAsString() : e.getKey();
			w.edition = wo.has("edition") ? wo.get("edition").getAsString() : "java";
			w.kind = wo.has("kind") ? wo.get("kind").getAsString() : "unknown";
			if (wo.has("waypoints") && wo.get("waypoints").isJsonArray())
				for (JsonElement p : wo.getAsJsonArray("waypoints")) {
					if (!p.isJsonObject()) continue;
					JsonObject po = p.getAsJsonObject();
					if (!po.has("x") || !po.has("z")) continue;
					Waypoint wp = GSON.fromJson(po, Waypoint.class);
					if (wp.id == null || wp.id.isEmpty()) wp.id = newId();
					wp.dimension = normalizeDimension(wp.dimension);
					w.waypoints.add(wp);
				}
			outWorlds.put(e.getKey(), w);
		}
		return null;
	}

	public OpResult load() {
		if (!Files.isRegularFile(file)) return OpResult.ok("aucun waypoint");
		JsonElement el = ProfileStore.readJson(file);
		String err = fromJson(el);
		return err == null ? OpResult.ok("") : OpResult.fail("Waypoints illisibles : " + err);
	}

	public OpResult save() {
		return ProfileStore.writeAtomic(file, GSON.toJson(toJson()));
	}

	public OpResult exportTo(Path target) {
		OpResult r = ProfileStore.writeAtomic(target, GSON.toJson(toJson()));
		return r.ok() ? OpResult.ok("Waypoints exportés vers " + target.getFileName()) : r;
	}

	/** Fusionne un fichier umc-waypoints ; ignore les identifiants déjà présents. */
	public OpResult importFrom(Path source) {
		Map<String, World> incoming = new LinkedHashMap<>();
		List<Category> cats = new ArrayList<>();
		String err = parse(ProfileStore.readJson(source), incoming, cats);
		if (err != null) return OpResult.fail("Import impossible : " + err);
		int added = 0;
		for (Map.Entry<String, World> e : incoming.entrySet()) {
			World dst = worlds.computeIfAbsent(e.getKey(), k -> {
				World w = new World();
				w.name = e.getValue().name;
				w.edition = e.getValue().edition;
				w.kind = e.getValue().kind;
				return w;
			});
			for (Waypoint wp : e.getValue().waypoints) {
				if (dst.waypoints.stream().anyMatch(o -> o.id.equals(wp.id))) continue;
				if ("user".equals(wp.source)) wp.source = "import";
				dst.waypoints.add(wp);
				added++;
			}
		}
		for (Category c : cats)
			if (categories.stream().noneMatch(o -> o.id().equals(c.id()))) categories.add(c);
		return OpResult.ok(added + " waypoint(s) importé(s)");
	}
}
