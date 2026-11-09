using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using Newtonsoft.Json.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using Reflex.Port.Content;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Tests
{
    // Object shading replay (_re/notes/object-shading.md): the object draws of a recorded frame of the original renderer
    // (render_capture REFLEX_OBJ_DUMP; prepared by the objshade tools' prep.py into <capture>/replay: object-space meshes, the draw's
    // matrix, its textures resolved to install assets, its constants and render states) rendered with Reflex/Vehicle - the material
    // made by the port's own factory (VehicleCatalog / ReflexObjectShading) for the game material the draw used - into an FP16 target
    // from the frame's own view-projection, with the frame's global constants, VSM moments, volume shadow and reflection map; each
    // draw is compared with the software-rasterized original on the pixels it owns in the frame (the capture's id buffer).
    //   REFLEX_OS_CAP=<capture dir> REFLEX_OS_OUT=<dir> Unity -batchmode -quit -projectPath <bench> -executeMethod Reflex.Port.Tests.ObjShadeTests.Replay
    // Writes <out>/replay_<frame>.json (per draw: program, material, pixels, mean |d|, means, constant mismatches) and
    // <out>/replay_<frame>_unity.raw (the draws' pixels composited: FP16 RGBA, rows top first, the capture's .raw layout).
    public static class ObjShadeTests
    {
        const int W = 1280, H = 720;

        static Vector4 V4(JToken t) => new Vector4((float)t[0], (float)t[1], (float)t[2], (float)t[3]);

        public static void Replay()
        {
            string capDir = Environment.GetEnvironmentVariable("REFLEX_OS_CAP");
            string outDir = Environment.GetEnvironmentVariable("REFLEX_OS_OUT") ?? Path.Combine(capDir, "replay");
            var log = new StringBuilder();
            try
            {
                Directory.CreateDirectory(outDir);
                var man = JObject.Parse(File.ReadAllText(Path.Combine(capDir, "replay", "manifest.json")));
                int frame = (int)man["frame"];
                var g = man["globals"];
                var vsf = g["vsf"]; var psf = g["psf"];

                // ---- the frame's global constants (render-pipeline.md section 3) ----
                Shader.SetGlobalFloat("_RfxRawPipeline", 1);
                Shader.SetGlobalVector("_RfxSunDir", V4(psf[55])); Shader.SetGlobalVector("_RfxSunColor", V4(psf[56]));
                for (int i = 0; i < 4; i++) { Shader.SetGlobalVector("_RfxLightDir" + i, V4(psf[45 + 2 * i])); Shader.SetGlobalVector("_RfxLightColor" + i, V4(psf[46 + 2 * i])); }
                Shader.SetGlobalVector("_RfxBounceColor", V4(psf[54])); Shader.SetGlobalVector("_RfxSpecularColor", V4(psf[61]));
                Shader.SetGlobalVector("_RfxColorRefSample", V4(vsf[84])); Shader.SetGlobalVector("_RfxColorRef", new Vector4(1, 1, 1, 1));   // Editor_Default_White
                Shader.SetGlobalVector("_RfxFogDistance", V4(vsf[14])); Shader.SetGlobalVector("_RfxFogColorStart", V4(vsf[15])); Shader.SetGlobalVector("_RfxFogColorEnd", V4(vsf[16]));
                var delta = V4(vsf[16]) - V4(vsf[15]);
                log.AppendLine($"fog delta c26 {V4(vsf[26])} end - start {delta}");
                Shader.SetGlobalFloat("_RfxVsmOn", 1);
                Shader.SetGlobalVectorArray("_RfxShadowProj1", new[] { V4(vsf[17]), V4(vsf[18]), V4(vsf[19]), V4(vsf[20]) });
                Shader.SetGlobalVectorArray("_RfxShadowProj2", new[] { V4(vsf[21]), V4(vsf[22]), V4(vsf[23]), V4(vsf[24]) });
                Shader.SetGlobalVector("_RfxShadowPlane", V4(psf[25]));
                var created = new List<UnityEngine.Object>();
                if (g["vsm"] != null && g["vsm"].Type == JTokenType.String)
                {
                    var vsm = LoadRaw(Path.Combine(capDir, "rt", (string)g["vsm"]), out int vw, out int vh, out int vf);
                    var t = new Texture2D(vw, vh, TextureFormat.RGFloat, false, true) { filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp };
                    var px = new float[vw * vh * 2];
                    for (int y = 0; y < vh; y++) for (int x = 0; x < vw; x++) { int s = (y * vw + x) * 4, d = ((vh - 1 - y) * vw + x) * 2; px[d] = vsm[s]; px[d + 1] = vsm[s + 1]; }
                    t.SetPixelData(px, 0); t.Apply(false, false); created.Add(t);
                    Shader.SetGlobalTexture("_RfxVsmMap", t);
                    log.AppendLine($"vsm {g["vsm"]} {vw}x{vh} format {vf}");
                }
                Shader.SetGlobalVector("_RfxVolumeShadowParams", new Vector4((float)vsf[39][0], 0, 0, 0));
                var vol = g["volume"]?["asset"];
                if (vol != null && vol.Type == JTokenType.Array)
                {
                    var t = ReflexObjectShading.InstallTexture((string)vol[0], (string)vol[1], (string)vol[2], 3, 3, 2, 2, 1);
                    if (t) { Shader.SetGlobalTexture("_RfxVolumeShadowMap", t); Shader.SetGlobalVector("_RfxVolumeShadowParams", new Vector4((float)vsf[39][0], 1, 0, 0)); }
                    log.AppendLine($"volume shadow {vol} {(t ? "bound" : "MISSING")}");
                }
                var cref = g["colorref"]?["asset"];
                Shader.SetGlobalFloat("_RfxColorRefOn", 0);
                if (cref != null && cref.Type == JTokenType.Array)
                {
                    var t = ReflexObjectShading.InstallTexture((string)cref[0], (string)cref[1], (string)cref[2], 3, 3, 2, 2, 2);
                    if (t) { Shader.SetGlobalTexture("_RfxColorRefMap", t); Shader.SetGlobalFloat("_RfxColorRefOn", 1); }
                    log.AppendLine($"colour reference {cref} {(t ? "bound" : "MISSING")}");
                }
                var refl = g["reflection"]?["asset"];
                Shader.SetGlobalFloat("_RfxReflectionOn", 0);
                if (refl != null && refl.Type == JTokenType.Array)
                {
                    int flip = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_OS_CUBEFLIP"), out int cf) ? cf : 0;
                    var c = flip == 0 ? ReflexObjectShading.InstallTexture((string)refl[0], (string)refl[1], (string)refl[2], 3, 3, 2, 2, 2) as Cubemap
                                      : ReflexObjectShading.InstallCube((string)refl[0], (string)refl[1], (string)refl[2], flip);
                    if (c) { Shader.SetGlobalTexture("_RfxReflectionCube", c); Shader.SetGlobalFloat("_RfxReflectionOn", 1); }
                    log.AppendLine($"reflection {refl} {(c ? "bound" : "MISSING")}");
                }

                // ---- the frame's view-projection: D3D (row-vector-free rows, clip z 0..1) -> Unity's GL-style projection, D3D9 pixel centres ----
                var vp = g["viewProjection"];
                var r0 = V4(vp[0]); var r1 = V4(vp[1]); var r2 = V4(vp[2]); var r3 = V4(vp[3]);
                r0 += r3 * (1f / W); r1 -= r3 * (1f / H);   // pixel (i, j) of D3D9 = the scene point at i, j (Unity: i + 0.5)
                var glz = 2 * r2 - r3;
                var proj = new Matrix4x4(); proj.SetRow(0, r0); proj.SetRow(1, r1); proj.SetRow(2, glz); proj.SetRow(3, r3);
                var camPos = V4(vsf[8]);
                var flipZ = Matrix4x4.Scale(new Vector3(1, 1, -1));
                // the camera's view (billboards and particles face it): Unity's right-handed view = flipZ x the D3D view; the projection then
                // gives the frame's own view-projection
                Matrix4x4 viewU = flipZ, projU = proj * flipZ;
                if (g["view"] is JArray vwr && vwr.Count == 4)
                {
                    var Vd = new Matrix4x4(); for (int i = 0; i < 4; i++) Vd.SetRow(i, V4(vwr[i]));
                    viewU = flipZ * Vd; projU = proj * viewU.inverse;
                }
                var c14 = V4(vsf[14]); Shader.SetGlobalVector("_RfxFogParams", new Vector4(c14.x, c14.y, V4(vsf[15]).w, V4(vsf[16]).w));
                Shader.SetGlobalVector("_WorldSpaceCameraPos", camPos);

                // ---- the reference ----
                var ids = LoadIds(Path.Combine(capDir, "rt", (string)g["ids"]));
                var idsFinal = LoadIds(Path.Combine(capDir, "rt", $"f{frame}_ids.raw"));
                var refFinal = g["hdrFinal"]?.Type == JTokenType.String ? LoadRaw(Path.Combine(capDir, "rt", (string)g["hdrFinal"]), out _, out _, out _) : null;
                var buffers = new List<ComputeBuffer>();
                Material depthMat = null; Texture2D sceneDepth = null;
                string depthFile = Path.Combine(capDir, "rt", $"f{frame}_depth_preparticles.raw");
                if (File.Exists(depthFile))
                {
                    var z = LoadRaw(depthFile, out int zw, out int zh, out _);
                    sceneDepth = new Texture2D(zw, zh, TextureFormat.RFloat, false, true) { filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp };
                    var zf = new float[zw * zh]; for (int y = 0; y < zh; y++) for (int x = 0; x < zw; x++) zf[(zh - 1 - y) * zw + x] = z[(y * zw + x) * 4];
                    sceneDepth.SetPixelData(zf, 0); sceneDepth.Apply(false, false); created.Add(sceneDepth);
                }
                Texture TexOf(JToken dd, string sl)
                {
                    var t = dd["textures"]?[sl]; var a = t?["asset"]; var ss = t?["ss"];
                    if (a == null || a.Type != JTokenType.Array) return null;
                    return ss != null && ss.Type == JTokenType.Array ? ReflexObjectShading.InstallTexture((string)a[0], (string)a[1], (string)a[2], (int)ss[0], (int)ss[1], (int)ss[4], (int)ss[5], (int)ss[6], Math.Max(1, (int)ss[9]))
                                                                     : ReflexObjectShading.InstallTexture((string)a[0], (string)a[1], (string)a[2]);
                }
                var refHdr = LoadRaw(Path.Combine(capDir, "rt", (string)g["hdr"]), out _, out _, out _);
                var composite = new float[W * H * 4];
                var results = new JArray();
                var rt = new RenderTexture(W, H, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear) { name = "replay" }; rt.Create();
                var read = new Texture2D(W, H, TextureFormat.RGBAFloat, false, true);
                var cb = new CommandBuffer { name = "replay" };
                var materials = new Dictionary<string, (Material m, string key)>();
                Texture2D[] plates = null;
                foreach (var d in man["draws"])
                {
                    var res = new JObject { ["draw"] = d["draw"], ["program"] = d["program"], ["pixels"] = d["pixels"] };
                    results.Add(res);
                    if (d["skip"] != null) { res["skip"] = d["skip"]; continue; }
                    int draw = (int)d["draw"];
                    string kind = (string)d["kind"] ?? "object";
                    var idsArr = (string)d["ids"] == "ids" ? idsFinal : ids; var refArr = (string)d["ids"] == "ids" ? refFinal : refHdr;
                    var pre = d["pre"]?.Type == JTokenType.String ? (string)d["pre"] : null;
                    cb.Clear();
                    cb.SetRenderTarget(rt);
                    if (pre != null)
                    {
                        var preTex = RawToTexture(LoadRaw(Path.Combine(capDir, pre), out _, out _, out _)); created.Add(preTex);
                        cb.Blit(preTex, rt); cb.SetRenderTarget(rt); cb.ClearRenderTarget(true, false, Color.clear);
                    }
                    else cb.ClearRenderTarget(true, true, new Color(0, 0, 0, -1));
                    if (kind == "particle" && sceneDepth)   // the scene's depth before the particles: they are hidden where the original's were
                    {
                        if (!depthMat) { depthMat = new Material(Shader.Find("Hidden/Reflex/ObjShadeReplayDepth")); created.Add(depthMat); }
                        depthMat.SetTexture("_MainTex", sceneDepth);
                        cb.SetViewProjectionMatrices(Matrix4x4.identity, Matrix4x4.identity);
                        cb.DrawProcedural(Matrix4x4.identity, depthMat, 0, MeshTopology.Triangles, 3);
                    }
                    cb.SetViewProjectionMatrices(viewU, projU);   // a right-handed view as a Unity camera's (winding / culling as Unity expects)
                    cb.SetGlobalVector("_WorldSpaceCameraPos", camPos);
                    if (kind == "bbcrowd")
                    {
                        // Reflex/Original Crowd Billboard on the section mesh, the frame's constants (time c65, emptyChairRatio c66, isAnimating c71, colorTable c72)
                        var mesh = LoadMesh2(Path.Combine(capDir, "replay", (string)d["mesh"])); created.Add(mesh);
                        var mrow = d["matrix"]; var M = new Matrix4x4();
                        for (int i = 0; i < 4; i++) M.SetRow(i, new Vector4((float)mrow[i][0], (float)mrow[i][1], (float)mrow[i][2], (float)mrow[i][3]));
                        var c = d["consts"];
                        var m = new Material(Shader.Find("Reflex/Original Crowd Billboard")); created.Add(m);
                        var t0 = TexOf(d, "0"); var t1 = TexOf(d, "1") ?? t0;
                        if (t0) m.SetTexture("_BaseMap", t0); if (t1) m.SetTexture("_EmptyChair", t1);
                        m.SetFloat("_CrowdTime", (float)c["time"][0]); m.SetFloat("_EmptyChairRatio", (float)c["ratio"][0]); m.SetFloat("_IsAnimating", (float)c["animating"][0]);
                        Shader.SetGlobalVectorArray("_RfxCrowdColorTable", ((JArray)c["colorTable"]).Select(V4).ToArray());
                        res["material"] = "Reflex/Original Crowd Billboard";
                        cb.DrawMesh(mesh, M, m, 0, 0);
                    }
                    else if (kind == "particle")
                    {
                        // Reflex/Original Particles over the instance records (OriginalParticles' ReflexParticle layout), the manager's constants
                        var bytes = File.ReadAllBytes(Path.Combine(capDir, "replay", (string)d["particles"])); int count = (int)d["count"];
                        var buf = new ComputeBuffer(count * 4, 16); buf.SetData(bytes); buffers.Add(buf);
                        var c = d["consts"];
                        var m = new Material(Shader.Find("Reflex/Original Particles")); created.Add(m);
                        var rs = d["rs"];
                        m.SetFloat("_SrcBlend", (float)BlendOf((int)rs["SRCBLEND"])); m.SetFloat("_DstBlend", (float)BlendOf((int)rs["DESTBLEND"]));
                        var props = new MaterialPropertyBlock();
                        props.SetBuffer("_Particles", buf); props.SetInt("_Base", 0);
                        var t0 = TexOf(d, "0"); if (t0) props.SetTexture("_MainTex", t0);
                        props.SetVector("_Luminance", V4(c["lum"])); props.SetVector("_LuminanceDelta", V4(c["delta"])); props.SetVector("_Fade", V4(c["fade"]));
                        props.SetFloat("_Soft", (bool)c["soft"] ? 1 : 0);
                        res["material"] = "Reflex/Original Particles";
                        cb.DrawProcedural(Matrix4x4.identity, m, 0, MeshTopology.Triangles, count * 6, 1, props);
                    }
                    else
                    {
                    var mesh = LoadMesh(Path.Combine(capDir, "replay", (string)d["mesh"])); created.Add(mesh);
                    var mrow = d["matrix"]; var M = new Matrix4x4();
                    for (int i = 0; i < 4; i++) M.SetRow(i, new Vector4((float)mrow[i][0], (float)mrow[i][1], (float)mrow[i][2], (float)mrow[i][3]));
                    // the game material of the draw: a material of the textures' packages with this program and these textures
                    string sig = (string)d["program"] + "|" + string.Join(",", ((JObject)d["textures"]).Properties().Select(p => p.Name + "=" + (p.Value["asset"]?.Type == JTokenType.Array ? string.Join("/", p.Value["asset"]) : (string)p.Value["raw"] ?? "?")));
                    if (!materials.TryGetValue(sig, out var mm))
                    {
                        var texs = ((JObject)d["textures"]).Properties().Where(p => p.Value["asset"]?.Type == JTokenType.Array)
                            .Select(p => (int.Parse(p.Name), (string)p.Value["asset"][0], (string)p.Value["asset"][1], (string)p.Value["asset"][2])).ToList();
                        var consts = ((JObject)d["psc"]).Properties().ToDictionary(p => p.Name, p => V4(p.Value[0]));
                        var mat = ReflexObjectShading.MaterialFor((string)d["program"], texs, consts, out string key, g["levelDbs"]?.Select(x => (string)x));
                        if (!mat) { mat = CaptureMaterial(d, log); key = "capture-bound"; }
                        mm = (mat, key);
                        materials[sig] = mm; created.Add(mat);
                    }
                    res["material"] = mm.key;
                    string ptag = mm.m.GetTag(ReflexPlates.Tag, false, "");
                    if (ptag.Length > 0)   // the run-time plates (ReflexPlates): the frame's rider "RIDER" / 998
                    {
                        if (plates == null)
                        {
                            var (j, cp) = ReflexPlateRaster.Rider("RIDER", "998", false, false);
                            plates = new[] { ReflexPlates.ToTexture(j, 128, "jersey plate"), ReflexPlates.ToTexture(cp, 128, "cp plate"), ReflexPlates.ToTexture(ReflexPlateRaster.Bike("998", false), 128, "bike plate") };
                        }
                        var pp = ptag.Split(':'); string tn = pp[1].ToLowerInvariant();
                        var pt = tn == "numbers" ? plates[2] : tn.Contains("cp_namnum") ? plates[1] : plates[0];
                        string slot = pp[0].StartsWith("_Tex", StringComparison.Ordinal) ? pp[0].Substring(4) : null;
                        var dev = slot != null ? d["textures"][slot]?["raw"] : null;   // the frame's own plate (this rider's name / number)
                        if (dev != null && dev.Type == JTokenType.String) { var dt = DeviceTexture(Path.Combine(capDir, (string)dev), d["textures"][slot]["ss"]); if (dt) { created.Add(dt); pt = dt; } }
                        if (slot != null) mm.m.SetTexture(pp[0], pt);
                    }
                    foreach (var p in ((JObject)d["psc"]).Properties()) if (p.Name.StartsWith("usr_mud_", StringComparison.Ordinal)) mm.m.SetVector(p.Name, V4(p.Value[0]));   // ReflexMud: the rider's record
                    // the material's constants against the draw's (non-global) pixel constants
                    var mis = new JArray();
                    foreach (var p in ((JObject)d["psc"]).Properties())
                    {
                        if (!p.Name.StartsWith("usr_", StringComparison.Ordinal) && p.Name != "gTextureOffset" || p.Name.StartsWith("usr_mud_", StringComparison.Ordinal)) continue;
                        var want = V4(p.Value[0]); var have = mm.m.GetVector(p.Name);
                        if ((want - have).magnitude > 1e-4f) mis.Add($"{p.Name}: capture {want} material {have}");
                    }
                    if (mis.Count > 0) res["constantMismatch"] = mis;
                    cb.DrawMesh(mesh, M, mm.m, 0, 0);
                    }
                    Graphics.ExecuteCommandBuffer(cb);
                    var prev = RenderTexture.active; RenderTexture.active = rt;
                    read.ReadPixels(new Rect(0, 0, W, H), 0, 0); read.Apply(); RenderTexture.active = prev;
                    var px = read.GetRawTextureData<float>();
                    double sum = 0; int n = 0, miss = 0; Vector3 mr = default, mu = default;
                    for (int y = 0; y < H; y++)
                        for (int x = 0; x < W; x++)
                        {
                            if (idsArr[y * W + x] != draw) continue;
                            int ui = ((H - 1 - y) * W + x) * 4, ri = (y * W + x) * 4;
                            if (pre == null && px[ui + 3] == -1) { miss++; continue; }
                            float dr = 0; bool finite = true;
                            for (int k = 0; k < 3; k++) { float a = refArr[ri + k], b = px[ui + k]; if (!float.IsFinite(a) || !float.IsFinite(b)) finite = false; dr += Mathf.Abs(a - b); }
                            if (!finite) continue;
                            sum += dr / 3; n++; mr += new Vector3(refArr[ri], refArr[ri + 1], refArr[ri + 2]); mu += new Vector3(px[ui], px[ui + 1], px[ui + 2]);
                            for (int k = 0; k < 4; k++) composite[ri + k] = px[ui + k];
                        }
                    res["compared"] = n; res["missing"] = miss;
                    if (n > 0) { res["meanAbs"] = sum / n; res["meanRef"] = new JArray(mr.x / n, mr.y / n, mr.z / n); res["meanUnity"] = new JArray(mu.x / n, mu.y / n, mu.z / n); }
                }
                File.WriteAllText(Path.Combine(outDir, $"replay_{frame}.json"), new JObject { ["log"] = log.ToString(), ["draws"] = results }.ToString());
                using (var f = new BinaryWriter(File.Create(Path.Combine(outDir, $"replay_{frame}_unity.raw"))))
                {
                    f.Write(116); f.Write(W); f.Write(H); f.Write(W * 16);
                    foreach (var v in composite) f.Write(v);
                }
                foreach (var o in created) if (o) UnityEngine.Object.DestroyImmediate(o);
                foreach (var bf in buffers) bf.Release();
                rt.Release();
                log.AppendLine("ok");
            }
            catch (Exception e) { log.AppendLine("FAILED " + e); }
            Debug.Log("[ObjShade] " + log);
        }

        // ------------------------------------------------------------------------------------------- the material rule
        // Which material each part binding of each bike ends up with in the running game (a world image of a 12-rider race, read through
        // the plugin): the vehicle's own model V+0x1328 (ModelAsset; LOD records at +0x130, 0x1c each, the bindings at +0x10 / +0x14,
        // 16 bytes: surface, node, the part's ModelAssetHandle, the MaterialAssetHandle; handles' names inline at +4), against the port's
        // ReflexBikeModel for the slot's BikeSpec, and against the game's own rule (VehicleLoader_OverrideMaterials FUN_0053d8b0 with
        // MaterialKey FUN_00543fc0) applied to the bikes' part / material package pairs.
        //   REFLEX_OS_WORLD=<world.bin> REFLEX_OS_DEFORM=<deform.bin> REFLEX_OS_OUT=<dir> ... -executeMethod Reflex.Port.Tests.ObjShadeTests.BindingRule
        public static void BindingRule()
        {
            var log = new StringBuilder();
            string outDir = Environment.GetEnvironmentVariable("REFLEX_OS_OUT") ?? Path.GetTempPath();
            IntPtr w = IntPtr.Zero;
            try
            {
                w = Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_create(ReflexInstall.Exe, Path.Combine(ReflexCrt.Dir, "msvcr90.dll"),
                    Environment.GetEnvironmentVariable("REFLEX_OS_WORLD"), Environment.GetEnvironmentVariable("REFLEX_OS_DEFORM"));
                if (w == IntPtr.Zero) throw new Exception("world not created");
                var buf = new byte[256];
                uint U(uint a) => Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(w, a, buf, 4) == 0 ? BitConverter.ToUInt32(buf, 0) : 0u;
                string S(uint a) { if (a == 0 || Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(w, a, buf, 128) != 0) return "?"; int e = Array.IndexOf(buf, (byte)0, 0, 128); return Encoding.ASCII.GetString(buf, 0, e < 0 ? 128 : e); }
                var roster = Reflex.Port.OriginalWorld.ReflexRoster.Read(w);
                int total = 0, portOk = 0, ruleOk = 0, firstOk = 0, hypOk = 0;
                foreach (var e in roster.entries)
                {
                    uint V = e.slot.vehicle, M = U(V + 0x1328), lb = U(M + 0x130), bb = U(lb + 0x10), be = U(lb + 0x14);
                    var game = new Dictionary<(string, int), string>();
                    for (uint b = bb; b < be && b - bb < 16 * 128; b += 16) game[(S(U(b + 8) + 4), (int)U(b))] = S(U(b + 0xc) + 4);
                    var spec = e.bike ?? BikeSpec.MarsFLite;
                    var bike = ReflexBikeModel.Build(spec);
                    log.AppendLine($"slot {e.slot.slot} {e.name}: {spec.chassisPkg} kit {spec.kitPkg} tyres {spec.tirePkg} parts {string.Join(",", spec.partModels.Select(p => p.pkg))} materials {string.Join(",", (spec.materials ?? new List<(string, string)>()).Select(p => p.pkg))}");
                    // the game's rule on this bike's pairs: chassis <- kit; the tyre models <- their own package, then the wheels' (the
                    // (2, 6) cross pair); a part model <- its own material package
                    var wheels = (spec.materials ?? new List<(string db, string pkg)>()).Where(p => p.db.IndexOf("Wheels", StringComparison.OrdinalIgnoreCase) >= 0).ToList();
                    foreach (var p in bike.parts)
                    {
                        var key = game.Keys.FirstOrDefault(k => p.name.StartsWith(k.Item1 + "_" + k.Item2 + "_", StringComparison.Ordinal));
                        if (key.Item1 == null) { log.AppendLine($"   {p.name}: not in the game's model"); continue; }
                        total++;
                        string want = game[key], mat = p.part.GetComponent<Renderer>().sharedMaterial.name;
                        string port = mat.Contains(" -> ") ? mat.Substring(mat.IndexOf(" -> ", StringComparison.Ordinal) + 4) : mat;
                        var pkgs = new List<(string db, string pkg)>();
                        if (key.Item1 == spec.chassisModel) { if (spec.kitPkg != null) pkgs.Add((spec.kitDb, spec.kitPkg)); }
                        else if (key.Item1 == spec.frontModel || key.Item1 == spec.rearModel) { pkgs.Add((spec.tireDb, spec.tirePkg)); pkgs.AddRange(wheels); }
                        else
                        {
                            int k = spec.partModels.FindIndex(pm => ReflexObjectShading.ModelNamesOf(pm.db, pm.pkg).Contains(key.Item1));
                            var pmats = k >= 0 && k < spec.partMaterials.Count ? spec.partMaterials[k] : null;
                            if (pmats == null && k >= 0) foreach (var m in spec.materials ?? new List<(string db, string pkg)>()) if (string.Equals(m.db, spec.partModels[k].db, StringComparison.OrdinalIgnoreCase)) { pmats = m; break; }
                            if (k >= 0) pkgs.Add(spec.partModels[k]);   // the part's own model package (its own materials, the (i, i) pass)
                            if (pmats != null) pkgs.Add(pmats.Value);
                        }
                        string Rule(bool firstWins)
                        {
                            string cur = p.material;
                            foreach (var (db, pkg) in pkgs)
                            {
                                bool done = false;
                                foreach (string m in ReflexObjectShading.MaterialNamesOf(db, pkg))
                                {
                                    string kc = ReflexVehicles.MaterialKey(cur), km = ReflexVehicles.MaterialKey(m);
                                    if (km.Length > 0 && kc.Length >= km.Length && string.Compare(kc, 0, km, 0, km.Length, StringComparison.OrdinalIgnoreCase) == 0) { cur = m; if (firstWins) { done = true; break; } }
                                }
                                if (done) continue;
                            }
                            return cur;
                        }
                        // hypothesis R: over the vehicle's material packages in order (the model's own package first), the material of the
                        // binding's exact name, else the first whose key prefixes the binding's key
                        // hypothesis R: the first material whose key prefixes the binding's key, over the vehicle's material packages in part
                        // order (the kit first) and then the model's own package; none: the binding as it is
                        string R()
                        {
                            var all = new List<(string db, string pkg)>(spec.materials ?? new List<(string db, string pkg)>());
                            string kb = ReflexVehicles.MaterialKey(p.material);
                            foreach (var (db, pkg) in all) foreach (string m in ReflexObjectShading.MaterialNamesOf(db, pkg)) { string km = ReflexVehicles.MaterialKey(m); if (km.Length > 0 && kb.Length >= km.Length && string.Compare(kb, 0, km, 0, km.Length, StringComparison.OrdinalIgnoreCase) == 0) return m; }
                            return p.material;
                        }
                        string hyp = R(); bool h = string.Equals(hyp, want, StringComparison.OrdinalIgnoreCase); if (h) hypOk++;
                        if (!h) log.AppendLine($"   R: {key.Item1} s{key.Item2} binding {p.material}: game {want} | R {hyp}");
                        string rule = Rule(false), first = Rule(true);
                        bool a = string.Equals(port, want, StringComparison.OrdinalIgnoreCase), r = string.Equals(rule, want, StringComparison.OrdinalIgnoreCase), f = string.Equals(first, want, StringComparison.OrdinalIgnoreCase);
                        if (a) portOk++; if (r) ruleOk++; if (f) firstOk++;
                        if (!a || !r || !f) log.AppendLine($"   {key.Item1} s{key.Item2} binding {p.material}: game {want} | port {(a ? "=" : port)} | rule {(r ? "=" : rule)} | first-match {(f ? "=" : first)} [{string.Join(",", pkgs.Select(x => x.pkg))}]");
                    }
                    bike.Destroy();
                }
                log.AppendLine($"bindings {total}: the port's choice = the game's on {portOk}, the game's rule (last match) on {ruleOk}, first match on {firstOk}, R (the first key match over the parts' material packages, else the model's own material of the binding's name) on {hypOk}");
            }
            catch (Exception ex) { log.AppendLine("FAILED " + ex); }
            finally { if (w != IntPtr.Zero) Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_destroy(w); }
            File.WriteAllText(Path.Combine(outDir, "binding_rule.txt"), log.ToString());
            Debug.Log("[ObjShade] " + log);
        }

        /// The Super Buggy's run-time shader constants in the running world (ReflexVehicles.CollectConstants): the override list of every
        /// slot's vehicle and the values over a run with the throttle held (REFLEX_OS_WORLD / REFLEX_OS_DEFORM: a Super Buggy world).
        public static void VehicleConstants()
        {
            var log = new StringBuilder(); IntPtr w = IntPtr.Zero;
            try
            {
                w = Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_create(ReflexInstall.Exe, Path.Combine(ReflexCrt.Dir, "msvcr90.dll"),
                    Environment.GetEnvironmentVariable("REFLEX_OS_WORLD"), Environment.GetEnvironmentVariable("REFLEX_OS_DEFORM"));
                if (w == IntPtr.Zero) throw new Exception("world not created");
                var slots = new Reflex.Port.OriginalWorld.ReflexSlot[16];
                int n = Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_slots(w, slots, 16);
                var keys = new byte[256]; keys[0x11] = 0x80;   // W: throttle
                var flags = new byte[64]; var scratch = new byte[64 * 16];
                for (int f = 0; f <= 1200; f++)
                {
                    if (Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_frame(w, 1f / 60, keys, null, 0) != 0) { log.AppendLine("frame failed at " + f); break; }
                    if (f % 150 != 0) continue;
                    var line = new StringBuilder($"frame {f}:");
                    for (int k = 0; k < n; k++)
                    {
                        ReflexVehicles.NodeFlags(w, slots[k].vehicle, flags, scratch);
                        if (ReflexVehicles.TryConstants(slots[k].vehicle, out var names, out var values) && names.Length > 0)
                            line.Append($" slot {slots[k].slot}" + string.Concat(names.Select((nm, i) => $" {nm}={values[i].x:0.###}")));
                    }
                    log.AppendLine(line.ToString());
                }
            }
            catch (Exception ex) { log.AppendLine("FAILED " + ex); }
            finally { if (w != IntPtr.Zero) Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_destroy(w); }
            Debug.Log("[ObjShade] " + log);
        }

        /// Builds one AI vehicle (REFLEX_OS_VID) and logs any exception in full.
        public static void BuildVehicle()
        {
            var log = new StringBuilder();
            try
            {
                var t = new ReflexGameTables(new Reflex.Port.FrontEnd.FeTableSource(Reflex.Port.FrontEnd.FeTables.Shared));
                int vid = int.Parse(Environment.GetEnvironmentVariable("REFLEX_OS_VID") ?? "72");
                var fe = Reflex.Port.FrontEnd.FeTables.Shared;
                int model = Reflex.Port.FrontEnd.FeTables.I(fe.Rows("AIVehicle").First(x => Reflex.Port.FrontEnd.FeTables.I(x, "AIVehicleId") == vid), "VehicleModelId");
                var spec = ReflexVehicleSpecs.Bike(t, model, t.Where("AIVehicleParts", "AIVehicleId", vid).Select(i => t.Int("AIVehicleParts", "VehiclePartId", i)).ToList());
                var m = ReflexBikeModel.Build(spec);
                log.AppendLine($"built {m.parts.Count} parts");
            }
            catch (Exception e) { log.AppendLine("FAILED " + e); }
            Debug.Log("[ObjShade] " + log);
        }

        /// The front end's garage scene (UI_Compound: the MXGarage preview's level) as a level: its lighting entities.
        public static void GarageLighting()
        {
            var log = new StringBuilder();
            foreach (string lv in new[] { "UI_Compound", "UIScene" })
            {
                try
                {
                    var c = ReflexLevelContent.Load(lv, new ReflexLevelContent.Options { scenery = false, terrain = false, tracks = false });
                    var l = c.lighting;
                    log.AppendLine($"{lv}: sun {l.sunDirection} x {l.sunColor}; lights {string.Join(" ", l.lightColors)} bounce {l.bounceColor} offset {l.lightDirectionOffset}; fog {l.fogDistanceStart}..{l.fogDistanceEnd} {l.fogColorStart} {l.fogAmountStart}..{l.fogAmountEnd}; hdr {l.hasHdrSettings} key {l.hdr.keyValue}");
                    log.AppendLine("   " + c.report);
                }
                catch (Exception e) { log.AppendLine(lv + ": " + e.Message); }
            }
            Debug.Log("[ObjShade] " + log);
        }

        /// Compiles Reflex/Vehicle (every program drawn once) and logs the shader's messages.
        public static void Compile()
        {
            var sh = Shader.Find("Reflex/Vehicle");
            var log = new StringBuilder($"shader {(sh ? sh.name : "MISSING")} supported {(sh ? sh.isSupported : false)}\n");
            if (sh)
            {
                foreach (var m in ShaderUtil.GetShaderMessages(sh)) log.AppendLine($"{m.severity} {m.message} {m.file}:{m.line}");
                var t0 = DateTime.Now;
                var rt = new RenderTexture(64, 64, 24, RenderTextureFormat.ARGBHalf); rt.Create();
                var mesh = Resources.GetBuiltinResource<Mesh>("Cube.fbx");
                var cb = new CommandBuffer(); cb.SetRenderTarget(rt); cb.ClearRenderTarget(true, true, Color.black);
                cb.SetViewProjectionMatrices(Matrix4x4.TRS(new Vector3(0, 0, -5), Quaternion.identity, Vector3.one).inverse, Matrix4x4.Perspective(60, 1, 0.1f, 100));
                var mats = new List<Material>();
                for (int i = 0; i < ReflexObjectShading.Programs.Length; i++) { var m = new Material(sh); m.SetFloat("_Program", i); mats.Add(m); cb.DrawMesh(mesh, Matrix4x4.identity, m, 0, 0); }
                Graphics.ExecuteCommandBuffer(cb);
                var prev = RenderTexture.active; RenderTexture.active = rt; var t = new Texture2D(64, 64, TextureFormat.RGBAFloat, false, true); t.ReadPixels(new Rect(0, 0, 64, 64), 0, 0); RenderTexture.active = prev;
                log.AppendLine($"drawn {mats.Count} programs in {(DateTime.Now - t0).TotalSeconds:F1} s, centre {t.GetPixel(32, 32)}");
                foreach (var m in ShaderUtil.GetShaderMessages(sh)) log.AppendLine($"{m.severity} {m.message} {m.file}:{m.line}");
            }
            Debug.Log("[ObjShade] " + log);
        }

        /// No game material found: the draw's own binding (its textures from the install, its constants as captured).
        static Material CaptureMaterial(JToken d, StringBuilder log)
        {
            var m = new Material(ReflexVehicleShader.Get()) { name = "capture " + d["draw"] };
            m.SetFloat("_Program", ReflexObjectShading.ProgramId((string)d["program"]));
            foreach (var p in ((JObject)d["psc"]).Properties()) if (p.Name.StartsWith("usr_", StringComparison.Ordinal) || p.Name == "gTextureOffset") m.SetVector(p.Name, V4(p.Value[0]));
            foreach (var p in ((JObject)d["textures"]).Properties())
            {
                var a = p.Value["asset"]; if (a == null || a.Type != JTokenType.Array) continue;
                var ss = p.Value["ss"];
                var t = ReflexObjectShading.InstallTexture((string)a[0], (string)a[1], (string)a[2], (int)ss[0], (int)ss[1], (int)ss[4], (int)ss[5], (int)ss[6], Math.Max(1, (int)ss[9]));
                if (t is Cubemap) m.SetTexture("_Cube" + p.Name, t); else if (t) m.SetTexture("_Tex" + p.Name, t);
            }
            var rs = d["rs"];
            m.SetFloat("_Cull", (int)rs["CULL"] == 1 ? 0 : (int)rs["CULL"] == 2 ? 1 : 2);
            m.SetFloat("_ZWrite", (int)rs["ZWRITE"]);
            m.SetFloat("_AlphaTest", (int)rs["ALPHATEST"]); m.SetFloat("_AlphaRef", (int)rs["ALPHAREF"]); m.SetFloat("_AlphaFunc", (int)rs["ALPHAFUNC"]);
            if ((int)rs["BLEND"] != 0) { m.SetFloat("_SrcBlend", (float)BlendOf((int)rs["SRCBLEND"])); m.SetFloat("_DstBlend", (float)BlendOf((int)rs["DESTBLEND"])); }
            log.AppendLine($"draw {d["draw"]}: no game material found for {d["program"]} ({string.Join(", ", ((JObject)d["textures"]).Properties().Select(p => p.Name + "=" + p.Value["label"]))})");
            return m;
        }
        static BlendMode BlendOf(int d3d) => d3d switch { 1 => BlendMode.Zero, 2 => BlendMode.One, 5 => BlendMode.SrcAlpha, 6 => BlendMode.OneMinusSrcAlpha, 9 => BlendMode.DstColor, _ => BlendMode.One };

        /// A texture as the recorded device held it (render_capture rt/tex_<object>.raw: format, levels, then per level w, h, pitch, size,
        /// bytes): A8R8G8B8 / X8R8G8B8 (the run-time plates) as BGRA32, rows to Unity's order; the capture's sampler states.
        static Texture2D DeviceTexture(string path, JToken ss)
        {
            var b = File.ReadAllBytes(path); int fmt = BitConverter.ToInt32(b, 0), levels = BitConverter.ToInt32(b, 4), o = 8;
            if (fmt != 21 && fmt != 22 || levels < 1) return null;
            int w0 = BitConverter.ToInt32(b, o), h0 = BitConverter.ToInt32(b, o + 4);
            var t = new Texture2D(w0, h0, TextureFormat.BGRA32, levels, true) { name = Path.GetFileName(path) };
            for (int l = 0; l < levels; l++)
            {
                int w = BitConverter.ToInt32(b, o), h = BitConverter.ToInt32(b, o + 4), pitch = BitConverter.ToInt32(b, o + 8), n = BitConverter.ToInt32(b, o + 12); o += 16;
                var px = new byte[w * h * 4];
                for (int y = 0; y < h; y++) { Buffer.BlockCopy(b, o + y * pitch, px, (h - 1 - y) * w * 4, w * 4); if (fmt == 22) for (int x = 0; x < w; x++) px[((h - 1 - y) * w + x) * 4 + 3] = 255; }
                t.SetPixelData(px, l); o += n;
            }
            t.Apply(false, false);
            if (ss != null && ss.Type == JTokenType.Array)
            {
                TextureWrapMode W(int a) => a == 3 || a == 4 ? TextureWrapMode.Clamp : a == 2 ? TextureWrapMode.Mirror : TextureWrapMode.Repeat;
                t.wrapModeU = W((int)ss[0]); t.wrapModeV = W((int)ss[1]); t.filterMode = (int)ss[6] >= 2 ? FilterMode.Trilinear : FilterMode.Bilinear;
            }
            return t;
        }

        static Mesh LoadMesh2(string path)   // positions, normals, uv, colours (RGBA floats), triangles
        {
            var b = File.ReadAllBytes(path); int nv = BitConverter.ToInt32(b, 0), nt = BitConverter.ToInt32(b, 4), o = 8;
            Vector3[] V3() { var a = new Vector3[nv]; for (int i = 0; i < nv; i++, o += 12) a[i] = new Vector3(BitConverter.ToSingle(b, o), BitConverter.ToSingle(b, o + 4), BitConverter.ToSingle(b, o + 8)); return a; }
            var pos = V3(); var nrm = V3();
            var uv = new Vector2[nv]; for (int i = 0; i < nv; i++, o += 8) uv[i] = new Vector2(BitConverter.ToSingle(b, o), BitConverter.ToSingle(b, o + 4));
            var col = new Color[nv]; for (int i = 0; i < nv; i++, o += 16) col[i] = new Color(BitConverter.ToSingle(b, o), BitConverter.ToSingle(b, o + 4), BitConverter.ToSingle(b, o + 8), BitConverter.ToSingle(b, o + 12));
            var tri = new int[nt * 3]; for (int i = 0; i < tri.Length; i++, o += 4) tri[i] = BitConverter.ToInt32(b, o);
            var m = new Mesh { indexFormat = IndexFormat.UInt32 }; m.vertices = pos; m.normals = nrm; m.uv = uv; m.colors = col; m.triangles = tri; m.bounds = new Bounds(Vector3.zero, Vector3.one * 1e6f);
            return m;
        }

        static Mesh LoadMesh(string path)
        {
            var b = File.ReadAllBytes(path); int nv = BitConverter.ToInt32(b, 0), nt = BitConverter.ToInt32(b, 4), o = 8;
            Vector3[] V3() { var a = new Vector3[nv]; for (int i = 0; i < nv; i++, o += 12) a[i] = new Vector3(BitConverter.ToSingle(b, o), BitConverter.ToSingle(b, o + 4), BitConverter.ToSingle(b, o + 8)); return a; }
            Vector2[] V2() { var a = new Vector2[nv]; for (int i = 0; i < nv; i++, o += 8) a[i] = new Vector2(BitConverter.ToSingle(b, o), BitConverter.ToSingle(b, o + 4)); return a; }
            var pos = V3(); var nrm = V3(); var tan = V3(); var uv0 = V2(); var uv1 = V2();
            var tri = new int[nt * 3]; for (int i = 0; i < tri.Length; i++, o += 4) tri[i] = BitConverter.ToInt32(b, o);
            var m = new Mesh { indexFormat = IndexFormat.UInt32 };
            m.vertices = pos; m.normals = nrm; m.tangents = tan.Select(t => new Vector4(t.x, t.y, t.z, 1)).ToArray(); m.uv = uv0; m.uv2 = uv1;
            m.triangles = tri; m.bounds = new Bounds(Vector3.zero, Vector3.one * 1e6f);
            return m;
        }

        static float[] LoadRaw(string path, out int w, out int h, out int fmt)
        {
            var b = File.ReadAllBytes(path); fmt = BitConverter.ToInt32(b, 0); w = BitConverter.ToInt32(b, 4); h = BitConverter.ToInt32(b, 8); int pitch = BitConverter.ToInt32(b, 12);
            var o = new float[w * h * 4];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    int s = 16 + y * pitch, d = (y * w + x) * 4;
                    if (fmt == 113) for (int k = 0; k < 4; k++) o[d + k] = Mathf.HalfToFloat(BitConverter.ToUInt16(b, s + x * 8 + 2 * k));
                    else if (fmt == 116) for (int k = 0; k < 4; k++) o[d + k] = BitConverter.ToSingle(b, s + x * 16 + 4 * k);
                    else if (fmt == 34) { o[d] = BitConverter.ToUInt16(b, s + x * 4) / 65535f; o[d + 1] = BitConverter.ToUInt16(b, s + x * 4 + 2) / 65535f; }
                    else if (fmt == 114) o[d] = BitConverter.ToSingle(b, s + x * 4);
                }
            return o;
        }

        static Texture2D RawToTexture(float[] px)
        {
            var t = new Texture2D(W, H, TextureFormat.RGBAFloat, false, true) { filterMode = FilterMode.Point };
            var f = new float[px.Length];
            for (int y = 0; y < H; y++) Array.Copy(px, y * W * 4, f, (H - 1 - y) * W * 4, W * 4);
            t.SetPixelData(f, 0); t.Apply(false, false);
            return t;
        }

        static uint[] LoadIds(string path)
        {
            var b = File.ReadAllBytes(path); int w = BitConverter.ToInt32(b, 4), h = BitConverter.ToInt32(b, 8);
            var o = new uint[w * h]; Buffer.BlockCopy(b, 16, o, 0, o.Length * 4); return o;
        }
    }
}
