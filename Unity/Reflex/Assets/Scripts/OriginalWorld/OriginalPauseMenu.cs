using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.UI;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The original pause menu (page "Paused Menu Page": PauseMenu.lyr + Header.lyr, PauseMenu.lua; layouts, fonts, art and
    // strings read at run time from the install's Database/MXUI by ReflexUiData), rebuilt with the original layout, fonts, art and strings:
    //   backer (0,0,0,140), the list at (149,75) (rows Item0..7: Background, CursorBackground focused (217,164,65) /
    //   focusable (12,23,25,128), Text focused (14,15,15) / focusable (89,88,78)), the slanted "PAUSE" header.
    // The rows are PauseMenu:Start's, decided by the game's own Lua in the running world (OriginalEventIntro.PauseRows; WP-5): a race
    // RESUME, RESTART, TRICK LIBRARY, OPTIONS, QUIT; Free Ride RESUME, VEHICLE TUNING (the equipped vehicle is the player's), VEHICLE
    // LIST, TRICK LIBRARY, OPTIONS, QUIT (verified against the original's own pause page rendered by the boot's recording device:
    // scratchpad/wp5/shots/orig_pause_*.png). RESTART -> Restart Event confirmation popup, QUIT -> Quit Event confirmation popup,
    // OPTIONS -> the game's own PauseOptions panel (below), TRICK LIBRARY -> StuntPreview.component (below), VEHICLE LIST ->
    // OnVehicleList (the quick view; OriginalQuickView), VEHICLE TUNING -> OnVehicleTuning (the GarageVehicleTuning popup: WP-G).
    // OnPauseListFocusedChanged: the container of the highlighted row shows - RESUME: Free Ride's map (FreerideContainer: the level's
    // HUDMapIconId image, the riders' arrows from App.GetPlayerPosition / GetPlayerRotation) or an activated challenge's description
    // (ChallengeEventContainer); TRICK LIBRARY: the stunt list; OPTIONS: PauseOptions.
    // App.EnableCameraBlur( TRUE, 0 ) of PauseMenu:Start enables the BlurTransitionEffect of the view's post settings (00743240: post +0 = 1,
    // +0xb0 effect +4 on, +8 mode 3), but the PC build draws no blur: the original's pause frame recorded by the boot's software D3D9 device
    // has the same post chain as a race frame (copies, bright pass, luminance, adaptation, bloom, tonemap) and a sharp 3D scene under the
    // backer. So the port draws none either (1:1).
    // The pause is the game's App.AllowPause: none on the Event Intro / Outro pages (OriginalEventIntro.PauseAllowed). Esc opens it (and
    // is Back / Resume, as in the game), Up/Down move, Enter accepts, F3 (Start) resumes; the mouse clicks rows and buttons.
    // PauseOptions (PauseOptions.component at (641,67), PauseOptions.lua): shown while the OPTIONS row is highlighted, focused by
    // accepting it (the ButtonLegend shows BACK; Esc / BACK / a click on the legend returns to the list). Rows: Driving Camera
    // (STR_OPT_DC_1..4), HUD Visibility (ON/OFF), Online Player Name (ON/OFF), Effects / Music / Engine volume (0..100 % in steps
    // of 10); Left/Right change the value (arrows hidden at the ends), changes apply at once and persist (the game's UI settings
    // DrivingCamera, HUDVisibility, OnlinePlayerName, EffectsVolume, MusicVolume, EngineVolume). The focused row is gold
    // (FocusedColor), the others FocusableColor. The client maps the settings onto what it has: Driving Camera -> the game's own
    // UI setting in the running world (its Visual / Game Play / User / 1st Person presets, OriginalInput) on the game camera, HUD Visibility -> the HUD,
    // Effects / Music -> the game-sound and music volume; Online Player Name has no effect offline and EngineVolume is stored
    // for the audio (OriginalPauseMenu.EngineVolume).
    // StuntPreview (StuntPreview.component at (660,50), StuntPreview.lua): every stunt of the four tiers (App.GetStuntDescriptorTierContainer,
    // a display name already listed skipped) in its 7-row dynamic list (name from MXRaven_Stunts, gesture image material "Stunt_" + the key
    // without "STR_"), the focused stunt's tier (STR_STUNT_TIER1..4), the scrollbar; accepting TRICK LIBRARY focuses it (BACK returns);
    // OnAcceptStuntList previews the stunt only with the garage loaded (MXGarage:IsLoaded: never in a race), so accept does nothing here.
    // A world whose UI runs live (OriginalLiveUi: the arcade races and the modes) pauses as the PC game does: Esc is MapKey_UI_Pause1 (and
    // MapKey_UI_Back1) in KeyBind_Core.xml, so the key goes to the game (OriginalWorldRuntime forwards it: GameOwnsPause), which pauses
    // itself (MXUIManager_OnPauseRequest) and shows its own "Paused Menu Page" - rows, TRICK LIBRARY, OPTIONS, VEHICLE LIST, the Free Ride
    // map - mirrored live; this page then stays closed. It remains the pause page of the worlds without the live UI (a MOTOCAREER race).
    // The client's additions (Open Unleashed's pause QUICK SETTINGS, Options): after the game's own rows (RowKeys stays the game's),
    // three more rows built from the list's own row (its layout, art, font and colours): CAMERA (the game's four presets and the
    // Dynamic FPV view; Left / Right or Enter cycle), HUD STYLE (ORIGINAL / MODERN, at once), ALL OPTIONS (the full pages:
    // Reflex.Port.Options.ReflexOptions; this page stays open under them). Over the game's own live pause page the same three are
    // a panel beside it (Reflex.Port.Options.ReflexPauseExtras).
    [DisallowMultipleComponent]
    public sealed class OriginalPauseMenu : MonoBehaviour
    {
        /// The game's own pause page answers Esc in this world (its UI runs live, the race is not in the port's post-race pages).
        public static bool GameOwnsPause(OriginalWorldRuntime rt)
        {
            if (!rt || OriginalPostRace.Active) return false;
            // the plugin's pages (pages.cpp: the COM stubs and the quick view patch the game's own pause page needs) answered for this world
            if (OriginalEventIntro.NativeMissing || !rt.TryGetComponent<OriginalEventIntro>(out var pages) || !pages.HasState) return false;
            return rt.TryGetComponent<OriginalLiveUi>(out var live) && live.enabled && live.Running && !live.Unavailable;
        }

        public OriginalWorldRuntime runtime;
        public bool IsOpen { get; private set; }

        /// The VEHICLE TUNING row's action (PauseMenu:OnVehicleTuning: ShowVehicleTuningPopup; the GarageVehicleTuning popup, WP-G); null: nothing.
        public static Action<OriginalPauseMenu> OnVehicleTuning;
        /// The VEHICLE LIST row's action in a world whose UI is not live (PauseMenu:OnQuickViewKeypress: ShowVehiclesQuickViewPopup; a live
        /// world shows the game's own popup, OriginalQuickView); null: nothing.
        public static Action<OriginalPauseMenu> OnVehicleList;

        static readonly Color CursorFocused = new Color(217 / 255f, 164 / 255f, 65 / 255f, 1f), CursorIdle = new Color(12 / 255f, 23 / 255f, 25 / 255f, 128 / 255f);
        static readonly Color TextFocused = new Color(14 / 255f, 15 / 255f, 15 / 255f, 1f), TextIdle = new Color(89 / 255f, 88 / 255f, 78 / 255f, 1f);
        static readonly Color ButtonText = new Color(170 / 255f, 169 / 255f, 151 / 255f, 1f);
        // the race's rows when the world cannot tell (an older plugin): what PauseMenu:Start gives a single-player race
        static readonly string[] RaceRows = { "STR_RESUME_GAME", "STR_RESTART_GAME", "STR_STUNTS", "STR_OPTIONS", "STR_QUIT" };

        Canvas canvas; OriginalUiTree tree; RectTransform root;
        OriginalButtonLegend legend;
        readonly List<(string text, Action action)> items = new List<(string, Action)>();
        readonly List<string> kinds = new List<string>();
        readonly List<OriginalUiTree.Node> rows = new List<OriginalUiTree.Node>();
        readonly List<OriginalUiTree.Node> allRows = new List<OriginalUiTree.Node>();
        int focus; bool pausedBefore;
        Dictionary<string, string> loc;
        OriginalConfirmPopup popups;   // the Restart / Quit Event confirmation popups
        OriginalEventIntro pages;
        FeAssets assets;

        /// The rows shown (localization keys, in order), for tests.
        public IReadOnlyList<string> RowKeys => kinds;
        public string FocusedRow => focus < kinds.Count ? kinds[focus] : null;
        public bool StuntsFocused => stuntsFocused;
        public int StuntCount => stunts.Count;
        public string Tier => tierText != null && tierText.text ? tierText.text.Text : null;

        string Str(string key, string fallback) => loc != null && loc.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v) ? v : fallback;

        void Update()
        {
            if (runtime && (runtime.OwnMenuActive || runtime.racePause && runtime.racePause.OwnsInput)) return;
            if (runtime && !pages) pages = OriginalEventIntro.For(runtime);
            if (IsOpen && legend != null) { float dt = Time.unscaledDeltaTime; legend.Tick(dt); tree.Tick(dt); }
            if (pendingOpen) { TickPendingOpen(); return; }
            var kb = Keyboard.current; var pad = OriginalPadInput.Read();
            if (kb == null && !pad.Any && !IsOpen) return;
            // the keyboard and every gamepad as one set of buttons (OriginalPadInput: the d-pad / left stick step, A accepts, B backs, Start is the game's Start)
            bool up = kb != null && kb.upArrowKey.wasPressedThisFrame || pad.up, down = kb != null && kb.downArrowKey.wasPressedThisFrame || pad.down;
            bool left = kb != null && kb.leftArrowKey.wasPressedThisFrame || pad.left, right = kb != null && kb.rightArrowKey.wasPressedThisFrame || pad.right;
            bool accept = kb != null && kb.enterKey.wasPressedThisFrame || pad.accept, back = kb != null && kb.escapeKey.wasPressedThisFrame || pad.back;
            bool start = kb != null && kb.f3Key.wasPressedThisFrame || pad.start, openKey = kb != null && kb.escapeKey.wasPressedThisFrame || pad.start;
            bool optionsOpen = runtime && runtime.optionsMenu && (runtime.optionsMenu.IsOpen || runtime.optionsMenu.ClosedFrame == Time.frameCount)
                               || Reflex.Port.Options.ReflexOptions.IsOpen || Reflex.Port.Options.ReflexOptions.ClosedFrame == Time.frameCount;
            if (IsOpen && canvas && canvas.enabled == Reflex.Port.Options.ReflexOptions.IsOpen) { canvas.enabled = !Reflex.Port.Options.ReflexOptions.IsOpen; if (canvas.enabled) RefreshExtras(); }   // the full options pages over it
            if (!IsOpen)
            {
                if (openKey && !OriginalOnboardAdjust.KeyboardCaptured && !optionsOpen && !OriginalPostRace.Active && !GameOwnsPause(runtime) && Allowed()) Open();   // no pause page while the post-race pages are up
                return;
            }
            if (optionsOpen) return;   // the options panel (from OPTIONS) has the keyboard; closing it returns here
            if (popups != null && popups.IsOpen) { popups.HandleKeys(kb, pad); return; }
            if (optionsFocused)   // PauseOptions: NavList up/down, the row's list left/right; Back returns to the pause list
            {
                if (up) FocusOption(optFocus - 1);
                if (down) FocusOption(optFocus + 1);
                if (left) ChangeOption(optFocus, -1);
                if (right) ChangeOption(optFocus, +1);
                if (back) FocusOptions(false);
                else if (start) Close();
                return;
            }
            if (stuntsFocused)   // StuntPreview: the list up / down; Back returns to the pause list (OnBackKeypress), Start resumes
            {
                if (up) FocusStunt(stuntIndex - 1);
                if (down) FocusStunt(stuntIndex + 1);
                if (back) FocusStunts(false);
                else if (start) Close();
                return;
            }
            if (up) Focus(focus - 1);
            if (down) Focus(focus + 1);
            if (focus >= extraStart && focus - extraStart < extraAdjust.Count && extraAdjust[focus - extraStart] != null)   // the client's value rows
            {
                if (left) { extraAdjust[focus - extraStart](-1); RefreshExtras(); }
                if (right) { extraAdjust[focus - extraStart](+1); RefreshExtras(); }
            }
            if (accept) items[focus].action();
            else if (back || start) Close();
        }

        // MXUIManager_OnPauseRequest: App.AllowPause (the Event Intro / Outro pages turn it off), no pause over the rules popup
        bool Allowed()
        {
            if (pages && !pages.PauseAllowed) return false;
            if (runtime && runtime.postChallenge && runtime.postChallenge.Active) return false;   // PostChallengePopup:Start App.AllowPause( FALSE )
            // a mode the owned pause page covers (OwnRacePause): it opens in its own windows (the countdown, the event in progress), not this page
            if (runtime && runtime.racePause && !(runtime.EventIdentity?.StartsWith("Tutorial/", StringComparison.Ordinal) ?? false) && !Reflex.Port.Content.ReflexGarage.TestDrive.Active
                && (runtime.EventUi.known & Reflex.Port.RxCore.RxEventUiKnown.ActivityType) != 0 && Reflex.Port.OpenFrontEnd.OwnModes.PauseType(runtime.EventUi.activity_type)) return false;
            // in a Free Ride activation area the pause key starts the area's challenge instead (MXUIManager_OnPauseRequest 00598e40)
            if (runtime && runtime.TryGetComponent<Reflex.Port.OpenFrontEnd.OwnActivationArea>(out var area) && area.ClaimsPause) return false;
            if (runtime && runtime.TryGetComponent<OriginalCareerRace>(out var cr) && cr.RulesOpen) return false;
            return true;
        }

        // ------------------------------------------------------------------------------------- open / close
        // Free Ride's RESUME shows the riders on the map (App.GetPlayerPosition / GetPlayerRotation at PauseMenu:Start): asked of the world
        // before the pause stops its frames (at most a few frames).
        bool pendingOpen; float pendingWait; List<(float x, float z, float rot, bool local)> pendingPoints; readonly object pointsGate = new object();
        public void Open()
        {
            if (runtime && runtime.OwnResultsPresentation) return;
            if (runtime && runtime.tutorial) { runtime.tutorial.PauseRequest(); return; }   // MotoSkills: the pause request in tutorial mode is GotoTutorialPauseMenuPage (OpenFrontEnd/OpenTutorial.cs)
            if (runtime && runtime.racePause && runtime.racePause.CanOpen) { runtime.racePause.Open(); return; }
            if (IsOpen || pendingOpen) return;
            if (runtime && runtime.loadArena) { runtime.loadArena.PauseRequest(); return; }   // the test-ride arena while an event loads: START (OpenFrontEnd/OpenLoadArena.cs)
            if (runtime && runtime.testRide) { runtime.testRide.Open(); return; }   // the open core's TEST RIDE: its own pause page (OpenFrontEnd/OpenTestRide.cs)
            if (Reflex.Port.Content.ReflexGarage.TestDrive.Active) { QuitToFrontEnd(); return; }   // WP-G: a test drive's pause request ends it (MXUIManager_OnPauseRequest 00598e40)
            if (!canvas && !Build()) return;
            var sim = runtime ? runtime.Simulation : null;
            if (pages && pages.NeedsFreeRideMap && sim != null && runtime.Running && !runtime.paused)
            {
                pendingOpen = true; pendingWait = 0.25f; lock (pointsGate) pendingPoints = null;
                pages.AskPlayerPoints(p => { lock (pointsGate) pendingPoints = p; });
                return;
            }
            OpenNow(null);
        }
        void TickPendingOpen()
        {
            List<(float, float, float, bool)> p; lock (pointsGate) p = pendingPoints;
            pendingWait -= Time.unscaledDeltaTime;
            if (p == null && pendingWait > 0) return;
            pendingOpen = false; OpenNow(p);
        }

        void OpenNow(List<(float x, float z, float rot, bool local)> points)
        {
            IsOpen = true; canvas.enabled = true;
            pausedBefore = runtime && runtime.paused;
            if (runtime) runtime.paused = true;
            AudioListener.pause = true;
            Reflex.Port.Platform.ReflexCursor.Want(this, true);   // shown for a mouse player only; a pad player's stays hidden where it is
            SetupRows();
            RefreshFreeRideMap(points);
            Focus(0);
        }

        public void Close()
        {
            if (!IsOpen) return;
            popups?.Close(); FocusOptions(false); FocusStunts(false); legend?.Reset();
            IsOpen = false; canvas.enabled = false; Reflex.Port.Platform.ReflexCursor.Want(this, false);
            if (runtime) runtime.paused = pausedBefore;
            AudioListener.pause = false;
        }

        bool Build()
        {
            Dictionary<string, object> doc;
            try { var data = OriginalUiAssets.Data; loc = data.Strings("InGame", "MXRaven_Default_Strings"); doc = data.BuildPause(); }
            catch (Exception e) { Debug.LogWarning("Original pause menu not built (MX vs ATV Reflex install?): " + e.Message); return false; }
            // (the mouse's: this menu reads its keys and pad itself, so the module's navigation is off; kept on, it outlived the race and doubled
            // every d-pad step of the front end's menus, w-uibatch)
            if (!EventSystem.current) { var es = new GameObject("EventSystem", typeof(EventSystem), typeof(InputSystemUIInputModule)); Reflex.Port.OpenFrontEnd.OuUi.NavigationOff(es.GetComponent<EventSystem>()); if (Application.isPlaying) DontDestroyOnLoad(es); }
            var cgo = new GameObject("Original pause menu", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
            cgo.transform.SetParent(transform, false);
            canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 50;
            var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
            // full-screen backer (stretched to the screen, not just the 1280x720 area)
            var backer = new GameObject("Backer", typeof(RectTransform), typeof(Image)); backer.transform.SetParent(cgo.transform, false);
            var brt = (RectTransform)backer.transform; brt.anchorMin = Vector2.zero; brt.anchorMax = Vector2.one; brt.offsetMin = brt.offsetMax = Vector2.zero;
            backer.GetComponent<Image>().color = new Color(0, 0, 0, 140 / 255f);
            var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
            root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
            tree = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
            // PauseMenu.lyr's own drawing order: the Free Ride map / challenge description, the list, then the panels
            foreach (var e in OriginalUi.Arr(doc, "layout"))
            {
                if (!(e is Dictionary<string, object> d)) continue;
                switch (OriginalUi.S(d, "name"))
                {
                    case "FreerideContainer": freeRidePanel = tree.Build(d, root, null); break;
                    case "ChallengeEventContainer": challengePanel = tree.Build(d, root, null); break;
                    case "PauseList": tree.Build(d, root, null); break;
                    case "PauseOptions.component": optionsPanel = tree.Build(d, root, null); break;
                    case "StuntPreview.component": stuntsPanel = tree.Build(d, root, null); break;
                }
            }
            BuildOptions();
            BuildStunts();
            OriginalUiTree.SetVisible(freeRidePanel, false); OriginalUiTree.SetVisible(challengePanel, false);
            legend = new OriginalButtonLegend(root);   // page "Paused Menu Page": PauseMenu.lyr, ButtonLegend.lyr, Header.lyr
            legend.SetAction("back", () => { if (optionsFocused) FocusOptions(false); else if (stuntsFocused) FocusStunts(false); });
            if (OriginalUi.Obj(doc, "header") is Dictionary<string, object> header)
            {
                var h = tree.Build(header, root, null);
                foreach (var n in tree.byPath.Values) if (n.name == "Header Text") OriginalUiTree.SetText(n, Str("STR_PAUSE", "PAUSE"));
            }
            var popupDocs = OriginalUi.Obj(doc, "confirmPopups");
            popups = new OriginalConfirmPopup(tree, root, loc, OriginalUi.Obj(popupDocs, "RestartEventConfirmationPopup.lyr"), OriginalUi.Obj(popupDocs, "QuitEventConfirmationPopup.lyr"));
            allRows.Clear();
            for (int i = 0; i < 8; i++)
            {
                var row = tree.Find("PauseList/NavListContainer/Item" + i);
                if (row == null) continue;
                int index = allRows.Count;
                Clickable(tree.Child(row, "Background"), () => { if (index < items.Count) { Focus(index); items[index].action(); } });
                allRows.Add(row);
            }
            BuildExtraRows();
            canvas.enabled = false;
            return true;
        }

        // PauseMenu:Start's list (m_PauseListData) and OnFormatPauseList: the rows' texts
        void SetupRows()
        {
            var keys = pages && pages.PauseRowsReady ? pages.PauseRows.ToList() : RaceRows.ToList();
            items.Clear(); kinds.Clear(); rows.Clear();
            foreach (var k in keys)
            {
                Action a;
                switch (k)
                {
                    case "STR_RESUME_GAME": a = Close; break;
                    case "STR_RESTART_GAME": a = () => popups.ShowRestart(() => { Close(); if (runtime) runtime.RestartEvent(); }, runtime && runtime.IsChallengeWorld); break;
                    case "STR_GARAGE_VEHICLE_TUNE": a = () => OnVehicleTuning?.Invoke(this); break;
                    case "STR_QUICKVIEW": a = () => OnVehicleList?.Invoke(this); break;
                    case "STR_STUNTS": a = () => FocusStunts(true); break;
                    case "STR_OPTIONS": a = () => FocusOptions(true); break;
                    case "STR_QUIT": a = () => popups.ShowQuit(Quit, runtime && runtime.IsChallengeWorld); break;
                    default: continue;   // STR_PLAYERS / friends: online only
                }
                items.Add((Str(k, k), a)); kinds.Add(k);
            }
            for (int i = 0; i < allRows.Count; i++)
            {
                bool used = i < items.Count;
                OriginalUiTree.SetVisible(allRows[i], used);
                if (!used) continue;
                OriginalUiTree.SetText(tree.Child(allRows[i], "Text"), items[i].text);
                rows.Add(allRows[i]);
            }
            SetupExtras();
            // the ChallengeEventContainer's texts (PopulateChallengeEvent: SetLocalizationKey by EventType)
            if (challengePanel != null && pages && pages.ChallengeTitle != null)
            {
                OriginalUiTree.SetText(tree.Child(challengePanel, "TitleText"), Str(pages.ChallengeTitle, pages.ChallengeTitle));
                var d = tree.Child(challengePanel, "DescriptionText");
                string s = Str(pages.ChallengeDescription, pages.ChallengeDescription);
                if (d != null && d.text && d.text.font != null && d.rect.sizeDelta.x > 0) s = d.text.font.Wrap(s, d.text.textHeight, d.rect.sizeDelta.x);
                OriginalUiTree.SetText(d, s);
            }
        }

        // ------------------------------------------------------------------------------------- the client's rows (Options)
        readonly List<OriginalUiTree.Node> extraRows = new List<OriginalUiTree.Node>();
        readonly List<Action<int>> extraAdjust = new List<Action<int>>();
        readonly List<Func<string>> extraText = new List<Func<string>>();
        int extraStart = int.MaxValue;
        /// The client's rows shown after the game's (CAMERA, HUD STYLE, ALL OPTIONS), their texts (tests).
        public IReadOnlyList<string> ExtraRowTexts { get { var l = new List<string>(); foreach (var t in extraText) l.Add(t()); return l; } }
        public int ExtraStart => extraStart;

        static object CloneJson(object o)
        {
            if (o is Dictionary<string, object> d) { var c = new Dictionary<string, object>(d.Count); foreach (var kv in d) c[kv.Key] = CloneJson(kv.Value); return c; }
            if (o is List<object> l) { var c = new List<object>(l.Count); foreach (var x in l) c.Add(CloneJson(x)); return c; }
            return o;
        }
        static void Rename(Dictionary<string, object> e, string from, string to)
        {
            if (e.TryGetValue("path", out var p) && p is string ps) e["path"] = ps.Replace(from, to);
            if (e.TryGetValue("children", out var k) && k is List<object> kids) foreach (var c in kids) if (c is Dictionary<string, object> cd) Rename(cd, from, to);
        }

        // five more rows like the list's own (a copy of its last row's layout, placed below the rows in use at each open)
        void BuildExtraRows()
        {
            extraRows.Clear();
            if (allRows.Count < 2 || allRows[0].json == null) return;
            var model = allRows[allRows.Count - 1]; var parent = model.parent;
            if (parent == null) return;
            for (int k = 0; k < 5; k++)
            {
                if (!(CloneJson(model.json) is Dictionary<string, object> json)) return;
                string name = "ClientItem" + k;
                json["name"] = name; Rename(json, model.name, name);
                var node = tree.Build(json, parent.rect, parent);
                parent.children.Add(node);
                int index = k;
                Clickable(tree.Child(node, "Background"), () => { int i = extraStart + index; if (i < items.Count) { Focus(i); items[i].action(); } });
                OriginalUiTree.SetVisible(node, false);
                extraRows.Add(node);
            }
        }

        void SetupExtras()
        {
            extraStart = items.Count; extraAdjust.Clear(); extraText.Clear();
            foreach (var r in extraRows) OriginalUiTree.SetVisible(r, false);
            if (extraRows.Count < 3 || rows.Count == 0) return;
            // the spacing of the rows in use (the list's slots are not evenly spaced in every layout)
            int used = rows.Count;
            Vector2 last = rows[used - 1].basePos, step = used > 1 ? (last - rows[0].basePos) / (used - 1) : allRows[1].basePos - allRows[0].basePos;
            void Add(Func<string> text, Action<int> adjust, Action accept)
            {
                int k = extraText.Count; var node = extraRows[k];
                node.rect.anchoredPosition = node.basePos = last + step * (rows.Count - used + 1);
                OriginalUiTree.SetVisible(node, true);
                extraText.Add(text); extraAdjust.Add(adjust);
                items.Add((text(), accept)); rows.Add(node);
            }
            Add(() => "CAMERA: " + Reflex.Port.Options.ReflexClientLive.CameraName, d => Reflex.Port.Options.ReflexClientLive.CycleCamera(d),
                () => { Reflex.Port.Options.ReflexClientLive.CycleCamera(1); RefreshExtras(); });
            Add(() => "HUD: " + Reflex.Port.Options.ReflexSettings.HudStyleNames[(int)Reflex.Port.Options.ReflexSettings.HudStyle], d => ToggleHudStyle(), () => { ToggleHudStyle(); RefreshExtras(); });
            Add(() => "ALL OPTIONS", null, () => Reflex.Port.Options.ReflexOptions.Open(Reflex.Port.Options.ReflexOptions.Controls));
            // REPLAY / PHOTO MODE (Reflex.Port.Replay.ReflexReplay, w-replayphoto2): the viewer over this page, back here when it closes
            if (extraRows.Count >= 5 && Reflex.Port.Replay.ReflexReplay.Offered(runtime))
            {
                Add(() => "REPLAY", null, () => Reflex.Port.Replay.ReflexReplay.Of(runtime)?.Open(Reflex.Port.Replay.ReflexReplay.View.Replay, Reflex.Port.Replay.ReflexReplay.Origin.Pause, RefreshExtras));
                Add(() => "PHOTO MODE", null, () => { var r = Reflex.Port.Replay.ReflexReplay.Of(runtime); if (r && r.Open(Reflex.Port.Replay.ReflexReplay.View.Photo, Reflex.Port.Replay.ReflexReplay.Origin.Pause, RefreshExtras)) r.OpenedPhotoDirect(); });
            }
            RefreshExtras();
        }

        static void ToggleHudStyle()
        {
            var s = Reflex.Port.Options.ReflexSettings.HudStyle;
            Reflex.Port.Options.ReflexSettings.HudStyle = s == Reflex.Port.Options.HudStyle.Modern ? Reflex.Port.Options.HudStyle.Original : Reflex.Port.Options.HudStyle.Modern;
            Reflex.Port.Options.ReflexSettings.Save();
        }

        void RefreshExtras()
        {
            for (int k = 0; k < extraText.Count && extraStart + k < rows.Count; k++)
            {
                string t = extraText[k]();
                items[extraStart + k] = (t, items[extraStart + k].action);
                OriginalUiTree.SetText(tree.Child(rows[extraStart + k], "Text"), t);
            }
        }

        void Clickable(OriginalUiTree.Node n, Action onClick)
        {
            if (n == null || !n.graphic) return;
            n.graphic.raycastTarget = true;
            var b = n.rect.gameObject.AddComponent<Button>(); b.transition = Selectable.Transition.None; b.onClick.AddListener(() => onClick());
        }

        void Focus(int i)
        {
            if (rows.Count == 0) return;
            if (optionsFocused) FocusOptions(false);
            if (stuntsFocused) FocusStunts(false);
            focus = (i + rows.Count) % rows.Count;
            for (int k = 0; k < rows.Count; k++)
            {
                OriginalUiTree.SetColor(tree.Child(rows[k], "CursorBackground"), k == focus ? CursorFocused : CursorIdle);
                OriginalUiTree.SetColor(tree.Child(rows[k], "Text"), k == focus ? TextFocused : TextIdle);
            }
            // OnPauseListFocusedChanged: the container tied to the highlighted row is shown (RESUME: the default container)
            string kind = focus < kinds.Count ? kinds[focus] : null;
            bool challenge = pages && pages.ChallengeTitle != null, map = pages && pages.NeedsFreeRideMap && mapShown;
            OriginalUiTree.SetVisible(optionsPanel, kind == "STR_OPTIONS");
            OriginalUiTree.SetVisible(stuntsPanel, kind == "STR_STUNTS");
            OriginalUiTree.SetVisible(challengePanel, kind == "STR_RESUME_GAME" && challenge);
            OriginalUiTree.SetVisible(freeRidePanel, kind == "STR_RESUME_GAME" && !challenge && map);
            if (optionsPanel != null && optionsPanel.rect.gameObject.activeSelf) RefreshOptions();
            if (stuntsPanel != null && stuntsPanel.rect.gameObject.activeSelf) RefreshStunts();
        }

        // ------------------------------------------------------------------------------------- Free Ride map (FreerideContainer)
        // RefreshFreeRideMap: the level's map (EventTypeLocations of EventTypeId 9 at the location: HUDMapIconId; none -> the container
        // stays hidden), one PlayerPoint per rider at ((x + 2048) / 4096 * 388, 388 - (z + 2048) / 4096 * 388), turned by its rotation,
        // RGBA (60,161,176) for the exclusive device's rider, (230,230,230) for the others. The challenge points (the career's challenge
        // list) are not drawn (PopulateChallengeList: the player's PlayerChallenges records).
        bool mapShown;
        void RefreshFreeRideMap(List<(float x, float z, float rot, bool local)> points)
        {
            mapShown = false;
            if (freeRidePanel == null || !pages || !pages.NeedsFreeRideMap) return;
            var img = tree.Child(freeRidePanel, "FreeRideLocationImage");
            assets ??= new FeAssets(null) { tables = FeTables.Shared };
            var (tex, uv) = pages.FreeRideMapIcon >= 0 ? assets.Icon(pages.FreeRideMapIcon) : (null, new Rect(0, 0, 1, 1));
            if (tex == null || img == null || !(img.graphic is RawImage ri)) return;
            ri.texture = tex; ri.uvRect = uv; mapShown = true;
            foreach (var c in freeRidePanel.children) if (c.name.StartsWith("ChallengePoint", StringComparison.Ordinal) || c.name.StartsWith("PlayerPoint", StringComparison.Ordinal)) OriginalUiTree.SetVisible(c, false);
            OriginalUiTree.SetVisible(tree.Child(freeRidePanel, "ChallengeContainer"), false);
            if (points == null) return;
            for (int i = 0; i < points.Count && i < Reflex.Port.RxCore.RxRiderCap.Max; i++)
            {
                var p = tree.Child(freeRidePanel, "PlayerPoint" + i) ?? MorePlayerPoint(i); if (p == null) break;
                OriginalUiTree.SetVisible(p, true);
                OriginalUiTree.SetColor(p, points[i].local ? new Color(60 / 255f, 161 / 255f, 176 / 255f, 1f) : new Color(230 / 255f, 230 / 255f, 230 / 255f, 1f));
                float mx = (points[i].x + 2048f) / 4096f * 388f, my = 388f - (points[i].z + 2048f) / 4096f * 388f;
                p.rect.anchoredPosition = p.basePos = new Vector2(mx, -my);
                p.rect.localRotation = Quaternion.Euler(0, 0, -points[i].rot);   // SetZRotation: degrees, the game's y-down screen space
            }
        }

        /// (w-rc-online) the page has 12 rider arrows (PlayerPoint0 .. 11); a field of more riders (up to RxRiderCap.Max) gets more, made from the first.
        OriginalUiTree.Node MorePlayerPoint(int i)
        {
            var first = tree.Child(freeRidePanel, "PlayerPoint0"); if (first == null || !first.rect) return null;
            var go = Instantiate(first.rect.gameObject, first.rect.parent, false); go.name = "PlayerPoint" + i;
            var n = new OriginalUiTree.Node { path = first.path + "#" + i, name = "PlayerPoint" + i, cls = first.cls, rect = (RectTransform)go.transform, graphic = go.GetComponent<Graphic>(),
                basePos = first.basePos, baseScale = first.baseScale, baseColor = first.baseColor, parent = first.parent };
            (first.parent ?? freeRidePanel).children.Add(n);
            return n;
        }

        // ------------------------------------------------------------------------------------- PauseOptions
        // (UIFocusContainer rows of PauseOptions.component/Grouped/NavList; PauseOptions.lua PopulateSettings / OnSettingsListFocusedChanged)
        sealed class Option
        {
            public string container, setting; public string[] labels; public Func<string[]> labelsOf; public Func<int> read; public Action<int> write;
            public OriginalUiTree.Node bg, title, item, left, right; public int index;
        }
        readonly List<Option> options = new List<Option>();
        OriginalUiTree.Node optionsPanel, stuntsPanel, freeRidePanel, challengePanel; bool optionsFocused; int optFocus;
        OriginalMusic musicSource;

        static string[] Percents() { var l = new string[11]; for (int i = 0; i < 11; i++) l[i] = i * 10 + "%"; return l; }
        string[] Keys(params string[] k) { var l = new string[k.Length]; for (int i = 0; i < k.Length; i++) l[i] = Str(k[i], k[i]); return l; }

        /// The game's EngineVolume UI setting as a 0..1 scale for the engine sounds (stored; the audio may apply it).
        public static float EngineVolume => PlayerPrefs.GetInt("ReflexUI.EngineVolume", 10) / 10f;

        // DrivingCamera 0..3 = Visual, Game Play, User, 1st Person: the game's own camera presets (UI setting DrivingCamera; the game
        // switches the player's camera entity P+0x974[0 / 1 / 4 / 2], FUN_0054aef0 / 0054c9b0, as its Camera key does), shown by the
        // client's game camera (mode 0). Dynamic FPV (mode 1) and the follow camera (2) are the port's extras (F7, OPTIONS CAMERA).
        public static readonly int[] CameraModeOf = { 0, 0, 0, 0 };   // also used by the front end's OPTIONS page (Reflex.Port.FrontEnd)
        // the DrivingCamera row lists the cameras that are ON in OPTIONS > CAMERA & HUD > CAMERA CYCLE (the game's views, DYNAMIC FPV, the player's presets)
        static string[] CameraLabels(string[] game)
            => Reflex.Port.Options.ReflexCameraPresets.Listed().Select(e => e.builtin >= 0 && e.builtin < game.Length ? game[e.builtin] : e.name).ToArray();
        int DrivingCameraIndex() { string cur = Reflex.Port.Options.ReflexCameraPresets.CurrentId; return Math.Max(0, Reflex.Port.Options.ReflexCameraPresets.Listed().FindIndex(e => e.id == cur)); }
        void SetDrivingCamera(int v) { var list = Reflex.Port.Options.ReflexCameraPresets.Listed(); if (v >= 0 && v < list.Count) Reflex.Port.Options.ReflexCameraPresets.Select(list[v].id); }

        void BuildOptions()
        {
            options.Clear();
            if (optionsPanel == null) return;
            var onOff = Keys("STR_OPT_ON", "STR_OPT_OFF");
            void Add(string container, string setting, string[] labels, Func<int> read, Action<int> write)
            {
                var c = tree.Find("PauseOptions.component/Grouped/NavList/" + container); if (c == null) return;
                var list = tree.Child(c, "List");
                options.Add(new Option { container = container, setting = setting, labels = labels, read = read, write = write,
                    bg = tree.Child(c, "Background"), title = tree.Child(c, "Title"), item = list != null && list.children.Count > 0 ? list.children[0] : null,
                    left = tree.Child(c, "LeftArrow"), right = tree.Child(c, "RightArrow") });
            }
            var gameCams = Keys("STR_OPT_DC_1", "STR_OPT_DC_2", "STR_OPT_DC_3", "STR_OPT_DC_4").Concat(new[] { "DYNAMIC FPV" }).ToArray();
            Add("DrivingCameraContainer", "DrivingCamera", CameraLabels(gameCams), DrivingCameraIndex, SetDrivingCamera);
            if (options.Count > 0 && options[options.Count - 1].setting == "DrivingCamera") options[options.Count - 1].labelsOf = () => CameraLabels(gameCams);
            Add("HUDVisibilityContainer", "HUDVisibility", onOff, () => runtime && !runtime.OriginalHudVisible ? 1 : 0, v => { if (runtime) runtime.OriginalHudVisible = v == 0; });
            Add("OnlinePlayerNameContainer", "OnlinePlayerName", onOff, () => PlayerPrefs.GetInt("ReflexUI.OnlinePlayerName", 0), v => PlayerPrefs.SetInt("ReflexUI.OnlinePlayerName", v));
            Add("EffectsVolumeContainer", "EffectsVolume", Percents(), () => runtime && runtime.gameAudio ? Mathf.Clamp(Mathf.RoundToInt(runtime.gameAudio.masterVolume * 10f), 0, 10) : 10,
                v => { if (runtime && runtime.gameAudio) { runtime.gameAudio.masterVolume = v / 10f; PlayerPrefs.SetFloat("ReflexAudio.master", v / 10f); } });
            Add("MusicVolumeContainer", "MusicVolume", Percents(), () => { var m = Music(); return m ? Mathf.Clamp(Mathf.RoundToInt(m.volume * 10f), 0, 10) : 6; },
                v => { var m = Music(); if (m) m.volume = v / 10f; PlayerPrefs.SetFloat("ReflexAudio.music", v / 10f); });
            Add("EngineVolumeContainer", "EngineVolume", Percents(), () => PlayerPrefs.GetInt("ReflexUI.EngineVolume", 10), v => PlayerPrefs.SetInt("ReflexUI.EngineVolume", v));
            for (int i = 0; i < options.Count; i++)
            {
                int row = i;
                Clickable(options[i].bg, () => { if (!optionsFocused) FocusOptions(true); FocusOption(row); });
                Clickable(options[i].left, () => { if (!optionsFocused) FocusOptions(true); FocusOption(row); ChangeOption(row, -1); });
                Clickable(options[i].right, () => { if (!optionsFocused) FocusOptions(true); FocusOption(row); ChangeOption(row, +1); });
            }
            OriginalUiTree.SetVisible(optionsPanel, false);
        }

        OriginalMusic Music() { if (!musicSource) musicSource = FindAnyObjectByType<OriginalMusic>(); return musicSource; }

        /// Reads the settings into the rows (PopulateSettings: SetNumItems + SetSelectedAbsoluteIndex) and colours them.
        void RefreshOptions()
        {
            foreach (var o in options) { if (o.labelsOf != null) o.labels = o.labelsOf(); o.index = Mathf.Clamp(o.read(), 0, o.labels.Length - 1); }
            ColourOptions();
        }

        void ChangeOption(int row, int d)
        {
            if (row < 0 || row >= options.Count) return;
            var o = options[row]; int n = Mathf.Clamp(o.index + d, 0, o.labels.Length - 1);
            if (n == o.index) return;   // NoScrollSoundEvent: DirectionalFail
            o.index = n; o.write(n); PlayerPrefs.Save();
            if (o.labelsOf != null) { o.labels = o.labelsOf(); o.index = Mathf.Clamp(o.read(), 0, o.labels.Length - 1); }   // (an OFF camera that was current leaves the list)
            ColourOptions();
        }

        void FocusOption(int row) { if (options.Count == 0) return; optFocus = (row + options.Count) % options.Count; ColourOptions(); }

        void FocusOptions(bool on)
        {
            if (on && (optionsPanel == null || options.Count == 0)) return;
            if (optionsFocused == on) return;
            optionsFocused = on;
            if (on) { optFocus = 0; RefreshOptions(); }
            else ColourOptions();
            ListFocus();
            legend?.Set("back", on, "STR_BACK");
        }

        // the pause list loses (regains) focus: its cursor shows as idle (focusable) while a sub-panel is focused
        void ListFocus()
        {
            for (int k = 0; k < rows.Count; k++)
            {
                bool f = k == focus && !optionsFocused && !stuntsFocused;
                OriginalUiTree.SetColor(tree.Child(rows[k], "CursorBackground"), f ? CursorFocused : CursorIdle);
                OriginalUiTree.SetColor(tree.Child(rows[k], "Text"), f ? TextFocused : TextIdle);
            }
        }

        void ColourOptions()
        {
            for (int i = 0; i < options.Count; i++)
            {
                var o = options[i]; bool f = optionsFocused && i == optFocus;
                OriginalUiTree.SetColor(o.bg, OriginalUiTree.StateColor(o.bg, f, optionsFocused));
                OriginalUiTree.SetColor(o.title, OriginalUiTree.StateColor(o.title, f, optionsFocused));
                OriginalUiTree.SetColor(o.item, OriginalUiTree.StateColor(o.item, f, optionsFocused));
                OriginalUiTree.SetText(o.item, o.labels[o.index]);
                // UpdateArrows: the left arrow hides at the first entry, the right at the last; the arrows only show (FocusedColor) on the focused row
                OriginalUiTree.SetVisible(o.left, o.index > 0); OriginalUiTree.SetVisible(o.right, o.index < o.labels.Length - 1);
                OriginalUiTree.SetColor(o.left, OriginalUiTree.StateColor(o.left, f, optionsFocused)); OriginalUiTree.SetColor(o.right, OriginalUiTree.StateColor(o.right, f, optionsFocused));
            }
        }

        // ------------------------------------------------------------------------------------- StuntPreview (TRICK LIBRARY)
        readonly List<(int tier, string name, string key)> stunts = new List<(int, string, string)>();
        readonly List<OriginalUiTree.Node> stuntSlots = new List<OriginalUiTree.Node>();
        OriginalUiTree.Node tierText, scrollbar, slider; bool stuntsFocused; int stuntIndex, stuntTop;
        Dictionary<string, string> stuntNames;
        System.Threading.Tasks.Task<List<(int tier, string name, string key)>> stuntsLoading;

        void BuildStunts()
        {
            if (stuntsPanel == null) return;
            tierText = tree.Find("StuntPreview.component/HeaderContainer/StuntTierComponent") ?? tree.Child(tree.Child(stuntsPanel, "HeaderContainer"), "StuntTierComponent");
            var list = tree.Child(tree.Child(stuntsPanel, "StuntList"), "NavListContainer");
            if (list != null) stuntSlots.AddRange(list.children.OrderBy(c => c.rect.anchoredPosition.y * -1));
            var bg = tree.Child(tree.Child(stuntsPanel, "StuntList"), "BackgroundImages");
            scrollbar = tree.Child(bg, "Scrollbar"); slider = tree.Child(scrollbar, "Slider");
            for (int i = 0; i < stuntSlots.Count; i++) { int s = i; Clickable(tree.Child(stuntSlots[i], "Background"), () => { if (!stuntsFocused) FocusStunts(true); FocusStunt(stuntTop + s); }); }
            OriginalUiTree.SetVisible(stuntsPanel, false);
        }

        // PopulateStuntList: the game's stunt descriptors (asked of the world at its start; with an older plugin the profile session's,
        // FeStuntPreview.LoadAll), a display name already listed skipped
        bool LoadStunts()
        {
            if (stunts.Count > 0) return true;
            if (pages && pages.StuntsReady) { stunts.AddRange(pages.Stunts); return true; }
            if (pages && !OriginalEventIntro.NativeMissing && !pages.StuntsAnswered) return false;   // the world's answer is coming
            stuntsLoading ??= FeStuntPreview.LoadAll();
            if (!stuntsLoading.IsCompleted) return false;
            if (!stuntsLoading.IsFaulted) foreach (var s in stuntsLoading.Result) if (!stunts.Any(d => d.key == s.key)) stunts.Add(s);
            return true;
        }

        void RefreshStunts()
        {
            if (!LoadStunts()) { foreach (var s in stuntSlots) OriginalUiTree.SetVisible(s, false); OriginalUiTree.SetText(tierText, ""); return; }
            stuntNames ??= OriginalUiAssets.Data.Strings("InGame", "MXRaven_Stunts");
            int n = stuntSlots.Count, count = stunts.Count;
            stuntIndex = Mathf.Clamp(stuntIndex, 0, Math.Max(0, count - 1));
            if (stuntIndex < stuntTop) stuntTop = stuntIndex; if (stuntIndex >= stuntTop + n) stuntTop = stuntIndex - n + 1; stuntTop = Mathf.Clamp(stuntTop, 0, Math.Max(0, count - n));
            assets ??= new FeAssets(null) { tables = FeTables.Shared };
            for (int s = 0; s < n; s++)
            {
                int d = stuntTop + s; bool show = d < count; var slot = stuntSlots[s];
                OriginalUiTree.SetVisible(slot, show); if (!show) continue;
                var (tier, name, key) = stunts[d];
                bool f = stuntsFocused && d == stuntIndex;
                var bg = tree.Child(slot, "Background"); var text = tree.Child(slot, "Text"); var img = tree.Child(slot, "StuntGestureImage");
                OriginalUiTree.SetText(text, stuntNames.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v) ? v : key);   // App.SetDynamicText( text, locKey, "MXRaven_Stunts" )
                if (img?.graphic is RawImage ri) { var (tex, uv) = assets.Material("Stunt_" + (key.Length > 4 ? key.Substring(4) : key)); ri.texture = tex; ri.uvRect = uv; }
                OriginalUiTree.SetColor(bg, OriginalUiTree.StateColor(bg, f, true)); OriginalUiTree.SetColor(text, OriginalUiTree.StateColor(text, f, true)); OriginalUiTree.SetColor(img, OriginalUiTree.StateColor(img, f, true));
            }
            // the list's scrollbar (the slider's length and place by the window)
            OriginalUiTree.SetVisible(scrollbar, count > n);
            if (slider != null && scrollbar != null && count > n)
            {
                float h = scrollbar.rect.sizeDelta.y, sh = Mathf.Max(8, h * n / count);
                slider.rect.sizeDelta = new Vector2(slider.rect.sizeDelta.x, sh); slider.rect.anchoredPosition = slider.basePos = new Vector2(slider.rect.anchoredPosition.x, -(h - sh) * stuntTop / Math.Max(1, count - n));
            }
            // OnStuntListFocusedChanged: the focused stunt's tier
            OriginalUiTree.SetText(tierText, count > 0 ? Str("STR_STUNT_TIER" + stunts[stuntIndex].tier, "Tier " + stunts[stuntIndex].tier) : "");
        }

        void FocusStunt(int i) { if (stunts.Count == 0) return; stuntIndex = Mathf.Clamp(i, 0, stunts.Count - 1); RefreshStunts(); }

        // OnAcceptStunts: the StuntPreview container takes the focus, BACK in the legend; OnBackKeypress gives it back to the list
        void FocusStunts(bool on)
        {
            if (on && stuntsPanel == null) return;
            if (stuntsFocused == on) return;
            stuntsFocused = on;
            if (on) { stuntIndex = 0; stuntTop = 0; }
            RefreshStunts();
            ListFocus();
            legend?.Set("back", on, "STR_BACK");
        }

        void Quit()
        {
            Close();
            // a challenge started in a Free Ride activation area: QUIT goes back to that Free Ride (PauseMenu:OnQuitEventPopupResult with
            // EventTypeActivated: App.QuitToFreeRide)
            if (Reflex.Port.OpenFrontEnd.OwnActivationArea.QuitToFreeRide(runtime)) return;
            QuitToFrontEnd();
        }

        /// Leaving the event for the front end: the FrontEnd scene when it is in the build (Reflex.Port.FrontEnd), else the editor
        /// stops playing / a player exits.
        public static void QuitToFrontEnd()
        {
            AudioListener.pause = false;
            if (Application.CanStreamedLevelBeLoaded("FrontEnd")) { UnityEngine.SceneManagement.SceneManager.LoadScene("FrontEnd"); return; }
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }
    }
}
