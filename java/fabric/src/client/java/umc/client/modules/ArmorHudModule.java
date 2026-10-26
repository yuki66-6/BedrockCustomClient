package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.item.ItemStack;
import umc.client.gui.Draw;
import umc.client.hud.HudContext;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;

import java.util.ArrayList;
import java.util.List;

/** Pièces d'armure portées et objets en main, avec leur durabilité. */
public final class ArmorHudModule extends Module {
	private final Settings.Choice layout = add(new Settings.Choice("layout", "Disposition", "Sens d'empilement", List.of("Vertical", "Horizontal"), 0));
	private final Settings.Choice durability = add(new Settings.Choice("durability", "Durabilité", "Format de la durabilité",
			List.of("Pourcentage", "Restante", "Aucune"), 0));
	private final Settings.Bool mainHand = add(new Settings.Bool("show_mainhand", "Main principale", "Affiche l'objet tenu", true));
	private final Settings.Bool offHand = add(new Settings.Bool("show_offhand", "Main secondaire", "Affiche l'objet de la main gauche", false));

	private static final EquipmentSlot[] ARMOR = {EquipmentSlot.HEAD, EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET};

	public ArmorHudModule() {
		super("armor_hud", "Armor HUD", "Armure portée et durabilité", Category.RENDER, false);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultX() {
		return 0.9f;
	}

	@Override
	public float defaultY() {
		return 0.6f;
	}

	private String durabilityText(ItemStack s) {
		if (!s.isDamageableItem() || durability.get() == 2) return s.getCount() > 1 ? String.valueOf(s.getCount()) : "";
		int left = s.getMaxDamage() - s.getDamageValue();
		return durability.get() == 0 ? Math.round(100f * left / s.getMaxDamage()) + "%" : String.valueOf(left);
	}

	private int durabilityColor(HudContext ctx, ItemStack s) {
		if (!s.isDamageableItem()) return ctx.theme().text();
		float f = (s.getMaxDamage() - s.getDamageValue()) / (float) s.getMaxDamage();
		return f > 0.5f ? ctx.theme().good() : f > 0.2f ? ctx.theme().warn() : ctx.theme().bad();
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		var p = ctx.mc().player;
		List<ItemStack> items = new ArrayList<>();
		if (p != null) {
			if (mainHand.on()) items.add(p.getItemBySlot(EquipmentSlot.MAINHAND));
			if (offHand.on()) items.add(p.getItemBySlot(EquipmentSlot.OFFHAND));
			for (EquipmentSlot slot : ARMOR) items.add(p.getItemBySlot(slot));
			items.removeIf(ItemStack::isEmpty);
		}
		if (items.isEmpty()) {
			if (!ctx.editing()) return new int[] {0, 0};
			int w = ctx.font().width("Armor HUD") + 6;
			if (ctx.background()) Draw.round(g, 0, 0, w, 14, 2, ctx.theme().hudBg());
			Draw.text(g, ctx.font(), "Armor HUD", 3, 3, ctx.theme().textDim(), false);
			return new int[] {w, 14};
		}
		boolean vertical = layout.get() == 0;
		int textW = 0;
		for (ItemStack s : items) textW = Math.max(textW, ctx.font().width(durabilityText(s)));
		int cellW = vertical ? 18 + textW + 2 : 18 + Math.max(0, textW - 16) + 2, cellH = vertical ? 17 : 27;
		int w = vertical ? cellW + 2 : items.size() * cellW + 2, h = vertical ? items.size() * cellH + 2 : cellH + 2;
		if (ctx.background()) Draw.round(g, 0, 0, w, h, 2, ctx.theme().hudBg());
		boolean shadow = ctx.theme().textShadow.on();
		for (int i = 0; i < items.size(); i++) {
			ItemStack s = items.get(i);
			int x = vertical ? 1 : 1 + i * cellW, y = vertical ? 1 + i * cellH : 1;
			g.item(s, x, y);
			String d = durabilityText(s);
			if (vertical) Draw.text(g, ctx.font(), d, x + 18, y + 4, durabilityColor(ctx, s), shadow);
			else Draw.text(g, ctx.font(), d, x + (16 - ctx.font().width(d)) / 2, y + 17, durabilityColor(ctx, s), shadow);
		}
		return new int[] {w, h};
	}
}
