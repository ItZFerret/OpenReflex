// TEST ONLY Editor batch tool (tests/gc_spikes): GcAllocReport.Run reads REFLEX_PROFILE_FILE (a player's binary profiler log with GC.Alloc
// call stacks) and writes REFLEX_PROFILE_REPORT: the main thread's managed allocations per frame grouped by the script marker above them
// (the MonoBehaviour / player loop sample) and by allocating call site (the first frames of the managed call stack), and the markers of
// the slowest frames.  Unity -batchmode -projectPath <project> -executeMethod GcAllocReport.Run
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.Profiling;
using UnityEditorInternal;
using Unity.Profiling.LowLevel.Unsafe;

public static class GcAllocReport
{
    sealed class Agg { public long bytes; public int count; }

    public static void Run()
    {
        string file = Environment.GetEnvironmentVariable("REFLEX_PROFILE_FILE"), outPath = Environment.GetEnvironmentVariable("REFLEX_PROFILE_REPORT");
        int depth = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_PROFILE_STACK"), out var d) ? d : 4;
        int markerDepth = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_PROFILE_DEPTH"), out var md) ? md : 4;
        var sb = new StringBuilder();
        try
        {
            if (!ProfilerDriver.LoadProfile(file, false)) throw new Exception("LoadProfile failed: " + file);
            int first = ProfilerDriver.firstFrameIndex, last = ProfilerDriver.lastFrameIndex;
            var byMarker = new Dictionary<string, Agg>(); var bySite = new Dictionary<string, Agg>(); var byPath = new Dictionary<string, Agg>();
            var stack = new List<ulong>(); var frameMs = new List<(int f, float ms, long alloc)>();
            var resolved = new Dictionary<ulong, string>();
            long total = 0; int frames = 0, gcAllocs = 0;
            var slowMarkers = new Dictionary<int, List<(string, float)>>();
            for (int f = first; f <= last; f++)
            {
                using (var v = ProfilerDriver.GetRawFrameDataView(f, 0))
                {
                    if (v == null || !v.valid) continue;
                    frames++;
                    long frameAlloc = 0;
                    int gcId = v.GetMarkerId("GC.Alloc");
                    // the parent chain: samples are in depth-first order with their children counts
                    var parents = new List<(int idx, int left)>();
                    var names = new List<string>();
                    var top = new List<(string, float)>();
                    for (int i = 0; i < v.sampleCount; i++)
                    {
                        while (parents.Count > 0 && parents[parents.Count - 1].left == 0) { parents.RemoveAt(parents.Count - 1); names.RemoveAt(names.Count - 1); }
                        if (parents.Count > 0) { var p = parents[parents.Count - 1]; parents[parents.Count - 1] = (p.idx, p.left - 1); }
                        string name = v.GetSampleName(i);
                        if (parents.Count <= markerDepth && v.GetSampleTimeMs(i) > 1) top.Add((new string(' ', parents.Count * 2) + name, v.GetSampleTimeMs(i)));
                        if (v.GetSampleMarkerId(i) == gcId)
                        {
                            long size = v.GetSampleMetadataCount(i) > 0 ? v.GetSampleMetadataAsLong(i, 0) : 0;
                            frameAlloc += size; gcAllocs++;
                            // the nearest script / named marker above: skip generic player-loop wrappers
                            string owner = "?"; string path = "";
                            for (int k = names.Count - 1; k >= 0; k--)
                            {
                                string n = names[k];
                                if (owner == "?" && n != "GC.Alloc" && !n.StartsWith("GC.")) owner = n;
                            }
                            path = string.Join(" > ", names.Skip(Math.Max(0, names.Count - 3)));
                            Add(byMarker, owner, size); Add(byPath, path, size);
                            stack.Clear();
                            v.GetSampleCallstack(i, stack);
                            var site = new StringBuilder();
                            int shown = 0;
                            foreach (var addr in stack)
                            {
                                if (!resolved.TryGetValue(addr, out var m))
                                {
                                    var mi = v.ResolveMethodInfo(addr);
                                    m = string.IsNullOrEmpty(mi.methodName) ? "" : mi.methodName + (mi.sourceFileLine > 0 ? ":" + mi.sourceFileLine : "");
                                    resolved[addr] = m;
                                }
                                if (m.Length == 0) continue;
                                if (shown > 0) site.Append(" <- ");
                                site.Append(m);
                                if (++shown >= depth) break;
                            }
                            Add(bySite, site.Length > 0 ? site.ToString() : "(no stack) " + owner, size);
                        }
                        int ch = v.GetSampleChildrenCount(i);
                        parents.Add((i, ch)); names.Add(name);
                    }
                    total += frameAlloc;
                    float ms = v.frameTimeMs;
                    frameMs.Add((f, ms, frameAlloc));
                    slowMarkers[f] = top;
                }
            }
            if (frames == 0) throw new Exception("no frames");
            var times = frameMs.Select(x => x.ms).OrderBy(x => x).ToArray();
            sb.AppendLine($"file {file}");
            sb.AppendLine($"frames {frames}  mean {times.Average():0.00} ms  median {times[times.Length / 2]:0.00}  p99 {times[(int)(times.Length * .99)]:0.00}  max {times.Last():0.00}");
            sb.AppendLine($"main thread GC.Alloc: {total / 1024.0 / frames:0.00} KB/frame, {gcAllocs / (double)frames:0.0} allocations/frame, {total / 1048576.0:0.00} MB in all");
            sb.AppendLine();
            sb.AppendLine("== by the marker above the allocation (KB/frame, allocations/frame)");
            foreach (var kv in byMarker.OrderByDescending(x => x.Value.bytes).Take(60))
                sb.AppendLine($"{kv.Value.bytes / 1024.0 / frames,9:0.000} KB {kv.Value.count / (double)frames,7:0.00}  {kv.Key}");
            sb.AppendLine();
            sb.AppendLine("== by marker path (last three)");
            foreach (var kv in byPath.OrderByDescending(x => x.Value.bytes).Take(60))
                sb.AppendLine($"{kv.Value.bytes / 1024.0 / frames,9:0.000} KB {kv.Value.count / (double)frames,7:0.00}  {kv.Key}");
            sb.AppendLine();
            sb.AppendLine("== by call site (" + depth + " frames)");
            foreach (var kv in bySite.OrderByDescending(x => x.Value.bytes).Take(120))
                sb.AppendLine($"{kv.Value.bytes / 1024.0 / frames,9:0.000} KB {kv.Value.count / (double)frames,7:0.00}  {kv.Key}");
            sb.AppendLine();
            sb.AppendLine("== the slowest frames (markers over 1 ms, depth <= " + markerDepth + ")");
            foreach (var x in frameMs.OrderByDescending(x => x.ms).Take(8))
            {
                sb.AppendLine($"-- frame {x.f}: {x.ms:0.00} ms, alloc {x.alloc / 1024.0:0.0} KB");
                foreach (var (n, t) in slowMarkers[x.f].Take(120)) sb.AppendLine($"   {t,8:0.00} ms {n}");
            }
        }
        catch (Exception e) { sb.AppendLine("ERROR " + e); }
        System.IO.File.WriteAllText(outPath, sb.ToString());
        EditorApplication.Exit(0);
    }

    static void Add(Dictionary<string, Agg> d, string k, long size)
    {
        if (!d.TryGetValue(k, out var a)) d[k] = a = new Agg();
        a.bytes += size; a.count++;
    }
}
