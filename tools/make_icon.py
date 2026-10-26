"""Génère assets/app.ico (16, 32, 48, 256 px) sans dépendance externe.

Motif : carré sombre arrondi, liseré bleu d'accent et réticule blanc.
Usage : python tools/make_icon.py
"""
import math
import os
import struct

ACCENT = (255, 138, 69)  # BGR de (69, 138, 255)
BG = (23, 20, 19)        # BGR anthracite


def pixel(size, x, y):
    s = size
    cx = cy = (s - 1) / 2
    r = s * 0.22                       # rayon des coins
    half = s / 2 - 0.5
    dx = max(abs(x - cx) - (half - r), 0)
    dy = max(abs(y - cy) - (half - r), 0)
    dist = math.hypot(dx, dy) - r      # distance signée au carré arrondi
    if dist > 0.5:
        return (0, 0, 0, 0)
    alpha = int(255 * min(1.0, 0.5 - dist)) if dist > -0.5 else 255
    border = s * 0.07
    color = BG
    if dist > -border:
        color = ACCENT
    # Réticule : quatre branches + point central.
    t = max(1.0, s * 0.06)
    gap = s * 0.10
    arm = s * 0.26
    ax, ay = abs(x - cx), abs(y - cy)
    if (ay <= t / 2 and gap <= ax <= arm) or (ax <= t / 2 and gap <= ay <= arm) or math.hypot(x - cx, y - cy) <= t * 0.75:
        color = (245, 245, 245)
    return (*color, alpha)


def bmp_image(size):
    rows = []
    for y in reversed(range(size)):  # BMP : de bas en haut
        row = b"".join(bytes(pixel(size, x, y)) for x in range(size))
        rows.append(row)
    xor = b"".join(rows)
    mask_row = ((size + 31) // 32) * 4
    andmask = b"\x00" * (mask_row * size)
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, len(xor) + len(andmask), 0, 0, 0, 0)
    return header + xor + andmask


def main():
    sizes = [16, 32, 48, 256]
    images = [bmp_image(s) for s in sizes]
    out = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    for s, img in zip(sizes, images):
        out += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(img), offset)
        offset += len(img)
    out += b"".join(images)
    path = os.path.join(os.path.dirname(__file__), "..", "assets", "app.ico")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(out)
    print("écrit", os.path.normpath(path), len(out), "octets")


if __name__ == "__main__":
    main()
