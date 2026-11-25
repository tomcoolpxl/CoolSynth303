"""Builds icon-<303|808>.ico from the rendered PNGs, every size its own drawing (not scaled from the largest),
and checks it reads back with all of them."""
import os

from PIL import Image

here = os.path.dirname(os.path.abspath(__file__))
png = os.path.join(here, "png")
sizes = [256, 128, 96, 64, 48, 40, 32, 24, 20, 16]
for icon in ("303", "808"):
    images = [Image.open(os.path.join(png, f"icon-{icon}-{s}.png")).convert("RGBA") for s in sizes]
    out = os.path.join(here, f"icon-{icon}.ico")
    images[0].save(out, format="ICO", sizes=[(s, s) for s in sizes], append_images=images[1:])
    back = Image.open(out)
    print(icon, os.path.getsize(out), "bytes, sizes", sorted(back.info.get("sizes", [])))
