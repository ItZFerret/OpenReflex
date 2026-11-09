#!/usr/bin/env python3
"""run.py <App.app> [--env K=V ...] [--seconds N] [--out DIR] [--name NAME] -- runs one sandboxed test player (w-rmphoto, test only).

The generic launcher of the cinematic-effects showcase and the replay probe: a unique bundle id per run, a scratch save, writes to the
user's preferences / application support denied (as tests/lighting_showcase/run.py), -reflexTestWindow (a window that never takes the
focus: tests/nofocus.py), the game files from the sol_garage extraction. The test is chosen by the environment:
  REFLEX_CINE_PLAN=<json>   the cinematic showcase (fixture/CinematicShowcase.cs)
  REFLEX_REPLAY_PROBE=1 REFLEX_REPLAY_EVENT=... REFLEX_REPLAY_SHOTS=<dir>   the replay probe (Scripts/Replay/ReflexReplay.Probe.cs)
Output: <out>/screenshots/*.png, console-<name>.log, player-<name>.log."""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus

R = Path(__file__).resolve().parents[2]
G = "/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--env", action="append", default=[])
ap.add_argument("--seconds", type=int, default=900)
ap.add_argument("--width", type=int, default=1920)
ap.add_argument("--height", type=int, default=1080)
ap.add_argument("--out", type=Path)
ap.add_argument("--name", default="run")
ap.add_argument("--game", default=G)
a = ap.parse_args()
A = a.out.resolve() if a.out else R / "builds/cine/runs" / datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
A.mkdir(parents=True, exist_ok=True)
app = A / "Player.app"
if app.exists(): shutil.rmtree(app)
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
test_id = "local.reflex.cine.test-" + A.name.lower() + "-" + a.name
info.update(CFBundleIdentifier=test_id, CFBundleName="Cine Test")
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
subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)
shots = A / "screenshots"; shots.mkdir(exist_ok=True)
save = A / f"save-{a.name}"; save.mkdir(exist_ok=True); (save / "ReflexCache").mkdir(exist_ok=True)
log = A / f"player-{a.name}.log"
args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0",
        "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(log)]
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=a.game, REFLEX_SKIP_INTRO="1",
           REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_REPLAY_SHOTS=str(shots), REFLEX_PHOTO_DIR=str(shots), REFLEX_FPS_CAP="240")
env.update(dict(kv.split("=", 1) for kv in a.env))
timedout = False
with (A / f"console-{a.name}.log").open("w") as clog:
    front = nofocus.frontmost()
    child = subprocess.Popen(args, cwd=R, env=env, stdout=clog, stderr=subprocess.STDOUT)
    nofocus.hand_back(front)
    print(a.name, A, "pid", child.pid, flush=True)
    try: code = child.wait(timeout=a.seconds)
    except subprocess.TimeoutExpired:
        timedout = True; child.terminate()
        try: code = child.wait(timeout=10)
        except subprocess.TimeoutExpired: child.kill(); code = child.wait()
text = log.read_text(errors="replace") if log.exists() else ""
keys = ("[cine]", "[ReplayProbe]", "[Replay]", "error CS", "Exception")
lines = [l for l in text.splitlines() if any(k in l for k in keys)]
print("\n".join(l[:300] for l in lines[-40:]))
print(a.name, "exit", code, "timedout", timedout, "shots", sorted(p.name for p in shots.glob("*.png")), flush=True)
print(A)
