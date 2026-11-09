#!/usr/bin/env python3
"""run.py <Showcase.app> [--pages a,b,c|all] [--width W --height H] [--game DIR] [--client-root DIR] [--menu3d 0|1] [--seconds N] -- runs the UI showcase (test only).

The player (fixture/UiShowcase.cs) opens each requested page with synthetic data, takes a screenshot of it and quits. Output:
builds/menus/runs/<UTC stamp>/ (player.log, screenshots/<page>.png). A sandbox profile denies writes to the user's real preferences and
application support (the bundle id is unique per run); the save is a scratch directory. No race is ridden."""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--pages", default="all")
ap.add_argument("--width", type=int, default=1920)
ap.add_argument("--height", type=int, default=1080)
ap.add_argument("--seconds", type=int, default=240)
ap.add_argument("--batch", action="store_true", help="-batchmode: no window at all (never takes the focus); the shots are composed offscreen at --width x --height (fixture/UiShowcaseOffscreen.cs)")
ap.add_argument("--out", type=Path, default=None, help="the run folder's parent (default builds/menus/runs)")
ap.add_argument("--walk", action="store_true", help="walk every page with a synthetic pad and keyboard after its screenshot (controller test)")
ap.add_argument("--game", type=Path, default=Path("/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"))
ap.add_argument("--env", action="append", default=[], help="K=V passed to the player (REFLEX_LIGHTING=enhanced ...)")
ap.add_argument("--menu3d", choices=["0", "1"], default=None, help="REFLEX_MENU3D: 0 keeps the 2D pictures behind the menus (the 3D menu world is on by default)")
ap.add_argument("--seed-save", type=Path, default=None, help="a player snapshot (core-player-career-v2.bin, e.g. testdata/moto_career_flow/core-player-career-v2.bin) copied into the scratch save before the player starts (the profile the pages read is then that real save)")
ap.add_argument("--seed-dir", type=Path, default=None, help="a whole test save folder (save.json and the player snapshots, e.g. testdata/moto_career_flow: the unlocked clone) copied into the scratch save before the player starts")
ap.add_argument("--client-root", type=Path, default=None, help="the client root whose Mods/Bikes and Mods/EngineSounds the player scans (bike mods: pages bikemod-*); default: the run folder")
a = ap.parse_args()
assert a.game.is_dir(), "missing the game-data extraction: " + str(a.game)
A = (a.out or R / "builds/menus/runs") / datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
A.mkdir(parents=True)
app = A / "Showcase.app"
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
test_id = "local.reflex.showcase.test-" + A.name.lower()
info.update(CFBundleIdentifier=test_id, CFBundleName="Showcase Test")
if a.batch: info.update(NSAppSleepDisabled=True, LSAppNapIsDisabled=True)   # (App Nap would throttle a player in the background: its frame times would lie)
(app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
save = A / "save"; save.mkdir()
if a.seed_save: shutil.copy2(a.seed_save, save / "core-player-career-v2.bin")
if a.seed_dir:
    for f in sorted(a.seed_dir.iterdir()):
        if f.is_file() and f.suffix in (".json", ".bin"): shutil.copy2(f, save / f.name)
shots = A / "screenshots"; shots.mkdir()
home = Path.home(); prefs = home / "Library/Preferences"; support = home / "Library/Application Support"
domains = {shipped, "unity.DefaultCompany.Reflex", "unity.DefaultCompany.OpenReflex", "com.Unity-Technologies.com.unity.template.urp-blank",
           "unity.UnityTechnologies.com.unity.template.urp-blank"}
sb = "(version 1)\n(allow default)\n"
for d in sorted(domains):
    sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(support / d))
    for f in [prefs, prefs / "ByHost"]:
        sb += "(deny file-write* (regex %s))\n" % json.dumps("^" + re.escape(str(f / d)) + r"(\..*)?$")
for n in ["Reflex", "OpenReflex", "Reflex Core Garage Bench"]:
    sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(support / "DefaultCompany" / n))
(A / "protection.sb").write_text(sb)
subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)
args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0",
        "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(A / "player.log")]
if a.batch: args.insert(4, "-batchmode")
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(a.game),
           REFLEX_UI_SHOWCASE=a.pages, REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_UI_WALK="1" if a.walk else "0",
           REFLEX_CLIENT_ROOT=str((a.client_root or A).resolve()))
if a.menu3d is not None: env["REFLEX_MENU3D"] = a.menu3d
if a.batch: env["REFLEX_SHOT_SIZE"] = f"{a.width}x{a.height}"
for kv in a.env: k, _, v = kv.partition("="); env[k] = v
timedout = False
with (A / "console.log").open("w") as log:
    _front = nofocus.frontmost()
    child = subprocess.Popen(args, cwd=R, env=env, stdout=log, stderr=subprocess.STDOUT)
    nofocus.hand_back(_front)
    print(A, "pid", child.pid, flush=True)
    try: code = child.wait(timeout=a.seconds)
    except subprocess.TimeoutExpired:
        timedout = True; child.terminate()
        try: code = child.wait(timeout=10)
        except subprocess.TimeoutExpired: child.kill(); code = child.wait()
logtext = (A / "player.log").read_text(errors="replace") if (A / "player.log").exists() else ""
lines = [l for l in logtext.splitlines() if "[ui-showcase]" in l]
print("\n".join(lines[-(400 if a.walk else 40):]))
print("exit", code, "timedout", timedout, "shots", sorted(p.name for p in shots.glob("*.png")))
sys.exit(0 if code == 0 and not timedout else 1)
