package umc.client.gui;

import net.minecraft.client.gui.Font;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.network.chat.Component;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.DoubleConsumer;
import java.util.function.IntPredicate;

/**
 * Interface en mode immédiat : chaque image redessine les widgets et enregistre
 * leurs zones cliquables ; les clics sont testés sur les zones de l'image précédente.
 */
public final class Ui {
	/** Glisser (curseur, déplacement de fenêtre…) actif tant que le bouton est maintenu. */
	public interface Drag {
		void drag(double mx, double my);
	}

	private record Hit(int x, int y, int w, int h, int[] clip, IntPredicate click, Drag drag) {
		boolean contains(double mx, double my) {
			if (!Draw.inside(mx, my, x, y, w, h)) return false;
			return clip == null || Draw.inside(mx, my, clip[0], clip[1], clip[2], clip[3]);
		}
	}

	public GuiGraphicsExtractor g;
	public Font font;
	public Theme theme;
	public int mx, my;
	public float dt;

	private final List<Hit> hits = new ArrayList<>();
	private List<Hit> lastHits = List.of();
	private int[] clip;
	private final Map<Object, Float> anims = new HashMap<>();
	private String tooltip;
	public Drag activeDrag;
	public TextField focused;
	/** Position du clic en cours de traitement. */
	public double clickX, clickY;

	public void begin(GuiGraphicsExtractor g, Font font, Theme theme, int mx, int my, float dt) {
		this.g = g;
		this.font = font;
		this.theme = theme;
		this.mx = mx;
		this.my = my;
		this.dt = dt;
		hits.clear();
		tooltip = null;
		if (anims.size() > 4000) anims.clear();
	}

	public void end() {
		lastHits = new ArrayList<>(hits);
		if (tooltip != null && !tooltip.isEmpty()) g.setTooltipForNextFrame(font, Component.literal(tooltip), mx, my);
	}

	public void clip(int x, int y, int w, int h) {
		clip = new int[] {x, y, w, h};
		g.enableScissor(x, y, x + w, y + h);
	}

	public void unclip() {
		clip = null;
		g.disableScissor();
	}

	public boolean hovered(int x, int y, int w, int h) {
		if (!Draw.inside(mx, my, x, y, w, h)) return false;
		return clip == null || Draw.inside(mx, my, clip[0], clip[1], clip[2], clip[3]);
	}

	/** Zone cliquable ; {@code click} reçoit le bouton (0 gauche, 1 droit) et indique s'il a consommé le clic. */
	public void hit(int x, int y, int w, int h, IntPredicate click) {
		hits.add(new Hit(x, y, w, h, clip, click, null));
	}

	public void dragHit(int x, int y, int w, int h, Drag drag) {
		hits.add(new Hit(x, y, w, h, clip, b -> false, drag));
	}

	public boolean mouseClicked(double mx, double my, int button) {
		clickX = mx;
		clickY = my;
		for (int i = lastHits.size() - 1; i >= 0; i--) {
			Hit h = lastHits.get(i);
			if (!h.contains(mx, my)) continue;
			if (h.drag() != null) {
				if (button != 0) continue;
				activeDrag = h.drag();
				activeDrag.drag(mx, my);
				return true;
			}
			if (h.click().test(button)) return true;
		}
		return false;
	}

	public void tooltip(String text) {
		tooltip = text;
	}

	public float anim(Object key, float target) {
		float v = anims.getOrDefault(key, target);
		v += (target - v) * theme.lerpFactor(dt);
		if (Math.abs(v - target) < 0.002f) v = target;
		anims.put(key, v);
		return v;
	}

	// ---------------------------------------------------------------- widgets

	public void text(String s, int x, int y, int color) {
		Draw.text(g, font, s, x, y, color, false);
	}

	public boolean button(int x, int y, int w, int h, String label, boolean enabled, Runnable action, String tip) {
		boolean hov = enabled && hovered(x, y, w, h);
		float t = anim("btn" + x + ":" + y + ":" + label, hov ? 1f : 0f);
		int bg = enabled ? Draw.mix(theme.card(), Draw.withAlpha(theme.accent(), 0.85f), t * 0.6f) : Draw.withAlpha(theme.card(), 0.5f);
		Draw.round(g, x, y, w, h, 3, bg);
		String l = Draw.ellipsize(font, label, w - 4);
		Draw.text(g, font, l, x + (w - font.width(l)) / 2, y + (h - 8) / 2, enabled ? theme.text() : theme.textDim(), false);
		if (hovered(x, y, w, h) && tip != null) tooltip(tip);
		if (enabled) hit(x, y, w, h, b -> {
			if (b != 0) return false;
			action.run();
			return true;
		});
		return hov;
	}

	public void toggle(int x, int y, Object key, boolean on, Runnable action) {
		Draw.toggle(g, x, y, anim(key, on ? 1f : 0f), theme.accent());
		hit(x - 2, y - 2, 22, 14, b -> {
			if (b != 0) return false;
			action.run();
			return true;
		});
	}

	/** Curseur horizontal ; {@code setFraction} reçoit 0..1. */
	public void slider(int x, int y, int w, float fraction, DoubleConsumer setFraction) {
		fraction = Math.clamp(fraction, 0f, 1f);
		Draw.round(g, x, y, w, 4, 2, 0xFF3A3F4D);
		Draw.round(g, x, y, Math.max(4, Math.round(w * fraction)), 4, 2, theme.accent());
		int kx = x + Math.round(w * fraction) - 3;
		Draw.round(g, kx, y - 2, 7, 8, 3, 0xFFFFFFFF);
		dragHit(x - 3, y - 4, w + 6, 12, (mx, my) -> setFraction.accept(Math.clamp((mx - x) / w, 0, 1)));
	}

	public void textField(int x, int y, int w, int h, TextField field, String placeholder, Runnable onFocus) {
		boolean focus = focused == field;
		Draw.round(g, x, y, w, h, 3, focus ? 0xFF2A2F3C : theme.card());
		if (focus) Draw.rect(g, x + 2, y + h - 1, w - 4, 1, theme.accent());
		String s = field.get();
		boolean empty = s.isEmpty();
		String shown = empty && !focus ? placeholder : s;
		int maxW = w - 8;
		if (font.width(shown) > maxW) {
			while (!shown.isEmpty() && font.width(shown) > maxW) shown = shown.substring(1);
		}
		int tx = x + 4, ty = y + (h - 8) / 2;
		Draw.text(g, font, shown, tx, ty, empty && !focus ? theme.textDim() : theme.text(), false);
		if (focus && (System.currentTimeMillis() / 500) % 2 == 0) Draw.rect(g, tx + font.width(shown) + 1, ty - 1, 1, 10, theme.text());
		hit(x, y, w, h, b -> {
			if (b == 1) field.set("");
			focused = field;
			if (onFocus != null) onFocus.run();
			return true;
		});
	}
}
