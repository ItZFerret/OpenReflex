// Diagnostics only (players; off unless REFLEX_REPLAY_PROBE=1): a scripted run of REPLAY / PHOTO MODE, as Open Unleashed's -mxaReplayAt /
// -mxaPhotoTake captures and MxaMenuCapture.ReplayKeys did it. It presses nothing in the game and writes no save.
//   REFLEX_REPLAY_EVENT=<event key> REFLEX_REPLAY_AI=<n>   launch that event from the front end once it has settled (as the bench does)
//   REFLEX_REPLAY_AT=<s>                                  open the viewer after s seconds of recorded race (default 20)
//   REFLEX_PHOTO_DIR=<dir>                                the photos go there, not to Pictures (ReflexReplay.Photo.cs)
//   REFLEX_REPLAY_SHOTS=<dir>                             screenshots of the viewer (with its UI) at each step
//   REFLEX_REPLAY_PROBE_QUIT=1                            quit when done
// It opens REPLAY, scrubs, steps every camera mode, follows another rider, sets and flies camera keys, opens PHOTO MODE, takes photos
// with the logo in three corners at three aspects / resolutions, closes, checks the live moment came back, and logs each step as
// "[ReplayProbe] ..." with the recording's size and cost ("[ReplayProbe] MEASURE ...").
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Linq;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Replay
{
    public sealed partial class ReflexReplay
    {
        static readonly bool probe = Environment.GetEnvironmentVariable("REFLEX_REPLAY_PROBE") == "1";
        bool probeStarted;
        int probeTerrainIdx = -1; float probeTerrainLive, probeDeformLive;

        static float EnvF(string name, float fallback) =>
            float.TryParse(Environment.GetEnvironmentVariable(name), NumberStyles.Float, CultureInfo.InvariantCulture, out float v) ? v : fallback;

        void ProbeTick()
        {
            if (!probe || probeStarted || Active) return;
            if (Buffer.Duration < EnvF("REFLEX_REPLAY_AT", 20f)) return;
            probeStarted = true;
            StartCoroutine(ProbeRun());
        }

        static void P(string s) => Debug.Log("[ReplayProbe] " + s);

        IEnumerator Wait(float seconds) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < seconds) yield return null; }

        /// The view with the viewer's UI over it, 1920 x 1080, drawn offscreen (works without a window: -batchmode): the game camera into
        /// one target, the UI document's panel into another (a copy of its PanelSettings with a target texture), blended on the CPU.
        IEnumerator Shot(string name)
        {
            string dir = Environment.GetEnvironmentVariable("REFLEX_REPLAY_SHOTS");
            if (string.IsNullOrEmpty(dir) || !Ui || !camera) yield break;
            Directory.CreateDirectory(dir);
            const int W = 1920, H = 1080;
            var doc = Ui.Document; var original = doc.panelSettings;
            RenderTexture uiRt = null; PanelSettings copy = null;
            try
            {
                uiRt = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32) { name = "replay probe ui" };
                copy = Instantiate(original); copy.targetTexture = uiRt; copy.clearColor = true; copy.colorClearValue = new Color(0, 0, 0, 0);
                doc.panelSettings = copy;
            }
            catch (Exception e) { P("shot " + name + ": " + e.Message); yield break; }
            yield return null; yield return null; yield return new WaitForEndOfFrame();
            try
            {
                var camRt = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32);
                var prev = camera.targetTexture; var rect = camera.rect;
                camera.rect = new Rect(0, 0, 1, 1); camera.targetTexture = camRt; camera.Render(); camera.targetTexture = prev; camera.rect = rect;
                Texture2D Read(RenderTexture r) { var a = RenderTexture.active; RenderTexture.active = r; var t = new Texture2D(W, H, TextureFormat.RGBA32, false); t.ReadPixels(new Rect(0, 0, W, H), 0, 0); t.Apply(false); RenderTexture.active = a; return t; }
                var bg = Read(camRt); var fg = Read(uiRt);
                if (OffscreenFlip < 0) OffscreenFlip = 0;   // (upright: the display gamma ramp no longer turns offscreen renders, ReflexGammaOutput)
                if (OffscreenFlip == 1) FlipRows(bg);
                var b = bg.GetPixels32(); var f = fg.GetPixels32();
                for (int i = 0; i < b.Length; i++) { float al = f[i].a / 255f; b[i] = new Color32((byte)(f[i].r * al + b[i].r * (1 - al)), (byte)(f[i].g * al + b[i].g * (1 - al)), (byte)(f[i].b * al + b[i].b * (1 - al)), 255); }
                bg.SetPixels32(b); bg.Apply(false);
                File.WriteAllBytes(Path.Combine(dir, name + ".png"), bg.EncodeToPNG());
                Destroy(bg); Destroy(fg); camRt.Release(); Destroy(camRt);
            }
            catch (Exception e) { P("shot " + name + ": " + e.Message); }
            finally { doc.panelSettings = original; if (copy) Destroy(copy); if (uiRt) { uiRt.Release(); Destroy(uiRt); } }
            yield return null;
        }

        IEnumerator ProbeRun()
        {
            long gc0 = GC.GetTotalMemory(false);
            P(FormattableString.Invariant($"MEASURE actors {actors.Count} riders {TargetCount} full {Buffer.FullTotal} packed {Buffer.NodeTotal} frameBytes {Buffer.FrameBytes} hz {1f / Buffer.Interval:0}") +
              FormattableString.Invariant($" frames {Buffer.Count} seconds {Buffer.Duration:0.00} capacityFrames {Buffer.Capacity} keptSeconds {Buffer.Seconds:0} ringMB {Buffer.FullBytes / 1048576f:0.0}") +
              FormattableString.Invariant($" MBperMinute {Buffer.FrameBytes / Buffer.Interval * 60f / 1048576f:0.0} recordMs {RecordMs:0.0000} particlesPerSet {ParticlesPerSet:0} particleRingMB {Particles.Bytes / 1048576f:0}") +
              FormattableString.Invariant($" particleMBperMinute {ParticlesPerSet * ReplayParticleRing.Words * 2f * 30f * 60f / 1048576f:0.0} managedMB {gc0 / 1048576f:0}"));
            float minutes = Mathf.Max(1e-3f, Buffer.Duration / 60f);
            P(FormattableString.Invariant($"MEASURE2 terrain texels {terrainTexels} words {Terrain.Written} MBperMinute {Terrain.Written * 2f / minutes / 1048576f:0.00} ringMB {Terrain.Bytes / 1048576f:0}") +
              FormattableString.Invariant($" sound commands {soundCommands} perSecond {soundCommands / (minutes * 60f):0} MBperMinute {soundCommands * AudioRing.CommandBytes / minutes / 1048576f:0.00} ringMB {Sound.Bytes / 1048576f:0}") +
              FormattableString.Invariant($" packFrames {(Buffer.Count > 0 ? Buffer.Last.packCount : 0)} mudWords {(Buffer.Count > 0 ? Buffer.Last.mud.Length : 0)}"));
            {   // the worker's managed float -> half against Unity's
                var rng = new System.Random(7); int diff = 0, total = 0; float hWorst = 0f;
                for (int i = 0; i < 200000; i++)
                {
                    float v = (float)((rng.NextDouble() * 2 - 1) * Math.Pow(10, rng.Next(-8, 5)));
                    ushort ha = ReplayParticleRing.HalfOf(v), hb = Mathf.FloatToHalf(v); total++;
                    if (ha != hb) { diff++; float fa = Mathf.HalfToFloat(ha), fb = Mathf.HalfToFloat(hb); hWorst = Mathf.Max(hWorst, Mathf.Abs(fa - fb) / Mathf.Max(1e-6f, Mathf.Abs(fb))); }
                }
                P(FormattableString.Invariant($"half: managed vs Unity {total - diff} of {total} identical, worst relative difference {hWorst:0.0e0}"));
            }
            var rt = Runtime;
            var cam = rt.chaseCamera;
            Vector3 camBefore = cam ? cam.transform.position : Vector3.zero;
            Vector3 chassisBefore = rt.chassis ? rt.chassis.position : Vector3.zero;
            bool ok = Open(View.Replay, Origin.Live);
            P($"open replay {ok} active {Active} frames {Buffer.Count} duration {Buffer.Duration:0.00}");
            if (!ok) yield break;
            yield return Wait(1.5f);
            P(FormattableString.Invariant($"playing {Playing} t {PlayTime - Buffer.StartTime:0.00} camera {CameraLabel}"));
            yield return Shot("rp_01_open");
            // scrub: seek back, step frames, shuttle
            Seek(Buffer.EndTime - 8f); yield return Wait(0.3f);
            float t1 = PlayTime; Step(5); float t2 = PlayTime; Shuttle(-1.5f); float t3 = PlayTime;
            P(FormattableString.Invariant($"scrub seek {t1 - Buffer.StartTime:0.00} step+5 {t2 - t1:0.000} s shuttle {t3 - t2:0.00} s"));
            int probeActor = actors.Count > 1 && !actors[1].props ? 1 : 0;
            Vector3 a = actors[probeActor].full[0].position;
            Seek(Buffer.StartTime + 1f); yield return Wait(0.3f);
            Vector3 b = actors[probeActor].full[0].position;
            TargetAt(PlayTime, probeActor, out Vector3 want, out _);
            P(FormattableString.Invariant($"pose moves with the timeline: {actors[probeActor].name} {a} -> {b} ({(a - b).magnitude:0.00} ft), recorded {want} (off {(b - want).magnitude:0.000} ft)"));
            // follow another rider and step through every camera
            if (TargetCount > 1) SetTarget(1);
            Seek(Buffer.EndTime - 10f);
            if (!Playing) TogglePlay();
            foreach (Cam c in Enum.GetValues(typeof(Cam)))
            {
                SetCamera(c); yield return Wait(0.6f);
                P(FormattableString.Invariant($"camera {c}: {CameraLabel} at {camera.transform.position} fov {camera.fieldOfView:0.0} target {TargetName(Target)} info '{TargetInfo()}'"));
                yield return Shot("rp_cam_" + c.ToString().ToLowerInvariant());
            }
            // keyframes: three keys around the rider, the camera on each key at its moment
            Pause(); SetCamera(Cam.Free);
            float k0 = Buffer.EndTime - 9f, k1 = Buffer.EndTime - 6f, k2 = Buffer.EndTime - 3f;
            foreach (var (at, off) in new[] { (k0, new Vector3(14, 4, -10)), (k1, new Vector3(4, 22, 18)), (k2, new Vector3(-18, 6, 4)) })
            {
                Seek(at); yield return Wait(0.1f);
                TargetAt(at, Target, out Vector3 tp, out _);
                FreeCameraAt(tp + Vector3.up * 2.6f + off, tp + Vector3.up * 2.6f); yield return Wait(0.15f);
                AddKey();
            }
            SetCamera(Cam.Keyframes);
            float worst = 0f;
            for (int i = 0; i < keys.Count; i++) { Seek(keys[i].t); yield return Wait(0.15f); worst = Mathf.Max(worst, (camera.transform.position - keys[i].pos).magnitude); }
            P(FormattableString.Invariant($"keyframes {keys.Count} keys, {CameraLabel}, camera on its key at the key's moment (off by {worst:0.000} ft)"));
            ClearKeys();
            // SOUND: the recorded commands played again at 1x; silent at 4x, in reverse, held and in PHOTO MODE
            {
                var audio = rt.gameAudio;
                Seek(Buffer.EndTime - 12f); Speed = 1f; if (!Playing) TogglePlay();
                yield return Wait(2f);
                P($"sound 1x: audible {Audible} listener paused {AudioListener.pause} isolated {(audio ? audio.ReplayIsolated : false)} replay events {(audio ? audio.ReplayInstancesPlaying : -1)} audible voices {(audio ? audio.ReplayVoicesAudible : -1)} commands replayed {SoundReplayed} packs {replayPacks.Count}");
                yield return Shot("rp_sound_1x");
                ChangeSpeed(1); ChangeSpeed(1); yield return Wait(0.5f);
                P($"sound {SpeedLabel}: audible {Audible} replay events {(audio ? audio.ReplayInstancesPlaying : -1)}");
                Speed = -1f; yield return Wait(0.5f);
                P($"sound reverse: audible {Audible} replay events {(audio ? audio.ReplayInstancesPlaying : -1)}");
                Speed = 1f; Pause(); yield return Wait(0.3f);
                P($"sound held: audible {Audible} listener paused {AudioListener.pause} isolated {(audio ? audio.ReplayIsolated : false)}");
            }
            // TERRAIN: the frames' recorded deltas on the mirrors as the timeline moves; the live terrain back at Close
            {
                int k = -1, idx = -1; float dh = 0f, dd = 0f;
                for (int i = Buffer.Count / 2; i < Buffer.Count - 1 && k < 0; i++)
                {
                    var f = Buffer[i];
                    if (f.terrainCount < 10 || !Terrain.Valid(f.terrainStart, f.terrainCount)) continue;
                    // the first patch's first texel, and every change of it in this frame
                    idx = Terrain[f.terrainStart + 6] | Terrain[f.terrainStart + 7] << 16; k = i;
                    long at = f.terrainStart, end = f.terrainStart + f.terrainCount;
                    while (at + 6 <= end)
                    {
                        int count = Terrain[at + 4] | Terrain[at + 5] << 16; at += 6;
                        for (int j = 0; j < count; j++, at += 4) if ((Terrain[at] | Terrain[at + 1] << 16) == idx) { dh += F(Terrain[at + 2]); dd += F(Terrain[at + 3]); }
                    }
                }
                if (k < 0) P("terrain: no frame with deltas");
                else
                {
                    TerrainSample(idx, out float hLive, out float dLive);
                    Seek(Buffer[k - 1].time); yield return Wait(0.2f);
                    TerrainSample(idx, out float h0, out float d0);
                    Seek(Buffer[k].time); yield return Wait(0.2f);
                    TerrainSample(idx, out float h1, out float d1);
                    Seek(Buffer.StartTime); yield return Wait(0.3f);
                    Seek(Buffer.EndTime); yield return Wait(0.3f);
                    TerrainSample(idx, out float hEnd, out float dEnd);
                    P(FormattableString.Invariant($"terrain: frame {k} of {Buffer.Count} texel {idx} recorded delta height {dh:0.000000} deform {dd:0.000000}; shown frame {k - 1} -> {k}: height {h0:0.000000} -> {h1:0.000000} (change {h1 - h0:0.000000}) deform {d0:0.0000} -> {d1:0.0000} (change {d1 - d0:0.000000}); live {hLive:0.000000} / {dLive:0.0000}, after start and back to the end {hEnd:0.000000} / {dEnd:0.0000}; floor frame {terrainFloor}"));
                    probeTerrainIdx = idx; probeTerrainLive = hLive; probeDeformLive = dLive;
                }
            }
            // PHOTO MODE: three shots with the logo
            SetCamera(Cam.Chase); Seek(Buffer.EndTime - 4f); yield return Wait(0.3f);
            PhotoFromReplay(); yield return Wait(0.5f);
            yield return Shot("rp_photo_ui");
            string dir = PhotoFolder;
            var shots = new[] { (logo: 1, aspect: 0, res: 0), (logo: 3, aspect: 2, res: 1), (logo: 2, aspect: 6, res: 2) };
            foreach (var s in shots)
            {
                Photo.logo = s.logo; Photo.aspect = s.aspect; Photo.resolution = s.res;
                yield return Wait(0.3f);
                yield return Shot($"rp_photo_frame_{s.logo}");
                TakePhoto();
                float t0 = Time.realtimeSinceStartup;
                while ((Capturing || LastPhotoPath == null) && Time.realtimeSinceStartup - t0 < 30f) yield return null;
                var path = LastPhotoPath;
                bool exists = path != null && File.Exists(path);
                P($"photo logo {LogoNames[s.logo]} aspect {AspectNames[s.aspect]} res {ResolutionNames[s.res]} -> {path} exists {exists} size {(exists ? new FileInfo(path).Length : 0)} " +
                  $"{(LastPhoto ? LastPhoto.width + "x" + LastPhoto.height : "")} dir {dir}");
                LastPhotoPath = null;
                yield return Wait(0.5f);
            }
            Photo.logo = 0; Photo.aspect = 0; Photo.resolution = 0;
            // back: the live moment exactly as it was
            SetFace(View.Replay); yield return Wait(0.3f);
            Close(); yield return Wait(0.3f);
            Vector3 camAfter = cam ? cam.transform.position : Vector3.zero, chassisAfter = rt.chassis ? rt.chassis.position : Vector3.zero;
            if (probeTerrainIdx >= 0) { TerrainSample(probeTerrainIdx, out float hc, out float dc); P(FormattableString.Invariant($"terrain after Close: height {hc:0.000000} deform {dc:0.0000} (live {probeTerrainLive:0.000000} / {probeDeformLive:0.0000}, exact {hc == probeTerrainLive && dc == probeDeformLive})")); }
            P(FormattableString.Invariant($"closed: active {Active} paused {rt.paused} viewer {rt.ViewerActive} chassis back {(chassisAfter - chassisBefore).magnitude:0.0000} ft camera {(camAfter - camBefore).magnitude:0.000} ft pose {PoseMs:0.000} ms"));
            // the pause page's entry: OPEN the page, press its REPLAY row, close the viewer: the page comes back on that row
            var pause = rt.racePause;
            if (pause)
            {
                pause.Open();
                float t0 = Time.realtimeSinceStartup;
                while (!pause.IsOpen && Time.realtimeSinceStartup - t0 < 4f) yield return null;
                yield return Wait(0.5f);
                var row = pause.Root?.Q<VisualElement>("pause-replay");
                P($"pause page open {pause.IsOpen} rows [{string.Join(", ", pause.RowNames)}] replay row {(row != null)}");
                if (row?.userData is Reflex.Port.OpenFrontEnd.OuNavItem item && item.submit != null)
                {
                    item.submit(); yield return Wait(1f);
                    P($"from the pause page: active {Active} from {OpenedFrom} face {Face} paused {rt.paused} pause page hidden {pause.Root.ClassListContains("ou-pause-hidden")}");
                    Close(); yield return Wait(0.5f);
                    P($"back on the pause page: open {pause.IsOpen} page {pause.Page} hidden {pause.Root.ClassListContains("ou-pause-hidden")} paused {rt.paused}");
                }
                pause.Close(); yield return Wait(0.3f);
                P($"pause closed: paused {rt.paused}");
            }
            P(FormattableString.Invariant($"MEASURE after managedMB {GC.GetTotalMemory(false) / 1048576f:0} recordMs {RecordMs:0.0000} poseMs {PoseMs:0.0000}"));
            P("DONE");
            if (Environment.GetEnvironmentVariable("REFLEX_REPLAY_PROBE_QUIT") == "1") { yield return Wait(1f); Application.Quit(); }
        }

        // ---------------------------------------------------------------------------------------- the launch

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void ProbeBoot()
        {
            if (!probe || string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_REPLAY_EVENT"))) return;
            Application.runInBackground = true;   // launched from a shell the window may never get focus
            var go = new GameObject("Open Reflex replay probe") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            go.AddComponent<ReplayProbeLauncher>();
        }
    }

    /// (diagnostics) launches REFLEX_REPLAY_EVENT from the front end once it has settled.
    sealed class ReplayProbeLauncher : MonoBehaviour
    {
        IEnumerator Start()
        {
            float t0 = Time.realtimeSinceStartup; int calm = 0;
            while (Time.realtimeSinceStartup - t0 < 60f && (Time.realtimeSinceStartup - t0 < 5f || calm < 90))
            {
                yield return null;
                calm = Time.unscaledDeltaTime < 0.1f ? calm + 1 : 0;
            }
            string key = Environment.GetEnvironmentVariable("REFLEX_REPLAY_EVENT");
            int ai = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_REPLAY_AI"), out int n) ? n : 11;
            Reflex.Port.Content.ReflexEvents.Event ev = null;
            try { ev = Reflex.Port.Content.ReflexEvents.Find(key) ?? Reflex.Port.Content.ReflexEvents.All.FirstOrDefault(e => e.key.StartsWith(key, StringComparison.OrdinalIgnoreCase)); }
            catch (Exception e) { Debug.Log("[ReplayProbe] events: " + e.Message); }
            if (ev == null) { Debug.Log("[ReplayProbe] FAIL no event " + key); yield break; }
            Debug.Log($"[ReplayProbe] LAUNCH {ev.key} ai {ai}");
            ReflexEventSelect.Launch(ev.key, ai, false);
            Destroy(gameObject);
        }
    }
}
