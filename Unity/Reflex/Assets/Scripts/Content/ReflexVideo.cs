// Bink videos of the install (Videos/*.bik) played with the install's own decoder: <install>/binkw32.dll runs unchanged in the
// plugin's x86 interpreter / JIT (native bink.cpp, _re/notes/bink.md), driven exactly as the game's XenonBinkVideo (MXReflex.exe
// 0x5950b0..0x595880) drives it. Nothing of the videos is in the project: the .bik and the DLL are read from ReflexInstall.
// The OPEN REFLEX player bundles no legacy plugin: there (or whenever the plugin is missing) the same .bik files are decoded by the machine's
// ffmpeg through a pipe (ReflexVideoSource.cs: the raw Y / U / V / A planes, drawn by the same Reflex/Bink Video program).
//
// OriginalVideo (one per video) = the game's XenonBinkVideo + its render target:
// * a decode thread runs the game's per-frame step (XenonBinkVideo::vf8 00595470: decode when due, BinkWait) against a clock that
//   starts when the first frame is shown (Bink's own timer starts at its first BinkWait), so the frames come at the file's rate;
// * the four planes (Y, cR, cB, A; A8 in the game, R8 here) are uploaded as the game uploads them and turned into the render
//   target by Reflex/Bink Video (the game's ps_2_0 YUV / YUVA programs, hand-ported): the target is the layer's TextureAsset
//   (1280_720_VideoRenderTarget ...), the whole video stretched over it as the game's quad (00594a40) does;
// * the sound is the PCM the DLL hands to its sound system (the BINKSND interface implemented natively), streamed to an
//   AudioSource; volume as the game's DirectSound glue applies BinkSetVolume (0x18011bc0: 3333.33 * log10(v / 32767) hundredths
//   of a dB, i.e. gain (v / 32767)^(5/3)).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Content
{
    public static class ReflexBink
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct Info
        {
            public uint width, height, frames, fpsNum, fpsDen, hasAlpha, audioRate, audioChannels, audioBits, ywidth, yheight, cwidth, cheight;
            public double Fps => fpsDen == 0 ? 30 : (double)fpsNum / fpsDen;
        }
        const string Lib = "reflexworld";
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_bink_open__fn(string dll, string file, int preload, out Info info);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_open__fn> reflex_bink_open__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_open__fn>("reflex_bink_open");
        public static IntPtr reflex_bink_open(string dll, string file, int preload, out Info info) => reflex_bink_open__p.Get()(dll, file, preload, out info);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_bink_update__fn(IntPtr player, double ms, out int frame, out int done);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_update__fn> reflex_bink_update__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_update__fn>("reflex_bink_update");
        public static int reflex_bink_update(IntPtr player, double ms, out int frame, out int done) => reflex_bink_update__p.Get()(player, ms, out frame, out done);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_bink_planes__fn(IntPtr player, byte[] y, byte[] cr, byte[] cb, byte[] a);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_planes__fn> reflex_bink_planes__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_planes__fn>("reflex_bink_planes");
        public static int reflex_bink_planes(IntPtr player, byte[] y, byte[] cr, byte[] cb, byte[] a) => reflex_bink_planes__p.Get()(player, y, cr, cb, a);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_bink_audio__fn(IntPtr player, short[] output, int max);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_audio__fn> reflex_bink_audio__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_audio__fn>("reflex_bink_audio");
        public static int reflex_bink_audio(IntPtr player, short[] output, int max) => reflex_bink_audio__p.Get()(player, output, max);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_bink_control__fn(IntPtr player, int what, int value);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_control__fn> reflex_bink_control__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_control__fn>("reflex_bink_control");
        public static int reflex_bink_control(IntPtr player, int what, int value) => reflex_bink_control__p.Get()(player, what, value);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_bink_error__fn(IntPtr player);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_error__fn> reflex_bink_error__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_error__fn>("reflex_bink_error");
        static IntPtr reflex_bink_error(IntPtr player) => reflex_bink_error__p.Get()(player);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate void reflex_bink_close__fn(IntPtr player);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_close__fn> reflex_bink_close__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_bink_close__fn>("reflex_bink_close");
        public static void reflex_bink_close(IntPtr player) => reflex_bink_close__p.Get()(player);
        public static string Error(IntPtr player) => Marshal.PtrToStringAnsi(reflex_bink_error(player)) ?? "";

        public const int GotoStart = 1, Pause = 2, SoundOnOff = 3, Volume = 4, Play = 5;

        /// The install's decoder and a video by the game's name ("Videos\\Title" -> <install>/Videos/Title.bik; XenonBinkVideoManager::vf1
        /// 00595a00 formats "%s%s%s" with ".bik", the PAL variant only on PAL consoles).
        public static string Dll => ReflexInstall.PathOf("binkw32.dll");
        public static string PathOf(string video) => ReflexInstall.PathOf(video.EndsWith(".bik", StringComparison.OrdinalIgnoreCase) ? video : video + ".bik");

        /// The game's DirectSound glue (binkw32 0x18011bc0): BinkSetVolume value -> linear gain.
        public static float Gain(int binkVolume) => binkVolume <= 0 ? 0f : Mathf.Pow(Mathf.Min(binkVolume, 32767) / 32767f, 5f / 3f);
    }

    /// One Bink video of the install (the game's XenonBinkVideo and its render target). Create with OriginalVideo.Create, then
    /// Play / Stop; Texture is the render target; Done / Looped are raised on the main thread as XenonBinkVideo::vf8 reports them.
    [ExecuteAlways]
    public sealed class OriginalVideo : MonoBehaviour
    {
        public string video;                 // the game's name, "Videos\\Title"
        public bool loadToMemory;            // BINKPRELOADALL (UIVideoComponent LoadToMemory)
        public bool loop;                    // UIVideoComponent Loop (XenonBinkVideo+0x7c): at the last frame BinkGoto(1) and Looped
        public Vector2Int targetSize;        // the TextureAsset render target (0: the video's own size)
        [Range(0, 1)] public float volume = 1f;   // the game's volume product (XenonBinkVideo::vf5 00595410: BinkSetVolume(round(v * 32768)))

        public RenderTexture Texture { get; private set; }
        public ReflexBink.Info Info => info;
        public bool IsOpen => opened;
        public bool IsPlaying => playing;
        public bool HasFrame { get; private set; }
        public int Frame => shownFrame;
        public string Error => error;
        public bool Failed => failed;           // BinkOpen / the decoder failed (the caller goes on as after the last frame)
        public event Action Done, Looped;
        /// Decode cost on the decode thread (ms per decoded frame, BinkDoFrame + BinkNextFrame + plane copy).
        public double DecodeMsAverage => decoded > 0 ? decodeMs / decoded : 0;
        public double DecodeMsWorst => decodeWorst;
        public int FramesDecoded => decoded;
        public int FramesShown => framesShown;
        /// Main-thread cost of a new frame (plane upload + the YUV draw into the target), ms average.
        public double UploadMsAverage => framesShown > 0 ? uploadMs / framesShown : 0;
        double uploadMs;
        /// Sound samples (per channel) the AudioSource has taken from the decoder's PCM so far.
        public long AudioSamplesPlayed => info.audioChannels > 0 ? Interlocked.Read(ref audioTaken) / info.audioChannels : 0;
        long audioTaken;

        ReflexBink.Info info;
        volatile bool opened, failed, quit, playing;
        string error;
        Thread thread;
        readonly AutoResetEvent wake = new AutoResetEvent(false);
        readonly object gate = new object();
        readonly Queue<int> commands = new Queue<int>();
        byte[][] work, ready, shown;
        int readySeq, shownSeq, readyFrame, shownFrame;
        int doneSeq, doneSeen, loopSeq, loopSeen;
        // clock (monotonic, as RADTimerRead): held from a Play until its first frame is on screen, then running (BinkGoto(1) restarts
        // Bink's own timer at the next BinkWait, so the frames and the sound start together)
        readonly Stopwatch watch = Stopwatch.StartNew();
        double clockBase, heldMs; long lastUs; volatile bool clockRunning; bool waitingFirst;
        // stats
        double decodeMs, decodeWorst; int decoded, framesShown;
        // pictures
        Texture2D[] planes;
        Material material;
        // sound
        AudioSource source;
        AudioClip clip;
        float[] ring; int ringRead, ringWrite, ringCount;
        int binkVolume = 32768;

        public static OriginalVideo Create(GameObject host, string video, bool loadToMemory, bool loop = false, Vector2Int targetSize = default)
        {
            var v = host.AddComponent<OriginalVideo>();
            v.video = video; v.loadToMemory = loadToMemory; v.loop = loop; v.targetSize = targetSize;
            return v;
        }

        /// Opens the video (XenonBinkVideoManager::vf1: BinkOpen) on the decode thread; Play may come at once.
        public void Open()
        {
            if (thread != null) return;
            string dll = null, file;
            bool open = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex;   // the open player bundles no binkw32 interpreter: ffmpeg decodes the .bik
            try { if (!open) dll = ReflexBink.Dll; file = ReflexBink.PathOf(video); }
            catch (Exception e) { Fail(e.Message); return; }
            if (!File.Exists(file)) { Fail(file + " not in the install"); return; }
            if (open && FfmpegBinkSource.Locate() == null) { Fail("ffmpeg not found: needed to play " + video + " (REFLEX_FFMPEG)"); return; }
            quit = false;
            thread = new Thread(() => Run(dll, file, open)) { IsBackground = true, Name = "Bink " + video, Priority = System.Threading.ThreadPriority.AboveNormal };
            thread.Start();
        }

        /// XenonBinkVideo::vf1 00595210 (UIVideoComponent Play): volume, BinkGoto(1), sound on, the first frame decoded at once.
        public void Play()
        {
            Open();
            lock (gate) { commands.Enqueue(ReflexBink.Play); doneSeen = doneSeq; loopSeen = loopSeq; }
            heldMs = Volatile.Read(ref lastUs) / 1000.0; clockRunning = false; playing = true; waitingFirst = true; HasFrame = false;
            if (source) { source.Stop(); lock (gate) { ringRead = ringWrite = ringCount = 0; } }
            wake.Set();
        }

        /// XenonBinkVideo::vf3 005952f0 (Stop): playing off, BinkSetSoundOnOff(0). The last frame stays in the target.
        public void Stop()
        {
            if (!playing && thread == null) return;
            playing = false;
            lock (gate) commands.Enqueue(-ReflexBink.Play);
            if (source) source.Stop();
            wake.Set();
        }

        /// BinkClose (XenonBinkVideoManager::vf3 / vf5): ends the decode thread and the player.
        public void Close()
        {
            playing = false; quit = true; wake.Set();
            if (thread != null && !thread.Join(2000)) Debug.LogWarning("[Video] " + video + ": decode thread did not stop");
            thread = null; opened = false;
            if (source) source.Stop();
        }

        public void SetVolume(float v)
        {
            volume = Mathf.Clamp01(v);
            binkVolume = (int)Math.Round(volume * 32768.0);   // 00595440: _ftol(copysign(0.5, x) + x), x = v * 32768
            if (source) source.volume = ReflexBink.Gain(binkVolume);
            lock (gate) commands.Enqueue(1000 + binkVolume);
            wake.Set();
        }

        void Fail(string e) { error = e; failed = true; playing = false; Debug.LogWarning("[Video] " + video + ": " + e); }

        // -------------------------------------------------------------------------------------------- decode thread
        void Run(string dll, string file, bool useFfmpeg)
        {
            IBinkSource p = null;
            try
            {
                ReflexBink.Info inf = default; string openError = null; bool ok = false;
                if (!useFfmpeg)
                {
                    p = new NativeBinkSource(dll);
                    try { ok = p.Open(file, loadToMemory, out inf, out openError); }
                    catch (Exception e) when (e is DllNotFoundException || e is EntryPointNotFoundException) { openError = e.Message; useFfmpeg = true; }   // no legacy plugin: ffmpeg decodes the file
                    if (!ok && useFfmpeg) { p.Dispose(); p = null; }
                }
                if (useFfmpeg)
                {
                    p = new FfmpegBinkSource();
                    ok = p.Open(file, loadToMemory, out inf, out openError);
                }
                if (!ok) { error = openError ?? "BinkOpen failed"; failed = true; return; }
                info = inf;
                byte[][] Make() => new[] { new byte[inf.ywidth * inf.yheight], new byte[inf.cwidth * inf.cheight], new byte[inf.cwidth * inf.cheight], inf.hasAlpha != 0 ? new byte[inf.ywidth * inf.yheight] : null };
                lock (gate)
                {
                    work = Make(); ready = Make(); shown = Make();
                    if (inf.audioRate > 0) ring = new float[(int)(inf.audioRate * inf.audioChannels * 4)];
                }
                p.Control(ReflexBink.Volume, binkVolume);
                opened = true;
                var pcm = new short[16384];
                bool run = false; var sw = new Stopwatch();
                while (!quit)
                {
                    int cmd = 0;
                    lock (gate) while (commands.Count > 0)
                    {
                        cmd = commands.Dequeue();
                        if (cmd == ReflexBink.Play) { p.Control(ReflexBink.Play, loop ? 1 : 0); run = true; }
                        else if (cmd == -ReflexBink.Play) { p.Control(ReflexBink.SoundOnOff, 0); run = false; }
                        else if (cmd >= 1000) p.Control(ReflexBink.Volume, cmd - 1000);
                    }
                    if (run)
                    {
                        double ms = clockRunning ? watch.Elapsed.TotalMilliseconds - clockBase : heldMs;
                        Volatile.Write(ref lastUs, (long)(ms * 1000));
                        sw.Restart();
                        int r = p.Update(ms, out int frame, out int done);
                        if (r < 0) { error = "Bink: " + p.Error; failed = true; run = false; playing = false; break; }
                        if (r == 1)
                        {
                            p.Planes(work[0], work[1], work[2], work[3]);
                            double t = sw.Elapsed.TotalMilliseconds; decodeMs += t; decodeWorst = Math.Max(decodeWorst, t); decoded++;
                            lock (gate) { (work, ready) = (ready, work); readySeq++; readyFrame = frame; }
                            // XenonBinkVideo::vf8 00595621: the last frame -> loop (vf4: BinkGoto(1), Looped) or stop (vf3: sound off, Done)
                            if (frame >= info.frames)
                            {
                                if (loop) { p.Control(ReflexBink.GotoStart, 0); Interlocked.Increment(ref loopSeq); }
                                else { p.Control(ReflexBink.SoundOnOff, 0); run = false; Interlocked.Increment(ref doneSeq); }
                            }
                        }
                        if (ring != null)
                            for (int n; (n = p.Audio(pcm, pcm.Length)) > 0;)
                                lock (gate) for (int i = 0; i < n; i++)
                                {
                                    if (ringCount == ring.Length) { ringRead = (ringRead + 1) % ring.Length; ringCount--; }   // overflow: drop the oldest
                                    ring[ringWrite] = pcm[i] / 32768f; ringWrite = (ringWrite + 1) % ring.Length; ringCount++;
                                }
                    }
                    wake.WaitOne(run ? 1 : 50);
                }
            }
            catch (Exception e) { error = e.Message; failed = true; }
            finally { p?.Dispose(); }
        }

        void OnRead(float[] data)
        {
            lock (gate)
            {
                int n = Math.Min(data.Length, ringCount);
                for (int i = 0; i < n; i++) { data[i] = ring[ringRead]; ringRead = (ringRead + 1) % ring.Length; }
                ringCount -= n; audioTaken += n;
                for (int i = n; i < data.Length; i++) data[i] = 0;
            }
        }

        // -------------------------------------------------------------------------------------------- main thread
        void Update() => Pump();
        /// The main thread's part (Update; edit-mode callers drive it themselves): the new frame into the target, the clock start,
        /// the events.
        public void Pump()
        {
            if (failed && error != null) { Debug.LogWarning("[Video] " + video + ": " + error); error = null; }
            if (!opened) return;
            if (planes == null) Setup();
            bool fresh = false;
            lock (gate) if (readySeq != shownSeq) { (shown, ready) = (ready, shown); shownSeq = readySeq; shownFrame = readyFrame; fresh = true; }
            if (fresh)
            {
                long t0 = Stopwatch.GetTimestamp();
                for (int i = 0; i < 4; i++) if (planes[i] && shown[i] != null) { planes[i].LoadRawTextureData(shown[i]); planes[i].Apply(false, false); }
                var prev = RenderTexture.active;
                Graphics.Blit(null, Texture, material);
                RenderTexture.active = prev;
                HasFrame = true; framesShown++; uploadMs += (Stopwatch.GetTimestamp() - t0) * 1000.0 / Stopwatch.Frequency;
                if (waitingFirst)
                {   // the first frame of a Play is on screen: Bink's clock and the sound start now
                    waitingFirst = false; clockBase = watch.Elapsed.TotalMilliseconds - heldMs; clockRunning = true;
                    if (source && playing) source.Play();
                }
            }
            if (doneSeen != doneSeq) { doneSeen = doneSeq; playing = false; if (source) source.Stop(); Done?.Invoke(); }   // vf3: sound off
            if (loopSeen != loopSeq) { loopSeen = loopSeq; Looped?.Invoke(); }
        }

        void Setup()
        {
            var inf = info;
            Texture2D Plane(uint w, uint h, string n) => new Texture2D((int)w, (int)h, TextureFormat.R8, false, true) { name = video + " " + n, filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            planes = new[] { Plane(inf.ywidth, inf.yheight, "Y"), Plane(inf.cwidth, inf.cheight, "cR"), Plane(inf.cwidth, inf.cheight, "cB"), inf.hasAlpha != 0 ? Plane(inf.ywidth, inf.yheight, "A") : null };
            int tw = targetSize.x > 0 ? targetSize.x : (int)inf.width, th = targetSize.y > 0 ? targetSize.y : (int)inf.height;
            bool srgb = QualitySettings.activeColorSpace == ColorSpace.Linear;
            Texture = new RenderTexture(tw, th, 0, RenderTextureFormat.ARGB32, srgb ? RenderTextureReadWrite.sRGB : RenderTextureReadWrite.Linear)
            { name = video + " (" + tw + "x" + th + " video render target)", filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            Texture.Create();
            var prev = RenderTexture.active; RenderTexture.active = Texture; GL.Clear(false, true, Color.clear); RenderTexture.active = prev;   // the game clears it to 0
            var shader = Shader.Find("Reflex/Bink Video");
            material = new Material(shader) { name = "Bink " + video, hideFlags = HideFlags.DontSave };
            material.SetTexture("_YTex", planes[0]); material.SetTexture("_CrTex", planes[1]); material.SetTexture("_CbTex", planes[2]);
            if (planes[3]) material.SetTexture("_ATex", planes[3]);
            material.SetFloat("_Alpha", inf.hasAlpha != 0 ? 1 : 0); material.SetFloat("_SrgbTarget", srgb ? 1 : 0);
            if (inf.audioRate > 0)
            {
                source = gameObject.AddComponent<AudioSource>();
                source.playOnAwake = false; source.spatialBlend = 0; source.loop = true; source.volume = ReflexBink.Gain(binkVolume); source.bypassReverbZones = true;
                clip = AudioClip.Create(video + " (Bink sound)", (int)inf.audioRate * 3600, (int)inf.audioChannels, (int)inf.audioRate, true, OnRead);
                source.clip = clip;
                if (playing && !waitingFirst) source.Play();
            }
        }

        static void Kill(UnityEngine.Object o) { if (Application.isPlaying) Destroy(o); else DestroyImmediate(o); }
        void OnDestroy()
        {
            Close();
            if (Texture) { Texture.Release(); Kill(Texture); }
            if (planes != null) foreach (var t in planes) if (t) Kill(t);
            if (material) Kill(material);
            if (clip) Kill(clip);
            if (decoded > 0) Debug.Log($"[Video] {video}: {decoded} frames decoded, {DecodeMsAverage:0.00} ms average, {decodeWorst:0.0} ms worst ({info.width}x{info.height} at {info.Fps:0.##} fps), {AudioSamplesPlayed} sound samples played");
        }
    }
}
