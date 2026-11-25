"""Contrast of the 303's palettes: WCAG 2 ratio and APCA Lc for each element against what lies next to it.

Use: python tools/contrast.py src/ui/Theme.cpp   (a ! marks a pair under its threshold)

WCAG 2: L = 0.2126 R + 0.7152 G + 0.0722 B on linearised sRGB; ratio (L1 + 0.05) / (L2 + 0.05).
Thresholds: text 4.5:1 (1.4.3), a control's or a graphic's edge against what is next to it 3:1 (1.4.11).
APCA (SAPC 0.0.98G-4g): |Lc| 15 is barely there, 30 the floor for large solid non-text, 45 fine detail, 60 small text.
"""
import re
import sys

src = open(sys.argv[1], encoding="utf-8").read()


def palettes():
    out = {}
    for name in ("graphite", "silver", "steel", "olive"):
        m = re.search(r"Palette " + name + r"\(\)\s*\{(.*?)\n        \}", src, re.S)
        body = m.group(1)
        p = dict(out["graphite"]) if name != "graphite" else {}
        for k, v in re.findall(r"p\.(\w+) = C\(0x([0-9a-fA-F]{8})\)", body):
            p[k] = int(v, 16) & 0xFFFFFF
        out[name] = p
    return out


def rgb(c):
    return ((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0


def mix(a, b, t=0.5):
    ra, rb = rgb(a), rgb(b)
    return sum(int(round((x + (y - x) * t) * 255)) << s for x, y, s in zip(ra, rb, (16, 8, 0)))


def lum(c):
    def lin(u):
        return u / 12.92 if u <= 0.04045 else ((u + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(u) for u in rgb(c))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def ratio(a, b):
    la, lb = sorted((lum(a), lum(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def apca(txt, bg):
    def ys(c):
        r, g, b = rgb(c)
        y = 0.2126729 * r ** 2.4 + 0.7151522 * g ** 2.4 + 0.0721750 * b ** 2.4
        return y + (0.022 - y) ** 1.414 if y < 0.022 else y
    yt, yb = ys(txt), ys(bg)
    if abs(yb - yt) < 0.0005:
        return 0.0
    if yb > yt:
        s = (yb ** 0.56 - yt ** 0.57) * 1.14
        return 0.0 if s < 0.1 else (s - 0.027) * 100
    s = (yb ** 0.65 - yt ** 0.62) * 1.14
    return 0.0 if s > -0.1 else (s + 0.027) * 100


P = palettes()


def pairs(p):
    plate = mix(p["plateTop"], p["plateBottom"])
    ground = mix(p["faceTop"], p["faceBottom"])
    screen = mix(p["screenTop"], p["screenBottom"])
    display = mix(p["displayTop"], p["displayBottom"])
    bar = mix(p["barTop"], p["barBottom"])
    cap = p["capMid"]
    cellOff = mix(p["cell"], p["cellOff"])
    lit = mix(p["litTop"], p["litBottom"])
    half = mix(p["halfTop"], p["halfBottom"])
    slotE = mix(p["slotEmptyTop"], p["slotEmptyBottom"])
    slotH = mix(p["slotHasTop"], p["slotHasBottom"])
    dialog = mix(p["dialogTop"], p["dialogBottom"])
    # (what, element, next to it, kind)  kind: ui = a control's face or edge, 3:1; text 4.5:1
    return [
        ("plate key's cap on its plate", cap, plate, "ui"),
        ("plate key's rim on its plate", p["capEdge"], plate, "ui"),
        ("top-bar key's cap on the bar", cap, bar, "ui"),
        ("key's cap on a screen (Locks, Mutate, +)", cap, screen, "ui"),
        ("grid's empty cell on the screen", cellOff, screen, "ui"),
        ("grid's lit cell on the screen", lit, screen, "ui"),
        ("grid's half-lit cell on the screen", half, screen, "ui"),
        ("empty pattern slot in its bay", slotE, screen, "ui"),
        ("filled pattern slot in its bay", slotH, screen, "ui"),
        ("display (well) on a plate", display, plate, "ui"),
        ("screen on its plate", screen, plate, "ui"),
        ("name strip / bar on the ground", p["stencil"], ground, "ui"),
        ("orange note on the screen", p["orange"], screen, "ui"),
        ("plate printing (label)", p["label"], plate, "text"),
        ("plate printing, dim", p["dim"], plate, "text"),
        ("key's ink on its cap", p["capInk"], cap, "text"),
        ("screen label", p["screenLabel"], screen, "text"),
        ("screen dim (Rest, KEY, MOVE)", p["screenDim"], screen, "text"),
        ("step numbers on the screen", p["gridNumber"], screen, "text"),
        ("Tie/Rest on an empty cell", p["gridOff"], cellOff, "text"),
        ("Up/Note on a lit cell", p["keyInk"], lit, "text"),
        ("slot number on an empty slot", p["slotInk"], slotE, "text"),
        ("amber on a display", p["amber"], display, "text"),
        ("Up/Down on a half-lit cell", p["gridHalf"], half, "text"),
        ("A/B, LOCKS, chain on an off key", p["slotInk"], cellOff, "text"),
        ("filled slot's number", p["slotLabel"], slotH, "text"),
        ("empty slot's number", p["slotDim"], slotE, "text"),
        ("edited slot's number", p["slotEdited"], slotH, "text"),
        ("chosen Options page on its key", p["keyInk"], p["key"], "text"),
        ("dialog label on its ground", p["screenLabel"], dialog, "text"),
        ("dialog dim on its ground", p["screenDim"], dialog, "text"),
    ]


names = list(P)
print(f"{'pair':44s} " + " ".join(f"{n:>14s}" for n in names))
print(f"{'':44s} " + " ".join(f"{'ratio   Lc':>14s}" for n in names))
rows = {n: pairs(P[n]) for n in names}
for i, (what, _, _, kind) in enumerate(rows["graphite"]):
    need = 3.0 if kind == "ui" else 4.5
    cells = []
    for n in names:
        _, a, b, _ = rows[n][i]
        r = ratio(a, b)
        lc = apca(a, b)
        flag = "!" if r < need else " "
        cells.append(f"{r:5.2f}{flag} {lc:6.1f}")
    print(f"{what + (' [3:1]' if kind == 'ui' else ' [4.5]'):44s} " + " ".join(f"{c:>14s}" for c in cells))
