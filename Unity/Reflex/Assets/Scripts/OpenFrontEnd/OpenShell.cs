// The OPEN REFLEX shell (FrontEndSwitch puts it in the front-end scene when FRONT END is OPEN), UI Toolkit, built in code after
// Open Unleashed's MxaMainMenu (Scripts/UI/MxaMainMenu*.cs):
//   install   first run / no install found: CONNECT FOLDER, a folder browser that works with a gamepad, a path field and the native
//             folder dialog in the Editor; the folder must be an install by ReflexInstall's rule (MXReflex.exe + Database/) and is
//             saved through ReflexInstall.Set (PlayerPrefs ReflexGameDir) (OpenShell.Library.cs)
//   library   GAME LIBRARY: the Reflex install's card and the world cache (ReflexWorldFiles: the built worlds, their size, REBUILD,
//             the build progress while one is built) (OpenShell.Library.cs)
//   main      the hub (Resources/OpenFrontEnd/FrontEnd/Hub.uxml): the wordmark, RACE / FREE RIDE / CAREER / GARAGE / OPTIONS and
//             GAME LIBRARY / QUIT over a location's picture, the CONTINUE card (the last event started, its setup: RIDE NOW) or
//             QUICK START, and the MOTOCARD card
//   browser   the track browser (OpenShell.Browser.cs) and the setup page (OpenShell.Setup.cs); START EVENT launches through
//             ReflexEventSelect.Launch as the original's LocationSelect does once the event is loaded in the test-ride arena
//             (OpenShell.LoadArena.cs: PRESS START); the loading screen (OpenLoading) stays over the race scene until its world runs
//   CAREER / GARAGE: the original front end's own pages (OriginalEmbed), back here when it returns to its main menu
//   OPTIONS   the client's options pages (Reflex.Port.Options.ReflexOptions.Open) over the shell, whose input waits meanwhile; their
//             SYSTEM > GAME FOLDER opens GAME LIBRARY (ReflexOptionsHooks.OpenGameLibrary, set while the shell runs)
//   a crash   the session before ended unexpectedly (Reflex.Port.Platform.ReflexCrashLog): a card on the hub offers a support report
// Sounds and music: Reflex's own UI sounds (UIRegistry.xml) and UI music themes from the install (FeSounds / FeUiMusic, the original
// front end's player), at the original's tiers (hub 2, event select 3, location select 5).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell : MonoBehaviour
    {
        public enum Screen { None, Install, Library, Main, Browser, Setup, Options, Loading, Working, Garage, Credits, Motocard, MotoSkills, PhysicsStudio, Play }

        /// The front-end scene's original front end (disabled while the shell runs).
        public FrontEndManager original;
        public static OpenShell Instance { get; private set; }
        public Screen Current { get; private set; }
        public VisualElement Root { get; private set; }
        /// The screen layer (the current screen's elements).
        public VisualElement Layer => _layer;
        public OuNav Nav => _nav;
        /// A text box has the keyboard (the rider name, the search box): single-key shortcuts (SKIP SONG) stay quiet.
        public bool TextFocused { get { var f = _layer?.focusController?.focusedElement; return f is TextElement || f is TextField; } }
        public OuMenuInput Input => _input;
        /// The focused item's first label (tests).
        public string FocusedText => _nav.Focused?.Q<Label>()?.text;
        public FeSounds Sounds => _sounds;
        /// Called every frame while the current screen is up (a screen another part builds into the shell); cleared by Clear.
        public Action ScreenUpdate;

        OuMenuInput _input;
        readonly OuNav _nav = new OuNav();
        VisualElement _bg, _layer, _hints, _status;
        Label _statusText;
        FeSounds _sounds;

        // where a race started from the shell comes back to (the setup page of its event, as the original comes back to its event select)
        static string s_returnEvent; static bool s_returnFreeRide, s_returnPractice;   // (practice: back to the PRACTICE setup page)
        public static void ResetStatics() { s_returnEvent = null; s_returnFreeRide = false; s_returnPractice = false; s_returnCareer = false; s_returnChallenges = false; ChallengeLaunchOverride = null; QuitOverride = null; OpenSetup.LaunchOverride = null; }

        void Awake()
        {
            Instance = this;
            Reflex.Port.RxCore.RxMapScale.Reset();   // MAP SCALE: the front end's menu world is always at 100 %
            Reflex.Port.RxCore.RxPracticeMode.Reset();   // PRACTICE: set again by a practice launch only
            Reflex.Port.Rendering.ReflexWeather.SetRide(0, 1);   // WEATHER: the menus (and the loading arena) are CLEAR; a ride's launch sets its own
            Reflex.Port.Options.ReflexClientLive.ApplyFrameRate();
            Root = OuUi.CreateDocument(gameObject);
            OuTheme.Apply(Root);
            OuUi.EnsureEventSystem();
            _input = new OuMenuInput();
            _bg = OuUi.Div("mxa-bg", Root);
            // the menu world's dimming behind the pages that are not the hub or the garage (OpenShell.MenuWorld.cs)
            _worldDim = OuUi.Div("ou-world-dim", Root);
            _worldDim.pickingMode = PickingMode.Ignore;
            _worldDim.style.position = Position.Absolute; _worldDim.style.left = 0; _worldDim.style.top = 0; _worldDim.style.right = 0; _worldDim.style.bottom = 0;
            _worldDim.style.backgroundColor = new Color(6 / 255f, 7 / 255f, 9 / 255f, 0.6f);
            _worldDim.style.display = DisplayStyle.None;
            _scrim = OuUi.Div("mxa-scrim", Root); _scrim.style.backgroundImage = OuUi.Scrim;
            _bottomFade = OuUi.Div("ou-bottom-fade", Root); _bottomFade.style.backgroundImage = OuUi.ScrimBottom;
            _layer = OuUi.Div("mxa-layer", Root);
            // the bottom bar: the small wordmark and a status line on the left, the button hints on the right
            _status = OuUi.Div("ou-statusbar", Root);
            OuTheme.Logo(OuUi.Div("ou-status-logo", _status), 30, true);
            _statusText = OuUi.Text("", "ou-status-text", _status);
            _hints = OuUi.Div("mxa-hints", Root);
            _nav.Screen = _layer;
            Options.ReflexOptionsHooks.OpenGameLibrary = OpenLibraryFromOptions;   // OPTIONS > SYSTEM > GAME FOLDER
            Options.ReflexOptionsHooks.OpenCredits = OpenCreditsFromOptions;      // OPTIONS > SYSTEM > ABOUT > CREDITS
        }

        void Start()
        {
            OuUi.EnsureEventSystem();   // (again once the scene that was left is gone: its EventSystem may have been the one Awake saw)
            if (OpenMenuWorldProbe.Wanted) Debug.Log($"[MenuWorldProbe] shell start at {Time.realtimeSinceStartup:F2} s");
            OpenLoadArena.Abandon();   // (an arena ride that never reached START / its own way back)
            if (Reflex.Port.RxCore.RxChampWeekend.Active) Reflex.Port.RxCore.RxChampWeekend.Abandon();   // RESTORED CONTENT: a championship weekend never outlives the race scene (ChampWeekendFlow)
            bool returnToCareer = s_returnCareer; s_returnCareer = false;
            if (ReflexInstall.Dir == null) { Debug.LogWarning("[OpenReflex] no MX vs ATV Reflex install found: CONNECT FOLDER"); ShowImportWelcome(false); return; }
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("REFLEX_GAME_DIR")) && !File.Exists(Path.Combine(ReflexInstall.Dir, ReflexAssetImport.ExtractedMarker))) { ShowImportWelcome(false); return; }   // (w-importflow: IMPORT YOUR GAME, OpenShell.ImportFlow.cs)
            if (PackPromptDue()) { ShowPackPrompt(() => StartConnected(returnToCareer)); return; }   // a loose library: PACK YOUR GAME FILES (OpenShell.Pack.cs)
            StartConnected(returnToCareer);
        }

        /// The rest of Start once the library is there (after the pack prompt, when it was shown).
        void StartConnected(bool returnToCareer)
        {
            { string root = ReflexInstall.Dir; System.Threading.Tasks.Task.Run(() => RxFiles.Warm(root)); }   // (a packed library's archives opened ahead: Content/RxFiles.cs)
            CleanUntamedStaging();   // (what an Untamed import that did not end left in Mods/Tracks: OpenShell.UntamedImport.cs)
            Connected();
            if (TestRideReturn()) return;   // back from the garage's TEST RIDE: the garage where it was left (OpenShell.TestRide.cs)
            if (TutorialReturn()) return;   // back from the MotoSkills lessons: the MOTOSKILLS page (OpenShell.Tutorials.cs)
            if (ChallengesReturn()) return;   // back from a Free Ride challenge: the CHALLENGES page (OpenShell.Challenges.cs)
            if (returnToCareer && Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            {
                ShowCoreCareer();
                return;
            }
            if (s_returnEvent != null && OpenContent.Find(s_returnEvent) is OpenEvent ev)
            {   // back from a race the shell started: its setup page (RIDE AGAIN), the browser behind it
                bool fr = s_returnFreeRide, practice = s_returnPractice && PracticeEvent(ev); s_returnEvent = null; s_returnPractice = false;
                _freeRide = fr; _practice = practice; _setup = practice ? OpenHistory.PracticeSetupOf(ev.key) : OpenHistory.SetupOf(ev.key); _browseEvent = ev;
                ShowSetupPage();
                return;
            }
            if (MotocardWelcomeDue()) { ShowMotocardWelcome(ShowMain); return; }   // the first run: the MOTOCARD welcome (OpenShell.Welcome.cs)
            ShowMain();
        }

        /// The install is there: its sounds, music and events.
        void Connected()
        {
            if (_sounds == null)
            {
                try
                {
                    _sounds = new FeSounds(this) { volume = PlayerPrefs.GetFloat("ReflexAudio.master", 1f) };   // EffectsVolume, as the original front end
                    OuSound.Player = _sounds;
                    _sounds.music?.SetEnable(true);   // App.SetEnableUIMusic( TRUE ): a theme drawn at random
                }
                catch (Exception e) { Debug.LogWarning("[OpenReflex] UI sounds / music not read from the install: " + e.Message); }
            }
            var n = OpenContent.Events.Count;
            Debug.Log($"[OpenReflex] {OpenReflex.Name}: {ReflexInstall.Dir}: {n} events" + (OpenMenuWorldProbe.Wanted ? $" (at {Time.realtimeSinceStartup:F2} s)" : ""));
            WorldStart();   // the 3D menu world behind the pages (OpenMenuWorld; read on a worker thread)
            _arenaWarmFrom = Time.realtimeSinceStartup;
        }

        void OnDestroy()
        {
            ReleaseCareerCatalog();
            ReleaseGarage();
            WorldStop();
            _assetCancel?.Cancel();
            StopUntamedImport();   // (a running Untamed scan / import: OpenShell.UntamedImport.cs)
            _input?.Dispose();
            if (OuSound.Player == _sounds) OuSound.Player = null;
            _sounds?.music?.Detach();   // (the static music hooks held the destroyed shell, w-gcspikes)
            if (Options.ReflexOptionsHooks.OpenGameLibrary == (Action)OpenLibraryFromOptions) Options.ReflexOptionsHooks.OpenGameLibrary = null;
            if (Options.ReflexOptionsHooks.OpenCredits == (Action)OpenCreditsFromOptions) Options.ReflexOptionsHooks.OpenCredits = null;
            if (Instance == this) Instance = null;
        }

        void Update()
        {
            float dt = Time.unscaledDeltaTime;
            _sounds?.Tick(dt);
            if (_input == null) return;
            // the options pages over the shell have the keys and the pad (and the frame they close in: the Esc that closed them)
            bool optionsUp = Options.ReflexOptions.IsOpen || Options.ReflexOptions.ClosedFrame == Time.frameCount;
            // (MENU PERF, Platform/ReflexMenuPerf.cs: the parts of a menu frame, named in its hitch lines)
            using (Platform.ReflexMenuPerf.Measure("menu input and actions"))
            {
                if (optionsUp) _input.Step();   // (keeps the key repeat from firing at once after they close)
                else if (IntroBlocksInput) _input.Step();   // (the 3D menu's intro: a press skips it, the menu takes input after it)
                else _nav.Update(_input);
            }
            using (Platform.ReflexMenuPerf.Measure("screen update"))
            {
                if (Current == Screen.Browser) { UpdateBrowser(); }
                else if (Current == Screen.Setup) UpdateSetupPage();
                else if (Current == Screen.Library) UpdateLibrary();
                else if (Current == Screen.Working) UpdateWorking();
                ScreenUpdate?.Invoke();
            }
            using (Platform.ReflexMenuPerf.Measure("menu world screen")) WorldUpdate();
            using (Platform.ReflexMenuPerf.Measure("arena warm check")) ArenaWarmUpdate();   // (the load arena's session made ahead: OpenShell.LoadArena.cs)
            UpdateBuild();
            RefreshCheckpointNotice();
        }

        /// The music tier (App.SetUIMusicTier: the original's pages set 1..5).
        public void MusicTier(int tier) => _sounds?.music?.SetTier(tier);

        /// Starts a screen: the layer emptied and classed for it, the navigation callbacks cleared.
        public void Clear(Screen s) => Clear(s, null);
        public void Clear(Screen s, string screenClass)
        {
            Current = s;
            Platform.ReflexMenuPerf.Note("screen " + s);
            StopNarration();
            if (s != Screen.Motocard) ReleaseStuntPreview();
            ReleaseCareerCatalog();
            ReleaseKeyboard();
            ReleaseGarage();
            ReleaseBrowser();
            _layer.Clear();
            _layer.ClearClassList();
            _layer.AddToClassList("mxa-layer");
            _layer.AddToClassList("mxa-screen-" + (screenClass ?? s.ToString().ToLowerInvariant()));
            _nav.TextScroll = null;   // (a page that scrolls its own text sets it again: OpenShell.ImportFlow.cs)
            _nav.OnCancel = null; _nav.OnTab = null; _nav.OnAlt = null; _nav.OnFind = null; _nav.DefaultItem = null; ScreenUpdate = null;
            _hints.style.display = DisplayStyle.Flex;   // (the career pages hide the shell's bars for their own legend band)
            _status.style.display = s == Screen.Loading ? DisplayStyle.None : DisplayStyle.Flex;
            _status.EnableInClassList("ou-statusbar-hub", s == Screen.Main || s == Screen.Play);
            _statusText.text = ReflexInstall.Dir != null ? $"{OpenContent.Events.Count} EVENTS" : "";
            checkpointNormalStatus = _statusText.text;
            RefreshCheckpointNotice();
            if (s != Screen.None && s != Screen.Loading) OuSound.Play("PageIn");
            _worldScreenDirty = true;
            IntroAbort();   // (another screen: the intro's walk goes on under it, the chrome is back)
        }

        /// The full-screen picture behind the screen (null: none).
        /// (a location backdrop from OpenContent.Backdrop drifts slowly, unless REDUCE MOTION is on; the hub's 3D scene sets null)
        public void SetBackground(Texture2D t) => OpenBackdrop.Apply(_bg, t);

        /// The bottom bar's button hints.
        public void Hints(params (string key, string label)[] hints) => OuUi.Hints(_hints, hints);
        void Hints(bool back, string submit = null)
        {
            var list = new List<(string, string)> { (OuUi.KeySubmit, submit ?? "SELECT") };
            if (back) list.Add((OuUi.KeyCancel, "BACK"));
            OuUi.Hints(_hints, list.ToArray());
        }

        // ------------------------------------------------------------------------------------------------ main

        /// The hub's picture: the location of the event last started, else the first National's.
        OpenEvent HubEvent()
        {
            var r = OpenHistory.Recent().Select(x => OpenContent.Find(x.eventKey)).FirstOrDefault(x => x != null);
            return r ?? OpenContent.Events.FirstOrDefault(x => x.activity == "National") ?? OpenContent.Events.FirstOrDefault();
        }
        Texture2D HubPicture()
        {
            var r = HubEvent();
            return r != null ? OpenContent.Backdrop(r) : null;
        }

        /// True while the page on screen was opened from PLAY (CAREER, RACE, FREE RIDE, MOTOSKILLS): their BACK goes to that page, not the hub.
        bool _fromPlay;
        string _playFocus = "ou-hub-career";

        /// Where BACK from a PLAY page goes: PLAY when it was opened from there, else the hub.
        void ShowHome() { if (_fromPlay) ShowPlay(); else ShowMain(); }

        /// A PLAY row's action: remembers where the page came from (and the row, to focus it again on the way back).
        Action FromPlay(string rowName, Action open) => () => { _fromPlay = true; _playFocus = rowName; open(); };

        /// PLAY's rows, in order: name, label, caption, action (the pages the original hub listed as rows of its own).
        List<(string name, string label, string caption, Action open)> PlayRows()
        {
            string raceTypes = string.Join(" · ", OpenContent.Types(false).Take(4).Select(t => t.name)) + (OpenContent.Types(false).Count > 4 ? " · MORE" : "");
            bool core = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex;
            var rows = new List<(string, string, string, Action)>
            {
                ("ou-hub-career", "CAREER", core ? "YOUR PROGRESS · NATIONAL · WAYPOINT RACE" : "MOTOCAREER · SERIES · HEATS AND MOTOS",
                    () => { if (core) ShowCoreCareer(); else OpenOriginalFor(OriginalEmbed.Purpose.Career); }),
                ("ou-hub-race", "RACE", raceTypes, () => ShowBrowser(false)),
                ("ou-hub-freeride", "FREE RIDE", "EVERY LOCATION · CHALLENGES · NO LAPS · NO RIVALS", () => ShowBrowser(true)),
            };
            if (core)   // PRACTICE: the lap races as endless practice sessions (the core's PRACTICE)
                rows.Insert(2, ("ou-hub-practice", "PRACTICE", "UNLIMITED LAPS · YOU ALONE · LAP TIMES", () => ShowBrowser(OpenBrowse.Practice)));
            if (core && ReflexInstall.Packages.Has(ReflexTutorials.Level))   // the original's MOTOSKILLS (MainMenu:OnTutorialKeypress): the lessons in the compound
                rows.Add(("ou-hub-motoskills", "MOTOSKILLS", MotoSkillsCaption(), ShowMotoSkills));
            return rows;
        }

        /// The hub's frame (the logo, the left menu column, the cards' column): the skeleton shared by the hub and PLAY.
        VisualElement HubFrame(out VisualElement menu, out VisualElement small, out VisualElement cards)
        {
            var hub = OuTheme.Clone("Hub", _layer) ?? OuUi.Div("ou-hub", _layer);
            OuTheme.Logo(OuTheme.Part(hub, "hub-logo", "ou-hub-logo"), 150);
            menu = OuTheme.Part(hub, "hub-menu", "ou-hub-menu");
            small = OuTheme.Part(hub, "hub-small", "ou-hub-small");
            cards = OuTheme.Part(hub, "hub-cards", "ou-hub-cards");
            return hub;
        }

        public void ShowMain()
        {
            Clear(Screen.Main);
            _fromPlay = false;
            SetBackground(HubPicture());
            MusicTier(2);   // MainMenu:Start
            var hub = HubFrame(out var menu, out var small, out var cards);
            var rows = PlayRows();
            var single = HubButton(menu, "PLAY", string.Join(" · ", rows.Select(r => r.label)), ShowPlay);
            single.name = "ou-hub-play";
            HubButton(menu, "GARAGE", "MOTOCARD · GEAR · VEHICLES · TUNING", ShowGarage).name = "ou-hub-garage";
            var opts = HubButton(menu, "OPTIONS", "CONTROLS · AUDIO · VIDEO · CAMERA & HUD · SYSTEM", ShowOptions);
            opts.name = "ou-hub-options";
            var library = OuUi.Button("GAME LIBRARY", "ou-chip", () => ShowLibrary(true), small);
            library.name = "ou-hub-library";
            // PHYSICS STUDIO (OpenShell.PhysicsStudio.cs): the chip shows the config the rides use when one is active
            PhysicsLibrary.SyncPending();
            string physics = RxCore.RxPhysicsStudio.PendingName;
            var studio = OuUi.Button(string.IsNullOrEmpty(RxCore.RxPhysicsStudio.PendingText) ? "PHYSICS STUDIO" : "PHYSICS: " + physics.ToUpperInvariant(), "ou-chip", () => ShowPhysicsStudio(0), small);
            studio.name = "ou-hub-physics";
            var credits = OuUi.Button("CREDITS", "ou-chip", () => ShowCredits(), small);
            credits.name = "ou-hub-credits";
            var quit = OuUi.Button("QUIT", "ou-chip", ConfirmQuit, small);   // asks first (OpenShell.Popups.cs: QuitConfirmationPopup)
            quit.name = "ou-hub-quit";
            var bigRows = new List<VisualElement>(menu.Children().Where(c => c.userData is OuNavItem));
            OuWidgets.LinkColumn(bigRows);
            var chips = new[] { library, studio, credits, quit };
            OuWidgets.LinkRow(chips);
            // the last big row down to the chips (the first chip, or the one under it) and every chip back up to it
            ((OuNavItem)opts.userData).down = library;
            foreach (var b in chips) ((OuNavItem)b.userData).up = opts;
            CrashCard(cards);
            ContinueCard(cards);
            MotocardCard(cards);
            if (_world == null || _world.Status == OpenMenuWorld.State.Failed) PortraitRefresh();   // (with the world, its screen change draws it once the rider is built)
            _statusText.text = $"{OpenContent.Events.Count} EVENTS";
            if (_crashStatus != null && Platform.ReflexCrashLog.PromptDismissed) { _statusText.text = _crashStatus.ToUpperInvariant(); _crashStatus = null; }
            _nav.DefaultItem = () => single;
            _nav.OnCancel = () => false;
            Hints(false);
            _nav.Focus(single);
            HubCards3D(cards);
            IntroBegin();   // the first hub entry with the 3D menu: the loading hold, the walk-in, then the menu (OpenShell.MenuWorld.cs)
        }

        /// PLAY: the hub's catch-all page for the rides on your own (CAREER, RACE, FREE RIDE, MOTOSKILLS), in the hub's frame; BACK
        /// (B / Esc) returns to the hub.
        public void ShowPlay()
        {
            Clear(Screen.Play);
            SetBackground(HubPicture());
            MusicTier(2);
            HubFrame(out var menu, out var small, out var cards);
            OuUi.Text("PLAY", "ou-hub-eyebrow", menu);
            var buttons = new List<VisualElement>();
            foreach (var (name, label, caption, open) in PlayRows())
            {
                var b = HubButton(menu, label, caption, FromPlay(name, open));
                b.name = name;
                buttons.Add(b);
            }
            small.AddToClassList("ou-hub-small-back");
            var back = OuUi.Button("BACK", "ou-chip", ShowMain, small);
            back.name = "ou-hub-back";
            OuWidgets.LinkColumn(buttons);
            ((OuNavItem)buttons[buttons.Count - 1].userData).down = back;
            ((OuNavItem)back.userData).up = buttons[buttons.Count - 1];
            HubCards3D(cards);
            _statusText.text = $"{OpenContent.Events.Count} EVENTS";
            var first = buttons.FirstOrDefault(x => x.name == _playFocus) ?? buttons[0];
            _nav.DefaultItem = () => first;
            _nav.OnCancel = () => { ShowMain(); return true; };
            Hints(true);
            _nav.Focus(first);
        }

        /// A hub entry: the slanted bar behind it on focus, the big label and a caption line.
        static VisualElement HubButton(VisualElement parent, string label, string caption, Action submit)
        {
            var b = OuUi.Button(label, "ou-hub-button", submit, parent);
            OuTheme.Backdrop(b, "ou-hub-button-bg");
            OuUi.Text(caption, "ou-hub-caption", b);
            return b;
        }

        /// CONTINUE: the last event started (OpenHistory) with RIDE NOW, or a first-run QUICK START into RACE.
        void ContinueCard(VisualElement parent)
        {
            var recent = OpenHistory.Recent().FirstOrDefault(r => OpenContent.Find(r.eventKey) != null);
            var ev = recent != null ? OpenContent.Find(recent.eventKey) : null;
            var card = OuUi.Div("ou-card-panel ou-continue", parent);
            var shot = OuUi.Div("ou-continue-shot ou-continue-photo", card);
            var tex = ev != null ? OpenContent.Picture(ev) : OpenContent.Picture(HubEvent());
            if (tex) shot.style.backgroundImage = tex;
            var copy = OuUi.Div("ou-continue-copy", shot);
            OuUi.Text(ev != null ? "CONTINUE" : "QUICK START", "ou-eyebrow", copy);
            OuUi.Text(ev != null ? ev.locationName : "PICK YOUR FIRST EVENT", "ou-continue-title", copy);
            OuUi.Text(ev != null ? ev.typeName + "  ·  " + SetupLine(ev, recent) : "Every event type and every location of your install.",
                      "ou-continue-sub", copy);
            var go = OuUi.Button(ev != null ? "RIDE NOW" : "CHOOSE AN EVENT", "mxa-start ou-cta", ev != null ? (Action)(() => { _freeRide = ev.freeRide; StartEvent(ev, recent); }) : () => ShowBrowser(false), card);
            go.name = "ou-continue";
            OuTheme.Backdrop(go);
        }

        /// The session before ended unexpectedly (Reflex.Port.Platform.ReflexCrashLog.PreviousCrashed): CREATE SUPPORT REPORT (the
        /// platform's ReflexSupport.CreateReportForPlayer: a redacted ZIP shown in Finder / Explorer, nothing uploaded) or DISMISS,
        /// as Open Unleashed's MxaCrashPrompt.
        string _crashStatus;
        void CrashCard(VisualElement parent)
        {
            if (!Platform.ReflexCrashLog.PreviousCrashed || Platform.ReflexCrashLog.PromptDismissed) return;
            var card = OuUi.Div("ou-card-panel ou-crash-card", parent);
            card.name = "ou-crash-card";
            OuUi.Text("LAST SESSION", "ou-eyebrow", card);
            OuUi.Text(OpenReflex.Name + " CLOSED UNEXPECTEDLY", "ou-crash-title", card);
            OuUi.Text(_crashStatus ?? ("A support report collects the logs" + (Platform.ReflexCrashLog.PreviousCrashReport != null ? " and the crash report" : "") +
                      " into a ZIP on your computer (your user name and folders are taken out). Nothing is sent anywhere."), "mxa-note ou-crash-text", card);
            var row = OuUi.Div("ou-crash-buttons", card);
            OuUi.Button("CREATE SUPPORT REPORT", "ou-chip ou-chip-accent", () =>
            {
                string st; try { st = Platform.ReflexSupport.CreateReportForPlayer(); } catch (Exception e) { st = "The report could not be made: " + e.Message; }
                _crashStatus = st; Platform.ReflexCrashLog.PromptDismissed = true; ShowMain();
            }, row).name = "ou-crash-report";
            OuUi.Button("DISMISS", "ou-chip", () => { Platform.ReflexCrashLog.PromptDismissed = true; ShowMain(); }, row).name = "ou-crash-dismiss";
        }

        /// "AI AMATEUR · 3 LAPS · MX LITE".
        static string SetupLine(OpenEvent ev, OpenSetup s)
        {
            var e = ev.ev; var parts = new List<string>();
            if (s.Rules(e))
            {
                parts.Add(s.Ai(e) > 0 ? "AI " + OpenContent.AiName(s.Ai(e)) : "NO AI");
                int laps = s.Laps(e); if (laps > 0) parts.Add(laps + (laps == 1 ? " LAP" : " LAPS"));
            }
            parts.Add(OpenContent.VehicleTypeName(s.Type(e)));
            return string.Join("  ·  ", parts);
        }

        /// The MOTOCARD card: the profile's name (the offline persona), into the MOTOCARD pages (OpenShell.Motocard.cs).
        void MotocardCard(VisualElement parent)
        {
            var card = OuUi.Item(OuUi.Div("ou-card-panel ou-garage-card", parent), () => ShowMotocard());
            card.name = "ou-garage-card";
            PortraitFrame(card, "ou-garage-card-portrait");
            var copy = OuUi.Div("ou-garage-card-copy", card);
            OuUi.Text("MOTOCARD", "ou-eyebrow", copy);
            OuUi.Text((ReflexProfile.HasPersona ? ReflexProfile.Persona : "YOUR RIDER").ToUpperInvariant(), "ou-garage-card-name", copy);
            bool core = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex;
            string status = core ? (File.Exists(CoreGarageProfile.FilePath) ? "SAVED RIDER  ·  VEHICLES  ·  TUNING" : "YOUR INSTALLED RIDER  ·  CHANGES SAVE AUTOMATICALLY") : ReflexProfile.HasSave ? "SAVE GAME  ·  MOTOCAREER  ·  GARAGE" : "NO SAVE YET  ·  CAREER CREATES YOUR MOTOCARD";
            OuUi.Text(status, "ou-garage-card-sub", copy);
            OuUi.Text(core ? "STATS · ACHIEVEMENTS · RECORDS · TRICK LIBRARY" : "GEAR, VEHICLES AND TUNING IN THE GARAGE", "ou-garage-card-sub ou-faint", copy);
        }

        void Quit()
        {
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }

        public static string ShortPath(string p)
        {
            if (string.IsNullOrEmpty(p)) return "(not set)";
            return p.Length <= 46 ? p : "…" + p.Substring(p.Length - 45);
        }

        // --------------------------------------------------------------------------------------------- the original's pages

        System.Threading.Tasks.Task<bool> _signIn; OriginalEmbed.Purpose _signInFor; float _workingAge;

        /// CAREER / GARAGE: the original's pages, signed in first (the profile session, as the title's load page does).
        public void OpenOriginalFor(OriginalEmbed.Purpose purpose)
        {
            if (purpose == OriginalEmbed.Purpose.Career && Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) { ShowCoreCareer(); return; }
            if (purpose == OriginalEmbed.Purpose.Options) { OpenOriginal(OriginalEmbed.Purpose.Options, "Options Page"); return; }
            try { _signIn = ReflexProfile.Open(); }
            catch (Exception e) { Debug.LogWarning("[OpenReflex] profile not opened: " + e.Message); _signIn = System.Threading.Tasks.Task.FromResult(false); }
            _signInFor = purpose;
            ShowWorking(purpose == OriginalEmbed.Purpose.Career ? "CAREER" : "GARAGE", "SIGNING IN", "Reading your MOTOCARD from the save…");
        }

        void UpdateWorking()
        {
            _workingAge += Time.unscaledDeltaTime;
            if (_signIn == null || !_signIn.IsCompleted || _workingAge < 0.25f) return;
            var task = _signIn; _signIn = null;
            var p = ReflexProfile.Current;
            bool ready = !task.IsFaulted && task.Result && p != null && p.signedIn && p.MotocardSetup;
            if (_signInFor == OriginalEmbed.Purpose.Career) OpenOriginal(_signInFor, ready ? "Main Menu Page" : null);
            else OpenOriginal(_signInFor, ready ? "Garage Page" : null);
        }

        /// A screen that waits (signing in): title, a line, a spinner bar; Esc goes back to the hub.
        void ShowWorking(string title, string what, string text)
        {
            Clear(Screen.Working, "install");
            _workingAge = 0;
            _layer.AddToClassList("mxa-dim");
            OuTheme.Header(_layer, title, OpenReflex.Name);
            var box = OuUi.Div("mxa-panel mxa-install ou-working", _layer);
            OuUi.Text(what, "mxa-screen-title", box);
            OuUi.Text(text, "mxa-install-text", box);
            var bar = OuUi.Div("ou-progress ou-progress-busy", box);
            OuTheme.Backdrop(OuUi.Div("ou-progress-fill", bar));
            _nav.OnCancel = () => { _signIn = null; ShowHome(); return true; };
            Hints((OuUi.KeyCancel, "BACK"));
        }

        /// The original front end at page (null: its start page: the title, which signs in and makes the MOTOCARD), for purpose; the
        /// shell goes and comes back (a new front-end scene) when the original returns to its main menu.
        public void OpenOriginal(OriginalEmbed.Purpose purpose, string page)
        {
            if (!original) { Debug.LogWarning("[OpenReflex] no original front end in this scene"); return; }
            var fm = original;
            OuSound.Play("Accept");
            gameObject.SetActive(false);
            OriginalEmbed.Attach(fm, purpose, page);
            Destroy(gameObject);   // its UI, sounds and music go; the original's own take over
        }

        // ------------------------------------------------------------------------------------------------ options

        Action<string> _optSoundBefore; int _optFocusHub = -1;

        /// OPTIONS: the client's options pages (Reflex.Port.Options.ReflexOptions: CONTROLS .. SYSTEM, with FRONT END and GAME FOLDER on
        /// SYSTEM) over the shell; the shell's input waits while they are up, their sounds are the shell's (Reflex's UI sounds).
        public void ShowOptions() => ShowOptions(Options.ReflexOptions.Controls);
        public void ShowOptions(int tab)
        {
            if (Options.ReflexOptions.IsOpen) return;
            bool fromHub = Current == Screen.Main;
            _optSoundBefore = Options.OptUi.Sound;
            Options.OptUi.Sound = OuSound.Play;
            // the shell's own layer, hints and status step aside (their text must not show through the pages)
            _layer.style.display = DisplayStyle.None; _hints.style.display = DisplayStyle.None; _status.style.display = DisplayStyle.None;
            Options.ReflexOptions.Open(tab, () =>
            {
                Options.OptUi.Sound = _optSoundBefore;
                if (!this) return;
                _layer.style.display = DisplayStyle.Flex; _hints.style.display = DisplayStyle.Flex; _status.style.display = Current == Screen.Loading ? DisplayStyle.None : DisplayStyle.Flex;
                // back where they were opened (the hub's OPTIONS); GAME FOLDER closes them and opens the library itself
                if (fromHub && Current == Screen.Main) _nav.Focus(_layer.Q<VisualElement>("ou-hub-options"));
            });
        }

        /// SYSTEM > GAME FOLDER (ReflexOptionsHooks.OpenGameLibrary): GAME LIBRARY, DONE back to OPTIONS > SYSTEM.
        void OpenLibraryFromOptions()
        {
            if (!this) return;
            ShowLibrary(false);
        }

        /// SYSTEM > ABOUT > CREDITS: the credits screen, Back to OPTIONS > SYSTEM.
        void OpenCreditsFromOptions()
        {
            if (!this) return;
            ShowCredits(() => ShowOptions(Options.ReflexOptions.System));
        }

        // ------------------------------------------------------------------------------------------------ launch

        /// START EVENT / RIDE NOW: remembered (CONTINUE), the event loaded in the test-ride arena (PRESS START), then ReflexEventSelect.Launch as
        /// LocationSelect does (the loading screen when the arena cannot load it).
        void StartEvent(OpenEvent ev, OpenSetup setup)
        {
            if (ev == null || setup == null) return;
            var s = setup.Clone(); s.eventKey = ev.key;
            if (s.practice) OpenHistory.SaveSetup(s);   // (PRACTICE keeps its own setup; it is not a RECENT race)
            else OpenHistory.Remember(s);
            s_returnEvent = ev.key; s_returnFreeRide = _freeRide; s_returnPractice = s.practice;
            Clear(Screen.Loading);
            _hints.Clear();
            _sounds?.music?.SetEnable(false);   // InGameLoadingPage:Start: App.SetEnableUIMusic( FALSE )
            if (OpenSetup.LaunchOverride == null && ShowLoadArena(ev, s)) return;   // the test-ride arena while it loads, then PRESS START (OpenShell.LoadArena.cs)
            if (OpenSetup.LaunchOverride == null) OpenLoading.Show(ev, s);
            s.Launch();
        }
    }
}
