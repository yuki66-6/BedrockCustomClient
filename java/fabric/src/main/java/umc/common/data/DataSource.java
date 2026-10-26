package umc.common.data;

/** Provenance d'une donnée (identifiants : shared/schemas/datapoint.schema.json). */
public enum DataSource {
	GAME_API("game_api"),
	RAW_INPUT("raw_input"),
	WINDOW_MANAGER("window_manager"),
	SERVER_QUERY("server_query"),
	SAVE_FILE("save_file"),
	OVERLAY("overlay"),
	SIMULATION("simulation"),
	USER("user"),
	NONE("none");

	public final String id;

	DataSource(String id) {
		this.id = id;
	}
}
