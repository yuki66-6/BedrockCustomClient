package umc.client.gui;

import java.util.function.Consumer;

/** Champ de saisie minimal (ajout en fin, retour arrière, collage). */
public final class TextField {
	private final StringBuilder text = new StringBuilder();
	private final int maxLength;
	private Consumer<String> onChange = s -> {};
	private Consumer<String> onSubmit = s -> {};

	public TextField(int maxLength) {
		this.maxLength = maxLength;
	}

	public TextField onChange(Consumer<String> c) {
		this.onChange = c;
		return this;
	}

	public TextField onSubmit(Consumer<String> c) {
		this.onSubmit = c;
		return this;
	}

	public String get() {
		return text.toString();
	}

	public void set(String s) {
		text.setLength(0);
		type(s == null ? "" : s);
	}

	public void type(String s) {
		for (int i = 0; i < s.length() && text.length() < maxLength; i++) {
			char c = s.charAt(i);
			if (c >= 32 && c != 127) text.append(c);
		}
		onChange.accept(get());
	}

	public void backspace(boolean word) {
		if (text.isEmpty()) return;
		if (!word) text.setLength(text.length() - 1);
		else {
			int i = text.length();
			while (i > 0 && text.charAt(i - 1) == ' ') i--;
			while (i > 0 && text.charAt(i - 1) != ' ') i--;
			text.setLength(i);
		}
		onChange.accept(get());
	}

	public void submit() {
		onSubmit.accept(get());
	}
}
