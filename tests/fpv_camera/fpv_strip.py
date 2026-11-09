#!/usr/bin/env python3
"""fpv_strip.py OUT_PREFIX RUN [RUN2] [--fps N] [--labels A,B] -- the recorded Dynamic FPV frames of FpvRideProbe (REFLEX_FPV_REC) as a
video and a contact sheet (test only, w-fpvrewrite). One run: OUT_PREFIX.mp4 and OUT_PREFIX_sheet.jpg. Two runs (before / after): side by
side, each labelled. Uses the installed ffmpeg (libx264) and PIL."""
import argparse, subprocess, tempfile
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ap = argparse.ArgumentParser()
ap.add_argument("out"); ap.add_argument("runs", nargs="+", type=Path)
ap.add_argument("--fps", type=int, default=30); ap.add_argument("--labels", default="")
ap.add_argument("--sheet-every", type=int, default=12)
a = ap.parse_args()
labels = a.labels.split(",") if a.labels else [r.name.split("Z-", 1)[-1] for r in a.runs]
frames = [sorted((r / "screenshots").glob("rec_*.jpg")) or sorted((r / "screenshots").glob("rec_*.png")) for r in a.runs]
n = min(len(f) for f in frames)
assert n > 0, "no recorded frames"
try: font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 22)
except Exception: font = ImageFont.load_default()
tmp = Path(tempfile.mkdtemp(prefix="fpvstrip-"))
sheet = []
for i in range(n):
    ims = [Image.open(f[i]).convert("RGB") for f in frames]
    W, H = ims[0].size
    out = Image.new("RGB", (W * len(ims) + 8 * (len(ims) - 1), H), "black")
    for k, im in enumerate(ims):
        out.paste(im, (k * (W + 8), 0)); g = ImageDraw.Draw(out)
        g.rectangle([k * (W + 8) + 8, 8, k * (W + 8) + 16 + g.textlength(labels[k], font=font), 38], fill=(0, 0, 0))
        g.text((k * (W + 8) + 12, 10), labels[k], fill="white", font=font)
    out.save(tmp / f"f_{i:05d}.png")
    if i % a.sheet_every == 0: sheet.append(out)
subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(a.fps), "-i", str(tmp / "f_%05d.png"), "-c:v", "libx264", "-pix_fmt", "yuv420p",
                "-crf", "20", "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2", a.out + ".mp4"], check=True)
cols = 1 if len(a.runs) > 1 else 3
W, H = sheet[0].size; rows = (len(sheet) + cols - 1) // cols
scale = 0.5
sw, sh = int(W * scale), int(H * scale)
cs = Image.new("RGB", (sw * cols, sh * rows), "white")
for i, im in enumerate(sheet): cs.paste(im.resize((sw, sh)), ((i % cols) * sw, (i // cols) * sh))
cs.save(a.out + "_sheet.jpg", quality=85)
for p in tmp.iterdir(): p.unlink()
tmp.rmdir()
print(a.out + ".mp4", n, "frames;", a.out + "_sheet.jpg")
