package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.client.telemetry.ClickTracker;
import umc.common.settings.Settings;

import java.util.List;

/** Clics par seconde réels (chaque appui de la touche d'attaque / d'utilisation). */
public final class CpsModule extends Module {
	private final Settings.Choice mode = add(new Settings.Choice("mode", "Affichage", "Boutons comptés",
			List.of("Gauche", "Gauche | Droit"), 1));
	private final Settings.Bool showPeak = add(new Settings.Bool("show_peak", "Afficher le record", "Meilleur CPS de la session", false));

	public CpsModule() {
		super("cps", "CPS Counter", "Clics par seconde (gauche et droit)", Category.COMBAT, false);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.36f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		String v = mode.get() == 0 ? String.valueOf(ClickTracker.left()) : ClickTracker.left() + " | " + ClickTracker.right();
		HudText t = new HudText(ctx).label("CPS ", v);
		if (showPeak.on()) t.add("  max " + ClickTracker.LEFT.peak(), ctx.theme().textDim());
		return t.draw(g);
	}
}
