package umc.common.data;

import java.util.Optional;

/**
 * Donnée exposée par un {@code GameAdapter} : valeur, disponibilité, provenance,
 * horodatage, fraîcheur et motif. Même contrat que {@code bcc::DataPoint} (C++).
 *
 * @param value        absente si la donnée n'est pas disponible
 * @param availability disponibilité au moment de la mesure
 * @param source       provenance
 * @param timestamp    secondes (horloge monotone)
 * @param maxAge       validité en secondes (0 = illimitée)
 * @param reason       motif lisible si indisponible
 */
public record DataPoint<T>(Optional<T> value, Availability availability, DataSource source, double timestamp, double maxAge,
		String reason) {

	public static <T> DataPoint<T> available(T value, DataSource source, double timestamp, double maxAge) {
		return new DataPoint<>(Optional.of(value), Availability.AVAILABLE, source, timestamp, maxAge, "");
	}

	public static <T> DataPoint<T> missing(Availability availability, String reason, DataSource source) {
		return new DataPoint<>(Optional.empty(), availability, source, 0, 0, reason);
	}

	/** Disponibilité réelle à l'instant {@code now} : une valeur trop ancienne devient « stale ». */
	public Availability availabilityAt(double now) {
		if (availability == Availability.AVAILABLE && maxAge > 0 && now - timestamp > maxAge) return Availability.STALE;
		return availability;
	}

	public boolean usable(double now) {
		return value.isPresent() && availabilityAt(now) == Availability.AVAILABLE;
	}

	public T orElse(T fallback) {
		return value.orElse(fallback);
	}
}
