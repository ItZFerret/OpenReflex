// TEST ONLY: the controller / keyboard walk of the showcase. With REFLEX_UI_WALK=1 every page the showcase opens is walked after its
// screenshot: SYNTHETIC devices (a virtual Gamepad and Keyboard of the Input System, so the real bindings, the stick's hysteresis and
// the repeat are in the loop) press d-pad / stick / A / B / LB / RB / triggers / Y / X, and after every step a focused element must
// exist on the screen that is up (no lost focus, no trap). It also checks that every item of the screen can be reached by the
// directions (the graph OuNav.Neighbour makes), that a held stick repeats at a sane rate, and that B / Esc leaves a screen with a
// focus still on something. Journeys (nav-*) go through screens with real input: the hub into the browser, its setup page and back;
// the garage's tabs, its strip and its purchase dialog; the options' tabs.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static partial class UiShowcaseNav
    {
        const string Tag = "[ui-showcase] ";
        static Gamepad pad; static Keyboard kb; static Mouse mouse;
        public static int Failures;
        public static bool Walking => Environment.GetEnvironmentVariable("REFLEX_UI_WALK") == "1";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            if (Walking) UiShowcase.AfterShot = Walk;
            UiShowcase.Register("nav-hub-browser", HubBrowserSetup);
            UiShowcase.Register("nav-shell", ShellScreens);
            UiShowcase.Register("nav-garage", GarageJourney);
            UiShowcase.Register("nav-options", OptionsJourney);
            UiShowcase.Register("nav-mouse", MouseJourney);
            UiShowcase.Register("nav-longlist", LongLists);
            UiShowcase.Register("nav-hoverjitter", HoverJitter);
        }

        // ------------------------------------------------------------------------------------------------ devices

        static void Devices()
        {
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;   // (the runner hands the focus back to the user: the player is not the app in front, its virtual pad must still work)
            if (pad == null || !pad.added) pad = InputSystem.AddDevice<Gamepad>("showcase pad");
            if (kb == null || !kb.added) kb = InputSystem.AddDevice<Keyboard>("showcase keyboard");
            if (mouse == null || !mouse.added) mouse = InputSystem.AddDevice<Mouse>("showcase mouse");
            // the machine's own pads, keyboards and mice are not part of the test (a controller's drifting stick on the desk moved the garage's CLASS filter
            // during a run; a key typed in another app reached the player, which ignores the focus)
            foreach (var d in InputSystem.devices.ToArray())
                if ((d is Gamepad && d != pad || d is Keyboard && d != kb || d is Mouse && d != mouse || d is Pointer && !(d is Mouse)) && d.enabled) InputSystem.DisableDevice(d);
        }
        /// The machine's own pads, keyboards and mice off for a test page (a key typed in another app must not reach the player).
        internal static void IsolateDevices() => Devices();
        /// The virtual pad held in a state (sticks, triggers, buttons) until the next one (the load arena's ride).
        internal static void PadState(GamepadState state) { Devices(); InputSystem.QueueStateEvent(pad, state); }
        internal static IEnumerator PadTap(GamepadButton b, int frames = 3)
        {
            Devices();
            InputSystem.QueueStateEvent(pad, new GamepadState(b));
            for (int i = 0; i < frames; i++) { Jit(); yield return null; }
            InputSystem.QueueStateEvent(pad, new GamepadState());
            for (int i = 0; i < 3; i++) { Jit(); yield return null; }
        }

        // a pointer resting over the list that is not still: a mouse / touchpad / remote desktop that reports a pixel or two of noise every frame
        static int jitterEnters, jitterEntersMoving; static bool jitterOn; static Vector2 jitterAt, jitterLast; static int jitterPhase;
        static void Jit()
        {
            if (!jitterOn) return;
            jitterPhase++;
            var at = jitterAt + new Vector2((jitterPhase % 2) * 3 - 1.5f, ((jitterPhase / 2) % 2) * 3 - 1.5f);
            InputSystem.QueueStateEvent(mouse, new MouseState { position = at, delta = at - jitterLast });
            jitterLast = at;
        }
        static IEnumerator JFrames(int n) { for (int i = 0; i < n; i++) { Jit(); yield return null; } }

        static IEnumerator PadTrigger(bool right, int frames = 3)
        {
            Devices();
            var s = new GamepadState { leftTrigger = right ? 0 : 1, rightTrigger = right ? 1 : 0 };
            InputSystem.QueueStateEvent(pad, s);
            for (int i = 0; i < frames; i++) yield return null;
            InputSystem.QueueStateEvent(pad, new GamepadState());
            for (int i = 0; i < 3; i++) yield return null;
        }

        internal static IEnumerator Stick(Vector2 v, float seconds)
        {
            Devices();
            InputSystem.QueueStateEvent(pad, new GamepadState { leftStick = v });
            for (float t = 0; t < seconds; t += Time.unscaledDeltaTime) yield return null;
            InputSystem.QueueStateEvent(pad, new GamepadState());
            for (int i = 0; i < 4; i++) yield return null;
        }

        internal static IEnumerator Key(Key key, int frames = 3)
        {
            Devices();
            InputSystem.QueueStateEvent(kb, new KeyboardState(key));
            for (int i = 0; i < frames; i++) yield return null;
            InputSystem.QueueStateEvent(kb, new KeyboardState());
            for (int i = 0; i < 3; i++) yield return null;
        }

        static IEnumerator Dpad(GamepadButton b) { yield return PadTap(b); }

        // ------------------------------------------------------------------------------------------------ the probe

        public struct Probe { public bool anyItems; public VisualElement focused; public UIDocument doc; public string name; public int items; }

        /// The topmost menu document that shows items, and what has the focus in it.
        public static Probe Look()
        {
            var best = new Probe { name = "-" };
            float order = float.MinValue;
            foreach (var d in UnityEngine.Object.FindObjectsByType<UIDocument>(FindObjectsSortMode.None))
            {
                var root = d.rootVisualElement;
                if (root == null || root.panel == null || !Shown(root)) continue;
                int n = 0; foreach (var e in root.Query<VisualElement>(className: "mxa-item").ToList()) if (e.userData is OuNavItem && Shown(e) && e.enabledInHierarchy) n++;
                if (n == 0 || d.sortingOrder < order) continue;
                order = d.sortingOrder;
                var f = root.panel.focusController?.focusedElement as VisualElement;
                bool ok = f != null && f.userData is OuNavItem && Shown(f) && root.Contains(f);
                best = new Probe { anyItems = true, focused = ok ? f : null, doc = d, items = n, name = ok ? Label(f) : "(none)" };
            }
            return best;
        }

        static bool Shown(VisualElement e)
        {
            for (var p = e; p != null; p = p.parent) if (p.resolvedStyle.display == DisplayStyle.None || p.resolvedStyle.visibility == Visibility.Hidden || p.resolvedStyle.opacity <= 0.001f && p.parent == null) return false;
            return true;
        }

        static string Label(VisualElement f)
        {
            if (!string.IsNullOrEmpty(f.name)) return f.name;
            var l = f.Q<Label>(); return l != null ? l.text : f.GetType().Name;
        }

        public static void Check(bool ok, string screen, string what)
        {
            if (ok) return;
            Failures++;
            Debug.LogError(Tag + "NAV FAIL [" + screen + "] " + what);
        }

        internal static IEnumerator Settle(string screen, string step, int frames = 6)
        {
            for (int i = 0; i < frames; i++) yield return null;
            var p = Look();
            Debug.Log(Tag + "nav [" + screen + "] " + step + " -> " + p.name + " (" + p.items + " items)");
            if (p.anyItems) Check(p.focused != null, screen, "no focused item after " + step);
        }

        // ------------------------------------------------------------------------------------------------ the walk of a page

        /// After a page's screenshot: the walk. Called by UiShowcase.Shot when REFLEX_UI_WALK=1.
        public static IEnumerator Walk(string screen) => Walk(screen, true);
        public static IEnumerator Walk(string screen, bool bumpers)
        {
            Devices();
            for (int i = 0; i < 20 && !Look().anyItems; i++) yield return null;
            var first = Look();
            if (!first.anyItems) { Debug.Log(Tag + "nav [" + screen + "] no items (a display page)"); yield break; }
            // 1. the initial focus: a screen opens with something focused (given a few frames: the layout, the page's own focus call)
            for (int i = 0; i < 30 && Look().focused == null; i++) yield return null;
            var start = Look();
            Check(start.focused != null, screen, "no initial focus (" + start.items + " items)");
            yield return Reach(screen);
            // 2. the directions and the bumpers, the buttons that cannot leave the screen (no A / B here: a journey does those)
            foreach (var (b, n) in new[] { (GamepadButton.DpadDown, "down"), (GamepadButton.DpadDown, "down"), (GamepadButton.DpadDown, "down"), (GamepadButton.DpadRight, "right"), (GamepadButton.DpadLeft, "left"),
                                          (GamepadButton.DpadUp, "up"), (GamepadButton.DpadUp, "up"), (GamepadButton.DpadUp, "up"), (GamepadButton.DpadUp, "up"), (GamepadButton.RightShoulder, "RB"), (GamepadButton.LeftShoulder, "LB"), (GamepadButton.LeftShoulder, "LB") })
            {
                if (!bumpers && (b == GamepadButton.RightShoulder || b == GamepadButton.LeftShoulder)) continue;
                yield return PadTap(b); yield return Settle(screen, n);
            }
            yield return PadTrigger(true); yield return Settle(screen, "RT");
            yield return PadTrigger(false); yield return Settle(screen, "LT");
            // 3. a held stick: one step, then repeats at a sane rate (0.4 s first delay, 0.13 s between)
            var before = Look().name; int changes = 0; string last = before;
            Devices();
            InputSystem.QueueStateEvent(pad, new GamepadState { leftStick = new Vector2(0, -1) });
            for (float t = 0; t < 1.2f; t += Time.unscaledDeltaTime)
            {
                yield return null;
                var p = Look(); if (p.name != last) { changes++; last = p.name; }
            }
            InputSystem.QueueStateEvent(pad, new GamepadState());
            for (int i = 0; i < 4; i++) yield return null;
            yield return Settle(screen, "stick down held 1.2 s (" + changes + " steps)");
            Debug.Log(Tag + "nav [" + screen + "] held stick: " + changes + " steps in 1.2 s");
            Check(changes <= 9, screen, "a held stick stepped " + changes + " times in 1.2 s (too fast)");
            // 4. the keyboard
            yield return Key(UnityEngine.InputSystem.Key.DownArrow); yield return Settle(screen, "key down");
            yield return Key(UnityEngine.InputSystem.Key.UpArrow); yield return Settle(screen, "key up");
            if (bumpers)
            {
                yield return Key(UnityEngine.InputSystem.Key.E); yield return Settle(screen, "key E");
                yield return Key(UnityEngine.InputSystem.Key.Q); yield return Settle(screen, "key Q");
            }
        }

        /// Every item of the screen is reachable from the focused one by the directions (OuNav.Neighbour; value rows keep left / right).
        static IEnumerator Reach(string screen)
        {
            OuNav nav = null;
            foreach (var n in OuNav.Live()) if (n.IsShown && n.Focused != null && (nav == null || n.Items().Count > nav.Items().Count)) nav = n;
            if (nav == null) { Debug.Log(Tag + "nav [" + screen + "] reach: no OuNav with focus"); yield break; }
            var items = nav.Items(); var start = nav.Focused;
            var seen = new HashSet<VisualElement> { start }; var queue = new Queue<VisualElement>(); queue.Enqueue(start);
            while (queue.Count > 0)
            {
                var cur = queue.Dequeue();
                foreach (var d in new[] { new Vector2Int(0, 1), new Vector2Int(0, -1), new Vector2Int(1, 0), new Vector2Int(-1, 0) })
                {
                    if (d.x != 0 && ((OuNavItem)cur.userData).adjust != null) continue;
                    var next = nav.Neighbour(cur, d);
                    if (next != null && seen.Add(next)) queue.Enqueue(next);
                }
            }
            var lost = items.Where(e => !seen.Contains(e)).ToList();
            Debug.Log(Tag + "nav [" + screen + "] reach: " + seen.Count + " of " + items.Count + " items" + (lost.Count > 0 ? " unreachable: " + string.Join(", ", lost.Take(8).Select(Label)) : ""));
            Check(lost.Count == 0, screen, lost.Count + " of " + items.Count + " items cannot be reached by the directions: " + string.Join(", ", lost.Take(8).Select(Label)));
            yield break;
        }

        // ------------------------------------------------------------------------------------------------ journeys

        static IEnumerator HubBrowserSetup()
        {
            var shell = OpenShell.Instance; Devices();
            shell.ShowMain(); yield return UiShowcase.Frames(30);
            yield return Settle("hub", "open");
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(40);                 // PLAY -> the PLAY page
            yield return Settle("play", "A on PLAY");
            Check(shell.Current == OpenShell.Screen.Play, "play", "A on PLAY did not open the PLAY page (" + shell.Current + ")");
            yield return Walk("play");
            var race = shell.Layer.Q<VisualElement>("ou-hub-race"); Check(race != null, "play", "no RACE row on the PLAY page");
            if (race != null) { shell.Nav.Focus(race); yield return UiShowcase.Frames(10); }
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(40);                 // RACE -> the browser
            yield return Settle("browser", "A on RACE");
            Check(shell.Current == OpenShell.Screen.Browser, "browser", "A on RACE did not open the browser (" + shell.Current + ")");
            yield return Walk("browser");
            yield return PadTap(GamepadButton.South); yield return UiShowcase.Frames(60);                 // a track -> the setup page
            yield return Settle("setup", "A on a track");
            yield return Walk("setup");
            yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(30);                  // B back
            yield return Settle("browser again", "B");
            yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(30);                  // B: the browser was opened from PLAY, so back to PLAY
            yield return Settle("play again", "B");
            Check(shell.Current == OpenShell.Screen.Play, "play", "B from the browser did not come back to the PLAY page (" + shell.Current + ")");
            yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(30);
            yield return Settle("hub again", "B");
            Check(shell.Current == OpenShell.Screen.Main, "hub", "B three times did not come back to the hub (" + shell.Current + ")");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator ShellScreens()
        {
            var shell = OpenShell.Instance; Devices();
            shell.ShowMain(); yield return UiShowcase.Frames(30); yield return Walk("hub");
            shell.ShowCredits(); yield return UiShowcase.Frames(40); yield return Walk("credits");
            yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(20); yield return Settle("hub after credits", "B");
            shell.ShowLibrary(true); yield return UiShowcase.Frames(40); yield return Walk("library");
            yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(20); yield return Settle("hub after library", "B");
            shell.ShowInstall(false); yield return UiShowcase.Frames(40); yield return Walk("install");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// The garage's drill-down with the pad alone (w-garageflow), a screenshot at every level: the hub's GARAGE, GARAGE > RIDER > HELMET > a
        /// brand / model > a colour previewed (B: back to the line, the helmet worn before is still worn), then equipped; RB: VEHICLE > a part type >
        /// a brand / model > an item previewed, then equipped; MY RIDES > a class > its vehicles; a purchase dialog (the focus is trapped and comes
        /// back); B all the way out, every B one level up with the focus on the entry it came from.
        static IEnumerator GarageJourney()
        {
            var shell = OpenShell.Instance; Devices();
            var walk = UiShowcase.AfterShot; UiShowcase.AfterShot = null;   // (this page is its own walk)
            Reflex.Port.Content.ReflexProfile.Disabled = false;
            if (!Reflex.Port.Content.CoreGarageProfile.Open()) throw new InvalidOperationException(Reflex.Port.Content.CoreGarageProfile.Error);
            if (Reflex.Port.Content.CoreGarageProfile.UnlockEverythingEnabled) Reflex.Port.Content.CoreGarageProfile.SetUnlockEverything(false);
            UiShowcaseGarage.Credits(250000);
            { var pv = Reflex.Port.Content.ReflexProfile.Open().Result ? Reflex.Port.Content.ReflexProfile.Current.T("PlayerVehicle") : null; if (pv != null && pv.rows > 0) Reflex.Port.Content.ReflexProfile.SelectRider(0, -1, pv.Int("PlayerVehicleId", 0)).Wait(); }
            shell.ShowGarage(); shell.ShowMain(); yield return UiShowcase.Frames(30);
            string F() => shell.Nav.Focused?.name ?? "(none)";
            void Where(string step) => Debug.Log(Tag + "garageflow " + step + ": " + (shell.GarageCrumb ?? "") + " | " + shell.GarageTitle + " (depth " + shell.GarageDepth + "), focus " + F());
            // one d-pad press at a time (held for a single frame, so a slow frame never turns it into a held repeat); at the list's end it turns round
            IEnumerator PadTo(Func<VisualElement, bool> want, GamepadButton dir, int max = 40)
            {
                bool turned = false;
                for (int i = 0; i < max && !(shell.Nav.Focused != null && want(shell.Nav.Focused)); i++)
                {
                    var was = shell.Nav.Focused;
                    yield return PadTap(dir, 1); yield return UiShowcase.Frames(3);
                    if (shell.Nav.Focused == was && !turned)
                    {
                        turned = true;
                        dir = dir == GamepadButton.DpadDown ? GamepadButton.DpadUp : dir == GamepadButton.DpadUp ? GamepadButton.DpadDown : dir == GamepadButton.DpadLeft ? GamepadButton.DpadRight : GamepadButton.DpadLeft;
                    }
                }
            }
            IEnumerator A(int frames = 40) { yield return PadTap(GamepadButton.South, 1); yield return UiShowcase.Frames(frames); }
            IEnumerator B(int frames = 40) { yield return PadTap(GamepadButton.East, 1); yield return UiShowcase.Frames(frames); }
            IEnumerator Shot(string n) { yield return UiShowcase.Frames(60); yield return UiShowcase.Shot("garageflow-" + n); }
            bool Has(VisualElement e, string pill) => e.Query<Label>(className: "ou-pill").ToList().Any(l => l.text == pill);

            // the hub's GARAGE with the pad: down to it, A
            for (int i = 0; i < 6 && shell.FocusedText != "GARAGE"; i++) { yield return PadTap(GamepadButton.DpadDown, 1); yield return UiShowcase.Frames(3); }
            Check(shell.FocusedText == "GARAGE", "garage", "the hub's GARAGE could not be reached with the d-pad (on " + shell.FocusedText + ")");
            yield return A(120);
            Check(shell.Current == OpenShell.Screen.Garage && shell.GarageDepth == 1, "garage", "A on GARAGE did not open the garage's home (" + shell.Current + ", depth " + shell.GarageDepth + ")");
            Where("home"); yield return Walk("garage-home", false);
            shell.Nav.Focus(shell.Layer.Q<VisualElement>("garage-h-vehicle")); yield return UiShowcase.Frames(4);
            yield return Shot("0-home");

            // ---- RIDER > HELMET > a brand / model > a colour
            yield return PadTo(e => e.name == "garage-h-rider", GamepadButton.DpadDown);
            Check(F() == "garage-h-rider", "garage", "the d-pad did not reach RIDER (" + F() + ")");
            yield return A(); Where("rider");
            Check(shell.GarageDepth == 2 && shell.GarageTitle == "RIDER", "garage", "A on RIDER: " + shell.GarageTitle);
            yield return Shot("r1-rider");
            int helmetType = OpenGarageData.GearTypes().FirstOrDefault(t => t.factory == "Helmet").id;
            yield return PadTo(e => e.name == "garage-gear-" + helmetType, GamepadButton.DpadDown);
            Check(F() == "garage-gear-" + helmetType, "garage", "the d-pad did not reach HELMET (" + F() + ")");
            yield return A(); Where("helmet");
            Check(shell.GarageDepth == 3 && shell.GarageTitle == "HELMET" && (shell.GarageCrumb ?? "").EndsWith("RIDER"), "garage", "A on HELMET: " + shell.GarageCrumb + " | " + shell.GarageTitle);
            yield return Shot("r2-helmet-brands");
            int wornHelmet = OpenGarageData.EquippedGear(helmetType);
            // a line the worn helmet is not in, with something to wear in it
            yield return PadTo(e => e.name != null && e.name.StartsWith("garage-line-") && !Has(e, "EQUIPPED"), GamepadButton.DpadDown);
            string lineCard = F();
            Check(lineCard.StartsWith("garage-line-"), "garage", "no brand / model line to open (" + lineCard + ")");
            yield return A(); Where("helmet line");
            Check(shell.GarageDepth == 4, "garage", "A on a line did not open its styles (depth " + shell.GarageDepth + ")");
            yield return Shot("r3-helmet-line");
            yield return PadTap(GamepadButton.DpadDown, 1); yield return UiShowcase.Frames(20);
            string colour = F();
            var tag = shell.Layer.Q<VisualElement>("garage-preview-tag");
            Check(tag != null, "garage", "a focused style that is not worn has no PREVIEW tag on the stage");
            Where("helmet colour previewed");
            yield return Shot("r4-helmet-colour-preview");
            yield return B(); Where("back from the colour");
            Check(shell.GarageDepth == 3 && F() == lineCard, "garage", "B from the styles did not return to the line card (" + F() + ", want " + lineCard + ")");
            Check(OpenGarageData.EquippedGear(helmetType) == wornHelmet, "garage", "browsing changed the worn helmet");
            yield return Shot("r5-back-to-brands");
            yield return A(); yield return PadTo(e => e.name == colour, GamepadButton.DpadDown, 8);
            var item = shell.Nav.Focused;
            bool owned = item != null && (Has(item, "OWNED") || Has(item, "EQUIPPED"));
            yield return A(30);
            if (shell.GarageHasDialog)
            {   // for sale: the purchase dialog (the scratch profile has the credits); the focus is trapped in it
                Where("purchase dialog"); yield return Shot("r6-purchase-dialog");
                for (int i = 0; i < 3; i++) { yield return PadTap(GamepadButton.DpadUp, 1); yield return PadTap(GamepadButton.DpadDown, 1); yield return PadTap(GamepadButton.DpadLeft, 1); yield return PadTap(GamepadButton.DpadRight, 1); }
                Check(F().StartsWith("garage-"), "garage dialog", "the focus left the dialog (" + F() + ")");
                yield return PadTo(e => e.name == "garage-buy", GamepadButton.DpadLeft, 3);
                yield return A(60);
                Check(!shell.GarageHasDialog, "garage dialog", "BUY did not close the dialog");
            }
            Check(F() == colour, "garage", "the focus did not stay on the style equipped (" + F() + ", want " + colour + ")");
            int now = OpenGarageData.EquippedGear(helmetType);
            Check(now != wornHelmet && "garage-item-" + now == colour, "garage", "A did not equip the style (" + now + ", owned before " + owned + ")");
            Check(shell.Nav.Focused != null && Has(shell.Nav.Focused, "EQUIPPED"), "garage", "the equipped style is not marked EQUIPPED");
            Where("helmet equipped"); yield return Shot("r7-helmet-equipped");
            yield return B(); Check(shell.GarageDepth == 3 && F() == lineCard && Has(shell.Nav.Focused, "EQUIPPED"), "garage", "the line of the new helmet is not marked EQUIPPED (" + F() + ")");
            yield return B(); Check(shell.GarageDepth == 2 && F() == "garage-gear-" + helmetType, "garage", "B did not return to HELMET (" + F() + ")");
            yield return Shot("r8-rider-after");

            // ---- RB: VEHICLE > a part type > a brand / model > an item
            yield return PadTap(GamepadButton.RightShoulder, 1); yield return UiShowcase.Frames(40); Where("RB");
            Check(shell.GarageDepth == 2 && shell.GarageTitle == "VEHICLE", "garage", "RB did not switch to VEHICLE (" + shell.GarageTitle + ")");
            yield return Shot("v1-vehicle");
            int vehicle = OpenGarageData.Equipped;
            var parts = OpenGarageData.PartTypes(vehicle);
            var exhaust = parts.FirstOrDefault(t => t.factory == "Exhaust"); int partType = exhaust.name != null ? exhaust.id : parts.First().id;
            yield return PadTo(e => e.name == "garage-part-" + partType, GamepadButton.DpadDown);
            Check(F() == "garage-part-" + partType, "garage", "the d-pad did not reach the part " + partType + " (" + F() + ")");
            yield return A(); Where("part type");
            Check(shell.GarageDepth == 3, "garage", "A on a part type did not open it");
            yield return Shot("v2-part-brands");
            int wornPart = OpenGarageData.EquippedPart(vehicle, partType);
            yield return PadTo(e => e.name != null && e.name.StartsWith("garage-line-") && !Has(e, "EQUIPPED"), GamepadButton.DpadDown);
            string partLine = F();
            if (partLine.StartsWith("garage-line-"))
            {
                yield return A(); Where("part line"); yield return Shot("v3-part-line");
                yield return PadTap(GamepadButton.DpadDown, 1); yield return UiShowcase.Frames(20);
            }
            else Debug.Log(Tag + "garageflow no unequipped part line: the single items");
            string partItem = F();
            Where("part previewed"); yield return Shot("v4-part-preview");
            yield return A(30);
            if (shell.GarageHasDialog) { yield return PadTo(e => e.name == "garage-buy", GamepadButton.DpadLeft, 3); yield return A(60); }
            int partNow = OpenGarageData.EquippedPart(vehicle, partType);
            Check(partNow != wornPart && "garage-item-" + partNow == partItem, "garage", "A did not fit the part (" + partNow + " vs " + partItem + ")");
            Where("part fitted"); yield return Shot("v5-part-fitted");
            while (shell.GarageDepth > 2) yield return B();
            Check(F() == "garage-part-" + partType, "garage", "B did not return to the part type (" + F() + ")");

            // ---- MY RIDES > a class > its vehicles
            yield return PadTo(e => e.name == "garage-my-rides", GamepadButton.DpadUp);
            yield return A(); Where("my rides"); yield return Shot("v6-my-rides");
            Check(shell.GarageDepth == 3 && F().StartsWith("garage-class-"), "garage", "MY RIDES did not open the classes (" + F() + ")");
            yield return A(); Where("class"); yield return Shot("v7-class-vehicles");
            Check(shell.GarageDepth == 4 && (F().StartsWith("garage-vehicle-") || F().StartsWith("garage-model-") || F().StartsWith("garage-mod-")), "garage", "a class did not open its vehicles (" + F() + ")");
            yield return PadTap(GamepadButton.DpadDown, 1); yield return UiShowcase.Frames(30);
            Where("vehicle previewed"); yield return Shot("v8-vehicle-preview");

            // ---- B all the way out
            for (int depth = shell.GarageDepth; depth > 1; depth--) { yield return B(); Check(shell.GarageDepth == depth - 1, "garage", "B went from depth " + depth + " to " + shell.GarageDepth); }
            Check(F() == "garage-h-vehicle", "garage", "the home did not focus VEHICLE after coming back up (" + F() + ")");
            yield return B(30);
            Check(shell.Current == OpenShell.Screen.Main, "garage", "B on the home did not leave the garage (" + shell.Current + ")");
            yield return Settle("hub after garage", "B");
            // the keyboard and the mouse's back: Esc goes up one level, the header's BACK too
            shell.ShowGarage(OpenShell.GarageGear); yield return UiShowcase.Frames(60);
            yield return A(); int d0 = shell.GarageDepth;
            yield return Key(UnityEngine.InputSystem.Key.Escape); yield return UiShowcase.Frames(20);
            Check(shell.GarageDepth == d0 - 1, "garage", "Esc did not go up one level (" + d0 + " -> " + shell.GarageDepth + ")");
            yield return Key(UnityEngine.InputSystem.Key.E); yield return UiShowcase.Frames(30);
            Check(shell.GarageTitle == "VEHICLE", "garage", "E did not switch to VEHICLE (" + shell.GarageTitle + ")");
            Debug.Log(Tag + "garageflow done, " + Failures + " nav failures so far");
            UiShowcase.AfterShot = walk;
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// The screen position (Input System: origin bottom-left) of the middle of an element.
        static Vector2 ScreenPoint(VisualElement e)
        {
            var root = e.panel.visualTree; float k = UnityEngine.Screen.height / root.worldBound.height;
            var c = e.worldBound.center; return new Vector2(c.x * k, UnityEngine.Screen.height - c.y * k);
        }

        static IEnumerator MouseMove(Vector2 to, int frames = 4)
        {
            Devices();
            var from = mouse.position.ReadValue();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = to, delta = to - from });
            yield return null;
            for (int i = 1; i < frames; i++) { InputSystem.QueueStateEvent(mouse, new MouseState { position = to }); yield return null; }
        }

        static IEnumerator MouseClick(Vector2 at)
        {
            yield return MouseMove(at);
            InputSystem.QueueStateEvent(mouse, new MouseState { position = at }.WithButton(UnityEngine.InputSystem.LowLevel.MouseButton.Left, true)); yield return null; yield return null;
            InputSystem.QueueStateEvent(mouse, new MouseState { position = at }); for (int i = 0; i < 4; i++) yield return null;
        }

        /// The mouse: hovering focuses an item, a click uses it, and a list scrolling under a resting pointer does not take the focus
        /// from the pad.
        static IEnumerator MouseJourney()
        {
            var shell = OpenShell.Instance; Devices();
            shell.ShowMain(); yield return UiShowcase.Frames(40);
            var options = shell.Layer.Q<VisualElement>("ou-hub-options"); var garage = shell.Layer.Q<VisualElement>("ou-hub-garage");
            yield return MouseMove(ScreenPoint(garage)); yield return Settle("mouse hub", "hover GARAGE");
            Check(Look().focused == garage, "mouse", "hovering GARAGE did not focus it (" + Look().name + ")");
            yield return MouseMove(ScreenPoint(options)); yield return Settle("mouse hub", "hover OPTIONS");
            Check(Look().focused == options, "mouse", "hovering OPTIONS did not focus it (" + Look().name + ")");
            yield return PadTap(GamepadButton.DpadUp); yield return Settle("mouse hub", "pad up");
            Check(Look().focused == garage, "mouse", "the pad did not move the focus off the resting pointer's item (" + Look().name + ")");
            // a click
            var credits = shell.Layer.Q<VisualElement>("ou-hub-credits");
            yield return MouseClick(ScreenPoint(credits)); yield return UiShowcase.Frames(30);
            Check(shell.Current == OpenShell.Screen.Credits, "mouse", "a click on CREDITS did not open it (" + shell.Current + ")");
            yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(20); yield return Settle("mouse hub", "back");
            // the browser: a pointer resting on a card while the pad walks the list
            shell.ShowBrowser(false); yield return UiShowcase.Frames(60);
            var cards = shell.Nav.Items().Where(e => e.ClassListContains("ou-card")).ToList();
            if (cards.Count > 3)
            {
                yield return MouseMove(ScreenPoint(cards[2])); yield return Settle("mouse browser", "hover a card"); yield return UiShowcase.Frames(60);   // (the pointer rests)
                var seen = new List<VisualElement> { Look().focused };
                for (int i = 0; i < cards.Count + 1; i++)
                {
                    yield return PadTap(GamepadButton.DpadDown); yield return UiShowcase.Frames(8);
                    seen.Add(Look().focused);
                }
                Debug.Log(Tag + "nav [mouse browser] focus under a resting pointer while the pad walks: " + string.Join(" > ", seen.Select(e => e == null ? "-" : cards.IndexOf(e).ToString())));
                Check(seen.Distinct().Count() >= 3, "mouse", "the pad's steps were taken over by the resting pointer (" + seen.Distinct().Count() + " distinct items)");
            }
            else Debug.Log(Tag + "nav [mouse browser] too few cards (" + cards.Count + ")");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ long lists (w-padscroll)

        /// The scrolling list of the screen on top: the scrollable ScrollView holding the most items.
        static ScrollView BiggestList(Probe p)
        {
            ScrollView best = null; int bestN = 0;
            if (p.doc == null) return null;
            foreach (var sv in p.doc.rootVisualElement.Query<ScrollView>().ToList())
            {
                if (sv.resolvedStyle.display == DisplayStyle.None || ScrollMax(sv) < 4f) continue;
                int n = ListItems(sv).Count; if (n > bestN) { bestN = n; best = sv; }
            }
            return best;
        }

        static float ScrollMax(ScrollView sv) => Mathf.Max(0f, sv.verticalScroller.highValue);

        static List<VisualElement> ListItems(ScrollView sv)
        {
            var l = new List<VisualElement>();
            foreach (var e in sv.contentContainer.Query<VisualElement>(className: "mxa-item").ToList()) if (e.userData is OuNavItem && Shown(e) && e.enabledInHierarchy) l.Add(e);
            return l;
        }

        /// At least 85% of the row is inside the list's viewport.
        static bool InView(ScrollView sv, VisualElement e)
        {
            var v = sv.contentViewport.worldBound; var b = e.worldBound;
            float inside = Mathf.Min(b.yMax, v.yMax) - Mathf.Max(b.yMin, v.yMin);
            return inside >= 0.85f * b.height;
        }

        /// Pad down (or up) through the biggest scrolling list of the screen up: every press must move to a later (earlier) row and keep the
        /// row in view, and the walk must reach the last (first) row. "Jumped back" = the focus left the list or went to an earlier row before the end.
        static IEnumerator LongList(string screen, int cap = 160, string shotName = null, bool jitter = false)
        {
            for (int i = 0; i < 30 && Look().focused == null; i++) yield return null;
            for (int i = 0; i < 40; i++) yield return null;
            var p = Look();
            if (shotName != null) yield return UiShowcase.Shot(shotName + "-open");
            var sv = BiggestList(p);
            if (sv == null) { Debug.Log(Tag + "longlist [" + screen + "] no scrollable list (" + p.items + " items)"); yield break; }
            var items = ListItems(sv);
            // into the list first (a header / tabs may hold the first focus)
            for (int i = 0; i < 12 && (p.focused == null || !items.Contains(p.focused)); i++) { yield return PadTap(GamepadButton.DpadDown); yield return Settle(screen, "into the list"); p = Look(); }
            Check(p.focused != null && items.Contains(p.focused), screen, "the list could not be entered with the d-pad");
            if (p.focused == null || !items.Contains(p.focused)) yield break;
            if (jitter)
            {   // the pointer rests over the second row's first card (the top of what is shown), with noise
                var over = items.Find(e => InView(sv, e) && e.worldBound.yMin > sv.contentViewport.worldBound.yMin + 20f) ?? items[Mathf.Min(1, items.Count - 1)];
                Devices(); jitterAt = ScreenPoint(over); jitterLast = jitterAt;
                InputSystem.QueueStateEvent(mouse, new MouseState { position = jitterAt, delta = jitterAt - mouse.position.ReadValue() });
                for (int k = 0; k < 4; k++) yield return null;
                Debug.Log(Tag + "longlist [" + screen + "] pointer placed: focus on " + (Look().focused != null ? Label(Look().focused) : "-") + " (the card under it: " + Label(over) + ")");
                jitterOn = true; jitterEnters = 0; jitterEntersMoving = 0;
                foreach (var it in items) { var el = it; el.RegisterCallback<PointerEnterEvent>(_ => { jitterEnters++; if (OuUi.MouseMoving()) jitterEntersMoving++; }); }
                Debug.Log(Tag + "longlist [" + screen + "] pointer resting over " + Label(over) + " with 1.5 px of noise per frame");
            }
            // back to the top row first
            for (int i = 0; i < cap && Mathf.Abs(Look().focused.worldBound.yMin - items[0].worldBound.yMin) > 12f; i++) { yield return PadTap(GamepadButton.DpadUp); yield return UiShowcase.Frames(3); if (!items.Contains(Look().focused)) break; }
            foreach (var dirDown in new[] { true, false })
            {
                var seen = new HashSet<VisualElement>(); int back = 0, offView = 0, steps = 0, same = 0, leftFrom = -1, wasted = 0; VisualElement prev = Look().focused; string trail = "";
                for (int i = 0; i < cap; i++, steps++)
                {
                    yield return PadTap(dirDown ? GamepadButton.DpadDown : GamepadButton.DpadUp); yield return JFrames(4);
                    var f = Look().focused;
                    if (f == null) { Check(false, screen, "the focus was lost on step " + i); break; }
                    if (!items.Contains(f) && sv.contentContainer.Contains(f)) items = ListItems(sv);   // (a row of the list was made again, the garage's ENGINE SOUND: the list is read again)
                    int at = items.IndexOf(f), was = items.IndexOf(prev);
                    if (at < 0) { leftFrom = was; Debug.Log(Tag + "longlist [" + screen + "] " + (dirDown ? "down" : "up") + " step " + i + " left the list for " + Label(f) + " from row " + was + " of " + items.Count); trail += " >" + Label(f); break; }
                    if (f == prev) { wasted++; if (++same >= 3) break; continue; }
                    same = 0;
                    if (was >= 0 && (dirDown ? at < was : at > was)) { back++; Debug.Log(Tag + "longlist [" + screen + "] " + (dirDown ? "down" : "up") + " step " + i + " JUMPED BACK from row " + was + " to row " + at + " of " + items.Count + " (offset " + sv.scrollOffset.y.ToString("F0") + " of " + ScrollMax(sv).ToString("F0") + ")"); }
                    if (!InView(sv, f)) { offView++; Debug.Log(Tag + "longlist [" + screen + "] step " + i + " row " + at + " is out of view (offset " + sv.scrollOffset.y.ToString("F0") + ")"); }
                    seen.Add(f); prev = f;
                    if (i % 10 == 0) Debug.Log(Tag + "longlist [" + screen + "] " + (dirDown ? "down" : "up") + " step " + i + " row " + at + " of " + items.Count + " offset " + sv.scrollOffset.y.ToString("F0") + "/" + ScrollMax(sv).ToString("F0"));
                }
                wasted = Mathf.Max(0, wasted - same);   // (the presses at the end of the list do not count)
                var end = Look().focused;
                bool atEnd = end != null && items.Contains(end) && Mathf.Abs(end.worldBound.yMin - (dirDown ? items[items.Count - 1] : items[0]).worldBound.yMin) < 12f;
                if (!atEnd && leftFrom >= 0) atEnd = Mathf.Abs(items[leftFrom].worldBound.yMin - (dirDown ? items[items.Count - 1] : items[0]).worldBound.yMin) < 12f;   // (left the list from its last / first row: the end)
                Debug.Log(Tag + "longlist [" + screen + "] " + (dirDown ? "DOWN" : "UP") + ": " + seen.Count + " rows visited of " + items.Count + ", ended on row " + (end != null ? items.IndexOf(end) : -1) + " (" + (atEnd ? "the " + (dirDown ? "last" : "first") + " row" : "NOT the end") + "), jumped back " + back + "x, out of view " + offView + "x, scroll offset " + sv.scrollOffset.y.ToString("F0") + " of " + ScrollMax(sv).ToString("F0") + trail);
                Check(wasted == 0, screen, wasted + " " + (dirDown ? "down" : "up") + " presses moved nothing before the end (the focus was taken back)");
                Check(back == 0, screen, (dirDown ? "down" : "up") + " jumped back " + back + " times in a long list");
                Check(atEnd, screen, (dirDown ? "down" : "up") + " never reached the " + (dirDown ? "last" : "first") + " row");
                Check(offView == 0, screen, offView + " focused rows were not scrolled into view");
                if (dirDown && shotName != null) yield return UiShowcase.Shot(shotName);
            }
            if (jitter) Debug.Log(Tag + "longlist [" + screen + "] pointer enters during the walk: " + jitterEnters + " (of them while the pointer counted as moving: " + jitterEntersMoving + ")");
            jitterOn = false;
        }

        /// More events than the scratch install has (the real one has the Alive levels and the downloads): copies of the events under other names.
        static void PadEvents(int copies)
        {
            var list = OpenContent.Events as List<OpenEvent>;
            if (list == null || list.Count > 400) return;
            var src = list.ToList();
            for (int k = 1; k <= copies; k++)
                foreach (var e in src)
                {
                    var c = new OpenEvent { ev = e.ev, key = e.key, activity = e.activity, typeName = e.typeName, typeDescription = e.typeDescription, typeVehicles = e.typeVehicles,
                        locationName = e.locationName + " " + (k + 1), description = e.description, typeId = e.typeId, previewIcon = e.previewIcon, mapIcon = e.mapIcon, posterIcon = e.posterIcon,
                        typeIcon = e.typeIcon, dlc = e.dlc, freeRide = e.freeRide, order = e.order + k * 0.01f, group = e.group };
                    c.haystack = e.haystack + " copy" + (k + 1);
                    list.Add(c);
                }
            Debug.Log(Tag + "padded the events: " + src.Count + " -> " + list.Count);
        }

        /// A pointer that rests over a list but is never perfectly still (a touchpad, a remote desktop, a mouse with noise) must not take the focus
        /// from the pad when the list scrolls under it.
        static IEnumerator HoverJitter()
        {
            var shell = OpenShell.Instance; Devices();
            PadEvents(5);
            foreach (var free in new[] { false, true })
            {
                OpenHistory.View = 0;
                shell.ShowMain(); yield return UiShowcase.Frames(20);
                shell.ShowBrowser(free); yield return UiShowcase.Frames(60);
                yield return LongList("jitter browser " + (free ? "free ride" : "race") + " grid", 160, "hoverjitter-" + (free ? "free" : "race"), true);
            }
            jitterOn = false;
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator LongLists()
        {
            var shell = OpenShell.Instance; Devices();
            PadEvents(5);
            foreach (var (free, view) in new[] { (false, 0), (false, 1), (true, 0) })
            {
                OpenHistory.View = view;
                shell.ShowMain(); yield return UiShowcase.Frames(20);
                shell.ShowBrowser(free); yield return UiShowcase.Frames(60);
                yield return LongList("browser " + (free ? "free ride" : "race") + (view == 0 ? " grid" : " list"), 160, "longlist-browser-" + (free ? "free" : "race") + (view == 0 ? "-grid" : "-list"));
            }
            OpenHistory.View = 0;
            // a held stick down the grid: the focus must end on the last row, never on the top
            shell.ShowMain(); yield return UiShowcase.Frames(20);
            shell.ShowBrowser(false); yield return UiShowcase.Frames(60);
            {
                var sv = BiggestList(Look());
                if (sv != null)
                {
                    var items = ListItems(sv); int last = -1, back = 0;
                    Devices();
                    InputSystem.QueueStateEvent(pad, new GamepadState { leftStick = new Vector2(0, -1) });
                    for (float t = 0; t < 14f; t += Time.unscaledDeltaTime)
                    {
                        yield return null;
                        var f = Look().focused; int at = f != null ? items.IndexOf(f) : -2;
                        if (at >= 0 && at < last) back++;
                        if (at >= 0) last = at;
                    }
                    InputSystem.QueueStateEvent(pad, new GamepadState());
                    for (int i = 0; i < 4; i++) yield return null;
                    Debug.Log(Tag + "longlist [browser held stick] ended on row " + last + " of " + items.Count + ", jumped back " + back + "x");
                    Check(back == 0 && last >= items.Count - 4, "browser held stick", "ended on row " + last + " of " + items.Count + " after 14 s held, jumped back " + back + "x");
                }
            }
            shell.ShowMain(); yield return UiShowcase.Frames(20);
            shell.ShowCredits(); yield return UiShowcase.Frames(40); yield return LongList("credits");
            shell.ShowMain(); yield return UiShowcase.Frames(20);
            shell.ShowPhysicsStudio(0); yield return UiShowcase.Frames(40); yield return LongList("physics studio");
            shell.ShowMain(); yield return UiShowcase.Frames(20);
            for (int tab = 0; tab < ReflexOptions.Tabs.Length; tab++)
            {
                shell.ShowOptions(tab); yield return UiShowcase.Frames(40);
                yield return LongList("options-" + ReflexOptions.Tabs[tab]);
                yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(30);
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator OptionsJourney()
        {
            var shell = OpenShell.Instance; Devices();
            shell.ShowMain(); yield return UiShowcase.Frames(30);
            for (int tab = 0; tab < ReflexOptions.Tabs.Length; tab++)
            {
                shell.ShowOptions(tab); yield return UiShowcase.Frames(40);
                yield return Walk("options-" + ReflexOptions.Tabs[tab]);
                yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(30);
                Check(!ReflexOptions.IsOpen, "options", "B did not close the options (tab " + tab + ")");
                yield return Settle("hub after options", "B");
            }
        }
    }
}
