// TEST ONLY: the rider's number on a bike mod's plates (w-fb-modplates). Page (a scratch save, REFLEX_SAVE_DIR; the client root's Mods/Bikes):
//   modplates   GARAGE > VEHICLE (unlock everything, the profile's plate FERRET / 998): for a few bikes that a mod re-skins, the original's card and
//               then the mod's card, each from the left, the right, the front plate and three quarters (modplates-<n>-<title>-<stock|mod>-<left|right|front|frontclose|34>.png);
//               REFLEX_MODPLATES_TITLES = comma-separated title parts picks the bikes (default: three spread over the list; "all": every bike with a mod
//               card), REFLEX_MODPLATES_STOCK=0 leaves the originals out. The log lines
//               "[modplates-test] ..." carry the plate slots of the shown models (how many materials take the bike plate).
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseModPlates
    {
        const string Tag = "[modplates-test] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("modplates", Page);

        static Type StageType => typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
        static object Stage() => typeof(OpenShell).GetField("garageStage", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance);
        static void Angles(float yaw, float pitch) { var st = Stage(); st.GetType().GetMethod("SetAngles").Invoke(st, new object[] { yaw, pitch }); }
        static void Frame(string shot) { var st = Stage(); st.GetType().GetMethod("Frame").Invoke(st, new object[] { Enum.Parse(StageType.GetNestedType("Shot"), shot), true, true }); }
        static void Zoom(float amount) { var st = Stage(); st.GetType().GetMethod("Zoom").Invoke(st, new object[] { amount }); }

        static string Slots()
        {
            var sb = new System.Text.StringBuilder();
            foreach (var h in UnityEngine.Object.FindObjectsByType<ReflexPlateTextures>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
            {
                int n = 0, mats = 0; var names = new HashSet<string>();
                foreach (var r in h.GetComponentsInChildren<Renderer>(true))
                    foreach (var m in r.sharedMaterials)
                    {
                        mats++; string tag = m ? m.GetTag(ReflexPlates.Tag, false, "") : "";
                        if (tag.Length > 0) { n++; names.Add(tag.Split(':')[0] + ":" + tag.Split(':')[1]); }
                    }
                sb.Append(" [" + h.gameObject.name + ": " + n + " of " + mats + " materials take " + string.Join("/", names) + "; bike plate " + (h.bike ? h.bike.name : "none") + "]");
            }
            return sb.ToString();
        }

        static IEnumerator Shots(string label)
        {
            Frame("Vehicle"); Angles(0, 8); Zoom(0.45f); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot(label + "-left");
            Frame("Vehicle"); Angles(180, 8); Zoom(0.45f); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot(label + "-right");
            Frame("Front"); Angles(70, 6); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot(label + "-front");
            Frame("Front"); Angles(70, 4); Zoom(0.35f); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot(label + "-frontclose");
            Frame("Vehicle"); Angles(40, 10); Zoom(0.6f); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot(label + "-34");
        }

        static IEnumerator Page()
        {
            if (BikeMods.All.Count == 0) { Debug.LogError(Tag + "FAIL no bike mod in the client root"); yield break; }
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (!CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            CoreGarageProfile.SetRiderText("Name", "FERRET"); CoreGarageProfile.SetRiderText("Number", "998");
            ReflexProfile.Open();
            StageType.GetField("NoAutoTurn").SetValue(null, true);
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            var shell = OpenShell.Instance;
            shell.ShowGarage(OpenShell.GarageVehicle);
            for (int i = 0; i < 220; i++) yield return null;

            var all = OpenGarageData.Vehicles();
            bool HasMod(GarageItem v) => shell.Nav.Items().Any(e => e.name == "garage-mod-" + v.id + "-0");
            var withMod = all.Where(HasMod).GroupBy(v => v.id).Select(g => g.First()).ToList();
            Debug.Log(Tag + "vehicles " + all.Count + ", with a mod card " + withMod.Count + ": " + string.Join(" | ", withMod.Select(v => v.title + "#" + v.id)));
            var want = (Environment.GetEnvironmentVariable("REFLEX_MODPLATES_TITLES") ?? "").Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            var picks = want.Length == 1 && want[0] == "all" ? withMod : want.Length > 0 ? want.Select(w => withMod.FirstOrDefault(v => v.title.IndexOf(w.Trim(), StringComparison.OrdinalIgnoreCase) >= 0)).Where(v => v != null).ToList()
                      : withMod.Count == 0 ? new List<GarageItem>() : new[] { 0, withMod.Count / 2, withMod.Count - 1 }.Distinct().Select(k => withMod[k]).ToList();
            int n = 0;
            foreach (var v in picks)
            {
                string label = "modplates-" + n++ + "-" + v.title.Replace(' ', '_').ToLowerInvariant();
                var stock = shell.Nav.Items().FirstOrDefault(e => e.name != null && (e.name == "garage-model-" + v.id || e.name == "garage-vehicle-" + v.playerVehicle));
                if (stock != null && Environment.GetEnvironmentVariable("REFLEX_MODPLATES_STOCK") != "0")
                {
                    shell.Nav.Focus(stock); for (int i = 0; i < 200; i++) yield return null;
                    Debug.Log(Tag + label + " stock: slots" + Slots());
                    yield return Shots(label + "-stock");
                }
                else Debug.Log(Tag + "no original card for " + v.title);
                var mod = shell.Nav.Items().FirstOrDefault(e => e.name == "garage-mod-" + v.id + "-0");
                if (mod == null) { Debug.Log(Tag + "FAIL no mod card for " + v.title); continue; }
                shell.Nav.Focus(mod); for (int i = 0; i < 330; i++) yield return null;
                Debug.Log(Tag + label + " mod: slots" + Slots());
                yield return Shots(label + "-mod");
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }
    }
}
