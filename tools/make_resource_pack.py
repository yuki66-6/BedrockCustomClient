"""Génère le pack de ressources Bedrock « UMC PvP UI » (fichier .mcpack).

Le pack ne contient que des images dessinées ici (aucune texture de Mojang) :
réticules au choix (sous-packs), hotbar sombre et case sélectionnée à la couleur
du thème. Il s'importe dans Minecraft par un double-clic sur le .mcpack ; rien
n'est copié automatiquement dans le jeu.

    python tools/make_resource_pack.py            # dist/UMC-PvP-UI-<version>.mcpack
    python tools/make_resource_pack.py --check    # génère puis vérifie le contenu
"""

import argparse
import json
import struct
import sys
import zipfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = [1, 1, 0]
# Identifiants fixes : une nouvelle version remplace l'ancienne dans Minecraft.
HEADER_UUID = "6f1e0c2a-3b4d-4e5f-8a9b-0c1d2e3f4a5b"
MODULE_UUID = "7a2f1d3b-4c5e-4f60-9b8c-1d2e3f4a5b6c"

ACCENT = (69, 138, 255)
WHITE = (255, 255, 255, 255)


# ---------------------------------------------------------------- PNG

def png(width, height, pixels):
    """PNG RGBA 8 bits ; pixels[y][x] = (r, g, b, a)."""
    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for r, g, b, a in row:
            raw += bytes((r, g, b, a))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")


def canvas(w, h, fill=(0, 0, 0, 0)):
    return [[fill for _ in range(w)] for _ in range(h)]


def rect(img, x, y, w, h, color):
    for yy in range(max(0, y), min(len(img), y + h)):
        for xx in range(max(0, x), min(len(img[0]), x + w)):
            img[yy][xx] = color


# ---------------------------------------------------------------- réticules
# Même grille que le réticule du jeu : image 16×16, centre en (7, 7).

C = 7


def crosshair(style):
    img = canvas(16, 16)

    def arms(first, last, directions=((1, 0), (-1, 0), (0, 1), (0, -1))):
        for d in range(first, last + 1):
            for dx, dy in directions:
                img[C + dy * d][C + dx * d] = WHITE

    if style == "cross":  # identique au réticule du jeu
        arms(0, 4)
    if style == "gap":
        arms(3, 6)
    if style == "cross_dot":
        arms(3, 6)
        img[C][C] = WHITE
    if style == "dot":
        rect(img, C, C, 1, 1, WHITE)
    if style == "big_dot":
        arms(0, 1)
    if style == "t":
        arms(2, 5, ((1, 0), (-1, 0), (0, 1)))
    if style == "square":
        for d in range(-3, 4):
            for x, y in ((C + d, C - 3), (C + d, C + 3), (C - 3, C + d), (C + 3, C + d)):
                img[y][x] = WHITE
        img[C][C] = WHITE
    if style == "circle":
        for y in range(16):
            for x in range(16):
                r = ((x - C) ** 2 + (y - C) ** 2) ** 0.5
                if 3.5 <= r < 4.5:
                    img[y][x] = WHITE
        img[C][C] = WHITE
    return png(16, 16, img)


# Sous-packs : choix du réticule dans les réglages du pack (icône d'engrenage).
CROSSHAIRS = [
    ("croix", "Croix (vanilla)", "cross"),
    ("croix_ecart", "Croix avec écart", "gap"),
    ("croix_point", "Croix écartée + point", "cross_dot"),
    ("point", "Point", "dot"),
    ("petit_plus", "Petit plus", "big_dot"),
    ("t", "T", "t"),
    ("carre", "Carré", "square"),
    ("cercle", "Cercle", "circle"),
]


# ---------------------------------------------------------------- hotbar
# Tailles du jeu : hotbar_0..8 = 20×22, extrémités = 1×22, case sélectionnée = 24×24.

PANEL = (12, 14, 20, 170)
EDGE = (255, 255, 255, 46)


def hotbar_slot(last):
    img = canvas(20, 22, PANEL)
    rect(img, 0, 0, 20, 1, EDGE)
    rect(img, 0, 21, 20, 1, EDGE)
    if not last:
        rect(img, 19, 3, 1, 16, (255, 255, 255, 22))
    return png(20, 22, img)


def hotbar_cap():
    img = canvas(1, 22, EDGE)
    return png(1, 22, img)


def selected_slot():
    img = canvas(24, 24)
    a = ACCENT + (255,)
    soft = ACCENT + (60,)
    rect(img, 2, 0, 20, 2, a)
    rect(img, 2, 22, 20, 2, a)
    rect(img, 0, 2, 2, 20, a)
    rect(img, 22, 2, 2, 20, a)
    for x, y in ((1, 1), (22, 1), (1, 22), (22, 22)):
        img[y][x] = a
    rect(img, 2, 2, 20, 20, soft)
    return png(24, 24, img)


def pack_icon():
    s = 128
    img = canvas(s, s)
    for y in range(s):
        for x in range(s):
            dx, dy = min(x, s - 1 - x), min(y, s - 1 - y)
            corner = dx < 20 and dy < 20 and (20 - dx) ** 2 + (20 - dy) ** 2 > 400
            if not corner:
                img[y][x] = (16, 18, 26, 255)
            r = ((x - 63.5) ** 2 + (y - 63.5) ** 2) ** 0.5
            if 30 <= r < 38:
                img[y][x] = ACCENT + (255,)
    rect(img, 60, 34, 8, 22, WHITE)
    rect(img, 60, 72, 8, 22, WHITE)
    rect(img, 34, 60, 22, 8, WHITE)
    rect(img, 72, 60, 22, 8, WHITE)
    return png(s, s, img)


# ---------------------------------------------------------------- pack

def manifest():
    return {
        "format_version": 2,
        "header": {
            "name": "UMC PvP UI",
            "description": "Réticules au choix, hotbar sombre et case sélectionnée bleue. Universal Minecraft Client.",
            "uuid": HEADER_UUID,
            "version": VERSION,
            "min_engine_version": [1, 21, 0],
        },
        "modules": [{"type": "resources", "uuid": MODULE_UUID, "version": VERSION}],
        "subpacks": [{"folder_name": folder, "name": name, "memory_tier": 0} for folder, name, _ in CROSSHAIRS],
    }


def files():
    out = {
        "manifest.json": json.dumps(manifest(), ensure_ascii=False, indent=2).encode("utf-8"),
        "pack_icon.png": pack_icon(),
        "textures/ui/cross_hair.png": crosshair("cross"),
        "textures/ui/selected_hotbar_slot.png": selected_slot(),
        "textures/ui/hotbar_start_cap.png": hotbar_cap(),
        "textures/ui/hotbar_end_cap.png": hotbar_cap(),
    }
    for i in range(9):
        out[f"textures/ui/hotbar_{i}.png"] = hotbar_slot(i == 8)
    for folder, _, style in CROSSHAIRS:
        out[f"subpacks/{folder}/textures/ui/cross_hair.png"] = crosshair(style)
    return out


EXPECTED_SIZES = {"cross_hair.png": (16, 16), "selected_hotbar_slot.png": (24, 24), "hotbar_start_cap.png": (1, 22),
                  "hotbar_end_cap.png": (1, 22), "pack_icon.png": (128, 128)}


def check(path):
    errors = []
    with zipfile.ZipFile(path) as z:
        names = set(z.namelist())
        m = json.loads(z.read("manifest.json").decode("utf-8"))
        if m["header"]["uuid"] == m["modules"][0]["uuid"]:
            errors.append("UUID d'en-tête et de module identiques")
        for sp in m.get("subpacks", []):
            if f"subpacks/{sp['folder_name']}/textures/ui/cross_hair.png" not in names:
                errors.append(f"sous-pack sans réticule : {sp['folder_name']}")
        for name in names:
            if "\\" in name:
                errors.append(f"séparateur Windows dans le zip : {name}")
            if not name.endswith(".png"):
                continue
            data = z.read(name)
            if data[:8] != b"\x89PNG\r\n\x1a\n":
                errors.append(f"PNG invalide : {name}")
                continue
            w, h = struct.unpack(">II", data[16:24])
            base = name.rsplit("/", 1)[-1]
            want = (20, 22) if base.startswith("hotbar_") and base[7:8].isdigit() else EXPECTED_SIZES.get(base)
            if want and (w, h) != want:
                errors.append(f"{name} : {w}×{h} au lieu de {want[0]}×{want[1]}")
    return errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "dist"))
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    target = out_dir / ("UMC-PvP-UI-" + ".".join(map(str, VERSION)) + ".mcpack")
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in sorted(files().items()):
            z.writestr(name, data)
    print(f"Pack : {target}")
    if args.check:
        errors = check(target)
        for e in errors:
            print("ERREUR :", e)
        if errors:
            return 1
        print("Vérification : OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
