package umc.common.data;

/** État d'une capacité pour un adaptateur donné, avec le motif si elle n'est pas disponible. */
public record CapabilityInfo(Capability capability, Availability availability, DataSource source, String reason) {
}
