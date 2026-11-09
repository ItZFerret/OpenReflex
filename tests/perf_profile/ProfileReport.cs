// TEST ONLY Editor batch tool: ProfileReport.Run reads REFLEX_PROFILE_FILE (a player's binary profiler log) and writes
// REFLEX_PROFILE_REPORT: the main thread's frame times, the mean cost of the markers (depth <= 6), and the markers of the
// slowest frames.  Unity -batchmode -projectPath <project> -executeMethod ProfileReport.Run
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditorInternal;
using UnityEditor.Profiling;

public static class ProfileReport
{
    public static void Run()
    {
        string file = Environment.GetEnvironmentVariable("REFLEX_PROFILE_FILE"), outPath = Environment.GetEnvironmentVariable("REFLEX_PROFILE_REPORT");
        var sb = new StringBuilder();
        try
        {
            if (!ProfilerDriver.LoadProfile(file, false)) throw new Exception("LoadProfile failed: " + file);
            int first = ProfilerDriver.firstFrameIndex, last = ProfilerDriver.lastFrameIndex;
            var frames = new List<(int i, float ms)>();
            var total = new Dictionary<string, double>(); var calls = new Dictionary<string, double>();
            var perFrame = new Dictionary<int, List<(string, float)>>();
            for (int f = first; f <= last; f++)
            {
                using (var v = ProfilerDriver.GetHierarchyFrameDataView(f, 0, HierarchyFrameDataView.ViewModes.MergeSamplesWithTheSameName, HierarchyFrameDataView.columnTotalTime, false))
                {
                    if (v == null || !v.valid) continue;
                    float frameMs = v.frameTimeMs; frames.Add((f, frameMs));
                    var list = new List<(string, float)>();
                    Walk(v, v.GetRootItemID(), 0, "", total, calls, list);
                    perFrame[f] = list;
                }
            }
            if (frames.Count == 0) throw new Exception("no frames");
            var ms = frames.Select(x => x.ms).OrderBy(x => x).ToArray();
            sb.AppendLine($"frames {frames.Count} ({first}..{last})  mean {ms.Average():0.00} ms  median {ms[ms.Length / 2]:0.00}  p95 {ms[(int)(ms.Length * .95)]:0.00}  p99 {ms[(int)(ms.Length * .99)]:0.00}  max {ms.Last():0.00}");
            sb.AppendLine($"frames over 25 ms: {ms.Count(x => x > 25)}, over 33 ms: {ms.Count(x => x > 33.4)}, over 50 ms: {ms.Count(x => x > 50)}");
            sb.AppendLine("\n== mean ms per frame by marker path (depth <= 6, >= 0.15 ms)");
            foreach (var kv in total.OrderByDescending(k => k.Value).Where(k => k.Value / frames.Count >= 0.15).Take(120))
                sb.AppendLine($"{kv.Value / frames.Count,8:0.00} ms  {kv.Key}");
            sb.AppendLine("\n== the 12 slowest frames: their markers >= 2 ms");
            foreach (var fr in frames.OrderByDescending(x => x.ms).Take(12))
            {
                sb.AppendLine($"-- frame {fr.i}: {fr.ms:0.00} ms");
                foreach (var (name, t) in perFrame[fr.i].Where(x => x.Item2 >= 2).OrderByDescending(x => x.Item2).Take(25)) sb.AppendLine($"   {t,8:0.00} ms  {name}");
            }
        }
        catch (Exception e) { sb.AppendLine("ERROR " + e); }
        File.WriteAllText(outPath, sb.ToString());
        EditorApplication.Exit(0);
    }
    static void Walk(HierarchyFrameDataView v, int id, int depth, string path, Dictionary<string, double> total, Dictionary<string, double> calls, List<(string, float)> list)
    {
        var kids = new List<int>(); v.GetItemChildren(id, kids);
        foreach (int k in kids)
        {
            string name = v.GetItemName(k); float t = v.GetItemColumnDataAsFloat(k, HierarchyFrameDataView.columnTotalTime);
            string p = path.Length == 0 ? name : path + " > " + name;
            total[p] = (total.TryGetValue(p, out var a) ? a : 0) + t;
            list.Add((p, t));
            if (depth < 6 && t >= 0.05f) Walk(v, k, depth + 1, p, total, calls, list);
        }
    }
}
