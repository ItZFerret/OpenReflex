// TEST ONLY: the garage's high-resolution bike with the Maxxis IT / SI / SX tyres (ReflexBikeModel.HighResAll). Their HR twins name their
// models apart from the normal package (OBJ_FRT_Tire_Maxxis -> HR_OBJ_FRT_Tire_Maxxis_IT, HR_OBJ_FRT_TireMaxxis_SX): the HR build used to
// fail on them and the garage fell back to the race models of the whole bike (low poly, low-res textures). Page (a scratch save,
// REFLEX_SAVE_DIR; muted):
//   hr-tires   UNLOCK EVERYTHING; for each of the Maxxis tyres (and Maxxis SM, whose twin was always found, as a control): equip it on the equipped bike, check that the
//              HR spec takes the tyre's twin with both of its models and builds, show GARAGE > VEHICLE > CUSTOMIZE (the equipped bike: hr-tire-<name>; then the same bike with the race models, as the
//              fallback showed it: hr-tire-<name>-race-models) and check that the
//              garage preview did not fall back ("[Garage] preview with the high-resolution models" not logged).
// Log lines "[hrtires-test] ..." (PASS / FAIL).
using System;
using System.Collections;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseHrTires
    {
        const string Tag = "[hrtires-test] ";
        const System.Reflection.BindingFlags All = System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public;
        static int fails;
        static void Check(bool ok, string what) { Debug.Log(Tag + (ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("hr-tires", Page); }

        sealed class Mute : MonoBehaviour { void LateUpdate() { if (AudioListener.volume != 0f) AudioListener.volume = 0f; } }   // test players are muted

        static int fallbacks;
        static void OnLog(string msg, string stack, LogType type) { if (msg.Contains("preview with the high-resolution models")) fallbacks++; }

        static IEnumerator Page()
        {
            UiShowcaseNav.IsolateDevices();
            AudioListener.volume = 0f; new GameObject("hrtires-mute").AddComponent<Mute>().hideFlags = HideFlags.DontSave;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (!CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            Application.logMessageReceived += OnLog;
            int pv = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            var tireType = OpenGarageData.PartTypes(pv).FirstOrDefault(t => t.factory.IndexOf("Tire", StringComparison.OrdinalIgnoreCase) >= 0);
            Check(tireType.factory != null, "the equipped bike has a tyre slot (" + tireType.factory + ")");
            var tables = OpenGarageData.PartTables();
            string Pkg(GarageItem it) => OpenGarageData.PartPackages(tables, it.id).model?.pkg ?? "";
            var items = OpenGarageData.PartItems(model, pv, tireType.id);
            var shell = OpenShell.Instance;
            foreach (string want in new[] { "MX_Tire_Maxxis_IT", "MX_Tire_Maxxis_SI", "MX_Tire_Maxxis_SX", "MX_Tire_Maxxis_SM" })
            {
                var it = items.FirstOrDefault(i => Pkg(i).Equals(want, StringComparison.OrdinalIgnoreCase));
                if (it == null) { Check(false, want + ": no such tyre for this bike (" + string.Join(",", items.Select(Pkg)) + ")"); continue; }
                Check(OpenGarageData.EquipPart(pv, it, tireType.tuning), want + ": equipped (" + it.title + ")");
                ReflexProfile.Open();
                var spec = OpenGarageData.BikeOfModel(model);
                var hr = ReflexBikeModel.HighRes(spec);
                Debug.Log(Tag + want + ": normal " + spec.tirePkg + " " + spec.frontModel + " / " + spec.rearModel + "; HR " + hr.chassisPkg + ", " + hr.tirePkg + " " + hr.frontModel + " / " + hr.rearModel);
                Check(hr.tirePkg == "HR_" + want && hr.frontModel.StartsWith("HR_OBJ_FRT_Tire") && hr.rearModel.StartsWith("HR_OBJ_BCK_Tire") && hr.chassisPkg.StartsWith("HR_"),
                      want + ": the HR spec takes the tyre's twin and the HR chassis");
                ReflexBikeModel built = null;
                try { built = ReflexBikeModel.Build(spec, highRes: true); Check(built != null, want + ": the HR bike builds"); }
                catch (Exception e) { Check(false, want + ": the HR bike builds: " + e.Message); }
                built?.Destroy();
                int before = fallbacks;
                shell.ShowGarage(OpenShell.GarageParts); yield return UiShowcase.Frames(240);
                Check(fallbacks == before, want + ": the garage preview kept the high-resolution models");
                string shot = "hr-tire-" + want.Substring("MX_Tire_".Length);
                yield return UiShowcase.Shot(shot);
                // the same bike with the race models (what the failed HR build fell back to), for the comparison
                var preview = typeof(OpenShell).GetField("garagePreview", All)?.GetValue(shell) as FeGarage;
                if (preview != null)
                {
                    preview.highRes = false; preview.Unload(); typeof(OpenShell).GetField("garageShownKey", All)?.SetValue(shell, null);
                    shell.ShowGarage(OpenShell.GarageHome); yield return UiShowcase.Frames(20);
                    shell.ShowGarage(OpenShell.GarageParts); yield return UiShowcase.Frames(240);
                    yield return UiShowcase.Shot(shot + "-race-models");
                    preview.highRes = true; preview.Unload(); typeof(OpenShell).GetField("garageShownKey", All)?.SetValue(shell, null);
                }
                shell.ShowGarage(OpenShell.GarageHome); yield return UiShowcase.Frames(30);
            }
            Application.logMessageReceived -= OnLog;
            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
            yield return UiShowcase.Frames(10);
        }
    }
}
