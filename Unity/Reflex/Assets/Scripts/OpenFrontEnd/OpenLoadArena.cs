// The event load RIDDEN in the test-ride arena (w-loadarena; the player's report: "I'd like to see the test ride arena while loading an event
// just like reflex. When event loads they can press START on the controller to enter."). START EVENT / RIDE NOW starts the event's core session
// in the background (OpenEventPrestart: the main core image) and the player rides the garage's TEST RIDE in the compound meanwhile, in the core's
// second image (RxNativeLoader.LoadArena: a second loaded copy of the library, its own arena and session on its own thread; the core allows one
// session per image). A card names the event and fills its bar; once the event is loaded it says PRESS START (the pad's Start, Esc or Enter),
// and the race scene loads with the event, its runtime taking the loaded session over.
//   OpenLoadArena   the launch from the shell (the black, the race scene with the test drive), START and the way back
//   OwnLoadArena    the race scene's part during the ride (attached by OriginalWorldRuntime in place of OwnTestRide): the card, the prompt, START
// Nothing heavy runs on the main thread while riding: the event's load is the core's (its worker), the event's Unity world is built after START
// (under the black and the loading page), so the ride keeps its frame rate. The ride saves nothing (TestDrivePersistence) and rides the original
// physics (PHYSICS STUDIO's values stay the event's).
// Where riding cannot be (REFLEX_LOADARENA_RIDE=0, the second image could not be loaded, a custom track): the front end's compound with the
// same card (OpenShell.LoadArena.cs).
// Fast (w-arenafast): the second image and the arena's session are made in the menus (OpenArenaWarm), so START EVENT only covers the scene
// change with a short black and the race scene builds the ride's Unity world; the log's "[LoadArena] +<ms>" lines give each step's time.
using System;
using System.IO;
using Reflex.Port.Content;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OpenLoadArena
    {
        static OpenEvent s_ev; static OpenSetup s_setup;
        /// An event is loading while the player rides the arena (the race scene's runtime starts the test drive in the arena image).
        public static bool Riding => s_ev != null;
        public static OpenEvent Event => s_ev;
        /// Tests: "launch" (to the arena), "riding", "entering" (START: to the event), "leaving" (back to the menu), "" none.
        public static string Phase { get; internal set; } = "";
        /// Diagnostics: seconds from START EVENT to the arena's first drawn ride frame.
        public static float RideSeconds { get; internal set; } = -1f;
        static double s_began = -1, s_markAt;
        /// The black before the race scene loads (seconds; the garage's TEST RIDE keeps its 0.35): the arena's session is made ahead (OpenArenaWarm),
        /// so the black is only the cover of the scene change.
        const float CurtainSeconds = 0.12f;

        /// The arena can ride now (why: the reason it cannot). The second image is checked when it is loaded (OpenArenaWarm.LoadImage).
        public static bool CanRide(out string why)
        {
            why = null;
            if (Environment.GetEnvironmentVariable("REFLEX_LOADARENA_RIDE") == "0") why = "REFLEX_LOADARENA_RIDE=0";
            else if (Environment.GetEnvironmentVariable("REFLEX_LOADARENA") == "0") why = "REFLEX_LOADARENA=0";
            else if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) why = "not the open core";
            else if (OpenTestRide.Current != null || ReflexGarage.TestDrive.Active || Riding) why = "a test ride is running";
            return why == null;
        }

        /// The vehicle the arena rides for this event: the event's own when it is one of the player's (else null: the equipped one); this
        /// session only, nothing is saved.
        public static ReflexWorldFiles.Garage Selection(OpenEvent ev, OpenSetup setup)
        {
            var g = ev?.ev != null ? setup?.Garage(ev.ev) : null;
            return g != null && g.vehicleOwner == ReflexWorldFiles.Garage.Player1
                ? new ReflexWorldFiles.Garage { riderOwner = g.riderOwner, riderId = g.riderId, riderGearSet = g.riderGearSet,   // (the rider too: a pro rider left in the
                                                 vehicleOwner = ReflexWorldFiles.Garage.Player1, vehicleId = g.vehicleId, vehicleTypeId = g.vehicleTypeId }   // profile does not own the player's vehicle)
                : null;
        }

        /// The front end is up while an arena ride was still on (its ride stopped and RETURN TO MENU, a scene change around Leave / Enter):
        /// the test drive ends, so the next event is not taken for a test ride (no intro, START leaving to the menu).
        public static void Abandon()
        {
            if (!Riding && Phase.Length == 0) return;
            Debug.LogWarning("[LoadArena] the front end is up during the arena ride (phase " + (Phase.Length > 0 ? Phase : "-") + "): the ride is ended");
            if (Phase != "entering") OpenEventPrestart.Release("back to the menu");
            ReflexGarage.TestDrive.End();
            s_ev = null; s_setup = null; Phase = ""; s_began = -1;
        }

        /// START EVENT with the ridable arena (the event's session already started: OpenEventPrestart.Begin; startedAt: the press, for the
        /// timing lines). False when it cannot ride here.
        public static bool Begin(OpenEvent ev, OpenSetup setup, double startedAt, out string why)
        {
            if (!CanRide(out why)) return false;
            if (ev?.ev == null || setup == null) { why = "no event"; return false; }
            if (ev.ev.trackMod != null) { why = "a custom track (its files are bound while its session starts; the arena's scene would unbind them)"; return false; }
            s_began = s_markAt = startedAt; Phase = "launch";
            Mark("START EVENT: the event's session starts on its worker");
            if (!OpenArenaWarm.LoadImage(out why, out string image)) { Phase = ""; s_began = -1; why = "the core's second image: " + why; return false; }
            s_ev = ev; s_setup = setup; RideSeconds = -1f;
            var selection = Selection(ev, setup);
            Mark("the core's second image " + image);
            // the arena's session: the one kept warm in the menus, else started now (before the black and the scene change, not after them)
            if (!OpenArenaWarm.Claim(selection, out string session)) session = "the race scene starts it (" + session + ")";
            Mark("the arena's session " + session);
            var run = ReflexGarage.TestDrive.Begin();
            Debug.Log("[LoadArena] " + ev.key + ": the test-ride arena while it loads (the core's second image " + RxNativeLoader.ArenaPath + (selection != null ? ", vehicle " + selection.vehicleId : ", the equipped vehicle") + ")");
            OpenTestRideCurtain.Close("LOADING", (ev.typeName + "  ·  " + ev.locationName).Trim(' ', '·'), () =>
            {
                Mark("the black is up: the race scene loads");
                ReflexEventSelect.Launch(run.ev.key, 0, false, "Reflex", 0, selection);
            }, CurtainSeconds);
            return true;
        }

        /// A step of the launch in the log: "[LoadArena] +<ms since START EVENT> ms  <step>  (<ms since the step before>)", from START EVENT until
        /// the ride is in full view (a player's log shows where the time went).
        internal static void Mark(string step)
        {
            if (s_began < 0) return;
            double now = Time.realtimeSinceStartupAsDouble;
            Debug.Log($"[LoadArena] +{(now - s_began) * 1000:F0} ms  {step}  ({(now - s_markAt) * 1000:F0} ms)");
            s_markAt = now;
        }
        /// The launch's timing lines end (the ride is in full view, or the launch went another way).
        internal static void MarksEnd(string step) { Mark(step); s_began = -1; }

        /// The arena's first frames are drawn.
        internal static void Started(string frames = null)
        {
            if (Phase != "launch") return;
            Phase = "riding"; RideSeconds = (float)(Time.realtimeSinceStartupAsDouble - s_began);
            Debug.Log($"[LoadArena] riding the arena {RideSeconds:F2} s after START EVENT");
            Mark("the ride's first frames are drawn: the black lifts" + (frames != null ? " (their frame times: " + frames + " ms)" : ""));
            OpenTestRideCurtain.Open();
        }

        /// PRESS START (the event is loaded): the black, then the race scene with the event (the loading page over its setup).
        internal static void Enter()
        {
            if (!Riding || Phase == "entering" || Phase == "leaving") return;
            var pre = OpenEventPrestart.Current;
            if (!pre || !pre.Ready) return;
            Phase = "entering"; s_began = -1;
            var ev = s_ev; var setup = s_setup;
            pre.Launched();
            Debug.Log("[LoadArena] " + ev.key + ": START");
            OpenTestRideCurtain.Close(ev.typeName, ev.locationName, () =>
            {
                ReflexGarage.TestDrive.End();
                s_ev = null; s_setup = null;
                OpenLoading.Show(ev, setup);   // (under the black: the event's world is set up behind it)
                setup.Launch();                // (the arena's world goes with the scene, its session is closed on its own thread)
                Phase = "";
                OpenTestRideCurtain.Open();
            });
        }

        /// The event could not load: back to the menu (its session is closed; nothing was saved).
        internal static void Leave(string why)
        {
            if (!Riding || Phase == "leaving" || Phase == "entering") return;
            Phase = "leaving"; s_began = -1;
            Debug.LogWarning("[LoadArena] back to the menu: " + why);
            OpenEventPrestart.Release("the event could not load");
            OpenTestRideCurtain.Close("", "BACK TO THE MENU", () =>
            {
                ReflexGarage.TestDrive.End();
                s_ev = null; s_setup = null; Phase = "";
                OriginalPauseMenu.QuitToFrontEnd();
                OpenTestRideCurtain.Open();
            });
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void Reset() { s_ev = null; s_setup = null; Phase = ""; RideSeconds = -1f; s_began = -1; }   // (domain reload is off)
    }

    /// The race scene's part of the arena (attached by OriginalWorldRuntime when OpenLoadArena.Riding): the card (the event, its bar, the
    /// prompt) over the ride, and START.
    [DefaultExecutionOrder(-210), DisallowMultipleComponent]
    public sealed class OwnLoadArena : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        GameObject host;
        VisualElement root, card, fill, prompt, cover; Label eyebrow, title, line;
        string promptKey, firstFrames = ""; double frameAt; int shownFrames, requestFrame = -1; bool started, lifted, readyShown, failedShown; float readyAt = -1f, nudgeAt = -10f;
        OriginalLevel propLevel; bool propsHidden; int propsChecks;

        /// Tests: the prompt is up.
        public bool PromptShown => prompt != null && prompt.style.display == DisplayStyle.Flex;
        public string Line => line?.text;

        public static OwnLoadArena Attach(OriginalWorldRuntime rt)
        {
            if (!rt) return null;
            var c = rt.GetComponent<OwnLoadArena>() ?? rt.gameObject.AddComponent<OwnLoadArena>();
            c.runtime = rt;
            return c;
        }

        void Awake() { if (!runtime) runtime = GetComponent<OriginalWorldRuntime>(); }

        void Build()
        {
            if (root != null) return;
            OuUi.EnsureEventSystem();
            host = new GameObject("Open Reflex load arena"); host.transform.SetParent(transform, false);
            host.AddComponent<LateMark>();
            root = OuUi.CreateDocument(host, 65); OuTheme.Apply(root);
            root.pickingMode = PickingMode.Ignore;
            // a cover for a ride that could not start (the event still loads under it)
            cover = OuUi.Div("", root); cover.pickingMode = PickingMode.Ignore;
            var vs = cover.style; vs.position = Position.Absolute; vs.left = 0; vs.right = 0; vs.top = 0; vs.bottom = 0; vs.backgroundColor = new Color(0.04f, 0.045f, 0.05f, 1f);
            cover.style.display = DisplayStyle.None;
            // the card bottom-left, in the test ride's card style: the event, its bar, the line, PRESS START
            card = OuUi.Div("", root); card.name = "loadarena-card"; card.pickingMode = PickingMode.Ignore;
            // (top-left: the bottom-left is the game's own (the music's now-playing notice), the top centre its compass, the bottom-right the speed)
            var cs = card.style; cs.position = Position.Absolute; cs.left = 56; cs.top = 48; cs.paddingLeft = 24; cs.paddingRight = 30; cs.paddingTop = 16; cs.paddingBottom = 20;
            cs.minWidth = 520;
            cs.backgroundColor = new Color(0.04f, 0.045f, 0.05f, 0.80f); cs.borderLeftWidth = 5; cs.borderLeftColor = new Color(1f, 0.80f, 0.28f);
            var ev = OpenLoadArena.Event;
            eyebrow = OuUi.Text("", "ou-eyebrow2 ou-label", card); eyebrow.enableRichText = false; eyebrow.name = "loadarena-eyebrow";
            eyebrow.style.color = new Color(1f, 0.80f, 0.28f);
            title = OuUi.Text(ev != null ? ev.locationName.ToUpperInvariant() : "", "ou-stat-value ou-display", card); title.enableRichText = false; title.name = "loadarena-title";
            title.style.fontSize = 52; title.style.color = Color.white; title.style.marginTop = 2; title.style.marginBottom = 8;
            var bar = OuUi.Div("ou-loading-bar", card); bar.style.marginLeft = 0; bar.style.width = 460;
            fill = OuTheme.Backdrop(bar, "ou-loading-bar-fill");
            line = OuUi.Text("", "ou-loading-sub", card); line.name = "loadarena-line"; line.enableRichText = false;
            // (not OuUi.Div("mxa-hints"): a tracked hint bar is placed along the screen's bottom)
            prompt = OuUi.Div("", card); prompt.AddToClassList("mxa-hints"); prompt.AddToClassList("ou-loadarena-prompt"); prompt.name = "loadarena-start";
            prompt.style.display = DisplayStyle.None;
            SetEyebrow("LOADING");
        }

        void SetEyebrow(string state)
        {
            var ev = OpenLoadArena.Event;
            eyebrow.text = state + (ev != null ? "  ·  " + ev.typeName.ToUpperInvariant() + (ev.ev.spec.trackId > 0 ? "  ·  TRACK " + (ev.ev.spec.trackId + 1) : "") : "");
        }

        /// PRESS <the pause button> TO verb, in the caps of the device in use.
        void RenderPrompt(string verb)
        {
            string key = OuGlyphs.Menu;
            promptKey = key + verb;
            prompt.Clear();
            var h = OuUi.Div("mxa-hint", prompt);
            OuUi.Text("PRESS", "mxa-hint-label", h).style.marginRight = 10;
            OuUi.Text(key, "mxa-key " + (OuGlyphs.CapClass(key) ?? ""), h);
            OuUi.Text(verb, "mxa-hint-label", h);
        }

        /// The compound's tutorial furniture is not part of the ride (as OwnTestRide.HideTutorialProps).
        void HideTutorialProps()
        {
            if (propsHidden && ++propsChecks > 120) return;
            if (!propLevel) propLevel = FindAnyObjectByType<OriginalLevel>();
            if (!propLevel || propLevel.Content == null) return;
            int n = OpenTutorialProps.Hide(propLevel.Content);
            if (n > 0 || !propsHidden) propsHidden = true;
        }

        /// The pause button (OriginalPauseMenu routes the game's pause request here too): START once the event is loaded.
        public void PauseRequest()
        {
            if (requestFrame == Time.frameCount) return;
            requestFrame = Time.frameCount;
            var pre = OpenEventPrestart.Current;
            string failure = pre ? pre.Failure : null;
            if (failure != null || !pre && OpenLoadArena.Phase != "entering") { OuSound.Play("Back"); OpenLoadArena.Leave(failure ?? "the event's session is gone"); return; }
            if (pre.Ready && Time.unscaledTime - readyAt >= 0.25f) { OuSound.Play("Accept"); OpenLoadArena.Enter(); return; }
            nudgeAt = Time.unscaledTime;   // (still loading: the line says so)
        }

        static bool Pressed()
        {
            var kb = Keyboard.current;
            if (kb != null && (kb.escapeKey.wasPressedThisFrame || kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame)) return true;
            foreach (var pad in Gamepad.all) if (pad.startButton.wasPressedThisFrame) return true;
            return false;
        }

        void Update()
        {
            if (!runtime || !OpenLoadArena.Riding && OpenLoadArena.Phase != "entering") return;
            Build();
            string rideFailure = runtime.FailureMessage;
            if (rideFailure == null && runtime.SimDriver?.Open is OpenReflexSimulation sim && sim.CurrentPhase == OpenReflexSimulation.Phase.Failed) rideFailure = sim.Failure;
            if (rideFailure != null && cover.style.display == DisplayStyle.None)
            {   // the ride could not start: the event still loads, the card over a plain background (START works as before)
                Debug.LogWarning("[LoadArena] the arena's ride stopped: " + rideFailure);
                cover.style.display = DisplayStyle.Flex;
                OpenLoadArena.Started();
            }
            if (runtime.Running)
            {
                HideTutorialProps();
                if (!started)
                {
                    double now = Time.realtimeSinceStartupAsDouble;
                    // each frame: its time, of it the scripts' part (this Update to the last LateUpdate; the rest is the engine: rendering, uploads)
                    if (shownFrames > 0) firstFrames += (shownFrames > 1 ? ", " : "") + ((now - frameAt) * 1000).ToString("F0") + (LateMark.At > frameAt ? " (scripts " + ((LateMark.At - frameAt) * 1000).ToString("F0") + ")" : "");
                    frameAt = now;
                    if (++shownFrames >= 4) { started = true; OpenLoadArena.Started(firstFrames); }
                }
                if (started && !lifted && !OpenTestRideCurtain.Up) { lifted = true; OpenLoadArena.MarksEnd("the black is gone: the ride is in full view"); }
            }
            var pre = OpenEventPrestart.Current;
            string failure = pre ? pre.Failure : OpenLoadArena.Phase == "entering" ? null : "the event's session is gone";
            if (failure != null)
            {
                if (!failedShown)
                {
                    failedShown = true;
                    SetEyebrow("THE EVENT COULD NOT LOAD"); line.text = failure.ToUpperInvariant(); fill.style.width = Length.Percent(0);
                    prompt.style.display = DisplayStyle.Flex; RenderPrompt("TO GO BACK");
                    OuSound.Play("Locked");
                }
                if (promptKey != OuGlyphs.Menu + "TO GO BACK") RenderPrompt("TO GO BACK");
            }
            else if (pre)
            {
                fill.style.width = Length.Percent(Mathf.Clamp01(pre.Progress) * 100f);
                if (!pre.Ready)
                {
                    int dots = 1 + (int)(Time.unscaledTime * 2.5f) % 3;
                    line.text = (Time.unscaledTime - nudgeAt < 1.2f ? "STILL LOADING" : OpenContent.Str("STR_LOADING", "LOADING").ToUpperInvariant()) + new string('.', dots) + "  ·  RIDE THE COMPOUND MEANWHILE";
                }
                else
                {
                    if (!readyShown)
                    {
                        readyShown = true; readyAt = Time.unscaledTime;
                        SetEyebrow("READY"); line.text = "EVENT LOADED";
                        prompt.style.display = DisplayStyle.Flex; RenderPrompt("TO START");
                        OuSound.Play("PageIn");
                        Debug.Log($"[LoadArena] {pre.EventKey}: PRESS START ({(pre.ReadySeconds >= 0 ? pre.ReadySeconds : pre.Age):F2} s after START EVENT)");
                    }
                    if (promptKey != OuGlyphs.Menu + "TO START") RenderPrompt("TO START");
                    prompt.style.opacity = OuMotion.Reduced ? 1f : 0.62f + 0.38f * (0.5f + 0.5f * Mathf.Cos((Time.unscaledTime - readyAt) * 3.2f));
                }
            }
            card.style.display = OpenLoadArena.Phase == "entering" || OpenLoadArena.Phase == "leaving" ? DisplayStyle.None : DisplayStyle.Flex;
            if (OpenLoadArena.Phase != "riding" && !(OpenLoadArena.Phase == "launch" && cover.style.display == DisplayStyle.Flex)) return;
            if (ReflexOptions.IsOpen || ReflexOptions.ClosedFrame == Time.frameCount) return;
            if (Pressed()) PauseRequest();
        }

        void OnDestroy() { if (host) Destroy(host); }

        /// The end of the frame's scripts (the last LateUpdate): the first frames' timing line splits each frame there.
        [DefaultExecutionOrder(32000)]
        sealed class LateMark : MonoBehaviour
        {
            public static double At;
            void LateUpdate() { At = Time.realtimeSinceStartupAsDouble; }
        }
    }
}
