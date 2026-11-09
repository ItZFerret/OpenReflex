#!/usr/bin/env python3
"""ab_table.py <ab run dir> [--md OUT] -- the before / after table of a bench.py ab run: per scene and window the offscreen frame time
(the camera rendered back to back + one GPU readback: max(CPU, GPU) without the window server) of each variant, its main-thread share,
the change; the ablations as costs (window minus the full frame of the same mode); the live windows' simulation job / core frame."""
import argparse, json
from pathlib import Path

ap = argparse.ArgumentParser(); ap.add_argument("run", type=Path); ap.add_argument("--md", type=Path)
a = ap.parse_args()
variants = {}
for d in sorted(a.run.glob("*-r*")):
    label = d.name.rsplit("-r", 1)[0]
    for p in d.glob("*_perf.json"):
        variants.setdefault(label, {}).setdefault(p.stem.replace("_perf", ""), []).append(json.loads(p.read_text()))
labels = sorted(variants, key=lambda l: (l != "base", l))
scenes = sorted({s for v in variants.values() for s in v}, key=lambda s: ["national", "supercross", "alpine", "menu", "quarry", "dunes", "sx01"].index(s) if s in ["national", "supercross", "alpine", "menu", "quarry", "dunes", "sx01"] else 99)

def windows(label, scene):
    out = {}
    for rep in variants.get(label, {}).get(scene, []):
        for w in rep["windows"]:
            key = (w["label"], w.get("variant", ""))
            out.setdefault(key, []).append(w)
    return out

def mean(ws, k):
    xs = [w.get(k, 0) for w in ws if w.get(k, 0)]
    return sum(xs) / len(xs) if xs else 0

lines = []
for scene in scenes:
    lines.append(f"### {scene}\n")
    lines.append("| window | " + " | ".join(f"{l} ms (main)" for l in labels) + " | change |")
    lines.append("|---" * (2 + len(labels)) + "|")
    W = {l: windows(l, scene) for l in labels}
    keys = [k for k in W[labels[0]] if k[1] in ("",) or k[1].startswith("-")]
    for k in keys:
        vals = [mean(W[l].get(k, []), "offscreenMs") for l in labels]
        mains = [mean(W[l].get(k, []), "offscreenCpuMs") for l in labels]
        if not any(vals): continue
        ch = f"{(vals[-1] / vals[0] - 1) * 100:+.0f} %" if vals[0] and vals[-1] else "-"
        lines.append(f"| {k[0]} {k[1]} | " + " | ".join(f"{v:.2f} ({m:.2f})" for v, m in zip(vals, mains)) + f" | {ch} |")
    live = [k for k in W[labels[0]] if k[1] == "live"]
    if live:
        lines.append("\nlive (the world running, batch mode, 60 fps pacing): sim job without the frame-cap wait / core frame, ms per game frame\n")
        lines.append("| window | " + " | ".join(f"{l} job / core" for l in labels) + " |")
        lines.append("|---" * (1 + len(labels)) + "|")
        for k in live:
            lines.append(f"| {k[0]} | " + " | ".join(f"{mean(W[l].get(k, []), 'simJobMs'):.2f} / {mean(W[l].get(k, []), 'simFrameMs'):.2f}" for l in labels) + " |")
    lines.append("")
text = "\n".join(lines)
print(text)
if a.md: a.md.write_text(text + "\n")
