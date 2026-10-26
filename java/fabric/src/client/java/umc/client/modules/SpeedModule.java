package umc.client.modules;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.client.telemetry.ClickTracker;
import umc.common.settings.Settings;
import umc.common.util.GameMath;

import java.util.List;

/** Vitesse réelle du joueur, mesurée sur ses positions successives (lissée sur ½ seconde). */
public final class SpeedModule extends Module {
	private final Settings.Choice unit = add(new Settings.Choice("unit", "Unité", "Unité d'affichage", List.of("blocs/s", "km/h"), 0));
	private final Settings.Int decimals = add(new Settings.Int("decimals", "Décimales", "Précision affichée", 1, 0, 3, "%d"));
	private final Settings.Bool vertical = add(new Settings.Bool("show_vertical", "Vitesse verticale", "Ajoute la vitesse de montée / chute", false));

	private final GameMath.SpeedTracker tracker = new GameMath.SpeedTracker(10);

	public SpeedModule() {
		super("speed", "Speed Meter", "Vitesse de déplacement réelle", Category.MOVEMENT, false);
	}

	@Override
	protected void onTick(Minecraft mc) {
		if (mc.player == null) {
			tracker.clear();
			return;
		}
		tracker.sample(ClickTracker.now(), mc.player.getX(), mc.player.getY(), mc.player.getZ());
	}

	@Override
	protected void onDisable() {
		tracker.clear();
	}

	private String format(double blocksPerSecond) {
		double v = unit.get() == 1 ? blocksPerSecond * 3.6 : blocksPerSecond;
		return String.format("%." + decimals.get() + "f %s", v, unit.get() == 1 ? "km/h" : "b/s");
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.42f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		HudText t = new HudText(ctx).label("Vitesse ", format(tracker.horizontal()));
		if (vertical.on()) t.row().label("Vertical ", format(tracker.vertical()));
		return t.draw(g);
	}
}
