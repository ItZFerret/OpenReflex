// TEST ONLY: the MODERN gauge's REV GAUGE (w-revgauge: Hud/ReflexHudView.cs BuildGauge / UpdateGauge, Hud/ReflexHudModel.cs RpmFloor /
// RpmGauge, Options/ReflexSettings.cs RevGauge, the OPTIONS > CAMERA & HUD row "hud-rev-gauge"). Synthetic models only, no race is ridden.
//   "revgauge-hud"      the gauge on the HUD showcase's backdrop at a few engine speeds (each shot is the whole screen; the gauge is bottom right):
//                       a 250F (idle 1,500, limiter 14,000) at idle, 7,000 (a launch), 10,200 (after an upshift), 12,300 (before one), 13,300
//                       (amber), on the limiter (two moments of the flash), an ATV (1,500 / 11,500) at 9,000, then REV GAUGE SPEED and OFF, and
//                       the gauge-only supplement (presentation 1) over the CLASSIC HUD with RPM; plus checks of RpmFloor / RpmGauge
//   "revgauge-options"  the REV GAUGE row: a save without the key reads RPM, the row cycles RPM > SPEED > OFF > RPM and saves each
using System.Collections;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseRevGauge
    {
        const string Tag = "[ui-showcase] [revgauge] ";
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("revgauge-hud", Hud); UiShowcase.Register("revgauge-options", OptionsPage); }

        // test players are muted (the hub's music too)
        static void Mute()
        {
            AudioListener.volume = 0f; AudioListener.pause = true;
            if (GameObject.Find("revgauge mute TEST ONLY") == null) { var go = new GameObject("revgauge mute TEST ONLY"); Object.DontDestroyOnLoad(go); go.AddComponent<KeepMuted>(); }
        }
        sealed class KeepMuted : MonoBehaviour { void LateUpdate() { AudioListener.volume = 0f; AudioListener.pause = true; } }

        static void Check(bool ok, string what) { UiShowcaseNav.Check(ok, "revgauge", what); if (ok) Debug.Log(Tag + "ok " + what); }

        // ------------------------------------------------------------------------------------------------ the gauge
        static ReflexHudModel Model(float idle, float max, float rpm, int gear, float mph)
        {
            var m = new ReflexHudModel { mode = HudMode.FreeRide, valid = true, racing = true, state = 2, haveHud = true, onTrack = true, speedMph = mph, gear = gear, heading = 40f };
            m.hasRpm = true; m.idleRpm = idle; m.maxRpm = max; m.rpm = rpm; m.rpmGauge = ReflexHudModel.RpmGauge(rpm, idle, max);
            return m;
        }

        static IEnumerator Hud()
        {
            Mute();
            Check(Mathf.Approximately(ReflexHudModel.RpmFloor(1500f, 14000f), 4200f) && Mathf.Approximately(ReflexHudModel.RpmFloor(1500f, 11500f), 3450f), "the floor: 0.3 x the limiter (250F 4,200, ATV 3,450)");
            Check(Mathf.Approximately(ReflexHudModel.RpmFloor(3000f, 6000f), 3000f), "the floor is never below idle (a truck idling at 3,000 of 6,000)");
            Check(ReflexHudModel.RpmGauge(1500f, 1500f, 14000f) == 0f && ReflexHudModel.RpmGauge(4200f, 1500f, 14000f) == 0f && ReflexHudModel.RpmGauge(14000f, 1500f, 14000f) == 1f, "empty at idle and at the floor, full on the limiter");
            Check(Mathf.Abs(ReflexHudModel.RpmGauge(9100f, 1500f, 14000f) - 0.5f) < 1e-4f, "half way: 9,100 rpm on the 250F");

            var backdrop = HudShowcase.Backdrop();
            var view = ReflexHudView.Create(null, "HUD gallery revgauge", 30001);
            view.Options.scale = 1f; view.Options.opacity = 1f; view.Options.standings = true; view.Options.gauge = true; view.Options.reduceMotion = false; view.Options.metric = false;
            view.Visible = true;
            yield return Show(view, Model(1500f, 14000f, 1500f, 0, 0f), "revgauge-1-idle");
            yield return Show(view, Model(1500f, 14000f, 7000f, 0, 9f), "revgauge-2-launch-7000");
            yield return Show(view, Model(1500f, 14000f, 10200f, 2, 38f), "revgauge-3-upshift-10200");
            yield return Show(view, Model(1500f, 14000f, 12300f, 2, 46f), "revgauge-4-before-shift-12300");
            yield return Show(view, Model(1500f, 14000f, 13300f, 3, 52f), "revgauge-5-amber-13300");
            yield return Show(view, Model(1500f, 14000f, 14000f, 1, 30f), "revgauge-6-limiter-a");
            yield return new WaitForSecondsRealtime(0.1f);
            view.Apply(Model(1500f, 14000f, 14000f, 1, 30f), 0.1f, 2);
            yield return UiShowcase.Shot("revgauge-6-limiter-b");
            yield return Show(view, Model(1500f, 11500f, 9000f, 1, 33f), "revgauge-7-atv-9000");
            view.Options.revGauge = ReflexSettings.RevGaugeSpeed;
            yield return Show(view, Model(1500f, 14000f, 12300f, 2, 46f), "revgauge-8-speed-46mph");
            view.Options.revGauge = ReflexSettings.RevGaugeOff;
            yield return Show(view, Model(1500f, 14000f, 12300f, 2, 46f), "revgauge-9-off");
            view.Options.revGauge = ReflexSettings.RevGaugeRpm;
            yield return Show(view, Model(1500f, 14000f, 11000f, 2, 41f), "revgauge-10-classic-supplement", 1);
            view.Destroy();
            Object.Destroy(backdrop);
            ReflexHudKit.Scale = 1f; ReflexHudKit.PanelOpacity = 1f;
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator Show(ReflexHudView view, ReflexHudModel m, string shot, int presentation = 2)
        {
            float t0 = Time.unscaledTime;
            while (Time.unscaledTime < t0 + 1.0f) { view.Apply(m, Time.unscaledDeltaTime, presentation); yield return null; }
            view.Apply(m, 0.0001f, presentation);
            Debug.Log(Tag + shot + ": rpm " + m.rpm + " of " + m.idleRpm + " / " + m.maxRpm + ", gauge " + m.rpmGauge.ToString("F3") + ", ring " + ReflexSettings.RevGaugeNames[view.Options.revGauge]);
            yield return UiShowcase.Shot(shot);
        }

        // ------------------------------------------------------------------------------------------------ the option
        static VisualElement Q(string name) => ReflexOptions.Root?.Q<VisualElement>(name);
        static bool Adjust(string name, int d) { var v = Q(name); if (v?.userData is OuNavItem it && it.adjust != null) { it.adjust(d); return true; } Debug.LogWarning(Tag + "no value row " + name); return false; }
        static string Value(string name) => Q(name)?.Q<Label>(className: "mxa-value")?.text;
        static IEnumerator Reveal(string name)
        {
            var v = Q(name); if (v == null) yield break;
            v.Focus(); var sv = v.GetFirstAncestorOfType<ScrollView>(); sv?.ScrollTo(v);
            yield return UiShowcase.Frames(12);
        }

        static IEnumerator OptionsPage()
        {
            Mute();
            ReflexSave.Delete("Options.HudRevGauge");
            Check(ReflexSettings.RevGauge == ReflexSettings.RevGaugeRpm, "a save without Options.HudRevGauge reads RPM");
            var shell = OpenShell.Instance;
            if (shell != null) shell.ShowOptions(ReflexOptions.CameraHud); else ReflexOptions.Open(ReflexOptions.CameraHud);
            yield return UiShowcase.Frames(40);
            Check(Q("hud-rev-gauge") != null, "OPTIONS > CAMERA & HUD has the REV GAUGE row");
            yield return Reveal("hud-rev-gauge");
            Check(Value("hud-rev-gauge") == "RPM", "REV GAUGE shows RPM (" + Value("hud-rev-gauge") + ")");
            yield return UiShowcase.Shot("revgauge-options-1-rpm");
            Adjust("hud-rev-gauge", 1); yield return UiShowcase.Frames(6);
            Check(Value("hud-rev-gauge") == "SPEED" && ReflexSettings.RevGauge == ReflexSettings.RevGaugeSpeed && ReflexSave.GetInt("Options.HudRevGauge", -1) == 1, "right: SPEED, saved 1");
            yield return UiShowcase.Shot("revgauge-options-2-speed");
            Adjust("hud-rev-gauge", 1); yield return UiShowcase.Frames(6);
            Check(Value("hud-rev-gauge") == "OFF" && ReflexSettings.RevGauge == ReflexSettings.RevGaugeOff, "right: OFF");
            Adjust("hud-rev-gauge", 1); yield return UiShowcase.Frames(6);
            Check(Value("hud-rev-gauge") == "RPM" && ReflexSettings.RevGauge == ReflexSettings.RevGaugeRpm, "right: back to RPM");
            Adjust("hud-rev-gauge", -1); yield return UiShowcase.Frames(6);
            Check(Value("hud-rev-gauge") == "OFF", "left from RPM: OFF");
            var o = new HudOptions(); o.FromSettings();
            Check(o.revGauge == ReflexSettings.RevGaugeOff, "the HUD's options read it (HudOptions.revGauge)");
            Adjust("hud-rev-gauge", 1); yield return UiShowcase.Frames(6);
            ReflexOptions.Close();
            yield return UiShowcase.Frames(20);
        }
    }
}
