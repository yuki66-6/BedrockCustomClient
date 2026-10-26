package umc.client.module;

/** Catégories de modules (identifiants : shared/schemas/module-definition.schema.json). */
public enum Category {
	COMBAT("combat", "Combat"),
	MOVEMENT("movement", "Mouvement"),
	RENDER("render", "Rendu"),
	PLAYER("player", "Joueur"),
	UTILITY("utility", "Utilitaires");

	public final String id;
	public final String label;

	Category(String id, String label) {
		this.id = id;
		this.label = label;
	}
}
