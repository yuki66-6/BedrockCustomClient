package umc.client.telemetry;

import umc.common.data.Capability;
import umc.common.data.CapabilityInfo;
import umc.common.data.DataPoint;

import java.util.ArrayList;
import java.util.List;

/**
 * Interface commune aux intégrations de jeu (même contrat que
 * bedrock/src/integration/GameAdapter.h). Toute donnée est un DataPoint :
 * jamais de valeur inventée, toujours un motif si elle n'est pas disponible.
 */
public interface GameAdapter {
	String name();

	CapabilityInfo capability(Capability c);

	default List<CapabilityInfo> capabilities() {
		List<CapabilityInfo> out = new ArrayList<>();
		for (Capability c : Capability.values()) out.add(capability(c));
		return out;
	}

	DataPoint<Integer> gameFps();

	DataPoint<Integer> latencyMs();

	/** {x, y, z} du joueur. */
	DataPoint<double[]> position();

	DataPoint<Float> yaw();

	/** « overworld », « nether », « end » ou identifiant complet. */
	DataPoint<String> dimension();

	/** Clé de monde pour les waypoints (« java:sp:… », « java:mp:… », « java:realm:… »). */
	DataPoint<String> worldKey();

	DataPoint<String> worldName();

	DataPoint<Long> seed();
}
