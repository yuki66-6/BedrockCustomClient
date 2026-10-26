package umc.client.modules;

import java.util.List;

public final class ToggleSneakModule extends ToggleKeyModule {
	public ToggleSneakModule() {
		super("toggle_sneak", "ToggleSneak", "Accroupi en bascule (un appui pour activer, un pour relâcher)", "Accroupi",
				List.of("Bascule (touche d'accroupissement)"), mc -> mc.options.keyShift, mc -> mc.options.toggleCrouch());
	}
}
