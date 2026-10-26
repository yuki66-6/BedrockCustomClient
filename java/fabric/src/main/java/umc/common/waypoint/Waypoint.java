package umc.common.waypoint;

/** Waypoint au format commun (shared/schemas/waypoints.schema.json). */
public final class Waypoint {
	public String id = "";
	public String name = "Waypoint";
	public int x, y = 64, z;
	public String dimension = "overworld";
	public String category = "other";
	public String color = "#458AFF";
	public String icon = "pin";
	public boolean visible = true;
	public long createdAt;
	public String source = "user";

	public Waypoint() {
	}

	public Waypoint(String name, int x, int y, int z, String dimension) {
		this.name = name;
		this.x = x;
		this.y = y;
		this.z = z;
		this.dimension = WaypointStore.normalizeDimension(dimension);
	}

	public double distanceTo(double px, double py, double pz) {
		double dx = x + 0.5 - px, dy = y - py, dz = z + 0.5 - pz;
		return Math.sqrt(dx * dx + dy * dy + dz * dz);
	}

	/** Couleur ARGB opaque depuis « #RRGGBB » / « #RRGGBBAA ». */
	public int argb() {
		try {
			String h = color.startsWith("#") ? color.substring(1) : color;
			int rgb = (int) Long.parseLong(h.substring(0, 6), 16);
			int a = h.length() >= 8 ? Integer.parseInt(h.substring(6, 8), 16) : 255;
			return (a << 24) | rgb;
		} catch (RuntimeException e) {
			return 0xFF458AFF;
		}
	}
}
