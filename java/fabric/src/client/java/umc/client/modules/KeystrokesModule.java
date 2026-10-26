package umc.client.modules;

import net.minecraft.client.KeyMapping;
import net.minecraft.client.Options;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.gui.Draw;
import umc.client.hud.HudContext;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.client.telemetry.ClickTracker;
import umc.client.telemetry.Keys;
import umc.common.settings.Settings;

/** Touches de déplacement et boutons de la souris réellement appuyés (libellés selon vos raccourcis). */
public final class KeystrokesModule extends Module {
	private final Settings.Int keySize = add(new Settings.Int("key_size", "Taille des touches", "Côté d'une touche (pixels GUI)", 20, 12, 32, "%d px"));
	private final Settings.Bool showMouse = add(new Settings.Bool("show_mouse", "Boutons de souris", "Affiche clic gauche et droit", true));
	private final Settings.Bool showCps = add(new Settings.Bool("show_cps", "CPS sous la souris", "Clics par seconde dans les boutons", true));
	private final Settings.Bool showSpace = add(new Settings.Bool("show_space", "Barre d'espace", "Affiche la touche de saut", true));
	private final Settings.Color pressed = add(new Settings.Color("pressed_color", "Couleur appuyée", "Fond d'une touche appuyée", 0xCCFFFFFF));

	private final float[] anim = new float[7];
	private long lastNanos = System.nanoTime();

	public KeystrokesModule() {
		super("keystrokes", "Keystrokes", "Touches de déplacement et clics à l'écran", Category.MOVEMENT, false);
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.5f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		Options o = ctx.mc().options;
		long now = System.nanoTime();
		float dt = Math.min(0.1f, (now - lastNanos) / 1e9f);
		lastNanos = now;
		float k = ctx.theme().lerpFactor(dt);
		int s = keySize.get(), gap = 2, w = 3 * s + 2 * gap;
		int y = 0;
		key(g, ctx, 0, o.keyUp, s + gap, y, s, s, null, k);
		y += s + gap;
		key(g, ctx, 1, o.keyLeft, 0, y, s, s, null, k);
		key(g, ctx, 2, o.keyDown, s + gap, y, s, s, null, k);
		key(g, ctx, 3, o.keyRight, 2 * (s + gap), y, s, s, null, k);
		y += s + gap;
		if (showMouse.on()) {
			int half = (w - gap) / 2;
			key(g, ctx, 4, o.keyAttack, 0, y, half, s, showCps.on() ? ClickTracker.left() + " CPS" : null, k);
			key(g, ctx, 5, o.keyUse, half + gap, y, w - half - gap, s, showCps.on() ? ClickTracker.right() + " CPS" : null, k);
			y += s + gap;
		}
		if (showSpace.on()) {
			int h = Math.max(8, s / 2);
			key(g, ctx, 6, o.keyJump, 0, y, w, h, "", k);
			y += h + gap;
		}
		return new int[] {w, y - gap};
	}

	private void key(GuiGraphicsExtractor g, HudContext ctx, int index, KeyMapping mapping, int x, int y, int w, int h, String sub, float k) {
		boolean down = mapping.isDown();
		anim[index] += ((down ? 1f : 0f) - anim[index]) * k;
		int bg = Draw.mix(ctx.background() ? ctx.theme().hudBg() : 0x00000000, pressed.argb(), anim[index]);
		Draw.round(g, x, y, w, h, 2, bg);
		int fg = Draw.mix(ctx.theme().text(), 0xFF101010, anim[index]);
		String label = sub != null && sub.isEmpty() ? "" : Draw.ellipsize(ctx.font(), Keys.label(mapping), w - 2);
		boolean shadow = ctx.theme().textShadow.on() && anim[index] < 0.5f;
		if (sub == null || sub.isEmpty()) {
			if (!label.isEmpty())
				Draw.text(g, ctx.font(), label, x + (w - ctx.font().width(label)) / 2, y + (h - 8) / 2, fg, shadow);
			else Draw.rect(g, x + w / 4, y + h / 2, w / 2, 1, fg);
		} else {
			Draw.text(g, ctx.font(), label, x + (w - ctx.font().width(label)) / 2, y + 2, fg, shadow);
			g.pose().pushMatrix();
			g.pose().translate(x + w / 2f, y + h - 7);
			g.pose().scale(0.6f, 0.6f);
			Draw.text(g, ctx.font(), sub, -ctx.font().width(sub) / 2, 0, fg, false);
			g.pose().popMatrix();
		}
	}
}
