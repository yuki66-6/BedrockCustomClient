package umc.common.data;

/** Capacités communes Java/Bedrock (identifiants et ordre : shared/schemas/capabilities.json). */
public enum Capability {
	KEYBOARD_INPUT("keyboard_input", "Clavier"),
	MOUSE_INPUT("mouse_input", "Souris et CPS"),
	GAME_WINDOW("game_window", "Fenêtre du jeu"),
	GAME_FPS("game_fps", "FPS du jeu"),
	OVERLAY_FPS("overlay_fps", "FPS de l'overlay"),
	SERVER_LATENCY("server_latency", "Latence serveur"),
	PLAYER_POSITION("player_position", "Position du joueur"),
	PLAYER_ROTATION("player_rotation", "Orientation du joueur"),
	PLAYER_VELOCITY("player_velocity", "Vitesse du joueur"),
	PLAYER_HEALTH("player_health", "Santé"),
	PLAYER_HUNGER("player_hunger", "Faim"),
	PLAYER_ARMOR("player_armor", "Armure et durabilité"),
	INVENTORY("inventory", "Inventaire"),
	POTION_EFFECTS("potion_effects", "Effets de potion actifs"),
	DIMENSION("dimension", "Dimension actuelle"),
	WORLD_IDENTITY("world_identity", "Identité du monde / serveur"),
	WORLD_SEED("world_seed", "Seed du monde"),
	SAVED_WORLD_MAP("saved_world_map", "Carte depuis les sauvegardes"),
	DEATH_EVENTS("death_events", "Événements de mort"),
	IN_WORLD_RENDER("in_world_render", "Rendu dans le monde"),
	FOV_CONTROL("fov_control", "Contrôle du champ de vision"),
	MOVEMENT_TOGGLES("movement_toggles", "Bascule sprint / accroupi"),
	CHAT_COMMANDS("chat_commands", "Commandes de chat autorisées");

	public final String id;
	public final String label;

	Capability(String id, String label) {
		this.id = id;
		this.label = label;
	}

	public static Capability fromId(String id) {
		for (Capability c : values())
			if (c.id.equals(id)) return c;
		return null;
	}
}
