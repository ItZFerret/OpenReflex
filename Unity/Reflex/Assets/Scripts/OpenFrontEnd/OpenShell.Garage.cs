// The OPEN REFLEX garage: a drill-down after the original Reflex garage's own flow (GarageHUB.lua: VEHICLE / RIDER; GarageRiderCustomize.lua:
// the gear type, then its store list of product lines, then the line's items; GarageMyRides.lua: the vehicle types, then their models;
// GarageVehicleCustomize.lua: the part types, then lines, then items), one level at a time on the left over the 3D stage on the right.
//   GARAGE          VEHICLE · RIDER · PRO RIDERS, then the MOTOCARD and UNLOCK EVERYTHING
//   VEHICLE         MY RIDES (the vehicle classes, then their models: equip / buy, bike mods), ENGINE SOUND, VEHICLE TUNING, then one entry per
//                   customizable part type of the equipped vehicle (CUSTOMIZE VEHICLE) > product line ("FMF FACTORY 4.1") > item ("TITANIUM")
//   RIDER           NAME & NUMBER (name, jersey name, number 1..999), then one entry per gear type (GEAR, HELMET, BOOTS ...) > product
//                   line ("FOX V3") > item ("SX BLACK")
//   PRO RIDERS      who rides: your rider or a pro rider on a pro's vehicle
// A breadcrumb and the level's title say where you are (GARAGE › RIDER › HELMET › FOX V3). A / Enter / click goes in, B / Esc / the header's
// BACK goes up exactly one level and the focus comes back to the entry it was on; LB / RB (Q / E) switch between VEHICLE and RIDER from
// anywhere. What is focused in a line (or a single item) is previewed on the stage at once, tagged PREVIEW; going back without confirming
// shows the equipped setup again; A equips it (or buys it, behind the purchase dialog). The equipped choice is marked at every level.
// Every change goes through ReflexProfile / CoreGarageProfile (the game's own tables, saved at once). The stage (OpenGarageStage): drag /
// right stick turns it, wheel / triggers zoom; T / X is the TEST RIDE (OpenShell.TestRide.cs).
// Test hooks: garage-h-vehicle / garage-h-rider / garage-h-pro (home), garage-up (the header's BACK), garage-tab-0 / garage-tab-2 (the RIDER /
// VEHICLE switch, class ou-gtab-on), garage-gear-<type>, garage-part-<type>, garage-line-<product>, garage-item-<id>, garage-class-<type> /
// garage-class-all / garage-class-mods, garage-vehicle-<PlayerVehicleId> (EQUIPPED pill), garage-model-<id>, garage-mod-<model>-<n>,
// garage-own-rider, garage-pro-<rider>-<vehicle>, garage-tuning-<n>, garage-unlock-everything, garage-name / garage-jersey-name /
// garage-number, garage-motocard, garage-back; GarageCurrentTab / GaragePath / GarageTitle; MoveGarageTuning.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    /// Places other parts of the product hang their garage rows on (the engine sound picker of the audio work).
    public static class OpenGarageHooks
    {
        /// ENGINE SOUND slot of the VEHICLE level: (container to add the row to, the PlayerVehicleId, its VehicleModelId), for the equipped own
        /// vehicle. The default hosts w-audio's EngineSoundPicker.Row (per bike, by vehicle model id, an OuUi.Spinner: left / right change it);
        /// null draws nothing.
        public static Action<VisualElement, int, int> EngineSound = (container, playerVehicle, model) =>
        {
            var row = FeGarageData.Unique("VehicleModel", "VehicleModelId", model);
            string title = OpenGarageData.Title(FeGarageData.ProductOfAsset(FeTables.I(row, "StoreAssetId")), "BIKE").ToUpperInvariant();
            EngineSoundPicker.Row(container, model, "ENGINE SOUND  ·  " + title, "garage-engine-sound");
        };
    }

    public sealed partial class OpenShell
    {
        /// Where ShowGarage(int) opens the garage (the old tab numbers keep their meaning: the level a player finds that tab's rows on).
        public const int GarageRider = 0, GarageProRiders = 1, GarageVehicle = 2, GarageGear = 3, GarageParts = 4, GarageTuning = 5, GarageHome = 6, GarageIdentity = 7,
                         GarageRiderPresets = 8, GarageBikePresets = 9;
        public const int GarageTabCount = 10;

        /// The levels of the drill-down.
        enum GKind { Home, Rider, Identity, GearSection, GearLine, ProRiders, Vehicle, Classes, Models, PartSection, PartLine, Tuning, RiderPresets, BikePresets, Preset }

        /// One level on the way down: what it shows (a, b: its gear / part type, product line, vehicle class) and the entry last focused on it.
        sealed class GLevel
        {
            public GKind kind; public int a = -1, b = -1; public string focus;
            public GLevel(GKind kind, int a = -1, int b = -1, string focus = null) { this.kind = kind; this.a = a; this.b = b; this.focus = focus; }
            public override string ToString() => (int)kind + "," + a + "," + b + "," + (focus ?? "");
            public static GLevel Parse(string s)
            {
                var p = (s ?? "").Split(',');
                if (p.Length < 4 || !int.TryParse(p[0], out int k) || !Enum.IsDefined(typeof(GKind), k)) return null;
                int.TryParse(p[1], out int a); int.TryParse(p[2], out int b);
                return new GLevel((GKind)k, a, b, p[3].Length > 0 ? p[3] : null);
            }
        }

        readonly List<GLevel> garageStack = new List<GLevel>();
        GLevel GTop => garageStack.Count > 0 ? garageStack[garageStack.Count - 1] : null;
        string garageHomeFocus;                               // the home level's entry, kept between visits
        readonly Dictionary<GKind, string> garageBranchFocus = new Dictionary<GKind, string>();   // the VEHICLE / RIDER levels' entries (LB / RB come back there)

        FeGarage garagePreview;
        OpenGarageStage garageStage;
        ScrollView garageScroll;
        VisualElement garageRows, garageView, garageIdBlock, garageStats, garageDialog, garageBadges, garageHead;
        Label garageStatus, garageName, garageNumber, garageSub, garageMake, garageModelName, garageCredits, garageHint, garageCrumb, garageTitle, garageLevelSub;
        bool garageBuilt;
        OuNav.Place garageDialogReturn;
        VisualElement garageTyping;     // the row whose text box has the keyboard (the NUMBER row), else null
        Action garageTypeCancel;        // drops what was typed there
        int garageTypeEnded = -10;      // the frame typing ended (the Enter / Esc that ended it must not also press the row / go back)
        int garageClickFrame = -10;     // the frame a text row was clicked (a mouse click types into the box, only the pad's A opens the on-screen keyboard)

        // what the stage shows: built one moment after the focus settles, so a pad scrolling a list does not build a model per row
        string garagePendingKey, garageShownKey;
        Func<(BikeSpec bike, RiderSpec rider, bool beside)> garagePendingMake;
        OpenGarageStage.Shot garagePendingShot; bool garagePendingReset; float garagePendingAt;
        string garagePadShown;
        /// The menu world's garage framing: the fitted distance times this.
        public static float GarageFit = 0.9f;
        // the vehicle the card and the bars describe
        int garageSubjectModel = -1, garageSubjectVehicle = -1; string garageSubjectMod;

        /// The old tab the current level belongs to (GarageRider ... GarageHome).
        public int GarageCurrentTab
        {
            get
            {
                switch (GTop?.kind)
                {
                    case GKind.Rider: case GKind.Identity: return GarageRider;
                    case GKind.GearSection: case GKind.GearLine: return GarageGear;
                    case GKind.ProRiders: return GarageProRiders;
                    case GKind.Classes: case GKind.Models: return GarageVehicle;
                    case GKind.Vehicle: case GKind.PartSection: case GKind.PartLine: return GarageParts;
                    case GKind.Tuning: return GarageTuning;
                    case GKind.RiderPresets: return GarageRiderPresets;
                    case GKind.BikePresets: return GarageBikePresets;
                    case GKind.Preset: return GTop.a == 1 ? GarageBikePresets : GarageRiderPresets;
                    default: return GarageHome;
                }
            }
        }
        public bool GarageHasDialog => garageDialog != null;
        /// Tests: the way down as the breadcrumb shows it ("GARAGE › RIDER › HELMET"), and the current level's title.
        public string GarageCrumb => garageCrumb?.text;
        public string GarageTitle => garageTitle?.text;
        public int GarageDepth => garageStack.Count;
        /// The levels from the top, as the test ride keeps them across the ride.
        public string GaragePath => string.Join("/", garageStack.Select(l => l.ToString()));

        // ------------------------------------------------------------------------------------------------ the screen

        /// The garage's home (the entry last focused there).
        public void ShowGarage() => ShowGarage(GarageHome);

        /// The garage on the level a player finds that part on (GarageRider: the RIDER level; GarageVehicle: the vehicles of every class; ...).
        public void ShowGarage(int tab)
        {
            garageStack.Clear();
            garageStack.Add(new GLevel(GKind.Home, focus: garageHomeFocus));
            void Push(GKind k, string homeFocus, int a = -1, int b = -1, string focus = null)
            {
                if (garageStack.Count == 1 && homeFocus != null) garageStack[0].focus = homeFocus;
                garageStack.Add(new GLevel(k, a, b, focus));
            }
            switch (tab)
            {
                case GarageRider: Push(GKind.Rider, "garage-h-rider", focus: garageBranchFocus.TryGetValue(GKind.Rider, out var rf) ? rf : null); break;
                case GarageGear: Push(GKind.Rider, "garage-h-rider", focus: FirstGearSectionName()); break;
                case GarageIdentity: Push(GKind.Rider, "garage-h-rider", focus: "garage-identity"); Push(GKind.Identity, null); break;
                case GarageProRiders: Push(GKind.ProRiders, "garage-h-pro"); break;
                case GarageVehicle: Push(GKind.Vehicle, "garage-h-vehicle", focus: "garage-my-rides"); Push(GKind.Classes, null, focus: "garage-class-all"); Push(GKind.Models, null, -1); break;
                case GarageParts: Push(GKind.Vehicle, "garage-h-vehicle", focus: FirstPartSectionName()); break;
                case GarageTuning: Push(GKind.Vehicle, "garage-h-vehicle", focus: "garage-tuning"); Push(GKind.Tuning, null); break;
                case GarageRiderPresets: Push(GKind.Rider, "garage-h-rider", focus: "garage-rider-presets"); Push(GKind.RiderPresets, null); break;
                case GarageBikePresets: Push(GKind.Vehicle, "garage-h-vehicle", focus: "garage-bike-presets"); Push(GKind.BikePresets, null); break;
            }
            OpenGarage();
        }

        /// The garage on a path GaragePath gave (the test ride's way back); a path that no longer reads opens the home.
        public void ShowGaragePath(string path)
        {
            var levels = (path ?? "").Split('/').Select(GLevel.Parse).ToList();
            if (levels.Count == 0 || levels.Any(l => l == null) || levels[0].kind != GKind.Home) { ShowGarage(GarageHome); return; }
            garageStack.Clear(); garageStack.AddRange(levels);
            OpenGarage();
        }

        /// The garage as it was left (the MOTOCARD's B comes back here).
        void ResumeGarage()
        {
            if (garageStack.Count == 0) { ShowGarage(GarageHome); return; }
            OpenGarage();
        }

        string FirstGearSectionName() { var t = OpenGarageData.P != null ? OpenGarageData.GearTypes() : null; return t != null && t.Count > 0 ? "garage-gear-" + t[0].id : null; }
        string FirstPartSectionName() { var t = OpenGarageData.P != null && !OpenGarageData.Pro ? OpenGarageData.PartTypes(OpenGarageData.Equipped) : null; return t != null && t.Count > 0 ? "garage-part-" + t[0].id : null; }

        void OpenGarage()
        {
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) { OpenOriginalFor(OriginalEmbed.Purpose.Garage); return; }
            if (!garageBuilt) { BikeMods.Refresh(); KitPacks.Refresh(); }   // (bike mods / gear and kit packs dropped into Mods since the last visit)
            var open = ReflexProfile.Open();
            Clear(Screen.Garage);
            SetBackground(null);
            var uss = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuGarage");
            if (uss && !Root.styleSheets.Contains(uss)) Root.styleSheets.Add(uss);
            OuTheme.Header(_layer, OpenContent.Str("STR_GARAGE", "GARAGE"), "YOUR RIDER · YOUR RIDE");
            if (!open.IsCompletedSuccessfully || !open.Result || OpenGarageData.P == null)
            {
                OuUi.Text(ReflexProfile.Error ?? "Your garage could not be opened.", "mxa-note", _layer);
                var back = OuUi.Button("BACK", "mxa-main-button", ShowMain, _layer);
                _nav.DefaultItem = () => back; _nav.OnCancel = () => { ShowMain(); return true; }; _nav.Focus(back);
                Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "BACK"));
                return;
            }
            if (garageStack.Count == 0) garageStack.Add(new GLevel(GKind.Home));
            garageSubjectModel = -1;
            BuildGarageFrame();
            garageBuilt = true;
            ScreenUpdate = GarageUpdate;
            _nav.OnCancel = GarageBack;
            _nav.OnTab = GarageSwitchBranch;
            _nav.OnPage = _ => { };          // the triggers zoom the stage here, they do not page the list
            _nav.DefaultItem = GarageDefaultItem;
            RebuildGarageLevel(GTop.focus, true);
        }

        void BuildGarageFrame()
        {
            var garage = OuTheme.Clone("Garage", _layer) ?? OuUi.Div("ou-garage", _layer);
            var panel = OuTheme.Part(garage, "garage-panel", "ou-panel ou-garage-panel");

            // the head: the RIDER / VEHICLE switch, the way down, the level's title (the original's orange title bar) and what to do on it
            garageHead = OuTheme.Part(panel, "garage-head", "ou-ghead"); garageHead.Clear();
            var tabs = OuUi.Div("ou-gtabs ou-gbranch", garageHead);
            var tabsKey = OuUi.Div("ou-gbranch-key", tabs); tabsKey.name = "garage-tabs-key"; tabsKey.pickingMode = PickingMode.Ignore;
            OuUi.Hints(tabsKey, (OuUi.KeyTabs, ""));   // (the cap follows the device in use: LB / RB, L1 / R1, Q / E)
            foreach (var (label, name, kind) in new[] { ("VEHICLE", "garage-tab-2", GKind.Vehicle), (OpenContent.Str("STR_GARAGE_RIDER", "RIDER").ToUpperInvariant(), "garage-tab-0", GKind.Rider) })
            {
                var k = kind;
                var t = OuUi.Div("mxa-button ou-gtab", tabs); t.name = name;
                OuUi.Text(label, "mxa-button-label", t); OuUi.Div("ou-gtab-bar", t);
                t.pickingMode = PickingMode.Position;   // (a mouse target; the pad switches with the shoulders, so the tab is not in the focus order)
                t.RegisterCallback<ClickEvent>(e => { if (e.button != 0) return; OuSound.Play("LeftRight"); GarageGoBranch(k); });
            }
            var crumbRow = OuUi.Div("ou-gcrumb-row", garageHead);
            var up = OuUi.Div("ou-gup", crumbRow); up.name = "garage-up";
            OuUi.Text("‹", "ou-gup-arrow", up); OuUi.Text("BACK", "ou-gup-label", up);
            up.pickingMode = PickingMode.Position;
            up.RegisterCallback<ClickEvent>(e => { if (e.button != 0) return; OuSound.Play("Back"); GarageBack(); });
            garageCrumb = OuUi.Text("", "ou-gcrumb", crumbRow); garageCrumb.name = "garage-crumb"; garageCrumb.enableRichText = false;
            var bar = OuUi.Div("ou-glevel-bar", garageHead);
            garageTitle = OuUi.Text("", "ou-glevel-title", bar); garageTitle.name = "garage-title"; garageTitle.enableRichText = false;
            garageLevelSub = OuUi.Text("", "ou-glevel-sub", garageHead); garageLevelSub.name = "garage-level-sub"; garageLevelSub.enableRichText = false;

            garageScroll = panel.Q<ScrollView>("garage-rows") ?? new ScrollView { name = "garage-rows" };
            garageScroll.AddToClassList("ou-garage-rows"); garageScroll.AddToClassList("mxa-option-rows");
            garageScroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden; garageScroll.verticalScrollerVisibility = ScrollerVisibility.Auto;
            if (garageScroll.parent == null) panel.Add(garageScroll);
            garageRows = garageScroll.contentContainer;
            // the right mouse button on the list goes up a level too (the pad's B)
            garageScroll.RegisterCallback<PointerUpEvent>(e => { if (e.button == 1 && garageDialog == null && osk == null) { OuSound.Play("Back"); GarageBack(); } });
            garageStatus = OuUi.Text("CHANGES SAVE AUTOMATICALLY", "mxa-note ou-garage-status", panel);
            var actions = OuTheme.Part(panel, "garage-actions", "ou-garage-actions");
            var ride = OuUi.Button("TEST RIDE", "mxa-start ou-cta ou-garage-ride", () => GarageTestRide(), actions); ride.name = "garage-test-ride";   // (OpenShell.TestRide.cs)
            var done = OuUi.Button("DONE", "mxa-start ou-cta", ShowMain, actions); done.name = "garage-back";
            OuTheme.Backdrop(done);
            OuWidgets.LinkRow(new[] { ride, done });

            // the stage
            var world = MenuWorld;
            var stage = OuTheme.Part(garage, "garage-stage", "ou-stage");
            var glow = OuTheme.Part(stage, "stage-glow", "ou-stage-glow"); glow.style.backgroundImage = OpenGarageStage.Glow;
            var floor = OuTheme.Part(stage, "stage-floor", "ou-stage-floor"); floor.style.backgroundImage = OuUi.ScrimBottom;
            garageView = OuTheme.Part(stage, "stage-view", "ou-stage-view");
            garageView.pickingMode = PickingMode.Position;
            if (world != null)
            {   // the menu world: the rider and his vehicle stand in the compound behind the page; the stage is a window onto it
                stage.AddToClassList("ou-stage-3d");
                stage.style.backgroundColor = new Color(0, 0, 0, 0);
                stage.style.borderTopWidth = 0; stage.style.borderBottomWidth = 0; stage.style.borderLeftWidth = 0; stage.style.borderRightWidth = 0;
                glow.style.display = DisplayStyle.None; floor.style.display = DisplayStyle.None;
                garagePreview = world.Hero;
                garageStage = new OpenGarageStage(garagePreview) { fitScale = GarageFit, detailFitScale = 1.35f };
                // the cards under the view stay readable over the lit compound floor
                var bottomPart = stage.Q<VisualElement>("stage-bottom");
                if (bottomPart != null) { bottomPart.style.backgroundImage = OuUi.ScrimBottom; bottomPart.style.unityBackgroundScaleMode = ScaleMode.StretchToFill; }
                world.GarageMode();
                garageView.RegisterCallback<GeometryChangedEvent>(_ => GarageFitTexture());
                HookStageMouse(garageView);
            }
            else try
            {
                garagePreview = new FeGarage(transform) { stageFloor = true };
                garageStage = new OpenGarageStage(garagePreview);
                var image = new Image { name = "garage-preview", scaleMode = ScaleMode.ScaleToFit, pickingMode = PickingMode.Ignore, image = garagePreview.Texture };
                image.AddToClassList("ou-garage-image"); garageView.Add(image);
                garageView.RegisterCallback<GeometryChangedEvent>(_ => GarageFitTexture());
                HookStageMouse(garageView);
            }
            catch (Exception e) { garageStatus.text = "Preview unavailable: " + e.Message; Debug.LogWarning("[Garage] " + e); garagePreview = null; garageStage = null; }

            garageIdBlock = OuTheme.Part(stage, "stage-id", "ou-stage-id");
            var nameRow = OuUi.Div("ou-stage-name-row", garageIdBlock);
            garageNumber = OuUi.Text("", "ou-stage-number2", nameRow);
            garageName = OuUi.Text("", "ou-stage-name", nameRow);
            garageSub = OuUi.Text("", "ou-stage-sub", garageIdBlock);
            var top = OuUi.Div("ou-stage-top", stage);
            garageCredits = OuUi.Text("", "ou-pill ou-pill-gold ou-stage-credits", top);
            garageHint = OuUi.Text("", "ou-stage-hint2", stage);

            var bottom = OuTheme.Part(stage, "stage-bottom", "ou-stage-bottom");
            var model = OuUi.Div("ou-stage-model", bottom);
            var copy = OuUi.Div("ou-stage-model-copy", model);
            garageMake = OuUi.Text("", "ou-stage-make", copy);
            garageModelName = OuUi.Text("", "ou-stage-model-name", copy);
            garageBadges = OuUi.Div("ou-stage-badges", copy);
            OuUi.Div("ou-spacer", model);
            garageStats = OuUi.Div("ou-gstats", model);
            GarageHints();
        }

        // ------------------------------------------------------------------------------------------------ the stage

        void GarageFitTexture()
        {
            if (garagePreview == null || garageView == null) return;
            if (garagePreview.world)
            {   // the menu world: the subject framed in the stage's part of the screen
                var full = Root.worldBound; var r = garageView.worldBound;
                if (float.IsNaN(r.width) || r.width < 32 || r.height < 32 || full.width < 32) return;
                garageStage.viewCenter = new Vector2((r.center.x - full.center.x) / (full.width * 0.5f), -(r.center.y - full.center.y) / (full.height * 0.5f));
                garageStage.viewHeight = r.height / full.height; garageStage.screenAspect = full.width / full.height;
                garageStage.SetAspect(r.width / r.height);
                garageStage.Refit();
                if (OpenGarageStage.Verbose) Debug.Log($"[garage-stage] world view {r} of {full}: centre {garageStage.viewCenter}, height {garageStage.viewHeight:F2}, aspect {r.width / r.height:F2}");
                return;
            }
            float w = garageView.resolvedStyle.width, h = garageView.resolvedStyle.height;
            if (float.IsNaN(w) || float.IsNaN(h) || w < 32 || h < 32) return;
            float ratio = UnityEngine.Screen.height / OuLayout.LayoutHeight;
            int pw = Mathf.RoundToInt(w * ratio), ph = Mathf.RoundToInt(h * ratio);
            float cap = 1600f / Mathf.Max(pw, ph); if (cap < 1f) { pw = Mathf.RoundToInt(pw * cap); ph = Mathf.RoundToInt(ph * cap); }
            pw = Mathf.Max(256, pw & ~1); ph = Mathf.Max(256, ph & ~1);
            garagePreview.Resize(pw, ph);
            garageStage?.SetAspect(w / h);
            garageStage?.Refit();
        }

        // mouse: drag turns, the wheel zooms
        void HookStageMouse(VisualElement view)
        {
            bool drag = false; Vector2 last = default;
            view.RegisterCallback<PointerDownEvent>(e => { if (e.button != 0 && e.button != 1) return; drag = true; last = e.position; view.CapturePointer(e.pointerId); });
            view.RegisterCallback<PointerMoveEvent>(e =>
            {
                if (!drag || garageStage == null) return;
                var d = (Vector2)e.position - last; last = e.position;
                garageStage.Orbit(-d.x * 0.45f, d.y * 0.25f);
            });
            view.RegisterCallback<PointerUpEvent>(e => { drag = false; if (view.HasPointerCapture(e.pointerId)) view.ReleasePointer(e.pointerId); });
            view.RegisterCallback<WheelEvent>(e => { garageStage?.Zoom(-e.delta.y * 0.04f); e.StopPropagation(); });
        }

        /// Every frame: the camera, the right stick and the triggers, and the model that is due.
        void GarageUpdate()
        {
            if (GarageTestRideKey()) return;   // T / X: the test ride (OpenShell.TestRide.cs)
            if (garagePreview == null || garageStage == null) return;
            float dt = MenuTime.Delta;
            var focused = _layer.focusController?.focusedElement;
            bool typing = focused is TextElement || focused is TextField;
            if (!typing && garageDialog == null && osk == null)
            {
                var look = _input.Look;
                if (look.sqrMagnitude > 0.04f) garageStage.Orbit(look.x * 120f * dt, look.y * 50f * dt);
                float zoom = _input.Zoom;
                if (Mathf.Abs(zoom) > 0.05f) garageStage.Zoom(zoom * 1.4f * dt);
            }
            if (garagePendingMake != null && Time.unscaledTime >= garagePendingAt) GarageShowPending();
            if (garagePreview.world && MenuWorld is OpenMenuWorld world)
            {   // the previewed vehicle came (or the equipped one came back): the shot fits it
                string shown = (world.PreviewKey ?? "") + "|" + garagePreview.BuildSerial;
                garageStage.holdAim = world.Walking;   // (the pair walks into a fixed shot, the camera follows them)
                garageStage.aimOffset = world.WalkOffset;
                world.SpotAim = garagePreview.root.transform.position + garageStage.AimPoint + garageStage.aimOffset;   // (the key light on the shot's subject)
                if (shown != garagePadShown && !world.Walking) { garagePadShown = shown; garageStage.Refit(); }
            }
            garageStage.autoTurn = !OuMotion.Reduced;
            garageStage.Tick(dt);
            if (!OuMotion.Reduced && !garagePreview.world) garagePreview.Animate(dt);   // the rider's idle (the game's garage clip) holds its first pose with REDUCE MOTION (the menu world plays its own)
        }

        /// The branch the current level is on (the VEHICLE or RIDER level at its head, else the home / PRO RIDERS).
        GKind GarageBranch => garageStack.Count > 1 ? garageStack[1].kind : GKind.Home;
        bool GarageOnRider => GarageBranch == GKind.Rider || GarageBranch == GKind.ProRiders;
        bool GarageOnVehicle => GarageBranch == GKind.Vehicle && GTop.kind != GKind.Classes && GTop.kind != GKind.Models;

        /// The stage will show this one moment after the focus settles (the same key twice shows once).
        void GarageShow(string key, Func<(BikeSpec bike, RiderSpec rider, bool beside)> make, OpenGarageStage.Shot shot, bool resetAngles)
        {
            if (garageStage == null) return;
            if (garagePreview != null && garagePreview.world)
            {   // the menu world: the rider and his vehicle stay together (a gear preview keeps the vehicle, a part preview the rider); the RIDER
                // levels frame the rider, the VEHICLE levels the vehicle
                MenuWorld?.EndPreview();
                var inner = make;
                make = () =>
                {
                    var (b, r, beside) = inner();
                    if (b == null || r == null) { var (eb, er, _) = EquippedHero(); b ??= eb; r ??= er; beside = true; }
                    return (b, r, beside);
                };
                if (shot == OpenGarageStage.Shot.Overview && GarageOnRider) shot = OpenGarageStage.Shot.Rider;
                else if (shot == OpenGarageStage.Shot.Overview && GarageOnVehicle) shot = OpenGarageStage.Shot.Vehicle;
            }
            key += "|m" + BikeMods.Version;   // (a bike model choice changed: the same bike is built again)
            if (key == garageShownKey && garagePendingMake == null)
            {
                if (shot != garageStage.Current || resetAngles) garageStage.Frame(shot, resetAngles);
                return;
            }
            garagePendingKey = key; garagePendingMake = make; garagePendingShot = shot; garagePendingReset |= resetAngles;
            garagePendingAt = garageShownKey == null ? 0 : Time.unscaledTime + 0.12f;
        }

        void GarageShowPending()
        {
            var make = garagePendingMake; garagePendingMake = null;
            bool reset = garagePendingReset; garagePendingReset = false;
            using var perf = Platform.ReflexMenuPerf.Measure("garage preview built");
            try
            {
                var (bike, rider, beside) = make();
                try { garagePreview.ShowSpecs(bike, rider, beside); }
                catch (Exception e)
                {   // a high-resolution twin the install lacks (a pro's tyre): the normal models, else the rider alone
                    Debug.LogWarning("[Garage] preview with the high-resolution models: " + e.Message);
                    garagePreview.Unload();   // (the failed build left its key behind)
                    garagePreview.highRes = false;
                    try { garagePreview.ShowSpecs(bike, rider, beside); }
                    catch (Exception e2) { Debug.LogWarning("[Garage] preview: " + e2.Message); garagePreview.ShowSpecs(null, rider, false); }
                    finally { garagePreview.highRes = true; }
                }
                garageShownKey = garagePendingKey;
            }
            catch (Exception e) { Debug.LogWarning("[Garage] preview: " + e.Message); garageShownKey = null; }
            garageStage.Frame(garagePendingShot, reset, false);
        }

        void ReleaseGarage()
        {
            if (garagePreview != null && garagePreview.world) MenuWorld?.EndPreview();   // (the menu world's hero stays: the hub shows him)
            else garagePreview?.Destroy();
            if (garageStack.Count > 0 && garageStack[0].kind == GKind.Home && garageBuilt) garageHomeFocus = garageStack[0].focus;
            garagePreview = null; garageStage = null; garagePadShown = null;
            garageRows = null; garageScroll = null; garageView = null; garageDialog = null; garageHead = null;
            garageStatus = null; garageName = null; garageNumber = null; garageSub = null; garageMake = null; garageModelName = null; garageCredits = null; garageHint = null;
            garageCrumb = null; garageTitle = null; garageLevelSub = null;
            garageBadges = null; garageStats = null; garageIdBlock = null; garageTyping = null; garageTypeCancel = null;
            garagePendingMake = null; garageShownKey = null; garagePendingKey = null; garageBuilt = false;
            garageSoundSlot = null; garageSoundModel = -1;
            if (_nav != null) _nav.Modal = null;
        }

        // ------------------------------------------------------------------------------------------------ levels and focus

        VisualElement GarageDefaultItem()
        {
            if (garageTyping != null && _nav.IsOn(garageTyping)) return garageTyping;   // (a typed box was left by the pad: the focus goes back to its row)
            if (garageRows != null)
            {
                var first = OuUi.FirstItem(garageRows);
                if (first != null) return first;
            }
            return _layer.Q<VisualElement>("garage-back");
        }

        /// Down one level (the entry focused now is remembered on the level left).
        void GarageEnter(GKind kind, int a = -1, int b = -1)
        {
            if (!garageBuilt) return;
            if (osk != null) CloseKeyboard(false);
            if (garageDialog != null) GarageCloseDialog(false);
            var top = GTop; if (top != null && _nav.Focused != null && garageRows != null && garageRows.Contains(_nav.Focused)) top.focus = _nav.Focused.name;
            garageStack.Add(new GLevel(kind, a, b));
            RebuildGarageLevel(null, true);
        }

        /// Up exactly one level, on the entry it was left from; the home's B leaves the garage.
        void GarageUp()
        {
            if (garageStack.Count <= 1) { ShowMain(); return; }
            var left = GTop;
            garageStack.RemoveAt(garageStack.Count - 1);
            if (garageStack.Count == 2) garageBranchFocus[GarageBranch] = GTop.focus;
            // the level left was browsed without confirming: the stage shows the equipped setup again (the level's own focus shows it)
            if (left.kind == GKind.Models && garagePreview != null && garagePreview.world && MenuWorld is OpenMenuWorld world && !world.PickPending) world.EndPreview();
            RebuildGarageLevel(GTop.focus, true);
        }

        /// LB / RB: VEHICLE <-> RIDER from any level (each comes back on the entry it was left on). At the home LB is VEHICLE, RB RIDER.
        void GarageSwitchBranch(int d)
        {
            if (!garageBuilt || garageDialog != null || osk != null) return;
            var branch = GarageBranch;
            GKind to = branch == GKind.Vehicle ? GKind.Rider : branch == GKind.Rider ? GKind.Vehicle : d < 0 ? GKind.Vehicle : GKind.Rider;
            GarageGoBranch(to);
        }

        void GarageGoBranch(GKind to)
        {
            if (!garageBuilt) return;
            if (osk != null) CloseKeyboard(false);
            if (garageDialog != null) GarageCloseDialog(false);
            if (garageStack.Count == 2 && GTop.kind == to) return;
            if (garageStack.Count >= 2) { var head = garageStack[1]; if (head.kind == GKind.Vehicle || head.kind == GKind.Rider) garageBranchFocus[head.kind] = garageStack.Count == 2 && _nav.Focused != null ? _nav.Focused.name : head.focus; }
            if (garagePreview != null && garagePreview.world && MenuWorld is OpenMenuWorld world && !world.PickPending) world.EndPreview();
            garageStack.RemoveRange(1, garageStack.Count - 1);
            garageStack[0].focus = to == GKind.Vehicle ? "garage-h-vehicle" : "garage-h-rider";
            garageStack.Add(new GLevel(to, focus: garageBranchFocus.TryGetValue(to, out var f) ? f : null));
            RebuildGarageLevel(GTop.focus, true);
        }

        /// The current level built again from the profile. The focus goes to the entry named (else the level's remembered one, else the first);
        /// entered = the level was just entered (the list starts at the top).
        void RebuildGarageLevel(string focusName, bool entered = false)
        {
            if (garageRows == null || GTop == null) return;
            using var perf = Platform.ReflexMenuPerf.Measure("garage page built");
            var place = _nav.Remember();
            garageRows.Clear();
            garageTyping = null; garageTypeCancel = null; garageSoundSlot = null; garageSoundModel = -1;
            if (entered) garageScroll.scrollOffset = Vector2.zero;
            var top = GTop;
            bool built;
            try { built = BuildGarageLevel(top); }
            catch (Exception e) { Debug.LogWarning("[Garage] level " + top.kind + ": " + e); built = false; }
            if (!built && garageStack.Count > 1)
            {   // the level cannot be shown any more (the vehicle it was for went away, a pro rider was chosen): one up
                garageStack.RemoveAt(garageStack.Count - 1);
                RebuildGarageLevel(GTop.focus, true);
                return;
            }
            GarageHeadText();
            GarageStageHeader();
            string remembered = focusName ?? top.focus;
            VisualElement target = remembered != null ? garageRows.Q<VisualElement>(remembered) : null;
            if (target != null && !_nav.IsOn(target)) target = null;
            if (target != null) _nav.Focus(target);
            else if (!entered && place.valid && _nav.Focused != null && !garageRows.Contains(_nav.Focused)) { /* the focus is on TEST RIDE or DONE: it stays */ }
            else if (!entered && place.valid) _nav.Restore(place, null);
            else _nav.Focus(GarageDefaultItem());
            if (_nav.Focused == null || !garageRows.Contains(_nav.Focused)) GarageLevelIdle();   // (nothing on the level took the focus: its own stage)
            GarageHints();
        }

        /// Notes where the focus is on this level (coming back up to it puts it there).
        void GarageRowFocused(VisualElement row)
        {
            if (row != null && !string.IsNullOrEmpty(row.name) && GTop != null) GTop.focus = row.name;
        }

        bool GarageBack()
        {
            if (garageDialog != null) { GarageCloseDialog(); return true; }
            if (garageTypeCancel != null) { garageTypeCancel(); return true; }
            if (Time.frameCount - garageTypeEnded <= 1) return true;   // B while typing a number: the typing is dropped, the row stays
            GarageUp();
            return true;
        }

        /// The legend band: what A does on this level, B, the shoulders, the test ride and the camera.
        void GarageHints()
        {
            var top = GTop;
            string submit = top == null ? "SELECT" : top.kind == GKind.GearLine || top.kind == GKind.PartLine ? "EQUIP / BUY" : top.kind == GKind.Models ? "RIDE THIS / BUY" : top.kind == GKind.Tuning ? "ADJUST" : "SELECT";
            string back = garageStack.Count <= 1 ? "DONE" : "BACK";
            Hints((OuUi.KeySubmit, submit), (OuUi.KeyCancel, back), (OuUi.KeyTabs, "VEHICLE / RIDER"), ("T", "TEST RIDE"), (OuGlyphs.Look, "TURN"), (OuGlyphs.Zoom, "ZOOM"));
            if (garageHint != null) garageHint.text = "DRAG TO TURN  ·  WHEEL TO ZOOM";
        }

        /// The head: the way down (each level's short name), the level's title and what to choose on it, the branch switch.
        void GarageHeadText()
        {
            if (garageCrumb == null) return;
            garageCrumb.text = string.Join("  ›  ", garageStack.Take(garageStack.Count - 1).Select(GarageLevelName));
            garageTitle.text = GarageLevelName(GTop);
            garageLevelSub.text = GarageLevelHelp(GTop);
            var up = garageHead.Q<VisualElement>("garage-up");
            if (up != null) up.Q<Label>(className: "ou-gup-label").text = garageStack.Count <= 1 ? "DONE" : "BACK";
            garageHead.Q<VisualElement>("garage-tab-2")?.EnableInClassList("ou-gtab-on", GarageBranch == GKind.Vehicle);
            garageHead.Q<VisualElement>("garage-tab-0")?.EnableInClassList("ou-gtab-on", GarageBranch == GKind.Rider);
            garageCrumb.style.display = garageStack.Count <= 1 ? DisplayStyle.None : DisplayStyle.Flex;
        }

        // ------------------------------------------------------------------------------------------------ the stage's cards

        /// The rider's name and the credits; the vehicle's card and bars of the subject.
        void GarageStageHeader()
        {
            if (garageName == null) return;
            bool pro = OpenGarageData.Pro;
            string persona = (ReflexProfile.HasPersona ? ReflexProfile.Persona : "YOUR RIDER").ToUpperInvariant();
            if (pro)
            {
                var ride = OpenGarageData.ProRides().FirstOrDefault(r => r.rider == OpenGarageData.ProId && r.vehicle == OpenGarageData.Equipped);
                garageName.text = ride?.name ?? "PRO RIDER"; garageSub.text = "PRO RIDER"; garageNumber.text = "";
            }
            else
            {
                garageName.text = persona; garageSub.text = "YOUR RIDER  ·  " + GarageWearing();
                string number = OpenGarageData.Tab("PlayerRider").Str("Number", 0);
                garageNumber.text = string.IsNullOrEmpty(number) ? "" : "#" + number;
            }
            garageCredits.text = OpenGarageData.Unlocked ? "UNLOCKED PROFILE" : "CREDITS  " + OpenGarageData.Balance;
            garageCredits.EnableInClassList("ou-pill-good", OpenGarageData.Unlocked);
            garageCredits.EnableInClassList("ou-pill-gold", !OpenGarageData.Unlocked);
            if (GTop?.kind != GKind.Models || garageSubjectModel < 0) GarageSubject(OpenGarageData.EquippedModel, pro ? -1 : OpenGarageData.Equipped);
            else GarageSubject(garageSubjectModel, garageSubjectVehicle, garageSubjectMod);
        }

        /// A line of what the rider wears: the helmet's and the jersey's product lines.
        string GarageWearing()
        {
            var parts = new List<string>();
            foreach (var ty in OpenGarageData.GearTypes().Where(t => t.factory == "Helmet" || t.factory == "Jersey"))
            {
                int g = OpenGarageData.EquippedGear(ty.id);
                if (g < 0) continue;
                var it = OpenGarageData.GearItems(ty.id).FirstOrDefault(i => i.id == g);
                if (it != null) parts.Add(string.IsNullOrEmpty(it.line) ? it.title : it.line);
            }
            return parts.Count > 0 ? string.Join(" · ", parts.Distinct()).ToUpperInvariant() : "READY TO RIDE";
        }

        static readonly Dictionary<string, Texture2D> s_modLogos = new Dictionary<string, Texture2D>();
        /// A mod's brand logo (an image file of its folder; null without one or when it cannot be read).
        static Texture2D ModLogo(string path)
        {
            if (string.IsNullOrEmpty(path)) return null;
            if (s_modLogos.TryGetValue(path, out var t) && t) return t;
            try
            {
                t = new Texture2D(2, 2, TextureFormat.RGBA32, false) { name = "mod_logo", hideFlags = HideFlags.DontSave };
                if (!ImageConversion.LoadImage(t, System.IO.File.ReadAllBytes(path))) t = null;
            }
            catch (Exception e) { Debug.LogWarning("[BikeMods] logo " + path + ": " + e.Message); t = null; }
            return s_modLogos[path] = t;
        }

        /// The stage's title block for a vehicle model: modId = the mod whose bike is in view ("" the original, null the one the player rides as: the
        /// equipped bike's choice). A mod's bike is titled as the mod names it (brand line, model line, its own logo when it has one).
        void GarageSubject(int model, int playerVehicle, string modId = null)
        {
            if (garageMake == null) return;
            garageSubjectModel = model; garageSubjectVehicle = playerVehicle; garageSubjectMod = modId;
            var row = FeGarageData.Unique("VehicleModel", "VehicleModelId", model);
            garageBadges.Clear(); garageStats.Clear();
            if (row == null) { garageMake.text = ""; garageModelName.text = "NO VEHICLE"; return; }
            int product = FeGarageData.ProductOfAsset(FeTables.I(row, "StoreAssetId"));
            int type = FeTables.I(row, "VehicleTypeId");
            string brand = OpenGarageData.BrandName(product), title = OpenGarageData.Title(product, "VEHICLE");
            BikeMod mod = modId == null ? (playerVehicle >= 0 && playerVehicle == (OpenGarageData.Pro ? -1 : OpenGarageData.Equipped) ? BikeMods.ChosenFor(model) : null) : BikeMods.Find(modId);
            if (mod != null && !OpenGarageData.ModsOfModel(model).Contains(mod)) mod = null;
            var named = mod == null ? null : BikeMods.NameFor(mod, model, title);
            if (named != null && named.brand.Length > 0) brand = named.brand;
            if (named != null && named.model.Length > 0) title = named.model;
            garageMake.text = (brand ?? OpenGarageData.TypeName(type)).ToUpperInvariant();
            garageModelName.text = title.ToUpperInvariant();
            string ownClass = mod?.rx?.bikes.FirstOrDefault(b => b.vehicleModelId == model)?.garageClass;   // (an imported Alive 50cc / 650: its own class)
            OuUi.Text(ownClass ?? OpenGarageData.TypeName(type), "ou-badge", garageBadges);
            if (mod != null) OuUi.Text("MOD", "ou-badge", garageBadges);
            if (playerVehicle >= 0 && playerVehicle == (OpenGarageData.Pro ? -1 : OpenGarageData.Equipped) && (mod?.id ?? "") == BikeMods.ChoiceFor(model)) OuUi.Text("EQUIPPED", "ou-badge ou-badge-dark", garageBadges);
            Texture2D logoTex = null;
            if (mod != null) logoTex = ModLogo(named?.logo);
            else { int icon = OpenGarageData.BrandIcon(product); logoTex = icon >= 0 ? OpenContent.Icon(icon) : null; }
            if (logoTex) { var logo = OuUi.Div("ou-stage-brand", garageBadges); logo.style.backgroundImage = logoTex; }
            foreach (var s in OpenGarageData.Stats(model, playerVehicle))
            {
                var col = OuUi.Div("ou-gstat", garageStats);
                OuUi.Text(s.name, "ou-gstat-label", col);
                GarageBars(col, s);
            }
        }

        /// The stage's title block for a piece of gear or a part in view: its product line (or brand) over its name, the section, whether it is
        /// equipped (else PREVIEW: what A would put on), its price; the vehicle's bars stay for a part.
        void GarageItemSubject(GarageItem item, string section, bool part)
        {
            if (garageMake == null || item == null) return;
            if (part) GarageSubject(OpenGarageData.EquippedModel, OpenGarageData.Equipped);
            else { garageStats.Clear(); garageSubjectModel = -1; }
            garageBadges.Clear();
            garageMake.text = (!string.IsNullOrEmpty(item.line) ? item.line : OpenGarageData.BrandName(item.product) ?? section).ToUpperInvariant();
            garageModelName.text = item.title.ToUpperInvariant();
            OuUi.Text(section, "ou-badge", garageBadges);
            if (item.equipped) OuUi.Text("EQUIPPED", "ou-badge ou-badge-dark", garageBadges);
            else
            {
                var tag = OuUi.Text("PREVIEW", "ou-badge ou-badge-preview", garageBadges); tag.name = "garage-preview-tag";
                OuUi.Text(item.Available ? "OWNED" : item.locked ? "LOCKED" : item.price.ToString("N0") + " CREDITS", "ou-badge", garageBadges);
            }
            int icon = item.brandIcon >= 0 ? item.brandIcon : item.parent >= 0 ? OpenGarageData.BrandIcon(item.parent) : -1;
            var logoTex = icon >= 0 ? OpenContent.Icon(icon) : null;
            if (logoTex) { var logo = OuUi.Div("ou-stage-brand", garageBadges); logo.style.backgroundImage = logoTex; }
        }

        /// The performance bars of an attribute: the model's base (white) and the tuning's bonus (red) on each of its two bars.
        static void GarageBars(VisualElement parent, GarageStat s)
        {
            if (!s.two)
            {
                var single = OuUi.Div("ou-gbar", parent);
                var f = OuUi.Div("ou-gbar-base", single); f.style.width = Length.Percent(Mathf.Clamp(s.base1, 0, 100));
                return;
            }
            for (int k = 0; k < 2; k++)
            {
                int b = Mathf.Clamp(k == 0 ? s.base1 : s.base2, 0, 100), bonus = k == 0 ? s.bonus1 : s.bonus2;
                var track = OuUi.Div("ou-gbar", parent);
                var fill = OuUi.Div("ou-gbar-base", track); fill.style.width = Length.Percent(b);
                var extra = OuUi.Div("ou-gbar-bonus", track); extra.style.left = Length.Percent(b); extra.style.width = Length.Percent(Mathf.Clamp(bonus, 0, 100 - b));
            }
        }

        // ------------------------------------------------------------------------------------------------ shared row widgets

        /// A list card: title, a line under it and pills on the right; go = it leads one level down (a chevron). Named, focusable; submit on
        /// A / Enter / click. logo: a brand icon on the left.
        VisualElement GarageCard(string name, string title, string sub, Action submit, params (string text, string kind)[] pills)
            => GarageCard(name, title, sub, false, null, submit, pills);

        VisualElement GarageCard(string name, string title, string sub, bool go, Texture2D logo, Action submit, params (string text, string kind)[] pills)
        {
            var card = OuUi.Item(OuUi.Div("ou-gcard" + (go ? " ou-gcard-go" : ""), garageRows), submit);
            card.name = name;
            if (logo) { var l = OuUi.Div("ou-gcard-logo", card); l.style.backgroundImage = logo; l.pickingMode = PickingMode.Ignore; }
            var copy = OuUi.Div("ou-gcard-copy", card);
            OuUi.Text(title, "ou-gcard-title", copy).enableRichText = false;
            if (!string.IsNullOrEmpty(sub)) OuUi.Text(sub, "ou-gcard-sub", copy).enableRichText = false;
            var tags = OuUi.Div("ou-gcard-tags", card);
            foreach (var (text, kind) in pills) if (!string.IsNullOrEmpty(text)) OuWidgets.Pill(tags, text, kind);
            if (go) OuUi.Text("›", "ou-gcard-chevron", card).pickingMode = PickingMode.Ignore;
            card.RegisterCallback<FocusInEvent>(_ => GarageRowFocused(card));
            return card;
        }

        void GarageNote(string text) => OuUi.Text(text, "mxa-note ou-garage-note", garageRows);

        /// (w-gearmods) The gear / kit packs being installed and those that could not be loaded: the file and why (Mods/Gear, KitPacks).
        void GarageKitNotes(bool bikeKits)
        {
            List<KitPacks.Problem> problems; List<string> installing;
            try { problems = KitPacks.Problems; installing = KitPacks.Installing; } catch (Exception e) { Debug.LogWarning("[KitPacks] " + e.Message); return; }
            if (installing.Count > 0) GarageNote("INSTALLING " + (bikeKits ? "KIT" : "GEAR") + " PACKS  ·  " + string.Join(", ", installing).ToUpperInvariant() + "  ·  they show up when done");
            foreach (var p in problems.Take(4))
            {
                string where = p.where; string root = ReflexMods.ClientRoot;
                if (root != null && where.StartsWith(root, StringComparison.Ordinal)) where = where.Substring(root.Length).TrimStart('/', '\\');
                GarageNote("NOT LOADED  ·  " + where + "  ·  " + p.why);
            }
            if (problems.Count > 4) GarageNote("... and " + (problems.Count - 4) + " more packs that could not be loaded (the log lists them: [KitPacks] NOT LOADED)");
        }

        void GarageStatus(string text) { if (garageStatus != null) garageStatus.text = text; }

        /// The result of a change: the status line, the level again with the focus where it was.
        void GarageChanged(bool ok, string message, string focusName = null)
        {
            if (!ok) { GarageStatus((ReflexProfile.Error ?? CoreGarageProfile.Error ?? "The change could not be saved.").ToUpperInvariant()); OuSound.Play("Locked"); return; }
            GarageStatus(message);
            garageShownKey = null;
            RebuildGarageLevel(focusName ?? _nav.Focused?.name);
        }

        // ------------------------------------------------------------------------------------------------ dialogs

        /// A confirmation / message over the garage: the focus stays inside it until it closes, then returns to where it was.
        void GarageOpenDialog(string eyebrow, string title, string[] lines, params (string label, string name, Action action, bool primary)[] buttons)
        {
            if (garageDialog != null) GarageCloseDialog(false);
            var place = _nav.Remember();
            var root = OuUi.Div("ou-gdialog-root", _layer);
            var card = OuWidgets.Dialog(root);
            OuUi.Text(eyebrow, "ou-eyebrow2 ou-label", card);
            OuUi.Text(title.ToUpperInvariant(), "ou-dialog-title ou-display", card).enableRichText = false;
            foreach (var l in lines) OuUi.Text(l, "ou-dialog-text", card).enableRichText = false;
            var row = OuUi.Div("ou-button-row", card);
            var made = new List<VisualElement>();
            foreach (var b in buttons)
            {
                var action = b.action;
                var btn = OuUi.Button(b.label, "mxa-main-button" + (b.primary ? " ou-primary" : ""), () => { GarageCloseDialog(); action?.Invoke(); }, row);
                btn.name = b.name; made.Add(btn);
            }
            OuWidgets.LinkRow(made);
            garageDialog = root; garageDialogReturn = place;
            _nav.Modal = card;
            Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "CLOSE"));
            if (made.Count > 0) _nav.Focus(made[0]);
        }

        void GarageCloseDialog(bool restore = true)
        {
            if (garageDialog == null) return;
            var root = garageDialog; garageDialog = null;
            _nav.Modal = null;
            root.RemoveFromHierarchy();
            if (restore)
            {
                _nav.Restore(garageDialogReturn, null);
                GarageHints();
            }
        }

        /// BUY: the price, the credits before and after; or why it cannot be bought.
        void GaragePurchase(GarageItem item, Action<GarageItem> afterBuy)
        {
            int balance = FeGarageData.Balance;
            if (item.locked)
            {
                GarageOpenDialog("LOCKED", item.title, new[] { "This is not unlocked yet. Keep playing career events to unlock it." }, ("OK", "garage-dialog-ok", null, true));
                return;
            }
            if (balance < item.price)
            {
                GarageOpenDialog("NOT ENOUGH CREDITS", item.title, new[] { "PRICE  " + item.price.ToString("N0") + "   ·   YOUR CREDITS  " + balance.ToString("N0"), "Earn credits in career events to buy this." }, ("OK", "garage-dialog-ok", null, true));
                return;
            }
            string what = string.IsNullOrEmpty(item.line) || item.mod != null ? item.title : item.line + "  ·  " + item.title;
            GarageOpenDialog("PURCHASE", what, new[] { "PRICE  " + item.price.ToString("N0") + " CREDITS", "YOUR CREDITS  " + balance.ToString("N0") + "  →  " + (balance - item.price).ToString("N0") },
                ("BUY", "garage-buy", () => GarageBuy(item, afterBuy), true), ("CANCEL", "garage-cancel", null, false));
        }

        void GarageBuy(GarageItem item, Action<GarageItem> afterBuy)
        {
            if (!CoreGarageProfile.PurchaseAsset(item.asset, item.price)) { GarageStatus((CoreGarageProfile.Error ?? "The purchase could not be saved.").ToUpperInvariant()); OuSound.Play("Locked"); return; }
            ReflexProfile.Open();   // the profile again (the purchase is in it)
            OuSound.Play("Purchase");
            GarageStatus("PURCHASED  ·  " + item.title.ToUpperInvariant());
            garageShownKey = null;
            afterBuy?.Invoke(item);
            RebuildGarageLevel(GTop?.focus);
        }
    }
}
