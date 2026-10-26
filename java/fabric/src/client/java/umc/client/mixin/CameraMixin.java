package umc.client.mixin;

import net.minecraft.client.Camera;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;
import umc.client.modules.ZoomModule;

/** Zoom : divise le champ de vision rendu par la caméra (aucune autre modification). */
@Mixin(Camera.class)
public abstract class CameraMixin {
	@Inject(method = "getFov", at = @At("RETURN"), cancellable = true)
	private void umc$zoom(CallbackInfoReturnable<Float> cir) {
		float d = ZoomModule.divisor();
		if (d != 1f) cir.setReturnValue(cir.getReturnValue() / d);
	}
}
