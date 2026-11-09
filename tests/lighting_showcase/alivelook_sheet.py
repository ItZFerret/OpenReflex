#!/usr/bin/env python3
"""alivelook_sheet.py ALIVE_SHOTS REFLEX_SHOTS OUT_DIR LEVEL... -- side-by-side contact sheets of the ALIVE LOOK comparison (w-alivereflex):
one image per level, a row per view, ALIVE on the left and REFLEX on the right (the run.py screenshots <level>_<view>_original.png)."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

alive, reflex, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
out.mkdir(parents=True, exist_ok=True)
W = 960
for level in sys.argv[4:]:
    views = sorted({p.name[len(level) + 1:-len("_original.png")] for p in alive.glob(level + "_*_original.png")} |
                   {p.name[len(level) + 1:-len("_original.png")] for p in reflex.glob(level + "_*_original.png")},
                   key=lambda v: ["chase", "ground", "wide", "sun"].index(v) if v in ["chase", "ground", "wide", "sun"] else 99)
    if not views:
        print("no shots for", level); continue
    rows = []
    for v in views:
        pair = []
        for d, label in ((alive, "ALIVE"), (reflex, "REFLEX")):
            f = d / f"{level}_{v}_original.png"
            im = Image.open(f).convert("RGB") if f.exists() else Image.new("RGB", (1920, 1080), (60, 0, 60))
            im = im.resize((W, int(im.height * W / im.width)), Image.LANCZOS)
            dr = ImageDraw.Draw(im); dr.rectangle([0, 0, 230, 18], fill=(0, 0, 0)); dr.text((4, 3), f"{level} {v}  {label}", fill=(255, 255, 255))
            pair.append(im)
        rows.append(pair)
    h = rows[0][0].height
    sheet = Image.new("RGB", (2 * W + 6, len(rows) * (h + 6)), (16, 16, 16))
    for r, (a, b) in enumerate(rows):
        sheet.paste(a, (0, r * (h + 6))); sheet.paste(b, (W + 6, r * (h + 6)))
    p = out / f"{level}_alive_vs_reflex.jpg"
    sheet.save(p, quality=90)
    print(p)
