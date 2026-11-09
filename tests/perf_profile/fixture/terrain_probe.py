"""terrain_probe.py <staged Reflex project> -- TEST ONLY (bench.py build --probe-terrain): times the main-thread consumers of the core's
terrain and tyre-track exports in a staged copy (OriginalTerrain.ApplyPatch, OriginalTracks.Apply: the original methods are renamed and
wrapped) and writes their means every 240 Unity frames to <REFLEX_PREVIEW_SHOTS>/terrain-probe.txt as
"[terr-probe] frames N: patches/frame, patch ms/frame, texels/frame, tracks applies/frame, ms per apply, ms/frame". The product source is not
changed; works on any tree whose methods have the same two signatures (before / after w-terrtiles)."""
import sys
from pathlib import Path
stage = Path(sys.argv[1])
def patch(rel, edits):
    p = stage / rel; s = p.read_text()
    for old, new in edits:
        assert s.count(old) == 1, (rel, old, s.count(old))
        s = s.replace(old, new)
    p.write_text(s)
probe = stage / "Assets/Scripts/Platform/PerfBench/TerrProbe.cs"
probe.write_text('''using System;
using System.Diagnostics;
using UnityEngine;
namespace Reflex.Port.OriginalWorld
{
    public static class TerrProbe
    {
        static double patchMs, tracksMs; static long patches, texels, applies; static int start = -1;
        public static long Begin() => Stopwatch.GetTimestamp();
        public static void Patch(long t0, int w, int h) { patchMs += (Stopwatch.GetTimestamp() - t0) * 1000.0 / Stopwatch.Frequency; patches++; texels += (long)w * h; Tick(); }
        public static void Tracks(long t0) { tracksMs += (Stopwatch.GetTimestamp() - t0) * 1000.0 / Stopwatch.Frequency; applies++; Tick(); }
        static void Tick()
        {
            int f = Time.frameCount; if (start < 0) start = f;
            if (f - start < 240) return;
            double n = f - start;
            string line = "[terr-probe] frames " + (f - start) + ": patches/frame " + (patches / n).ToString("0.00") + ", patch ms/frame " + (patchMs / n).ToString("0.000") + ", texels/frame " + (texels / n).ToString("0") +
                ", tracks applies/frame " + (applies / n).ToString("0.000") + ", ms per apply " + (applies > 0 ? tracksMs / applies : 0).ToString("0.000") + ", tracks ms/frame " + (tracksMs / n).ToString("0.000") + ", total ms/frame " + ((patchMs + tracksMs) / n).ToString("0.000");
            UnityEngine.Debug.Log(line);
            System.IO.File.AppendAllText(System.IO.Path.Combine(Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS") ?? "/tmp", "terrain-probe.txt"), line + "\\n");
            patchMs = tracksMs = 0; patches = texels = applies = 0; start = f;
        }
    }
}
''')
patch("Assets/Scripts/OriginalWorld/OriginalTerrain.cs", [
    ("        public void ApplyPatch(OriginalSimulation.TerrainPatch p)\n        {",
     "        public void ApplyPatch(OriginalSimulation.TerrainPatch p) { long t0 = TerrProbe.Begin(); int pw = p.w, ph = p.h; ApplyPatchTimed(p); TerrProbe.Patch(t0, pw, ph); }\n        void ApplyPatchTimed(OriginalSimulation.TerrainPatch p)\n        {")])
patch("Assets/Scripts/OriginalWorld/OriginalTracks.cs", [
    ("        public void Apply(float[] data, int triangles)\n        {",
     "        public void Apply(float[] data, int triangles) { long t0 = TerrProbe.Begin(); ApplyTimed(data, triangles); TerrProbe.Tracks(t0); }\n        void ApplyTimed(float[] data, int triangles)\n        {")])
print("terrain probe patched", stage)
