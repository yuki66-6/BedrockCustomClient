package umc.common.data;

/** Disponibilité d'une donnée (identifiants : shared/schemas/datapoint.schema.json). */
public enum Availability {
	AVAILABLE("available", "Disponible"),
	STALE("stale", "Périmé"),
	UNAVAILABLE("unavailable", "Indisponible"),
	UNSUPPORTED("unsupported", "Non pris en charge"),
	PERMISSION_REQUIRED("permission_required", "Permission requise");

	public final String id;
	public final String label;

	Availability(String id, String label) {
		this.id = id;
		this.label = label;
	}
}
