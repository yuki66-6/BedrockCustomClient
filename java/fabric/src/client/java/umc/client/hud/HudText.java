package umc.client.hud;

import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.gui.Draw;

import java.util.ArrayList;
import java.util.List;

/** Bloc de texte HUD multi-lignes et multicolore, avec fond optionnel. */
public final class HudText {
	private record Segment(String text, int color) {
	}

	public static final int PAD = 3;

	private final List<List<Segment>> rows = new ArrayList<>();
	private final HudContext ctx;

	public HudText(HudContext ctx) {
		this.ctx = ctx;
	}

	public HudText row() {
		rows.add(new ArrayList<>());
		return this;
	}

	public HudText add(String text, int color) {
		if (rows.isEmpty()) row();
		rows.getLast().add(new Segment(text, color));
		return this;
	}

	public HudText text(String text) {
		return add(text, ctx.theme().text());
	}

	/** « Libellé : » en couleur d'accent puis la valeur. */
	public HudText label(String label, String value) {
		return add(label, ctx.theme().accent()).add(value, ctx.theme().text());
	}

	public boolean isEmpty() {
		return rows.isEmpty();
	}

	public int[] draw(GuiGraphicsExtractor g) {
		int lh = ctx.font().lineHeight + 1;
		int w = 0;
		for (List<Segment> r : rows) {
			int rw = 0;
			for (Segment s : r) rw += ctx.font().width(s.text());
			w = Math.max(w, rw);
		}
		int width = w + 2 * PAD, height = rows.size() * lh + 2 * PAD - 1;
		if (ctx.background()) Draw.round(g, 0, 0, width, height, 2, ctx.theme().hudBg());
		boolean shadow = ctx.theme().textShadow.on();
		int y = PAD;
		for (List<Segment> r : rows) {
			int x = PAD;
			for (Segment s : r) {
				Draw.text(g, ctx.font(), s.text(), x, y, s.color(), shadow);
				x += ctx.font().width(s.text());
			}
			y += lh;
		}
		return new int[] {width, height};
	}
}
