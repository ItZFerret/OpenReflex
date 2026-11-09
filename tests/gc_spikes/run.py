#!/usr/bin/env python3
"""run.py <GcSpikes.app from build.py> [--script STEPS] [--ai N] [--results S] [--profile] [--fps N] [--out DIR] -- the GC SPIKES run (test only).

One -batchmode player (no window, never takes the focus, silent: the fixture holds AudioListener.volume at 0; the save is untouched) runs fixture/GcSpikeProbe.cs's script (default: three ridden 3-lap Nationals at
NAT_Stillwater with computer riders, the results page held 25 s each, back to the front end between them, plus loads of other tracks),
with the shipped perf log on (-reflexPerfLog: the "perf t=..." lines of a tester's session log). --profile: profiler captures of the
first ride (riding and the results page) into the run directory; report them with report.py. Sandboxed like tests/lighting_showcase:
a unique bundle id per run, a scratch save, writes to the user's preferences / application support denied. Output:
builds/gc_spikes/runs/<stamp>/ (player.log, the session log under save/, RESULT.json with the probe's lines)."""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
G = "/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--script", default="ride:National/NAT_Stillwater,sit:National/NAT_Bedford:20,ride:National/NAT_Stillwater,sit:National/NAT_Concord:20,ride:National/NAT_Stillwater")
ap.add_argument("--ai", type=int, default=5)
ap.add_argument("--results", type=int, default=25)
ap.add_argument("--profile", action="store_true")
ap.add_argument("--profile-seconds", type=int, default=15)
ap.add_argument("--fps", type=int, default=75, help="the frame cap (VSync off in a batch player); the tester's display is 74.9 Hz")
ap.add_argument("--seconds", type=int, default=5400)
ap.add_argument("--env", action="append", default=[], help="KEY=VALUE for the player (repeatable)")
ap.add_argument("--out", type=Path)
a = ap.parse_args()
assert Path(G).is_dir(), "missing the game-data extraction: " + G
A = a.out.resolve() if a.out else R / "builds/gc_spikes/runs" / datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
A.mkdir(parents=True)
app = A / "GcSpikes.app"
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
info.update(CFBundleIdentifier="local.reflex.gcspikes.test-" + A.name.lower(), CFBundleName="GC Spikes Test")
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
sb += "(deny file-write* (subpath %s))\n" % json.dumps(G)   # never write into the game files
(A / "protection.sb").write_text(sb)
subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)
save = A / "save"; (save / "ReflexCache").mkdir(parents=True)
log = A / "player.log"
args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-batchmode", "-reflexTestWindow", "-screen-fullscreen", "0",
        "-screen-width", "1920", "-screen-height", "1080", "-logFile", str(log), "-reflexPerfLog", "-reflexLogDir", str(A / "logs")]
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=G, REFLEX_SKIP_INTRO="1",
           REFLEX_FPS_CAP=str(a.fps), REFLEX_GCSPIKE_SCRIPT=a.script, REFLEX_GCSPIKE_AI=str(a.ai), REFLEX_GCSPIKE_RESULTS=str(a.results),
           REFLEX_GCSPIKE_PROFILE_SECONDS=str(a.profile_seconds), REFLEX_PERF_LOG="1")
if a.profile: env["REFLEX_GCSPIKE_PROFILE"] = str(A / "profile")
env.update(dict(kv.split("=", 1) for kv in a.env))
timedout = False
with (A / "console.log").open("w") as clog:
    _front = nofocus.frontmost()
    child = subprocess.Popen(args, cwd=R, env=env, stdout=clog, stderr=subprocess.STDOUT)
    nofocus.hand_back(_front)
    print("run", A, "pid", child.pid, flush=True)
    try: code = child.wait(timeout=a.seconds)
    except subprocess.TimeoutExpired:
        timedout = True; child.terminate()
        try: code = child.wait(timeout=10)
        except subprocess.TimeoutExpired: child.kill(); code = child.wait()
text = log.read_text(errors="replace") if log.exists() else ""
lines = [l for l in text.splitlines() if "[gcspike]" in l or "error CS" in l or "Exception" in l]
(A / "RESULT.json").write_text(json.dumps({"exit": code, "timedout": timedout, "lines": lines}, indent=1))
print("\n".join(l[:600] for l in lines if "PHASE" in l or "HEAP" in l or "TOTAL" in l or "FAIL" in l or "RESULT" in l or "HITCH" in l))
print("exit", code, "timedout", timedout, A)
