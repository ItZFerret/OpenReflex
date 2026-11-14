// The decoders behind OriginalVideo (ReflexVideo.cs): the Bink step the decode thread drives, and the two ways of getting it.
//
//  * NativeBinkSource: the install's binkw32.dll in libreflexworld's x86 interpreter (the legacy plugin, _re/notes/bink.md). The OPEN
//    REFLEX player bundles no legacy plugin (OpenReflexBuild), so it has no Bink decoder of its own.
//  * FfmpegBinkSource: the machine's own ffmpeg decodes the same .bik files (FFmpeg's Bink 1 decoder, "binkvideo" + "binkaudio_dct")
//    into the raw planes the game's YUV program takes: `ffmpeg -i x.bik -f rawvideo -pix_fmt yuv420p|yuva420p pipe:1` is the file's own
//    Y / U / V (/ A) planes with no colour conversion, so Reflex/Bink Video (the game's ps_2_0 programs) still draws the picture and the
//    frame timing, the end / loop events and the volume are OriginalVideo's, unchanged. Nothing is converted to a file and nothing of the
//    videos is committed: the pipe holds the frames and ffmpeg is found on the machine (REFLEX_FFMPEG, the usual install folders, PATH);
//    without it the video fails the way a missing file does (the page goes on as after the last frame).
//    Differences from binkw32: the planes are the decoder's, padded to 16 like Bink's YABuffer; frames are due at the header's rate
//    from the first update after a Play (BinkWait's clock) and a late frame drops the ones before it (BinkShouldSkip).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Content
{
    /// What OriginalVideo's decode thread needs of a Bink player (the reflex_bink_* calls).
    internal interface IBinkSource : IDisposable
    {
        /// Opens the file; false (with error) when it cannot be.
        bool Open(string file, bool preload, out ReflexBink.Info info, out string error);
        /// ReflexBink.Play (value: 1 for a looping video), GotoStart, Pause, SoundOnOff, Volume.
        void Control(int what, int value);
        /// The game's per-frame step at time ms: 1 when a new frame was decoded, 0 otherwise, &lt; 0 on error.
        int Update(double ms, out int frame, out int done);
        void Planes(byte[] y, byte[] cr, byte[] cb, byte[] a);
        /// Interleaved 16-bit PCM not taken yet.
        int Audio(short[] output, int max);
        string Error { get; }
    }

    internal sealed class NativeBinkSource : IBinkSource
    {
        readonly string dll; IntPtr p;
        public NativeBinkSource(string dll) { this.dll = dll; }
        public string Error => ReflexBink.Error(p);
        public bool Open(string file, bool preload, out ReflexBink.Info info, out string error)
        {
            p = ReflexBink.reflex_bink_open(dll, file, preload ? 1 : 0, out info);
            error = p == IntPtr.Zero ? "BinkOpen: " + ReflexBink.Error(IntPtr.Zero) : null;
            return p != IntPtr.Zero;
        }
        public void Control(int what, int value) => ReflexBink.reflex_bink_control(p, what, what == ReflexBink.Play ? 0 : value);
        public int Update(double ms, out int frame, out int done) => ReflexBink.reflex_bink_update(p, ms, out frame, out done);
        public void Planes(byte[] y, byte[] cr, byte[] cb, byte[] a) => ReflexBink.reflex_bink_planes(p, y, cr, cb, a);
        public int Audio(short[] output, int max) => ReflexBink.reflex_bink_audio(p, output, max);
        public void Dispose() { if (p != IntPtr.Zero) ReflexBink.reflex_bink_close(p); p = IntPtr.Zero; }
    }

    internal sealed class FfmpegBinkSource : IBinkSource
    {
        // ------------------------------------------------------------------------------------------------ ffmpeg
        static string located; static bool searched;
        /// The ffmpeg executable: REFLEX_FFMPEG, the copy bundled with the client (<client root>/ffmpeg: the Windows package ships an
        /// LGPL build there), the usual install folders, then PATH. Null when there is none.
        /// Locate, searching again when the last search found nothing or what it found is gone (an ffmpeg put in place while the game runs).
        public static string Relocate()
        {
            try { if (searched && (located == null || !File.Exists(located))) { searched = false; located = null; } } catch (Exception) { }
            return Locate();
        }

        public static string Locate()
        {
            if (searched) return located;
            searched = true;
            string exe = Environment.OSVersion.Platform == PlatformID.Win32NT ? "ffmpeg.exe" : "ffmpeg";
            var dirs = new List<string>();
            string env = Environment.GetEnvironmentVariable("REFLEX_FFMPEG");
            if (!string.IsNullOrEmpty(env)) { if (File.Exists(env)) { located = env; return located; } dirs.Add(env); }
            try { string root = ReflexMods.ClientRoot; if (!string.IsNullOrEmpty(root)) dirs.Add(Path.Combine(root, "ffmpeg")); } catch (Exception) { }
            try { string own = ReflexMods.PlayerRoot; if (!string.IsNullOrEmpty(own)) dirs.Add(Path.Combine(own, "ffmpeg")); } catch (Exception) { }   // (beside the player under a launcher's client root)
            dirs.AddRange(new[] { "/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin", "/usr/bin" });
            foreach (string d in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(Path.PathSeparator)) if (d.Length > 0) dirs.Add(d);
            foreach (string d in dirs)
            {
                try { string f = Path.Combine(d, exe); if (File.Exists(f)) { located = f; return located; } } catch (Exception) { }
            }
            return null;
        }

        static Process Spawn(string exe, List<string> args, Action<string> err)
        {
            var psi = new ProcessStartInfo(exe) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, RedirectStandardInput = false, CreateNoWindow = true };
            foreach (string a in args) psi.ArgumentList.Add(a);
            var pr = Process.Start(psi);
            pr.ErrorDataReceived += (s, e) => { if (e.Data != null) err(e.Data); };
            pr.BeginErrorReadLine();
            return pr;
        }

        static void Kill(Process pr)
        {
            if (pr == null) return;
            try { if (!pr.HasExited) pr.Kill(); } catch (Exception) { }
            try { pr.Dispose(); } catch (Exception) { }
        }

        // ------------------------------------------------------------------------------------------------ state
        string file, ffmpeg, lastError = "";
        ReflexBink.Info inf;
        int frames; double frameMs;
        bool alpha;
        // video
        Process vp; Stream vs; bool vpLoop, vpHasFrames, vsyncFallback;
        byte[] raw; int ylen, clen;              // one raw frame: Y, U, V (, A)
        int curFrame; bool haveFrame;            // 1-based frame of this pass; any frame decoded since the open
        double t0; bool t0Set, ended;
        // audio
        Process ap; Thread at; volatile bool atQuit; int audioGen;
        readonly Queue<byte[]> aq = new Queue<byte[]>(); int aqBytes, aqHead; long audioTaken;
        double lastMs; int audioBytesPerSample = 2;

        public string Error => lastError;

        public bool Open(string path, bool preload, out ReflexBink.Info info, out string error)
        {
            info = default; error = null; file = path;
            ffmpeg = Locate();
            if (ffmpeg == null) { error = "ffmpeg not found (needed to play the Bink videos: install it, or point REFLEX_FFMPEG at it)"; return false; }
            try
            {
                // the Bink 1 header: "BIK?" size frames largest frames width height rate rate-div flags tracks, then per track the largest
                // frame, rate and flags
                var h = new byte[64];
                using (var f = File.OpenRead(path)) if (f.Read(h, 0, 64) < 52) { error = "not a Bink file"; return false; }
                if (h[0] != 'B' || h[1] != 'I' || h[2] != 'K') { error = "not a Bink file"; return false; }
                uint U(int o) => BitConverter.ToUInt32(h, o);
                inf.width = U(20); inf.height = U(24); inf.frames = U(8); inf.fpsNum = U(28); inf.fpsDen = U(32);
                alpha = (U(36) & 0x00100000) != 0; inf.hasAlpha = alpha ? 1u : 0u;
                uint tracks = U(40);
                if (tracks > 0 && 44 + 4 * tracks + 4 <= 64)
                {
                    int o = 44 + 4 * (int)tracks; inf.audioRate = BitConverter.ToUInt16(h, o);
                    inf.audioChannels = (BitConverter.ToUInt16(h, o + 2 * (int)tracks) & 0x2000) != 0 ? 2u : 1u;   // BINK_AUD_STEREO
                    inf.audioBits = 16;
                }
                if (inf.width == 0 || inf.height == 0 || inf.frames == 0 || inf.fpsNum == 0 || inf.fpsDen == 0) { error = "bad Bink header"; return false; }
                inf.ywidth = (inf.width + 15) & ~15u; inf.yheight = (inf.height + 15) & ~15u;   // Bink's YABuffer (CheckerEffectAlpha 1098x398 -> 1104x400)
                inf.cwidth = inf.ywidth / 2; inf.cheight = inf.yheight / 2;
                frames = (int)inf.frames; frameMs = 1000.0 * inf.fpsDen / inf.fpsNum;
                ylen = (int)(inf.ywidth * inf.yheight); clen = (int)(inf.cwidth * inf.cheight);
                raw = new byte[ylen + 2 * clen + (alpha ? ylen : 0)];
                info = inf;
                StartVideo(false);
                return true;
            }
            catch (Exception e) { error = e.Message; return false; }
        }

        void StartVideo(bool loop)
        {
            StopVideo();
            var a = new List<string> { "-nostdin", "-v", "error" };
            if (loop) { a.Add("-stream_loop"); a.Add("-1"); }
            a.Add("-i"); a.Add(file);
            a.AddRange(new[] { "-map", "0:v:0" });
            if (inf.ywidth != inf.width || inf.yheight != inf.height) { a.Add("-vf"); a.Add($"pad={inf.ywidth}:{inf.yheight}:0:0"); }
            a.AddRange(new[] { "-f", "rawvideo", "-pix_fmt", alpha ? "yuva420p" : "yuv420p" });
            if (vsyncFallback) { a.Add("-vsync"); a.Add("0"); } else { a.Add("-fps_mode"); a.Add("passthrough"); }   // every frame once, as it is
            a.Add("pipe:1");
            vp = Spawn(ffmpeg, a, s => lastError = s);
            vs = vp.StandardOutput.BaseStream; vpLoop = loop; vpHasFrames = false;
        }

        void StopVideo() { Kill(vp); vp = null; vs = null; }

        bool ReadFrame()
        {
            int n = 0;
            try { while (n < raw.Length) { int r = vs.Read(raw, n, raw.Length - n); if (r <= 0) break; n += r; } }
            catch (Exception) { }
            if (n == raw.Length) { vpHasFrames = true; return true; }
            return false;
        }

        // ------------------------------------------------------------------------------------------------ audio
        void StartAudio()
        {
            StopAudio();
            if (inf.audioRate == 0) return;
            var a = new List<string> { "-nostdin", "-v", "error", "-i", file, "-map", "0:a:0", "-f", "s16le", "-acodec", "pcm_s16le",
                                       "-ac", inf.audioChannels.ToString(), "-ar", inf.audioRate.ToString(), "pipe:1" };
            ap = Spawn(ffmpeg, a, s => lastError = s);
            var proc = ap; var stream = ap.StandardOutput.BaseStream; int gen = ++audioGen;
            lock (aq) { aq.Clear(); aqBytes = 0; aqHead = 0; audioTaken = 0; }
            atQuit = false;
            at = new Thread(() =>
            {
                var buf = new byte[16384];
                try
                {
                    while (!atQuit && gen == audioGen)
                    {
                        lock (aq) while (aqBytes > inf.audioRate * inf.audioChannels * 2 * 3 && !atQuit && gen == audioGen) Monitor.Wait(aq, 50);   // 3 s ahead at most
                        int r = stream.Read(buf, 0, buf.Length);
                        if (r <= 0) break;
                        var c = new byte[r]; Buffer.BlockCopy(buf, 0, c, 0, r);
                        lock (aq) { if (gen != audioGen) break; aq.Enqueue(c); aqBytes += r; }
                    }
                }
                catch (Exception) { }
            }) { IsBackground = true, Name = "Bink sound (ffmpeg)" };
            at.Start();
        }

        void StopAudio()
        {
            atQuit = true; audioGen++;
            lock (aq) Monitor.PulseAll(aq);
            Kill(ap); ap = null;
            if (at != null) { at.Join(500); at = null; }
            lock (aq) { aq.Clear(); aqBytes = 0; aqHead = 0; }
        }

        // ------------------------------------------------------------------------------------------------ IBinkSource
        public void Control(int what, int value)
        {
            if (what == ReflexBink.Play)
            {   // XenonBinkVideo::vf1: BinkGoto(1), sound on, the first frame due at once
                bool loop = value != 0;
                if (vp == null || vpHasFrames || vpLoop != loop) StartVideo(loop);
                curFrame = 0; t0Set = false; ended = false; haveFrame = false;
                StartAudio();
            }
            else if (what == ReflexBink.GotoStart)
            {   // the loop: the process goes on (-stream_loop), the next pass starts one video length after the last
                t0 += frames * frameMs; curFrame = 0; ended = false;
                if (inf.audioRate > 0) StartAudio();
            }
        }

        public int Update(double ms, out int frame, out int done)
        {
            done = 0; frame = curFrame; lastMs = ms;
            if (vp == null) return -1;
            if (!t0Set) { t0 = ms; t0Set = true; }
            if (ended) return 0;
            int due = (int)Math.Min(frames, Math.Floor((ms - t0) / frameMs) + 1);
            if (due <= curFrame) return 0;
            bool got = false;
            while (curFrame < due)
            {
                if (!ReadFrame())
                {
                    if (!vpHasFrames && !haveFrame && !vsyncFallback && lastError.IndexOf("fps_mode", StringComparison.OrdinalIgnoreCase) >= 0)
                    {   // an ffmpeg older than 5.1
                        vsyncFallback = true; StartVideo(vpLoop); return 0;
                    }
                    if (!haveFrame) { lastError = "ffmpeg decoded no frame of " + Path.GetFileName(file) + (lastError.Length > 0 ? ": " + lastError : ""); return -1; }
                    curFrame = frames; ended = true; frame = curFrame; done = 1;   // the stream ended early: the last picture stays
                    return got ? 1 : 0;
                }
                curFrame++; got = true; haveFrame = true;
            }
            frame = curFrame; done = frame >= frames ? 1 : 0;
            return 1;
        }

        public void Planes(byte[] y, byte[] cr, byte[] cb, byte[] a)
        {
            if (y != null) Buffer.BlockCopy(raw, 0, y, 0, Math.Min(ylen, y.Length));
            if (cb != null) Buffer.BlockCopy(raw, ylen, cb, 0, Math.Min(clen, cb.Length));            // ffmpeg: Y, U (Cb), V (Cr), A
            if (cr != null) Buffer.BlockCopy(raw, ylen + clen, cr, 0, Math.Min(clen, cr.Length));
            if (alpha && a != null) Buffer.BlockCopy(raw, ylen + 2 * clen, a, 0, Math.Min(ylen, a.Length));
        }

        public int Audio(short[] output, int max)
        {
            if (inf.audioRate == 0) return 0;
            // the sound follows the video clock (about half a second ahead): the whole track is not handed over at once
            double sec = Math.Max(0, (lastMs - t0) / 1000.0) + 0.6;
            long allowed = (long)(sec * inf.audioRate) * inf.audioChannels - audioTaken / 2;
            int n = (int)Math.Min(Math.Min(max, allowed), int.MaxValue);
            if (n <= 0) return 0;
            int got = 0;
            lock (aq)
            {
                while (got < n && aq.Count > 0)
                {
                    var c = aq.Peek();
                    int avail = (c.Length - aqHead) / 2, take = Math.Min(avail, n - got);
                    for (int i = 0; i < take; i++) output[got + i] = (short)(c[aqHead + 2 * i] | (c[aqHead + 2 * i + 1] << 8));
                    got += take; aqHead += 2 * take; aqBytes -= 2 * take;
                    if (aqHead >= c.Length - 1) { aq.Dequeue(); aqHead = 0; }
                }
                audioTaken += 2L * got;
                Monitor.PulseAll(aq);
            }
            return got;
        }

        public void Dispose() { StopAudio(); StopVideo(); }
    }
}
