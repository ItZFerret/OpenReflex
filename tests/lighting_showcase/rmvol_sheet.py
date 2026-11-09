#!/usr/bin/env python3
"""rmvol_sheet.py OUT_DIR RUN_DIR... -- side-by-side sheets of the VOLUMETRIC LIGHT comparison (w-rmvol): for every view shot in both modes,
the original's fake shafts (enhanced-novol, left) and the volumetric light (enhanced, right) at half size, labelled. Path frames: f00 and
the last frame only. Writes OUT_DIR/<prefix>_<view>.jpg."""
import re, sys
from pathlib import Path
from PIL import Image, ImageDraw

out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
rx = re.compile(r"^(.*)_([a-z0-9]+)_(enhanced-novol|enhanced|original)(_f\d\d)?\.png$")
for run in sys.argv[2:]:
    shots = {}
    for p in sorted(Path(run, "screenshots").glob("*.png")):
        m = rx.match(p.name)
        if not m: continue
        frame = m.group(4) or ""
        if frame and frame not in ("_f00", "_f15"): continue
        shots.setdefault((m.group(1), m.group(2) + frame), {})[m.group(3)] = p
    for (prefix, view), modes in sorted(shots.items()):
        if "enhanced" not in modes or "enhanced-novol" not in modes: continue
        a = Image.open(modes["enhanced-novol"]).convert("RGB"); b = Image.open(modes["enhanced"]).convert("RGB")
        w, h = a.size[0] // 2, a.size[1] // 2
        sheet = Image.new("RGB", (w * 2 + 8, h + 34), (16, 16, 16))
        sheet.paste(a.resize((w, h), Image.LANCZOS), (0, 34)); sheet.paste(b.resize((w, h), Image.LANCZOS), (w + 8, 34))
        d = ImageDraw.Draw(sheet)
        d.text((8, 10), f"{prefix} {view}: ORIGINAL FAKE SHAFTS (ENHANCED, volumetric off)", fill=(170, 200, 255))
        d.text((w + 16, 10), "VOLUMETRIC LIGHT", fill=(255, 210, 120))
        sheet.save(out / f"{prefix}_{view}.jpg", quality=90)
        print(out / f"{prefix}_{view}.jpg")
