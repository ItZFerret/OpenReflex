// LIGHTING: ORIGINAL / ENHANCED (OPTIONS > VIDEO, ReflexSettings.Lighting; default ORIGINAL). Not part of the original renderer.
//
// ORIGINAL is the port of the original renderer pass for pass (_re/notes/render-pipeline.md) and this component changes nothing of it.
// ENHANCED, live while a level runs (OriginalLevel adds this to the game camera next to ReflexRenderPipeline):
//   shadows   real-time sun shadows from the level's own sun: 2 / 4 cascades over 70 / 110 / 150 m (2048 px) by SHADOW QUALITY (LOW / MEDIUM / HIGH),
//             soft filtering, Unity's stable (texel-snapped) cascade fit; the static scenery casts too (in ORIGINAL only the dynamic
//             objects cast, as the original's VSM), every lit surface receives; contact shadows towards the sun (MEDIUM / HIGH)
//   ambient   screen-space ambient occlusion on the ambient term (MEDIUM half / HIGH full resolution); the sky's colour and direction on
//             the original ambient rig (spherical harmonics of the enhanced sky, normalised so the level's light level stays)
//   skies     a CC0 HDRI sky per outdoor level matched to its time of day and its original sky (ReflexEnhancedSky); reflections of that
//             sky on glossy scenery (Fresnel), the bikes / riders (the level's reflection map) and the water
//   post      the original's eye adaptation timing (per game frame, same rate and clamp) metering the whole frame, a mip-chain bloom, a
//             filmic tonemap and a light per-level grade (ReflexEnhancedPasses / Shaders/ReflexEnhancedPost.shader)
// Supercross stadiums, the night levels and the menus keep their original sky. Switching back restores every value it changed.
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Rendering
{
    [DisallowMultipleComponent]
    [RequireComponent(typeof(Camera))]
    public sealed class ReflexEnhancedLighting : MonoBehaviour
    {
        /// Tests: forces the mode (0 ORIGINAL, 1 ENHANCED; -1: the option). REFLEX_LIGHTING=original|enhanced sets it at start.
        public static int Override = -1;
        /// The mode the option asks for (OPTIONS > VIDEO > LIGHTING).
        public static bool Requested => Override >= 0 ? Override == 1 : Options.ReflexSettings.Lighting == 1;
        /// The quality tier: ENHANCED QUALITY (OPTIONS > VIDEO) when chosen, else AUTO: SHADOW QUALITY (graphics.cfg ShadowQuality 0..2) at most
        /// AutoTier (what the GPU suits); tests: REFLEX_LIGHTING_TIER.
        public static int Tier
        {
            get
            {
                if (TierOverride >= 0) return TierOverride;
                int q = -1; try { q = Options.ReflexSettings.EnhancedQuality; } catch { }
                if (q >= 0) return q;
                int sq; try { sq = Mathf.Clamp(Content.ReflexLaunchSettings.Current.shadowQuality, 0, 2); } catch { sq = 2; }
                return Mathf.Min(sq, AutoTier);
            }
        }
        /// The ENHANCED tier this machine's GPU suits (AUTO): integrated graphics or under 3 GB of video memory LOW, under 6 GB MEDIUM,
        /// else HIGH (a discrete 6 GB+ card, Apple silicon). An estimate from SystemInfo only; ENHANCED QUALITY overrides it.
        public static int AutoTier
        {
            get
            {
                if (autoTier >= 0) return autoTier;
                int vram = SystemInfo.graphicsMemorySize;   // MB (Apple: the working set the GPU may use)
                string name = (SystemInfo.graphicsDeviceName ?? "") + " " + (SystemInfo.graphicsDeviceVendor ?? "");
                bool apple = name.IndexOf("Apple", StringComparison.OrdinalIgnoreCase) >= 0;
                bool integrated = !apple && (name.IndexOf("Intel", StringComparison.OrdinalIgnoreCase) >= 0 && name.IndexOf("Arc", StringComparison.OrdinalIgnoreCase) < 0
                                  || name.IndexOf("Radeon(TM) Graphics", StringComparison.OrdinalIgnoreCase) >= 0 || name.IndexOf("Vega", StringComparison.OrdinalIgnoreCase) >= 0 && vram < 3000);
                autoTier = integrated || vram > 0 && vram < 3000 ? 0 : vram > 0 && vram < 6000 && !apple ? 1 : 2;
                return autoTier;
            }
        }
        static int autoTier = -1;
        public static int TierOverride = -1;
        /// Diagnostics: 1 shows the ambient occlusion, 2 the contact shadows, 3 the occlusion pass's reconstructed position (2 m world checker),
        /// 4 its reconstructed normal's y, instead of the picture (_RfxEnhDebugView).
        public static int DebugView { get => debugView; set { debugView = value; Shader.SetGlobalFloat("_RfxEnhDebugView", value); } }
        static int debugView;

        public OriginalLevel level;
        [Header("Result (read only)")]
        public bool active;
        public int activeTier = -1;
        int activeDrawDistance = -1;   // DRAW DISTANCE (ReflexDrawDistance) the shadows were set for
        [TextArea(2, 6)] public string report;

        ReflexRenderPipeline pipeline;
        ReflexEnhancedSky sky; bool skyTried;
        Renderer dome; Material domeOriginal;
        readonly List<Renderer> casters = new List<Renderer>();
        Terrain castingTerrain; ShadowCastingMode terrainMode;
        ShadowState saved;
        Light sun; LightShadows sunShadows; SoftShadowQuality sunSoft; bool sunSaved;
        Texture savedReflection; float savedReflectionOn; bool reflectionSwapped;
        OriginalWater water; Cubemap waterCube;
        static readonly GlobalKeyword EnhancedKeyword = GlobalKeyword.Create("_RFX_ENHANCED");
        static readonly int SkyCubeId = Shader.PropertyToID("_RfxEnhSkyCube"), ReflectionCubeId = Shader.PropertyToID("_RfxReflectionCube"),
            ReflectionOnId = Shader.PropertyToID("_RfxReflectionOn");
        static readonly int[] ShIds = { Shader.PropertyToID("_RfxEnhSHAr"), Shader.PropertyToID("_RfxEnhSHAg"), Shader.PropertyToID("_RfxEnhSHAb"),
            Shader.PropertyToID("_RfxEnhSHBr"), Shader.PropertyToID("_RfxEnhSHBg"), Shader.PropertyToID("_RfxEnhSHBb"), Shader.PropertyToID("_RfxEnhSHC") };

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ReadEnvironment()
        {
            string v = Environment.GetEnvironmentVariable("REFLEX_LIGHTING");
            if (string.Equals(v, "enhanced", StringComparison.OrdinalIgnoreCase)) Override = 1;
            else if (string.Equals(v, "original", StringComparison.OrdinalIgnoreCase)) Override = 0;
            if (int.TryParse(Environment.GetEnvironmentVariable("REFLEX_LIGHTING_TIER"), out int tier)) TierOverride = Mathf.Clamp(tier, 0, 2);
            Shader.SetKeyword(EnhancedKeyword, false);
        }

        void OnEnable() { pipeline = GetComponent<ReflexRenderPipeline>(); }

        void OnDisable() { if (active) Apply(false, activeTier); }

        void OnDestroy()
        {
            if (active) Apply(false, activeTier);
            sky?.Dispose(); sky = null;
        }

        void LateUpdate()
        {
            if (!pipeline) pipeline = GetComponent<ReflexRenderPipeline>();
            if (!pipeline || !level || !level.Loaded) return;
            bool want = Requested; int tier = Tier;
            if (want != active || (want && (tier != activeTier || ReflexDrawDistance.Level != activeDrawDistance))) Apply(want, tier);
        }

        /// Switches the mode (live). Every value ENHANCED changes is put back when it is switched off.
        public void Apply(bool on, int tier)
        {
            if (active) Restore();
            active = false; activeTier = -1;
            if (!on || !pipeline) { report = "ORIGINAL"; return; }
            var profile = ReflexEnhancedSky.For(level ? level.level : null);
            var s = Settings(tier, profile, pipeline.hdr);
            Shader.SetKeyword(EnhancedKeyword, true);
            SetShadows(tier);
            if (profile.sceneryShadows) SetCasters(tier); else casters.Clear();
            SetSky(s);
            pipeline.enhanced = s;
            active = true; activeTier = tier; activeDrawDistance = ReflexDrawDistance.Level;
            report = $"ENHANCED tier {tier} ({(tier == 0 ? "LOW" : tier == 1 ? "MEDIUM" : "HIGH")}): shadows {ShadowReport()}, {casters.Count} scenery casters, occlusion "
                     + (s.occlusion ? $"1/{s.occlusionDownscale} res, contact {(s.contact ? "on" : "off")}" : "off") + "; " + (sky != null ? sky.report : "original sky (" + (profile.sky == null ? "no profile" : "not built") + ")");
            Debug.Log("[Reflex lighting] " + report);
        }

        static ReflexEnhancedSettings Settings(int tier, ReflexEnhancedSky.Profile p, ReflexHdrSettings hdr)
        {
            var s = new ReflexEnhancedSettings
            {
                occlusion = tier >= 1, occlusionDownscale = 2, aoSamples = tier >= 2 ? 10 : 6,
                contact = tier >= 1, contactDistance = tier >= 2 ? 60f : 40f,
                skyAmbient = p.skyAmbient, reflection = p.reflection,
                saturation = p.saturation, contrast = p.contrast, gain = p.gain,
            };
            s.exposureBias *= p.postExposure;
            // bloom follows the level's own bloom strength (its HDRSettings Bloom Attenuation, 0.3 .. 0.6)
            s.bloomIntensity = 0.06f * p.bloom * Mathf.Clamp(hdr.bloomAttenuation / 0.5f, 0.5f, 1.5f);
            return s;
        }

        // ------------------------------------------------------------------------------------------------ shadows
        struct ShadowState
        {
            public UniversalRenderPipelineAsset asset; public float distance, border, depthBias, normalBias; public int cascades, resolution;
            public float split2; public Vector3 split4;
        }

        void SetShadows(int tier)
        {
            sun = RenderSettings.sun;
            if (sun)
            {
                sunSaved = true; sunShadows = sun.shadows;
                var ld = sun.GetComponent<UniversalAdditionalLightData>();
                sunSoft = ld ? ld.softShadowQuality : SoftShadowQuality.UsePipelineSettings;
                sun.shadows = LightShadows.Soft;
                if (ld) ld.softShadowQuality = tier >= 2 ? SoftShadowQuality.High : SoftShadowQuality.Medium;
            }
            // the URP asset is shared: changed in player builds only (in the editor it would be saved into the project), restored on exit
            if (Application.isEditor || !(GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp)) return;
            saved = new ShadowState
            {
                asset = urp, distance = urp.shadowDistance, cascades = urp.shadowCascadeCount, split2 = urp.cascade2Split, split4 = urp.cascade4Split,
                resolution = urp.mainLightShadowmapResolution, border = urp.cascadeBorder, depthBias = urp.shadowDepthBias, normalBias = urp.shadowNormalBias,
            };
            // DRAW DISTANCE HIGH / ULTRA (w-rmlod): the shadow distance times 2 / 3 (world units are the level's feet: 150 is ~46 m) with the splits
            // pulled in to keep the near cascades as sharp as they were, and a wider fade at the range end
            float reach = ReflexDrawDistance.ShadowScale;
            urp.shadowDistance = (tier == 0 ? 70f : tier == 1 ? 110f : 150f) * reach;
            urp.shadowCascadeCount = tier == 0 ? 2 : 4;
            urp.cascade2Split = reach > 1f ? 0.18f : 0.25f;
            urp.cascade4Split = reach > 1f ? (tier == 1 ? new Vector3(0.05f, 0.15f, 0.38f) : new Vector3(0.035f, 0.1f, 0.28f))
                                           : tier == 1 ? new Vector3(0.08f, 0.22f, 0.5f) : new Vector3(0.06f, 0.18f, 0.45f);
            urp.cascadeBorder = reach > 1f ? 0.25f : 0.15f;
            urp.mainLightShadowmapResolution = reach > 2.5f && tier >= 2 ? 4096 : 2048;   // 4096 cost ~4 ms more at HIGH (Apple M-series, 1080p) for little at the default distance: the first cascade is ~20 m wide
            urp.shadowDepthBias = 0.6f;
            urp.shadowNormalBias = 0.6f;
        }

        string ShadowReport()
        {
            if (GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp)
                return $"{urp.shadowCascadeCount} cascades {urp.shadowDistance:0} m {urp.mainLightShadowmapResolution} px" + (Application.isEditor ? " (editor: the project's)" : "");
            return "?";
        }

        void SetCasters(int tier)
        {
            casters.Clear();
            var scenery = Shader.Find("Reflex/Original Scenery");
            foreach (var r in FindObjectsByType<MeshRenderer>(FindObjectsInactive.Include, FindObjectsSortMode.None))
            {
                if (r.shadowCastingMode != ShadowCastingMode.Off) continue;
                var m = r.sharedMaterial;
                if (!m || m.shader != scenery || m.renderQueue >= 2450 || !m.HasProperty("_ZWrite") || m.GetFloat("_ZWrite") < 0.5f) continue;
                r.shadowCastingMode = ShadowCastingMode.On;
                casters.Add(r);
            }
            // the terrain itself does not cast: its hills already shadow the track and the objects through the original's static terrain
            // shadow maps (the terrain's visibility map, the volume shadow map), and drawing it into four cascades costs ~6 ms at 1080p
        }

        // ------------------------------------------------------------------------------------------------ sky
        void SetSky(ReflexEnhancedSettings s)
        {
            if (!skyTried && level.SceneryRoot)
            {
                skyTried = true;
                var skyShader = Shader.Find("Reflex/Original Sky");
                foreach (var r in level.SceneryRoot.GetComponentsInChildren<MeshRenderer>(true))
                    if (r.sharedMaterial && r.sharedMaterial.shader == skyShader) { dome = r; domeOriginal = r.sharedMaterial; break; }
                if (dome)
                {
                    var l = level.Content.lighting;
                    Texture origCube = Shader.GetGlobalFloat(ReflectionOnId) > 0.5f ? Shader.GetGlobalTexture(ReflectionCubeId) : null;
                    sky = ReflexEnhancedSky.Build(level.level, dome, domeOriginal, origCube, l.sunDirection.normalized, l.sunColor);
                }
            }
            if (sky != null && dome)
            {
                dome.sharedMaterial = sky.material;
                for (int i = 0; i < 7; i++) Shader.SetGlobalVector(ShIds[i], sky.sh[i]);
                Shader.SetGlobalTexture(SkyCubeId, sky.cube);
                s.skyCube = true; s.skyCubeMips = Mathf.Log(sky.cube.width, 2f);
                // the bikes and riders reflect the new sky (their programs sample the level's reflection map, s14)
                savedReflection = Shader.GetGlobalTexture(ReflectionCubeId); savedReflectionOn = Shader.GetGlobalFloat(ReflectionOnId); reflectionSwapped = true;
                Shader.SetGlobalTexture(ReflectionCubeId, sky.cube); Shader.SetGlobalFloat(ReflectionOnId, 1f);
                // the water: its own reflection map near the horizon (the shores it reflects), the new sky above
                water = level.GetComponent<OriginalWater>();
                if (water)
                {
                    if (!waterCube) waterCube = sky.ReflectionWith(water.LevelReflection);
                    if (waterCube) water.SetReflectionOverride(waterCube);
                }
            }
            else
            {
                // no new sky: the ambient keeps its colour (SH = 1), no sky reflections
                for (int i = 0; i < 7; i++) Shader.SetGlobalVector(ShIds[i], i == 0 || i == 1 || i == 2 ? new Vector4(0, 0, 0, 1) : Vector4.zero);
                s.skyCube = false;
            }
        }

        void Restore()
        {
            pipeline.enhanced = null;
            Shader.SetKeyword(EnhancedKeyword, false);
            if (saved.asset)
            {
                var u = saved.asset;
                u.shadowDistance = saved.distance; u.shadowCascadeCount = saved.cascades; u.cascade2Split = saved.split2; u.cascade4Split = saved.split4;
                u.mainLightShadowmapResolution = saved.resolution; u.cascadeBorder = saved.border; u.shadowDepthBias = saved.depthBias; u.shadowNormalBias = saved.normalBias;
                saved = default;
            }
            if (sunSaved && sun)
            {
                sun.shadows = sunShadows;
                var ld = sun.GetComponent<UniversalAdditionalLightData>(); if (ld) ld.softShadowQuality = sunSoft;
            }
            sunSaved = false;
            foreach (var r in casters) if (r) r.shadowCastingMode = ShadowCastingMode.Off;
            casters.Clear();
            if (castingTerrain) castingTerrain.shadowCastingMode = terrainMode;
            castingTerrain = null;
            if (dome && domeOriginal) dome.sharedMaterial = domeOriginal;
            if (reflectionSwapped)
            {
                if (savedReflection) Shader.SetGlobalTexture(ReflectionCubeId, savedReflection);
                Shader.SetGlobalFloat(ReflectionOnId, savedReflectionOn);
                reflectionSwapped = false;
            }
            if (water) water.SetReflectionOverride(null);
            water = null;
        }
    }
}
