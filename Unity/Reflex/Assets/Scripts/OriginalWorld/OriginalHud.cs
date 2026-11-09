using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The original in-race HUD of a National (layer SinglePlayerNational of the InGame page, read at run time from the install's
    // Database/MXUI package HudNationalSinglePlayer.<LANG> by ReflexUiData; bindings in _re/notes/ui.md), driven by the running game's values:
    //   rank             MXUIPlayerRankComponent: place (1 in Pre/Countdown), new text 0.75 s after a change + RankChange
    //   lap counter      MXUILapCountComponent: "%d/%d" clamp(laps + 1, 1, N), hidden in race state (A+0x238) 5
    //   lap time         MXUILapTimeComponent: the last lap while D+0x130 <= D+0x10DC (3 s hold), else the lap timer
    //   best lap         MXUIBestLapTimeComponent: blank until the first best lap, colour 0xFFBEA91F
    //   time behind / ahead  -(D+0x10B4) red when negative else green / D+0x10B8 green
    //   speedometer      int(V+0x150 * 0.68182) mph
    //   OFF TRACK / penalty  RuleOfftrackTimePenalty: on-track flag; the penalty shown red for 3 s back on track
    //   WRONG WAY        RuleBackwards flag
    //   flag             MXUIFlagContainer: the white FINAL LAP flag on the last lap (OneLapLeft) until the finish
    //   camera name      MXUICameraNameComponent: 2 s after a camera change
    // Elements of other modes (stunts, reflex arrows, flooding, network) stay hidden; the Eliminator message, the Outrun gate counter, the Section
    // Race times and the Freestyle judges are OriginalHud.RaceHud.cs.
    // In race state Post (4) the page changes (GotoPostEvent): the HUD goes and the original post-race pages show instead
    // (OriginalPostRace: the results table, then the CONTINUE / RESTART menu).
    // HUD STYLE (Options): ORIGINAL is this HUD / the live one exactly as described here; MODERN hides this canvas (ClientHides) for
    // the client's own (Reflex.Port.Hud.ReflexModernHud, which also steps the live HUD layers aside); H hides either.
    //
    // WP-3 (_re/notes/hud.md): the arcade races (LiveRace) show the game's own HUD instead, live (OriginalLiveUi: the InGame page graph
    // runs in the world, the hub routes to the activity's page - "National Single Player Page" SinglePlayer_National_Layer, Supercross
    // SinglePlayerSX_NAT, Terracross SinglePlayerOmnicross (CountDownComponent), Powercross SinglePlayerPowercross, Waypoint
    // SinglePlayerWaypoint (compass, distance, WaypointGateIndicator), Free Ride SinglePlayerFreeride (+ AAA / tutorial activation) - after
    // the EventIntro page, and every component is the game's MXUI*Component: rank "1ST" with its 0.75 s delay, laps, timers, stunt names,
    // reflex arrows, flags, eliminator ...). The hand-built HUD below is the fallback when the live UI is unavailable (an older plugin, no
    // InGamePageGraph, the layer not built); the post-race pages stay the port's. A MOTOCAREER race keeps the hand-built HUD (its rules /
    // gate / post pages are OriginalCareerRace's). UiSound plays the UI sounds the game's UI plays in the race.
    [DisallowMultipleComponent]
    public sealed partial class OriginalHud : MonoBehaviour
    {
        public bool visible = true;

        Canvas canvas; RectTransform root; OriginalUiTree tree;
        UnityEngine.UI.Text waypointLabel;
        bool powercrossHud, supercrossHud;
        OriginalUiTree.Node beginningCountdown; string powercrossGo;
        string R => packageHud != null ? packageDashboard + "/" : powercrossHud ? "Power Cross Dashboard/" : supercrossHud ? "SX_NAT Race Hud/" : "National Race Hud/";
        // The HUD route of the other career activities (w-career): the route's package of the install, its layer and its dashboard.
        // The generic race elements of the layer (rank, laps, lap time, speedometer, gaps, off track, wrong way, flags, camera name) are
        // driven as on the National HUD where the layer has them; the activity's own components (the Section Race times, the FMX score
        // card, the Outrun section timer and gate counter) are drawn from the core's copies (OriginalHud.RaceHud.cs).
        string packageHud, packageLayer, packageDashboard;
        static readonly Dictionary<string, (string layer, string dashboard)> PackageHuds = new Dictionary<string, (string, string)>(StringComparer.Ordinal)
        {
            ["HudTerracrossSinglePlayer"] = ("SinglePlayerOmnicross.lyr", "OmnicrossDashboard"),
            ["HudFreestyleSinglePlayer"] = ("SinglePlayerFMX.lyr", "SinglePlayerFMX"),
            ["HudSectionRaceSinglePlayer"] = ("SinglePlayerSectionRace.lyr", "Section Race Hud"),
            ["HudOutrunSinglePlayer"] = ("SinglePlayerOutRunner.lyr", "OutRunnerHUD"),
        };
        public static bool HasPackageHud(string route) => route != null && PackageHuds.ContainsKey(route);
        public void RequirePackageHud(string route)
        {
            if (!PackageHuds.TryGetValue(route ?? "", out var layer)) throw new InvalidOperationException("No authored HUD layer for the route " + route + ".");
            if (powercrossHud || supercrossHud || (built && packageHud != route)) throw new InvalidOperationException("The HUD was already selected for another race mode.");
            packageHud = route; packageLayer = layer.layer; packageDashboard = layer.dashboard;
            Build();
        }
        // Called before career render readiness; never advertise a different package after construction.
        public void SelectPowercrossHud()
        {
            if (supercrossHud || (built && !powercrossHud)) throw new InvalidOperationException("The HUD was already selected for another race mode.");
            powercrossHud = true;
        }
        public void RequirePowercrossHud() { SelectPowercrossHud(); Build(); }
        public void SelectSupercrossHud()
        {
            if (powercrossHud || (built && !supercrossHud)) throw new InvalidOperationException("The HUD was already selected for another race mode.");
            supercrossHud = true;
        }
        public void RequireSupercrossHud() { SelectSupercrossHud(); Build(); }

        OriginalUiTree.Node rank, laps, timer, best, speed, behind, ahead, offTrack, penalty, wrongWay, flag, finalLap, cameraLabel;
        readonly Reflex.Port.Hud.ReflexHudModel timedLaps = new Reflex.Port.Hud.ReflexHudModel();   // (a timed championship stage's lap counter)
        // the format elements (w-career): the eliminator's riders remaining, the overtake start countdown, the Outrun section timer
        OriginalUiTree.Node ridersRemaining, ridersRemainingText, overtakeCountdown, outrunTimer;
        bool overtakeArmed; float overtakeClock; int lastRaceState = -1; string overtakeGo;
        int shownRank = -1, pendingRank = -1; float rankTimer;
        bool wasOnTrack = true; float penaltyTimer; float cameraTimer; int lastCameraMode = -1;
        int lastLaps = -1; bool built, failed;
        OriginalPostRace post; bool postFailed;

        CanvasGroup speedometerGroup;
        float speedometerPreviousAlpha;
        bool speedometerLeased, suppressLegacySpeedometer;
        // Only the authored Speedometer Component is masked. Its active state,
        // text, child animations and every other classic widget remain owned by the HUD.
        public void SetLegacySpeedometerHidden(bool hide)
        {
            suppressLegacySpeedometer = hide;
            if (hide)
            {
                if (speed == null || !speed.rect) return;
                if (!speedometerGroup) speedometerGroup = speed.rect.GetComponent<CanvasGroup>() ?? speed.rect.gameObject.AddComponent<CanvasGroup>();
                if (!speedometerLeased) { speedometerPreviousAlpha = speedometerGroup.alpha; speedometerLeased = true; }
                speedometerGroup.alpha = 0f;
            }
            else if (speedometerLeased)
            {
                // A later external alpha edit wins; never restore another owner's change.
                if (speedometerGroup && speedometerGroup.alpha == 0f) speedometerGroup.alpha = speedometerPreviousAlpha;
                speedometerLeased = false;
            }
        }
        void OnDisable() => SetLegacySpeedometerHidden(false);

        public bool Built => built;
        /// HUD STYLE MODERN (the client's HUD instead: Reflex.Port.Hud.ReflexModernHud, for the modes it covers; a mode it does not cover keeps this
        /// one) or H (the HUD hidden) keep this one off.
        public static bool ClientHides => Reflex.Port.Options.ReflexSettings.HudHidden || (Reflex.Port.Options.ReflexSettings.HudStyle == Reflex.Port.Options.HudStyle.Modern && Reflex.Port.Hud.ReflexModernHud.CoversMode);
        public Canvas Canvas => canvas;
        public OriginalUiTree Tree => tree;

        public Texture2D Texture(string key) => OriginalUiAssets.Texture(key);
        public OriginalFont FontFor(string metricsKey) => OriginalUiAssets.Font(metricsKey);

        public void Build()
        {
            if (built || failed) return;
            try { BuildHud(); }
            catch (Exception e) { failed = true; Debug.LogWarning("Original HUD not built (MX vs ATV Reflex install?): " + e.Message); }
        }

        void BuildHud()
        {
            var cgo = new GameObject(packageHud != null ? "Original HUD (" + packageHud + ")" : powercrossHud ? "Original HUD (Powercross)" : supercrossHud ? "Original HUD (Supercross)" : "Original HUD (National)", typeof(Canvas), typeof(CanvasScaler));
            cgo.transform.SetParent(transform, false);
            canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 10;
            var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;   // the game's 1280x720 space, whole
            var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
            root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
            tree = new OriginalUiTree(Texture, FontFor);
            var doc = packageHud != null ? OriginalUiAssets.Data.BuildPackageHud(packageHud, packageLayer, packageDashboard + "/")
                : powercrossHud ? OriginalUiAssets.Data.BuildPowercrossHud() : supercrossHud ? OriginalUiAssets.Data.BuildSupercrossHud() : OriginalUiAssets.Data.BuildHud();   // the layer's elements, layer animations and the full tree (component animations)
            foreach (var e in OriginalUi.Arr(doc, "elements")) tree.Build((Dictionary<string, object>)e, root, null);
            tree.AddAnimations(OriginalUi.Arr(doc, "layerAnimations"));
            tree.AddComponentAnimations(OriginalUi.Obj(doc, "layerJson"));
            rank = tree.Find(R + "SinglePlayerRankComponent"); laps = tree.Find(R + "SinglePlayer LapCounterComponent");
            timer = tree.Find(R + "Timer Dashboard Component"); best = tree.Find(R + "BestLapTime"); speed = tree.Find(R + "Speedometer Component");
            behind = tree.Find(R + "TimeBehindLeaderComponent"); ahead = tree.Find(R + "TimeAhead Component");
            offTrack = tree.Find(R + "OffTrackComponent"); penalty = tree.Find(R + "OffTrack Penalty"); wrongWay = tree.Find(R + "WrongWaycomponent");
            flag = tree.Find(R + "flag Container"); finalLap = tree.Child(flag, "FinalLapContainer"); cameraLabel = tree.Find(R + "SinglePlayerCameraLabel");
            // hidden until their events (or other modes)
            foreach (string name in new[] { "Arrows", "StuntFeedbackComponent", "FloodingComponent", "PlayerWinsComponent", "DisconnectedComponent",
                                            "PlayerEliminatedComponent", "RidersRemainingComponent", "OverTakeCountdownComponent", "OffTrackComponent",
                                            "OffTrack Penalty", "WrongWaycomponent", "SinglePlayerCameraLabel",
                                            "OutrunnerSectionTimer", "Outrunner Gate Indicator", "ScoreCard", "Judges Panel", "FMXJudgesFeedback",
                                            "SectionLeadersContainer", "Best Section Container", "Timer Dashboard Container", "EventTimerComponent",
                                            "CountDownComponent", "MXUIBeginningCountdownComponent" })
                OriginalUiTree.SetVisible(tree.Find(R + name), false);
            if (flag != null) foreach (var c in flag.children) OriginalUiTree.SetVisible(c, false);
            ridersRemaining = tree.Find(R + "RidersRemainingComponent"); ridersRemainingText = tree.Child(ridersRemaining, "Riders Remaining");
            overtakeCountdown = tree.Find(R + "OverTakeCountdownComponent"); outrunTimer = tree.Find(R + "OutrunnerSectionTimer");
            OriginalUiAssets.Data.Strings().TryGetValue("STR_GO", out overtakeGo);
            BindReflexArrows();
            BindRaceHud();   // the Eliminator message, the gate counter, the event timer, the Section Race times, the Freestyle judges (OriginalHud.RaceHud.cs)
            if (supercrossHud && (rank == null || laps == null || timer == null || best == null || speed == null))
                throw new InvalidOperationException("Authored Supercross HUD is missing required race components.");
            if (powercrossHud && (rank == null || laps == null || timer == null || best == null || speed == null))
                throw new InvalidOperationException("Authored Powercross HUD is missing required race components.");
            if (powercrossHud)
            {
                beginningCountdown = tree.Find(R + "CountDownComponent");
                if (beginningCountdown == null) throw new InvalidOperationException("Authored Powercross countdown component is missing.");
                var strings = OriginalUiAssets.Data.Strings();
                if (!strings.TryGetValue("STR_GO", out powercrossGo)) throw new InvalidOperationException("Authored countdown localization STR_GO is missing.");
                OriginalUiTree.SetVisible(beginningCountdown, false);
            }
            built = true;
            tree.Play("HUDIntro");
        }

        // The post-race pages (race state Post): opened with the local player's row, closed when the event restarts.
        void TickPost(float dt, bool inPost, ReflexRaceState race, ReflexHudState hud, bool haveHud)
        {
            if (TryGetComponent<OriginalWorldRuntime>(out var owner) && (owner.OwnResultsPresentation || owner.OwnedCareerActive || Reflex.Port.OpenFrontEnd.ChampWeekendFlow.CardUp(owner)))
            {
                if (post != null && post.IsOpen) post.Close();
                return;
            }
            if (TryGetComponent<OriginalCareerRace>(out var career) && career.HasRace) { career.TickPost(dt, inPost); return; }   // MOTOCAREER's own pages
            if (inPost && post == null && !postFailed)
            {
                try
                {
                    var runtime = GetComponent<OriginalWorldRuntime>();
                    // CONTINUE of a career race: the game completes the series event and saves in this world first (ReflexCareer)
                    post = new OriginalPostRace(transform, () => { if (Reflex.Port.OpenFrontEnd.ChampWeekendFlow.InStage && runtime) { Reflex.Port.OpenFrontEnd.ChampWeekendFlow.StageOver(runtime); return; } if (Reflex.Port.Content.ReflexCareer.Current != null && runtime) Reflex.Port.Content.ReflexCareer.Finish(runtime.StopSimulation()); OriginalPauseMenu.QuitToFrontEnd(); },   // (RESTORED CONTENT: a championship stage's CONTINUE: the weekend card)
                                                () => { if (runtime) runtime.Restart(); }, !Reflex.Port.RxCore.RxChampWeekend.StageInSession);   // (RESTORED CONTENT: no RESTART after a championship stage)
                }
                catch (Exception e) { postFailed = true; Debug.LogWarning("Original post-race pages not built (MX vs ATV Reflex install?): " + e.Message); }
            }
            if (post == null) return;
            if (inPost && !post.IsOpen)
            {
                if (!post.AskGameTable(GetComponent<OriginalWorldRuntime>()?.Simulation, dt, GetComponent<OriginalWorldRuntime>()?.Roster?.Local?.name)) return;   // the game's own table (WP-7)
                // the game's table lists every rider from its standings (reflex_world_race_drivers), in the standings' order (the core writes
                // them in it: the table's Index, numbered Index + 1; the places repeat after InProgress, 00565570); a rider still on track has
                // no finishing time yet. Names: the race's Player List (the roster), the local profile's name.
                string name = Reflex.Port.Content.ReflexProfile.HasPersona ? Reflex.Port.Content.ReflexProfile.Persona : GetComponent<OriginalWorldRuntime>()?.Roster?.Local?.name ?? "Player";
                var runtime = GetComponent<OriginalWorldRuntime>();
                var rows = new List<OriginalPostRace.Row>();
                if (runtime && runtime.DriverCount > 1)
                {
                    var drivers = new List<ReflexDriverResult>();
                    for (int i = 0; i < runtime.DriverCount; i++) drivers.Add(runtime.Drivers[i]);
                    for (int i = 0; i < drivers.Count; i++)
                    {
                        var d = drivers[i];
                        var entry = runtime.Roster?.entries.Find(e => e.slot.slot == d.slot);
                        string n = d.local != 0 ? name : !string.IsNullOrEmpty(entry?.name) ? entry.name : "RIDER " + (d.slot + 1);
                        rows.Add(new OriginalPostRace.Row {
                            index = i + 1, name = n, c0 = d.finished != 0 ? ResultTime(d.race_time) : ResultTime(0f), c1 = ResultTime(d.best_lap),
                            c2 = ResultTime(d.local != 0 && haveHud ? hud.penalty : 0f) });
                    }
                }
                else rows.Add(new OriginalPostRace.Row {
                    index = Mathf.Max(1, race.place), name = name, c0 = ResultTime(race.race_time), c1 = ResultTime(race.best_lap),
                    c2 = ResultTime(haveHud ? hud.penalty : 0f) });
                if (!post.HasGameTable) post.SetRows(rows);   // without the game's table (an older libreflexworld): the port's own rows
                post.Open();
            }
            else if (!inPost && post.IsOpen) post.Close();
            post.Tick(dt);
        }

        // MM:SS.cc; a time that was never set (0, or FLT_MAX for no best lap) prints as dashes
        static string ResultTime(float t) => t <= 0f || t >= 3.0e38f ? "--:--.--" : OriginalUi.Time(t);

        static void TimerText(OriginalUiTree.Node component, string shadowName, string foreName, string text, Color? fore = null)
        {
            if (component == null) return;
            var sh = component.children.Find(c => c.name == shadowName); var fg = component.children.Find(c => c.name == foreName);
            OriginalUiTree.SetText(sh, text); OriginalUiTree.SetText(fg, text);
            if (fore.HasValue) OriginalUiTree.SetColor(fg, fore.Value);
        }

        // The format components of the layer, from the core's copies (rx_session_race_format):
        //   MXUIEliminatorComponent (mode 5): visible, "riders left/drivers" (005f6ec0; "drivers/drivers" before the first elimination);
        //   MXUIOverTakeCountdownComponentStub (mode 6): at InProgress entered the clock = drivers * OvertakeStartDelayPerDriver (MXRegistry
        //     MXGame, 1.0) - 0.01, shown as its whole seconds while >= 1, else STR_GO, hidden once <= -2, minus dt each frame (005fbbf0 / vf11);
        //   MXUIOutrunSectionTimerComponent (mode 8): the local player's section time left from the first section on.
        void TickFormat(float dt, ReflexRaceState race, OriginalWorldRuntime runtime)
        {
            bool have = runtime && runtime.HasRaceFormat;
            var f = have ? runtime.RaceFormat : default;
            OriginalUiTree.SetVisible(ridersRemaining, have && f.activityMode == 5);
            if (have && f.activityMode == 5)
            {
                int left = race.state <= 1 || f.ridersLeft <= 0 || f.ridersLeft > f.drivers ? f.drivers : f.ridersLeft;
                OriginalUiTree.SetText(ridersRemainingText, left + "/" + f.drivers);
            }
            if (have && f.activityMode == 6 && race.state == 2 && lastRaceState != 2)
            {
                overtakeArmed = true; overtakeClock = f.drivers * 1.0f - 0.01f;
            }
            lastRaceState = race.state;
            if (overtakeArmed)
            {
                OriginalUiTree.SetVisible(overtakeCountdown, true);
                OriginalUiTree.SetText(overtakeCountdown, overtakeClock >= 1f ? ((int)overtakeClock).ToString() : (overtakeGo ?? "GO!"));
                if (overtakeClock <= -2f) { overtakeArmed = false; OriginalUiTree.SetVisible(overtakeCountdown, false); }
                overtakeClock -= dt;
            }
            else OriginalUiTree.SetVisible(overtakeCountdown, false);
            bool outrun = have && f.activityMode == 8 && f.section >= 0;
            OriginalUiTree.SetVisible(outrunTimer, outrun);
            if (outrun) TimerText(outrunTimer, "Timer Dashboard Component Shadow", "Timer Dashboard Component", OriginalUi.Time(f.sectionTime));
            TickRaceHud(runtime);
            TickReflexArrows(runtime);   // the wreck-recovery arrows (OriginalHud.Reflex.cs)
        }

        /// The hand-built HUD's own camera name shows (ReflexCameraToast then stays away).
        public bool CameraLabelShown => cameraLabel != null && cameraTimer > 0 && canvas && canvas.enabled && visible;

        public void Tick(float dt, ReflexRaceState race, ReflexHudState hud, bool haveHud, int cameraMode, string cameraName)
        {
            // A package HUD of a timed activity (Freestyle, Section Race) has no laps: it shows while the race runs.
            bool inRace = race.valid != 0 && (race.num_laps > 0 || (packageHud != null && race.state >= 1 && race.state <= 3));
            // (WP-V2: a timed career series, Freestyle, has no laps: its Post still opens the career's pages)
            bool inPost = race.valid != 0 && race.state == 4 && (race.num_laps > 0 || TryGetComponent<OriginalCareerRace>(out var careerRace) && careerRace.HasRace);
            // the game's own pages, live (OriginalLiveUi): the modes' HUD, popups and post-event pages; a race's HUD (its post-race pages
            // stay the port's). A race whose live UI is unavailable falls back to the hand-built HUD.
            if (!powercrossHud && !supercrossHud && TryGetComponent<OriginalLiveUi>(out var live) && live.enabled && !(live.race && live.Unavailable))
            {
                if (canvas) canvas.enabled = false;
                if (live.race) TickPost(dt, inPost, race, hud, haveHud);
                return;
            }
            if (!built) Build();
            if (!built) return;
            TickPost(dt, inPost, race, hud, haveHud);
            canvas.enabled = visible && inRace && !inPost && !ClientHides;
            if (!canvas.enabled) return;
            tree.Tick(dt);
            SetLegacySpeedometerHidden(suppressLegacySpeedometer);
            var runtime = GetComponent<OriginalWorldRuntime>();
            bool waypoint = runtime && runtime.IsWaypoint;
            if (powercrossHud)
            {
                if (!runtime || !runtime.HasPowercross) throw new InvalidOperationException("Powercross requires copied native countdown state.");
                var pc = runtime.Powercross;
                if (pc.component_bound != 1 || pc.visible > 1 || pc.exited > 1 || pc.displayed_step < 0 || pc.displayed_step > 3 || float.IsNaN(pc.hold_remaining) || float.IsInfinity(pc.hold_remaining))
                    throw new InvalidOperationException("Native Powercross countdown component lifecycle is unavailable.");
                OriginalUiTree.SetVisible(beginningCountdown, pc.visible != 0);
                OriginalUiTree.SetText(beginningCountdown, pc.displayed_step == 0 ? powercrossGo : pc.displayed_step.ToString(System.Globalization.CultureInfo.InvariantCulture));
            }
            OriginalUiTree.SetVisible(behind, !waypoint); OriginalUiTree.SetVisible(ahead, !waypoint);
            TickFormat(dt, race, runtime);
            if (waypoint && !waypointLabel)
            {
                waypointLabel = Reflex.Port.Hud.ReflexHudKit.Label(root, "WaypointProgress", Reflex.Port.Hud.ReflexHudKit.Bold, 23,
                    Color.white, TextAnchor.UpperLeft, new Vector2(0, 1), new Vector2(0, 1), new Vector2(140, -145), new Vector2(330, 70));
                var shadow = waypointLabel.gameObject.AddComponent<UnityEngine.UI.Shadow>(); shadow.effectColor = Color.black; shadow.effectDistance = new Vector2(1, -1);
            }
            if (waypointLabel)
            {
                waypointLabel.gameObject.SetActive(waypoint);
                if (waypoint) waypointLabel.text = Reflex.Port.Hud.WaypointHudText.Progress(runtime.HasWaypoint, runtime.Waypoint, Reflex.Port.Options.ReflexSettings.Metric);
            }

            // rank
            int place = race.state <= 1 ? 1 : Mathf.Max(1, race.place);
            if (shownRank < 0) { shownRank = place; OriginalUiTree.SetText(rank, place.ToString()); }
            if (place != shownRank && place != pendingRank) { pendingRank = place; rankTimer = 0.75f; }
            if (pendingRank > 0) { rankTimer -= dt; if (rankTimer <= 0) { shownRank = pendingRank; pendingRank = -1; OriginalUiTree.SetText(rank, shownRank.ToString()); tree.Play("RankChange", rank); } }

            // laps
            int n = Mathf.Max(1, race.num_laps);
            timedLaps.FillTimedOnly(runtime);
            if (timedLaps.timed) OriginalUiTree.SetText(laps, timedLaps.TimedText(true));   // RESTORED CONTENT (championship TIMED RACES): the clock, "+1", FINAL; never the placeholder total
            else if (n >= 0x61) OriginalUiTree.SetText(laps, Mathf.Max(Mathf.Clamp(race.laps_completed + 1, 1, n), Reflex.Port.Practice.RxPractice.LapNumber).ToString());   // PRACTICE: endless, "n"
            else OriginalUiTree.SetText(laps, $"{Mathf.Clamp(race.laps_completed + 1, 1, n)}/{n}");
            OriginalUiTree.SetVisible(laps, !haveHud || hud.activity_state != 5);

            // lap time (3 s hold of the last lap)
            float shown = !waypoint && haveHud && hud.hold_clock <= hud.hold_until ? race.last_lap : race.lap_time;
            TimerText(timer, "Timer Dashboard Component Shadow", "Timer Dashboard Component", OriginalUi.Time(shown));

            // best lap
            bool hasBest = race.best_lap > 0 && race.best_lap < 3.0e38f;
            TimerText(best, "LastLapTimeShadow", "LastLapTimeFore", hasBest ? OriginalUi.Time(race.best_lap) : "", new Color(190 / 255f, 169 / 255f, 31 / 255f));

            if (haveHud)
            {
                float tb = -hud.time_behind;
                TimerText(behind, "LastLapTimeShadow", "LastLapTimeFore", OriginalUi.Time(tb), tb < 0 ? new Color(0.8f, 0.2f, 0.2f) : new Color(0, 145 / 255f, 0));
                TimerText(ahead, "LastLapTimeShadow", "LastLapTimeFore", OriginalUi.Time(hud.time_ahead), new Color(0, 145 / 255f, 0));
                OriginalUiTree.SetText(speed, Mathf.Max(0, (int)(hud.speed * 0.68182f)).ToString());

                bool racing = race.state == 2;
                bool onTrack = hud.on_track != 0 || !racing;
                if (onTrack && !wasOnTrack && hud.penalty > 0) penaltyTimer = 3f;
                if (!onTrack) penaltyTimer = 0;
                wasOnTrack = onTrack;
                if (penaltyTimer > 0) penaltyTimer -= dt;
                if (offTrack != null && !onTrack != offTrack.rect.gameObject.activeSelf) { OriginalUiTree.SetVisible(offTrack, !onTrack); if (!onTrack) tree.Play("OffTrackNotificationIn", offTrack); }
                OriginalUiTree.SetVisible(penalty, penaltyTimer > 0 && hud.penalty > 0);
                if (penalty != null) foreach (var c in penalty.children) OriginalUiTree.SetText(c, OriginalUi.Time(hud.penalty));
                OriginalUiTree.SetVisible(wrongWay, racing && hud.wrong_way != 0);
            }

            // white flag on the last lap (OneLapLeft) until the finish (EndOfRace)
            bool lastLap = timedLaps.timed ? timedLaps.finalLap : race.state == 2 && race.laps_completed == race.num_laps - 1;
            if (finalLap != null && lastLap != finalLap.rect.gameObject.activeSelf) { OriginalUiTree.SetVisible(finalLap, lastLap); if (lastLap) tree.Play("FlagIn", flag); }
            lastLaps = race.laps_completed;

            // camera name toast (2 s; cameraMode: the runtime's camera key, ReflexCameraPresets.ViewKey: a switch between any two cameras)
            if (lastCameraMode >= 0 && cameraMode != lastCameraMode && Reflex.Port.Options.ReflexSettings.CameraToast) { cameraTimer = 2f; OriginalUiTree.SetText(cameraLabel, cameraName); }
            lastCameraMode = cameraMode;
            if (cameraTimer > 0) cameraTimer -= dt;
            OriginalUiTree.SetVisible(cameraLabel, cameraTimer > 0);
        }

        // ---------------------------------------------------------------------------------------------- WP-3: the live race HUD
        /// The activities whose HUD is the game's own, live (OriginalLiveUi.IsLive): the arcade races. A MOTOCAREER series race keeps the
        /// hand-built HUD (OriginalCareerRace runs its rules popup, gate selection and post pages; the game's own would show twice).
        public static readonly HashSet<string> LiveActivities = new HashSet<string>(StringComparer.Ordinal)
        { "National", "Supercross", "Terracross", "Powercross", "WaypointRace", "FreeRide", "UTVMoto" };
        /// Off: every race shows the hand-built HUD (a switch for comparisons; the game's own HUD is the default).
        public static bool LiveRaces = true;
        public static bool LiveRace(Reflex.Port.Content.ReflexEvents.Event e) =>
            LiveRaces && e != null && LiveActivities.Contains(e.spec.type) && !(Reflex.Port.Content.ReflexCareer.Current is Reflex.Port.Content.ReflexCareer.Race cr && cr.eventKey == e.key);

        // The UI sounds of the race (reflex_world_ui_sounds: what the game's UI sound manager g_UISoundManager [0xd94a84] plays -
        // UISound_Play 005e37e0, UISound_PlayIfIdle 005e3840, UISound_Stop 005e38c0 - in the world, where UI_02 is not loaded): played
        // as WP-F's FeSounds plays App.PlayUISound in the front end, UIRegistry.xml UIName -> the UI_02 event, by the port's FMOD Event
        // player - here the race's own OriginalAudio, with UI_02 added to its library (UI channel: plays on through the pause).
        // play: restart (UISound_Play stops a playing one first); loop: only when not playing (the warning loops); stop.
        static Dictionary<string, string> uiSoundEvents;   // UIName -> event path
        static readonly Dictionary<string, uint> uiSoundWrappers = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase);
        static readonly HashSet<string> uiSoundLooping = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        static OriginalAudio uiSoundPlayer; static uint uiSoundSerial;
        /// The UI sounds played in this session, newest last ("play RankChangeUp -> UI_02/UI/RANK_CHANGE_UP"; tests).
        public static readonly List<string> UiSoundLog = new List<string>();

        public static void UiSound(Component near, string kind, string uiName)
        {
            if (string.IsNullOrEmpty(uiName)) return;
            try
            {
                if (uiSoundEvents == null)
                {
                    uiSoundEvents = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                    string xml = System.Text.RegularExpressions.Regex.Replace(System.IO.File.ReadAllText(Reflex.Port.Content.ReflexInstall.PathOf("UIRegistry.xml")), "<!--.*?-->", "", System.Text.RegularExpressions.RegexOptions.Singleline);
                    foreach (System.Text.RegularExpressions.Match m in System.Text.RegularExpressions.Regex.Matches(xml, "<UISound\\s+eventName=\"([^\"]+)\"\\s+UIName=\"([^\"]+)\""))
                        if (!uiSoundEvents.ContainsKey(m.Groups[2].Value)) uiSoundEvents[m.Groups[2].Value] = m.Groups[1].Value;
                }
                if (!uiSoundEvents.TryGetValue(uiName, out string path)) { Note(kind + " " + uiName + " (no UISound)"); return; }
                if (!uiSoundPlayer)   // the race's player (FeSounds' own front-end players do not follow the settings)
                {
                    foreach (var a in FindObjectsByType<OriginalAudio>()) if (a.followSettings) { uiSoundPlayer = a; break; }
                    if (!uiSoundPlayer) { Note(kind + " " + uiName + " (no race audio)"); return; }
                    uiSoundPlayer.AddProjects(new[] { "UI_02" }, false);   // UIAudio/SFX; the game's own commands do not wait for it
                    uiSoundWrappers.Clear(); uiSoundLooping.Clear();
                }
                if (!uiSoundWrappers.TryGetValue(path, out uint w)) uiSoundWrappers[path] = w = 0x00fd0001u + 2u * (uint)uiSoundWrappers.Count;   // odd: never a game object
                if (kind == "loop" && uiSoundLooping.Contains(path)) return;
                if (kind == "play" || kind == "stop") uiSoundPlayer.Handle(new ReflexAudioCommand { sub = 1, wrapper = w, serial = ++uiSoundSerial, path = "", position = new float[3], param = "" });
                if (kind == "stop") { uiSoundLooping.Remove(path); Note("stop " + uiName); return; }
                if (kind == "loop") uiSoundLooping.Add(path);
                uiSoundPlayer.Handle(new ReflexAudioCommand { sub = 0, wrapper = w, serial = ++uiSoundSerial, path = path, position = new float[3], param = "" });
                Note(kind + " " + uiName + " -> " + path);
            }
            catch (Exception e) { Note(kind + " " + uiName + " failed: " + e.Message); }
        }
        static void Note(string s) { lock (UiSoundLog) { UiSoundLog.Add(Time.unscaledTime.ToString("F1", System.Globalization.CultureInfo.InvariantCulture) + " " + s); if (UiSoundLog.Count > 512) UiSoundLog.RemoveRange(0, 128); } }

        /// What OriginalUiTree draws as plain boxes, completed (the live layers, OriginalLiveUi): a UIIconGeometryComponent draws its icon
        /// geometry (OriginalIconGeometry: EventIntro's tyres and backing brush stroke) instead of its 1x1White material's rectangle; a
        /// UIMaskContainer clips its children to its box (its MaskingTexture, GradientMaskWhiteCenter of the compass, is opaque white
        /// everywhere: a plain rectangle; RectMask2D).
        public static void Complete(OriginalUiTree.Node n)
        {
            if (n == null) return;
            var st = OriginalUi.Obj(n.json, "style");
            if (n.cls == "UIIconGeometryComponent" && OriginalUi.S(st, "iconGeometry") is string ig && OriginalUiAssets.Data.IconGeometryOf(ig) is Reflex.Port.Content.ReflexUiData.IconGeometry g)
            {
                var go = n.rect.gameObject;
                if (n.graphic) DestroyImmediate(n.graphic);
                var gr = go.AddComponent<OriginalIconGeometry>(); gr.geometry = g; gr.color = n.baseColor; gr.raycastTarget = false;
                n.graphic = gr;
            }
            if (n.cls == "UIMaskContainer" && !n.rect.GetComponent<RectMask2D>()) n.rect.gameObject.AddComponent<RectMask2D>();
            foreach (var c in n.children) Complete(c);
        }
    }

    /// A UIIconGeometryComponent's icon geometry (ReflexUiData.IconGeometry: a triangle list in its asset box, centred, y down, colour per
    /// vertex) stretched to the component's box and tinted by its colour (UIIconGeometryComponent with its 1x1White material).
    public sealed class OriginalIconGeometry : MaskableGraphic
    {
        public Reflex.Port.Content.ReflexUiData.IconGeometry geometry;
        protected override void OnPopulateMesh(VertexHelper vh)
        {
            vh.Clear();
            var g = geometry; if (g == null || g.argb == null || g.width <= 0 || g.height <= 0) return;
            Rect r = rectTransform.rect; Vector2 c = r.center; float sx = r.width / g.width, sy = r.height / g.height;
            Color tint = color;
            for (int i = 0; i < g.argb.Length; i++)
            {
                uint a = g.argb[i];
                var col = new Color(((a >> 16) & 255) / 255f * tint.r, ((a >> 8) & 255) / 255f * tint.g, (a & 255) / 255f * tint.b, (a >> 24) / 255f * tint.a);
                vh.AddVert(new Vector3(c.x + g.xy[2 * i] * sx, c.y - g.xy[2 * i + 1] * sy), col, Vector2.zero);
            }
            for (int i = 0; i + 2 < g.argb.Length; i += 3) vh.AddTriangle(i, i + 1, i + 2);
        }
    }
}
