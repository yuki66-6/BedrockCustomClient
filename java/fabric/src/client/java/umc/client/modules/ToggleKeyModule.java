package umc.client.modules;

import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.client.OptionInstance;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import umc.client.hud.HudContext;
import umc.client.hud.HudText;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.client.telemetry.Keys;
import umc.common.settings.Settings;

import java.util.List;
import java.util.function.Function;

/**
 * Base de ToggleSprint / ToggleSneak : maintient la touche vanilla « appuyée »
 * comme si le joueur la tenait (aucune action impossible au clavier).
 */
public abstract class ToggleKeyModule extends Module {
	private final Settings.Choice mode;
	private final Settings.Bool showHud = add(new Settings.Bool("show_status", "Afficher l'état", "Indique à l'écran si la bascule est active", true));
	private final Function<Minecraft, KeyMapping> mapping;
	private final Function<Minecraft, OptionInstance<Boolean>> vanillaToggle;
	private final String label;
	private boolean toggled;
	private boolean wasDown;

	protected ToggleKeyModule(String id, String name, String description, String label, List<String> modes,
			Function<Minecraft, KeyMapping> mapping, Function<Minecraft, OptionInstance<Boolean>> vanillaToggle) {
		super(id, name, description, Category.MOVEMENT, false);
		this.mode = add(new Settings.Choice("mode", "Mode", "Bascule à chaque appui ou maintien permanent", modes, 0));
		this.mapping = mapping;
		this.vanillaToggle = vanillaToggle;
		this.label = label;
	}

	private boolean vanillaToggleOn(Minecraft mc) {
		return mc.options != null && vanillaToggle.apply(mc).get();
	}

	/** Mode 0 = bascule à chaque appui ; mode 1 = toujours actif (sprint uniquement). */
	private boolean active(Minecraft mc) {
		return mode.get() == 1 || toggled;
	}

	@Override
	protected void onTick(Minecraft mc) {
		if (mc.player == null || vanillaToggleOn(mc)) return;
		KeyMapping key = mapping.apply(mc);
		boolean phys = Keys.physicallyDown(mc, key);
		if (mode.get() == 0) {
			if (phys && !wasDown && mc.gui.screen() == null) toggled = !toggled;
			wasDown = phys;
		}
		key.setDown(active(mc) || phys);
	}

	@Override
	protected void onDisable() {
		Minecraft mc = Minecraft.getInstance();
		toggled = false;
		if (mc.options != null && !vanillaToggleOn(mc)) {
			KeyMapping key = mapping.apply(mc);
			key.setDown(Keys.physicallyDown(mc, key));
		}
	}

	@Override
	public String limitation() {
		Minecraft mc = Minecraft.getInstance();
		return mc.options != null && vanillaToggleOn(mc)
				? "L'option vanilla « " + label + " : basculer » est active : désactivez-la pour utiliser ce module." : "";
	}

	@Override
	public boolean hasHud() {
		return true;
	}

	@Override
	public float defaultY() {
		return 0.95f;
	}

	@Override
	public int[] renderHud(GuiGraphicsExtractor g, HudContext ctx) {
		if (!showHud.on() && !ctx.editing()) return new int[] {0, 0};
		if (vanillaToggleOn(ctx.mc())) return new HudText(ctx).add(label + " : option vanilla active", ctx.theme().warn()).draw(g);
		boolean on = active(ctx.mc());
		if (!on && !ctx.editing()) return new int[] {0, 0};
		String state = mode.get() == 1 ? "toujours" : on ? "activé (bascule)" : "désactivé";
		return new HudText(ctx).label(label + " ", state).draw(g);
	}
}
