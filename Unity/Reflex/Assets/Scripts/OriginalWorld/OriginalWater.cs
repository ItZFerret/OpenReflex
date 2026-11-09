using System;
using System.Collections.Generic;
using System.Threading;
using UnityEngine;
using UnityEngine.Rendering;
using Reflex.Port.Content;

namespace Reflex.Port.OriginalWorld
{
    public static class ReflexWaterNative
    {
        /// The wake splats the game queued since the last call (vertices: x, y, strength, u, v; centres: world x, z per 6 vertices),
        /// cleared in the game as FUN_007ed300 does (plugin_api.cpp). Returns the vertex count.
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_water_splats__fn(IntPtr world, [System.Runtime.InteropServices.Out] float[] vertices, uint maxVertices, [System.Runtime.InteropServices.Out] float[] centres, uint maxCentres, [System.Runtime.InteropServices.Out] uint[] counts);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_water_splats__fn> reflex_world_water_splats__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_water_splats__fn>("reflex_world_water_splats");
        public static int reflex_world_water_splats(IntPtr world, [System.Runtime.InteropServices.Out] float[] vertices, uint maxVertices, [System.Runtime.InteropServices.Out] float[] centres, uint maxCentres, [System.Runtime.InteropServices.Out] uint[] counts) => reflex_world_water_splats__p.Get()(world, vertices, maxVertices, centres, maxCentres, counts);
    }

    // The level's water drawn as the original renderer draws it (WP-R2, _re/notes/water.md; data and CPU logic in
    // Content/ReflexWater.cs, the programs in Shaders/ReflexWater.shader "Reflex/Original Water"). One call registers it:
    // OriginalWater.Attach(level) after the level is loaded (OriginalLevel.LoadNow). Per frame, for the game camera, before it renders:
    //  - the clipmap of every water surface is placed around the camera (FUN_007e5940) and one renderer per original draw gets its mesh
    //    (the original's strip, first n primitives) and its constants (FUN_007e9490); levels 0 / 1 use the lod0 programs, the rest lod1;
    //  - the ocean height map (64 x 64 R32F) is recomputed once per game frame (FUN_007eb4a0): spectrum and time come from the running
    //    game's ocean object (read once; the game only advances it while rendering, so the time steps here with the game's frame dt);
    //    the open core builds that object without its spectrum (only the renderer reads it), so there the spectrum is Ocean.Generate's
    //    (the same Phillips spectrum from the CRT generator, other random phases) and the steps follow the presented game frames;
    //  - river UV scroll (surface +0x48c += dt * FlowRate, wrapped) likewise;
    //  - the wake simulation (512 x 512, three R32F targets) steps once per game frame (FUN_007ed300), with the pressure splats the
    //    game queued on that frame (the vehicles in the water): from the legacy world (reflex_world_water_splats) or the open core
    //    (rx_session_water_splats, w-water: the session takes the queue at each frame's render boundary as 007ed300 does).
    // Read only on the guest (reflex_world_read / reflex_world_find_object between frames): the bike state hashes are unchanged.
    [DisallowMultipleComponent]
    public sealed class OriginalWater : MonoBehaviour
    {
        // elapsed milliseconds since a Stopwatch.GetTimestamp() (no Stopwatch object a frame, w-gcspikes)
        static double Ms(long since) => (System.Diagnostics.Stopwatch.GetTimestamp() - since) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
        public OriginalLevel level;
        public OriginalWorldRuntime runtime;
        [Tooltip("Camera the clipmap follows and the water is drawn for (the runtime's chase camera / Camera.main when empty).")]
        public Camera targetCamera;
        [Tooltip("Read the ocean spectrum / time and the wake splats from the running game (else a spectrum from the CRT generator).")]
        public bool useGameState = true;

        [Header("Result (read only)")]
        public int surfaces; public int drawsLastFrame; public float oceanTime; public float gameClock; public bool oceanFromGame;
        public int WakeSteps => wakeSteps; public int SplatsReceived => splatsReceived; public double Milliseconds => milliseconds;
        /// The wake steps' CPU cost per Unity frame (ms, smoothed): the command buffer, the splats' mesh.
        public double WakeMilliseconds => wakeMilliseconds;
        [TextArea(2, 6)] public string report;

        public const int WakeSize = 512;

        /// Registers the water of a loaded level (no-op on dry levels). Main thread.
        public static OriginalWater Attach(OriginalLevel lvl)
        {
            try
            {
                if (!lvl || lvl.Content == null) return null;
                var water = ReflexWater.Load(lvl.Content);
                if (water.surfaces.Count == 0) return null;
                var w = lvl.GetComponent<OriginalWater>() ?? lvl.gameObject.AddComponent<OriginalWater>();
                w.level = lvl; w.runtime = lvl.runtime;
                w.Begin(water);
                return w;
            }
            catch (Exception e) { Debug.LogWarning("OriginalWater: " + e.Message + "\n" + e.StackTrace, lvl); return null; }   // never stops the level load
        }

        // ------------------------------------------------------------------------------------------------ state

        sealed class SurfaceState
        {
            public ReflexWater.Surface data;
            public ReflexWater.Clipmap clipmap;
            public Material lod0, lod1;
            public Texture2D desc, flow;
            public float flowOffset;                       // surface +0x48c
            public uint gameAddress;                       // the XenonWaterAsset's surface (asset + 0x10) in the running game
            public Vector4[] world = new Vector4[4];
        }
        sealed class Piece { public GameObject go; public MeshFilter filter; public MeshRenderer renderer; public MaterialPropertyBlock block = new MaterialPropertyBlock(); }

        ReflexWater water;
        readonly List<SurfaceState> states = new List<SurfaceState>();
        readonly List<Piece> pool = new List<Piece>();
        readonly Dictionary<long, Mesh> meshes = new Dictionary<long, Mesh>();
        readonly List<UnityEngine.Object> created = new List<UnityEngine.Object>();
        Shader shader;
        Texture2D oceanTexture;
        RenderTexture[] wake = new RenderTexture[3]; int wakeCurrent; Material wakeMaterial; CommandBuffer wakeCommands; Vector2 wakeSnap, wakeShiftPrev;
        int wakeSteps, splatsReceived; double milliseconds, wakeMilliseconds; readonly Mesh[] splatMeshes = new Mesh[4];
        ReflexWater.Ocean ocean; bool oceanDirty = true;
        // w-rng: the open core's ocean seed (rx_session_rng: the level loader stream's state before Ocean_InitSpectrum's draws): 0 not asked,
        // 1 asked, 2 the core's seed, 3 none (an older core / no ocean drawn: the generator's default)
        volatile int coreOceanSeedState; uint coreOceanSeed;
        float lastClock = float.NaN; bool frameAdvanced; int pendingSteps;
        long stepFrame = long.MaxValue;   // the game frame the newest pending wake step stands for (open core: floor(PresentedGameFrame))
        double lastGameFrame = double.NaN; float lastGameTime;   // the open path's last stepped game frame / clock
        Transform root;

        static readonly int VertexOffsetId = Shader.PropertyToID("gVertexOffset"), UVOffsetId = Shader.PropertyToID("gUVOffset"),
            MeshResolutionId = Shader.PropertyToID("gMeshResolution"), TextureWidthId = Shader.PropertyToID("gTextureWidth"),
            TilingRateId = Shader.PropertyToID("gTilingRate"), UVMultiplierId = Shader.PropertyToID("gUVMultiplier"),
            TileWidthId = Shader.PropertyToID("gTileWidth"), FlowRateId = Shader.PropertyToID("gFlowRate"), RiverHeightId = Shader.PropertyToID("gRiverHeightValue"),
            WorldId = Shader.PropertyToID("gWorld"), RotationId = Shader.PropertyToID("gRotationMatrix"),
            OceanMapId = Shader.PropertyToID("OceanMapVertSampler"), OceanWaveId = Shader.PropertyToID("OceanWaveSampler"),
            WakeVertId = Shader.PropertyToID("WakeVertSampler"), WakeId = Shader.PropertyToID("WakeSampler"),
            DescId = Shader.PropertyToID("DescVertSampler"), UVMapId = Shader.PropertyToID("UVVertSampler"), ReflectionId = Shader.PropertyToID("ReflectionSampler"),
            TexLastId = Shader.PropertyToID("TexLast"), TexLastLastId = Shader.PropertyToID("TexLastLast"), TexMaskId = Shader.PropertyToID("TexMask"),
            SimShiftId = Shader.PropertyToID("g_SimShift"), PressureTexId = Shader.PropertyToID("pressure_texture");

        void Begin(ReflexWater w)
        {
            Clear();
            water = w;
            shader = Shader.Find("Reflex/Original Water");
            if (!shader) { report = "shader Reflex/Original Water not found (Always Included Shaders)"; Debug.LogWarning("OriginalWater: " + report, this); return; }
            var rootGo = new GameObject("Original water") { hideFlags = HideFlags.DontSave };
            root = rootGo.transform; root.SetParent(transform, false); created.Add(rootGo);
            oceanTexture = new Texture2D(ReflexWater.Ocean.N, ReflexWater.Ocean.N, TextureFormat.RFloat, false, true)
                { name = "Ocean height map", wrapMode = TextureWrapMode.Repeat, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            created.Add(oceanTexture);
            for (int i = 0; i < 3; i++)
            {
                wake[i] = new RenderTexture(WakeSize, WakeSize, 0, RenderTextureFormat.RFloat, RenderTextureReadWrite.Linear)
                    { name = "Water wake " + i, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, useMipMap = false, hideFlags = HideFlags.DontSave };
                wake[i].Create(); created.Add(wake[i]);
                var prev = RenderTexture.active; RenderTexture.active = wake[i]; GL.Clear(false, true, Color.clear); RenderTexture.active = prev;
            }
            wakeMaterial = new Material(shader) { hideFlags = HideFlags.DontSave }; created.Add(wakeMaterial);
            var content = level.Content;
            Cubemap levelReflection = LoadCube(content, water.reflectionMap);
            Texture2D pressure = LoadTexture(content, "WaterPressure", TextureWrapMode.Clamp);
            if (pressure) { wakeMaterial.SetTexture(TexMaskId, pressure); wakeMaterial.SetTexture(PressureTexId, pressure); }
            var sb = new System.Text.StringBuilder();
            foreach (var s in water.surfaces)
            {
                var st = new SurfaceState { data = s, clipmap = new ReflexWater.Clipmap(s.asset.sizeX, s.asset.sizeZ) };
                var m = s.asset.matrix;
                for (int r = 0; r < 4; r++) st.world[r] = new Vector4(m[r], m[4 + r], m[8 + r], m[12 + r]);   // D3D rows -> Unity rows (c64..c67)
                // MAP SCALE (core/plan/MAP_SCALE.md): the surface drawn in the scaled world (S x gWorld: its rows x, y, z times the scale);
                // the clipmap below keeps working in the asset's own space with the camera brought back by the scale
                float ms = level ? level.MapScale : 1f;
                if (ms != 1f) for (int r = 0; r < 3; r++) st.world[r] *= ms;
                if (s.asset.desc != null)
                {
                    st.desc = new Texture2D(s.asset.descW, s.asset.descH, TextureFormat.BGRA32, false, true)
                        { name = s.waterAsset + " description", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
                    st.desc.SetPixelData(s.asset.desc, 0); st.desc.Apply(false, true); created.Add(st.desc);   // D3D row order: the programs' uv as is
                }
                if (s.asset.flow != null)
                {
                    st.flow = new Texture2D(s.asset.flowW, s.asset.flowH, TextureFormat.RGHalf, false, true)
                        { name = s.waterAsset + " flow", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
                    st.flow.SetPixelData(s.asset.flow, 0); st.flow.Apply(false, true); created.Add(st.flow);
                }
                string family = s.family == "Water_Ocean" ? "_WATER_OCEAN" : s.family == "Water_Pool" ? "_WATER_POOL" : "_WATER_WATER";
                st.lod0 = MakeMaterial(st, family, false, levelReflection);
                st.lod1 = MakeMaterial(st, family, true, levelReflection);
                states.Add(st);
                sb.Append(s.name).Append(' ').Append(s.waterAsset).Append(' ').Append(s.family).Append(s.River ? " river" : " lake").Append(' ')
                  .Append(st.clipmap.count).Append(" levels; ");
            }
            surfaces = states.Count;
            report = $"{states.Count} water surfaces: {sb}" + (water.warnings.Count > 0 ? "\nwarnings: " + string.Join("; ", water.warnings) : "");
            RenderPipelineManager.beginCameraRendering -= OnBegin;
            RenderPipelineManager.beginCameraRendering += OnBegin;
            boundSim = null; requested = false;
        }

        /// LIGHTING: ENHANCED (Rendering/ReflexEnhancedLighting): the level's reflection cube replaced by the enhanced sky's cube;
        /// null puts the original cube back. The ORIGINAL mode never calls it.
        public void SetReflectionOverride(Texture cube)
        {
            foreach (var st in states)
                foreach (var m in new[] { st.lod0, st.lod1 })
                {
                    if (!m) continue;
                    if (!originalReflection.ContainsKey(m)) originalReflection[m] = m.GetTexture(ReflectionId);
                    var t = cube ? cube : originalReflection[m];
                    if (t) m.SetTexture(ReflectionId, t);
                }
        }
        readonly Dictionary<Material, Texture> originalReflection = new Dictionary<Material, Texture>();
        /// The level's reflection cube the water samples (ReflectionSampler; null on a dry level).
        public Texture LevelReflection
        {
            get
            {
                foreach (var st in states) if (st.lod0) return originalReflection.TryGetValue(st.lod0, out var t) ? t : st.lod0.GetTexture(ReflectionId);
                return null;
            }
        }

        Material MakeMaterial(SurfaceState st, string family, bool lod1, Cubemap levelReflection)
        {
            var s = st.data;
            var m = new Material(shader) { name = s.materialAsset + (lod1 ? " lod1" : " lod0"), hideFlags = HideFlags.DontSave };
            m.EnableKeyword(family);
            if (s.River) m.EnableKeyword("_WATER_RIVER");
            if (lod1) m.EnableKeyword("_WATER_LOD1");
            foreach (var kv in s.parameters)
                if (kv.Value != null && kv.Value.Length >= 4 && m.HasProperty(kv.Key)) m.SetVector(kv.Key, new Vector4(kv.Value[0], kv.Value[1], kv.Value[2], kv.Value[3]));
            m.SetVectorArray(WorldId, st.world);
            m.SetVector(RiverHeightId, new Vector4(s.asset.heightScale, s.asset.heightOffset, 0f, 0f));   // c213 (surface +0x490 / +0x494)
            if (st.desc) m.SetTexture(DescId, st.desc);
            if (st.flow) m.SetTexture(UVMapId, st.flow);
            m.SetTexture(OceanMapId, oceanTexture); m.SetTexture(OceanWaveId, oceanTexture);
            m.SetTexture(WakeVertId, wake[0]); m.SetTexture(WakeId, wake[0]);
            if (levelReflection) m.SetTexture(ReflectionId, levelReflection);
            // material texture slots = pixel sampler registers (s0, s1): foam (Water), static / cube reflection (Water_Ocean), ripple map /
            // cube reflection (Water_Pool)
            var content = level.Content;
            string[] slots = s.family == "Water_Ocean" ? new[] { "usr_sampler_static_reflection", "usr_sampler_cube_reflection" }
                           : s.family == "Water_Pool" ? new[] { "usr_sampler_ripple_map", "usr_sampler_cube_reflection" }
                           : new[] { "usr_sampler_foam" };
            for (int i = 0; i < slots.Length && i < s.textures.Count; i++)
            {
                if (string.IsNullOrEmpty(s.textures[i])) continue;
                if (slots[i].EndsWith("cube_reflection", StringComparison.Ordinal)) { var c = LoadCube(content, s.textures[i]); if (c) m.SetTexture(slots[i], c); }
                else { var t = LoadTexture(content, s.textures[i], TextureWrapMode.Repeat); if (t) m.SetTexture(slots[i], t); }
            }
            created.Add(m);
            return m;
        }

        static Texture2D LoadTextureStatic(ReflexLevelContent content, string name) => content.HasAsset(name, "texture") ? ReflexTex.ToTexture(ReflexTex.Parse(name, content.ReadAsset(name, "texture")), TextureWrapMode.Clamp, TextureWrapMode.Clamp, FilterMode.Trilinear) : null;   // tests

        readonly Dictionary<string, Texture> textureCache = new Dictionary<string, Texture>(StringComparer.Ordinal);
        Texture2D LoadTexture(ReflexLevelContent content, string name, TextureWrapMode wrap)
        {
            if (string.IsNullOrEmpty(name)) return null;
            if (textureCache.TryGetValue(name, out var t)) return t as Texture2D;
            Texture2D r = null;
            try { if (content.HasAsset(name, "texture")) r = ReflexTex.ToTexture(ReflexTex.Parse(name, content.ReadAsset(name, "texture")), wrap, wrap, FilterMode.Trilinear); }
            catch (Exception e) { Debug.LogWarning($"OriginalWater: texture {name}: {e.Message}"); }
            if (r) created.Add(r);
            textureCache[name] = r; return r;
        }
        Cubemap LoadCube(ReflexLevelContent content, string name)
        {
            if (string.IsNullOrEmpty(name)) return null;
            if (textureCache.TryGetValue(name, out var t)) return t as Cubemap;
            Cubemap r = null;
            try { if (content.HasAsset(name, "texture")) r = ToCube(ReflexWater.ReadCube(name, content.ReadAsset(name, "texture"))); }
            catch (Exception e) { Debug.LogWarning($"OriginalWater: cube {name}: {e.Message}"); }
            if (r) created.Add(r);
            textureCache[name] = r; return r;
        }

        /// The reflection cube (ReflexWater.ReadCube: all six faces' level tables precede the face data) as a Unity cubemap; the
        /// faces' texels as stored (D3D face and row order sample identically in Unity).
        static Cubemap ToCube(ReflexWater.Cube t)
        {
            if (t == null || !SystemInfo.SupportsTextureFormat(t.format)) return null;
            var c = new Cubemap(t.size, t.format, t.levels) { name = t.name, filterMode = FilterMode.Trilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            for (int f = 0; f < 6; f++) for (int l = 0; l < t.levels; l++) c.SetPixelData(t.faces[f][l], l, (CubemapFace)f);
            c.Apply(false, true);
            return c;
        }

        void Clear()
        {
            RenderPipelineManager.beginCameraRendering -= OnBegin;
            foreach (var o in created) if (o) { if (Application.isPlaying) Destroy(o); else DestroyImmediate(o); }
            created.Clear(); states.Clear(); pool.Clear(); textureCache.Clear();
            foreach (var m in meshes.Values) if (m) { if (Application.isPlaying) Destroy(m); else DestroyImmediate(m); }
            meshes.Clear();
            wakeCommands?.Release(); wakeCommands = null;
            ocean = null; coreOceanSeedState = 0; lastClock = float.NaN; lastGameFrame = double.NaN; wakeSnap = Vector2.zero; wakeShiftPrev = Vector2.zero; pendingSteps = 0; lock (gate) { batches.Clear(); }
            boundOpen = null; openSplatFrame = 0; Interlocked.Exchange(ref openQueued, 0); stepFrame = long.MaxValue;
        }

        void OnDestroy() => Clear();
        void OnDisable() { foreach (var p in pool) if (p.go) p.go.SetActive(false); }

        // ------------------------------------------------------------------------------------------------ the running game

        OriginalSimulation boundSim; bool requested; int queued;
        Action<IntPtr> collect;
        readonly object gate = new object();
        // simulation thread -> main thread
        byte[] oceanBlock; float oceanBlockTime; bool oceanBlockFresh;
        float collectedClock, collectedDt; bool hasClock;
        readonly Dictionary<int, (uint surface, float flow)> collectedSurfaces = new Dictionary<int, (uint, float)>();
        // the game's splats not drawn yet, one batch per game frame (frame = rx_session_frames of the open core's frame it was queued
        // on, drawn by the wake step of that frame; long.MaxValue for the legacy world, whose splats go into the next step)
        sealed class SplatBatch { public long frame; public float[] vertices, centres; }
        readonly List<SplatBatch> batches = new List<SplatBatch>();
        const int MaxBatches = 64;
        readonly float[] splatBuffer = new float[0x5a0], centreBuffer = new float[2 * 0x30]; readonly uint[] splatCounts = new uint[2];
        bool splatExport = true;
        // the open core (rx_session_water_splats): the frames read so far, per bound simulation
        Reflex.Port.RxCore.OpenReflexSimulation boundOpen; ulong openSplatFrame; int openQueued; bool openSplatExport = true;
        Action<IntPtr> collectOpen;
        readonly float[] openVertices = new float[0x5a0 * 8], openCentres = new float[2 * 0x30 * 8];
        Reflex.Port.RxCore.RxWaterSplats openInfo;
        void AddBatch(long frame, float[] v, int nv, float[] c, int nc, int vo = 0, int co = 0)
        {
            var b = new SplatBatch { frame = frame, vertices = new float[nv * 5], centres = new float[nc * 2] };
            Array.Copy(v, vo * 5, b.vertices, 0, nv * 5); Array.Copy(c, co * 2, b.centres, 0, nc * 2);
            lock (gate) { batches.Add(b); if (batches.Count > MaxBatches) batches.RemoveAt(0); splatsReceived += nc; }
        }
        /// The open core's splats since the last read (on the simulation worker, between frames).
        void CollectOpen(IntPtr session)
        {
            try
            {
                for (int guard = 0; guard < 8; guard++)
                {
                    if (!Reflex.Port.RxCore.RxWaterNative.Read(session, openSplatFrame, openVertices, openCentres, ref openInfo)) break;
                    int vo = 0, co = 0;
                    for (int k = 0; k < openInfo.frames; k++)
                    {
                        int nv = (int)openInfo.frameVertices[k], nc = (int)openInfo.frameCentres[k];
                        AddBatch((long)openInfo.frame[k], openVertices, nv, openCentres, nc, vo, co);
                        vo += nv; co += nc;
                    }
                    openSplatFrame = openInfo.lastFrame;
                    if (openInfo.truncated == 0) break;   // more frames than the buffers hold: read on
                }
            }
            catch (EntryPointNotFoundException) { openSplatExport = false; }   // an older core: no wake splats
            catch (Exception e) { Debug.LogWarning("OriginalWater: " + e.Message); }
            finally { Interlocked.Exchange(ref openQueued, 0); }
        }

        void Collect(IntPtr w)
        {
            try
            {
                var b4 = new byte[4];
                uint U(uint a) => ReflexWorldNative.reflex_world_read(w, a, b4, 4) == 0 ? BitConverter.ToUInt32(b4, 0) : 0u;
                float F(uint a) => ReflexWorldNative.reflex_world_read(w, a, b4, 4) == 0 ? BitConverter.ToSingle(b4, 0) : 0f;
                const uint Engine = 0xd728d0;                    // Engine (DAT_00a95844 +0x10): +0x18 the frame's dt, +0x3c the game clock
                float clock = F(Engine + 0x3c), dt = F(Engine + 0x18);
                if (!requested)
                {
                    uint oc = U(0xa9578c);                       // the shared ocean object (0x14080 bytes)
                    byte[] block = null; float t = 0f;
                    if (oc != 0) { block = new byte[0x14080]; if (ReflexWorldNative.reflex_world_read(w, oc, block, (uint)block.Length) != 0) block = null; else t = BitConverter.ToSingle(block, 0x14018); }
                    var found = new Dictionary<int, (uint, float)>();
                    foreach (var st in states)
                    {
                        uint e = ReflexWorldNative.reflex_world_find_object(w, 0x9729dc, 0x14, (uint)st.data.entityId, 0x00400000, 0x7fff0000);   // WaterEntity, +0x14 id
                        if (e == 0) continue;
                        uint asset = U(U(e + 0x90) + 0x44);
                        if (asset == 0 || U(asset) != 0x972a50) continue;   // XenonWaterAsset
                        uint surface = asset + 0x10;
                        found[st.data.entityId] = (surface, F(surface + 0x48c));
                    }
                    lock (gate) { oceanBlock = block; oceanBlockTime = t; oceanBlockFresh = true; foreach (var kv in found) collectedSurfaces[kv.Key] = kv.Value; }
                    requested = true;
                }
                if (splatExport)
                {
                    int nv = ReflexWaterNative.reflex_world_water_splats(w, splatBuffer, (uint)(splatBuffer.Length / 5), centreBuffer, (uint)(centreBuffer.Length / 2), splatCounts);
                    if (nv > 0) AddBatch(long.MaxValue, splatBuffer, nv, centreBuffer, (int)splatCounts[1]);
                }
                lock (gate) { collectedClock = clock; collectedDt = dt; hasClock = true; }
            }
            catch (EntryPointNotFoundException) { splatExport = false; }   // an older plugin: no wake splats
            catch (Exception e) { Debug.LogWarning("OriginalWater: " + e.Message); }
            finally { Interlocked.Exchange(ref queued, 0); }
        }

        /// Tests: the river UV scroll of a recorded frame (surface +0x48c) for the surface with this entity id (all when 0).
        public void SetFlowOffset(int entityId, float offset) { foreach (var st in states) if (entityId == 0 || st.data.entityId == entityId) st.flowOffset = offset; }

        /// Tests: a recorded wake target sampled instead of the simulated one (null: the simulation).
        [NonSerialized] public Texture wakeOverride;
        /// Tests: the wake target the water samples now (512 x 512 R32F, the newest step).
        public Texture CurrentWake => wakeOverride ? wakeOverride : wake[wakeCurrent];

        int SetCoreSeed(uint seed) { coreOceanSeed = seed; return 2; }

        /// Tests: the ocean object of a recorded frame (bytes of DAT_00a9578c's object) and its time.
        public void SetOcean(ReflexWater.Ocean o) { ocean = o; oceanDirty = true; oceanFromGame = true; }

        void Update() => Tick();

        /// Per Unity frame (Update; tests call it directly): the running game's frame counter / dt, the ocean, flows.
        public void Tick()
        {
            if (states.Count == 0) return;
            long watch = System.Diagnostics.Stopwatch.GetTimestamp();
            try { TickNow(); } finally { milliseconds = milliseconds == 0 ? Ms(watch) : milliseconds * 0.95 + Ms(watch) * 0.05; }
        }

        void TickNow()
        {
            if (!runtime) runtime = level && level.runtime ? level.runtime : FindAnyObjectByType<OriginalWorldRuntime>();
            var sim = useGameState && runtime && runtime.Running ? runtime.Simulation : null;
            if (sim != boundSim) { boundSim = sim; requested = false; Interlocked.Exchange(ref queued, 0); lock (gate) { hasClock = false; } lastClock = float.NaN; }
            collect ??= Collect;
            if (sim != null && Interlocked.Exchange(ref queued, 1) == 0) sim.BetweenFrames(collect);
            var openSim = sim == null && useGameState && runtime && runtime.Running ? runtime.SimDriver?.Open : null;
            if (openSim != boundOpen) { boundOpen = openSim; openSplatFrame = 0; Interlocked.Exchange(ref openQueued, 0); lock (gate) { batches.Clear(); } }
            collectOpen ??= CollectOpen;
            if (openSim != null && openSplatExport && Interlocked.Exchange(ref openQueued, 1) == 0) openSim.BetweenFrames(collectOpen);

            float clock, dt; bool clockValid; byte[] block = null; float blockTime = 0;
            lock (gate)
            {
                clock = collectedClock; dt = collectedDt; clockValid = hasClock;
                if (oceanBlockFresh) { block = oceanBlock; blockTime = oceanBlockTime; oceanBlockFresh = false; }
                foreach (var st in states) if (collectedSurfaces.TryGetValue(st.data.entityId, out var v) && st.gameAddress != v.surface) { st.gameAddress = v.surface; st.flowOffset = v.flow; }
            }
            if (block != null) { ocean = ReflexWater.Ocean.FromGame(block, 0, blockTime); oceanFromGame = true; oceanDirty = true; lastClock = clock; }
            if (ocean == null && (sim == null || !useGameState))
            {
                var open = sim == null && runtime && runtime.Running ? runtime.SimDriver : null;
                if (open != null && open.Open != null)
                {   // the open core: the game's own random phases, from the core's ocean seed (w-rng)
                    if (coreOceanSeedState == 0)
                    {
                        coreOceanSeedState = 1;
                        open.BetweenFrames(session =>
                        {
                            try { coreOceanSeedState = Reflex.Port.RxCore.RxRngNative.Read(session, out var st) && st.oceanValid ? SetCoreSeed(st.oceanSeed) : 3; }
                            catch (EntryPointNotFoundException) { coreOceanSeedState = 3; }
                        });
                    }
                    if (coreOceanSeedState >= 2) { ocean = ReflexWater.Ocean.Generate(coreOceanSeedState == 2 ? coreOceanSeed : 1u); oceanDirty = true; }
                }
                else { ocean = ReflexWater.Ocean.Generate(); oceanDirty = true; }
            }

            // game frames since the last Unity frame: the original renders (and steps the ocean, flows and wake) once per game frame
            // (the game clock +0x3c advances by the frame dt +0x18 per game frame; the time the frames since the last Unity frame covered
            // steps the ocean and the flows, one wake step per frame)
            int steps = 0; float advance = 0f;   // wake steps (whole game frames) and the time the ocean / flows move on
            if (sim != null && clockValid)
            {
                if (!float.IsNaN(lastClock) && clock > lastClock && dt > 0f) { float covered = clock - lastClock; steps = Mathf.Clamp(Mathf.RoundToInt(covered / dt), 1, 8); dt = covered / steps; advance = covered; }
                lastClock = clock;
            }
            else if (sim == null && runtime && runtime.Running)
            {
                // the open core (no legacy simulation to read): the game frames presented since the last step (OriginalWorldRuntime.
                // PresentedGameFrame, PresentedGameTime = the game's clock of that frame), as the original steps once per frame it
                // draws; nothing while no game frame advances (pause). Stepping every Unity frame instead ran the wake 2x (120 Hz) to
                // 5x+ (uncapped) faster than the game. The wake steps once per whole game frame; the ocean and the river flow follow the
                // presented clock itself (w-water): with FRAME PACING INTERPOLATED the frame on screen is a blend of two game frames
                // and its clock moves on every Unity frame, so the waves and the flow move with the interpolated world instead of
                // jumping one or two game frames at a time (the "choppy" water at 30-40 fps). ORIGINAL pacing presents whole frames:
                // the same steps as before.
                double f = runtime.PresentedGameFrame; float gt = runtime.PresentedGameTime;
                if (double.IsNaN(f)) { }
                else if (double.IsNaN(lastGameFrame) || f < lastGameFrame || gt < lastGameTime) { lastGameFrame = f; lastGameTime = gt; }
                else
                {
                    int n = (int)(Math.Floor(f) - Math.Floor(lastGameFrame));
                    if (n > 0) { steps = Math.Min(n, 8); lastGameFrame = f; stepFrame = (long)Math.Floor(f); }
                    if (gt > lastGameTime) { advance = gt - lastGameTime; dt = steps > 1 ? advance / steps : advance; lastGameTime = gt; }
                }
            }
            else if (sim == null && Application.isPlaying) { steps = 1; dt = Time.deltaTime; advance = dt; }
            if (sim != null || !(runtime && runtime.Running)) stepFrame = long.MaxValue;   // legacy world / no game: splats go into the next step
            frameAdvanced = steps > 0; pendingSteps += steps;
            if (advance > 0f)
            {   // as `steps` frames of dt: the time of all but the last, then FUN_007eb4a0 for the last (the map of its start time)
                if (ocean != null) { if (advance > dt) ocean.time = ocean.time + (advance - dt) > 86400f ? 0f : ocean.time + (advance - dt); ocean.Update(dt); oceanDirty = true; }
                foreach (var st in states) { float fl = advance * st.data.flowRate + st.flowOffset; st.flowOffset = fl >= 1f ? fl - Mathf.Floor(fl) : fl; }
            }
            if (ocean != null && oceanDirty && steps == 0 && !Application.isPlaying) { ocean.Update(0f); }   // edit mode / tests: the frame of the given time
            if (ocean != null && oceanDirty) { oceanTexture.SetPixelData(ocean.heights, 0); oceanTexture.Apply(false, false); oceanDirty = false; }
            if (ocean != null) oceanTime = ocean.time;
            gameClock = clock;
        }

        // ------------------------------------------------------------------------------------------------ drawing

        Camera Target => targetCamera ? targetCamera : runtime && runtime.chaseCamera ? runtime.chaseCamera : Camera.main;

        void OnBegin(ScriptableRenderContext context, Camera cam)
        {
            if (states.Count == 0 || cam != Target) return;
            long wakeWatch = System.Diagnostics.Stopwatch.GetTimestamp();
            if (pendingSteps > 0) { StepWake(context, cam, Math.Min(pendingSteps, 4)); pendingSteps = 0; frameAdvanced = false; }
            wakeMilliseconds = wakeMilliseconds * 0.95 + Ms(wakeWatch) * 0.05;
            var screen = new Vector2(cam.pixelWidth, cam.pixelHeight);
            Vector3 cp = cam.transform.position;
            var rotation = ReflexWater.WakeProjection(cp, WakeSize);
            int used = 0;
            foreach (var st in states)
            {
                var cm = st.clipmap; cm.Update(level && level.MapScale != 1f ? cp / level.MapScale : cp, st.data.asset.matrix);   // MAP SCALE: in the level's own feet
                foreach (var m in new[] { st.lod0, st.lod1 })
                {
                    m.SetVectorArray(RotationId, rotation);
                    m.SetVector(TileWidthId, cm.TileWidth());
                    m.SetVector(FlowRateId, new Vector4(st.flowOffset, 0f, 0f, 0f));
                    Texture wt = wakeOverride ? wakeOverride : wake[wakeCurrent];
                    m.SetTexture(WakeVertId, wt); m.SetTexture(WakeId, wt);
                }
                foreach (var d in cm.Draws())
                {
                    cm.DrawConstants(d.level, d.piece, screen, WakeSize, out var c201, out var c202, out var c204, out var c191, out var c190, out var c205);
                    var p = PieceAt(used++);
                    p.filter.sharedMesh = MeshFor(d.piece, d.buffer, d.prims);
                    p.renderer.sharedMaterial = d.level <= 1 ? st.lod0 : st.lod1;
                    p.block.Clear();
                    p.block.SetVector(VertexOffsetId, c201); p.block.SetVector(UVOffsetId, c202); p.block.SetVector(MeshResolutionId, c204);
                    p.block.SetVector(TextureWidthId, c191); p.block.SetVector(TilingRateId, c190); p.block.SetVector(UVMultiplierId, c205);
                    p.renderer.SetPropertyBlock(p.block);
                    if (!p.go.activeSelf) p.go.SetActive(true);
                }
            }
            for (int i = used; i < pool.Count; i++) if (pool[i].go.activeSelf) pool[i].go.SetActive(false);
            drawsLastFrame = used;
        }

        Piece PieceAt(int i)
        {
            while (pool.Count <= i)
            {
                var go = new GameObject("water draw " + pool.Count) { hideFlags = HideFlags.DontSave };
                go.transform.SetParent(root, false);
                var p = new Piece { go = go, filter = go.AddComponent<MeshFilter>(), renderer = go.AddComponent<MeshRenderer>() };
                p.renderer.shadowCastingMode = ShadowCastingMode.Off; p.renderer.receiveShadows = false;
                p.renderer.lightProbeUsage = LightProbeUsage.Off; p.renderer.reflectionProbeUsage = ReflectionProbeUsage.Off;
                p.renderer.allowOcclusionWhenDynamic = false;
                pool.Add(p); created.Add(go);
            }
            return pool[i];
        }

        /// The original's vertex buffer of the piece with the first prims primitives of its strip (as a triangle list).
        Mesh MeshFor(int piece, int buffer, int prims)
        {
            long key = ((long)piece << 40) | ((long)buffer << 32) | (uint)prims;
            if (meshes.TryGetValue(key, out var m) && m) return m;
            var b = ReflexWater.Buffers;
            var v = b.vertices[piece];
            var pos = new Vector3[v.Length]; for (int i = 0; i < v.Length; i++) pos[i] = new Vector3(v[i].x, v[i].y, 0f);
            var strip = buffer == 0 ? b.inner : buffer == 1 ? b.rows : buffer == 2 ? b.columns : b.columnsOdd;
            m = new Mesh { name = $"water p{piece} b{buffer} {prims}", hideFlags = HideFlags.DontSave, indexFormat = IndexFormat.UInt16 };
            m.vertices = pos;
            m.SetTriangles(ReflexWater.StripToTriangles(strip, prims), 0, false);
            m.bounds = new Bounds(Vector3.zero, Vector3.one * 1e6f);   // the program moves the grid anywhere over the water
            meshes[key] = m;
            return m;
        }

        // FUN_007ed300: one wake step per game frame. The target follows the camera snapped to whole metres (x87 fmod 1); g_SimShift c200 =
        // (-dx, -dz, -(dx + dx'), -(dz + dz')) with (dx, dz) this step's move of the snap in units of the 512 m target and (dx', dz') the
        // previous step's (+0x3b0 the last snap, +0x398 / +0x39c the last move; both start at 0). TexLast = the current target,
        // TexLastLast = the one before, written into the third (a full-target quad), then the pressure splats the game queued since
        // the last step into the same target (triangle list; each splat's vertices offset by its centre minus the snap, 256 m = 1).
        void StepWake(ScriptableRenderContext context, Camera cam, int steps)
        {
            Vector3 cp = cam.transform.position;
            var snap = new Vector2((float)(cp.x - (cp.x % 1.0)), (float)(cp.z - (cp.z % 1.0)));
            wakeCommands ??= new CommandBuffer { name = "Water wake" };
            wakeCommands.Clear();
            for (int s = 0; s < steps; s++)
            {
                var d = s == 0 ? (snap - wakeSnap) / WakeSize : Vector2.zero;
                int last = wakeCurrent, lastLast = (wakeCurrent + 1) % 3, target = (wakeCurrent + 2) % 3;
                var block = new MaterialPropertyBlock();
                block.SetTexture(TexLastId, wake[last]); block.SetTexture(TexLastLastId, wake[lastLast]);
                block.SetVector(SimShiftId, new Vector4(-d.x, -d.y, -(d.x + wakeShiftPrev.x), -(d.y + wakeShiftPrev.y)));
                wakeCommands.SetRenderTarget(wake[target]);
                wakeCommands.DrawProcedural(Matrix4x4.identity, wakeMaterial, 1, MeshTopology.Triangles, 3, 1, block);
                wakeShiftPrev = d;
                // this step's game frame: the newest pending step is stepFrame, the ones before it the frames before (FUN_007ed300 draws
                // the splats queued since the previous step); a batch of a frame not on screen yet waits for its step
                long upto = stepFrame == long.MaxValue ? (s == steps - 1 ? long.MaxValue : long.MinValue) : stepFrame - (steps - 1 - s);
                var mesh = BuildSplats(snap, upto, s);
                if (mesh) wakeCommands.DrawMesh(mesh, Matrix4x4.identity, wakeMaterial, 0, 2);
                wakeCurrent = target;
            }
            wakeSnap = snap;
            context.ExecuteCommandBuffer(wakeCommands);
            wakeSteps += steps;
        }

        // The splats taken from the game (reflex_world_water_splats / rx_session_water_splats) of the frames up to `upto`, as a mesh of
        // (x, y, strength) positions and (u, v), placed like 007ed300 (each splat's vertices moved by its centre minus the snap, 256 = 1).
        readonly List<Vector3> splatPos = new List<Vector3>(); readonly List<Vector2> splatUv = new List<Vector2>(); readonly List<int> splatIdx = new List<int>();
        Mesh BuildSplats(Vector2 snap, long upto, int slot)
        {
            const float half = WakeSize / 2;
            splatPos.Clear(); splatUv.Clear(); splatIdx.Clear();
            lock (gate)
            {
                for (int b = 0; b < batches.Count; b++)
                {
                    var batch = batches[b];
                    if (batch.frame > upto)
                    {   // a frame far ahead of the one on screen (a restarted / rewound presentation) would never get its step
                        if (batch.frame != long.MaxValue && upto != long.MinValue && batch.frame > upto + 3600) batches.RemoveAt(b--);
                        continue;
                    }
                    batches.RemoveAt(b--);
                    var v = batch.vertices; var c = batch.centres; int n = v.Length / 5;
                    for (int i = 0; i < n; i++)
                    {
                        int k = (i * 5) / 30; float cx = 2 * k + 1 < c.Length ? c[2 * k] : snap.x, cz = 2 * k + 1 < c.Length ? c[2 * k + 1] : snap.y;
                        splatPos.Add(new Vector3(v[5 * i] + (cx - snap.x) / half, v[5 * i + 1] + (snap.y - cz) / half, v[5 * i + 2]));
                        splatUv.Add(new Vector2(v[5 * i + 3], v[5 * i + 4]));
                    }
                }
            }
            int count = splatPos.Count - splatPos.Count % 3;
            if (count == 0) return null;
            for (int i = 0; i < count; i++) splatIdx.Add(i);
            var mesh = splatMeshes[slot];
            if (!mesh) { mesh = splatMeshes[slot] = new Mesh { name = "Water wake splats " + slot, hideFlags = HideFlags.DontSave }; mesh.MarkDynamic(); created.Add(mesh); }
            mesh.Clear();
            mesh.SetVertices(splatPos); mesh.SetUVs(0, splatUv); mesh.SetTriangles(splatIdx, 0, false);
            mesh.bounds = new Bounds(Vector3.zero, Vector3.one * 10f);
            return mesh;
        }
    }
}
