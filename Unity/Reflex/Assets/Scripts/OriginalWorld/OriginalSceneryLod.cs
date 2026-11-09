using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.Rendering;

namespace Reflex.Port.OriginalWorld
{
    // The original renderer's draw distance, fade and LOD choice for the static scenery (Model_Submit 007f1b00, Model_LodForDistance
    // 007f8420; the data and the rules: Content/ReflexSceneryLod.cs). Every frame for the game camera: culled instances get
    // Renderer.forceRenderingOff (independent of the enabled / active flags other components use), the LOD's mesh and materials are
    // swapped in when it changes, and the fade alpha the original would draw with is given to the renderer as _RfxModelAlpha (0..1);
    // while it fades the renderer draws through blended clones of its materials (PlatformModelAllSemiTransEvent: opaque ones become
    // SrcAlpha / InvSrcAlpha in the transparent queue). Instances the running game hides (a physics prop's ModelInstance +0xa2,
    // OriginalProps) are kept off.
    [DisallowMultipleComponent]
    public sealed class OriginalSceneryLod : MonoBehaviour
    {
        // elapsed milliseconds since a Stopwatch.GetTimestamp() (no Stopwatch object a frame, w-gcspikes)
        static double Ms(long since) => (System.Diagnostics.Stopwatch.GetTimestamp() - since) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
        public OriginalLevel level;
        public Camera viewer;
        [Tooltip("Cull beyond the model's draw distance as the original (ModelAsset +0x120).")]
        public bool cull = true;
        [Tooltip("Switch to the model's further LODs at their distances.")]
        public bool switchLods = true;
        [Tooltip("The renderer divisor ([[0xa95844 + 0x1c + 4 [0xa95844 + 4]] + 0x19d0], 1 with one view).")]
        public uint divisor = 1;

        [Header("Result (read only)")]
        public int instances; public int multiLod; public int culled; public int fading; public int nonZeroLod;
        public double milliseconds;
        /// Tests (REFLEX_FOREST_STATS=1): LOD changes of the instances this frame, and flips (an instance back in the LOD it left within 8 frames).
        public static bool collectStats = Environment.GetEnvironmentVariable("REFLEX_FOREST_STATS") == "1";
        public int lodChanges, lodFlips, shadowOnlyCount;

        Task<Dictionary<string, ReflexLevelContent.ModelLodData>> reading;
        List<ReflexLevelContent.LodInstance> list;
        /// The placed instances with their current LOD (null until the LOD data is read; tests / probes).
        public IReadOnlyList<ReflexLevelContent.LodInstance> Instances => list;
        readonly Dictionary<Transform, ReflexLevelContent.LodInstance> byTransform = new Dictionary<Transform, ReflexLevelContent.LodInstance>();
        Vector3[] centres;
        ReflexLevelContent content;
        static readonly int ScrollId = Shader.PropertyToID("_ScrollParams"), OffsetId = Shader.PropertyToID("_RfxTextureOffset"), ScrollOverrideId = Shader.PropertyToID("_RfxScrollOverride"), AlphaSourceId = Shader.PropertyToID("_AlphaSource");
        static readonly int AlphaId = Shader.PropertyToID("_RfxModelAlpha"), FadeOpaqueId = Shader.PropertyToID("_RfxFadeOpaque"),
            SrcBlendId = Shader.PropertyToID("_SrcBlend"), DstBlendId = Shader.PropertyToID("_DstBlend");
        MaterialPropertyBlock mpb;
        readonly Dictionary<Material, Material> fadedOf = new Dictionary<Material, Material>();
        readonly Dictionary<Material[], Material[]> fadedSets = new Dictionary<Material[], Material[]>();

        Material Faded(Material m)
        {
            if (!m) return m;
            if (fadedOf.TryGetValue(m, out var f) && f) return f;
            if (!m.HasProperty(FadeOpaqueId)) return fadedOf[m] = m;   // not Reflex/Original Scenery
            f = new Material(m) { name = m.name + " (fading)" };
            if (m.GetFloat(SrcBlendId) == 1f && m.GetFloat(DstBlendId) == 0f)
            {
                f.SetFloat(SrcBlendId, (float)UnityEngine.Rendering.BlendMode.SrcAlpha); f.SetFloat(DstBlendId, (float)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
                f.SetFloat(FadeOpaqueId, 1f); f.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Transparent;
            }
            return fadedOf[m] = f;
        }
        Material[] FadedSet(Material[] set)
        {
            if (fadedSets.TryGetValue(set, out var f)) return f;
            f = new Material[set.Length]; for (int i = 0; i < set.Length; i++) f[i] = Faded(set[i]);
            return fadedSets[set] = f;
        }

        void OnDestroy()
        {
            foreach (var kv in fadedOf) if (kv.Value && kv.Value != kv.Key) Destroy(kv.Value);
            fadedOf.Clear(); fadedSets.Clear();
        }

        /// Starts reading the level's model LOD data (worker thread); the instances are set up once it is done.
        public void Begin(OriginalLevel lvl)
        {
            level = lvl; content = lvl ? lvl.Content : null; list = null; byTransform.Clear();
            if (content == null) return;
            var c = content;
            reading = Task.Run(() => c.ReadLodData());
        }

        [Tooltip("Instances the running game does not draw (ModelInstance +0xa2 clear) stay off.")]
        public OriginalProps props;

        void LateUpdate()
        {
            if (content == null || (level && level.Content != content)) { if (level && level.Content != null) Begin(level); else return; }
            if (list == null)
            {
                if (reading == null || !reading.IsCompleted) return;
                if (reading.IsFaulted) { Debug.LogWarning("OriginalSceneryLod: " + reading.Exception?.GetBaseException().Message, this); reading = null; list = new List<ReflexLevelContent.LodInstance>(); return; }
                list = content.BuildLods(reading.Result);
                centres = new Vector3[list.Count];
                for (int i = 0; i < list.Count; i++)
                {
                    var li = list[i]; byTransform[li.instance.transform] = li;
                    centres[i] = li.instance.transform.TransformPoint(li.model.boundsCentre);
                    if (li.meshes.Length > 1) multiLod++;
                }
                instances = list.Count;
            }
            long sw = System.Diagnostics.Stopwatch.GetTimestamp();
            if (!props) props = GetComponent<OriginalProps>();
            var cam = viewer ? viewer : (level && level.runtime && level.runtime.chaseCamera ? level.runtime.chaseCamera : Camera.main);
            if (!cam) return;
            Vector3 eye = cam.transform.position;
            float mapScale2 = level ? level.MapScale * level.MapScale : 1f;
            mpb ??= new MaterialPropertyBlock();
            int nCulled = 0, nFading = 0, nLod = 0, nChanges = 0, nFlips = 0, nShadowOnly = 0;
            // DRAW DISTANCE HIGH / ULTRA (Rendering/ReflexDrawDistance): the model draw and LOD distances stretched, LOD changes with hysteresis, the
            // fade a dither on the opaque material, a culled model that casts a shadow kept as a shadow caster while its shadow can be seen
            bool re = ReflexDrawDistance.Remastered;
            ReflexDrawDistance.ApplyGlobals();
            float ss = re ? ReflexDrawDistance.SceneryScale : 1f;
            float shadowFile = re ? ReflexDrawDistance.ShadowDistanceNow / (level ? level.MapScale : 1f) : 0f, shadowReach2 = (shadowFile + 40f) * (shadowFile + 40f);
            float hOut = (1f + ReflexDrawDistance.SceneryHysteresis) * (1f + ReflexDrawDistance.SceneryHysteresis), hIn = (1f - ReflexDrawDistance.SceneryHysteresis) * (1f - ReflexDrawDistance.SceneryHysteresis);
            for (int i = 0; i < list.Count; i++)
            {
                var li = list[i];
                if (!li.renderer) continue;
                Vector3 c = li.dynamic ? li.instance.transform.TransformPoint(li.model.boundsCentre) : centres[i];
                float dx = c.x - eye.x, dy = c.y - eye.y, dz = c.z - eye.z;
                float f = dx * dx + dy * dy + dz * dz;
                if (mapScale2 != 1f) f /= mapScale2;   // MAP SCALE: the level's draw / LOD distances measured in its own (unscaled) feet
                uint d2 = f >= 4.2949673e9f ? uint.MaxValue : (uint)Math.Round(f, MidpointRounding.ToEven);   // ROUND (fistp, nearest)
                uint D = divisor == 0 ? li.model.drawDistance : li.model.drawDistance / divisor;
                bool off = false; byte alpha = 255;
                if (!li.alwaysVisible)
                {
                    if (re)
                    {
                        if (cull && D != 0)
                        {
                            float full = D * ss * D * ss, start = full * 0.7f, fd = d2;
                            if (full < fd) off = true;
                            else if (start < fd) alpha = unchecked((byte)(255 - (byte)(int)((fd - start) / (full - start) * 255f)));
                        }
                    }
                    else if (cull && D != 0 && unchecked(D * D) < d2) off = true;
                    else
                    {
                        float full = unchecked(D * D), start = full * 0.7f, fd = d2;
                        if (start < fd) alpha = unchecked((byte)(255 - (byte)(int)((fd - start) / (full - start) * 255f)));
                    }
                }
                // past its draw distance a shadow caster stays in the shadow maps (shadows only) while its shadow can be seen
                bool shadowOnly = false;
                if (off && re && d2 < shadowReach2 && li.renderer.shadowCastingMode != UnityEngine.Rendering.ShadowCastingMode.Off) { shadowOnly = true; off = false; }
                if (shadowOnly != li.shadowOnly)
                {
                    if (shadowOnly) { if (li.renderer.shadowCastingMode == UnityEngine.Rendering.ShadowCastingMode.On) li.renderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.ShadowsOnly; }
                    else if (li.renderer.shadowCastingMode == UnityEngine.Rendering.ShadowCastingMode.ShadowsOnly) li.renderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.On;
                    li.shadowOnly = shadowOnly;
                }
                off |= props && props.IsHidden(li.instance.transform);
                if (li.renderer.forceRenderingOff != off) li.renderer.forceRenderingOff = off;
                if (li.shadowOnly) nShadowOnly++;
                if (off) { nCulled++; continue; }
                if (alpha != li.alpha)
                {
                    li.renderer.GetPropertyBlock(mpb); mpb.SetFloat(AlphaId, alpha / 255f);
                    // back to full: a block that only held the fade goes (the material's own _RfxModelAlpha is 1); a renderer with a
                    // property block is drawn outside the SRP Batcher, so every model that once faded would stay the slower draw
                    if (alpha == 255 && !mpb.HasVector(ScrollId) && !mpb.HasVector(ScrollOverrideId) && !mpb.HasVector(OffsetId) && !mpb.HasFloat(AlphaSourceId)) li.renderer.SetPropertyBlock(null);
                    else li.renderer.SetPropertyBlock(mpb);
                    li.alpha = alpha;
                }
                if (alpha < 255) nFading++;
                int lod = 0;
                var ld = li.model.lodDistances;
                if (switchLods && ld.Length > 1)
                {
                    if (!re)
                    {
                        for (lod = ld.Length - 1; lod > 0; lod--) if (unchecked(ld[lod] * ld[lod]) <= d2) break;
                    }
                    else
                    {
                        // hysteresis: the LOD that holds when the distances are pushed out / pulled in; the current LOD stays between them
                        int lodOut = 0, lodIn = 0; float fd = d2;
                        for (int q = ld.Length - 1; q > 0; q--) { float Lq = ld[q] * ss; if (Lq * Lq * hOut <= fd) { lodOut = q; break; } }
                        for (int q = ld.Length - 1; q > 0; q--) { float Lq = ld[q] * ss; if (Lq * Lq * hIn <= fd) { lodIn = q; break; } }
                        int cur = Math.Min(li.lod, ld.Length - 1);
                        lod = cur > lodIn ? lodIn : cur < lodOut ? lodOut : cur;
                    }
                }
                if (lod >= li.meshes.Length) lod = li.meshes.Length - 1;
                bool fade = !re && alpha < 255;
                if (lod != li.lod || fade != li.faded)
                {
                    if (collectStats && lod != li.lod)
                    {
                        nChanges++;
                        if (li.lodBack == lod && Time.frameCount - li.lodBackFrame <= 8) nFlips++;
                        li.lodBack = li.lod; li.lodBackFrame = Time.frameCount;
                    }
                    if (lod != li.lod && li.meshes[lod]) li.filter.sharedMesh = li.meshes[lod];
                    li.lod = lod; li.faded = fade;
                    var set = li.materials[lod] ?? li.materials[0];
                    if (set != null) li.renderer.sharedMaterials = fade ? FadedSet(set) : set;
                }
                if (lod > 0) nLod++;
            }
            culled = nCulled; fading = nFading; nonZeroLod = nLod; lodChanges = nChanges; lodFlips = nFlips; shadowOnlyCount = nShadowOnly;
            milliseconds = milliseconds == 0 ? Ms(sw) : milliseconds * 0.95 + Ms(sw) * 0.05;
        }
    }
}
