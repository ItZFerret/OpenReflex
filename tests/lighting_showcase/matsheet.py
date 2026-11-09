#!/usr/bin/env python3
"""matsheet.py <screenshots dir> <prefix> <out dir> [--views a,b] [--modes m1,m2] [--cols N] -- comparison sheets of the MATERIALS: ENHANCED showcase.

For each view one sheet <out>/<prefix>_<view>_sheet.png: the shots of the given modes (default enhanced-matorig, enhanced-matenh, enhanced-matenh-dirt35,
enhanced-matenh-dirt70) side by side, each labelled, scaled to a common width. PIL only."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ap = argparse.ArgumentParser()
ap.add_argument("shots", type=Path)
ap.add_argument("prefix")
ap.add_argument("out", type=Path)
ap.add_argument("--views", default="front34,rear34,low,side,rider,rearlow")
ap.add_argument("--modes", default="enhanced-matorig,enhanced-matenh,enhanced-matenh-dirt35,enhanced-matenh-dirt70")
ap.add_argument("--cols", type=int, default=2)
ap.add_argument("--width", type=int, default=960)
a = ap.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
try:
    font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 28)
except OSError:
    font = ImageFont.load_default()
LABEL = {"enhanced-matorig": "MATERIALS: ORIGINAL", "enhanced-matenh": "MATERIALS: ENHANCED", "enhanced-matenh-dirt35": "ENHANCED + dirt 35%",
         "enhanced-matenh-dirt70": "ENHANCED + dirt 70%", "enhanced-matenh-dirt70-wet": "ENHANCED + dirt 70% wet"}
for view in a.views.split(","):
    tiles = []
    for m in a.modes.split(","):
        p = a.shots / f"{a.prefix}_{view}_{m}.png"
        if not p.exists(): continue
        im = Image.open(p).convert("RGB")
        im = im.resize((a.width, int(im.height * a.width / im.width)), Image.LANCZOS)
        d = ImageDraw.Draw(im)
        t = LABEL.get(m, m)
        d.rectangle([0, 0, 10 + d.textlength(t, font=font) + 10, 40], fill=(0, 0, 0))
        d.text((10, 4), t, fill=(255, 220, 110), font=font)
        tiles.append(im)
    if not tiles: continue
    cols = min(a.cols, len(tiles)); rows = (len(tiles) + cols - 1) // cols
    w, h = tiles[0].size
    sheet = Image.new("RGB", (cols * w, rows * h))
    for i, t in enumerate(tiles): sheet.paste(t, ((i % cols) * w, (i // cols) * h))
    out = a.out / f"{a.prefix}_{view}_sheet.png"
    sheet.save(out)
    print(out)
