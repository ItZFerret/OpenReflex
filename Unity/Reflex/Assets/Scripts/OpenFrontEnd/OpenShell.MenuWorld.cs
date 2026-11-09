// The shell over the front end's 3D menu world (OpenMenuWorld): the pictures step aside once the world is ready, the hub frames the player's
// rider and vehicle in the part of the screen right of its menu (and they walk in the first time the hub is entered), the garage's pages look
// at them through their own stages (OpenShell.Garage.cs), every other page keeps the world as a dimmed backdrop. Without the world (loading,
// REFLEX_MENU3D=0, a level that cannot be read) everything is as before: the hub's location picture.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        OpenMenuWorld _world;
        VisualElement _worldDim;
        float _bgOpacity = 1f;
        bool _worldScreenDirty, _heroWalked;
        VisualElement _scrim, _bottomFade;

        /// The menu world when it is ready (null: loading, off or failed).
        public OpenMenuWorld MenuWorld => _world && _world.Status == OpenMenuWorld.State.Ready ? _world : null;

        // ---- the rider's portrait (OpenRiderPortrait): the MOTOCARD card and the garage's RIDER tab show it
        OpenRiderPortrait _portrait;
        internal OpenRiderPortrait Portrait => _portrait ??= new OpenRiderPortrait(transform);
        /// Draws the portrait when the equipped gear changed (nothing when it did not).
        void PortraitRefresh()
        {
            if (Environment.GetEnvironmentVariable("REFLEX_PORTRAIT") == "0") return;   // (diagnostics: the pages without the portrait)
            using var perf = Platform.ReflexMenuPerf.Measure("portrait");
            try { Portrait.Refresh(MenuWorld); }
            catch (Exception e) { Debug.LogWarning("[Portrait] " + e.Message); }
        }
        /// A framed portrait of the player's own rider in parent (the picture follows the gear), at the size the USS gives .ou-portrait.
        VisualElement PortraitFrame(VisualElement parent, string name)
        {
            var frame = OuUi.Div("ou-portrait", parent); frame.name = name;
            var pic = OuUi.Div("ou-portrait-pic", frame);
            Portrait.Bind(pic);
            OuUi.Div("ou-portrait-bar", frame);
            return frame;
        }

        /// The compound's status boards show an event (null: none): the one the hub continues, the one under the RACE browser's cursor.
        void BoardsShow(OpenEvent ev, OpenSetup setup = null)
        {
            try { MenuWorld?.Boards?.Show(ev, setup); } catch (Exception e) { Debug.LogWarning("[StatusBoards] " + e.Message); }
        }

        void WorldStart()
        {
            if (_world || _worldDim == null) return;
            _world = OpenMenuWorld.Ensure(transform);
            if (_world) _world.Changed += () => { if (this) _worldScreenDirty = true; };
        }

        void WorldStop()
        {
            _portrait?.Destroy(); _portrait = null;
            ReleaseStuntPreview();
            if (_world) Destroy(_world.gameObject);
            _world = null;
        }

        /// The world's part of the frame: the background picture fading out once it is ready, the screen's use of it when the screen changed.
        void WorldUpdate()
        {
            if (_worldDim == null) return;
            IntroUpdate();
            bool ready = MenuWorld != null;
            float target = ready || _intro != IntroPhase.None ? 0f : 1f;   // (the intro shows no picture: black, then the world)
            if (!Mathf.Approximately(_bgOpacity, target))
            {
                _bgOpacity = OuMotion.Reduced ? target : Mathf.MoveTowards(_bgOpacity, target, MenuTime.Delta / 0.6f);
                _bg.style.opacity = _bgOpacity;
                _bg.style.display = _bgOpacity <= 0.001f ? DisplayStyle.None : DisplayStyle.Flex;
            }
            if (_worldScreenDirty) WorldScreen();
        }

        /// The screen's use of the world: the hub's composition, the garage's (its own stages), a dimmed backdrop elsewhere.
        void WorldScreen()
        {
            var w = MenuWorld;
            if (w == null) { _worldDim.style.display = DisplayStyle.None; _worldScreenDirty = false; return; }
            bool hub = Current == Screen.Main || Current == Screen.Play, garage = Current == Screen.Garage;
            _worldDim.style.display = hub || garage || Current == Screen.Loading ? DisplayStyle.None : DisplayStyle.Flex;
            if (garage) { _worldScreenDirty = false; return; }
            if (Current == Screen.Setup && SetupPreviewWorld(w)) return;   // the setup's RIDER tab: the vehicle preview window
            _worldDim.style.opacity = 1f;
            if (!HubRegion(hub, out var center, out var height, out var screenAspect, out var aspect)) return;   // (not laid out yet: next frame)
            _worldScreenDirty = false;
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var (bike, rider, key) = EquippedHero();
            float tSetup = (float)sw.Elapsed.TotalMilliseconds;
            bool walkIn = hub && !_heroWalked && _intro == IntroPhase.None;   // (the intro starts its own walk)
            if (hub) _heroWalked = true;
            w.HubMode(center, height, screenAspect, aspect, !hub);
            // the event load's arena (OpenShell.LoadArena.cs): the rider and the vehicle the event is ridden with (read on a worker first)
            if (Current == Screen.Loading && _arena != null && EventHeroSpecs(_arena.ev.ev, _arena.setup, out var eb, out var er, out var ek)) w.ShowHeroAsync(eb, er, ek, false);
            else w.ShowHero(bike, rider, key, walkIn);
            PortraitRefresh();
            if (hub) BoardsShow(HubEvent());
            if (OpenMenuWorldProbe.Wanted) Debug.Log($"[MenuWorldProbe] screen {Current} at {Time.realtimeSinceStartup:F2} s: equipped setup {tSetup:F0} ms, all {sw.Elapsed.TotalMilliseconds:F0} ms");
        }

        /// The hub's view: right of the menu column, the cards' column overlapping its right edge; other pages: the screen's right part.
        bool HubRegion(bool hub, out Vector2 center, out float height, out float screenAspect, out float aspect)
        {
            center = default; height = 1f; screenAspect = 16f / 9f; aspect = 1f;
            var full = Root.worldBound;
            if (float.IsNaN(full.width) || full.width < 16 || full.height < 16) return false;
            screenAspect = full.width / full.height;
            float x0 = full.xMin + full.width * 0.40f, x1 = full.xMin + full.width * 0.80f, y0 = full.yMin + full.height * 0.10f, y1 = full.yMin + full.height * 0.86f;
            if (hub)
            {
                var left = _layer.Q<VisualElement>("hub-left"); var cards = _layer.Q<VisualElement>("hub-cards");
                if (left == null || float.IsNaN(left.worldBound.width) || left.worldBound.width < 1) return false;
                x0 = left.worldBound.xMax + full.width * 0.01f;
                x1 = cards != null && cards.worldBound.width > 1 ? cards.worldBound.xMin + cards.worldBound.width * HubCardsOverlap : full.xMax - full.width * 0.05f;
            }
            var r = Rect.MinMaxRect(x0, y0, Mathf.Max(x0 + 32, x1), y1);
            center = new Vector2((r.center.x - full.center.x) / (full.width * 0.5f), -(r.center.y - full.center.y) / (full.height * 0.5f));
            height = r.height / full.height;
            aspect = r.width / r.height;
            return true;
        }
        /// How far into the hub's card column the hero's view reaches (a fraction of its width).
        public static float HubCardsOverlap = 0.0f;

        /// The player's rider and vehicle as the garage has them equipped (a pro rider's setup when one is chosen); the default rider and bike
        /// without a profile.
        (BikeSpec bike, RiderSpec rider, string key) EquippedHero()
        {
            using var perf = Platform.ReflexMenuPerf.Measure("equipped hero lookup");
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !ReflexProfile.Disabled)
            {
                try
                {
                    var open = ReflexProfile.Open();
                    if (open.IsCompletedSuccessfully && open.Result && OpenGarageData.P != null)
                    {
                        if (OpenGarageData.Pro)
                        {
                            var cur = OpenGarageData.ProRides().FirstOrDefault(r => r.rider == OpenGarageData.ProId && r.vehicle == OpenGarageData.Equipped);
                            if (cur != null) { var (pb, pr) = OpenGarageData.ProSpecs(cur); return (pb, pr, "pro:" + cur.rider + ":" + cur.vehicle); }
                        }
                        int vehicle = OpenGarageData.Equipped;
                        var pv = OpenGarageData.Tab("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", vehicle);
                        int model = row < 0 ? -1 : pv.Int("VehicleModelId", row);
                        var gear = OpenGarageData.EquippedGearIds();
                        var forced = OpenMenuWorldProbe.HeroClass;   // (diagnostics: the hero of a vehicle class, for the per-class recordings)
                        if (forced != null)
                        {
                            var v = OpenGarageData.Vehicles().FirstOrDefault(x => string.Equals(OpenGarageData.TypeName(x.type), forced, StringComparison.OrdinalIgnoreCase));
                            if (v != null) return (OpenGarageData.BikeOfModel(v.id), OpenGarageData.RiderWithGear(gear), "class:" + forced + ":" + v.id + ":" + string.Join(",", gear));
                            Debug.Log("[MenuWorldProbe] no vehicle of the class " + forced + "; the classes: " + string.Join(", ", OpenGarageData.Vehicles().Select(x => OpenGarageData.TypeName(x.type)).Distinct()));
                        }
                        return (model >= 0 ? OpenGarageData.BikeOfModel(model) : null, OpenGarageData.RiderWithGear(gear),
                                "own:" + model + ":" + string.Join(",", gear) + ":" + string.Join(",", OpenGarageData.EquippedPartIds(vehicle)) + ":m" + BikeMods.Version);
                    }
                }
                catch (Exception e) { Debug.LogWarning("[MenuWorld] the equipped setup: " + e.Message); }
            }
            return (BikeSpec.MarsFLite, RiderSpec.AnswerRed(), "default");
        }

        // ------------------------------------------------------------------------------------------------ the intro (the first hub entry)
        // Hold: the OPEN REFLEX logo on black with a progress line until the compound, the rider and his vehicle are built and the frames come in
        // time (OpenMenuWorld.SceneSettled: the first frames compile their pipelines); Fade: from black into the scene, the walk starts with the
        // intro's close shot; Playing: no hub chrome at all; Reveal: the camera eases out to the hub's composition while the logo, the menu and the
        // cards fade and slide in, then PLAY takes the focus. A press skips to the end; REDUCE MOTION: no walk, the menu fades in; later hub visits
        // (from the garage, the options, an event) show the menu at once. The world failing: the picture and the menu at once.
        enum IntroPhase { None, Hold, Fade, Playing, Reveal }
        static bool s_introPlayed;
        /// Tests: no intro (the hub takes input at once).
        public static bool SkipIntro;
        IntroPhase _intro;
        VisualElement _introOverlay, _introBar;
        float _introAt, _focusAt;
        bool _introSkip;
        void OnApplicationFocus(bool focus) { if (focus) _focusAt = MenuTime.Now; }
        public static float IntroHoldTimeout = 20f, IntroFadeSeconds = 0.7f, IntroRevealSeconds = 0.95f;
        bool IntroBlocksInput => _intro != IntroPhase.None;
        /// Diagnostics / tests: the intro's phase.
        public string IntroState => _intro.ToString();

        void IntroBegin()
        {
            if (SkipIntro || s_introPlayed || _intro != IntroPhase.None || !_world || _world.Status == OpenMenuWorld.State.Failed) return;
            s_introPlayed = true; _heroWalked = true;
            _intro = IntroPhase.Hold; _introAt = MenuTime.Now;
            _introOverlay = OuUi.Div("ou-intro", Root);
            _introOverlay.pickingMode = PickingMode.Ignore;
            var o = _introOverlay.style;
            o.position = Position.Absolute; o.left = 0; o.top = 0; o.right = 0; o.bottom = 0; o.backgroundColor = Color.black;
            o.alignItems = Align.Center; o.justifyContent = Justify.Center;
            var logo = OuUi.Div("ou-intro-logo", _introOverlay); OuTheme.Logo(logo, 150);
            var track = OuUi.Div("ou-intro-track", _introOverlay);
            track.style.width = 240; track.style.height = 3; track.style.marginTop = 34; track.style.backgroundColor = new Color(1, 1, 1, 0.12f); track.style.overflow = Overflow.Hidden;
            _introBar = OuUi.Div("ou-intro-bar", track);
            _introBar.style.position = Position.Absolute; _introBar.style.top = 0; _introBar.style.bottom = 0; _introBar.style.width = 80; _introBar.style.backgroundColor = new Color(0.91f, 0.11f, 0.14f, 0.9f);
            IntroChrome(false);
            _bgOpacity = 0f; _bg.style.opacity = 0f; _bg.style.display = DisplayStyle.None;   // (no picture under the black)
        }

        /// The hub's chrome (the layer, the scrims, the bars) hidden for the intro, or shown (staggered: a fade and a lift).
        void IntroChrome(bool on, bool animate = false)
        {
            float a = on ? 1f : 0f;
            _layer.style.opacity = a; _scrim.style.opacity = a; _bottomFade.style.opacity = a;
            _hints.style.opacity = a; _status.style.opacity = a;
            StepReveal(99f); _reveal.Clear();   // (an entrance cut short ends in place)
            if (on && animate)
            {   // the entrance: each part fades and slides in after the one before (driven by the menu's clock, not by style transitions)
                float d = 0f;
                var left = _layer.Q<VisualElement>("hub-left");
                if (left != null) foreach (var c in left.Children()) { _reveal.Add((c, d, -28f, 0f)); d += 0.07f; }
                var cards = _layer.Q<VisualElement>("hub-cards");
                if (cards != null) { float dc = 0.12f; foreach (var c in cards.Children()) { _reveal.Add((c, dc, 0f, 22f)); dc += 0.08f; } }
                foreach (var e in new[] { _scrim, _bottomFade }) _reveal.Add((e, 0f, 0f, 0f));
                foreach (var e in new[] { _hints, _status }) _reveal.Add((e, 0.25f, 0f, 0f));
                StepReveal(0f);
            }
        }
        readonly List<(VisualElement e, float delay, float dx, float dy)> _reveal = new List<(VisualElement, float, float, float)>();
        public static float RevealPartSeconds = 0.38f;
        void StepReveal(float since)
        {
            foreach (var (e, delay, dx, dy) in _reveal)
            {
                if (e == null) continue;
                float u = Mathf.Clamp01((since - delay) / RevealPartSeconds); u = 1f - (1f - u) * (1f - u) * (1f - u);
                e.style.opacity = u;
                e.style.translate = new Translate(dx * (1f - u), dy * (1f - u));
            }
        }

        static bool AnyPress()
        {
            var kb = UnityEngine.InputSystem.Keyboard.current; if (kb != null && kb.anyKey.wasPressedThisFrame) return true;
            var ms = UnityEngine.InputSystem.Mouse.current; if (ms != null && (ms.leftButton.wasPressedThisFrame || ms.rightButton.wasPressedThisFrame)) return true;
            foreach (var gp in UnityEngine.InputSystem.Gamepad.all)
                foreach (var c in gp.allControls)
                    if (c is UnityEngine.InputSystem.Controls.ButtonControl b && !c.synthetic && b.wasPressedThisFrame) return true;
            return false;
        }

        void IntroUpdate()
        {
            if (_intro == IntroPhase.None) return;
            var w = _world;
            float since = MenuTime.Now - _introAt;
            if (_introBar != null) _introBar.style.left = Length.Percent(((MenuTime.Now * 0.55f) % 1.4f - 0.4f) * 100f);   // (a line running through)
            // (a press within half a second of the window getting the focus is the system's re-sync of the devices, not the player)
            bool press = Application.isFocused && MenuTime.Now - _focusAt > 0.5f && AnyPress();
            switch (_intro)
            {
                case IntroPhase.Hold:
                    if (!w || w.Status == OpenMenuWorld.State.Failed) { IntroEnd(false); return; }
                    if (press) _introSkip = true;   // (the scene is not there yet: it comes up in its end state, the menu with it)
                    if (!w.SceneSettled && since < IntroHoldTimeout) return;
                    if (_introSkip) { w.SnapCamera(); IntroEnd(false); return; }
                    _intro = IntroPhase.Fade; _introAt = MenuTime.Now;
                    w.Intro = !OuMotion.Reduced;
                    if (!OuMotion.Reduced) w.StartWalk();
                    w.SnapCamera();   // (the shot starts where it should, under the black)
                    Debug.Log($"[MenuWorld] intro: the scene settled after {since:F2} s; {(w.Walking ? "the walk-in" : "no walk")}");
                    return;
                case IntroPhase.Fade:
                    if (press) { IntroSkip(); return; }
                    float k = Mathf.Clamp01(since / IntroFadeSeconds);
                    if (_introOverlay != null) _introOverlay.style.opacity = 1f - k * k * (3 - 2 * k);
                    if (k >= 1f) { _introOverlay?.RemoveFromHierarchy(); _introOverlay = null; _introBar = null; _intro = IntroPhase.Playing; _introAt = MenuTime.Now; }
                    return;
                case IntroPhase.Playing:
                    if (press) { IntroSkip(); return; }
                    if (w && w.Walking) return;
                    if (w) w.Intro = false;   // the camera eases out to the hub's composition
                    IntroChrome(true, true);
                    _intro = IntroPhase.Reveal; _introAt = MenuTime.Now;
                    return;
                case IntroPhase.Reveal:
                    if (press) { StepReveal(99f); IntroEnd(true); return; }
                    StepReveal(since);
                    if (since < IntroRevealSeconds) return;
                    StepReveal(99f); _reveal.Clear();
                    IntroEnd(true);
                    return;
            }
        }

        /// Diagnostics: a press during the intro.
        internal void ProbeSkip() { if (_intro == IntroPhase.Hold) _introSkip = true; else if (_intro != IntroPhase.None) IntroSkip(); }

        /// A press: the end state at once (the vehicle in place, the menu shown).
        void IntroSkip()
        {
            if (_world) { _world.FinishWalk(); _world.Intro = false; _world.SnapCamera(); }
            IntroEnd(false);
        }

        void IntroEnd(bool revealed)
        {
            _introOverlay?.RemoveFromHierarchy(); _introOverlay = null; _introBar = null;
            if (!revealed) IntroChrome(true);
            _intro = IntroPhase.None;
            if (Current == Screen.Main) _nav.Focus(_layer.Q<VisualElement>("ou-hub-play"));
        }

        /// Another screen came during the intro: its chrome back, the walk goes on (OpenMenuWorld keeps playing it under any page).
        void IntroAbort()
        {
            if (_intro == IntroPhase.None) return;
            if (_world) _world.Intro = false;
            _introOverlay?.RemoveFromHierarchy(); _introOverlay = null; _introBar = null;
            IntroChrome(true);
            _intro = IntroPhase.None;
        }

        /// The hub's cards a little more compact with the 3D menu (the rider and his vehicle stand in the space between the menu and them); the
        /// cards' style is the hub's own.
        void HubCards3D(VisualElement cards)
        {
            if (cards == null || !OpenMenuWorld.Wanted || _world == null || _world.Status == OpenMenuWorld.State.Failed) return;
            cards.style.width = 440;
            foreach (var shot in cards.Query<VisualElement>(className: "ou-continue-shot").ToList()) shot.style.height = 150;
            foreach (var panel in cards.Query<VisualElement>(className: "ou-card-panel").ToList()) panel.style.marginTop = 10;
        }

        /// Diagnostics (OpenMenuWorldProbe's "pick" step): the world walks in another bike of the garage's list, as a pick does (the profile is not
        /// changed).
        internal void ProbePick(string kind = "bike")
        {
            var w = MenuWorld; if (w == null) return;
            var (_, rider, key) = EquippedHero();
            var other = OpenGarageData.Vehicles().Where(v => v.id != OpenGarageData.EquippedModel && v.id != _probeLast).Select(v => v.id).FirstOrDefault(m =>
            {
                var b = OpenGarageData.BikeOfModel(m); if (b == null) return false;
                string k = b.atv ? "atv" : b.machine ? "machine" : "bike";
                return k == kind && (kind != "machine" || _probeMachine == null || ((b.chassisDb ?? "").IndexOf(_probeMachine, StringComparison.OrdinalIgnoreCase) >= 0));
            });
            _probeLast = other;
            if (other <= 0) return;
            w.EndPreview();
            w.ShowHeroAsync(OpenGarageData.BikeOfModel(other), rider, "probe-pick:" + other + ":" + key, true);
        }

        int _probeLast; internal string _probeMachine;
        /// The walk of a vehicle just picked in the garage (the new equipped setup, read on a worker thread first).
        void WorldPicked()
        {
            var w = MenuWorld; if (w == null) return;
            w.EndPreview(false);
            var (bike, rider, key) = EquippedHero();
            w.ShowHeroAsync(bike, rider, key, true);
        }
    }
}
