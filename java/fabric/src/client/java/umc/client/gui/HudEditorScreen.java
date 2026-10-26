package umc.client.gui;

import com.mojang.blaze3d.platform.InputConstants;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.input.KeyEvent;
import net.minecraft.client.input.MouseButtonEvent;
import net.minecraft.network.chat.Component;
import umc.client.Umc;
import umc.client.hud.HudManager;
import umc.common.settings.Setting;

/** Éditeur du HUD : glisser pour déplacer, molette pour la taille, clic droit pour réinitialiser. */
public final class HudEditorScreen extends Screen {
	private static final int SNAP = 4;

	private final Screen parent;
	private HudManager.Box dragging;
	private double offX, offY;
	private int guideX = -1, guideY = -1;

	public HudEditorScreen(Screen parent) {
		super(Component.literal("Éditeur HUD"));
		this.parent = parent;
	}

	@Override
	public boolean isPauseScreen() {
		return false;
	}

	@Override
	public void extractBackground(GuiGraphicsExtractor g, int mx, int my, float pt) {
		g.fill(0, 0, width, height, 0x40000000);
	}

	private HudManager.Box boxAt(double mx, double my) {
		var boxes = Umc.hud.boxes();
		for (int i = boxes.size() - 1; i >= 0; i--) {
			HudManager.Box b = boxes.get(i);
			if (Draw.inside(mx, my, b.x() - 1, b.y() - 1, Math.max(b.w(), 8) + 2, Math.max(b.h(), 8) + 2)) return b;
		}
		return null;
	}

	@Override
	public void extractRenderState(GuiGraphicsExtractor g, int mx, int my, float pt) {
		Theme t = Umc.theme;
		int cx = width / 2, cy = height / 2;
		Draw.rect(g, cx, 0, 1, height, 0x22FFFFFF);
		Draw.rect(g, 0, cy, width, 1, 0x22FFFFFF);
		if (guideX >= 0) Draw.rect(g, guideX, 0, 1, height, Draw.withAlpha(t.accent(), 0.8f));
		if (guideY >= 0) Draw.rect(g, 0, guideY, width, 1, Draw.withAlpha(t.accent(), 0.8f));
		Umc.hud.render(g, true);
		HudManager.Box hover = dragging != null ? dragging : boxAt(mx, my);
		for (HudManager.Box b : Umc.hud.boxes()) {
			boolean hov = hover != null && hover.module() == b.module();
			Draw.border(g, b.x() - 1, b.y() - 1, Math.max(b.w(), 8) + 2, Math.max(b.h(), 8) + 2,
					hov ? t.accent() : 0x66FFFFFF);
		}
		if (hover != null) {
			String label = hover.module().name() + "  ×" + String.format("%.2f", Umc.hud.scaleOf(hover.module()));
			int lx = Math.clamp(hover.x(), 2, Math.max(2, width - font.width(label) - 2));
			int ly = hover.y() > 12 ? hover.y() - 11 : hover.y() + hover.h() + 3;
			Draw.round(g, lx - 2, ly - 2, font.width(label) + 4, 11, 2, 0xCC000000);
			Draw.text(g, font, label, lx, ly, t.text(), false);
		}
		String help = "Glisser : déplacer · Molette : taille · Clic droit : position par défaut · Échap : retour";
		int hw = font.width(help) + 10;
		Draw.round(g, (width - hw) / 2, height - 22, hw, 14, 3, 0xCC000000);
		Draw.text(g, font, help, (width - hw) / 2 + 5, height - 19, t.textDim(), false);
		if (Umc.hud.boxes().isEmpty()) {
			String none = "Aucun élément HUD actif : activez des modules dans le menu.";
			Draw.text(g, font, none, (width - font.width(none)) / 2, cy - 20, t.text(), true);
		}
	}

	@Override
	public boolean mouseClicked(MouseButtonEvent e, boolean doubleClick) {
		HudManager.Box b = boxAt(e.x(), e.y());
		if (b == null) return true;
		if (e.button() == 1) {
			Umc.hud.resetLayout(b.module().id());
			return true;
		}
		if (e.button() == 0) {
			dragging = b;
			offX = e.x() - b.x();
			offY = e.y() - b.y();
		}
		return true;
	}

	@Override
	public boolean mouseDragged(MouseButtonEvent e, double dx, double dy) {
		if (dragging == null) return true;
		int w = dragging.w(), h = dragging.h();
		int x = (int) Math.round(e.x() - offX), y = (int) Math.round(e.y() - offY);
		guideX = guideY = -1;
		if (Umc.hud.snap.on()) {
			if (Math.abs(x) <= SNAP) x = 0;
			if (Math.abs(x + w - width) <= SNAP) x = width - w;
			if (Math.abs(y) <= SNAP) y = 0;
			if (Math.abs(y + h - height) <= SNAP) y = height - h;
			if (Math.abs(x + w / 2 - width / 2) <= SNAP) {
				x = width / 2 - w / 2;
				guideX = width / 2;
			}
			if (Math.abs(y + h / 2 - height / 2) <= SNAP) {
				y = height / 2 - h / 2;
				guideY = height / 2;
			}
		}
		x = Math.clamp(x, 0, Math.max(0, width - w));
		y = Math.clamp(y, 0, Math.max(0, height - h));
		HudManager.Layout l = Umc.hud.layout(dragging.module().id());
		l.x = x / (float) width;
		l.y = y / (float) height;
		Setting.bumpRevision();
		return true;
	}

	@Override
	public boolean mouseReleased(MouseButtonEvent e) {
		dragging = null;
		guideX = guideY = -1;
		return true;
	}

	@Override
	public boolean mouseScrolled(double mx, double my, double sx, double sy) {
		HudManager.Box b = boxAt(mx, my);
		if (b == null || sy == 0) return true;
		HudManager.Layout l = Umc.hud.layout(b.module().id());
		if (l.x < 0) {
			l.x = b.x() / (float) width;
			l.y = b.y() / (float) height;
		}
		l.scale = Math.clamp(Math.round((l.scale + (sy > 0 ? 0.1f : -0.1f)) * 10f) / 10f, 0.5f, 3f);
		Setting.bumpRevision();
		return true;
	}

	@Override
	public boolean keyPressed(KeyEvent e) {
		if (e.key() == InputConstants.KEY_ESCAPE) {
			onClose();
			return true;
		}
		return super.keyPressed(e);
	}

	@Override
	public void onClose() {
		minecraft.gui.setScreen(parent);
	}
}
