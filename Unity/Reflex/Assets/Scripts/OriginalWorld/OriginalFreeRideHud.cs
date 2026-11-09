using System;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The ORIGINAL HUD of the open core's Free Ride and its challenges (HUD STYLE ORIGINAL): the install's own layers, read at run time and
    // drawn with their own nodes, atlas materials and animations, from the core's state. The open core has no UI page of the game's there
    // (OriginalHud's race canvas is off without a race driver), so the layers' dashboards are built with BuildLayerOwn and every component
    // the Free Ride's page shows is driven here:
    //   SinglePlayerFreeride.lyr         the Free Ride's page (Freeride Single Player Page; also BigAir / Pathfinder / StepUp, which have no page of
    //                                    their own in the InGame page graph): the Hit Discoverd Component, the compass, the EventTimerComponent
    //   SinglePlayerHillClimb(+Height)   HillClimb Single Player Page: the timer on its DurationWatermark, PositionWatermark + HeightAchieved
    //   SinglePlayerTrailBlazer.lyr      TrailBlazer Single Player Page: the timer, the compass with the FinishLine icon and the distance label
    //   SinglePlayerTargetPractice.lyr   Target Practice Single Player Page: the timer, the Target Hit Component, the compass with the target icons
    //   SinglePlayerWaypoint.lyr         Waypoint Single Player Page (HudWaypointRaceSinglePlayer): its compass only, the waypoint race's direction cue
    //                                    (the rest of the race HUD is OriginalHud's National layer)
    //
    // MXUIHitDiscoveredComponent (006045b0 / 006046f0 / 00604670): it starts hidden; the "HitDiscoveryFound" event sets the text "%d/%d" (found / total),
    // restarts its timer and shows it (the layer's HitDiscoveryIn animation, 0.23 s); once the timer passes 3 s it hides (HitDiscoveryOut, 0.23 s).
    // MXUITargetPracticeComponent (006047f0 / 006048b0) is the same component for "TargetPracticeFound" (hits / total).
    //
    // MXUICompassComponent (005fd480 vf11 and its helpers; the markers come from the activity: rx_session_compass, game/rx_compass_api.h):
    //   * the heading (+0x100) is the camera's: the angle of its forward vector's XZ part from +Z, clockwise (acos of the dot product, 360 - it on the
    //     west side), in degrees;
    //   * 005fdf80: the CompassImage ("CompassTile", the strip's repeating texture) scrolls: its texture window is [-s, w - s] in tiles with
    //     s = (359 - heading) / 359 * w and w = +0xfc = 8 (the constructor's 8.0: eight tiles of 45 degrees across the 2560 px strip; +0x10c = 7.111 px
    //     per degree);
    //   * 005fe060: the four letters sit at 320 + wrapped(angle - heading) * 7.111 in the mask container (640 wide, centred on 320; the container clips);
    //   * a marker's bearing: the angle between the camera's forward and the direction to the mark (XZ), signed by the camera's right side, clamped to
    //     +-45 degrees (DAT_009d78c8; 90 in split screen), the icon at 320 + bearing * 7.111;
    //   * the Free Ride (UI kind 8, 005fe1e0) in the Race and Ride challenges' discovery mode: the ten challenge icons (BigAir .. Wheelie, each its
    //     own colour of the CompassChallenge glyph) at the level's StartActivity trigger of that type, alpha (2500^2 - d^2) / 2500^2 within 2500 ft;
    //   * a TargetPractice challenge (UI kind 0x1d, 006023a0): the 16 TargetPracticeN icons of the targets not yet hit, alpha 1 - d^2 / 500^2 (at
    //     least 0.1) within 500 ft;
    //   * a Trailblazer challenge (UI kind 0x1b, 00601e20): the FinishLine icon at "Trailblazer_Finish_Trigger", and the Distance Label under it with
    //     the distance in the player's units (FUN_005dc5b0 type 8);
    //   * a Waypoint race (UI kind 6, 005ff150): vf1_oe0 files the level's WaypointRaceGate triggers in the slots WaypointGate1..16 (the order of its
    //     scene search); every frame each slot's icon is placed at its gate's bearing and hidden, then the slot after the local driver's last gate
    //     (+0x10d4 + 1: the next gate) is shown at alpha 1 with the Distance Label under it (the XZ distance) and, when there is one, the slot after
    //     that at alpha 0.35 (DAT_009d78c4). A gate off to the side or behind sits at the strip's edge (the 45 degree clamp): that is the way to
    //     turn. "Player Wins" for the local driver (00602ac0) hides them for the rest of the race. The original has no 3D direction arrow: the
    //     drivers' "Show Arrow" / "Hide Arrow" / "Set Arrow Target" events have no subscriber in Reflex (an Untamed leftover).
    // The Center Indicator is always up. The players and tag ball icons belong to the party modes and stay hidden.
    public sealed class OriginalFreeRideHud : IDisposable
    {
        /// The layer (page) a frame shows.
        public enum Page { None, FreeRide, HillClimb, Trailblazer, TargetPractice, Waypoint }

        /// One frame's inputs (the host fills them from the model).
        public struct Frame
        {
            public float dt;
            /// The HUD may be on screen at all (HUD STYLE ORIGINAL, no menu, the HUD not hidden, no post-race page).
            public bool allowed;
            public Page page;
            /// The camera: the compass' heading and the bearings are its (the position is its origin for the distances).
            public Vector3 viewPosition, viewForward;
            public RxCompassState compass; public bool hasCompass;
            /// Hit Discovery (the Free Ride's discovery mode): the tracker's counts; a find raises the serial.
            public int discoveryFound, discoveryTotal, discoverySerial;
            /// The EventTimerComponent: shown, the time it prints (seconds).
            public bool timerShown; public float timerValue;
            /// The challenge's value (HillClimb: the best height in feet, TargetPractice: the hits) and whether the core has one.
            public bool valueValid; public float value;
            /// MeasurementSystem 1: distances in metres.
            public bool metric;
        }

        const float DiscoveryShown = 3f;           // 00604670: the timer past 3.0 s hides it
        const float PixelsPerDegree = 2560f / 360f;   // +0x10c = the strip's width / 360
        const float StripTiles = 8f;               // +0xfc (the constructor's 8.0): the CompassTile repeats eight times across the strip
        const float CenterX = 320f;                // the mask container's centre
        const float MarkerClamp = 45f;             // DAT_009d78c8
        const float AreaRange2 = 6.25e6f;          // 2500 ft squared (005fe1e0)
        const float TargetRange2 = 250000f;        // 500 ft squared (006023a0)
        const float GateAfterNextAlpha = 0.35f;    // DAT_009d78c4: the gate after the next one (005ff150)
        public const int GateSlots = 16;           // WaypointGate1..16
        static readonly string[] ChallengeIcons = { "BigAir", "HillClimb", "Jumper", "Pathfinder", "StepUp", "Stoppie", "Trailblazer", "HitDiscovery", "TargetPractice", "Wheelie" };

        readonly GameObject host; readonly Canvas canvas;
        readonly RectTransform root;
        readonly Dictionary<Page, View> views = new Dictionary<Page, View>();
        readonly HashSet<Page> failed = new HashSet<Page>();
        View active;

        /// The frame's page is on screen (tests).
        public Page Current => active != null ? active.page : Page.None;
        /// The page is built (the layer is in the install).
        public bool Built(Page p) => views.TryGetValue(p, out var v) && v.built;
        public Canvas Canvas => canvas;
        /// The counter (Hit Discovery on the Free Ride's layer, Target Hit on the TargetPractice's) is on screen (tests).
        public bool Shown => active != null && active.counterShown;
        public string Text => active?.counterMessage?.text ? active.counterMessage.text.Text : "";
        /// Tests: the compass' nodes of the current page.
        public CompassView Compass => active?.compass;
        /// Tests: the view of a page (null before it was first shown).
        public View ViewOf(Page p) => views.TryGetValue(p, out var v) ? v : null;

        public OriginalFreeRideHud(Transform parent)
        {
            host = new GameObject("Original HUD (Free Ride)", typeof(Canvas), typeof(CanvasScaler));
            host.transform.SetParent(parent, false);
            canvas = host.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 10;
            var sc = host.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;   // the game's 1280x720 space, whole
            var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(host.transform, false);
            root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
            canvas.enabled = false;
        }

        // ------------------------------------------------------------------------------------------------------------------------ one page
        /// The compass component's nodes.
        public sealed class CompassView
        {
            public OriginalUiTree.Node root, mask, image, north, east, south, west, finish, label, center;
            public readonly OriginalUiTree.Node[] challenge = new OriginalUiTree.Node[10], target = new OriginalUiTree.Node[16], gate = new OriginalUiTree.Node[GateSlots];
            public RawImage tile; public float tileWidth;
            /// The heading drawn (degrees) and, per icon index, whether it is up (tests).
            public float heading;
            public readonly bool[] challengeUp = new bool[10], targetUp = new bool[16], gateUp = new bool[GateSlots];
            /// Per gate slot, the alpha it was drawn with and its x on the strip (tests).
            public readonly float[] gateAlpha = new float[GateSlots], gateX = new float[GateSlots];
            public bool finishUp; public string labelText = "";
            /// The scroll the strip was set to (tiles of texture space), tests.
            public float scroll;
        }

        public sealed class View
        {
            public Page page; public bool built;
            public GameObject go; public RectTransform rect;
            public OriginalUiTree tree;
            public OriginalUiTree.Node timer, timerShadow, timerFore, watermark, counter, counterMessage, heightAchieved;
            public CompassView compass;
            // the counter (Hit Discovery / Target Hit)
            public bool counterShown, counterOut; public float counterFor = -1f;
            public int seenSerial = int.MinValue, seenHits = -1;
            public string timerText = "";
        }

        View Get(Page p)
        {
            if (views.TryGetValue(p, out var v)) return v;
            if (failed.Contains(p)) return null;
            v = new View { page = p };
            try { Build(v); views[p] = v; }
            catch (Exception e)
            {
                failed.Add(p);
                Debug.LogWarning("[OriginalHud] the " + p + " layer was not built (MX vs ATV Reflex install?): " + e.Message);
                if (v.go) UnityEngine.Object.Destroy(v.go);
                return null;
            }
            return v;
        }

        void Build(View v)
        {
            v.go = new GameObject("Original HUD (" + v.page + ")", typeof(RectTransform));
            v.go.transform.SetParent(root, false);
            v.rect = (RectTransform)v.go.transform;
            v.rect.anchorMin = v.rect.anchorMax = v.rect.pivot = new Vector2(0.5f, 0.5f); v.rect.sizeDelta = new Vector2(1280, 720);
            v.tree = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
            string[] layers; string[] keep;
            switch (v.page)
            {
                case Page.HillClimb:
                    layers = new[] { "SinglePlayerHillClimb.lyr", "SinglePlayerHillClimbHeight.lyr" };
                    keep = new[] { "EventTimerComponent", "DurationWatermark", "PositionWatermark", "HeightAchieved", "Distance Label" }; break;
                case Page.Trailblazer:
                    layers = new[] { "SinglePlayerTrailBlazer.lyr" };
                    keep = new[] { "EventTimerComponent", "DurationWatermark", "SinglePlayerCompassComponent" }; break;
                case Page.Waypoint:
                    layers = new[] { "SinglePlayerWaypoint.lyr" };
                    keep = new[] { "SinglePlayerCompassComponent" }; break;
                case Page.TargetPractice:
                    layers = new[] { "SinglePlayerTargetPractice.lyr" };
                    keep = new[] { "EventTimerComponent", "DurationWatermark", "Target Hit Component", "SinglePlayerCompassComponent" }; break;
                default:
                    layers = new[] { "SinglePlayerFreeride.lyr" };
                    keep = new[] { "EventTimerComponent", "Hit Discoverd Component", "SinglePlayerCompassComponent" }; break;
            }
            foreach (string layer in layers)
            {
                // BuildLayerOwn resolves the atlas materials (HitDiscoveryIcon, the compass' glyphs); the layer's dashboards and loose elements are all built
                var doc = OriginalUiAssets.Data.BuildLayerOwn(v.page == Page.Waypoint ? "HudWaypointRaceSinglePlayer" : "InGame", layer);
                foreach (var e in OriginalUi.Arr(doc, "elements")) if (e is Dictionary<string, object> d) v.tree.Build(d, v.rect, null);
                v.tree.AddAnimations(OriginalUi.Arr(doc, "layerAnimations"));
                v.tree.AddComponentAnimations(OriginalUi.Obj(doc, "layerJson"));
            }
            foreach (var r in v.tree.roots) OriginalHud.Complete(r);   // UIMaskContainer clips (the compass' strip)
            foreach (var r in v.tree.roots) OnlyKeep(r, new HashSet<string>(keep));
            v.timer = v.tree.Find("EventTimerComponent");
            v.timerShadow = v.tree.Child(v.timer, "Timer Dashboard Component Shadow"); v.timerFore = v.tree.Child(v.timer, "Timer Dashboard Component");
            v.heightAchieved = v.tree.Find("HeightAchieved"); v.watermark = v.tree.Find("DurationWatermark");
            v.counter = v.tree.Find(v.page == Page.TargetPractice ? "Target Hit Component" : "Hit Discoverd Component");
            v.counterMessage = v.tree.Child(v.counter, v.page == Page.TargetPractice ? "Target Hit message" : "Hit Discovered message");
            if (v.counter != null) { OriginalUiTree.SetVisible(v.counter, false); OriginalUiTree.SetText(v.counterMessage, ""); }   // vf3: SetVisible( FALSE ) until a find
            OriginalUiTree.SetVisible(v.timer, false);
            v.compass = BuildCompass(v.tree);
            v.built = v.page == Page.Waypoint ? v.compass != null : v.timer != null && (v.page == Page.HillClimb || v.compass != null);
            if (!v.built) throw new InvalidOperationException("the layer has no timer / compass component");
            v.go.SetActive(false);
        }

        // Hides every node that is not one of the kept components, a container of one, or inside one.
        static bool OnlyKeep(OriginalUiTree.Node n, HashSet<string> keep)
        {
            if (keep.Contains(n.name)) return true;
            bool any = false;
            foreach (var c in n.children) any |= OnlyKeep(c, keep);
            if (!any) OriginalUiTree.SetVisible(n, false);
            return any;
        }

        CompassView BuildCompass(OriginalUiTree tree)
        {
            var c = new CompassView { root = tree.Find("SinglePlayerCompassComponent") };
            if (c.root == null) return null;
            c.center = tree.Child(c.root, "Center Indicator"); c.mask = tree.Child(c.root, "MaskContainer");
            c.image = tree.Child(c.mask, "CompassImage");
            c.north = tree.Child(c.mask, "North"); c.east = tree.Child(c.mask, "East"); c.south = tree.Child(c.mask, "South"); c.west = tree.Child(c.mask, "West");
            c.finish = tree.Child(c.mask, "FinishLine"); c.label = tree.Child(c.mask, "Distance Label");
            for (int i = 0; i < 10; i++) c.challenge[i] = tree.Child(c.mask, ChallengeIcons[i]);
            for (int i = 0; i < 16; i++) c.target[i] = tree.Child(c.mask, "TargetPractice" + (i + 1));
            for (int i = 0; i < GateSlots; i++) c.gate[i] = tree.Child(c.mask, "WaypointGate" + (i + 1));
            // vf2 (005fc120) hides every icon, the labels' distance and the finish line; the activity shows the ones it has a mark for
            foreach (var n in c.mask.children)
                if (n != c.image && n != c.north && n != c.east && n != c.south && n != c.west) OriginalUiTree.SetVisible(n, false);
            OriginalUiTree.SetVisible(c.label, false);
            if (c.image?.graphic is RawImage ri && ri.texture != null)
            {
                var tex = ri.texture; tex.wrapMode = TextureWrapMode.Repeat;
                c.tile = ri; c.tileWidth = tex.width;   // (the texture's own width, 180 px, stretched over a tile of 320 px)
                var st = OriginalUi.Obj(c.image.json, "style");
                if (OriginalUi.Arr(st, "atlasUv") != null) Debug.LogWarning("[OriginalHud] the compass strip is an atlas sprite: it cannot scroll as a repeating tile");
            }
            return c;
        }

        // ------------------------------------------------------------------------------------------------------------------------ per frame
        /// One frame.
        public void Tick(in Frame f)
        {
            View want = null;
            if (f.allowed && f.page != Page.None)
            {
                want = Get(f.page);
                if (want == null && f.page != Page.FreeRide) want = Get(Page.FreeRide);   // a challenge layer missing in the install: the Free Ride's page
            }
            if (active != null && active != want) { active.go.SetActive(false); HideCounter(active); }
            active = want;
            canvas.enabled = active != null;
            if (active == null) return;
            if (!active.go.activeSelf) active.go.SetActive(true);
            TickTimer(active, f);
            TickCounter(active, f);
            TickHeight(active, f);
            if (active.compass != null) TickCompass(active, f);
            active.tree.Tick(f.dt);
        }

        void TickTimer(View v, in Frame f)
        {
            if (v.timer == null) return;   // the Waypoint page keeps the compass only
            OriginalUiTree.SetVisible(v.timer, f.timerShown);
            OriginalUiTree.SetVisible(v.watermark, f.timerShown);
            if (!f.timerShown) return;
            string t = OriginalUi.Time(f.timerValue);
            if (t == v.timerText) return;
            v.timerText = t;
            OriginalUiTree.SetText(v.timerShadow, t); OriginalUiTree.SetText(v.timerFore, t);
        }

        // MXUIHillClimbDistanceComponent (00604350 -> 006043e0): the height achieved, FUN_005dc5b0(8, int(feet)) in the player's units
        void TickHeight(View v, in Frame f)
        {
            if (v.heightAchieved == null) return;
            OriginalUiTree.SetVisible(v.heightAchieved, true);
            OriginalUiTree.SetVisible(v.tree.Find("PositionWatermark"), true);
            OriginalUiTree.SetVisible(v.tree.Find("Distance Label"), true);
            OriginalUiTree.SetText(v.heightAchieved, Distance(f.valueValid ? Mathf.Max(0f, f.value) : 0f, f.metric));
        }

        void TickCounter(View v, in Frame f)
        {
            if (v.counter == null) return;
            bool find = false; string text = null;
            if (v.page == Page.TargetPractice)
            {   // TargetPracticeFound {player, hits, total}: "%d/%d"
                int hits = f.valueValid ? Mathf.RoundToInt(f.value) : 0, total = 0;
                if (f.hasCompass) for (int i = 0; i < (int)f.compass.count && i < RxCompassState.Marks; i++) if (f.compass.Type(i) == RxCompassState.TypeTarget) total++;
                if (hits < v.seenHits) v.seenHits = hits;
                if (v.seenHits < 0) v.seenHits = hits;
                else if (hits > v.seenHits) { v.seenHits = hits; find = total > 0; text = hits + "/" + total; }
            }
            else
            {   // HitDiscoveryFound: the tracker's counts, a find raises the serial; a drop of the serial is a new activity (its count is the baseline)
                if (f.discoverySerial < v.seenSerial) { v.seenSerial = f.discoverySerial; HideCounter(v); }
                if (v.seenSerial == int.MinValue) v.seenSerial = f.discoverySerial;
                else if (f.discoverySerial > v.seenSerial) { v.seenSerial = f.discoverySerial; find = f.discoveryTotal > 0; text = f.discoveryFound + "/" + f.discoveryTotal; }
            }
            if (find)
            {   // 006046f0: the text, the timer back to 0, visible
                OriginalUiTree.SetText(v.counterMessage, text);
                v.counterFor = 0f; v.counterOut = false;
                if (!v.counterShown) { OriginalUiTree.SetVisible(v.counter, true); v.counterShown = true; }
                v.tree.Play("HitDiscoveryIn", v.counter);
            }
            if (v.counterShown)
            {
                v.counterFor += f.dt;
                if (!v.counterOut && v.counterFor >= DiscoveryShown) { v.counterOut = true; v.tree.Play("HitDiscoveryOut", v.counter); v.counterFor = DiscoveryShown; }
                else if (v.counterOut && !v.tree.IsPlaying("HitDiscoveryOut", v.counter)) HideCounter(v);
            }
        }

        static void HideCounter(View v)
        {
            if (v.counter == null || !v.counterShown) return;
            v.tree.Finish("HitDiscoveryOut", v.counter);
            OriginalUiTree.SetVisible(v.counter, false);
            v.counterShown = false; v.counterFor = -1f; v.counterOut = false;
        }

        // ------------------------------------------------------------------------------------------------------------------------ the compass
        void TickCompass(View v, in Frame f)
        {
            var c = v.compass;
            var fwd = new Vector3(f.viewForward.x, 0f, f.viewForward.z);
            if (fwd.sqrMagnitude < 1e-6f) fwd = new Vector3(0f, 0f, 1f);
            fwd.Normalize();
            var right = new Vector3(fwd.z, 0f, -fwd.x);   // the camera's right (Unity's left-handed y-up space: up x forward)
            float heading = Heading(fwd);
            c.heading = heading;
            // 005fdf80: the strip's texture window
            float frac = (359f - heading) / 359f, w = StripTiles, s = frac * w;
            c.scroll = -s;
            if (c.tile != null) c.tile.uvRect = new Rect(-s, 0f, w, 1f);
            // 005fe060: N, E, S, W
            PlaceLetter(c.north, 0f, heading); PlaceLetter(c.east, 90f, heading); PlaceLetter(c.south, 180f, heading); PlaceLetter(c.west, 270f, heading);

            for (int i = 0; i < 10; i++) { c.challengeUp[i] = false; }
            for (int i = 0; i < 16; i++) { c.targetUp[i] = false; }
            for (int i = 0; i < GateSlots; i++) { c.gateUp[i] = false; c.gateAlpha[i] = 0f; }
            c.finishUp = false; bool labelUp = false;
            Vector3 vp = f.viewPosition;
            if (f.hasCompass && f.compass.valid != 0)
            {
                var st = f.compass;
                int n = Mathf.Min((int)st.count, RxCompassState.Marks);
                if (v.page == Page.FreeRide && st.challengeKind < 0 && st.discoveryMode != 0)
                {   // 005fe1e0: one icon per challenge type (the last mark of a type wins, as the slot is overwritten)
                    for (int i = 0; i < n; i++)
                    {
                        if (st.Type(i) != RxCompassState.TypeArea) continue;
                        int k = st.Index(i); if (k < 0 || k >= 10) continue;
                        var p = st.Position(i);
                        Place(c.challenge[k], vp, new Vector3(p.x, p.y, p.z), fwd, right, AreaRange2, 0f, out _);
                        c.challengeUp[k] = true;
                    }
                }
                else if (v.page == Page.TargetPractice)
                {   // 006023a0: the targets not hit yet, nearer is brighter (at least 0.1)
                    for (int i = 0; i < n; i++)
                    {
                        if (st.Type(i) != RxCompassState.TypeTarget || st.Aux(i) != 0) continue;
                        int k = st.Index(i); if (k < 0 || k >= 16) continue;
                        var p = st.Position(i);
                        Place(c.target[k], vp, new Vector3(p.x, p.y, p.z), fwd, right, TargetRange2, 0.1f, out _);
                        c.targetUp[k] = true;
                    }
                }
                else if (v.page == Page.Trailblazer)
                {   // 00601e20: the finish and its distance
                    for (int i = 0; i < n; i++)
                    {
                        if (st.Type(i) != RxCompassState.TypeFinish) continue;
                        var p = st.Position(i);
                        var mark = new Vector3(p.x, p.y, p.z);
                        Place(c.finish, vp, mark, fwd, right, 0f, 0f, out float x);
                        c.finishUp = true; labelUp = true;
                        if (c.label != null)
                        {
                            float dx = mark.x - vp.x, dz = mark.z - vp.z;
                            c.labelText = Distance(Mathf.Sqrt(dx * dx + dz * dz), f.metric);
                            OriginalUiTree.SetText(c.label, c.labelText);
                            SetX(c.label, x);
                        }
                        break;
                    }
                }
                else if (v.page == Page.Waypoint && st.waypointFinished == 0)
                {   // 005ff150: the next gate (slot gate + 1) at alpha 1 with its distance, the one after it at 0.35
                    int next = st.waypointGate + 1;
                    for (int i = 0; i < n; i++)
                    {
                        if (st.Type(i) != RxCompassState.TypeGate) continue;
                        int k = st.Index(i); if (k < 0 || k >= GateSlots) continue;
                        if (k != next && k != next + 1) continue;
                        var p = st.Position(i);
                        var mark = new Vector3(p.x, p.y, p.z);
                        Place(c.gate[k], vp, mark, fwd, right, 0f, 0f, out float x);
                        float alpha = k == next ? 1f : GateAfterNextAlpha;
                        var g = c.gate[k];
                        if (g != null && g.graphic) g.graphic.color = new Color(g.baseColor.r, g.baseColor.g, g.baseColor.b, alpha);   // vf 0x80: the alpha itself
                        c.gateUp[k] = true; c.gateAlpha[k] = alpha; c.gateX[k] = x;
                        if (k == next && c.label != null)
                        {
                            float dx = mark.x - vp.x, dz = mark.z - vp.z;
                            c.labelText = Distance(Mathf.Sqrt(dx * dx + dz * dz), f.metric);
                            OriginalUiTree.SetText(c.label, c.labelText);
                            SetX(c.label, x);
                            labelUp = true;
                        }
                    }
                }
            }
            for (int i = 0; i < 10; i++) OriginalUiTree.SetVisible(c.challenge[i], c.challengeUp[i]);
            for (int i = 0; i < 16; i++) OriginalUiTree.SetVisible(c.target[i], c.targetUp[i]);
            for (int i = 0; i < GateSlots; i++) OriginalUiTree.SetVisible(c.gate[i], c.gateUp[i]);
            OriginalUiTree.SetVisible(c.finish, c.finishUp);
            OriginalUiTree.SetVisible(c.label, labelUp && c.label != null);
        }

        /// The camera's heading in degrees: the forward vector's angle from +Z, clockwise (005fd480: acos of the dot with +Z, 360 - it on the west side).
        public static float Heading(Vector3 forward)
        {
            float x = forward.x, z = forward.z, len = Mathf.Sqrt(x * x + z * z);
            if (len < 1e-4f) return 0f;
            float deg = Mathf.Acos(Mathf.Clamp(z / len, -1f, 1f)) * Mathf.Rad2Deg;
            return x >= 0f ? deg : 360f - deg;
        }

        /// A marker's signed bearing (degrees, clamped to +-45; positive on the camera's right) from the camera to a mark.
        public static float Bearing(Vector3 from, Vector3 mark, Vector3 forward, Vector3 right)
        {
            var d = new Vector3(mark.x - from.x, 0f, mark.z - from.z);
            if (d.sqrMagnitude <= 0f) return 0f;
            d.Normalize();
            float cos = Mathf.Clamp(forward.x * d.x + forward.z * d.z, -1f, 1f);
            float deg = Mathf.Min(Mathf.Acos(cos) * Mathf.Rad2Deg, MarkerClamp);
            return d.x * right.x + d.z * right.z < 0f ? -deg : deg;
        }

        // The icon at its bearing; alpha from the distance when range2 > 0 ((range^2 - d^2) / range^2 within the range, d^2 clamped to it, floored at `floor`)
        void Place(OriginalUiTree.Node icon, Vector3 from, Vector3 mark, Vector3 forward, Vector3 right, float range2, float floor, out float x)
        {
            float bearing = Bearing(from, mark, forward, right);
            x = CenterX + bearing * PixelsPerDegree;
            if (icon == null) return;
            SetX(icon, x);
            if (range2 > 0f)
            {
                float dx = mark.x - from.x, dz = mark.z - from.z, d2 = Mathf.Min(dx * dx + dz * dz, range2);
                float a = Mathf.Max(floor, 1f - d2 / range2);
                if (icon.graphic) icon.graphic.color = new Color(icon.baseColor.r, icon.baseColor.g, icon.baseColor.b, icon.baseColor.a * a);
            }
            else if (icon.graphic) icon.graphic.color = icon.baseColor;
        }

        static void SetX(OriginalUiTree.Node n, float x)
        {
            if (n == null) return;
            var p = n.rect.anchoredPosition; p.x = x; n.rect.anchoredPosition = p;
        }

        static void PlaceLetter(OriginalUiTree.Node n, float angle, float heading)
        {
            if (n == null) return;
            SetX(n, CenterX + Mathf.DeltaAngle(heading, angle) * PixelsPerDegree);
        }

        /// FUN_005dc5b0( 8, feet ): a distance in the player's units ("123 ft", "45 m"; miles / kilometres from a mile / a kilometre on).
        public static string Distance(float feet, bool metric)
        {
            var inv = CultureInfo.InvariantCulture;
            if (metric)
            {
                float m = feet * 0.3048f;
                return m >= 1000f ? (m / 1000f).ToString("0.0", inv) + " km" : Mathf.RoundToInt(m).ToString(inv) + " m";
            }
            return feet >= 5280f ? (feet / 5280f).ToString("0.0", inv) + " mi" : Mathf.RoundToInt(feet).ToString(inv) + " ft";
        }

        /// The page's components hidden at once (a menu, the other HUD style, a new activity).
        public void Hide()
        {
            foreach (var v in views.Values) { HideCounter(v); v.go.SetActive(false); }
            active = null; canvas.enabled = false;
        }

        public void Dispose() { if (host) UnityEngine.Object.Destroy(host); }
    }
}
