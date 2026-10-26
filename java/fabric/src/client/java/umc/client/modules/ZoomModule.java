package umc.client.modules;

import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import umc.client.module.Category;
import umc.client.module.Module;
import umc.common.settings.Settings;

/**
 * Zoom tant que la touche « Zoom » est maintenue (réglable dans les contrôles du jeu).
 * Divise le champ de vision renvoyé par Camera.getFov (voir CameraMixin).
 */
public final class ZoomModule extends Module {
	private final Settings.Float factor = add(new Settings.Float("factor", "Facteur", "Grossissement", 4f, 1.5f, 10f, "x%.1f"));
	private final Settings.Bool smooth = add(new Settings.Bool("smooth", "Transition douce", "Zoom progressif", true));
	private final Settings.Bool cinematic = add(new Settings.Bool("cinematic", "Caméra cinématique", "Active la caméra lissée vanilla pendant le zoom", false));

	private static ZoomModule instance;
	private KeyMapping key;
	private float current = 1f;
	private long lastNanos = System.nanoTime();
	private boolean cinematicApplied;
	private boolean previousSmoothCamera;

	public ZoomModule() {
		super("zoom", "Zoom", "Zoom en maintenant une touche (C par défaut)", Category.RENDER, true);
		instance = this;
	}

	public void setKey(KeyMapping key) {
		this.key = key;
	}

	private boolean held() {
		return enabled() && key != null && key.isDown() && Minecraft.getInstance().gui.screen() == null;
	}

	@Override
	protected void onTick(Minecraft mc) {
		boolean want = held() && cinematic.on();
		if (want && !cinematicApplied) {
			previousSmoothCamera = mc.options.smoothCamera;
			mc.options.smoothCamera = true;
			cinematicApplied = true;
		} else if (!want && cinematicApplied) {
			mc.options.smoothCamera = previousSmoothCamera;
			cinematicApplied = false;
		}
	}

	@Override
	protected void onDisable() {
		if (cinematicApplied) {
			Minecraft.getInstance().options.smoothCamera = previousSmoothCamera;
			cinematicApplied = false;
		}
		current = 1f;
	}

	/** Diviseur appliqué au FOV (1 = pas de zoom), appelé à chaque image. */
	public static float divisor() {
		ZoomModule z = instance;
		if (z == null) return 1f;
		long now = System.nanoTime();
		float dt = Math.min(0.1f, (now - z.lastNanos) / 1e9f);
		z.lastNanos = now;
		float target = z.held() ? z.factor.value() : 1f;
		if (!z.smooth.on()) z.current = target;
		else z.current += (target - z.current) * Math.min(1f, dt * 14f);
		if (Math.abs(z.current - target) < 0.001f) z.current = target;
		return z.current;
	}

	@Override
	public String limitation() {
		return "La sensibilité de la souris n'est pas réduite pendant le zoom (option « caméra cinématique » possible).";
	}
}
