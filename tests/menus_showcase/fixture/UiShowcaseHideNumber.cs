// TEST ONLY: GARAGE > RIDER > NAME & NUMBER's HIDE NUMBER · BIKE / · GEAR (ReflexPlateText.HideBikeSetting / HideRiderSetting). Page (a scratch
// save, REFLEX_SAVE_DIR; muted):
//   hidenumber  the page with both SHOWN (hn-page-shown), the bike's front plate and the rider's chest / back with the number (hn-*-shown),
//               HIDE NUMBER · BIKE and · GEAR switched by their rows (save keys, the preview's plate key), the same shots without it
//               (hn-*-hidden), then a race (REFLEX_HIDENUMBER_EVENT, default Supercross/SX_DLC01_FreightYard): the local rider's plate text and
//               the online look's flags carry both, chase-camera shots at the gate and riding (hn-race-*).
// Log lines "[hidenumber-test] ..." (PASS / FAIL).
using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseHideNumber
    {
        const string Tag = "[hidenumber-test] ";
        static int fails;
        static void Check(bool ok, string what) { Debug.Log(Tag + (ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("hidenumber", Page); }

        sealed class Mute : MonoBehaviour { void LateUpdate() { if (AudioListener.volume != 0f) AudioListener.volume = 0f; } }   // test players are muted

        static Type StageType => typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
        static object Stage() => typeof(OpenShell).GetField("garageStage", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance);
        static void Angles(float yaw, float pitch) { var st = Stage(); st.GetType().GetMethod("SetAngles").Invoke(st, new object[] { yaw, pitch }); }
        static void Frame(string shot) { var st = Stage(); st.GetType().GetMethod("Frame").Invoke(st, new object[] { Enum.Parse(StageType.GetNestedType("Shot"), shot), true, true }); }
        static void Zoom(float amount) { var st = Stage(); st.GetType().GetMethod("Zoom").Invoke(st, new object[] { amount }); }
        static VisualElement Item(string name) => OpenShell.Instance.Nav.Items().FirstOrDefault(e => e.name == name);
        static IEnumerator Submit(string name, int frames = 90)
        {
            var e = Item(name);
            if (e == null) { Check(false, "no menu entry " + name); yield break; }
            OpenShell.Instance.Nav.Focus(e); yield return UiShowcase.Frames(20);
            (e.userData as OuNavItem)?.submit?.Invoke();
            yield return UiShowcase.Frames(frames);
        }

        static IEnumerator Shots(string state)
        {
            Frame("Front"); Angles(70, 6); yield return UiShowcase.Frames(60); yield return UiShowcase.Shot("hn-bike-front-" + state);
            Frame("Vehicle"); Angles(180, 8); Zoom(0.45f); yield return UiShowcase.Frames(60); yield return UiShowcase.Shot("hn-bike-side-" + state);
            Frame("Torso"); Angles(0, 4); yield return UiShowcase.Frames(60); yield return UiShowcase.Shot("hn-rider-chest-" + state);
            Frame("Rider"); Angles(180, 4); yield return UiShowcase.Frames(60); yield return UiShowcase.Shot("hn-rider-back-" + state);
        }

        static IEnumerator Page()
        {
            UiShowcaseNav.IsolateDevices();
            AudioListener.volume = 0f; new GameObject("hidenumber-mute").AddComponent<Mute>().hideFlags = HideFlags.DontSave;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            CoreGarageProfile.SetRiderText("Name", "FERRET"); CoreGarageProfile.SetRiderText("Number", "998");
            ReflexProfile.Open();
            StageType.GetField("NoAutoTurn").SetValue(null, true);
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            ReflexPlateText.HideBikeSetting = false; ReflexPlateText.HideRiderSetting = false;
            var shell = OpenShell.Instance;

            shell.ShowGarage(OpenShell.GarageIdentity); yield return UiShowcase.Frames(200);
            Check(Item("garage-hide-bike-number") != null && Item("garage-hide-gear-number") != null, "NAME & NUMBER has both HIDE NUMBER rows");
            OpenShell.Instance.Nav.Focus(Item("garage-hide-bike-number")); yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("hn-page-shown");
            var shown = ReflexPlateText.ForSpecs(null, null);
            Check(!shown.hideBikeNumber && !shown.hideRiderNumber, "both shown by default: " + shown);
            yield return Shots("shown");

            shell.ShowGarage(OpenShell.GarageIdentity); yield return UiShowcase.Frames(120);
            yield return Submit("garage-hide-bike-number", 60);
            yield return Submit("garage-hide-gear-number", 60);
            Check(ReflexPlateText.HideBikeSetting && ReflexPlateText.HideRiderSetting
                  && Options.ReflexSave.GetBool(ReflexPlateText.HideBikeKey) && Options.ReflexSave.GetBool(ReflexPlateText.HideRiderKey), "the rows switch both on (save keys)");
            var hidden = ReflexPlateText.ForSpecs(null, null);
            Check(hidden.hideBikeNumber && hidden.hideRiderNumber && hidden.Key != shown.Key && hidden.Flags == (shown.Flags | 24), "the preview's plate text hides both: " + hidden + " flags " + hidden.Flags);
            Check(new ReflexPlateText { name = hidden.name, number = hidden.number }.WithFlags(hidden.Flags).Key == hidden.Key, "flags round-trip (online look / ghost)");
            OpenShell.Instance.Nav.Focus(Item("garage-hide-bike-number")); yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("hn-page-hidden");
            yield return Shots("hidden");

            string evName = Environment.GetEnvironmentVariable("REFLEX_HIDENUMBER_EVENT") ?? "Supercross/SX_DLC01_FreightYard";
            if (evName != "none")
            {
                shell.ShowMain(); yield return UiShowcase.Frames(30);
                var ev = ReflexEvents.Find(evName) ?? ReflexEvents.All.FirstOrDefault(e => e.spec.type == "Supercross");
                Debug.Log(Tag + "launching " + ev?.key);
                ReflexEventSelect.Launch(ev.key, 3, false);
                OriginalWorldRuntime rt = null;
                for (float t = 0; t < 400f; t += Time.unscaledDeltaTime)
                {
                    rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                    if (rt && rt.FailureMessage != null) { Check(false, "world: " + rt.FailureMessage); break; }
                    if (rt && rt.Running && (rt.IntroCardShown || rt.EventUi.race_state >= 1)) break;
                    yield return null;
                }
                if (rt && rt.Running)
                {
                    var local = rt.Roster?.Local;
                    Debug.Log(Tag + "race local plate: " + local?.plate);
                    Check(local?.plate != null && local.plate.hideBikeNumber && local.plate.hideRiderNumber && (local.plate.Flags & 24) == 24, "the race's local rider hides both numbers (look flags " + local?.plate?.Flags + ")");
                    var others = rt.Roster.entries.Where(e => e != local && e.plate != null).ToList();
                    Check(others.Count > 0 && others.All(e => !e.plate.hideBikeNumber && !e.plate.hideRiderNumber), "the other riders keep their numbers (" + others.Count + ")");
                    float w = Time.realtimeSinceStartup;
                    if (rt.IntroCardShown) { while (Time.realtimeSinceStartup - w < 1.5f) yield return null; yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); }   // SKIP INTRO
                    for (float t = 0; t < 60f && !(rt.Race.state == 2 || rt.Race.valid == 0); t += Time.unscaledDeltaTime) yield return null;   // at the gate
                    w = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - w < 2f) yield return null;
                    yield return UiShowcase.Shot("hn-race-gate");
                    for (float t = 0; t < 30f && rt.Race.state == 2; t += Time.unscaledDeltaTime) yield return null;
                    UiShowcaseNav.PadState(new UnityEngine.InputSystem.LowLevel.GamepadState { rightTrigger = 0.8f });
                    w = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - w < 4f) yield return null;
                    yield return UiShowcase.Shot("hn-race-ride");
                    UiShowcaseNav.PadState(new UnityEngine.InputSystem.LowLevel.GamepadState());
                }
                else Check(false, "the race did not run");
            }
            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
            yield return UiShowcase.Frames(10);
        }
    }
}
