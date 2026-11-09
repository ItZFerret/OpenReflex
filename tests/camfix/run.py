#!/usr/bin/env python3
"""run.py APP TAG PLAN... [--env K=V ...] -- the driving camera renders (w-camfix): each plan (tests/camfix/plan-<name>.json) through
tests/lighting_showcase/run.py --batch (a -batchmode player: no window, never takes focus; the fixture renders the camera into a target), on the
player's game files (read only). The plans select the starter custom camera CLOSE CHASE ("camera": "custom:1"), shoot the start gate (Countdown),
the gate drop and the race in ORIGINAL and ENHANCED, the near-plane pairs (2 ft = the game's / 0.25 ft = now) and write <prefix>_camlog.csv (the
custom camera's weight, the game camera's / the held distance and the drawn camera's distance from the bike every frame).
Output: REFLEX-lead builds/camfix/runs/TAG/<plan>/screenshots; the run's copy of the app is removed afterwards (disk)."""
import argparse, shutil, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
LEAD = Path("/Users/payton/Documents/Development/REFLEX-lead")
ap = argparse.ArgumentParser()
ap.add_argument("app"); ap.add_argument("tag"); ap.add_argument("plans", nargs="+")
ap.add_argument("--env", action="append", default=[])
ap.add_argument("--out", default=str(LEAD / "builds/camfix/runs"))
a = ap.parse_args()
for name in a.plans:
    out = Path(a.out) / a.tag / name
    if out.exists(): shutil.rmtree(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [sys.executable, str(R / "tests/lighting_showcase/run.py"), a.app, "--batch", "--plan", str(R / f"tests/camfix/plan-{name}.json"),
           "--out", str(out), "--seconds", "420", "--width", "1280", "--height", "720"]
    for e in a.env: cmd += ["--env", e]
    print(a.tag, name, flush=True)
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    shutil.rmtree(out / "Lighting.app", ignore_errors=True)
    shots = sorted(p.name for p in (out / "screenshots").glob("*.png"))
    print("  ", len(shots), "shots", flush=True)
