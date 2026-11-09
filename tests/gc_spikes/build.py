#!/usr/bin/env python3
"""build.py <abs out dir> [--release] -- the GC SPIKES test player (test only, never shipped; w-gcspikes).

Stages a copy of Unity/Reflex (builds/gc_spikes/stage/Reflex; the working tree is not modified; a missing Library is seeded with an APFS
clone of REFLEX_GCSPIKES_LIBRARY, default the lead's player stage, so the first build does not reimport everything), then:
  * adds fixture/GcSpikeProbe.cs (races back to back: per-frame GC allocations, the GC count, the managed heap after each race, profiler
    captures with GC.Alloc call stacks), the mapped-input driver and authored route of tests/moto_career_flow (the Stillwater ride), and
    the Editor report fixture/GcAllocReport.cs;
  * enables main's editor-only test hooks the input driver needs (testHeldKeys, the copied race witness, the race-line observer), as
    tests/moto_career_flow/build.py does;
  * a DEVELOPMENT player (the profiler and the GC Allocated In Frame counter) unless --release (the shipped configuration: the perf line's
    numbers as a tester's player has them);
  * builds with OpenReflexBuild.BuildMacBatch and the working tree's core/build/librxcore.dylib (core/build.sh first).
Output: <out>/GcSpikes.app and build.log."""
import argparse, os, re, shutil, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
UNITY = "/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
ap = argparse.ArgumentParser()
ap.add_argument("out", type=Path)
ap.add_argument("--release", action="store_true")
a = ap.parse_args()
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
stage = R / "builds/gc_spikes/stage/Reflex"
core = R / "core/build/librxcore.dylib"
assert core.is_file(), "build the core first: core/build.sh"
stage.parent.mkdir(parents=True, exist_ok=True)
seed = Path(os.environ.get("REFLEX_GCSPIKES_LIBRARY", "/Users/payton/Documents/Development/REFLEX-lead/builds/player/stage/Reflex/Library"))
if not (stage / "Library").is_dir() and seed.is_dir():
    stage.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cp", "-cR", str(seed), str(stage / "Library")], check=True)   # clonefile: no extra disk until it diverges
subprocess.run(["rsync", "-a", "--delete", "--exclude", "Temp/", "--exclude", "Logs/", "--exclude", "Builds/", "--exclude", "/Library/",
                "--exclude", "/UserSettings/", str(R / "Unity/Reflex") + "/", str(stage) + "/"], check=True)
S = stage / "Assets/Scripts"
F = Path(__file__).resolve().parent / "fixture"
M = R / "tests/moto_career_flow/fixture"
dest = S / "Platform/GcSpikes"
dest.mkdir(parents=True, exist_ok=True)
shutil.copy2(F / "GcSpikeProbe.cs", dest / "GcSpikeProbe.cs")
shutil.copy2(M / "MotoInputDriver.cs", dest / "MotoInputDriver.cs")
shutil.copy2(M / "MotoAuthoredRoute.cs", dest / "MotoAuthoredRoute.cs")
(stage / "Assets/Editor/Profiling").mkdir(parents=True, exist_ok=True)
shutil.copy2(F / "GcAllocReport.cs", stage / "Assets/Editor/Profiling/GcAllocReport.cs")

def patch(path, pairs):
    s = path.read_text()
    for old, new in pairs:
        assert s.count(old) == 1, (path.name, old[:60])
        s = s.replace(old, new)
    path.write_text(s)

ow = S / "OriginalWorld/OriginalWorldRuntime.cs"
patch(ow, [
    ('#if UNITY_EDITOR\n        [Tooltip("Editor tests: DirectInput scan codes held in addition to the keyboard.")]\n        public int[] testHeldKeys = Array.Empty<int>();\n#endif\n',
     '        [Tooltip("Testing: DirectInput scan codes held in addition to the keyboard.")]\n        public int[] testHeldKeys = Array.Empty<int>();\n'),
    ('#if UNITY_EDITOR\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;\n#endif\n',
     '            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;\n'),
    ('        public ReflexRaceState Race => race;\n',
     '        public ReflexRaceState Race => race;\n'
     '        public uint TestCopiedRaceFrame { get; private set; }\n        public uint TestCopiedRacePoseFrame { get; private set; }\n'
     '        public bool TestCopiedRacePoseFinite { get; private set; }\n        public long TestCopiedRaceWorld { get; private set; }\n'
     '        public string TestCopiedRaceIdentity { get; private set; }\n'),
    ('                if (CheckFinite(done.bike)) {',
     '                TestCopiedRaceFrame = done.bike.frames; TestCopiedRacePoseFrame = done.bike.frames; TestCopiedRaceWorld = WorldGeneration; TestCopiedRaceIdentity = EventIdentity;\n'
     '                TestCopiedRacePoseFinite = CheckFinite(done.bike);\n'
     '                if (TestCopiedRacePoseFinite) {'),
    ('        void ApplyView(OriginalSimulation.Results v)\n',
     '        public Action<float[], int, bool, uint> testRaceLineObserver;\n        void ApplyView(OriginalSimulation.Results v)\n'),
    ('            lastAlpha = p.Alpha; lastA = p.A.simTime; lastB = p.B.simTime;\n',
     '            lastAlpha = p.Alpha; lastA = p.A.simTime; lastB = p.B.simTime;\n'
     '            testRaceLineObserver?.Invoke(v.raceLine, v.raceLinePoints, v.raceLineClosed, v.bike.frames);\n'),
])
if not a.release:
    patch(stage / "Assets/Editor/Build/OpenReflexBuild.cs", [
        ("scenes = ReflexBuild.Scenes, locationPathName = output, target = BuildTarget.StandaloneOSX, options = BuildOptions.None",
         "scenes = ReflexBuild.Scenes, locationPathName = output, target = BuildTarget.StandaloneOSX, options = BuildOptions.Development"),
    ])

app = out / "GcSpikes.app"
env = dict(os.environ, REFLEX_RXCORE_PATH=str(core), REFLEX_OPEN_PLAYER=str(app))
r = subprocess.run(["perl", "-e", "alarm shift; exec @ARGV", "7200", UNITY, "-batchmode", "-nographics", "-quit",
                    "-projectPath", str(stage), "-executeMethod", "Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch",
                    "-logFile", str(out / "build.log")], env=env)
log = (out / "build.log").read_text(errors="replace") if (out / "build.log").exists() else ""
errors = sorted(set(re.findall(r".*error CS\d+.*", log)))
for e in errors[:60]: print(e)
ok = r.returncode == 0 and "[open-build] Succeeded" in log and app.is_dir()
print(("BUILT " if ok else "BUILD FAILED ") + str(app))
sys.exit(0 if ok else 1)
