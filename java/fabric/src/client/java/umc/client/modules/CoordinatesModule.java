package umc.client.modules;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;
import umc.common.util.GameMath;

/** Coordonnées du joueur, équivalent Nether / Overworld et chunk. */
public final class CoordinatesModule extends Module {
	private final Settings.Int decimals = add(new Settings.Int("decimals", "Décimales", "Précision des coordonnées", 0, 0, 2, "%d"));
	private final Settings.Bool showOther = add(new Settings.Bool("show_other_dimension", "Équivalent Nether / Overworld",
			"Coordonnées converties (÷ 8 ou × 8)", true));
	private final Settings.Bool showChunk = add(new Settings.Bool("show_chunk", "Chunk", "Numéro du chunk et position dans le chunk", false));
	private final Settings.Bool showDimension = add(new Settings.Bool("show_dimension", "Dimension", "Nom de la dimension actuelle", false));

	public CoordinatesModule() {
		super("coordinates", "Coordinates HUD", "Position X Y Z et conversion Nether", Category.MOVEMENT, true);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.02f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		HudText t = new HudText(ctx);
		var p = ctx.mc().player;
		if (p == null) return t.label("XYZ ", "aucun monde").draw(g);
		String f = "%." + decimals.get() + "f";
		t.label("X ", String.format(f, p.getX())).label("  Y ", String.format(f, p.getY())).label("  Z ", String.format(f, p.getZ()));
		String dim = ctx.adapter().dimension().orElse("overworld");
		int bx = p.getBlockX(), bz = p.getBlockZ();
		if (showOther.on()) {
			if (dim.equals("overworld"))
				t.row().add("Nether ", ctx.theme().textDim()).text(GameMath.overworldToNether(bx) + ", " + GameMath.overworldToNether(bz));
			else if (dim.equals("nether"))
				t.row().add("Overworld ", ctx.theme().textDim()).text(GameMath.netherToOverworld(bx) + ", " + GameMath.netherToOverworld(bz));
		}
		if (showChunk.on())
			t.row().add("Chunk ", ctx.theme().textDim())
					.text((bx >> 4) + ", " + (bz >> 4) + "  (" + Math.floorMod(bx, 16) + ", " + Math.floorMod(bz, 16) + ")");
		if (showDimension.on()) t.row().add("Dimension ", ctx.theme().textDim()).text(dim);
		return t.draw(g);
	}
}
