// TEST ONLY: the front end's 3D menu world (OpenFrontEnd/OpenMenuWorld): the compound behind the hub, the push-bike walk-in, the garage's
// in-place PREVIEW while browsing, the walk of a picked vehicle, the RIDER / GEAR framing, a page over the dimmed world; the load and frame times.
// A scratch save (REFLEX_SAVE_DIR), nothing of the user's.
//   menu3d-survey   the compound seen from the hero spot (four sides, from above) and the terrain's flatness around it (log)
//   menu3d-hub      the hub: the walk-in at a few times (menu3d-walk-*), then the settled hub (menu3d-hub), the frame times
//   menu3d-garage   VEHICLE: a browsed vehicle in the hero spot (PREVIEW), a fast scroll through the list (frame times), RIDER, GEAR
//   menu3d-pick     VEHICLE: a vehicle bought and equipped: the walk with it (menu3d-pick-*)
//   menu3d-pages    OPTIONS and CREDITS over the dimmed world
using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseMenu3D
    {
        const string Tag = "[ui-showcase] [menu3d] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("menu3d-survey", Survey);
            UiShowcase.Register("menu3d-hub", Hub);
            UiShowcase.Register("menu3d-garage", Garage);
            UiShowcase.Register("menu3d-pick", Pick);
            UiShowcase.Register("menu3d-pages", Pages);
        }

        static IEnumerator Ready()
        {
            float t0 = Time.realtimeSinceStartup;
            while (OpenMenuWorld.Instance == null && Time.realtimeSinceStartup - t0 < 10f) yield return null;
            var w = OpenMenuWorld.Instance;
            if (w == null) { Debug.Log(Tag + "no menu world (REFLEX_MENU3D=0, or the 3D menu is off)"); yield break; }
            while (w.Status == OpenMenuWorld.State.Loading && Time.realtimeSinceStartup - t0 < 120f) yield return null;
            Debug.Log(Tag + $"world {w.Status} {w.Error}: read {w.ReadSeconds:F2} s, build {w.BuildSeconds:F2} s, ready {w.ReadySeconds:F2} s after its start");
        }

        static void Profile()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            UiShowcaseGarage.Credits(250000);
            ReflexProfile.Open();
        }

        static IEnumerator WaitHero(float seconds = 30f)
        {
            var w = OpenMenuWorld.Instance;
            float t0 = Time.realtimeSinceStartup;
            while (w != null && (w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t0 < seconds) yield return null;
        }

        /// The frame times of the next frames, measured here (with or without the world): the wall clock (the window's presentation, which the
        /// system paces) and the main thread's work (from the first Update to the end of the frame's rendering on the main thread).
        static IEnumerator Sample(string what, int frames)
        {
            var t = new float[frames];
            FrameWork.Reset();
            for (int i = 0; i < frames; i++) { yield return null; t[i] = Time.unscaledDeltaTime * 1000f; }
            var o = t.OrderBy(x => x).ToArray();
            var wk = FrameWork.Take(); var gpu = FrameWork.TakeGpu(); var gc = FrameWork.TakeAlloc();
            Debug.Log(Tag + $"frame time {what}: wall mean {o.Average():F2} ms, p95 {o[(int)(o.Length * 0.95f)]:F2} ms, max {o[o.Length - 1]:F2} ms; " +
                      $"main-thread work mean {wk.mean:F2} ms, p95 {wk.p95:F2} ms, max {wk.max:F2} ms; " +
                      (gpu.n > 0 ? $"GPU mean {gpu.mean:F2} ms, p95 {gpu.p95:F2} ms (FrameTimingManager, {gpu.n} frames); " : "GPU: no frame timings; ") +
                      $"GC alloc mean {gc.meanBytes:F0} B/frame (max {gc.maxBytes} B, {gc.framesWithAlloc} frames allocating); " +
                      $"over {frames} frames at {Screen.width}x{Screen.height} (vSync {QualitySettings.vSyncCount}, target {Application.targetFrameRate}, display {Screen.currentResolution.refreshRateRatio.value:F0} Hz)");
        }

        /// The main thread's work per frame: from the earliest Update to the end of the frame (WaitForEndOfFrame, after the cameras rendered).
        [DefaultExecutionOrder(-32000)]
        sealed class FrameWork : MonoBehaviour
        {
            static FrameWork s_i; static readonly System.Collections.Generic.List<float> s_ms = new System.Collections.Generic.List<float>(), s_gpu = new System.Collections.Generic.List<float>();
            static readonly FrameTiming[] s_timing = new FrameTiming[1];
            static Unity.Profiling.ProfilerRecorder s_gc;
            static readonly System.Collections.Generic.List<long> s_alloc = new System.Collections.Generic.List<long>();
            public static (double meanBytes, long maxBytes, int framesWithAlloc) TakeAlloc()
            {
                if (s_alloc.Count == 0) return (0, 0, 0);
                return (s_alloc.Average(), s_alloc.Max(), s_alloc.Count(x => x > 0));
            }
            public static (float mean, float p95, int n) TakeGpu()
            {
                if (s_gpu.Count == 0) return (0, 0, 0);
                var o = s_gpu.OrderBy(x => x).ToArray();
                return (o.Average(), o[Mathf.Min(o.Length - 1, (int)(o.Length * 0.95f))], o.Length);
            }
            float start;
            public static void Reset()
            {
                if (!s_i) { var g = new GameObject("frame work probe (TEST)"); UnityEngine.Object.DontDestroyOnLoad(g); s_i = g.AddComponent<FrameWork>(); }
                if (!s_gc.Valid) s_gc = Unity.Profiling.ProfilerRecorder.StartNew(Unity.Profiling.ProfilerCategory.Memory, "GC Allocated In Frame");
                s_ms.Clear(); s_gpu.Clear(); s_alloc.Clear();
            }
            public static (float mean, float p95, float max) Take()
            {
                if (s_ms.Count == 0) return (0, 0, 0);
                var o = s_ms.OrderBy(x => x).ToArray();
                return (o.Average(), o[Mathf.Min(o.Length - 1, (int)(o.Length * 0.95f))], o[o.Length - 1]);
            }
            void Update()
            {
                start = Time.realtimeSinceStartup;
                FrameTimingManager.CaptureFrameTimings();
                if (FrameTimingManager.GetLatestTimings(1, s_timing) > 0 && s_timing[0].gpuFrameTime > 0) s_gpu.Add((float)s_timing[0].gpuFrameTime);
                if (s_gc.Valid) s_alloc.Add(s_gc.LastValue);
            }
            IEnumerator Start() { var eof = new WaitForEndOfFrame(); while (true) { yield return eof; if (start > 0) s_ms.Add((Time.realtimeSinceStartup - start) * 1000f); } }
        }

        static void Stats(string what)
        {
            var w = OpenMenuWorld.Instance; if (w == null) return;
            var (mean, p95, max, n) = w.FrameStats();
            Debug.Log(Tag + $"frame time {what}: mean {mean:F2} ms ({1000f / Mathf.Max(0.01f, mean):F0} fps), p95 {p95:F2} ms, max {max:F2} ms over {n} frames at {Screen.width}x{Screen.height}");
        }

        // ------------------------------------------------------------------------------------------------ survey

        static IEnumerator Survey()
        {
            yield return Ready();
            var w = OpenMenuWorld.Instance; if (w == null || w.Status != OpenMenuWorld.State.Ready) yield break;
            // the terrain around the centre: the flattest 24 x 30 ft windows (the walk needs 20 ft behind the spot)
            var best = new System.Collections.Generic.List<(float range, float x, float z, float y)>();
            for (float x = -120; x <= 120; x += 8)
                for (float z = -200; z <= 200; z += 8)
                {
                    float lo = float.MaxValue, hi = float.MinValue;
                    for (float dx = -12; dx <= 12; dx += 3) for (float dz = -24; dz <= 8; dz += 3) { float y = w.GroundAt(x + dx, z + dz); lo = Mathf.Min(lo, y); hi = Mathf.Max(hi, y); }
                    best.Add((hi - lo, x, z, w.GroundAt(x, z)));
                }
            foreach (var b in best.OrderBy(b => b.range).Take(12)) Debug.Log(Tag + $"flat window at ({b.x}, {b.z}): y {b.y:F2}, range {b.range:F2} ft");
            Debug.Log(Tag + $"hero spot {OpenMenuWorld.HeroSpot}: y {w.GroundAt(OpenMenuWorld.HeroSpot.x, OpenMenuWorld.HeroSpot.z):F2}");
            UiShowcase.ShellVisible(false);
            OpenMenuWorld.ManualCamera = true;
            var cam = w.Camera; var hero = OpenMenuWorld.HeroSpot; hero.y = w.GroundAt(hero.x, hero.z);
            cam.transform.SetPositionAndRotation(new Vector3(hero.x, hero.y + 420, hero.z - 1), Quaternion.LookRotation(Vector3.down, Vector3.forward));
            yield return UiShowcase.Frames(10); yield return UiShowcase.Shot("menu3d-survey-top");
            cam.transform.SetPositionAndRotation(new Vector3(0, 900, -1), Quaternion.LookRotation(Vector3.down, Vector3.forward));
            float fov = cam.fieldOfView; cam.fieldOfView = 60;
            yield return UiShowcase.Frames(10); yield return UiShowcase.Shot("menu3d-survey-top-wide");
            cam.fieldOfView = fov;
            foreach (int a in new[] { 0, 90, 180, 270 })
            {
                var dir = Quaternion.Euler(0, a, 0) * Vector3.forward;
                cam.transform.SetPositionAndRotation(hero + Vector3.up * 6f - dir * 2f, Quaternion.LookRotation(dir + Vector3.down * 0.05f, Vector3.up));
                cam.fieldOfView = 60;
                yield return UiShowcase.Frames(10); yield return UiShowcase.Shot("menu3d-survey-look-" + a);
            }
            cam.fieldOfView = fov;
            OpenMenuWorld.ManualCamera = false;
            UiShowcase.ShellVisible(true);
        }

        // ------------------------------------------------------------------------------------------------ the hub

        static IEnumerator Hub()
        {
            Profile();
            var shell = OpenShell.Instance;
            OpenMenuWorld.Still = true;
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            shell.ShowMain();
            yield return Ready();
            var w = OpenMenuWorld.Instance;
            if (w == null || w.Status != OpenMenuWorld.State.Ready)
            {   // the pictures (REFLEX_MENU3D=0 or no level): the same frame-time measure for comparison
                for (int i = 0; i < 60; i++) yield return null;
                yield return UiShowcase.Shot("menu3d-hub-fallback");
                yield return Sample("hub without the menu world (picture)", 400);
                yield break;
            }
            yield return WaitHero();
            for (int i = 0; i < 6 && !w.Walking; i++) yield return null;
            if (!w.Walking) { w.StartWalk(); yield return null; }   // (the hub's first walk may have run before this page)
            Debug.Log(Tag + "hub: hero " + w.HeroKey + " walking " + w.Walking + (w.Walking ? "" : " (no walk: " + ReflexPushAnim.Error + ")"));
            if (w.Walking)
            {
                foreach (float t in new[] { 0.2f, 1.6f, 3.2f, 4.8f, 6.1f, 6.8f })
                {
                    w.SetWalkTime(t); w.SnapCamera();
                    yield return UiShowcase.Frames(4);
                    if (!w.Walking) break;
                    w.SetWalkTime(t);
                    yield return UiShowcase.Shot("menu3d-walk-" + t.ToString("0.0"));
                }
                w.SetWalkTime(99f);
            }
            for (int i = 0; i < 30; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-hub");
            // the frame time of the settled hub (the idle and the drift running, as a player sees it)
            OpenMenuWorld.Still = false; Reflex.Port.FrontEnd.FeGarage.RiderPlays = true;
            // the still shot: the camera and the bike must not move at all once settled (the rider's idle plays)
            {
                var cam = w.Camera.transform; Vector3 c0 = cam.position; Quaternion r0 = cam.rotation;
                for (int i = 0; i < 240; i++) yield return null;   // (settled)
                c0 = cam.position; r0 = cam.rotation;
                var riderRoot = w.Hero.RiderModel?.root ? w.Hero.RiderModel.root.transform : null;
                var bikeT = w.Hero.root.GetComponentsInChildren<MeshRenderer>().Select(m => m.transform).Where(t => riderRoot == null || !t.IsChildOf(riderRoot)).ToArray();
                var b0 = bikeT.Select(t => t.position).ToArray();
                float camMove = 0, camTurn = 0, bikeMove = 0;
                for (int i = 0; i < 120; i++)
                {
                    yield return null;
                    camMove = Mathf.Max(camMove, (cam.position - c0).magnitude); camTurn = Mathf.Max(camTurn, Quaternion.Angle(cam.rotation, r0));
                    for (int k = 0; k < bikeT.Length; k++) if (bikeT[k]) bikeMove = Mathf.Max(bikeMove, (bikeT[k].position - b0[k]).magnitude);
                }
                Debug.Log(Tag + $"still check over 120 frames: camera moved {camMove:F6} ft / {camTurn:F5} deg, the bike's {bikeT.Length} parts moved {bikeMove:F6} ft" + (camMove < 1e-3f && bikeMove < 1e-4f ? " (still: under 1/8 of a pixel)" : " FAIL not still"));
            }
            yield return Sample("hub (settled, idle playing)", 400);
            // the walk again, at speed, its frame times
            w.StartWalk();
            yield return Sample("hub during the walk", 200);
            for (float t0 = Time.realtimeSinceStartup; w.Walking && Time.realtimeSinceStartup - t0 < 12f;) yield return null;
            yield return UiShowcase.Shot("menu3d-hub-live");
            OpenMenuWorld.Still = true; Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
        }

        // ------------------------------------------------------------------------------------------------ the garage

        static VisualElement El(string name) => OpenShell.Instance.Layer.Q<VisualElement>(name);

        static IEnumerator Garage()
        {
            Profile();
            yield return Ready();
            var w = OpenMenuWorld.Instance;
            OpenMenuWorld.Still = true; Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            Type stage = typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
            stage.GetField("NoAutoTurn").SetValue(null, true);
            stage.GetField("Verbose").SetValue(null, true);
            var shell = OpenShell.Instance;
            shell.ShowGarage(OpenShell.GarageVehicle);
            for (int i = 0; i < 90; i++) yield return null;
            if (w != null) w.SetWalkTime(99f);
            yield return UiShowcase.Shot("menu3d-garage-vehicle-equipped");
            var cards = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-model-")).ToList();
            Debug.Log(Tag + "vehicle cards not owned: " + cards.Count);
            if (cards.Count > 0)
            {
                shell.Nav.Focus(cards[0]);
                for (float t0 = Time.realtimeSinceStartup; w != null && (w.PreviewKey == null || w.PreviewBusy) && Time.realtimeSinceStartup - t0 < 10f;) yield return null;
                for (int i = 0; i < 45; i++) yield return null;
                Debug.Log(Tag + $"preview: {w?.PreviewKey} built in {w?.LastPreviewBuildMs:F0} ms (main thread); tag {(El("garage-preview-tag") != null ? "PREVIEW shown" : "FAIL no PREVIEW tag")}; rider drawn {w?.Hero?.RiderModel?.root.activeInHierarchy}");
                yield return UiShowcase.Shot("menu3d-garage-vehicle");
                // a fast scroll: a card every other frame; the frames must stay short (the models are built only where the focus stops)
                int n = Mathf.Min(cards.Count, 12);
                FrameWork.Reset();
                float s0 = Time.realtimeSinceStartup;
                for (int k = 1; k < n; k++) { shell.Nav.Focus(cards[k]); yield return null; yield return null; }
                var wk = FrameWork.Take();
                Debug.Log(Tag + $"garage scroll through {n - 1} cards (a card every other frame): {(Time.realtimeSinceStartup - s0) * 1000f / ((n - 1) * 2):F2} ms per frame (wall), main-thread work mean {wk.mean:F2} ms, max {wk.max:F2} ms");
                for (float t0 = Time.realtimeSinceStartup; w != null && w.PreviewBusy && Time.realtimeSinceStartup - t0 < 10f;) yield return null;
                for (int i = 0; i < 45; i++) yield return null;
                Debug.Log(Tag + $"preview after the scroll: {w?.PreviewKey} built in {w?.LastPreviewBuildMs:F0} ms");
                yield return UiShowcase.Shot("menu3d-garage-browse");
                yield return Sample("garage VEHICLE preview (settled)", 200);
                // back to the equipped card: the equipped vehicle and the rider return, the PREVIEW tag goes
                var eq = shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith("garage-vehicle-"));
                if (eq != null)
                {
                    shell.Nav.Focus(eq);
                    for (int i = 0; i < 60; i++) yield return null;
                    Debug.Log(Tag + $"back to the equipped card: preview {w?.PreviewKey ?? "none"}, tag {(El("garage-preview-tag") != null ? "FAIL still PREVIEW" : "gone")}, rider drawn {w?.Hero?.RiderModel?.root.activeInHierarchy}");
                    yield return UiShowcase.Shot("menu3d-garage-vehicle-back");
                }
            }
            shell.ShowGarage(OpenShell.GarageRider);
            for (int i = 0; i < 90; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-garage-rider");
            shell.ShowGarage(OpenShell.GarageGear);
            for (int i = 0; i < 30; i++) yield return null;
            var row = shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith("garage-gear-"));
            if (row != null) shell.Nav.Focus(row);
            for (int i = 0; i < 90; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-garage-gear");
            shell.ShowGarage(OpenShell.GarageParts);
            for (int i = 0; i < 90; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-garage-parts");
            shell.ShowMain();
            for (int i = 0; i < 60; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-hub-after-garage");
        }

        static IEnumerator Pick()
        {
            Profile();
            yield return Ready();
            var w = OpenMenuWorld.Instance; if (w == null) yield break;
            OpenMenuWorld.Still = true; Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage").GetField("NoAutoTurn").SetValue(null, true);
            var shell = OpenShell.Instance;
            shell.ShowGarage(OpenShell.GarageVehicle);
            for (int i = 0; i < 60; i++) yield return null;
            w.SetWalkTime(99f);
            var card = shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith("garage-model-"));
            if (card == null) { Debug.Log(Tag + "no vehicle to buy"); yield break; }
            shell.Nav.Focus(card);
            for (float t0 = Time.realtimeSinceStartup; (w.PreviewKey == null || w.PreviewBusy) && Time.realtimeSinceStartup - t0 < 10f;) yield return null;
            for (int i = 0; i < 20; i++) yield return null;
            (card.userData as OuNavItem)?.submit?.Invoke();
            for (int i = 0; i < 10; i++) yield return null;
            var buy = El("garage-buy");
            if (buy == null) { Debug.LogError(Tag + "FAIL no purchase dialog"); yield break; }
            (buy.userData as OuNavItem)?.submit?.Invoke();
            float t1 = Time.realtimeSinceStartup;
            while (!w.Walking && Time.realtimeSinceStartup - t1 < 20f) yield return null;
            Debug.Log(Tag + $"pick: walking {w.Walking} after {Time.realtimeSinceStartup - t1:F2} s, hero {w.HeroKey} (built in {w.LastHeroBuildMs:F0} ms)");
            if (w.Walking)
                foreach (float t in new[] { 0.5f, 2.5f, 4.5f, 6.0f })
                {
                    w.SetWalkTime(t); w.SnapCamera();
                    yield return UiShowcase.Frames(4);
                    if (!w.Walking) break;
                    w.SetWalkTime(t);
                    yield return UiShowcase.Shot("menu3d-pick-" + t.ToString("0.0"));
                }
            w.SetWalkTime(99f);
            for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-pick-end");
            shell.ShowMain();
            for (int i = 0; i < 60; i++) yield return null;
        }

        static IEnumerator Pages()
        {
            yield return Ready();
            var shell = OpenShell.Instance;
            OpenMenuWorld.Still = true;
            shell.ShowMain();
            for (int i = 0; i < 30; i++) yield return null;
            OpenMenuWorld.Instance?.SetWalkTime(99f);
            shell.ShowOptions();
            for (int i = 0; i < 60; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-options");
            Reflex.Port.Options.ReflexOptions.Close();
            for (int i = 0; i < 20; i++) yield return null;
            shell.ShowCredits();
            for (int i = 0; i < 60; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-credits");
            shell.ShowBrowser(false);
            for (int i = 0; i < 90; i++) yield return null;
            yield return UiShowcase.Shot("menu3d-race");
            Profile();
            shell.ShowCoreCareer();
            for (float t0 = Time.realtimeSinceStartup; Time.realtimeSinceStartup - t0 < 6f;) yield return null;
            yield return UiShowcase.Shot("menu3d-career");
            shell.ShowMain();
            for (int i = 0; i < 20; i++) yield return null;
        }
    }
}
