package umc.client.mixin;

import com.mojang.blaze3d.platform.InputConstants;
import net.minecraft.client.KeyMapping;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import umc.client.telemetry.ClickTracker;

/** Compte chaque appui réel (KeyMapping.click est appelé une fois par pression de touche ou de bouton). Lecture seule. */
@Mixin(KeyMapping.class)
public abstract class KeyMappingMixin {
	@Inject(method = "click", at = @At("HEAD"))
	private static void umc$countClick(InputConstants.Key key, CallbackInfo ci) {
		ClickTracker.onKey(key);
	}
}
