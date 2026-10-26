package umc.client.gui;

import net.minecraft.client.gui.Font;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/** Primitives de dessin (rectangles arrondis, interrupteurs…) construites sur les remplissages du jeu. */
public final class Draw {
	private Draw() {
	}

	public static int withAlpha(int argb, float alpha) {
		int a = Math.round(((argb >>> 24) & 0xff) * Math.clamp(alpha, 0f, 1f));
		return (a << 24) | (argb & 0xFFFFFF);
	}

	public static int mix(int a, int b, float t) {
		t = Math.clamp(t, 0f, 1f);
		int ca = (int) (((a >>> 24) & 0xff) + ((((b >>> 24) & 0xff) - ((a >>> 24) & 0xff)) * t));
		int cr = (int) (((a >> 16) & 0xff) + ((((b >> 16) & 0xff) - ((a >> 16) & 0xff)) * t));
		int cg = (int) (((a >> 8) & 0xff) + ((((b >> 8) & 0xff) - ((a >> 8) & 0xff)) * t));
		int cb = (int) ((a & 0xff) + (((b & 0xff) - (a & 0xff)) * t));
		return (ca << 24) | (cr << 16) | (cg << 8) | cb;
	}

	public static void rect(GuiGraphicsExtractor g, int x, int y, int w, int h, int argb) {
		if (w <= 0 || h <= 0) return;
		g.fill(x, y, x + w, y + h, argb);
	}

	/** Rectangle aux coins arrondis (rayon en pixels GUI, approximé ligne par ligne). */
	public static void round(GuiGraphicsExtractor g, int x, int y, int w, int h, int r, int argb) {
		if (w <= 0 || h <= 0) return;
		r = Math.min(r, Math.min(w, h) / 2);
		if (r <= 0) {
			rect(g, x, y, w, h, argb);
			return;
		}
		rect(g, x, y + r, w, h - 2 * r, argb);
		for (int i = 0; i < r; i++) {
			double dy = r - i - 0.5;
			int inset = (int) Math.round(r - Math.sqrt(r * r - dy * dy));
			rect(g, x + inset, y + i, w - 2 * inset, 1, argb);
			rect(g, x + inset, y + h - 1 - i, w - 2 * inset, 1, argb);
		}
	}

	public static void border(GuiGraphicsExtractor g, int x, int y, int w, int h, int argb) {
		rect(g, x, y, w, 1, argb);
		rect(g, x, y + h - 1, w, 1, argb);
		rect(g, x, y + 1, 1, h - 2, argb);
		rect(g, x + w - 1, y + 1, 1, h - 2, argb);
	}

	/** Interrupteur animé : {@code t} de 0 (désactivé) à 1 (activé). */
	public static void toggle(GuiGraphicsExtractor g, int x, int y, float t, int accent) {
		int w = 18, h = 10;
		round(g, x, y, w, h, 5, mix(0xFF3A3F4D, accent, t));
		int knob = x + 1 + Math.round(t * (w - h));
		round(g, knob, y + 1, h - 2, h - 2, 4, 0xFFFFFFFF);
	}

	public static void text(GuiGraphicsExtractor g, Font font, String s, int x, int y, int argb, boolean shadow) {
		g.text(font, s, x, y, argb | (argb >>> 24 == 0 ? 0xFF000000 : 0), shadow);
	}

	public static String ellipsize(Font font, String s, int maxWidth) {
		if (font.width(s) <= maxWidth) return s;
		return font.plainSubstrByWidth(s, Math.max(0, maxWidth - font.width("…"))) + "…";
	}

	public static boolean inside(double mx, double my, int x, int y, int w, int h) {
		return mx >= x && my >= y && mx < x + w && my < y + h;
	}
}
