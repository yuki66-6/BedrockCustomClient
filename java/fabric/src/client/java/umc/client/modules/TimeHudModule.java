package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;
import umc.common.util.GameMath;

import java.time.LocalTime;
import java.time.format.DateTimeFormatter;

/** Heure du jeu (horloge de l'Overworld), numéro du jour et heure réelle. */
public final class TimeHudModule extends Module {
	private static final DateTimeFormatter CLOCK = DateTimeFormatter.ofPattern("HH:mm");

	private final Settings.Bool showGame = add(new Settings.Bool("show_game_time", "Heure du jeu", "Horloge de l'Overworld (hh:mm)", true));
	private final Settings.Bool showDay = add(new Settings.Bool("show_day", "Jour", "Numéro du jour en jeu", true));
	private final Settings.Bool showReal = add(new Settings.Bool("show_real_time", "Heure réelle", "Horloge de l'ordinateur", false));

	public TimeHudModule() {
		super("time_hud", "Time HUD", "Heure du jeu, jour et heure réelle", Category.RENDER, false);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.16f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		HudText t = new HudText(ctx);
		var level = ctx.mc().level;
		if (level != null) {
			long time = level.getOverworldClockTime();
			if (showGame.on()) t.label("Heure ", GameMath.gameClock(time));
			if (showDay.on()) t.label(showGame.on() ? "  Jour " : "Jour ", String.valueOf(GameMath.gameDay(time)));
		}
		if (showReal.on()) {
			if (!t.isEmpty()) t.row();
			t.label("Réel ", LocalTime.now().format(CLOCK));
		}
		if (t.isEmpty()) t.add("Time HUD", ctx.theme().textDim());
		return t.draw(g);
	}
}
