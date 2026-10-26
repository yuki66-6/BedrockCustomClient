package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;

/** Images par seconde mesurées par le jeu. */
public final class FpsModule extends Module {
	private final Settings.Bool colored = add(new Settings.Bool("colored", "Couleur selon la valeur", "Vert ≥ 60, orange ≥ 30, rouge en dessous", true));

	public FpsModule() {
		super("fps", "FPS Counter", "Images par seconde du jeu", Category.RENDER, true);
	}

	public static int color(HudContext ctx, int fps) {
		return fps >= 60 ? ctx.theme().good() : fps >= 30 ? ctx.theme().warn() : ctx.theme().bad();
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.3f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		int fps = ctx.adapter().gameFps().orElse(0);
		return new HudText(ctx).add("FPS ", ctx.theme().accent()).add(String.valueOf(fps), colored.on() ? color(ctx, fps) : ctx.theme().text())
				.draw(g);
	}
}
