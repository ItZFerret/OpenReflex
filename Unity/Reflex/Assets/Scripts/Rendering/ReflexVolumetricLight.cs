// LIGHTING: ENHANCED > VOLUMETRIC LIGHT (OPTIONS > VIDEO; default ON). Not part of the original renderer: ORIGINAL never runs any of this.
//
// The original game fakes its light shafts with geometry: the supercross stadiums' roof-light cones (SX_KI / SX_MAN_B / SX_FD / SX_SP /
// SX_DLC_FY_LightBeam and the FreightYard's SX_DLC_FY_Light_Grad dust sheets: the scenery shader's mode 12, a scrolling dust texture
// x 5 faded by a view-angle gradient and a beam mask), the god-ray / light-ray cards (godraytest, FF_Lightrays, NAT_DA_GodRay_NoTemp,
// NAT_BE_FogPlane god rays: Shaders/ReflexLightGlow.shader programs 1 and 2) and the light trailers' beam cards (Template_LightGlow
// named *Beam*). With VOLUMETRIC LIGHT on in ENHANCED those renderers are hidden (Renderer.forceRenderingOff, given back when it is off)
// and the light is scattered by a participating medium instead (Shaders/Resources/ReflexVolumetric.shader):
//   - the sun, shadowed by ENHANCED's cascaded sun shadow map (scenery, trees, grandstands, the bikes) and the level's own volume
//     shadow map (hills): real shafts through trees, roof openings and past the riders; outdoor levels only (in the stadiums the
//     "sun" is the overhead lighting rig)
//   - one spot light per fake cone / card, placed and aimed where it was: its geometry's principal axis (the narrow end is the lamp),
//     its opening angle from the end radii, its colour from the beam's dust texture (or the card's vertex colours), its brightness
//     set so the middle of the beam seen from the side is about as bright as the original card was; it follows the renderer's
//     transform and is on while the renderer would draw (LOD, scenario state)
//   - the medium: height fog over the ground under the camera (density from the level's own DistanceFog: end amount / end distance,
//     its colour as the ambient term), a thin ground-dust layer over the terrain with slow billows (thicker in the stadiums), FOG DENSITY
//     LOW / MEDIUM / HIGH on top
// Ray marched at half (LOW: quarter) resolution with blue-noise start offsets, accumulated over frames with reprojection and a
// neighbourhood clamp, upsampled by depth and composited onto the HDR scene before the transparent objects, so the ENHANCED post
// chain (adaptation, bloom, tone mapping) treats it like the rest of the picture. The transparent objects (blended scenery / vehicle
// materials, particles, glows, grass, video screens) take the medium in front of them in their own shaders (Shaders/ReflexVolumetricApply.hlsl:
// the march's result at the pixel, published as _RfxVolResult, cut back to the fragment's distance), so they sit in the haze instead of
// drawing over it (w-mudlight). Quality follows ENHANCED QUALITY unless
// VOLUMETRIC QUALITY is set: LOW 1/4 res 10 steps, MEDIUM 1/2 res 12 steps, HIGH 1/2 res 18 steps (+ 3 / 4 / 6 samples per visible spot light).
// Test build: F10 switches between the volumetric light and the original's fake shafts (an on-screen label says which).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.InputSystem;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Rendering
{
    [DisallowMultipleComponent]
    public sealed class ReflexVolumetricLight : MonoBehaviour
    {
        // ------------------------------------------------------------------------------------------------ options
        /// VOLUMETRIC LIGHT (OPTIONS > VIDEO, ENHANCED only): 1 ON (default), 0 OFF (the original's fake shafts).
        public static int Option { get => Mathf.Clamp(ReflexSave.GetInt("Options.Volumetric", 1), 0, 1); set { ReflexSave.SetInt("Options.Volumetric", Mathf.Clamp(value, 0, 1)); abForce = -1; } }
        /// VOLUMETRIC QUALITY: -1 AUTO (ENHANCED QUALITY's tier), 0 LOW, 1 MEDIUM, 2 HIGH.
        public static int QualityOption { get => Mathf.Clamp(ReflexSave.GetInt("Options.VolumetricQuality", -1), -1, 2); set => ReflexSave.SetInt("Options.VolumetricQuality", Mathf.Clamp(value, -1, 2)); }
        /// FOG DENSITY: 0 LOW, 1 MEDIUM (default), 2 HIGH.
        public static int DensityOption { get => Mathf.Clamp(ReflexSave.GetInt("Options.VolumetricDensity", 1), 0, 2); set => ReflexSave.SetInt("Options.VolumetricDensity", Mathf.Clamp(value, 0, 2)); }
        /// Tests: forces it (0 off, 1 on; -1 the option). REFLEX_VOLUMETRIC=0|1 at start.
        public static int Override = -1;
        /// Tests: 1 shows only the volumetric light (the scene black), 2 only its transmittance.
        public static int DebugView;
        static int abForce = -1;   // F10 (this session only)
        public static bool Requested => Override >= 0 ? Override == 1 : abForce >= 0 ? abForce == 1 : Option == 1;
        public static int Tier => TierOverride >= 0 ? TierOverride : QualityOption >= 0 ? QualityOption : ReflexEnhancedLighting.Tier;
        /// Tests: the quality tier / FOG DENSITY instead of the options (-1: the options).
        public static int TierOverride = -1, DensityOverride = -1;
        static int Density => DensityOverride >= 0 ? Mathf.Clamp(DensityOverride, 0, 2) : DensityOption;
        static readonly float[] DensityScale = { 0.5f, 1f, 1.8f };
        static float DensityScaleTest = 1f;   // tests: REFLEX_VOL_DENSITY_SCALE

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ReadEnvironment()
        {
            string v = Environment.GetEnvironmentVariable("REFLEX_VOLUMETRIC");
            if (v == "0") Override = 0; else if (v == "1") Override = 1;
            if (float.TryParse(Environment.GetEnvironmentVariable("REFLEX_VOL_DENSITY_SCALE"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float ds) && ds > 0f) DensityScaleTest = ds;
        }

        /// The game camera starts rendering in ENHANCED (ReflexRenderPipeline.OnBegin): the volumetric pass, when it is on.
        internal static void BeginCamera(ReflexRenderPipeline pipeline, Camera cam, ScriptableRenderer renderer)
        {
            Shader.SetGlobalVector(ApplyId, Vector4.zero);   // (on again from the pass, when it composites this frame)
            if (!pipeline || !cam) return;
            var v = cam.GetComponent<ReflexVolumetricLight>();
            if (!v) v = cam.gameObject.AddComponent<ReflexVolumetricLight>();
            if (!v.Prepare(cam)) return;
            v.pass.ConfigureInput(ScriptableRenderPassInput.Depth);
            renderer.EnqueuePass(v.pass);
        }

        /// The game camera finished rendering (ReflexRenderPipeline.OnEnd): other cameras' transparent objects see no medium.
        internal static void EndCamera() { Shader.SetGlobalVector(ApplyId, Vector4.zero); }

        /// The transparent objects' inputs (Shaders/ReflexVolumetricApply.hlsl): _RfxVolApply x on, y full pixels per low pixel, zw 1 / low size;
        /// _RfxVolApply2 x height falloff (1/m), y the march's max distance, zw full size; _RfxVolResult the temporal pass's result.
        internal static readonly int ApplyId = Shader.PropertyToID("_RfxVolApply"), Apply2Id = Shader.PropertyToID("_RfxVolApply2"),
            ResultId = Shader.PropertyToID("_RfxVolResult");

        // ------------------------------------------------------------------------------------------------ state
        [Header("Result (read only)")]
        public bool active;
        public int fakesHidden, spotLights, visibleSpots;
        [TextArea(3, 10)] public string report;

        ReflexRenderPipeline pipeline; ReflexEnhancedLighting enhanced;
        OriginalLevel level; ReflexLevelContent content; string levelName;
        ReflexVolumetricPass pass;
        Profile profile; int profileDensity = -1;
        int seenMaterials = -1; float nextScan; int scans;
        bool labelShown; float labelUntil;
        float groundY = float.NaN;
        Texture2D ground; Vector4 groundRect; bool groundTried;

        sealed class Fake { public Renderer renderer; public bool hidden; }
        readonly Dictionary<Renderer, Fake> fakes = new Dictionary<Renderer, Fake>();

        /// One light that replaces a fake shaft: in the renderer's local space (its model's own coordinates).
        sealed class Shaft
        {
            public Renderer renderer; public string name;
            public Vector3 narrow, dir; public float length, r0, r1;   // the narrow end's centre, towards the wide end, radii
            public Color colour; public float brightness;
        }
        readonly List<Shaft> shafts = new List<Shaft>();
        readonly Dictionary<string, List<(Vector3 narrow, Vector3 dir, float length, float r0, float r1, Color vcol)>> shapeCache =
            new Dictionary<string, List<(Vector3, Vector3, float, float, float, Color)>>();

        /// The level's medium (ReflexVolumetricLight.ProfileFor).
        sealed class Profile
        {
            public bool stadium;
            public float sigma, heightFalloff, sunScale, sunGain, ambientScale, dust, dustHeight, g, forward, spotBoost, spotGain;
            public Color ambient; public Vector3 sun;
            public string Describe() => $"{(stadium ? "stadium" : "outdoor")}: sigma {sigma * 1000f:0.00}/km, H {1f / Mathf.Max(heightFalloff, 1e-5f):0} m, sun x{sunScale:0.00}, " +
                                        $"ambient x{ambientScale:0.00} {ambient}, dust {dust * 1000f:0.00}/km over {dustHeight:0.0} m, g {g:0.00}";
        }

        void OnEnable()
        {
            pipeline = GetComponent<ReflexRenderPipeline>();
            enhanced = GetComponent<ReflexEnhancedLighting>();
            pass = new ReflexVolumetricPass(this);
        }

        void OnDisable() { RestoreFakes(); active = false; }

        void OnDestroy()
        {
            RestoreFakes();
            pass?.Dispose(); pass = null;
            if (ground) Destroy(ground);
            ground = null;
        }

        // ------------------------------------------------------------------------------------------------ per frame
        void LateUpdate()
        {
            if (!pipeline) pipeline = GetComponent<ReflexRenderPipeline>();
            if (!enhanced) enhanced = GetComponent<ReflexEnhancedLighting>();
            var lvl = enhanced ? enhanced.level : null;
            bool enh = pipeline && pipeline.enhanced != null && lvl && lvl.Loaded;
            if (lvl != level || (lvl && lvl.Content != content)) ResetLevel(lvl);

            // F10 (test build): the volumetric light / the original's fake shafts
            var kb = Keyboard.current;
            if (enh && kb != null && kb.ctrlKey.isPressed && kb.f10Key.wasPressedThisFrame && !OriginalInput.IsCameraKey(Key.F10))
            {
                abForce = Requested ? 0 : 1;
                labelShown = true; labelUntil = Time.unscaledTime + 4f;
                Debug.Log("[Reflex volumetric] CTRL+F10: " + (Requested ? "VOLUMETRIC LIGHT" : "ORIGINAL FAKE SHAFTS"));
            }

            bool want = enh && Requested;
            if (want)
            {
                if (profile == null || profileDensity != Density) { profile = ProfileFor(level); profileDensity = Density; }
                if (content.SceneryMaterials.Count != seenMaterials || Time.unscaledTime >= nextScan) Scan();
                foreach (var f in fakes.Values)
                    if (f.renderer && !f.hidden) { f.renderer.forceRenderingOff = true; f.hidden = true; }
                fakesHidden = fakes.Count;
            }
            else RestoreFakes();
            if (want != active) Debug.Log("[Reflex volumetric] " + (want ? "on: " + report : "off"));
            active = want;
            if (Timing) MeasurePasses();
        }

        // tests (REFLEX_VOL_TIMING=1): the GPU time of each volumetric pass, averaged over 120 frames, in the log
        static readonly bool Timing = Environment.GetEnvironmentVariable("REFLEX_VOL_TIMING") == "1";
        static readonly string[] TimedPasses = { "Reflex volumetric march", "Reflex volumetric temporal", "Reflex volumetric composite", "Reflex occlusion" };
        readonly double[] passSum = new double[4]; int passFrames;
        void MeasurePasses()
        {
            for (int i = 0; i < TimedPasses.Length; i++)
            {
                var smp = ReflexPassSamplers.Get(TimedPasses[i]);
                smp.enableRecording = true;
                passSum[i] += smp.gpuElapsedTime;
            }
            if (++passFrames < 120) return;
            Debug.Log($"[Reflex volumetric] GPU ms (tier {Tier}, {(active ? "on" : "off")}): march {passSum[0] / passFrames:0.000} temporal {passSum[1] / passFrames:0.000} composite {passSum[2] / passFrames:0.000} (occlusion {passSum[3] / passFrames:0.000}) spots {visibleSpots}");
            Array.Clear(passSum, 0, passSum.Length); passFrames = 0;
        }

        void ResetLevel(OriginalLevel lvl)
        {
            RestoreFakes();
            fakes.Clear(); shafts.Clear(); shapeCache.Clear();
            level = lvl; content = lvl ? lvl.Content : null; levelName = lvl ? lvl.level : null;
            profile = null; seenMaterials = -1; nextScan = 0; scans = 0; groundY = float.NaN;
            if (ground) Destroy(ground);
            ground = null; groundTried = false;
            pass?.ResetHistory();
        }

        void RestoreFakes()
        {
            foreach (var f in fakes.Values)
                if (f.hidden) { if (f.renderer) f.renderer.forceRenderingOff = false; f.hidden = false; }
            fakesHidden = 0;
        }

        void OnGUI()
        {
            if (!labelShown || pipeline == null || pipeline.enhanced == null) return;
            bool on = Requested;
            string text = on ? "VOLUMETRIC LIGHT  (CTRL+F10: original shafts)" : "ORIGINAL FAKE SHAFTS  (CTRL+F10: volumetric light)";
            float a = Time.unscaledTime < labelUntil ? 1f : 0.55f;
            var style = new GUIStyle(GUI.skin.label) { fontSize = Mathf.Max(14, Screen.height / 54), fontStyle = FontStyle.Bold };
            var size = style.CalcSize(new GUIContent(text));
            var r = new Rect(Screen.width - size.x - 24, Screen.height - size.y - 24, size.x + 12, size.y + 6);
            GUI.color = new Color(0, 0, 0, 0.55f * a); GUI.DrawTexture(r, Texture2D.whiteTexture);
            style.normal.textColor = on ? new Color(1f, 0.82f, 0.45f, a) : new Color(0.75f, 0.85f, 1f, a);
            GUI.color = Color.white;
            GUI.Label(new Rect(r.x + 6, r.y + 3, size.x, size.y), text, style);
        }

        // ------------------------------------------------------------------------------------------------ the level
        static Profile ProfileFor(OriginalLevel lvl)
        {
            string name = Reflex.Port.Content.TrackMods.Style(lvl ? lvl.level ?? "" : "");   // (a custom Supercross track: a stadium too)
            var l = lvl && lvl.Content != null ? lvl.Content.lighting : null;
            bool stadium = name.StartsWith("SX_", StringComparison.OrdinalIgnoreCase) || name.IndexOf("FunFactory", StringComparison.OrdinalIgnoreCase) >= 0;
            float density = DensityScale[Density] * DensityScaleTest;
            // the level's distance fog as an extinction: the end amount reached at the end distance
            float fogSigma = 0.0008f;
            Color fogColour = new Color(0.55f, 0.6f, 0.68f);
            if (l != null && l.fogDistanceEnd > 1f)
            {
                fogSigma = -Mathf.Log(1f - Mathf.Clamp(l.fogAmountEnd, 0.02f, 0.95f)) / Mathf.Max(l.fogDistanceEnd, 50f);
                fogColour = Color.Lerp(l.fogColorStart, l.fogColorEnd, 0.5f);
            }
            var p = new Profile { stadium = stadium };
            p.sigma = Mathf.Clamp(fogSigma * 2.5f, 0.0012f, stadium ? 0.0025f : 0.006f) * density;
            p.heightFalloff = 1f / (stadium ? 30f : 45f);
            p.sun = l != null ? l.sunColor : Vector3.one;
            p.sunScale = stadium ? 0f : 1f;
            p.sunGain = 8f;    // the sun's in-scattering over the physical value: visible shafts in light haze without thickening the haze itself
            p.ambient = fogColour; p.ambientScale = stadium ? 0.06f : 0.03f;
            p.dust = (stadium ? 2.5f : 0.8f) * p.sigma; p.dustHeight = stadium ? 3.5f : 2.5f;
            p.g = 0.7f; p.forward = 0.85f;
            p.spotBoost = 1f; p.spotGain = 0.5f;
            return p;
        }

        // the ground under the camera (the height fog's base) and a 128 x 128 height texture of the terrain (the dust layer)
        void UpdateGround(Vector3 camPos)
        {
            var t = level && level.terrain ? level.terrain.UnityTerrain : null;
            if (!groundTried && t && t.terrainData)
            {
                groundTried = true;
                const int N = 128;
                var size = t.terrainData.size; var origin = t.GetPosition();
                var h = new float[N * N];
                for (int z = 0; z < N; z++)
                    for (int x = 0; x < N; x++)
                    {
                        var wp = new Vector3(origin.x + size.x * (x + 0.5f) / N, 0, origin.z + size.z * (z + 0.5f) / N);
                        h[z * N + x] = t.SampleHeight(wp) + origin.y;
                    }
                ground = new Texture2D(N, N, TextureFormat.RFloat, false, true) { name = "Reflex volumetric ground", filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
                ground.SetPixelData(h, 0); ground.Apply(false, true);
                // texel centres at (x + 0.5) / N: uv = (p - origin) / size
                groundRect = new Vector4(origin.x, origin.z, 1f / Mathf.Max(size.x, 1f), 1f / Mathf.Max(size.z, 1f));
            }
            float gy = camPos.y - 3f;
            if (t && t.terrainData)
            {
                var o = t.GetPosition(); var s = t.terrainData.size;
                if (camPos.x >= o.x && camPos.z >= o.z && camPos.x <= o.x + s.x && camPos.z <= o.z + s.z) gy = t.SampleHeight(camPos) + o.y;
            }
            groundY = float.IsNaN(groundY) || Mathf.Abs(gy - groundY) > 60f ? gy : Mathf.Lerp(groundY, gy, 0.05f);
        }

        // ------------------------------------------------------------------------------------------------ fakes
        static readonly int ModeId = Shader.PropertyToID("_Mode"), ProgramId = Shader.PropertyToID("_Program");

        static bool Named(string s, params string[] keys)
        {
            if (string.IsNullOrEmpty(s)) return false;
            foreach (var k in keys) if (s.IndexOf(k, StringComparison.OrdinalIgnoreCase) >= 0) return true;
            return false;
        }

        /// Finds the level's fake shafts (renderers drawing one of their materials) and the lights that replace them.
        void Scan()
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            scans++;
            seenMaterials = content.SceneryMaterials.Count;
            nextScan = Time.unscaledTime + (scans < 4 ? 2f : 10f);   // ambient models and LOD clones appear after the load
            var scenery = Shader.Find("Reflex/Original Scenery");
            var glow = Shader.Find(ReflexLightGlow.ShaderName);
            var names = new Dictionary<Material, string>();
            foreach (var kv in content.SceneryMaterials) if (kv.Value) names[kv.Value] = kv.Key;
            int added = 0;
            foreach (var r in level.GetComponentsInChildren<MeshRenderer>(true))
            {
                if (fakes.ContainsKey(r)) continue;
                var mats = r.sharedMaterials;
                List<int> subs = null;
                for (int s = 0; s < mats.Length; s++)
                {
                    var m = mats[s];
                    if (!m) continue;
                    bool fake = false;
                    if (m.shader == scenery && m.HasProperty(ModeId) && Mathf.Abs(m.GetFloat(ModeId) - ReflexSceneryMaterial.ModeLightBeam) < 0.5f) fake = true;
                    else if (m.shader == glow && m.HasProperty(ProgramId))
                    {
                        int prog = Mathf.RoundToInt(m.GetFloat(ProgramId));
                        string label = (names.TryGetValue(m, out var n) ? n : m.name) + " " + r.name;
                        if (prog == 1) fake = true;                                                   // godraytest, FF_Lightrays
                        else if (prog == 2 && Named(label, "godray", "god_ray", "lightray", "rays")) fake = true;   // NAT_DA_GodRay_NoTemp, NAT_BE_B_GodRays
                        else if (prog == 0 && Named(label, "beam")) fake = true;                       // the spot-light trailers' beams
                    }
                    if (!fake) continue;
                    (subs ??= new List<int>()).Add(s);
                }
                if (subs == null) continue;
                // outdoors the sun's shafts replace the god-ray cards; in the stadiums (and for the roof-light cones everywhere) a spot light each
                var mf = r.GetComponent<MeshFilter>();
                foreach (int s in subs)
                {
                    var m = mats[s];
                    bool cone = m.shader == scenery || Mathf.RoundToInt(m.GetFloat(ProgramId)) == 0;
                    if (!profile.stadium && !cone) continue;
                    AddShafts(r, mf ? mf.sharedMesh : null, s, m, names.TryGetValue(m, out var mn) ? mn : null);
                }
                fakes[r] = new Fake { renderer = r };
                added++;
            }
            spotLights = shafts.Count;
            report = $"{levelName}: {fakes.Count} fake shaft renderers hidden, {shafts.Count} spot lights; {profile.Describe()}; scan {sw.Elapsed.TotalMilliseconds:0.0} ms";
            if (added > 0)
            {
                Debug.Log("[Reflex volumetric] " + report);
                foreach (var sh in shafts.Skip(shafts.Count - Math.Min(shafts.Count, 40)))
                {
                    var w = sh.renderer.localToWorldMatrix;
                    Debug.Log($"[Reflex volumetric]   spot {sh.name}: narrow {w.MultiplyPoint3x4(sh.narrow):F1} dir {w.MultiplyVector(sh.dir).normalized:F2} length {sh.length:0.0} r {sh.r0:0.00}..{sh.r1:0.00} colour {sh.colour} brightness {sh.brightness:0.00}");
                }
            }
        }

        void AddShafts(Renderer r, Mesh mesh, int submesh, Material m, string materialName)
        {
            if (!mesh) return;
            string key = mesh.name;
            if (!content.sceneModels.TryGetValue(key, out var md))
            {
                int cut = key.IndexOf(" (", StringComparison.Ordinal);
                if (cut < 0 || !content.sceneModels.TryGetValue(key.Substring(0, cut), out md)) { Debug.Log($"[Reflex volumetric] {r.name}: model {key} not found (no light)"); return; }
            }
            if (submesh >= md.triangles.Count) return;
            string ck = md.name + "#" + submesh;
            if (!shapeCache.TryGetValue(ck, out var shapes)) shapeCache[ck] = shapes = Analyse(md, submesh);
            // colour and brightness: the beam's dust texture (mode 12: rgb = dust x 5) or the card's vertex colours
            Color tint = Color.white; float bright = 0.35f;
            if (materialName != null && content.sceneMaterials.TryGetValue(materialName, out var lm) && lm.textures.Count > 0 && m.shader.name == "Reflex/Original Scenery")
            {
                var avg = AverageColour(lm.textures[0]);
                if (avg.HasValue) { var c = avg.Value; float mx = Mathf.Max(c.r, Mathf.Max(c.g, c.b), 1e-3f); tint = new Color(c.r / mx, c.g / mx, c.b / mx); bright = Mathf.Clamp(Lum(c) * 5f * 0.22f, 0.12f, 1.2f); }
            }
            foreach (var sh in shapes)
            {
                Color c = tint; float b = bright;
                if (m.shader.name != "Reflex/Original Scenery")
                {
                    var v = sh.vcol; float mx = Mathf.Max(v.r, Mathf.Max(v.g, v.b), 1e-3f);
                    c = new Color(v.r / mx, v.g / mx, v.b / mx); b = Mathf.Clamp(mx * Mathf.Max(v.a, 0.25f) * 0.6f, 0.1f, 1f);
                }
                shafts.Add(new Shaft { renderer = r, name = (materialName ?? m.name) + " on " + r.name, narrow = sh.narrow, dir = sh.dir, length = sh.length, r0 = sh.r0, r1 = sh.r1, colour = c, brightness = b });
            }
        }

        static float Lum(Color c) => 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;

        // the average colour of a level texture from its smallest mip of at least 4 x 4 (raw values: the original samples without sRGB)
        Color? AverageColour(string texture)
        {
            if (!content.sceneTextures.TryGetValue(texture, out var lt) || lt.mips.Count == 0 || lt.cube) return null;
            int level = 0, w = lt.width, h = lt.height;
            while (level + 1 < lt.mips.Count && Mathf.Max(1, w >> 1) >= 4 && Mathf.Max(1, h >> 1) >= 4) { level++; w = Mathf.Max(1, w >> 1); h = Mathf.Max(1, h >> 1); }
            var b = lt.mips[level];
            double r = 0, g = 0, bl = 0; int n = 0;
            void Add(float cr, float cg, float cb) { r += cr; g += cg; bl += cb; n++; }
            switch (lt.format)
            {
                case TextureFormat.DXT1: case TextureFormat.DXT5:
                {
                    int stride = lt.format == TextureFormat.DXT1 ? 8 : 16, off = lt.format == TextureFormat.DXT1 ? 0 : 8;
                    for (int i = 0; i + stride <= b.Length; i += stride)
                    {
                        int c0 = b[i + off] | b[i + off + 1] << 8, c1 = b[i + off + 2] | b[i + off + 3] << 8;
                        Add(((c0 >> 11) & 31) / 31f * 0.5f + ((c1 >> 11) & 31) / 31f * 0.5f, ((c0 >> 5) & 63) / 63f * 0.5f + ((c1 >> 5) & 63) / 63f * 0.5f, (c0 & 31) / 31f * 0.5f + (c1 & 31) / 31f * 0.5f);
                    }
                    break;
                }
                case TextureFormat.RGBA32: for (int i = 0; i + 4 <= b.Length; i += 4) Add(b[i] / 255f, b[i + 1] / 255f, b[i + 2] / 255f); break;
                case TextureFormat.ARGB32: for (int i = 0; i + 4 <= b.Length; i += 4) Add(b[i + 1] / 255f, b[i + 2] / 255f, b[i + 3] / 255f); break;
                case TextureFormat.BGRA32: for (int i = 0; i + 4 <= b.Length; i += 4) Add(b[i + 2] / 255f, b[i + 1] / 255f, b[i] / 255f); break;
                case TextureFormat.RGB24: for (int i = 0; i + 3 <= b.Length; i += 3) Add(b[i] / 255f, b[i + 1] / 255f, b[i + 2] / 255f); break;
                case TextureFormat.Alpha8: case TextureFormat.R8: for (int i = 0; i < b.Length; i++) Add(b[i] / 255f, b[i] / 255f, b[i] / 255f); break;
                default: return null;
            }
            return n == 0 ? (Color?)null : new Color((float)(r / n), (float)(g / n), (float)(bl / n));
        }

        // The cones / cards of one submesh: connected pieces (welded by position), each one's principal axis (the longest extent, or for a
        // wide cone the axis its radius changes along), the narrow end (the lamp) and the end radii.
        static List<(Vector3, Vector3, float, float, float, Color)> Analyse(ReflexLevelContent.ModelData md, int submesh)
        {
            var list = new List<(Vector3, Vector3, float, float, float, Color)>();
            var tris = md.triangles[submesh]; var pos = md.positions;
            var parent = new Dictionary<int, int>();
            int Find(int x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; }
            void Union(int a, int b) { a = Find(a); b = Find(b); if (a != b) parent[a] = b; }
            var weld = new Dictionary<Vector3Int, int>();
            int Key(int v)
            {
                var p = pos[v]; var q = new Vector3Int(Mathf.RoundToInt(p.x * 200f), Mathf.RoundToInt(p.y * 200f), Mathf.RoundToInt(p.z * 200f));
                if (!weld.TryGetValue(q, out int k)) { weld[q] = k = v; parent[k] = k; }
                return k;
            }
            for (int i = 0; i + 2 < tris.Length; i += 3) { int a = Key(tris[i]), b = Key(tris[i + 1]), c = Key(tris[i + 2]); Union(a, b); Union(b, c); }
            var groups = new Dictionary<int, List<int>>();
            var vcolSum = new Dictionary<int, Vector4>();
            var seen = new HashSet<int>();
            for (int i = 0; i < tris.Length; i++)
            {
                int v = tris[i];
                if (!seen.Add(v)) continue;
                int g = Find(Key(v));
                if (!groups.TryGetValue(g, out var lst)) groups[g] = lst = new List<int>();
                lst.Add(v);
                var c = md.colors != null && v < md.colors.Length ? (Color)md.colors[v] : Color.white;
                vcolSum[g] = (vcolSum.TryGetValue(g, out var cs) ? cs : Vector4.zero) + new Vector4(c.r, c.g, c.b, c.a);
            }
            foreach (var kv in groups)
            {
                var vs = kv.Value;
                if (vs.Count < 3) continue;
                Vector3 mean = Vector3.zero; foreach (int v in vs) mean += pos[v]; mean /= vs.Count;
                // covariance, eigenvectors (Jacobi)
                double xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
                foreach (int v in vs) { var d = pos[v] - mean; xx += d.x * d.x; xy += d.x * d.y; xz += d.x * d.z; yy += d.y * d.y; yz += d.y * d.z; zz += d.z * d.z; }
                var axes = Eigen(new double[,] { { xx, xy, xz }, { xy, yy, yz }, { xz, yz, zz } }, out var values);
                // candidates: the major and the minor axis; the one along which the radius changes most steadily is the beam's
                Vector3 best = axes[0]; double bestScore = -1;
                foreach (int ai in new[] { 0, 2 })
                {
                    var ax = axes[ai];
                    double st = 0, sr = 0, stt = 0, srr = 0, str = 0; int n = vs.Count;
                    foreach (int v in vs)
                    {
                        var d = pos[v] - mean; float t = Vector3.Dot(d, ax); float rr = (d - ax * t).magnitude;
                        st += t; sr += rr; stt += t * t; srr += rr * rr; str += t * rr;
                    }
                    double cov = str / n - st / n * sr / n, vt = stt / n - st / n * st / n, vr = srr / n - sr / n * sr / n;
                    double corr = vt > 1e-9 && vr > 1e-9 ? Math.Abs(cov / Math.Sqrt(vt * vr)) : 0;
                    double score = corr + (ai == 0 ? 0.15 : 0);   // a plain card (no radius trend): its long axis
                    if (score > bestScore) { bestScore = score; best = ax; }
                }
                float tmin = float.MaxValue, tmax = float.MinValue;
                foreach (int v in vs) { float t = Vector3.Dot(pos[v] - mean, best); tmin = Mathf.Min(tmin, t); tmax = Mathf.Max(tmax, t); }
                float len = tmax - tmin; if (len < 0.05f) continue;
                float rLo = 0, rHi = 0;
                foreach (int v in vs)
                {
                    var d = pos[v] - mean; float t = Vector3.Dot(d, best); float rr = (d - best * t).magnitude;
                    if (t < tmin + 0.2f * len) rLo = Mathf.Max(rLo, rr);
                    if (t > tmax - 0.2f * len) rHi = Mathf.Max(rHi, rr);
                }
                if (len < 0.8f * Mathf.Max(rLo, rHi)) continue;   // a lens cap / disc of the fixture, not a beam
                bool flip = rHi < rLo;
                Vector3 dir = flip ? -best : best;
                Vector3 narrow = mean + best * (flip ? tmax : tmin);
                var sum = vcolSum[kv.Key] / vs.Count;
                list.Add((narrow, dir, len, Mathf.Min(rLo, rHi), Mathf.Max(rLo, rHi), new Color(sum.x, sum.y, sum.z, sum.w)));
            }
            return list;
        }

        // eigenvectors of a symmetric 3 x 3 matrix, by decreasing eigenvalue (cyclic Jacobi)
        static Vector3[] Eigen(double[,] a, out double[] values)
        {
            var v = new double[,] { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
            for (int sweep = 0; sweep < 32; sweep++)
            {
                double off = Math.Abs(a[0, 1]) + Math.Abs(a[0, 2]) + Math.Abs(a[1, 2]);
                if (off < 1e-12) break;
                for (int p = 0; p < 2; p++)
                    for (int q = p + 1; q < 3; q++)
                    {
                        if (Math.Abs(a[p, q]) < 1e-15) continue;
                        double th = 0.5 * Math.Atan2(2 * a[p, q], a[q, q] - a[p, p]);
                        double c = Math.Cos(th), s = Math.Sin(th);
                        for (int k = 0; k < 3; k++) { double akp = a[k, p], akq = a[k, q]; a[k, p] = c * akp - s * akq; a[k, q] = s * akp + c * akq; }
                        for (int k = 0; k < 3; k++) { double apk = a[p, k], aqk = a[q, k]; a[p, k] = c * apk - s * aqk; a[q, k] = s * apk + c * aqk; }
                        for (int k = 0; k < 3; k++) { double vkp = v[k, p], vkq = v[k, q]; v[k, p] = c * vkp - s * vkq; v[k, q] = s * vkp + c * vkq; }
                    }
            }
            var idx = new[] { 0, 1, 2 }.OrderByDescending(i => a[i, i]).ToArray();
            values = idx.Select(i => a[i, i]).ToArray();
            return idx.Select(i => new Vector3((float)v[0, i], (float)v[1, i], (float)v[2, i]).normalized).ToArray();
        }

        // ------------------------------------------------------------------------------------------------ the frame's parameters
        internal sealed class Frame
        {
            public int scale, steps, spotSamples;
            public Vector4 medium, medium2, dust, groundRect, sun, ambient, shadow, spotCount;
            public Texture groundTex;
            public readonly Vector4[] spotPos = new Vector4[MaxSpots], spotDir = new Vector4[MaxSpots], spotCol = new Vector4[MaxSpots];
            public int debug;
        }
        internal const int MaxSpots = 24;
        internal readonly Frame frame = new Frame();
        static readonly Plane[] planes = new Plane[6];
        readonly List<(float d, int i)> order = new List<(float, int)>();

        /// Fills the frame's parameters (main thread, before the camera renders); false: nothing to draw.
        bool Prepare(Camera cam)
        {
            if (!active || profile == null || content == null || pass == null) return false;
            int tier = Tier;
            var f = frame;
            f.scale = tier == 0 ? 4 : 2; f.steps = tier == 0 ? 10 : tier == 1 ? 12 : 18; f.spotSamples = tier == 0 ? 3 : tier == 1 ? 4 : 6;
            f.debug = DebugView;
            var p = profile;
            UpdateGround(cam.transform.position);
            float shadowDistance = GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp ? urp.shadowDistance : 150f;
            int cascades = GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset u2 ? u2.shadowCascadeCount : 1;
            float maxDist = p.stadium ? 220f : Mathf.Clamp(shadowDistance, 60f, 140f);
            f.medium = new Vector4(p.sigma * ReflexWeather.MediumScale, p.heightFalloff, groundY, maxDist);   // (WEATHER: rain and snow thicken the air)
            f.medium2 = new Vector4(0.6f, 0.55f, p.g, p.forward);
            f.dust = new Vector4(p.dust, 1f / Mathf.Max(p.dustHeight, 0.1f), Time.time * 0.02f, ground ? 1f : 0f);
            f.groundRect = groundRect; f.groundTex = ground ? (Texture)ground : Texture2D.blackTexture;
            var sun = RenderSettings.sun;
            bool sunShadows = sun && sun.shadows != LightShadows.None && shadowDistance > 0f;
            float sg = p.sunScale * p.sunGain * ReflexWeather.SunScale;   // (WEATHER: the clouds dim the sun)
            f.sun = new Vector4(p.sun.x * sg, p.sun.y * sg, p.sun.z * sg, sg > 0f ? 1f : 0f);
            f.ambient = new Vector4(p.ambient.r * p.ambientScale, p.ambient.g * p.ambientScale, p.ambient.b * p.ambientScale, 0f);
            f.shadow = new Vector4(sunShadows ? 1f : 0f, Mathf.Max(1, cascades), Shader.GetGlobalVector("_RfxVolumeShadowParams").y > 0.5f ? 1f : 0f, shadowDistance);

            // the spot lights in view (nearest first)
            GeometryUtility.CalculateFrustumPlanes(cam, planes);
            order.Clear();
            var camPos = cam.transform.position;
            float phase0 = HG(0.35f, 0f);
            float sigmaRef = Mathf.Max(p.sigma * p.spotBoost, 1e-5f);
            for (int i = 0; i < shafts.Count; i++)
            {
                var s = shafts[i];
                var r = s.renderer;
                if (!r || !r.enabled || !r.gameObject.activeInHierarchy) continue;
                var w = r.localToWorldMatrix;
                Vector3 a = w.MultiplyPoint3x4(s.narrow), b = w.MultiplyPoint3x4(s.narrow + s.dir * s.length);
                float rad = s.r1 * w.lossyScale.x + 1f;
                var bounds = new Bounds((a + b) * 0.5f, Vector3.zero); bounds.Encapsulate(a); bounds.Encapsulate(b); bounds.Expand(rad * 2f);
                if (!GeometryUtility.TestPlanesAABB(planes, bounds)) continue;
                order.Add((bounds.SqrDistance(camPos), i));
            }
            order.Sort((x, y) => x.d.CompareTo(y.d));
            int n = Mathf.Min(order.Count, MaxSpots);
            for (int k = 0; k < n; k++)
            {
                var s = shafts[order[k].i];
                var w = s.renderer.localToWorldMatrix;
                Vector3 narrow = w.MultiplyPoint3x4(s.narrow);
                Vector3 dirW = w.MultiplyVector(s.dir); float scaleW = dirW.magnitude; dirW /= Mathf.Max(scaleW, 1e-6f);
                float len = s.length * scaleW, r1 = s.r1 * scaleW;
                // the lamp: a small source at the card's narrow end (the cards are wide fans; a real lamp is a few metres across), the
                // cone opening to the card's far width; the light carries on past the card's end (to the floor) and fades by distance
                float r0 = Mathf.Min(s.r0 * scaleW, Mathf.Max(1f, 0.02f * len));
                float tan = Mathf.Clamp((r1 - r0) / Mathf.Max(len, 1e-3f), Mathf.Tan(2f * Mathf.Deg2Rad), Mathf.Tan(60f * Mathf.Deg2Rad));
                float back = r0 / tan;                         // the virtual apex behind the lamp's face
                Vector3 apex = narrow - dirW * back;
                float range = back + len * 1.6f;
                float cosOut = Mathf.Cos(Mathf.Atan(tan)), cosIn = Mathf.Cos(Mathf.Atan(tan) * 0.35f);
                // brightness: seen from the side, half way along, about the original card's
                float sMid = back + len * 0.5f;
                float intensity = s.brightness * p.spotGain * sMid / (sigmaRef * phase0 * 2f * tan * 0.55f);
                f.spotPos[k] = new Vector4(apex.x, apex.y, apex.z, range);
                f.spotDir[k] = new Vector4(dirW.x, dirW.y, dirW.z, cosOut);
                f.spotCol[k] = new Vector4(s.colour.r * intensity, s.colour.g * intensity, s.colour.b * intensity, cosIn);
            }
            for (int k = n; k < MaxSpots; k++) { f.spotPos[k] = Vector4.zero; f.spotDir[k] = new Vector4(0, -1, 0, 1); f.spotCol[k] = Vector4.zero; }
            visibleSpots = n;
            f.spotCount = new Vector4(n, p.spotBoost, 0.35f, 0.3f);   // w: the ground dust lit by the spots at 30 % (the beams end in a soft glow on the floor, not a hot spot)
            return true;
        }

        static float HG(float g, float c) { float g2 = g * g; return (1f - g2) / (4f * Mathf.PI * Mathf.Pow(Mathf.Max(1f + g2 - 2f * g * c, 1e-4f), 1.5f)); }
    }

    /// The volumetric light's render-graph passes (march, temporal, composite) for the game camera, before the transparent objects.
    sealed class ReflexVolumetricPass : ScriptableRenderPass, IDisposable
    {
        readonly ReflexVolumetricLight owner;
        Material material;
        RTHandle histA, histB; bool flip, historyValid;
        Matrix4x4 prevVP; Vector3 prevCamPos; Quaternion prevCamRot; int frameIndex, lastFrame = -1;
        readonly List<MaterialPropertyBlock> blocks = new List<MaterialPropertyBlock>();
        int blockIndex;
        const int PassMarch = 0, PassTemporal = 1, PassComposite = 2;
        static readonly int DepthId = Shader.PropertyToID("_RfxVolDepth"), SrcId = Shader.PropertyToID("_RfxVolSrc"), HistoryId = Shader.PropertyToID("_RfxVolHistory"),
            NoiseId = Shader.PropertyToID("_RfxVolNoise"), GroundId = Shader.PropertyToID("_RfxVolGround"), FullId = Shader.PropertyToID("_RfxVolFull"),
            LowId = Shader.PropertyToID("_RfxVolLow"), ParamsId = Shader.PropertyToID("_RfxVolParams"), ZId = Shader.PropertyToID("_RfxVolZParams"),
            TlId = Shader.PropertyToID("_RfxVolTopLeft"), XeId = Shader.PropertyToID("_RfxVolXExtent"), YeId = Shader.PropertyToID("_RfxVolYExtent"),
            CamId = Shader.PropertyToID("_RfxVolCam"), PrevVPId = Shader.PropertyToID("_RfxVolPrevVP"), MediumId = Shader.PropertyToID("_RfxVolMedium"),
            Medium2Id = Shader.PropertyToID("_RfxVolMedium2"), DustId = Shader.PropertyToID("_RfxVolDust"), GroundRectId = Shader.PropertyToID("_RfxVolGroundRect"),
            SunId = Shader.PropertyToID("_RfxVolSun"), AmbientId = Shader.PropertyToID("_RfxVolAmbient"), ShadowId = Shader.PropertyToID("_RfxVolShadow"),
            TemporalId = Shader.PropertyToID("_RfxVolTemporal"), SpotPosId = Shader.PropertyToID("_RfxVolSpotPos"), SpotDirId = Shader.PropertyToID("_RfxVolSpotDir"),
            SpotColId = Shader.PropertyToID("_RfxVolSpotCol"), SpotCountId = Shader.PropertyToID("_RfxVolSpotCount"),
            ShadowMapId = Shader.PropertyToID("_MainLightShadowmapTexture"), DebugId = Shader.PropertyToID("_RfxVolDebug");

        /// Tests (cost of each pass): REFLEX_VOL_PASSES bit 1 march, 2 temporal, 4 composite (default all).
        static readonly int PassMask = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_VOL_PASSES"), out int m) ? m : 7;

        public ReflexVolumetricPass(ReflexVolumetricLight owner)
        {
            this.owner = owner;
            renderPassEvent = RenderPassEvent.BeforeRenderingTransparents;
            profilingSampler = new ProfilingSampler("Reflex volumetric light");
        }

        public void Dispose()
        {
            histA?.Release(); histB?.Release(); histA = histB = null;
            if (material) CoreUtils.Destroy(material);
            material = null;
        }

        public void ResetHistory() { historyValid = false; }

        class PassData
        {
            internal Material material; internal int pass; internal MaterialPropertyBlock mpb;
            internal TextureHandle depth, src, history, shadow; internal Texture noise, ground;
            internal Vector4 full, low, prm, z, tl, xe, ye, cam, medium, medium2, dust, groundRect, sun, ambient, shadowPrm, temporal, spotCount; internal float debug;
            internal Matrix4x4 prevVP; internal Vector4[] spotPos, spotDir, spotCol;
        }

        public override void RecordRenderGraph(RenderGraph rg, ContextContainer frameData)
        {
            var f = owner.frame;
            if (!material)
            {
                var shader = Resources.Load<Shader>("ReflexVolumetric");
                if (!shader || !shader.isSupported) { Debug.LogError("ReflexVolumetricPass: Resources/ReflexVolumetric (Hidden/Reflex/Volumetric) not found or not supported"); return; }
                material = CoreUtils.CreateEngineMaterial(shader);
            }
            var res = frameData.Get<UniversalResourceData>();
            var camData = frameData.Get<UniversalCameraData>();
            var depth = res.cameraDepthTexture;
            if (!depth.IsValid() || res.isActiveTargetBackBuffer) return;
            var color = res.activeColorTexture;
            var cd = rg.GetTextureDesc(color);
            int W = cd.width, H = cd.height, s = Mathf.Max(1, f.scale);
            int w = (W + s - 1) / s, h = (H + s - 1) / s;
            blockIndex = 0;

            if (histA == null || histA.rt == null || histA.rt.width != w || histA.rt.height != h)
            {
                histA?.Release(); histB?.Release();
                histA = RTHandles.Alloc(w, h, GraphicsFormat.R16G16B16A16_SFloat, filterMode: FilterMode.Bilinear, wrapMode: TextureWrapMode.Clamp, name: "Reflex volumetric history A");
                histB = RTHandles.Alloc(w, h, GraphicsFormat.R16G16B16A16_SFloat, filterMode: FilterMode.Bilinear, wrapMode: TextureWrapMode.Clamp, name: "Reflex volumetric history B");
                historyValid = false;
            }

            // the camera (as the occlusion pass: rays through the near-plane corners in world orientation; GL projection, uv.y 0 = bottom)
            var cam = camData.camera;
            Matrix4x4 view = camData.GetViewMatrix(), proj = camData.GetProjectionMatrix();
            var camView = view; camView.SetColumn(3, new Vector4(0, 0, 0, 1));
            var inv = (proj * camView).inverse;
            Vector3 tl = inv.MultiplyPoint(new Vector3(-1, 1, -1)), tr = inv.MultiplyPoint(new Vector3(1, 1, -1)), bl = inv.MultiplyPoint(new Vector3(-1, -1, -1));
            float n = cam.nearClipPlane, fa = cam.farClipPlane;
            Vector4 zp = SystemInfo.usesReversedZBuffer
                ? new Vector4(-1f + fa / n, 1f, (-1f + fa / n) / fa, 1f / fa)
                : new Vector4(1f - fa / n, fa / n, (1f - fa / n) / fa, fa / n / fa);
            var vp = proj * view;
            Vector3 camPos = cam.transform.position; var camRot = cam.transform.rotation;
            // a cut (replay camera switch, respawn): no history
            bool cut = (camPos - prevCamPos).sqrMagnitude > 25f || Quaternion.Angle(camRot, prevCamRot) > 35f || Time.frameCount - lastFrame > 4;
            float temporal = historyValid && !cut ? 0.9f : 0f;
            if (Time.frameCount != lastFrame) frameIndex++;
            lastFrame = Time.frameCount;

            var common = new PassData
            {
                full = new Vector4(W, H, 1f / W, 1f / H), low = new Vector4(w, h, 1f / w, 1f / h), prm = new Vector4(s, f.steps, f.spotSamples, frameIndex & 1023),
                z = zp, tl = tl, xe = tr - tl, ye = bl - tl, cam = new Vector4(camPos.x, camPos.y, camPos.z, n),
                medium = f.medium, medium2 = f.medium2, dust = f.dust, groundRect = f.groundRect, sun = f.sun, ambient = f.ambient, shadowPrm = f.shadow,
                temporal = new Vector4(temporal, 1.25f, 0, 0), spotCount = f.spotCount, prevVP = prevVP,
                spotPos = f.spotPos, spotDir = f.spotDir, spotCol = f.spotCol, noise = ReflexBlueNoise.Texture, ground = f.groundTex, debug = f.debug,
            };
            var shadowTex = res.mainShadowsTexture;
            if (!shadowTex.IsValid()) common.shadowPrm.x = 0f;

            var march = rg.CreateTexture(new TextureDesc(w, h) { colorFormat = GraphicsFormat.R16G16B16A16_SFloat, name = "Reflex volumetric march", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp });
            flip = !flip;
            var prevH = rg.ImportTexture(flip ? histB : histA);
            var nextH = rg.ImportTexture(flip ? histA : histB);
            if ((PassMask & 1) != 0) Add(rg, "Reflex volumetric march", PassMarch, march, common, depth, default, default, shadowTex.IsValid() ? shadowTex : default, AccessFlags.Write);
            if ((PassMask & 2) != 0) Add(rg, "Reflex volumetric temporal", PassTemporal, nextH, common, depth, march, prevH, default, AccessFlags.Write);
            if ((PassMask & 4) != 0) Add(rg, "Reflex volumetric composite", PassComposite, color, common, depth, nextH, default, default, AccessFlags.ReadWrite);
            // the transparent objects that draw after the composite take the medium in front of them from the same result
            bool apply = (PassMask & 6) == 6 && f.debug == 0;
            Shader.SetGlobalVector(ReflexVolumetricLight.ApplyId, apply ? new Vector4(1f, s, 1f / w, 1f / h) : Vector4.zero);
            Shader.SetGlobalVector(ReflexVolumetricLight.Apply2Id, new Vector4(f.medium.y, f.medium.w, W, H));

            historyValid = true;
            prevVP = vp; prevCamPos = camPos; prevCamRot = camRot;
        }

        void Add(RenderGraph rg, string name, int passIndex, TextureHandle dst, PassData c, TextureHandle depth, TextureHandle src, TextureHandle history, TextureHandle shadow, AccessFlags dstAccess)
        {
            using (var builder = rg.AddRasterRenderPass<PassData>(name, out var d, ReflexPassSamplers.Get(name)))
            {
                d.material = material; d.pass = passIndex;
                if (blockIndex >= blocks.Count) blocks.Add(new MaterialPropertyBlock());
                d.mpb = blocks[blockIndex++];
                d.depth = depth; d.src = src; d.history = history; d.shadow = shadow; d.noise = c.noise; d.ground = c.ground;
                d.full = c.full; d.low = c.low; d.prm = c.prm; d.z = c.z; d.tl = c.tl; d.xe = c.xe; d.ye = c.ye; d.cam = c.cam;
                d.medium = c.medium; d.medium2 = c.medium2; d.dust = c.dust; d.groundRect = c.groundRect; d.sun = c.sun; d.ambient = c.ambient;
                d.shadowPrm = c.shadowPrm; d.temporal = c.temporal; d.spotCount = c.spotCount; d.prevVP = c.prevVP;
                d.spotPos = c.spotPos; d.spotDir = c.spotDir; d.spotCol = c.spotCol; d.debug = c.debug;
                builder.UseTexture(depth);
                if (src.IsValid()) builder.UseTexture(src);
                if (history.IsValid()) builder.UseTexture(history);
                if (shadow.IsValid()) builder.UseTexture(shadow);
                builder.SetRenderAttachment(dst, 0, dstAccess);
                if (passIndex == PassComposite && src.IsValid()) builder.SetGlobalTextureAfterPass(src, ReflexVolumetricLight.ResultId);   // (the transparent objects: ReflexVolumetricApply.hlsl)
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    var p = data.mpb; p.Clear();
                    p.SetTexture(DepthId, data.depth);
                    if (data.src.IsValid()) p.SetTexture(SrcId, data.src);
                    if (data.history.IsValid()) p.SetTexture(HistoryId, data.history);
                    if (data.shadow.IsValid()) p.SetTexture(ShadowMapId, data.shadow);
                    p.SetTexture(NoiseId, data.noise); p.SetTexture(GroundId, data.ground);
                    p.SetVector(FullId, data.full); p.SetVector(LowId, data.low); p.SetVector(ParamsId, data.prm); p.SetVector(ZId, data.z);
                    p.SetVector(TlId, data.tl); p.SetVector(XeId, data.xe); p.SetVector(YeId, data.ye); p.SetVector(CamId, data.cam);
                    p.SetMatrix(PrevVPId, data.prevVP); p.SetVector(MediumId, data.medium); p.SetVector(Medium2Id, data.medium2); p.SetVector(DustId, data.dust);
                    p.SetVector(GroundRectId, data.groundRect); p.SetVector(SunId, data.sun); p.SetVector(AmbientId, data.ambient); p.SetVector(ShadowId, data.shadowPrm);
                    p.SetVector(TemporalId, data.temporal); p.SetVector(SpotCountId, data.spotCount); p.SetFloat(DebugId, data.debug);
                    p.SetVectorArray(SpotPosId, data.spotPos); p.SetVectorArray(SpotDirId, data.spotDir); p.SetVectorArray(SpotColId, data.spotCol);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, data.pass, MeshTopology.Triangles, 3, 1, p);
                });
            }
        }
    }
}
