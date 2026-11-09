#!/usr/bin/env python3
"""sheet.py RUNS OUT LEVEL DONOR [--before TAG] [--after TAG] -- the RESTORED LOOK contact sheets (w-fresnolook): per mode (original / enhanced)
one image, a row per view of LEVEL (chase, gate, wide, overview, rhythm, top), BEFORE on the left and AFTER on the right, and a last row
with the DONOR level's own chase and wide views (the reference). RUNS: builds/fresnolook/runs (TAG/<plan>/screenshots/<plan>_<view>_<mode>.png)."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw

ap = argparse.ArgumentParser()
ap.add_argument("runs"); ap.add_argument("out"); ap.add_argument("level"); ap.add_argument("donor")
ap.add_argument("--before", default="before"); ap.add_argument("--after", default="after")
ap.add_argument("--views", default="chase,gate,wide,overview,rhythm,top,stands")
a = ap.parse_args()
runs, out = Path(a.runs), Path(a.out); out.mkdir(parents=True, exist_ok=True)
W = 800


def tile(path, label):
    im = Image.open(path).convert("RGB") if path.exists() else Image.new("RGB", (1600, 900), (60, 0, 60))
    im = im.resize((W, int(im.height * W / im.width)), Image.LANCZOS)
    d = ImageDraw.Draw(im); d.rectangle([0, 0, 8 + 6 * len(label), 16], fill=(0, 0, 0)); d.text((4, 3), label, fill=(255, 255, 255))
    return im


for mode in ("original", "enhanced"):
    rows = []
    for v in a.views.split(","):
        rows.append([tile(runs / tag / a.level / "screenshots" / f"{a.level}_{v}_{mode}.png", f"{a.level} {v} {mode} {tag.upper()}")
                     for tag in (a.before, a.after)])
    after_donor = a.after if (runs / a.after / a.donor).exists() else a.before
    rows.append([tile(runs / after_donor / a.donor / "screenshots" / f"{a.donor}_{v}_{mode}.png", f"DONOR {a.donor} {v} {mode} (shipped)")
                 for v in ("chase", "wide")])
    h = rows[0][0].height
    sheet = Image.new("RGB", (2 * W + 6, len(rows) * (h + 6)), (16, 16, 16))
    for r, (x, y) in enumerate(rows):
        sheet.paste(x, (0, r * (h + 6))); sheet.paste(y, (W + 6, r * (h + 6)))
    p = out / f"{a.level}_{mode}.jpg"
    sheet.save(p, quality=88)
    print(p)
