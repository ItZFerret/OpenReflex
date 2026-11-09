#!/usr/bin/env python3
"""alivelook_run.py APP TAG LEVEL... [--looks alive,reflex] [--env K=V ...] -- the ALIVE LOOK comparison renders (w-alivereflex): each level in each
look through run.py (one player per level and look, -reflexTestWindow, never takes focus), on the Alive overlay builds/alive/combined/map-a5.
Output: builds/alivelook/runs/TAG/<look>-<level>/ (screenshots/), then the side-by-side sheets in builds/alivelook/TAG/."""
import argparse, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("app"); ap.add_argument("tag"); ap.add_argument("levels", nargs="+")
ap.add_argument("--looks", default="alive,reflex")
ap.add_argument("--env", action="append", default=[])
ap.add_argument("--donor", action="append", default=[], help="LEVEL=REFLEX_LEVEL: that level's donor for this run (REFLEX_ALIVE_DONOR)")
ap.add_argument("--alive-from", help="reuse the ALIVE renders of this earlier tag")
ap.add_argument("--game", default=str(R / "builds/alive/combined/map-a5"))
a = ap.parse_args()
plans = R / "builds/alivelook/plans"
subprocess.run([sys.executable, str(R / "tests/lighting_showcase/plan-alivelook.py"), str(plans)] + a.levels, check=True, stdout=subprocess.DEVNULL)
runs = R / "builds/alivelook/runs" / a.tag
for look in a.looks.split(","):
    for level in a.levels:
        out = runs / f"{look}-{level}"
        if (out / "RESULT.json").exists(): continue
        if out.exists(): subprocess.run(["rm", "-rf", str(out)], check=True)
        cmd = [sys.executable, str(R / "tests/lighting_showcase/run.py"), a.app, "--plan", str(plans / f"plan-{level}.json"), "--game", a.game,
               "--out", str(out), "--seconds", "600", "--env", f"REFLEX_ALIVE_LOOK={look}", "--env", "REFLEX_LOOK_DUMP=1"]
        for e in a.env: cmd += ["--env", e]
        for d in a.donor:
            if d.split("=", 1)[0] == level: cmd += ["--env", "REFLEX_ALIVE_DONOR=" + d.split("=", 1)[1]]
        print(look, level, flush=True)
        subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
alive_dir, reflex_dir = runs / "shots-alive", runs / "shots-reflex"
for look, d in (("alive", alive_dir), ("reflex", reflex_dir)):
    d.mkdir(parents=True, exist_ok=True)
    for level in a.levels:
        src = (R / "builds/alivelook/runs" / a.alive_from if look == "alive" and a.alive_from else runs) / f"{look}-{level}" / "screenshots"
        for p in src.glob("*.png"):
            (d / p.name).unlink(missing_ok=True); (d / p.name).symlink_to(p)
subprocess.run([sys.executable, str(R / "tests/lighting_showcase/alivelook_sheet.py"), str(alive_dir), str(reflex_dir), str(R / "builds/alivelook" / a.tag)] + a.levels)
