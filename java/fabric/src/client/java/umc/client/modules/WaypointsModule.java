package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.Umc;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;
import umc.common.util.GameMath;
import umc.common.waypoint.Waypoint;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/**
 * Liste HUD des waypoints du monde et de la dimension actuels, triés par distance,
 * avec flèche de direction. Gestion complète dans la page « Waypoints » du menu.
 */
public final class WaypointsModule extends Module {
	private final Settings.Int maxShown = add(new Settings.Int("max_shown", "Nombre affiché", "Waypoints les plus proches affichés", 5, 1, 15, "%d"));
	private final Settings.Int maxDistance = add(new Settings.Int("max_distance", "Distance max", "0 = illimitée", 0, 0, 20000, "%d blocs"));
	private final Settings.Bool showArrow = add(new Settings.Bool("show_arrow", "Flèche de direction", "Direction relative au regard", true));
	private final Settings.Bool showCoords = add(new Settings.Bool("show_coords", "Coordonnées", "Affiche X Y Z de chaque waypoint", false));

	public WaypointsModule() {
		super("waypoints", "Waypoints", "Points enregistrés du monde actuel, distance et direction", Category.UTILITY, true);
	}

	@Override
	public String limitation() {
		return "Balises 3D dans le monde pas encore implémentées : liste HUD et page Waypoints uniquement.";
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultX() {
		return 0.01f;
	}

	@Override
	public float defaultY() {
		return 0.7f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		var p = ctx.mc().player;
		HudText t = new HudText(ctx);
		String key = ctx.adapter().worldKey().orElse(null);
		if (p == null || key == null) return ctx.editing() ? t.add("Waypoints (aucun monde)", ctx.theme().textDim()).draw(g) : new int[] {0, 0};
		List<Waypoint> list = new ArrayList<>();
		for (Waypoint wp : Umc.waypoints.forWorldAndDimension(key, ctx.adapter().dimension().orElse("overworld"))) {
			if (!wp.visible) continue;
			if (maxDistance.get() > 0 && wp.distanceTo(p.getX(), p.getY(), p.getZ()) > maxDistance.get()) continue;
			list.add(wp);
		}
		if (list.isEmpty()) return ctx.editing() ? t.add("Aucun waypoint ici", ctx.theme().textDim()).draw(g) : new int[] {0, 0};
		list.sort(Comparator.comparingDouble(wp -> wp.distanceTo(p.getX(), p.getY(), p.getZ())));
		for (int i = 0; i < Math.min(maxShown.get(), list.size()); i++) {
			Waypoint wp = list.get(i);
			t.row();
			if (showArrow.on())
				t.add(GameMath.arrow(GameMath.relativeAngle(p.getYRot(), p.getX(), p.getZ(), wp.x + 0.5, wp.z + 0.5)) + " ", ctx.theme().accent());
			t.add(wp.name, wp.argb()).add(String.format("  %.0f m", wp.distanceTo(p.getX(), p.getY(), p.getZ())), ctx.theme().textDim());
			if (showCoords.on()) t.add("  " + wp.x + " " + wp.y + " " + wp.z, ctx.theme().textDim());
		}
		return t.draw(g);
	}
}
