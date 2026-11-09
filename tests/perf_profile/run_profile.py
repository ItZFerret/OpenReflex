#!/usr/bin/env python3
"""run_profile.py <MotoFlow.app from profile_build.py> -- run.py plus a profiler capture window (PROFILE_START / PROFILE_SECONDS,
seconds after start; default 200 / 40) into <run>/capture.raw. Otherwise:

run.py <MotoFlow.app> [--seconds N] [--width W --height H] -- runs the National Moto career-flow acceptance (test only).

The probe (fixture/MotoCareerFlowProbe.cs) drives the real game the way a player does:
  1. the actual Garage selection of the type-3 Moto export;
  2. the event browser, then the painted "Skip practice and choose gate" button (no practice riding is claimed);
  3. two ordinary-input three-lap races with 12 riders;
  4. the real replays, the aggregate scores and the reward;
  5. two durable save checkpoints with backup, then Return to the catalog.
It only presses mapped keys and clicks painted controls. It writes no race, finish, score or ACK state.

The run uses a COPY of the test save (testdata/moto_career_flow) and a guest profile. A sandbox profile denies writes to the
player's real saves, preferences and application support. Output: builds/moto_flow/runs/<UTC stamp>/ (player.log, screenshots,
RESULT.json). PASS = exit 0 and every required probe marker in the log, every milestone screenshot, no managed exception, and
the protected locations unchanged."""
import argparse, datetime, hashlib, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--seconds", type=int, default=12000)
ap.add_argument("--width", type=int, default=1280)
ap.add_argument("--height", type=int, default=720)
ap.add_argument("--expected-credits", type=int, default=25)
a = ap.parse_args()
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
I = R / "testdata/moto_career_flow"
fixture = next(x for x in json.loads((I / "fixture-profiles.json").read_text())["profiles"] if x["type"] == 3)
assert sha(I / "selected-moto-player24.bin") == fixture["sha256"], "the type-3 export differs from its recorded hash"
game = R / "builds/sol_garage/game files"
assert game.is_dir(), "missing the reused game-data extraction builds/sol_garage/game files"

A = R / "builds/perf_profile/runs" / datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
A.mkdir(parents=True)
app = A / "MotoFlow.app"
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
# A unique bundle identifier per run: Unity writes PlayerPrefs (window size, fullscreen mode, ...) through cfprefsd into the
# bundle id's preferences domain, which the sandbox cannot block. The built player's default id is the one the real game uses.
shipped_id = info["CFBundleIdentifier"]
info.update(CFBundleIdentifier="local.reflex.motoflow.test-" + A.name.lower(), CFBundleName="Moto Flow Test")
(app / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
save = A / "save"; save.mkdir()
shots = A / "screenshots"; shots.mkdir()
for n in ["save.json", "core-player-career-v2.bin", "selected-moto-player24.bin"]:
    shutil.copy2(I / n, save / n)

# protected locations: the player's real saves, preferences and application support for every bundle id this project used
home = Path.home(); prefs = home / "Library/Preferences"; support = home / "Library/Application Support"
domains = {shipped_id, "com.Unity-Technologies.com.unity.template.urp-blank",
           "unity.UnityTechnologies.com.unity.template.urp-blank", "unity.DefaultCompany.Reflex", "unity.DefaultCompany.OpenReflex",
           "unity.DefaultCompany.Reflex Core Garage Bench", "com.DefaultCompany.ReflexCoreGarageBench"}
def tree(p):
    p = Path(p)
    if p.is_dir(): return {str(q): sha(q) for q in p.rglob("*") if q.is_file()}
    return {str(p): sha(p)} if p.is_file() else {}
def pref_files():
    return sorted(str(q) for f in [prefs, prefs / "ByHost"] if f.is_dir() for q in f.iterdir()
                  if any(q.name == d + ".plist" or q.name.startswith(d + ".") for d in domains))
protect = [support / "DefaultCompany" / n for n in ["Reflex Core Garage Bench", "Reflex", "OpenReflex"]] + [support / d for d in domains] \
          + [Path(q) for q in pref_files()] + [I]
before = {str(q): tree(q) for q in protect}; prefs_before = pref_files()
sb = "(version 1)\n(allow default)\n" + "".join(f"(deny file-write* (subpath {json.dumps(str(q))}))\n" for q in protect)
for d in sorted(domains):
    for f in [prefs, prefs / "ByHost"]:
        rx = "^" + re.escape(str(f / d)) + r"(\..*)?$"
        sb += "(deny file-write* (regex " + json.dumps(rx) + "))\n"
(A / "protection.sb").write_text(sb)
subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)

args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0",
        "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(A / "player.log")]
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(game),
           REFLEX_MOTO_CAREER_FLOW_PROBE="1", REFLEX_MOTO_OWNER=str(fixture["owner"]), REFLEX_MOTO_VEHICLE=str(fixture["ownerVehicle"]),
           REFLEX_PROFILE_FILE=str(A / "capture.raw"), REFLEX_PROFILE_START=os.environ.get("PROFILE_START", "200"),
           REFLEX_PROFILE_SECONDS=os.environ.get("PROFILE_SECONDS", "40"),
           REFLEX_MOTO_MODEL=str(fixture["model"]), REFLEX_MOTO_EXPECT_CREDITS=str(a.expected_credits), REFLEX_PREVIEW_SHOTS=str(shots))
(A / "plan.json").write_text(json.dumps({"argv": args, "env": {k: v for k, v in env.items() if k.startswith("REFLEX_")},
                                          "coreSha256": sha(app / "Contents/PlugIns/librxcore.dylib")}, indent=2) + "\n")
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

log = (A / "player.log").read_text(errors="replace") if (A / "player.log").exists() else ""
tag = "[moto-career-flow-probe] "
markers = ["GARAGE_TAB_CONSUMED", "GARAGE_VEHICLE_CONSUMED", "SCREENSHOT_CONTROL practice-choice control=career-moto-skip-practice",
           "TEST USER left-click delivered career-moto-skip-practice", "GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS",
           "NATURAL_MOTO_FINISH moto=1", "NATURAL_MOTO_FINISH moto=2", "UNLOCKED_LEASE_PASS", "MOTO_NATIVE_PASS", "BACKUP_CODEC_PASS",
           "ACTUAL_OUTCOME_DURABLE_PASS", "POST_RETURN_CATALOG_PASS", "TWO_MOTO_REPLAY_RETURN_PASS", "PASS actual Return",
           "NATIVE_NATIONAL_HUD_PAINT moto=1", "NATIVE_NATIONAL_HUD_PAINT moto=2", "ACTUAL_FINAL_STANDING"]
need_shots = ["garage", "browser", "practice-choice", "startup", "gate1", "gate2", "moto1-riding", "moto1-results", "moto2-riding",
              "results", "rewards", "saved", "return"]
missing_markers = [m for m in markers if tag + m not in log]
missing_shots = [n for n in need_shots if not ((shots / (n + ".png")).is_file() and (shots / (n + ".png")).stat().st_size > 0)]
exceptions = [x for x in ["MissingMethodException", "TypeLoadException", "NullReferenceException"] if x in log]
changed = [q for q, h in before.items() if tree(q) != h] + (["preference file set"] if pref_files() != prefs_before else [])
protected_ok = not changed
passed = code == 0 and not timedout and not missing_markers and not missing_shots and not exceptions and protected_ok \
         and "USER prestart SkipPractice; practice riding NOT CLAIMED" in log
result = {"passed": passed, "exit": code, "timedout": timedout, "protectedUnchanged": protected_ok, "protectedChanged": changed, "missingMarkers": missing_markers,
          "missingScreenshots": missing_shots, "managedExceptions": exceptions,
          "lastProbeLines": [l for l in log.splitlines() if tag in l][-12:]}
(A / "RESULT.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
sys.exit(0 if passed else 1)
