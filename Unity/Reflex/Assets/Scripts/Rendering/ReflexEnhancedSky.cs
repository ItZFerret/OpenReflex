// LIGHTING: ENHANCED (OPTIONS > VIDEO): the per-level HDRI skies and what is derived from them. Not part of the original renderer.
//
// Each outdoor level gets one of the CC0 Poly Haven skies in Assets/ThirdParty/Skies (LICENSES.md), chosen for the level's original time
// of day and weather (its sun elevation / colour, its fog, its original sky; dusk stays dusk, overcast stays overcast). Supercross
// stadiums, the night levels and the menus keep their original sky (no profile). For a level with a profile Build():
//   1. prepares the HDRI on the GPU (its sun clamped away: the level's sun glow / billboard stays the sun; mips for the low frequencies),
//   2. unwraps the ORIGINAL sky dome into a 128 x 64 lat-long image (its low frequencies steer the HDRI's colours, so each level keeps
//      its look; near the horizon the original sky image itself stays),
//   3. matches the HDRI's exposure and tint to the original sky (mean colour at 8..60 degrees of elevation),
//   4. probes the final sky by direction and derives the ambient spherical harmonics (normalised: a colour / direction ratio around 1,
//      the level's own light brightness stays) and a 64 px mip-mapped reflection cube (below the horizon: the level's own reflection map).
// The dome then draws Shaders/ReflexEnhancedSky.shader instead of Reflex/Original Sky while ENHANCED is on.
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Rendering
{
    public sealed class ReflexEnhancedSky : IDisposable
    {
        /// One downloaded sky: its file under Resources/ReflexSkies, where its sun stands (u of the equirect, measured), and the luminance
        /// its sun is clamped to (about six times its sky's mean between 5 and 60 degrees).
        public sealed class Hdri
        {
            public string file; public float sunU; public float clamp;
            public Hdri(string file, float sunU, float clamp) { this.file = file; this.sunU = sunU; this.clamp = clamp; }
        }

        public static readonly Dictionary<string, Hdri> Hdris = new Dictionary<string, Hdri>(StringComparer.Ordinal)
        {
            { "clear",         new Hdri("kloofendal_43d_clear_puresky_2k", 0.6003f, 3.6f) },
            { "partly_cloudy", new Hdri("kloofendal_48d_partly_cloudy_puresky_2k", 0.5949f, 3.0f) },
            { "afternoon",     new Hdri("qwantani_afternoon_puresky_2k", 0.6003f, 2.0f) },
            { "late_afternoon",new Hdri("qwantani_late_afternoon_puresky_2k", 0.6003f, 5.0f) },
            { "noon",          new Hdri("qwantani_noon_puresky_2k", 0.6003f, 1.6f) },
            { "golden",        new Hdri("kloppenheim_06_puresky_2k", 0.6130f, 8.0f) },
            { "cumulus",       new Hdri("sunflowers_puresky_2k", 0.6003f, 3.2f) },
            { "cloud_layer",   new Hdri("kloppenheim_03_puresky_2k", 0.6003f, 4f) },
            { "soft_overcast", new Hdri("farm_field_puresky_2k", 0.6033f, 9f) },
            { "overcast",      new Hdri("kloofendal_overcast_puresky_2k", 0.5818f, 9f) },
            { "misty",         new Hdri("kloofendal_28d_misty_puresky_2k", 0.4700f, 10f) },
        };

        /// How a level uses its sky and grades its picture (defaults: the HDRI's clouds and dynamic range, the original's colours).
        public sealed class Profile
        {
            public string sky;                 // key of Hdris; null: the original sky stays (stadiums, night, menus)
            public float keep = 0.65f;         // 0: the HDRI's own gradient (exposure / tint matched), 1: the original sky's low frequencies
            public float horizonLow = 0.015f, horizonHigh = 0.16f;   // sin(elevation) where the HDRI starts / is full (below: the original)
            public float exposure = 1f;        // on top of the automatic match
            public float glow = 0.6f;          // the level sun's glow in the sky
            public float skyAmbient = 0.4f;    // the sky's colour on the ambient term
            public float reflection = 0.35f;
            public float saturation = 1.06f, contrast = 1.05f, bloom = 1f;
            public float skyDetail = 0.7f;     // the HDRI's cloud contrast around its low frequencies (1: as photographed)
            public float skySaturation = 0.85f;
            public bool sceneryShadows = true; // the static scenery casts real-time shadows (off indoors / at night: the overhead "sun" is the stadium lights)
            public Vector3 gain = Vector3.one;  // white balance (warm < 1 on blue)
            public float postExposure = 1f;    // the post chain's exposure bias (1 for every stock level; MTX tracks, Content/ExperimentalMtxLighting.cs)
            public Profile With(Action<Profile> f) { var p = (Profile)MemberwiseClone(); f(p); return p; }
        }

        static Profile P(string sky) => new Profile { sky = sky };
        static readonly Profile NoSky = new Profile { sky = null, skyAmbient = 0.25f, sceneryShadows = false };

        /// Per level (database name). Chosen from the levels' AmbientDirectionalLight / DistanceFog / sky (sun elevation and colour, fog
        /// colours) and checked in the preview shots.
        public static readonly Dictionary<string, Profile> Levels = new Dictionary<string, Profile>(StringComparer.OrdinalIgnoreCase)
        {
            { "NAT_Concord", P("partly_cloudy") },
            { "NAT_ConcordB", P("afternoon") },
            { "NAT_Bedford", P("clear") },
            { "NAT_BedfordB", P("afternoon") },
            { "NAT_Renne", P("partly_cloudy") },
            { "NAT_RenneB", P("misty") },
            { "NAT_Stillwater", P("partly_cloudy") },
            { "NAT_StillwaterB", P("late_afternoon") },
            { "NAT_DLC05_Dalton", P("golden") },
            { "NAT_DLC06_DaltonB", P("overcast").With(p => { p.saturation = 1.0f; p.skySaturation = 0.35f; p.keep = 0.85f; p.glow = 0f; }) },
            { "FR_Alpine", P("cloud_layer").With(p => { p.keep = 0.85f; p.glow = 1.4f; p.skyDetail = 0.55f; p.skySaturation = 0.7f; }) },
            { "FR_Cheyenne", P("clear") },
            { "FR_Gibson", P("partly_cloudy") },
            { "PX_Laguna", P("clear") },
            { "PX_Laguna_BW", P("clear") },
            { "PX_LagunaB", P("misty") },
            { "PX_LagunaB_BW", P("misty") },
            { "TRX_Halifax", P("cumulus") },
            { "TRX_HalifaxBW", P("noon") },
            { "TRX_Lynchburg", P("afternoon") },
            { "TRX_LynchburgBW", P("noon") },
            { "TRX_Verde", P("clear") },
            { "TRX_VerdeBW", P("afternoon") },
            // FMX_Dover (dusk) keeps its painted sunset: its dome is a Template_Luma_Sky_Nighttime sky (Reflex/Original Sky Night)
            { "FMX_Dover", new Profile { sky = null } },
            { "FMX_DoverB", P("soft_overcast") },
            { "FMX_Springfield", P("clear") },
        };

        public static Profile For(string level)
        {
            var p = level != null && Levels.TryGetValue(level, out var found) ? found : NoSky;
            // an MTX track on Concord's infrastructure: its post exposure, its indoor arenas as stadiums (Content/ExperimentalMtxLighting.cs)
            return Content.ExperimentalTerrain.For(level) ? Content.ExperimentalMtxLighting.EnhancedProfile(p) : p;
        }

        // ------------------------------------------------------------------------------------------------- built per level
        public readonly Profile profile;
        public Material material;              // Reflex/Enhanced Sky for the dome
        public Cubemap cube;                   // reflections
        public Vector4[] sh = new Vector4[7];  // _RfxEnhSHAr .. _RfxEnhSHC
        public string report = "";
        RenderTexture prepared; Texture2D origLow;
        static readonly Dictionary<string, Texture2D> loaded = new Dictionary<string, Texture2D>();

        const int LW = 128, LH = 64, CubeSize = 64;

        ReflexEnhancedSky(Profile p) { profile = p; }

        public void Dispose()
        {
            if (material) UnityEngine.Object.Destroy(material);
            if (cube) UnityEngine.Object.Destroy(cube);
            if (prepared) { prepared.Release(); UnityEngine.Object.Destroy(prepared); }
            if (origLow) UnityEngine.Object.Destroy(origLow);
            foreach (var c in extra) if (c) UnityEngine.Object.Destroy(c);
            extra.Clear();
            material = null; cube = null; prepared = null; origLow = null;
        }

        /// The enhanced sky of a level for its original sky dome (renderer + material); null when the level keeps its original sky.
        /// origCube: the level's own reflection map (or null). sunDir: towards the sun. sunColor: the level's sun colour.
        /// The reflection cube again with another of the level's reflection maps below / near the horizon (the water's own map).
        public Cubemap ReflectionWith(Texture origCube)
        {
            if (!material || !origCube) return null;
            material.SetTexture(OrigCubeId, origCube); material.SetFloat(OrigCubeOnId, 1f);
            var probe = ReadLatLong(null, PassProbe, Vector4.zero, out _);
            material.SetTexture(OrigCubeId, firstCube); material.SetFloat(OrigCubeOnId, firstCube ? 1f : 0f);
            var c = MakeCube(probe); extra.Add(c);
            return c;
        }
        Texture firstCube; readonly List<Cubemap> extra = new List<Cubemap>();

        public static ReflexEnhancedSky Build(string level, Renderer dome, Material original, Texture origCube, Vector3 sunDir, Vector3 sunColor)
        {
            var p = For(level);
            if (p.sky == null || !dome || !original || !Hdris.TryGetValue(p.sky, out var info)) return null;
            var shader = Shader.Find("Reflex/Enhanced Sky");
            if (!shader) { Debug.LogWarning("ReflexEnhancedSky: shader Reflex/Enhanced Sky not found (Always Included Shaders)"); return null; }
            if (!loaded.TryGetValue(info.file, out var hdri) || !hdri)
            {
                hdri = Resources.Load<Texture2D>("ReflexSkies/" + info.file);
                if (!hdri) { Debug.LogWarning("ReflexEnhancedSky: sky not found: Resources/ReflexSkies/" + info.file); return null; }
                loaded[info.file] = hdri;
            }
            var s = new ReflexEnhancedSky(p);
            try { s.Make(shader, hdri, info, dome, original, origCube, sunDir, sunColor); }
            catch (Exception e) { Debug.LogWarning("ReflexEnhancedSky " + level + ": " + e); s.Dispose(); return null; }
            return s;
        }

        static readonly int HdriId = Shader.PropertyToID("_SkyHdri"), OrigLowId = Shader.PropertyToID("_SkyOrigLow"), RotId = Shader.PropertyToID("_SkyRot"),
            GradeId = Shader.PropertyToID("_SkyGrade"), HorizonId = Shader.PropertyToID("_SkyHorizon"), GlowId = Shader.PropertyToID("_SkyGlow"),
            PrepId = Shader.PropertyToID("_SkyPrep"), UnwrapSizeId = Shader.PropertyToID("_RfxUnwrapSize"), ForceRawId = Shader.PropertyToID("_RfxEnhForceRaw"),
            OrigCubeId = Shader.PropertyToID("_SkyOrigCube"), OrigCubeOnId = Shader.PropertyToID("_SkyOrigCubeOn"), DetailId = Shader.PropertyToID("_SkyDetail"),
            SkySunId = Shader.PropertyToID("_SkySunDir");
        const int PassUnwrap = 1, PassPrepare = 2, PassProbe = 3;

        void Make(Shader shader, Texture2D hdri, Hdri info, Renderer dome, Material original, Texture origCube, Vector3 sunDir, Vector3 sunColor)
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            material = new Material(shader) { name = original.name + " (enhanced)", hideFlags = HideFlags.DontSave };
            material.CopyPropertiesFromMaterial(original);
            if (original.IsKeywordEnabled("_SKY_ENTITY_COMP")) material.EnableKeyword("_SKY_ENTITY_COMP");

            // 1. the HDRI, sun clamped, with mips
            prepared = new RenderTexture(hdri.width, hdri.height, 0, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear)
                { name = info.file + " (prepared)", useMipMap = true, autoGenerateMips = false, wrapModeU = TextureWrapMode.Repeat, wrapModeV = TextureWrapMode.Clamp,
                  filterMode = FilterMode.Trilinear, hideFlags = HideFlags.DontSave };
            prepared.Create();
            material.SetTexture(HdriId, hdri);
            material.SetVector(PrepId, new Vector4(info.clamp, 0, 0, 0));
            FullScreen(prepared, PassPrepare);
            material.SetTexture(HdriId, prepared);

            // 2. the original dome unwrapped (raw values)
            var orig = Unwrap(dome, out int covered);
            origLow = new Texture2D(LW, LH, TextureFormat.RGBAHalf, false, true) { name = "original sky (lat-long)", wrapModeU = TextureWrapMode.Repeat, wrapModeV = TextureWrapMode.Clamp,
                filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            origLow.SetPixels(orig); origLow.Apply(false, false);
            material.SetTexture(OrigLowId, origLow);

            // 3. exposure / tint: the HDRI's mean (a low mip, already clamped) against the original's, 8..60 degrees
            var low = ReadLatLong(prepared, PassPrepare, new Vector4(1e9f, Mathf.Log(prepared.width / (float)LW, 2f), 0, 0), out _);
            Vector3 mo = BandMean(orig, 8, 60), mh = BandMean(low, 8, 60);
            float lo = Lum(mo), lh = Mathf.Max(Lum(mh), 1e-5f);
            float exposure = lo > 1e-5f ? lo / lh : 1f;
            Vector3 tint = lo > 1e-5f ? new Vector3(mo.x / lo / Mathf.Max(mh.x / lh, 1e-4f), mo.y / lo / Mathf.Max(mh.y / lh, 1e-4f), mo.z / lo / Mathf.Max(mh.z / lh, 1e-4f)) : Vector3.one;
            tint = new Vector3(Mathf.Clamp(tint.x, 0.6f, 1.6f), Mathf.Clamp(tint.y, 0.6f, 1.6f), Mathf.Clamp(tint.z, 0.6f, 1.6f));
            tint = Vector3.Lerp(Vector3.one, tint, 0.6f);
            exposure *= profile.exposure;
            // the sky's sun: the original sky's painted sun when it has one (a texel far brighter than the sky around it; on Alpine it
            // stands lower than the light's direction), else the level's light direction
            var painted = PaintedSun(orig, lo, sunDir);
            var skySun = painted ?? sunDir;
            material.SetVector(SkySunId, new Vector4(skySun.x, skySun.y, skySun.z, 0));
            float phi = Mathf.Atan2(skySun.z, skySun.x) / (2f * Mathf.PI);
            material.SetVector(RotId, new Vector4(Frac(info.sunU - 0.5f - phi), 0, 0, 0));
            material.SetVector(GradeId, new Vector4(exposure * tint.x, exposure * tint.y, exposure * tint.z, profile.keep));
            material.SetVector(HorizonId, new Vector4(profile.horizonLow, profile.horizonHigh, 1f, Mathf.Log(prepared.width / 32f, 2f)));
            material.SetVector(DetailId, new Vector4(profile.skyDetail, profile.skySaturation, 0, 0));
            // the level's sun glow: its colour at a brightness relative to the sky (a sun billboard / light glow of the level stays on top)
            Vector3 g = sunColor * (lo * 0.6f * profile.glow);
            material.SetVector(GlowId, new Vector4(g.x, g.y, g.z, 180f));
            material.SetFloat("_SkyZTest", original.GetFloat("_SkyZTest"));
            material.SetFloat("_Cull", original.GetFloat("_Cull"));

            // 4. the final sky by direction -> SH and the reflection cube
            firstCube = origCube;
            material.SetTexture(OrigCubeId, origCube ? origCube : null);
            material.SetFloat(OrigCubeOnId, origCube ? 1f : 0f);
            var probe = ReadLatLong(null, PassProbe, Vector4.zero, out _);
            if (!origCube) FillGround(probe, mo);
            ProjectSH(probe);
            cube = MakeCube(probe);
            string dbg = Environment.GetEnvironmentVariable("REFLEX_ENH_SKY_DEBUG");   // diagnostics: the lat-long images as PNG
            if (!string.IsNullOrEmpty(dbg))
            {
                DumpPng(orig, System.IO.Path.Combine(dbg, info.file + "_original_latlong.png"), lo);
                DumpPng(low, System.IO.Path.Combine(dbg, info.file + "_hdri_latlong.png"), lh);
                DumpPng(probe, System.IO.Path.Combine(dbg, info.file + "_probe_latlong.png"), lo);
            }
            report = $"sky {info.file}: sun {(painted != null ? "painted in the original sky" : "the light's")} {skySun}, exposure {exposure:0.###} tint ({tint.x:0.##}, {tint.y:0.##}, {tint.z:0.##}) original sky {mo.x:0.###},{mo.y:0.###},{mo.z:0.###}"
                     + $" ({covered} of {LW * LH} texels covered) hdri {mh.x:0.###},{mh.y:0.###},{mh.z:0.###}; reflections below the horizon {(origCube ? "the level's map" : "estimated ground")}; {sw.ElapsedMilliseconds} ms";
        }

        static void DumpPng(Color[] px, string path, float mean)
        {
            var t = new Texture2D(LW, LH, TextureFormat.RGBA32, false);
            var c = new Color[px.Length]; float k = 0.5f / Mathf.Max(mean, 1e-4f);
            for (int i = 0; i < px.Length; i++) { var v = px[i] * k; c[i] = new Color(v.r / (1 + v.r), v.g / (1 + v.g), v.b / (1 + v.b), 1); }
            t.SetPixels(c); t.Apply();
            try { System.IO.File.WriteAllBytes(path, t.EncodeToPNG()); } catch (Exception e) { Debug.LogWarning(e.Message); }
            UnityEngine.Object.Destroy(t);
        }

        static float Frac(float x) => x - Mathf.Floor(x);

        // the brightest 3 x 3 neighbourhood of the original sky above 3 degrees, if it is at least 1.5 times the sky's mean and near the light's azimuth
        static Vector3? PaintedSun(Color[] px, float mean, Vector3 light)
        {
            float best = 0; int bx = 0, by = 0;
            for (int y = 1; y < LH - 1; y++)
            {
                if (((y + 0.5f) / LH - 0.5f) * 180f < 3f) continue;
                for (int x = 0; x < LW; x++)
                {
                    float s = 0;
                    for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) { var c = px[(y + j) * LW + (x + i + LW) % LW]; s += 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }
                    if (s > best) { best = s; bx = x; by = y; }
                }
            }
            if (mean <= 0 || best / 9f < 1.5f * mean) return null;
            var d = LatLongDir((bx + 0.5f) / LW, (by + 0.5f) / LH);
            // a bright cloud is not the sun: the painted sun stands within 40 degrees of the light's azimuth
            float da = Mathf.Abs(Mathf.DeltaAngle(Mathf.Atan2(d.z, d.x) * Mathf.Rad2Deg, Mathf.Atan2(light.z, light.x) * Mathf.Rad2Deg));
            return da < 40f ? d : (Vector3?)null;
        }
        static float Lum(Vector3 c) => 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;

        // the original sky dome rendered into a lat-long target, read back, oriented (alpha = 2 + dir.y) and its holes filled
        Color[] Unwrap(Renderer dome, out int covered)
        {
            var rt = RenderTexture.GetTemporary(new RenderTextureDescriptor(LW, LH, RenderTextureFormat.ARGBFloat, 0) { sRGB = false });
            var cmd = new CommandBuffer { name = "Reflex enhanced sky: unwrap" };
            cmd.SetRenderTarget(rt);
            cmd.ClearRenderTarget(false, true, Color.clear);
            cmd.SetGlobalVector(UnwrapSizeId, new Vector4(LW, LH, 0, 0));
            for (int sub = 0; sub < Mathf.Max(1, dome.sharedMaterials.Length); sub++) cmd.DrawRenderer(dome, material, sub, PassUnwrap);
            Graphics.ExecuteCommandBuffer(cmd);
            cmd.Release();
            var px = Read(rt);
            RenderTexture.ReleaseTemporary(rt);
            Orient(px);
            covered = 0;
            for (int i = 0; i < px.Length; i++) if (px[i].a > 0.5f) covered++;
            // fill: each row's holes from its covered texels (seam), then empty rows from the nearest covered row (below the dome)
            var rowHas = new bool[LH];
            for (int y = 0; y < LH; y++)
            {
                int n = 0; Color sum = Color.clear;
                for (int x = 0; x < LW; x++) { var c = px[y * LW + x]; if (c.a > 0.5f) { n++; sum += c; } }
                rowHas[y] = n > LW / 8;
                if (!rowHas[y]) continue;
                for (int x = 0; x < LW; x++)
                {
                    if (px[y * LW + x].a > 0.5f) continue;
                    Color a = Color.clear, b = Color.clear; int da = 0, db = 0;
                    for (int k = 1; k < LW && (a.a <= 0.5f); k++) { a = px[y * LW + (x - k + LW) % LW]; da = k; }
                    for (int k = 1; k < LW && (b.a <= 0.5f); k++) { b = px[y * LW + (x + k) % LW]; db = k; }
                    px[y * LW + x] = a.a > 0.5f && b.a > 0.5f ? Color.Lerp(a, b, da / (float)(da + db)) : sum / n;
                }
            }
            for (int y = 0; y < LH; y++)
            {
                if (rowHas[y]) continue;
                int best = -1;
                for (int k = 1; k < LH && best < 0; k++)
                {
                    if (y + k < LH && rowHas[y + k]) best = y + k;
                    else if (y - k >= 0 && rowHas[y - k]) best = y - k;
                }
                for (int x = 0; x < LW; x++) px[y * LW + x] = best >= 0 ? px[best * LW + x] : new Color(0.5f, 0.6f, 0.75f, 1f);
            }
            for (int i = 0; i < px.Length; i++) px[i].a = 1f;
            return px;
        }

        // a full-screen pass of the sky material into a lat-long target, read back (rows oriented bottom = -90 degrees)
        Color[] ReadLatLong(Texture source, int pass, Vector4 prep, out bool flipped)
        {
            var rt = RenderTexture.GetTemporary(new RenderTextureDescriptor(LW, LH, RenderTextureFormat.ARGBFloat, 0) { sRGB = false });
            if (pass == PassPrepare)
            {
                material.SetVector(PrepId, prep);
                material.SetTexture(HdriId, source);
                FullScreen(rt, PassPrepare);
                material.SetTexture(HdriId, prepared);
            }
            else
            {
                Shader.SetGlobalFloat(ForceRawId, 1f);
                FullScreen(rt, pass);
                Shader.SetGlobalFloat(ForceRawId, 0f);
            }
            var px = Read(rt);
            RenderTexture.ReleaseTemporary(rt);
            flipped = pass != PassPrepare && Orient(px);
            return px;
        }

        // one full-screen triangle of a pass of the sky material (its vertex program builds the triangle from SV_VertexID)
        void FullScreen(RenderTexture dst, int pass)
        {
            var cmd = new CommandBuffer { name = "Reflex enhanced sky" };
            cmd.SetRenderTarget(dst);
            cmd.DrawProcedural(Matrix4x4.identity, material, pass, MeshTopology.Triangles, 3, 1);
            Graphics.ExecuteCommandBuffer(cmd);
            cmd.Release();
            if (dst.useMipMap && !dst.autoGenerateMips) dst.GenerateMips();
        }

        static Color[] Read(RenderTexture rt)
        {
            var prev = RenderTexture.active;
            RenderTexture.active = rt;
            var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBAFloat, false, true);
            t.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0, false);
            t.Apply(false);
            RenderTexture.active = prev;
            var px = t.GetPixels();
            UnityEngine.Object.Destroy(t);
            return px;
        }

        // the alpha holds 2 + dir.y: flip the rows when the top row reads lower than the bottom row
        static bool Orient(Color[] px)
        {
            float top = 0, bottom = 0; int nt = 0, nb = 0;
            for (int y = 0; y < LH / 4; y++)
                for (int x = 0; x < LW; x++)
                {
                    var a = px[y * LW + x].a; if (a > 0.5f) { bottom += a; nb++; }
                    var b = px[(LH - 1 - y) * LW + x].a; if (b > 0.5f) { top += b; nt++; }
                }
            bool flip = nt > 0 && nb > 0 ? top / nt < bottom / nb : nb > nt;
            if (!flip) return false;
            for (int y = 0; y < LH / 2; y++)
                for (int x = 0; x < LW; x++) { var t = px[y * LW + x]; px[y * LW + x] = px[(LH - 1 - y) * LW + x]; px[(LH - 1 - y) * LW + x] = t; }
            return true;
        }

        static Vector3 BandMean(Color[] px, float lo, float hi)
        {
            Vector3 s = Vector3.zero; float w = 0;
            for (int y = 0; y < LH; y++)
            {
                float el = ((y + 0.5f) / LH - 0.5f) * 180f;
                if (el < lo || el > hi) continue;
                float k = Mathf.Cos(el * Mathf.Deg2Rad);
                for (int x = 0; x < LW; x++) { var c = px[y * LW + x]; s += new Vector3(c.r, c.g, c.b) * k; w += k; }
            }
            return w > 0 ? s / w : Vector3.zero;
        }

        // without the level's reflection map: an estimated ground below the horizon (a fifth of the horizon sky)
        static void FillGround(Color[] px, Vector3 horizon)
        {
            for (int y = 0; y < LH; y++)
            {
                float dy = Mathf.Sin(((y + 0.5f) / LH - 0.5f) * Mathf.PI);
                if (dy >= 0.05f) continue;
                float t = Mathf.Clamp01((0.05f - dy) / 0.15f);
                var g = new Color(horizon.x * 0.22f, horizon.y * 0.2f, horizon.z * 0.17f, 1);
                for (int x = 0; x < LW; x++) px[y * LW + x] = Color.Lerp(px[y * LW + x], g, t);
            }
        }

        static Vector3 LatLongDir(float u, float v)
        {
            float az = (u - 0.5f) * 2f * Mathf.PI, el = (v - 0.5f) * Mathf.PI;
            return new Vector3(Mathf.Cos(el) * Mathf.Cos(az), Mathf.Sin(el), Mathf.Cos(el) * Mathf.Sin(az));
        }

        // order-2 SH of the irradiance, normalised so that its mean luminance over the sphere is 1, packed as Unity's SHAr .. SHC
        void ProjectSH(Color[] px)
        {
            var c = new Vector3[9];
            float dA = (2f * Mathf.PI / LW) * (Mathf.PI / LH);
            for (int y = 0; y < LH; y++)
            {
                float el = ((y + 0.5f) / LH - 0.5f) * Mathf.PI, w = Mathf.Cos(el) * dA;
                for (int x = 0; x < LW; x++)
                {
                    var d = LatLongDir((x + 0.5f) / LW, (y + 0.5f) / LH);
                    var col = px[y * LW + x]; var L = new Vector3(col.r, col.g, col.b) * w;
                    c[0] += L * 0.282095f;
                    c[1] += L * (0.488603f * d.x); c[2] += L * (0.488603f * d.y); c[3] += L * (0.488603f * d.z);
                    c[4] += L * (1.092548f * d.x * d.y); c[5] += L * (1.092548f * d.y * d.z); c[6] += L * (0.315392f * (3f * d.z * d.z - 1f));
                    c[7] += L * (1.092548f * d.x * d.z); c[8] += L * (0.546274f * (d.x * d.x - d.y * d.y));
                }
            }
            float a0 = Mathf.PI, a1 = 2f * Mathf.PI / 3f, a2 = Mathf.PI / 4f;
            c[0] *= a0; for (int i = 1; i < 4; i++) c[i] *= a1; for (int i = 4; i < 9; i++) c[i] *= a2;
            // each channel divided by its own mean irradiance over the sphere (the constant term E0 Y00): the ratio keeps the sky's
            // direction and its colour differences between directions, but no colour cast of the whole ambient (the level's mood stays)
            for (int ch = 0; ch < 3; ch++)
            {
                float mean = Mathf.Max(c[0][ch] * 0.282095f, 1e-6f);
                for (int i = 0; i < 9; i++) c[i][ch] /= mean;
            }
            Vector4 V(float x, float y, float z, float w) => new Vector4(x, y, z, w);
            for (int ch = 0; ch < 3; ch++)
            {
                float E(int i) => c[i][ch];
                sh[ch] = V(0.488603f * E(1), 0.488603f * E(2), 0.488603f * E(3), 0.282095f * E(0) - 0.315392f * E(6));
                sh[3 + ch] = V(1.092548f * E(4), 1.092548f * E(5), 3f * 0.315392f * E(6), 1.092548f * E(7));
            }
            sh[6] = V(0.546274f * c[8].x, 0.546274f * c[8].y, 0.546274f * c[8].z, 1);
        }

        // the lat-long as a cube (Unity's face convention: +X (-z, -y), -X (z, -y), +Y (x, z), -Y (x, -z), +Z (x, -y), -Z (-x, -y); rows top-down)
        static Cubemap MakeCube(Color[] px)
        {
            var cube = new Cubemap(CubeSize, TextureFormat.RGBAHalf, true) { name = "Reflex enhanced sky (reflections)", filterMode = FilterMode.Trilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            var face = new Color[CubeSize * CubeSize];
            for (int f = 0; f < 6; f++)
            {
                for (int y = 0; y < CubeSize; y++)
                    for (int x = 0; x < CubeSize; x++)
                    {
                        float u = (x + 0.5f) / CubeSize * 2f - 1f, v = (y + 0.5f) / CubeSize * 2f - 1f;
                        Vector3 d;
                        switch ((CubemapFace)f)
                        {
                            case CubemapFace.PositiveX: d = new Vector3(1, -v, -u); break;
                            case CubemapFace.NegativeX: d = new Vector3(-1, -v, u); break;
                            case CubemapFace.PositiveY: d = new Vector3(u, 1, v); break;
                            case CubemapFace.NegativeY: d = new Vector3(u, -1, -v); break;
                            case CubemapFace.PositiveZ: d = new Vector3(u, -v, 1); break;
                            default: d = new Vector3(-u, -v, -1); break;
                        }
                        face[y * CubeSize + x] = SampleLatLong(px, d.normalized);
                    }
                cube.SetPixels(face, (CubemapFace)f, 0);
            }
            cube.Apply(true, false);
            return cube;
        }

        static Color SampleLatLong(Color[] px, Vector3 d)
        {
            float u = 0.5f + Mathf.Atan2(d.z, d.x) / (2f * Mathf.PI), v = 0.5f + Mathf.Asin(Mathf.Clamp(d.y, -1f, 1f)) / Mathf.PI;
            float fx = u * LW - 0.5f, fy = Mathf.Clamp(v * LH - 0.5f, 0, LH - 1);
            int x0 = Mathf.FloorToInt(fx), y0 = Mathf.FloorToInt(fy); float tx = fx - x0, ty = fy - y0;
            int y1 = Mathf.Min(y0 + 1, LH - 1);
            Color P(int x, int y) => px[y * LW + ((x % LW) + LW) % LW];
            return Color.Lerp(Color.Lerp(P(x0, y0), P(x0 + 1, y0), tx), Color.Lerp(P(x0, y1), P(x0 + 1, y1), tx), ty);
        }
    }
}
