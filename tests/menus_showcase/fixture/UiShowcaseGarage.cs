// TEST ONLY: the garage in the showcase, opened as a player does (the core profile of the scratch save, no race): every tab, the strip
// of the gear and part rows, the purchase dialog, in the career profile (credits and prices) and in the unlocked one. A scratch save
// (REFLEX_SAVE_DIR), nothing of the user's.
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
    static class UiShowcaseGarage
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("garage-rider", () => Tab("garage-rider", OpenShell.GarageRider, false, null));
            UiShowcase.Register("garage-proriders", () => Tab("garage-proriders", OpenShell.GarageProRiders, false, null));
            UiShowcase.Register("garage-vehicle", () => Tab("garage-vehicle", OpenShell.GarageVehicle, false, "garage-model-"));
            UiShowcase.Register("garage-gear", () => Tab("garage-gear", OpenShell.GarageGear, false, null, true));
            UiShowcase.Register("garage-parts", () => Tab("garage-parts", OpenShell.GarageParts, false, null, true));
            UiShowcase.Register("garage-tuning", () => Tab("garage-tuning", OpenShell.GarageTuning, false, null));
            UiShowcase.Register("garage-home", () => Tab("garage-home", OpenShell.GarageHome, false, null));
            UiShowcase.Register("garage-identity", () => Tab("garage-identity", OpenShell.GarageIdentity, false, null));
            UiShowcase.Register("garage-unlocked-vehicle", () => Tab("garage-unlocked-vehicle", OpenShell.GarageVehicle, true, null));
            UiShowcase.Register("garage-unlocked-gear", () => Tab("garage-unlocked-gear", OpenShell.GarageGear, true, null, true));
            UiShowcase.Register("garage-unlocked-parts", () => Tab("garage-unlocked-parts", OpenShell.GarageParts, true, null, true));
            UiShowcase.Register("garage-buy", Buy);
            UiShowcase.Register("garage-number", Number);
            UiShowcase.Register("garage-pro-rider", () => Pro("garage-pro-rider", OpenShell.GarageProRiders));
            UiShowcase.Register("garage-pro-gear", () => Pro("garage-pro-gear", OpenShell.GarageGear));
            UiShowcase.Register("garage-orbit", Orbit);
            UiShowcase.Register("garage-clips", Clips);
            UiShowcase.Register("garage-stand", Stand);
        }

        internal static void Credits(int n)
        {
            var change = typeof(CoreGarageProfile).GetMethod("Change", BindingFlags.Static | BindingFlags.NonPublic);
            change.Invoke(null, new object[] { (Action<ProfileData>)(p => { var t = p.T("PlayerRider"); t.columns["AccountBalance"][0] = (double)n; }) });
            ReflexProfile.Open();
        }

        internal static IEnumerator Open(int tab, bool unlocked)
        {
            UiShowcaseNav.IsolateDevices();   // (the machine's own keyboard: a T typed in another app started a test ride under the page)
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (CoreGarageProfile.UnlockEverythingEnabled != unlocked && !CoreGarageProfile.SetUnlockEverything(unlocked)) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (!unlocked) Credits(250000);
            ReflexProfile.Open();
            var tables = ReflexProfile.Current.tables;
            OpenGarageStageDebug();
            if (!unlocked)
            {   // the jersey name and number as the motocard writes them (a scratch save)
                Debug.Log("[ui-showcase] SetRiderText Number " + CoreGarageProfile.SetRiderText("Number", "21") + " " + CoreGarageProfile.Error);
                Debug.Log("[ui-showcase] SetRiderText Name " + CoreGarageProfile.SetRiderText("Name", "ferret") + " " + CoreGarageProfile.Error);
                ReflexProfile.Open();
            }
            OpenShell.Instance.ShowGarage(tab);
            for (int i = 0; i < 200; i++) yield return null;
        }

        static Type StageType => typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
        static void OpenGarageStageDebug() { StageType.GetField("Verbose").SetValue(null, true); StageType.GetField("NoAutoTurn").SetValue(null, true); }
        static object Stage() => typeof(OpenShell).GetField("garageStage", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance);
        static void Angles(float yaw, float pitch) { var st = Stage(); st.GetType().GetMethod("SetAngles").Invoke(st, new object[] { yaw, pitch }); }

        static VisualElement El(string name) => OpenShell.Instance.Layer.Q<VisualElement>(name);

        static IEnumerator Tab(string shot, int tab, bool unlocked, string focusPrefix, bool strip = false)
        {
            yield return Open(tab, unlocked);
            var shell = OpenShell.Instance;
            if (focusPrefix != null)
            {
                var item = shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith(focusPrefix));
                if (item != null) shell.Nav.Focus(item);
            }
            if (strip)
            {
                var row = shell.Nav.Items().FirstOrDefault(e => e.name != null && (e.name.StartsWith("garage-gear-") || e.name.StartsWith("garage-part-")));
                if (row != null) { shell.Nav.Focus(row); for (int i = 0; i < 40; i++) yield return null; (row.userData as OuNavItem)?.submit?.Invoke(); }
            }
            for (int i = 0; i < 120; i++) yield return null;
            yield return UiShowcase.Shot(shot);
            shell.ShowMain();
            yield return UiShowcase.Frames(5);
        }

        /// The stage from four sides: the rider alone (GEAR) and the vehicle alone (PARTS), to see which way they face.
        static IEnumerator Orbit()
        {
            yield return Open(OpenShell.GarageGear, true);
            var shell = OpenShell.Instance;
            var row = shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith("garage-gear-"));
            if (row != null) shell.Nav.Focus(row);
            for (int i = 0; i < 60; i++) yield return null;
            foreach (int yaw in new[] { 0, 90, 180, 270 }) { Angles(yaw, 8); for (int i = 0; i < 20; i++) yield return null; yield return UiShowcase.Shot("orbit-rider-" + yaw); }
            shell.ShowGarage(OpenShell.GarageParts);
            for (int i = 0; i < 100; i++) yield return null;
            foreach (int yaw in new[] { 0, 90, 180, 270 }) { Angles(yaw, 8); for (int i = 0; i < 20; i++) yield return null; yield return UiShowcase.Shot("orbit-bike-" + yaw); }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }


        static FeGarageHandle Preview() => new FeGarageHandle((Reflex.Port.FrontEnd.FeGarage)typeof(OpenShell).GetField("garagePreview", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance));
        sealed class FeGarageHandle { public readonly Reflex.Port.FrontEnd.FeGarage g; public FeGarageHandle(Reflex.Port.FrontEnd.FeGarage g) { this.g = g; } }

        /// The six RiderUIAnimations clips of the garage rider (SC_RU_HC_Anim_NoLoop_01..06) at a few times, rider beside the bike.
        static IEnumerator Clips()
        {
            yield return Open(OpenShell.GarageRider, false);
            var g = Preview().g;
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            Angles(38, 8);
            foreach (int n in new[] { 1, 2, 3, 4, 5, 6 })
                foreach (float t in new[] { 0f, 4f })
                {
                    g.SetRiderClip("SC_RU_HC_Anim_NoLoop_0" + n, t);
                    Debug.Log("[ui-showcase] clip " + g.RiderPose);
                    for (int i = 0; i < 12; i++) yield return null;
                    yield return UiShowcase.Shot("clip-" + n + "-t" + (int)t);
                }
            OpenShell.Instance.ShowMain(); yield return UiShowcase.Frames(5);
        }


        /// The rider beside the bike (RIDER tab) and alone (GEAR tab) from four sides, in the garage's own idle clip.
        static IEnumerator Stand()
        {
            yield return Open(OpenShell.GarageRider, false);
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            foreach (int yaw in new[] { 0, 90, 180, 270 }) { Angles(yaw, 8); for (int i = 0; i < 20; i++) yield return null; yield return UiShowcase.Shot("stand-beside-" + yaw); }
            var shell = OpenShell.Instance;
            shell.ShowGarage(OpenShell.GarageGear);
            for (int i = 0; i < 100; i++) yield return null;
            Angles(40, 6); for (int i = 0; i < 20; i++) yield return null; yield return UiShowcase.Shot("stand-gear");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// A pro rider selected (the rider tab shows him on his own vehicle; GEAR / PARTS / TUNING say his setup is his own).
        static IEnumerator Pro(string shot, int tab)
        {
            yield return Open(OpenShell.GarageProRiders, false);
            var shell = OpenShell.Instance;
            var pro = shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith("garage-pro-"));
            if (pro == null) { UnityEngine.Debug.Log("[ui-showcase] no pro rider in this profile"); yield break; }
            (pro.userData as OuNavItem)?.submit?.Invoke();
            for (int i = 0; i < 30; i++) yield return null;
            if (tab != OpenShell.GarageProRiders) { shell.ShowGarage(tab); }
            for (int i = 0; i < 120; i++) yield return null;
            yield return UiShowcase.Shot(shot);
            // back to the own rider for the next page
            var pv = ReflexProfile.Current.T("PlayerVehicle");
            if (pv.rows > 0) ReflexProfile.SelectRider(0, -1, pv.Int("PlayerVehicleId", 0)).Wait();
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// The NUMBER box as a player uses it: the row focused (the shot), A opens typing, digits only, 1..999 (1500 -> 999, 0 -> 1), an empty box goes
        /// back, Esc drops what was typed, left / right step (and wrap). The saved number is read from the PlayerRider row each time.
        static IEnumerator Number()
        {
            yield return Open(OpenShell.GarageIdentity, false);
            var shell = OpenShell.Instance;
            OuDevice.Injected = false;   // (the keyboard was used last: a pad's A opens the on-screen keyboard instead, see garage-pad-text)
            string Saved() => OpenGarageData.Tab("PlayerRider").Str("Number", 0);
            var row = El("garage-number"); var field = row?.Q<TextField>();
            if (row == null || field == null) { Debug.Log("[ui-showcase] number: FAIL no garage-number row"); yield break; }
            shell.Nav.Focus(row); for (int i = 0; i < 30; i++) yield return null;
            yield return UiShowcase.Shot("garage-number-row");
            void Key(KeyCode k) { using (var e = KeyDownEvent.GetPooled('\0', k, EventModifiers.None)) { var inner = field.Q("unity-text-input"); inner.SendEvent(e); } }
            bool Typing() => field.focusController?.focusedElement is VisualElement fe && (fe == field || field.Contains(fe));
            int fails = 0;
            void Expect(string what, string got, string want) { bool ok = got == want; if (!ok) fails++; Debug.Log("[ui-showcase] number: " + (ok ? "ok   " : "FAIL ") + what + " -> '" + got + "' (want '" + want + "')"); }
            IEnumerator TypeIn(string text, KeyCode end, string want, string what, bool shot = false)
            {
                (row.userData as OuNavItem).submit.Invoke(); yield return UiShowcase.Frames(8);
                Expect(what + ": typing opened", Typing().ToString(), "True");
                field.value = text; yield return UiShowcase.Frames(4);
                if (shot) yield return UiShowcase.Shot("garage-number-typing");
                Key(end); yield return UiShowcase.Frames(8);
                Expect(what + ": typing closed", Typing().ToString(), "False");
                Expect(what + ": saved", Saved(), want);
                Expect(what + ": box", field.value, want);
                Expect(what + ": focus back on the row", (shell.Nav.Focused == row).ToString(), "True");
            }
            Expect("start", Saved(), "21");
            yield return TypeIn("12a34", KeyCode.Return, "999", "12a34 (digits 1234, capped)", true);
            yield return TypeIn("457", KeyCode.Return, "457", "457");
            yield return TypeIn("0", KeyCode.Return, "1", "0 (raised to 1)");
            yield return TypeIn("", KeyCode.Return, "1", "empty (reverts)");
            yield return TypeIn("777", KeyCode.Escape, "1", "777 then Esc (dropped)");
            yield return TypeIn("abc", KeyCode.Return, "1", "letters only (reverts)");
            var item = row.userData as OuNavItem;
            item.adjust(-1); yield return UiShowcase.Frames(3); Expect("left from 1 wraps", Saved(), "999");
            item.adjust(+1); yield return UiShowcase.Frames(3); Expect("right from 999 wraps", Saved(), "1");
            item.adjust(+1); yield return UiShowcase.Frames(3); Expect("right steps", Saved(), "2");
            Expect("box follows the steps", field.value, "2");
            Debug.Log("[ui-showcase] number: " + (fails == 0 ? "ALL OK" : fails + " FAILED"));
            yield return UiShowcase.Shot("garage-number-after");
            OuDevice.Injected = null;
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator Buy()
        {
            yield return Open(OpenShell.GarageVehicle, false);
            var shell = OpenShell.Instance;
            var item = shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith("garage-model-"));
            if (item != null) { shell.Nav.Focus(item); for (int i = 0; i < 40; i++) yield return null; (item.userData as OuNavItem)?.submit?.Invoke(); }
            for (int i = 0; i < 60; i++) yield return null;
            yield return UiShowcase.Shot("garage-buy");
            shell.ShowMain();
            yield return UiShowcase.Frames(5);
        }
    }
}
