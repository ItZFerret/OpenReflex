#!/usr/bin/env python3
"""build.py <abs out dir> [--stage NAME] [--tree COMMIT] -- builds a UI SHOWCASE test player (test only, never shipped).

Stages a copy of Unity/Reflex (builds/menus/stage-NAME/Reflex; the working tree is not modified), adds the fixture files of
tests/menus_showcase/fixture/ (UiShowcase.cs drives the pages with synthetic data and takes screenshots; no race is ridden), and
builds the player with OpenReflexBuild.BuildMacBatch and the working tree's core/build/librxcore.dylib (core/build.sh first). The stage's
Library is kept between builds (an incremental import).
Output: <out>/Showcase.app and build.log. Several agents can build at once with different --stage names."""
import argparse, os, re, shutil, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
UNITY = "/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
ap = argparse.ArgumentParser()
ap.add_argument("out", type=Path)
ap.add_argument("--stage", default="main")
ap.add_argument("--tree", default=None, help="stage COMMIT's Unity/Reflex (git archive) instead of the working tree's (a before / after comparison; the fixture is the working tree's)")
ap.add_argument("--frame-timing", action="store_true", help="the staged copy's PlayerSettings.enableFrameTimingStats on (FrameTimingManager: CPU / GPU frame times in the test player)")
a = ap.parse_args()
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
stage = R / "builds/menus" / ("stage-" + a.stage) / "Reflex"
core = R / "core/build/librxcore.dylib"
assert core.is_file(), "build the core first: core/build.sh"
stage.parent.mkdir(parents=True, exist_ok=True)
src = R / "Unity/Reflex"
if a.tree:
    import io, tarfile
    tmp = R / "builds/menus" / ("tree-" + a.stage)
    if tmp.exists(): shutil.rmtree(tmp)
    tmp.mkdir(parents=True)
    data = subprocess.run(["git", "-C", str(R), "archive", a.tree, "Unity/Reflex"], check=True, capture_output=True).stdout
    tarfile.open(fileobj=io.BytesIO(data)).extractall(tmp)
    src = tmp / "Unity/Reflex"
subprocess.run(["rsync", "-a", "--delete", "--exclude", "Temp/", "--exclude", "Logs/", "--exclude", "Builds/", "--exclude", "/Library/", "--exclude", "/UserSettings/",
                str(src) + "/", str(stage) + "/"], check=True)
if a.frame_timing:
    ps = stage / "ProjectSettings/ProjectSettings.asset"
    ps.write_text(ps.read_text().replace("enableFrameTimingStats: 0", "enableFrameTimingStats: 1"))
dest = stage / "Assets/Scripts/Platform/Showcase"
if dest.exists(): shutil.rmtree(dest)
dest.mkdir(parents=True)
for f in sorted((Path(__file__).resolve().parent / "fixture").glob("*.cs")): shutil.copy2(f, dest / f.name)
app = out / "Showcase.app"
env = dict(os.environ, REFLEX_RXCORE_PATH=str(core), REFLEX_OPEN_PLAYER=str(app))
r = subprocess.run(["perl", "-e", "alarm shift; exec @ARGV", "5400", UNITY, "-batchmode", "-nographics", "-quit",
                    "-projectPath", str(stage), "-executeMethod", "Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch",
                    "-logFile", str(out / "build.log")], env=env)
log = (out / "build.log").read_text(errors="replace") if (out / "build.log").exists() else ""
errors = sorted(set(re.findall(r".*error CS\d+.*", log)))
for e in errors[:60]: print(e)
ok = r.returncode == 0 and "[open-build] Succeeded" in log and app.is_dir()
print(("BUILT " if ok else "BUILD FAILED ") + str(app))
sys.exit(0 if ok else 1)
