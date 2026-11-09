using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // MOTOCAREER in the race scene (a career world: ReflexCareer.Current; native/drafts/world_runtime/career.h, _re/notes/career.md).
    // The in-game pages of a series event, driven by the running game's own state through reflex_world_career (between frames, on the
    // simulation thread: OriginalSimulation.BetweenFrames):
    //   rules     the activity holds its race in Pre while its rules page is pending (activity +0xff068, set when the activity type has
    //             a RulesType whose PlayerEvent "Rule_<type>" is not set): RulesPopup (Shared RulesPopup.lyr, RulesPopup.lua: title /
    //             format / description by RulesType); ACCEPT -> End: App.PreActivityRulesPageDone; the toggle ("don't show again") first
    //             sets Rule_<type> (career::world_command "rules_done <0|1>").
    //   post      race state Post: "Show Activity Results" (the series' standings and points, SeriesEventStatus) and PostEventResults with
    //             the game's "Activity Results" table (PopulateResultsTable); the menu: CONTINUE = PostEventMenu:SeriesContinue, RESTART
    //             only with SeriesAllowRestart. SeriesContinue: Moto -> the Moto Results Page (PopulateSeriesMotoResultsTable), then
    //             PostEventOverlay:OnPostStateComplete; other formats -> OnPostStateComplete.
    //   complete  OnPostStateComplete: "HeatOrLCQ" (Heat / Moto with another race; "GateSelection" after the practice qualifier, whose
    //             post event goes straight to the first race's gate pick) -> MXSeriesManager::HandleAdvanceNextSubEvent: the
    //             race's outcome is kept (ReflexCareer.Race.outcomes) and the scene reloads with the world of the series' next race (a
    //             boot that replays the series up to it; ReflexWorldFiles); otherwise the series event completes and saves in this world
    //             (ReflexCareer.Finish: CompleteSeriesEvent unless "Failed", CompleteSeries, the save) and, when CompleteSeriesEvent sent
    //             GotoRewardsPage, the Post Event Reward Page (progress:GetRewards) before the front end.
    [DisallowMultipleComponent]
    public sealed class OriginalCareerRace : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        ReflexCareer.Race race;
        readonly object gate = new object();
        string statusJson, resultsJson, motoJson, outcomeHex;   // answers from the simulation thread
        bool asked, resultsAsked, motoAsked, outcomeAsked, completing;
        string format = "", seriesStatus = "", rulesType = ""; bool allowRestart; int raceState = -1;
        bool liveRules;       // WP-V2: the game's own RulesPopup runs (OriginalLiveUi), not the port's
        bool dontShowRules;   // the rules popup's toggle in this race (the outcome's flag bit 0 for the boots of the next races)
        OriginalRulesPopup rules;
        OriginalPostRace post;

        /// A career race runs in this scene (its race, kept after ReflexCareer.Finish cleared ReflexCareer.Current).
        public bool HasRace => race != null;
        public bool RulesOpen => rules != null && rules.IsOpen;
        public OriginalRulesPopup Rules => rules;
        public OriginalPostRace Post => post;
        public string SeriesStatus => seriesStatus;
        /// The menu's CONTINUE / the moto results' accept (tests drive the pages with these).
        public void Continue() { if (post != null && post.PageName == "Moto") Complete(); else SeriesContinue(); }

        void Awake() { runtime = GetComponent<OriginalWorldRuntime>(); race = ReflexCareer.Current; }

        /// The world was (re)created (OriginalWorldRuntime.Create: a new race from the world image).
        public void OnWorldCreated() { lock (gate) { statusJson = resultsJson = motoJson = outcomeHex = null; } asked = resultsAsked = motoAsked = outcomeAsked = completing = false; raceState = -1; rules?.Close(); if (post != null && post.IsOpen) post.Close(); gateChecked = false; gatePage?.Close(); gatePage = null; }

        // The Gate Selection page first (WP-5, OriginalGateSelection): it holds the race, then the series' own pages follow.
        bool gateChecked; OriginalGateSelection gatePage;
        public OriginalGateSelection GatePage => gatePage;

        void Ask(string command, Action<string> answer)
        {
            var sim = runtime ? runtime.Simulation : null;
            if (sim == null) { answer(null); return; }
            sim.BetweenFrames(w => { string r = ReflexCareer.WorldCommand(w, command); lock (gate) answer(r ?? ""); });
        }

        void Update()
        {
            if (race == null || !runtime || !runtime.Running) return;
            if (!gateChecked) { gateChecked = true; gatePage = OriginalGateSelection.For(runtime, race, reload => { if (!reload) gatePage = null; }); gatePage?.Show(); }
            if (gatePage != null) { gatePage.Tick(Time.unscaledDeltaTime); return; }
            if (liveRules && TryGetComponent<OriginalLiveUi>(out var liveUi) && liveUi.Unavailable) { liveRules = false; asked = false; }   // WP-V2
            if (!asked) { asked = true; Ask("status", r => statusJson = r); }
            string st; lock (gate) { st = statusJson; statusJson = null; }
            if (st != null) OnStatus(st);
            if (rules != null && rules.IsOpen) rules.Tick(Time.unscaledDeltaTime);
        }

        void OnStatus(string json)
        {
            var o = OriginalUi.ParseJson(json) as Dictionary<string, object>;
            if (o == null) { Debug.LogWarning("[Career] no series status: " + json); return; }
            format = OriginalUi.S(o, "format") ?? ""; seriesStatus = OriginalUi.S(o, "status") ?? ""; rulesType = OriginalUi.S(o, "rulesType") ?? "";
            allowRestart = OriginalUi.F(o, "allowRestart") != 0; raceState = (int)OriginalUi.F(o, "raceState", -1);
            bool hold = OriginalUi.F(o, "rulesHold") != 0;
            Debug.Log("[Career] " + race + ": format " + format + ", status " + seriesStatus + ", rules " + (hold ? rulesType : "none") + ", restart " + allowRestart);
            if (!hold) return;
            // WP-V2: a mode's career world (Freestyle, Section Race) runs the game's UI live, whose own RulesPopup holds the race and sets
            // Rule_<type>: no second popup (asked again when the live UI turns out unavailable)
            if (TryGetComponent<OriginalLiveUi>(out var live) && !live.Unavailable) { liveRules = true; Debug.Log("[Career] the game's own RulesPopup (live UI)"); return; }
            if (dontShowRules) { Ask("rules_done 1", _ => { }); return; }   // a restarted race: the profile already has Rule_<type> (this world's copy was reset)
            try
            {
                rules ??= new OriginalRulesPopup(transform);
                rules.Show(rulesType, toggle => { ReflexUiVoice.Stop(runtime ? runtime.gameAudio : null, ReflexUiVoice.RulesWrapper); dontShowRules |= toggle; Ask("rules_done " + (toggle ? 1 : 0), _ => { }); });
                // RulesPopup:Start's narration (App.PlayUILocalizedSoundEvent( "AudioLocalizedActivity", ... ), stopped at End)
                var vo = ReflexUiVoice.RulesVoice(rulesType);
                if (vo.HasValue) ReflexUiVoice.Play(runtime ? runtime.gameAudio : null, vo.Value.db, vo.Value.package, vo.Value.eventPath, ReflexUiVoice.RulesWrapper);
            }
            catch (Exception e) { Debug.LogWarning("[Career] rules popup not built (" + e.Message + "): the rules page is accepted"); Ask("rules_done 0", _ => { }); }
        }

        // ---------------------------------------------------------------------------------------------------- post (OriginalHud)
        /// Replaces OriginalHud's post-race pages for a career race. inPost: race state Post.
        public void TickPost(float dt, bool inPost)
        {
            if (race == null) return;
            if (inPost && !resultsAsked && !OriginalPostRace.OutroHold(transform, dt)) { resultsAsked = true; Ask("results", r => resultsJson = r); }   // after the Event Outro page
            string rj, mj, oh; lock (gate) { rj = resultsJson; resultsJson = null; mj = motoJson; motoJson = null; oh = outcomeHex; outcomeHex = null; }
            if (rj != null) OpenResults(rj);
            if (mj != null) OpenMotoResults(mj);
            if (oh != null) NextRace(oh);
            post?.Tick(dt);
        }

        void OpenResults(string json)
        {
            var o = OriginalUi.ParseJson(json) as Dictionary<string, object>;
            seriesStatus = OriginalUi.S(o, "status") ?? seriesStatus;
            Debug.Log("[Career] results: SeriesEventStatus " + seriesStatus);
            try
            {
                post = new OriginalPostRace(transform, SeriesContinue, () => { if (runtime) runtime.Restart(); }, allowRestart);
                var (headers, rows) = Table(OriginalUi.Obj(o, "table"));
                post.SetHeaders(headers); post.SetRows(rows); post.Open();
            }
            catch (Exception e) { Debug.LogWarning("[Career] post-race pages not built (" + e.Message + ")"); SeriesContinue(); }
        }

        // PostEventMenu:SeriesContinue
        void SeriesContinue()
        {
            if (completing) return;
            if (format == "Moto" && !motoAsked) { motoAsked = true; Ask("moto_results", r => motoJson = r); return; }   // GotoMotoResultsPage
            Complete();
        }

        void OpenMotoResults(string json)
        {
            var o = OriginalUi.ParseJson(json) as Dictionary<string, object>;
            var (headers, rows) = Table(OriginalUi.Obj(o, "table"));
            try { post.ShowMotoResults(rows, headers, Complete); }   // PostEventMotoResults: accept -> PostActivityResultsPageDone
            catch (Exception e) { Debug.LogWarning("[Career] moto results page not built (" + e.Message + ")"); Complete(); }
        }

        // PostEventOverlay:OnPostStateComplete (tour)
        void Complete()
        {
            if (completing) return;
            completing = true;
            if (seriesStatus == "HeatOrLCQ" || seriesStatus == "GateSelection")   // HandleAdvanceNextSubEvent (after a practice: the gate pick) - the series' next race
            {
                if (!outcomeAsked) { outcomeAsked = true; Ask("outcome " + (dontShowRules ? 1 : 0), r => outcomeHex = r); }
                return;
            }
            var world = runtime ? runtime.StopSimulation() : IntPtr.Zero;
            string result = ReflexCareer.Finish(world);
            var rewards = RewardsOf(result);
            if (rewards != null && post != null)
                try { post.ShowRewards(rewards, OriginalPauseMenu.QuitToFrontEnd); return; }   // PostEventRewards: accept -> GotoCareerRaceAndRidePage
                catch (Exception e) { Debug.LogWarning("[Career] rewards page not built (" + e.Message + ")"); }
            OriginalPauseMenu.QuitToFrontEnd();
        }

        void NextRace(string hex)
        {
            if (string.IsNullOrEmpty(hex)) { Debug.LogWarning("[Career] no race outcome: back to the front end"); ReflexCareer.Current = null; OriginalPauseMenu.QuitToFrontEnd(); return; }
            race.outcomes.Add(hex);
            Debug.Log("[Career] next race of the series: " + race);
            if (runtime) runtime.StopSimulation();
            ReflexEventSelect.Launch(race.eventKey, 0, false, SceneManager.GetActiveScene().name);
        }

        /// The "Activity Results" table: row 0's Name / Column0..2 are the column titles; the rows with Index >= 0 sorted by it.
        static (string[] headers, List<OriginalPostRace.Row> rows) Table(Dictionary<string, object> t)
        {
            var rows = new List<OriginalPostRace.Row>(); string[] headers = null;
            var cols = OriginalUi.Obj(t, "columns"); if (cols == null) return (null, rows);
            List<object> C(string n) => cols.TryGetValue(n, out var v) ? v as List<object> : null;
            var index = C("Index"); var name = C("Name"); var c0 = C("Column0"); var c1 = C("Column1"); var c2 = C("Column2");
            string S(List<object> l, int i) => l != null && i < l.Count ? l[i] as string ?? "" : "";
            int n = index?.Count ?? 0;
            if (n > 0) headers = new[] { S(name, 0), S(c0, 0), S(c1, 0), S(c2, 0) };
            var order = new List<(int idx, int row)>();
            for (int i = 0; i < n; i++) if (index[i] is double d && d >= 0) order.Add(((int)d, i));
            foreach (var (idx, row) in order.OrderBy(x => x.idx))
                rows.Add(new OriginalPostRace.Row { index = idx + 1, name = S(name, row), c0 = S(c0, row), c1 = S(c1, row), c2 = S(c2, row) });
            return (headers, rows);
        }

        /// finish_event's rewards when CompleteSeriesEvent sent GotoRewardsPage (null: GotoCareerRaceAndRidePage).
        static OriginalPostRace.Rewards RewardsOf(string finishJson)
        {
            if (finishJson == null) return null;
            var o = OriginalUi.ParseJson(finishJson) as Dictionary<string, object>;
            var rw = OriginalUi.Obj(o, "rewards");
            if (rw == null || OriginalUi.F(rw, "page") == 0) return null;
            var r = new OriginalPostRace.Rewards { prize = OriginalUi.S(rw, "baseText"), bonus = OriginalUi.S(rw, "bonusText"), total = OriginalUi.S(rw, "totalText"),
                                                   credits = OriginalUi.F(rw, "total") >= 0, place = (int)OriginalUi.F(rw, "place", 1000) };
            var unlocks = OriginalUi.Arr(rw, "unlocks");
            if (unlocks != null) foreach (var u in unlocks) r.items.Add(OriginalUi.S(u, "title") ?? "");
            return r;
        }
    }

    // RulesPopup (Shared RulesPopup.lyr, RulesPopup.lua): the darkened box with the rules' title, format and description for the
    // activity's RulesType; ACCEPT (Enter / the pad's A / a click on AcceptButtonText) or the toggle (F1 / X / ToggleButtonText:
    // OnToggleKeypress, "don't show again"). Back does nothing (OnBackKeypress: disabled).
    public sealed class OriginalRulesPopup
    {
        static readonly Dictionary<string, (string title, string format, string description)> Texts = new Dictionary<string, (string, string, string)>
        {
            ["Freestyle"] = ("STR_RULE_TITLE_FREESTYLE", "STR_RULE_FORMAT_FREESTYLE", "STR_RULE_DESCRIPTION_FREESTYLE"),
            ["Freeride"] = ("STR_RULE_TITLE_FREERIDE", null, "STR_RULE_DESCRIPTION_FREERIDE"),
            ["SectionRace"] = ("STR_RULE_TITLE_SECTIONRACE", null, "STR_RULE_DESCRIPTION_SECTIONRACE"),
            ["Faceoff"] = ("STR_RULE_TITLE_FACEOFF", null, "STR_RULE_DESCRIPTION_FACEOFF"),
            ["Eliminator"] = ("STR_RULE_TITLE_ELIMINATION", null, "STR_RULE_DESCRIPTION_ELIMINATION"),
            ["Overtake"] = ("STR_RULE_TITLE_OVERTHROW", null, "STR_RULE_DESCRIPTION_OVERTHROW"),
            ["SpeednStyle"] = ("STR_RULE_TITLE_SPEEDSTYLE", null, "STR_RULE_DESCRIPTION_SPEEDSTYLE"),
            ["Outrun"] = ("STR_RULE_TITLE_OUTRUNNER", null, "STR_RULE_DESCRIPTION_OUTRUNNER"),
            ["Rival"] = ("STR_RULE_TITLE_RIVAL", null, "STR_RULE_DESCRIPTION_RIVAL"),
            ["MotoPractice"] = ("STR_RULE_TITLE_MOTO_QUALIFIER", "STR_RULE_FORMAT_MOTO_QUALIFIER", "STR_RULE_DESCRIPTION_MOTO_QUALIFIER"),
            ["HeatPractice"] = ("STR_RULE_TITLE_HEAT_QUALIFIER", "STR_RULE_FORMAT_HEAT_QUALIFIER", "STR_RULE_DESCRIPTION_HEAT_QUALIFIER"),
            ["Heat"] = ("STR_RULE_TITLE_HEAT", "STR_RULE_FORMAT_HEAT", "STR_RULE_DESCRIPTION_HEAT"),
            ["LCQ"] = ("STR_RULE_TITLE_HEAT_LCQ", "STR_RULE_FORMAT_HEAT_LCQ", "STR_RULE_DESCRIPTION_HEAT_LCQ"),
            ["HeatMainEvent"] = ("STR_RULE_TITLE_HEAT_MAIN", "STR_RULE_FORMAT_HEAT_MAIN", "STR_RULE_DESCRIPTION_HEAT_MAIN"),
            ["Moto"] = ("STR_RULE_TITLE_MOTO", "STR_RULE_FORMAT_MOTO", "STR_RULE_DESCRIPTION_MOTO"),
        };

        readonly Canvas canvas; readonly OriginalUiTree tree; readonly Dictionary<string, string> loc, shared;
        Action<bool> done;
        public bool IsOpen => done != null;
        public static string LastShown;   // the RulesType last shown (tests)

        public OriginalRulesPopup(Transform parent)
        {
            var data = OriginalUiAssets.Data; loc = data.Strings("InGame", "MXRaven_Default_Strings");
            try { shared = data.Strings("Shared", "MXRaven_Default_Strings"); } catch (Exception) { shared = null; }
            var cgo = new GameObject("Original rules popup", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
            cgo.transform.SetParent(parent, false);
            canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 60;
            var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
            var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
            var root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
            tree = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
            var l = data.BuildLayer("Shared", "RulesPopup.lyr");
            foreach (var e in OriginalUi.Arr(l, "elements")) tree.Build((Dictionary<string, object>)e, root, null);
            tree.AddAnimations(OriginalUi.Arr(l, "layerAnimations"));
            Clickable(tree.Find("AcceptButtonText"), () => Finish(false));
            Clickable(tree.Find("ToggleButtonText"), () => Finish(true));
            canvas.enabled = false;
        }

        string Str(string key) => key == null ? "" : loc.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v) ? v : shared != null && shared.TryGetValue(key, out v) ? v : key;

        static void Clickable(OriginalUiTree.Node n, Action onClick)
        {
            if (n == null || !n.graphic) return;
            n.graphic.raycastTarget = true;
            var b = n.rect.gameObject.AddComponent<Button>(); b.transition = Selectable.Transition.None; b.onClick.AddListener(() => onClick());
        }

        /// RulesPopup:Start for `rulesType`; `onDone(toggle)` when it is accepted (toggle: OnToggleKeypress).
        public void Show(string rulesType, Action<bool> onDone)
        {
            LastShown = rulesType;
            Texts.TryGetValue(rulesType ?? "", out var t);
            void Set(string node, string key, bool wrap)
            {
                var n = tree.Find(node); OriginalUiTree.SetVisible(n, key != null);
                if (n == null || key == null) return;
                string s = Str(key);
                if (wrap && n.text && n.text.font != null && n.rect.sizeDelta.x > 0) s = n.text.font.Wrap(s, n.text.textHeight, n.rect.sizeDelta.x);
                OriginalUiTree.SetText(n, s);
            }
            Set("TitleText", t.title, false); Set("FormatText", t.format, true); Set("MessageText", t.description, true);
            done = onDone; canvas.enabled = true; Reflex.Port.Platform.ReflexCursor.Want(canvas, true);
            Debug.Log("[Career] rules popup: " + rulesType);
        }

        /// ACCEPT (toggle false) or the toggle (OnToggleKeypress, "don't show again").
        public void Finish(bool toggle) { var d = done; if (d == null) return; Close(); d(toggle); }
        public void Close() { done = null; canvas.enabled = false; Reflex.Port.Platform.ReflexCursor.Want(canvas, false); }

        public void Tick(float dt)
        {
            if (!IsOpen) return;
            tree.Tick(dt);
            var kb = Keyboard.current; var gp = Gamepad.current;
            if (kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame) || gp != null && gp.buttonSouth.wasPressedThisFrame) Finish(false);
            else if (kb != null && kb.f1Key.wasPressedThisFrame || gp != null && gp.buttonWest.wasPressedThisFrame) Finish(true);
        }
    }
}
