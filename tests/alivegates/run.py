#!/usr/bin/env python3
"""run.py APP TAG PLAN... [--game DIR] [--env K=V ...] -- the Alive start gate renders (w-alivegates): each plan (tests/alivegates/plan-<name>.json)
through tests/lighting_showcase/run.py --batch (a -batchmode player: no window, muted, never takes focus; the fixture renders the camera into a
target; MASTER VOLUME 0 in its scratch save), on an Alive overlay with the race levels (default REFLEX-lead builds/alive/combined/map-a7-fresno, read only). Each plan shoots the gate of
an Alive race at its default MAP SCALE from in front of the bar and along it: in the Countdown (the bar up), 0.1 s after GO (dropping), 0.45 s
and 0.6 s after GO (down, the riders leaving). Output: REFLEX-lead builds/alivegates/runs/TAG/<plan>/screenshots; the run's copy of the app is
removed afterwards (disk)."""
import argparse, os, shutil, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
LEAD = Path("/Users/payton/Documents/Development/REFLEX-lead")
ap = argparse.ArgumentParser()
ap.add_argument("app"); ap.add_argument("tag"); ap.add_argument("plans", nargs="+")
ap.add_argument("--game", default=str(LEAD / "builds/alive/combined/map-a7-fresno"))
ap.add_argument("--env", action="append", default=[])
ap.add_argument("--out", default=str(LEAD / "builds/alivegates/runs"))
a = ap.parse_args()
for name in a.plans:
    out = Path(a.out) / a.tag / name
    if out.exists(): shutil.rmtree(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [sys.executable, str(R / "tests/lighting_showcase/run.py"), a.app, "--batch", "--plan", str(R / f"tests/alivegates/plan-{name}.json"),
           "--game", a.game, "--out", str(out), "--seconds", "420", "--width", "1280", "--height", "720"]
    for e in a.env: cmd += ["--env", e]
    print(a.tag, name, flush=True)
    # muted: the scratch save starts with MASTER VOLUME 0 (tests/alivegates/muted-save.json; never the user's save)
    env = dict(os.environ, SHOWCASE_SAVE_JSON=str(R / "tests/alivegates/muted-save.json"))
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    shutil.rmtree(out / "Lighting.app", ignore_errors=True)
    shots = sorted(p.name for p in (out / "screenshots").glob("*.png"))
    print("  ", len(shots), "shots", flush=True)
