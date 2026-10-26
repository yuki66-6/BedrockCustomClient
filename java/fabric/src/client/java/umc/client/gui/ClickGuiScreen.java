package umc.client.gui;

import com.google.gson.JsonElement;
import com.mojang.blaze3d.platform.InputConstants;
import net.fabricmc.fabric.api.client.keymapping.v1.KeyMappingHelper;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.input.CharacterEvent;
import net.minecraft.client.input.KeyEvent;
import net.minecraft.client.input.MouseButtonEvent;
import net.minecraft.network.chat.Component;
import net.minecraft.server.permissions.Permissions;
import net.minecraft.util.Util;
import umc.client.Umc;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.client.telemetry.ClickTracker;
import umc.client.telemetry.Keys;
import umc.common.data.Availability;
import umc.common.data.CapabilityInfo;
import umc.common.modules.ModuleDefinitions;
import umc.common.profile.BuiltinProfiles;
import umc.common.profile.OpResult;
import umc.common.profile.ProfileStore;
import umc.common.settings.Setting;
import umc.common.settings.Settings;
import umc.common.waypoint.Waypoint;

import java.nio.file.Path;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** ClickGUI du mod : modules par catégorie, recherche, favoris, réglages, waypoints, profils, capacités. */
public final class ClickGuiScreen extends Screen {
	private enum Page {
		MODULES("Modules"), WAYPOINTS("Waypoints"), PROFILES("Profils"), CAPABILITIES("Capacités"), SETTINGS("Réglages");

		final String label;

		Page(String label) {
			this.label = label;
		}
	}

	private record Builtin(String file, String name, String description) {
	}

	private static final int SIDEBAR = 98, HEADER = 26, FOOTER = 14, PAD = 8;
	private static final DateTimeFormatter STAMP = DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss");
	private static List<Builtin> builtins;

	private static Page lastPage = Page.MODULES;
	private static Category lastCategory;
	private static boolean lastFavorites;

	private Page page;
	private Category category;
	private boolean favorites;
	private Module detail;
	private Settings.Color colorOpen;
	private Module capturing;

	private final Ui ui = new Ui();
	private final TextField search = new TextField(40);
	private final TextField wpName = new TextField(32);
	private final TextField profileName = new TextField(32);
	private final Map<Settings.Text, TextField> textFields = new HashMap<>();
	private int wpCategory;

	private float scroll, scrollTarget;
	private int contentH, viewH;
	private float open;
	private long lastNanos;
	private int wx = Integer.MIN_VALUE, wy, ww, wh;
	private String confirmKey = "";
	private double confirmUntil;
	private List<String> profileList = List.of();
	private double profileListTime = -10;

	public ClickGuiScreen() {
		super(Component.literal("Universal Minecraft Client"));
		boolean remember = Umc.gui.rememberPage.on();
		page = remember ? lastPage : Page.MODULES;
		category = remember ? lastCategory : null;
		favorites = remember && lastFavorites;
		search.onChange(s -> scrollTarget = 0);
		wpName.onSubmit(s -> addWaypoint());
		profileName.onSubmit(s -> createProfile());
	}

	@Override
	public boolean isPauseScreen() {
		return Umc.gui.pauseGame.on();
	}

	@Override
	public void extractBackground(GuiGraphicsExtractor g, int mx, int my, float pt) {
		if (Umc.theme.dimBackground.on()) g.fill(0, 0, width, height, Draw.withAlpha(0x99000000, open));
	}

	private static float ease(float x) {
		float i = 1 - x;
		return 1 - i * i * i;
	}

	@Override
	public void extractRenderState(GuiGraphicsExtractor g, int mx, int my, float pt) {
		long now = System.nanoTime();
		float dt = lastNanos == 0 ? 0 : Math.min(0.1f, (now - lastNanos) / 1e9f);
		lastNanos = now;
		Theme t = Umc.theme;
		open = t.animations.on() ? Math.min(1f, open + dt * t.animSpeed.value() * 0.5f) : 1f;
		scroll += (scrollTarget - scroll) * t.lerpFactor(dt);
		layoutWindow();
		ui.begin(g, font, t, mx, my, dt);
		float s = 0.92f + 0.08f * ease(open);
		g.pose().pushMatrix();
		g.pose().translate(wx + ww / 2f, wy + wh / 2f);
		g.pose().scale(s, s);
		g.pose().translate(-(wx + ww / 2f), -(wy + wh / 2f));
		drawWindow(g);
		g.pose().popMatrix();
		ui.end();
	}

	private void layoutWindow() {
		ww = Math.min(Umc.gui.width.get(), width - 16);
		wh = Math.min(Umc.gui.height.get(), height - 16);
		if (wx == Integer.MIN_VALUE) {
			wx = (width - ww) / 2;
			wy = (height - wh) / 2;
		}
		wx = Math.clamp(wx, 0, Math.max(0, width - ww));
		wy = Math.clamp(wy, 0, Math.max(0, height - wh));
	}

	// ---------------------------------------------------------------- cadre

	private void drawWindow(GuiGraphicsExtractor g) {
		Theme t = Umc.theme;
		Draw.round(g, wx - 2, wy, ww + 4, wh + 3, 6, 0x50000000);
		Draw.round(g, wx, wy, ww, wh, 5, t.panel());
		Draw.round(g, wx, wy, SIDEBAR, wh, 5, t.sidebar());
		Draw.rect(g, wx + SIDEBAR - 5, wy, 5, wh, t.sidebar());
		Draw.rect(g, wx + SIDEBAR, wy, 1, wh, t.border());

		// Déplacement par l'en-tête : enregistré en premier, les widgets passent au-dessus.
		ui.dragHit(wx, wy, ww, HEADER, new Ui.Drag() {
			double ox = Double.NaN, oy;

			@Override
			public void drag(double mx, double my) {
				if (Double.isNaN(ox)) {
					ox = mx - wx;
					oy = my - wy;
				}
				wx = (int) (mx - ox);
				wy = (int) (my - oy);
			}
		});
		sidebar(g);
		header(g);

		int cx = wx + SIDEBAR + PAD, cy = wy + HEADER, cw = ww - SIDEBAR - 2 * PAD;
		viewH = wh - HEADER - FOOTER;
		ui.clip(wx + SIDEBAR + 1, cy, ww - SIDEBAR - 1, viewH);
		int y0 = cy + 4 - Math.round(scroll);
		int end = switch (page) {
			case MODULES -> modulesPage(g, cx, y0, cw);
			case WAYPOINTS -> waypointsPage(g, cx, y0, cw);
			case PROFILES -> profilesPage(g, cx, y0, cw);
			case CAPABILITIES -> capabilitiesPage(g, cx, y0, cw);
			case SETTINGS -> settingsPage(g, cx, y0, cw);
		};
		ui.unclip();
		contentH = end - y0 + 6;
		scrollTarget = Math.clamp(scrollTarget, 0, Math.max(0, contentH - viewH));
		if (contentH > viewH) {
			int barH = Math.max(16, viewH * viewH / contentH);
			int barY = cy + Math.round((viewH - barH) * (scroll / Math.max(1, contentH - viewH)));
			Draw.round(g, wx + ww - 4, barY, 2, barH, 1, 0x55FFFFFF);
		}
		footer(g);

		// Poignée de redimensionnement.
		int hx = wx + ww - 9, hy = wy + wh - 9;
		for (int i = 0; i < 3; i++) Draw.rect(g, hx + 6 - 2 * i, hy + 7, 1 + 2 * i, 1, 0x66FFFFFF);
		ui.dragHit(hx - 2, hy - 2, 11, 11, new Ui.Drag() {
			double sx = Double.NaN, sy;
			int sw, sh;

			@Override
			public void drag(double mx, double my) {
				if (Double.isNaN(sx)) {
					sx = mx;
					sy = my;
					sw = ww;
					sh = wh;
				}
				Umc.gui.width.set((int) (sw + mx - sx));
				Umc.gui.height.set((int) (sh + my - sy));
			}
		});
	}

	private void sidebar(GuiGraphicsExtractor g) {
		Theme t = Umc.theme;
		int x = wx + 9, y = wy + 9;
		ui.text("UMC", x, y, t.accent());
		ui.text("Java", x + font.width("UMC "), y, t.textDim());
		y += 17;
		y = navItem(g, "Tous les modules", y, page == Page.MODULES && category == null && !favorites, () -> openModules(null, false));
		for (Category c : Category.values())
			y = navItem(g, c.label, y, page == Page.MODULES && category == c && !favorites, () -> openModules(c, false));
		y = navItem(g, "★ Favoris", y, page == Page.MODULES && favorites, () -> openModules(null, true));
		y += 5;
		for (Page p : new Page[] {Page.WAYPOINTS, Page.PROFILES, Page.CAPABILITIES, Page.SETTINGS})
			y = navItem(g, p.label, y, page == p, () -> openPage(p));
		if (y + 22 <= wy + wh)
			ui.button(wx + 6, wy + wh - 20, SIDEBAR - 12, 14, "Éditer le HUD", true, this::openHudEditor, "Déplacer et redimensionner les éléments HUD");
	}

	private int navItem(GuiGraphicsExtractor g, String label, int y, boolean selected, Runnable action) {
		Theme t = Umc.theme;
		int x = wx + 4, w = SIDEBAR - 8, h = 13;
		boolean hov = ui.hovered(x, y, w, h);
		float a = ui.anim("nav" + label, selected ? 1f : hov ? 0.45f : 0f);
		if (a > 0) Draw.round(g, x, y, w, h, 3, Draw.withAlpha(t.accent(), 0.22f * a));
		if (selected) Draw.rect(g, x, y + 3, 2, h - 6, t.accent());
		ui.text(label, x + 7, y + 3, selected ? t.text() : Draw.mix(t.textDim(), t.text(), a));
		ui.hit(x, y, w, h, b -> {
			if (b != 0) return false;
			action.run();
			return true;
		});
		return y + h + 1;
	}

	private void header(GuiGraphicsExtractor g) {
		Theme t = Umc.theme;
		String title = page != Page.MODULES ? page.label
				: detail != null ? detail.name() : favorites ? "Favoris" : category == null ? "Tous les modules" : category.label;
		ui.text(title, wx + SIDEBAR + PAD, wy + 9, t.text());
		if (page == Page.MODULES && detail == null) {
			int fw = Math.min(140, (ww - SIDEBAR) / 2);
			ui.textField(wx + ww - PAD - fw, wy + 5, fw, 15, search, "Rechercher…", null);
		}
		Draw.rect(g, wx + SIDEBAR + 1, wy + HEADER - 2, ww - SIDEBAR - 1, 1, t.border());
	}

	private void footer(GuiGraphicsExtractor g) {
		Theme t = Umc.theme;
		int fy = wy + wh - FOOTER;
		Draw.rect(g, wx + SIDEBAR + 1, fy, ww - SIDEBAR - 1, 1, t.border());
		String msg = Umc.statusMessage();
		int maxW = ww - SIDEBAR - 2 * PAD - 12;
		if (!msg.isEmpty()) ui.text(Draw.ellipsize(font, msg, maxW), wx + SIDEBAR + PAD, fy + 4, Umc.statusOk() ? t.good() : t.bad());
		else {
			String s = "Profil « " + Umc.profiles.activeProfile() + " »" + (Umc.profiles.dirty() ? " • modifié" : "") + " • "
					+ Keys.label(Umc.openGuiKey) + " ou Échap pour fermer";
			ui.text(Draw.ellipsize(font, s, maxW), wx + SIDEBAR + PAD, fy + 4, t.textDim());
		}
	}

	private void openModules(Category c, boolean fav) {
		page = Page.MODULES;
		category = c;
		favorites = fav;
		detail = null;
		colorOpen = null;
		scroll = scrollTarget = 0;
	}

	private void openPage(Page p) {
		page = p;
		detail = null;
		colorOpen = null;
		scroll = scrollTarget = 0;
	}

	private void openDetail(Module m) {
		detail = m;
		colorOpen = null;
		scroll = scrollTarget = 0;
	}

	private void openHudEditor() {
		minecraft.gui.setScreen(new HudEditorScreen(this));
	}

	// ---------------------------------------------------------------- utilitaires

	private boolean confirming(String key) {
		return key.equals(confirmKey) && ClickTracker.now() < confirmUntil;
	}

	/** Deux clics en moins de 3 s pour les actions destructrices. */
	private boolean confirm(String key) {
		if (confirming(key)) {
			confirmKey = "";
			return true;
		}
		confirmKey = key;
		confirmUntil = ClickTracker.now() + 3;
		Umc.status("Cliquez une seconde fois pour confirmer", true);
		return false;
	}

	/** Texte sur plusieurs lignes ; retourne le y suivant. */
	private int wrap(String s, int x, int y, int w, int color) {
		StringBuilder line = new StringBuilder();
		for (String word : s.split(" ")) {
			String candidate = line.isEmpty() ? word : line + " " + word;
			if (font.width(candidate) > w && !line.isEmpty()) {
				ui.text(line.toString(), x, y, color);
				y += 10;
				line.setLength(0);
				line.append(word);
			} else {
				line.setLength(0);
				line.append(candidate);
			}
		}
		if (!line.isEmpty()) {
			ui.text(line.toString(), x, y, color);
			y += 10;
		}
		return y;
	}

	private int section(String title, int x, int y, int w) {
		y += 4;
		ui.text(title, x, y, Umc.theme.accent());
		Draw.rect(ui.g, x + font.width(title) + 6, y + 4, w - font.width(title) - 6, 1, Umc.theme.border());
		return y + 13;
	}

	private int row(GuiGraphicsExtractor g, int x, int y, int w, int h) {
		Draw.round(g, x, y, w, h, 3, ui.hovered(x, y, w, h) ? Umc.theme.cardHover() : Umc.theme.card());
		return y;
	}

	private static String statusLabel(String status) {
		return switch (status) {
			case "tested" -> "testé";
			case "implemented" -> "implémenté";
			case "partial" -> "partiel";
			case "unsupported" -> "non pris en charge";
			default -> "prévu";
		};
	}

	private static String dimLabel(String d) {
		return switch (d) {
			case "overworld" -> "Overworld";
			case "nether" -> "Nether";
			case "end" -> "End";
			default -> d;
		};
	}

	// ---------------------------------------------------------------- modules

	private int modulesPage(GuiGraphicsExtractor g, int x, int y, int w) {
		if (detail != null) return moduleDetail(g, x, y, w);
		Theme t = Umc.theme;
		String q = search.get().trim().toLowerCase(Locale.ROOT);
		List<Module> list = new ArrayList<>();
		for (Module m : Umc.modules.all()) {
			if (favorites && !m.favorite()) continue;
			if (category != null && m.category() != category) continue;
			if (!q.isEmpty() && !m.name().toLowerCase(Locale.ROOT).contains(q) && !m.description().toLowerCase(Locale.ROOT).contains(q)
					&& !m.id().contains(q))
				continue;
			list.add(m);
		}
		int cols = w >= 300 ? 2 : 1, gap = 6, cardW = (w - (cols - 1) * gap) / cols, cardH = 34;
		for (int i = 0; i < list.size(); i++) card(g, list.get(i), x + (i % cols) * (cardW + gap), y + (i / cols) * (cardH + gap), cardW, cardH);
		y += ((list.size() + cols - 1) / cols) * (cardH + gap);
		if (list.isEmpty()) {
			ui.text(favorites ? "Aucun favori : cliquez sur ☆ dans la carte d'un module." : "Aucun module ne correspond.", x, y + 2, t.textDim());
			y += 16;
		}
		// Modules prévus : affichés comme tels, jamais activables.
		if (!favorites) {
			List<String> planned = new ArrayList<>();
			for (ModuleDefinitions.Definition d : Umc.definitions.all()) {
				if (Umc.modules.find(d.id()) != null || d.java() == null || !d.java().status().equals("planned")) continue;
				if (category != null && !d.category().equals(category.id)) continue;
				if (!q.isEmpty() && !d.name().toLowerCase(Locale.ROOT).contains(q)) continue;
				planned.add(d.name());
			}
			if (!planned.isEmpty()) {
				y += 2;
				y = wrap("Prévu, pas encore disponible sur Java : " + String.join(", ", planned) + ".", x, y, w, t.textDim());
			}
		}
		return y;
	}

	private void card(GuiGraphicsExtractor g, Module m, int x, int y, int w, int h) {
		Theme t = Umc.theme;
		boolean hov = ui.hovered(x, y, w, h);
		float hv = ui.anim("card" + m.id(), hov ? 1f : 0f);
		float on = ui.anim("on" + m.id(), m.enabled() ? 1f : 0f);
		Draw.round(g, x, y, w, h, 4, Draw.mix(t.card(), t.cardHover(), hv));
		if (on > 0.01f) Draw.round(g, x, y + 6, 2, h - 12, 1, Draw.withAlpha(t.accent(), on));
		ui.hit(x, y, w, h, b -> {
			if (b == 0) openDetail(m);
			else if (b == 1) m.toggle();
			else return false;
			return true;
		});
		String name = Draw.ellipsize(font, m.name(), w - 62);
		ui.text(name, x + 8, y + 7, t.text());
		if (m.keybind() != Module.NO_KEY) ui.text("[" + Keys.glfwName(m.keybind()) + "]", x + 12 + font.width(name), y + 7, t.textDim());
		ui.text(Draw.ellipsize(font, m.description(), w - 38), x + 8, y + 20, t.textDim());
		if (hov) ui.tooltip("Clic : réglages · clic droit : activer / désactiver");
		if (!m.lastError().isEmpty()) {
			ui.text("!", x + w - 50, y + 7, t.bad());
			if (ui.hovered(x + w - 52, y + 5, 8, 12)) ui.tooltip(m.lastError());
		}
		int sx = x + w - 40, sy = y + 6;
		ui.text(m.favorite() ? "★" : "☆", sx, sy, m.favorite() ? t.accent() : t.textDim());
		ui.hit(sx - 2, sy - 2, 12, 12, b -> {
			if (b != 0) return false;
			m.setFavorite(!m.favorite());
			return true;
		});
		ui.toggle(x + w - 26, y + (h - 10) / 2, "tg" + m.id(), m.enabled(), m::toggle);
	}

	private int moduleDetail(GuiGraphicsExtractor g, int x, int y, int w) {
		Theme t = Umc.theme;
		Module m = detail;
		ui.button(x, y, 56, 14, "← Retour", true, () -> openDetail(null), null);
		String state = m.enabled() ? "Activé" : "Désactivé";
		ui.text(state, x + w - 28 - font.width(state), y + 3, m.enabled() ? t.good() : t.textDim());
		ui.toggle(x + w - 22, y + 2, "tg" + m.id(), m.enabled(), m::toggle);
		y += 20;
		y = wrap(m.description(), x, y, w, t.text());
		ModuleDefinitions.Definition d = Umc.definitions.find(m.id());
		if (d != null && d.java() != null) {
			String note = d.java().note();
			y = wrap("Statut Java : " + statusLabel(d.java().status()) + (note == null || note.isEmpty() ? "" : " — " + note), x, y, w, t.textDim());
		}
		if (!m.limitation().isEmpty()) y = wrap("Limite : " + m.limitation(), x, y, w, t.warn());
		if (!m.lastError().isEmpty()) y = wrap("Erreur : " + m.lastError(), x, y, w, t.bad());
		y += 4;
		for (Setting<?> s : m.settings())
			if (s.visible()) y = settingRow(g, s, x, y, w);
		y = keybindRow(g, m, x, y, w);
		y += 4;
		String key = "reset:" + m.id();
		ui.button(x, y, 150, 14, confirming(key) ? "Confirmer la réinitialisation" : "Réinitialiser le module", true, () -> {
			if (confirm(key)) {
				m.resetToDefaults();
				Setting.bumpRevision();
				Umc.status("« " + m.name() + " » réinitialisé", true);
			}
		}, "Valeurs par défaut et raccourci supprimé");
		return y + 18;
	}

	private int keybindRow(GuiGraphicsExtractor g, Module m, int x, int y, int w) {
		Theme t = Umc.theme;
		row(g, x, y, w, 16);
		ui.text("Raccourci", x + 6, y + 4, t.text());
		boolean cap = capturing == m;
		String label = cap ? "Appuyez sur une touche…" : Keys.glfwName(m.keybind());
		int bw = Math.max(70, font.width(label) + 12);
		ui.button(x + w - bw - 2, y + 1, bw, 14, label, true, () -> capturing = cap ? null : m,
				"Clic : choisir · Échap : annuler · Retour arrière : aucun · clic droit : effacer");
		ui.hit(x + w - bw - 2, y + 1, bw, 14, b -> {
			if (b != 1) return false;
			m.setKeybind(Module.NO_KEY);
			capturing = null;
			return true;
		});
		return y + 18;
	}

	private TextField fieldFor(Settings.Text s) {
		TextField f = textFields.computeIfAbsent(s, k -> new TextField(k.maxLength()));
		if (ui.focused != f) {
			f.onChange(v -> {});
			f.set(s.get());
			f.onChange(s::set);
		}
		return f;
	}

	private int settingRow(GuiGraphicsExtractor g, Setting<?> s, int x, int y, int w) {
		Theme t = Umc.theme;
		boolean slider = s instanceof Settings.Int || s instanceof Settings.Float;
		int h = slider ? 24 : 18;
		row(g, x, y, w, h - 2);
		if (ui.hovered(x, y, w, h - 2) && !s.description().isEmpty()) ui.tooltip(s.description());
		ui.text(Draw.ellipsize(font, s.name(), w / 2), x + 6, y + 5, t.text());
		switch (s) {
			case Settings.Bool b -> {
				ui.hit(x, y, w, h - 2, btn -> {
					if (btn != 0) return false;
					b.set(!b.on());
					return true;
				});
				ui.toggle(x + w - 24, y + 4, b, b.on(), () -> b.set(!b.on()));
			}
			case Settings.Int i -> {
				String v = i.formatted();
				ui.text(v, x + w - 6 - font.width(v), y + 5, t.textDim());
				ui.slider(x + 6, y + 16, w - 12, (i.get() - i.min()) / (float) Math.max(1, i.max() - i.min()),
						f -> i.set((int) Math.round(i.min() + f * (i.max() - i.min()))));
			}
			case Settings.Float f -> {
				String v = f.formatted();
				ui.text(v, x + w - 6 - font.width(v), y + 5, t.textDim());
				ui.slider(x + 6, y + 16, w - 12, (f.value() - f.min()) / Math.max(1e-6f, f.max() - f.min()),
						fr -> f.set((float) (f.min() + fr * (f.max() - f.min()))));
			}
			case Settings.Choice c -> {
				int bw = Math.min(140, w / 2), bx = x + w - bw - 3;
				Draw.round(g, bx, y + 2, bw, 12, 3, 0xFF2A2F3C);
				String cur = Draw.ellipsize(font, c.current(), bw - 22);
				ui.text("‹", bx + 4, y + 4, t.accent());
				ui.text("›", bx + bw - 8, y + 4, t.accent());
				ui.text(cur, bx + (bw - font.width(cur)) / 2, y + 4, t.text());
				ui.hit(bx, y + 2, bw, 12, b -> {
					if (b == 1) c.cycle(-1);
					else if (b == 0) c.cycle(ui.clickX < bx + bw / 2.0 ? -1 : 1);
					else return false;
					return true;
				});
			}
			case Settings.Color c -> {
				int sx = x + w - 30;
				Draw.rect(g, sx, y + 3, 13, 10, 0xFFFFFFFF);
				Draw.rect(g, sx + 13, y + 3, 13, 10, 0xFF808080);
				Draw.rect(g, sx, y + 3, 26, 10, c.argb());
				Draw.border(g, sx, y + 3, 26, 10, t.border());
				ui.hit(x, y, w, h - 2, b -> {
					if (b != 0) return false;
					colorOpen = colorOpen == c ? null : c;
					return true;
				});
			}
			case Settings.Text tx -> {
				int bw = Math.min(150, w / 2);
				ui.textField(x + w - bw - 3, y + 2, bw, 12, fieldFor(tx), "", null);
			}
			default -> {
			}
		}
		y += h;
		if (s == colorOpen) y = colorEditor(g, colorOpen, x, y, w);
		return y;
	}

	private int colorEditor(GuiGraphicsExtractor g, Settings.Color c, int x, int y, int w) {
		Theme t = Umc.theme;
		String[] names = {"Rouge", "Vert", "Bleu", "Opacité"};
		int[] shifts = {16, 8, 0, 24};
		for (int k = 0; k < 4; k++) {
			int sh = shifts[k];
			int val = (c.argb() >>> sh) & 0xff;
			ui.text(names[k], x + 10, y + 1, t.textDim());
			ui.text(String.valueOf(val), x + w - 24, y + 1, t.textDim());
			ui.slider(x + 64, y + 3, w - 100, val / 255f, f -> {
				int nv = (int) Math.round(f * 255);
				c.set((c.argb() & ~(0xff << sh)) | (nv << sh));
			});
			y += 12;
		}
		int[] presets = {0xFFFFFFFF, 0xFF000000, 0xFF458AFF, 0xFF4DDB85, 0xFFF2525A, 0xFFFFB84D, 0xFFB46CFF, 0xFF33E0E0};
		int px = x + 10;
		for (int p : presets) {
			Draw.round(g, px, y, 12, 10, 2, p);
			ui.hit(px, y, 12, 10, b -> {
				if (b != 0) return false;
				c.set((p & 0xFFFFFF) | (c.argb() & 0xFF000000));
				return true;
			});
			px += 15;
		}
		ui.text(String.format("#%08X", c.argb()), px + 6, y + 1, t.textDim());
		return y + 16;
	}

	// ---------------------------------------------------------------- waypoints

	private void addWaypoint() {
		var cats = Umc.waypoints.categories();
		var cat = cats.get(Math.floorMod(wpCategory, cats.size()));
		String name = wpName.get().trim();
		if (name.isEmpty()) name = "Waypoint " + (Umc.waypoints.forWorld(Umc.adapter.worldKey().orElse("")).size() + 1);
		OpResult r = Umc.addWaypointHere(name, cat.id(), cat.color());
		Umc.status(r);
		if (r.ok()) wpName.set("");
	}

	private void teleport(Waypoint wp) {
		var conn = minecraft.getConnection();
		if (conn == null) return;
		// Commande normale, vérifiée par le serveur : rien n'est forcé côté client.
		conn.sendCommand("tp @s " + wp.x + ".5 " + wp.y + " " + wp.z + ".5");
		Umc.status("Commande /tp envoyée vers « " + wp.name + " » (le serveur l'accepte ou la refuse)", true);
	}

	private int waypointsPage(GuiGraphicsExtractor g, int x, int y, int w) {
		Theme t = Umc.theme;
		var p = minecraft.player;
		String key = Umc.adapter.worldKey().orElse(null);
		if (p == null || key == null) {
			y = wrap("Rejoignez un monde pour gérer ses waypoints. " + Umc.waypoints.worlds().size() + " monde(s) enregistré(s) dans "
					+ Umc.waypoints.file(), x, y, w, t.textDim());
			return exportRow(x, y + 4);
		}
		String dim = Umc.adapter.dimension().orElse("overworld");
		ui.text(Draw.ellipsize(font, "Monde : " + Umc.adapter.worldName().orElse("?") + "  (" + dimLabel(dim) + ")", w), x, y, t.text());
		y += 13;
		var cats = Umc.waypoints.categories();
		var cat = cats.get(Math.floorMod(wpCategory, cats.size()));
		ui.textField(x, y, w - 172, 14, wpName, "Nom du waypoint (Entrée)", null);
		ui.button(x + w - 168, y, 78, 14, cat.name(), true, () -> wpCategory++, "Catégorie du nouveau waypoint (clic pour changer)");
		ui.button(x + w - 86, y, 86, 14, "Ajouter ici", true, this::addWaypoint,
				"Position actuelle : " + p.getBlockX() + " " + p.getBlockY() + " " + p.getBlockZ());
		y += 20;
		boolean canTp = p.permissions().hasPermission(Permissions.COMMANDS_GAMEMASTER);
		List<Waypoint> list = new ArrayList<>(Umc.waypoints.forWorld(key));
		list.sort(Comparator.comparing((Waypoint wp) -> !wp.dimension.equals(dim)).thenComparingDouble(wp -> wp.distanceTo(p.getX(), p.getY(), p.getZ())));
		if (list.isEmpty()) {
			y = wrap("Aucun waypoint dans ce monde. Touche « " + Keys.label(Umc.addWaypointKey) + " » : ajout rapide à votre position.", x, y, w, t.textDim());
		}
		for (Waypoint wp : list) {
			boolean same = wp.dimension.equals(dim);
			row(g, x, y, w, 18);
			Draw.round(g, x + 5, y + 5, 8, 8, 4, wp.argb());
			int nameW = Math.min(w / 3, font.width(wp.name));
			ui.text(Draw.ellipsize(font, wp.name, w / 3), x + 18, y + 5, wp.visible ? t.text() : t.textDim());
			String info = wp.x + " " + wp.y + " " + wp.z + " · " + dimLabel(wp.dimension)
					+ (same ? String.format(" · %.0f m", wp.distanceTo(p.getX(), p.getY(), p.getZ())) : "");
			int infoX = x + 24 + nameW, buttons = 40 + 32 + 46 + 10;
			ui.text(Draw.ellipsize(font, info, Math.max(0, w - (infoX - x) - buttons)), infoX, y + 5, t.textDim());
			int bx = x + w - 42;
			String dk = "wpdel:" + wp.id;
			ui.button(bx, y + 2, 40, 14, confirming(dk) ? "Sûr ?" : "Suppr.", true, () -> {
				if (!confirm(dk)) return;
				Umc.waypoints.remove(key, wp.id);
				OpResult r = Umc.waypoints.save();
				Umc.status(r.ok() ? OpResult.ok("Waypoint « " + wp.name + " » supprimé") : r);
			}, "Supprimer (cliquer deux fois)");
			bx -= 32;
			ui.button(bx, y + 2, 30, 14, "/tp", canTp && same, () -> teleport(wp), !same ? "Waypoint dans une autre dimension"
					: canTp ? "Envoie « /tp » au serveur (commande autorisée pour vous)" : "Commande /tp non autorisée ici (triche désactivée ou pas opérateur)");
			bx -= 46;
			ui.button(bx, y + 2, 44, 14, wp.visible ? "Visible" : "Masqué", true, () -> {
				wp.visible = !wp.visible;
				Umc.waypoints.save();
			}, "Afficher dans la liste HUD");
			y += 20;
		}
		return exportRow(x, y + 4);
	}

	private int exportRow(int x, int y) {
		ui.button(x, y, 80, 14, "Exporter", true, () -> {
			Path target = Umc.configDir.resolve("exports").resolve("umc-waypoints-" + LocalDateTime.now().format(STAMP) + ".json");
			Umc.status(Umc.waypoints.exportTo(target));
		}, "Copie de tous les waypoints dans config/umc/exports");
		ui.text("Import : glissez un fichier .json sur cette fenêtre", x + 86, y + 3, Umc.theme.textDim());
		return y + 18;
	}

	// ---------------------------------------------------------------- profils

	private void createProfile() {
		OpResult r = Umc.profiles.create(profileName.get().trim());
		Umc.status(r);
		if (r.ok()) profileName.set("");
		profileListTime = -10;
	}

	private static List<Builtin> builtins() {
		if (builtins == null) {
			List<Builtin> out = new ArrayList<>();
			for (String f : BuiltinProfiles.FILES) out.add(new Builtin(f, BuiltinProfiles.displayName(f), BuiltinProfiles.description(f)));
			builtins = out;
		}
		return builtins;
	}

	private int profilesPage(GuiGraphicsExtractor g, int x, int y, int w) {
		Theme t = Umc.theme;
		ProfileStore ps = Umc.profiles;
		if (ClickTracker.now() - profileListTime > 1) {
			profileList = ps.listProfiles();
			profileListTime = ClickTracker.now();
		}
		ui.text(Draw.ellipsize(font, "Profil actif : " + ps.activeProfile() + (ps.dirty() ? " (modifications non enregistrées)" : ""), w), x, y, t.text());
		y += 13;
		ui.textField(x, y, w - 196, 14, profileName, "Nom du profil (Entrée = créer)", null);
		ui.button(x + w - 192, y, 58, 14, "Créer", true, this::createProfile, "Nouveau profil avec les valeurs par défaut");
		ui.button(x + w - 130, y, 74, 14, "Dupliquer", true, () -> {
			OpResult r = ps.duplicate(profileName.get().trim());
			Umc.status(r);
			if (r.ok()) profileName.set("");
			profileListTime = -10;
		}, "Copie du profil actif sous le nom saisi");
		ui.button(x + w - 52, y, 52, 14, "Sauver", true, () -> Umc.status(ps.save()), "Enregistrer le profil actif maintenant");
		y += 20;
		for (String name : profileList) {
			boolean active = name.equals(ps.activeProfile());
			row(g, x, y, w, 18);
			ui.text(Draw.ellipsize(font, name + (active ? "  (actif)" : ""), w - 190), x + 6, y + 5, active ? t.accent() : t.text());
			int bx = x + w - 42;
			String dk = "pdel:" + name;
			ui.button(bx, y + 2, 40, 14, confirming(dk) ? "Sûr ?" : "Suppr.", true, () -> {
				if (!confirm(dk)) return;
				Umc.status(ps.remove(name));
				profileListTime = -10;
			}, "Déplacer dans la corbeille .trash (cliquer deux fois)");
			bx -= 60;
			ui.button(bx, y + 2, 58, 14, "Exporter", active, () -> {
				Path target = Umc.configDir.resolve("exports").resolve(name + "-" + LocalDateTime.now().format(STAMP) + ".json");
				Umc.status(ps.exportTo(target));
			}, active ? "Copie dans config/umc/exports" : "Chargez ce profil pour l'exporter");
			bx -= 60;
			ui.button(bx, y + 2, 58, 14, "Charger", !active, () -> {
				Umc.status(ps.load(name));
				profileListTime = -10;
			}, null);
			y += 20;
		}
		if (!ps.lastWarnings().isEmpty()) {
			y = wrap("Avertissements du dernier chargement :", x, y + 2, w, t.warn());
			for (int i = 0; i < Math.min(4, ps.lastWarnings().size()); i++) y = wrap("• " + ps.lastWarnings().get(i), x + 6, y, w - 6, t.textDim());
		}
		y = section("Profils prédéfinis", x, y, w);
		for (Builtin b : builtins()) {
			row(g, x, y, w, 22);
			ui.text(b.name(), x + 6, y + 3, t.text());
			ui.text(Draw.ellipsize(font, b.description(), w - 90), x + 6, y + 12, t.textDim());
			ui.button(x + w - 72, y + 4, 70, 14, "Appliquer", true, () -> {
				JsonElement doc = BuiltinProfiles.load(b.file());
				Umc.status(doc == null ? OpResult.fail("Profil prédéfini introuvable") : ps.useBuiltin(b.name(), doc));
				profileListTime = -10;
			}, "Crée un profil personnel modifiable à partir de ce modèle");
			y += 24;
		}
		y += 4;
		ui.button(x, y, 110, 14, "Ouvrir le dossier", true, () -> Util.getPlatform().openPath(Umc.configDir), Umc.configDir.toString());
		ui.text("Import : glissez un profil .json sur la fenêtre", x + 116, y + 3, t.textDim());
		return y + 18;
	}

	// ---------------------------------------------------------------- capacités

	private int capabilitiesPage(GuiGraphicsExtractor g, int x, int y, int w) {
		Theme t = Umc.theme;
		y = wrap("Source : " + Umc.adapter.name() + ". Rien n'est simulé : une donnée absente est indiquée comme indisponible, avec son motif.", x, y, w, t.textDim());
		y += 4;
		int col1 = Math.min(150, w * 2 / 5), col2 = 104;
		for (CapabilityInfo c : Umc.adapter.capabilities()) {
			row(g, x, y, w, 15);
			ui.text(Draw.ellipsize(font, c.capability().label, col1 - 8), x + 6, y + 4, t.text());
			int color = switch (c.availability()) {
				case AVAILABLE -> t.good();
				case STALE, PERMISSION_REQUIRED -> t.warn();
				case UNAVAILABLE -> t.textDim();
				case UNSUPPORTED -> t.bad();
			};
			ui.text(c.availability().label, x + col1, y + 4, color);
			String reason = c.reason() == null ? "" : c.reason();
			ui.text(Draw.ellipsize(font, reason, w - col1 - col2 - 8), x + col1 + col2, y + 4, t.textDim());
			if (!reason.isEmpty() && ui.hovered(x, y, w, 15)) ui.tooltip(reason);
			y += 16;
		}
		int implemented = 0, planned = 0;
		for (ModuleDefinitions.Definition d : Umc.definitions.all()) {
			if (d.java() == null) continue;
			if (d.java().status().equals("planned")) planned++;
			else if (d.java().shown()) implemented++;
		}
		y = section("Modules", x, y, w);
		y = wrap("Java : " + implemented + " module(s) disponible(s), " + planned + " prévu(s). Les modules prévus ne sont pas activables.", x, y, w, t.textDim());
		long unavailable = Umc.adapter.capabilities().stream().filter(c -> c.availability() != Availability.AVAILABLE).count();
		return wrap(unavailable + " capacité(s) actuellement non disponible(s).", x, y, w, t.textDim());
	}

	// ---------------------------------------------------------------- réglages

	private int settingsPage(GuiGraphicsExtractor g, int x, int y, int w) {
		Theme t = Umc.theme;
		y = section("Thème", x, y - 4, w);
		for (Setting<?> s : Umc.theme.settings()) y = settingRow(g, s, x, y, w);
		y = section("HUD", x, y, w);
		for (Setting<?> s : Umc.hud.settings()) y = settingRow(g, s, x, y, w);
		ui.button(x, y, 110, 14, "Éditer le HUD", true, this::openHudEditor, "Déplacer et redimensionner les éléments");
		y += 18;
		y = section("Menu et sauvegarde", x, y, w);
		y = settingRow(g, Umc.gui.pauseGame, x, y, w);
		y = settingRow(g, Umc.gui.autosave, x, y, w);
		y = settingRow(g, Umc.gui.rememberPage, x, y, w);
		y = section("Réinitialisation", x, y, w);
		String rk = "resetall";
		ui.button(x, y, 200, 14, confirming(rk) ? "Confirmer : tout réinitialiser" : "Tout réinitialiser (profil actif)", true, () -> {
			if (!confirm(rk)) return;
			Umc.modules.resetAll();
			Umc.theme.resetSettings();
			Umc.hud.resetAll();
			Setting.bumpRevision();
			Umc.status("Modules, thème et HUD remis par défaut", true);
		}, "Les autres profils et les waypoints ne sont pas touchés");
		y += 18;
		y = section("Confidentialité", x, y, w);
		return wrap("Aucune donnée n'est envoyée sur Internet. Profils et waypoints restent dans " + Umc.configDir + ".", x, y, w, t.textDim());
	}

	// ---------------------------------------------------------------- entrées

	@Override
	public boolean mouseClicked(MouseButtonEvent e, boolean doubleClick) {
		ui.focused = null;
		capturing = null;
		ui.mouseClicked(e.x(), e.y(), e.button());
		return true;
	}

	@Override
	public boolean mouseReleased(MouseButtonEvent e) {
		ui.activeDrag = null;
		return true;
	}

	@Override
	public boolean mouseDragged(MouseButtonEvent e, double dx, double dy) {
		if (ui.activeDrag != null) ui.activeDrag.drag(e.x(), e.y());
		return true;
	}

	@Override
	public boolean mouseScrolled(double mx, double my, double sx, double sy) {
		scrollTarget = Math.clamp(scrollTarget - (float) sy * 30f, 0, Math.max(0, contentH - viewH));
		return true;
	}

	private static boolean isOpenKey(int key) {
		InputConstants.Key k = KeyMappingHelper.getBoundKeyOf(Umc.openGuiKey);
		return k != null && k.getType() == InputConstants.Type.KEYSYM && k.getValue() == key;
	}

	@Override
	public boolean keyPressed(KeyEvent e) {
		int k = e.key();
		if (capturing != null) {
			if (k == InputConstants.KEY_BACKSPACE || k == InputConstants.KEY_DELETE) capturing.setKeybind(Module.NO_KEY);
			else if (k != InputConstants.KEY_ESCAPE) capturing.setKeybind(k);
			capturing = null;
			return true;
		}
		TextField f = ui.focused;
		if (f != null) {
			if (k == InputConstants.KEY_BACKSPACE) f.backspace(e.hasControlDown());
			else if (k == InputConstants.KEY_RETURN || k == InputConstants.KEY_NUMPADENTER) f.submit();
			else if (k == InputConstants.KEY_ESCAPE) ui.focused = null;
			else if (k == InputConstants.KEY_V && e.hasControlDown()) f.type(minecraft.keyboardHandler.getClipboard());
			return true;
		}
		if (k == InputConstants.KEY_ESCAPE && detail != null) {
			openDetail(null);
			return true;
		}
		if (isOpenKey(k)) {
			onClose();
			return true;
		}
		return super.keyPressed(e);
	}

	@Override
	public boolean charTyped(CharacterEvent e) {
		if (ui.focused == null || !e.isAllowedChatCharacter()) return false;
		ui.focused.type(e.codepointAsString());
		return true;
	}

	@Override
	public void onFilesDrop(List<Path> paths) {
		for (Path p : paths) {
			if (!p.getFileName().toString().toLowerCase(Locale.ROOT).endsWith(".json")) {
				Umc.status("Fichier ignoré (JSON attendu) : " + p.getFileName(), false);
				continue;
			}
			JsonElement doc = ProfileStore.readJson(p);
			if (doc == null) Umc.status("JSON illisible : " + p.getFileName(), false);
			else if (ProfileStore.isProfileDocument(doc)) {
				Umc.status(Umc.profiles.importFrom(p, null));
				profileListTime = -10;
			} else if (doc.isJsonObject() && doc.getAsJsonObject().has("format")
					&& "umc-waypoints".equals(doc.getAsJsonObject().get("format").getAsString())) {
				OpResult r = Umc.waypoints.importFrom(p);
				if (r.ok()) {
					OpResult saved = Umc.waypoints.save();
					if (!saved.ok()) r = saved;
				}
				Umc.status(r);
			} else Umc.status(p.getFileName() + " n'est ni un profil ni un fichier de waypoints UMC", false);
		}
	}

	@Override
	public void onClose() {
		lastPage = page;
		lastCategory = category;
		lastFavorites = favorites;
		if (Umc.gui.autosave.on() && Umc.profiles.dirty()) Umc.profiles.save();
		super.onClose();
	}
}
