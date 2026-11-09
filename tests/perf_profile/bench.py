#!/usr/bin/env python3
"""bench.py -- the scene benchmark (test only, never shipped): fixture/PerfBench.cs in a staged copy of Unity/Reflex.

  bench.py build <abs out dir> [--tree COMMIT] [--stage NAME] [--dev]
      Stages Unity/Reflex (the working tree, or COMMIT's tree: `git archive`) into builds/perf_profile/stage-NAME/Reflex (its Library
      is kept between builds and seeded from another stage when missing), adds fixture/PerfBench.cs, switches on Frame Timing Stats in
      the staged copy only, builds <out>/Bench.app with OpenReflexBuild.BuildMacBatch and core/build/librxcore.dylib. --dev: a
      Development build (Unity's GPU recorders need one on some platforms; CPU times are then a little higher).
  bench.py run <Bench.app> [--scenes national,supercross,alpine,menu] [--res 1920x1080,2560x1440] [--tiers 0,1,2] [--quick]
      One player process per scene, one after the other (never in parallel: they would share the GPU). Sandboxed like
      tests/lighting_showcase (unique bundle id, scratch save, writes to the user's preferences / application support denied), App Nap
      off for the copy (NSAppSleepDisabled / LSAppNapIsDisabled), focus handed back (tests/nofocus.py), frame cap 240 (VSync off).
      Output: builds/perf_profile/bench/<stamp>/ (player logs, screenshots/<scene>_*.png, <scene>_perf.json, SUMMARY.md).
  bench.py table <run dir> [<run dir> ...]   -- SUMMARY.md of one run, or a before / after table of two (or more) runs.
  Input latency (w-latency): `run ... --live 20 --env REFLEX_LATENCY_PROBE=1` (fixture/LatencyProbe.cs: a virtual keyboard toggles W, the
  player log's "[latency]" lines give the Unity frames / ms until the presented game frame ran with it); add --env REFLEX_PACING=...
"""
import argparse, datetime, json, os, plistlib, re, shutil, subprocess, sys, tarfile, io
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus

R = Path(__file__).resolve().parents[2]
UNITY = "/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
G = "/Users/payton/Documents/Development/REFLEX/builds/sol_garage/game files"
SEEDS = [R / "builds/perf_profile", Path("/Users/payton/Documents/Development/REFLEX-lead/builds/player/stage/Reflex/Library")]   # an imported Library (APFS clone)

SCENES = {
    # a 12-rider National at the gate (the game's own camera, the field lined up)
    # (posed: the frozen game frame can differ by a frame between builds; a posed camera at the bike keeps before / after views matched)
    "national": {"scene": "National/NAT_Stillwater", "prefix": "national", "ai": 11, "t": 15,
                 "view": {"cam": "pose", "frame": "bike", "offset": [3, 2.5, -8], "look": "anchor", "lookY": 1.2}},
    # a National late in the race (w-terrtiles): 11 AI, 240 s of race, the field strung out and the tyre-track rings full; use with
    # --modes original --tiers 2 --no-ablations --no-breakdown --window 1 --live 20 so that only the live window (the world running) measures
    "renne_late": {"scene": "National/NAT_Renne", "prefix": "renne_late", "ai": 11, "t": 240,
                   "view": {"cam": "pose", "frame": "bike", "offset": [3, 2.5, -8], "look": "anchor", "lookY": 1.2}},
    # the same race looking down at the ground beside the parked bike (tyre tracks / ruts in view; before / after screenshots of the tracks)
    "renne_ground": {"scene": "National/NAT_Renne", "prefix": "renne_ground", "ai": 11, "t": 100,
                     "view": {"cam": "pose", "frame": "bike", "offset": [2, 9, -3], "look": "anchor", "lookY": 0}},
    # a Supercross stadium with its crowds, at the gate
    "supercross": {"scene": "Supercross/SX_Kingston", "prefix": "supercross", "ai": 11, "t": 15,
                   "view": {"cam": "pose", "frame": "bike", "offset": [-3, 3, -10], "look": "anchor", "lookY": 1.2}},
    # Free Ride Alpine: the lake, the forest, the mountains (a posed camera over the water towards the sun's side)
    "alpine": {"scene": "FreeRide/FR_Alpine", "prefix": "alpine", "ai": 0, "t": 13,
               "view": {"cam": "pose", "frame": "sun", "offset": [0, 40, 70], "look": "yawpitch", "yaw": 180, "pitch": -14, "fov": 70}},
    # the front end's 3D menu hub (UI_Compound behind the UI Toolkit hub)
    "menu": {"scene": "menu", "prefix": "menu", "ai": 0, "t": 10},
    # converted MX vs ATV Alive levels (the player's game files overlay map-a5: the scenery shader's compiled Alive programs)
    # Quarry Basin: the forest-heavy one (trees, billboards, the volume shadow), at the bike's spawn
    "quarry": {"scene": "FreeRide/NAT_DLC_Quarry_A", "prefix": "quarry", "ai": 0, "t": 8, "game": "alive"},
    # Devil's Playground: the whole-program grass cliffs (up to 430 ops per pixel, 12 textures), posed towards the cliffs
    "dunes": {"scene": "FreeRide/FR_Dunes", "prefix": "dunes", "ai": 0, "t": 8, "game": "alive"},
    # Lexington: an Alive Supercross stadium (glass / window / banner whole programs, the crowd)
    "sx01": {"scene": "FreeRide/SX_DLC_SX01", "prefix": "sx01", "ai": 0, "t": 8, "game": "alive"},
    # RIDER CAP (w-rc-client): the whole field in view at the gate (the camera ahead of the player's gate slot, high, looking back over the
    # gate and the rows behind it); run with --env REFLEX_RIDERS=12 / 24 / 32 (REFLEX_BIGFIELD_LOD=0: a big field drawn in full)
    "national_field": {"scene": "National/NAT_Stillwater", "prefix": "national_field", "ai": 2, "t": 15,
                       "view": {"cam": "pose", "frame": "bike", "offset": [0, 16, 45], "look": "anchor", "lookY": 1.2, "fov": 70}},
    "supercross_field": {"scene": "Supercross/SX_Kingston", "prefix": "supercross_field", "ai": 2, "t": 15,
                         "view": {"cam": "pose", "frame": "bike", "offset": [0, 16, 45], "look": "anchor", "lookY": 1.2, "fov": 70}},
    # an Alive National (Quarry Basin, 110 %; the race overlay map-a7)
    "alive_field": {"scene": "National/NAT_DLC_Quarry_A", "prefix": "alive_field", "ai": 2, "t": 15, "game": "alive_race",
                    "view": {"cam": "pose", "frame": "bike", "offset": [0, 16, 45], "look": "anchor", "lookY": 1.2, "fov": 70}},
}
GAMES = {"base": G, "alive": str(R / "builds/alive/combined/map-a5"),
         "alive_race": os.environ.get("REFLEX_BENCH_ALIVE_RACE", str(R / "builds/alive/combined/map-a7"))}


def conditions():
    """What else uses the GPU / display while a run measures: the Chrome Remote Desktop host (a connected session encodes the screen
    on the GPU: earlier runs saw frame rates roughly halved), its 'Remoting session is active' power assertion, the busiest processes."""
    out = {"utc": datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")}
    # the machine's load and every other Reflex / Unity process (other agents' test players and builds share the CPU / GPU)
    try:
        out["load_avg"] = [round(x, 2) for x in os.getloadavg()]
        pg = subprocess.run(["pgrep", "-fl", "Reflex|Unity|AliveTest|MotoFlow"], capture_output=True, text=True, timeout=10).stdout.splitlines()
        out["other_processes"] = [l[:200] for l in pg if "pgrep" not in l and "bench.py" not in l][:20]
    except Exception as e:
        out["load_error"] = str(e)
    try:
        ps = subprocess.run(["ps", "-Ao", "pid,pcpu,comm"], capture_output=True, text=True, timeout=10).stdout.splitlines()[1:]
        rows = []
        for l in ps:
            f = l.split(None, 2)
            if len(f) == 3:
                try: rows.append((float(f[1]), f[2].rsplit("/", 1)[-1], int(f[0])))
                except ValueError: pass
        out["remoting_host_cpu"] = sum(c for c, n, _ in rows if n.startswith("remoting_me2me_host") and "service" not in n)
        out["busiest"] = [f"{n} {c:.0f}%" for c, n, _ in sorted(rows, reverse=True)[:6]]
        a = subprocess.run(["pmset", "-g", "assertions"], capture_output=True, text=True, timeout=10).stdout
        out["remote_session_assertion"] = "Remoting session is active" in a
    except Exception as e:
        out["error"] = str(e)
    # a sampled CPU share over 2 s is what tells a connected (streaming) session from an idle host
    try:
        t = subprocess.run(["top", "-l", "2", "-s", "2", "-n", "8", "-stats", "pid,cpu,command", "-o", "cpu"], capture_output=True, text=True, timeout=20).stdout
        last = t.split("PID")[-1].splitlines()[1:]
        out["top_2s"] = [" ".join(x.split()[1:3]) for x in last if x.strip()][:8]
        out["remoting_host_cpu_2s"] = sum(float(x.split()[1]) for x in last if len(x.split()) >= 3 and x.split()[2].startswith("remoting_me2me"))
    except Exception as e:
        out["top_error"] = str(e)
    out["remote_connected"] = bool(out.get("remote_session_assertion")) and out.get("remoting_host_cpu_2s", 0) > 3.0
    return out
ABLATIONS = ["noshadow", "noocc", "nocontact", "noscenery", "notrees", "cascades2", "noopaque", "o:noopaque", "o:nodepthtex", "noalivevm", "o:noalivevm", "o:noalivevmall",
             "o:nopost", "o:hide-terrain", "o:hide-forest", "o:hide-Water", "o:hide-Crowd", "o:hide-Scenery"]


def build(a):
    out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    stage = R / "builds/perf_profile" / ("stage-" + a.stage) / "Reflex"
    core = Path(a.core).resolve() if a.core else R / "core/build/librxcore.dylib"
    assert core.is_file(), "build the core first: core/build.sh"
    stage.mkdir(parents=True, exist_ok=True)
    if not (stage / "Library").is_dir():
        seeds = sorted(R.glob("builds/perf_profile/stage-*/Reflex/Library")) + [SEEDS[1]]
        seed = next((s for s in seeds if s.is_dir() and s != stage / "Library"), None)
        if seed:
            print("seeding the Library from", seed, flush=True)
            subprocess.run(["cp", "-cR", str(seed), str(stage / "Library")], check=True)   # APFS clone
    src = R / "Unity/Reflex"
    if a.tree:
        tmp = R / "builds/perf_profile" / ("tree-" + a.stage)
        if tmp.exists(): shutil.rmtree(tmp)
        tmp.mkdir(parents=True)
        data = subprocess.run(["git", "-C", str(R), "archive", a.tree, "Unity/Reflex"], check=True, capture_output=True).stdout
        tarfile.open(fileobj=io.BytesIO(data)).extractall(tmp)
        src = tmp / "Unity/Reflex"
    subprocess.run(["rsync", "-a", "--delete", "--exclude", "Temp/", "--exclude", "Logs/", "--exclude", "Builds/", "--exclude", "Library/",
                    str(src) + "/", str(stage) + "/"], check=True)
    dest = stage / "Assets/Scripts/Platform/PerfBench"
    if dest.exists(): shutil.rmtree(dest)
    dest.mkdir(parents=True)
    shutil.copy2(Path(__file__).resolve().parent / "fixture/PerfBench.cs", dest / "PerfBench.cs")
    shutil.copy2(Path(__file__).resolve().parent / "fixture/LatencyProbe.cs", dest / "LatencyProbe.cs")   # inert unless REFLEX_LATENCY_PROBE=1
    res = stage / "Assets/Resources/PerfBenchShaders"
    if res.exists(): shutil.rmtree(res)
    if a.extra_shaders:   # test variants of shipped shaders (the bench's swap=<shader>=<variant> ablations load them from Resources)
        res.mkdir(parents=True)
        for f in sorted(Path(a.extra_shaders).glob("*.shader")): shutil.copy2(f, res / f.name)
    if a.probe_sim: subprocess.run([sys.executable, str(Path(__file__).resolve().parent / "fixture/sim_probe.py"), str(stage)], check=True)
    if a.probe_terrain: subprocess.run([sys.executable, str(Path(__file__).resolve().parent / "fixture/terrain_probe.py"), str(stage)], check=True)
    ps = stage / "ProjectSettings/ProjectSettings.asset"
    ps.write_text(ps.read_text().replace("enableFrameTimingStats: 0", "enableFrameTimingStats: 1"))
    gs = stage / "ProjectSettings/GraphicsSettings.asset"; t = gs.read_text()
    t = t.replace("m_LogWhenShaderIsCompiled: 1", "m_LogWhenShaderIsCompiled: 0")
    if a.log_shaders:   # (a development player logs every shader variant it compiles at run time: the startup / load stalls)
        assert "m_LogWhenShaderIsCompiled: 0" in t, "GraphicsSettings: m_LogWhenShaderIsCompiled moved"
        t = t.replace("m_LogWhenShaderIsCompiled: 0", "m_LogWhenShaderIsCompiled: 1")
    gs.write_text(t)
    if a.dev:
        b = stage / "Assets/Editor/Build/OpenReflexBuild.cs"; s = b.read_text()
        old = "target = BuildTarget.StandaloneOSX, options = BuildOptions.None"
        assert s.count(old) == 1, "OpenReflexBuild: the mac build options line moved"
        b.write_text(s.replace(old, "target = BuildTarget.StandaloneOSX, options = BuildOptions.Development"))
    app = out / "Bench.app"
    env = dict(os.environ, REFLEX_RXCORE_PATH=str(core), REFLEX_OPEN_PLAYER=str(app))
    r = subprocess.run(["perl", "-e", "alarm shift; exec @ARGV", "7200", UNITY, "-batchmode", "-nographics", "-quit",
                        "-projectPath", str(stage), "-executeMethod", "Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch",
                        "-logFile", str(out / "build.log")], env=env)
    log = (out / "build.log").read_text(errors="replace") if (out / "build.log").exists() else ""
    errors = sorted(set(re.findall(r".*(?:error CS\d+|Shader error).*", log)))
    for e in errors[:60]: print(e)
    ok = r.returncode == 0 and "[open-build] Succeeded" in log and app.is_dir()
    (out / "BUILT_FROM.txt").write_text((a.tree or "working tree " + subprocess.run(["git", "-C", str(R), "log", "--oneline", "-1"], capture_output=True, text=True).stdout.strip()) + ("\n(dev)" if a.dev else "") + "\n")
    print(("BUILT " if ok else "BUILD FAILED ") + str(app))
    sys.exit(0 if ok else 1)


def prepare(src, A, tag):
    """A sandboxed copy of a bench player in run dir A: unique bundle id, App Nap off, the protection profile; returns its executable."""
    A.mkdir(parents=True, exist_ok=True)
    (A / "APP.txt").write_text(str(src.resolve()) + "\n" + ((src.parent / "BUILT_FROM.txt").read_text() if (src.parent / "BUILT_FROM.txt").exists() else ""))
    app = A / "Bench.app"
    if app.exists(): shutil.rmtree(app)
    shutil.copytree(src, app, symlinks=True)
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    exe = app / "Contents/MacOS" / info["CFBundleExecutable"]
    shipped = info["CFBundleIdentifier"]
    info.update(CFBundleIdentifier="local.reflex.perfbench.test-" + tag.lower().replace("_", "-"), CFBundleName="Perf Bench Test",
                NSAppSleepDisabled=True, LSAppNapIsDisabled=True)   # App Nap would throttle a background window
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
    for g in GAMES.values():
        sb += "(deny file-write* (subpath %s))\n" % json.dumps(str(Path(g).resolve()))
    (A / "protection.sb").write_text(sb)
    subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(app)], check=True, capture_output=True)
    return exe


def run_scene(exe, A, name, a):
    shots = A / "screenshots"; shots.mkdir(exist_ok=True)
    res = [int(x) for r_ in a.res.split(",") for x in r_.split("x")]
    tiers = [int(t) for t in a.tiers.split(",")]
    plan = dict(SCENES[name]); plan.update(res=res, tiers=tiers, ablations=[] if a.quick or a.no_ablations else (a.ablations.split(",") if a.ablations else ABLATIONS))
    gk = plan.pop("game", "base"); game = GAMES[gk]
    if gk != "alive": plan["ablations"] = [x for x in plan["ablations"] if "alivevm" not in x]
    assert Path(game).is_dir(), "missing the game files: " + game
    if a.quick: plan.update(window=2, breakdown=0, live=3)
    if a.window: plan.update(window=a.window)
    if a.no_breakdown: plan.update(breakdown=0)
    if a.live >= 0: plan.update(live=a.live)
    if a.ai >= 0: plan.update(ai=a.ai)
    if a.identity: plan.update(identity=True)
    if a.modes: plan.update(modes=a.modes.split(","))
    if a.interleave: plan.update(interleave=a.interleave)
    pf = A / f"plan-{name}.json"; pf.write_text(json.dumps(plan, indent=1))
    save = A / f"save-{name}"; (save / "ReflexCache").mkdir(parents=True, exist_ok=True)
    log = A / f"player-{name}.log"
    args = ["/usr/bin/sandbox-exec", "-f", str(A / "protection.sb"), str(exe), "-reflexTestWindow", "-screen-fullscreen", "0",
            "-screen-width", str(res[0]), "-screen-height", str(res[1]), "-logFile", str(log)]
    if a.batch: args.insert(4, "-batchmode")   # no window: frames are not held by the window server (a background window's drawables / display link)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("REFLEX_", "RX_", "DYLD_"))}
    env.update(REFLEX_SIM="open", REFLEX_FRONTEND="open", REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=game, REFLEX_SKIP_INTRO="1",
               REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_PERF_PLAN=str(pf), REFLEX_NO_WELCOME="1")
    if a.fps_cap > 0: env["REFLEX_FPS_CAP"] = str(a.fps_cap)   # 0: the player's own default (DISPLAY RATE: VSync, no cap)
    for kv in a.env:
        k, _, v = kv.partition("="); assert k.startswith("REFLEX_"), kv; env[k] = v
    cond = {"before": conditions()}
    timedout = False
    with (A / f"console-{name}.log").open("w") as clog:
        front = nofocus.frontmost()
        child = subprocess.Popen(args, cwd=R, env=env, stdout=clog, stderr=subprocess.STDOUT)
        nofocus.hand_back(front)
        print(name, A, "pid", child.pid, flush=True)
        if a.sample > 0 or a.sample_load > 0:
            import time
            t0 = time.time(); key = "LIVE BEGIN" if a.sample > 0 else a.sample_key
            while child.poll() is None and time.time() - t0 < a.seconds:
                if log.exists() and key in log.read_text(errors="replace"): break
                time.sleep(0.2)
            if child.poll() is None:   # (--sample-load: from --sample-key, every thread)
                subprocess.run(["sample", str(child.pid), str(int(a.sample or a.sample_load)), "1", "-file", str(A / f"sample-{name}{'' if a.sample > 0 else '-load'}.txt")], capture_output=True)
        try: code = child.wait(timeout=a.seconds)
        except subprocess.TimeoutExpired:
            timedout = True; child.terminate()
            try: code = child.wait(timeout=10)
            except subprocess.TimeoutExpired: child.kill(); code = child.wait()
    cond["after"] = conditions()
    (A / f"conditions-{name}.json").write_text(json.dumps(cond, indent=1))
    text = log.read_text(errors="replace") if log.exists() else ""
    for l in text.splitlines():
        if "[perf-bench]" in l and ("WINDOW" in l or "FAIL" in l or "RUNNING" in l or "RES " in l): print(l[:300])
    print(name, "exit", code, "timedout", timedout, "remote connected", cond["before"].get("remote_connected"), cond["after"].get("remote_connected"),
          "load", cond["before"].get("load_avg"), cond["after"].get("load_avg"), "others", len(cond["before"].get("other_processes", [])), flush=True)
    src = shots / (plan["prefix"] + "_perf.json")
    if src.exists():
        rep = json.loads(src.read_text()); rep["conditions"] = cond
        (A / src.name).write_text(json.dumps(rep, indent=1))


def run(a):
    A = R / "builds/perf_profile/bench" / (datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ") + ("-" + a.tag if a.tag else ""))
    exe = prepare(a.app, A, A.name)
    for name in a.scenes.split(","): run_scene(exe, A, name, a)
    table([A], A / "SUMMARY.md")
    print(A)


def ab(a):
    """A / B interleaved: per round and scene, both players one after the other (the order flips every round: A B, B A, ...), so a
    remote-desktop session or another load affects both the same; each variant's results go to <run>/<label>-r<round>/."""
    stamp = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
    A = R / "builds/perf_profile/bench" / (stamp + "-ab")
    apps = [(l, Path(p)) for l, p in (x.split("=", 1) for x in a.apps)]
    exes = {l: prepare(p, A / ("app-" + l), stamp + "-" + l) for l, p in apps}
    dirs = {}
    for rnd in range(a.rounds):
        for name in a.scenes.split(","):
            order = apps if (rnd % 2 == 0) else list(reversed(apps))
            for l, _ in order:
                d = A / f"{l}-r{rnd}"; d.mkdir(parents=True, exist_ok=True)
                shutil.copy2(A / ("app-" + l) / "protection.sb", d / "protection.sb")
                run_scene(exes[l], d, name, a)
                dirs.setdefault(l, []).append(d)
    table(sorted({d for v in dirs.values() for d in v}, key=lambda d: (d.name.split("-r")[1], d.name)), A / "SUMMARY.md")
    print(A)


def load(run):
    out = {}
    for p in sorted(Path(run).glob("*_perf.json")):
        out[p.stem.replace("_perf", "")] = json.loads(p.read_text())
    return out


def table(runs, dest=None):
    runs = [Path(r) for r in runs]
    data = [load(r) for r in runs]
    lines = []
    if len(runs) == 1:
        for scene, rep in data[0].items():
            lines.append(f"### {scene} ({rep.get('scene')}; {rep.get('device')} {rep.get('api')}; load {rep.get('loadSeconds', 0):.1f} s, longest load frame {rep.get('loadMaxFrameMs', 0):.0f} ms, longest front-end frame {rep.get('frontEndMaxFrameMs', 0):.0f} ms)\n")
            lines.append("| window | offscreen ms (main thr) | frame ms (p95) | CPU main | render thr | GPU ms (p95) | GC B/frame | draws | SetPass | batches | tris M | casters | switch hitch ms | game fps | sim job / core ms |")
            lines.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
            for w in rep["windows"]:
                if w.get("variant") == "breakdown": continue
                lines.append(f"| {w['label']} {w.get('variant') if w.get('variant') not in ('', 'live') else ''} | {w.get('offscreenMs', 0):.2f} ({w.get('offscreenCpuMs', 0):.2f}) | {w['frameMs']:.2f} ({w['frameP95']:.2f}) | {w['mainMs']:.2f} | {w['renderMs']:.2f} | {w['gpuMs']:.2f} ({w['gpuP95']:.2f}) | {w['gcBytesPerFrame']:.0f} | {w['drawCalls']:.0f} | {w['setPass']:.0f} | {w['batches']:.0f} | {w['triangles'] / 1e6:.2f} | {w['shadowCasters']:.0f} | {w['switchHitchMs']:.0f} | {w.get('gameFps', 0):.1f} | " + (f"{w['simJobMs']:.2f} / {w['simFrameMs']:.2f}" if w.get('simJobs') else "-") + " |")
            for w in rep["windows"]:
                if w.get("variant") == "breakdown" and w.get("passes"):
                    lines.append(f"\n<details><summary>{w['label']}</summary>\n\n```\n" + "\n".join(w["passes"][:45]) + "\n```\n</details>")
            lines.append("")
    else:
        names = [r.name for r in runs]
        for scene in data[0]:
            lines.append(f"### {scene}\n")
            lines.append("| window | " + " | ".join(f"offscreen ms {n}" for n in names) + " | " + " | ".join(f"offscreen main {n}" for n in names) + " | " + " | ".join(f"GPU ms {n}" for n in names) + " | " + " | ".join(f"CPU main {n}" for n in names) + " | " + " | ".join(f"frame {n}" for n in names) + " | " + " | ".join(f"draws/SetPass {n}" for n in names) + " | " + " | ".join(f"GC B/f {n}" for n in names) + " |")
            lines.append("|---" * (1 + 7 * len(names)) + "|")
            for w in data[0][scene]["windows"]:
                if w.get("variant") == "breakdown": continue
                key = (w["label"], w.get("variant"))
                row = [next((x for x in d.get(scene, {}).get("windows", []) if (x["label"], x.get("variant")) == key), None) for d in data]
                f = lambda k, fmt="{:.2f}": " | ".join(fmt.format(x.get(k, 0)) if x else "-" for x in row)
                lines.append(f"| {w['label']} {w.get('variant') if w.get('variant') not in ('', 'live') else ''} | {f('offscreenMs')} | {f('offscreenCpuMs')} | {f('gpuMs')} | {f('mainMs')} | {f('frameMs')} | " + " | ".join(f"{x['drawCalls']:.0f}/{x['setPass']:.0f}" if x else "-" for x in row) + f" | {f('gcBytesPerFrame', '{:.0f}')} |")
            lines.append("")
    text = "\n".join(lines) + "\n"
    if dest: Path(dest).write_text(text)
    else: print(text)


ap = argparse.ArgumentParser()
sub = ap.add_subparsers(dest="cmd", required=True)
b = sub.add_parser("build"); b.add_argument("out", type=Path); b.add_argument("--tree"); b.add_argument("--stage", default="main"); b.add_argument("--dev", action="store_true")
b.add_argument("--core", help="the librxcore.dylib to bundle (default core/build's)")
b.add_argument("--log-shaders", action="store_true", help="the player logs each shader variant it compiles (needs --dev)")
b.add_argument("--extra-shaders", help="a directory of test shader variants for the swap= ablations (copied into the stage's Resources/PerfBenchShaders)")
b.add_argument("--probe-sim", action="store_true", help="time the simulation thread's job parts (fixture/sim_probe.py; test only)")
b.add_argument("--probe-terrain", action="store_true", help="time the main thread's terrain patches and tyre-track Apply (fixture/terrain_probe.py; test only; terrain-probe.txt in the run dir's shots)")
def scene_args(x):
    x.add_argument("--scenes", default="national,supercross,alpine,menu,quarry,dunes,sx01")
    x.add_argument("--res", default="1920x1080,2560x1440"); x.add_argument("--tiers", default="0,1,2"); x.add_argument("--quick", action="store_true")
    x.add_argument("--window", type=float, default=0); x.add_argument("--no-ablations", action="store_true"); x.add_argument("--no-breakdown", action="store_true")
    x.add_argument("--fps-cap", type=int, default=240, help="240: measures throughput (the GPU stays clocked up); pacing is not measured")
    x.add_argument("--env", action="append", default=[], metavar="K=V")
    x.add_argument("--live", type=float, default=-1, help="seconds of the live window (the world running) per mode")
    x.add_argument("--modes", default="", help="original,enhanced (default both)")
    x.add_argument("--ai", type=int, default=-1, help="AI riders (overrides the scene's)")
    x.add_argument("--identity", action="store_true", help="log the _ALPHATEST_ON identity check (fixture: Identity)")
    x.add_argument("--batch", action="store_true", help="-batchmode: no window (pacing without the window server's throttling of a background window)")
    x.add_argument("--tag", default="", help="a name suffix for the run directory")
    x.add_argument("--sample", type=float, default=0, help="macOS `sample` of the player for N s once the live window starts (all threads)")
    x.add_argument("--sample-load", type=float, default=0, help="macOS `sample` of the player for N s from the --sample-key line (default: the event's launch, after the front end settled)")
    x.add_argument("--sample-key", default="[perf-bench] LAUNCH", help="the log line that starts --sample-load ('[perf-bench] BEGIN': the front end's start too)")
    x.add_argument("--interleave", type=int, default=0, help="ablations: N offscreen measures with and without, in turns (medians; INTERLEAVED log lines)")
    x.add_argument("--ablations", default="", help="comma list instead of the default ablations (e.g. o:swap=Reflex/Original HFTerrain=PerfBench/T1)")
    x.add_argument("--seconds", type=int, default=900)
r_ = sub.add_parser("run"); r_.add_argument("app", type=Path); scene_args(r_)
ab_ = sub.add_parser("ab"); ab_.add_argument("apps", nargs="+", help="label=path/to/Bench.app"); ab_.add_argument("--rounds", type=int, default=2); scene_args(ab_)
t_ = sub.add_parser("table"); t_.add_argument("runs", nargs="+"); t_.add_argument("--out")
a = ap.parse_args()
if a.cmd == "build": build(a)
elif a.cmd == "run": run(a)
elif a.cmd == "ab": ab(a)
else: table(a.runs, a.out)
