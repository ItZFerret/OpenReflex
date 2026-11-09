// TEST ONLY (tests/alive_bikes): drives Content/AliveImport/AliveBikes.cs outside Unity. Scans the sources, lists the Alive motorcycles,
// converts the chosen ones (--bikes <Alive VehicleModelIds>|all) into --out DIR/<slug> (a .rxmod folder each), and checks every written
// map is a PNG and the mesh reads back (RxMesh layout).
//   AliveBikes.dll [--out DIR] [--bikes 41,36|all] [--dump] [--install CONTENT] SOURCE...
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using Reflex.Port.Content.AliveImport;

static class Program
{
    static int Main(string[] args)
    {
        var sources = new List<string>(); string outDir = null, bikes = null, install = null; bool dump = false;
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--out") outDir = args[++i];
            else if (args[i] == "--bikes") bikes = args[++i];
            else if (args[i] == "--dump") dump = true;
            else if (args[i] == "--install") install = args[++i];   // a content folder: AliveBikes.Import into <content>/AliveBikes, as the game's IMPORT does
            else sources.Add(args[i]);
        }
        var clock = System.Diagnostics.Stopwatch.StartNew();
        using var found = AliveImporter.Scan(sources, s => { }, CancellationToken.None);
        Console.WriteLine($"scan {clock.Elapsed.TotalSeconds:0.0} s: {found.levels.Count} levels, {found.bikes.Count} bikes, {found.volumes.Count} sources");
        foreach (var w in found.warnings) Console.WriteLine("warning: " + w);
        foreach (var b in found.bikes)
        {
            Console.WriteLine($"  #{b.aliveId,-3} {b.Label,-24} {b.vehicleType,-6} {b.physics,-16} {b.pack,-30} " + (b.Importable ? $"rides as #{b.ridesAs} {b.ridesAsName}" : "NOT IMPORTED: " + b.problem));
            if (dump) foreach (var p in b.parts) Console.WriteLine("        " + p);
        }
        if (install != null)
        {
            var chosen = bikes == null || bikes == "all" ? found.bikes.Where(b => b.Importable).ToList() : bikes.Split(',').Select(int.Parse).Select(id => found.bikes.First(b => b.aliveId == id)).ToList();
            var rs = AliveBikes.Import(chosen, found, install, s => { }, CancellationToken.None);
            foreach (var r in rs) Console.WriteLine($"{r.bike.Label}: " + (r.ok ? $"installed {r.folder}" : "FAILED " + r.error));
            return rs.All(r => r.ok) ? 0 : 1;
        }
        if (outDir == null || bikes == null) return 0;
        var want = bikes == "all" ? found.bikes.Where(b => b.Importable).ToList() : bikes.Split(',').Select(int.Parse).Select(id => found.bikes.First(b => b.aliveId == id)).ToList();
        Directory.CreateDirectory(outDir);
        int bad = 0;
        foreach (var b in want)
        {
            string dest = Path.Combine(outDir, b.Slug);
            if (Directory.Exists(dest)) Directory.Delete(dest, true);
            var r = AliveBikes.Convert(b, found, dest, CancellationToken.None);
            Console.WriteLine($"{b.Label}: " + (r.ok ? $"ok {r.surfaces} surfaces, {r.vertices} vertices, {r.triangles} triangles, {r.maps} maps, {r.bytes / 1e6:0.0} MB, {r.seconds:0.0} s" : "FAILED " + r.error));
            foreach (var n in r.notes) Console.WriteLine("    note: " + n);
            if (!r.ok) { bad++; continue; }
            foreach (var f in Directory.GetFiles(Path.Combine(dest, "maps")))
            {
                var h = File.ReadAllBytes(f);
                if (h.Length < 8 || h[1] != 'P' || h[2] != 'N' || h[3] != 'G') { Console.WriteLine("    BAD PNG " + f); bad++; }
            }
        }
        return bad == 0 ? 0 : 1;
    }
}
