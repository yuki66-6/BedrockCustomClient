package umc.common.profile;

/** Résultat d'une opération sur les profils ou les waypoints. */
public record OpResult(boolean ok, String message) {
	public static OpResult ok(String message) {
		return new OpResult(true, message);
	}

	public static OpResult fail(String message) {
		return new OpResult(false, message);
	}
}
