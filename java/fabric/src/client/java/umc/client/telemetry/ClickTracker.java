package umc.client.telemetry;

import com.mojang.blaze3d.platform.InputConstants;
import net.minecraft.client.Minecraft;
import umc.common.util.GameMath;

/**
 * Clics réels de l'utilisateur, alimentés par KeyMappingMixin à chaque appui
 * (plus précis qu'un échantillonnage par tick). Comptés seulement en jeu.
 */
public final class ClickTracker {
	public static final GameMath.CpsCounter LEFT = new GameMath.CpsCounter();
	public static final GameMath.CpsCounter RIGHT = new GameMath.CpsCounter();

	private ClickTracker() {
	}

	public static double now() {
		return System.nanoTime() / 1e9;
	}

	public static void onKey(InputConstants.Key key) {
		Minecraft mc = Minecraft.getInstance();
		if (mc == null || mc.options == null || mc.player == null || mc.gui.screen() != null) return;
		if (mc.options.keyAttack.matches(key)) LEFT.click(now());
		else if (mc.options.keyUse.matches(key)) RIGHT.click(now());
	}

	public static int left() {
		return LEFT.count(now());
	}

	public static int right() {
		return RIGHT.count(now());
	}
}
