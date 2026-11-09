using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // Verifies the run-time UI reader (ReflexUiData, from the install) against the Python exporter's outputs in
    // _re/formats/out/ui (the reference): layer trees, hud/pause descriptions, fonts, textures (pixels) and string tables.
    // Run: Unity -batchmode -quit -projectPath <bench> -executeMethod Reflex.Port.Editor.UiContentTests.Verify   (REFLEX_REPO, REFLEX_GAME_DIR set)
    public static class UiContentTests
    {
        static string Ref => Path.Combine(Environment.GetEnvironmentVariable("REFLEX_REPO") is string r && r.Length > 0 ? r : Path.GetFullPath(Path.Combine(Application.dataPath, "../../..")), "_re/formats/out/ui");
        static readonly StringBuilder report = new StringBuilder();
        static void Log(string s) { report.AppendLine(s); Debug.Log("[UiVerify] " + s); }

        // numbers compare exactly (both sides come from the same decimal text or the same float32); everything else by value
        static void Diff(object a, object b, string path, List<string> diffs, HashSet<string> ignore, int max = 40)
        {
            if (diffs.Count >= max) return;
            if (a is Dictionary<string, object> da && b is Dictionary<string, object> db)
            {
                foreach (var k in da.Keys.Union(db.Keys))
                {
                    if (ignore != null && ignore.Contains(k)) continue;
                    bool ha = da.TryGetValue(k, out var va), hb = db.TryGetValue(k, out var vb);
                    if (!ha || !hb) { diffs.Add($"{path}/{k}: missing in {(ha ? "reference" : "port")}"); continue; }
                    Diff(va, vb, path + "/" + k, diffs, ignore, max);
                }
            }
            else if (a is List<object> la && b is List<object> lb)
            {
                if (la.Count != lb.Count) { diffs.Add($"{path}: list length {la.Count} (port) vs {lb.Count} (reference)"); return; }
                for (int i = 0; i < la.Count; i++) Diff(la[i], lb[i], path + "[" + i + "]", diffs, ignore, max);
            }
            else if (a is double xa && b is double xb) { if (xa != xb) diffs.Add($"{path}: {xa:R} (port) vs {xb:R} (reference)"); }
            else if (!Equals(a, b)) diffs.Add($"{path}: {Show(a)} (port) vs {Show(b)} (reference)");
        }
        static string Show(object o) => o == null ? "null" : o is string s ? "\"" + s + "\"" : o.ToString();

        static object Json(string rel) => OriginalUi.ParseJson(File.ReadAllText(Path.Combine(Ref, rel)));

        static int Compare(string what, object port, object reference, HashSet<string> ignore = null)
        {
            var diffs = new List<string>();
            Diff(port, reference, what, diffs, ignore);
            Log($"{what}: {(diffs.Count == 0 ? "identical" : diffs.Count + "+ differences")}");
            foreach (var d in diffs.Take(15)) Log("  " + d);
            return diffs.Count;
        }

        public static void Verify()
        {
            int bad = 0;
            try
            {
                ReflexUiData.ResetShared();
                var data = new ReflexUiData("ENG");
                Log("install: " + ReflexInstall.Dir + ", language " + data.language);
                var sw = System.Diagnostics.Stopwatch.StartNew();

                // ---- hud.json / pause.json (hand-written research notes excluded: binding, vtables)
                var hud = data.BuildHud(); var hudRef = Json("hud.json");
                var ign = new HashSet<string> { "binding", "vtables" };
                bad += Compare("hud.elements", hud["elements"], OriginalUi.Arr(hudRef, "elements"), ign);
                bad += Compare("hud.layerAnimations", hud["layerAnimations"], OriginalUi.Arr(hudRef, "layerAnimations"));
                bad += Compare("hud.dependencies", hud["dependenciesDeclaredByLayer"], OriginalUi.Arr(hudRef, "dependenciesDeclaredByLayer"));
                bad += Compare("hud.layerJson", hud["layerJson"], Json("layers/HudNationalSinglePlayer.ENG/SinglePlayerNational.lyr.json"));
                var pause = data.BuildPause(); var pauseRef = Json("pause.json");
                foreach (var k in new[] { "layout", "header", "buttonLegend", "confirmPopups", "animations", "dependenciesDeclaredByLayer" })
                    bad += Compare("pause." + k, pause[k], ((Dictionary<string, object>)pauseRef)[k]);
                Log($"hud/pause built in {sw.ElapsedMilliseconds} ms");

                // ---- every layer / page graph of the five exported packages
                int layers = 0, layerBad = 0;
                foreach (var pkg in new[] { "Base", "BaseLocalized", "InGame", "Shared", "HudNationalSinglePlayer" })
                {
                    string full = data.Pkg(pkg);
                    foreach (var name in data.Names(full, "uicmpnt"))
                    {
                        string rel = $"layers/{full}/{name}.json";
                        if (!File.Exists(Path.Combine(Ref, rel))) { Log("  no reference for " + rel); continue; }
                        var diffs = new List<string>();
                        Diff(data.LayerJson(pkg, name), Json(rel), rel, diffs, null);
                        layers++; if (diffs.Count > 0) { layerBad++; Log($"layer {full}/{name}: {diffs.Count}+ differences, e.g. {diffs[0]}"); }
                    }
                }
                Log($"layers compared: {layers}, differing: {layerBad}"); bad += layerBad;

                // ---- strings
                int tables = 0;
                foreach (var f in Directory.GetFiles(Path.Combine(Ref, "loc/ENG"), "*.json"))
                {
                    var j = OriginalUi.ParseJson(File.ReadAllText(f));
                    string pkg = OriginalUi.S(j, "package"), table = OriginalUi.S(j, "table");
                    var mine = data.Strings(pkg, table); var theirs = OriginalUi.Obj(j, "strings");
                    int d = mine.Count != theirs.Count ? 1 : 0;
                    foreach (var kv in theirs) if (!mine.TryGetValue(kv.Key, out var v) || v != (string)kv.Value) d++;
                    Log($"strings {pkg}/{table}: {mine.Count} keys, {d} differences"); bad += d; tables++;
                }

                // ---- fonts
                int fontCount = 0;
                foreach (var f in Directory.GetFiles(Path.Combine(Ref, "fonts"), "*.json"))
                {
                    string key = "ui/fonts/" + Path.GetFileName(f);
                    var j = OriginalUi.ParseJson(File.ReadAllText(f));
                    var res = data.FontData(key);
                    if (res == null) { Log("font missing " + key); bad++; continue; }
                    var atlasRef = LoadPng(Path.Combine(Ref, OriginalUi.S(j, "texture").Substring(3)));
                    var refFont = OriginalFont.Load(File.ReadAllText(f), atlasRef);
                    var mine = OriginalFont.Create(res);
                    int d = 0;
                    if (refFont.size != mine.size || refFont.letterSpacing != mine.letterSpacing || refFont.glyphs.Count != mine.glyphs.Count) d++;
                    foreach (var kv in refFont.glyphs)
                        if (!mine.glyphs.TryGetValue(kv.Key, out var g) || !g.Equals(kv.Value)) d++;
                    string texKey = OriginalUi.S(j, "texture");
                    if (res.texturePackage + "/" + res.name != texKey.Substring(12, texKey.Length - 16)) { d++; Log("  atlas package differs " + res.texturePackage); }
                    int px = ComparePixels(res.atlas, atlasRef);
                    Log($"font {Path.GetFileName(f)}: size {mine.size} glyphs {mine.glyphs.Count}, glyph/metric differences {d}, atlas pixel differences {px}");
                    bad += d + px; fontCount++;
                }

                // ---- texture atlases (the exporter's name-scan parse is the reference where it found real rects: w, h > 0)
                int atlasCount = 0, spritesChecked = 0, atlasBad = 0;
                foreach (var f in Directory.GetFiles(Path.Combine(Ref, "atlas"), "*.json", SearchOption.AllDirectories))
                {
                    var j = OriginalUi.ParseJson(File.ReadAllText(f));
                    string pkg = OriginalUi.S(j, "package"), name = OriginalUi.S(j, "name");
                    var mine = data.Atlas(pkg, name); int declared = (int)OriginalUi.F(j, "declaredSprites");
                    int d = mine.Count != declared ? 1 : 0;
                    foreach (var sp in OriginalUi.Arr(j, "sprites"))
                    {
                        if (OriginalUi.F(sp, "w") <= 0) continue;
                        spritesChecked++;
                        string sn = OriginalUi.S(sp, "name");
                        // the exporter's name scan drops a leading digit ("2x32Gradient" -> "x32Gradient")
                        if (!mine.TryGetValue(sn, out var m)) foreach (var kv in mine) if (kv.Key.EndsWith(sn, StringComparison.Ordinal) && kv.Key.Length == sn.Length + 1 && char.IsDigit(kv.Key[0])) { m = kv.Value; break; }
                        if (m == null || m.u0 != (float)OriginalUi.F(sp, "u0") || m.u1 != (float)OriginalUi.F(sp, "u1") || m.v0 != (float)OriginalUi.F(sp, "v0") || m.v1 != (float)OriginalUi.F(sp, "v1") ||
                            m.w != (int)OriginalUi.F(sp, "w") || m.h != (int)OriginalUi.F(sp, "h") || m.texture != OriginalUi.S(sp, "texture")) { d++; Log($"  atlas {pkg}/{name}: sprite {sn} differs"); }
                    }
                    Log($"atlas {pkg}/{name}: {mine.Count} sprites (declared {declared}), differences {d}"); atlasBad += d; atlasCount++;
                }
                Log($"atlases: {atlasCount}, exporter sprites compared: {spritesChecked}, differing: {atlasBad}"); bad += atlasBad;

                // ---- textures
                int texCount = 0, texBad = 0;
                foreach (var dir in Directory.GetDirectories(Path.Combine(Ref, "textures")))
                    foreach (var png in Directory.GetFiles(dir, "*.png"))
                    {
                        string key = "ui/textures/" + Path.GetFileName(dir) + "/" + Path.GetFileName(png);
                        var t = data.Texture(key); var r = LoadPng(png);
                        int px = t == null ? -1 : ComparePixels(t, r);
                        if (px != 0) { texBad++; Log($"texture {key}: {(t == null ? "missing" : px + " differing pixels")}"); }
                        texCount++;
                    }
                Log($"textures compared: {texCount}, differing: {texBad}"); bad += texBad;
                Log($"total {sw.ElapsedMilliseconds} ms; {tables} string tables, {fontCount} fonts");
            }
            catch (Exception e) { Log("EXCEPTION " + e); bad++; }
            Log(bad == 0 ? "UI VERIFY PASSED" : "UI VERIFY FAILED (" + bad + ")");
            File.WriteAllText(Path.Combine(Path.GetTempPath(), "ui-verify.txt"), report.ToString());
            EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        static Texture2D LoadPng(string path)
        {
            var t = new Texture2D(2, 2, TextureFormat.RGBA32, false, false);
            Debug.Assert(ImageConversion.LoadImage(t, File.ReadAllBytes(path), false));
            return t;
        }

        /// Differing pixels (any channel) between two textures (both bottom-up); -1 if the sizes differ.
        public static int ComparePixels(Texture2D a, Texture2D b)
        {
            if (!a || !b || a.width != b.width || a.height != b.height) return -1;
            var pa = a.GetPixels32(); var pb = b.GetPixels32(); int d = 0;
            for (int i = 0; i < pa.Length; i++) if (pa[i].r != pb[i].r || pa[i].g != pb[i].g || pa[i].b != pb[i].b || pa[i].a != pb[i].a) d++;
            return d;
        }
    }
}
