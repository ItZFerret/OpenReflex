// TEST ONLY: the garage with the pad (w-garagepad). The VEHICLE tab's cards walked top to bottom with the d-pad (the focus reaches the last
// card and every focused card is scrolled into view), and the rider tab's text boxes used with the pad (A types into the box, the on-screen
// keyboard writes the name and the number, B closes it, nothing the pad does ends the typing by itself).
using System;
using System.Collections;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static partial class UiShowcaseNav
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void RegisterGaragePad()
        {
            UiShowcase.Register("garage-pad-vehicle", GaragePadVehicle);
            UiShowcase.Register("garage-pad-text", GaragePadText);
        }

        /// The VEHICLE tab, career profile (most bikes for sale) and the unlocked one: down through every card, then back up.
        static IEnumerator GaragePadVehicle()
        {
            var shell = OpenShell.Instance; Devices();
            var walk = UiShowcase.AfterShot; UiShowcase.AfterShot = null;   // (this page is its own walk: the generic one after a screenshot would move the focus under it)
            foreach (bool unlocked in new[] { true, false })
            {
                yield return UiShowcaseGarage.Open(OpenShell.GarageVehicle, unlocked);
                string tag = "garage vehicle " + (unlocked ? "unlocked" : "career");
                yield return LongList(tag, 240, "garage-pad-vehicle-" + (unlocked ? "unlocked" : "career"));
                var sv = shell.Layer.Q<ScrollView>("garage-rows");
                var cards = sv.contentContainer.Query<VisualElement>(className: "ou-gcard").ToList();
                Debug.Log(Tag + "garagepad [" + tag + "] " + cards.Count + " cards, scroll " + sv.scrollOffset.y.ToString("F0") + "/" + ScrollMax(sv).ToString("F0"));
                shell.ShowMain(); yield return UiShowcase.Frames(5);
            }
            UiShowcase.AfterShot = walk;
        }

        static bool KeyboardOpen(OpenShell shell) { var p = typeof(OpenShell).GetProperty("KeyboardOpen"); return p != null && (bool)p.GetValue(shell); }

        /// The rider tab's text boxes with the pad: A on NAME starts editing and nothing the pad does ends it by itself (a second A used to take the
        /// focus out of the box, like B), the on-screen keyboard types the name and the number with the pad alone, DONE keeps, B drops and does not
        /// leave the garage.
        static IEnumerator GaragePadText()
        {
            var shell = OpenShell.Instance; Devices();
            var walk = UiShowcase.AfterShot; UiShowcase.AfterShot = null;   // (see GaragePadVehicle)
            yield return UiShowcaseGarage.Open(OpenShell.GarageIdentity, false);   // (GARAGE > RIDER > NAME & NUMBER)
            OuDevice.Injected = true;   // (the pad was used last)
            var row = shell.Layer.Q<VisualElement>("garage-name"); var field = row?.Q<TextField>();
            Check(row != null && field != null, "garage text", "no NAME row");
            if (row == null || field == null) yield break;
            bool Native() => field.focusController?.focusedElement is VisualElement fe && (fe == field || field.Contains(fe));
            bool Editing() => KeyboardOpen(shell) || Native();
            string Typed() { var l = shell.Layer.Q<Label>(className: "ou-osk-text"); return l == null ? null : l.text.Substring(0, Math.Max(0, l.text.Length - 1)); }   // (the blinking caret is the last character)
            string Name() => ReflexProfile.HasPersona ? ReflexProfile.Persona : "";
            string Focus() => Look().name;
            shell.Nav.Focus(row); yield return UiShowcase.Frames(20);
            string was = Name();
            Debug.Log(Tag + "garagepad text: name now '" + was + "'");

            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            Check(Editing(), "garage text", "A on NAME did not start editing (focus " + Focus() + ")");
            yield return UiShowcase.Frames(40);
            Check(Editing(), "garage text", "the editing ended by itself");
            if (!KeyboardOpen(shell))
            {   // the box itself has the focus: a second A must not take it out (the report), and typed text reaches the name
                yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
                Check(Editing(), "garage text", "a second A on the open box ended the editing (focus " + Focus() + ")");
                Devices(); InputSystem.QueueTextEvent(kb, 'Z'); yield return UiShowcase.Frames(5);
                Debug.Log(Tag + "garagepad text: native box after a typed Z: '" + field.text + "'");
                Check(field.text.Contains("Z"), "garage text", "typing into the box did not change it ('" + field.text + "')");
                OuDevice.Injected = null; UiShowcase.AfterShot = walk;
                yield break;
            }

            // ---- the on-screen keyboard
            yield return UiShowcase.Shot("garage-pad-keyboard-open");
            Check(Focus() == "osk-q", "garage text", "the keyboard opened on " + Focus() + " (want osk-q)");
            Check(string.Equals(Typed(), was, StringComparison.OrdinalIgnoreCase), "garage text", "the keyboard did not start from the name ('" + Typed() + "' vs '" + was + "')");
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(6);
            Check(Typed() == "Q", "garage text", "A on Q gave '" + Typed() + "' (the first key replaces the selected name)");
            yield return PadTap(GamepadButton.DpadRight); yield return UiShowcase.Frames(4);
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(6);
            yield return PadTap(GamepadButton.North); yield return UiShowcase.Frames(6);    // Y: a space
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(6);    // W again
            Check(Typed() == "QW W", "garage text", "Q W Y W gave '" + Typed() + "'");
            yield return PadTap(GamepadButton.West); yield return UiShowcase.Frames(6);     // X: delete
            Check(Typed() == "QW ", "garage text", "X gave '" + Typed() + "'");
            yield return PadTap(GamepadButton.West); yield return UiShowcase.Frames(6);
            Check(Typed() == "QW", "garage text", "X again gave '" + Typed() + "'");
            yield return UiShowcase.Shot("garage-pad-keyboard-typed");
            // the left stick walks the keys too, the bumpers and the triggers leave the keyboard alone
            yield return PadTap(GamepadButton.RightShoulder); yield return PadTrigger(true); yield return UiShowcase.Frames(6);
            Check(KeyboardOpen(shell) && shell.GarageCurrentTab == OpenShell.GarageRider, "garage text", "a bumper / trigger changed the page under the keyboard");
            for (int i = 0; i < 3; i++) { yield return PadTap(GamepadButton.DpadDown); yield return UiShowcase.Frames(3); }
            yield return PadTap(GamepadButton.DpadRight); yield return PadTap(GamepadButton.DpadRight); yield return UiShowcase.Frames(3);
            Check(Focus() == "osk-done", "garage text", "the pad did not reach DONE (on " + Focus() + ")");
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            Check(!KeyboardOpen(shell), "garage text", "DONE did not close the keyboard");
            // (the persona name is a file of the user's cache, which the test's sandbox denies: the saved name is read from the JERSEY NAME row below)
            Check(shell.Nav.Focused == row, "garage text", "the focus did not come back to the NAME row (" + Focus() + ")");
            Check(shell.Current == OpenShell.Screen.Garage, "garage text", "the garage was left");

            // B drops what was typed and keeps the garage
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            yield return PadTap(GamepadButton.South); yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(6);
            Check(Typed() == "QQ", "garage text", "second opening typed '" + Typed() + "'");
            yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(20);
            Check(!KeyboardOpen(shell) && shell.Current == OpenShell.Screen.Garage, "garage text", "B did not just close the keyboard (" + shell.Current + ")");
            Check(shell.Nav.Focused == row, "garage text", "B: the focus did not come back to the row (" + Focus() + ")");

            // the jersey name is saved in the profile: DONE keeps, B drops; it takes no space or hyphen
            var jersey = shell.Layer.Q<VisualElement>("garage-jersey-name");
            string Jersey() => OpenGarageData.Tab("PlayerRider").Str("Name", 0);
            Check(Jersey().ToLowerInvariant() == "ferret", "garage text", "the jersey name started as '" + Jersey() + "'");
            shell.Nav.Focus(jersey); yield return UiShowcase.Frames(10);
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            Check(KeyboardOpen(shell) && shell.Layer.Q<VisualElement>("osk--") == null && shell.Layer.Q<VisualElement>("osk-space") == null, "garage text", "the jersey name's keyboard has a space / hyphen key");
            yield return PadTap(GamepadButton.South); yield return PadTap(GamepadButton.DpadRight); yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(6);
            Check(Typed() == "QW", "garage text", "jersey name typed '" + Typed() + "'");
            yield return PadTap(GamepadButton.North); yield return UiShowcase.Frames(6);   // Y: no space on this one
            Check(Typed() == "QW", "garage text", "Y gave a space in the jersey name ('" + Typed() + "')");
            yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(20);
            Check(Jersey().ToLowerInvariant() == "ferret", "garage text", "B kept the jersey name '" + Jersey() + "'");
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            yield return PadTap(GamepadButton.South); yield return PadTap(GamepadButton.DpadRight); yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(6);
            for (int i = 0; i < 3; i++) { yield return PadTap(GamepadButton.DpadDown); yield return UiShowcase.Frames(3); }
            yield return PadTap(GamepadButton.DpadRight); yield return PadTap(GamepadButton.DpadRight); yield return UiShowcase.Frames(3);
            Check(Focus() == "osk-done", "garage text", "the pad did not reach the jersey keyboard's DONE (on " + Focus() + ")");
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(40);
            Check(!KeyboardOpen(shell) && Jersey() == "QW", "garage text", "DONE: the jersey name is '" + Jersey() + "'");
            Check(jersey.Q<TextField>().value == "QW", "garage text", "the jersey name's box shows '" + jersey.Q<TextField>().value + "'");

            // ---- the number: a pad of digits, 1..999, 3 digits at most
            var num = shell.Layer.Q<VisualElement>("garage-number");
            shell.Nav.Focus(num); yield return UiShowcase.Frames(10);
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            Check(KeyboardOpen(shell) && Focus() == "osk-1", "garage text", "A on NUMBER: keyboard " + KeyboardOpen(shell) + " on " + Focus());
            Check(shell.Layer.Q<VisualElement>("osk-q") == null, "garage text", "the number keyboard has letters");
            yield return UiShowcase.Shot("garage-pad-keyboard-number");
            string Saved() => OpenGarageData.Tab("PlayerRider").Str("Number", 0);
            Check(Typed() == "21", "garage text", "the number keyboard started from '" + Typed() + "'");
            yield return PadTap(GamepadButton.South); yield return PadTap(GamepadButton.DpadRight); yield return PadTap(GamepadButton.South);   // 1 2
            yield return PadTap(GamepadButton.DpadRight); yield return PadTap(GamepadButton.South); yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(6);   // 3 3 (the fourth is refused)
            Check(Typed() == "123", "garage text", "1 2 3 3 gave '" + Typed() + "' (3 digits at most)");
            for (int i = 0; i < 3; i++) { yield return PadTap(GamepadButton.DpadDown); yield return UiShowcase.Frames(3); }
            Check(Focus() == "osk-done", "garage text", "the pad did not reach the number keyboard's DONE (on " + Focus() + ")");
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            Check(!KeyboardOpen(shell) && Saved() == "123", "garage text", "DONE: number is '" + Saved() + "'");
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            for (int i = 0; i < 3; i++) { yield return PadTap(GamepadButton.DpadDown); yield return UiShowcase.Frames(3); }   // 1 > 4 > 7 > DELETE
            yield return PadTap(GamepadButton.DpadRight); yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(6);
            Check(Typed() == "0", "garage text", "0 gave '" + Typed() + "'");
            yield return PadTap(GamepadButton.DpadRight); yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(20);
            Check(Saved() == "1", "garage text", "0 should be raised to 1, saved '" + Saved() + "'");
            OuDevice.Injected = null; UiShowcase.AfterShot = walk;
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }
    }
}
