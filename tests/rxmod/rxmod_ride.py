#!/usr/bin/env python3
"""rxmod_ride.py <Showcase.app> --model ID --mod MODID --mods <Mods folder> [--name N] [--env K=V ...] [--seconds N] [--window]
-- a .rxmod bike (core/plan/RXMOD.md) ridden in a race with AI (test only).

Runs a COPY of a showcase test player (tests/menus_showcase/build.py: the fixture RxModRideProbe.cs is in it) as tests/rider_grips/grip_ride.py
does: a unique bundle id, a sandbox denying writes to the user's preferences, application support and the game files, a scratch save
(REFLEX_SAVE_DIR) seeded MUTED (MASTER VOLUME 0) with the bike mod chosen for the model ("BikeModel.Model.<model>": <mod>), the Mods folder linked
into the run's client root (read only), -batchmode (no window at all: it never takes the focus; the probe renders its shots offscreen) unless
--window, and nofocus.hand_back. --mod none: no mod chosen (the probe then expects the original bike: REFLEX_RXMOD_EXPECT=none).
Output: builds/rxmod/runs/<UTC stamp>-<name>/ (player.log, screenshots/, save/)."""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--model", type=int, required=True)
ap.add_argument("--mod", required=True, help="the mod id (its path under Mods/Bikes, e.g. 'NFG COLE/NFG COLE - 2023 Yamaha YZ-F.rxmod') or none")
ap.add_argument("--mods", type=Path, required=True, help="the Mods folder (Mods/Bikes ...) linked into the run's client root")
ap.add_argument("--name", default="run")
ap.add_argument("--seconds", type=int, default=900)
ap.add_argument("--width", type=int, default=1600)
ap.add_argument("--height", type=int, default=900)
ap.add_argument("--game", type=Path, default=Path("/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"))
ap.add_argument("--env", action="append", default=[])
ap.add_argument("--window", action="store_true", help="a window instead of -batchmode (it still never keeps the focus)")
ap.add_argument("--out", type=Path)
a = ap.parse_args()
A = (a.out.resolve() if a.out else R / "builds/rxmod/runs") / (datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + a.name)
A.mkdir(parents=True)
app = A / "RxModRide.app"
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
info.update(CFBundleIdentifier="local.reflex.rxmod.test-" + A.name.lower(), CFBundleName="RxMod Ride Test", NSAppSleepDisabled=True, LSAppNapIsDisabled=True)
(app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
save = A / "save"; save.mkdir(); shots = A / "screenshots"; shots.mkdir()
seed = {"format": "reflex-client-save", "version": 1, "ints": {}, "floats": {"Options.MasterVolume": 0.0}, "strings": {}}
if a.mod != "none": seed["strings"]["BikeModel.Model.%d" % a.model] = a.mod
(save / "save.json").write_text(json.dumps(seed, indent=1))
(A / "Mods").symlink_to(a.mods.resolve(), target_is_directory=True)
home = Path.home(); prefs = home / "Library/Preferences"; support = home / "Library/Application Support"
sb = "(version 1)\n(allow default)\n"
for d in sorted({shipped, "unity.DefaultCompany.Reflex", "unity.DefaultCompany.OpenReflex"}):
    sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(support / d))
    for f in [prefs, prefs / "ByHost"]:
        sb += "(deny file-write* (regex %s))\n" % json.dumps("^" + re.escape(str(f / d)) + r"(\..*)?$")
for n in ["Reflex", "OpenReflex", "Reflex Core Garage Bench"]:
    sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(support / "DefaultCompany" / n))
sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(a.game))
sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(a.mods.resolve()))   # the shared Mods folder: read only
(A / "protection.sb").write_text(sb)
subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True, capture_output=True)
args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0",
        "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(A / "player.log")]
if not a.window: args.insert(4, "-batchmode")
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(a.game), REFLEX_CLIENT_ROOT=str(A),
           REFLEX_RXMOD_PROBE="1", REFLEX_RXMOD_MODEL=str(a.model), REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_FPS_CAP="60", REFLEX_PACING="interpolated",
           REFLEX_MENU3D="0", REFLEX_MUTE="1", REFLEX_SHOT_SIZE=f"{a.width}x{a.height}")
if a.mod == "none": env["REFLEX_RXMOD_EXPECT"] = "none"
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
    if "[RxModRide]" in l or "Exception" in l or "Reflex bike " in l or "[RxMod]" in l: print(l[:700])
ok = code == 0 and not timedout and "[RxModRide] PASS" in text
print("exit", code, "timedout", timedout, "shots", sorted(p.name for p in shots.glob("*.png")))
shutil.rmtree(app, ignore_errors=True)   # (the player copy: the disk)
print("DONE" if ok else "FAIL", A)
sys.exit(0 if ok else 1)
