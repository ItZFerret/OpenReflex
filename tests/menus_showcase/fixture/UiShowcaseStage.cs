// TEST ONLY: where the garage rider stands next to the vehicle (w-stage). Pages:
//   stage-classes   GARAGE > VEHICLE (unlock everything): one vehicle of every class (the first of each VehicleType, and the Blaze 250F with and without
//                   the bike mod when a client root with one is given), each from the four sides (orbit 0 / 90 / 180 / 270): shots stage-<class>-<yaw>.png,
//                   and the log line "[stage-test] <vehicle>: clearance X ft, soles Y ft" (shortest distance of the rider's skin to the vehicle's vertices;
//                   the soles' height over the plate and their distance inside its rim), a FAIL when the clearance is under 0.5 ft or a sole is off the floor
//   stage-pro       a pro rider on his own vehicle from the four sides
//   stage-gear      the rider on the GEAR tab wearing each gear piece REFLEX_STAGE_GEAR names (comma-separated filters on "<line> <title>",
//                   default every O'Neal Mayhem colourway and O'Neal Hardwear White/Green), from the four sides and close from three: shots
//                   stage-gear-<line>-<title>-<yaw>.png and -close-<0 | 90 | 270>.png, and per piece the log line "[stage-test] gear <label>: <renderer> -> <material>
//                   [<sampler>:<texture> ...]" (what the jersey's materials sample)
// A scratch save (REFLEX_SAVE_DIR), nothing of the user's.
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
    static class UiShowcaseStage
    {
        const string Tag = "[stage-test] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("stage-classes", Classes);
            UiShowcase.Register("stage-pro", Pro);
            UiShowcase.Register("stage-mods", Mods);
            UiShowcase.Register("stage-fourwheel", FourWheel);
            UiShowcase.Register("stage-gear", Gear);
        }

        static Type StageType => typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
        static object Stage() => typeof(OpenShell).GetField("garageStage", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance);
        static void Angles(float yaw, float pitch) { var st = Stage(); st.GetType().GetMethod("SetAngles").Invoke(st, new object[] { yaw, pitch }); }
        static Reflex.Port.FrontEnd.FeGarage Preview() => (Reflex.Port.FrontEnd.FeGarage)typeof(OpenShell).GetField("garagePreview", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance);

        static IEnumerator Open(int tab)
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (!CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            StageType.GetField("NoAutoTurn").SetValue(null, true);
            Reflex.Port.FrontEnd.FeGarage.Verbose = true;
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            OpenShell.Instance.ShowGarage(tab);
            for (int i = 0; i < 200; i++) yield return null;
        }

        static IEnumerator Around(string label, int waitFrames = 90)
        {
            for (int i = 0; i < waitFrames; i++) yield return null;
            float c = Preview().MeasureClearance(out float sole, out float slack);
            bool bad = c < 0.5f || Mathf.Abs(sole) > 0.15f || slack < 0.3f;
            Debug.Log(Tag + (bad ? "FAIL " : "") + label + ": clearance " + c.ToString("0.00") + " ft, soles " + sole.ToString("0.00") + " ft over the floor, soles " + slack.ToString("0.00") + " ft inside the plate's rim");
            foreach (int yaw in new[] { 0, 90, 180, 270 })
            {
                Angles(yaw, 8); for (int i = 0; i < 25; i++) yield return null;
                yield return UiShowcase.Shot("stage-" + label + "-" + yaw);
            }
        }

        static IEnumerator Classes()
        {
            yield return Open(OpenShell.GarageVehicle);
            var shell = OpenShell.Instance;
            var all = OpenGarageData.Vehicles();
            var picks = new List<GarageItem>();
            foreach (var g in all.GroupBy(v => v.type)) picks.Add(g.First());
            var only = (Environment.GetEnvironmentVariable("REFLEX_STAGE_ONLY") ?? "").Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            Debug.Log(Tag + "vehicles " + all.Count + ", classes " + string.Join(" | ", picks.Select(p => p.type + ":" + p.title + "#" + p.id)));
            foreach (var v in picks)
            {
                var item = shell.Nav.Items().FirstOrDefault(e => e.name != null && (e.name == "garage-model-" + v.id || e.name == "garage-vehicle-" + v.playerVehicle));
                if (item == null) { Debug.Log(Tag + "FAIL no card for " + v.title); continue; }
                string label = (OpenGarageData.TypeName(v.type) + "-" + v.title).Replace(' ', '_').ToLowerInvariant();
                if (only.Length > 0 && !only.Any(o => label.Contains(o))) continue;
                shell.Nav.Focus(item);
                yield return Around(label, 120);
            }
            // the Blaze 250F as the original card and as its mod card (a vehicle of its own)
            if (BikeMods.All.Count > 0 && (only.Length == 0 || only.Contains("mod")))
            {
                var blaze = all.FirstOrDefault(v => v.title.IndexOf("BLAZE 250F", StringComparison.OrdinalIgnoreCase) >= 0) ?? all.First();
                foreach (var name in new[] { "garage-vehicle-" + blaze.playerVehicle, "garage-model-" + blaze.id, "garage-mod-" + blaze.id + "-0" })
                {
                    var item = shell.Nav.Items().FirstOrDefault(e => e.name == name);
                    if (item == null) continue;
                    shell.Nav.Focus(item);
                    yield return Around("mx_lite-blaze250f-" + (name.StartsWith("garage-mod-") ? "mod" : "orig"), name.StartsWith("garage-mod-") ? 330 : 120);
                }
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// Every vehicle that is not a motocross bike (ATVs, UTVs, buggies, trucks) in the VEHICLE list, each from the side, the front, the rear and a three-quarter
        /// view: shots stage-4w-<type>-<title>-<yaw>.png (REFLEX_STAGE_ONLY: a filter on the card's label).
        static IEnumerator FourWheel()
        {
            yield return Open(OpenShell.GarageVehicle);
            var shell = OpenShell.Instance;
            var only = (Environment.GetEnvironmentVariable("REFLEX_STAGE_ONLY") ?? "").Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            var cards = OpenGarageData.Vehicles().Where(v => string.IsNullOrEmpty(v.modId) && !ReflexAtv.FactoryOfType(v.type).StartsWith("Motocross", StringComparison.OrdinalIgnoreCase)).ToList();
            Debug.Log(Tag + "four-wheel cards " + cards.Count + ": " + string.Join(" | ", cards.Select(c => OpenGarageData.TypeName(c.type) + ":" + c.title + "#" + c.id)));
            foreach (var v in cards)
            {
                string label = ("4w-" + OpenGarageData.TypeName(v.type) + "-" + v.title).Replace(' ', '_').ToLowerInvariant();
                if (only.Length > 0 && !only.Any(o => label.Contains(o))) continue;
                var item = shell.Nav.Items().FirstOrDefault(e => e.name != null && (e.name == "garage-model-" + v.id || e.name == "garage-vehicle-" + v.playerVehicle));
                if (item == null) { Debug.Log(Tag + "FAIL no card for " + v.title); continue; }
                shell.Nav.Focus(item);
                for (int i = 0; i < 120; i++) yield return null;
                DumpParts(label);
                foreach (int yaw in new[] { 0, 90, 180, 270 })
                {
                    Angles(yaw, 14); for (int i = 0; i < 25; i++) yield return null;
                    yield return UiShowcase.Shot("stage-" + label + "-" + yaw);
                }
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// Log line per part of the shown vehicle: "[stage-test] parts <label>: <part> | node <hierarchy node> | <material> | <vertices> vertices, world bounds centre / size".
        static void DumpParts(string label)
        {
            var g = Preview();
            var bikeF = g.GetType().GetField("bike", BindingFlags.Instance | BindingFlags.NonPublic);
            var bike = bikeF?.GetValue(g) as ReflexBikeModel;
            if (bike == null) { Debug.Log(Tag + "parts " + label + ": no vehicle"); return; }
            foreach (var p in bike.parts)
            {
                if (!p.part) continue;
                var mf = p.part.GetComponent<MeshFilter>(); var mr = p.part.GetComponent<MeshRenderer>();
                var b = mr ? mr.bounds : default;
                Debug.Log(Tag + "parts " + label + ": " + p.name + " | node " + (p.node >= 0 && p.node < bike.hierarchyNodes.Length ? bike.hierarchyNodes[p.node] : "?") + " | " + p.material
                          + " | " + (mf && mf.sharedMesh ? mf.sharedMesh.vertexCount : 0) + " vertices, active " + p.part.gameObject.activeInHierarchy
                          + ", centre " + b.center.ToString("F2") + " size " + b.size.ToString("F2"));
            }
        }

        /// Every mod card of the VEHICLE list in turn (what the mod's bikes are): the side, the front, the other side and a close-up of the tank / shrouds.
        static IEnumerator Mods()
        {
            yield return Open(OpenShell.GarageVehicle);
            var shell = OpenShell.Instance;
            var names = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-mod-")).Select(e => e.name).ToList();
            Debug.Log(Tag + "mod cards " + names.Count + ": " + string.Join(",", names));
            foreach (var n in names)
            {
                var item = shell.Nav.Items().FirstOrDefault(e => e.name == n);
                if (item == null) continue;
                shell.Nav.Focus(item);
                for (int i = 0; i < 330; i++) yield return null;
                foreach (int yaw in new[] { 0, 90, 180 })
                {
                    Angles(yaw, 6); for (int i = 0; i < 25; i++) yield return null;
                    yield return UiShowcase.Shot("stage-" + n + "-" + yaw);
                }
                var st = Stage();
                Angles(25, 8); st.GetType().GetMethod("Zoom").Invoke(st, new object[] { 1.1f });
                for (int i = 0; i < 60; i++) yield return null;
                yield return UiShowcase.Shot("stage-" + n + "-close");
                st.GetType().GetMethod("ResetView").Invoke(st, null);
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// The rider on the GEAR tab in each gear piece REFLEX_STAGE_GEAR names (header): worn through the profile (the scratch save), the
        /// stage rebuilt, then the four sides and three close-ups.
        static IEnumerator Gear()
        {
            yield return Open(OpenShell.GarageGear);
            var shell = OpenShell.Instance;
            var filters = (Environment.GetEnvironmentVariable("REFLEX_STAGE_GEAR") ?? "o'neal mayhem,o'neal hardwear white/green")
                .Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries).Select(f => f.Trim().ToLowerInvariant()).ToArray();
            var picks = OpenGarageData.GearTypes().SelectMany(t => OpenGarageData.GearItems(t.id))
                .Where(it => filters.Any(f => (it.line + " " + it.title).ToLowerInvariant().Contains(f))).ToList();
            Debug.Log(Tag + "gear pieces " + picks.Count + ": " + string.Join(" | ", picks.Select(p => p.line + " " + p.title + "#" + p.id)));
            foreach (var it in picks)
            {
                string label = (it.line + " " + it.title).Trim();
                if (!OpenGarageData.EquipGear(it)) { Debug.Log(Tag + "FAIL cannot wear " + label); continue; }
                shell.ShowGarage(OpenShell.GarageGear);
                for (int i = 0; i < 150; i++) yield return null;
                var rider = Preview().RiderModel;
                if (rider?.root)
                    foreach (var r in rider.root.GetComponentsInChildren<Renderer>(true))
                        foreach (var m in r.sharedMaterials)
                            if (m && m.name.StartsWith("jersey:", StringComparison.Ordinal))
                                Debug.Log(Tag + "gear " + label + ": " + r.name + " -> " + m.name + " [" + string.Join(", ", Enumerable.Range(0, 6)
                                    .Where(k => m.HasProperty("_Tex" + k) && m.GetTexture("_Tex" + k)).Select(k => k + ":" + m.GetTexture("_Tex" + k).name)) + "]");
                string file = "stage-gear-" + new string(label.ToLowerInvariant().Select(c => char.IsLetterOrDigit(c) ? c : '_').ToArray());
                foreach (int yaw in new[] { 0, 90, 180, 270 })
                {
                    Angles(yaw, 6); for (int i = 0; i < 25; i++) yield return null;
                    yield return UiShowcase.Shot(file + "-" + yaw);
                }
                var st = Stage();
                foreach (int yaw in new[] { 0, 90, 270 })
                {
                    Angles(yaw, 6); st.GetType().GetMethod("Zoom").Invoke(st, new object[] { 0.9f });
                    for (int i = 0; i < 60; i++) yield return null;
                    yield return UiShowcase.Shot(file + "-close-" + yaw);
                    st.GetType().GetMethod("ResetView").Invoke(st, null);
                }
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator Pro()
        {
            yield return Open(OpenShell.GarageProRiders);
            var shell = OpenShell.Instance;
            var pros = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-pro-")).ToList();
            if (pros.Count == 0) { Debug.Log(Tag + "no pro rider in this profile"); yield break; }
            foreach (int k in new[] { 0, pros.Count / 2, pros.Count - 1 }.Distinct())
            {
                (pros[k].userData as OuNavItem)?.submit?.Invoke();
                yield return Around("pro-" + k, 150);
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
