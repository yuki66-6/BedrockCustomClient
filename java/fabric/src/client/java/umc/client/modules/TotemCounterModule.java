package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import umc.client.gui.Draw;
import umc.client.hud.HudContext;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;

/** Nombre de totems d'immortalité dans l'inventaire (main secondaire et armure comprises). */
public final class TotemCounterModule extends Module {
	private final Settings.Bool hideWhenZero = add(new Settings.Bool("hide_when_zero", "Masquer à zéro", "N'affiche rien sans totem", false));
	private final Settings.Int warnBelow = add(new Settings.Int("warn_below", "Alerte sous", "Couleur rouge à partir de ce nombre", 1, 0, 10, "%d"));

	private static final ItemStack ICON = new ItemStack(Items.TOTEM_OF_UNDYING);

	public TotemCounterModule() {
		super("totem_counter", "Totem Counter", "Nombre de totems dans l'inventaire", Category.COMBAT, false);
	}

	public static int count(Inventory inv) {
		int n = 0;
		for (int i = 0; i < inv.getContainerSize(); i++) {
			ItemStack s = inv.getItem(i);
			if (s.getItem() == Items.TOTEM_OF_UNDYING) n += s.getCount();
		}
		return n;
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultX() {
		return 0.47f;
	}

	@Override
	public float defaultY() {
		return 0.62f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		int n = ctx.mc().player == null ? 0 : count(ctx.mc().player.getInventory());
		if (n == 0 && hideWhenZero.on() && !ctx.editing()) return new int[] {0, 0};
		String s = "×" + n;
		int w = 18 + ctx.font().width(s) + 4, h = 20;
		if (ctx.background()) Draw.round(g, 0, 0, w, h, 2, ctx.theme().hudBg());
		g.item(ICON, 2, 2);
		int color = n < warnBelow.get() || n == 0 ? ctx.theme().bad() : ctx.theme().text();
		Draw.text(g, ctx.font(), s, 20, 6, color, ctx.theme().textShadow.on());
		return new int[] {w, h};
	}
}
