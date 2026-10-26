package umc.client.modules;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.DeathScreen;
import net.minecraft.network.chat.Component;
import umc.client.Umc;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.profile.OpResult;
import umc.common.settings.Settings;
import umc.common.waypoint.Waypoint;

import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/** Crée un waypoint « Mort » à l'endroit de chaque mort (écran de mort du jeu). */
public final class DeathWaypointModule extends Module {
	private static final DateTimeFormatter STAMP = DateTimeFormatter.ofPattern("dd/MM HH:mm");

	private final Settings.Int keepLast = add(new Settings.Int("keep_last", "Morts conservées", "Plus anciennes supprimées au-delà", 5, 1, 20, "%d"));
	private final Settings.Bool chatMessage = add(new Settings.Bool("chat_message", "Message local", "Affiche les coordonnées dans le chat (visible par vous seul)", true));

	private boolean recorded;

	public DeathWaypointModule() {
		super("death_waypoint", "Death Waypoint", "Waypoint automatique à l'endroit de la mort", Category.PLAYER, true);
	}

	@Override
	protected void onTick(Minecraft mc) {
		if (mc.player == null) return;
		boolean dead = mc.gui.screen() instanceof DeathScreen;
		if (!dead) {
			if (mc.player.isAlive()) recorded = false;
			return;
		}
		if (recorded) return;
		recorded = true;
		OpResult r = Umc.addWaypointHere("Mort " + LocalDateTime.now().format(STAMP), "death", "#9EA3B3", "death");
		if (r.ok()) prune();
		if (chatMessage.on()) mc.player.sendSystemMessage(Component.literal("[UMC] " + r.message()));
	}

	private void prune() {
		String key = Umc.adapter.worldKey().orElse(null);
		if (key == null) return;
		List<Waypoint> deaths = new ArrayList<>();
		for (Waypoint wp : Umc.waypoints.forWorld(key)) if ("death".equals(wp.source)) deaths.add(wp);
		deaths.sort(Comparator.comparingLong(wp -> wp.createdAt));
		for (int i = 0; i < deaths.size() - keepLast.get(); i++) Umc.waypoints.remove(key, deaths.get(i).id);
		Umc.waypoints.save();
	}
}
