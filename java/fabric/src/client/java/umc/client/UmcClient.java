package umc.client;

import com.mojang.blaze3d.platform.InputConstants;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientLifecycleEvents;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keymapping.v1.KeyMappingHelper;
import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElementRegistry;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import umc.client.gui.ClickGuiScreen;
import umc.client.gui.GuiSettings;
import umc.client.gui.HudEditorScreen;
import umc.client.gui.Theme;
import umc.client.hud.HudManager;
import umc.client.module.Module;
import umc.client.module.ModuleManager;
import umc.client.modules.ArmorHudModule;
import umc.client.modules.CoordinatesModule;
import umc.client.modules.CpsModule;
import umc.client.modules.CrosshairModule;
import umc.client.modules.DeathWaypointModule;
import umc.client.modules.DirectionModule;
import umc.client.modules.FpsModule;
import umc.client.modules.InfoBarModule;
import umc.client.modules.KeystrokesModule;
import umc.client.modules.PingModule;
import umc.client.modules.PotionEffectsModule;
import umc.client.modules.SpeedModule;
import umc.client.modules.TimeHudModule;
import umc.client.modules.ToggleSneakModule;
import umc.client.modules.ToggleSprintModule;
import umc.client.modules.TotemCounterModule;
import umc.client.modules.WaypointsModule;
import umc.client.modules.ZoomModule;
import umc.client.telemetry.ClickTracker;
import umc.client.telemetry.FabricGameAdapter;
import umc.common.modules.ModuleDefinitions;
import umc.common.profile.OpResult;
import umc.common.profile.ProfileSection;
import umc.common.profile.ProfileStore;
import umc.common.waypoint.WaypointStore;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Point d'entrée du mod : services, modules, profils (config/umc), touches,
 * HUD et boucle de tick. Aucune connexion réseau propre au mod.
 */
public final class UmcClient implements ClientModInitializer {
	public static final Logger LOG = LoggerFactory.getLogger("umc");
	private static final KeyMapping.Category CATEGORY = KeyMapping.Category.register(Identifier.fromNamespaceAndPath("umc", "main"));

	private final Map<Integer, Boolean> keyWasDown = new HashMap<>();

	@Override
	public void onInitializeClient() {
		Minecraft mc = Minecraft.getInstance();
		String version = FabricLoader.getInstance().getModContainer("umc").map(c -> c.getMetadata().getVersion().getFriendlyString()).orElse("dev");

		Umc.configDir = FabricLoader.getInstance().getConfigDir().resolve("umc");
		Umc.definitions = ModuleDefinitions.fromClasspath();
		Umc.theme = new Theme();
		Umc.gui = new GuiSettings();
		Umc.adapter = new FabricGameAdapter(mc);
		Umc.modules = new ModuleManager();
		registerModules(Umc.modules);
		Umc.hud = new HudManager(Umc.modules, Umc.theme, Umc.adapter);

		Umc.waypoints = new WaypointStore(Umc.configDir.resolve("waypoints").resolve("umc-waypoints.json"));
		OpResult wr = Umc.waypoints.load();
		if (!wr.ok()) LOG.warn(wr.message());

		ProfileStore ps = new ProfileStore(Umc.configDir, "java", version);
		Umc.profiles = ps;
		ps.addSection(new ProfileSection("modules", "modules", Umc.modules::toJson, j -> Umc.modules.fromJson(j, ps::isKnownElsewhere),
				Umc.modules::resetAll));
		ps.addSection(new ProfileSection("theme", "theme", Umc.theme::settingsToJson, Umc.theme::load, Umc.theme::resetSettings));
		ps.addSection(new ProfileSection("hud", "hud", Umc.hud::toJson, Umc.hud::fromJson, Umc.hud::resetAll));
		ps.addSection(new ProfileSection("java/keybinds", null, Umc.modules::keybindsToJson, Umc.modules::keybindsFromJson, () -> {}));
		ps.addSection(new ProfileSection("java/gui", null, Umc.gui::settingsToJson, j -> Umc.gui.settingsFromJson(j, "menu"),
				Umc.gui::resetSettings));
		// Modules connus ailleurs (Bedrock ou prévus) : ignorés sans avertissement.
		ps.setKnownElsewhere(id -> Umc.definitions.find(id) != null);
		OpResult pr = ps.loadStartupProfile();
		LOG.info("[UMC] {}", pr.message());
		for (String w : ps.lastWarnings()) LOG.warn("[UMC] {}", w);
		ps.setAutosave(Umc.gui.autosave.on(), 1.5);
		Umc.gui.autosave.onChange(() -> ps.setAutosave(Umc.gui.autosave.on(), 1.5));

		Umc.openGuiKey = KeyMappingHelper.registerKeyMapping(new KeyMapping("key.umc.open_gui", InputConstants.KEY_INSERT, CATEGORY));
		Umc.zoomKey = KeyMappingHelper.registerKeyMapping(new KeyMapping("key.umc.zoom", InputConstants.KEY_C, CATEGORY));
		Umc.addWaypointKey = KeyMappingHelper.registerKeyMapping(new KeyMapping("key.umc.add_waypoint", InputConstants.KEY_B, CATEGORY));
		Umc.modules.get(ZoomModule.class).setKey(Umc.zoomKey);

		HudElementRegistry.addLast(Identifier.fromNamespaceAndPath("umc", "hud"), (g, delta) -> {
			if (!(mc.gui.screen() instanceof HudEditorScreen)) Umc.hud.render(g, false);
		});
		CrosshairModule crosshair = Umc.modules.get(CrosshairModule.class);
		HudElementRegistry.replaceElement(VanillaHudElements.CROSSHAIR, vanilla -> (g, delta) -> {
			if (crosshair.enabled()) crosshair.draw(g);
			else vanilla.extractRenderState(g, delta);
		});

		ClientTickEvents.START_CLIENT_TICK.register(client -> {
			for (Module m : Umc.modules.all())
				if (m.enabled()) m.tick(client);
		});
		ClientTickEvents.END_CLIENT_TICK.register(this::endTick);
		ClientLifecycleEvents.CLIENT_STOPPING.register(client -> {
			if (ps.dirty()) ps.save();
		});
		LOG.info("[UMC] {} modules Java chargés, configuration : {}", Umc.modules.all().size(), Umc.configDir);
	}

	private static void registerModules(ModuleManager m) {
		m.register(new KeystrokesModule());
		m.register(new CpsModule());
		m.register(new FpsModule());
		m.register(new CoordinatesModule());
		m.register(new DirectionModule());
		m.register(new PingModule());
		m.register(new ArmorHudModule());
		m.register(new PotionEffectsModule());
		m.register(new SpeedModule());
		m.register(new ToggleSprintModule());
		m.register(new ToggleSneakModule());
		m.register(new ZoomModule());
		m.register(new WaypointsModule());
		m.register(new DeathWaypointModule());
		m.register(new CrosshairModule());
		m.register(new TotemCounterModule());
		m.register(new TimeHudModule());
		m.register(new InfoBarModule());
		// Les identifiants doivent correspondre à shared/module-definitions (vérifié aussi par les tests).
		Set<String> registered = new HashSet<>();
		for (Module mod : m.all()) registered.add(mod.id());
		if (!registered.equals(new HashSet<>(ModuleDefinitions.JAVA_MODULE_IDS)))
			LOG.error("[UMC] Modules enregistrés incohérents avec ModuleDefinitions.JAVA_MODULE_IDS : {}", registered);
	}

	private void endTick(Minecraft mc) {
		while (Umc.openGuiKey.consumeClick())
			if (mc.gui.screen() == null) mc.gui.setScreen(new ClickGuiScreen());
		while (Umc.addWaypointKey.consumeClick()) {
			String key = Umc.adapter.worldKey().orElse("");
			OpResult r = Umc.addWaypointHere("Waypoint " + (Umc.waypoints.forWorld(key).size() + 1), "other", null);
			Umc.status(r);
			if (mc.player != null) mc.player.sendOverlayMessage(Component.literal(r.message()));
		}
		moduleKeybinds(mc);
		Umc.profiles.update(ClickTracker.now());
	}

	/** Raccourcis des modules : front montant de la touche, uniquement en jeu sans écran ouvert. */
	private void moduleKeybinds(Minecraft mc) {
		boolean ingame = mc.player != null && mc.gui.screen() == null;
		List<Module> all = Umc.modules.all();
		for (Module m : all) {
			int key = m.keybind();
			if (key == Module.NO_KEY) continue;
			boolean down = ingame && InputConstants.isKeyDown(mc.getWindow(), key);
			boolean was = keyWasDown.getOrDefault(key, false);
			if (down && !was) {
				m.toggle();
				if (mc.player != null)
					mc.player.sendOverlayMessage(Component.literal(m.name() + " : " + (m.enabled() ? "activé" : "désactivé")));
			}
		}
		for (Module m : all) {
			int key = m.keybind();
			if (key != Module.NO_KEY) keyWasDown.put(key, ingame && InputConstants.isKeyDown(mc.getWindow(), key));
		}
	}
}
