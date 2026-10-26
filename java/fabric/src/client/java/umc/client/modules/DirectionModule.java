package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;
import umc.common.util.GameMath;

import java.util.List;

/** Direction du regard : point cardinal, axe et angle. */
public final class DirectionModule extends Module {
	private final Settings.Choice style = add(new Settings.Choice("style", "Style", "Format de la direction",
			List.of("Cardinal + axe", "Cardinal complet", "Degrés"), 0));
	private final Settings.Bool showPitch = add(new Settings.Bool("show_pitch", "Inclinaison", "Angle vertical du regard", false));

	public DirectionModule() {
		super("direction", "Direction HUD", "Point cardinal et axe regardés", Category.MOVEMENT, false);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.1f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		var p = ctx.mc().player;
		HudText t = new HudText(ctx);
		if (p == null) return t.label("Direction ", "aucun monde").draw(g);
		float yaw = p.getYRot();
		String v = switch (style.get()) {
			case 1 -> GameMath.cardinalFull(yaw);
			case 2 -> String.format("%.1f°", GameMath.normalizeYaw(yaw));
			default -> GameMath.cardinal(yaw) + " (" + GameMath.axis(yaw) + ")";
		};
		t.label("Direction ", v);
		if (showPitch.on()) t.add(String.format("  %.1f°", p.getXRot()), ctx.theme().textDim());
		return t.draw(g);
	}
}
