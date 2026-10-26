package umc.client.modules;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.gui.Draw;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;

import java.util.List;

/** Réticule personnalisé : remplace l'élément CROSSHAIR vanilla tant que le module est actif. */
public final class CrosshairModule extends Module {
	private final Settings.Choice style = add(new Settings.Choice("style", "Forme", "Forme du réticule",
			List.of("Croix", "Point", "Croix + point", "Carré", "T"), 0));
	private final Settings.Color color = add(new Settings.Color("color", "Couleur", "Couleur du réticule", 0xFFFFFFFF));
	private final Settings.Int size = add(new Settings.Int("size", "Longueur", "Longueur des branches", 5, 1, 15, "%d px"));
	private final Settings.Int gap = add(new Settings.Int("gap", "Écart", "Espace au centre", 2, 0, 10, "%d px"));
	private final Settings.Int thickness = add(new Settings.Int("thickness", "Épaisseur", "Épaisseur des traits", 1, 1, 4, "%d px"));
	private final Settings.Bool outline = add(new Settings.Bool("outline", "Contour", "Contour sombre pour la lisibilité", true));
	private final Settings.Bool thirdPerson = add(new Settings.Bool("third_person", "En vue 3ᵉ personne", "Affiche aussi le réticule hors vue subjective", false));

	public CrosshairModule() {
		super("crosshair", "Custom Crosshair", "Réticule personnalisé (forme, couleur, taille)", Category.RENDER, false);
	}

	@Override
	public String limitation() {
		return "Images de réticule personnalisées : disponibles sur Bedrock, pas encore sur Java.";
	}

	/** Dessine le réticule au centre de l'écran ; retourne false si rien n'est dessiné. */
	public boolean draw(GuiGraphicsExtractor g) {
		Minecraft mc = Minecraft.getInstance();
		if (mc.options == null || mc.player == null) return false;
		if (!thirdPerson.on() && !mc.options.getCameraType().isFirstPerson()) return false;
		if (mc.player.isSpectator()) return false;
		drawAt(g, g.guiWidth() / 2, g.guiHeight() / 2);
		return true;
	}

	public void drawAt(GuiGraphicsExtractor g, int cx, int cy) {
		int c = color.argb(), t = thickness.get(), l = size.get(), gp = gap.get();
		int s = style.get();
		if (outline.on()) shape(g, cx, cy, s, t, l, gp, 0xAA000000, 1);
		shape(g, cx, cy, s, t, l, gp, c, 0);
	}

	private static void shape(GuiGraphicsExtractor g, int cx, int cy, int style, int t, int l, int gap, int c, int o) {
		int h0 = t / 2;
		boolean cross = style == 0 || style == 2 || style == 4;
		if (cross) {
			bar(g, cx + gap + h0 + (t % 2), cy - h0, l, t, c, o);
			bar(g, cx - gap - h0 - l, cy - h0, l, t, c, o);
			bar(g, cx - h0, cy + gap + h0 + (t % 2), t, l, c, o);
			if (style != 4) bar(g, cx - h0, cy - gap - h0 - l, t, l, c, o);
		}
		if (style == 1 || style == 2) bar(g, cx - h0, cy - h0, t, t, c, o);
		if (style == 3) {
			int half = gap + l;
			bar(g, cx - half, cy - half, 2 * half + 1, t, c, o);
			bar(g, cx - half, cy + half - t + 1, 2 * half + 1, t, c, o);
			bar(g, cx - half, cy - half, t, 2 * half + 1, c, o);
			bar(g, cx + half - t + 1, cy - half, t, 2 * half + 1, c, o);
		}
	}

	private static void bar(GuiGraphicsExtractor g, int x, int y, int w, int h, int c, int o) {
		Draw.rect(g, x - o, y - o, w + 2 * o, h + 2 * o, c);
	}
}
