package umc.client.modules;

import java.util.List;

public final class ToggleSprintModule extends ToggleKeyModule {
	public ToggleSprintModule() {
		super("toggle_sprint", "ToggleSprint", "Sprint en bascule ou permanent", "Sprint",
				List.of("Bascule (touche de sprint)", "Toujours sprinter"), mc -> mc.options.keySprint, mc -> mc.options.toggleSprint());
	}
}
