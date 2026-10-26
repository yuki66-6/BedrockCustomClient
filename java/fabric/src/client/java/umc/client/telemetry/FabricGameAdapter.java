package umc.client.telemetry;

import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.PlayerInfo;
import net.minecraft.client.multiplayer.ServerData;
import net.minecraft.client.server.IntegratedServer;
import net.minecraft.world.level.storage.LevelResource;
import umc.common.data.Availability;
import umc.common.data.Capability;
import umc.common.data.CapabilityInfo;
import umc.common.data.DataPoint;
import umc.common.data.DataSource;
import umc.common.waypoint.WaypointStore;

import java.nio.file.Path;

/** Adaptateur Java : données lues par l'API du jeu via Fabric (mod chargé dans le client). */
public final class FabricGameAdapter implements GameAdapter {
	private static final String NOT_IN_GAME = "Aucun monde chargé.";

	private final Minecraft mc;

	public FabricGameAdapter(Minecraft mc) {
		this.mc = mc;
	}

	private static double now() {
		return ClickTracker.now();
	}

	private boolean inGame() {
		return mc.player != null && mc.level != null;
	}

	@Override
	public String name() {
		return "Minecraft Java (mod Fabric)";
	}

	@Override
	public CapabilityInfo capability(Capability c) {
		boolean game = inGame();
		return switch (c) {
			case KEYBOARD_INPUT, MOUSE_INPUT, GAME_WINDOW -> new CapabilityInfo(c, Availability.AVAILABLE, DataSource.GAME_API, "");
			case GAME_FPS -> new CapabilityInfo(c, Availability.AVAILABLE, DataSource.GAME_API, "FPS réel mesuré par le jeu.");
			case OVERLAY_FPS -> new CapabilityInfo(c, Availability.UNSUPPORTED, DataSource.NONE,
					"Sans objet : le menu est dessiné par le jeu lui-même, pas par un overlay.");
			case SERVER_LATENCY -> latencyMs().value().isPresent()
					? new CapabilityInfo(c, Availability.AVAILABLE, DataSource.GAME_API, "Latence du tableau des joueurs (mesurée par le serveur).")
					: new CapabilityInfo(c, Availability.UNAVAILABLE, DataSource.GAME_API, latencyMs().reason());
			case PLAYER_POSITION, PLAYER_ROTATION, PLAYER_VELOCITY, PLAYER_HEALTH, PLAYER_HUNGER, PLAYER_ARMOR, INVENTORY,
					POTION_EFFECTS, DIMENSION, WORLD_IDENTITY, DEATH_EVENTS ->
				game ? new CapabilityInfo(c, Availability.AVAILABLE, DataSource.GAME_API, "")
						: new CapabilityInfo(c, Availability.UNAVAILABLE, DataSource.GAME_API, NOT_IN_GAME);
			case WORLD_SEED -> seed().value().isPresent()
					? new CapabilityInfo(c, Availability.AVAILABLE, DataSource.GAME_API, "Monde solo : seed lue sur le serveur intégré.")
					: new CapabilityInfo(c, seed().availability(), DataSource.NONE, seed().reason());
			case SAVED_WORLD_MAP -> new CapabilityInfo(c, Availability.UNSUPPORTED, DataSource.NONE,
					"Carte Java pas encore implémentée dans cette version.");
			case IN_WORLD_RENDER -> new CapabilityInfo(c, Availability.UNSUPPORTED, DataSource.NONE,
					"Balises 3D des waypoints pas encore implémentées (liste HUD disponible).");
			case FOV_CONTROL, MOVEMENT_TOGGLES -> new CapabilityInfo(c, Availability.AVAILABLE, DataSource.GAME_API, "");
			case CHAT_COMMANDS -> new CapabilityInfo(c, Availability.PERMISSION_REQUIRED, DataSource.NONE,
					"Les commandes (/tp, /effect…) dépendent des droits du monde ou du serveur ; aucune n'est envoyée automatiquement.");
		};
	}

	@Override
	public DataPoint<Integer> gameFps() {
		return DataPoint.available(mc.getFps(), DataSource.GAME_API, now(), 2);
	}

	@Override
	public DataPoint<Integer> latencyMs() {
		if (!inGame() || mc.getConnection() == null) return DataPoint.missing(Availability.UNAVAILABLE, NOT_IN_GAME, DataSource.GAME_API);
		PlayerInfo info = mc.getConnection().getPlayerInfo(mc.player.getUUID());
		if (info == null) return DataPoint.missing(Availability.UNAVAILABLE, "Joueur absent du tableau des joueurs.", DataSource.GAME_API);
		return DataPoint.available(info.getLatency(), DataSource.GAME_API, now(), 10);
	}

	@Override
	public DataPoint<double[]> position() {
		if (!inGame()) return DataPoint.missing(Availability.UNAVAILABLE, NOT_IN_GAME, DataSource.GAME_API);
		return DataPoint.available(new double[] {mc.player.getX(), mc.player.getY(), mc.player.getZ()}, DataSource.GAME_API, now(), 1);
	}

	@Override
	public DataPoint<Float> yaw() {
		if (!inGame()) return DataPoint.missing(Availability.UNAVAILABLE, NOT_IN_GAME, DataSource.GAME_API);
		return DataPoint.available(mc.player.getYRot(), DataSource.GAME_API, now(), 1);
	}

	@Override
	public DataPoint<String> dimension() {
		if (!inGame()) return DataPoint.missing(Availability.UNAVAILABLE, NOT_IN_GAME, DataSource.GAME_API);
		String id = mc.level.dimension().identifier().toString();
		return DataPoint.available(WaypointStore.normalizeDimension(id), DataSource.GAME_API, now(), 0);
	}

	@Override
	public DataPoint<String> worldKey() {
		if (!inGame()) return DataPoint.missing(Availability.UNAVAILABLE, NOT_IN_GAME, DataSource.GAME_API);
		IntegratedServer sp = mc.getSingleplayerServer();
		if (sp != null) {
			Path root = sp.getWorldPath(LevelResource.ROOT).normalize();
			return DataPoint.available("java:sp:" + root.getFileName(), DataSource.GAME_API, now(), 0);
		}
		ServerData server = mc.getCurrentServer();
		if (server != null) {
			String key = server.isRealm() ? "java:realm:" + server.name : "java:mp:" + server.ip.toLowerCase();
			return DataPoint.available(key, DataSource.GAME_API, now(), 0);
		}
		return DataPoint.missing(Availability.UNAVAILABLE, "Type de partie inconnu.", DataSource.GAME_API);
	}

	@Override
	public DataPoint<String> worldName() {
		if (!inGame()) return DataPoint.missing(Availability.UNAVAILABLE, NOT_IN_GAME, DataSource.GAME_API);
		IntegratedServer sp = mc.getSingleplayerServer();
		if (sp != null) return DataPoint.available(sp.getWorldData().getLevelName(), DataSource.GAME_API, now(), 0);
		ServerData server = mc.getCurrentServer();
		return server != null ? DataPoint.available(server.name, DataSource.GAME_API, now(), 0)
				: DataPoint.missing(Availability.UNAVAILABLE, "Type de partie inconnu.", DataSource.GAME_API);
	}

	@Override
	public DataPoint<Long> seed() {
		IntegratedServer sp = mc.getSingleplayerServer();
		if (sp == null)
			return DataPoint.missing(Availability.UNAVAILABLE,
					inGame() ? "Un serveur ne transmet pas sa seed au client ; saisie manuelle possible." : NOT_IN_GAME, DataSource.NONE);
		return DataPoint.available(sp.getWorldGenSettings().options().seed(), DataSource.GAME_API, now(), 0);
	}
}
