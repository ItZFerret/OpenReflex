#!/usr/bin/env python3
"""montage.py OUT.png [--crop x0,y0,x1,y1] [--zoom Z] [--label] FILE... -- stacks the crops of the images vertically (or --cols N in a grid) with their names:
the before / after stills of the draw distance."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw
ap = argparse.ArgumentParser()
ap.add_argument("out"); ap.add_argument("files", nargs="+")
ap.add_argument("--crop", default="0,0,1,1"); ap.add_argument("--zoom", type=float, default=1.0); ap.add_argument("--cols", type=int, default=1)
ap.add_argument("--labels", default="", help="comma separated labels instead of the file names")
a = ap.parse_args()
x0, y0, x1, y1 = [float(v) for v in a.crop.split(",")]
labels = a.labels.split(",") if a.labels else [Path(f).stem for f in a.files]
tiles = []
for f, lab in zip(a.files, labels):
    im = Image.open(f).convert("RGB"); W, H = im.size
    im = im.crop((int(x0 * W), int(y0 * H), int(x1 * W), int(y1 * H)))
    if a.zoom != 1.0: im = im.resize((int(im.width * a.zoom), int(im.height * a.zoom)), Image.LANCZOS)
    d = ImageDraw.Draw(im); d.rectangle((0, 0, 8 + 7 * len(lab), 16), fill=(0, 0, 0)); d.text((4, 2), lab, fill=(255, 255, 255))
    tiles.append(im)
w, h = tiles[0].size; cols = max(1, a.cols); rows = (len(tiles) + cols - 1) // cols
sheet = Image.new("RGB", (w * cols, h * rows))
for i, t in enumerate(tiles): sheet.paste(t, ((i % cols) * w, (i // cols) * h))
sheet.save(a.out)
print(a.out, sheet.size)
