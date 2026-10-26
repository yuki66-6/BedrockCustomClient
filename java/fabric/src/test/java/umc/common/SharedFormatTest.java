package umc.common;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import umc.common.data.Availability;
import umc.common.data.Capability;
import umc.common.data.DataPoint;
import umc.common.data.DataSource;
import umc.common.modules.ModuleDefinitions;
import umc.common.profile.BuiltinProfiles;
import umc.common.profile.OpResult;
import umc.common.profile.ProfileSection;
import umc.common.profile.ProfileStore;
import umc.common.settings.Setting;
import umc.common.settings.Settings;
import umc.common.settings.SettingsGroup;
import umc.common.util.GameMath;
import umc.common.waypoint.Waypoint;
import umc.common.waypoint.WaypointStore;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Stream;

import static org.junit.jupiter.api.Assertions.*;

/** Tests de la logique commune Java/Bedrock (sans lancer Minecraft). */
class SharedFormatTest {
	static final Path SHARED = Path.of(System.getProperty("umc.sharedDir", "../../shared"));

	/** Faux module : réglages + état, comme un module du client. */
	static final class FakeModule extends SettingsGroup {
		final String id;
		boolean enabled, enabledByDefault;
		final Settings.Float size;
		final Settings.Choice style;
		final Settings.Color color;

		FakeModule(String id, boolean enabledByDefault) {
			this.id = id;
			this.enabled = this.enabledByDefault = enabledByDefault;
			size = add(new Settings.Float("size", "Taille", "", 7f, 1f, 40f, "%.0f"));
			style = add(new Settings.Choice("style", "Type", "", List.of("Croix", "Point", "Cercle"), 0));
			color = add(new Settings.Color("color", "Couleur", "", 0xFF5AA6FF));
		}
	}

	static ProfileStore storeWith(Path dir, Map<String, FakeModule> modules, List<String> warningsSink) {
		ProfileStore store = new ProfileStore(dir, "java", "test");
		store.setKnownElsewhere(id -> Set.of("reach_lab", "combat_hud", "sprint", "jumps", "session", "player_info").contains(id));
		store.addSection(new ProfileSection("modules", "modules", () -> {
			JsonObject o = new JsonObject();
			for (FakeModule m : modules.values()) {
				JsonObject mo = new JsonObject();
				mo.addProperty("enabled", m.enabled);
				mo.add("settings", m.settingsToJson());
				o.add(m.id, mo);
			}
			return o;
		}, json -> {
			List<String> w = new ArrayList<>();
			for (Map.Entry<String, JsonElement> e : json.getAsJsonObject().entrySet()) {
				FakeModule m = modules.get(e.getKey());
				if (m == null) {
					if (!store.isKnownElsewhere(e.getKey())) w.add("module inconnu " + e.getKey());
					continue;
				}
				JsonObject mo = e.getValue().getAsJsonObject();
				if (mo.has("enabled")) m.enabled = mo.get("enabled").getAsBoolean();
				if (mo.has("settings")) w.addAll(m.settingsFromJson(mo.get("settings"), m.id));
			}
			warningsSink.addAll(w);
			return w;
		}, () -> modules.values().forEach(m -> {
			m.resetSettings();
			m.enabled = m.enabledByDefault;
		})));
		return store;
	}

	@Test
	void dataPointFreshnessAndReason() {
		DataPoint<Integer> d = DataPoint.available(5, DataSource.GAME_API, 10, 1);
		assertTrue(d.usable(10.5));
		assertEquals(Availability.STALE, d.availabilityAt(11.5));
		DataPoint<Float> u = DataPoint.missing(Availability.UNSUPPORTED, "motif", DataSource.NONE);
		assertTrue(u.value().isEmpty());
		assertEquals("motif", u.reason());
		assertEquals("permission_required", Availability.PERMISSION_REQUIRED.id);
	}

	@Test
	void capabilitiesMatchSharedJsonAndCpp() throws IOException {
		JsonObject j = ProfileStore.readJson(SHARED.resolve("schemas/capabilities.json")).getAsJsonObject();
		List<String> fromJson = new ArrayList<>();
		for (JsonElement c : j.getAsJsonArray("capabilities")) fromJson.add(c.getAsJsonObject().get("id").getAsString());
		List<String> fromJava = new ArrayList<>();
		for (Capability c : Capability.values()) fromJava.add(c.id);
		assertEquals(fromJson, fromJava, "même liste et même ordre que le JSON (et donc que le C++)");
	}

	@Test
	void settingsJsonMatchesBedrockFormat() {
		FakeModule m = new FakeModule("crosshair", false);
		m.style.set(2);
		m.color.set(0x80FF0000);
		JsonObject j = m.settingsToJson();
		assertEquals("Cercle", j.get("style").getAsString());
		assertEquals(4, j.getAsJsonArray("color").size());
		assertEquals(1.0, j.getAsJsonArray("color").get(0).getAsDouble(), 1e-6);
		FakeModule other = new FakeModule("crosshair", false);
		assertTrue(other.settingsFromJson(j, "t").isEmpty());
		assertEquals(2, other.style.get());
		assertEquals(0x80FF0000, other.color.argb());
		JsonObject bad = new JsonObject();
		bad.addProperty("size", "pas un nombre");
		bad.addProperty("inconnu", 1);
		assertEquals(2, other.settingsFromJson(bad, "t").size());
		long rev = Setting.revision();
		other.size.set(100f);
		assertEquals(40f, other.size.value());
		assertTrue(Setting.revision() > rev);
	}

	@Test
	void profileRoundTripAndAutosave(@TempDir Path dir) {
		Map<String, FakeModule> mods = new LinkedHashMap<>();
		mods.put("crosshair", new FakeModule("crosshair", false));
		ProfileStore store = storeWith(dir, mods, new ArrayList<>());
		assertTrue(store.loadStartupProfile().ok());
		mods.get("crosshair").enabled = true;
		mods.get("crosshair").size.set(12f);
		assertTrue(store.save().ok());
		JsonObject doc = ProfileStore.readJson(store.profilePath("default")).getAsJsonObject();
		assertEquals("umc-profile", doc.get("format").getAsString());
		assertEquals("java", doc.get("edition").getAsString());
		mods.get("crosshair").size.set(3f);
		assertTrue(store.load("default").ok());
		assertEquals(12f, mods.get("crosshair").size.value());
		store.setAutosave(true, 1.0);
		store.update(10);
		mods.get("crosshair").size.set(20f);
		store.update(10.2);
		assertTrue(store.dirty());
		store.update(11.5);
		assertFalse(store.dirty());
	}

	@Test
	void migratesBedrockV1Profile(@TempDir Path dir) throws IOException {
		Map<String, FakeModule> mods = new LinkedHashMap<>();
		mods.put("crosshair", new FakeModule("crosshair", false));
		ProfileStore store = storeWith(dir, mods, new ArrayList<>());
		store.loadStartupProfile();
		Files.writeString(store.profilePath("ancien"),
				"{\"format\": 1, \"sections\": {\"modules\": {\"crosshair\": {\"enabled\": true, \"settings\": {\"style\": \"Point\"}}}}}");
		OpResult r = store.load("ancien");
		assertTrue(r.ok());
		assertTrue(mods.get("crosshair").enabled);
		assertEquals("Point", mods.get("crosshair").style.current());
		assertEquals("umc-profile", ProfileStore.readJson(store.profilePath("ancien")).getAsJsonObject().get("format").getAsString());
		assertTrue(Files.exists(dir.resolve("profiles/.backup-v1/ancien.json")));
	}

	@Test
	void corruptProfileIsSetAside(@TempDir Path dir) throws IOException {
		Map<String, FakeModule> mods = new LinkedHashMap<>();
		mods.put("crosshair", new FakeModule("crosshair", false));
		ProfileStore store = storeWith(dir, mods, new ArrayList<>());
		store.loadStartupProfile();
		Files.writeString(store.profilePath("casse"), "{ pas du json");
		assertFalse(store.load("casse").ok());
		try (Stream<Path> s = Files.list(dir.resolve("profiles"))) {
			assertTrue(s.anyMatch(p -> p.getFileName().toString().contains(".corrupt-")));
		}
		assertFalse(store.listProfiles().stream().anyMatch(n -> n.contains("corrupt")));
		assertNotNull(ProfileStore.validateName("a/b"));
		assertNotNull(ProfileStore.validateName("con"));
		assertNull(ProfileStore.validateName("PvP Hive"));
	}

	@Test
	void builtinProfilesApplyWithoutWarnings(@TempDir Path dir) throws IOException {
		Map<String, FakeModule> mods = new LinkedHashMap<>();
		for (String id : ModuleDefinitions.JAVA_MODULE_IDS) mods.put(id, new FakeModule(id, id.equals("keystrokes") || id.equals("info_bar")));
		List<String> sink = new ArrayList<>();
		ProfileStore store = storeWith(dir, mods, sink);
		store.loadStartupProfile();
		List<Path> files;
		try (Stream<Path> s = Files.list(SHARED.resolve("profiles"))) {
			files = s.filter(p -> p.toString().endsWith(".json")).toList();
		}
		assertEquals(6, files.size());
		for (Path f : files) {
			JsonElement doc = ProfileStore.readJson(f);
			OpResult r = store.useBuiltin(doc.getAsJsonObject().get("name").getAsString(), doc);
			assertTrue(r.ok(), f.toString());
			assertTrue(store.lastWarnings().isEmpty(), f + " : " + store.lastWarnings());
		}
		JsonElement perf = ProfileStore.readJson(SHARED.resolve("profiles/performance.json"));
		store.useBuiltin("Performance", perf);
		assertTrue(mods.get("fps").enabled);
		assertFalse(mods.get("keystrokes").enabled);
	}

	@Test
	void builtinProfileListMatchesSharedFiles() throws IOException {
		List<String> files = new ArrayList<>();
		try (Stream<Path> s = Files.list(SHARED.resolve("profiles"))) {
			s.filter(p -> p.toString().endsWith(".json")).forEach(p -> files.add(p.getFileName().toString().replaceFirst("\\.json$", "")));
		}
		files.sort(null);
		assertEquals(files, BuiltinProfiles.FILES, "BuiltinProfiles.FILES doit lister shared/profiles");
		for (String f : BuiltinProfiles.FILES) {
			assertNotNull(BuiltinProfiles.load(f), "profil embarqué illisible : " + f);
			assertFalse(BuiltinProfiles.displayName(f).isBlank());
		}
	}

	@Test
	void moduleDefinitionsMatchRegisteredJavaModules() throws IOException {
		ModuleDefinitions defs = ModuleDefinitions.parse(Files.readString(SHARED.resolve("module-definitions/modules.json")));
		Set<String> registered = new HashSet<>(ModuleDefinitions.JAVA_MODULE_IDS);
		for (String id : registered) {
			ModuleDefinitions.Definition d = defs.find(id);
			assertNotNull(d, "définition manquante : " + id);
			assertTrue(d.java().shown(), id + " doit être disponible pour Java");
		}
		for (ModuleDefinitions.Definition d : defs.all()) {
			if (d.java().shown() && d.java().providedBy().equals("module"))
				assertTrue(registered.contains(d.id()), "module annoncé pour Java mais absent : " + d.id());
			for (String c : d.requiredCapabilities()) assertNotNull(Capability.fromId(c), "capacité inconnue : " + c);
		}
	}

	@Test
	void waypointsSharedWithBedrock(@TempDir Path dir) {
		WaypointStore store = new WaypointStore(dir.resolve("umc-waypoints.json"));
		assertTrue(store.load().ok());
		Waypoint wp = store.add("java:sp:Monde", "Monde", "singleplayer", new Waypoint("Base", 10, 70, -20, "minecraft:the_nether"));
		assertEquals("nether", wp.dimension);
		assertFalse(wp.id.isEmpty());
		assertTrue(store.save().ok());
		WaypointStore reloaded = new WaypointStore(store.file());
		assertTrue(reloaded.load().ok());
		assertEquals(1, reloaded.forWorldAndDimension("java:sp:Monde", "nether").size());
		assertEquals(0xFF458AFF, reloaded.forWorld("java:sp:Monde").getFirst().argb());
		assertTrue(reloaded.importFrom(SHARED.resolve("waypoints/example-waypoints.json")).ok());
		assertEquals(1, reloaded.forWorld("java:sp:Exemple").size());
		assertEquals("0 waypoint(s) importé(s)", reloaded.importFrom(SHARED.resolve("waypoints/example-waypoints.json")).message());
		assertFalse(reloaded.importFrom(SHARED.resolve("profiles/survival.json")).ok());
		assertTrue(reloaded.remove("java:sp:Monde", wp.id));
	}

	@Test
	void gameMath() {
		assertEquals(-13, GameMath.overworldToNether(-100));
		assertEquals(12, GameMath.overworldToNether(100));
		assertEquals(800, GameMath.netherToOverworld(100));
		assertEquals("S", GameMath.cardinal(0));
		assertEquals("O", GameMath.cardinal(90));
		assertEquals("N", GameMath.cardinal(-180));
		assertEquals("E", GameMath.cardinal(-90));
		assertEquals(0f, GameMath.relativeAngle(0, 0, 0, 0, 10), 1e-3); // cible au sud, regard au sud
		assertEquals(90f, Math.abs(GameMath.relativeAngle(0, 0, 0, 10, 0)), 1e-3);
		assertEquals("1:05", GameMath.formatTicks(65 * 20, false));
		assertEquals("∞", GameMath.formatTicks(0, true));
		assertEquals("IV", GameMath.roman(4));
		assertEquals("12:00", GameMath.gameClock(6000));
		assertEquals(2, GameMath.gameDay(24000));
		GameMath.CpsCounter cps = new GameMath.CpsCounter();
		for (int i = 0; i < 10; i++) cps.click(1.0 + i * 0.05);
		assertEquals(10, cps.count(1.5));
		assertEquals(9, cps.count(2.04));
		GameMath.SpeedTracker sp = new GameMath.SpeedTracker(5);
		for (int i = 0; i < 5; i++) sp.sample(i * 0.05, i * 0.28, 64, 0);
		assertEquals(5.6, sp.horizontal(), 1e-6); // 0,28 bloc / tick = 5,6 blocs/s (sprint)
	}
}
