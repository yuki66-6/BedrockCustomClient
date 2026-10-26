"""Génère « UMC Tweaks » : une collection de packs de ressources Bedrock activables
un par un (fichier .mcaddon).

Chaque réglage est un pack séparé : après import (double-clic sur le .mcaddon),
on active dans Paramètres → Ressources globales ceux que l'on veut, et on les
cumule. Les variantes d'un même réglage (forme du réticule, couleur…) sont des
sous-packs, choisis avec l'icône d'engrenage du pack.

Deux sortes de packs :
- dessinés par ce script (réticules, hotbar, sélecteur, citrouille, reflet) :
  aucune image de Mojang, générés partout (CI comprise) ;
- dérivés du jeu installé sur ce PC (feu bas, verre propre) : le script lit les
  textures de Minecraft en lecture seule et les modifie localement. Ces images
  restent sur ton PC : elles ne sont jamais committées ni publiées. Sans
  Minecraft installé, ces packs sont simplement omis.

Rien n'est copié automatiquement dans le jeu.

    python tools/make_resource_pack.py            # dist/UMC-Tweaks-<version>.mcaddon
    python tools/make_resource_pack.py --check    # génère puis vérifie le contenu
"""

import argparse
import json
import os
import struct
import sys
import uuid
import zipfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = [1, 1, 0]
# Espace de noms fixe : les UUID restent identiques d'une version à l'autre,
# une nouvelle version remplace donc l'ancienne dans Minecraft.
NAMESPACE = uuid.UUID("5b0c7e8e-2f4a-4c1d-9e3b-6a7d8c9e0f12")
GAME_PACKS = Path(os.environ.get("UMC_MINECRAFT_PACKS", r"C:\XboxGames\Minecraft for Windows\Content\data\resource_packs"))

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


def decode_png(data):
    """Décode un PNG 8 bits (RGBA, RGB, gris, palette) en liste de lignes RGBA."""
    w, h, depth, ctype = struct.unpack(">IIBB", data[16:26])
    if depth != 8:
        raise ValueError("profondeur non gérée : %d" % depth)
    pos, idat, palette, trns = 8, b"", None, None
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        if kind == b"IDAT":
            idat += body
        elif kind == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        pos += 12 + length
    bpp = {6: 4, 2: 3, 0: 1, 4: 2, 3: 1}[ctype]
    raw, stride, prev, rows = zlib.decompress(idat), w * bpp, bytearray(w * bpp), []
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            up = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + up) & 255
            elif f == 3:
                line[x] = (line[x] + (a + up) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(up - c), abs(a - c), abs(a + up - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else up if pb <= pc else c)) & 255
        row = []
        for x in range(w):
            px = line[x * bpp:(x + 1) * bpp]
            if ctype == 6:
                row.append(tuple(px))
            elif ctype == 2:
                row.append((px[0], px[1], px[2], 255))
            elif ctype == 0:
                row.append((px[0], px[0], px[0], 255))
            elif ctype == 4:
                row.append((px[0], px[0], px[0], px[1]))
            else:
                r, g, b = palette[px[0]]
                row.append((r, g, b, trns[px[0]] if trns and px[0] < len(trns) else 255))
        rows.append(row)
        prev = line
    return w, h, rows


def canvas(w, h, fill=(0, 0, 0, 0)):
    return [[fill for _ in range(w)] for _ in range(h)]


def rect(img, x, y, w, h, color):
    for yy in range(max(0, y), min(len(img), y + h)):
        for xx in range(max(0, x), min(len(img[0]), x + w)):
            img[yy][xx] = color


# ---------------------------------------------------------------- textures du jeu (lecture seule)

def read_game_texture(archive, name):
    """Lit une image dans une archive .brarchive du jeu installé (None si absente)."""
    for pack in ("vanilla", "vanilla_base"):
        path = GAME_PACKS / pack / "__brarchive" / "textures" / archive
        if not path.is_file():
            continue
        d = path.read_bytes()
        if len(d) < 16 or d[:4] != bytes.fromhex("7d2725b1"):
            continue
        count = struct.unpack_from("<I", d, 8)[0]
        base = 16 + 256 * count
        for i in range(count):
            e = 16 + 256 * i
            if d[e + 1:e + 1 + d[e]].decode(errors="replace") == name:
                off, size = struct.unpack_from("<II", d, e + 248)
                return d[base + off:base + off + size]
    return None


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
DIGITS = {  # police 3×5
    "1": ["010", "110", "010", "010", "111"], "2": ["110", "001", "010", "100", "111"],
    "3": ["110", "001", "010", "001", "110"], "4": ["101", "101", "111", "001", "001"],
    "5": ["111", "100", "110", "001", "110"], "6": ["011", "100", "111", "101", "111"],
    "7": ["111", "001", "010", "010", "010"], "8": ["111", "101", "111", "101", "111"],
    "9": ["111", "101", "111", "001", "110"],
}


def hotbar_slot(index, numbered):
    img = canvas(20, 22, PANEL)
    rect(img, 0, 0, 20, 1, EDGE)
    rect(img, 0, 21, 20, 1, EDGE)
    if index < 8:
        rect(img, 19, 3, 1, 16, (255, 255, 255, 22))
    if numbered:
        for y, line in enumerate(DIGITS[str(index + 1)]):
            for x, bit in enumerate(line):
                if bit == "1":
                    img[2 + y][2 + x] = (255, 255, 255, 150)
    return png(20, 22, img)


def hotbar_cap():
    return png(1, 22, canvas(1, 22, EDGE))


def selected_slot(color):
    img = canvas(24, 24)
    a, soft = color + (255,), color + (60,)
    rect(img, 2, 0, 20, 2, a)
    rect(img, 2, 22, 20, 2, a)
    rect(img, 0, 2, 2, 20, a)
    rect(img, 22, 2, 2, 20, a)
    for x, y in ((1, 1), (22, 1), (1, 22), (22, 22)):
        img[y][x] = a
    rect(img, 2, 2, 20, 20, soft)
    return png(24, 24, img)


SELECTOR_COLORS = [
    ("bleu", "Bleu", ACCENT), ("rouge", "Rouge", (242, 82, 90)), ("vert", "Vert", (77, 219, 133)),
    ("violet", "Violet", (180, 108, 255)), ("orange", "Orange", (255, 168, 60)), ("blanc", "Blanc", (235, 238, 245)),
]


def soft_glint():
    """Reflet d'enchantement discret : bandes violettes peu lumineuses (dessinées ici)."""
    s = 128
    img = canvas(s, s, (0, 0, 0, 255))
    for y in range(s):
        for x in range(s):
            v = ((x + y) % 64) / 64.0
            k = max(0.0, 1.0 - abs(v - 0.5) * 4)
            img[y][x] = (int(60 * k), int(30 * k), int(90 * k), 255)
    return png(s, s, img)


def pack_icon(kind):
    s = 128
    img = canvas(s, s)
    for y in range(s):
        for x in range(s):
            dx, dy = min(x, s - 1 - x), min(y, s - 1 - y)
            if not (dx < 20 and dy < 20 and (20 - dx) ** 2 + (20 - dy) ** 2 > 400):
                img[y][x] = (16, 18, 26, 255)
    a = ACCENT + (255,)
    if kind == "crosshair":
        rect(img, 60, 30, 8, 26, WHITE)
        rect(img, 60, 72, 8, 26, WHITE)
        rect(img, 30, 60, 26, 8, WHITE)
        rect(img, 72, 60, 26, 8, WHITE)
    elif kind == "hotbar":
        for i in range(5):
            rect(img, 14 + i * 20, 52, 18, 24, (60, 64, 76, 255))
        rect(img, 52, 48, 24, 4, a)
        rect(img, 52, 76, 24, 4, a)
        rect(img, 50, 48, 4, 32, a)
        rect(img, 74, 48, 4, 32, a)
    else:
        for y in range(s):
            for x in range(s):
                r = ((x - 63.5) ** 2 + (y - 63.5) ** 2) ** 0.5
                if 28 <= r < 38:
                    img[y][x] = a
    return png(s, s, img)


# ---------------------------------------------------------------- packs dérivés du jeu installé

def low_fire():
    """Feu bas : chaque image de l'animation du feu descend de 7 pixels."""
    out = {}
    for name in ("fire_0.png", "fire_1.png"):
        data = read_game_texture("blocks.brarchive", name)
        if data is None:
            return None
        w, h, px = decode_png(data)
        shift = 7
        img = canvas(w, h)
        for frame in range(h // w):
            top = frame * w
            for y in range(w - shift):
                img[top + y + shift] = list(px[top + y])
        out["textures/blocks/" + name] = png(w, h, img)
    return out


def clear_glass():
    """Verre propre : bord conservé, intérieur uni (sans reflets) gardant la teinte du verre coloré."""
    names = ["glass.png"] + ["glass_" + c + ".png" for c in (
        "black", "blue", "brown", "cyan", "gray", "green", "light_blue", "lime", "magenta", "orange", "pink", "purple",
        "red", "silver", "white", "yellow")]
    out = {}
    for name in names:
        data = read_game_texture("blocks.brarchive", name)
        if data is None:
            continue
        w, h, px = decode_png(data)
        inner = [px[y][x] for y in range(1, h - 1) for x in range(1, w - 1)]
        n = len(inner)
        avg = tuple(sum(p[i] for p in inner) // n for i in range(4))
        fill = (0, 0, 0, 0) if name == "glass.png" else avg
        img = [[px[y][x] if x in (0, w - 1) or y in (0, h - 1) else fill for x in range(w)] for y in range(h)]
        out["textures/blocks/" + name] = png(w, h, img)
    return out or None


# ---------------------------------------------------------------- définition des packs

def packs():
    """(identifiant, nom, description, icône, fichiers, sous-packs) ; sous-pack = (dossier, nom, fichiers)."""
    result = [
        ("crosshair", "UMC • Réticule", "9 réticules au choix (engrenage du pack).", "crosshair",
         {"textures/ui/cross_hair.png": crosshair("cross")},
         [(f, n, {"textures/ui/cross_hair.png": crosshair(s)}) for f, n, s in CROSSHAIRS]),
        ("hotbar", "UMC • Hotbar sombre", "Hotbar translucide, numéros 1 à 9 en option (engrenage).", "hotbar",
         dict({"textures/ui/hotbar_start_cap.png": hotbar_cap(), "textures/ui/hotbar_end_cap.png": hotbar_cap()},
              **{f"textures/ui/hotbar_{i}.png": hotbar_slot(i, False) for i in range(9)}),
         [("sans_numeros", "Sans numéros", {}),
          ("numeros", "Avec numéros", {f"textures/ui/hotbar_{i}.png": hotbar_slot(i, True) for i in range(9)})]),
        ("selector", "UMC • Case sélectionnée", "Contour coloré de la case sélectionnée (couleur : engrenage).", "hotbar",
         {"textures/ui/selected_hotbar_slot.png": selected_slot(ACCENT)},
         [(f, n, {"textures/ui/selected_hotbar_slot.png": selected_slot(c)}) for f, n, c in SELECTOR_COLORS]),
        ("clear_pumpkin", "UMC • Citrouille transparente", "Retire le voile de la citrouille portée sur la tête.", "other",
         {"textures/misc/pumpkinblur.png": png(256, 256, canvas(256, 256))}, []),
        ("soft_glint", "UMC • Enchantement discret", "Reflet des objets enchantés plus doux.", "other",
         {"textures/misc/enchanted_item_glint.png": soft_glint(), "textures/misc/enchanted_actor_glint.png": soft_glint()}, []),
    ]
    fire = low_fire()
    if fire:
        result.append(("low_fire", "UMC • Feu bas", "Flammes plus basses (aussi à l'écran quand tu brûles).", "other", fire, []))
    glass = clear_glass()
    if glass:
        result.append(("clear_glass", "UMC • Verre propre", "Verre sans reflets, bord conservé.", "other", glass, []))
    return result


def manifest(pack_id, name, description, subpacks):
    m = {
        "format_version": 2,
        "header": {
            "name": name,
            "description": description + " Universal Minecraft Client.",
            "uuid": str(uuid.uuid5(NAMESPACE, pack_id + ":header")),
            "version": VERSION,
            "min_engine_version": [1, 21, 0],
        },
        "modules": [{"type": "resources", "uuid": str(uuid.uuid5(NAMESPACE, pack_id + ":module")), "version": VERSION}],
    }
    if subpacks:
        m["subpacks"] = [{"folder_name": f, "name": n, "memory_tier": 0} for f, n, _ in subpacks]
    return m


def addon_files():
    out = {}
    for pack_id, name, description, icon, files, subpacks in packs():
        folder = "UMC_" + pack_id
        out[f"{folder}/manifest.json"] = json.dumps(manifest(pack_id, name, description, subpacks), ensure_ascii=False,
                                                    indent=2).encode("utf-8")
        out[f"{folder}/pack_icon.png"] = pack_icon(icon)
        for path, data in files.items():
            out[f"{folder}/{path}"] = data
        for sub, _, sub_files in subpacks:
            out[f"{folder}/subpacks/{sub}/.keep"] = b""
            for path, data in sub_files.items():
                out[f"{folder}/subpacks/{sub}/{path}"] = data
    return out


# ---------------------------------------------------------------- vérification

EXPECTED = {"cross_hair.png": (16, 16), "selected_hotbar_slot.png": (24, 24), "hotbar_start_cap.png": (1, 22),
            "hotbar_end_cap.png": (1, 22), "pack_icon.png": (128, 128), "pumpkinblur.png": (256, 256),
            "enchanted_item_glint.png": (128, 128), "enchanted_actor_glint.png": (128, 128),
            "fire_0.png": (16, 512), "fire_1.png": (16, 512)}


def check(path):
    errors, uuids = [], set()
    with zipfile.ZipFile(path) as z:
        names = set(z.namelist())
        for mpath in sorted(n for n in names if n.endswith("/manifest.json")):
            folder = mpath[:-len("manifest.json")]
            m = json.loads(z.read(mpath).decode("utf-8"))
            for u in (m["header"]["uuid"], m["modules"][0]["uuid"]):
                if u in uuids:
                    errors.append(f"UUID en double : {u}")
                uuids.add(u)
            if folder + "pack_icon.png" not in names:
                errors.append(f"icône manquante : {folder}")
            for sp in m.get("subpacks", []):
                if not any(n.startswith(f"{folder}subpacks/{sp['folder_name']}/") for n in names):
                    errors.append(f"sous-pack vide : {folder}{sp['folder_name']}")
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
            want = (20, 22) if base.startswith("hotbar_") and base[7:8].isdigit() else EXPECTED.get(base)
            if base.startswith("glass"):
                want = (16, 16)
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
    target = out_dir / ("UMC-Tweaks-" + ".".join(map(str, VERSION)) + ".mcaddon")
    files = addon_files()
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in sorted(files.items()):
            z.writestr(name, data)
    count = sum(1 for n in files if n.endswith("/manifest.json"))
    print(f"Collection : {target} ({count} packs)")
    if not (GAME_PACKS / "vanilla").is_dir():
        print("Minecraft Bedrock introuvable : packs dérivés du jeu (feu bas, verre propre) omis.")
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
