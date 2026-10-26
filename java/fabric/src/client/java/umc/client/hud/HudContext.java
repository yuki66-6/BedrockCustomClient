package umc.client.hud;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.Font;
import umc.client.gui.Theme;
import umc.client.telemetry.GameAdapter;

/** Contexte de dessin d'un élément HUD. */
public record HudContext(Minecraft mc, Font font, Theme theme, GameAdapter adapter, boolean editing, boolean background) {
}
