// TEST ONLY (tests/untamed_import): drives Content/UntamedImport/UntamedImporter.cs outside Unity. Scans the sources (MX vs ATV Untamed
// disc images or extracted disc folders), prints every source, warning and stadium found (ready, or why not; the custom track converted
// before when --tracks gives a Mods/Tracks folder), and checks the sources and the tracks folder were only read.
//   UntamedImport.dll [--tracks DIR] [--expect N] SOURCE...
// Exit 0: the scan finished, nothing it read changed, and (with --expect) exactly N stadiums are ready.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using Reflex.Port.Content.UntamedImport;

static class Program
{
    static int Main(string[] args)
    {
        var sources = new List<string>(); string tracks = null; int expect = -1;
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--tracks") tracks = args[++i];
            else if (args[i] == "--expect") expect = int.Parse(args[++i]);
            else sources.Add(args[i]);
        }
        var before = Snapshot(sources.Concat(tracks != null ? new[] { tracks } : new string[0]));
        var clock = System.Diagnostics.Stopwatch.StartNew();
        int ready;
        using (var found = UntamedImporter.Scan(sources, tracks, (s, f) => { }, CancellationToken.None))
        {
            Console.WriteLine($"scan {clock.Elapsed.TotalSeconds:0.0} s: {found.Ready.Count()} of {found.tracks.Count} stadiums ready, {found.volumes.Count} sources");
            foreach (var s in found.sources) Console.WriteLine("source: " + s);
            foreach (var w in found.warnings) Console.WriteLine("warning: " + w);
            foreach (var t in found.tracks)
                Console.WriteLine($"  {t.known.level,-22} {t.Label,-28} {t.known.Kind,-11} {t.bytes,10} " + (t.Ok ? "READY" : "NOT READY: " + t.problem)
                                  + (t.installed != null ? "  installed: " + t.installed : ""));
            if (found.unsupported.Count > 0) Console.WriteLine("not supported: " + string.Join(", ", found.unsupported));
            ready = found.Ready.Count();
        }
        var after = Snapshot(sources.Concat(tracks != null ? new[] { tracks } : new string[0]));
        bool unchanged = before.Count == after.Count && before.All(kv => after.TryGetValue(kv.Key, out var v) && v == kv.Value);
        if (!unchanged) Console.WriteLine("FAIL: a source or the tracks folder changed during the scan");
        if (expect >= 0 && ready != expect) Console.WriteLine($"FAIL: {ready} stadiums ready, expected {expect}");
        bool ok = unchanged && (expect < 0 || ready == expect);
        Console.WriteLine(ok ? "UNTAMED IMPORT PASS" : "UNTAMED IMPORT FAIL");
        return ok ? 0 : 1;
    }

    /// Size and last write time of every file under the paths (a disc image: itself).
    static Dictionary<string, (long, DateTime)> Snapshot(IEnumerable<string> paths)
    {
        var r = new Dictionary<string, (long, DateTime)>(StringComparer.Ordinal);
        foreach (var p in paths)
        {
            if (File.Exists(p)) { var fi = new FileInfo(p); r[fi.FullName] = (fi.Length, fi.LastWriteTimeUtc); }
            else if (Directory.Exists(p))   // (folders the importer can't read either are skipped; hidden and system files kept)
                foreach (var f in Directory.EnumerateFiles(p, "*", new EnumerationOptions { RecurseSubdirectories = true, IgnoreInaccessible = true, AttributesToSkip = 0 }))
                { var fi = new FileInfo(f); r[fi.FullName] = (fi.Length, fi.LastWriteTimeUtc); }
        }
        return r;
    }
}
