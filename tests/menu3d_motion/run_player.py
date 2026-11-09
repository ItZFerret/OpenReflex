#!/usr/bin/env python3
"""run_player.py <OpenReflex.app> [--seconds N] [--script STEPS] [--capture FROM:SECONDS] [--fps-cap N] [--width W --height H] -- runs a COPY of the real
player (core/tools/build_mac_player.sh) sandboxed: a unique bundle id, a scratch save, writes to the user's preferences denied; no showcase, no test
time control. The menu world's probe (OpenFrontEnd/OpenMenuWorldProbe.cs) logs every rendered frame to motion.csv, optionally captures consecutive
frames and runs page changes after the first walk starts. Output: builds/menu3d_motion/<UTC stamp>/ (player.log, motion.csv, frames/)."""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--seconds", type=int, default=25)
ap.add_argument("--script", default="")
ap.add_argument("--capture", default="")
ap.add_argument("--fps-cap", type=int, default=60, help="REFLEX_FPS_CAP (default 60, VSync off: the window renders in the background; 0: the player's own pacing)")
ap.add_argument("--width", type=int, default=1920)
ap.add_argument("--height", type=int, default=1080)
ap.add_argument("--bundle", default="", help="a fixed bundle id suffix (the system's shader cache is per bundle id: a second run with the same id is warm)")
ap.add_argument("--record", default="", help="a deterministic recording into this folder (REFLEX_MENU3D_RECORD: 60 fps game time, every frame a JPEG; the player quits at its end)")
ap.add_argument("--hero-class", default="", help="REFLEX_MENU3D_HERO_CLASS: the hero rides the first vehicle of this class")
ap.add_argument("--menu3d", default="", help="REFLEX_MENU3D (0: the 2D pictures)")
ap.add_argument("--env", action="append", default=[], help="KEY=VALUE for the player (repeatable; e.g. the test ride probe: REFLEX_TESTRIDE_SCRIPT=mx,atv)")
ap.add_argument("--game", type=Path, default=Path("/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"))
a = ap.parse_args()
A = R / "builds/menu3d_motion" / datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
A.mkdir(parents=True)
app = A / "OpenReflex.app"
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
info.update(CFBundleIdentifier="local.reflex.menu3d.motion-" + (a.bundle or A.name.lower()), CFBundleName="Menu3D Motion Test")
(app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
save = A / "save"; save.mkdir()
home = Path.home(); prefs = home / "Library/Preferences"; support = home / "Library/Application Support"
sb = "(version 1)\n(allow default)\n"
for d in sorted({shipped, "unity.DefaultCompany.Reflex", "unity.DefaultCompany.OpenReflex"}):
    sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(support / d))
    for f in [prefs, prefs / "ByHost"]:
        sb += "(deny file-write* (regex %s))\n" % json.dumps("^" + re.escape(str(f / d)) + r"(\..*)?$")
for n in ["Reflex", "OpenReflex"]:
    sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(support / "DefaultCompany" / n))
(A / "protection.sb").write_text(sb)
subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(app)], check=True, capture_output=True)
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(a.game), REFLEX_CLIENT_ROOT=str(A), REFLEX_MENU3D_MOTION=str(A / "motion.csv"))
if a.script: env["REFLEX_MENU3D_SCRIPT"] = a.script
if a.capture: env["REFLEX_MENU3D_CAPTURE"] = str(A / "frames"); env["REFLEX_MENU3D_CAPTURE_AT"] = a.capture
if a.fps_cap: env["REFLEX_FPS_CAP"] = str(a.fps_cap)
if a.menu3d: env["REFLEX_MENU3D"] = a.menu3d
if a.record: env["REFLEX_MENU3D_RECORD"] = a.record
if a.hero_class: env["REFLEX_MENU3D_HERO_CLASS"] = a.hero_class
for kv in a.env:
    k, _, v = kv.partition("=")
    env[k] = v.replace("{run}", str(A))   # ({run}: this run's folder)
args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0", "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(A / "player.log")]
_front = nofocus.frontmost()
child = subprocess.Popen(args, cwd=A, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
nofocus.hand_back(_front)
print(A, "pid", child.pid, flush=True)
# Never pull the window to the front: the project lead works on this machine while tests run (activating stole focus every run).
# Runs keep a frame cap (REFLEX_FPS_CAP, VSync off), so a background window keeps producing frames.
try: child.wait(timeout=a.seconds)
except subprocess.TimeoutExpired:
    child.terminate()
    try: child.wait(timeout=10)
    except subprocess.TimeoutExpired: child.kill()
log = (A / "player.log").read_text(errors="replace") if (A / "player.log").exists() else ""
for l in log.splitlines():
    if "[MenuWorld" in l or "[TestRide" in l or "[MotoSkills" in l or "[TutorialProbe" in l: print(l[:300])
