// TEST ONLY: the event load from the shell (OpenShell.StartEvent, the RACE browser's START EVENT / the hub's RIDE NOW) in the real flow: a real
// event of the install is started, what is on screen is taken every second while it loads (no placeholder world may ever show), and the
// timings of the load are logged. Page "loadarena" (REFLEX_LOADARENA_EVENT=<event key>, default National/NAT_Concord; AI off).
using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseLoadArena
    {
        const string Tag = "[ui-showcase] [loadarena] ";
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("loadarena", () => Journey(false)); UiShowcase.Register("loadarena-proown", () => Journey(true)); UiShowcase.Register("loadarena-career", Career); }

        static string State(float t0)
        {
            var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            string sim = rt && rt.SimDriver?.Open is RxCore.OpenReflexSimulation s ? s.CurrentPhase.ToString() : "-";
            return $"t={Time.realtimeSinceStartup - t0:F2} scene={SceneManager.GetActiveScene().name} runtime={(bool)rt} running={(rt && rt.Running)} sim={sim} loading={(bool)OpenLoading.Current} line='{OpenLoading.Current?.Line}' fps={1f / Mathf.Max(1e-4f, Time.unscaledDeltaTime):F0}";
        }

        static long Resident() { try { using var p = System.Diagnostics.Process.GetCurrentProcess(); p.Refresh(); return p.WorkingSet64 / (1024 * 1024); } catch (Exception) { return -1; } }

        // proOwn (page "loadarena-proown"): the profile left on a pro rider, the event's setup on the player's own vehicle (the arena's ride once
        // failed there: "Rider and vehicle ownership do not match", then the event was taken for a test ride: no intro, START to the menu)
        static IEnumerator Journey(bool proOwn)
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            if (proOwn)
            {
                var pro = OpenGarageData.ProRides().FirstOrDefault(p => p.vehicleType == 3) ?? OpenGarageData.ProRides().FirstOrDefault();
                UiShowcaseNav.Check(pro != null && OpenGarageData.SelectPro(pro), "loadarena", "no pro rider selected");
                Debug.Log(Tag + "the profile on the pro rider " + pro?.name + " (rider " + pro?.rider + ", vehicle " + pro?.vehicle + ")");
            }
            var shell = OpenShell.Instance;
            string key = Environment.GetEnvironmentVariable("REFLEX_LOADARENA_EVENT");
            if (string.IsNullOrEmpty(key)) key = "National/NAT_Concord";
            var ev = OpenContent.Find(key);
            UiShowcaseNav.Check(ev != null, "loadarena", "no event " + key);
            if (ev == null) yield break;
            // the 3D menu world first (the arena)
            for (float t = 0; t < 60f && OpenMenuWorld.Instance && OpenMenuWorld.Instance.Status == OpenMenuWorld.State.Loading; t += Time.unscaledDeltaTime) yield return null;
            Debug.Log(Tag + "menu world " + (OpenMenuWorld.Instance ? OpenMenuWorld.Instance.Status.ToString() : "none") + $", resident {Resident()} MB");
            // a few seconds in the menus, as a player has: the arena's session is made meanwhile (OpenArenaWarm; REFLEX_LOADARENA_WARM=0: not)
            float w0 = Time.realtimeSinceStartup;
            long beforeWarm = Resident(); float sampled = 0;
            while (Time.realtimeSinceStartup - w0 < (OpenArenaWarm.Enabled ? 20f : 6f) && !(OpenArenaWarm.Current && OpenArenaWarm.Current.Ready))
            {
                if (!OpenArenaWarm.Current && !RxCore.RxNativeLoader.ArenaLoaded && Time.realtimeSinceStartup - sampled > 0.2f) { beforeWarm = Resident(); sampled = Time.realtimeSinceStartup; }
                yield return null;
            }
            Debug.Log(Tag + "arena warm: " + (!OpenArenaWarm.Enabled ? "off" : OpenArenaWarm.Current ? (OpenArenaWarm.Current.Ready ? "ready" : "not ready") : "none") + $" after {Time.realtimeSinceStartup - w0:F1} s in the menus; resident {beforeWarm} MB before the second image and its session, {Resident()} MB now");
            yield return new WaitForSecondsRealtime(2f);
            Debug.Log(Tag + $"resident in the menus before START EVENT: {Resident()} MB (arena warm {(OpenArenaWarm.Current && OpenArenaWarm.Current.Ready ? "ready" : "none")})");
            yield return UiShowcase.Shot("loadarena-0-hub");
            var setup = new OpenSetup { eventKey = ev.key, ai = 0, vehicleChoiceExplicit = proOwn };
            if (proOwn)
            {   // the player's own rider on the event's vehicle type (Vehicle Select's YOUR RIDER, id -1: listed when the profile owns a vehicle of the type)
                int type = setup.Type(ev.ev); var riders = OpenSetup.Riders(ev.ev, type);
                Debug.Log(Tag + "type " + type + " riders: " + string.Join(", ", riders.Take(4).Select(r => r.id + " " + r.name)));
                if (!riders.Any(r => r.id == -1))
                    foreach (int t in new[] { 3, 1, 2, 4, 5 }) { var rs = OpenSetup.Riders(ev.ev, t); if (rs.Any(r => r.id == -1)) { type = t; break; } }
                setup.vehicleType = type; setup.rider = -1;
                Debug.Log(Tag + "the event's garage: " + setup.Garage(ev.ev));
                UiShowcaseNav.Check(setup.Garage(ev.ev)?.riderOwner == ReflexWorldFiles.Garage.Player1, "loadarena", "the setup is not on the player's own rider");
            }
            float t0 = Time.realtimeSinceStartup;
            Debug.Log(Tag + "START " + ev.key);
            typeof(OpenShell).GetMethod("StartEvent", BindingFlags.Instance | BindingFlags.NonPublic).Invoke(shell, new object[] { ev, setup });
            // (the first screenshot after the launch: a capture is a long frame of its own, and would land in the launch's timing lines)
            int shot = 1; float nextShot = 1.2f, nextLog = 0;
            float runningAt = -1, readyAt = -1, startAt = -1, rideAt = -1, viewAt = -1; bool throttle = false; string eventScene = null;
            bool keyboard = Environment.GetEnvironmentVariable("REFLEX_LOADARENA_KEY") == "enter";
            while (Time.realtimeSinceStartup - t0 < 150f)
            {
                float t = Time.realtimeSinceStartup - t0;
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                var sh = OpenShell.Instance;
                var own = rt ? rt.loadArena : null;
                // the ridden arena (OpenLoadArena: the test drive in the core's second image) or the shown one (the shell's)
                string phase = own ? (own.PromptShown ? "ready" : "loading") + "/ridden-" + OpenLoadArena.Phase : sh ? sh.LoadArenaPhase ?? "-" : "(no shell)";
                if (t >= nextLog) { Debug.Log(Tag + State(t0) + " arena=" + phase + " progress=" + (OpenEventPrestart.Current ? OpenEventPrestart.Current.Progress.ToString("F2") : "-") + (own ? " line='" + own.Line + "'" : "") + (rt && rt.TryGetVehicle(out var v) ? $" speed={v.speed:F1}" : "")); nextLog = t + 0.25f; }
                if (own && rt.Running && rideAt < 0) { rideAt = t; Debug.Log(Tag + "RIDING THE ARENA at " + t.ToString("F2") + " s"); }
                if (own && rideAt >= 0 && viewAt < 0 && !OpenTestRideCurtain.Up) { viewAt = t; Debug.Log(Tag + "THE ARENA IN FULL VIEW at " + t.ToString("F2") + " s"); }
                // ride while it loads: the throttle (the pad's right trigger) and a little steering
                if (own && rt.Running && startAt < 0 && !throttle) { throttle = true; UiShowcaseNav.PadState(new UnityEngine.InputSystem.LowLevel.GamepadState { rightTrigger = 1f, leftStick = new Vector2(0.35f, 0f) }); }
                if (rt && rt.Running && !own && runningAt < 0 && startAt >= 0) { runningAt = t; eventScene = rt.EventIdentity; Debug.Log(Tag + "RUNNING " + eventScene + " at " + t.ToString("F2") + " s" + $" ({t - startAt:F2} s after START)"); UiShowcaseNav.Check(!ReflexGarage.TestDrive.Active, "loadarena", "the event runs as a test drive"); }
                if (own && rt.FailureMessage != null) { UiShowcaseNav.Check(false, "loadarena", "the arena's ride failed: " + rt.FailureMessage.Split('\n')[0]); yield return UiShowcase.Shot("loadarena-ride-failed"); break; }
                if (startAt < 0 && !own && rt && rt.Running) UiShowcaseNav.Check(false, "loadarena", "the event ran before PRESS START");
                if (phase == "failed" || OpenEventPrestart.Current && OpenEventPrestart.Current.Failure != null) { UiShowcaseNav.Check(false, "loadarena", "the event's load failed: " + OpenEventPrestart.Current?.Failure); yield return UiShowcase.Shot("loadarena-failed"); break; }
                if (phase.StartsWith("ready") && readyAt < 0)
                {
                    readyAt = t; Debug.Log(Tag + "READY at " + t.ToString("F2") + " s (" + phase + ")");
                    yield return UiShowcase.Shot("loadarena-ready");
                    yield return new WaitForSecondsRealtime(2f);
                    yield return UiShowcase.Shot("loadarena-ready-2s");
                    UiShowcaseNav.PadState(new UnityEngine.InputSystem.LowLevel.GamepadState());   // (the throttle off)
                    yield return UiShowcase.Frames(10);
                    startAt = Time.realtimeSinceStartup - t0;
                    Debug.Log(Tag + "START (" + (keyboard ? "Enter" : "pad Start") + ") at " + startAt.ToString("F2") + " s");
                    if (keyboard) yield return UiShowcaseNav.Key(UnityEngine.InputSystem.Key.Enter);
                    else yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.Start);
                    nextShot = 0; continue;
                }
                if (t >= nextShot)
                {
                    yield return UiShowcase.Shot($"loadarena-{shot:00}-t{t:F1}{(startAt >= 0 ? "-after-start" : rideAt >= 0 ? "-riding" : "")}");
                    shot++; nextShot = t < 4 || startAt >= 0 && t - startAt < 4 ? t + 0.5f : t + 1.5f;
                }
                if (runningAt >= 0 && t - runningAt > 6f) break;
                yield return null;
            }
            UiShowcaseNav.PadState(new UnityEngine.InputSystem.LowLevel.GamepadState());
            Debug.Log(Tag + "END " + State(t0));
            UiShowcaseNav.Check(runningAt >= 0 && eventScene != null && eventScene.StartsWith(ev.ev.spec.type + "/"), "loadarena", "the event never ran (" + eventScene + ")");
            UiShowcaseNav.Check(readyAt >= 0 && startAt >= 0, "loadarena", "the arena never offered PRESS START");
            Debug.Log(Tag + $"TIMES riding {rideAt:F2} s, in full view {viewAt:F2} s, ready {readyAt:F2} s, START {startAt:F2} s, running {runningAt:F2} s (START to running {(runningAt - startAt):F2} s), ride first frame {OpenLoadArena.RideSeconds:F2} s");
        }

        // ------------------------------------------------------------------------------------------------ a career moto: its load and the Gate Selection's view
        // Page "loadarena-career" (run.py --seed-save testdata/moto_career_flow/core-player-career-v2.bin): the career's Series 14 (a National
        // moto, SKIP PRACTICE AND CHOOSE GATE) launched as the career hub does, its rules accepted, the gate selection's view taken.
        static UnityEngine.UIElements.VisualElement Find(string name)
        {
            UnityEngine.UIElements.VisualElement best = null; int order = int.MinValue;
            foreach (var doc in UnityEngine.Object.FindObjectsByType<UnityEngine.UIElements.UIDocument>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
            {
                var v = doc.rootVisualElement?.Q<UnityEngine.UIElements.VisualElement>(name);
                if (v != null && v.resolvedStyle.display != UnityEngine.UIElements.DisplayStyle.None && v.enabledInHierarchy && doc.sortingOrder >= order) { best = v; order = (int)doc.sortingOrder; }
            }
            return best;
        }
        static bool Press(string name)
        {
            var v = Find(name);
            if (v?.userData is OuNavItem nav && nav.submit != null) { Debug.Log(Tag + "submit " + name); nav.submit(); return true; }
            return false;
        }

        static IEnumerator Career()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            if (!RxCore.RxNativeLoader.IsLoaded) RxCore.RxNativeLoader.Load(RxCore.RxNativeLoader.ResolvePath(RxCore.RxNativeLoader.PluginDirectory(Application.dataPath, Application.isEditor)));
            var presentation = new OwnCareerPresentationInstallation(OwnCareerPresentationInstallation.InstalledHuds, OpenShell.ReturnToOwnedCareer, OpenShell.ReturnToOwnedCareer);
            // the moto's vehicle (the moto career flow's own pick: the save's player vehicle 2, a type-3 Moto bike)
            Debug.Log(Tag + "unlock everything (the test save's unlocked clone): " + CoreGarageProfile.SetUnlockEverything(true));
            Debug.Log(Tag + "career vehicle 2 equipped: " + OpenGarageData.SelectVehicle(2));
            var task = RxCore.OpenReflexSimulation.ReadCareerCatalogAsync(ReflexInstall.Require(), CareerEventCatalog.Read(), CoreGarageProfile.CapturePersistence(), presentation.Capabilities, default);
            while (!task.IsCompleted) yield return null;
            if (task.IsFaulted) { UiShowcaseNav.Check(false, "loadarena-career", "catalog: " + task.Exception.GetBaseException().Message); yield break; }
            RxCore.CareerEventChoice pick = null;
            foreach (var c in task.Result) if (c.Entry.SeriesId == 14) pick = c;
            if (pick == null || !pick.CanSelect) foreach (var c in task.Result) if (c.Entry.IsMoto && c.CanSelect) { pick = c; break; }
            foreach (var c in task.Result) if (c.Entry.IsMoto) Debug.Log(Tag + $"moto series {c.Entry.SeriesId} {c.Entry.TrackAsset}: select {c.CanSelect} (locked {c.Locked}, eligible {c.Eligible}, supported {c.Supported}) {c.Reason}");
            UiShowcaseNav.Check(pick != null && pick.CanSelect, "loadarena-career", "no selectable moto series");
            if (pick == null || !pick.CanSelect) yield break;
            Debug.Log(Tag + "career series " + pick.Entry.SeriesId + " " + pick.Entry.TrackAsset + " " + pick.Entry.Activity + " / " + pick.Entry.Format);
            float t0 = Time.realtimeSinceStartup;
            OwnCareerLaunch.Launch(pick, presentation, motoSkipPractice: true);
            int shot = 1; float nextShot = 0.2f, nextLog = 0, gateAt = -1, lastPress = -10;
            while (Time.realtimeSinceStartup - t0 < 150f)
            {
                float t = Time.realtimeSinceStartup - t0;
                if (t >= nextLog) { Debug.Log(Tag + State(t0)); nextLog = t + 0.5f; }
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                var gv = rt ? typeof(OriginalWorldRuntime).GetField("gateView", BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(rt) as OwnGateView : null;
                if (gv != null && gv.Active && gv.Ready && gv.Focus >= 0 && gateAt < 0) { gateAt = t; Debug.Log(Tag + "GATE VIEW at " + t.ToString("F2") + " s, terrain built " + (rt.terrain && rt.terrain.Built)); }
                if (t - lastPress > 1.5f && gateAt < 0 && (Press("career-rules-accept") || Press("career-moto-gates"))) lastPress = t;
                if (t >= nextShot && gateAt < 0) { yield return UiShowcase.Shot($"loadarena-career-{shot:00}-t{t:F1}"); shot++; nextShot = t + 1.5f; }
                if (gateAt >= 0 && t - gateAt > 2.5f)
                {
                    yield return UiShowcase.Shot("loadarena-career-gate");
                    UiShowcaseNav.Check(rt.terrain && rt.terrain.Built, "loadarena-career", "the gate selection shows no terrain");
                    break;
                }
                yield return null;
            }
            UiShowcaseNav.Check(gateAt >= 0, "loadarena-career", "the gate selection never showed");
        }
    }
}
