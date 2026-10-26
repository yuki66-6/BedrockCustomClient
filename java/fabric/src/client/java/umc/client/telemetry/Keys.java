package umc.client.telemetry;

import com.mojang.blaze3d.platform.InputConstants;
import net.fabricmc.fabric.api.client.keymapping.v1.KeyMappingHelper;
import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import org.lwjgl.glfw.GLFW;

/** État physique des touches (indépendant de l'état « appuyé » que les modules de bascule imposent). */
public final class Keys {
	private Keys() {
	}

	public static boolean physicallyDown(Minecraft mc, KeyMapping mapping) {
		InputConstants.Key key = KeyMappingHelper.getBoundKeyOf(mapping);
		if (key == null || key.getValue() < 0) return false;
		if (key.getType() == InputConstants.Type.MOUSE)
			return GLFW.glfwGetMouseButton(mc.getWindow().handle(), key.getValue()) == GLFW.GLFW_PRESS;
		if (key.getType() == InputConstants.Type.KEYSYM) return InputConstants.isKeyDown(mc.getWindow(), key.getValue());
		return false;
	}

	/** Nom court de la touche liée (« Z », « Espace », « Bouton 4 »…). */
	public static String label(KeyMapping mapping) {
		InputConstants.Key key = KeyMappingHelper.getBoundKeyOf(mapping);
		return key == null ? "?" : key.getDisplayName().getString();
	}

	public static String glfwName(int code) {
		if (code < 0) return "Aucun";
		return InputConstants.Type.KEYSYM.getOrCreate(code).getDisplayName().getString();
	}
}
