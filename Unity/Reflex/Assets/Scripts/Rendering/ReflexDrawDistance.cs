// DRAW DISTANCE: ORIGINAL / HIGH / ULTRA (OPTIONS > VIDEO, ReflexSettings.DrawDistance; w-rmlod). Not part of the original renderer.
//
// ORIGINAL changes nothing: the forest, the scenery and the grass keep the original renderer's distances, hard LOD records and fades, and
// the shadow distance stays the project's (and ENHANCED's own tier distances). HIGH and ULTRA are the "remastered" draw distances:
//   trees     the 3D trees reach TreeScale times as far (every depth of ForestSystem - the LOD records, the leaf / branch / frond dissolve
//             and the billboard fade - is stretched by it), measured as the distance from the camera instead of the depth along its
//             view direction (the depth changes when the camera only turns, so a tree near a threshold flipped back and forth as the
//             rider steered), the LOD records cross-faded with a dither over a band instead of swapped, and the shadow casters chosen
//             by their distance to the camera and the sun, not by what the camera sees nor by the tree's LOD (Rendering / OriginalForest.cs)
//   scenery   the draw distance and the LOD distances of every model times SceneryScale, LOD changes with hysteresis (a different
//             switch-in and switch-out distance), the draw-distance fade dithered (opaque, so the object keeps casting its shadow) and a
//             model kept as a shadow caster after its draw distance (OriginalSceneryLod.cs)
//   grass     the cell window times GrassScale (OriginalForest.cs)
//   shadows   the shadow distance times ShadowScale (ENHANCED: its tier's) or OriginalLightingShadowScale (ORIGINAL lighting: the project's 50) with cascade splits to
//             match (ReflexEnhancedLighting, ReflexDrawDistanceDriver below)
// REFLEX_DRAW_DISTANCE=original|high|ultra sets it at start (tests). Distances are in world units (the level's feet).
using System;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Rendering
{
    public static class ReflexDrawDistance
    {
        public const int Original = 0, High = 1, Ultra = 2;
        /// Tests: forces the level (-1: the option). REFLEX_DRAW_DISTANCE=original|high|ultra sets it at start.
        public static int Override = -1;
        public static int Level { get { if (Override >= 0) return Override; if (!Application.isPlaying) return Original; try { return Options.ReflexSettings.DrawDistance; } catch { return High; } } }   // (edit-mode tests compare the original renderer)
        public static bool Remastered => Level > 0;
        public static string Name(int level) => level == 2 ? "ULTRA" : level == 1 ? "HIGH" : "ORIGINAL";

        /// ForestSystem depths (the 3D tree range, the LOD records, the billboard fade) are divided by this.
        public static float TreeScale => Level == 2 ? 2.5f : Level == 1 ? 1.7f : 1f;
        /// ModelAsset draw distance and LOD distances times this.
        public static float SceneryScale => Level == 2 ? 2.4f : Level == 1 ? 1.6f : 1f;
        /// GrassSystem MaxDrawDistance / FadeDistance times this.
        public static float GrassScale => Level == 2 ? 1.8f : Level == 1 ? 1.4f : 1f;
        /// ENHANCED lighting: the main-light shadow distance (its tier's) times this.
        public static float ShadowScale => Level == 2 ? 3f : Level == 1 ? 2f : 1f;
        /// ORIGINAL lighting: the project's 50 units (15 m: only the bikes, the riders and the forest's 3D trees cast, so a longer reach is cheap) times this.
        public static float OriginalLightingShadowScale => Level == 2 ? 5f : Level == 1 ? 3f : 1f;
        /// Cross-fade half-width of a tree LOD record edge: a share of the edge's distance, at least MinBand (world units, before the scale).
        public const float TreeBandShare = 0.07f, TreeBandMin = 4f;
        /// Scenery LOD hysteresis: switch outward at (1 + share) x the LOD distance, back inward at (1 - share) x.
        public const float SceneryHysteresis = 0.06f;

        public static readonly int GlobalId = Shader.PropertyToID("_RfxDrawDist");   // x: 1 / TreeScale, y: distance (1) or view depth (0), z: dithered model fade

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ReadEnvironment()
        {
            string v = Environment.GetEnvironmentVariable("REFLEX_DRAW_DISTANCE");
            if (string.Equals(v, "original", StringComparison.OrdinalIgnoreCase)) Override = 0;
            else if (string.Equals(v, "high", StringComparison.OrdinalIgnoreCase)) Override = 1;
            else if (string.Equals(v, "ultra", StringComparison.OrdinalIgnoreCase)) Override = 2;
            Shader.SetGlobalVector(GlobalId, new Vector4(1f, 0f, 0f, 0f));
        }

        /// The shader constant of the level now in force (the forest and the scenery set it before they draw, whatever the driver did).
        public static void ApplyGlobals()
        {
            bool re = Level > 0;
            Shader.SetGlobalVector(GlobalId, new Vector4(1f / TreeScale, re ? 1f : 0f, re ? 1f : 0f, 0f));
        }

        /// The asset's own main-light shadow distance (what the project ships; captured before anything changes it).
        public static float BaseShadowDistance { get; private set; } = 50f;
        static bool baseKnown;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (!baseKnown && GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp) { BaseShadowDistance = urp.shadowDistance; baseKnown = true; }
            if (UnityEngine.Object.FindAnyObjectByType<ReflexDrawDistanceDriver>()) return;
            var go = new GameObject("Reflex draw distance"); UnityEngine.Object.DontDestroyOnLoad(go); go.hideFlags = HideFlags.HideAndDontSave;
            go.AddComponent<ReflexDrawDistanceDriver>();
        }

        /// The shadow distance now in force (the URP asset's).
        public static float ShadowDistanceNow => GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp ? urp.shadowDistance : BaseShadowDistance;
    }

    /// Per frame: the shader globals of the draw distance and, while ENHANCED lighting is off, the shadow distance (ENHANCED sets its own).
    [DefaultExecutionOrder(-100)]
    public sealed class ReflexDrawDistanceDriver : MonoBehaviour
    {
        int lastLevel = -1;

        void Update()
        {
            int level = ReflexDrawDistance.Level;
            ReflexDrawDistance.ApplyGlobals();
            if (!(GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp) || Application.isEditor) return;
            if (ReflexEnhancedLighting.Requested && FindEnhancedActive()) { lastLevel = -1; return; }   // ENHANCED sets the distance (and restores it on exit)
            float want = ReflexDrawDistance.BaseShadowDistance * ReflexDrawDistance.OriginalLightingShadowScale;
            if (level != lastLevel || !Mathf.Approximately(urp.shadowDistance, want)) { urp.shadowDistance = want; lastLevel = level; }
        }

        static ReflexEnhancedLighting enh;
        static bool FindEnhancedActive()
        {
            if (!enh) enh = FindAnyObjectByType<ReflexEnhancedLighting>();
            return enh && enh.active;
        }
    }
}
