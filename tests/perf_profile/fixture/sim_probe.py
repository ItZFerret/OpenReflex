"""sim_probe.py <staged Reflex project> -- TEST ONLY (bench.py build --probe-sim): times the parts of the simulation thread's job in a
staged copy of OpenReflexSimulation.cs (the frame, every export group of Collect, the rest of the job) and logs their means every 240
jobs as "[sim-probe] ..." lines. The product source is not changed."""
import sys
from pathlib import Path
p = Path(sys.argv[1]) / "Assets/Scripts/OpenReflex/OpenReflexSimulation.cs"
s = p.read_text()
def rep(old, new, count=1):
    global s
    assert s.count(old) == count, (old, s.count(old))
    s = s.replace(old, new)
# the probe's state and helpers (class members)
rep("        void Collect(Results r)\n        {\n",
    "        static readonly string[] __names = { \"frame\", \"pose/race/hud\", \"mode states\", \"audio\", \"line/vehicle/standings\", \"camera/particles\", \"ambient\", \"node flags\", \"objects\", \"terrain+tracks\", \"slots\", \"mud\", \"podium\", \"scenario models\", \"job other\" };\n"
    "        static readonly double[] __t = new double[16]; static long __last; static int __jobs; static double __job;\n"
    "        static void __L(int k) { long now = Stopwatch.GetTimestamp(); __t[k] += (now - __last) * 1000.0 / Stopwatch.Frequency; __last = now; }\n"
    "        static void __Report(double jobMs)\n"
    "        {\n"
    "            __job += jobMs; if (++__jobs < 240) return;\n"
    "            { double sum = 0; for (int i = 0; i < 14; i++) sum += __t[i]; __t[14] = __job - sum; }\n"
    "            var sb = new System.Text.StringBuilder(\"[sim-probe] \" + __jobs + \" jobs, job \" + (__job / __jobs).ToString(\"0.000\") + \" ms:\");\n"
    "            for (int i = 0; i < __names.Length; i++) sb.Append(\" \" + __names[i] + \" \" + (__t[i] / __jobs).ToString(\"0.000\"));\n"
    "            System.IO.File.AppendAllText(System.IO.Path.Combine(Environment.GetEnvironmentVariable(\"REFLEX_PREVIEW_SHOTS\") ?? \"/tmp\", \"sim-probe.txt\"), sb.ToString() + \"\\n\"); Array.Clear(__t, 0, __t.Length); __jobs = 0; __job = 0;\n"
    "        }\n"
    "        void Collect(Results r)\n        {\n            __last = Stopwatch.GetTimestamp();\n")
rep("            if (!hasHudExport) r.hasHud = false;\n", "            if (!hasHudExport) r.hasHud = false;\n            __L(1);\n")
rep("            if (!r.hasReflexHud) r.reflexHud = default;\n", "            if (!r.hasReflexHud) r.reflexHud = default;\n            __L(2);\n")
rep("                catch (EntryPointNotFoundException) { hasSoundExport = false; r.sound.Clear(); }\n", "                catch (EntryPointNotFoundException) { hasSoundExport = false; r.sound.Clear(); }\n            __L(3);\n")
rep("            r.hasCamera = RxApiNative.rx_session_camera(session, ref r.camera) == 0;\n", "            __L(4);\n            r.hasCamera = RxApiNative.rx_session_camera(session, ref r.camera) == 0;\n")
rep("            r.hasAmbient = RxApiNative.rx_session_ambient(session, ref r.ambient) == 0;\n", "            __L(5);\n            r.hasAmbient = RxApiNative.rx_session_ambient(session, ref r.ambient) == 0;\n")
rep("            if (hasNodeFlags && r.hasVehicle)\n", "            __L(6);\n            if (hasNodeFlags && r.hasVehicle)\n")
rep("            CollectObjects(r);\n            if (trackTerrain) { CollectTerrain(r); CollectTracks(r); }\n            CollectSlots(r);\n            CollectMud(r);\n            CollectPodium(r);\n            CollectScenarioModels(r);\n",
    "            __L(7);\n            CollectObjects(r); __L(8);\n            if (trackTerrain) { CollectTerrain(r); CollectTracks(r); } __L(9);\n            CollectSlots(r); __L(10);\n            CollectMud(r); __L(11);\n            CollectPodium(r); __L(12);\n            CollectScenarioModels(r); __L(13);\n")
rep("            r.frameMilliseconds = frameWatch.Elapsed.TotalMilliseconds;\n", "            r.frameMilliseconds = frameWatch.Elapsed.TotalMilliseconds; __t[0] += r.frameMilliseconds;\n")
# the job line (before / after w-perf: the job's time without its frame-cap wait)
for old_job in ["                        { r = back; ResetResults(r); RunJob(r); r.milliseconds = watch.Elapsed.TotalMilliseconds; }\n",
                "                        { r = back; ResetResults(r); RunJob(r); r.milliseconds = watch.Elapsed.TotalMilliseconds - r.capWaitMilliseconds; }\n"]:
    if s.count(old_job) == 1: rep(old_job, old_job.replace("; }\n", "; __Report(r.milliseconds); }\n")); break
else: raise SystemExit("sim_probe: the job line moved")
p.write_text(s)
print("sim probe patched", p)
