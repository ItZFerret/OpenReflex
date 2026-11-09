using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Rendering
{
    /// The terrain's RT_Bump target (_re/notes/rt-bump.md, render-pipeline.md section 5), re-rendered every frame for the
    /// target camera before it renders, as HFTerrainTextureRenderEvent (007c1010) does:
    ///   one A8R8G8B8 768 x 1536 target, three 768 x 512 slices stacked; cleared to 0x8080ff00;
    ///   per slice: the level's HFBumpModelRSData meshes in ascending SortBias (Hidden/Reflex/RTBump, alpha blended, RGB only),
    ///   then the tyre-track ribbons (OriginalTracks' mesh and ps_hft_tracks material, multiply blend, RGB only).
    /// The slices are the camera's ground-footprint trapezoid split along the horizontal view direction (RTBump_FitCascadeMatrices
    /// 007ba8d0, structure read from recorded frames): far edge 384 m ahead with half width 384 tan(hfov), near edge at the terrain hit
    /// of the bottom frustum edge less 4 m, each side through its corner hit (at least 7 m from the axis at the near edge), slab bounds
    /// near, near + 16, near + 64, 384 (checked against the matrices of recorded Concord, Kingston and Alpine frames). One projective map
    /// u = U.p / W.p serves all slabs; slab k has v_k = V_k.p / W.p (0 at its near bound, 1 at its far bound), and the terrain samples
    /// RT_Bump at (u, sum_k min(sat(v_k / 3), 1/3)) as vs_hft_lod0. The bump normal taps are g_UVOneStep = one texel along the
    /// camera's horizontal right, rotated (right.x / 768, right.z / 1536, -right.z / 768, right.x / 1536).
    /// The alpha channel is the dynamic VSM shadow in the original (ps_hfb_shadows, slice 2 only); it stays at 1 here (Unity's main
    /// light shadows stand in for it in the terrain shader).
    [ExecuteAlways]
    [DisallowMultipleComponent]
    public sealed class ReflexRTBump : MonoBehaviour
    {
        public struct Draw { public Mesh mesh; public int submesh; public Material material; public int pass; public Matrix4x4 matrix; public string name; }

        /// The cascade layout of one frame: the trapezoid in the camera's horizontal frame and the maps over p = (x, z, 1).
        public struct Fit
        {
            public Vector2 origin, forward, right; public float near, nearHalf, farHalf; public bool hit;
            public Vector4 u, w, v0, v1, v2, step;
        }

        public const int Width = 768, SliceHeight = 512, Slices = 3;
        public const float FarAlong = 384f, Slab1 = 16f, Slab2 = 64f, MinHalfWidth = 7f, NoHitNear = -3.5f, NearMargin = 4f;
        static readonly Color ClearColor = new Color(128f / 255f, 1f, 0f, 1f);   // 0x8080ff00 (alpha: the shadow pass writes 1 without casters)

        [Tooltip("Camera whose view the slices follow (Camera.main when empty).")]
        public Camera targetCamera;
        [Tooltip("The live terrain material (its _DeformMap feeds HFB_1tex_deformed).")]
        public Material terrainMaterial;
        [Tooltip("The level's terrain (its live material when terrainMaterial is empty).")]
        public OriginalWorld.OriginalTerrain terrain;
        public OriginalWorld.OriginalTracks tracks;
        /// Ground height at world (x, z) for the near-edge hit; the active Unity terrains when null.
        public Func<float, float, float> groundHeight;
        [NonSerialized] public List<Draw> draws = new List<Draw>();

        public RenderTexture Target => rt;
        public Fit LastFit { get; private set; }

        RenderTexture rt;
        // OPEN REFLEX: a world-locked near detail target for the bump normal. RT_Bump's slices follow the camera, so they are redrawn on a
        // new texel grid every frame and the same ground is resampled differently each frame; the bump normal's finite differences turn
        // that into shading that crawls as the camera turns (measured: freezing the layout removes it). The same draws go top-down into a
        // NearSpan metre square (NearSize texels, no rotation) whose origin is snapped to its texel lattice, so a ground point lands on the
        // same texel every frame; the terrain's bump taps read it inside the square (cross-faded to RT_Bump over its edge band). The
        // tint, the specular mask, the displacement and the slices themselves stay the original's. DebugNoNear (showcase 'nonear') = off.
        RenderTexture rtNear;
        public static int NearSize = 2048;
        public static float NearSpan = 56f, NearAhead = 0.2f, NearBand = 6f;
        public static bool DebugNoNear;
        CommandBuffer cmd;
        static ReflexRTBump activeOwner;
        Camera renderingCamera;

        static readonly int RTId = Shader.PropertyToID("_RfxRTBump"), ParamsId = Shader.PropertyToID("_RfxRTBumpParams");
        static readonly int NearId = Shader.PropertyToID("_RfxRTBumpNear"), NearRectId = Shader.PropertyToID("_RfxRTBumpNearRect"), NearOnId = Shader.PropertyToID("_RfxRTBumpNearOn");
        static readonly int UId = Shader.PropertyToID("_RfxRTBumpU"), WId = Shader.PropertyToID("_RfxRTBumpW");
        static readonly int V0Id = Shader.PropertyToID("_RfxRTBumpV0"), V1Id = Shader.PropertyToID("_RfxRTBumpV1"), V2Id = Shader.PropertyToID("_RfxRTBumpV2");
        static readonly int StepId = Shader.PropertyToID("_RfxRTBumpStep");
        static readonly int DeformRTId = Shader.PropertyToID("_RfxDeformRT"), DeformParamsId = Shader.PropertyToID("_RfxDeformParams");
        static readonly int DeformMapId = Shader.PropertyToID("_DeformMap"), UseDeformMapId = Shader.PropertyToID("_UseDeformMap");

        void OnEnable()
        {
            RenderPipelineManager.beginCameraRendering += OnBegin;
            RenderPipelineManager.endCameraRendering += OnEnd;
        }

        void OnDisable()
        {
            RenderPipelineManager.beginCameraRendering -= OnBegin;
            RenderPipelineManager.endCameraRendering -= OnEnd;
            ClearOwnedState();
            if (rt) { rt.Release(); DestroyImmediate(rt); }
            if (rtNear) { rtNear.Release(); DestroyImmediate(rtNear); }
            rtNear = null;
            cmd?.Release();
            rt = null; cmd = null;
        }

        void OnBegin(ScriptableRenderContext context, Camera c)
        {
            var target = targetCamera ? targetCamera : Camera.main;
            // Foreign subscribers must not erase the rendering camera's RT_Bump binding.
            // The owner clears validity when its camera finishes, so other cameras use the fallback.
            if (c != target) return;
            if (!rt)
            {
                rt = new RenderTexture(Width, SliceHeight * Slices, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear)
                    { name = "RT_Bump", filterMode = FilterMode.Trilinear, wrapMode = TextureWrapMode.Clamp, useMipMap = true, autoGenerateMips = false, hideFlags = HideFlags.DontSave };   // mips: the terrain's smoothed vertex displacement (RfxRTBumpDisplace); its pixel taps read mip 0
                rt.Create();
            }
            cmd ??= new CommandBuffer { name = "RT_Bump" };
            bool fromCore = CoreFit(c, out var coreFit);
            var fit = fromCore ? coreFit : FitCascades(c, groundHeight ?? UnityTerrainHeight);
            // test aid (showcase 'rtfreeze'): FreezeHold keeps the layout of the frame it was set in (the slices stop following the camera)
            if (FreezeHold) { if (!frozen) { frozenFit = fit; frozen = true; Debug.Log("[RT_Bump] layout frozen"); } fit = frozenFit; } else frozen = false;
            LastFit = fit;

            cmd.Clear();
            cmd.SetRenderTarget(rt);
            cmd.ClearRenderTarget(false, true, ClearColor);
            var tm = terrainMaterial ? terrainMaterial : terrain && terrain.UnityTerrain ? terrain.UnityTerrain.materialTemplate : null;
            Texture deform = tm && tm.HasProperty(DeformMapId) && tm.GetFloat(UseDeformMapId) > 0.5f ? tm.GetTexture(DeformMapId) : null;
            cmd.SetGlobalTexture(DeformRTId, deform ? deform : Texture2D.grayTexture);
            cmd.SetGlobalVector(DeformParamsId, new Vector4(deform ? 1f : 0f, 0f, 0f, 0f));
            bool trackPass = tracks && tracks.TryGetDraw(out _, out _, out _, out _);
            for (int k = 0; k < Slices; k++)
            {
                cmd.SetViewport(new Rect(0, k * SliceHeight, Width, SliceHeight));
                var vp = SliceMatrix(fit, k);      // GL convention: the command buffer converts it for the render texture
                cmd.SetViewProjectionMatrices(Matrix4x4.identity, vp);
                foreach (var d in draws)
                    if (d.mesh && d.material) cmd.DrawMesh(d.mesh, d.matrix, d.material, d.submesh, d.pass);
                if (trackPass && tracks.TryGetDraw(out var ribbons, out var decal, out var centre, out var half))
                {
                    // ribbon positions are terrain-normalized (x, z) in position.xy (vs_hft_tracks: world = pos * gWorldHalfSize)
                    var toWorld = new Matrix4x4(new Vector4(half.x, 0, 0, 0), new Vector4(0, 0, half.y, 0), Vector4.zero, new Vector4(centre.x, 0, centre.y, 1));
                    cmd.SetViewProjectionMatrices(toWorld, vp);
                    cmd.DrawMesh(ribbons, Matrix4x4.identity, decal, 0, 0);
                }
            }
            cmd.GenerateMips(rt);
            bool near = !DebugNoNear && NearSize >= 64;
            if (near)
            {
                if (!rtNear || rtNear.width != NearSize)
                {
                    if (rtNear) { rtNear.Release(); DestroyImmediate(rtNear); }
                    rtNear = new RenderTexture(NearSize, NearSize, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear)
                        { name = "RT_Bump near", filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
                    rtNear.Create();
                }
                float texel = NearSpan / NearSize;
                var camXZ = new Vector2(c.transform.position.x, c.transform.position.z);
                Vector2 centre = camXZ + fit.forward * (NearSpan * NearAhead);
                float x0 = Mathf.Floor((centre.x - 0.5f * NearSpan) / texel) * texel, z0 = Mathf.Floor((centre.y - 0.5f * NearSpan) / texel) * texel;
                var nvp = new Matrix4x4();
                nvp.SetRow(0, new Vector4(2f / NearSpan, 0f, 0f, -2f * x0 / NearSpan - 1f));
                nvp.SetRow(1, new Vector4(0f, 0f, 2f / NearSpan, -2f * z0 / NearSpan - 1f));
                nvp.SetRow(2, Vector4.zero);
                nvp.SetRow(3, new Vector4(0f, 0f, 0f, 1f));
                cmd.SetRenderTarget(rtNear);
                cmd.SetViewport(new Rect(0, 0, NearSize, NearSize));
                cmd.ClearRenderTarget(false, true, ClearColor);
                cmd.SetViewProjectionMatrices(Matrix4x4.identity, nvp);
                foreach (var d in draws)
                    if (d.mesh && d.material) cmd.DrawMesh(d.mesh, d.matrix, d.material, d.submesh, d.pass);
                if (trackPass && tracks.TryGetDraw(out var ribbonsN, out var decalN, out var centreN, out var halfN))
                {
                    var toWorldN = new Matrix4x4(new Vector4(halfN.x, 0, 0, 0), new Vector4(0, 0, halfN.y, 0), Vector4.zero, new Vector4(centreN.x, 0, centreN.y, 1));
                    cmd.SetViewProjectionMatrices(toWorldN, nvp);
                    cmd.DrawMesh(ribbonsN, Matrix4x4.identity, decalN, 0, 0);
                }
                cmd.SetGlobalTexture(NearId, rtNear);
                cmd.SetGlobalVector(NearRectId, new Vector4(x0, z0, 1f / NearSpan, NearBand));
            }
            cmd.SetGlobalFloat(NearOnId, near ? 1f : 0f);
            cmd.SetGlobalTexture(RTId, rt);
            cmd.SetGlobalVector(ParamsId, new Vector4(1f, trackPass ? 1f : 0f, 0f, 0f));
            cmd.SetGlobalVector(UId, fit.u); cmd.SetGlobalVector(WId, fit.w);
            cmd.SetGlobalVector(V0Id, fit.v0); cmd.SetGlobalVector(V1Id, fit.v1); cmd.SetGlobalVector(V2Id, fit.v2);
            cmd.SetGlobalVector(StepId, fit.step);
            context.ExecuteCommandBuffer(cmd);
            activeOwner = this;
            renderingCamera = c;
        }

        void OnEnd(ScriptableRenderContext context, Camera c)
        {
            if (c == renderingCamera) ClearOwnedState();
        }

        void ClearOwnedState()
        {
            if (activeOwner != this) return;
            Shader.SetGlobalVector(ParamsId, Vector4.zero);
            activeOwner = null;
            renderingCamera = null;
        }

        /// RT_Bump's displacement channel (blue, 0..1: the terrain's vertices rise by it, RfxRTBumpDisplace) of the given draws, drawn top-down
        /// over the world square [x0, x0 + span] x [z0, z0 + span] into size x size texels and read back to the CPU (one GPU round trip: for a
        /// fixed spot, e.g. where the menu world's hero stands). Row j holds z = z0 + (j + 0.5) span / size, column i x = x0 + (i + 0.5) span / size.
        /// Null without draws or on a failure.
        public static float[] DisplacementTopDown(List<Draw> draws, float x0, float z0, float span, int size)
        {
            if (draws == null || draws.Count == 0 || size < 2 || span <= 0) return null;
            RenderTexture target = null, prev = RenderTexture.active; Texture2D read = null;
            try
            {
                target = RenderTexture.GetTemporary(size, size, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
                var vp = new Matrix4x4();
                vp.SetRow(0, new Vector4(2f / span, 0f, 0f, -2f * x0 / span - 1f));
                vp.SetRow(1, new Vector4(0f, 0f, 2f / span, -2f * z0 / span - 1f));
                vp.SetRow(2, Vector4.zero);
                vp.SetRow(3, new Vector4(0f, 0f, 0f, 1f));
                using (var c = new CommandBuffer { name = "RT_Bump displacement read" })
                {
                    c.SetRenderTarget(target);
                    c.SetViewport(new Rect(0, 0, size, size));
                    c.ClearRenderTarget(false, true, ClearColor);
                    c.SetGlobalTexture(DeformRTId, Texture2D.grayTexture);
                    c.SetGlobalVector(DeformParamsId, Vector4.zero);
                    c.SetViewProjectionMatrices(Matrix4x4.identity, vp);
                    foreach (var d in draws) if (d.mesh && d.material) c.DrawMesh(d.mesh, d.matrix, d.material, d.submesh, d.pass);
                    Graphics.ExecuteCommandBuffer(c);
                }
                RenderTexture.active = target;
                read = new Texture2D(size, size, TextureFormat.RGBA32, false, true);
                read.ReadPixels(new Rect(0, 0, size, size), 0, 0, false);
                var px = read.GetPixels32();
                var o = new float[size * size];
                for (int i = 0; i < o.Length; i++) o[i] = px[i].b / 255f;
                return o;
            }
            catch (Exception e) { Debug.LogWarning("[RT_Bump] displacement read: " + e.Message); return null; }
            finally
            {
                RenderTexture.active = prev;
                if (target) RenderTexture.ReleaseTemporary(target);
                if (read) { if (Application.isPlaying) Destroy(read); else DestroyImmediate(read); }
            }
        }

        public static float UnityTerrainHeight(float x, float z)
        {
            foreach (var t in Terrain.activeTerrains)
            {
                if (!t || !t.terrainData) continue;
                Vector3 o = t.transform.position, s = t.terrainData.size;
                if (x >= o.x && x <= o.x + s.x && z >= o.z && z <= o.z + s.z) return o.y + t.SampleHeight(new Vector3(x, 0, z));
            }
            return float.NegativeInfinity;
        }

        // The game's own layout (core rx_session_rtbump_fit: RTBump_FitCascadeMatrices 007ba8d0 and 007c1010's camera preparation, exact in
        // the oracle): the camera goes to the core worker between frames and the latest trapezoid comes back (at most a game frame old;
        // the slices and the terrain's lookups always use the same one). FitCascades below is the stand-in until the first result (or
        // without the Open Reflex core).
        Reflex.Port.OriginalWorld.OriginalWorldRuntime world; float worldLook; bool frozen; Fit frozenFit;
        public static bool FreezeHold;
        readonly float[] fitPos = new float[3], fitFwd = new float[3], fitUp = new float[3];
        bool CoreFit(Camera c, out Fit fit)
        {
            fit = default;
            if (!Application.isPlaying) return false;
            if (!world && Time.unscaledTime >= worldLook) { worldLook = Time.unscaledTime + 1f; world = FindFirstObjectByType<Reflex.Port.OriginalWorld.OriginalWorldRuntime>(); }
            var sim = world ? world.SimDriver?.Open : null;
            if (sim == null) return false;
            var tr = c.transform;
            Vector3 p = tr.position, f = tr.forward, u = tr.up;
            fitPos[0] = p.x; fitPos[1] = p.y; fitPos[2] = p.z; fitFwd[0] = f.x; fitFwd[1] = f.y; fitFwd[2] = f.z; fitUp[0] = u.x; fitUp[1] = u.y; fitUp[2] = u.z;
            if (world.FramesHeld || world.ViewerActive) return HeldFit(sim, c, out fit);
            heldValid = false;
            sim.RequestRTBumpFit((float[])fitPos.Clone(), (float[])fitFwd.Clone(), (float[])fitUp.Clone(), c.fieldOfView * Mathf.Deg2Rad, c.aspect, c.nearClipPlane);
            var o = sim.LatestRTBumpFit(out _);
            if (o == null || !(o[1] > o[0])) return false;
            fit = FromTrapezoid(o);
            return true;
        }

        // No game frames run (the replay viewer, photo mode, a pause): the riding path's request above waits for the next game frame, so the
        // layout stayed the last riding camera's while the replay / photo camera looked elsewhere (w-replayterrain: away from that footprint the
        // terrain lost its detail layers, ruts, tracks and bump shading, with a hard edge where the stale footprint began). Instead the game's
        // fit of this camera now, asked of the idle worker directly (OpenReflexSimulation.RTBumpFitNow, in the same render frame: a camera cut
        // never shows the previous camera's layout); asked again only when the camera changed (a held shot costs nothing); the stand-in fit
        // if the worker does not answer in time.
        readonly float[] heldOut = new float[12], heldKey = new float[12];
        bool heldValid; Fit heldFit;
        const double HeldWaitMs = 4.0;
        /// (tests) the main thread's wait for the last held fit, ms.
        public static float HeldFitMs;
        bool HeldFit(Reflex.Port.RxCore.OpenReflexSimulation sim, Camera c, out Fit fit)
        {
            float fov = c.fieldOfView * Mathf.Deg2Rad, aspect = c.aspect, near = c.nearClipPlane;
            bool same = heldValid && heldKey[9] == fov && heldKey[10] == aspect && heldKey[11] == near;
            for (int i = 0; i < 3 && same; i++) same = heldKey[i] == fitPos[i] && heldKey[3 + i] == fitFwd[i] && heldKey[6 + i] == fitUp[i];
            if (same) { fit = heldFit; return true; }
            long t0 = System.Diagnostics.Stopwatch.GetTimestamp();
            bool got = sim.RTBumpFitNow(fitPos, fitFwd, fitUp, fov, aspect, near, HeldWaitMs, heldOut);
            HeldFitMs = (float)((System.Diagnostics.Stopwatch.GetTimestamp() - t0) * 1000.0 / System.Diagnostics.Stopwatch.Frequency);
            if (got && heldOut[1] > heldOut[0])
            {
                heldFit = FromTrapezoid(heldOut);
                for (int i = 0; i < 3; i++) { heldKey[i] = fitPos[i]; heldKey[3 + i] = fitFwd[i]; heldKey[6 + i] = fitUp[i]; }
                heldKey[9] = fov; heldKey[10] = aspect; heldKey[11] = near;
                heldValid = true;
                fit = heldFit;
                return true;
            }
            heldValid = false;
            fit = default;
            return false;   // (OnBegin: the stand-in fit of this camera)
        }

        /// Fit from the game's trapezoid (rx_session_rtbump_fit's out: near, far, near left / right x, far left / right x in the camera's
        /// horizontal frame, the horizontal forward (x, z), right (x, z), the position (x, z)): the side lines through the corners, the
        /// slabs near, near + 16, near + 64, far.
        public static Fit FromTrapezoid(float[] o)
        {
            float near = o[0], far = o[1], ax = o[2], bx = o[3], cx = o[4], dx = o[5];
            var f2 = new Vector2(o[6], o[7]); var r2 = new Vector2(o[8], o[9]);
            var fit = new Fit { origin = new Vector2(o[10], o[11]), forward = f2, right = r2, near = near, hit = true,
                                nearHalf = 0.5f * (bx - ax), farHalf = 0.5f * (dx - cx) };
            float depth = far - near;
            float BR = (dx - bx) / depth, AR = bx - BR * near;            // right side x = AR + BR a
            float BL = -(cx - ax) / depth, AL = -ax - BL * near;          // left side distance (-x) = AL + BL a
            float org = Vector2.Dot(f2, fit.origin), os = Vector2.Dot(r2, fit.origin);
            Vector4 along = new Vector4(f2.x, f2.y, -org, 0), across = new Vector4(r2.x, r2.y, -os, 0);
            float B = 0.5f * (BR + BL), A = 0.5f * (AR + AL);
            fit.w = B * along + new Vector4(0, 0, A, 0);
            fit.u = 0.5f * (BR * along + new Vector4(0, 0, AR, 0)) - 0.5f * across;
            float[] bound = { near, near + Slab1, near + Slab2, far };
            Vector4 V(int k)
            {
                float hwFar = A + B * bound[k + 1];
                float ck = hwFar / (bound[k + 1] - bound[k]);
                return ck * (along - new Vector4(0, 0, bound[k], 0));
            }
            fit.v0 = V(0); fit.v1 = V(1); fit.v2 = V(2);
            fit.step = new Vector4(r2.x / Width, r2.y / (SliceHeight * Slices), -r2.y / Width, r2.x / (SliceHeight * Slices));
            return fit;
        }

        /// The slab trapezoids for camera c (see the class comment).
        public static Fit FitCascades(Camera c, Func<float, float, float> height)
        {
            var tr = c.transform;
            Vector3 pos = tr.position, f = tr.forward, up = tr.up, right = tr.right;
            var f2 = new Vector2(f.x, f.z);
            if (f2.sqrMagnitude < 1e-8f) f2 = new Vector2(up.x, up.z);
            f2.Normalize();
            var r2 = new Vector2(f2.y, -f2.x);
            float tanV = Mathf.Tan(c.fieldOfView * 0.5f * Mathf.Deg2Rad), tanH = tanV * c.aspect;
            var fit = new Fit { origin = new Vector2(pos.x, pos.z), forward = f2, right = r2, farHalf = FarAlong * tanH };

            // near edge: the bottom frustum edge rays (left corner, centre, right corner) against the terrain; the slabs start NearMargin
            // before the closest hit, and each side runs from its far corner through its corner hit (at least MinHalfWidth at the near edge)
            float near = float.MaxValue;
            var cornerAlong = new float[2]; var cornerSide = new float[2]; var cornerHit = new bool[2];
            for (int sx = -1; sx <= 1; sx++)
            {
                Vector3 dir = f - up * tanV + right * (tanH * sx);
                if (!RayHit(pos, dir, height, out var p)) continue;
                var rel = new Vector2(p.x - pos.x, p.z - pos.z);
                float a = Vector2.Dot(rel, f2);
                near = Mathf.Min(near, a);
                if (sx != 0) { int ci = sx > 0 ? 0 : 1; cornerHit[ci] = true; cornerAlong[ci] = a; cornerSide[ci] = Mathf.Abs(Vector2.Dot(rel, r2)); }
                fit.hit = true;
            }
            near = fit.hit ? near - NearMargin : NoHitNear;
            near = Mathf.Min(near, FarAlong - Slab2 - 1f);
            fit.near = near;
            float SideAtNear(int ci)
            {
                if (!cornerHit[ci] || cornerAlong[ci] >= FarAlong - 1e-3f) return MinHalfWidth;
                float slope = (fit.farHalf - cornerSide[ci]) / (FarAlong - cornerAlong[ci]);
                return Mathf.Max(MinHalfWidth, cornerSide[ci] - (cornerAlong[ci] - near) * slope);
            }
            float right0 = SideAtNear(0), left0 = SideAtNear(1);
            fit.nearHalf = 0.5f * (right0 + left0);

            // p = (x, z, 1): along a(p) = f2 . (p - pos), side s(p) = r2 . (p - pos); right / left side offsets R(a), L(a) linear in a,
            // half width hw = (R + L) / 2 = A + B a (the common denominator); u = (R - s) / (R + L)
            float o = Vector2.Dot(f2, fit.origin), os = Vector2.Dot(r2, fit.origin);
            Vector4 along = new Vector4(f2.x, f2.y, -o, 0), across = new Vector4(r2.x, r2.y, -os, 0);
            float BR = (fit.farHalf - right0) / (FarAlong - near), AR = right0 - BR * near;
            float BL = (fit.farHalf - left0) / (FarAlong - near), AL = left0 - BL * near;
            float B = 0.5f * (BR + BL), A = 0.5f * (AR + AL);
            fit.w = B * along + new Vector4(0, 0, A, 0);
            fit.u = 0.5f * (BR * along + new Vector4(0, 0, AR, 0)) - 0.5f * across;   // u = 0 on the right side, 1 on the left
            float[] bound = { near, near + Slab1, near + Slab2, FarAlong };
            Vector4 V(int k)
            {
                float hwFar = A + B * bound[k + 1];
                float ck = hwFar / (bound[k + 1] - bound[k]);
                return ck * (along - new Vector4(0, 0, bound[k], 0));      // v_k = c_k (a - a_k) / hw
            }
            fit.v0 = V(0); fit.v1 = V(1); fit.v2 = V(2);
            fit.step = new Vector4(r2.x / Width, r2.y / (SliceHeight * Slices), -r2.y / Width, r2.x / (SliceHeight * Slices));
            return fit;
        }

        public static bool RayHit(Vector3 origin, Vector3 dir, Func<float, float, float> height, out Vector3 hit)
        {
            hit = default;
            float horiz = new Vector2(dir.x, dir.z).magnitude;
            if (horiz < 1e-5f) { float h0 = height(origin.x, origin.z); if (dir.y < 0 && !float.IsInfinity(h0)) { hit = new Vector3(origin.x, h0, origin.z); return true; } return false; }
            const float step = 0.5f;                      // horizontal metres
            float dt = step / horiz, tMax = (FarAlong + 16f) / horiz, prev = 0f;
            for (float t = dt; t <= tMax; t += dt)
            {
                var p = origin + dir * t;
                float h = height(p.x, p.z);
                if (float.IsNegativeInfinity(h)) { prev = t; continue; }
                if (p.y <= h)
                {
                    float lo = prev, hi = t;
                    for (int i = 0; i < 12; i++)
                    {
                        float m = (lo + hi) * 0.5f; var q = origin + dir * m;
                        if (q.y <= height(q.x, q.z)) hi = m; else lo = m;
                    }
                    hit = origin + dir * hi;
                    return true;
                }
                prev = t;
            }
            return false;
        }

        /// Clip transform of slice k over (x, y, z, 1) (OpenGL convention; y is ignored): x = 2u - 1, y = 2 v_k - 1, shifted by half a
        /// texel as D3D9 rasterizes (pixel i is the point u = i / 768, sampled back at its texel centre (i + 0.5) / 768).
        public static Matrix4x4 SliceMatrix(in Fit fit, int k)
        {
            Vector4 v = k == 0 ? fit.v0 : k == 1 ? fit.v1 : fit.v2;
            Vector4 Row(Vector4 r) => new Vector4(r.x, 0f, r.y, r.z);
            var m = new Matrix4x4();
            m.SetRow(0, Row(2f * fit.u - fit.w + fit.w * (1f / Width)));
            m.SetRow(1, Row(2f * v - fit.w + fit.w * (1f / SliceHeight)));
            m.SetRow(2, Vector4.zero);
            m.SetRow(3, Row(fit.w));
            return m;
        }
    }
}
