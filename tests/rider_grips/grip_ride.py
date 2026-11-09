#!/usr/bin/env python3
"""grip_ride.py <Showcase.app> [--name N] [--env K=V ...] [--seconds N] -- the rider's hands on the grips in a real ride (test only, w-grips).

Runs a COPY of a showcase test player (tests/menus_showcase/build.py: the fixture GripRideProbe.cs is in it) as tests/leaderboards/ghost_ride.py
does: a unique bundle id, a sandbox denying writes to the user's preferences, application support and the game files, a scratch save
(REFLEX_SAVE_DIR), a frame cap so the background window keeps rendering, and nofocus.hand_back (the window never keeps the focus). The probe
rides REFLEX_GRIP_EVENT (National NAT_Concord) on the profile's bike or REFLEX_GRIP_GARAGE, measures the hands against the grips (GRIP IK off
/ on), shoots the Dynamic FPV view and close-ups and tries tricks. Output: builds/grips/runs/<UTC stamp>-<name>/ (player.log, screenshots/
with grips.csv, save/).
w-gripwidth: --batch runs the player with -batchmode (no window at all; the probe renders its shots offscreen), --out picks the run
directory, --save-json seeds the scratch save (e.g. a bike mod choice "BikeModel.Model.<VehicleModelId>": "<mod id>") and --mods links a
Mods folder (Mods/Bikes ...) into the run's client root (read only)."""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--name", default="run")
ap.add_argument("--seconds", type=int, default=900)
ap.add_argument("--width", type=int, default=1600)
ap.add_argument("--height", type=int, default=900)
ap.add_argument("--game", type=Path, default=Path("/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"))
ap.add_argument("--env", action="append", default=[], help="K=V passed to the player (REFLEX_GRIP_GARAGE=4,3,-1,-1 ...)")
ap.add_argument("--batch", action="store_true", help="-batchmode: no window; the probe renders the shots offscreen")
ap.add_argument("--out", type=Path, help="the runs directory (default builds/grips/runs)")
ap.add_argument("--save-json", type=Path, help="a save.json to seed the scratch save with")
ap.add_argument("--mods", type=Path, help="a Mods folder to link into the run's client root")
a = ap.parse_args()
A = (a.out.resolve() if a.out else R / "builds/grips/runs") / (datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + a.name)
A.mkdir(parents=True)
app = A / "GripRide.app"
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
info.update(CFBundleIdentifier="local.reflex.grips.test-" + A.name.lower(), CFBundleName="Grip Ride Test")
(app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
save = A / "save"; save.mkdir(); shots = A / "screenshots"; shots.mkdir()
if a.save_json: shutil.copy(a.save_json, save / "save.json")
if a.mods: (A / "Mods").symlink_to(a.mods.resolve(), target_is_directory=True)
home = Path.home(); prefs = home / "Library/Preferences"; support = home / "Library/Application Support"
sb = "(version 1)\n(allow default)\n"
for d in sorted({shipped, "unity.DefaultCompany.Reflex", "unity.DefaultCompany.OpenReflex"}):
    sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(support / d))
    for f in [prefs, prefs / "ByHost"]:
        sb += "(deny file-write* (regex %s))\n" % json.dumps("^" + re.escape(str(f / d)) + r"(\..*)?$")
for n in ["Reflex", "OpenReflex", "Reflex Core Garage Bench"]:
    sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(support / "DefaultCompany" / n))
sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(a.game))
(A / "protection.sb").write_text(sb)
subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True, capture_output=True)
args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0",
        "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(A / "player.log")]
if a.batch: args.insert(4, "-batchmode")
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(a.game), REFLEX_CLIENT_ROOT=str(A),
           REFLEX_GRIP_PROBE="1", REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_FPS_CAP="60", REFLEX_PACING="interpolated", REFLEX_MENU3D="0")
if a.batch: env["REFLEX_SHOT_SIZE"] = f"{a.width}x{a.height}"
for kv in a.env: k, _, v = kv.partition("="); env[k] = v
timedout = False
with (A / "console.log").open("w") as log:
    _front = nofocus.frontmost()
    child = subprocess.Popen(args, cwd=A, env=env, stdout=log, stderr=subprocess.STDOUT)
    nofocus.hand_back(_front)
    print(A, "pid", child.pid, flush=True)
    try: code = child.wait(timeout=a.seconds)
    except subprocess.TimeoutExpired:
        timedout = True; child.terminate()
        try: code = child.wait(timeout=10)
        except subprocess.TimeoutExpired: child.kill(); code = child.wait()
text = (A / "player.log").read_text(errors="replace") if (A / "player.log").exists() else ""
for l in text.splitlines():
    if "[GripProbe]" in l or "Exception" in l: print(l[:600])
ok = code == 0 and not timedout and "[GripProbe] DONE" in text
print("exit", code, "timedout", timedout, "shots", sorted(p.name for p in shots.glob("*.png")))
shutil.rmtree(app, ignore_errors=True)   # (the player copy: the disk)
print("DONE" if ok else "FAIL", A)
sys.exit(0 if ok else 1)
