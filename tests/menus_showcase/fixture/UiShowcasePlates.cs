// TEST ONLY: the player's own jersey name / number on the stage's models (w-plates). Pages (a scratch save, REFLEX_SAVE_DIR, nothing of the user's):
//   plates-garage   GARAGE > RIDER: the default RIDER / 998, then the JERSEY NAME row typed to FERRET (the real text field path: the plates are made again
//                   at once) and the NUMBER changed 998 -> 42 -> 998: the bike from its four sides, the front number plate, the jersey back and the chest
//                   protector close up (plates-garage-*.png); the log lines "[plates-test] ..." carry the plate text on the models and the dark / jersey / invert flags
//   plates-classes  GARAGE > VEHICLE (unlock everything): one vehicle of every class (MX, ATV, UTV, trucks, buggy, and a bike mod when the client root has one), the side
//                   and the front close up (plates-class-<class>-*.png)
//   plates-hub      the hub's hero (the menu world), the name typed in the garage, back on the hub (plates-hub-*.png), and the MOTOCARD portrait's key
//   plates-pro      a pro rider on his own vehicle: his own name and number (plates-pro-*.png)
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcasePlates
    {
        const string Tag = "[plates-test] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("plates-garage", Garage);
            UiShowcase.Register("plates-classes", Classes);
            UiShowcase.Register("plates-hub", Hub);
            UiShowcase.Register("plates-pro", Pro);
        }

        static Type StageType => typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
        static object Stage() => typeof(OpenShell).GetField("garageStage", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance);
        static void Angles(float yaw, float pitch) { var st = Stage(); st.GetType().GetMethod("SetAngles").Invoke(st, new object[] { yaw, pitch }); }
        static void Frame(string shot, bool reset = true)
        {
            var st = Stage(); var type = StageType;
            var shotType = type.GetNestedType("Shot");
            st.GetType().GetMethod("Frame").Invoke(st, new object[] { Enum.Parse(shotType, shot), reset, true });
        }
        /// The close shots' fit (OpenGarageStage.detailFitScale: < 1 comes closer).
        static void Detail(float scale) { var st = Stage(); st.GetType().GetField("detailFitScale").SetValue(st, scale); }
        static void Zoom(float amount) { var st = Stage(); st.GetType().GetMethod("Zoom").Invoke(st, new object[] { amount }); }
        static Reflex.Port.FrontEnd.FeGarage Preview() => (Reflex.Port.FrontEnd.FeGarage)typeof(OpenShell).GetField("garagePreview", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance);
        static VisualElement El(string name) => OpenShell.Instance.Layer.Q<VisualElement>(name);

        static IEnumerator Open(int tab, bool unlock)
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (unlock && !CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            StageType.GetField("NoAutoTurn").SetValue(null, true);
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            OpenShell.Instance.ShowGarage(tab);
            for (int i = 0; i < 220; i++) yield return null;
        }

        /// How many materials of the models carry a run-time plate slot (ReflexPlates.Tag) and which textures the slots name.
        static string Slots()
        {
            var sb = new System.Text.StringBuilder();
            foreach (var h in UnityEngine.Object.FindObjectsByType<ReflexPlateTextures>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
            {
                int n = 0; var names = new HashSet<string>(); int mats = 0;
                foreach (var r in h.GetComponentsInChildren<Renderer>(true))
                    foreach (var m in r.sharedMaterials)
                    {
                        mats++; string tag = m ? m.GetTag(ReflexPlates.Tag, false, "") : "";
                        if (tag.Length > 0) { n++; names.Add(tag.Split(':')[1]); }
                    }
                sb.Append(" [" + h.gameObject.name + ": " + n + " of " + mats + " materials take " + string.Join("/", names) + "]");
            }
            return sb.ToString();
        }

        static void Report(string what)
        {
            var g = Preview();
            var t = ReflexPlateText.ForSpecs(g.ShownBike, g.ShownRider);
            Debug.Log(Tag + what + ": plates on the models '" + g.PlateKey + "'; the profile says " + t + (g.ShownRider != null && g.ShownRider.plateRider > 0 ? " (pro rider " + g.ShownRider.plateRider + ")" : "") +
                      "; slots" + Slots() + "; stage world mode " + g.world + ", highRes " + g.highRes + ", vehicle " + g.VehicleKind);
        }

        /// Types into a garage text row exactly as a player does (the row's field; the value-changed path saves it).
        static bool Type(string row, string text)
        {
            var field = El(row)?.Q<TextField>();
            if (field == null) { Debug.Log(Tag + "FAIL no text field " + row); return false; }
            field.value = text;
            return true;
        }

        static IEnumerator Around(string label, bool close = true)
        {
            foreach (int yaw in new[] { 0, 90, 180, 270 })
            {
                Angles(yaw, 8); for (int i = 0; i < 25; i++) yield return null;
                yield return UiShowcase.Shot("plates-" + label + "-" + yaw);
            }
        }

        // ------------------------------------------------------------------------------------------------ the garage

        static IEnumerator Garage()
        {
            yield return Open(OpenShell.GarageIdentity, false);   // (NAME & NUMBER: the jersey name and number rows)
            var shell = OpenShell.Instance;
            Report("garage opened (the default profile)");
            Frame("Vehicle"); Angles(0, 8); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot("plates-garage-0-default-RIDER-998");
            // the real input path: the JERSEY NAME field typed to FERRET
            Type("garage-jersey-name", "FERRET");
            for (int i = 0; i < 20; i++) yield return null;
            Report("name typed");
            Frame("Vehicle");
            yield return Around("garage-ferret-998");
            // the front number plate and the side panels close up (the bike's left, front), the jersey back and the chest protector
            Frame("Front"); Angles(70, 6); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot("plates-garage-front-plate");
            Frame("Vehicle"); Angles(0, 6); Zoom(0.55f); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot("plates-garage-side-plate-left");
            Frame("Vehicle"); Angles(180, 6); Zoom(0.55f); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot("plates-garage-side-plate-right");
            Detail(0.42f);
            Frame("Torso"); foreach (int yaw in new[] { 225, 255, 285 }) { Angles(yaw, 4); for (int i = 0; i < 60; i++) yield return null; yield return UiShowcase.Shot("plates-garage-jersey-back-" + yaw); }
            Detail(1f);
            Frame("Rider"); Angles(255, 4); for (int i = 0; i < 60; i++) yield return null;
            yield return UiShowcase.Shot("plates-garage-rider-back");
            // a new number is on the models at once
            var number = El("garage-number");
            if (number != null)
            {
                var item = shell.Nav.Items().FirstOrDefault(e => e.name == "garage-number");
                for (int step = 0; step < 4 && item != null; step++) { (item.userData as OuNavItem)?.adjust?.Invoke(-1); }
            }
            for (int i = 0; i < 20; i++) yield return null;
            Report("number stepped down");
            Frame("Vehicle"); Angles(0, 6); Zoom(0.55f); for (int i = 0; i < 40; i++) yield return null;
            yield return UiShowcase.Shot("plates-garage-number-changed-side");
            Detail(0.42f); Frame("Torso"); Angles(255, 4); for (int i = 0; i < 60; i++) yield return null;
            yield return UiShowcase.Shot("plates-garage-number-changed-back"); Detail(1f);
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ every class

        static IEnumerator Classes()
        {
            yield return Open(OpenShell.GarageVehicle, true);
            Type("garage-jersey-name", "FERRET");   // (not on this tab: the profile is set below)
            CoreGarageProfile.SetRiderText("Name", "FERRET"); CoreGarageProfile.SetRiderText("Number", "998"); ReflexProfile.Open();
            var shell = OpenShell.Instance;
            var all = OpenGarageData.Vehicles();
            var picks = new List<GarageItem>();
            foreach (var g in all.GroupBy(v => v.type)) picks.Add(g.First());
            var only = (Environment.GetEnvironmentVariable("REFLEX_PLATES_ONLY") ?? "").Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            Debug.Log(Tag + "vehicles " + all.Count + ", classes " + string.Join(" | ", picks.Select(p => p.type + ":" + p.title + "#" + p.id)));
            foreach (var v in picks)
            {
                var item = shell.Nav.Items().FirstOrDefault(e => e.name != null && (e.name == "garage-model-" + v.id || e.name == "garage-vehicle-" + v.playerVehicle));
                if (item == null) { Debug.Log(Tag + "FAIL no card for " + v.title); continue; }
                string label = (OpenGarageData.TypeName(v.type) + "-" + v.title).Replace(' ', '_').ToLowerInvariant();
                if (only.Length > 0 && !only.Any(o => label.Contains(o))) continue;
                shell.Nav.Focus(item);
                for (int i = 0; i < 150; i++) yield return null;
                Report(label);
                foreach (int yaw in new[] { 0, 180, 90 })
                {
                    Frame("Vehicle"); Angles(yaw, 8); Zoom(yaw == 90 ? 0.3f : 0.45f); for (int i = 0; i < 40; i++) yield return null;
                    yield return UiShowcase.Shot("plates-class-" + label + "-" + yaw);
                }
            }
            if (BikeMods.All.Count > 0 && (only.Length == 0 || only.Contains("mod")))
            {
                var blaze = all.FirstOrDefault(v => v.title.IndexOf("BLAZE 250F", StringComparison.OrdinalIgnoreCase) >= 0) ?? all.First();
                foreach (var name in new[] { "garage-mod-" + blaze.id + "-0" })
                {
                    var item = shell.Nav.Items().FirstOrDefault(e => e.name == name);
                    if (item == null) { Debug.Log(Tag + "no mod card " + name); continue; }
                    shell.Nav.Focus(item);
                    for (int i = 0; i < 330; i++) yield return null;
                    Report("mod " + name);
                    foreach (int yaw in new[] { 0, 180, 90 })
                    {
                        Frame("Vehicle"); Angles(yaw, 8); Zoom(0.45f); for (int i = 0; i < 40; i++) yield return null;
                        yield return UiShowcase.Shot("plates-class-mod-" + yaw);
                    }
                }
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ the hub

        static IEnumerator Hub()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            OpenMenuWorld.Still = true;
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            var shell = OpenShell.Instance;
            shell.ShowMain();
            float t0 = Time.realtimeSinceStartup;
            while (OpenMenuWorld.Instance == null && Time.realtimeSinceStartup - t0 < 10f) yield return null;
            var w = OpenMenuWorld.Instance;
            if (w == null) { Debug.Log(Tag + "no menu world"); yield break; }
            while (w.Status == OpenMenuWorld.State.Loading && Time.realtimeSinceStartup - t0 < 120f) yield return null;
            while ((w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t0 < 150f) yield return null;
            for (int i = 0; i < 6 * 60 && w.Walking; i++) yield return null;
            w.SetWalkTime(99f);
            for (int i = 0; i < 60; i++) yield return null;
            Debug.Log(Tag + "hub hero plates '" + w.Hero.PlateKey + "'");
            yield return UiShowcase.Shot("plates-hub-default");
            // the name typed in the garage, then back on the hub
            shell.ShowGarage(OpenShell.GarageIdentity);
            for (int i = 0; i < 160; i++) yield return null;
            Type("garage-jersey-name", "FERRET");
            for (int i = 0; i < 30; i++) yield return null;
            Debug.Log(Tag + "garage (world mode " + Preview().world + ") plates '" + Preview().PlateKey + "'; hero plates '" + w.Hero.PlateKey + "'");
            shell.ShowMain();
            for (int i = 0; i < 200; i++) yield return null;
            Debug.Log(Tag + "hub again: hero plates '" + w.Hero.PlateKey + "', portrait '" + shell.Portrait.Key + "'");
            yield return UiShowcase.Shot("plates-hub-ferret");
            if (w.Hero.RiderModel?.root)
            {   // the hero's back, for the jersey (the hub camera is on his left)
                var cam = w.Camera.transform; var pos = cam.position; var rot = cam.rotation;
                var hero = w.Hero.root.transform.position;
                Vector3 target = hero + new Vector3(0, 4.5f, 0);
                cam.position = target + new Vector3(-5.0f, 1.2f, -5.0f); cam.LookAt(target);
                yield return UiShowcase.Frames(3);
                yield return UiShowcase.Shot("plates-hub-hero-back");
                cam.position = pos; cam.rotation = rot;
            }
            // the MOTOCARD portrait
            var tex = shell.Portrait.Texture;
            if (tex != null)
            {
                var rt = RenderTexture.active; RenderTexture.active = tex;
                var read = new Texture2D(tex.width, tex.height, TextureFormat.RGBA32, false); read.ReadPixels(new Rect(0, 0, tex.width, tex.height), 0, 0); read.Apply();
                RenderTexture.active = rt;
                System.IO.File.WriteAllBytes(System.IO.Path.Combine(Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS"), "plates-portrait.png"), read.EncodeToPNG());
                Debug.Log(Tag + "portrait written " + tex.width + "x" + tex.height);
            }
            OpenMenuWorld.Still = false;
            yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ a pro rider

        static IEnumerator Pro()
        {
            yield return Open(OpenShell.GarageProRiders, false);
            var shell = OpenShell.Instance;
            var pros = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-pro-")).ToList();
            if (pros.Count == 0) { Debug.Log(Tag + "no pro rider in this profile"); yield break; }
            int shown = 0;
            foreach (int k in new[] { 0, pros.Count / 2 }.Distinct())
            {
                (pros[k].userData as OuNavItem)?.submit?.Invoke();
                for (int i = 0; i < 160; i++) yield return null;
                Report("pro " + k + " " + pros[k].name);
                Frame("Vehicle"); Angles(0, 6); Zoom(0.45f); for (int i = 0; i < 40; i++) yield return null;
                yield return UiShowcase.Shot("plates-pro-" + shown + "-side");
                Detail(0.42f); Frame("Torso"); Angles(255, 4); for (int i = 0; i < 60; i++) yield return null;
                yield return UiShowcase.Shot("plates-pro-" + shown + "-back"); Detail(1f);
                shown++;
                shell.ShowGarage(OpenShell.GarageProRiders);
                for (int i = 0; i < 60; i++) yield return null;
                pros = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-pro-")).ToList();
            }
            var pv = ReflexProfile.Current.T("PlayerVehicle");
            if (pv.rows > 0) ReflexProfile.SelectRider(0, -1, pv.Int("PlayerVehicleId", 0)).Wait();
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }
    }
}
