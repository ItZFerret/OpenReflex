using System;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    // The Event Intro page (InGame EventIntro.lyr + EventIntro.lua, _re/notes/in-race-pages.md): while the race activity's intro scenario plays
    // (Pre, after the rules page: UIRoutingMessage GotoEventIntroPage, MXActivityRaceStub::vf25 005681e0, until the HUD page at the countdown) the
    // event's title (EventText: EventTypeLocations row of the event's type / location / track -> MXTableHelper:TitleLocalizationString( StoreProductId ))
    // over its location (LocationText: EventLocation.LocalizationId; hidden in the Free Ride) sits over the fly-by, in 0.4 s (ScenarioHeaderIn).
    // Accept (EventIntro's UIKeypressTrigger MapKey_UI_Accept -> SkipToNextScenario -> [0xa7f748] = 1) ends the intro; the pad's X does the same in the
    // game's own button test. This page is drawn in the OPEN restyle (the dotted patch and the orange bar of the career pages) with the original's
    // texts. The "skip" legend is a port addition: the original page carries no text for it.
    // The Event Outro page (EventOutro.lyr + EventOutro.lua: an empty layer, focused, with the same UIKeypressTrigger MapKey_UI_Accept ->
    // SkipToNextScenario; Present turns AllowPause off) is up while a post-race scenario plays: Post 1, the RaceFinishScenario (the finish replay,
    // the winner, the walk, the podium) and Post 3, the career's RaceResultsScenario. Its scenarios are skippable (vf5 00794f70 returns 1) and have
    // no next step to skip to (vf4 0074fad0), so Accept (Enter, the pad's A: KeyBind_Core.xml MapKey_UI_Accept1 / 2) ends the whole scenario and
    // the game goes on to its results (w-uibatch: "the cutscenes after a race are unskippable"; it was believed nothing read the word during Post,
    // but MXActivityRace_UpdatePost 00569a90 runs the same 007473f0 on the finish and results scenarios). Here the same skip as the intro's, and
    // the same port-added legend; the race's own input is held by the results presentation in Post, so Accept is read here.
    // A skip does not always end the intro: in a National / Supercross (activity type 0 / 1) RaceIntroScenario::vf4 (00749780) jumps to its last step,
    // the start board shot (step 6, about 4 s), which vf5 (00749fb0, LoopScenario 0) does not let the player skip, and the race waits for the start
    // board after it; every other intro ends at once. The card and its legend go as soon as the skip is sent (w-raceflow): the title over the board
    // shot read as a skip that did nothing, and a second press has nothing left to skip.
    [DisallowMultipleComponent]
    public sealed class OwnEventIntro : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        GameObject host; VisualElement root, layer, card, hintBar;
        Label eventText, locationText;
        string title = "", location = ""; bool freeRide;
        bool shown, everShown, doneForWorld; float shownAt, nextWhy; bool skipSent;
        VisualElement outroBar; bool outroShown, outroSkipSent, outroHeld; float outroAt; int outroPost = -1;
        public bool OutroShown => outroShown;
        public static int OutroSkips;   // tests

        public bool Shown => shown;
        public string EventTextValue => title;
        public string LocationTextValue => location;
        public bool LocationVisible => shown && !freeRide && !string.IsNullOrEmpty(location);
        public static int Skips;   // tests

        /// The event's title and location as EventIntro:Start computes them (title: "Unknown Event Type Location" without a row).
        public static void Texts(int eventTypeId, int eventLocationId, int trackId, out string eventLine, out string locationLine)
        {
            eventLine = "Unknown Event Type Location"; locationLine = "";
            try
            {
                var ev = ReflexEvents.For(eventTypeId, eventLocationId, trackId);
                var tables = FeTables.Shared;
                if (ev != null)
                {
                    string key = tables.TitleKey(ev.storeProductId);
                    if (!string.IsNullOrEmpty(key)) eventLine = OpenContent.Str(key, key);
                    if (ev.locationLocId != -1) { string lk = tables.LocalizationKey(ev.locationLocId); if (!string.IsNullOrEmpty(lk)) locationLine = OpenContent.Str(lk, lk); }
                    return;
                }
                // not an event the port runs (a career series' location): the same rows straight from the tables
                var rows = ReflexEvents.Tables;
                if (rows.TryGetValue("EventTypeLocations", out var etl))
                {
                    var row = etl.FirstOrDefault(r => Rfx.Int(r, "EventTypeId") == eventTypeId && Rfx.Int(r, "EventLocationId") == eventLocationId && Rfx.Int(r, "TrackId", 0) == trackId);
                    if (row != null) { string key = tables.TitleKey(Rfx.Int(row, "StoreProductId")); if (!string.IsNullOrEmpty(key)) eventLine = OpenContent.Str(key, key); }
                }
                if (rows.TryGetValue("EventLocation", out var el))
                {
                    var row = el.FirstOrDefault(r => Rfx.Int(r, "EventLocationId") == eventLocationId);
                    if (row != null) { int id = Rfx.Int(row, "LocalizationId", -1); if (id != -1) { string lk = tables.LocalizationKey(id); if (!string.IsNullOrEmpty(lk)) locationLine = OpenContent.Str(lk, lk); } }
                }
            }
            catch (Exception e) { Debug.LogWarning("[EventIntro] texts: " + e.Message); }
        }

        /// The showcase / tests: the card on screen without a world (the title and the location as given).
        internal static OwnEventIntro Preview(GameObject host, string eventLine, string locationLine, bool freeRide = false)
        {
            var a = host.GetComponent<OwnEventIntro>() ?? host.AddComponent<OwnEventIntro>();
            a.title = eventLine; a.location = locationLine; a.freeRide = freeRide;
            a.Show();
            return a;
        }
        internal VisualElement CardRoot => root;

        public static OwnEventIntro Attach(OriginalWorldRuntime rt, int eventTypeId, int eventLocationId, int trackId, bool freeRide, ReflexEvents.Event ev = null)
        {
            if (!rt) return null;
            var a = rt.GetComponent<OwnEventIntro>() ?? rt.gameObject.AddComponent<OwnEventIntro>();
            a.runtime = rt; rt.eventIntro = a; a.freeRide = freeRide; a.doneForWorld = false; a.everShown = false; a.skipSent = false;
            Texts(eventTypeId, eventLocationId, trackId, out a.title, out a.location);
            // RESTORED CONTENT (championship weekend): the stage this session races ("NATIONAL  ·  MOTO 1")
            string stage = ev != null ? RxChampWeekend.ArmedStageLabel(ev.location) : null;
            if (stage == null && RxChampWeekend.StageInSession) stage = RxChampWeekend.StageLabel(RxChampWeekend.SessionStage);
            if (!string.IsNullOrEmpty(stage)) a.title = (a.title ?? "") + "  ·  " + stage;
            if (a.eventText != null) a.Fill();
            Debug.Log("[EventIntro] " + a.title + " / " + (freeRide ? "(free ride: no location)" : a.location));
            return a;
        }

        void Build()
        {
            if (root != null) return;
            OuUi.EnsureEventSystem();
            host = new GameObject("Open Reflex event intro"); host.transform.SetParent(transform, false);
            root = OuUi.CreateDocument(host, 28); OuTheme.Apply(root);
            root.style.display = DisplayStyle.None; root.pickingMode = PickingMode.Ignore;
            layer = OuUi.Div("mxa-layer oc-intro-layer", root); layer.pickingMode = PickingMode.Ignore;
            card = OuUi.Div("oc-intro-card", layer); card.pickingMode = PickingMode.Ignore;
            // HeaderContainer: the event's title on the dotted patch, the location on the orange bar under it
            var patch = OuUi.Div("oc-intro-patch", card); patch.pickingMode = PickingMode.Ignore; patch.style.backgroundImage = OcTex.Tile;
            eventText = OuUi.Text("", "oc-intro-title", patch); eventText.enableRichText = false;
            var bar = OuUi.Div("oc-intro-bar", card); bar.pickingMode = PickingMode.Ignore;
            locationText = OuUi.Text("", "oc-intro-loc", bar); locationText.enableRichText = false;
            hintBar = OuUi.Div("oc-intro-hint", layer); hintBar.pickingMode = PickingMode.Ignore;
            OuUi.Hints(hintBar, (OuUi.KeySubmit, "SKIP INTRO"));
            outroBar = OuUi.Div("oc-intro-hint", layer); outroBar.pickingMode = PickingMode.Ignore;
            OuUi.Hints(outroBar, (OuUi.KeySubmit, "SKIP"));
            outroBar.style.display = DisplayStyle.None;
            Fill();
        }

        void Fill()
        {
            if (eventText == null) return;
            eventText.text = (title ?? "").ToUpperInvariant();
            locationText.text = location ?? "";
            locationText.parent.style.display = freeRide || string.IsNullOrEmpty(location) ? DisplayStyle.None : DisplayStyle.Flex;
        }

        // the page is up from the end of the rules page until the HUD page (the countdown): Pre, no rules hold, the intro scenario playing
        bool Wanted()
        {
            if (!runtime || !runtime.Running || runtime.paused || doneForWorld) return false;
            if (runtime.EventIdentity != null && (runtime.EventIdentity.StartsWith("Tutorial/", StringComparison.Ordinal))) return false;
            if (ReflexGarage.TestDrive.Active) return false;
            var ui = runtime.EventUi;
            if (ui.race_state != 0 || ui.rules_hold != 0) return false;
            if (skipSent) return false;   // skipped: the rest of the intro (a National's start board shot) plays without the card
            return runtime.ScenarioKind == 3;
        }

        // the Event Outro page's window: a post-race scenario plays (Post 1 / 3) and no page of ours is over the world
        bool OutroWanted()
        {
            if (!runtime || !runtime.Running || runtime.paused) return false;
            if (runtime.EventIdentity != null && runtime.EventIdentity.StartsWith("Tutorial/", StringComparison.Ordinal)) return false;
            if (ReflexGarage.TestDrive.Active) return false;
            var ui = runtime.EventUi;
            if ((ui.known & RxEventUiKnown.PostState) == 0 || ui.race_state != 4 || (ui.post_state != 1 && ui.post_state != 3)) return false;
            if (runtime.ScenarioKind != 1 && runtime.ScenarioKind != 2) return false;
            if (runtime.ViewerActive || runtime.OwnPauseActive || runtime.CareerPageUp || runtime.raceResults && runtime.raceResults.Active) return false;
            return true;
        }

        void UpdateOutro()
        {
            int post = runtime.EventUi.post_state;
            bool want = OutroWanted() && !(outroSkipSent && post == outroPost);
            if (want && !outroShown)
            {
                Build();
                outroShown = true; outroAt = Time.unscaledTime; outroPost = post; outroSkipSent = false;
                outroHeld = AcceptHeld();   // a button held across the finish line is not a skip: it has to be let go first
                root.style.display = DisplayStyle.Flex; OuMotion.Sync(root);
                card.style.display = DisplayStyle.None; hintBar.style.display = DisplayStyle.None; outroBar.style.display = DisplayStyle.Flex;
                outroBar.style.opacity = 0f;
                Debug.Log("[EventOutro] shown (post " + post + ", scenario " + runtime.ScenarioKind + ")");
            }
            else if (!want && outroShown) HideOutro();
            if (!outroShown) return;
            if (outroHeld) outroHeld = AcceptHeld();
            else if (AcceptPressed() && !outroSkipSent && Time.unscaledTime - outroAt > 0.25f) SkipOutro();
            outroBar.style.opacity = Mathf.Clamp01((Time.unscaledTime - outroAt - 1.2f) / 0.5f);
        }

        void HideOutro()
        {
            outroShown = false;
            if (outroBar != null) outroBar.style.display = DisplayStyle.None;
            if (card != null) card.style.display = DisplayStyle.Flex;
            if (hintBar != null) hintBar.style.display = DisplayStyle.Flex;
            if (!shown && root != null) root.style.display = DisplayStyle.None;
        }

        static bool AcceptPressed()
        {
            var kb = Keyboard.current; bool accept = kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame);
            foreach (var gp in Gamepad.all) if (gp.buttonSouth.wasPressedThisFrame) accept = true;
            return accept;
        }
        static bool AcceptHeld()
        {
            var kb = Keyboard.current; bool held = kb != null && (kb.enterKey.isPressed || kb.numpadEnterKey.isPressed);
            foreach (var gp in Gamepad.all) if (gp.buttonSouth.isPressed) held = true;
            return held;
        }

        /// The Event Outro page's SkipToNextScenario: the post-race scenario ends at its next update (the results follow).
        public void SkipOutro()
        {
            var open = runtime ? runtime.SimDriver?.Open : null;
            if (open == null) return;
            outroSkipSent = true; OutroSkips++;
            open.BetweenFrames(s => { try { RxApiNative.rx_session_skip_scenario(s); } catch (EntryPointNotFoundException) { } });
            if (outroBar != null) outroBar.style.display = DisplayStyle.None;
            Debug.Log("[EventOutro] skip requested (post " + outroPost + ")");
        }

        void Update()
        {
            if (!runtime) return;
            UpdateOutro();
            if (outroShown) return;
            if (everShown && runtime.EventUi.race_state >= 1) doneForWorld = true;   // the HUD page took over for this world (a world's earlier activity may be past Pre)
            bool want = Wanted();
            if (!want && runtime.ScenarioKind == 3 && !skipSent && Time.unscaledTime > nextWhy) { nextWhy = Time.unscaledTime + 2f; Debug.Log("[EventIntro] not shown: running " + runtime.Running + " paused " + runtime.paused + " done " + doneForWorld + " state " + runtime.EventUi.race_state + " hold " + runtime.EventUi.rules_hold + " id " + runtime.EventIdentity); }
            if (want && !shown) Show();
            else if (!want && shown) Hide();
            if (!shown) return;
            // accept = SkipToNextScenario
            var kb = Keyboard.current; bool accept = kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame);
            foreach (var gp in Gamepad.all) if (gp.buttonSouth.wasPressedThisFrame) accept = true;
            if (accept && !skipSent && Time.unscaledTime - shownAt > 0.25f) Skip();
            hintBar.style.opacity = Mathf.Clamp01((Time.unscaledTime - shownAt - 1.2f) / 0.5f);   // the legend comes in after a moment
        }

        void Show()
        {
            Build();
            shown = true; everShown = true; shownAt = Time.unscaledTime; skipSent = false;
            root.style.display = DisplayStyle.Flex;
            OuMotion.Sync(root);
            card.RemoveFromClassList("ou-in"); OuMotion.Enter(card, 0f, true);   // ScenarioHeaderIn
            hintBar.style.opacity = 0f;
            Debug.Log("[EventIntro] shown: " + title + (freeRide ? "" : " / " + location));
        }

        void Hide()
        {
            shown = false;
            if (root != null) root.style.display = DisplayStyle.None;
        }

        /// SkipToNextScenario: the core's [0xa7f748] = 1 (rx_session_skip_scenario), consumed by the intro scenario's next update.
        public void Skip()
        {
            var open = runtime ? runtime.SimDriver?.Open : null;
            if (open == null) return;
            skipSent = true; Skips++;
            open.BetweenFrames(s => { try { RxApiNative.rx_session_skip_scenario(s); } catch (EntryPointNotFoundException) { } });
            Debug.Log("[EventIntro] skip requested");
        }

        void OnDestroy() { if (host) Destroy(host); }
    }
}
