#!/usr/bin/env python3
"""fbhud_ride.py <Showcase.app> [--script all|persist] [--seed-dir DIR] [--name N] [--env K=V ...] [--seconds N] [--batch] -- w-fb-hud's four
playtester items in the real flow (test only): the fixture FbHudProbe.cs of a showcase test player (tests/menus_showcase/build.py).

Runs a COPY of the player as tests/rider_grips/grip_ride.py does: a unique bundle id, a sandbox denying writes to the user's preferences,
application support and the game files, a scratch save (REFLEX_SAVE_DIR), muted (the probe holds the listener at 0), nofocus.hand_back; --batch:
-batchmode (no window). --script all (run 1: the hub's and the pause page's CONTROLS, REV GAUGE in a ride, RESET TRACK, the music bag) or
persist (run 2: --seed-dir = run 1's save/ folder: the bindings and REV GAUGE after a restart). Output: builds/fbhud/runs/<UTC stamp>-<name>/
(player.log, save/)."""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--name", default="run")
ap.add_argument("--script", default="all")
ap.add_argument("--seed-dir", type=Path, help="a save folder to copy into the scratch save (run 1's save/)")
ap.add_argument("--seconds", type=int, default=900)
ap.add_argument("--width", type=int, default=1600)
ap.add_argument("--height", type=int, default=900)
ap.add_argument("--game", type=Path, default=Path("/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"))
ap.add_argument("--env", action="append", default=[], help="K=V passed to the player")
ap.add_argument("--batch", action="store_true", help="-batchmode: no window")
ap.add_argument("--out", type=Path, help="the runs directory (default builds/fbhud/runs)")
a = ap.parse_args()
A = (a.out.resolve() if a.out else R / "builds/fbhud/runs") / (datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + a.name)
A.mkdir(parents=True)
app = A / "FbHud.app"
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
info.update(CFBundleIdentifier="local.reflex.fbhud.test-" + A.name.lower(), CFBundleName="FB HUD Test")
(app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
save = A / "save"; save.mkdir(); shots = A / "screenshots"; shots.mkdir()
if a.seed_dir:
    for f in sorted(a.seed_dir.iterdir()):
        if f.is_file() and f.suffix in (".json", ".bin"): shutil.copy2(f, save / f.name)
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
           REFLEX_FBHUD_PROBE="1", REFLEX_FBHUD_SCRIPT=a.script, REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_FPS_CAP="60", REFLEX_PACING="interpolated", REFLEX_MENU3D="0")
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
    if "[FbHudProbe]" in l or "[TrackReset]" in l or "[Controls]" in l or "[Music]" in l or "Exception" in l: print(l[:600])
ok = code == 0 and not timedout and "[FbHudProbe] RESULT PASS" in text
print("exit", code, "timedout", timedout, "shots", sorted(p.name for p in shots.glob("*.png")))
shutil.rmtree(app, ignore_errors=True)   # (the player copy: the disk)
print("DONE" if ok else "FAIL", A)
sys.exit(0 if ok else 1)
