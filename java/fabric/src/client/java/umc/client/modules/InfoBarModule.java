package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;

import java.time.LocalTime;
import java.time.format.DateTimeFormatter;

/** Barre compacte : FPS, ping, coordonnées et heure réelle sur une ligne. */
public final class InfoBarModule extends Module {
	private static final DateTimeFormatter CLOCK = DateTimeFormatter.ofPattern("HH:mm");

	private final Settings.Bool showFps = add(new Settings.Bool("show_fps", "FPS", "Images par seconde", true));
	private final Settings.Bool showPing = add(new Settings.Bool("show_ping", "Ping", "Latence serveur", true));
	private final Settings.Bool showCoords = add(new Settings.Bool("show_coords", "Coordonnées", "Position X Y Z", true));
	private final Settings.Bool showClock = add(new Settings.Bool("show_clock", "Heure réelle", "Horloge de l'ordinateur", true));

	public InfoBarModule() {
		super("info_bar", "Info Bar", "FPS, ping, position et heure sur une ligne", Category.RENDER, false);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultX() {
		return 0.35f;
	}

	@Override
	public float defaultY() {
		return 0.005f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		HudText t = new HudText(ctx);
		boolean first = true;
		if (showFps.on()) {
			int fps = ctx.adapter().gameFps().orElse(0);
			t.add("FPS ", ctx.theme().accent()).add(String.valueOf(fps), FpsModule.color(ctx, fps));
			first = false;
		}
		if (showPing.on()) {
			if (!first) t.add("  •  ", ctx.theme().textDim());
			t.add("Ping ", ctx.theme().accent()).add(PingModule.text(ctx), PingModule.color(ctx));
			first = false;
		}
		if (showCoords.on() && ctx.mc().player != null) {
			if (!first) t.add("  •  ", ctx.theme().textDim());
			var p = ctx.mc().player;
			t.add("XYZ ", ctx.theme().accent()).text(p.getBlockX() + " " + p.getBlockY() + " " + p.getBlockZ());
			first = false;
		}
		if (showClock.on()) {
			if (!first) t.add("  •  ", ctx.theme().textDim());
			t.text(LocalTime.now().format(CLOCK));
			first = false;
		}
		if (first) t.add("Info Bar (tout est masqué)", ctx.theme().textDim());
		return t.draw(g);
	}
}
