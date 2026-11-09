// CINEMATIC EFFECTS (OPEN REFLEX; not part of the original renderer): depth of field (bokeh), motion blur and lens effects for REPLAY and
// PHOTO MODE only. Riding never records any of this: ReflexCinematic.Current is null unless the replay viewer owns the shot
// (Scripts/Replay/ReflexReplay.Cinematic.cs sets it every frame it does), and without it the original chain runs exactly as before.
//
// Where it sits in the frame (ReflexRenderPipeline.cs has the two hooks, ReflexPostPass.RecordRenderGraph and OnBegin):
//   HDR stage      ReflexCinematic.RecordHdr, between the world and Reflex's tonemap: PHOTO MODE's shutter accumulation, motion blur, depth of
//                  field. The blur happens on the HDR image, so a lamp or the sun keeps its energy in a bokeh disc and streaks properly.
//   Display stage  ReflexCinematicLensPass, after the tonemap (same pass event, queued after Reflex's): lens flare / dirt, chromatic
//                  aberration, vignette, film grain, on the tonemapped (display referred) image.
// Inputs: the depth (URP's depth copy / pre-pass: the vehicles carry a DepthOnly pass) and, for motion blur, URP's motion vector texture:
// camera motion reprojected from depth plus the per-object vectors of the renderers whose shader has a MotionVectors pass (Reflex/Vehicle:
// the bikes, the riders, the wheels). The motion vectors are URP's own, so the temporal AA of w-rmaa shares them.
// The effects' shader is Resources/ReflexCinematic (Assets/Shaders/Cinematic/Resources: a Resources shader is in every player build).
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Rendering
{
    /// One shot's lens and camera effects. Distances are in feet (the game's unit).
    public sealed class ReflexCinematicSettings
    {
        // depth of field (bokeh)
        public bool dof;
        public float focusDistance = 20f;     // the plane in focus (eye depth)
        public float aperture = 2.8f;         // f-number
        public float focalScale = 2.4f;       // the lens is this much longer than the field of view says: a scene in feet is shot like a miniature
        public float maxBlur = 0.025f;         // the widest circle of confusion, as a share of the picture's height
        public float focusZone = 4f;          // ft each side of the focus plane kept fully sharp (the whole rider and bike, not just the point focused on)
        public float focusZoneNear = -1f;     // ft in front of the focus plane kept sharp (-1: focusZone); the replay keeps the ground before the rider sharp
        public float highlights = 1.5f;       // how much brighter lights weigh in the bokeh
        // motion blur (velocity from URP's motion vectors)
        public bool motionBlur;
        public float shutterAngle = 180f;     // degrees of a 1/60 s frame the shutter is open
        public float worldDt;                 // world seconds this frame advanced (0: the frame's real time)
        public float maxMotion = 0.05f;       // the longest streak, uv
        public float maxScale = 6f;           // the most the shutter stretches one frame's motion vectors (the replay: 1, a streak never longer than the frame's own motion)
        // PHOTO MODE's shutter: this frame is sub-frame accumulateIndex of a mean (0 starts it, -1: not accumulating)
        public int accumulateIndex = -1;
        public bool accumulateHold;           // the mean is complete: keep it (the frame is not blended in)
        // lens (0 = off)
        public float chromatic, vignette, grain, flare, dirt;
        public float lampThreshold = 0.98f;   // a lamp / glint: this white (and out of its surroundings)

        public bool HasLens => chromatic > 0f || vignette > 0f || grain > 0f || flare > 0f;
        public bool Any => dof || motionBlur || accumulateIndex >= 0 || HasLens;

        public ReflexCinematicSettings Clone() => (ReflexCinematicSettings)MemberwiseClone();
    }

    /// The effects for the game camera now.
    public static class ReflexCinematic
    {
        /// What the replay viewer wants drawn this frame (null: nothing; set every frame by whoever owns the shot).
        public static ReflexCinematicSettings Current;
        /// Tests: the settings in force instead of the viewer's (a fixture sets fixed values for matched pictures).
        public static ReflexCinematicSettings Override;
        /// The last frame the replay viewer / photo mode set Current (SPEED BLUR, ReflexSpeedBlur.cs, stays out of those frames).
        public static int ReplayFrame = -10;

        /// PHOTO MODE's shutter is averaging sub-frames posed at different moments: every one must be a picture of its own moment (the temporal AA
        /// starts its history over each frame instead of blending in the previous sub-frame's: w-replayterrain, the doubled rider and the dark
        /// fringes of the preview).
        public static bool Accumulating { get { var s = Override ?? Current; return s != null && s.accumulateIndex >= 0 && !s.accumulateHold && !KeepHistory; } }
        /// Tests: the old behaviour (the history kept between the shutter's sub-frames).
        public static bool KeepHistory;

        /// A camera cut / seek / pose jump: no motion blur for the next frames (their vectors would smear the jump).
        public static void Cut(int frames = 2) { suppress = Mathf.Max(suppress, frames); }

        internal static ReflexCinematicSettings frame;   // this frame's, taken at BeginCamera (null: the pass is off)
        static int suppress;
        static Material material;
        static Texture2D dirt;
        static readonly List<MaterialPropertyBlock> blocks = new List<MaterialPropertyBlock>();
        static int blockIndex;
        static RTHandle accA, accB; static bool accFlip; static int accW, accH, accDrawn, accLastIndex = -1;
        static ReflexCinematicLensPass lensPass;
        static bool loggedMissing;

        static readonly int Src0 = Shader.PropertyToID("_CineSrc0"), Src1 = Shader.PropertyToID("_CineSrc1"), Src2 = Shader.PropertyToID("_CineSrc2"),
            Src3 = Shader.PropertyToID("_CineSrc3"), DirtId = Shader.PropertyToID("_CineDirt"), SizeId = Shader.PropertyToID("_CineSize"),
            FullId = Shader.PropertyToID("_CineFull"), DofZoneId = Shader.PropertyToID("_CineDofZone"), ZId = Shader.PropertyToID("_CineZ"), DofId = Shader.PropertyToID("_CineDof"),
            MbId = Shader.PropertyToID("_CineMb"), LensId = Shader.PropertyToID("_CineLens"), FlareId = Shader.PropertyToID("_CineFlare"),
            AccId = Shader.PropertyToID("_CineAcc"), ViewId = Shader.PropertyToID("_CineView"), DofZoneNearId = Shader.PropertyToID("_CineDofZoneNear");
        const int PassMotionBlur = 0, PassDofPre = 1, PassDofGather = 2, PassDofComposite = 3, PassAccumulate = 4, PassFlareBright = 5, PassFlareGhosts = 6, PassLensFinal = 7;

        // per-frame shader values (set on every draw)
        static Vector4 vZ, vDof, vMb, vLens, vFlare, vAcc, vView, vFull;
        static float vDofZone, vDofZoneNear;

        /// Called from ReflexRenderPipeline.OnBegin for the game camera, after the original post pass is queued.
        internal static void BeginCamera(ScriptableRenderer renderer, Camera cam)
        {
            var s = Override ?? Current;
            if (s != null && !s.Any) s = null;
            if (s == null) { frame = null; ReleaseIdle(); return; }
            if (!EnsureResources()) { frame = null; return; }
            idleFrames = 0;
            if (suppress > 0 && (s.motionBlur)) { s = s.Clone(); s.motionBlur = false; }
            if (suppress > 0) suppress--;
            frame = s;
            blockIndex = 0;
            var input = ScriptableRenderPassInput.None;
            if (s.dof || s.motionBlur || s.flare > 0f) input |= ScriptableRenderPassInput.Depth;
            if (s.motionBlur) input |= ScriptableRenderPassInput.Motion;
            lensPass ??= new ReflexCinematicLensPass();
            lensPass.ConfigureInput(input);
            renderer.EnqueuePass(lensPass);
        }

        static int idleFrames;
        static void ReleaseIdle()
        {
            if (accA == null || ++idleFrames < 120) return;
            accA.Release(); accB.Release(); accA = accB = null;
        }

        static bool EnsureResources()
        {
            if (material) return true;
            var sh = Resources.Load<Shader>("ReflexCinematic");
            if (!sh) { if (!loggedMissing) { loggedMissing = true; Debug.LogError("ReflexCinematic: Resources/ReflexCinematic shader not found"); } return false; }
            material = CoreUtils.CreateEngineMaterial(sh);
            return true;
        }

        // ------------------------------------------------------------------------------------------- the frame's values

        static void SetFrame(UniversalCameraData camData, ReflexCinematicSettings s, int W, int H)
        {
            var cam = camData.camera;
            float n = cam.nearClipPlane, f = cam.farClipPlane;
            vZ = SystemInfo.usesReversedZBuffer
                ? new Vector4(-1f + f / n, 1f, (-1f + f / n) / f, 1f / f)
                : new Vector4(1f - f / n, f / n, (1f - f / n) / f, f / n / f);
            float tanHalf = Mathf.Tan(Mathf.Clamp(cam.fieldOfView, 1f, 150f) * 0.5f * Mathf.Deg2Rad);
            vView = new Vector4(cam.aspect, tanHalf, f, 0);
            vFull = new Vector4(W, H, 1f / W, 1f / H);
            // depth of field: thin lens. Focal length from the field of view on a 24 mm tall sensor (scaled: the scene is in feet), the
            // circle of confusion c(d) = A f / (S - f) * (d - S) / d on the sensor, in pixels of the picture's height
            float focalFt = 12f / tanHalf * s.focalScale / 304.8f;
            float S = Mathf.Max(s.focusDistance, focalFt * 1.5f);
            float A = focalFt / Mathf.Max(0.5f, s.aperture);
            float k = A * focalFt / (S - focalFt) / (24f / 304.8f) * H;
            vDof = new Vector4(S, k, Mathf.Max(2f, s.maxBlur * H), s.highlights);
            vDofZone = Mathf.Max(0f, s.focusZone);
            vDofZoneNear = s.focusZoneNear >= 0f ? s.focusZoneNear : vDofZone;
            float dt = s.worldDt > 1e-5f ? s.worldDt : Mathf.Max(Time.unscaledDeltaTime, 1e-4f);
            float scale = Mathf.Clamp(s.shutterAngle / 360f / 60f / dt, 0f, Mathf.Max(0f, s.maxScale));
            vMb = new Vector4(scale, s.maxMotion, Mathf.Max(0.5f, 1.5f), 0f);
            vLens = new Vector4(s.chromatic, s.vignette, s.grain, (Time.frameCount % 997) * 0.37f + 1f);
            vFlare = new Vector4(s.flare, s.dirt, s.lampThreshold, 0f);
            vAcc = new Vector4(s.accumulateHold ? 0f : s.accumulateIndex <= 0 ? 1f : 1f / (s.accumulateIndex + 1), 0, 0, 0);
        }

        // ------------------------------------------------------------------------------------------------ drawing

        class PassData
        {
            internal Material material; internal int pass; internal MaterialPropertyBlock mpb;
            internal TextureHandle s0, s1, s2, s3; internal Vector4 size;
            internal Vector4 z, dof, mb, lens, flare, acc, view, full;
            internal float dofZone, dofZoneNear;
            internal Texture dirt;
        }

        static TextureHandle Make(RenderGraph rg, int w, int h, GraphicsFormat fmt, string name) =>
            rg.CreateTexture(new TextureDesc(Mathf.Max(1, w), Mathf.Max(1, h)) { colorFormat = fmt, name = name, filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, clearBuffer = false });

        static void Draw(RenderGraph rg, string name, int pass, TextureHandle dst, Vector2Int srcSize, TextureHandle s0, TextureHandle s1 = default, TextureHandle s2 = default, TextureHandle s3 = default)
        {
            using (var builder = rg.AddRasterRenderPass<PassData>(name, out var d, ReflexPassSamplers.Get(name)))
            {
                d.material = material; d.pass = pass;
                if (blockIndex >= blocks.Count) blocks.Add(new MaterialPropertyBlock());
                d.mpb = blocks[blockIndex++];
                d.s0 = s0; d.s1 = s1; d.s2 = s2; d.s3 = s3;
                if (s0.IsValid()) builder.UseTexture(s0);
                if (s1.IsValid()) builder.UseTexture(s1);
                if (s2.IsValid()) builder.UseTexture(s2);
                if (s3.IsValid()) builder.UseTexture(s3);
                builder.SetRenderAttachment(dst, 0, AccessFlags.Write);
                d.size = new Vector4(srcSize.x, srcSize.y, 1f / Mathf.Max(1, srcSize.x), 1f / Mathf.Max(1, srcSize.y));
                d.z = vZ; d.dof = vDof; d.dofZone = vDofZone; d.dofZoneNear = vDofZoneNear; d.mb = vMb; d.lens = vLens; d.flare = vFlare; d.acc = vAcc; d.view = vView; d.full = vFull; d.dirt = dirt;
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    var p = data.mpb; p.Clear();
                    if (data.s0.IsValid()) p.SetTexture(Src0, data.s0);
                    if (data.s1.IsValid()) p.SetTexture(Src1, data.s1);
                    if (data.s2.IsValid()) p.SetTexture(Src2, data.s2);
                    if (data.s3.IsValid()) p.SetTexture(Src3, data.s3);
                    p.SetTexture(DirtId, data.dirt ? data.dirt : Texture2D.blackTexture);
                    p.SetVector(SizeId, data.size); p.SetVector(FullId, data.full); p.SetVector(ZId, data.z); p.SetVector(DofId, data.dof); p.SetFloat(DofZoneId, data.dofZone); p.SetFloat(DofZoneNearId, data.dofZoneNear);
                    p.SetVector(MbId, data.mb); p.SetVector(LensId, data.lens); p.SetVector(FlareId, data.flare); p.SetVector(AccId, data.acc); p.SetVector(ViewId, data.view);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, data.pass, MeshTopology.Triangles, 3, 1, p);
                });
            }
        }

        // ---------------------------------------------------------------------------------------------- HDR stage

        /// PHOTO MODE's accumulation, motion blur and depth of field on the HDR scene image; returns the image to measure and tonemap
        /// (src itself when nothing applies).
        internal static TextureHandle RecordHdr(RenderGraph rg, ContextContainer frameData, UniversalResourceData res, TextureHandle src, int W, int H)
        {
            var s = frame;
            if (s == null || !material) return src;
            if (!(s.dof || s.motionBlur || s.accumulateIndex >= 0)) return src;
            var camData = frameData.Get<UniversalCameraData>();
            SetFrame(camData, s, W, H);
            var depth = res.cameraDepthTexture; var mv = res.motionVectorColor;
            var fmt = GraphicsFormat.R16G16B16A16_SFloat;
            var full = new Vector2Int(W, H);
            var cur = src;

            if (s.accumulateIndex >= 0)
            {
                // the mean's weight from the sub-frames actually drawn into it (w-replayterrain: from the sub-frame's index, a sub-frame the
                // viewer posed but no render drew - a frame without a camera render, a second render of one - left the previous mean in the
                // picture: a ghost of the last shot / moment)
                if (s.accumulateHold || s.accumulateIndex == accLastIndex && accDrawn > 0) vAcc.x = 0f;   // (the mean is complete / this sub-frame is in it already: kept)
                else
                {
                    if (s.accumulateIndex == 0 || s.accumulateIndex < accLastIndex) accDrawn = 0;
                    vAcc.x = 1f / (accDrawn + 1); accDrawn++;
                    accLastIndex = s.accumulateIndex;
                }
                if (accA == null || accW != W || accH != H)
                {
                    vAcc.x = 1f;   // (a new history: this frame starts the mean)
                    accA?.Release(); accB?.Release();
                    accA = RTHandles.Alloc(W, H, fmt, name: "Reflex cinematic accumulation A", filterMode: FilterMode.Point);
                    accB = RTHandles.Alloc(W, H, fmt, name: "Reflex cinematic accumulation B", filterMode: FilterMode.Point);
                    accW = W; accH = H;
                }
                accFlip = !accFlip;
                var prev = rg.ImportTexture(accFlip ? accB : accA);
                var next = rg.ImportTexture(accFlip ? accA : accB);
                Draw(rg, "Reflex cinematic accumulate", PassAccumulate, next, full, cur, prev);
                cur = next;
            }
            if (s.motionBlur && s.accumulateIndex < 0 && depth.IsValid() && mv.IsValid())
            {
                var mb = Make(rg, W, H, fmt, "Reflex cinematic motion blur");
                Draw(rg, "Reflex cinematic motion blur", PassMotionBlur, mb, full, cur, mv, depth);
                cur = mb;
            }
            if (s.dof && depth.IsValid())
            {
                var half = new Vector2Int(Mathf.Max(1, W / 2), Mathf.Max(1, H / 2));
                var pre = Make(rg, half.x, half.y, fmt, "Reflex cinematic dof half");
                var gather = Make(rg, half.x, half.y, fmt, "Reflex cinematic dof bokeh");
                var outp = Make(rg, W, H, fmt, "Reflex cinematic dof");
                Draw(rg, "Reflex cinematic dof prefilter", PassDofPre, pre, full, cur, default, depth);
                Draw(rg, "Reflex cinematic dof bokeh", PassDofGather, gather, half, pre);
                Draw(rg, "Reflex cinematic dof composite", PassDofComposite, outp, half, cur, gather, depth);
                cur = outp;
            }
            return cur;
        }

        // ------------------------------------------------------------------------------------------ display stage

        internal static void RecordLens(RenderGraph rg, ContextContainer frameData)
        {
            var s = frame;
            if (s == null || !s.HasLens || !material) return;
            var res = frameData.Get<UniversalResourceData>();
            if (res.isActiveTargetBackBuffer) return;
            var camData = frameData.Get<UniversalCameraData>();
            var src = res.activeColorTexture;
            var desc = rg.GetTextureDesc(src);
            int W = desc.width, H = desc.height;
            SetFrame(camData, s, W, H);
            var depth = res.cameraDepthTexture;
            vView.w = depth.IsValid() ? 1f : 0f;
            var full = new Vector2Int(W, H);
            var fmt = desc.colorFormat;
            var flare = default(TextureHandle);
            if (s.flare > 0f)
            {
                var q = new Vector2Int(Mathf.Max(1, W / 4), Mathf.Max(1, H / 4));
                var bright = Make(rg, q.x, q.y, GraphicsFormat.R16G16B16A16_SFloat, "Reflex cinematic bright");
                flare = Make(rg, q.x, q.y, GraphicsFormat.R16G16B16A16_SFloat, "Reflex cinematic flare");
                if (!dirt) dirt = MakeDirt();
                Draw(rg, "Reflex cinematic flare bright", PassFlareBright, bright, full, src, default, depth);
                Draw(rg, "Reflex cinematic flare ghosts", PassFlareGhosts, flare, q, bright);
            }
            var dst = Make(rg, W, H, fmt, "Reflex cinematic lens");
            Draw(rg, "Reflex cinematic lens", PassLensFinal, dst, full, src, flare);
            res.cameraColor = dst;
        }

        /// The lens dirt: soft smudges and dust specks, white = dirty (a procedural 512 x 512, made once).
        static Texture2D MakeDirt()
        {
            const int N = 512;
            var px = new byte[N * N];
            var rng = new System.Random(20261031);
            // smudges: a few soft low-frequency blobs
            var blobs = new List<(float x, float y, float r, float a)>();
            for (int i = 0; i < 26; i++) blobs.Add(((float)rng.NextDouble(), (float)rng.NextDouble(), 0.05f + (float)rng.NextDouble() * 0.16f, 0.25f + (float)rng.NextDouble() * 0.55f));
            // dust: many tiny discs, a few bigger ones
            var specks = new List<(float x, float y, float r, float a)>();
            for (int i = 0; i < 520; i++) specks.Add(((float)rng.NextDouble(), (float)rng.NextDouble(), 0.0016f + (float)Math.Pow(rng.NextDouble(), 3) * 0.012f, 0.35f + (float)rng.NextDouble() * 0.65f));
            var field = new float[N * N];
            foreach (var b in blobs)
            {
                int x0 = Mathf.Max(0, (int)((b.x - b.r * 1.6f) * N)), x1 = Mathf.Min(N - 1, (int)((b.x + b.r * 1.6f) * N));
                int y0 = Mathf.Max(0, (int)((b.y - b.r * 1.6f) * N)), y1 = Mathf.Min(N - 1, (int)((b.y + b.r * 1.6f) * N));
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                    {
                        float dx = (x + 0.5f) / N - b.x, dy = (y + 0.5f) / N - b.y;
                        float d = Mathf.Sqrt(dx * dx + dy * dy) / b.r;
                        if (d < 1.6f) field[y * N + x] += b.a * Mathf.Exp(-d * d * 2.2f);
                    }
            }
            foreach (var b in specks)
            {
                int x0 = Mathf.Max(0, (int)((b.x - b.r * 2f) * N)), x1 = Mathf.Min(N - 1, (int)((b.x + b.r * 2f) * N));
                int y0 = Mathf.Max(0, (int)((b.y - b.r * 2f) * N)), y1 = Mathf.Min(N - 1, (int)((b.y + b.r * 2f) * N));
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                    {
                        float dx = (x + 0.5f) / N - b.x, dy = (y + 0.5f) / N - b.y;
                        float d = Mathf.Sqrt(dx * dx + dy * dy) / b.r;
                        if (d < 2f) field[y * N + x] += b.a * Mathf.Clamp01(1.4f - d);
                    }
            }
            for (int i = 0; i < px.Length; i++) px[i] = (byte)Mathf.RoundToInt(Mathf.Clamp01(field[i]) * 255f);
            var t = new Texture2D(N, N, TextureFormat.R8, false, true) { name = "Reflex lens dirt", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.HideAndDontSave };
            t.SetPixelData(px, 0); t.Apply(false, true);
            return t;
        }
    }

    /// The display stage: queued behind Reflex's own post pass (same event, enqueued later: URP keeps the order).
    sealed class ReflexCinematicLensPass : ScriptableRenderPass
    {
        public ReflexCinematicLensPass()
        {
            renderPassEvent = (RenderPassEvent)((int)RenderPassEvent.BeforeRenderingPostProcessing + 1);   // (Reflex's pass is at BeforeRenderingPostProcessing: after its tonemap, whatever the queue order)
            profilingSampler = new ProfilingSampler("Reflex cinematic lens");
            requiresIntermediateTexture = true;
        }

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData) => ReflexCinematic.RecordLens(renderGraph, frameData);
    }
}
