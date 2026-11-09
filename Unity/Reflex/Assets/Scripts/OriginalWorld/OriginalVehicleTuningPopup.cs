using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The pause page's VEHICLE TUNING (PauseMenu:OnVehicleTuning -> PopupPageStack "VehicleTuningPopup": Shared VehicleTuningPopup.lyr,
    // GarageVehicleTuning.lua; WP-G, _re/notes/garage.md), on the running world. Offered as PauseMenu.lua offers it: EventType FreeRide (or not
    // in the real world) and the equipped vehicle's owner is the player (MXTableHelper:GetEquippedVehicle owner == Player1). The values are
    // the running vehicle's (garage::world_command "tuning": the spec's values minus the model's defaults = the PlayerVehicleTuning bonuses,
    // Braking / Steering); the rows, bars, Left / Right steps of 5 bonus points and the Braking / Steering lists as GarageVehicleTuning (see
    // FrontEndGarageTuning.cs). Enter and Esc close (TravelBack); End with a change: GarageVehicleTuning:End's table writes and
    // App.ApplyVehicleTuning( device ) in the world's own Lua state, between its frames - the tuning is on the running physics from the next
    // frame. App.RequestSaveGame is left to the race's save on leaving (ReflexPersistence.Leave: RequestSaveGame and the worker's write).
    public sealed class OriginalVehicleTuningPopup : MonoBehaviour
    {
        static readonly string[] Order = { "engine", "suspension", "tires", "braking", "steering" };
        static readonly string[] Description = { "STR_TUNE_ENGINE_DESCRIPTION", "STR_TUNE_SUSPENSION_DESCRIPTION", "STR_TUNE_TIRES_DESCRIPTION", "STR_TUNE_BRAKING_DESCRIPTION", "STR_TUNE_STEERING_DESCRIPTION" };
        public static OriginalVehicleTuningPopup Current { get; private set; }
        public static bool IsOpen => Current && Current.open;

        public OriginalWorldRuntime runtime;
        Canvas canvas; OriginalUiTree tree; RectTransform root; Dictionary<string, string> loc;
        OriginalUiTree.Node layerNode, tuningList, brakingList, steeringList, description;
        readonly OriginalUiTree.Node[] rows = new OriginalUiTree.Node[5];
        readonly Dictionary<string, OriginalUiTree.Node> attributes = new Dictionary<string, OriginalUiTree.Node>();
        /// attribute -> { base1, bonus1, base2, bonus2 } (m_AttributeValues).
        public readonly Dictionary<string, int[]> values = new Dictionary<string, int[]>();
        public int focus, braking = 1, steering = 1;
        public bool Modified { get; private set; }
        bool open; Action closed;
        int playerVehicleId = -1;
        /// The tuning End wrote into the world last (tests), and the world's answer to the next "tuning 0" after it.
        public static ReflexGarage.Tuning? LastWritten; public static string LastState;

        /// PauseMenu.lua's needsVehicleTuning: Free Ride (the real world's FreeRide events; the UI world's test drive never pauses) with the
        /// player's own vehicle (the Vehicle Select owner Player1 or the profile's, not a pro's).
        public static bool Offered(OriginalWorldRuntime runtime)
        {
            if (!runtime || (!OpenReflexSelector.UseOpenReflex && !ReflexGarage.Available)) return false;
            if (OpenReflexSelector.UseOpenReflex && ReflexProfile.Current == null) return false;
            var ev = ReflexEvents.Find(runtime.level);
            if (ev == null || ev.spec.type != "FreeRide") return false;
            var g = runtime.garage;
            if (OpenReflexSelector.UseOpenReflex)
            {
                // A default launch inherits the persisted pro selection even without an explicit garage override.
                if (g != null && g.vehicleOwner >= 0) return g.vehicleOwner == ReflexWorldFiles.Garage.Player1;
                return ReflexProfile.Current.T("PlayerRider").Int("LastRiderId", 0, 0) < 1;
            }
            return g == null || g.vehicleOwner < 0 || g.vehicleOwner == ReflexWorldFiles.Garage.Player1;
        }

        /// The port's own pause page's VEHICLE TUNING row (a world whose UI is not live; a live world shows the game's own popup, mirrored).
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void Register()
        {
            OriginalPauseMenu.OnVehicleTuning = pm =>
            {
                if (!pm || !pm.runtime) return;
                pm.enabled = false;   // the pause page's keys wait while the popup is up
                if (Show(pm.runtime, () => { if (pm) pm.enabled = true; }) == null) pm.enabled = true;
            };
        }

        public static OriginalVehicleTuningPopup Show(OriginalWorldRuntime runtime, Action onClosed = null)
        {
            if (!runtime || OpenReflexSelector.UseOpenReflex && !Offered(runtime)) return null;
            var p = runtime.GetComponent<OriginalVehicleTuningPopup>() ?? runtime.gameObject.AddComponent<OriginalVehicleTuningPopup>();
            p.runtime = runtime; p.closed = onClosed;
            if (!p.canvas && !p.Build()) return null;
            p.Open();
            return p;
        }

        bool Build()
        {
            Dictionary<string, object> doc;
            try
            {   // BuildLayerOwn: the layer's own package first, its texatlas icons (IconTuningMuffler ...) and icon geometry (Ticks20) described
                var data = OriginalUiAssets.Data; loc = data.Strings("InGame", "MXRaven_Default_Strings");
                var own = data.BuildLayerOwn("Shared", "VehicleTuningPopup.lyr");
                doc = new Dictionary<string, object>
                {
                    ["name"] = "VehicleTuningPopup.lyr", ["class"] = "UILayer", ["path"] = "VehicleTuningPopup.lyr", ["children"] = own["elements"],
                    ["layout"] = new Dictionary<string, object> { ["position"] = new List<object> { 0.0, 0.0 }, ["size"] = new List<object> { 1280.0, 720.0 } },
                };
            }
            catch (Exception e) { Debug.LogWarning("[Garage] the tuning popup was not built: " + e.Message); return false; }
            var cgo = new GameObject("Original vehicle tuning popup", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
            cgo.transform.SetParent(transform, false);
            canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 60;   // over the pause page (50)
            var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
            var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
            root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
            tree = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
            layerNode = tree.Build(doc, root, null);
            OriginalHud.Complete(layerNode);   // icon geometry (the bars' Ticks20) instead of its 1x1White rectangle
            HideBareGeometry(layerNode);
            tuningList = Deep(layerNode, "TuningList"); brakingList = Deep(layerNode, "BrakingList"); steeringList = Deep(layerNode, "SteeringList"); description = Deep(layerNode, "DescriptionText");
            descriptionWidth = description != null ? description.rect.sizeDelta.x : 0;
            string[] rowNames = { "Engine_Attribute", "Suspension_Attribute", "Tires_Attribute", "BrakingContainer", "SteeringContainer" };
            for (int i = 0; i < 5; i++) rows[i] = Deep(tuningList, rowNames[i]);
            attributes["engine"] = rows[0]; attributes["suspension"] = rows[1]; attributes["tires"] = rows[2];
            canvas.enabled = false;
            return layerNode != null && tuningList != null;
        }

        float descriptionWidth;
        /// A UIIconGeometryComponent without its geometry draws nothing (the front end's rule), not its material's rectangle.
        static void HideBareGeometry(OriginalUiTree.Node n)
        {
            if (n == null) return;
            if (n.cls == "UIIconGeometryComponent" && !(n.graphic is OriginalIconGeometry) && n.graphic) n.graphic.enabled = false;
            foreach (var c in n.children) HideBareGeometry(c);
        }

        static OriginalUiTree.Node Deep(OriginalUiTree.Node n, string name)
        {
            if (n == null) return null;
            foreach (var c in n.children) { if (string.Equals(c.name, name, StringComparison.OrdinalIgnoreCase)) return c; var d = Deep(c, name); if (d != null) return d; }
            return null;
        }
        string Str(string key) => loc != null && loc.TryGetValue(key, out var v) ? v : key;

        void Open()
        {
            Current = this; open = true; canvas.enabled = true; Modified = false; focus = 0;
            Read();
            Refresh();
        }

        /// UpdateVehicleStats: the running vehicle's tuning (bars: the model's defaults + the bonuses; the Braking / Steering lists' focus).
        void Read()
        {
            foreach (var a in new[] { "engine", "suspension", "tires" }) values[a] = new int[4];
            if (OpenReflexSelector.UseOpenReflex)
            {
                // The paused worker may still own its last frame; use the managed persisted snapshot here.
                var profile = ReflexProfile.Current;
                if (profile == null) return;
                var garage = runtime.garage;
                playerVehicleId = garage != null && garage.vehicleOwner == ReflexWorldFiles.Garage.Player1 && garage.vehicleId >= 0
                    ? garage.vehicleId : profile.T("PlayerRider").Int("LastEquippedVehicle", 0, 0);
                var vehicles = profile.T("PlayerVehicle");
                int row = vehicles.Find("PlayerVehicleId", playerVehicleId);
                int model = vehicles.Int("VehicleModelId", row, -1);
                var defaults = FeTables.Shared.Rows("VehicleModel").FirstOrDefault(m => FeTables.I(m, "VehicleModelId") == model);
                var tuning = profile.T("PlayerVehicleTuning"); int tuningRow = tuning.Find("PlayerVehicleId", playerVehicleId);
                int Cell(string name) => tuning.Int(name, tuningRow, 0);
                values["engine"] = new[] { FeTables.I(defaults, "DefaultEngine1", 0), Cell("Engine1"), FeTables.I(defaults, "DefaultEngine2", 0), Cell("Engine2") };
                values["suspension"] = new[] { FeTables.I(defaults, "DefaultSuspension1", 0), Cell("Suspension1"), FeTables.I(defaults, "DefaultSuspension2", 0), Cell("Suspension2") };
                values["tires"] = new[] { FeTables.I(defaults, "DefaultTire1", 0), Cell("Tire1"), FeTables.I(defaults, "DefaultTire2", 0), Cell("Tire2") };
                braking = Mathf.Clamp(Cell("Braking") * 2 / 100, 0, 2);
                steering = Mathf.Clamp(Cell("Steering") * 2 / 100, 0, 2);
                return;
            }
            // paused: no frame runs (OriginalWorldRuntime queues none), so the world is read between frames from here once the last one is done
            string j = runtime && runtime.Running && (runtime.Simulation == null || runtime.Simulation.Idle) ? ReflexGarage.State(ReflexGarage.World) : null;
            var o = j != null ? OriginalUi.ParseJson(j) as Dictionary<string, object> : null;
            var v = OriginalUi.Arr(o, "values"); var d = OriginalUi.Arr(o, "defaults");
            if (v == null || d == null || v.Count < 8 || d.Count < 8) return;
            int I(List<object> l, int i) => l[i] is double x ? (int)x : 0;
            values["engine"] = new[] { I(d, 0), I(v, 0) - I(d, 0), I(d, 1), I(v, 1) - I(d, 1) };
            values["suspension"] = new[] { I(d, 2), I(v, 2) - I(d, 2), I(d, 3), I(v, 3) - I(d, 3) };
            values["tires"] = new[] { I(d, 4), I(v, 4) - I(d, 4), I(d, 5), I(v, 5) - I(d, 5) };
            braking = Mathf.Clamp(I(v, 6) * 2 / 100, 0, 2); steering = Mathf.Clamp(I(v, 7) * 2 / 100, 0, 2);
        }

        public ReflexGarage.Tuning Tuning => new ReflexGarage.Tuning
        {
            engine1 = values["engine"][1], engine2 = values["engine"][3], suspension1 = values["suspension"][1], suspension2 = values["suspension"][3],
            tire1 = values["tires"][1], tire2 = values["tires"][3], braking = braking * 100 / 2, steering = steering * 100 / 2,
        };

        void Update()
        {
            if (!open) return;
            float dt = Time.unscaledDeltaTime; tree.Tick(dt);
            var kb = Keyboard.current; var gp = Gamepad.current;
            bool Up() => kb != null && kb.upArrowKey.wasPressedThisFrame || gp != null && gp.dpad.up.wasPressedThisFrame;
            bool Down() => kb != null && kb.downArrowKey.wasPressedThisFrame || gp != null && gp.dpad.down.wasPressedThisFrame;
            bool Left() => kb != null && kb.leftArrowKey.wasPressedThisFrame || gp != null && gp.dpad.left.wasPressedThisFrame;
            bool Right() => kb != null && kb.rightArrowKey.wasPressedThisFrame || gp != null && gp.dpad.right.wasPressedThisFrame;
            if (Up()) Navigate(0, -1); if (Down()) Navigate(0, 1);
            if (Left()) Navigate(-1, 0); if (Right()) Navigate(1, 0);
            if (kb != null && (kb.enterKey.wasPressedThisFrame || kb.escapeKey.wasPressedThisFrame) || gp != null && (gp.buttonSouth.wasPressedThisFrame || gp.buttonEast.wasPressedThisFrame)) Close();
        }

        /// The TuningList (UpDown, no wrap), an attribute row's OnKeyDownLeft / Right_Attribute, the Braking / Steering lists (LeftRight).
        public void Navigate(int dx, int dy)
        {
            if (dy != 0) { focus = Mathf.Clamp(focus + dy, 0, 4); Refresh(); return; }
            if (dx == 0) return;
            if (focus <= 2) Move(Order[focus], dx);
            else if (focus == 3) { int b = Mathf.Clamp(braking + dx, 0, 2); if (b != braking) { braking = b; Modified = true; } }
            else { int s = Mathf.Clamp(steering + dx, 0, 2); if (s != steering) { steering = s; Modified = true; } }
            Refresh();
        }
        void Move(string a, int dir)
        {
            var v = values[a]; float max = BarWidth(attributes[a]); int step = (int)(100 / (max / 20));
            if (dir < 0 ? v[1] - step >= 0 && v[3] + step + v[2] <= 100 : v[1] + step + v[0] <= 100 && v[3] - step >= 0) { v[1] += dir * step; v[3] -= dir * step; Modified = true; }
        }
        static float BarWidth(OriginalUiTree.Node row) { var bg = Deep(Deep(row, "Attribute1"), "Background"); return bg != null ? bg.rect.sizeDelta.x : 400; }

        void Refresh()
        {
            string d = Str(Description[focus]);   // the text box wraps at its width (the front end's text rule)
            var dt = description?.text;
            if (dt != null && dt.font != null && descriptionWidth > 0) d = string.Join("\n", d.Replace("\r", "").Split('\n').Select(l => dt.font.Wrap(l, dt.textHeight, descriptionWidth)));
            OriginalUiTree.SetText(description, d);
            for (int i = 0; i < 5; i++) Colour(rows[i], i == focus);
            Items(brakingList, braking, focus == 3); Items(steeringList, steering, focus == 4);
            foreach (var a in new[] { "engine", "suspension", "tires" })
            {
                var row = attributes[a]; if (row == null) continue; var v = values[a];
                Bars(Deep(row, "Attribute1"), v[0], v[1]); Bars(Deep(row, "Attribute2"), v[2], v[3]);
            }
        }
        static void Colour(OriginalUiTree.Node n, bool focused)
        {
            if (n == null) return;
            OriginalUiTree.SetColor(n, OriginalUiTree.StateColor(n, focused, true));
            foreach (var c in n.children) if (c.name != "BrakingList" && c.name != "SteeringList") Colour(c, focused);
        }
        static void Items(OriginalUiTree.Node list, int index, bool focused)
        {   // a list container focuses its child at the focus index while it has the focus (the front end's FeLayer rule)
            if (list == null) return;
            for (int i = 0; i < list.children.Count; i++) Colour(list.children[i], focused && i == index);
        }
        /// GarageVehicleTuning:UpdateAttribute for one bar: Base at x 0 floor(base / 100 * max / step) * step wide, Bonus after it.
        static void Bars(OriginalUiTree.Node bar, int baseValue, int bonusValue)
        {
            if (bar == null) return;
            float max = Deep(bar, "Background")?.rect.sizeDelta.x ?? 400, step = max / 20;
            float bw = Mathf.Floor(baseValue / 100f * max / step) * step, nw = Mathf.Floor(bonusValue / 100f * max / step) * step;
            Place(Deep(bar, "Base"), 0, bw); Place(Deep(bar, "Bonus"), bw, nw);
        }
        static void Place(OriginalUiTree.Node n, float x, float w)
        {
            if (n == null) return;
            n.rect.sizeDelta = new Vector2(w, n.rect.sizeDelta.y);
            n.rect.anchoredPosition = new Vector2(x + n.rect.pivot.x * w, n.rect.anchoredPosition.y); n.basePos = n.rect.anchoredPosition;
            OriginalUiTree.SetVisible(n, w > 0);
        }

        /// TravelBack -> End: the table writes and App.ApplyVehicleTuning in the world when something changed.
        public void Close()
        {
            if (!open) return;
            open = false; canvas.enabled = false;
            if (Modified && runtime)
            {
                var t = Tuning; LastWritten = t;
                if (OpenReflexSelector.UseOpenReflex)
                {
                    var saved = ReflexProfile.SetTuning(playerVehicleId, t);
                    if (saved.IsCompleted && !saved.IsFaulted && !saved.IsCanceled && saved.Result)
                        runtime.ApplyOpenTuning(t);   // queued on the session owner, before the next resumed frame
                    LastState = null;
                }
                else
                {
                    string lua = "lua " + ReflexGarage.EndLua(t);
                    void Apply(IntPtr w) { if (ReflexGarage.WorldCommand(w, lua) != null) { LastState = ReflexGarage.State(w); Debug.Log("[Garage] tuning on the running vehicle (" + t + "): " + LastState); } }
                    var sim = runtime.Simulation;
                    if (sim != null) sim.BetweenFrames(Apply); else Apply(ReflexGarage.World);   // before the world's next frame
                }
            }
            if (Current == this) Current = null;
            closed?.Invoke();
        }
    }
}
