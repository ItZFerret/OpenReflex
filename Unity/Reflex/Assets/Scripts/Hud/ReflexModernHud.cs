// The MODERN race HUD (OPTIONS > CAMERA & HUD > HUD STYLE MODERN; the pause menus' HUD STYLE), in Open Unleashed's MxaRaceHud look
// (MxaHudKit's panels and type). This component drives it: it fills the shared ReflexHudModel from the running game every frame and
// hands it to ReflexHudView (ReflexHudView.cs / .Modes.cs: the layout, the animations; the gallery drives the same view with
// synthetic models). What it shows, per mode, is in the view's header and the model's (ReflexHudModel.HudMode): lap races (Race,
// Waypoint, Terracross, Powercross), Eliminator, Overtake, Outrun, Section Race, Freestyle, Free Ride and its challenges, with the
// position / field board, the timers, the speed and gear gauge, the countdowns, the warnings and the mode's own counters.
// ORIGINAL keeps the game's own HUD exactly as before (OriginalLiveUi's live layers, or OriginalHud's hand-built fallback). MODERN
// hides the game's HUD page layers (a CanvasGroup at alpha 0 on the live layer views of the "... Single Player Page"; OriginalHud's
// canvas) and shows this one, in every mode it covers (ReflexHudModel.Covered: a Tutorial, and a live Free Ride with the game's own
// Hit Discovery page, keep the game's HUD: OriginalHud.ClientHides asks CoversMode); the post-race pages, popups and the pause page
// stay the game's. Switched live. H hides the HUD (both styles) for the session, unless the game binds H (its remap table).
using System.Collections.Generic;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    [DisallowMultipleComponent, DefaultExecutionOrder(900)]
    public sealed class ReflexModernHud : MonoBehaviour
    {
        OriginalWorldRuntime rt; OriginalLiveUi live;
        public readonly ReflexHudModel Model = new ReflexHudModel();
        ReflexHudView view;
        OriginalFreeRideHud originalFreeRide;   // HUD STYLE ORIGINAL: the install's Free Ride / challenge layers (compass, timer, Hit Discovery ...; the open core has no page of the game's there)
        Canvas canvas;   // the view's canvas (the career flow's probe reads this field)

        /// The MODERN HUD is on screen.
        public bool Shown => canvas && canvas.enabled && fullHud;
        public bool SpeedometerShown => canvas && canvas.enabled && presentation != 0;
        bool fullHud; int presentation;
        /// The game's live HUD layers are hidden by the style / H (tests).
        public bool LiveHudHidden { get; private set; }
        /// The game's HUD page shows in the live UI this frame (its "... Single Player Page" layers).
        public bool LiveHudPage { get; private set; }
        /// HUD STYLE ORIGINAL's Free Ride / challenge layers (tests; null until first shown).
        public OriginalFreeRideHud OriginalFreeRide => originalFreeRide;
        /// The view (the gallery and the tests read its options and counters).
        public ReflexHudView View => view;
        /// The bottom of the MODERN HUD's right column on screen (pixels from the top), 0 while the HUD or the column is not shown.
        public float RightColumnBottomPx => Shown && view != null ? view.RightColumnBottomPx : 0f;
        /// The left column's bottom (screen px from the top; 0 none), for PRACTICE's lap list (w-lb-practice).
        public float LeftColumnBottomPx => Shown && view != null ? view.LeftColumnBottomPx : 0f;

        /// The MODERN HUD has a layout for the race that runs now (a mode it does not cover keeps the game's HUD: OriginalHud.ClientHides).
        public static bool CoversMode { get; private set; } = true;

        float optionsAt = -1f;

        void Awake() { rt = GetComponent<OriginalWorldRuntime>(); }

        void Build()
        {
            if (view != null) return;
            view = ReflexHudView.Create(transform, "Modern HUD", 11);
            canvas = view.Canvas;
            view.Options.FromSettings();
        }

        // ------------------------------------------------------------------------------------------------ per frame
        void Update()
        {
            // H: the HUD hidden / shown (a port extra; the key stays the game's when its remap table binds it)
            var kb = Keyboard.current;
            if (kb != null && kb.hKey.wasPressedThisFrame && rt && rt.Running && !MenuOpen && !OriginalOnboardAdjust.KeyboardCaptured && !OriginalInput.GameBinds(Key.H))
                ReflexSettings.HudHidden = !ReflexSettings.HudHidden;
        }

        bool MenuOpen => ReflexOptions.IsOpen || rt && (rt.OwnMenuActive || rt.pauseMenu && rt.pauseMenu.IsOpen) || OriginalPostRace.Active;

        void LateUpdate() { var am = Reflex.Port.Platform.ReflexAllocMeter.Begin(); LateUpdateMetered(); Reflex.Port.Platform.ReflexAllocMeter.End(Reflex.Port.Platform.ReflexAllocMeter.Hud, am); }   // (the perf line's alloc share, w-gcspikes)
        void LateUpdateMetered()
        {
            if (!rt) return;
            if (!live) live = rt.GetComponent<OriginalLiveUi>();
            float real = Time.unscaledDeltaTime;
            FillModel(real);
            bool modern = ReflexSettings.HudStyle == HudStyle.Modern;
            bool liveHud = live && live.enabled && live.Running && !(live.race && live.Unavailable);
            LiveHudPage = liveHud && HudPageShown();
            // The modes the MODERN HUD lays out. The legacy plugin's live Free Ride page carries the game's own Hit Discovery component and stays the game's
            // there; the open core exports the discovery counts (rx_session_race_hud), which this HUD draws. A Tutorial's prompts are the game's.
            bool covered = Model.Covered && !(liveHud && Model.mode == HudMode.FreeRide);
            CoversMode = covered || Model.mode == HudMode.None;
            bool unobstructed = rt.Running && !Model.inPost && !OriginalPostRace.Active && !rt.IntroCardShown && rt.OriginalHudVisible && !ReflexSettings.HudHidden && !(rt.hovercraft && rt.hovercraft.Driving)
                                && !MenuOpen && (!liveHud || LiveHudPage);
            presentation = Presentation(modern, ReflexSettings.ClassicSpeedometer, covered, unobstructed);
            bool show = presentation != 0;
            fullHud = presentation == 2;
            // A gauge-only overlay must never suppress the classic race display.
            bool hideLive = liveHud && (modern && covered || ReflexSettings.HudHidden);
            if (hideLive != LiveHudHidden || hideLive) HideLiveHud(hideLive);
            if (show) Build();
            if (view != null)
            {
                if (show && !view.Visible) view.Visible = true;
                if (Time.unscaledTime >= optionsAt) { optionsAt = Time.unscaledTime + 0.25f; view.Options.FromSettings(); }
                if (view.Visible)
                {
                    view.Apply(Model, real, presentation);
                    // fully faded out: off the canvas
                    if (!show && view.GroupsShown == 0) view.Visible = false;
                }
            }
            // Gauge-only mode preserves the classic HUD, except its duplicate
            // authored numeric speed. Use actual canvas visibility, not just a preference.
            if (rt.originalHud) rt.originalHud.SetLegacySpeedometerHidden(SpeedometerShown);
            TickOriginalFreeRide(modern, unobstructed && !liveHud, real);
        }

        // A fill that throws (bad install data) must not stop the HUD: the clock, the lap and the rest still draw from what was filled, and the
        // error is logged once per message instead of every frame (a German install's missing trick names froze the timer and the lap, 2026-11-05).
        string fillError;
        void FillModel(float real)
        {
            try { Model.Fill(rt, rt.paused ? 0f : real); }
            catch (System.Exception e)
            {
                if (e.Message != fillError) { fillError = e.Message; Debug.LogException(e); }
            }
        }

        // HUD STYLE ORIGINAL in the open core's Free Ride, its challenges and the Waypoint race (its compass only): the install's layers (OriginalFreeRideHud): the compass (the camera's
        // heading; the challenge areas, the targets or the finish from rx_session_compass), the event timer, the counter ("n/m" for 3 s on a find / a
        // hit), the Hill Climb's height. A live Free Ride page (the legacy emulator's UI) carries the components itself; MODERN has its own panels.
        void TickOriginalFreeRide(bool modern, bool unobstructed, float dt)
        {
            var page = OriginalFreeRideHud.Page.None;
            if (Model.mode == HudMode.FreeRide) page = OriginalFreeRideHud.Page.FreeRide;
            else if (Model.mode == HudMode.Waypoint) page = OriginalFreeRideHud.Page.Waypoint;   // its compass: the way to the next gate (the race HUD is OriginalHud's)
            else if (Model.mode == HudMode.Challenge)
                page = Model.challengeKind == 1 ? OriginalFreeRideHud.Page.HillClimb : Model.challengeKind == 6 ? OriginalFreeRideHud.Page.Trailblazer
                     : Model.challengeKind == 8 ? OriginalFreeRideHud.Page.TargetPractice : OriginalFreeRideHud.Page.FreeRide;
            bool allowed = !modern && unobstructed && page != OriginalFreeRideHud.Page.None;
            if (originalFreeRide == null)
            {
                if (!allowed) return;
                originalFreeRide = new OriginalFreeRideHud(transform);
            }
            var cam = rt.chaseCamera ? rt.chaseCamera.transform : null;
            // the EventTimerComponent (MXUITimerComponent 005f7140) shows in every activity but the Free Ride's (UI kind 8): the time left, or the clock of an
            // activity that counts up (Trailblazer): the challenge's clock, whether or not it has a limit
            bool timerShown = Model.timerShown; float timerValue = Model.timerValue;
            if (Model.mode == HudMode.Challenge && rt.HasRaceHud)
            {
                var rh = rt.RaceHud;
                timerShown = true; timerValue = rh.clockCountsUp != 0 ? rh.clock : Mathf.Max(0f, rh.clockLimit - rh.clock);
            }
            var f = new OriginalFreeRideHud.Frame
            {
                dt = dt, allowed = allowed, page = page,
                viewPosition = cam ? cam.position : Vector3.zero, viewForward = cam ? cam.forward : Vector3.forward,
                compass = rt.Compass, hasCompass = rt.HasCompass,
                discoveryFound = Model.discoveryFound, discoveryTotal = Model.discoveryTotal, discoverySerial = Model.discoverySerial,
                timerShown = timerShown, timerValue = timerValue,
                valueValid = Model.mode == HudMode.Challenge && Model.challengeValueValid, value = Model.challengeValue,
                metric = ReflexSettings.Metric,
            };
            originalFreeRide.Tick(f);
        }

        // 0 hidden, 1 supplemental gauge, 2 full modern race HUD. `covered`: the MODERN HUD lays out the race that runs.
        internal static int Presentation(bool modern, bool classicGauge, bool covered, bool unobstructed)
            => !unobstructed ? 0 : modern && covered ? 2 : !modern && classicGauge ? 1 : 0;

        // ------------------------------------------------------------------------------------------------ the game's live HUD layers
        readonly HashSet<string> hudLayers = new HashSet<string>(System.StringComparer.Ordinal);
        readonly List<CanvasGroup> hidden = new List<CanvasGroup>();
        Transform liveRoot;

        bool HudPageShown()
        {
            var f = live.Last; if (f == null) return false;
            bool any = false;
            hudLayers.Clear();
            for (int i = 0; i < f.layers.Count; i++)
            {
                var l = f.layers[i];
                if ((l.state & 0x10) == 0 || l.page == null || !l.page.EndsWith("Single Player Page", System.StringComparison.Ordinal) || l.name == "PostEvent_ActivityDashboard") continue;
                hudLayers.Add(l.name); any = true;
            }
            return any;
        }

        void HideLiveHud(bool hide)
        {
            LiveHudHidden = hide;
            if (!liveRoot && live)
            {
                foreach (var c in live.GetComponentsInChildren<Canvas>(true))
                    if (c.name.StartsWith("Original UI (live)", System.StringComparison.Ordinal)) { liveRoot = c.transform.childCount > 0 ? c.transform.GetChild(0) : null; break; }
            }
            if (!liveRoot) return;
            for (int i = 0; i < liveRoot.childCount; i++)
            {
                var t = liveRoot.GetChild(i);
                bool h = hide && hudLayers.Contains(t.name);
                var g = t.GetComponent<CanvasGroup>();
                if (h && !g) { g = t.gameObject.AddComponent<CanvasGroup>(); g.interactable = false; g.blocksRaycasts = false; hidden.Add(g); }
                if (g && hidden.Contains(g)) { float a = h ? 0f : 1f; if (g.alpha != a) g.alpha = a; }
            }
        }

        void OnDisable()
        {
            CoversMode = true;
            if (rt && rt.originalHud) rt.originalHud.SetLegacySpeedometerHidden(false);
            if (LiveHudHidden) HideLiveHud(false);
            if (view != null) view.Visible = false;
            originalFreeRide?.Hide();
        }

        void OnDestroy() { CoversMode = true; originalFreeRide?.Dispose(); }
    }
}
