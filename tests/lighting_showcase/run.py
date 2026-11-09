#!/usr/bin/env python3
"""run.py <Lighting.app> [--shots a,b|all] [--width W --height H] [--tier 0|1|2] [--seconds N] -- runs the LIGHTING SHOWCASE (test only).

One player process per planned place (PLANS below): the fixture (fixture/LightingShowcase.cs) launches the event from the open front end,
holds the camera on each view and saves <prefix>_<view>_original.png / _enhanced.png plus <prefix>_perf.txt (frame times of both modes).
Sandboxed like tests/menus_showcase: a unique bundle id per run, a scratch save, writes to the user's preferences / application support
denied. --tier sets graphics.cfg ShadowQuality in the scratch save's copy (the ENHANCED quality tier). Output: builds/lighting/runs/<stamp>/."""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
G = "/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"

# views: "game" = the game's own camera; "pose" = a camera relative to the bike (frame bike / sun / world) or at a world point
PLANS = {
    "stillwater": {"event": "National/NAT_Stillwater", "prefix": "01_Stillwater", "settle": 0, "views": [
        {"name": "pits", "cam": "game", "t": 3},
        {"name": "rider", "cam": "game", "t": 9},
        {"name": "sky", "cam": "game", "t": 12},
        {"name": "gate", "cam": "game", "t": 15},
    ]},
    "dusk": {"event": "Freestyle/FMX_Dover", "prefix": "02_Dover_dusk", "settle": 0, "views": [
        {"name": "harbour", "cam": "game", "t": 3},
        {"name": "rider", "cam": "game", "t": 12},
    ]},
    "overcast": {"event": "National/NAT_DLC06_DaltonB", "prefix": "02b_DaltonB_overcast", "settle": 0, "views": [
        {"name": "rider", "cam": "game", "t": 6},
        {"name": "gate", "cam": "game", "t": 12},
    ]},
    "supercross": {"event": "Supercross/SX_Kingston", "prefix": "03_Kingston_SX", "settle": 0, "views": [
        {"name": "stadium", "cam": "game", "t": 3},
        {"name": "gate", "cam": "game", "t": 15},
    ]},
    "alpine": {"event": "FreeRide/FR_Alpine", "prefix": "04_Alpine_FreeRide", "settle": 0, "views": [
        {"name": "lake", "cam": "game", "t": 3},
        {"name": "ride", "cam": "game", "t": 12},
        {"name": "vista", "cam": "pose", "t": 13, "frame": "sun", "offset": [0, 40, 70], "look": "yawpitch", "yaw": 180, "pitch": -14, "fov": 70},
        {"name": "backlit", "cam": "pose", "t": 13.5, "frame": "sun", "offset": [0, 30, -70], "look": "yawpitch", "yaw": 0, "pitch": -6, "fov": 70},
    ]},
    "terracross": {"event": "Terracross/TRX_Halifax", "prefix": "05_Halifax_Terracross", "settle": 0, "views": [
        {"name": "valley", "cam": "game", "t": 3},
        {"name": "gate", "cam": "game", "t": 12},
    ]},
    "closeup": {"event": "Terracross/TRX_Halifax", "prefix": "06_RiderCloseup_Halifax", "settle": 0, "perf": 0, "views": [
        {"name": "front34", "cam": "pose", "t": 12, "frame": "bike", "offset": [4.6, 1.5, 4.2], "look": "anchor", "lookY": 0.6, "fov": 40},
        {"name": "rear34", "cam": "pose", "t": 12.5, "frame": "bike", "offset": [-4.2, 2.0, -4.8], "look": "anchor", "lookY": 0.5, "fov": 40},
        {"name": "shadow", "cam": "pose", "t": 13, "frame": "bike", "offset": [1.5, 6.5, -6.5], "look": "anchor", "lookY": -0.5, "fov": 50},
    ]},
}

# MATERIALS: ENHANCED / MUD BUILDUP (w-rmmat): the bike and rider close up in three lights, the materials ORIGINAL vs ENHANCED (both with LIGHTING:
# ENHANCED), then ENHANCED with forced dirt levels (dirtNN = NN % of the dirt amount, wet = wet mud)
_MAT_MODES = ["enhanced-matorig", "enhanced-matenh", "enhanced-matenh-dirt35", "enhanced-matenh-dirt70", "enhanced-matenh-dirt70-wet"]
_MAT_VIEWS = [
    {"name": "front34", "cam": "pose", "t": 12, "frame": "bike", "offset": [4.2, 1.4, 3.8], "look": "anchor", "lookY": 0.6, "fov": 40},
    {"name": "rear34", "cam": "pose", "t": 12.5, "frame": "bike", "offset": [-3.8, 1.8, -4.4], "look": "anchor", "lookY": 0.5, "fov": 40},
    {"name": "low", "cam": "pose", "t": 13, "frame": "bike", "offset": [2.6, 0.7, 1.6], "look": "anchor", "lookY": 0.35, "fov": 40},
    {"name": "side", "cam": "pose", "t": 13.5, "frame": "bike", "offset": [5.2, 1.2, 0.2], "look": "anchor", "lookY": 0.7, "fov": 36},
    {"name": "rider", "cam": "pose", "t": 14, "frame": "bike", "offset": [2.6, 1.9, 3.6], "look": "anchor", "lookY": 1.55, "fov": 42},
    {"name": "rearlow", "cam": "pose", "t": 14.5, "frame": "bike", "offset": [-1.9, 0.8, -3.0], "look": "anchor", "lookY": 0.45, "fov": 40},
]
# real riding: five computer riders from the gate, shots of each of them at 20 / 45 / 75 s (their mud record + wheels on the terrain: the build-up as it happens)
PLANS["matride"] = {"event": "National/NAT_Stillwater", "prefix": "14_MatRide", "settle": 0, "perf": 0, "adapt": 1.0, "ai": 5, "modes": ["enhanced-matorig", "enhanced-matenh"],
    "views": [{"name": "opp%d" % t, "cam": "pose", "t": t, "frame": "opp", "opp": -2, "offset": [3.6, 1.1, 2.8], "look": "anchor", "lookY": 0.55, "fov": 40} for t in (20, 45, 75)]}
# the cost: the materials ORIGINAL / ENHANCED alternated (the machine is shared: compare the pairs), in the game's own camera
PLANS["matperf"] = {"event": "National/NAT_Stillwater", "prefix": "15_MatPerf", "settle": 0, "perf": 4, "adapt": 1.0, "ai": 5,
    "modes": ["enhanced-matorig", "enhanced-matenh", "enhanced-matorig", "enhanced-matenh", "enhanced-matorig", "enhanced-matenh", "enhanced-matenh-dirt50", "enhanced-matorig"],
    "views": [{"name": "game", "cam": "game", "t": 16}]}
# MUD BUILDUP OFF / MATERIALS (w-mudoff): computer riders after 70 s of riding (their mud) and the player's bike at the gate, through the saved options
# (mudon / mudoff, matsetorig: MATERIALS ORIGINAL; menumat: LIGHTING ORIGINAL, then the menu's pick of MATERIALS: ENHANCED)
PLANS["mudoff"] = {"event": "National/NAT_Stillwater", "prefix": "16_MudOff", "settle": 0, "perf": 0, "adapt": 1.0, "ai": 5,
    "modes": ["enhanced-mudon", "enhanced-mudoff", "enhanced-matsetorig-mudon", "enhanced-matsetorig-mudoff", "original-mudon", "original-mudoff", "original-menumat"],
    "views": [{"name": "opp0", "cam": "pose", "t": 70, "frame": "opp", "opp": 0, "offset": [3.6, 1.1, 2.8], "look": "anchor", "lookY": 0.55, "fov": 40},
              {"name": "opp0rider", "cam": "pose", "t": 70.5, "frame": "opp", "opp": 0, "offset": [2.2, 1.6, 2.6], "look": "anchor", "lookY": 1.2, "fov": 40},
              {"name": "opp1", "cam": "pose", "t": 71, "frame": "opp", "opp": 1, "offset": [-3.4, 1.0, -3.0], "look": "anchor", "lookY": 0.5, "fov": 40},
              {"name": "player", "cam": "pose", "t": 71.5, "frame": "bike", "offset": [4.2, 1.4, 3.8], "look": "anchor", "lookY": 0.6, "fov": 40}]}
for _k, _ev, _pre in [("matsunny", "National/NAT_Stillwater", "10_MatSunny"), ("matdusk", "Freestyle/FMX_Dover", "11_MatDusk"),
                      ("matstadium", "Supercross/SX_Kingston", "12_MatStadium"), ("matmud", "Terracross/TRX_Halifax", "13_MatHalifax")]:
    PLANS[_k] = {"event": _ev, "prefix": _pre, "settle": 0, "perf": 3, "modes": _MAT_MODES, "views": _MAT_VIEWS}

ap = argparse.ArgumentParser()
ap.add_argument("app", type=Path)
ap.add_argument("--shots", default="all")
ap.add_argument("--width", type=int, default=1920)
ap.add_argument("--height", type=int, default=1080)
ap.add_argument("--tier", type=int, default=2)
ap.add_argument("--seconds", type=int, default=900)
ap.add_argument("--settle", type=float, default=8)
ap.add_argument("--plan", type=Path, help="a plan JSON of your own instead of --shots")
ap.add_argument("--scout", type=float, default=0, help="scouting: freeze the game's own camera every N seconds of the first 30 (ORIGINAL only) instead of the planned views")
ap.add_argument("--game", help="the game files directory (default: the sol_garage extraction; an Alive overlay such as builds/alive/combined/map-a5 for the Alive tracks)")
ap.add_argument("--env", action="append", default=[], help="KEY=VALUE for the player (repeatable; e.g. REFLEX_ALIVE_LOOK=reflex)")
ap.add_argument("--out", type=Path, help="the run directory (default builds/lighting/runs/<stamp>)")
ap.add_argument("--batch", action="store_true", help="-batchmode: no window at all; the fixture renders the camera into a target (w-fresnolook)")
a = ap.parse_args()
if a.game: G = str(Path(a.game).resolve())
assert Path(G).is_dir(), "missing the game-data extraction: " + G
names = list(PLANS) if a.shots == "all" else a.shots.split(",")
A = a.out.resolve() if a.out else R / "builds/lighting/runs" / datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
A.mkdir(parents=True)
app = A / "Lighting.app"
shutil.copytree(a.app, app, symlinks=True)
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
shipped = info["CFBundleIdentifier"]
test_id = "local.reflex.lighting.test-" + A.name.lower()
info.update(CFBundleIdentifier=test_id, CFBundleName="Lighting Test")
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
shots = A / "screenshots"; shots.mkdir()
results = {}
plans = [(a.plan.stem, json.loads(a.plan.read_text()))] if a.plan else [(n, PLANS[n]) for n in names]
for name, plan in plans:
    plan = dict(plan); plan.setdefault("settle", a.settle)
    if a.scout > 0:
        plan.update(settle=0, adapt=0.6, perf=0, modes=["original"], prefix="scout_" + plan["prefix"],
                    views=[{"name": "t%02d" % int(t), "cam": "game", "t": t} for t in [a.scout * k for k in range(1, int(30 / a.scout) + 1)]])
    pf = A / f"plan-{name}.json"; pf.write_text(json.dumps(plan, indent=1))
    save = A / f"save-{name}"; save.mkdir()
    # the ENHANCED tier: graphics.cfg ShadowQuality in the scratch save (the per-user copy ReflexLaunchSettings reads first)
    cache = save / "ReflexCache"; cache.mkdir()
    if os.environ.get("SHOWCASE_SAVE_JSON"): shutil.copy(os.environ["SHOWCASE_SAVE_JSON"], save / "save.json")   # seed settings (e.g. the camera)
    log = A / f"player-{name}.log"
    args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0",
            "-screen-width", str(a.width), "-screen-height", str(a.height), "-logFile", str(log)]
    if a.batch: args.insert(4, "-batchmode")
    env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
    env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=G, REFLEX_SKIP_INTRO="1",
               REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_LIGHTING_PLAN=str(pf), REFLEX_ENH_SKY_DEBUG=str(shots), REFLEX_FPS_CAP="240", REFLEX_LIGHTING_TIER=str(a.tier), REFLEX_RENDERER_LOG=os.environ.get("RENDERER_LOG", ""), REFLEX_RENDERER_DUMP=os.environ.get("RENDERER_DUMP", ""))
    env.update(dict(kv.split("=", 1) for kv in a.env))
    if a.batch: env["REFLEX_SHOT_SIZE"] = f"{a.width}x{a.height}"
    timedout = False
    with (A / f"console-{name}.log").open("w") as clog:
        _front = nofocus.frontmost()
        child = subprocess.Popen(args, cwd=R, env=env, stdout=clog, stderr=subprocess.STDOUT)
        nofocus.hand_back(_front)
        print(name, A, "pid", child.pid, flush=True)
        try: code = child.wait(timeout=a.seconds)
        except subprocess.TimeoutExpired:
            timedout = True; child.terminate()
            try: code = child.wait(timeout=10)
            except subprocess.TimeoutExpired: child.kill(); code = child.wait()
    text = log.read_text(errors="replace") if log.exists() else ""
    lines = [l for l in text.splitlines() if "[lighting-showcase]" in l or "[Reflex lighting]" in l or "ReflexEnhanced" in l or "error CS" in l]
    results[name] = {"exit": code, "timedout": timedout, "lines": lines[-30:],
                     "shots": sorted(p.name for p in shots.glob(plan["prefix"] + "_*.png"))}
    print("\n".join(l[:400] for l in lines[-20:]))
    print(name, "exit", code, "timedout", timedout, "shots", results[name]["shots"], flush=True)
(A / "RESULT.json").write_text(json.dumps(results, indent=2))
print(A)
