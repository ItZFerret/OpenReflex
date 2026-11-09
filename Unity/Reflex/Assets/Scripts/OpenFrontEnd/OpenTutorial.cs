// The MotoSkills tutorials on the OPEN REFLEX core (w-tutorials; _re/notes/tutorials.md): the six "MX Training" lessons ridden in the front end's
// own world UI_Compound, as the original rides them. The core runs the game's own TutorialManager (MXUIWorld + 0x22180: each lesson's start,
// checks, timers, outcome, voice requests, PlayerRider.TutorialsCompleted; core/game/fx_tutorial_manager.c) in the session's UI world
// (rx_session_tutorial_start); the pages the original's Lua shows around it are drawn here in the career look:
//   OpenTutorial     the launch from the shell's MOTOSKILLS page (the curtain, the race scene with the UI world's Free Ride) and the way back
//   OwnTutorial      the race scene's part: TutorialPauseMenu (the lesson list in the world), PreTutorialPopup (the lesson's title and
//                    instructions), TutorialActivation (the HUD label and hint), PostTutorialPopup / FailedTutorialPopup (RETRY / QUIT),
//                    TutorialConclusionPopup after the sixth; the lessons' voice; the lessons' objects as ShowLessonObjects leaves them
// Every text is the original's string (STR_TUTORIAL_<n>_*, the DYN_ texts the manager sets from their STR_ keys); nothing is decided here that
// the game decides: the pages only send the original pages' calls (App.EnableTutorial, App.DisableTutorial, App.PauseActiveWorld, RetryTutorial,
// the popup stack, SuspendPlayerInput) and show what the manager raised (ShowPreTutorialPopup, ShowPostTutorialPopup, ShowFailedTutorialPopup).
// The ridden MX Lite is the core's: MainMenu:OnTutorialKeypress's App.ChangeVehicle( row, 2 ) in rx_session_tutorial_start (w-tutcore); the
// session imports the profile with its equipped setup and the end equips that again before the profile is saved.
using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OpenTutorial
    {
        public const int Lessons = 6;   // TutorialPauseMenu.lua m_MaxTutorials

        /// The strings of lesson n (0..5): its title, the pause menu's brief description, the PreTutorialPopup's description.
        public static string Title(int n) => S("STR_TUTORIAL_" + (n + 1) + "_TITLE", "LESSON " + (n + 1));
        public static string Brief(int n) => S("STR_TUTORIAL_" + (n + 1) + "_BRIEF_DESC", "");
        public static string Description(int n) => S("STR_TUTORIAL_" + (n + 1) + "_DESCRIPTION", "");
        public static string S(string key, string def) { string v = OpenContent.Str(key, null); return string.IsNullOrEmpty(v) || v.StartsWith("STR_") || v.StartsWith("DYN_") ? def : v; }

        // The lessons' texts are the PC original's: "W key", "Space", "the mouse". With a pad as the active device (OuGlyphs.Current) the same
        // actions are named by the pad's buttons: gas = right trigger (Throttle), brake = left trigger, the stunt modifier = LB / L1, Rider Reflex = the
        // right stick (ControllerPresets Regular1: RiderReflexStickX / Y). The keyboard / mouse keeps the original's words.
        static readonly System.Text.RegularExpressions.Regex s_w = new System.Text.RegularExpressions.Regex(@"\b(?:the )?W key\b");
        static readonly System.Text.RegularExpressions.Regex s_s = new System.Text.RegularExpressions.Regex(@"\b(?:the )?S key\b");
        static readonly System.Text.RegularExpressions.Regex s_space = new System.Text.RegularExpressions.Regex(@"\b(?:the )?Space(?: ?bar| ?key)?\b");
        static readonly System.Text.RegularExpressions.Regex s_plusMouse = new System.Text.RegularExpressions.Regex(@"\+ (?:the |your )?mouse\b", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        static readonly System.Text.RegularExpressions.Regex s_mouse = new System.Text.RegularExpressions.Regex(@"\b(?:the |your )?mouse\b", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        /// A lesson text for the device in use: the pad's buttons where the original names the keyboard's and the mouse (identity on keyboard / mouse).
        public static string ForDevice(string text) => ForDevice(text, OuGlyphs.Current);
        public static string ForDevice(string text, OuGlyphs.Device device)
        {
            if (string.IsNullOrEmpty(text) || device == OuGlyphs.Device.Keyboard) return text;
            bool ps = device == OuGlyphs.Device.PlayStation;
            text = s_w.Replace(text, ps ? "R2" : "RT");
            text = s_s.Replace(text, ps ? "L2" : "LT");
            text = s_space.Replace(text, ps ? "L1" : "LB");
            text = s_plusMouse.Replace(text, "+ right stick");
            text = s_mouse.Replace(text, "the right stick");
            return text;
        }

        /// PlayerRider.TutorialsCompleted of the profile: the lessons unlocked are this + 1 (TutorialPauseMenu:Start).
        public static int Completed => Mathf.Clamp(ReflexProfile.Current?.T("PlayerRider").Int("TutorialsCompleted", 0, 0) ?? 0, 0, Lessons);

        /// The run (from START LESSON until the MOTOSKILLS page is back), null: none.
        public static ReflexTutorials.Open.Run Current => ReflexTutorials.Open.Current;
        /// The way back: the lesson to focus on the MOTOSKILLS page (OpenShell.TutorialReturn), -1 none.
        internal static int PendingReturn = -1;
        public static string FailedReason { get; internal set; }
        /// Tests / diagnostics: the run's last step ("launch", "riding", "returning", "menu").
        public static string Phase { get; internal set; } = "";
        public static float LastStartSeconds { get; private set; } = -1f;
        static float s_startAt;

        /// START LESSON on the MOTOSKILLS page: the black, then the race scene with the UI world's Free Ride and the tutorial mode.
        public static void Begin(int lesson)
        {
            if (Current != null) return;
            lesson = Mathf.Clamp(lesson, 0, Lessons - 1);
            var run = ReflexTutorials.Open.Begin(lesson);
            Phase = "launch"; s_startAt = Time.realtimeSinceStartup; FailedReason = null;
            Debug.Log("[MotoSkills] " + Title(lesson) + " (the core's App.ChangeVehicle( row, 2 ): the player's MX Lite)");
            OpenTestRideCurtain.Close("MOTOSKILLS", Title(lesson), () => ReflexEventSelect.Launch(run.ev.key, 0, false, "Reflex", 0, null));
        }

        /// QUIT MOTOSKILLS (the lesson list's row) / the conclusion's CONTINUE: the black, the front end's MOTOSKILLS page again.
        public static void Return(int focusLesson)
        {
            if (Current == null || Phase == "returning") return;
            Phase = "returning";
            PendingReturn = Mathf.Clamp(focusLesson, 0, Lessons - 1);
            OpenTestRideCurtain.Close("MOTOSKILLS", "BACK TO THE LESSONS", () => OriginalPauseMenu.QuitToFrontEnd());
        }

        /// The shell drew the MOTOSKILLS page again.
        internal static void Returned()
        {
            ReflexTutorials.Open.End();
            Phase = "menu";
            OpenTestRideCurtain.Open();
        }

        /// The lesson's first frames are drawn.
        internal static void Started()
        {
            LastStartSeconds = Time.realtimeSinceStartup - s_startAt;
            Phase = "riding";
            Debug.Log($"[MotoSkills] riding {LastStartSeconds:F2} s after START LESSON");
            OpenTestRideCurtain.Open();
        }

        internal static void Failed(string why)
        {
            Debug.LogWarning("[MotoSkills] the tutorial failed: " + why);
            FailedReason = why;
            Return(Current?.lesson ?? 0);
        }
    }

    /// The race scene's part of the MotoSkills tutorials (attached by OriginalWorldRuntime when ReflexTutorials.Open is active on the open core).
    [DefaultExecutionOrder(-210), DisallowMultipleComponent]
    public sealed class OwnTutorial : MonoBehaviour
    {
        public enum Page { None, Menu, Pre, Hud, Post, Failed, Conclusion, Leaving }

        OriginalWorldRuntime runtime;
        GameObject host;
        VisualElement root, hud, menuLayer, menuList, menuBody, menuHints, popupLayer, popupCard, popupButtons;
        Label hudLabel, hudHint, menuDesc, popupTitle, popupText;
        // PreTutorialPopup in the original's own layout (PreTutorialPopup.lyr): title, the looping Bink video, the crawling description
        VisualElement preLayer, preMask, preVideo, preButtons;
        Label preTitle, preText;
        OriginalVideo video; GameObject videoHost; bool videoShown;
        float crawlClock, crawlTextHeight = -1;
        OuMenuInput input; OuNav nav;
        Page page = Page.None;
        bool started, firstLesson;
        int shownFrames, openedFrame = -1, focusLesson;
        uint seenEvent, seenVoice, seenStop;
        int previousCompleted = -1;
        float failedFor, objectsAt;
        const uint VoiceWrapper = 0x00fe7100;

        /// The page on show (tests).
        public Page Current => page;
        /// One of the pages has the keyboard / pad: the game takes no input (OriginalWorldRuntime.OwnMenuActive).
        public bool BlocksInput => page != Page.None && page != Page.Hud;
        public string PopupTitle => page == Page.Pre ? preTitle?.text : popupTitle?.text;
        public string PopupText => page == Page.Pre ? preText?.text : popupText?.text;
        /// The PreTutorialPopup's video (tests, diagnostics): its name, whether a frame is on the screen, the frame number, the crawl's offset.
        public string VideoPath => video ? video.video : null;
        public bool VideoHasFrame => video && video.HasFrame;
        public bool VideoFailed => video && video.Failed;
        public int VideoFrame => video ? video.Frame : 0;
        public float CrawlOffset { get; private set; }
        public float CrawlTextHeight => crawlTextHeight;
        public string HudLabel => hudLabel?.text;
        public string HudHint => hudHint?.text;
        public RxCore.RxTutorialState State => Sim?.TutorialSnapshot;
        /// The lesson objects drawn (their level instances active), tests.
        public int ObjectsShown { get; private set; }
        public int ObjectsTotal { get; private set; }
        public string ObjectsShownNames { get; private set; } = "";

        RxCore.OpenReflexSimulation Sim => runtime ? runtime.SimDriver?.Open as RxCore.OpenReflexSimulation : null;

        public static OwnTutorial Attach(OriginalWorldRuntime rt)
        {
            if (!rt) return null;
            var c = rt.GetComponent<OwnTutorial>() ?? rt.gameObject.AddComponent<OwnTutorial>();
            c.runtime = rt;
            return c;
        }

        void Awake() { if (!runtime) runtime = GetComponent<OriginalWorldRuntime>(); }

        void Command(string what, int value = 0) => Sim?.TutorialCommand(what, value);

        // ------------------------------------------------------------------------------------------------------ the frame
        void Update()
        {
            if (!runtime) return;
            string failure = runtime.FailureMessage;
            if (failure == null && Sim != null && Sim.CurrentPhase == RxCore.OpenReflexSimulation.Phase.Failed) failure = Sim.Failure;
            if (failure != null)
            {
                failedFor += Time.unscaledDeltaTime;
                if (failedFor > 0.2f && OpenTutorial.Phase != "returning") OpenTutorial.Failed(failure);
                return;
            }
            if (!runtime.Running) return;
            var st = State;
            if (st == null || st.spawned == 0) return;
            Build();
            if (!started)
            {   // the first frames drawn: the curtain lifts and the chosen lesson starts (TutorialPauseMenu:OnAccept of the MOTOSKILLS page's row)
                if (++shownFrames < 4) return;
                started = true;
                seenEvent = st.eventSerial; seenVoice = st.voiceSerial; seenStop = st.stopSerial;
                previousCompleted = st.completed;
                OpenTutorial.Started();
                StartLesson(OpenTutorial.Current?.lesson ?? 0);
            }
            SyncObjects();
            Voice(st);
            // the manager's UI events, in order (ShowPreTutorialPopup / ShowPostTutorialPopup / ShowFailedTutorialPopup / ShowTutorialHUD)
            while (seenEvent < st.eventSerial)
            {
                int ev = st.EventAt(seenEvent); seenEvent++;
                if (ev == RxCore.RxTutorialState.EventPrePopup) ShowPre(st);
                else if (ev == RxCore.RxTutorialState.EventPostPopup) ShowPost(st);
                else if (ev == RxCore.RxTutorialState.EventFailedPopup) ShowFailed(st);
            }
            if (page == Page.Hud) RenderHud(st);
            if (page == Page.Pre) TickPre();
            if (OpenTutorial.Phase == "returning" || page == Page.Leaving) return;
            if (Reflex.Port.Options.ReflexOptions.IsOpen || Reflex.Port.Options.ReflexOptions.ClosedFrame == Time.frameCount) return;
            if (page == Page.Hud || page == Page.None) { if (PausePressed()) PauseRequest(); return; }
            if (openedFrame == Time.frameCount) return;
            nav?.Update(input);
        }

        static bool PausePressed()
        {
            if (Keyboard.current?.escapeKey.wasPressedThisFrame == true) return true;
            foreach (var pad in Gamepad.all) if (pad.startButton.wasPressedThisFrame) return true;
            return false;
        }

        /// MXUIManager_OnPauseRequest in tutorial mode: GotoTutorialPauseMenuPage (the lesson list), never the race's pause page.
        public void PauseRequest()
        {
            if (page == Page.Hud || page == Page.None) ShowMenu();
        }

        // ------------------------------------------------------------------------------------------------------ the lessons' objects
        /// ShowLessonObjects' decision on the level's instances: a TUT<n> model / scrolling texture is drawn while the core's entity is shown (+0xa2).
        void SyncObjects()
        {
            if (Time.unscaledTime < objectsAt) return;
            objectsAt = Time.unscaledTime + 0.1f;
            var objects = Sim?.TutorialObjects; if (objects == null) return;
            var level = FindAnyObjectByType<OriginalLevel>();
            if (!level || level.Content == null) return;
            var shown = new Dictionary<string, bool>(StringComparer.Ordinal);
            foreach (var o in objects) if (!string.IsNullOrEmpty(o.name)) shown[o.name] = o.shown;
            int on = 0, total = 0; string names = "";
            foreach (var inst in level.Content.instances)
            {
                if (!inst.transform || inst.name == null || !shown.TryGetValue(inst.name, out bool show)) continue;
                total++;
                if (show) { on++; names += (names.Length > 0 ? " " : "") + inst.name; }
                var go = inst.transform.gameObject;
                if (go.activeSelf != show) go.SetActive(show);
            }
            ObjectsShown = on; ObjectsTotal = total; ObjectsShownNames = names;
        }

        // ------------------------------------------------------------------------------------------------------ the voice
        /// UILocSound_Play( "AudioLocalizedTutorial", "Audio", <path>, 9, "TutorialManager" ): the manager's voice requests; its stops
        /// (LocSoundList_StopOwner) and the pages' App.StopTutorialAudio.
        void Voice(RxCore.RxTutorialState st)
        {
            if (st.stopSerial != seenStop) { seenStop = st.stopSerial; StopVoice(); }
            if (st.voiceSerial == seenVoice) return;
            seenVoice = st.voiceSerial;
            if (string.IsNullOrEmpty(st.voice)) return;
            StopVoice();
            bool ok = ReflexUiVoice.Play(runtime.gameAudio, "AudioLocalizedTutorial", "Audio", st.voice, VoiceWrapper);
            Debug.Log("[MotoSkills] voice " + st.voice + (ok ? "" : " (not installed)"));
        }
        void StopVoice() { if (runtime && runtime.gameAudio) ReflexUiVoice.Stop(runtime.gameAudio, VoiceWrapper); }

        // ------------------------------------------------------------------------------------------------------ the pages
        void Build()
        {
            if (root != null) return;
            OuUi.EnsureEventSystem();
            host = new GameObject("Open Reflex MotoSkills"); host.transform.SetParent(transform, false);
            root = OuUi.CreateDocument(host, 66); OuTheme.Apply(root);
            var style = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuRacePause");
            if (style) root.styleSheets.Add(style);
            // TutorialActivation: the lesson's label and hint, top centre
            hud = OuUi.Div("", root); hud.name = "tutorial-hud"; hud.pickingMode = PickingMode.Ignore;
            var hs = hud.style; hs.position = Position.Absolute; hs.left = 0; hs.right = 0; hs.top = 150; hs.alignItems = Align.Center;   // (under the Free Ride HUD's compass)
            var card = OuUi.Div("", hud); card.pickingMode = PickingMode.Ignore;
            var cs = card.style; cs.paddingLeft = 28; cs.paddingRight = 28; cs.paddingTop = 12; cs.paddingBottom = 14; cs.maxWidth = 980;
            cs.backgroundColor = new Color(0.04f, 0.045f, 0.05f, 0.80f); cs.borderTopWidth = 4; cs.borderTopColor = new Color(0.80f, 0.37f, 0.16f); cs.alignItems = Align.Center;
            hudLabel = OuUi.Text("", "ou-eyebrow2 ou-label", card); hudLabel.name = "tutorial-hud-label"; hudLabel.enableRichText = false;
            hudHint = OuUi.Text("", "ou-dialog-lead", card); hudHint.name = "tutorial-hud-hint"; hudHint.enableRichText = false;
            hudHint.style.whiteSpace = WhiteSpace.Normal; hudHint.style.unityTextAlign = TextAnchor.MiddleCenter; hudHint.style.marginTop = 4;
            hud.style.display = DisplayStyle.None;
            // TutorialPauseMenu: the lesson list (the world paused under it)
            menuLayer = OuUi.Div("mxa-layer mxa-dim ou-race-pause", root); menuLayer.name = "tutorial-menu";
            menuBody = OuUi.Div("ou-pause-body", menuLayer);
            menuHints = OuUi.Div("mxa-hints", menuLayer);
            menuLayer.style.display = DisplayStyle.None;
            // the popups (PreTutorialPopup, PostTutorialPopup, FailedTutorialPopup, TutorialConclusionPopup)
            popupLayer = OuUi.Div("mxa-layer ou-dialog-layer", root); popupLayer.name = "tutorial-popup";
            popupCard = OuWidgets.Dialog(popupLayer);
            popupTitle = OuUi.Text("", "ou-dialog-title ou-display", popupCard); popupTitle.name = "tutorial-popup-title"; popupTitle.enableRichText = false;
            popupText = OuUi.Text("", "ou-dialog-lead", popupCard); popupText.name = "tutorial-popup-text"; popupText.enableRichText = false;
            popupText.style.whiteSpace = WhiteSpace.Normal; popupText.style.maxWidth = 760;
            popupButtons = OuUi.Div("ou-button-row", popupCard);
            popupLayer.style.display = DisplayStyle.None;
            // PostTutorialPopup:MouseActivation (UI event MouseActivation_Container): a click on the popup = OnAcceptKeypress (the CONTINUE); the
            // FailedTutorialPopup's mouse handler only routes ButtonOne / ButtonTwo (its buttons, clicked as any item), the conclusion has none
            popupCard.RegisterCallback<ClickEvent>(ev => { if (page == Page.Post && ClickAccepts(ev)) PostContinue(); });
            BuildPre();
            nav = new OuNav { Screen = root, OnCancel = Cancel };
        }

        void Lease(bool on)
        {
            Reflex.Port.Platform.ReflexCursor.Want(this, on);   // App.ShowMouseCursor( on ): shown for a mouse player only
        }

        void Show(Page p)
        {
            page = p; openedFrame = Time.frameCount;
            menuLayer.style.display = p == Page.Menu ? DisplayStyle.Flex : DisplayStyle.None;
            popupLayer.style.display = p == Page.Post || p == Page.Failed || p == Page.Conclusion ? DisplayStyle.Flex : DisplayStyle.None;
            preLayer.style.display = p == Page.Pre ? DisplayStyle.Flex : DisplayStyle.None;
            if (p != Page.Pre) CloseVideo();                        // PreTutorialPopup:End: self.m_Video:Close()
            hud.style.display = p == Page.Hud ? DisplayStyle.Flex : DisplayStyle.None;
            bool modal = p != Page.Hud && p != Page.None && p != Page.Leaving;
            if (modal) { input?.Dispose(); input = new OuMenuInput(); runtime.ClearMenuInput(); }
            else { input?.Dispose(); input = null; }
            Lease(modal);
            if (p == Page.Post || p == Page.Failed || p == Page.Conclusion) { OuMotion.Sync(popupLayer); popupLayer.RemoveFromClassList("ou-in"); OuMotion.Fade(popupLayer); }
            if (p == Page.Pre) { OuMotion.Sync(preLayer); preLayer.RemoveFromClassList("ou-in"); OuMotion.Fade(preLayer); }
        }

        bool Cancel()
        {
            if (page == Page.Menu) return true;                    // TutorialPauseMenu: no back (QUIT MOTOSKILLS is its row)
            if (page == Page.Failed) { Quit(); return true; }       // FailedTutorialPopup:OnBackKeypress = QUIT
            if (page == Page.Conclusion) { ConclusionContinue(); return true; }
            return true;                                            // Pre / Post: OnBackKeypress does nothing
        }

        // TutorialPauseMenu:OnAccept: SuspendPlayerInput off, App.EnableTutorial( lesson ), App.SwitchToHud (the popup comes with the lesson's
        // start: its ShowPreTutorialPopup is raised by StartLesson at once, so the stack is up before the first frame of the lesson; its Start
        // sets SuspendPlayerInput again), then the menu's End: App.PauseActiveWorld( FALSE )
        void StartLesson(int lesson)
        {
            focusLesson = lesson; firstLesson = true;
            Command("suspend", 0);
            Command("lesson", lesson);
            Command("popup", 1);
            Command("suspend", 1);                                  // PreTutorialPopup:Start
            Command("pause", 0);                                    // TutorialPauseMenu:End: App.PauseActiveWorld( FALSE )
            OuSound.Play("Accept");
            Debug.Log("[MotoSkills] App.EnableTutorial( " + lesson + " ): " + OpenTutorial.Title(lesson));
            page = Page.None; Show(Page.None);
        }

        // ---- PreTutorialPopup: title STR_TUTORIAL_<n>_TITLE, DYN_TUTORIAL_DESCRIPTION; CONTINUE -> HidePreTutorialPopup + ShowTutorialHUD
        void ShowPre(RxCore.RxTutorialState st)
        {
            if (page == Page.Leaving) return;
            int n = Mathf.Clamp(st.currentTutorial - 1, 0, OpenTutorial.Lessons - 1);
            preTitle.text = OpenTutorial.Title(n).ToUpperInvariant();
            preText.text = OpenTutorial.ForDevice(OpenTutorial.S(st.description, OpenTutorial.Description(n)));
            Buttons(preButtons, ("CONTINUE", "tutorial-continue", PreContinue));
            Command("popup", 1);
            Command("suspend", 1);                                  // PreTutorialPopup:Start: SuspendPlayerInput TRUE
            OuSound.Play("PopupShow");
            Show(Page.Pre);
            StartVideo(n + 1);
            Debug.Log("[MotoSkills] PreTutorialPopup: " + preTitle.text + " / " + st.description);
        }

        // ---- the PreTutorialPopup's layout (PreTutorialPopup.lyr.xml, a 1280 x 720 page; x1.5 in the 1920 x 1080 panel):
        //      Background 0,110 1280x500 black 128 | Tutorial Popup Title 132,113 (ParalucentCondDemBol48, 204,95,41) | TitleBackground -37,169 1300x4 orange |
        //      Tutorial Video 644,211 512x256 (Loop, AutoStart off; PreTutorialPopup:Start: 512_256_VideoRenderTarget, Videos\Tutorial<n>, Play) |
        //      TextMaskContainer 133,211 500x300 holding the UIScrollingTextComponent (ParalucentCondLight28, 170,169,151; Vertical, TimeToPause 0,
        //      TimeToScroll 30, ScrollMode Wrap, Linear) | BottomContainer 485,537: the CONTINUE legend
        const float PageScale = 1.5f;
        const float CrawlSeconds = 30f, MaskHeight = 300f * PageScale;
        void BuildPre()
        {
            preLayer = OuUi.Div("ou-pretut-layer", root); preLayer.name = "tutorial-pre"; preLayer.pickingMode = PickingMode.Ignore;
            OuUi.Div("ou-pretut-band", preLayer);
            OuUi.Div("ou-pretut-rule", preLayer);
            var stage = OuUi.Div("ou-pretut-stage", preLayer); stage.pickingMode = PickingMode.Ignore;
            preTitle = OuUi.Text("", "ou-pretut-title", stage); preTitle.name = "tutorial-pre-title"; preTitle.enableRichText = false;
            preVideo = OuUi.Div("ou-pretut-video", stage); preVideo.name = "tutorial-pre-video";
            preMask = OuUi.Div("ou-pretut-mask", stage); preMask.name = "tutorial-pre-mask";
            preText = OuUi.Text("", "ou-pretut-text", preMask); preText.name = "tutorial-pre-text"; preText.enableRichText = false;
            preText.RegisterCallback<GeometryChangedEvent>(_ => { float h = preText.resolvedStyle.height; if (!float.IsNaN(h)) crawlTextHeight = h; });
            preButtons = OuUi.Div("ou-pretut-buttons", stage);
            // PreTutorialPopup:MouseActivation: a click on the popup (its band, the video, the text) = OnAcceptKeypress
            preLayer.RegisterCallback<ClickEvent>(ev => { if (page == Page.Pre && ClickAccepts(ev)) PreContinue(); });
            preLayer.style.display = DisplayStyle.None;
        }

        /// PreTutorialPopup:Start: the lesson's video, looping, in the 512 x 256 box.
        void StartVideo(int lesson)
        {
            CloseVideo();
            crawlClock = 0; CrawlOffset = 0;
            preText.style.top = 0;
            videoHost = new GameObject("Tutorial Video (Bink)"); videoHost.transform.SetParent(transform, false);
            video = OriginalVideo.Create(videoHost, "Videos\\Tutorial" + lesson, false, true, new Vector2Int(512, 256));
            video.SetVolume(Mathf.Clamp01(PlayerPrefs.GetFloat("ReflexAudio.master", 1f)));
            video.Play();
            videoShown = false;
            preVideo.style.backgroundImage = StyleKeyword.None;
            Debug.Log("[MotoSkills] PreTutorialPopup video Videos\\Tutorial" + lesson + " (512x256, loop)");
        }
        void CloseVideo()
        {
            if (video) { video.Close(); Destroy(video); }
            if (videoHost) Destroy(videoHost);
            video = null; videoHost = null; videoShown = false;
            if (preVideo != null) preVideo.style.backgroundImage = StyleKeyword.None;
        }

        /// Each frame of the popup: the video's picture into its box, and the description's crawl (UIScrollingTextComponent::vf11 005cd0c0, vf37 005cce60).
        void TickPre()
        {
            if (video && !videoShown && video.HasFrame && video.Texture)
            {
                preVideo.style.backgroundImage = new StyleBackground(Background.FromRenderTexture(video.Texture));
                videoShown = true;
            }
            // ScrollMode Wrap (the engine's names: Bounce 0, Wrap 1, Reset 2): when the text is taller than the box (ForceScroll is not set), it starts below the
            // box (offset = the box's height), runs linearly to just above it (offset = -the text's height) over TimeToScroll, then starts again; a text that fits stays at the top
            float box = MaskHeight, text = crawlTextHeight;
            if (text > box)
            {
                crawlClock = (crawlClock + Time.unscaledDeltaTime) % CrawlSeconds;
                float e = crawlClock / CrawlSeconds;
                CrawlOffset = box + (-text - box) * e;
            }
            else { crawlClock = 0; CrawlOffset = 0; }
            preText.style.top = CrawlOffset;
        }
        /// A left click that is not on one of the popup's own buttons (those submit themselves).
        static bool ClickAccepts(ClickEvent ev)
        {
            if (ev.button != 0) return false;
            for (var e = ev.target as VisualElement; e != null; e = e.parent) if (e.userData is OuNavItem) return false;
            return true;
        }
        void PreContinue()
        {
            if (page != Page.Pre || openedFrame == Time.frameCount) return;
            OuSound.Play("Accept");
            Command("popup", 0);                                    // PreTutorialPopup:End: the stack empties (SuspendPlayerInput FALSE, StopTutorialAudio)
            Command("suspend", 0);
            StopVoice();
            CloseVideo();
            Show(Page.Hud);
            RenderHud(State);
        }

        void RenderHud(RxCore.RxTutorialState st)
        {
            if (st == null) return;
            int n = Mathf.Clamp(st.currentTutorial - 1, 0, OpenTutorial.Lessons - 1);
            hudLabel.text = OpenTutorial.S(st.hudLabel, OpenTutorial.Title(n)).ToUpperInvariant();
            hudHint.text = OpenTutorial.ForDevice(OpenTutorial.S(st.activation, ""));
        }

        // ---- PostTutorialPopup: DYN_TUTORIAL_COMPLETION, CONTINUE -> the lesson list, or the conclusion when the sixth was just completed
        void ShowPost(RxCore.RxTutorialState st)
        {
            if (page == Page.Leaving) return;
            int n = Mathf.Clamp(st.currentTutorial - 1, 0, OpenTutorial.Lessons - 1);
            popupTitle.text = OpenTutorial.Title(n).ToUpperInvariant();
            popupText.text = OpenTutorial.ForDevice(OpenTutorial.S(st.completion, ""));
            Buttons(("CONTINUE", "tutorial-continue", PostContinue));
            Command("popup", 1);
            Command("suspend", 1);                                  // PostTutorialPopup:Start: SuspendPlayerInput TRUE, App.DisableTutorial( )
            Command("disable");
            OuSound.Play("PopupShow");
            Show(Page.Post);
            focusLesson = Mathf.Min(n + 1, OpenTutorial.Lessons - 1);
            Debug.Log($"[MotoSkills] PostTutorialPopup: {popupTitle.text} / {st.completion} (TutorialsCompleted {st.completed})");
        }
        void PostContinue()
        {
            if (page != Page.Post || openedFrame == Time.frameCount) return;
            OuSound.Play("Accept");
            Command("popup", 0);
            Command("suspend", 0);                                  // PostTutorialPopup:End
            StopVoice();
            var st = State;
            if (st != null && st.completed == OpenTutorial.Lessons && previousCompleted != st.completed) { ShowConclusion(); return; }
            ShowMenu();
        }

        // ---- FailedTutorialPopup: DYN_TUTORIAL_FAILED, RETRY ("RetryTutorial" + ShowTutorialHUD) / QUIT (App.DisableTutorial, the lesson list)
        void ShowFailed(RxCore.RxTutorialState st)
        {
            if (page == Page.Leaving) return;
            int n = Mathf.Clamp(st.currentTutorial - 1, 0, OpenTutorial.Lessons - 1);
            popupTitle.text = OpenTutorial.Title(n).ToUpperInvariant();
            popupText.text = OpenTutorial.ForDevice(OpenTutorial.S(st.failed, ""));
            Buttons(("RETRY", "tutorial-retry", Retry), ("QUIT", "tutorial-quit", Quit));
            Command("popup", 1);
            Command("suspend", 1);                                  // FailedTutorialPopup:Start
            OuSound.Play("PopupShow");
            Show(Page.Failed);
            focusLesson = n;
            Debug.Log("[MotoSkills] FailedTutorialPopup: " + popupTitle.text + " / " + st.failed);
        }
        void Retry()
        {
            if (page != Page.Failed || openedFrame == Time.frameCount) return;
            OuSound.Play("Accept");
            StopVoice();
            Command("popup", 0);
            Command("suspend", 0);                                  // FailedTutorialPopup:End
            Command("retry");                                       // RetryTutorial: the lesson again (its PreTutorialPopup when from the first step)
            Show(Page.Hud);
        }
        void Quit()
        {
            if (page != Page.Failed) return;
            OuSound.Play("Accept");
            StopVoice();
            Command("popup", 0);
            Command("suspend", 0);                                  // FailedTutorialPopup:End
            Command("disable");
            ShowMenu();
        }

        // ---- TutorialConclusionPopup: STR_TUTORIAL_CONCLUSION_TITLE / _BODY, the outro; CONTINUE -> the front end
        void ShowConclusion()
        {
            popupTitle.text = OpenTutorial.S("STR_TUTORIAL_CONCLUSION_TITLE", "MOTOSKILLS COMPLETED").ToUpperInvariant();
            popupText.text = OpenTutorial.S("STR_TUTORIAL_CONCLUSION_BODY", "");
            Buttons(("CONTINUE", "tutorial-continue", ConclusionContinue));
            Command("popup", 1);
            Command("suspend", 1);                                  // TutorialConclusionPopup:Start: SuspendPlayerInput TRUE, App.DisableTutorial( )
            Command("disable");
            StopVoice();
            ReflexUiVoice.Play(runtime.gameAudio, "AudioLocalizedTutorial", "Audio", "TUTORIAL_OUTRO/VO/OUTRO", VoiceWrapper);
            OuSound.Play("PopupShow");
            Show(Page.Conclusion);
        }
        void ConclusionContinue()
        {
            if (page != Page.Conclusion || openedFrame == Time.frameCount) return;
            OuSound.Play("Accept");
            Leave(0);
        }

        // ---- TutorialPauseMenu: the six lessons (TutorialsCompleted + 1 unlocked; the others' Lock), STR_TUTORIAL_<n>_BRIEF_DESC of the focused
        //      one; accept starts it; QUIT MOTOSKILLS leaves (EnableTutorialMode( FALSE ) when the session goes)
        public void ShowMenu()
        {
            if (page == Page.Leaving) return;
            Build();
            Command("popup", 0);
            Command("suspend", 0);                                  // (a popup's End, when one was up)
            Command("disable");                                     // TutorialPauseMenu:Start: App.DisableTutorial( ), SuspendPlayerInput TRUE,
            Command("suspend", 1);                                  //   App.PauseActiveWorld( TRUE ), StopTutorialAudio
            Command("pause", 1);
            StopVoice();
            var st = State;
            int completed = st != null && st.completed >= 0 ? st.completed : OpenTutorial.Completed;
            previousCompleted = completed;                          // PreviousTutorialsCompleted
            int available = Mathf.Min(completed + 1, OpenTutorial.Lessons);
            menuBody.Clear();
            OuTheme.Header(menuBody, "MOTOSKILLS", (completed >= OpenTutorial.Lessons ? "ALL LESSONS COMPLETED" : completed + " OF 6 LESSONS COMPLETED"));
            var pageEl = OuUi.Div("ou-page", menuBody);
            var side = OuUi.Div("ou-page-side ou-pause-side", pageEl);
            menuList = OuUi.Div("", side); menuList.style.width = 640;
            menuDesc = OuUi.Text("", "ou-dialog-lead", side); menuDesc.name = "tutorial-menu-desc"; menuDesc.enableRichText = false;
            menuDesc.style.whiteSpace = WhiteSpace.Normal; menuDesc.style.maxWidth = 640; menuDesc.style.marginTop = 18;
            VisualElement focus = null;
            for (int i = 0; i < OpenTutorial.Lessons; i++)
            {
                int n = i; bool locked = i >= available, done = i < completed;
                var b = OuUi.Item(OuUi.Div("mxa-button mxa-row", menuList), () => MenuAccept(n)); b.name = "tutorial-lesson-" + (i + 1);
                OuUi.Text((i + 1) + "   " + OpenTutorial.Title(i).ToUpperInvariant(), "mxa-row-label", b).enableRichText = false;
                OuUi.Text(done ? "COMPLETED" : locked ? "LOCKED" : "", "mxa-value", OuUi.Div("mxa-row-value", b));
                if (locked) b.AddToClassList("ou-locked");
                b.RegisterCallback<FocusInEvent>(_ => menuDesc.text = locked ? "Complete the lesson before to unlock this one." : OpenTutorial.ForDevice(OpenTutorial.Brief(n)));
                if (i == Mathf.Clamp(focusLesson, 0, available - 1)) focus = b;
            }
            var quit = OuUi.Button("QUIT MOTOSKILLS", "mxa-main-button", () => Leave(focusLesson), OuUi.Div("ou-actions mxa-pause-col mxa-pause-menu", side)); quit.name = "tutorial-quit-motoskills";
            quit.parent.style.marginTop = 16;
            OuUi.Hints(menuHints, (OuUi.KeySubmit, "SELECT"));
            OuSound.Play("PopupShow");
            Show(Page.Menu);
            var f = focus ?? quit;
            nav.DefaultItem = () => f;
            menuBody.schedule.Execute(() => { if (page == Page.Menu && f.panel != null) { nav.Focus(f); } });
            Debug.Log($"[MotoSkills] TutorialPauseMenu: {completed} completed, {available} available");
        }
        void MenuAccept(int lesson)
        {
            if (page != Page.Menu || openedFrame == Time.frameCount) return;
            var st = State;
            int completed = st != null && st.completed >= 0 ? st.completed : OpenTutorial.Completed;
            if (lesson >= Mathf.Min(completed + 1, OpenTutorial.Lessons)) { OuSound.Play("Locked"); return; }
            StartLesson(lesson);
        }

        void Leave(int focus)
        {
            if (page == Page.Leaving) return;
            Show(Page.Leaving);
            StopVoice();
            Command("popup", 0);
            Command("pause", 0);
            OpenTutorial.Return(focus);
        }

        void Buttons(params (string text, string name, Action submit)[] items) => Buttons(popupButtons, items);
        void Buttons(VisualElement popupButtons, params (string text, string name, Action submit)[] items)
        {
            popupButtons.Clear();
            VisualElement first = null;
            foreach (var (text, name, submit) in items)
            {
                var b = OuUi.Button(text, "mxa-main-button" + (first == null ? " ou-primary" : ""), submit, popupButtons); b.name = name;
                first ??= b;
            }
            nav.DefaultItem = () => first;
            popupButtons.schedule.Execute(() => { if (first != null && first.panel != null) nav.Focus(first); });
        }

        /// Tests: the focused item's own submit (a click on it).
        public void SubmitFocused() { var f = nav?.Focused; (f?.userData as OuNavItem)?.submit?.Invoke(); }
        /// Tests: the focus on the page's item of that name (a lesson row, QUIT MOTOSKILLS).
        public void FocusItem(string name)
        {
            var e = root?.Q<VisualElement>(name);
            if (e != null) { nav?.Focus(e); if (name.StartsWith("tutorial-lesson-") && int.TryParse(name.Substring(16), out int n)) focusLesson = n - 1; }
        }

        void OnDestroy() { input?.Dispose(); Lease(false); StopVoice(); CloseVideo(); if (host) Destroy(host); if (runtime && runtime.tutorial == this) runtime.tutorial = null; }
    }
}
