// The original front end (MXUI FrontEnd.<LANG>): page graph, page stack, popups, input and the hand-over to the race scene.
//
// Pages and their layers, edges (event -> page, Source/DestinationCompositeAnimation) and composite animations are read from the
// install's FrontEndPageGraph.pag (FrontEnd.<LANG>); popups from PopupPageStack.pag pushed by the OverlayPageGraph.pag edges; the
// loading page from TransitionPageGraph.pag (Shared.<LANG>). Each layer's Lua script is ported to C# (FrontEndScripts.cs).
// Engine behaviour reproduced: on a page change the source page's composite (PageOut) plays, then the old layers End, the new
// page's layers Start (in page order; the button legend is cleared first, as ButtonLegend:Start does) and its composite (PageIn)
// plays. Keys are the game's UI map keys (KeyBind_Core.xml): arrows / d-pad / left stick navigate, Enter / A accept, Esc / B
// back, F3 / Start start, F1 / X toggle, F2 / Y misc, Tab / Back select.
//
// Hand-over (LocationSelect:OnAccept -> App.SetupEvent / StartEvent / "GotoLoadingPage" -> TransitionPageGraph "InGame Loading
// Page" = Background01.lyr + LoadingScreen.lyr): the loading page is shown over everything (FrontEndLoadingOverlay, kept across
// the scene load) and ReflexEventSelect.Launch loads the race scene with the chosen event and AI; the page leaves when the race
// scene's original world is running (its first build from the install takes ~40 s; the build status is shown under LOADING).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.UI;
using UnityEngine.UI;

namespace Reflex.Port.FrontEnd
{
    [DisallowMultipleComponent]
    public sealed class FrontEndManager : MonoBehaviour
    {
        [Tooltip("Reflex/FrontEnd UI (the original DefaultIcon / TextIcon programs + UIMaskContainer masking, hand-ported).")]
        public Shader uiShader;
        [Tooltip("The race scene (build settings name) the front end hands the chosen event to.")]
        public string raceScene = "Reflex";
        [Tooltip("Remember the chosen event / AI in PlayerPrefs (ReflexEvent / ReflexAI) like the race scene's F2 list.")]
        public bool rememberChoice = true;
        [Tooltip("First page (FrontEndPageGraph.pag), after the startup sequence.")]
        public string startPage = "Title Page";
        [Tooltip("The startup sequence first, as a RELEASE build plays it (FrontEndIntroPageGraph.pag: legal screen, publisher / studio logo videos, intro video; FrontEndVideo.cs).")]
        public bool playIntro = true;
        /// Headless (batch-mode) runs start at the title, as the tests expect, unless this is set (BinkVideoTests).
        public static bool IntroInBatchMode;

        /// The page to return to from a quit event (the race scene's pause QUIT loads this scene again).
        public static string ReturnPage;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetStatics() { ReturnPage = null; LaunchOverride = null; LaunchedGarage = null; }   // play sessions start at the title (domain reload is off)

        public FeAssets assets;
        public OriginalButtonLegend legend;
        public FeSounds sounds;
        public readonly Dictionary<string, object> vars = new Dictionary<string, object>();   // App.SetUIVariable*
        public Canvas canvas; public RectTransform root;
        public string Status { get; private set; } = "";

        sealed class Edge { public string evt, dest, graph, srcAnim, dstAnim; public bool allowBack = true, clearBack, push, pop; }
        sealed class Page { public string name; public List<string> layers = new List<string>(); public List<Edge> edges = new List<Edge>(); public Dictionary<string, FeAnim> composites = new Dictionary<string, FeAnim>(); }
        readonly Dictionary<string, Page> pages = new Dictionary<string, Page>();
        readonly Dictionary<string, Page> popupPages = new Dictionary<string, Page>();
        readonly List<Edge> overlayEdges = new List<Edge>();
        readonly Dictionary<string, FeLayer> layers = new Dictionary<string, FeLayer>();
        RectTransform legendHolder;
        Page current;
        readonly List<Page> history = new List<Page>();
        readonly List<(Page page, List<FeLayer> layers)> popups = new List<(Page, List<FeLayer>)>();
        bool busy; float busyFor;
        string pendingHeader;

        public string CurrentPage => current?.name;
        public string TopPopup => popups.Count > 0 ? popups[popups.Count - 1].page.name : null;
        /// A page transition, or the Save Popup the game pushes while it saves (input goes to that popup then).
        public bool Busy => busy || savePopupUp;

        // -------------------------------------------------------------------------------------------- setup
        void Start() => Boot();
        void Update() => Step(Time.unscaledDeltaTime);

        /// Tests / bench: replaces the hand-over to the race scene (the event and the AI choice LocationSelect made).
        public static Action<ReflexEvents.Event, int> LaunchOverride;

        /// Builds the canvas and enters the first page (Start).
        public void Boot()
        {
            Reflex.Port.Options.ReflexClientLive.ApplyFrameRate();
            if (!EventSystem.current) new GameObject("EventSystem", typeof(EventSystem), typeof(InputSystemUIInputModule));
            var cgo = new GameObject("Original front end", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
            cgo.transform.SetParent(transform, false);
            canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 10;
            canvas.additionalShaderChannels |= AdditionalCanvasShaderChannels.TexCoord1;
            var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
            var bg = new GameObject("Black", typeof(RectTransform), typeof(Image)); bg.transform.SetParent(cgo.transform, false);
            var brt = (RectTransform)bg.transform; brt.anchorMin = Vector2.zero; brt.anchorMax = Vector2.one; brt.offsetMin = brt.offsetMax = Vector2.zero; bg.GetComponent<Image>().color = Color.black; bg.GetComponent<Image>().raycastTarget = false;
            var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
            root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
            try
            {
                assets = new FeAssets(uiShader ? uiShader : Shader.Find("Reflex/FrontEnd UI")) { tables = FeTables.Shared };
                LoadGraphs();
                var lg = new GameObject("ButtonLegend.lyr", typeof(RectTransform)); lg.transform.SetParent(root, false);
                legendHolder = (RectTransform)lg.transform; legendHolder.anchorMin = legendHolder.anchorMax = legendHolder.pivot = new Vector2(0, 1); legendHolder.sizeDelta = new Vector2(1280, 720);
                legend = new OriginalButtonLegend(legendHolder);
                foreach (var (b, key) in new[] { ("accept", "Accept"), ("back", "Back"), ("toggle", "Toggle"), ("misc", "Misc"), ("start", "Start"), ("dev", "Select") })
                    legend.SetAction(b, () => { if (!busy) DispatchKey(key); });
                lg.SetActive(false);
                Overlay();
                sounds = new FeSounds(this) { volume = PlayerPrefs.GetFloat("ReflexAudio.master", 1f) };   // EffectsVolume
            }
            catch (Exception e)
            {
                Status = "The original front end could not be read from the MX vs ATV Reflex install: " + e.Message;
                Debug.LogError(Status + "\n" + e);
                var t = new GameObject("Error", typeof(RectTransform), typeof(Text)); t.transform.SetParent(root, false);
                var tx = t.GetComponent<Text>(); tx.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf"); tx.text = Status; tx.color = Color.white; tx.fontSize = 22; tx.alignment = TextAnchor.MiddleCenter;
                ((RectTransform)t.transform).sizeDelta = new Vector2(1100, 400);
                enabled = false; return;
            }
            string first = ReturnPage != null && pages.ContainsKey(ReturnPage) ? ReturnPage : startPage;
            ReturnPage = null;
            // MOTOCAREER: CareerFrontEndLoadingPage:DoneLoading -> GotoVehicleReward when the series rewarded a vehicle (ReflexCareer)
            if (first == "Race and Ride Events" && ReflexCareer.RewardVehicleAsset >= 0 && pages.ContainsKey("Vehicle Reward Page")) first = "Vehicle Reward Page";
            if (first != startPage) { var mm = pages.GetValueOrDefault("Main Menu Page"); if (mm != null) history.Add(mm); }
            ReflexCareer.Current = null;   // a career race ended (or was quit) when the front end comes back
            if (first == "Race and Ride Events" || first == "Vehicle Reward Page") ReflexProfile.Open();   // back from a career race: the game reads the save the race wrote
            bool intro = playIntro && (!Application.isBatchMode || IntroInBatchMode) && first == startPage && pages.ContainsKey(FeVideoFlow.IntroFirstPage);   // InitialIntroScreen (RELEASE): GotoLegalScreenPage
            Enter(pages[intro ? FeVideoFlow.IntroFirstPage : first], null);
            if (first != startPage) { EnableUiMusic(true); SetUiMusicTier(1); }   // FrontEndLoadingPage / CareerFrontEndLoadingPage:Start
        }

        static Edge ParseEdge(Dictionary<string, object> d)
        {
            string type = OriginalUi.S(d, "type") ?? "";
            return new Edge
            {
                evt = d.GetValueOrDefault("Event")?.ToString(), dest = d.GetValueOrDefault("DestinationPage")?.ToString(), graph = d.GetValueOrDefault("DestinationPageGraph")?.ToString(),
                srcAnim = d.GetValueOrDefault("SourceCompositeAnimation")?.ToString(), dstAnim = d.GetValueOrDefault("DestinationCompositeAnimation")?.ToString(),
                allowBack = !(d.GetValueOrDefault("AllowBack") is bool ab && !ab) && !(d.GetValueOrDefault("AllowBack") is string abs && abs.Equals("false", StringComparison.OrdinalIgnoreCase)),
                clearBack = d.GetValueOrDefault("ClearBack") is bool cb && cb,
                push = type.Contains("PageStackPush"), pop = type.Contains("PageStackPop"),
            };
        }

        void ReadGraph(ReflexUiData.LayerData g, Dictionary<string, Page> into, List<Edge> overlay = null)
        {
            if (g == null) return;
            if (overlay != null)   // the page stack's own push / pop edges (PopupPageStack.pag root)
            {
                foreach (var e in OriginalUi.Arr(g.root, "edges") ?? new List<object>()) if (e is Dictionary<string, object> ed) overlay.Add(ParseEdge(ed));
                foreach (var e in OriginalUi.Arr(g.root, "triggers") ?? new List<object>()) if (e is Dictionary<string, object> ed && (OriginalUi.S(ed, "type") ?? "").Contains("Edge")) overlay.Add(ParseEdge(ed));
            }
            foreach (Dictionary<string, object> pn in OriginalUi.Arr(g.root, "children"))
            {
                var pr = (Dictionary<string, object>)pn["props"];
                var p = new Page { name = OriginalUi.S(pr, "Name") };
                if (p.name == null) continue;
                if (pr.GetValueOrDefault("Layer") is List<object> ls) foreach (var l in ls) if (l is string s) p.layers.Add(s);
                foreach (var e in OriginalUi.Arr(pn, "edges") ?? new List<object>()) if (e is Dictionary<string, object> ed) p.edges.Add(ParseEdge(ed));
                foreach (var e in OriginalUi.Arr(pn, "triggers") ?? new List<object>()) if (e is Dictionary<string, object> ed && (OriginalUi.S(ed, "type") ?? "").Contains("Edge")) p.edges.Add(ParseEdge(ed));
                foreach (var a in OriginalUi.Arr(pn, "animations") ?? new List<object>()) if (a is Dictionary<string, object> ad) { var fa = FeAnim.Parse(ad); p.composites[fa.name] = fa; }
                into[p.name] = p;
            }
        }

        void LoadGraphs()
        {
            ReadGraph(assets.Layer("FrontEndPageGraph.pag"), pages);
            ReadGraph(assets.Layer("TransitionPageGraph.pag"), pages);
            ReadGraph(assets.Layer("PopupPageStack.pag"), popupPages, overlayEdges);
            ReadGraph(assets.Layer(FeVideoFlow.IntroGraph), pages);   // the startup sequence (FrontEndIntro.<LANG>)
            if (!pages.ContainsKey(startPage)) throw new InvalidOperationException("FrontEndPageGraph.pag has no page " + startPage);
        }

        /// A layer by asset name (built once, kept for the session as the UI manager keeps loaded layers).
        public FeLayer Layer(string asset)
        {
            if (layers.TryGetValue(asset, out var l)) return l;
            l = new FeLayer(assets, asset, root);
            l.script = FeScripts.Create(OriginalUi.S(l.root.props, "Script") ?? asset);
            if (l.script != null) { l.script.layer = l; l.script.fe = this; l.script.Initialize(); }
            return layers[asset] = l;
        }

        // -------------------------------------------------------------------------------------------- App.* for the scripts
        public void PlaySound(string uiName) => sounds?.Play(uiName);
        /// App.PlayUILocalizedSoundEvent( database, package, event, owner ) / App.StopUILocalizedSoundEvent( owner ) (FrontEndSounds.cs).
        public void PlayLocalizedSound(string database, string package, string eventPath, string owner) => sounds?.PlayLocalized(database, package, eventPath, owner);
        public void StopLocalizedSound(string owner) => sounds?.StopLocalized(owner);
        public void SetButtonLegend(string button, bool visible, string key) => legend?.Set(button, visible, key);
        public void SetHeader(string key)
        {
            pendingHeader = key;
            foreach (var l in layers.Values) if (l.active && l.script is HeaderLayerScript h) h.SetHeader(key);
        }
        public string HeaderKey => pendingHeader;
        public T Var<T>(string k, T def = default) => vars.TryGetValue(k, out var v) && v is T t ? t : def;
        /// App.SetEnableUIMusic: the UI music themes (UI_MUS, FeUiMusic in FrontEndSounds.cs): a theme drawn at random when it starts.
        public void EnableUiMusic(bool on) => sounds?.music?.SetEnable(on);
        /// App.SetUIMusicTier( 1..5 ).
        public void SetUiMusicTier(int tier) => sounds?.music?.SetTier(tier);
        /// MusicVolume (the OPTIONS page / the race's ReflexAudio.music): FeUiMusic reads it every frame.
        public void ApplyMusicVolume() { }
        /// App.MuteUIMusic (Attract, CareerCompletion) and a MuteAudio video while it plays (0xa95820): the music channel ramps to 0 (0.25 s).
        public void MuteUiMusic(bool mute) => sounds?.music?.Mute(mute);
        public void QuitGame()
        {
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }

        /// App.SendUIEvent(event): a popup push / pop of the overlay graph, "TravelBack", or an edge of the current page.
        public void SendUIEvent(string evt)
        {
            if (evt == "PopupPageStack_Pop") { PopPopup(); return; }
            if (FeVideoFlow.OnUIEvent(this, ActiveLayers(), evt)) return;   // the layers' UIEventTriggers (video DoneEvents, FrontEndVideo.cs)
            var oe = overlayEdges.FirstOrDefault(e => e.evt == evt);
            if (oe != null)
            {
                if (oe.push && popupPages.TryGetValue(oe.dest, out var pp)) PushPopup(pp, oe.dstAnim);
                else if (oe.pop) PopPopup();
                return;
            }
            if (current == null) return;
            if (evt == "TravelBack")
            {
                if (history.Count == 0) return;
                var back = history[history.Count - 1]; history.RemoveAt(history.Count - 1);
                Go(back, current.composites.ContainsKey("PageOut") ? "PageOut" : null, back.composites.ContainsKey("PageIn") ? "PageIn" : null, false);
                return;
            }
            var edge = current.edges.FirstOrDefault(e => e.evt == evt);
            if (edge == null) { Debug.Log("[FrontEnd] no edge for '" + evt + "' on page " + current.name); return; }
            string dest = edge.dest;
            if (!pages.TryGetValue(dest, out var target)) { Debug.Log("[FrontEnd] page '" + dest + "' not in the graphs"); return; }
            if (edge.clearBack) history.Clear();
            if (edge.allowBack && current != null) history.Add(current);
            Go(target, edge.srcAnim, edge.dstAnim, true);
        }

        void Go(Page target, string srcAnim, string dstAnim, bool forward)
        {
            busy = true;
            float wait = 0;
            if (srcAnim != null && current != null && current.composites.TryGetValue(srcAnim, out var a)) { PlayComposite(a, ActiveLayers()); wait = a.length; }
            pendingSwitch = (target, dstAnim); busyFor = wait;
        }
        (Page page, string anim)? pendingSwitch;

        List<FeLayer> ActiveLayers() => layers.Values.Where(l => l.active).ToList();

        void PlayComposite(FeAnim a, List<FeLayer> among)
        {
            foreach (var tr in a.tracks)
            {
                var l = among.FirstOrDefault(x => x.Name == tr.target);
                if (l == null) continue;
                foreach (var r in tr.refs) l.Play(r);
            }
        }

        // OverlayPageGraph.pag's OverlayPage (always up, above the pages and popups): Fade.lyr (UIFadeLayer: FeFade) and PageWipe.lyr
        // (PageWipe.lua: PageWipeScript). PS3InviteOverlay (console), FPSOverlay / DebugProgressComplete (dev) and CursorOverlay (the
        // mouse cursor; the port's front end has no mouse) are not built. The front end's Lua sends neither FadeIn / FadeOut nor
        // ShowPageWipe (the in-race pages do); ShowPageWipe / Fade are here for callers that do.
        readonly HashSet<FeLayer> overlay = new HashSet<FeLayer>();
        public FeFade Fade { get; private set; }
        void Overlay()
        {
            foreach (string name in new[] { "Fade.lyr", "PageWipe.lyr" })
            {
                FeLayer l; try { l = Layer(name); } catch (Exception e) { Debug.LogWarning("[FrontEnd] overlay layer " + name + ": " + e.Message); continue; }
                overlay.Add(l); l.active = true; l.focused = false; l.holder.gameObject.SetActive(true);
                l.script?.Start();
                if (name == "Fade.lyr") Fade = new FeFade(l);
            }
        }
        void OverlayOnTop() { foreach (var l in overlay) l.holder.SetAsLastSibling(); }
        /// The UI event ShowPageWipe( name ): PageWipeTransition / PageWipeComplete come back through the callbacks.
        public void ShowPageWipe(string name, Action<string> transition = null, Action<string> complete = null)
        {
            if (layers.TryGetValue("PageWipe.lyr", out var l) && l.script is PageWipeScript w) w.Show(name, transition, complete);
            else { transition?.Invoke(name); complete?.Invoke(name); }
        }

        void Enter(Page target, string dstAnim)
        {
            var old = ActiveLayers().Where(l => !overlay.Contains(l)).ToList();
            foreach (var l in old) l.script?.End();
            var next = new List<FeLayer>();
            bool legendOn = false;
            foreach (string name in target.layers)
            {
                if (name == "ButtonLegend.lyr") { legendOn = true; legendHolder.SetAsLastSibling(); continue; }
                FeLayer l; try { l = Layer(name); } catch (Exception e) { Debug.LogWarning("[FrontEnd] layer " + name + ": " + e.Message); continue; }
                l.holder.SetAsLastSibling(); next.Add(l);
            }
            foreach (var l in old) if (!next.Contains(l)) { l.active = false; l.holder.gameObject.SetActive(false); }
            legend.Reset(); legendHolder.gameObject.SetActive(legendOn);
            current = target;
            foreach (var l in next) { l.active = true; l.focused = true; l.holder.gameObject.SetActive(true); }
            foreach (var p in popups) foreach (var l in p.layers) l.holder.SetAsLastSibling();
            OverlayOnTop();
            foreach (var l in next) l.script?.Start();
            if (pendingHeader != null) SetHeader(pendingHeader);
            if (dstAnim != null && target.composites.TryGetValue(dstAnim, out var a)) PlayComposite(a, next);
            foreach (var l in next) l.script?.Present();
            RefocusPage();
            foreach (var l in next) l.Tick(0f);   // their state colours now (a layer built this frame would show its raw art for a frame)
        }

        void PushPopup(Page p, string anim)
        {
            var ls = new List<FeLayer>();
            foreach (string name in p.layers)
            {
                FeLayer l; try { l = Layer(name); } catch (Exception e) { Debug.LogWarning("[FrontEnd] popup layer " + name + ": " + e.Message); continue; }
                l.holder.SetAsLastSibling(); l.active = true; l.focused = true; l.holder.gameObject.SetActive(true); ls.Add(l);
            }
            popups.Add((p, ls));
            OverlayOnTop();
            RefocusPage();
            foreach (var l in ls) l.script?.Start();
            if (anim != null && p.composites.TryGetValue(anim, out var a)) PlayComposite(a, ls);
            else foreach (var l in ls) if (l.Name != null && l.root.anims.ContainsKey("PopupShow")) l.Play("PopupShow");
            foreach (var l in ls) l.Tick(0f);   // their state colours now (the Save Popup's first frame was its raw white backing)
        }

        void PopPopup()
        {
            if (popups.Count == 0) return;
            var (p, ls) = popups[popups.Count - 1]; popups.RemoveAt(popups.Count - 1);
            foreach (var l in ls) l.focused = false;
            float wait = 0;
            if (p.composites.TryGetValue("PopupHide", out var a)) { PlayComposite(a, ls); wait = a.length; }
            void Close() { foreach (var l in ls) { l.script?.End(); l.active = false; l.holder.gameObject.SetActive(false); } RefocusPage(); }
            if (wait > 0) { closing.Add((wait, Close)); } else Close();
        }
        readonly List<(float t, Action a)> closing = new List<(float, Action)>();

        void RefocusPage()
        {
            bool top = popups.Count == 0;
            foreach (var l in layers.Values) if (l.active && !overlay.Contains(l) && !popups.Any(p => p.layers.Contains(l))) l.focused = top;
        }

        // -------------------------------------------------------------------------------------------- hand-over to the race
        /// LocationSelect:OnAccept: App.SetupEvent / StartEvent, then "GotoLoadingPage" (the InGame Loading Page) and the race scene.
        public void LaunchEvent(ReflexEvents.Event ev, int ai, int laps = 0, ReflexWorldFiles.Garage garage = null)
        {
            if (busy || ev == null) return;
            busy = true; busyFor = 0;
            float wait = 0;
            if (current != null && current.composites.TryGetValue("PageOut", out var a)) { PlayComposite(a, ActiveLayers()); wait = a.length; }
            ReturnPage = "Event Select Page"; launchRemember = true;
            launch = (ev, ai, wait); launchLaps = laps; LaunchedGarage = garage;
        }
        /// MOTOCAREER (StartRacePopup:End): the career event (its world: ReflexCareer.Current); back on the series' events page after.
        public void LaunchCareerEvent(ReflexEvents.Event ev)
        {
            if (ev == null) return;
            LaunchEvent(ev, 0);
            ReturnPage = "Race and Ride Events"; launchRemember = false;
        }
        /// MotoSkills (MainMenu:OnTutorialKeypress, WP-T): the tutorial's world (ReflexTutorials) behind TransitionPageGraph's "Tutorial Load
        /// Page"; back on the Main Menu Page after; the UI music is not stopped (TutorialLoadingPage.lua has no Start).
        public void LaunchTutorial(ReflexEvents.Event ev)
        {
            if (ev == null || busy) return;
            LaunchEvent(ev, 0);
            ReturnPage = "Main Menu Page"; launchRemember = false; launchPage = "Tutorial Load Page";
        }
        string launchPage;
        /// A page by name, as an edge without animations would enter it (the motocard setup's pages the port skips).
        public void JumpTo(string page) { if (pages.TryGetValue(page, out var p)) Go(p, null, null, true); }
        bool launchRemember = true;
        (ReflexEvents.Event ev, int ai, float wait)? launch;
        int launchLaps;
        /// The Vehicle Select choice of the last LaunchEvent (VehicleSelect:End's EquipVehicle / EquipRider + LocationSelect's class).
        public static ReflexWorldFiles.Garage LaunchedGarage;

        void DoLaunch(ReflexEvents.Event ev, int ai)
        {
            if (LaunchOverride != null) { LaunchOverride(ev, ai); busy = false; return; }
            string loading = launchPage ?? "InGame Loading Page"; launchPage = null;   // WP-T: the Tutorial Load Page
            var page = pages.GetValueOrDefault(loading);
            FrontEndLoadingOverlay.Show(assets, page?.layers ?? new List<string> { "Background01.lyr", "LoadingScreen.lyr" });
            if (loading == "InGame Loading Page") EnableUiMusic(false);   // InGameLoadingPage:Start: App.SetEnableUIMusic( FALSE )
            Debug.Log("[FrontEnd] launching " + ev.key + " AI " + ai + (LaunchedGarage != null ? ", vehicle select: " + LaunchedGarage : ""));
            ReflexEventSelect.Launch(ev.key, ai, rememberChoice && launchRemember, raceScene, launchLaps, LaunchedGarage);
        }

        // -------------------------------------------------------------------------------------------- frame
        static readonly string[] Keys = { "Accept", "Back", "Start", "Toggle", "Misc", "Select" };
        float repeatT; Vector2Int heldDir;

        /// One frame (Update): page switches, animations, scripts, the legend and (outside transitions) input.
        public void Step(float dt)
        {
            if (assets == null) return;
            for (int i = closing.Count - 1; i >= 0; i--) { var c = closing[i]; c.t -= dt; if (c.t <= 0) { closing.RemoveAt(i); c.a(); } else closing[i] = c; }
            if (launch.HasValue)
            {
                var l = launch.Value; l.wait -= dt; launch = l;
                if (l.wait <= 0) { launch = null; DoLaunch(l.ev, l.ai); }
            }
            else if (busy && pendingSwitch.HasValue)
            {
                busyFor -= dt;
                if (busyFor <= 0) { var (p, anim) = pendingSwitch.Value; pendingSwitch = null; Enter(p, anim); busy = false; }
            }
            foreach (var l in layers.Values.ToList()) if (l.active) { l.script?.Update(dt);   // (a copy: a script may open a page or popup)
                l.Tick(dt); l.UpdateMasks(canvas.transform); }
            legend?.Tick(dt);
            Fade?.Tick(dt);
            sounds?.Tick(dt);
            SavePopup();
            FeVideoFlow.Tick(this, ActiveLayers(), dt);   // Bink videos, the video layers' timed triggers (FrontEndVideo.cs)
            if (!busy && Application.isPlaying) PollInput(dt);
        }

        // The Save Popup (PopupPageStack "Save Popup", SavePopup.lyr, no script): the UI manager's "Game Save" listener (005a3da0) sends
        // ShowSavePopup when the saved-game worker starts writing, "Game Saved" / "Game Save Failed" HideSavePopup (005a3df0); the worker
        // keeps it up at least 1000 ms (009cf5b0, its Sleep). Here: while the profile session's "save" writes (ReflexProfile.SaveState).
        bool savePopupUp; double savePopupSince;
        void SavePopup()
        {
            var (saving, started, written) = ReflexProfile.SaveState;
            double now = ReflexProfile.Clock;
            if (!savePopupUp && saving && now - started > 0.05 && popupPages.ContainsKey("Save Popup"))
            {
                SendUIEvent("ShowSavePopup"); savePopupUp = TopPopup == "Save Popup"; savePopupSince = now;
            }
            else if (savePopupUp && !saving && (now - savePopupSince >= 1.0 || !written))
            {
                savePopupUp = false;
                if (TopPopup == "Save Popup") SendUIEvent("HideSavePopup");
            }
        }
        public bool SavePopupShown => savePopupUp;

        void PollInput(float dt)
        {
            var kb = Keyboard.current; var gp = Gamepad.current;
            bool Pressed(string k)
            {
                switch (k)
                {
                    case "Accept": return kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame) || gp != null && gp.buttonSouth.wasPressedThisFrame;
                    case "Back": return kb != null && kb.escapeKey.wasPressedThisFrame || gp != null && gp.buttonEast.wasPressedThisFrame;
                    case "Start": return kb != null && kb.f3Key.wasPressedThisFrame || gp != null && gp.startButton.wasPressedThisFrame;
                    case "Toggle": return kb != null && kb.f1Key.wasPressedThisFrame || gp != null && gp.buttonWest.wasPressedThisFrame;
                    case "Misc": return kb != null && kb.f2Key.wasPressedThisFrame || gp != null && gp.buttonNorth.wasPressedThisFrame;
                    case "Select": return kb != null && kb.tabKey.wasPressedThisFrame || gp != null && gp.selectButton.wasPressedThisFrame;
                }
                return false;
            }
            // direction: arrows, d-pad, left stick (MapKey_UI_Key_* / DPad_* / NavStick_*), with key repeat
            Vector2Int dir = Vector2Int.zero;
            if (kb != null) { if (kb.upArrowKey.isPressed) dir.y = -1; if (kb.downArrowKey.isPressed) dir.y = 1; if (kb.leftArrowKey.isPressed) dir.x = -1; if (kb.rightArrowKey.isPressed) dir.x = 1; }
            if (gp != null)
            {
                Vector2 s = gp.dpad.ReadValue() + gp.leftStick.ReadValue();
                if (s.y > 0.5f) dir.y = -1; else if (s.y < -0.5f) dir.y = 1;
                if (s.x < -0.5f) dir.x = -1; else if (s.x > 0.5f) dir.x = 1;
            }
            if (dir != heldDir) { heldDir = dir; repeatT = 0.45f; if (dir != Vector2Int.zero) Navigate(dir.x, dir.y); }
            else if (dir != Vector2Int.zero) { repeatT -= dt; if (repeatT <= 0) { repeatT = 0.12f; Navigate(dir.x, dir.y); } }
            foreach (var k in Keys) if (Pressed(k)) { DispatchKey(k); break; }
        }

        List<FeLayer> InputLayers()
        {
            if (popups.Count > 0) return popups[popups.Count - 1].layers;
            return layers.Values.Where(l => l.active && l.focused && !overlay.Contains(l) && !popups.Any(p => p.layers.Contains(l))).OrderByDescending(l => l.holder.GetSiblingIndex()).ToList();
        }

        /// A direction for the focused lists (bench / tests use it too).
        public void Navigate(int dx, int dy)
        {
            foreach (var l in InputLayers())
                if (l.Navigate(dx, dy, PlaySound)) { l.script?.OnNavigated(); return; }
            // no list takes it: the focused components' UIKeypressTriggers of the direction (VehicleTuningPopup's attribute rows: MapKey_UI_Left / Right)
            string dk = dx < 0 ? "Left" : dx > 0 ? "Right" : dy < 0 ? "Up" : "Down";
            foreach (var l in InputLayers().ToList()) if (l.FireKey("MapKey_UI_" + dk)) return;
        }

        /// A UI map key ("Accept", "Back", "Start", "Toggle", "Misc", "Select"): the focused components' keypress triggers.
        public void DispatchKey(string key)
        {
            if (busy) return;
            foreach (var l in InputLayers().ToList()) if (l.FireKey("MapKey_UI_" + key)) return;
        }
    }
}
