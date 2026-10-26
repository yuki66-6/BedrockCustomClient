package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.data.DataPoint;
import umc.common.settings.Settings;

/** Latence indiquée par le serveur dans le tableau des joueurs (« indisponible » sinon). */
public final class PingModule extends Module {
	private final Settings.Bool hideInSingleplayer = add(new Settings.Bool("hide_singleplayer", "Masquer en solo", "Le ping n'a pas de sens en solo", true));

	public PingModule() {
		super("ping", "Ping", "Latence avec le serveur", Category.RENDER, false);
	}

	public static String text(HudContext ctx) {
		if (ctx.mc().getSingleplayerServer() != null) return "solo";
		DataPoint<Integer> p = ctx.adapter().latencyMs();
		return p.value().map(v -> v + " ms").orElse("indisponible");
	}

	public static int color(HudContext ctx) {
		DataPoint<Integer> p = ctx.adapter().latencyMs();
		if (p.value().isEmpty() || ctx.mc().getSingleplayerServer() != null) return ctx.theme().textDim();
		int v = p.value().get();
		return v < 80 ? ctx.theme().good() : v < 160 ? ctx.theme().warn() : ctx.theme().bad();
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.33f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		if (hideInSingleplayer.on() && ctx.mc().getSingleplayerServer() != null && !ctx.editing()) return new int[] {0, 0};
		return new HudText(ctx).add("Ping ", ctx.theme().accent()).add(text(ctx), color(ctx)).draw(g);
	}
}
