#!/usr/bin/env python3
"""run.py <RT.app> <plan.json> --out DIR [--width W --height H] [--env K=V ...] -- runs the REPLAY TERRAIN showcase (w-replayterrain, test only).

A -batchmode player (no window: it never takes the focus or touches the mouse; tests/nofocus.py hands the focus back anyway), sandboxed like
tests/cinematic_showcase/run.py: a unique bundle id per run, a scratch save, writes to the user's preferences / application support denied,
the game files of the sol_garage extraction read only. The fixture mutes the game's sound. Output: <out>/screenshots/*.png,
console.log, player.log; the run's copy of the app is removed afterwards (disk)."""
import argparse, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus

R = Path(__file__).resolve().parents[2]
G = "/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path); ap.add_argument("plan", type=Path)
ap.add_argument("--out", type=Path, required=True)
ap.add_argument("--width", type=int, default=1600); ap.add_argument("--height", type=int, default=900)
ap.add_argument("--env", action="append", default=[])
a = ap.parse_args()
A = a.out.resolve(); A.mkdir(parents=True, exist_ok=True)
app = A / "RT.app"
if app.exists(): shutil.rmtree(app)
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
test_id = "local.reflex.rt.test-" + re.sub(r"[^a-z0-9]", "", A.name.lower())
info.update(CFBundleIdentifier=test_id, CFBundleName="RT Test")
(app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
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
subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
shots = A / "screenshots"; shots.mkdir(exist_ok=True)
save = A / "save"; save.mkdir(exist_ok=True); (save / "ReflexCache").mkdir(exist_ok=True)
log = A / "player.log"
args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-batchmode", "-reflexTestWindow", "-screen-fullscreen", "0",
        "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(log)]
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=G, REFLEX_SKIP_INTRO="1",
           REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_RT_PLAN=str(a.plan.resolve()), REFLEX_FPS_CAP="240", REFLEX_SHOT_SIZE=f"{a.width}x{a.height}")
env.update(dict(kv.split("=", 1) for kv in a.env))
with (A / "console.log").open("w") as clog:
    front = nofocus.frontmost()
    child = subprocess.Popen(args, cwd=R, env=env, stdout=clog, stderr=subprocess.STDOUT)
    nofocus.hand_back(front)
    print(A, "pid", child.pid, flush=True)
    code = child.wait()
text = log.read_text(errors="replace") if log.exists() else ""
for l in text.splitlines():
    if "[rt-showcase]" in l or "Exception" in l: print(l[:400])
print("exit", code, "shots", len(list(shots.glob("*.png"))))
shutil.rmtree(app, ignore_errors=True)
