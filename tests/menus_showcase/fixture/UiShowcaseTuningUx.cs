// TEST ONLY: the tuning rows (OpenFrontEnd/OuTuneRow.cs) of GARAGE > VEHICLE > VEHICLE TUNING and the race pause's TUNING. Page (a scratch save,
// REFLEX_SAVE_DIR; muted):
//   tuning-ux  UNLOCK EVERYTHING, the equipped bike gets the exhaust and the tires with the most tuning points (its suspension stays stock: the
//              row with no points), then VEHICLE TUNING: the page as it opens (tune-open), ENGINE moved two steps toward TOP SPEED
//              (tune-engine-right), all the way to the left (tune-engine-max-left), BRAKING one step to HARD (tune-braking-hard), STEERING
//              focused (tune-steering); checks: the ends' words, the verdicts, RIGHT moves toward the right-hand end, the values saved, the
//              stock suspension row refuses; then the race pause's TUNING page with the same rows (tune-pause).
// Log lines "[tuning-test] ..." (PASS / FAIL).
using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseTuningUx
    {
        const string Tag = "[tuning-test] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        static int fails;
        static void Check(bool ok, string what) { Debug.Log(Tag + (ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("tuning-ux", Page); }

        sealed class Mute : MonoBehaviour { void LateUpdate() { if (AudioListener.volume != 0f) AudioListener.volume = 0f; } }   // test players are muted

        static VisualElement Item(string name) => OpenShell.Instance.Nav.Items().FirstOrDefault(e => e.name == name);
        static IEnumerator Focus(string name, int frames = 40)
        {
            var e = Item(name);
            if (e == null) { Check(false, "no menu entry " + name); yield break; }
            OpenShell.Instance.Nav.Focus(e); yield return UiShowcase.Frames(frames);
        }
        static IEnumerator Adjust(string name, int d, int times = 1)
        {
            var e = Item(name); var item = e?.userData as OuNavItem;
            for (int i = 0; i < times && item?.adjust != null; i++) { item.adjust(d); yield return UiShowcase.Frames(12); }
            yield return UiShowcase.Frames(20);
        }
        static string Verdict(string name) => Item(name)?.Q<Label>(className: "ou-tune2-verdict")?.text ?? "?";
        static string[] Ends(string name) => Item(name)?.Query<Label>(className: "ou-tune2-end-name").ToList().Select(l => l.text).ToArray() ?? new string[0];
        static int[] Tuning()
        {
            var tt = OpenGarageData.Tab("PlayerVehicleTuning"); int row = tt.Find("PlayerVehicleId", OpenGarageData.Equipped);
            return OuTuneRow.Columns.Select(c => tt.Int(c, row, 0)).ToArray();
        }

        static IEnumerator Page()
        {
            UiShowcaseNav.IsolateDevices();
            AudioListener.volume = 0f; new GameObject("tuning-mute").AddComponent<Mute>().hideFlags = HideFlags.DontSave;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (!CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            int pv = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            // the exhaust and the tires with the most points (the suspension stays stock)
            foreach (var ty in OpenGarageData.PartTypes(pv).Where(t => !string.IsNullOrEmpty(t.tuning)))
            {
                if (ty.tuning.IndexOf("susp", StringComparison.OrdinalIgnoreCase) >= 0) continue;   // (forks / shocks: the suspension points)
                int Points(GarageItem it) { var r = FeGarageData.Unique("VehiclePart", "VehiclePartId", it.id); return FeTables.I(r, "DefaultTuneAttribute1", 0) + FeTables.I(r, "DefaultTuneAttribute2", 0); }
                var best = OpenGarageData.PartItems(model, pv, ty.id).Where(i => i.Available && i.kit == null).OrderByDescending(Points).FirstOrDefault();
                bool ok = best != null && OpenGarageData.EquipPart(pv, best, ty.tuning);
                Debug.Log(Tag + "part " + ty.factory + " (" + ty.tuning + "): " + best?.title + " points " + (best == null ? 0 : Points(best)) + " equipped " + ok);
                ReflexProfile.Open();
            }
            var shell = OpenShell.Instance;
            shell.ShowGarage(OpenShell.GarageTuning); yield return UiShowcase.Frames(200);
            yield return Focus("garage-tuning-0", 60);
            yield return UiShowcase.Shot("tune-open");
            var ends = Enumerable.Range(0, 5).Select(i => string.Join("/", Ends("garage-tuning-" + i))).ToArray();
            Debug.Log(Tag + "ends: " + string.Join(" | ", ends) + "; verdicts: " + string.Join(" | ", Enumerable.Range(0, 5).Select(i => Verdict("garage-tuning-" + i))));
            Check(ends.SequenceEqual(new[] { "ACCELERATION/TOP SPEED", "SOFT/HARD", "SLIPPERY/STICKY", "SOFT/HARD", "SLOW/QUICK" }), "every row names its two ends in the game's words");

            var t0 = Tuning();
            yield return Adjust("garage-tuning-0", +1, 2);
            var t1 = Tuning();
            Check(t1[1] == t0[1] + 10 && t1[0] == t0[0] - 10, $"RIGHT on ENGINE moves points to TOP SPEED (accel {t0[0]} -> {t1[0]}, top speed {t0[1]} -> {t1[1]}, saved): '{Verdict("garage-tuning-0")}'");
            yield return UiShowcase.Shot("tune-engine-right");
            yield return Adjust("garage-tuning-0", -1, 40);
            Check(Verdict("garage-tuning-0") == "MAX ACCELERATION", "LEFT all the way: '" + Verdict("garage-tuning-0") + "'");
            yield return UiShowcase.Shot("tune-engine-max-left");
            Check(Verdict("garage-tuning-1").StartsWith("STOCK PART"), "the stock suspension says it has no points: '" + Verdict("garage-tuning-1") + "'");
            var ts = Tuning(); yield return Adjust("garage-tuning-1", +1);
            Check(Tuning().SequenceEqual(ts), "the stock suspension does not move");

            yield return Focus("garage-tuning-3", 40);
            string braking = Verdict("garage-tuning-3");
            yield return Adjust("garage-tuning-3", +1);
            Check(braking == "STANDARD" && Verdict("garage-tuning-3") == "HARD" && Tuning()[6] == 100, $"BRAKING STANDARD -> HARD ('{braking}' -> '{Verdict("garage-tuning-3")}', {Tuning()[6]})");
            yield return UiShowcase.Shot("tune-braking-hard");
            yield return Focus("garage-tuning-4", 40);
            yield return UiShowcase.Shot("tune-steering");

            // the race pause's TUNING (the same rows)
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase pause");
            var page = go.AddComponent<OwnRacePause>(); page.enabled = false;
            typeof(OwnRacePause).GetField("open", All).SetValue(page, true);
            UiShowcasePages.Call(page, "Build");
            UiShowcasePages.Call(page, "ShowTuning");
            UiShowcasePages.DriveOf(go, page);
            yield return UiShowcase.Frames(80);
            yield return UiShowcase.Shot("tune-pause");
            typeof(OwnRacePause).GetField("open", All).SetValue(page, false); UnityEngine.Object.Destroy(go);
            UiShowcase.ShellVisible(true);

            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
            yield return UiShowcase.Frames(10);
        }
    }
}
