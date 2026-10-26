package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.world.effect.MobEffectInstance;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;
import umc.common.util.GameMath;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/** Effets actifs du joueur avec niveau et durée restante. */
public final class PotionEffectsModule extends Module {
	private final Settings.Choice sort = add(new Settings.Choice("sort", "Tri", "Ordre des effets", List.of("Durée restante", "Nom"), 0));
	private final Settings.Bool effectColor = add(new Settings.Bool("effect_color", "Couleur de l'effet", "Nom dans la couleur de la potion", true));
	private final Settings.Bool hideAmbient = add(new Settings.Bool("hide_ambient", "Masquer les balises", "Ignore les effets de balise (ambiants)", false));

	public PotionEffectsModule() {
		super("potion_effects", "Potion Effects HUD", "Effets actifs, niveau et durée", Category.RENDER, false);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultX() {
		return 0.85f;
	}

	@Override
	public float defaultY() {
		return 0.2f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		var p = ctx.mc().player;
		List<MobEffectInstance> effects = new ArrayList<>();
		if (p != null) for (MobEffectInstance e : p.getActiveEffects()) if (!(hideAmbient.on() && e.isAmbient())) effects.add(e);
		if (effects.isEmpty()) {
			if (!ctx.editing()) return new int[] {0, 0};
			return new HudText(ctx).add("Aucun effet actif", ctx.theme().textDim()).draw(g);
		}
		if (sort.get() == 0) effects.sort(Comparator.comparingInt((MobEffectInstance e) -> e.isInfiniteDuration() ? Integer.MAX_VALUE : e.getDuration()));
		else effects.sort(Comparator.comparing((MobEffectInstance e) -> e.getEffect().value().getDisplayName().getString()));
		HudText t = new HudText(ctx);
		for (MobEffectInstance e : effects) {
			String name = e.getEffect().value().getDisplayName().getString();
			if (e.getAmplifier() > 0) name += " " + GameMath.roman(e.getAmplifier() + 1);
			int color = effectColor.on() ? 0xFF000000 | e.getEffect().value().getColor() : ctx.theme().text();
			t.row().add(name, color).add("  " + GameMath.formatTicks(e.getDuration(), e.isInfiniteDuration()), ctx.theme().textDim());
		}
		return t.draw(g);
	}
}
