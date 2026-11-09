using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.Hud;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    // The pause page of every offline mode (the open core's worlds): National, Supercross, Terracross, Powercross, Waypoint, Freestyle, Section Race,
    // Free Ride and its challenges, and the series events of the open career. The rows are PauseMenu.lua's for the mode (OwnModes: no RESTART in
    // the Free Ride; VEHICLE TUNING, VEHICLE LIST and the map in it), the tiles the mode's own numbers. Start / Esc opens it and Start / F3 closes it;
    // the d-pad / left stick / arrows move, A / Enter accept, B / Esc go back (OuMenuInput / OuNav, as every menu), so a gamepad drives all of it.
    // OriginalPauseMenu (the install's art) stays only as the fallback of a world this page does not cover (a MotoSkills tutorial, the garage's test
    // ride has its own page).
    [DefaultExecutionOrder(-200), DisallowMultipleComponent]
    public sealed class OwnRacePause : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        GameObject host;
        VisualElement root, layer, body, hints, overlay;
        OuMenuInput input;
        OuNav nav;
        FeAssets trickAssets;
        readonly ReflexHudModel model = new ReflexHudModel();
        OwnPauseSpec spec;
        bool waiting, open, pausedBefore, audioBefore, closing;
        float waitStarted;
        int openedFrame = -1, closedFrame = -1;
        string page;
        bool holdHover; Vector2 hoverOrigin;
        readonly List<string> rowNames = new List<string>();
                public bool Active => waiting || open;
        public bool IsOpen => open;
        public VisualElement Root => root;
        public string Page => page;
        public int TrickCount { get; private set; }
        /// The rows of the main page, in order (their element names), for the showcase and the tests.
        public IReadOnlyList<string> RowNames => rowNames;
        public OwnPauseSpec Spec => spec;
        public bool OwnsInput => Active || CanOpen || closedFrame == Time.frameCount;
        public bool CanOpen => runtime && runtime.Running && runtime.SimDriver?.Mode == ReflexSimMode.Open
            && !runtime.paused && !runtime.OwnResultsPresentation && !OriginalPostRace.Active && !ReflexGarage.TestDrive.Active
            && !(runtime.pauseMenu && runtime.pauseMenu.IsOpen)
            && !(runtime.TryGetComponent<OriginalCareerRace>(out var career) && career.RulesOpen)
            && !(runtime.EventIdentity?.StartsWith("Tutorial/", StringComparison.Ordinal) ?? false)
            && !(runtime.TryGetComponent<OwnActivationArea>(out var area) && area.ClaimsPause)
            && !(runtime.preChallenge && runtime.preChallenge.Active) && !(runtime.postChallenge && runtime.postChallenge.Active)
            && Supports(runtime.EventUi);

        /// Why the pause cannot open now (diagnostics: the probe and the player log), null when it can.
        public string WhyNot()
        {
            if (!runtime) return "no runtime";
            if (!runtime.Running) return "world not running";
            if (runtime.SimDriver?.Mode != ReflexSimMode.Open) return "not the open core";
            if (runtime.paused) return "the runtime is paused";
            if (runtime.OwnResultsPresentation) return "the results are up";
            if (OriginalPostRace.Active) return "the legacy post-race pages are up";
            if (ReflexGarage.TestDrive.Active) return "a test drive";
            if (runtime.pauseMenu && runtime.pauseMenu.IsOpen) return "the original pause menu is open";
            if (runtime.TryGetComponent<OriginalCareerRace>(out var career) && career.RulesOpen) return "the career rules popup";
            if (runtime.EventIdentity?.StartsWith("Tutorial/", StringComparison.Ordinal) ?? false) return "a tutorial";
            if (runtime.TryGetComponent<OwnActivationArea>(out var area) && area.ClaimsPause) return "an activation area claims the pause key";
            if (runtime.preChallenge && runtime.preChallenge.Active || runtime.postChallenge && runtime.postChallenge.Active) return "a challenge popup";
            var ui = runtime.EventUi;
            if (!Supports(ui)) return $"event ui not supported (known {ui.known}, type {ui.activity_type}, state {ui.race_state}, rules {ui.rules_hold})";
            return null;
        }

        // The pause is the game's own App.AllowPause window: the start countdown and the event in progress (the intro and the outro pages turn
        // AllowPause off), rules dismissed, one of the offline modes (OwnModes.PauseType). Unknown native Lua AllowPause is never interpreted as
        // permission for other lifecycle windows.
        public static bool Supports(RxEventUiState state)
        {
            const RxEventUiKnown required = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState | RxEventUiKnown.RulesHold;
            return (state.known & required) == required && OwnModes.PauseType(state.activity_type)
                && (state.race_state == 1 || state.race_state == 2) && state.rules_hold == 0
                && ((state.known & RxEventUiKnown.PauseOwner) == 0 || state.native_pause_owner == 0)
                && ((state.known & RxEventUiKnown.AllowPause) == 0 || state.native_allow_pause == 1);
        }

        void Awake() { runtime = GetComponent<OriginalWorldRuntime>(); if (!GetComponent<PhysicsStudioBadge>()) gameObject.AddComponent<PhysicsStudioBadge>(); if (!GetComponent<PhysicsStudioOverlay>()) gameObject.AddComponent<PhysicsStudioOverlay>(); }   // PHYSICS STUDIO: the race badge and the live editor
        static bool PausePressed()
        {
            if (Keyboard.current?.escapeKey.wasPressedThisFrame == true) return true;
            foreach (var pad in Gamepad.all) if (pad.startButton.wasPressedThisFrame) return true;
            return false;
        }
        static bool ResumePressed()
        {
            if (Keyboard.current?.f3Key.wasPressedThisFrame == true) return true;
            foreach (var pad in Gamepad.all) if (pad.startButton.wasPressedThisFrame) return true;
            return false;
        }

        void Update()
        {
            if (holdHover && Mouse.current != null && (Mouse.current.position.ReadValue() - hoverOrigin).sqrMagnitude > 1f) holdHover = false;
            if (Active && (!runtime || !runtime.Running)) { CloseForWorldChange(); return; }
            if (waiting)
            {
                if (runtime.TrySettleMenuPause())
                {
                    if (!Supports(runtime.EventUi)) { Close(); return; }
                    waiting = false; open = true; openedFrame = Time.frameCount;
                    try { Build(); ShowMain(); } catch (Exception e) { Debug.LogException(e); Close(); }
                }
                else if (Time.unscaledTime - waitStarted > 2f) Close();
                return;
            }
            if (ReflexOptions.IsOpen)
            {
                if (open && Keyboard.current?.f4Key.wasPressedThisFrame == true) ReflexOptions.Close();
                return;
            }
            if (ReflexOptions.ClosedFrame == Time.frameCount) return;
            if (PhysicsStudioOverlay.Modal) return;   // PHYSICS STUDIO: the live editor has the pad (ShowPhysicsStudio)
            if (Reflex.Port.Replay.ReflexReplay.ViewerOpen || Reflex.Port.Replay.ReflexReplay.ClosedFrame == Time.frameCount) return;   // REPLAY / PHOTO MODE over the page
            if (!open)
            {
                if (closedFrame != Time.frameCount && CanOpen && PausePressed()) Open();
                return;
            }
            if (openedFrame == Time.frameCount) return;
            if (ResumePressed()) { Close(); return; }
            if (Keyboard.current?.f4Key.wasPressedThisFrame == true) { ShowOptions(); return; }
            nav.Update(input);
        }

        public void Open()
        {
            if (Active || !CanOpen || ReflexOptions.IsOpen || ReflexOptions.ClosedFrame == Time.frameCount) return;
            runtime.GetComponent<OriginalOnboardAdjust>()?.End();
            pausedBefore = runtime.paused; audioBefore = AudioListener.pause;
            waiting = true; waitStarted = Time.unscaledTime;
            runtime.paused = true; runtime.ClearMenuInput();
            AudioListener.pause = true;
            Reflex.Port.Platform.ReflexCursor.Want(this, true);   // shown for a mouse player only; a pad player's stays hidden where it is
        }

        void Build()
        {
            if (root != null) return;
            OuUi.EnsureEventSystem();
            host = new GameObject("Open Reflex race pause"); host.transform.SetParent(transform, false);
            root = OuUi.CreateDocument(host, 70); OuTheme.Apply(root);
            var style = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuRacePause");
            if (style) root.styleSheets.Add(style);
            layer = OuUi.Div("mxa-layer mxa-dim ou-race-pause", root);
            body = OuUi.Div("ou-pause-body", layer);
            // A newly rebuilt page must not move focus beneath a stationary mouse.
            body.RegisterCallback<PointerEnterEvent>(e => { if (holdHover) e.StopImmediatePropagation(); }, TrickleDown.TrickleDown);
            overlay = OuUi.Div("ou-overlay", layer); overlay.pickingMode = PickingMode.Ignore;
            overlay.RegisterCallback<PointerEnterEvent>(e => { if (holdHover) e.StopImmediatePropagation(); }, TrickleDown.TrickleDown);
            hints = OuUi.Div("mxa-hints", root);
            nav = new OuNav { Screen = layer, OnCancel = Back };
        }

        void ResetPage(string name, string title, string caption)
        {
            page = name; holdHover = true; hoverOrigin = Mouse.current?.position.ReadValue() ?? Vector2.zero; body.Clear(); overlay.Clear(); overlay.pickingMode = PickingMode.Ignore; overlay.RemoveFromClassList("ou-in");
            root.RemoveFromClassList("ou-pause-hidden");
            input?.Dispose(); input = new OuMenuInput();
            nav.OnAlt = null;
            if (name == "main" || name == "tricks" || name == "vehicles" || name == "tuning") OuTheme.Header(body, title, caption);
            OuUi.Hints(hints, (OuUi.KeySubmit, name == "tuning" ? "CHANGE" : "SELECT"), (OuUi.KeyCancel, name == "main" ? "RESUME" : "BACK"));
            openedFrame = Time.frameCount;
        }
        void Focus(VisualElement target)
        {
            nav.DefaultItem = () => target;
            body.schedule.Execute(() => { if (open && !ReflexOptions.IsOpen && target.panel != null) nav.Focus(target); });
        }
        VisualElement Button(string label, string name, Action action, VisualElement parent)
        {
            var button = OuUi.Button(label, "mxa-main-button", action, parent); button.name = name; return button;
        }

        void ShowMain(string focusName = "pause-resume")
        {
            if (!open) return;
            // the mode's page from the paused world: the HUD model filled from its last frame and the core's mode values (the showcase has no
            // world: its page stays as it was built)
            if (runtime) { model.Fill(runtime, 0f); spec = OwnModes.Pause(model, runtime.Race, OwnModes.PauseContext(runtime, model)); }
            if (spec != null) BuildMain(spec, focusName);
        }

        // (the page of a spec: the showcase builds it without a world, from synthetic data)
        internal void BuildMain(OwnPauseSpec value, string focusName)
        {
            spec = value;
            ResetPage("main", spec.title, spec.caption);
            var content = OuUi.Div("ou-page", body);
            var side = OuUi.Div("ou-page-side ou-pause-side", content);
            // where the event stands: the mode's own numbers, as tiles
            var tiles = spec.tiles.Select(t => (t.label, t.value, t.tone)).ToList();
            if (RxPhysicsStudio.EventActive) tiles.Add(("PHYSICS", "MODIFIED", "gold"));   // PHYSICS STUDIO: this event rides a changed config
            OuWidgets.StatGrid(side, tiles);
            // the rows scroll when the column cannot hold them (more tiles, a short window, a large UI scale): the focused row is kept in view
            var menu = PauseMenuColumn(side);
            rowNames.Clear();
            var resume = Row(menu, "RESUME", "pause-resume", Close); resume.AddToClassList("ou-primary");
            if (spec.restart) Row(menu, "RESTART", "pause-restart", () => Confirm(true));
            if (spec.resetTrack) Row(menu, "RESET TRACK", "pause-reset-track", ResetTrack);   // (w-fb-hud) the ruts and tyre tracks gone, the ride goes on
            if (Reflex.Port.Replay.ReflexReplay.Offered(runtime)) { Row(menu, "REPLAY", "pause-replay", () => OpenViewer(false)); Row(menu, "PHOTO MODE", "pause-photo", () => OpenViewer(true)); }
            if (spec.vehicleTuning) Row(menu, "VEHICLE TUNING", "pause-tuning", ShowTuning);
            if (spec.vehicleList) Row(menu, "VEHICLE LIST", "pause-vehicles", () => { prosShown = false; ShowVehicles(); });
            Row(menu, "TRICK LIBRARY", "pause-tricks", ShowTricks);
            if (PhysicsStudioOverlay.Available) Row(menu, "PHYSICS STUDIO", "pause-physics", ShowPhysicsStudio);   // PHYSICS STUDIO (w-physstudio2): the live editor
            Row(menu, "OPTIONS", "pause-options", ShowOptions);
            if (spec.retireLabel != null) Row(menu, spec.retireLabel, "pause-retire", ConfirmRetire);   // RESTORED CONTENT: a championship stage (ChampWeekendFlow)
            Row(menu, spec.quitLabel, "pause-quit", () => Confirm(false));
            OuMotion.Enter(side, 0.04f, 0.02f, true);
            if (spec.map) BuildMap(content);
            else if (!string.IsNullOrEmpty(spec.challengeTitle)) BuildChallenge(content);
            Focus(menu.Q<VisualElement>(focusName) ?? resume);
        }
        /// The scrolling column of a pause page's rows (returns the content the rows go into): it takes the height the tiles leave, so the page's rows can
        /// never run under the footer, and the navigation (OuNav.Follow) scrolls to the focused row. Shared with the test ride's pause.
        internal static VisualElement PauseMenuColumn(VisualElement side)
        {
            var scroll = new ScrollView(ScrollViewMode.Vertical) { name = "pause-menu-scroll" };
            scroll.AddToClassList("ou-pause-menu-scroll");
            scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            side.Add(scroll);
            var menu = scroll.contentContainer;
            menu.AddToClassList("ou-pause-menu-list"); menu.AddToClassList("mxa-pause-menu");
            return menu;
        }
        VisualElement Row(VisualElement menu, string label, string name, Action action) { rowNames.Add(name); return Button(label, name, action, menu); }

        // ------------------------------------------------------------------------------------------------ the Free Ride's map / the challenge's description

        static Texture2D arrow;
        // an arrowhead pointing up (the tip at the top centre, a notch in the base), white: tinted by the style
        static Texture2D Arrow()
        {
            if (arrow) return arrow;
            const int n = 64;
            var t = new Texture2D(n, n, TextureFormat.RGBA32, false) { name = "ou_pause_arrow", wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            var px = new Color32[n * n];
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++)
                {
                    float u = (x + 0.5f) / n * 2f - 1f, v = (y + 0.5f) / n;   // v: 0 bottom .. 1 top
                    float half = Mathf.Lerp(0.8f, 0f, Mathf.InverseLerp(0.06f, 0.96f, v));   // the width at this height: widest at the base
                    float d = half - Mathf.Abs(u);
                    bool inside = v > 0.06f && v < 0.96f && d > 0f;
                    if (v < 0.3f && Mathf.Abs(u) < (0.3f - v) * 1.4f) inside = false;        // the notch in the base
                    px[y * n + x] = inside ? new Color32(255, 255, 255, (byte)(255 * Mathf.Clamp01(d * 18f + 0.2f))) : new Color32(255, 255, 255, 0);
                }
            t.SetPixels32(px); t.Apply(false, true);
            return arrow = t;
        }

        // RefreshFreeRideMap: the location's map picture with one arrow at ((x + 2048) / 4096, 1 - (z + 2048) / 4096) of it, turned by the rider's heading
        void BuildMap(VisualElement content)
        {
            var panel = OcUi.Panel(content, "FREE RIDE MAP", out var panelBody, out _, "ou-pause-aside ou-pause-map");
            panel.name = "pause-map";
            var square = OuUi.Div("ou-pause-map-img", panelBody); square.name = "pause-map-image";
            var tex = OpenContent.Icon(spec.mapIcon);
            if (tex) square.style.backgroundImage = tex;
            else OuUi.Text("The map is not in this install.", "mxa-note", square);
            if (spec.hasBike)
            {
                var arrowEl = OuUi.Div("ou-pause-map-arrow", square); arrowEl.name = "pause-map-rider";
                arrowEl.pickingMode = PickingMode.Ignore;
                arrowEl.style.backgroundImage = Arrow();
                arrowEl.style.unityBackgroundImageTintColor = new Color(60 / 255f, 161 / 255f, 176 / 255f, 1f);   // the exclusive device's rider (PauseMenu.lua)
                float fx = Mathf.Clamp01((spec.bikeX + 2048f) / 4096f), fy = Mathf.Clamp01(1f - (spec.bikeZ + 2048f) / 4096f);
                arrowEl.style.left = Length.Percent(fx * 100f); arrowEl.style.top = Length.Percent(fy * 100f);
                arrowEl.style.rotate = new Rotate(new Angle(spec.heading, AngleUnit.Degree));
            }
            OuMotion.Enter(panel, 0.1f, false);
        }

        // ChallengeEventContainer: the activated challenge's title and description (STR_<ACTIVITY>_TITLE / _DESCRIPTION)
        void BuildChallenge(VisualElement content)
        {
            var panel = OcUi.Panel(content, spec.challengeTitle.ToUpperInvariant(), out var panelBody, out _, "ou-pause-aside ou-pause-challenge");
            panel.name = "pause-challenge";
            OuUi.Text(spec.challengeText ?? "", "ou-lead", panelBody).enableRichText = false;
            OuMotion.Enter(panel, 0.1f, false);
        }

        bool Back()
        {
            if (page == "main") Close();
            else if (page == "vehicle-confirm") ShowVehicles(pendingChoice?.key);
            else ShowMain(page == "tricks" ? "pause-tricks" : page == "restart" ? "pause-restart" : page == "retire" ? "pause-retire" : page == "vehicles" ? "pause-vehicles" : page == "tuning" ? "pause-tuning" : "pause-quit");
            return true;
        }

        // RESTORED CONTENT (championship weekend): RETIRE FROM <STAGE>: the stage ends with you last, the weekend card follows (ChampWeekendFlow.Retire)
        void ConfirmRetire()
        {
            var s = spec ?? new OwnPauseSpec();
            ResetPage("retire", s.retireTitle, s.retireLead);
            var area = OuWidgets.Dialog(overlay);
            OuUi.Text(s.retireLead, "ou-eyebrow2 ou-label", area);
            OuUi.Text(s.retireTitle, "ou-dialog-title ou-display", area);
            var row = OuUi.Div("ou-button-row", area);
            var cancel = Button("KEEP RIDING", "pause-cancel", () => ShowMain("pause-retire"), row);
            var go = Button("RETIRE", "pause-confirm", () => { var rt = runtime; Close(); ChampWeekendFlow.Retire(rt); }, row);
            OuWidgets.LinkRow(new[] { cancel, go });
            overlay.pickingMode = PickingMode.Position;
            OuMotion.Fade(overlay);
            Focus(cancel);
        }

        void Confirm(bool restart)
        {
            var s = spec ?? new OwnPauseSpec();
            ResetPage(restart ? "restart" : "quit", restart ? "RESTART EVENT?" : s.quitTitle, restart ? s.restartLead : s.quitLead);
            var area = OuWidgets.Dialog(overlay);
            OuUi.Text(restart ? s.restartLead : s.quitLead, "ou-eyebrow2 ou-label", area);
            OuUi.Text(restart ? "RESTART EVENT?" : s.quitTitle, "ou-dialog-title ou-display", area);
            var row = OuUi.Div("ou-button-row", area);
            var cancel = Button("KEEP RIDING", "pause-cancel", () => ShowMain(restart ? "pause-restart" : "pause-quit"), row);
            var go = Button(restart ? "RESTART" : s.quitLabel, "pause-confirm", () =>
            {
                Close();
                if (restart) runtime.RestartEvent();
                else if (ChampWeekendFlow.InStage) ChampWeekendFlow.QuitWeekend();   // RESTORED CONTENT: QUIT WEEKEND (the weekend ends, the menu)
                else if (!OwnActivationArea.QuitToFreeRide(runtime)) OriginalPauseMenu.QuitToFrontEnd();   // a challenge's quit goes back to its Free Ride
            }, row);
            OuWidgets.LinkRow(new[] { cancel, go });
            overlay.pickingMode = PickingMode.Position;
            OuMotion.Fade(overlay);
            Focus(cancel);
        }
        // REPLAY / PHOTO MODE (Reflex.Port.Replay.ReflexReplay): the page steps aside for the viewer and comes back on the same row
        void OpenViewer(bool photo)
        {
            var replay = Reflex.Port.Replay.ReflexReplay.Of(runtime);
            if (!open || !replay) return;
            string row = photo ? "pause-photo" : "pause-replay";
            root.AddToClassList("ou-pause-hidden"); input?.Dispose(); input = null;
            bool opened = replay.Open(photo ? Reflex.Port.Replay.ReflexReplay.View.Photo : Reflex.Port.Replay.ReflexReplay.View.Replay,
                                      Reflex.Port.Replay.ReflexReplay.Origin.Pause, () => { if (open && !closing) ShowMain(row); });
            if (!opened) ShowMain(row);
            else if (photo) replay.OpenedPhotoDirect();
        }

        // RESET TRACK (w-fb-hud, ReflexTrackReset): armed for the next frame, then back to the ride (the rider stays where he is)
        void ResetTrack()
        {
            if (!open || !runtime) return;
            if (ReflexTrackReset.Reset(runtime)) Close();
        }

        void ShowOptions()
        {
            if (!open || ReflexOptions.IsOpen) return;
            root.AddToClassList("ou-pause-hidden"); input?.Dispose(); input = null;
            ReflexOptions.Open(ReflexOptions.CameraHud, () => { if (open && !closing) ShowMain("pause-options"); });
        }

        // PHYSICS STUDIO (w-physstudio2, PhysicsStudioOverlay): the live editor over the paused race; BACK returns here
        void ShowPhysicsStudio()
        {
            if (!open || ReflexOptions.IsOpen) return;
            root.AddToClassList("ou-pause-hidden"); input?.Dispose(); input = null;
            PhysicsStudioOverlay.OpenFromPause(() => { if (open && !closing) ShowMain("pause-physics"); });
        }

        void ShowTricks()
        {
            ResetPage("tricks", "TRICK LIBRARY", "GESTURES AND TIERS");
            var back = Button("BACK", "tricks-back", () => ShowMain("pause-tricks"), body);
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("ou-trick-scroll"); body.Add(scroll);
            TrickCount = 0;
            try
            {
                var names = OriginalUiAssets.Data.Strings("InGame", "MXRaven_Stunts");
                trickAssets ??= new FeAssets(null) { tables = FeTables.Shared };
                var seen = new HashSet<string>();
                foreach (var stunt in CoreStuntCatalog.LoadAll())
                {
                    if (string.IsNullOrEmpty(stunt.key) || !seen.Add(stunt.key)) continue;
                    var row = OuUi.Div("ou-trick-row", scroll.contentContainer);
                    row.name = "trick-" + TrickCount++; OuUi.Item(row, null);
                    OuUi.Text("TIER " + stunt.tier, "ou-trick-tier", row);
                    OuUi.Text(names.TryGetValue(stunt.key, out var label) ? label : stunt.name.Replace('_', ' '), "ou-trick-name", row);
                    var (texture, uv) = trickAssets.Material("Stunt_" + (stunt.key.StartsWith("STR_") ? stunt.key.Substring(4) : stunt.key));
                    if (texture)
                    {
                        var gesture = new Image { image = texture, uv = uv, scaleMode = ScaleMode.ScaleToFit, pickingMode = PickingMode.Ignore };
                        gesture.AddToClassList("ou-trick-gesture"); row.Add(gesture);
                    }
                }
            }
            catch (Exception e) { Debug.LogWarning("[OpenReflex] trick library: " + e.Message); OuUi.Text("Trick data could not be loaded.", "mxa-note", scroll); }
            Focus(back);
        }

        // ------------------------------------------------------------------------------------------------ VEHICLE LIST (the Free Ride's quick view)
        // VehiclesQuickViewPopup + PauseMenu:OnQuickView{Player,AI}VehicleChanged: the choice is equipped and the same Free Ride loads with it (a new
        // session, as the original's Vehicle Loading Page amounts to: OriginalQuickView). Two lists, toggled with Y / F (the popup's Toggle button,
        // STR_PRO_RIDERS / STR_MYRIDES): MY RIDES, the player's vehicles (columns STR_VEHICLE_TYPE / STR_VEHICLE_NAME), and PRO RIDERS: every
        // PlayerSelectable AIRider x his AIRiderVehicle rows of the event's vehicle types (columns STR_PRO_RIDER / STR_VEHICLE_TYPE), a choice
        // equipping his AIVehicle and himself in his DefaultRiderGearSetId (AI owners; the profile is not touched). The performance bars of the
        // focused vehicle stand beside the list. The original builds the types from the UI variable VehicleClass; LocationSelect leaves it at -1 in
        // the arcade Free Ride (no rules), so its pro list is empty there; this port lists the vehicle types the Free Ride can ride (OpenSetup.VehicleTypes).

        // (the list of the vehicles shown: the showcase feeds its own)
        internal Func<List<GarageItem>> VehicleSource;
        /// The pro riders' rides the PRO RIDERS list offers (the showcase feeds its own); null: OpenGarageData.ProRides() of the event's vehicle types.
        internal Func<List<OpenGarageData.ProRide>> ProSource;
        bool prosShown;
        sealed class VehicleChoice
        {
            public string key, title, column0, column1; public bool riding;
            public GarageItem item; public OpenGarageData.ProRide pro;
        }
        VehicleChoice pendingChoice;

        /// The pro the world was launched with (AI owners), else the profile's persisted pro selection; false when the player's own rider rides.
        bool RidingPro(out int vehicleId, out int riderId)
        {
            vehicleId = riderId = -1;
            var g = runtime ? runtime.garage : null;
            if (g != null && g.vehicleOwner >= 0)
            {
                if (g.vehicleOwner != ReflexWorldFiles.Garage.AI) return false;
                vehicleId = g.vehicleId; riderId = g.riderId; return true;
            }
            try
            {
                var rider = ReflexProfile.Current?.T("PlayerRider");
                int who = rider?.Int("LastRiderId", 0, 0) ?? 0;
                if (who < 1) return false;
                riderId = who; vehicleId = rider.Int("LastEquippedVehicle", 0); return true;
            }
            catch (Exception) { return false; }
        }

        /// The PRO RIDERS list: PlayerSelectable riders x their rides of the Free Ride's vehicle types, the rider's rides in vehicle type order.
        List<OpenGarageData.ProRide> ProRides()
        {
            if (ProSource != null) return ProSource();
            var ev = runtime ? ReflexEvents.Find(runtime.level) : null;
            var types = ev != null ? OpenSetup.VehicleTypes(ev) : new List<int>();
            var all = OpenGarageData.ProRides();
            if (types.Count == 0) return all;
            return all.Where(r => types.Contains(r.vehicleType)).GroupBy(r => r.rider).SelectMany(g => g.OrderBy(r => types.IndexOf(r.vehicleType))).ToList();
        }

        void ShowVehicles(string focusName = null)
        {
            ResetPage("vehicles", "VEHICLE LIST", prosShown ? "CHOOSE A PRO RIDER FOR THIS FREE RIDE" : "CHOOSE A VEHICLE FOR THIS FREE RIDE");
            var content = OuUi.Div("ou-page", body);
            var side = OuUi.Div("ou-page-side ou-pause-side ou-pause-list" + (prosShown ? " ou-veh-pros" : ""), content); var aside = OuUi.Div("ou-page-main", content);
            var back = Button("BACK", "vehicles-back", () => ShowMain("pause-vehicles"), side);
            var choices = new List<VehicleChoice>();
            bool ridingPro = RidingPro(out int proVehicle, out int proRider);
            try
            {
                if (!prosShown)
                {
                    foreach (var v in (VehicleSource != null ? VehicleSource() : OpenGarageData.Vehicles()).Where(v => v.owned))
                        { var c = ChoiceOf(v); c.riding = v.equipped && !ridingPro; choices.Add(c); }
                }
                else
                {
                    foreach (var r in ProRides())
                        choices.Add(new VehicleChoice { key = "pause-pro-" + r.rider + "-" + r.vehicle, pro = r, title = r.name, column0 = r.name, column1 = OpenGarageData.TypeName(r.vehicleType).ToUpperInvariant(), riding = ridingPro && r.vehicle == proVehicle && r.rider == proRider });
                }
            }
            catch (Exception e) { Debug.LogWarning("[OpenReflex] vehicle list: " + e.Message); choices.Clear(); }
            // the Toggle button (VehiclesQuickViewPopup: shown for the Free Ride, which allows the AI riders)
            VisualElement toggle = null;
            Action flip = () => { prosShown = !prosShown; ShowVehicles(); };
            toggle = Button(prosShown ? OpenContent.Str("STR_MYRIDES", "MY RIDES").ToUpperInvariant() : OpenContent.Str("STR_PRO_RIDERS", "PRO RIDERS").ToUpperInvariant(), "vehicles-toggle", flip, side);
            nav.OnAlt = flip;
            OuUi.Hints(hints, (OuUi.KeySubmit, "SELECT"), (OuUi.KeyAlt, prosShown ? "MY RIDES" : "PRO RIDERS"), (OuUi.KeyCancel, "BACK"));
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("ou-pause-list-scroll"); scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden; side.Add(scroll);
            var details = OcUi.Panel(aside, "PERFORMANCE", out var detailBody, out var detailTitle, "ou-pause-aside ou-pause-stats");
            var bars = OuUi.Div("ou-pause-bars", detailBody);
            var rows = new List<VisualElement>(); VisualElement start = null;
            foreach (var c in choices)
            {
                var choice = c;
                var row = OuUi.Item(OuUi.Div("ou-trick-row ou-veh-row", scroll.contentContainer), () => Choose(choice));
                row.name = choice.key;
                OuUi.Text(choice.column0, "ou-trick-tier ou-veh-class", row).enableRichText = false;
                OuUi.Text(choice.column1, "ou-trick-name", row).enableRichText = false;
                if (choice.riding) OuWidgets.Pill(row, "RIDING", "good");
                row.RegisterCallback<FocusInEvent>(_ =>
                {
                    scroll.ScrollTo(row);
                    detailTitle.text = choice.title;
                    bars.Clear();
                    var stats = choice.item != null ? OpenGarageData.Stats(choice.item.id, choice.item.playerVehicle) : OpenGarageData.Stats(choice.pro.model, -1);
                    foreach (var st in stats) StatBars(bars, st);
                });
                rows.Add(row);
                if (choice.riding && start == null) start = row;
                if (focusName != null && row.name == focusName) start = row;
            }
            if (rows.Count == 0) OuUi.Text(prosShown ? "No pro rider has a vehicle for this Free Ride." : "No vehicle is owned.", "mxa-note", scroll.contentContainer);
            OuWidgets.LinkColumn(rows);
            ((OuNavItem)back.userData).down = toggle; ((OuNavItem)toggle.userData).up = back;
            if (rows.Count > 0)
            {
                ((OuNavItem)toggle.userData).down = rows[0]; ((OuNavItem)rows[0].userData).up = toggle;
                if (start == null) start = rows[0];
            }
            OuMotion.Enter(side, 0.04f, 0.02f, true);
            Focus(start ?? toggle);
            if (start != null) start.schedule.Execute(() => scroll.ScrollTo(start)).ExecuteLater(60);
        }

        static void StatBars(VisualElement parent, GarageStat s)
        {
            var group = OuUi.Div("ou-pause-stat", parent);
            OuUi.Text(s.name, "ou-pause-stat-name", group);
            var tracks = OuUi.Div("ou-pause-stat-bars", group);
            int count = s.two ? 2 : 1;
            for (int k = 0; k < count; k++)
            {
                int b = Mathf.Clamp(k == 0 ? s.base1 : s.base2, 0, 100), bonus = k == 0 ? s.bonus1 : s.bonus2;
                var track = OuUi.Div("ou-gbar", tracks);
                var fill = OuUi.Div("ou-gbar-base", track); fill.style.width = Length.Percent(b);
                if (s.two) { var extra = OuUi.Div("ou-gbar-bonus", track); extra.style.left = Length.Percent(b); extra.style.width = Length.Percent(Mathf.Clamp(bonus, 0, 100 - b)); }
            }
        }

        static VehicleChoice ChoiceOf(GarageItem v) => new VehicleChoice { key = "pause-vehicle-" + v.playerVehicle, item = v, title = v.title.ToUpperInvariant(), column0 = OpenGarageData.TypeName(v.type).ToUpperInvariant(), column1 = v.title.ToUpperInvariant(), riding = v.equipped };
        void ChooseVehicle(GarageItem item) => Choose(ChoiceOf(item));   // (the showcase asks for one of the player's vehicles)

        void Choose(VehicleChoice choice)
        {
            if (choice.riding) { ShowMain("pause-vehicles"); return; }
            pendingChoice = choice;
            ResetPage("vehicle-confirm", "CHANGE VEHICLE?", "");
            var area = OuWidgets.Dialog(overlay);
            OuUi.Text("THE FREE RIDE LOADS AGAIN", "ou-eyebrow2 ou-label", area);
            OuUi.Text(choice.pro != null ? "RIDE AS " + choice.pro.name + " ON THE " + choice.column1 + "?" : "RIDE THE " + choice.title + "?", "ou-dialog-title ou-display", area).enableRichText = false;
            var row = OuUi.Div("ou-button-row", area);
            var cancel = Button("KEEP MINE", "vehicle-cancel", () => ShowVehicles(choice.key), row);
            var go = Button("CHANGE", "vehicle-confirm", () => ChangeVehicle(choice), row);
            OuWidgets.LinkRow(new[] { cancel, go });
            overlay.pickingMode = PickingMode.Position;
            OuMotion.Fade(overlay);
            Focus(go);
        }

        void ChangeVehicle(VehicleChoice choice)
        {
            var ev = runtime ? ReflexEvents.Find(runtime.level) : null;
            if (ev == null) { ShowMain("pause-vehicles"); return; }
            ReflexWorldFiles.Garage garage;
            if (choice.pro != null)
            {   // PauseMenu:OnQuickViewAIVehicleChanged: EquipVehicle( AI vehicle ) and EquipRider( the pro, his DefaultRiderGearSetId ): this session only
                var r = choice.pro;
                garage = new ReflexWorldFiles.Garage { vehicleOwner = ReflexWorldFiles.Garage.AI, vehicleId = r.vehicle, vehicleTypeId = r.vehicleType, riderOwner = ReflexWorldFiles.Garage.AI, riderId = r.rider, riderGearSet = r.gearSet };
                Debug.Log("[Pause] VEHICLE LIST: pro " + r.name + " (rider " + r.rider + ", AI vehicle " + r.vehicle + ", type " + r.vehicleType + ") -> reload " + ev.key);
            }
            else
            {
                var item = choice.item;
                if (!OpenGarageData.SelectVehicle(item.playerVehicle)) { Debug.LogWarning("[Pause] the vehicle could not be equipped: " + ReflexProfile.Error); ShowMain("pause-vehicles"); return; }
                garage = new ReflexWorldFiles.Garage { vehicleOwner = ReflexWorldFiles.Garage.Player1, vehicleId = item.playerVehicle, riderOwner = ReflexWorldFiles.Garage.Player1, riderId = 0, riderGearSet = 0 };
                Debug.Log("[Pause] VEHICLE LIST: " + item.title + " (" + item.playerVehicle + ") -> reload " + ev.key);
            }
            var owner = runtime; string scene = SceneManager.GetActiveScene().name;
            Close();
            OwnAaaLoading.Show();
            AudioListener.pause = false;
            owner.StopSimulation();
            ReflexEventSelect.Launch(ev.key, 0, false, scene, 0, garage, Reflex.Port.RxCore.RxMapScale.Current, weather: Reflex.Port.Rendering.ReflexWeather.RideMode, weatherIntensity: Reflex.Port.Rendering.ReflexWeather.RideIntensity);   // the same world: its MAP SCALE and WEATHER kept
        }

        // ------------------------------------------------------------------------------------------------ VEHICLE TUNING
        // GarageVehicleTuning: engine / suspension / tires move bonus points between their two bars (steps of 5), braking and steering are 0 / 50 / 100;
        // each change is saved to the profile and put on the running vehicle's physics before the next frame (rx_session_tuning_set).
        static readonly string[] TuningColumns = { "Engine1", "Engine2", "Suspension1", "Suspension2", "Tire1", "Tire2", "Braking", "Steering" };

        void ShowTuning()
        {
            ResetPage("tuning", "VEHICLE TUNING", spec?.tiles.FirstOrDefault(t => t.label == "VEHICLE")?.value ?? "");
            var content = OuUi.Div("ou-page", body);
            var side = OuUi.Div("ou-page-side ou-pause-side ou-pause-tuning", content); var aside = OuUi.Div("ou-page-main", content);
            var back = Button("BACK", "tuning-back", () => ShowMain("pause-tuning"), side);
            var profile = ReflexProfile.Current;
            int vehicle = -1, model = -1, row = -1; ProfileData.Table tt = null;
            if (profile != null)
            {
                var g = runtime ? runtime.garage : null;
                vehicle = g != null && g.vehicleOwner == ReflexWorldFiles.Garage.Player1 && g.vehicleId >= 0 ? g.vehicleId : profile.T("PlayerRider").Int("LastEquippedVehicle", 0, 0);
                var owned = profile.T("PlayerVehicle");
                model = owned.Int("VehicleModelId", owned.Find("PlayerVehicleId", vehicle), -1);
                tt = profile.T("PlayerVehicleTuning"); row = tt.Find("PlayerVehicleId", vehicle);
            }
            var modelRow = model >= 0 ? FeTables.Shared.Rows("VehicleModel").FirstOrDefault(m => FeTables.I(m, "VehicleModelId") == model) : null;
            if (tt == null || row < 0 || modelRow == null)
            {
                OuUi.Text("No tuning is available for this vehicle.", "mxa-note", side);
                Focus(back); return;
            }
            int[] values = TuningColumns.Select(c => tt.Int(c, row, 0)).ToArray();
            var list = OuUi.Div("ou-pause-tune", side);
            var details = OcUi.Panel(aside, "WHAT IT DOES", out var detailBody, out _, "ou-pause-aside ou-pause-stats");
            var description = OuUi.Text("", "ou-lead", detailBody); description.enableRichText = false;
            var items = new List<VisualElement> { back };
            for (int i = 0; i < 5; i++)
            {
                // OuTuneRow: the same rows as the garage's VEHICLE TUNING (the two ends in the game's words, the verdict); saved and applied at once
                var tune = new OuTuneRow(list, "pause-tuning-" + i, "ou-pause-tune-row", i, values, modelRow);
                tune.changed = () => SaveTuning(vehicle, values);
                tune.row.RegisterCallback<FocusInEvent>(_ => description.text = tune.Description);
                items.Add(tune.row);
            }
            OuWidgets.LinkColumn(items);
            OuMotion.Enter(side, 0.04f, 0.02f, true);
            Focus(items.Count > 1 ? items[1] : back);
        }

        void SaveTuning(int vehicle, int[] v)
        {
            var tuning = new ReflexGarage.Tuning { engine1 = v[0], engine2 = v[1], suspension1 = v[2], suspension2 = v[3], tire1 = v[4], tire2 = v[5], braking = v[6], steering = v[7] };
            if (runtime == null) return;
            var saved = ReflexProfile.SetTuning(vehicle, tuning);
            if (saved.Result) runtime.ApplyOpenTuning(tuning);   // queued on the session owner, before the next resumed frame
            else { Debug.LogWarning("[Pause] the tuning could not be saved: " + ReflexProfile.Error); OuSound.Play("Locked"); }
        }

        public void CloseForWorldChange() => Close();
        public void Close()
        {
            if (!Active || closing) return;
            closing = true;
            try
            {
                if (ReflexOptions.IsOpen) ReflexOptions.Close();
                open = waiting = false; page = null; closedFrame = Time.frameCount;
                input?.Dispose(); input = null;
                root?.AddToClassList("ou-pause-hidden");
                if (runtime) { runtime.ClearMenuInput(); runtime.paused = pausedBefore; }
                AudioListener.pause = audioBefore;
                Reflex.Port.Platform.ReflexCursor.Want(this, false);
            }
            finally { closing = false; }
        }
        void OnDisable() => Close();
        void OnDestroy() { Close(); input?.Dispose(); if (trickAssets?.baseMaterial) Destroy(trickAssets.baseMaterial); }
    }
}
