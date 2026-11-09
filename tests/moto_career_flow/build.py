#!/usr/bin/env python3
"""build.py [out dir] -- builds the National Moto career-flow TEST player from the current main tree.

Test-only, never shipped. It stages a copy of Unity/Reflex (main is not modified), then:
  * adds the fixture files in fixture/: the career-flow probe, the arcade replay probe (run.py --arcade), the activation flow probe
    (run.py --activation), the post-race podium screenshots (PodiumShotProbe: watches only), the career series probe (run.py --series N),
    the eye adaptation / water log (LightWaterProbe: run.py --light-log; watches only), the Free Ride arrival scene (FreeRideArrivalProbe:
    run.py --arrival <level>; watches only), the ORIGINAL HUD ride (HudRestRideProbe: run.py --hudrest <HillClimb|Trailblazer|TargetPractice>; watches only), the stands crowd screenshots (CrowdShotProbe: run.py --crowd "<Activity>|<Activity>/<Level>"; holds the
    throttle only once the race runs),
    the mapped-input driver,
    the replay diagnostics, the authored route, and minimal ReflexSmoke / ReflexShotProbe hooks (guest profile before Awake; the
    probe chosen by the environment booted after the first scene);
  * enables main's editor-only test hooks in the staged copy: OriginalWorldRuntime.testHeldKeys and OriginalPacingProbe;
  * adds the probe's read-only witness of the completed native packet (TestCopiedRace*: frame, finite pose, world, event), its
    race-line observer (OriginalWorldRuntime.testRaceLineObserver) and the Garage identity (CareerWorker.ImportedVehicle);
  * builds the player with OpenReflexBuild.BuildMacBatch and main's core/build/librxcore.dylib.
Output: <out>/MotoFlow.app and build.log (default out: builds/moto_flow/<UTC stamp>)."""
import datetime, os, re, shutil, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
UNITY = "/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
out = Path(sys.argv[1]) if len(sys.argv) > 1 else R / "builds/moto_flow" / datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
out.mkdir(parents=True, exist_ok=True)
stage = R / "builds/moto_flow/stage/Reflex"
core = R / "core/build/librxcore.dylib"
assert core.is_file(), "build the core first: core/build.sh"

# 1. stage (the Library cache is copied too, so the build does not reimport everything)
stage.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(["rsync", "-a", "--delete", "--exclude", "Temp/", "--exclude", "Logs/", "--exclude", "Builds/", "--exclude", "/Library/", "--exclude", "/UserSettings/",
                str(R / "Unity/Reflex") + "/", str(stage) + "/"], check=True)
S = stage / "Assets/Scripts"

# 2. fixture files
F = Path(__file__).resolve().parent / "fixture"
place = {"ReflexSmoke.cs": "Platform", "ReflexShotProbe.cs": "Rendering", "MotoCareerFlowProbe.cs": "OpenFrontEnd",
         "MotoInputDriver.cs": "OpenFrontEnd", "ReplayFlowDiagnostics.cs": "OpenFrontEnd", "MotoAuthoredRoute.cs": "OpenFrontEnd",
         "ArcadeReplayFlowProbe.cs": "OpenFrontEnd", "ActivationFlowProbe.cs": "OpenFrontEnd", "PodiumShotProbe.cs": "OpenFrontEnd",
         "CareerSeriesFlowProbe.cs": "OpenFrontEnd", "LightWaterProbe.cs": "OpenFrontEnd",
         "FreeRideArrivalProbe.cs": "OpenFrontEnd", "CrowdShotProbe.cs": "OpenFrontEnd", "ReflexArrowsRideProbe.cs": "OpenFrontEnd",
         "HudRestRideProbe.cs": "OpenFrontEnd"}
for name, folder in place.items():
    shutil.copy2(F / name, S / folder / name)

def patch(path, pairs):
    s = path.read_text()
    for old, new in pairs:
        assert s.count(old) == 1, (path.name, old[:60])
        s = s.replace(old, new)
    path.write_text(s)

# 3. main's editor-only test hooks, enabled in the staged copy
ow = S / "OriginalWorld/OriginalWorldRuntime.cs"
patch(ow, [
    ('#if UNITY_EDITOR\n        [Tooltip("Editor tests: DirectInput scan codes held in addition to the keyboard.")]\n        public int[] testHeldKeys = Array.Empty<int>();\n#endif\n',
     '        [Tooltip("Testing: DirectInput scan codes held in addition to the keyboard.")]\n        public int[] testHeldKeys = Array.Empty<int>();\n'),
    ('#if UNITY_EDITOR\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;\n#endif\n',
     '            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;\n'),
    # 4. the probe's witness of the completed packet (the presented Bike may be interpolated)
    ('        public ReflexRaceState Race => race;\n',
     '        public ReflexRaceState Race => race;\n'
     '        public uint TestCopiedRaceFrame { get; private set; }\n        public uint TestCopiedRacePoseFrame { get; private set; }\n'
     '        public bool TestCopiedRacePoseFinite { get; private set; }\n        public long TestCopiedRaceWorld { get; private set; }\n'
     '        public string TestCopiedRaceIdentity { get; private set; }\n'),
    ('                if (CheckFinite(done.bike)) {',
     '                TestCopiedRaceFrame = done.bike.frames; TestCopiedRacePoseFrame = done.bike.frames; TestCopiedRaceWorld = WorldGeneration; TestCopiedRaceIdentity = EventIdentity;\n'
     '                TestCopiedRacePoseFinite = CheckFinite(done.bike);\n'
     '                if (TestCopiedRacePoseFinite) {'),
])
# the probe's race-line observer (synchronous, test only; no work when unset)
patch(ow, [
    ('        void ApplyView(OriginalSimulation.Results v)\n',
     '        public Action<float[], int, bool, uint> testRaceLineObserver;\n        void ApplyView(OriginalSimulation.Results v)\n'),
    ('            lastAlpha = p.Alpha; lastA = p.A.simTime; lastB = p.B.simTime;\n',
     '            lastAlpha = p.Alpha; lastA = p.A.simTime; lastB = p.B.simTime;\n'
     '            testRaceLineObserver?.Invoke(v.raceLine, v.raceLinePoints, v.raceLineClosed, v.bike.frames);\n'),
])
# the career worker's imported / resolved Garage vehicle identity, readable by the probe
patch(S / "OpenReflex/CareerWorker.cs", [
    ('        readonly object gate = new object();\n',
     '        readonly object gate = new object();\n'
     '        RxCareerVehicleIdentity? importedVehicle;\n'
     '        public RxCareerVehicleIdentity? ImportedVehicle { get { lock (gate) return importedVehicle; } }\n'),
    ('                Require(vehicleNative.Vehicle(session, generation, ref identity), "imported vehicle identity");\n',
     '                Require(vehicleNative.Vehicle(session, generation, ref identity), "imported vehicle identity");\n'
     '                lock (gate) importedVehicle = identity;\n'),
    ('                    throw new InvalidOperationException("Canonical vehicle selection changed imported identity or failed to resolve a class.");\n',
     '                    throw new InvalidOperationException("Canonical vehicle selection changed imported identity or failed to resolve a class.");\n'
     '                lock (gate) importedVehicle = resolvedIdentity;\n'),
])
# fast-forward (run.py --speed K, REFLEX_TEST_SPEED=K): K whole 1/60 s game frames back to back per job instead of one paced by
# the real clock -- the same frames as a perfectly paced 60 FPS session, as fast as the machine runs them (test player only)
patch(S / "OpenReflex/OpenReflexSimulation.cs", [
    ('            if (RealClock)\n            {\n                frames = calls = 1;',
     '            if (RealClock && TestSpeed > 0) { frames = calls = TestSpeed; dt = (float)FrameCap; lock (gate) { lastStart = Clock; requested += FrameCap * calls; stamp = requested; } }\n'
     '            else if (RealClock)\n            {\n                frames = calls = 1;'),
    ('        public const double FrameCap = 1.0 / 60.0;\n',
     '        public const double FrameCap = 1.0 / 60.0;\n'
     '        static readonly int TestSpeed = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_TEST_SPEED"), out var testSpeed) && testSpeed > 1 ? Math.Min(testSpeed, 8) : 0;\n'),
])
pp = S / "OriginalWorld/OriginalPacingProbe.cs"
s = pp.read_text()
assert s.startswith("#if UNITY_EDITOR\n") and s.rstrip().endswith("#endif")
pp.write_text(s[len("#if UNITY_EDITOR\n"):s.rstrip().rfind("#endif")])

# 5. build
app = out / "MotoFlow.app"
env = dict(os.environ, REFLEX_RXCORE_PATH=str(core), REFLEX_OPEN_PLAYER=str(app))
r = subprocess.run(["perl", "-e", "alarm shift; exec @ARGV", "5400", UNITY, "-batchmode", "-nographics", "-quit",
                    "-projectPath", str(stage), "-executeMethod", "Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch",
                    "-logFile", str(out / "build.log")], env=env)
log = (out / "build.log").read_text(errors="replace") if (out / "build.log").exists() else ""
errors = sorted(set(re.findall(r".*error CS\d+.*", log)))
for e in errors[:40]: print(e)
ok = r.returncode == 0 and "[open-build] Succeeded" in log and app.is_dir()
print(("BUILT " if ok else "BUILD FAILED ") + str(app))
sys.exit(0 if ok else 1)
