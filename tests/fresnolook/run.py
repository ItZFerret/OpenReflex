#!/usr/bin/env python3
"""run.py APP TAG PLAN... [--env K=V ...] [--game DIR] -- the RESTORED LOOK renders (w-fresnolook): each plan (tests/fresnolook/plan-<name>.json)
through tests/lighting_showcase/run.py --batch (a -batchmode player: no window, never takes focus; the fixture renders the camera into a
target), on the player's game files with the repaired Fresno B (REFLEX-lead builds/alive/combined/map-a7-fresno, read only).
Output: REFLEX-lead builds/fresnolook/runs/TAG/<plan>/screenshots; the run's copy of the app is removed afterwards (disk).
TAG "before" sets REFLEX_RESTORED_LOOK=off (the levels as shipped)."""
import argparse, shutil, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
LEAD = Path("/Users/payton/Documents/Development/REFLEX-lead")
ap = argparse.ArgumentParser()
ap.add_argument("app"); ap.add_argument("tag"); ap.add_argument("plans", nargs="+")
ap.add_argument("--env", action="append", default=[])
ap.add_argument("--game", default=str(LEAD / "builds/alive/combined/map-a7-fresno"))
ap.add_argument("--out", default=str(LEAD / "builds/fresnolook/runs"))
a = ap.parse_args()
for name in a.plans:
    out = Path(a.out) / a.tag / name
    if (out / "RESULT.json").exists(): print("done", out); continue
    if out.exists(): shutil.rmtree(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [sys.executable, str(R / "tests/lighting_showcase/run.py"), a.app, "--batch", "--plan", str(R / f"tests/fresnolook/plan-{name}.json"),
           "--game", a.game, "--out", str(out), "--seconds", "420", "--width", "1600", "--height", "900"]
    if a.tag.startswith("before"): cmd += ["--env", "REFLEX_RESTORED_LOOK=off"]
    for e in a.env: cmd += ["--env", e]
    print(a.tag, name, flush=True)
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    shutil.rmtree(out / "Lighting.app", ignore_errors=True)
    shots = sorted(p.name for p in (out / "screenshots").glob("*.png"))
    print("  ", len(shots), "shots", flush=True)
