package umc.common.settings;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonPrimitive;

import java.util.List;
import java.util.Objects;

/** Types de réglages concrets. */
public final class Settings {
	private Settings() {
	}

	public static final class Bool extends Setting<Boolean> {
		private boolean value;
		private final boolean def;

		public Bool(String id, String name, String desc, boolean def) {
			super(id, name, desc);
			this.value = this.def = def;
		}

		@Override public Boolean get() { return value; }
		public boolean on() { return value; }
		@Override public void set(Boolean v) {
			if (v == value) return;
			value = v;
			changed();
		}
		@Override public void reset() { set(def); }
		@Override public JsonElement toJson() { return new JsonPrimitive(value); }
		@Override public boolean fromJson(JsonElement j) {
			if (j == null || !j.isJsonPrimitive() || !j.getAsJsonPrimitive().isBoolean()) return false;
			set(j.getAsBoolean());
			return true;
		}
	}

	public static final class Int extends Setting<Integer> {
		private int value;
		private final int def, min, max;
		private final String format;

		public Int(String id, String name, String desc, int def, int min, int max, String format) {
			super(id, name, desc);
			this.min = min;
			this.max = max;
			this.value = this.def = Math.clamp(def, min, max);
			this.format = format;
		}

		@Override public Integer get() { return value; }
		public int min() { return min; }
		public int max() { return max; }
		public String formatted() { return String.format(format, value); }
		@Override public void set(Integer v) {
			int c = Math.clamp(v, min, max);
			if (c == value) return;
			value = c;
			changed();
		}
		@Override public void reset() { set(def); }
		@Override public JsonElement toJson() { return new JsonPrimitive(value); }
		@Override public boolean fromJson(JsonElement j) {
			if (j == null || !j.isJsonPrimitive() || !j.getAsJsonPrimitive().isNumber()) return false;
			double d = j.getAsDouble();
			if (!Double.isFinite(d)) return false;
			set((int) Math.round(Math.clamp(d, min, max)));
			return true;
		}
	}

	public static final class Float extends Setting<java.lang.Float> {
		private float value;
		private final float def, min, max;
		private final String format;

		public Float(String id, String name, String desc, float def, float min, float max, String format) {
			super(id, name, desc);
			this.min = min;
			this.max = max;
			this.value = this.def = Math.clamp(def, min, max);
			this.format = format;
		}

		@Override public java.lang.Float get() { return value; }
		public float value() { return value; }
		public float min() { return min; }
		public float max() { return max; }
		public String formatted() { return String.format(format, value); }
		@Override public void set(java.lang.Float v) {
			if (v == null || !java.lang.Float.isFinite(v)) return;
			float c = Math.clamp(v, min, max);
			if (c == value) return;
			value = c;
			changed();
		}
		@Override public void reset() { set(def); }
		@Override public JsonElement toJson() { return new JsonPrimitive(value); }
		@Override public boolean fromJson(JsonElement j) {
			if (j == null || !j.isJsonPrimitive() || !j.getAsJsonPrimitive().isNumber()) return false;
			double d = j.getAsDouble();
			if (!Double.isFinite(d)) return false;
			set((float) d);
			return true;
		}
	}

	/** Liste d'options, sérialisée par nom (stable si l'ordre change). */
	public static final class Choice extends Setting<Integer> {
		private final List<String> options;
		private int index;
		private final int def;

		public Choice(String id, String name, String desc, List<String> options, int def) {
			super(id, name, desc);
			this.options = List.copyOf(options);
			this.index = this.def = Math.clamp(def, 0, options.size() - 1);
		}

		@Override public Integer get() { return index; }
		public String current() { return options.get(index); }
		public List<String> options() { return options; }
		@Override public void set(Integer i) {
			int c = Math.clamp(i, 0, options.size() - 1);
			if (c == index) return;
			index = c;
			changed();
		}
		public void cycle(int delta) { set(Math.floorMod(index + delta, options.size())); }
		@Override public void reset() { set(def); }
		@Override public JsonElement toJson() { return new JsonPrimitive(current()); }
		@Override public boolean fromJson(JsonElement j) {
			if (j == null || !j.isJsonPrimitive()) return false;
			if (j.getAsJsonPrimitive().isString()) {
				int i = options.indexOf(j.getAsString());
				if (i < 0) return false;
				set(i);
				return true;
			}
			if (j.getAsJsonPrimitive().isNumber()) {
				int i = j.getAsInt();
				if (i < 0 || i >= options.size()) return false;
				set(i);
				return true;
			}
			return false;
		}
	}

	/** Couleur ARGB ; JSON [r, g, b, a] en flottants 0..1 (comme la version Bedrock). */
	public static final class Color extends Setting<Integer> {
		private int argb;
		private final int def;

		public Color(String id, String name, String desc, int defArgb) {
			super(id, name, desc);
			this.argb = this.def = defArgb;
		}

		@Override public Integer get() { return argb; }
		public int argb() { return argb; }
		@Override public void set(Integer v) {
			if (v == argb) return;
			argb = v;
			changed();
		}
		@Override public void reset() { set(def); }
		@Override public JsonElement toJson() {
			JsonArray a = new JsonArray();
			a.add(((argb >> 16) & 0xff) / 255f);
			a.add(((argb >> 8) & 0xff) / 255f);
			a.add((argb & 0xff) / 255f);
			a.add(((argb >>> 24) & 0xff) / 255f);
			return a;
		}
		@Override public boolean fromJson(JsonElement j) {
			if (j == null || !j.isJsonArray() || j.getAsJsonArray().size() != 4) return false;
			int[] c = new int[4];
			for (int i = 0; i < 4; i++) {
				JsonElement e = j.getAsJsonArray().get(i);
				if (!e.isJsonPrimitive() || !e.getAsJsonPrimitive().isNumber()) return false;
				c[i] = (int) Math.round(Math.clamp(e.getAsDouble(), 0, 1) * 255);
			}
			set((c[3] << 24) | (c[0] << 16) | (c[1] << 8) | c[2]);
			return true;
		}
	}

	public static final class Text extends Setting<String> {
		private String value;
		private final String def;
		private final int maxLength;

		public Text(String id, String name, String desc, String def, int maxLength) {
			super(id, name, desc);
			this.value = this.def = def;
			this.maxLength = maxLength;
		}

		@Override public String get() { return value; }
		public int maxLength() { return maxLength; }
		@Override public void set(String v) {
			if (v == null) v = "";
			if (v.length() > maxLength) v = v.substring(0, maxLength);
			if (Objects.equals(v, value)) return;
			value = v;
			changed();
		}
		@Override public void reset() { set(def); }
		@Override public JsonElement toJson() { return new JsonPrimitive(value); }
		@Override public boolean fromJson(JsonElement j) {
			if (j == null || !j.isJsonPrimitive() || !j.getAsJsonPrimitive().isString()) return false;
			set(j.getAsString());
			return true;
		}
	}
}
