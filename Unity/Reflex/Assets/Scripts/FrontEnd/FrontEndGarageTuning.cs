// WP-G: the garage's popups and the test drive page, ported from the shipping scripts GarageVehicleTuning.lua (VehicleTuningPopup.lyr),
// VehiclesQuickViewPopup.lua (VehiclesQuickViewPopup.lyr, with its VehiclePerformanceDisplayComponent), read at run time from the install
// (the test drive: GarageHubScript, FrontEndGarage.cs). The tables are the game's: the profile
// session's Player* tables (ReflexProfile) and MXTables_Default (FeTables). Changes go through the game: the tuning popup's End runs
// GarageVehicleTuning:End's own table writes and App.ApplyVehicleTuning in the profile session's Lua state, then RequestSaveGame
// ("save"), so the next race world made from the save spawns the vehicle with it (VehicleFactory_CreateVehicle -> 00543130; garage.h).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    public static class FeGarageScripts
    {
        public static FeScript Create(string script)
        {
            switch (script)
            {
                case "GarageVehicleTuning": return new GarageVehicleTuningScript();
                case "VehiclesQuickViewPopup": return new VehiclesQuickViewPopupScript();
                default: return null;
            }
        }
        /// GetComponent_UIContainer( name ) is case-blind in the engine ("engine_Attribute" finds "Engine_Attribute").
        public static FeNode FindCI(FeNode n, string name)
        {
            if (n == null) return null;
            foreach (var k in n.kids) { if (string.Equals(k.name, name, StringComparison.OrdinalIgnoreCase)) return k; var d = FindCI(k, name); if (d != null) return d; }
            return null;
        }
    }

    // GarageVehicleTuning.lua (PopupPageStack "VehicleTuningPopup", from GarageMyRides:OnEnableVehicleTuning and the pause page's VEHICLE
    // TUNING): the TuningList (engine / suspension / tires / braking / steering, the description STR_TUNE_<x>_DESCRIPTION of the focused
    // row), the attribute rows' bars (VehicleModel Default<attr>1/2 as the base, the PlayerVehicleTuning row of PlayerRider.LastEquippedVehicle
    // as the bonus), Left / Right on an attribute row move 5 bonus points (100 / (bar width / step) with 20 steps) between its two values
    // within base + bonus <= 100 (MapKey_UI_Left / Right triggers OnKeyDownLeft / Right_Attribute), the Braking / Steering lists (LeftRight:
    // 0 / 50 / 100 = Braking / 100 * 2). Accept and Back both close (TravelBack); End writes the row when anything changed (Back too), calls
    // App.ApplyVehicleTuning and RequestSaveGame, then App.SetUIVariableInt( "VehicleTuningComplete", TRUE ). OnResetKeypress does nothing.
    public sealed class GarageVehicleTuningScript : FeScript
    {
        static readonly string[] Order = { "engine", "suspension", "tires", "braking", "steering" };
        static readonly string[] Description = { "STR_TUNE_ENGINE_DESCRIPTION", "STR_TUNE_SUSPENSION_DESCRIPTION", "STR_TUNE_TIRES_DESCRIPTION", "STR_TUNE_BRAKING_DESCRIPTION", "STR_TUNE_STEERING_DESCRIPTION" };
        FeNode tuningList, brakingList, steeringList, description;
        readonly Dictionary<string, FeNode> attributes = new Dictionary<string, FeNode>();
        readonly Dictionary<string, float> stepPx = new Dictionary<string, float>();
        /// m_AttributeValues: attribute -> { base1, bonus1, base2, bonus2 }.
        public readonly Dictionary<string, int[]> values = new Dictionary<string, int[]>();
        public bool Modified { get; private set; }
        int vehicleId = -1;
        /// The values End wrote last (tests).
        public static ReflexGarage.Tuning? LastWritten;

        public override void Initialize()
        {
            tuningList = C("TuningList"); brakingList = C("BrakingList"); steeringList = C("SteeringList"); description = C("DescriptionText");
            foreach (var a in new[] { "engine", "suspension", "tires" })
            {
                var c = FeGarageScripts.FindCI(layer.root, a + "_Attribute"); attributes[a] = c;
                float max = c?.Kid("Attribute1")?.Kid("Background")?.size.x ?? 400;   // background:GetWidth( ) of the first bar
                stepPx[a] = max / 20;
            }
            functions["OnAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("PopupPageStack_Pop"); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("PopupPageStack_Pop"); };
            functions["OnResetKeypress"] = () => { };
            functions["OnKeyDownLeft_Attribute"] = () => Move(-1);
            functions["OnKeyDownRight_Attribute"] = () => Move(+1);
            if (tuningList != null) tuningList.onListChanged = (l, old) => OnTuningListFocusedChanged();
            if (brakingList != null) brakingList.onListChanged = (l, old) => Modified = true;   // OnBrakingListFocusedChanged
            if (steeringList != null) steeringList.onListChanged = (l, old) => Modified = true;
        }

        public override void Start()
        {
            fe.PlaySound("PopupShow");
            foreach (var a in new[] { "engine", "suspension", "tires" }) { values[a] = new int[4]; Update(a); }
            var p = ReflexProfile.Current;
            vehicleId = p?.T("PlayerRider").Int("LastEquippedVehicle", 0, 0) ?? -1;
            UpdateVehicleStats(vehicleId);
            Modified = false;
            if (tuningList != null) tuningList.focusIndex = 0;
            OnTuningListFocusedChanged();
            KeyGlyph.Refresh(fe, layer.root);   // BackImage's KeyMap glyph
        }

        public override void End()
        {
            if (Modified && ReflexProfile.Current != null)
            {
                var t = Current;
                // GarageVehicleTuning:End: the row's cells, App.ApplyVehicleTuning( device ), App.RequestSaveGame( device )
                var changed = ReflexProfile.SetTuning(vehicleId, t);
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !changed.Result) return;
                LastWritten = t;
                Debug.Log("[Garage] vehicle tuning of PlayerVehicleId " + vehicleId + ": " + t);
            }
            fe.vars["VehicleTuningComplete"] = 1;   // App.SetUIVariableInt( "VehicleTuningComplete", TRUE )
        }

        /// The eight values End writes: the bonuses, Braking / Steering = focused index / 2 * 100.
        public ReflexGarage.Tuning Current => new ReflexGarage.Tuning
        {
            engine1 = values["engine"][1], engine2 = values["engine"][3], suspension1 = values["suspension"][1], suspension2 = values["suspension"][3],
            tire1 = values["tires"][1], tire2 = values["tires"][3],
            braking = (brakingList?.focusIndex ?? 1) * 100 / 2, steering = (steeringList?.focusIndex ?? 1) * 100 / 2,
        };

        void Update(string a) { var v = values[a]; FeVehiclePerformance.Bars(attributes.GetValueOrDefault(a), v[0], v[2], v[1], v[3]); }

        void OnTuningListFocusedChanged()
        {
            int i = Mathf.Clamp(tuningList?.focusIndex ?? 0, 0, Description.Length - 1);
            description?.SetText(Str(Description[i]));
        }

        /// OnKeyDownLeft_Attribute (-1) / OnKeyDownRight_Attribute (+1) on the focused attribute row.
        public void Move(int dir)
        {
            int row = tuningList?.focusIndex ?? 0; if (row < 0 || row > 2) return;
            string a = Order[row]; var v = values[a];
            int step = (int)(100 / stepPx[a]);   // 100 / attributeInfo[ 1 ]
            int base1 = v[0], bonus1 = v[1], base2 = v[2], bonus2 = v[3];
            if (dir < 0 ? bonus1 - step >= 0 && bonus2 + step + base2 <= 100 : bonus1 + step + base1 <= 100 && bonus2 - step >= 0)
            {
                v[1] = bonus1 + dir * step; v[3] = bonus2 - dir * step;
                Update(a); Modified = true; fe.PlaySound("LeftRightDirectional");
            }
        }

        /// UpdateVehicleStats( vehicleId, device ): the base of the vehicle's model, the bonus of its PlayerVehicleTuning row, the
        /// Braking / Steering lists' focus.
        void UpdateVehicleStats(int id)
        {
            var p = ReflexProfile.Current; if (p == null) return;
            var pv = p.T("PlayerVehicle"); int model = pv.Int("VehicleModelId", pv.Find("PlayerVehicleId", id));
            var tt = p.T("PlayerVehicleTuning"); int row = tt.Find("PlayerVehicleId", id);
            int Cell(string c) => tt.Int(c, row, 0);
            if (brakingList != null) brakingList.focusIndex = Mathf.Clamp(Cell("Braking") * 2 / 100, 0, 2);   // SetFocusedComponent( Braking / 100 * 2 )
            if (steeringList != null) steeringList.focusIndex = Mathf.Clamp(Cell("Steering") * 2 / 100, 0, 2);
            var m = FeTables.Shared.Rows("VehicleModel").FirstOrDefault(r => FeTables.I(r, "VehicleModelId") == model);
            if (m == null) return;
            values["engine"] = new[] { FeTables.I(m, "DefaultEngine1", 0), Cell("Engine1"), FeTables.I(m, "DefaultEngine2", 0), Cell("Engine2") };
            values["suspension"] = new[] { FeTables.I(m, "DefaultSuspension1", 0), Cell("Suspension1"), FeTables.I(m, "DefaultSuspension2", 0), Cell("Suspension2") };
            values["tires"] = new[] { FeTables.I(m, "DefaultTire1", 0), Cell("Tire1"), FeTables.I(m, "DefaultTire2", 0), Cell("Tire2") };
            foreach (var a in new[] { "engine", "suspension", "tires" }) Update(a);
        }
    }

    // VehiclesQuickViewPopup.lua (PopupPageStack "VehiclesQuickViewPopup": GarageMyRides' toggle, Race and Ride Events / Challenges with
    // QuickViewSeriesId, the pause page's quick view): the vehicles to show by type (the series' VehicleClass with QuickViewSeriesId; none =
    // all in a tour / career; else the UI's VehicleClass and the pro riders allowed), the player's (PlayerRider.MotocardSetup) PlayerVehicle
    // rows - all of them sorted by VehicleModelId.VehicleTypeId, or per type; none found: the pro riders' instead - or the PlayerSelectable
    // AIRiders' AIRiderVehicle rows per type; columns STR_VEHICLE_TYPE / STR_VEHICLE_NAME (player) or STR_PRO_RIDER / STR_VEHICLE_TYPE;
    // Toggle (STR_PRO_RIDERS / STR_MYRIDES) when both are allowed; the performance display of the focused vehicle. Accept: the UI variable
    // QuickViewPlayerVehicleId (or QuickViewAIRiderId / QuickViewAIVehicleId), then TravelBack; End: VehicleQuickViewComplete.
    public sealed class VehiclesQuickViewPopupScript : FeScript
    {
        public sealed class Item { public string typeLoc, rider; public int vehicleId, modelId, product, aiRider = -1; }
        public readonly List<Item> items = new List<Item>();
        List<int> validTypes = new List<int>();
        FeNode list, column0, column1, toggle, perfNode; FeVehiclePerformance perf;
        public bool ShowPlayerVehicles { get; private set; }
        bool allowPlayer, allowAI; int top;
        public FeVehiclePerformance Performance => perf;
        public int Index => list?.absIndex ?? 0;

        public override void Initialize()
        {
            list = C("QuickList"); perfNode = C("VehiclePerformanceDisplay.component");
            var header = C("Header"); column0 = header?.Find("Column0Text"); column1 = header?.Find("Column1Text");
            toggle = C("Toggle");
            functions["OnQuickListAccept"] = OnQuickListAccept;
            functions["OnBackKeypress"] = () => { fe.SendUIEvent("PopupPageStack_Pop"); fe.PlaySound("Back"); };
            functions["OnToggleVehicles"] = OnToggleVehicles;
            if (list != null) list.onListChanged = (l, old) => OnVehicleListFocusedChanged();
        }

        public override void Start()
        {
            fe.PlaySound("PopupShow");
            allowAI = false; fe.vars["SuspendPlayerInput"] = 1;
            var p = ReflexProfile.Current;
            allowPlayer = p != null && p.MotocardSetup;
            ShowPlayerVehicles = allowPlayer;
            perf = perfNode != null ? new FeVehiclePerformance(perfNode, FeTables.Shared, p) : null;
            BuildVehicleTypes();
            KeyGlyph.Refresh(fe, layer.root);   // the Back / Toggle KeyMap glyphs
        }

        public override void End()
        {
            fe.vars["SuspendPlayerInput"] = 0;   // the game is not paused (the front end)
            fe.vars["VehicleQuickViewComplete"] = 1;
        }

        void BuildVehicleTypes()
        {
            validTypes = new List<int>();
            if (fe.vars.TryGetValue("QuickViewSeriesId", out var so) && so is int series)
            {   // the series' vehicle class
                int cls = FeTables.I(FeTables.Shared.Rows("Series").FirstOrDefault(r => FeTables.I(r, "SeriesId") == series), "VehicleClassId");
                allowAI = false; validTypes = TypesOf(cls);
            }
            else
            {
                // seriesManager:IsTourMode( ) or InCareerMode (the garage is reached from MOTOCAREER): the player's vehicles only
                if (!fe.Var("InCareerMode", false)) { allowAI = true; validTypes = TypesOf(fe.Var("VehicleClass", -1)); }
            }
            BuildVehicles();
            if (ShowPlayerVehicles) { column0?.SetText(Str("STR_VEHICLE_TYPE")); column1?.SetText(Str("STR_VEHICLE_NAME")); }
            else { column0?.SetText(Str("STR_PRO_RIDER")); column1?.SetText(Str("STR_VEHICLE_TYPE")); }
            if (toggle != null)
            {
                toggle.Visible = allowAI && (ShowPlayerVehicles || allowPlayer);
                toggle.Find("Text")?.SetText(Str(ShowPlayerVehicles ? "STR_PRO_RIDERS" : "STR_MYRIDES"));
            }
        }

        static List<int> TypesOf(int vehicleClass) =>
            FeTables.Shared.Rows("VehicleClassType").Where(r => FeTables.I(r, "VehicleClassId") == vehicleClass).Select(r => FeTables.I(r, "VehicleTypeId")).OrderBy(x => x).ToList();

        void BuildVehicles()
        {
            items.Clear();
            var S = FeTables.Shared; var p = ReflexProfile.Current;
            Dictionary<string, object> Model(int id) => S.Rows("VehicleModel").FirstOrDefault(r => FeTables.I(r, "VehicleModelId") == id);
            string TypeLoc(Dictionary<string, object> m) => FeTables.S(S.Rows("VehicleType").FirstOrDefault(r => FeTables.I(r, "VehicleTypeId") == FeTables.I(m, "VehicleTypeId")), "Name");
            if (ShowPlayerVehicles && p != null)
            {
                var pv = p.T("PlayerVehicle");
                var rows = Enumerable.Range(0, pv.rows).Select(i => (id: pv.Int("PlayerVehicleId", i), model: pv.Int("VehicleModelId", i)))
                           .Select(x => (x.id, x.model, m: Model(x.model))).Where(x => x.m != null).ToList();
                void Add((int id, int model, Dictionary<string, object> m) x) => items.Add(new Item { vehicleId = x.id, modelId = x.model, typeLoc = TypeLoc(x.m), product = FeGarageData.ProductOfAsset(FeTables.I(x.m, "StoreAssetId")) });
                if (validTypes.Count == 0) foreach (var x in rows.Select((x, i) => (x, i)).OrderBy(q => FeTables.I(q.x.m, "VehicleTypeId")).ThenBy(q => q.i).Select(q => q.x)) Add(x);
                else
                {
                    foreach (int t in validTypes) foreach (var x in rows.Where(x => FeTables.I(x.m, "VehicleTypeId") == t)) Add(x);
                    if (items.Count == 0) { ShowPlayerVehicles = false; allowPlayer = false; BuildVehicles(); return; }   // no player vehicle: the AI's
                }
            }
            else
            {
                var riders = S.Rows("AIRider").Where(r => FeTables.I(r, "PlayerSelectable", 0) != 0).Select(r => FeTables.I(r, "AIRiderId")).ToList();
                if (p != null && p.EventStatus("CC_raceorange") != 0) riders.Add(6);   // the cheat rider
                foreach (int rider in riders)
                    foreach (int t in validTypes)
                        foreach (var r in S.Rows("AIRiderVehicle").Where(r => FeTables.I(r, "AIRiderId") == rider && FeTables.I(r, "VehicleTypeId") == t))
                        {
                            int id = FeTables.I(r, "AIVehicleId"), model = FeTables.I(S.Rows("AIVehicle").FirstOrDefault(v => FeTables.I(v, "AIVehicleId") == id), "VehicleModelId");
                            var m = Model(model); if (m == null) continue;
                            items.Add(new Item { vehicleId = id, modelId = model, typeLoc = TypeLoc(m), product = FeGarageData.ProductOfAsset(FeTables.I(m, "StoreAssetId")), aiRider = rider,
                                                 rider = FeTables.S(S.Rows("AIRider").FirstOrDefault(a => FeTables.I(a, "AIRiderId") == rider), "DisplayName") });
                        }
            }
            if (list != null) { list.numItems = items.Count; list.absIndex = 0; }
            top = 0;
            OnVehicleListFocusedChanged();
        }

        void Format()
        {
            if (list == null) return;
            var slots = list.kids.Where(k => k.name.StartsWith("Item", StringComparison.Ordinal)).OrderBy(k => k.pos.y).ToList();
            int n = slots.Count, i = list.absIndex, count = items.Count;
            if (i < top) top = i; if (i >= top + n) top = i - n + 1; top = Mathf.Clamp(top, 0, Math.Max(0, count - n));
            for (int s = 0; s < n; s++)
            {
                int d = top + s; bool show = d < count; slots[s].Visible = show; if (!show) continue;
                var it = items[d];
                if (ShowPlayerVehicles) { slots[s].Find("Column0Text")?.SetText(Str(it.typeLoc ?? "")); slots[s].Find("Column1Text")?.SetText(FeGarageData.Title(fe, it.product)); }
                else { slots[s].Find("Column0Text")?.SetText(it.rider ?? ""); slots[s].Find("Column1Text")?.SetText(Str(it.typeLoc ?? "")); }
            }
            list.focusIndex = n == 0 ? -1 : list.kids.IndexOf(slots[Mathf.Clamp(i - top, 0, n - 1)]);
        }

        void OnVehicleListFocusedChanged()
        {
            Format();
            if (perf == null || items.Count == 0) return;
            var it = items[Mathf.Clamp(list.absIndex, 0, items.Count - 1)];
            if (ShowPlayerVehicles) perf.UpdateVehicleStats(it.vehicleId, ReflexWorldFiles.Garage.Player1);
            else perf.UpdateVehicleStats(it.vehicleId, ReflexWorldFiles.Garage.AI);
        }

        void OnQuickListAccept()
        {
            if (items.Count > 0)
            {
                var it = items[Mathf.Clamp(list.absIndex, 0, items.Count - 1)];
                if (ShowPlayerVehicles) fe.vars["QuickViewPlayerVehicleId"] = it.vehicleId;
                else { fe.vars["QuickViewAIRiderId"] = it.aiRider; fe.vars["QuickViewAIVehicleId"] = it.vehicleId; }
            }
            fe.SendUIEvent("PopupPageStack_Pop"); fe.PlaySound("Accept");
        }

        void OnToggleVehicles()
        {
            if (!(allowPlayer && allowAI)) return;
            fe.PlaySound("Accept"); ShowPlayerVehicles = !ShowPlayerVehicles;
            BuildVehicleTypes();
        }
    }
}
