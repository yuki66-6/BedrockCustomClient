package umc.common.util;

import java.util.ArrayDeque;
import java.util.Deque;

/** Calculs purs utilisés par les modules (testables sans lancer le jeu). */
public final class GameMath {
	private GameMath() {
	}

	/** Équivalent Nether d'une coordonnée Overworld (÷ 8) et inversement (× 8). */
	public static int overworldToNether(int v) {
		return Math.floorDiv(v, 8);
	}

	public static int netherToOverworld(int v) {
		return v * 8;
	}

	private static final String[] CARDINAL = {"S", "SO", "O", "NO", "N", "NE", "E", "SE"};
	private static final String[] CARDINAL_FULL = {"Sud", "Sud-Ouest", "Ouest", "Nord-Ouest", "Nord", "Nord-Est", "Est", "Sud-Est"};
	private static final String[] AXIS = {"+Z", "-X", "-X", "-Z", "-Z", "+X", "+X", "+Z"};

	/** Lacet Minecraft (0 = sud, 90 = ouest) ramené à [0, 360). */
	public static float normalizeYaw(float yaw) {
		float y = yaw % 360f;
		return y < 0 ? y + 360f : y;
	}

	public static int octant(float yaw) {
		return Math.floorMod(Math.round(normalizeYaw(yaw) / 45f), 8);
	}

	public static String cardinal(float yaw) {
		return CARDINAL[octant(yaw)];
	}

	public static String cardinalFull(float yaw) {
		return CARDINAL_FULL[octant(yaw)];
	}

	public static String axis(float yaw) {
		return AXIS[octant(yaw)];
	}

	/** Angle (degrés, -180..180) entre la direction du regard et une cible ; 0 = droit devant. */
	public static float relativeAngle(float playerYaw, double px, double pz, double tx, double tz) {
		double targetYaw = Math.toDegrees(Math.atan2(-(tx - px), tz - pz));
		double d = (targetYaw - playerYaw) % 360.0;
		if (d > 180) d -= 360;
		if (d < -180) d += 360;
		return (float) d;
	}

	private static final String[] ARROWS = {"↑", "↗", "→", "↘", "↓", "↙", "←", "↖"};

	public static String arrow(float relativeAngle) {
		return ARROWS[Math.floorMod(Math.round(relativeAngle / 45f), 8)];
	}

	/** « 1:05 », « 12:34 », « ∞ » pour une durée en ticks (20 par seconde). */
	public static String formatTicks(int ticks, boolean infinite) {
		if (infinite) return "∞";
		int s = Math.max(0, ticks / 20);
		return (s / 60) + ":" + String.format("%02d", s % 60);
	}

	public static String roman(int n) {
		String[] r = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
		return n >= 1 && n <= 10 ? r[n - 1] : String.valueOf(n);
	}

	/** Heure du jeu « hh:mm » depuis le temps du jour (0 = 6 h, 6000 = midi). */
	public static String gameClock(long dayTime) {
		long t = Math.floorMod(dayTime, 24000L);
		long hours = (t / 1000 + 6) % 24;
		long minutes = (t % 1000) * 60 / 1000;
		return String.format("%02d:%02d", hours, minutes);
	}

	public static long gameDay(long dayTime) {
		return Math.floorDiv(dayTime, 24000L) + 1;
	}

	/** Clics par seconde sur une fenêtre glissante d'une seconde. */
	public static final class CpsCounter {
		private final Deque<Double> clicks = new ArrayDeque<>();
		private int peak;

		public void click(double now) {
			clicks.addLast(now);
			count(now);
		}

		public int count(double now) {
			while (!clicks.isEmpty() && now - clicks.peekFirst() >= 1.0) clicks.removeFirst();
			peak = Math.max(peak, clicks.size());
			return clicks.size();
		}

		public int peak() {
			return peak;
		}
	}

	/** Vitesse (blocs/s) lissée à partir des positions successives. */
	public static final class SpeedTracker {
		private final int window;
		private final Deque<double[]> samples = new ArrayDeque<>();

		public SpeedTracker(int window) {
			this.window = Math.max(2, window);
		}

		public void sample(double time, double x, double y, double z) {
			samples.addLast(new double[] {time, x, y, z});
			while (samples.size() > window) samples.removeFirst();
		}

		public void clear() {
			samples.clear();
		}

		public double horizontal() {
			if (samples.size() < 2) return 0;
			double[] a = samples.peekFirst(), b = samples.peekLast();
			double dt = b[0] - a[0];
			if (dt <= 0) return 0;
			return Math.hypot(b[1] - a[1], b[3] - a[3]) / dt;
		}

		public double vertical() {
			if (samples.size() < 2) return 0;
			double[] a = samples.peekFirst(), b = samples.peekLast();
			double dt = b[0] - a[0];
			return dt <= 0 ? 0 : (b[2] - a[2]) / dt;
		}
	}
}
