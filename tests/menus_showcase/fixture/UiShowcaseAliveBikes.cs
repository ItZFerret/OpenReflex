// TEST ONLY: the MX vs ATV Alive bike import (Content/AliveImport/AliveBikes.cs, core/plan/ALIVE_BIKES.md) through the real screens of
// GAME LIBRARY > MX vs ATV ALIVE:
//   alive-bikes   the sources REFLEX_ALIVE_SOURCES (';'-separated: the disc image, the 5451087B folder) scanned as FIND LEVELS does, the list
//                 shot (alive-bikes-list: the levels, then ALIVE BIKES), every level unticked and the bikes left ticked, IMPORT, the results shot
//                 (alive-bikes-results), then BikeMods rescanned: each imported bike must be a .rxmod mod "AliveBikes/<slug>" riding as its
//                 Reflex bike. The content folder (REFLEX_GAME_DIR) must be a scratch folder: the bikes are written into its AliveBikes/.
// Result lines "[ui-showcase] alivebikes ..."; "FAIL alivebikes: ..." on a failure.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseAliveBikes
    {
        const string Tag = "[ui-showcase] ";
        const BindingFlags All = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("alive-bikes", Page);

        static void Log(string s) => Debug.Log(Tag + "alivebikes " + s);
        static void Fail(string s) => Debug.LogError(Tag + "FAIL alivebikes: " + s);
        static OpenShell Shell => OpenShell.Instance;
        static T Get<T>(string f) => (T)typeof(OpenShell).GetField(f, All).GetValue(Shell);
        static void Call(string m) => typeof(OpenShell).GetMethod(m, All).Invoke(Shell, null);

        static IEnumerator Page()
        {
            string src = Environment.GetEnvironmentVariable("REFLEX_ALIVE_SOURCES");
            if (string.IsNullOrEmpty(src)) { Fail("REFLEX_ALIVE_SOURCES is not set"); yield break; }
            Log("content folder " + ReflexInstall.Dir);
            var paths = Get<List<string>>("_aliveSourcePaths"); paths.Clear(); paths.AddRange(src.Split(';').Where(p => p.Length > 0));
            Shell.ShowAliveSources(); yield return UiShowcase.Frames(30);
            Call("BeginAliveScan");
            // (until the shell took the worker's result: UpdateAliveImport clears _aliveTask and shows the list)
            for (float t0 = Time.realtimeSinceStartup; Time.realtimeSinceStartup - t0 < 300f && Get<System.Threading.Tasks.Task>("_aliveTask") != null;) yield return null;
            yield return UiShowcase.Frames(30);
            var found = Get<AliveSources>("_aliveFound");
            if (found == null) { Fail("the scan found nothing (" + Get<Exception>("_aliveError")?.Message + ")"); yield break; }
            Log($"found {found.levels.Count} levels, {found.bikes.Count} bikes ({found.bikes.Count(b => b.Importable)} importable): " +
                string.Join(", ", found.bikes.Select(b => b.aliveId + "=" + b.Label + (b.Importable ? "->" + b.ridesAs : " (" + b.problem + ")"))));
            if (found.bikes.Count(b => b.Importable) == 0) { Fail("no importable bike"); yield break; }
            // the list: every level unticked (the bikes stay ticked), shown again
            var chosen = Get<HashSet<string>>("_aliveChosen");
            chosen.RemoveWhere(k => !k.StartsWith("bike:", StringComparison.Ordinal));
            Call("ShowAliveLevels"); yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("alive-bikes-list");
            var sv = Shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith("ou-alive-bike-"));
            if (sv == null) Fail("the list has no bike rows");
            else { Shell.Nav.Focus(sv); yield return UiShowcase.Frames(20); yield return UiShowcase.Shot("alive-bikes-list-bikes"); }
            Call("BeginAliveImport");
            yield return UiShowcase.Frames(5);   // (BeginAliveImport's task)
            for (float t0 = Time.realtimeSinceStartup; Time.realtimeSinceStartup - t0 < 900f && Get<System.Threading.Tasks.Task>("_aliveTask") != null;) yield return null;
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("alive-bikes-results");
            var results = Get<List<AliveBikeResult>>("_aliveBikeResults") ?? new List<AliveBikeResult>();
            foreach (var r in results) Log((r.ok ? "OK " : "FAILED ") + r.bike.Label + (r.ok ? $" {r.surfaces} surfaces {r.maps} maps {r.bytes >> 10} KB {r.seconds:0.0} s -> {r.folder}" : ": " + r.error));
            if (results.Count == 0 || results.Any(r => !r.ok)) Fail($"{results.Count(r => r.ok)} of {results.Count} bikes imported");
            // the garage's view: each one a .rxmod mod riding as its Reflex bike
            var mods = BikeMods.All.Where(m => m.id.StartsWith(AliveBikes.Folder + "/", StringComparison.Ordinal)).ToList();
            Log("BikeMods: " + mods.Count + " Alive bikes: " + string.Join(", ", mods.Select(m => m.id + " #" + string.Join("+", m.rx.bikes.Select(b => b.vehicleModelId)) + " " + m.rx.hash)));
            foreach (var r in results.Where(r => r.ok))
            {
                var m = mods.FirstOrDefault(x => x.id == AliveBikes.Folder + "/" + r.bike.Slug);
                if (m == null) Fail(r.bike.Label + " is not in BikeMods.All");
                else if (m.rx.bikes.Count != 1 || m.rx.bikes[0].vehicleModelId != r.bike.ridesAs) Fail(r.bike.Label + " rides as " + string.Join(",", m.rx.bikes.Select(b => b.vehicleModelId)) + ", not " + r.bike.ridesAs);
            }
            Log("done");
            Shell.ShowMain(); yield return UiShowcase.Frames(5);
        }
    }
}
