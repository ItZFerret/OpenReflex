using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using UnityEngine;
using Reflex.Port.Content;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.OriginalWorld
{
    // The level's in-level Bink screens played as the running game plays them (_re/notes/screens-ads.md; data in Content/ReflexVideoScreens.cs).
    // One call registers them: OriginalVideoScreens.Attach(level) after the scenery is built (OriginalLevel.LoadNow).
    //  - Each distinct video (a `bink` asset) is decoded once through an IBinkSource (Content/ReflexVideoSource.cs): the install's binkw32.dll
    //    in the legacy plugin (native bink.cpp, as the front-end videos: ReflexVideo.cs), or on the OPEN REFLEX path (no plugin bundled) the
    //    machine's ffmpeg, which decodes the same Bink 1 files into the file's own Y / U / V / A planes. The video is played at once with Loop
    //    on, as VideoEntity::vf5 00594d50 / XenonBinkVideoManager::vf2 00595da0 / XenonBinkVideo::vf1 do; at the last frame BinkGoto(1)
    //    (XenonBinkVideo::vf8 00595470 -> vf4). The frames come when BinkWait says so on a clock of real time (RADTimerRead), not game time.
    //    Several VideoEntities that name one video share its decoder and planes; several that name one target texture share the target.
    //    Without ffmpeg (open path) the targets stay at 0 (the game clears them to 0): the screens are dark, the level loads as ever.
    //  - The decode runs on one thread of its own for all screens of the level (never the simulation or the main thread); the main thread
    //    only uploads a new frame's four planes and draws them into the render target with the game's YUV program (Reflex/Bink Video,
    //    stretched over the whole target as 00594a40 draws it). The target is a linear ARGB32 texture of the "Texture To Override" header's
    //    size, black until the first frame (the game clears it to 0).
    //  - Pause: VideoEntity::vf19 00594f40 calls BinkPause when the scene's pause flag [[0xa946b8]+0x5a54|0x20c8]+0x20014 changes. In the legacy
    //    player the flag is read from the running world between frames (reflex_world_read: the bike state hashes are unchanged); on the open
    //    path (and for the port's own pause) it is OriginalWorldRuntime.paused, which the race pause page (OwnRacePause) sets as the game's
    //    pause does.
    //  - Materials: every scenery material that names a target samples it: Template_Prelit_Map_Ctl / Template_Prelit_Stadiums (modes of
    //    Reflex/Original Scenery) get the target in the texture slot ReflexSceneryMaterial gave the black stand-in; FY_Projector and
    //    SX_DLC_FY_Bink_ProjectionBeam are switched to Reflex/Original Video Screen (their own programs).
    [DisallowMultipleComponent]
    public sealed class OriginalVideoScreens : MonoBehaviour
    {
        public OriginalLevel level;
        public OriginalWorldRuntime runtime;
        [Tooltip("Pause the screens with the running game's pause flag (else only the port's own pause).")]
        public bool useGameState = true;

        [Header("Result (read only)")]
        public int screens; public int materialsBound; public bool paused; public bool gamePaused; public int gameEntitiesFound;
        public int framesDecoded, framesShown;
        [Tooltip("Decode thread: ms per decoded frame (BinkDoFrame .. BinkNextFrame + plane copy), average / worst.")]
        public double decodeMsAverage, decodeMsWorst;
        [Tooltip("Main thread: ms per Unity frame (plane uploads + YUV draws + the world read request), average.")]
        public double mainMsAverage;
        [TextArea(2, 8)] public string report;

        /// Tests (edit mode): decode synchronously, no thread (DecodeTo).
        [NonSerialized] public bool manual;
        /// Tests: every Attach decodes manually (TestManual) and is handed to TestAttached (e.g. DecodeTo chosen frames).
        public static bool TestManual, TestDisable; public static Action<OriginalVideoScreens> TestAttached;

        /// One decoded video: its source, planes and clock. Shared by every VideoEntity that names the same `bink` asset.
        sealed class Feed
        {
            public string bink; public ReflexVideoScreens.Screen first;
            public IBinkSource src; public ReflexBink.Info info; public bool opened, failed; public string error; public string file;
            public byte[][] work, ready, shown; public int readySeq, shownSeq, readyFrame, shownFrame, loops;
            public double clock; public bool pausedNow;
            public Texture2D[] planes; public Material yuv;
            public readonly List<Target> targets = new List<Target>();
        }

        /// One render target (a TextureAsset the VideoEntities override), shared by the entities that name it.
        sealed class Target { public string name; public RenderTexture rt; public Feed feed; public bool hasFrame; public bool colorRef; }

        sealed class Screen
        {
            public ReflexVideoScreens.Screen data;
            public Feed feed; public Target tgt;
            public RenderTexture target => tgt.rt;
            public uint gameEntity;     // the VideoEntity in the running world (0: not found)
            public bool colorRef;       // the target is the level's AmbientDirectionalLight _ColorReferenceMap (s13 of every lit program)
            public int shownFrame => feed.shownFrame;
        }

        readonly List<Screen> list = new List<Screen>();
        readonly List<Feed> feeds = new List<Feed>();
        readonly Dictionary<string, Target> targets = new Dictionary<string, Target>();
        readonly HashSet<Material> bound = new HashSet<Material>();
        readonly object gate = new object();
        Thread thread; volatile bool quit; readonly AutoResetEvent wake = new AutoResetEvent(false);
        volatile bool pauseRequest; int boundMaterialCount = -1;
        Shader screenShader, binkShader;
        ReflexLevelContent content;
        string cacheDir;
        double decodeTotal, mainTotal; int mainFrames;
        bool openPath;

        public IReadOnlyList<string> ScreenNames => list.Select(s => s.data.name).ToList();
        public RenderTexture TargetOf(int entityId) => list.FirstOrDefault(s => s.data.entityId == entityId)?.target;
        public int ShownFrame(int entityId) => list.FirstOrDefault(s => s.data.entityId == entityId)?.shownFrame ?? 0;
        /// Distinct videos decoded / distinct render targets of the level (the screens share both).
        public int Videos => feeds.Count;
        public int Targets => targets.Count;

        /// Registers the screens of a loaded level (no-op without VideoEntity). Main thread. Both players: the legacy one decodes in the
        /// plugin, the open one through the machine's ffmpeg.
        public static OriginalVideoScreens Attach(OriginalLevel lvl, bool manual = false)
        {
            try
            {
                if (!lvl || lvl.Content == null || TestDisable) return null;
                var data = ReflexVideoScreens.Read(lvl.Content);
                if (data.Count == 0) return null;
                var v = lvl.GetComponent<OriginalVideoScreens>() ?? lvl.gameObject.AddComponent<OriginalVideoScreens>();
                v.level = lvl; v.runtime = lvl.runtime; v.manual = manual || TestManual;
                v.Begin(lvl.Content, data);
                TestAttached?.Invoke(v);
                return v;
            }
            catch (Exception e) { Debug.LogWarning("OriginalVideoScreens: " + e.Message + "\n" + e.StackTrace, lvl); return null; }   // never stops the level load
        }

        void Begin(ReflexLevelContent c, List<ReflexVideoScreens.Screen> data)
        {
            Stop();
            content = c; cacheDir = ReflexInstall.CacheDir;
            openPath = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex;
            screenShader = Shader.Find("Reflex/Original Video Screen"); binkShader = Shader.Find("Reflex/Bink Video");
            if (!binkShader) { report = "shader Reflex/Bink Video not found (Always Included Shaders)"; Debug.LogWarning("OriginalVideoScreens: " + report, this); return; }
            var skipped = new List<string>();
            foreach (var d in data)
            {
                bool colorRef = d.colorReference;
                // a target no drawn material samples (FunFactory's FMX_BV_JumboTron_Bink: no material of the level names it) shows nothing in
                // the original either: not decoded
                if (d.uses.Count == 0 && !colorRef) { skipped.Add(d.name + " #" + d.entityId + " (" + d.target + ": sampled by nothing)"); continue; }
                if (!targets.TryGetValue(d.target, out var t))
                {
                    var st = d.uses.Count > 0 ? d.uses[0].sampler : null;
                    TextureWrapMode Wrap(int key) => colorRef ? TextureWrapMode.Clamp : st != null && st.TryGetValue(key, out var w) ? (w == 3f || w == 4f ? TextureWrapMode.Clamp : w == 2f ? TextureWrapMode.Mirror : TextureWrapMode.Repeat) : TextureWrapMode.Repeat;
                    t = new Target { name = d.target, colorRef = colorRef };
                    t.rt = new RenderTexture(Math.Max(1, d.width), Math.Max(1, d.height), 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear)
                    { name = $"{d.target} (video {d.bink})", useMipMap = false, filterMode = FilterMode.Bilinear, wrapModeU = Wrap(1), wrapModeV = Wrap(2), hideFlags = HideFlags.DontSave };
                    t.rt.Create();
                    var prev = RenderTexture.active; RenderTexture.active = t.rt; GL.Clear(false, true, Color.clear); RenderTexture.active = prev;   // cleared to 0
                    targets[d.target] = t;
                }
                else t.colorRef |= colorRef;
                var f = feeds.FirstOrDefault(x => x.bink == d.bink);
                if (f == null) { f = new Feed { bink = d.bink, first = d }; feeds.Add(f); }
                if (!f.targets.Contains(t)) f.targets.Add(t);
                if (t.feed == null) t.feed = f;
                list.Add(new Screen { data = d, colorRef = colorRef, feed = f, tgt = t });
            }
            screens = list.Count;
            BindMaterials();
            report = string.Join("\n", data.Select(d => d.ToString())) + $"\n{feeds.Count} distinct video(s) for {targets.Count} target(s) / {list.Count} screen(s)" + (skipped.Count > 0 ? "; not decoded: " + string.Join(", ", skipped) : "");
            if (openPath && feeds.Count > 0 && FfmpegBinkSource.Locate() == null)
            {   // the open player has no decoder of its own: the targets stay at 0 (dark) as a missing video would leave them
                string msg = "ffmpeg not found: the in-level video screens (" + string.Join(", ", feeds.Select(x => x.bink)) + ") stay dark (install ffmpeg or set REFLEX_FFMPEG)";
                report += "\n" + msg; foreach (var f in feeds) f.failed = true;
                Debug.LogWarning("OriginalVideoScreens: " + msg, this);
                return;
            }
            Debug.Log($"[VideoScreens] {list.Count} screen(s) of {data.Count} VideoEntities: {feeds.Count} distinct video(s) [{string.Join(", ", feeds.Select(x => x.bink))}] into {targets.Count} target(s) [{string.Join(", ", targets.Values.Select(x => x.name + " " + x.rt.width + "x" + x.rt.height))}], decoded by {(openPath ? "ffmpeg" : "the plugin")}");
            if (!manual) { quit = false; thread = new Thread(Run) { IsBackground = true, Name = "Reflex video screens", Priority = System.Threading.ThreadPriority.BelowNormal }; thread.Start(); }
        }

        // ------------------------------------------------------------------------------------------------ materials

        static readonly string[] SceneryTextureSlots = { "_BaseMap", "_AOMap", "_DetailMap", "_NormalMap", "_ComboMap" };

        void BindMaterials()
        {
            var mats = content.SceneryMaterials;
            if (mats.Count == boundMaterialCount) return;
            boundMaterialCount = mats.Count;
            foreach (var s in list)
                foreach (var u in s.data.uses)
                {
                    if (!mats.TryGetValue(u.material, out var m) || !m || bound.Contains(m)) continue;
                    if (!content.sceneMaterials.TryGetValue(u.material, out var lm)) continue;
                    bool own = Array.IndexOf(ReflexVideoScreens.OwnPrograms, u.pixelShader) >= 0;
                    if (own) { if (!Retarget(m, lm, s)) continue; }
                    else
                    {
                        bool any = false;
                        foreach (var p in SceneryTextureSlots) if (m.HasTexture(p) && m.GetTexture(p) == Texture2D.blackTexture) { m.SetTexture(p, s.target); any = true; }
                        if (!any) { content.Warn($"video screen {s.data.name}: material {u.material} ({u.pixelShader}) has no slot for {s.data.target}"); continue; }
                    }
                    bound.Add(m); materialsBound = bound.Count;
                }
        }

        static readonly int ColorRefOn = Shader.PropertyToID("_RfxColorRefOn"), ColorRefMap = Shader.PropertyToID("_RfxColorRefMap");
        /// BuenaVista / FunFactory: the video is the light entity's _ColorReferenceMap, sampled by every lit program as s13 at
        /// gColorRefProjection (ReflexCommon RfxColorRefAt; ReflexObjectShading.ApplyLevelGlobals binds a static map the same way).
        void BindColorReference()
        {
            foreach (var s in list)
                if (s.colorRef && s.tgt.hasFrame) { Shader.SetGlobalTexture(ColorRefMap, s.target); Shader.SetGlobalFloat(ColorRefOn, 1); }   // only once a frame is in it: a dark map would darken every lit program
        }

        // FY_Projector (s0 video, s1 screen alpha) / SX_DLC_FY_Bink_ProjectionBeam (s0 dust, s1 video, s2 gradient, s3 beam mask)
        bool Retarget(Material m, ReflexLevelContent.LevelMaterial lm, Screen s)
        {
            if (!screenShader) { content.Warn("shader Reflex/Original Video Screen not found (Always Included Shaders)"); return false; }
            Texture2D T(int slot) => slot < lm.textures.Count && lm.textures[slot] != s.data.target ? content.SceneryTexture(lm.textures[slot], lm.samplerStates[slot]) : null;
            int queue = m.renderQueue;
            m.shader = screenShader;
            if (lm.pixelShader == "FY_Projector")
            {
                m.SetFloat("_Program", 0);
                m.SetTexture("_VideoTex", s.target);
                var mask = T(1); if (mask) m.SetTexture("_MaskTex", mask);
                m.SetFloat("_Overbright", lm.parameters.TryGetValue("usr_overbright", out var ob) && ob != null && ob.Length > 0 ? ob[Math.Min(3, ob.Length - 1)] : 5.12f);   // CTAB default (0, 0, 0, 5.12)
            }
            else
            {
                m.SetFloat("_Program", 1);
                m.SetTexture("_VideoTex", s.target);
                var dust = T(0); if (dust) m.SetTexture("_DustTex", dust);
                var grad = T(2); if (grad) m.SetTexture("_GradTex", grad);
                var mask = T(3); if (mask) m.SetTexture("_MaskTex", mask);
            }
            m.renderQueue = queue;   // ReflexSceneryMaterial's queue for the material's render states (both blend: Transparent)
            return true;
        }

        // ------------------------------------------------------------------------------------------------ decode thread

        void Open(Feed f)
        {
            try
            {
                // the player reads files (the legacy plugin's, ffmpeg's): the `bink` resource goes to a file of the per-user cache; the legacy decoder
                // reads it whole (BINKPRELOADALL: deleted at once), ffmpeg opens it again for the looping pass (deleted at Stop)
                f.file = ReflexVideoScreens.TempFile(cacheDir, content, f.first);
                IBinkSource src = null;
                string error = null; bool ok = false; ReflexBink.Info inf = default;
                if (!openPath)
                {
                    src = new NativeBinkSource(ReflexBink.Dll);
                    try { ok = src.Open(f.file, true, out inf, out error); }
                    catch (Exception e) when (e is DllNotFoundException || e is EntryPointNotFoundException) { error = e.Message; src.Dispose(); src = null; }   // no legacy plugin: ffmpeg
                }
                if (src == null)
                {
                    src = new FfmpegBinkSource();
                    ok = src.Open(f.file, true, out inf, out error);
                }
                if (!ok) { src.Dispose(); f.failed = true; f.error = error ?? "BinkOpen failed"; return; }
                f.src = src; f.info = inf;
                byte[][] Make() => new[] { new byte[inf.ywidth * inf.yheight], new byte[inf.cwidth * inf.cheight], new byte[inf.cwidth * inf.cheight], inf.hasAlpha != 0 ? new byte[inf.ywidth * inf.yheight] : null };
                lock (gate) { f.work = Make(); f.ready = Make(); f.shown = Make(); }
                src.Control(ReflexBink.Play, 1);   // XenonBinkVideo::vf1: BinkGoto(1), sound on, the first frame due at once (Loop: +0x7c = 1)
                f.clock = 0; f.opened = true;
            }
            catch (Exception e) { f.failed = true; f.error = e.Message; }
            finally
            {
                if (!openPath && f.file != null) try { File.Delete(f.file); f.file = null; } catch (Exception) { }
            }
        }

        /// One step of a video (XenonBinkVideo::vf8): decode when due, the loop at the last frame. Returns true on a new frame.
        bool Step(Feed f)
        {
            int r = f.src.Update(f.clock, out int frame, out int done);
            if (r < 0) { f.failed = true; f.error = "Bink: " + f.src.Error; return false; }
            if (r != 1) return false;
            f.src.Planes(f.work[0], f.work[1], f.work[2], f.work[3]);
            lock (gate) { (f.work, f.ready) = (f.ready, f.work); f.readySeq++; f.readyFrame = frame; }
            if (frame >= f.info.frames) { f.src.Control(ReflexBink.GotoStart, 0); f.loops++; }   // Loop (+0x7c = 1): vf4 BinkGoto(1)
            return true;
        }

        void Run()
        {
            var watch = Stopwatch.StartNew(); double last = 0;
            try
            {
                foreach (var f in feeds) { if (quit) return; Open(f); }
                while (!quit)
                {
                    double now = watch.Elapsed.TotalMilliseconds, dt = now - last; last = now;
                    bool p = pauseRequest;
                    foreach (var f in feeds)
                    {
                        if (!f.opened || f.failed) continue;
                        if (p != f.pausedNow) { f.pausedNow = p; f.src.Control(ReflexBink.Pause, p ? 1 : 0); }   // VideoEntity::vf19: BinkPause
                        if (p) continue;
                        f.clock += dt;
                        double t0 = watch.Elapsed.TotalMilliseconds;
                        if (Step(f))
                        {
                            double ms = watch.Elapsed.TotalMilliseconds - t0;
                            decodeTotal += ms; framesDecoded++; decodeMsWorst = Math.Max(decodeMsWorst, ms); decodeMsAverage = decodeTotal / framesDecoded;
                        }
                    }
                    wake.WaitOne(2);
                }
            }
            catch (Exception e) { Debug.LogWarning("OriginalVideoScreens: " + e.Message); }
        }

        /// Tests (manual mode, edit mode): opens the players on this thread and decodes each screen's video up to frame frameFor(screen)
        /// (1-based) on the video's own clock, then draws it into the targets.
        public void DecodeTo(Func<ReflexVideoScreens.Screen, int> frameFor)
        {
            foreach (var s in list)
            {
                var f = s.feed;
                if (!f.opened && !f.failed) Open(f);
                if (!f.opened) continue;
                int want = Math.Max(1, frameFor(s.data));
                double step = 1000.0 * Math.Max(1u, f.info.fpsDen) / Math.Max(1u, f.info.fpsNum);
                for (int guard = 0; guard < 4 * want + 8 && f.readyFrame < want; guard++)
                {
                    Step(f);
                    if (f.readyFrame < want) f.clock += step * 0.5;
                }
                if (f.readyFrame != want) Debug.LogWarning($"OriginalVideoScreens: {s.data.name}: decoded frame {f.readyFrame}, wanted {want}");
            }
            Upload();
            BindColorReference();   // as Tick does (BuenaVista / FunFactory: the colour reference target is s13 of every lit program)
        }

        // ------------------------------------------------------------------------------------------------ main thread

        // simulation thread -> main thread
        int queued; OriginalSimulation boundSim; Action<IntPtr> collect; bool collectedPause; bool searched;
        readonly byte[] b4 = new byte[4];

        void Collect(IntPtr w)
        {
            try
            {
                uint U(uint a) => ReflexWorldNative.reflex_world_read(w, a, b4, 4) == 0 ? BitConverter.ToUInt32(b4, 0) : 0u;
                if (!searched)
                {
                    int found = 0;
                    foreach (var s in list) { s.gameEntity = ReflexWorldNative.reflex_world_find_object(w, 0x939b2c, 0x14, (uint)s.data.entityId, 0x00400000, 0x7fff0000); if (s.gameEntity != 0) found++; }   // VideoEntity, +0x14 id
                    gameEntitiesFound = found; searched = true;
                }
                bool any = false;
                foreach (var s in list)
                {
                    if (s.gameEntity == 0) continue;
                    // VideoEntity::vf19: the scene of the entity (+0x10) picks the world [[0xa946b8]+0x5a54] or [+0x20c8]; its +0x20014 is the pause flag
                    uint mgr = U(0xa95844), g = U(0xa946b8), sceneOf = U(s.gameEntity + 0x10);
                    uint world = sceneOf != 0 && sceneOf == U(mgr + 0x20) ? U(g + 0x5a54) : sceneOf != 0 && sceneOf == U(mgr + 0x1c) ? U(g + 0x20c8) : 0;
                    if (world != 0 && U(world + 0x20014) != 0) any = true;
                }
                lock (gate) collectedPause = any;
            }
            catch (Exception e) { Debug.LogWarning("OriginalVideoScreens: " + e.Message); }
            finally { Interlocked.Exchange(ref queued, 0); }
        }

        void Update() { if (!manual) Tick(); }

        public void Tick()
        {
            if (list.Count == 0) return;
            var sw = Stopwatch.StartNew();
            if (!runtime) runtime = level && level.runtime ? level.runtime : FindAnyObjectByType<OriginalWorldRuntime>();
            var sim = useGameState && !openPath && runtime && runtime.Running ? runtime.Simulation : null;   // the world read is the legacy plugin's; the open path pauses with runtime.paused
            if (sim != boundSim) { boundSim = sim; searched = false; Interlocked.Exchange(ref queued, 0); lock (gate) collectedPause = false; }
            collect ??= Collect;
            if (sim != null && Interlocked.Exchange(ref queued, 1) == 0) sim.BetweenFrames(collect);
            lock (gate) gamePaused = collectedPause;
            paused = gamePaused || (runtime && runtime.paused);
            pauseRequest = paused;
            BindMaterials();
            BindColorReference();
            Upload();
            foreach (var f in feeds) if (f.failed && f.error != null) { Debug.LogWarning($"OriginalVideoScreens: {f.bink}: {f.error}"); f.error = null; }
            mainTotal += sw.Elapsed.TotalMilliseconds; mainFrames++;
            mainMsAverage = mainTotal / mainFrames;
        }

        void Upload()
        {
            foreach (var f in feeds)
            {
                if (!f.opened) continue;
                bool fresh = false;
                lock (gate) if (f.readySeq != f.shownSeq) { (f.shown, f.ready) = (f.ready, f.shown); f.shownSeq = f.readySeq; f.shownFrame = f.readyFrame; fresh = true; }
                if (!fresh) continue;
                if (f.planes == null) Setup(f);
                for (int i = 0; i < 4; i++) if (f.planes[i] && f.shown[i] != null) { f.planes[i].LoadRawTextureData(f.shown[i]); f.planes[i].Apply(false, false); }   // once per video
                var prev = RenderTexture.active;
                foreach (var t in f.targets) { Graphics.Blit(null, t.rt, f.yuv); t.hasFrame = true; }   // 00594a40: the quad over the whole target, UV 0..1 over the whole planes
                RenderTexture.active = prev;
                framesShown++;
            }
        }

        void Setup(Feed f)
        {
            var inf = f.info;
            Texture2D Plane(uint w, uint h, string n) => new Texture2D((int)w, (int)h, TextureFormat.R8, false, true) { name = f.bink + " " + n, filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            f.planes = new[] { Plane(inf.ywidth, inf.yheight, "Y"), Plane(inf.cwidth, inf.cheight, "cR"), Plane(inf.cwidth, inf.cheight, "cB"), inf.hasAlpha != 0 ? Plane(inf.ywidth, inf.yheight, "A") : null };
            f.yuv = new Material(binkShader) { name = "Bink " + f.bink, hideFlags = HideFlags.DontSave };
            f.yuv.SetTexture("_YTex", f.planes[0]); f.yuv.SetTexture("_CrTex", f.planes[1]); f.yuv.SetTexture("_CbTex", f.planes[2]);
            if (f.planes[3]) f.yuv.SetTexture("_ATex", f.planes[3]);
            f.yuv.SetFloat("_Alpha", inf.hasAlpha != 0 ? 1 : 0);
            f.yuv.SetFloat("_SrgbTarget", 0);   // a linear target: the bytes are the game's (sampled raw by the world shaders)
        }

        void Stop()
        {
            quit = true; wake.Set();
            if (thread != null && !thread.Join(3000)) Debug.LogWarning("OriginalVideoScreens: decode thread did not stop");
            thread = null;
            foreach (var f in feeds)
            {
                if (f.src != null) { f.src.Dispose(); f.src = null; }
                if (f.file != null) { try { File.Delete(f.file); } catch (Exception) { } f.file = null; }
                if (f.planes != null) foreach (var t in f.planes) if (t) Kill(t);
                if (f.yuv) Kill(f.yuv);
            }
            foreach (var t in targets.Values) if (t.rt) { t.rt.Release(); Kill(t.rt); }
            if (list.Any(s => s.colorRef)) Shader.SetGlobalFloat(ColorRefOn, 0);   // the target is gone (the next level's ApplyLevelGlobals binds its own)
            list.Clear(); feeds.Clear(); targets.Clear(); bound.Clear(); boundMaterialCount = -1; screens = 0; materialsBound = 0;
            framesDecoded = framesShown = 0; decodeTotal = mainTotal = 0; mainFrames = 0; decodeMsAverage = decodeMsWorst = mainMsAverage = 0;
        }

        static void Kill(UnityEngine.Object o) { if (Application.isPlaying) Destroy(o); else DestroyImmediate(o); }

        void OnDestroy()
        {
            if (framesDecoded > 0) Debug.Log($"[VideoScreens] {screens} screens ({feeds.Count} videos, {materialsBound} materials bound): {framesDecoded} frames decoded, {decodeMsAverage:0.00} ms average, {decodeMsWorst:0.0} ms worst (decode thread); main thread {mainMsAverage:0.000} ms per frame");
            Stop();
        }
    }
}
