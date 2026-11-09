// The event load in the test-ride arena (w-loadarena; a player's report: "I'd like to see the test ride arena while loading an event just
// like reflex. When event loads they can press START on the controller to enter."). START EVENT / RIDE NOW no longer changes to the race
// scene at once (whose world drew nothing until the core's startup, the camera's clear colour and the sky under the floating scenery): the
// front end's 3D menu world, the compound the garage's TEST RIDE rides (UI_Compound), stays on screen with the player's rider beside his
// vehicle, the camera turning slowly around them, while the event's core session loads in the background (OpenEventPrestart). The card
// bottom-left names the event and fills its bar; when the event is loaded it says PRESS START (the pad's Start or A, Enter or Space, a
// click on it), and only then the race scene loads (OpenLoading over its last second of setup) and takes the loaded session over.
//   why not ridden  the core runs one session at a time (one arena: rx_api.h THREADING) and the event's load is one blocking call on the
//                   session's own thread (rx_session_event_load), then its load states as frames of that session: a test drive cannot run
//                   beside it, nor in the same session while it loads. The arena is shown, not ridden (the same compound, the same rider
//                   and vehicle as the test ride).
//   no 3D menu      (REFLEX_MENU3D=0, a compound that cannot be read): the event's location picture stands in behind the same card.
//   failure         the session's error on the card, B / Esc back to the hub (nothing was saved).
using System;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        sealed class LoadArenaState
        {
            public OpenEvent ev; public OpenSetup setup; public OpenEventPrestart pre;
            public VisualElement picture, fill, prompt, promptKeys; public Label eyebrow, line;
            public float readyAt = -1f, shown; public string device; public bool launching, failed;
        }
        LoadArenaState _arena;

        /// The arena's state for tests: "loading", "ready", "failed", "launching" (null: no event loading here).
        public string LoadArenaPhase => _arena == null ? null : _arena.launching ? "launching" : _arena.failed ? "failed" : _arena.readyAt >= 0 ? "ready" : "loading";

        /// START EVENT's load in the arena; false when this event loads the old way (OpenEventPrestart.Supported, or the session could not start).
        bool ShowLoadArena(OpenEvent ev, OpenSetup s)
        {
            double pressed = Time.realtimeSinceStartupAsDouble;
            var pre = OpenEventPrestart.Begin(ev, s, out string why);
            if (pre == null) { Debug.Log("[LoadArena] " + ev.key + ": loads without the arena (" + why + ")"); return false; }
            // ridden: the garage's test ride in the compound while it loads (OpenLoadArena.cs); else the compound here, shown
            if (OpenLoadArena.Begin(ev, s, pressed, out string whyNot)) { _nav.Suspended = () => true; return true; }
            Debug.Log("[LoadArena] " + ev.key + ": the arena is shown, not ridden (" + whyNot + ")");
            var a = _arena = new LoadArenaState { ev = ev, setup = s, pre = pre };
            _nav.Suspended = () => true;   // (the arena reads its own keys: nothing under it moves)
            var box = OuUi.Div("ou-loading ou-loadarena", _layer); box.name = "loadarena";
            box.pickingMode = PickingMode.Ignore;
            a.picture = OuUi.Div("ou-loading-shot", box);   // (the location's picture when there is no 3D menu world)
            OpenBackdrop.Apply(a.picture, OpenContent.Backdrop(ev));
            a.picture.style.display = MenuWorld != null ? DisplayStyle.None : DisplayStyle.Flex;
            var shade = OuUi.Div("ou-loading-shade", box); shade.style.backgroundImage = OuUi.ScrimBottom; shade.pickingMode = PickingMode.Ignore;
            OuTheme.Logo(OuUi.Div("ou-loading-logo", box), 64, true);
            var copy = OuUi.Div("ou-loading-copy", box);
            a.eyebrow = OuUi.Text("", "ou-eyebrow", copy); a.eyebrow.enableRichText = false;
            var title = OuUi.Text(ev.locationName, "ou-loading-title", copy); title.enableRichText = false; title.name = "loadarena-title";
            var bar = OuUi.Div("ou-loading-bar", copy);
            a.fill = OuTheme.Backdrop(bar, "ou-loading-bar-fill");
            a.line = OuUi.Text("", "ou-loading-sub", copy); a.line.name = "loadarena-line"; a.line.enableRichText = false;
            // PRESS START: under the bar once the event is loaded (the hint bar's caps, at the card's size)
            // (not OuUi.Div("mxa-hints"): a tracked hint bar is placed along the screen's bottom)
            a.prompt = OuUi.Div("", copy); a.prompt.AddToClassList("mxa-hints"); a.prompt.AddToClassList("ou-loadarena-prompt"); a.prompt.name = "loadarena-start";
            a.prompt.style.display = DisplayStyle.None;
            a.prompt.pickingMode = PickingMode.Position;
            a.prompt.RegisterCallback<ClickEvent>(e => { if (e.button == 0) LoadArenaEnter(); });
            LoadArenaText();
            OuMotion.Enter(copy, 0.05f, 0.02f, true);
            if (MenuWorld != null) MenuWorld.LoadingTurn = true;
            _worldScreenDirty = true;
            ScreenUpdate = LoadArenaUpdate;
            Debug.Log("[LoadArena] " + ev.key + ": the arena while it loads" + (MenuWorld != null ? " (the compound, " + MenuWorld.HeroKey + ")" : " (the location's picture: no 3D menu world)"));
            return true;
        }

        void LoadArenaText()
        {
            var a = _arena; if (a == null) return;
            string what = a.ev.typeName + (a.ev.ev.spec.trackId > 0 ? "  ·  TRACK " + (a.ev.ev.spec.trackId + 1) : "");
            if (a.failed) { a.eyebrow.text = "THE EVENT COULD NOT LOAD"; return; }
            a.eyebrow.text = (a.readyAt >= 0 ? "READY" : OpenContent.Str("STR_LOADING", "LOADING").ToUpperInvariant()) + "  ·  " + what;
        }

        /// The device in use's START: the pad's Start (Menu / Options), the keyboard's Enter.
        void LoadArenaPrompt(string verb)
        {
            var a = _arena; if (a == null) return;
            a.device = OuGlyphs.Current.ToString() + verb;
            a.prompt.Clear();
            var h = OuUi.Div("mxa-hint", a.prompt);
            OuUi.Text("PRESS", "mxa-hint-label", h).style.marginRight = 10;
            string key = verb == "BACK" ? OuGlyphs.Cancel : OuGlyphs.Current == OuGlyphs.Device.Keyboard ? "Enter" : OuGlyphs.Menu;
            OuUi.Text(key, "mxa-key " + (OuGlyphs.CapClass(key) ?? ""), h);
            OuUi.Text(verb == "BACK" ? "TO GO BACK" : "TO START", "mxa-hint-label", h);
        }

        void LoadArenaUpdate()
        {
            var a = _arena; if (a == null || a.launching) return;
            float dt = Time.unscaledDeltaTime;
            if (MenuWorld != null)
            {
                if (a.picture.style.display == DisplayStyle.Flex) a.picture.style.display = DisplayStyle.None;   // (the compound came up after all)
                if (!MenuWorld.LoadingTurn) MenuWorld.LoadingTurn = true;
            }
            var pre = a.pre;
            string failure = pre ? pre.Failure : "the session was closed";
            if (failure != null)
            {
                if (!a.failed)
                {
                    a.failed = true; a.readyAt = -1;
                    Debug.LogWarning("[LoadArena] " + a.ev.key + ": " + failure);
                    OuSound.Play("Locked");
                    LoadArenaText();
                    a.line.text = failure.ToUpperInvariant();
                    a.fill.style.width = Length.Percent(0);
                    a.prompt.style.display = DisplayStyle.Flex; LoadArenaPrompt("BACK");
                }
                if (a.device != OuGlyphs.Current + "BACK") LoadArenaPrompt("BACK");
                if (Keyboard.current?.escapeKey.wasPressedThisFrame == true || Keyboard.current?.backspaceKey.wasPressedThisFrame == true || PadPressed(p => p.buttonEast)) LoadArenaLeave();
                return;
            }
            a.fill.style.width = Length.Percent(Mathf.Clamp01(pre.Progress) * 100f);
            if (!pre.Ready)
            {
                int dots = 1 + (int)(Time.unscaledTime * 2.5f) % 3;
                a.line.text = OpenContent.Str("STR_LOADING", "LOADING").ToUpperInvariant() + new string('.', dots);
                return;
            }
            if (a.readyAt < 0)
            {
                a.readyAt = Time.unscaledTime;
                LoadArenaText();
                a.line.text = "EVENT LOADED";
                a.prompt.style.display = DisplayStyle.Flex; LoadArenaPrompt("START");
                OuSound.Play("PageIn");
                Debug.Log($"[LoadArena] {a.ev.key}: PRESS START ({(pre.ReadySeconds >= 0 ? pre.ReadySeconds : pre.Age):F2} s after START EVENT)");
            }
            if (a.device != OuGlyphs.Current + "START") LoadArenaPrompt("START");
            // the prompt breathes (not with REDUCE MOTION)
            a.prompt.style.opacity = OuMotion.Reduced ? 1f : 0.62f + 0.38f * (0.5f + 0.5f * Mathf.Cos((Time.unscaledTime - a.readyAt) * 3.2f));
            if (Time.unscaledTime - a.readyAt < 0.25f) return;   // (a press that was on its way as the prompt came up does not start it)
            var kb = Keyboard.current;
            bool go = kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame || kb.spaceKey.wasPressedThisFrame)
                      || PadPressed(p => p.startButton) || PadPressed(p => p.buttonSouth);
            if (go) LoadArenaEnter();
        }

        static bool PadPressed(Func<Gamepad, UnityEngine.InputSystem.Controls.ButtonControl> button)
        {
            foreach (var pad in Gamepad.all) if (button(pad).wasPressedThisFrame) return true;
            return false;
        }

        /// PRESS START: the race scene (OpenLoading over its setup), which takes the loaded session over.
        void LoadArenaEnter()
        {
            var a = _arena;
            if (a == null || a.launching || a.failed || !a.pre || !a.pre.Ready) return;
            a.launching = true;
            OuSound.Play("Accept");
            a.pre.Launched();
            if (MenuWorld != null) MenuWorld.LoadingTurn = false;
            Debug.Log("[LoadArena] " + a.ev.key + ": START");
            OpenLoading.Show(a.ev, a.setup);
            // one frame of the loading page before the blocking scene change
            int frame = Time.frameCount;
            ScreenUpdate = () => { if (Time.frameCount > frame) { ScreenUpdate = null; a.setup.Launch(); } };
        }

        /// The load arena's session kept warm while the front end is idle (OpenArenaWarm): a menu page up (no launch under way, no intro), the
        /// 3D menu world loaded, a few seconds after the front end started (its own loads first); for the vehicle of the event in view.
        void ArenaWarmUpdate()
        {
            if (Current == Screen.None || Current == Screen.Loading || Current == Screen.Install || Current == Screen.Working || Current == Screen.Garage || _arena != null || IntroBlocksInput) return;   // (the garage: its edits would remake it each time; after it)
            if (Time.realtimeSinceStartup - _arenaWarmFrom < 3f || _world && _world.Status == OpenMenuWorld.State.Loading) return;
            if (!OpenArenaWarm.Due) return;   // (its check is once a second: the selection is not looked up every frame, w-menufps: ~1 ms a frame)
            OpenArenaWarm.Tick(_browseEvent != null && _setup != null ? OpenLoadArena.Selection(_browseEvent, _setup) : null);
        }
        float _arenaWarmFrom = float.MaxValue;

        /// The event could not load: back to the hub (the session is closed).
        void LoadArenaLeave()
        {
            var a = _arena; _arena = null;
            if (a == null) return;
            OpenEventPrestart.Release("the arena was left");
            if (MenuWorld != null) MenuWorld.LoadingTurn = false;
            _nav.Suspended = null;
            OuSound.Play("Back");
            _sounds?.music?.SetEnable(true);
            ShowMain();
        }
    }
}
