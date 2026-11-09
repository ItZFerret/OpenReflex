// WP-8 checks (physics props, animated textures, scenery LOD / draw distance, night sky): content read from the install at run time,
// compared with what the running original world holds (dumps of native props_test, _re/notes/props.md).
//   Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.PropsTests.Verify -logFile <log>
//   env PROPS_LOD_REF=<props_test PROPS_LOD dump of NAT_Concord> (optional), PROPS_SHOTS=<dir>
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using Reflex.Port.Content;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.Editor
{
    public static class PropsTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Props] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static string Shots() { string d = Env("PROPS_SHOTS", Path.Combine(Path.GetTempPath(), "props-shots")); Directory.CreateDirectory(d); return d; }

        public static void Verify()
        {
            report.Clear(); bad = 0;
            try { Lods(); Scroll(); Flipbook(); NightSky(); }
            catch (Exception e) { Check(false, "exception: " + e); }
            Log("verify: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(Shots(), "props-verify-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        static ReflexLevelContent Load(string level, bool scenery = true) =>
            ReflexLevelContent.Load(level, new ReflexLevelContent.Options { scenery = scenery, terrain = false, tracks = false });

        // draw distances and LOD distances of the placed models = the running world's ModelAsset +0x120 / +0x130
        static void Lods()
        {
            var c = Load("NAT_Concord");
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var data = c.ReadLodData();
            Log($"NAT_Concord: {data.Count} placed models, LOD data read in {sw.ElapsedMilliseconds} ms; multi-LOD {data.Values.Count(d => d.lodDistances.Length > 1)}, " +
                $"extra LOD meshes {data.Values.Sum(d => d.ExtraLodMeshes)}");
            var bale = data.TryGetValue("GL_RoundBale", out var b) ? b : null;
            Check(bale != null && bale.drawDistance == 635 && bale.lodDistances.SequenceEqual(new uint[] { 0 }), $"GL_RoundBale draw 635 from trunc(radius 4.7089 * 135) (got {bale?.drawDistance})");
            var amb = data.TryGetValue("AV_Ambulance_01", out var a) ? a : null;
            Check(amb != null && amb.drawDistance == 775 && amb.lodDistances.SequenceEqual(new uint[] { 0, 350, 500 }) && amb.ExtraLodMeshes == 2,
                  $"AV_Ambulance_01 draw 775 (file), LODs 0 / 350 / 500 with 2 extra meshes (got {amb?.drawDistance} [{(amb == null ? "" : string.Join(",", amb.lodDistances))}])");
            string refPath = Env("PROPS_LOD_REF", "");
            if (File.Exists(refPath))
            {
                int same = 0, diff = 0, missing = 0;
                foreach (var line in File.ReadAllLines(refPath))
                {
                    var m = Regex.Match(line, @"^model (\S+)\s+flags \S+ lodcount (\d+) draw (\d+) lods:(.*)$");
                    if (!m.Success) continue;
                    if (!data.TryGetValue(m.Groups[1].Value, out var d)) { missing++; continue; }
                    var lods = m.Groups[4].Value.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries).Select(uint.Parse).ToArray();
                    if (d.drawDistance == uint.Parse(m.Groups[3].Value) && d.lodDistances.SequenceEqual(lods)) same++;
                    else { diff++; Log($"  differs: {m.Groups[1].Value} world {m.Groups[3].Value} [{string.Join(",", lods)}] port {d.drawDistance} [{string.Join(",", d.lodDistances)}]"); }
                }
                Check(diff == 0 && same > 50, $"draw / LOD distances equal to the running world's ModelAssets: {same} equal, {diff} differ ({missing} models of the dump not placed in the Base scene)");
            }
            else Log("(no PROPS_LOD_REF dump: the world comparison is skipped)");
            // the rules
            Check(ReflexLevelContent.ParseLodData("x", FakeModel(0, 4.70892906f)).drawDistance == 635, "trunc(4.70892906 * 135) = 635");
            c.Dispose();
        }

        static byte[] FakeModel(uint draw, float radius)
        {
            var b = new byte[172 + 108 + 1];
            BitConverter.GetBytes(0x88aabb00u).CopyTo(b, 0); BitConverter.GetBytes(4).CopyTo(b, 4); BitConverter.GetBytes(1).CopyTo(b, 12); BitConverter.GetBytes(1).CopyTo(b, 16);
            BitConverter.GetBytes(draw).CopyTo(b, 168); BitConverter.GetBytes(radius).CopyTo(b, 172 + 64 + 8);
            return b;
        }

        // gTextureOffset: ScrollingTextureEntity::vf19 values of the running Kingston world at frame 600 (props_test PROPS_TEX):
        // period 50 / accumulator 11.626 -> 0.23252; period -17 -> -0.68387; 130 / 5.764 -> 0.04434; 30 -> 0.19212
        static void Scroll()
        {
            (float period, double t, float expect)[] cases = { (50, 11.626, 0.23252f), (-17, 11.626, -0.68387f), (-25, 11.626, -0.46503f), (130, 5.764, 0.04434f), (30, 5.764, 0.19212f), (1, 0.764, 0.764f), (0, 3, 0) };
            foreach (var (p, t, e) in cases)
            {
                float v = ReflexLevelContent.ScrollOffset(p, t);
                Check(Math.Abs(v - e) < 2e-5f, $"ScrollOffset({p}, {t}) = {v:F5} (world {e:F5})");
            }
            var c = Load("SX_Kingston");
            int n = c.instances.Count(i => i.entityType == "ScrollingTextureRSData");
            Log($"SX_Kingston: {n} ScrollingTextureRSData instances");
            Check(n == 18, "SX_Kingston has 18 scrolling-texture instances (the light beams)");
        }

        // TextureAnimControllerRSData: FMX_Dover's FMX_DO_Caustic flipbook (32 frames of 64 x 64 DXT1, 7 levels each)
        static void Flipbook()
        {
            var c = Load("FMX_Dover", false);
            var fb = c.FlipbookFor("FMX_DO_Caustic");
            var f = fb?.frames;
            Check(f != null && f.Length == 32 && f.All(t => t && t.width == 64 && t.height == 64 && t.mipmapCount == 7 && t.format == TextureFormat.DXT1),
                  $"FMX_DO_Caustic: {f?.Length} frames {f?[0]?.width}x{f?[0]?.height} levels {f?[0]?.mipmapCount} {f?[0]?.format}");
            var h = c.FlipbookFrameHashes("FMX_DO_Caustic");
            Check(h != null && h.Distinct().Count() == 32, $"32 different frames ({h?.Distinct().Count()} distinct level-0 hashes)");
        }

        // Template_Luma_Sky_Nighttime: the three slots go to star mask / blend / sky, the parameters from the material
        static void NightSky()
        {
            foreach (var level in new[] { "SX_Manchester", "FMX_DLC03_FunFactory" })
            {
                var c = Load(level);
                var root = c.BuildScenery(null, new ReflexLevelContent.SceneryShaders { scenery = Shader.Find("Reflex/Original Scenery"), sky = Shader.Find("Reflex/Original Sky") });
                var shader = Shader.Find("Reflex/Original Sky Night");
                Check(shader, "shader Reflex/Original Sky Night");
                var m = c.BuildNightSky(shader);
                Check(m, $"{level}: night sky material ({c.sky?.materials.FirstOrDefault()})");
                if (m)
                {
                    string T(string p) => m.GetTexture(p) ? m.GetTexture(p).name : "-";
                    Log($"  star mask {T("_StarMask")} blend {T("_BlendMap")} sky {T("_BaseMap")} bracket {m.GetFloat("_Bracket")} blend colour {m.GetVector("_BlendColor")} star {m.GetVector("_StarColor")} r1 {m.GetVector("_StarR1")} r2 {m.GetVector("_StarR2")}");
                    if (level == "SX_Manchester")
                        Check(T("_StarMask") == "Black_Base_NoStar_Sky" && T("_BlendMap") == "SX_Manchester_A_Sky_Blend" && T("_BaseMap") == "SX_Manchester_A_Sky" && Mathf.Approximately(m.GetFloat("_Bracket"), 1.5f)
                              && Vector4.Distance(m.GetVector("_StarR1"), new Vector4(4, 0, 0, 0)) < 1e-6f && Vector4.Distance(m.GetVector("_StarR2"), new Vector4(0, 3, 0, 0)) < 1e-6f,
                              "SX_Manchester_A_New_Sky slots and parameters as the material");
                }
                if (root) UnityEngine.Object.DestroyImmediate(root);
            }
        }
    }
}
