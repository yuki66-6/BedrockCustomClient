package umc.client;

import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import umc.client.gui.GuiSettings;
import umc.client.gui.Theme;
import umc.client.hud.HudManager;
import umc.client.module.ModuleManager;
import umc.client.telemetry.ClickTracker;
import umc.client.telemetry.FabricGameAdapter;
import umc.common.data.DataPoint;
import umc.common.modules.ModuleDefinitions;
import umc.common.profile.OpResult;
import umc.common.profile.ProfileStore;
import umc.common.waypoint.Waypoint;
import umc.common.waypoint.WaypointStore;

import java.nio.file.Path;

/** Services partagés du mod, créés une fois par {@link UmcClient}. */
public final class Umc {
	public static Path configDir;
	public static ModuleDefinitions definitions;
	public static ModuleManager modules;
	public static Theme theme;
	public static HudManager hud;
	public static FabricGameAdapter adapter;
	public static ProfileStore profiles;
	public static WaypointStore waypoints;
	public static GuiSettings gui;
	public static KeyMapping openGuiKey;
	public static KeyMapping zoomKey;
	public static KeyMapping addWaypointKey;

	private static String status = "";
	private static boolean statusOk = true;
	private static double statusTime = -100;

	private Umc() {
	}

	/** Message affiché en bas du menu (résultat d'une action). */
	public static void status(OpResult r) {
		status(r.message(), r.ok());
	}

	public static void status(String message, boolean ok) {
		status = message;
		statusOk = ok;
		statusTime = ClickTracker.now();
	}

	public static String statusMessage() {
		return ClickTracker.now() - statusTime < 6 ? status : "";
	}

	public static boolean statusOk() {
		return statusOk;
	}

	/** Ajoute un waypoint dans le monde actuel (clé, nom et type déduits de l'adaptateur). */
	public static OpResult addWaypointHere(String name, String category, String color) {
		return addWaypointHere(name, category, color, "user");
	}

	public static OpResult addWaypointHere(String name, String category, String color, String source) {
		Minecraft mc = Minecraft.getInstance();
		DataPoint<String> key = adapter.worldKey();
		if (mc.player == null || key.value().isEmpty()) return OpResult.fail("Aucun monde chargé : waypoint non ajouté");
		Waypoint wp = new Waypoint(name, mc.player.getBlockX(), mc.player.getBlockY(), mc.player.getBlockZ(),
				adapter.dimension().orElse("overworld"));
		wp.category = category;
		if (color != null) wp.color = color;
		wp.source = source;
		String k = key.value().get();
		String kind = k.startsWith("java:sp:") ? "singleplayer" : k.startsWith("java:realm:") ? "realm" : "server";
		waypoints.add(k, adapter.worldName().orElse(""), kind, wp);
		OpResult saved = waypoints.save();
		return saved.ok() ? OpResult.ok("Waypoint « " + name + " » ajouté en " + wp.x + " " + wp.y + " " + wp.z) : saved;
	}
}
