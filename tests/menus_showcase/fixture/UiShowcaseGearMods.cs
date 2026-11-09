// TEST ONLY: community gear and bike kit packs (w-gearmods, Content/KitPacks.cs) in the garage and in a race. Page (a scratch save,
// REFLEX_SAVE_DIR; the client root's Mods/Gear holding the downloads as they are - archives, an archive of archives):
//   gearmods   waits for the archives to install, then GARAGE (unlock everything, the plate FERRET / 998):
//              RIDER: the level (notes), every gear type with pack pieces (its brand / model list), every pack piece previewed on the rider in
//              its line (gm-gear-<n>-<pack>-<title>.png), equips the Jett Lawrence jersey / boots / helmet (save keys), the HR spec's packages logged;
//              VEHICLE: for each vehicle model a kit pack targets, that bike equipped, GRAPHICS KIT's list, every kit previewed from the right, the
//              front three-quarters and the front (gm-kit-<n>-<pack>-<side|34|front>.png); one kit equipped;
//              RACE: REFLEX_GEARMODS_EVENT (default National/NAT_Stillwater) with the equipped bike and gear: the local rider's roster entry
//              logged (its packages: the packs' namespaces), chase-camera shots at the gate (gm-race-*.png).
// REFLEX_GEARMODS_SKIP_GEAR=1 leaves the rider gear previews out (the Jett Lawrence pieces are still equipped). REFLEX_GEARMODS_MODELS=1,9,24 previews
// those vehicle models' kits only; REFLEX_GEARMODS_RACE_KIT=<pack>:<title>:<model> (GEICO:Red:1) the kit equipped for the race. The player is muted.
// Log lines "[gearmods-test] ..." (PASS / FAIL lines for the checks).
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseGearMods
    {
        const string Tag = "[gearmods-test] ";
        static int fails;
        static void Check(bool ok, string what) { Debug.Log(Tag + (ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("gearmods", Page); UiShowcase.Register("gearmods-import", ImportPage); UiShowcase.Register("gearmods-dbpack", DbPackPage); }

        /// gearmods-dbpack: a game folder (run.py --game) whose Database holds a gear pack put there the original way (Distractions: its
        /// StoreProductId 3650 twice): the game's tables merge it as the original does, one row per id, UNLOCK EVERYTHING still works (its
        /// "Duplicate installed store product" check), and the helmet is in its stock line (Shift Agent) like the game's own DLC gear.
        static IEnumerator DbPackPage()
        {
            UiShowcaseNav.IsolateDevices();
            ReflexProfile.Disabled = false;
            Check(CoreGarageProfile.Open(), "the garage profile opens: " + CoreGarageProfile.Error);
            bool unlocked = CoreGarageProfile.UnlockEverythingEnabled || CoreGarageProfile.SetUnlockEverything(true);
            Check(unlocked, "UNLOCK EVERYTHING with the pack's tables merged: " + CoreGarageProfile.Error);
            ReflexProfile.Open();
            var products = ReflexEvents.Tables["StoreProducts"];
            Check(products.Count(r => Rfx.Int(r, "StoreProductId") == 3650) == 1, "one StoreProducts row 3650 (the pack repeats it): parent " + Rfx.Int(products.FirstOrDefault(r => Rfx.Int(r, "StoreProductId") == 3650), "ParentStoreProductId"));
            var helmet = Reflex.Port.OpenFrontEnd.OpenGarageData.GearItems(1).FirstOrDefault(i => i.title.IndexOf("Distractions", StringComparison.OrdinalIgnoreCase) >= 0);
            Check(helmet != null && helmet.kit == null, "the pack's helmet is a stock-merged item: " + helmet?.title + " in '" + helmet?.line + "' id " + helmet?.id + " dlc " + helmet?.dlc + " owned " + helmet?.owned);
            Debug.Log(Tag + "packs in Mods/Gear: " + KitPacks.All.Count);
            StageType.GetField("NoAutoTurn").SetValue(null, true);
            var shell = OpenShell.Instance;
            if (helmet != null)
            {
                shell.ShowGarage(OpenShell.GarageRider); yield return UiShowcase.Frames(80);
                yield return Submit("garage-gear-1", 120);
                if (helmet.parent >= 0) yield return Submit("garage-line-" + helmet.parent, 120);
                yield return Focus("garage-item-" + helmet.id, 260);
                yield return UiShowcase.Shot("gm-dbpack-helmet");
            }
            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
            yield return UiShowcase.Frames(5);
        }

        /// gearmods-import: the game files import (ReflexAssetImport) of a scratch install REFLEX_GEARMODS_IMPORT (a Database folder made by the
        /// test: a few stock databases, a gear pack put in the original way and an MXTables_DLC008 this version cannot read) into a scratch
        /// client folder: the import must finish, skip the DLC008 table with a warning naming it, and extract the pack.
        static IEnumerator ImportPage()
        {
            string src = Environment.GetEnvironmentVariable("REFLEX_GEARMODS_IMPORT");
            if (string.IsNullOrEmpty(src)) { Check(false, "REFLEX_GEARMODS_IMPORT not set"); yield break; }
            string dest = System.IO.Path.Combine(System.IO.Path.GetDirectoryName(src.TrimEnd('/')), "imported-" + DateTime.UtcNow.Ticks);
            var task = ReflexAssetImport.Import(src, dest, null, default);
            while (!task.IsCompleted) yield return null;
            if (task.IsFaulted) { Check(false, "import failed: " + task.Exception?.Flatten().InnerExceptions.FirstOrDefault()?.Message); yield break; }
            var r = task.Result;
            foreach (var w in r.warnings ?? new string[0]) Debug.Log(Tag + "import warning: " + w);
            Check((r.warnings ?? new string[0]).Any(w => w.StartsWith("Skipped MXTables_DLC008.dx9.database", StringComparison.Ordinal)), "the unreadable MXTables_DLC008 was skipped with a warning naming it");
            bool db8 = System.IO.File.Exists(System.IO.Path.Combine(dest, "Database", "MXTables_DLC008.dx9.database"));
            var packs = KitPacks.ScanInstall(System.IO.Path.Combine(dest, "Database"), true, new List<KitPacks.Problem>());
            Check(!db8 && packs.Count == 1 && packs[0].items.Count > 0, "the import finished (" + r.files + " files): DLC008 left out " + !db8 + "; the pack in Database read from the import: " + string.Join(", ", packs.Select(KitPacks.Describe)));
            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
            yield return UiShowcase.Frames(5);
        }

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
        static IEnumerator Focus(string name, int frames)
        {
            var e = Item(name);
            if (e == null) { Check(false, "no menu entry " + name); yield break; }
            OpenShell.Instance.Nav.Focus(e); yield return UiShowcase.Frames(frames);
        }
        static string Safe(string s) => new string((s ?? "").Select(c => char.IsLetterOrDigit(c) ? char.ToLowerInvariant(c) : '_').ToArray()).Trim('_');

        sealed class Mute : MonoBehaviour { void LateUpdate() { if (AudioListener.volume != 0f) AudioListener.volume = 0f; } }   // test players are muted

        static IEnumerator Page()
        {
            UiShowcaseNav.IsolateDevices();
            AudioListener.volume = 0f; new GameObject("gearmods-mute").AddComponent<Mute>().hideFlags = HideFlags.DontSave;
            // ---- the archives in Mods/Gear install on workers (an archive of archives: each inner one on a later scan)
            float t0 = Time.realtimeSinceStartup; int lastCount = -1; float stableSince = Time.realtimeSinceStartup;
            while (Time.realtimeSinceStartup - t0 < 240f)
            {
                KitPacks.Refresh();
                int n = KitPacks.All.Count, busy = KitPacks.Installing.Count;
                if (n != lastCount) { lastCount = n; stableSince = Time.realtimeSinceStartup; Debug.Log(Tag + "packs " + n + ", installing " + busy + " (" + string.Join(", ", KitPacks.Installing) + ")"); }
                if (busy == 0 && !TrackArchives.Busy && Time.realtimeSinceStartup - stableSince > 6f) break;
                float w = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - w < 1.5f) yield return null;
            }
            foreach (var p in KitPacks.All) Debug.Log(Tag + "pack " + KitPacks.Describe(p));
            foreach (var p in KitPacks.Problems) Debug.Log(Tag + "problem " + p);
            int want = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_GEARMODS_PACKS") ?? "", out int wp) ? wp : 0;
            Check(want <= 0 || KitPacks.All.Count == want, "packs installed and read: " + KitPacks.All.Count + (want > 0 ? " (expected " + want + ")" : ""));
            Check(KitPacks.Problems.Count == 0, "no pack problems (" + KitPacks.Problems.Count + ")");

            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (!CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            CoreGarageProfile.SetRiderText("Name", "FERRET"); CoreGarageProfile.SetRiderText("Number", "998");
            ReflexProfile.Open();
            StageType.GetField("NoAutoTurn").SetValue(null, true);
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            var shell = OpenShell.Instance;
            int own = OpenGarageData.FirstOwnVehicle();
            if (OpenGarageData.Pro || OpenGarageData.Equipped < 0) OpenGarageData.SelectVehicle(own);

            // ---- RIDER
            shell.ShowGarage(OpenShell.GarageRider); yield return UiShowcase.Frames(200);
            yield return UiShowcase.Shot("gm-rider-level");
            var kits = KitPacks.Current.byId.Values.ToList();
            var gearTypes = kits.Where(k => !k.bike).Select(k => k.type).Distinct().OrderBy(x => x).ToList();
            if (Environment.GetEnvironmentVariable("REFLEX_GEARMODS_SKIP_GEAR") == "1") gearTypes.Clear();   // (a run of the bike kits only)
            int n2 = 0;
            foreach (int type in gearTypes)
            {
                shell.ShowGarage(OpenShell.GarageRider); yield return UiShowcase.Frames(60);
                yield return Submit("garage-gear-" + type, 160);
                var groups = OpenGarageData.Groups(OpenGarageData.GearItems(type));
                var packLines = groups.Where(g => g.items.Any(i => i.kit != null)).ToList();
                Debug.Log(Tag + "gear type " + type + ": " + groups.Count + " entries; with pack pieces: " + string.Join(" | ", packLines.Select(g => g.title + " (" + g.items.Count(i => i.kit != null) + " of " + g.items.Count + ")")));
                if (packLines.Count > 0) yield return Focus("garage-line-" + packLines[0].line, 80);
                yield return UiShowcase.Shot("gm-gear-type-" + type + "-list");
                foreach (var it in OpenGarageData.GearItems(type).Where(i => i.kit != null))
                {
                    shell.ShowGarage(OpenShell.GarageRider); yield return UiShowcase.Frames(40);
                    yield return Submit("garage-gear-" + type, 120);
                    if (it.parent >= 0) yield return Submit("garage-line-" + it.parent, 120);
                    yield return Focus("garage-item-" + it.id, 260);
                    var ids = OpenGarageData.EquippedGearIds();
                    var spec = OpenGarageData.RiderWithGear(ids, it.id);
                    var part = spec?.parts.FirstOrDefault(p => p.matsDb != null && p.matsDb.StartsWith(it.kit.pack.Namespace));
                    var hr = Reflex.Port.FrontEnd.FeGarage.HighResRider(spec);
                    var hrPart = hr?.parts.FirstOrDefault(p => p.matsDb != null && p.matsDb.StartsWith(it.kit.pack.Namespace));
                    Check(part != null, $"gear {it.kit.pack.name} '{it.title}' (type {type}, line '{it.line}'): the preview's spec wears {part?.matsDb}/{part?.matsPkg} on {part?.modelDb}/{part?.modelPkg}; HR {hrPart?.matsDb}/{hrPart?.matsPkg} on {hrPart?.modelDb}/{hrPart?.modelPkg}");
                    yield return UiShowcase.Shot("gm-gear-" + n2++ + "-" + Safe(it.kit.pack.name) + "-" + Safe(it.title));
                }
            }
            // equip the Jett Lawrence pack's jersey, boots and helmet (owned at once: no purchase), check the save and the EQUIPPED marks
            var jett = kits.Where(k => !k.bike && k.pack.name.IndexOf("JettLawrence", StringComparison.OrdinalIgnoreCase) >= 0).ToList();
            foreach (var k in jett)
            {
                shell.ShowGarage(OpenShell.GarageRider); yield return UiShowcase.Frames(40);
                yield return Submit("garage-gear-" + k.type, 120);
                if (k.parent >= 0) yield return Submit("garage-line-" + k.parent, 120);
                yield return Submit("garage-item-" + k.id, 200);
                Check(OpenGarageData.EquippedGear(k.type) == k.id && Options.ReflexSave.GetString(KitPacks.GearKey + k.type, "") == k.Token, $"equipped {k.title}: save {KitPacks.GearKey}{k.type} = {Options.ReflexSave.GetString(KitPacks.GearKey + k.type, "")}");
            }
            shell.ShowGarage(OpenShell.GarageRider); yield return UiShowcase.Frames(200);
            yield return UiShowcase.Shot("gm-rider-equipped");

            // ---- VEHICLE: each model a kit pack fits
            var models = kits.Where(k => k.bike).SelectMany(k => k.models).Distinct().OrderBy(x => x).ToList();
            var only = (Environment.GetEnvironmentVariable("REFLEX_GEARMODS_MODELS") ?? "").Split(',').Select(x => int.TryParse(x.Trim(), out int m) ? m : -1).Where(m => m >= 0).ToList();
            if (only.Count > 0) models = models.Where(only.Contains).ToList();   // REFLEX_GEARMODS_MODELS=1,9,24: those vehicle models only
            int n3 = 0; KitItem raceKit = null; int raceModel = -1;
            foreach (int model in models)
            {
                int pv = OpenGarageData.PlayerVehicleOfModel(model);
                if (pv < 0) { Check(false, "no player vehicle of model " + model + " after UNLOCK EVERYTHING"); continue; }
                OpenGarageData.SelectVehicle(pv); ReflexProfile.Open();
                Debug.Log(Tag + "vehicle model " + model + ": " + OpenGarageData.Vehicles().FirstOrDefault(v => v.id == model)?.title);
                shell.ShowGarage(OpenShell.GarageParts); yield return UiShowcase.Frames(160);
                yield return Submit("garage-part-11", 160);
                yield return UiShowcase.Shot("gm-kit-model-" + model + "-list");
                foreach (var it in OpenGarageData.PartItems(model, pv, 11).Where(i => i.kit != null))
                {
                    shell.ShowGarage(OpenShell.GarageParts); yield return UiShowcase.Frames(40);
                    yield return Submit("garage-part-11", 120);
                    if (it.parent >= 0) yield return Submit("garage-line-" + it.parent, 120);
                    yield return Focus("garage-item-" + it.id, 300);
                    var spec = OpenGarageData.BikeWithPart(model, OpenGarageData.EquippedPartIds(pv), it.id);
                    var hr = ReflexBikeModel.HighRes(spec);
                    Check(spec != null && spec.kitDb != null && spec.kitDb.StartsWith(it.kit.pack.Namespace), $"kit {it.kit.pack.name} '{it.title}' on model {model}: chassis {spec?.chassisDb}/{spec?.chassisPkg}, kit {spec?.kitDb}/{spec?.kitPkg}; HR kit {hr?.kitDb}/{hr?.kitPkg}");
                    string label = "gm-kit-" + n3++ + "-" + model + "-" + Safe(it.kit.pack.name);
                    Frame("Vehicle"); Angles(180, 8); Zoom(0.45f); yield return UiShowcase.Frames(40); yield return UiShowcase.Shot(label + "-side");   // (the side the rider does not stand on)
                    Frame("Vehicle"); Angles(40, 10); Zoom(0.6f); yield return UiShowcase.Frames(40); yield return UiShowcase.Shot(label + "-34");
                    Frame("Front"); Angles(70, 6); yield return UiShowcase.Frames(40); yield return UiShowcase.Shot(label + "-front");
                    if (raceKit == null && it.kit.pack.name.IndexOf("Roczen", StringComparison.OrdinalIgnoreCase) >= 0) { raceKit = it.kit; raceModel = model; }
                }
            }
            string raceKitName = Environment.GetEnvironmentVariable("REFLEX_GEARMODS_RACE_KIT");   // "<pack>:<title>:<model>", e.g. GEICO:Red:1
            if (!string.IsNullOrEmpty(raceKitName))
            {
                var q = raceKitName.Split(':');
                raceKit = kits.FirstOrDefault(k => k.bike && k.pack.name == q[0] && (q.Length < 2 || k.title == q[1]) && (q.Length < 3 || k.models.Contains(int.Parse(q[2]))));
                raceModel = raceKit != null && q.Length >= 3 ? int.Parse(q[2]) : -1;
            }
            raceKit ??= kits.FirstOrDefault(k => k.bike); if (raceKit != null && raceModel < 0) raceModel = raceKit.models[0];
            if (raceKit != null)
            {
                int pv = OpenGarageData.PlayerVehicleOfModel(raceModel);
                OpenGarageData.SelectVehicle(pv); ReflexProfile.Open();
                shell.ShowGarage(OpenShell.GarageParts); yield return UiShowcase.Frames(80);
                yield return Submit("garage-part-11", 120);
                if (raceKit.parent >= 0) yield return Submit("garage-line-" + raceKit.parent, 120);
                yield return Submit("garage-item-" + raceKit.id, 200);
                Check(OpenGarageData.EquippedPart(pv, 11) == raceKit.id, $"equipped kit {raceKit.title} ({raceKit.pack.name}) on model {raceModel}: save {Options.ReflexSave.GetString(KitPacks.PartKey + raceModel + ".11", "")}");
                shell.ShowGarage(OpenShell.GarageHome); yield return UiShowcase.Frames(200);
                Frame("Overview"); Angles(30, 8); yield return UiShowcase.Frames(60);
                yield return UiShowcase.Shot("gm-home-equipped");
            }

            // ---- RACE with the equipped pack bike and gear
            string evName = Environment.GetEnvironmentVariable("REFLEX_GEARMODS_EVENT") ?? "National/NAT_Stillwater";
            if (evName != "none")
            {
                shell.ShowMain(); yield return UiShowcase.Frames(30);
                var ev = ReflexEvents.Find(evName) ?? ReflexEvents.All.FirstOrDefault(e => e.spec.type == "National");
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
                    Debug.Log(Tag + "race local: " + local + " kits [" + string.Join(", ", local?.kits ?? new string[0]) + "]");
                    Check(local != null && local.kits.Length > 0 && local.rider != null && (!kits.Any(k => !k.bike) || local.rider.parts.Any(p => p.matsDb != null && p.matsDb.StartsWith(BikeMods.NamespaceMark + KitPacks.IdMark)))
                        && (raceKit == null || local.bike != null && local.bike.kitDb != null && local.bike.kitDb.StartsWith(raceKit.pack.Namespace)), "the race's local rider wears the pack pieces (rider and bike kit)");
                    float w = Time.realtimeSinceStartup;
                    if (rt.IntroCardShown) { while (Time.realtimeSinceStartup - w < 1.5f) yield return null; yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); }   // SKIP INTRO
                    for (float t = 0; t < 60f && !(rt.Race.state == 2 || rt.Race.valid == 0); t += Time.unscaledDeltaTime) yield return null;   // at the gate
                    w = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - w < 2f) yield return null;
                    yield return UiShowcase.Shot("gm-race-gate");
                    // ride a few seconds after the gate drops (throttle held on the virtual pad): the chase camera behind the rider
                    for (float t = 0; t < 30f && rt.Race.state == 2; t += Time.unscaledDeltaTime) yield return null;
                    UiShowcaseNav.PadState(new UnityEngine.InputSystem.LowLevel.GamepadState { rightTrigger = 0.8f });
                    w = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - w < 4f) yield return null;
                    yield return UiShowcase.Shot("gm-race-ride");
                    UiShowcaseNav.PadState(new UnityEngine.InputSystem.LowLevel.GamepadState());
                }
                else Check(false, "the race did not run");
            }
            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
            yield return UiShowcase.Frames(10);
        }
    }
}
