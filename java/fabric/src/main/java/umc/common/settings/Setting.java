package umc.common.settings;

import com.google.gson.JsonElement;

import java.util.concurrent.atomic.AtomicLong;
import java.util.function.BooleanSupplier;

/**
 * Réglage d'un module ou d'un groupe. Même sérialisation JSON que la version
 * Bedrock (bedrock/src/core/Setting.h) pour que les profils soient communs.
 * Toute modification incrémente un compteur global (sauvegarde automatique).
 */
public abstract class Setting<T> {
	private static final AtomicLong REVISION = new AtomicLong(1);

	private final String id;
	private final String name;
	private final String description;
	private BooleanSupplier visibleIf = () -> true;
	private Runnable onChange = () -> {};

	protected Setting(String id, String name, String description) {
		this.id = id;
		this.name = name;
		this.description = description;
	}

	public static long revision() {
		return REVISION.get();
	}

	/** Signale un changement persistant qui n'est pas un réglage (position HUD, favori…). */
	public static void bumpRevision() {
		REVISION.incrementAndGet();
	}

	protected void changed() {
		REVISION.incrementAndGet();
		onChange.run();
	}

	public String id() {
		return id;
	}

	public String name() {
		return name;
	}

	public String description() {
		return description;
	}

	public boolean visible() {
		return visibleIf.getAsBoolean();
	}

	public Setting<T> visibleIf(BooleanSupplier predicate) {
		this.visibleIf = predicate;
		return this;
	}

	public Setting<T> onChange(Runnable callback) {
		this.onChange = callback;
		return this;
	}

	public abstract T get();

	public abstract void set(T value);

	public abstract void reset();

	public abstract JsonElement toJson();

	/** @return false si la valeur JSON est invalide (valeur actuelle conservée). */
	public abstract boolean fromJson(JsonElement json);

	public boolean persistent() {
		return true;
	}
}
