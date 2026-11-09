using System;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.OriginalWorld
{
    // The level's terrain as the original physics sees it: a Unity Terrain (GPU LOD) built from the
    // collision heights of the selected HFTerrainSurface in the running core or legacy world
    // (base * 2048 + (deformation - 0.5) * 2 * gain), placed on the
    // game's own grid. When the deformation surface changes (ruts), only the changed rectangle is
    // re-read and re-uploaded.
    //
    // Grid mapping: column c at x = min_x + c * spacing, row r at z = max_z - r * spacing.
    // Unity heightmap sample [j, i] sits at x = min_x + i * spacing, z = min_z + j * spacing,
    // i.e. column i and row (samples - 1 - j). The extra 2049th row/column repeats the edge.
    [DisallowMultipleComponent]
    public sealed class OriginalTerrain : MonoBehaviour
    {
        public TerrainLayer[] layers = Array.Empty<TerrainLayer>();
        [Tooltip("Optional terrain material; URP Terrain/Lit when empty.")]
        public Material materialTemplate;
        [Range(1, 16)] public float pixelError = 3f;
        public float basemapDistance = 2500f;

        static readonly int GridOriginId = Shader.PropertyToID("_GridOrigin");
        static readonly int DeformMapId = Shader.PropertyToID("_DeformMap");
        static readonly int UseDeformMapId = Shader.PropertyToID("_UseDeformMap");
        int DeformSize => (int)info.samples;   // the deformation surface matches the grid (2048 nationals / free ride, 1024 supercross)

        // The live deformation surface (the original DeformSampler, s10): rut darkening and detail fade in
        // the terrain shader. Unity row j holds grid row 2047 - j (texture v up, grid rows go -z).
        Texture2D deformMap;
        readonly System.Collections.Generic.Dictionary<long, Texture2D> deformScratch = new System.Collections.Generic.Dictionary<long, Texture2D>();
        // Patches are applied on rects rounded up to 32 samples (the core's tile rectangles already are), so a few scratch textures serve
        // every frame (no per-frame garbage); the rounding margin is filled from this CPU copy of the normalized heights (grid row order).
        // The scratch textures come in a few sizes (Bucket) so that rectangles of many different sizes share them.
        float[] mirror = Array.Empty<float>();
        static int Round32(int v, int n) => Math.Min(n, (v + 31) & ~31);
        static int Bucket(int v) => v <= 32 ? 32 : v <= 64 ? 64 : v <= 128 ? 128 : v <= 256 ? 256 : v <= 512 ? 512 : v;

        Terrain terrain;
        TerrainData data;
        Material runtimeMaterial;
        ReflexTerrainInfo info;
        float yMin, sizeY;

        public bool Built => terrain != null;
        public Terrain UnityTerrain => terrain;
        public ReflexTerrainInfo Info => info;

        public void Build(IntPtr world)
        {
            var source = new ReflexTerrainInfo();
            if (ReflexWorldNative.reflex_world_terrain(world, ref source) != 0) throw new InvalidOperationException("reflex_world_terrain failed: " + ReflexWorldNative.Error(world, "unknown"));
            int n = ValidateInfo(source);
            var heights = new float[n * n];
            if (ReflexWorldNative.reflex_world_terrain_heights(world, heights, 0, 0, n, n) != 0) throw new InvalidOperationException("reflex_world_terrain_heights failed: " + ReflexWorldNative.Error(world, "unknown"));
            var deformation = new float[n * n];
            if (ReflexWorldNative.reflex_world_terrain_deform(world, deformation, 0, 0, n, n) != 0) deformation = null;
            Build(source, heights, deformation);
        }

        // Call only while the session is exclusively owned by this thread. A running simulation should instead capture
        // these arrays on its worker and call the managed Build overload on Unity's main thread.
        public void BuildOpenCore(IntPtr session)
        {
            var source = new RxTerrainInfo();
            if (RxApiNative.rx_session_terrain(session, ref source) != 0) throw new InvalidOperationException("rx_session_terrain failed: " + RxApiNative.rx_session_last_error(session));
            int n = ValidateInfo(LegacyInfo(source));
            var heights = new float[n * n];
            if (RxApiNative.rx_session_terrain_heights(session, heights, 0, 0, n, n) != 0) throw new InvalidOperationException("rx_session_terrain_heights failed: " + RxApiNative.rx_session_last_error(session));
            var deformation = new float[n * n];
            if (RxApiNative.rx_session_terrain_deform(session, deformation, 0, 0, n, n) != 0) throw new InvalidOperationException("rx_session_terrain_deform failed: " + RxApiNative.rx_session_last_error(session));
            Build(source, heights, deformation);
        }

        // Snapshot arrays are packed in core grid order (row 0 = +z), independent of the source terrain's native stride.
        // All Unity allocations and uploads happen here on the main thread; no native world or cache file is needed.
        public void Build(RxTerrainInfo source, float[] heights, float[] deformation) => Build(LegacyInfo(source), heights, deformation);

        public void Build(ReflexTerrainInfo source, float[] heights, float[] deformation)
        {
            int n = ValidateInfo(source);
            int count = n * n;
            if (heights == null || heights.Length < count) throw new ArgumentException("Terrain snapshot has too few height samples.", nameof(heights));
            if (deformation != null && deformation.Length < count) throw new ArgumentException("Terrain snapshot has too few deformation samples.", nameof(deformation));
            float lo = float.MaxValue, hi = float.MinValue;
            for (int k = 0; k < count; k++)
            {
                float h = heights[k];
                if (!Finite(h)) throw new ArgumentException("Terrain snapshot contains a non-finite height.", nameof(heights));
                if (deformation != null && !Finite(deformation[k])) throw new ArgumentException("Terrain snapshot contains non-finite deformation.", nameof(deformation));
                if (h < lo) lo = h; if (h > hi) hi = h;
            }
            Release();
            info = source;
            // Room for the full deformation range (deformation in [0,1] moves a sample by +-gain).
            float margin = Mathf.Abs(info.gain) + 1f;
            yMin = lo - margin; sizeY = (hi - lo) + 2f * margin;
            mirror = new float[n * n];
            for (int k = 0; k < mirror.Length; k++) mirror[k] = (heights[k] - yMin) / sizeY;
            deformMirror = new float[n * n];
            if (deformation != null && deformation.Length >= n * n) Array.Copy(deformation, deformMirror, n * n);
            else for (int k = 0; k < deformMirror.Length; k++) deformMirror[k] = 0.5f;   // the original's neutral deformation value

            int res = NextHeightmapResolution(n);
            data = new TerrainData { heightmapResolution = res };
            data.size = new Vector3((res - 1) * info.spacing, sizeY, (res - 1) * info.spacing);
            var grid = new float[res, res];
            for (int j = 0; j < res; j++)
            {
                int r = Mathf.Clamp(n - 1 - j, 0, n - 1);
                for (int i = 0; i < res; i++)
                {
                    int c = Mathf.Min(i, n - 1);
                    grid[j, i] = (heights[r * n + c] - yMin) / sizeY;
                }
            }
            data.SetHeights(0, 0, grid);
            // Layers feed URP Terrain/Lit; the original-terrain material (has _GridOrigin) samples its own textures.
            bool originalMaterial = materialTemplate && materialTemplate.HasProperty(GridOriginId);
            if (layers != null && layers.Length > 0 && !originalMaterial) data.terrainLayers = layers;

            // Built by hand, NOT Terrain.CreateTerrainGameObject: that adds a TerrainCollider, and once a collider has existed the
            // TerrainData keeps a PhysX heightfield that Unity re-cooks on every SetHeights (deformation patches every frame), even
            // after the collider is disabled or destroyed. Physics is the original game's (librxcore), so no collider ever exists.
            var go = new GameObject("Original terrain", typeof(Terrain));
            go.GetComponent<Terrain>().terrainData = data;
            go.name = "Original terrain (live collision heights)";
            go.transform.SetParent(transform, false);
            go.transform.position = new Vector3(info.min_x, yMin, info.min_z);
            terrain = go.GetComponent<Terrain>();
            terrain.heightmapPixelError = Mathf.Min(pixelError, Reflex.Port.Options.ReflexSettings.TerrainPixelError);   // OPTIONS > VIDEO > TERRAIN DETAIL
            // HFTerrain owns its near/detail -> far/top-down transition in the shader.
            // It has no Unity TerrainLayers or generated basemap; switching to Unity's
            // base pass would discard those authored textures at long view distances.
            terrain.basemapDistance = originalMaterial ? float.MaxValue : basemapDistance;
            terrain.drawInstanced = true;
            // Original static terrain visibility is already sampled by HFTerrain. The original
            // dynamic shadow maps contain riders/vehicles/crowd, not terrain (render-pipeline.md §5).
            // Adding Unity terrain self-shadows produces a camera-relative dark cascade band.
            // Keep receiving dynamic shadows; ordinary Unity terrain still casts normally.
            terrain.shadowCastingMode = originalMaterial ? ShadowCastingMode.Off : ShadowCastingMode.On;
            if (materialTemplate)
            {
                // The original-terrain shader (Reflex/Original HFTerrain) maps world x/z to the height grid
                // (u = column / 2047, v = row / 2047); take the grid placement from the live world.
                var mat = materialTemplate;
                if (mat.HasProperty(GridOriginId))
                {
                    mat = new Material(materialTemplate) { name = materialTemplate.name + " (live grid)" };
                    mat.SetVector(GridOriginId, new Vector4(info.min_x, info.max_z, info.max_x - info.min_x, 0f));
                    runtimeMaterial = mat;
                }
                terrain.materialTemplate = mat;
                if (mat.HasProperty(DeformMapId) && runtimeMaterial) BuildDeformMap(deformation, mat);
            }
        }

        /// The collision height at a world point from the core's own grid (row 0 = max z, world z = max z - row x spacing, the core's
        /// convention: phys_terrain.c), bilinear over the last snapshot / patches: an independent check of the Unity terrain's placement
        /// (the MAP SCALE probe compares it with Terrain.SampleHeight).
        public float CoreHeightAt(float x, float z, out bool have)
        {
            have = false;
            if (mirror == null || mirror.Length < 4 || info.spacing <= 0f) return 0f;
            int n = (int)info.samples;
            float fc = (x - info.min_x) / info.spacing, fr = (info.max_z - z) / info.spacing;
            int c = Mathf.FloorToInt(fc), r = Mathf.FloorToInt(fr);
            if (c < 0 || r < 0 || c >= n - 1 || r >= n - 1) return 0f;
            float tx = fc - c, tz = fr - r;
            float h00 = mirror[r * n + c], h01 = mirror[r * n + c + 1], h10 = mirror[(r + 1) * n + c], h11 = mirror[(r + 1) * n + c + 1];
            float v = Mathf.Lerp(Mathf.Lerp(h00, h01, tx), Mathf.Lerp(h10, h11, tx), tz);
            have = true;
            return yMin + v * sizeY;
        }

        // Applies collision heights for grid columns [x0,x0+w), rows [z0,z0+h) (from the simulation thread).
        public void ApplyPatch(OriginalSimulation.TerrainPatch p)
        {
            if (!terrain || p.heights == null) return;
            long t0 = System.Diagnostics.Stopwatch.GetTimestamp();
            BeforePatch?.Invoke(this, p);   // (the replay records the change against the mirrors, before they take it)
            long t1 = System.Diagnostics.Stopwatch.GetTimestamp();
            ApplyPatchNow(p);
            // a slow patch in the session log (w-gcspikes: the end-of-race replay's whole-terrain patches held the main thread), at most one line
            // in 10 s
            double rec = (t1 - t0) * 1000.0 / System.Diagnostics.Stopwatch.Frequency, all = (System.Diagnostics.Stopwatch.GetTimestamp() - t0) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
            if (all > 20 && Time.realtimeSinceStartup - slowPatchLogged > 10f)
            {
                slowPatchLogged = Time.realtimeSinceStartup;
                Debug.Log(string.Format(System.Globalization.CultureInfo.InvariantCulture, "[terrain] slow patch {0} x {1} at ({2}, {3}): {4:0.0} ms on the main thread ({5:0.0} ms the replay's record)", p.w, p.h, p.x0, p.z0, all, rec));
            }
        }
        float slowPatchLogged = -100f;

        void ApplyPatchNow(OriginalSimulation.TerrainPatch p)
        {
            int n = (int)info.samples;
            if (p.deform != null && deformMirror.Length == n * n)
                for (int r = 0; r < p.h; r++) Array.Copy(p.deform, r * p.w, deformMirror, (p.z0 + r) * n + p.x0, p.w);
            MirrorRows(p, n);
            Reflex.Port.RxCore.TerrainPatchArrays.Return(p.heights);
            int W = Round32(p.w, n), H = Round32(p.h, n), X0 = Math.Min(p.x0, n - W), Z0 = Math.Min(p.z0, n - H);
            // Grid rows [Z0, Z0+H) -> Unity rows j = n-1-r, i.e. starting at n - (Z0+H); columns map 1:1.
            UploadHeights(W, H, X0, Z0, n);
            if (deformMap && p.deform != null) UploadDeform(p);
            if (p.deform != null) Reflex.Port.RxCore.TerrainPatchArrays.Return(p.deform);
        }

        // Unity's heightmap texture stores a normalized height h as h * 32766/65535 (R16_UNorm; checked 2026-10-25 with a CPU->GPU and a
        // GPU->CPU round trip). Deformation patches go straight into the GPU heightmap with TerrainHeightmapSyncControl.None: the CPU
        // path (SetHeights / SetHeightsDelayLOD + SyncHeightmap) makes Unity re-cook a PhysX heightfield for every patch even with no
        // TerrainCollider (sampled: ~21% of a race frame) and recompute the LOD error metric. Physics is librxcore's; the CPU copy of the
        // heights (Terrain.SampleHeight: the onboard camera's ground check) keeps the build-time heights, a rut of at most +-gain.
        const float HeightmapScale = 32766f / 65535f;
        readonly System.Collections.Generic.Dictionary<long, (Texture2D cpu, RenderTexture gpu)> heightScratch = new System.Collections.Generic.Dictionary<long, (Texture2D, RenderTexture)>();
        // The patch is read straight from the mirror into the scratch texture's pixels (rows flipped: texture row H-1-r is grid row Z0+r),
        // once per sample; the scratch is Bucket-sized and only its [0,W) x [0,H) corner is copied into the heightmap.
        void UploadHeights(int W, int H, int X0, int Z0, int n)
        {
            int SW = Bucket(W), SH = Bucket(H);
            long key = ((long)SW << 32) | (uint)SH;
            if (!heightScratch.TryGetValue(key, out var scratch))
            {
                if (heightScratch.Count > 64) ReleaseHeightScratch();
                scratch = (new Texture2D(SW, SH, TextureFormat.RFloat, false, true) { name = "Height patch" },
                           new RenderTexture(SW, SH, 0, Terrain.heightmapRenderTextureFormat, RenderTextureReadWrite.Linear) { name = "Height patch (heightmap format)" });
                heightScratch[key] = scratch;
            }
            var px = scratch.cpu.GetPixelData<float>(0);
            FillRows(px, mirror, W, H, X0, Z0, n, SW);
            scratch.cpu.Apply(false, false);
            Graphics.Blit(scratch.cpu, scratch.gpu);
            var previous = RenderTexture.active;
            RenderTexture.active = scratch.gpu;
            data.CopyActiveRenderTextureToHeightmap(new RectInt(0, 0, W, H), new Vector2Int(X0, n - (Z0 + H)), TerrainHeightmapSyncControl.None);
            RenderTexture.active = previous;
        }
        // The per-texel loops through pointers (no bounds checks, no NativeArray indexer): a whole-grid patch (the end-of-race replay's, 2048 x
        // 2048, every few seconds behind the results) is 4 M texels through each (w-gcspikes).
        unsafe void MirrorRows(OriginalSimulation.TerrainPatch p, int n)
        {
            float y0 = yMin, size = sizeY;
            if (p.heights.Length < p.w * p.h || mirror.Length < n * n) return;
            fixed (float* src = p.heights, dst = mirror)
                for (int r = 0; r < p.h; r++)
                {
                    float* d = dst + (p.z0 + r) * n + p.x0, s = src + r * p.w;
                    for (int c = 0; c < p.w; c++) d[c] = (s[c] - y0) / size;   // (as before: a division, the same values to the bit)
                }
        }
        static unsafe void FillRows(Unity.Collections.NativeArray<float> px, float[] m, int W, int H, int X0, int Z0, int n, int SW)
        {
            float* dst = (float*)Unity.Collections.LowLevel.Unsafe.NativeArrayUnsafeUtility.GetUnsafePtr(px);
            fixed (float* src = m)
                for (int r = 0; r < H; r++)
                {
                    float* d = dst + (H - 1 - r) * SW, s = src + (Z0 + r) * n + X0;
                    for (int c = 0; c < W; c++) d[c] = s[c] * HeightmapScale;
                }
        }

        void ReleaseHeightScratch()
        {
            foreach (var t in heightScratch.Values) { Destroy(t.cpu); t.gpu.Release(); Destroy(t.gpu); }
            heightScratch.Clear();
        }

        void BuildDeformMap(float[] grid, Material mat)
        {
            var flipped = new float[DeformSize * DeformSize];
            if (grid != null)
                for (int r = 0; r < DeformSize; r++) Array.Copy(grid, r * DeformSize, flipped, (DeformSize - 1 - r) * DeformSize, DeformSize);
            else
                for (int k = 0; k < flipped.Length; k++) flipped[k] = 0.5f; // the original's neutral deformation value
            deformMap = new Texture2D(DeformSize, DeformSize, TextureFormat.RFloat, false, true)
                { name = "Original deformation surface (live)", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear };
            deformMap.SetPixelData(flipped, 0); deformMap.Apply(false, false);
            mat.SetTexture(DeformMapId, deformMap); mat.SetFloat(UseDeformMapId, 1f);
        }

        // Copies a changed grid rect into the live texture on the GPU (no full re-upload).
        void UploadDeform(OriginalSimulation.TerrainPatch p)
        {
            int SW = Bucket(Round32(p.w, DeformSize)), SH = Bucket(Round32(p.h, DeformSize));
            long key = ((long)SW << 32) | (uint)SH;
            if (!deformScratch.TryGetValue(key, out var scratch))
            {
                if (deformScratch.Count > 64) { foreach (var t in deformScratch.Values) Destroy(t); deformScratch.Clear(); }
                scratch = new Texture2D(SW, SH, TextureFormat.RFloat, false, true) { name = "Deformation patch" };
                deformScratch[key] = scratch;
            }
            // Rows flipped straight into the texture's CPU copy (texture row 0 = grid row z0+h-1); only [0,w) x [0,h) is copied out.
            var dst = scratch.GetPixelData<float>(0);
            for (int r = 0; r < p.h; r++) Unity.Collections.NativeArray<float>.Copy(p.deform, r * p.w, dst, (p.h - 1 - r) * SW, p.w);
            scratch.Apply(false, false);
            Graphics.CopyTexture(scratch, 0, 0, 0, 0, p.w, p.h, deformMap, 0, 0, p.x0, DeformSize - (p.z0 + p.h));
        }

        // ---- the OPEN REFLEX replay (Reflex.Port.Replay.ReflexReplay, w-replayphoto2): ruts as they were at a moment ----
        /// Every patch, before it is applied (the mirrors still hold the old values).
        public static event Action<OriginalTerrain, OriginalSimulation.TerrainPatch> BeforePatch;
        /// The CPU copy of the deformation surface (grid row order, as the patches), kept with the heights' mirror.
        float[] deformMirror = Array.Empty<float>();
        public float[] HeightMirror => mirror;
        public float[] DeformMirror => deformMirror;
        public int Samples => (int)info.samples;
        public float HeightMin => yMin;
        public float HeightSize => sizeY;
        float[] replayDeformRect = Array.Empty<float>();
        /// Uploads the rect [x0, x0+w) x [z0, z0+h) of both mirrors (the replay changed them in place) to the heightmap and the
        /// deformation texture.
        public void ReplayUpload(int x0, int z0, int w, int h)
        {
            if (!terrain || w <= 0 || h <= 0) return;
            int n = (int)info.samples;
            x0 = Mathf.Clamp(x0, 0, n - 1); z0 = Mathf.Clamp(z0, 0, n - 1); w = Mathf.Min(w, n - x0); h = Mathf.Min(h, n - z0);
            int W = Round32(w, n), H = Round32(h, n), X0 = Math.Min(x0, n - W), Z0 = Math.Min(z0, n - H);
            UploadHeights(W, H, X0, Z0, n);
            if (!deformMap || deformMirror.Length != n * n) return;
            if (replayDeformRect.Length < w * h) replayDeformRect = new float[w * h];
            for (int r = 0; r < h; r++) Array.Copy(deformMirror, (z0 + r) * n + x0, replayDeformRect, r * w, w);
            UploadDeform(new OriginalSimulation.TerrainPatch { x0 = x0, z0 = z0, w = w, h = h, deform = replayDeformRect });
        }

        public void Release()
        {
            if (terrain) Destroy(terrain.gameObject);
            if (runtimeMaterial) Destroy(runtimeMaterial);
            if (deformMap) Destroy(deformMap);
            if (data) Destroy(data);
            foreach (var t in deformScratch.Values) if (t) Destroy(t);
            deformScratch.Clear(); mirror = Array.Empty<float>(); deformMirror = Array.Empty<float>(); ReleaseHeightScratch();
            terrain = null; data = null; runtimeMaterial = null; deformMap = null;
        }

        void OnDestroy() => Release();

        static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        static int ValidateInfo(ReflexTerrainInfo source)
        {
            int n = (int)source.samples;
            if (n < 2 || n > 4096 || (n & (n - 1)) != 0 && ((n - 1) & (n - 2)) != 0)
                throw new InvalidOperationException("Unexpected terrain size " + source.samples);
            if (!Finite(source.spacing) || source.spacing <= 0f || !Finite(source.gain) ||
                !Finite(source.min_x) || !Finite(source.min_z) || !Finite(source.max_x) || !Finite(source.max_z) ||
                source.max_x <= source.min_x || source.max_z <= source.min_z)
                throw new InvalidOperationException("Invalid terrain grid bounds, spacing or deformation gain.");
            return n;
        }

        // Presentation components retain their existing value type; this is a field copy, never an ABI reinterpretation.
        static ReflexTerrainInfo LegacyInfo(RxTerrainInfo source) => new ReflexTerrainInfo
        {
            samples = source.samples, stride = source.stride, spacing = source.spacing,
            min_x = source.min_x, min_z = source.min_z, max_x = source.max_x, max_z = source.max_z, gain = source.gain,
            deform_version = source.deform_version,
            dirty_x0 = source.dirty_x0, dirty_z0 = source.dirty_z0, dirty_x1 = source.dirty_x1, dirty_z1 = source.dirty_z1,
        };

        static int NextHeightmapResolution(int samples)
        {
            int res = 33;
            while (res < samples) res = (res - 1) * 2 + 1;
            return res;
        }
    }
}
