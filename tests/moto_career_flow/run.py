#!/usr/bin/env python3
"""run.py <MotoFlow.app> [--seconds N] [--width W --height H] [--arcade] -- runs the National Moto career-flow acceptance (test only).

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
the protected locations unchanged.

--arcade runs the arcade replay probe instead (fixture/ArcadeReplayFlowProbe.cs): the event browser's National at NAT_Stillwater
(no career), ridden to its natural finish with the same mapped-input driver, then the original's end-of-race replay through the
exhibition replay presenter and the results screen, RETURN TO MENU and the final save. Same sandbox, save copy and bundle id rule.

--activation runs the Free Ride activation probe (fixture/ActivationFlowProbe.cs): the open front end's FREE RIDE at FR_Alpine, an
authored route ridden with mapped keys to the Hill Climb activation area, the AAA notification, the pause key (a keyboard event) starting
the area's challenge behind the AAA loading page, the challenge ridden with the throttle to its end, the end-of-challenge page
(PostChallengePopup: the core's medal or none, RESTART / EXIT), RESTART once (the activity restarts in place), the end page again and
its EXIT back to the Free Ride. The probe also checks the challenge's rules popup (PreChallengePopup: held in Pre until accepted), the KeyMap
glyphs (ReflexKeyHints is switched on in the test player), the popup / voice sounds, the PageWipe at the end of the AAA loading page and
the pause page's RESTART in the challenge (the activity restarts in place)."""
import argparse, datetime, hashlib, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--seconds", type=int, default=12000)
ap.add_argument("--width", type=int, default=1280)
ap.add_argument("--height", type=int, default=720)
ap.add_argument("--expected-credits", default="table", help="the final reward: 'table' = the game table's prize for the actual place (default), or an explicit integer")
ap.add_argument("--laps", type=int, default=0, help="iteration: n-lap motos (the original's debug lap override); 0 = authored 3 (acceptance)")
ap.add_argument("--fps-cap", type=int, default=60, help="the player's frame cap (a window in the background keeps rendering; the lead asks every run to pass one)")
ap.add_argument("--speed", type=int, default=0, help="iteration: K game frames per job (fast-forward, 2..8); 0 = real time")
ap.add_argument("--arcade", action="store_true", help="the arcade National replay / results probe instead of the career flow")
ap.add_argument("--arrival", default="", help="the Free Ride arrival probe (fixture/FreeRideArrivalProbe.cs) on this level (FR_Alpine / FR_Cheyenne / FR_Gibson): start the Free Ride, screenshots of its intro scene, PASS when the ride runs")
ap.add_argument("--crowd", default="", help="the stands crowd screenshots (fixture/CrowdShotProbe.cs): \"<Activity>|<Activity>/<Level>\", e.g. \"Supercross|Supercross/SX_Kingston\": the event browser starts the event, shots of the race intro and the first seconds of the race, PASS after 12 s of racing")
ap.add_argument("--plates", action="store_true", help="with --crowd: the player's jersey name FERRET / number 998 written to the scratch save, and close-ups of the riders on the gate (fixture/CrowdShotProbe.cs PlatesShots: plates-chase-*.png)")
ap.add_argument("--no-ffmpeg", action="store_true", help="test: the sandbox refuses to start ffmpeg (the in-level video screens must stay dark, the race must still run)")
ap.add_argument("--pacing", default="", choices=["", "original", "interpolated"], help="REFLEX_PACING for the player (default: the option's)")
ap.add_argument("--wake", action="store_true", help="with --arrival FR_Alpine: after the arrival, the bike ridden off the dock into the lake (w-water): the water's wake / splats, screenshots wake-*.png")
ap.add_argument("--hover", action="store_true", help="with --arrival FR_Alpine: after the arrival, the drivable hovercraft (w-hover): prompt, boarding, inflation, driving over land and water, leaving")
ap.add_argument("--reflex-arrows", default="", choices=["", "success", "fail"], help="the wreck-recovery arrows probe (fixture/ReflexArrowsRideProbe.cs): an arcade National at NAT_Stillwater, a forced bad ride, the prompt shot in both HUD styles, then the flick (success) or none (fail)")
ap.add_argument("--activation", action="store_true", help="the Free Ride activation area probe (FR_Alpine -> Hill Climb -> EXIT) instead of the career flow")
ap.add_argument("--hudrest", default="", choices=["", "HillClimb", "Trailblazer", "TargetPractice"], help="the ORIGINAL HUD ride (fixture/HudRestRideProbe.cs): FR_Alpine's Free Ride with HUD STYLE ORIGINAL ridden to this challenge's area (the compass and its icons, H, MODERN), the challenge started and ridden (its own layer: timer, finish / targets / height), the pause page; screenshots hudrest-*.png")
ap.add_argument("--series", type=int, default=-1, help="the career series probe (fixture/CareerSeriesFlowProbe.cs) on this Series id instead of the Moto14 flow")
ap.add_argument("--pilot-scale", type=float, default=0, help="--series: the test pilot's target speeds scaled (iteration; 0 = 1)")
ap.add_argument("--restart", action="store_true", help="--series: the pause page's RESTART once, a few seconds into the race (REFLEX_CAREER_RESTART=1): the career activity restarts in place, the race is ridden again, and the durable result is still exactly one (two checkpoints, the wallet moved by the one native reward)")
ap.add_argument("--heat-practice", action="store_true", help="--series 41-46: choose PRACTICE (the practice qualifier) instead of SKIP PRACTICE")
ap.add_argument("--light-log", action="store_true", help="also log the HDR eye adaptation and the water per frame (fixture/LightWaterProbe.cs: screenshots/light-water.csv, lw-*.png)")
ap.add_argument("--vehicle-type", type=int, default=3, help="--series: the Garage vehicle of this VehicleType (fixture-profiles.json: 2 MX Lite, 3 MX, 4 ATV, 11 UTV, 12 Super Buggy, 14 Pro Lite, 15 Pro 2)")
ap.add_argument("--client-root", type=Path, default=None, help="the client root whose Mods/Bikes the player scans (w-bikemods); default: the run folder")
ap.add_argument("--bike-mod", default="", help="with --client-root: the bike mod id (its folder under Mods/Bikes) chosen for every vehicle model in the save copy (BikeModel.Model.<id>)")
a = ap.parse_args()
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
I = R / "testdata/moto_career_flow"
profiles = json.loads((I / "fixture-profiles.json").read_text())["profiles"]
fixture = next(x for x in profiles if x["type"] == 3)
assert sha(I / "selected-moto-player24.bin") == fixture["sha256"], "the type-3 export differs from its recorded hash"
series_mode = a.series >= 0
if series_mode:   # the same save (the unlocked clone holds every vehicle); the Garage equips the requested type's vehicle
    fixture = next(x for x in profiles if x["type"] == a.vehicle_type)
game = R / "builds/sol_garage/game files"
assert game.is_dir(), "missing the reused game-data extraction builds/sol_garage/game files"

A = R / "builds/moto_flow/runs" / datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
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
if a.bike_mod:   # the save copy: the bike mod chosen for every model (a model the mod has no chassis for keeps the original)
    sj = json.loads((save / "save.json").read_text())
    sj.setdefault("strings", {}).update({"BikeModel.Model.%d" % m: a.bike_mod for m in range(0, 200)})
    (save / "save.json").write_text(json.dumps(sj, indent=2))

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
if a.no_ffmpeg:   # ffmpeg is usually a symlink into the Cellar: deny the program folders it can live in
    sb += "(deny process-exec* " + " ".join('(subpath "%s")' % d for d in ["/opt/homebrew", "/usr/local", "/opt/local"]) + ")\n"
(A / "protection.sb").write_text(sb)
subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)

args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0",
        "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(A / "player.log")]
env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
env.update(**({"REFLEX_FREERIDE_HOVER_PROBE": "1"} if a.hover else {}), **({"REFLEX_FREERIDE_WAKE_PROBE": "1"} if a.wake else {}), **({"REFLEX_PACING": a.pacing} if a.pacing else {}), REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(game),
           **({"REFLEX_REFLEX_ARROWS_PROBE": "1", "REFLEX_REFLEX_ARROWS_MODE": a.reflex_arrows} if a.reflex_arrows else {"REFLEX_ARCADE_REPLAY_PROBE": "1"} if a.arcade else {"REFLEX_FREERIDE_ARRIVAL_PROBE": a.arrival} if a.arrival else {"REFLEX_CROWD_SHOT_PROBE": a.crowd} if a.crowd else {"REFLEX_ACTIVATION_FLOW_PROBE": "1"} if a.activation else {"REFLEX_HUDREST_PROBE": a.hudrest} if a.hudrest
              else {"REFLEX_CAREER_SERIES_FLOW_PROBE": "1", "REFLEX_CAREER_SERIES": str(a.series), **({"REFLEX_HEAT_PRACTICE": "1"} if a.heat_practice else {}), **({"REFLEX_CAREER_RESTART": "1"} if a.restart else {}), **({"REFLEX_PILOT_SCALE": str(a.pilot_scale)} if a.pilot_scale > 0 else {})} if series_mode else {"REFLEX_MOTO_CAREER_FLOW_PROBE": "1"}),
           **({"REFLEX_TEST_LAPS": str(a.laps)} if a.laps > 0 else {}),
           **({"REFLEX_LIGHT_PROBE": "1"} if a.light_log else {}),
           **({"REFLEX_PLATES_SHOTS": "1"} if a.plates else {}),
           **({"REFLEX_TEST_SPEED": str(a.speed)} if a.speed > 1 else {}), REFLEX_FPS_CAP=str(a.fps_cap), REFLEX_MOTO_OWNER=str(fixture["owner"]), REFLEX_MOTO_VEHICLE=str(fixture["ownerVehicle"]),
           REFLEX_MOTO_MODEL=str(fixture["model"]), REFLEX_MOTO_EXPECT_CREDITS=str(a.expected_credits), REFLEX_PREVIEW_SHOTS=str(shots),
           REFLEX_CLIENT_ROOT=str((a.client_root or A).resolve()))
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
if a.reflex_arrows:
    tag = "[reflex-arrows-probe] "
    markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "BROWSER_OPENED", "USER filter National", "USER selected National/NAT_Stillwater",
               "USER START EVENT National/NAT_Stillwater", "SWERVE at", "PROMPT direction=", "RESULT " + a.reflex_arrows, "PASS reflex " + a.reflex_arrows]
    need_shots = ["browser", "setup", "prompt-modern", "prompt-original", "result-modern"]
if a.arcade:
    tag = "[arcade-replay-probe] "
    markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "BROWSER_OPENED", "USER filter National", "USER selected National/NAT_Stillwater",
               "USER START EVENT National/NAT_Stillwater", "EXHIBITION_REPLAY_PRESENTER", "NATURAL_FINISH", "REPLAY_ACTIVE", "RESULTS_AUDIO listenerPause=False", "RESULTS_SHOWN",
               "USER RETURN TO MENU", "RETURN_SAVED_PASS", "PASS arcade"]
    need_shots = ["browser", "setup", "riding", "replay", "results", "return"]
if a.activation:
    tag = "[activation-flow-probe] "
    markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "BROWSER_OPENED", "USER selected FreeRide/FR_Alpine", "USER START RIDE FreeRide/FR_Alpine",
               "FREE_RIDE_CHALLENGES", "AREA_ENTERED HillClimb", "NOTIFICATION_SHOWN", "USER pause in the area", "AAA_LOADING AAALoadingLayer",
               "GLYPHS notification keymaps=", "VOICE vo (race) AudioLocalizedActivity/Audio_ACTIVITY_FREERIDE",
               "CHALLENGE_STARTED HillClimb/FR_Alpine", "PRE_POPUP title=", "PRE_HELD state=0", "GLYPHS pre keymaps=", "USER ACCEPT pre popup",
               "PAGE_WIPE AAALoading transitions=", "CHALLENGE_RUNNING kind=1 state=2",
               "USER pause in the challenge", "PAUSE_PAGE open in the challenge", "USER pause page RESTART", "USER restart-challenge popup YES", "PAUSE_RESTART sameWorld=1",
               "GLYPHS post keymaps=", "POST_POPUP medal=", "USER RESTART", "CHALLENGE_RESTARTED",
               "POST_POPUP_AGAIN medal=", "USER EXIT", "BACK_IN_FREE_RIDE FreeRide/FR_Alpine", "SOUNDS popupShow=", "PASS activation"]
    need_shots = ["browser", "setup", "riding", "area", "aaa", "pre", "challenge", "pause", "pause-restart", "post", "post2", "exit"]
if a.hudrest:
    tag = "[hudrest-ride-probe] "
    markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "BROWSER_OPENED", "USER selected FreeRide/FR_Alpine", "USER START RIDE FreeRide/FR_Alpine", "FREE_RIDE_HUD page FreeRide",
               "USER H", "HUD_HIDDEN_BY_H", "HUD_SHOWN_BY_H", "MODERN originalUp=False", "ORIGINAL_AGAIN", "AREA_ENTERED " + a.hudrest, "USER pause in the area", "CHALLENGE_STARTED " + a.hudrest + "/FR_Alpine",
               "PRE_POPUP HUD", "USER ACCEPT pre popup", "CHALLENGE_RUNNING kind=", "CHALLENGE_HUD page " + a.hudrest, "PAUSE_HUD page None", "PASS hudrest " + a.hudrest]
    need_shots = ["browser", "setup", "hudrest-1-freeride", "hudrest-2-riding", "hudrest-3-hidden", "hudrest-4-modern", "hudrest-5-area", "hudrest-6-rules", "hudrest-7-challenge-start", "hudrest-8-challenge-riding", "hudrest-9-challenge-later", "hudrest-10-pause"]
if a.arrival:
    tag = "[freeride-arrival-probe] "
    markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "BROWSER_OPENED", "USER START RIDE FreeRide/" + a.arrival, "INTRO_STARTED", "INTRO_ENDED", "PASS arrival"]
    need_shots = ["arrival-1-1", "arrival-after"]
    if a.hover:
        markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "BROWSER_OPENED", "USER START RIDE FreeRide/" + a.arrival, "INTRO_ENDED", "HOVER_READY", "HOVER_PROMPT", "HOVER_BOARDED",
                   "HOVER_INFLATED", "HOVER_LAND", "HOVER_WATER_ENTERED", "HOVER_WATER ", "HOVER_WALL ", "HOVER_BRAKED", "HOVER_LEFT", "HOVER_SUMMONED", "PASS hover"]
        need_shots = ["hover-1-prompt", "hover-3-flat", "hover-6-inflated", "hover-7-land", "hover-8-water", "hover-11-left", "hover-12-summoned"]
    if a.wake:
        markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "BROWSER_OPENED", "USER START RIDE FreeRide/" + a.arrival, "INTRO_ENDED", "WAKE_READY", "WAKE_OCEAN_STEPS", "WAKE_TARGET",
                   "WAKE_ENTERED", "WAKE_DONE", "PASS wake"]
        need_shots = ["wake-0-shore", "wake-1-water", "wake-3-field", "wake-4-water", "wake-6-field", "wake-8-water", "wake-9-after"]
if a.crowd:
    tag = "[crowd-shot-probe] "
    markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "BROWSER_OPENED", "USER START EVENT " + a.crowd.split("|")[1], "INTRO_STARTED", "RACING", "PASS crowd"]
    need_shots = ["crowd-intro-1", "crowd-race-1", "crowd-after"]
if series_mode:
    tag = "[career-series-flow-probe] "
    markers = ["GUEST_BEFORE_AWAKE", "REGISTERED_INIT_PASS", "GARAGE_VEHICLE_CONSUMED", "GARAGE_DONE", "BROWSER_OPENED", "USER selected series %d" % a.series,
               "RACE_RUNNING", "NATURAL_FINISH", "USER Continue results", "ACTUAL_REWARD", "USER Return -> FrontEnd", "ACTUAL_OUTCOME_DURABLE_PASS series=%d" % a.series,
               "POST_RETURN_CATALOG_PASS series=%d" % a.series, "PASS series %d" % a.series]
    need_shots = ["garage", "browser", "riding", "results", "saved", "return"]
    if a.restart:   # the pause page's RESTART in the race (CareerSeriesFlowProbe.TickRestart)
        markers += ["PAUSE_RESTART_OPENED", "PAUSE_RESTART_CONFIRMED", "PAUSE_RESTART_PRE", "PAUSE_RESTART_RACING"]
        need_shots += ["restarted"]
    if 41 <= a.series <= 46:   # the Heat format: its practice choice and the heat's gate page (the later rounds depend on the places)
        markers += ["USER heat practice choice", "HEAT_GATE_SELECTED", "HEAT_RACE_RUNNING round=heat"]
        need_shots += ["practice-choice", "gate-heat", "riding-heat"]
missing_markers = [m for m in markers if tag + m not in log]
if a.activation and "[PostChallenge] PAYOUT medal" not in log: missing_markers.append("[PostChallenge] PAYOUT medal (the challenge prize, w-career)")
missing_shots = [n for n in need_shots if not ((shots / (n + ".png")).is_file() and (shots / (n + ".png")).stat().st_size > 0)]
exceptions = [x for x in ["MissingMethodException", "TypeLoadException", "NullReferenceException"] if x in log]
changed = [q for q, h in before.items() if tree(q) != h] + (["preference file set"] if pref_files() != prefs_before else [])
protected_ok = not changed
passed = code == 0 and not timedout and not missing_markers and not missing_shots and not exceptions and protected_ok \
         and (bool(a.reflex_arrows) or a.arcade or a.activation or bool(a.hudrest) or bool(a.arrival) or bool(a.crowd) or series_mode or "USER prestart SkipPractice; practice riding NOT CLAIMED" in log)
result = {"passed": passed, "probe": "reflex-arrows " + a.reflex_arrows if a.reflex_arrows else "arcade" if a.arcade else "arrival " + a.arrival if a.arrival else "crowd " + a.crowd if a.crowd else "hudrest " + a.hudrest if a.hudrest else "activation" if a.activation else ("series %d" % a.series) if series_mode else "career",
          "mode": "acceptance" if a.laps == 0 and a.speed <= 1 else f"iteration (laps {a.laps or 3}, speed {a.speed or 1})", "exit": code, "timedout": timedout, "protectedUnchanged": protected_ok, "protectedChanged": changed, "missingMarkers": missing_markers,
          "missingScreenshots": missing_shots, "managedExceptions": exceptions,
          "lastProbeLines": [l for l in log.splitlines() if tag in l][-12:],
          # the post-race presentation (fixture/PodiumShotProbe.cs, informational: not a PASS condition)
          "podiumScreenshots": sorted(str(q) for q in shots.glob("podium-*.png")),
          "podiumLines": [l for l in log.splitlines() if "[podium-shots] " in l][-40:]}
(A / "RESULT.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
sys.exit(0 if passed else 1)
