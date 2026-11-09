using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Tests
{
    // The forest (Content/ReflexForest.cs, OriginalWorld/OriginalForest.cs, _re/notes/forest.md):
    //   Verify  every level with a ForestSystemRSData: forest + tree assets parse, every type has its LOD records and geometry, the
    //           materials and atlas load; REFLEX_FOREST_OUT=<dir> also writes <level>_forest.txt (per type constants, LOD records,
    //           every tree record, round-trip floats) for the comparison with the running game's memory (scratch fcompare.py).
    //   Render  RenderCompareTests.Render with the forest (REFLEX_FOREST_OFF=1: without it, the "before" image).
    //   Perf    OriginalForest's main-thread time for a level over a sweep of camera positions (REFLEX_RC_LEVEL).
    public static class ForestTests
    {
        public static readonly string[] Levels =
        {
            "FR_Alpine", "FR_Cheyenne", "FR_Gibson", "NAT_Bedford", "NAT_BedfordB", "NAT_Concord", "NAT_ConcordB", "NAT_DLC05_Dalton", "NAT_DLC06_DaltonB",
            "NAT_Renne", "NAT_RenneB", "NAT_Stillwater", "NAT_StillwaterB", "PX_Laguna", "PX_LagunaB", "PX_LagunaB_BW", "PX_Laguna_BW",
            "TRX_Halifax", "TRX_HalifaxBW", "TRX_Lynchburg", "TRX_LynchburgBW", "TRX_Verde", "TRX_VerdeBW",
        };

        static string R(float f) => f.ToString("R", CultureInfo.InvariantCulture);

        public static void Verify()
        {
            string outDir = Environment.GetEnvironmentVariable("REFLEX_FOREST_OUT");
            string only = Environment.GetEnvironmentVariable("REFLEX_FOREST_LEVELS");
            var levels = string.IsNullOrEmpty(only) ? Levels : only.Split(',');
            var log = new StringBuilder(); bool ok = true; int totalTrees = 0, totalTypes = 0;
            foreach (var level in levels)
            {
                try
                {
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    var c = ReflexLevelContent.Load(level, new ReflexLevelContent.Options { terrain = false, tracks = false });
                    var f = c.forest;
                    if (f == null) { log.AppendLine($"{level}: FAIL no forest"); ok = false; continue; }
                    int geo = 0, recs = 0;
                    foreach (var t in f.types) { recs += t.records.Count; for (int k = 0; k < 3; k++) if (t.geometry[k] != null) geo++; }
                    var problems = c.ForestProblems();
                    totalTrees += f.treeCount; totalTypes += f.types.Count;
                    log.AppendLine($"{level}: {(problems.Count == 0 ? "ok" : "FAIL")} {f.name}: {f.treeCount} trees in {f.clusterStart.Count(n => n >= 0)} clusters, {f.types.Count} types, {geo} meshes, {recs} LOD records, " +
                                   $"3D to {f.max3dDistance:F1}, billboards from {f.minBillboardStart:F1}, atlas {f.atlasDiffuse}; {sw.ElapsedMilliseconds} ms" +
                                   (problems.Count > 0 ? "\n    " + string.Join("\n    ", problems) : ""));
                    if (problems.Count > 0) ok = false;
                    if (!string.IsNullOrEmpty(outDir)) Dump(f, Path.Combine(outDir, level + "_forest.txt"));
                    c.Dispose();
                }
                catch (Exception e) { log.AppendLine($"{level}: FAIL {e}"); ok = false; }
            }
            // the per-tree record helpers against a hand-checked value (Concord cluster 5: rotation 1.5523164 -> pi / 4, sin / cos of the game)
            float q = ReflexLevelContent.QuantiseRotation(1.552316427230835f); ReflexLevelContent.TreeSinCos(q, out float sn, out float cs);
            bool recOk = q == 0.7853981852531433f && sn == 0.7071073651313782f && cs == 0.7071059942245483f;
            log.AppendLine($"tree record helpers: rotation {R(q)} sin {R(sn)} cos {R(cs)} {(recOk ? "ok" : "FAIL")}");
            ok &= recOk;
            log.AppendLine($"{levels.Length} levels, {totalTypes} tree types, {totalTrees} trees: {(ok ? "PASS" : "FAIL")}");
            Debug.Log("[ForestTests] " + log);
            if (!string.IsNullOrEmpty(outDir)) File.WriteAllText(Path.Combine(outDir, "verify.txt"), log.ToString());
        }

        static void Dump(ForestData f, string path)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            var sb = new StringBuilder();
            foreach (var t in f.types)
            {
                sb.Append("type ").Append(t.index).Append(' ').Append(t.treeAsset).Append(" bb ").Append(R(t.bbStart));
                sb.Append(" branch ").Append(string.Join(",", t.branchParams.Select(R))).Append(" frond ").Append(string.Join(",", t.frondParams.Select(R)));
                sb.Append(" billboard ").Append(string.Join(",", t.billboard.Select(R))).Append('\n');
                foreach (var r in t.records)
                    sb.Append("rec ").Append(R(r.dist)).Append(' ').Append(r.branchLod).Append(',').Append(r.frondLod).Append(',').Append(r.leafLod).Append(',').Append(r.leaf3dLod)
                      .Append(' ').Append(r.transition ? 1 : 0).Append(" leaf ").Append(string.Join(",", r.leaf.Select(R))).Append(" leaf3d ").Append(string.Join(",", r.leaf3d.Select(R))).Append('\n');
            }
            for (int i = 0; i < f.treeCount; i++) sb.Append("tree ").Append(string.Join(",", Enumerable.Range(0, 8).Select(k => R(f.trees[i * 8 + k])))).Append('\n');
            File.WriteAllText(path, sb.ToString());
        }

        public static void Render()
        {
            OriginalForest.disabledForTests = Environment.GetEnvironmentVariable("REFLEX_FOREST_OFF") == "1";
            // the recorded frame's wind (g_UpVectors c61..c64: 12 numbers)
            if (Environment.GetEnvironmentVariable("REFLEX_GRASS_UP") is string gu && gu.Length > 0)
            {
                var v = gu.Split(',').Select(x => float.Parse(x, CultureInfo.InvariantCulture)).ToArray();
                OriginalForest.grassUpOverride = Enumerable.Range(0, 4).Select(i => new Vector4(v[i * 3], v[i * 3 + 1], v[i * 3 + 2], 0)).ToArray();
            }
            try { RenderCompareTests.Render(); }
            finally { OriginalForest.disabledForTests = false; OriginalForest.grassUpOverride = null; }
            var f = UnityEngine.Object.FindAnyObjectByType<OriginalForest>();
            string outBase = Environment.GetEnvironmentVariable("REFLEX_RC_OUT");
            if (f && outBase != null) File.AppendAllText(outBase + "_unity.txt", $"forest: {f.report}\n  clusters {f.clustersVisible}, 3D trees {f.trees3d}, 3D draws {f.draws3d}, grass cells far {f.grassCellsFar} near {f.grassCellsNear}, {f.msLast:F3} ms\n");
        }

        /// Main-thread cost of OriginalForest over a sweep of camera positions around the level's trees (REFLEX_RC_LEVEL).
        public static void Perf()
        {
            string level = Environment.GetEnvironmentVariable("REFLEX_RC_LEVEL") ?? "FR_Alpine";
            string outBase = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? Path.Combine(Path.GetTempPath(), "forest_perf");
            var log = new StringBuilder();
            try
            {
                EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                var go = new GameObject("Original level");
                var ol = go.AddComponent<OriginalLevel>(); ol.level = level; ol.originalPipeline = false; ol.alignSun = false;
                ol.LoadNow();
                var forest = go.GetComponent<OriginalForest>();
                if (!forest) throw new Exception("no OriginalForest");
                log.AppendLine(forest.report);
                var camGo = new GameObject("Camera"); var cam = camGo.AddComponent<Camera>();
                var urp = camGo.AddComponent<UniversalAdditionalCameraData>(); urp.renderShadows = true;
                cam.fieldOfView = 40; cam.aspect = 16f / 9f; cam.nearClipPlane = 0.5f; cam.farClipPlane = 15000f;
                var rt = new RenderTexture(1280, 720, 24, RenderTextureFormat.ARGBHalf); cam.targetTexture = rt;
                var f = ol.Content.forest;
                foreach (bool locked in new[] { true, false })
                {
                    forest.lockedUpload = locked;
                    var rng = new System.Random(1);
                    var ms = new List<double>(); var sel = new List<double>(); var up = new List<double>(); var sub = new List<double>(); var counts = new List<int>(); var render = new List<double>();
                    for (int i = 0; i < 200; i++)
                    {
                        // a tree at random; the camera 2.5 m above its base, 30 m away, looking along a random heading (a race view in the woods)
                        int ti = rng.Next(f.treeCount);
                        var p = new Vector3(f.trees[ti * 8], f.trees[ti * 8 + 1], f.trees[ti * 8 + 2]);
                        float h = (float)(rng.NextDouble() * Math.PI * 2);
                        cam.transform.SetPositionAndRotation(p + new Vector3(Mathf.Cos(h) * 30f, 2.5f, Mathf.Sin(h) * 30f), Quaternion.Euler(0, (float)(rng.NextDouble() * 360), 0));
                        var sw = System.Diagnostics.Stopwatch.StartNew();
                        cam.Render();
                        if (i < 10) continue;
                        render.Add(sw.Elapsed.TotalMilliseconds); ms.Add(forest.msLast); sel.Add(forest.msSelect); up.Add(forest.msUpload); sub.Add(forest.msSubmit); counts.Add(forest.trees3d);
                    }
                    string St(List<double> l) { var a = l.OrderBy(x => x).ToArray(); return $"median {a[a.Length / 2]:F3} p95 {a[(int)(a.Length * 0.95)]:F3} mean {a.Average():F3} max {a[a.Length - 1]:F3}"; }
                    log.AppendLine($"{level} ({(locked ? "LockBufferForWrite ring" : "SetData")}): OriginalForest main thread per frame {St(ms)} ms | select {St(sel)} | upload {St(up)} | submit {St(sub)}; 3D trees per view mean {counts.Average():F0} max {counts.Max()}; whole cam.Render {St(render)} ms");
                }
                cam.targetTexture = null; rt.Release();
            }
            catch (Exception e) { log.AppendLine("FAILED " + e); }
            Directory.CreateDirectory(Path.GetDirectoryName(outBase));
            File.WriteAllText(outBase + "_perf.txt", log.ToString());
            Debug.Log("[ForestPerf] " + log);
        }
    }
}
