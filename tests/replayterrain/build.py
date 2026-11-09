#!/usr/bin/env python3
"""build.py <abs out dir> [--stage NAME] [--src DIR] [--seed LIBRARY] -- builds the REPLAY TERRAIN showcase test player (w-replayterrain, test only).

Like tests/cinematic_showcase/build.py: stages a copy of a Unity/Reflex tree (--src, default the working tree's; e.g. an export of main for
the "before" pictures) in builds/lighting/stage-NAME/Reflex (its Library kept between builds; --seed: an APFS clone of another stage's
Library on the first build), adds fixture/ReplayTerrainShowcase.cs, turns on the editor-only riding hook (testHeldKeys) in the staged copy
and builds the player with OpenReflexBuild.BuildMacBatch and the working tree's core/build/librxcore.dylib. Output: <out>/RT.app, build.log."""
import argparse, os, re, shutil, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
UNITY = "/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
ap = argparse.ArgumentParser()
ap.add_argument("out", type=Path)
ap.add_argument("--stage", default="replayterrain")
ap.add_argument("--src", type=Path, default=R / "Unity/Reflex")
ap.add_argument("--seed", type=Path)
a = ap.parse_args()
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
stage = R / "builds/lighting" / ("stage-" + a.stage) / "Reflex"
core = R / "core/build/librxcore.dylib"
assert core.is_file(), "the core is missing: core/build/librxcore.dylib"
stage.mkdir(parents=True, exist_ok=True)
if a.seed and not (stage / "Library").exists():
    subprocess.run(["cp", "-cR", str(a.seed), str(stage / "Library")], check=True)   # APFS clone: no copy of the data
subprocess.run(["rsync", "-a", "--delete", "--exclude", "Temp/", "--exclude", "Logs/", "--exclude", "Builds/", "--exclude", "Library/",
                str(a.src.resolve()) + "/", str(stage) + "/"], check=True)
dest = stage / "Assets/Scripts/Platform/ReplayTerrainShowcase"
if dest.exists(): shutil.rmtree(dest)
dest.mkdir(parents=True)
for f in sorted((Path(__file__).resolve().parent / "fixture").glob("*.cs")): shutil.copy2(f, dest / f.name)
rtp = stage / "Assets/Scripts/OriginalWorld/OriginalWorldRuntime.cs"; t = rtp.read_text()
for old, new in [('#if UNITY_EDITOR\n        [Tooltip("Editor tests: DirectInput scan codes held in addition to the keyboard.")]\n        public int[] testHeldKeys = Array.Empty<int>();\n#endif', '        public int[] testHeldKeys = Array.Empty<int>();'),
                 ('#if UNITY_EDITOR\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;\n#endif', '            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;')]:
    assert t.count(old) == 1, old; t = t.replace(old, new)
t = t.replace('            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;', '            if (Environment.GetEnvironmentVariable("REFLEX_RT_PLAN") != null) Array.Clear(keys, 0, keys.Length);\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;')
t = t.replace('uint mask = ReadPads(!menu && !paused);', 'uint mask = ReadPads(!menu && !paused); if (Environment.GetEnvironmentVariable("REFLEX_RT_PLAN") != null) { mask = 0; Array.Clear(pads, 0, pads.Length); }')
rtp.write_text(t)
app = out / "RT.app"
env = dict(os.environ, REFLEX_RXCORE_PATH=str(core), REFLEX_OPEN_PLAYER=str(app))
r = subprocess.run([UNITY, "-batchmode", "-nographics", "-quit",
                    "-projectPath", str(stage), "-executeMethod", "Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch",
                    "-logFile", str(out / "build.log")], env=env)
log = (out / "build.log").read_text(errors="replace") if (out / "build.log").exists() else ""
errors = sorted(set(re.findall(r".*(?:error CS\d+|Shader error).*", log)))
for e in errors[:60]: print(e)
ok = r.returncode == 0 and "[open-build] Succeeded" in log and app.is_dir()
print(("BUILT " if ok else "BUILD FAILED ") + str(app))
sys.exit(0 if ok else 1)
