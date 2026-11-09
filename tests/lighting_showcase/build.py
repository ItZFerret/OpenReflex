#!/usr/bin/env python3
"""build.py <abs out dir> [--stage NAME] -- builds the LIGHTING SHOWCASE test player (test only, never shipped).

Stages a copy of Unity/Reflex (builds/lighting/stage-NAME/Reflex; the working tree is not modified), adds fixture/LightingShowcase.cs
(loads one event, holds the camera on planned views, takes matched ORIGINAL / ENHANCED screenshots and frame times), switches on the
frame timing stats in the staged copy only (GPU times), and builds the player with OpenReflexBuild.BuildMacBatch and the working tree's
core/build/librxcore.dylib (core/build.sh first). Output: <out>/Lighting.app and build.log."""
import argparse, os, re, shutil, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
UNITY = "/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
ap = argparse.ArgumentParser()
ap.add_argument("out", type=Path)
ap.add_argument("--stage", default="main")
a = ap.parse_args()
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
stage = R / "builds/lighting" / ("stage-" + a.stage) / "Reflex"
core = R / "core/build/librxcore.dylib"
assert core.is_file(), "build the core first: core/build.sh"
stage.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(["rsync", "-a", "--delete", "--exclude", "Temp/", "--exclude", "Logs/", "--exclude", "Builds/", "--exclude", "Library/",
                str(R / "Unity/Reflex") + "/", str(stage) + "/"], check=True)
dest = stage / "Assets/Scripts/Platform/LightingShowcase"
if dest.exists(): shutil.rmtree(dest)
dest.mkdir(parents=True)
for f in sorted((Path(__file__).resolve().parent / "fixture").glob("*.cs")): shutil.copy2(f, dest / f.name)
ps = stage / "ProjectSettings/ProjectSettings.asset"
s = ps.read_text()
ps.write_text(s.replace("enableFrameTimingStats: 0", "enableFrameTimingStats: 1"))
app = out / "Lighting.app"
env = dict(os.environ, REFLEX_RXCORE_PATH=str(core), REFLEX_OPEN_PLAYER=str(app))
r = subprocess.run(["perl", "-e", "alarm shift; exec @ARGV", "7200", UNITY, "-batchmode", "-nographics", "-quit",
                    "-projectPath", str(stage), "-executeMethod", "Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch",
                    "-logFile", str(out / "build.log")], env=env)
log = (out / "build.log").read_text(errors="replace") if (out / "build.log").exists() else ""
errors = sorted(set(re.findall(r".*(?:error CS\d+|Shader error).*", log)))
for e in errors[:60]: print(e)
ok = r.returncode == 0 and "[open-build] Succeeded" in log and app.is_dir()
print(("BUILT " if ok else "BUILD FAILED ") + str(app))
sys.exit(0 if ok else 1)
