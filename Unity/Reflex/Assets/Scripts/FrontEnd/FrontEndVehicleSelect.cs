// The arcade Vehicle Select page (FrontEndPageGraph "Vehicle Select Page" = Background01 + ArcadeBackground + VehicleSelect.lyr):
// C# port of VehicleSelect.lua, its VehiclePerformanceDisplayComponent.lua (the "VehiclePerformanceDisplay.component" of the
// layer) and VehicleSelect_Cameras.lua, over the install's MXTables_Default (the tables the Lua reads through App.Lookup /
// LookupPlayerTable). The choice reaches the race as VehicleSelect:End makes it: MXTableHelper EquipVehicle / EquipRider on the
// local player's row, which the race scene's world build does inside the running game (ReflexWorldFiles.Garage ->
// ReflexEventSpec -> Boot::start_event; _re/notes/vehicle-select.md).
//
// The garage's 3D preview (MXGarage Load / RequestRider / RequestVehicle into the layer's RenderTarget image) is drawn by
// FeGarage: the chosen bike (ReflexBikeModel) and rider (ReflexRiderModel, bind pose; the garage's rider animation
// SC_RU_HC_Anim_NoLoop_01 is not played) built at run time from the install, through VehicleSelect_Cameras' camera for the type.
// Port rules (no counterpart in the port): no profile is signed in, so the player's own rider ("MY RIDES", the default
// profile's PlayerRider / PlayerVehicle) is offered as if its motocard were set up (else the default bike could not be chosen);
// vehicle types the port cannot draw (a factory class the game builds nothing for) are listed but locked; motocross bikes, ATVs and the
// machines (UTV, Super Buggy, Pro Lite / Pro 2 trucks: ReflexVehicles) are offered;
// splitscreen and the mouse handlers are not ported.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    /// The install's MXTables_Default as a ReflexGameTables source (the vehicle / rider specs of the preview).
    public sealed class FeTableSource : ReflexGameTables.ISource
    {
        readonly FeTables t;
        public FeTableSource(FeTables t) { this.t = t; }
        public int Rows(string table) { var r = t.Rows(table); return r.Count == 0 ? -1 : r.Count; }
        public int[] Ints(string table, string column)
        {
            var rows = t.Rows(table); if (rows.Count == 0 || !rows[0].ContainsKey(column)) return null;
            return rows.Select(r => FeTables.I(r, column)).ToArray();
        }
        public string[] Strings(string table, string column)
        {
            var rows = t.Rows(table); if (rows.Count == 0 || !rows[0].ContainsKey(column)) return null;
            return rows.Select(r => FeTables.S(r, column)).ToArray();
        }
    }

    public sealed class VehicleSelectScript : FeScript
    {
        // MXRiderOwner / MXVehicleOwner (0x9dcf30 / 0x9dceec): App.GetMXRiderOwner( "Player1" ) / ( "AI" )
        const int OwnerPlayer1 = ReflexWorldFiles.Garage.Player1, OwnerAI = ReflexWorldFiles.Garage.AI;
        const bool MotocardSetup = true;   // see the header: the port's default rider counts as set up

        public sealed class RiderItem { public int id; public string display, name, number; public int gearSet; }   // id -1: the player
        public sealed class TypeItem { public int id; public string loc; public bool locked; }
        public sealed class VehicleItem { public int id, storeAsset, modelId; }

        public readonly List<RiderItem> riders = new List<RiderItem>();
        public readonly List<TypeItem> types = new List<TypeItem>();
        public readonly List<VehicleItem> vehicles = new List<VehicleItem>();
        List<TypeItem> available = new List<TypeItem>();
        FeNode riderRoot, riderList, typeRoot, typeList, vehicleRoot, vehicleList, selectedType, perf, brand, renderTarget;
        bool riderActive = true, typeActive, vehicleActive, suppress, arcadeAccepted, loading;
        int typeTop, vehicleTop;
        float riderScroll = 1, riderScrollFrom, riderScrollTime = 0.15f; int riderScrollOther = -1;
        FeTables T => fe.assets.tables;
        public FeGarage garage;
        public int RiderIndex => riderList.absIndex;
        public int TypeIndex => typeList.absIndex;
        public int VehicleIndex => vehicleList.absIndex;
        public bool RiderListActive => riderActive;
        public bool TypeListActive => typeActive;
        public bool VehicleListActive => vehicleActive;

        public override void Initialize()
        {
            riderRoot = C("RiderList"); riderList = riderRoot.Find("NavListContainer");
            typeRoot = C("VehicleTypeList"); typeList = typeRoot.Find("NavListContainer");
            selectedType = C("SelectedVehicleTypeContainer");
            vehicleRoot = C("VehicleList"); vehicleList = vehicleRoot.Find("NavListContainer");
            perf = C("VehiclePerformanceDisplay.component");
            brand = C("BrandImageIcon"); renderTarget = C("RenderTarget");
            riderScrollTime = OriginalUi.F(riderList.props, "TimeToScroll", 0.15f);
            functions["OnAcceptRiderList"] = OnAcceptRiderList; functions["OnCancelRiderList"] = OnCancelRiderList;
            functions["OnAcceptVehicleTypeList"] = OnAcceptVehicleTypeList; functions["OnCancelVehicleTypeList"] = OnCancelVehicleTypeList;
            functions["OnAcceptVehicleList"] = OnAcceptVehicleList; functions["OnCancelVehicleList"] = OnCancelVehicleList;
            functions["MouseReActivation"] = () => { };
            riderList.onListChanged = (l, old) => { StartRiderScroll(l.absIndex - old, old); OnRiderListFocusedChanged(); };
            typeList.onListChanged = (l, old) => OnVehicleTypeListFocusedChanged();
            vehicleList.onListChanged = (l, old) => OnVehicleListFocusedChanged();
            fe.vars["Player1VehicleSelected"] = 0; fe.vars["Player2VehicleSelected"] = 0;
            if (renderTarget?.image) { renderTarget.image.texture = null; renderTarget.Visible = false; }   // until the garage renders
        }

        public override void Start()
        {
            riderActive = true; typeActive = false; vehicleActive = false;
            fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetHeader("STR_VEHICLE_SELECT");
            suppress = true; loading = false;
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) ReflexProfile.Open();
            perfDisplay = null;
            if (garage == null) { try { garage = new FeGarage(fe.transform); } catch (Exception e) { Debug.LogWarning("[FrontEnd] vehicle select garage: " + e.Message); } }
            if (garage != null && renderTarget?.image) { renderTarget.image.texture = garage.Texture; renderTarget.image.uvRect = new Rect(0, 0, 1, 1); renderTarget.image.Dirty(); renderTarget.Visible = true; }
            BuildValidVehicleTypes();
            arcadeAccepted = false;
            riderRoot.focusedFlag = true;
            typeRoot.focusedFlag = false; typeRoot.Visible = false;
            vehicleRoot.focusedFlag = false; vehicleRoot.Visible = false;
            selectedType.Visible = false;
            PopulateRiderList();
            // select the last equipped vehicle of the player (MXTableHelper GetEquippedVehicle / GetEquippedRider: this session's
            // last choice, else the profile's PlayerRider LastRiderId / LastEquippedVehicle)
            bool needRequest = true;
            var last = fe.Var<ReflexWorldFiles.Garage>("Garage");
            int riderId, vehicleId;
            if (last != null && last.riderOwner >= 0) { riderId = last.riderOwner == OwnerAI ? last.riderId : -1; vehicleId = last.vehicleId; }
            else { var pr = ProfileRows("PlayerRider").FirstOrDefault(); int lr = FeTables.I(pr, "LastRiderId", 0); riderId = lr < 1 ? -1 : lr; vehicleId = FeTables.I(pr, "LastEquippedVehicle", 0); }
            int ri = riders.FindIndex(r => r.id == riderId);
            if (ri >= 0)
            {
                riderList.absIndex = ri;
                int vt = riderId == -1 ? VehicleTypeOfModel(FeTables.I(Unique("PlayerVehicle", "PlayerVehicleId", vehicleId), "VehicleModelId"))
                                       : FeTables.I(T.Rows("AIRiderVehicle").FirstOrDefault(r => FeTables.I(r, "AIRiderId") == riderId && FeTables.I(r, "AIVehicleId") == vehicleId), "VehicleTypeId");
                if (vt != -1)
                {
                    PopulateVehicleTypeList();
                    int ti = types.FindIndex(x => x.id == vt);
                    if (ti >= 0)
                    {
                        typeList.absIndex = ti;
                        PopulateVehicleList();
                        suppress = false;
                        int vi = vehicles.FindIndex(x => x.id == vehicleId);
                        if (vi >= 0) { vehicleList.absIndex = vi; FormatVehicleList(); RequestNewVehicle(true, true); needRequest = false; }
                    }
                }
            }
            // "Make sure the list updates" (Lua): the rider's types are listed again from the first (only the rider is kept)
            OnRiderListFocusedChanged();
            suppress = false;
            if (needRequest) RequestNewVehicle(true, true);
            FormatAll();
        }

        public override void End()
        {
            // mxGarage:Unload( ): the preview models, its camera and render texture go (made again by the next Start)
            garage?.Destroy(); garage = null;
            if (renderTarget?.image) { renderTarget.image.texture = null; renderTarget.Visible = false; }
            if (arcadeAccepted && riders.Count > 0 && vehicles.Count > 0)
            {   // EquipVehicle( row, vehicleId, owner ) / EquipRider( row, riderId, gearSetId, owner ): handed to the race's world build
                var r = riders[riderList.absIndex]; var v = vehicles[vehicleList.absIndex];
                var g = new ReflexWorldFiles.Garage { vehicleTypeId = fe.Var("Player1VehicleTypeId", -1) };
                if (r.id == -1) { g.riderOwner = OwnerPlayer1; g.riderId = 0; g.riderGearSet = 0; g.vehicleOwner = OwnerPlayer1; g.vehicleId = v.id; }
                else { g.riderOwner = OwnerAI; g.riderId = r.id; g.riderGearSet = r.gearSet; g.vehicleOwner = OwnerAI; g.vehicleId = v.id; }
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !ReflexProfile.SelectRider(r.id < 0 ? 0 : r.id, r.gearSet, v.id).Result)
                    Debug.LogWarning("[Garage] rider selection was not saved: " + ReflexProfile.Error);
                fe.vars["Garage"] = g;
            }
        }

        public override void Update(float dt)
        {
            if (riderScroll < 1)
            {
                riderScroll = Mathf.Min(1, riderScroll + dt / Mathf.Max(0.01f, riderScrollTime));
                float e = 1 - Mathf.Pow(1 - riderScroll, 5);   // quintic ease-out
                riderList.aPos.x = riderScrollFrom * (1 - e);
                if (riderScroll >= 1) { riderScrollOther = -1; FormatRiderList(); }
            }
            garage?.Update(dt);
        }

        // ---------------------------------------------------------------------------------------- tables
        IEnumerable<Dictionary<string, object>> ProfileRows(string table)
        {
            var p = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex ? ReflexProfile.Current : null;
            if (p == null || !table.StartsWith("Player", StringComparison.Ordinal)) return T.Rows(table);
            var t = p.T(table); var rows = new List<Dictionary<string, object>>();
            for (int r = 0; r < t.rows; r++)
            {
                var row = new Dictionary<string, object>();
                foreach (var c in t.columns) if (r < c.Value.Count) row[c.Key] = c.Value[r];
                rows.Add(row);
            }
            return rows;
        }
        Dictionary<string, object> Unique(string table, string col, int v) => ProfileRows(table).FirstOrDefault(r => FeTables.I(r, col) == v);
        int VehicleTypeOfModel(int modelId) => FeTables.I(Unique("VehicleModel", "VehicleModelId", modelId), "VehicleTypeId");
        IEnumerable<Dictionary<string, object>> PlayerVehiclesOfType(int type) => ProfileRows("PlayerVehicle").Where(r => VehicleTypeOfModel(FeTables.I(r, "VehicleModelId")) == type);
        IEnumerable<Dictionary<string, object>> AiVehiclesOfType(int rider, int type) => T.Rows("AIRiderVehicle").Where(r => FeTables.I(r, "AIRiderId") == rider && FeTables.I(r, "VehicleTypeId") == type);
        /// MXTableHelper:StoreProductFromStoreAsset (StoreAssets).
        int StoreProductOf(int storeAsset) => FeTables.I(Unique("StoreAssets", "StoreAssetId", storeAsset), "StoreProductId");
        string TitleOf(int storeAsset) { string k = T.TitleKey(StoreProductOf(storeAsset)); return k == null ? "" : Str(k); }

        // ---------------------------------------------------------------------------------------- list generation
        static readonly int[] TypeSort = { 2, 3, 4, 11, 14, 12, 15 };
        void BuildValidVehicleTypes()
        {
            int eventTypeId = fe.Var("EventTypeId", 10);
            var classes = T.Rows("EventTypeVehicleClasses").Where(r => FeTables.I(r, "EventTypeId") == eventTypeId).Select(r => FeTables.I(r, "VehicleClassId"))
                           .Where(c => FeTables.S(Unique("VehicleClass", "VehicleClassId", c), "Format") == "Single").ToList();
            var list = new List<TypeItem>();
            foreach (int c in classes)
                foreach (var ct in T.Rows("VehicleClassType").Where(r => FeTables.I(r, "VehicleClassId") == c))
                {
                    var vt = Unique("VehicleType", "VehicleTypeId", FeTables.I(ct, "VehicleTypeId"));
                    if (vt == null) continue;
                    list.Add(new TypeItem { id = FeTables.I(vt, "VehicleTypeId"), loc = FeTables.S(vt, "Name"), locked = !ReflexVehicles.Drawable(FeTables.I(vt, "VehicleTypeId")) });
                }
            int Rank(TypeItem x) { int i = Array.IndexOf(TypeSort, x.id); return i < 0 ? 1000 : i; }
            available = list.Select((x, i) => (x, i)).OrderBy(p => Rank(p.x)).ThenBy(p => p.i).Select(p => p.x).ToList();
        }

        void PopulateRiderList()
        {
            riders.Clear();
            bool addPlayer = MotocardSetup && available.Any(a => PlayerVehiclesOfType(a.id).Any());
            if (addPlayer) riders.Add(new RiderItem { id = -1 });
            foreach (var r in T.Rows("AIRider").Where(r => FeTables.I(r, "PlayerSelectable", 0) != 0))   // no CC_allai / CC_raceorange cheats entered
                riders.Add(new RiderItem { id = FeTables.I(r, "AIRiderId"), display = FeTables.S(r, "DisplayName"), gearSet = FeTables.I(r, "DefaultRiderGearSetId"), name = FeTables.S(r, "Name"), number = FeTables.S(r, "Number") });
            // VehicleSelect_SortRiders: the player (no name) first, then by display name
            var sorted = riders.Select((x, i) => (x, i)).OrderBy(p => p.x.display == null ? 0 : 1).ThenBy(p => p.x.display ?? "", StringComparer.Ordinal).ThenBy(p => p.i).Select(p => p.x).ToList();
            riders.Clear(); riders.AddRange(sorted);
            riderList.numItems = riders.Count; riderList.absIndex = 0; riderList.focusIndex = 0;
            OnRiderListFocusedChanged();
        }

        void PopulateVehicleTypeList()
        {
            types.Clear();
            int rider = riders.Count > 0 ? riders[riderList.absIndex].id : -1;
            foreach (var a in available)
                if (rider == -1 ? PlayerVehiclesOfType(a.id).Any() : AiVehiclesOfType(rider, a.id).Any()) types.Add(a);
            typeList.numItems = types.Count; typeList.absIndex = 0;
            OnVehicleTypeListFocusedChanged();
        }

        void PopulateVehicleList()
        {
            vehicles.Clear();
            if (types.Count > 0)
            {
                int type = types[typeList.absIndex].id, rider = riders[riderList.absIndex].id;
                if (rider == -1)
                    foreach (var r in PlayerVehiclesOfType(type)) { int m = FeTables.I(r, "VehicleModelId"); vehicles.Add(new VehicleItem { id = FeTables.I(r, "PlayerVehicleId"), modelId = m, storeAsset = FeTables.I(Unique("VehicleModel", "VehicleModelId", m), "StoreAssetId") }); }
                else
                    foreach (var r in AiVehiclesOfType(rider, type))
                    {
                        int id = FeTables.I(r, "AIVehicleId"), m = FeTables.I(Unique("AIVehicle", "AIVehicleId", id), "VehicleModelId");
                        vehicles.Add(new VehicleItem { id = id, modelId = m, storeAsset = FeTables.I(Unique("VehicleModel", "VehicleModelId", m), "StoreAssetId") });
                    }
                // VehicleSelect_VehicleListSort: by the store product's title
                var sorted = vehicles.Select((x, i) => (x, i)).OrderBy(p => TitleOf(p.x.storeAsset), StringComparer.Ordinal).ThenBy(p => p.i).Select(p => p.x).ToList();
                vehicles.Clear(); vehicles.AddRange(sorted);
            }
            vehicleList.numItems = vehicles.Count; vehicleList.absIndex = 0;
            OnVehicleListFocusedChanged();
        }

        // ---------------------------------------------------------------------------------------- formatting
        static void SetItemText(FeNode item, string s) { var t = item?.Find("Text"); t?.SetText(s); }

        void FormatAll() { FormatRiderList(); FormatTypeList(); FormatVehicleList(); }

        // OnFormatRiderList: the scrolling container shows the focused rider in Item0 (the next one waits in Item1, behind the mask)
        void FormatRiderList()
        {
            var item0 = riderList.Kid("Item0"); var item1 = riderList.Kid("Item1");
            int i = riderList.absIndex;
            SetItemText(item0, RiderText(i));
            if (item1 != null)
            {
                int other = riderScrollOther >= 0 ? riderScrollOther : i + 1;
                item1.Visible = other >= 0 && other < riders.Count;
                SetItemText(item1, RiderText(other));
                item1.aPos.x = riderScrollOther >= 0 && riderScrollOther < i ? -2 * item1.pos.x : 0;   // an old item to the left slides out there
            }
            riderList.focusIndex = 0;
            var left = riderRoot.Kid("LeftArrow"); var right = riderRoot.Kid("RightArrow");
            if (left != null) left.Visible = i > 0;
            if (right != null) right.Visible = i < riders.Count - 1;
        }
        string RiderText(int i) => i < 0 || i >= riders.Count ? "" : riders[i].id == -1 ? Str("STR_MYRIDES") : riders[i].display;
        void StartRiderScroll(int delta, int old)
        {
            if (delta == 0) return;
            float spacing = riderList.Kid("Item1") is FeNode i1 && riderList.Kid("Item0") is FeNode i0 ? i1.pos.x - i0.pos.x : 284;
            riderScrollFrom = delta * spacing; riderScroll = 0; riderScrollOther = old;
            riderList.aPos.x = riderScrollFrom;
        }

        // a UIDynamicList of 5 slots: the first shown data index follows the cursor; the slot under it is the focused one
        void FormatDynamic(FeNode list, ref int top, int count, Func<int, string> text, Func<int, bool> dim)
        {
            var slots = list.kids.Where(k => k.name.StartsWith("Item", StringComparison.Ordinal)).OrderBy(k => k.pos.y).ToList();
            int n = slots.Count, i = list.absIndex;
            if (i < top) top = i; if (i >= top + n) top = i - n + 1; top = Mathf.Clamp(top, 0, Math.Max(0, count - n));
            for (int s = 0; s < n; s++)
            {
                int d = top + s; bool show = d < count;
                slots[s].Visible = show;
                if (!show) continue;
                SetItemText(slots[s], text(d));
                var t = slots[s].Find("Text"); if (t?.text) t.text.desaturation = dim(d) ? 1f : 0f;
                if (t != null) t.overrideColor = dim(d) ? new Color(0.45f, 0.45f, 0.45f, 1f) : (Color?)null;
            }
            list.focusIndex = list.kids.IndexOf(slots[Mathf.Clamp(i - top, 0, n - 1)]);
            // the scroll bar beside the list (BackgroundImages/Scrollbar/Slider): visible part and position
            var bar = list.parent?.Find("Scrollbar"); var slider = bar?.Kid("Slider");
            if (bar != null) bar.Visible = count > n;
            if (slider != null && count > n)
            {
                float h = bar.size.y, sh = Mathf.Max(8, h * n / count);
                slider.size.y = sh; slider.rt.sizeDelta = slider.size; slider.pos.y = (h - sh) * top / Math.Max(1, count - n);
            }
        }
        void FormatTypeList() => FormatDynamic(typeList, ref typeTop, types.Count, d => Str(types[d].loc), d => types[d].locked);
        void FormatVehicleList() => FormatDynamic(vehicleList, ref vehicleTop, vehicles.Count, d => TitleOf(vehicles[d].storeAsset), d => types.Count > 0 && types[typeList.absIndex].locked);

        // ---------------------------------------------------------------------------------------- focus changes
        void OnRiderListFocusedChanged()
        {
            if (riders.Count == 0) return;
            RequestNewVehicle(true, false);
            PopulateVehicleTypeList();
            FormatRiderList();
        }
        void OnVehicleTypeListFocusedChanged()
        {
            PopulateVehicleList();
            selectedType.Find("Text")?.SetText(types.Count > 0 ? Str(types[typeList.absIndex].loc) : "");
            FormatTypeList();
        }
        void OnVehicleListFocusedChanged()
        {
            RequestNewVehicle(false, true);
            FormatVehicleList();
        }

        // ---------------------------------------------------------------------------------------- input
        void OnAcceptRiderList()
        {
            riderRoot.focusedFlag = false;
            typeRoot.focusedFlag = true; typeRoot.Visible = true;
            fe.PlaySound("Accept");
            suppress = true; OnVehicleTypeListFocusedChanged(); suppress = false;
            riderActive = false; typeActive = true;
        }
        void OnCancelRiderList()
        {
            fe.SendUIEvent("TravelBack");
            fe.PlaySound("Back");
            riderActive = false;
        }
        void OnAcceptVehicleTypeList()
        {
            if (types.Count == 0) return;
            if (types[typeList.absIndex].locked) { fe.PlaySound("DirectionalFail"); return; }   // port: a type it cannot draw
            typeRoot.focusedFlag = false; typeRoot.Visible = false;
            selectedType.Visible = true;
            vehicleRoot.focusedFlag = true; vehicleRoot.Visible = true;
            fe.PlaySound("Accept");
            suppress = true; OnVehicleListFocusedChanged(); suppress = false;
            typeActive = false; vehicleActive = true;
        }
        void OnCancelVehicleTypeList()
        {
            riderRoot.focusedFlag = true;
            typeRoot.focusedFlag = false; typeRoot.Visible = false;
            fe.PlaySound("Back");
            typeList.absIndex = 0; OnVehicleTypeListFocusedChanged();   // SetSelectedAbsoluteIndex( 0 ) -> FocusedDataIndexChanged
            typeActive = false; riderActive = true;
        }
        void OnAcceptVehicleList()
        {
            if (vehicles.Count == 0) return;
            fe.PlaySound("Accept");
            fe.vars["Player1VehicleTypeId"] = types[typeList.absIndex].id;
            arcadeAccepted = true;
            vehicleActive = false;
            fe.SendUIEvent("GotoLocationsPage");
        }
        void OnCancelVehicleList()
        {
            typeRoot.focusedFlag = true; typeRoot.Visible = true;
            selectedType.Visible = false;
            vehicleRoot.focusedFlag = false; vehicleRoot.Visible = false;
            fe.PlaySound("Back");
            vehicleList.absIndex = 0; OnVehicleListFocusedChanged();
            vehicleActive = false; typeActive = true;
        }

        // ---------------------------------------------------------------------------------------- garage
        void RequestNewVehicle(bool loadRider, bool loadVehicle)
        {
            if (suppress || riders.Count == 0 || types.Count == 0 || vehicles.Count == 0) return;
            var r = riders[riderList.absIndex]; var v = vehicles[vehicleList.absIndex]; var type = types[typeList.absIndex];
            UpdateStoreProductBrand(StoreProductOf(v.storeAsset));
            if (loadVehicle) UpdateVehicleStats(v.id, r.id == -1 ? OwnerPlayer1 : OwnerAI);
            if (!loading) { layer.Stop("LoadingStart"); layer.Play("LoadingStart"); }
            loading = true;
            try { garage?.Show(this, r, v, type); }
            catch (Exception e) { Debug.LogWarning("[FrontEnd] vehicle select preview: " + e.Message); }
            loading = false; layer.Play("LoadingDone");   // MXGarage RiderLoaded + VehicleLoaded: the models are built at once here
            UpdateCamera(type.id);
        }

        /// VehicleSelect_Cameras.lua g_vehicleSelectCameraInformationTable: { camera, part, post offset, zoom, tweak } by VehicleTypeId.
        public static readonly Dictionary<int, (Vector3 cam, Vector3 part, Vector3 post)> Cameras = new Dictionary<int, (Vector3, Vector3, Vector3)>
        {
            [2] = (new Vector3(-10.33f, 3.00f, 9.82f), new Vector3(0.96f, 2.40f, -0.22f), new Vector3(-3.21f, -0.12f, -4.80f)),
            [3] = (new Vector3(-10.33f, 3.00f, 9.82f), new Vector3(0.96f, 2.40f, -0.22f), new Vector3(-3.21f, -0.12f, -4.80f)),
            [4] = (new Vector3(-10.33f, 3.00f, 9.82f), new Vector3(0.96f, 2.40f, -0.22f), new Vector3(-3.21f, -0.12f, -4.80f)),
            [11] = (new Vector3(-10.98f, 2.36f, 11.98f), new Vector3(1.70f, 2.40f, -0.46f), new Vector3(-7.43f, 1.23f, -13.29f)),
            [12] = (new Vector3(-10.99f, 2.36f, 11.99f), new Vector3(1.70f, 2.40f, -0.46f), new Vector3(-7.43f, 1.23f, -14.63f)),
            [14] = (new Vector3(-10.99f, 2.36f, 11.99f), new Vector3(1.70f, 2.40f, -0.46f), new Vector3(-7.43f, 1.23f, -14.63f)),
            [15] = (new Vector3(-10.99f, 2.36f, 11.99f), new Vector3(1.70f, 2.40f, -0.46f), new Vector3(-7.43f, 1.23f, -14.63f)),
        };
        void UpdateCamera(int typeId)
        {
            var c = Cameras.TryGetValue(typeId, out var x) ? x : Cameras[3];
            garage?.SetCamera(c.cam, c.part, c.post);
        }

        public void ShowTypeSpecs(RiderItem r, VehicleItem v, out BikeSpec bike, out RiderSpec rider)
        {
            var t = new ReflexGameTables(new FeTableSource(T));
            List<int> parts, gear;
            if (r.id == -1)
            {
                parts = ProfileRows("PlayerVehicleParts").Where(x => FeTables.I(x, "PlayerVehicleId") == v.id).Select(x => FeTables.I(x, "VehiclePartId")).ToList();
                gear = ProfileRows("PlayerRiderGear").Select(x => FeTables.I(x, "RiderGearId")).ToList();
            }
            else
            {
                parts = t.Where("AIVehicleParts", "AIVehicleId", v.id).Select(i => t.Int("AIVehicleParts", "VehiclePartId", i)).ToList();
                gear = t.Where("RiderGearSetGear", "RiderGearSetId", r.gearSet).Select(i => t.Int("RiderGearSetGear", "RiderGearId", i)).ToList();
            }
            bike = ReflexVehicleSpecs.Bike(t, v.modelId, parts);
            rider = gear.Count > 0 ? ReflexVehicleSpecs.Rider(t, gear) : null;
        }

        /// VehicleSelect:UpdateStoreProductBrand: StoreProducts.BrandId -> Brand.BrandIconId on the MXUIStreamableImageIcon.
        void UpdateStoreProductBrand(int storeProductId)
        {
            int icon = -1;
            var sp = Unique("StoreProducts", "StoreProductId", storeProductId);
            if (sp != null) { int b = FeTables.I(sp, "BrandId"); if (b != -1) icon = FeTables.I(Unique("Brand", "BrandId", b), "BrandIconId"); }
            if (brand == null) return;
            var (tex, uv) = icon != -1 ? fe.assets.Icon(icon) : (null, new Rect(0, 0, 1, 1));
            if (brand.image) { brand.image.texture = tex; brand.image.uvRect = uv; brand.image.Dirty(); }
            brand.Visible = icon != -1 && tex != null;
        }

        // ---------------------------------------------------------------------------------------- VehiclePerformanceDisplayComponent
        FeVehiclePerformance perfDisplay;
        /// The page's VehiclePerformanceDisplayComponent (the arcade's default tables: no profile is signed in).
        public FeVehiclePerformance Performance => perfDisplay ??= new FeVehiclePerformance(perf, T, Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex ? ReflexProfile.Current : null);
        /// VehiclePerformanceDisplayComponent:UpdateVehicleStats(vehicleId, owner).
        void UpdateVehicleStats(int vehicleId, int owner) => Performance.UpdateVehicleStats(vehicleId, owner);
    }

    /// VehiclePerformanceDisplayComponent.lua (the "VehiclePerformanceDisplay.component" of VehicleSelect.lyr, GarageMyRides.lyr,
    /// VehiclesQuickViewPopup.lyr; the motocard setup's page has its own FePerformanceDisplay, FrontEndMotocard.cs): engine / suspension / tires, two bars each (Attribute1 / 2: Base, then Bonus
    /// after it), 20 steps over the Background's width: width = floor(value / 100 * max / step) * step, step = max / 20.
    ///   UpdateVehicleStats(id, owner)   VehicleModel DefaultEngine1/2, DefaultSuspension1/2, DefaultTire1/2 of the owner's vehicle, plus the
    ///                                   bonus: a player's PlayerVehicleTuning row (customTuningTable), else the owner's vehicle parts whose
    ///                                   VehiclePartType has a TuningAttribute (DefaultTuneAttribute1/2); Preset / AI owners have no tuning table.
    ///   UpdateVehicleModelStats(model)  the model's defaults plus its Default VehicleModelParts with a TuningAttribute (a vehicle not owned).
    /// The player's tables are the profile's (the garage, the quick view) or, with profile null, the install's default tables (the arcade).
    public sealed class FeVehiclePerformance
    {
        readonly FeNode root; readonly FeTables T; readonly ProfileData profile;
        public FeVehiclePerformance(FeNode root, FeTables tables, ProfileData profile) { this.root = root; T = tables; this.profile = profile; }
        static readonly string[] Attributes = { "engine", "suspension", "tires" };
        /// The values shown last: per attribute { base1, base2, bonus1, bonus2 } (tests).
        public readonly Dictionary<string, int[]> Shown = new Dictionary<string, int[]>();
        Dictionary<string, object> Unique(string table, string col, int v) => T.Rows(table).FirstOrDefault(r => FeTables.I(r, col) == v);
        FeNode Attribute(string a) => root?.kids.FirstOrDefault(k => string.Equals(k.name, a + "_Attribute", StringComparison.OrdinalIgnoreCase));

        /// UpdateAttribute( attribute, base1, base2, bonus1, bonus2 ).
        public void UpdateAttribute(string attribute, int base1, int base2, int bonus1, int bonus2)
        {
            Shown[attribute] = new[] { base1, base2, bonus1, bonus2 };
            Bars(Attribute(attribute), base1, base2, bonus1, bonus2);
        }
        /// The two bars of an attribute container (GarageVehicleTuning:UpdateAttribute is the same code).
        public static void Bars(FeNode c, int base1, int base2, int bonus1, int bonus2)
        {
            if (c == null) return;
            int[,] values = { { base1, bonus1 }, { base2, bonus2 } };
            for (int index = 0; index < 2; index++)
            {
                var a = c.Kid("Attribute" + (index + 1)); if (a == null) continue;
                var bg = a.Kid("Background"); var baseC = a.Kid("Base"); var bonusC = a.Kid("Bonus");
                float max = bg != null ? bg.size.x : 160, step = max / 20;
                float baseWidth = Mathf.Floor(values[index, 0] / 100f * max / step) * step, bonusWidth = Mathf.Floor(values[index, 1] / 100f * max / step) * step;
                Place(baseC, 0, baseWidth); Place(bonusC, baseWidth, bonusWidth);
            }
        }
        static void Place(FeNode n, float x, float w)
        {   // SetXPosition / SetWidth (the position is the pivot's)
            if (n == null) return;
            n.size.x = w; n.rt.sizeDelta = n.size; n.pos.x = x + n.pivot.x * w;
            n.Visible = w > 0;
        }

        int PlayerModel(int vehicleId) => profile != null ? profile.T("PlayerVehicle").Int("VehicleModelId", profile.T("PlayerVehicle").Find("PlayerVehicleId", vehicleId))
                                                          : FeTables.I(Unique("PlayerVehicle", "PlayerVehicleId", vehicleId), "VehicleModelId");
        string TuningAttributeOf(int part) => FeTables.S(Unique("VehiclePartType", "VehiclePartTypeId", FeTables.I(Unique("VehiclePart", "VehiclePartId", part), "VehiclePartTypeId")), "TuningAttribute");
        int[] TuneOf(int part) { var r = Unique("VehiclePart", "VehiclePartId", part); return new[] { FeTables.I(r, "DefaultTuneAttribute1", 0), FeTables.I(r, "DefaultTuneAttribute2", 0) }; }

        public void UpdateVehicleStats(int vehicleId, int owner)
        {
            int modelId; var bonus = Attributes.ToDictionary(a => a, a => new int[2]);
            bool player = owner >= 0 && owner <= 3;
            IEnumerable<int> parts = null;
            if (owner == ReflexWorldFiles.Garage.AI) { modelId = FeTables.I(Unique("AIVehicle", "AIVehicleId", vehicleId), "VehicleModelId"); parts = T.Rows("AIVehicleParts").Where(r => FeTables.I(r, "AIVehicleId") == vehicleId).Select(r => FeTables.I(r, "VehiclePartId")); }
            else if (owner == ReflexWorldFiles.Garage.Preset) { modelId = FeTables.I(Unique("PresetVehicle", "PresetVehicleId", vehicleId), "VehicleModelId"); parts = T.Rows("PresetVehicleParts").Where(r => FeTables.I(r, "PresetVehicleId") == vehicleId).Select(r => FeTables.I(r, "VehiclePartId")); }
            else if (player) modelId = PlayerModel(vehicleId);
            else { foreach (var a in Attributes) UpdateAttribute(a, 0, 0, 0, 0); return; }   // "No support for this owner"
            var model = Unique("VehicleModel", "VehicleModelId", modelId);
            if (model == null) return;
            if (player)
            {   // the custom tuning (the player's tuning table always exists)
                int Cell(string c)
                {
                    if (profile != null) { var t = profile.T("PlayerVehicleTuning"); return t.Int(c, t.Find("PlayerVehicleId", vehicleId), 0); }
                    return FeTables.I(Unique("PlayerVehicleTuning", "PlayerVehicleId", vehicleId), c, 0);
                }
                bonus["engine"][0] = Cell("Engine1"); bonus["engine"][1] = Cell("Engine2");
                bonus["suspension"][0] = Cell("Suspension1"); bonus["suspension"][1] = Cell("Suspension2");
                bonus["tires"][0] = Cell("Tire1"); bonus["tires"][1] = Cell("Tire2");
            }
            else
                foreach (int part in parts)
                {
                    string attr = TuningAttributeOf(part);
                    if (string.IsNullOrEmpty(attr) || !bonus.ContainsKey(attr)) continue;
                    var tv = TuneOf(part); bonus[attr][0] = tv[0]; bonus[attr][1] = tv[1];
                }
            Show(model, bonus);
        }

        public void UpdateVehicleModelStats(int modelId)
        {
            var model = Unique("VehicleModel", "VehicleModelId", modelId);
            if (model == null) { foreach (var a in Attributes) UpdateAttribute(a, 0, 0, 0, 0); return; }
            var bonus = Attributes.ToDictionary(a => a, a => new int[2]);
            foreach (var r in T.Rows("VehicleModelParts").Where(r => FeTables.I(r, "VehicleModelId") == modelId && FeTables.I(r, "Default", 0) != 0))
            {
                int part = FeTables.I(r, "VehiclePartId"); string attr = TuningAttributeOf(part);
                if (string.IsNullOrEmpty(attr) || !bonus.ContainsKey(attr)) continue;
                var tv = TuneOf(part); bonus[attr][0] = tv[0]; bonus[attr][1] = tv[1];
            }
            Show(model, bonus);
        }

        void Show(Dictionary<string, object> model, Dictionary<string, int[]> bonus)
        {
            UpdateAttribute("engine", FeTables.I(model, "DefaultEngine1", 0), FeTables.I(model, "DefaultEngine2", 0), bonus["engine"][0], bonus["engine"][1]);
            UpdateAttribute("suspension", FeTables.I(model, "DefaultSuspension1", 0), FeTables.I(model, "DefaultSuspension2", 0), bonus["suspension"][0], bonus["suspension"][1]);
            UpdateAttribute("tires", FeTables.I(model, "DefaultTire1", 0), FeTables.I(model, "DefaultTire2", 0), bonus["tires"][0], bonus["tires"][1]);
        }
    }

    /// The garage preview behind the Vehicle Select page (MXGarage + its CustomizationCamera into "1280_720_VideoRenderTarget"):
    /// the chosen bike and rider, built at run time from the install, on their own layer far below the scene, rendered into a
    /// 1280x720 texture with a clear background (the arcade background layers show through it).
    /// As the original draws it (its own frame, rendered by the boot's software D3D9 device: garage.md "The garage frame"): only the
    /// garage's vehicle and rider - the HR_ twins (the bike on its HR stand) - into the FP16 scene target cleared to (0, 0, 0, 0), lit by
    /// UI_Compound's lights (ApplyFrontEndGlobals), then the frame's post chain with UI_Compound's HDRSettings (key value 0.15, the eye
    /// adaptation, bloom, the tonemap whose alpha keeps the cleared background transparent), composited by the page's RenderTarget image
    /// over the 2D background layers. UI_Compound's own scenery (terrain, sky, the compound) is not drawn behind the garage.
    public sealed class FeGarage
    {
        public const int Layer = 31;
        static readonly Vector3 Origin = new Vector3(0, -20000, 0);
        public readonly GameObject root; public readonly Camera camera; public readonly RenderTexture Texture;
        ReflexBikeModel bike; ReflexRiderModel rider; string bikeKey, riderKey;
        Vector3 camPos, camTarget, camWanted, targetWanted; float camT = 1;
        public string Shown { get; private set; } = "";

        /// The objects' layer (Layer for the garage's own picture; the scene's layer in world mode).
        readonly int layer;
        /// World mode (the front end's 3D menu, OpenFrontEnd/OpenMenuWorld): the models stand in a level that is drawn by that level's own camera;
        /// no texture, no camera of the garage's own, no plate; the camera the shell's stage asks for goes to CameraSink (world space).
        public readonly bool world;
        /// World mode: where the camera should be (eye, look-at point; world space), every time the garage places its camera.
        public Action<Vector3, Vector3> CameraSink;

        public FeGarage(Transform parent) : this(parent, Origin, null, Layer) { }

        /// externalCamera null: the garage's own picture (Texture) as the pages composite it, at origin on layer. Otherwise world mode: the
        /// models at origin (world space) on objectLayer, drawn by the caller's camera (its field of view is the one the stage fits with).
        public FeGarage(Transform parent, Vector3 origin, Camera externalCamera, int objectLayer)
        {
            layer = objectLayer; world = externalCamera != null;
            root = new GameObject(world ? "Menu world garage (rider and vehicle)" : "Vehicle Select garage (MXGarage preview)");
            root.transform.SetParent(parent, false); root.transform.position = origin; root.layer = layer;
            if (world)
            {
                camera = externalCamera;
                camPos = camWanted = new Vector3(-10.33f, 3, 9.82f); camTarget = targetWanted = new Vector3(0.96f, 2.4f, -0.22f); post = Vector3.zero;
                return;   // the level's lighting is the world's (OriginalLevel applied UI_Compound's globals)
            }
            Texture = new RenderTexture(1280, 720, 24, RenderTextureFormat.ARGB32) { name = "1280_720_VideoRenderTarget (garage)", antiAliasing = 4, hideFlags = HideFlags.DontSave };
            var cgo = new GameObject("Garage camera"); cgo.transform.SetParent(root.transform, false); cgo.layer = layer;
            camera = cgo.AddComponent<Camera>();
            camera.clearFlags = CameraClearFlags.SolidColor; camera.backgroundColor = new Color(0, 0, 0, 0);
            camera.cullingMask = 1 << layer; camera.targetTexture = Texture; camera.fieldOfView = 34; camera.nearClipPlane = 0.3f; camera.farClipPlane = 400;
            camera.allowHDR = false; camera.allowMSAA = true;
            if (OriginalPost) UsePostChain();
            var lgo = new GameObject("Garage key light"); lgo.transform.SetParent(root.transform, false); lgo.layer = layer;
            var key = lgo.AddComponent<Light>(); key.type = LightType.Directional; key.intensity = 1.25f; key.color = new Color(1f, 0.97f, 0.92f); key.shadows = LightShadows.None; key.cullingMask = 1 << layer;
            lgo.transform.rotation = Quaternion.Euler(38, 150, 0);
            ReflexObjectShading.ApplyFrontEndGlobals();   // Reflex/Vehicle: the garage's lighting is UI_Compound's (the key light above is not read by it)
            camPos = camWanted = new Vector3(-10.33f, 3, 9.82f); camTarget = targetWanted = new Vector3(0.96f, 2.4f, -0.22f);
            floor = MakeFloor();
            Place();
        }

        // ---- the stage floor (port extra): a round plate under the vehicle and the rider. The garage picture is composited over the page with an
        // alpha of 1 wherever something was drawn, so a soft shadow cannot be drawn into it; with nothing under them the models read as floating.
        static Mesh floorMesh; static Shader floorShader; GameObject floor;
        /// Draws the plate (the open shell's garage turns it on; the original pages keep the bare models).
        public bool stageFloor;
        GameObject MakeFloor()
        {
            try
            {
                if (!floorShader) floorShader = Shader.Find("Sprites/Default");
                if (!floorShader) return null;
                if (!floorMesh)
                {   // rings of vertices (radius 0 / 0.93 / 0.93 / 1.0) with colours: a dark plate that darkens to its rim, then a thin lit edge
                    const int n = 72;
                    var inner = new Color(0.17f, 0.14f, 0.15f, 1f); var rim = new Color(0.075f, 0.065f, 0.07f, 1f); var edge = new Color(0.36f, 0.085f, 0.08f, 1f);
                    float[] radius = { 0f, 0.93f, 0.93f, 1f }; Color[] color = { inner, rim, edge, edge };
                    var v = new Vector3[1 + 3 * n]; var c = new Color[v.Length]; var tris = new System.Collections.Generic.List<int>();
                    v[0] = Vector3.zero; c[0] = inner;
                    for (int r = 1; r < 4; r++)
                        for (int k = 0; k < n; k++)
                        {
                            float a = k * Mathf.PI * 2f / n;
                            v[1 + (r - 1) * n + k] = new Vector3(Mathf.Cos(a) * radius[r], 0, Mathf.Sin(a) * radius[r]); c[1 + (r - 1) * n + k] = color[r];
                        }
                    for (int k = 0; k < n; k++) { int k2 = (k + 1) % n; tris.AddRange(new[] { 0, 1 + k2, 1 + k }); }
                    for (int r = 1; r < 3; r++)
                        for (int k = 0; k < n; k++)
                        {
                            int k2 = (k + 1) % n, a0 = 1 + (r - 1) * n + k, a1 = 1 + (r - 1) * n + k2, b0 = 1 + r * n + k, b1 = 1 + r * n + k2;
                            tris.AddRange(new[] { a0, b1, b0, a0, a1, b1 });
                        }
                    floorMesh = new Mesh { name = "garage_floor", hideFlags = HideFlags.DontSave, vertices = v, colors = c, triangles = tris.ToArray() };
                    floorMesh.RecalculateBounds();
                }
                var g = new GameObject("Stage floor"); g.transform.SetParent(root.transform, false); g.layer = layer;
                g.AddComponent<MeshFilter>().sharedMesh = floorMesh;
                var mr = g.AddComponent<MeshRenderer>();
                mr.sharedMaterial = new Material(floorShader) { name = "garage_floor", color = Color.white, hideFlags = HideFlags.DontSave };
                mr.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off; mr.receiveShadows = false;
                g.SetActive(false);
                return g;
            }
            catch (Exception e) { Debug.LogWarning("[Garage] no stage floor: " + e.Message); return null; }
        }

        /// The plate under what is shown: centred on the vehicle and the rider together, as wide as they are plus a margin; none with nothing shown.
        void UpdateFloor()
        {
            if (!floor) return;
            if (!stageFloor) { floor.SetActive(false); return; }
            bool hasBike = BikeBounds(out var b), hasRider = rider != null && rider.root;
            if (!hasBike && !hasRider) { floor.SetActive(false); return; }
            float x0 = float.MaxValue, x1 = float.MinValue, z0 = float.MaxValue, z1 = float.MinValue;
            void Grow(float x, float z) { x0 = Mathf.Min(x0, x); x1 = Mathf.Max(x1, x); z0 = Mathf.Min(z0, z); z1 = Mathf.Max(z1, z); }
            if (hasBike) { Grow(b.min.x, b.min.z); Grow(b.max.x, b.max.z); }
            if (hasRider) { Grow(riderFoot.xMin, riderFoot.yMin); Grow(riderFoot.xMax, riderFoot.yMax); }   // the posed skin's own footprint (both feet inside)
            float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
            float r = Mathf.Sqrt(Mathf.Pow((x1 - x0) * 0.5f, 2) + Mathf.Pow((z1 - z0) * 0.5f, 2)) + 1.2f;
            floor.SetActive(true); plateC = new Vector2(cx, cz); plateR = r;
            floor.transform.localPosition = new Vector3(cx, -0.03f, cz); floor.transform.localScale = new Vector3(r, 1, r);
        }

        public void Show(VehicleSelectScript vs, VehicleSelectScript.RiderItem r, VehicleSelectScript.VehicleItem v, VehicleSelectScript.TypeItem type)
        {
            vs.ShowTypeSpecs(r, v, out var bs, out var rs);
            if (type.locked) bs = null;   // a type the port cannot draw
            string bk = bs?.Key ?? "-", rk = rs?.Key ?? "-";
            bool bikeBuilt = false, riderBuilt = false;
            if (bk != bikeKey)
            {
                KillBike(); bikeKey = bk; bikeBuilt = true;
                if (bs != null)
                {
                    bike = BuildBike(bs);
                    var b = Bounds(bike.chassis, bike.frontWheel.gameObject, bike.rearWheel.gameObject);   // built at the origin: the tyres onto the garage floor (y 0)
                    var lift = new Vector3(0, -b.min.y, 0);
                    Adopt(bike.chassis, lift); Adopt(bike.frontWheel.gameObject, lift); Adopt(bike.rearWheel.gameObject, lift);
                    KeepBikeRest(); GroundBike();
                }
            }
            if (rk != riderKey)
            {
                KillRider(); riderKey = rk; riderBuilt = true;
                if (rs != null)
                {
                    rider = BuildRider(rs);
                    if (rider.proxyRoot) rider.proxyRoot.gameObject.SetActive(false);
                    StandBeside();   // standing on the floor beside the vehicle, on the far side from the camera
                }
            }
            if (bikeBuilt && !riderBuilt && rider != null) MoveBeside();   // another vehicle (or model) came: the rider's spot follows its bounds
            UpdateFloor();
            Shown = (bs?.name ?? "none") + " / " + (rs != null ? string.Join(",", rs.parts.Select(p => p.modelPkg)) : "none");
        }

        /// A bike alone (MOTOCAREER's reward vehicle page: MXGarage:RequestVehicle of the rewarded VehicleModelId).
        public void ShowBike(BikeSpec bs) => ShowSpecs(bs, null, false);
        /// MOTOCAREER's garage (FrontEndGarage.cs): a bike and / or a rider (null: none); the rider beside the bike as Show places
        /// it, or alone at the origin (MXGarage:RemoveVehicle, the rider customization).
        public void ShowSpecs(BikeSpec bs, RiderSpec rs, bool riderBeside)
        {
            string bk = bs?.Key ?? "-", rk = (rs?.Key ?? "-") + (riderBeside ? "|b" : "|a");
            ShownBike = bs; ShownRider = rs;
            bool bikeBuilt = false, riderBuilt = false;
            if (bk != bikeKey)
            {
                KillBike(); bikeKey = bk; bikeBuilt = true;
                if (bs != null)
                {
                    bike = BuildBike(bs);
                    var b = Bounds(bike.chassis, bike.frontWheel.gameObject, bike.rearWheel.gameObject);
                    var lift = new Vector3(0, -b.min.y, 0);
                    Adopt(bike.chassis, lift); Adopt(bike.frontWheel.gameObject, lift); Adopt(bike.rearWheel.gameObject, lift);
                    KeepBikeRest(); GroundBike();
                }
            }
            if (rk != riderKey)
            {
                KillRider(); riderKey = rk; riderBuilt = true;
                if (rs != null)
                {
                    rider = BuildRider(rs);
                    if (rider.proxyRoot) rider.proxyRoot.gameObject.SetActive(false);
                    if (riderBeside) StandBeside(); else StandRider(Vector3.zero);
                }
            }
            if (bikeBuilt && !riderBuilt && rider != null && riderKey.EndsWith("|b", StringComparison.Ordinal)) MoveBeside();
            if (bikeBuilt || riderBuilt) BuildSerial++;
            UpdateFloor();
            RefreshPlates();   // (also for models already standing: the name / number may have changed)
            Shown = (bs?.name ?? "none") + " / " + (rs != null ? string.Join(",", rs.parts.Select(p => p.modelPkg)) : "none");
        }

        /// The number plates, the jersey and the chest protector name-over-number plates of what is shown (MXGarage binds the rider's own run-time
        /// plates on its preview: 00761d60 / 00760ce0, FUN_005520a0; FUN_005520d0 / FUN_00551110 make them from the PlayerRider's Name / Number,
        /// 256 x 256 on the high-resolution models). The text comes from here: the profile's own rider, or the pro rider the spec names
        /// (ReflexPlateText.ForSpecs); null makes none (the plates stay clear).
        public Func<BikeSpec, RiderSpec, ReflexPlateText> plateText = ReflexPlateText.ForSpecs;
        /// The plates are made again from the current profile when its name / number changed (cheap otherwise: the textures are keyed by their text).
        public void RefreshPlates()
        {
            if (bike == null && rider == null) return;
            try
            {
                var text = plateText?.Invoke(ShownBike, ShownRider);
                ReflexPlates.Apply(bike, rider, text, highRes);
                PlateKey = text?.Key + (highRes ? "|hr" : "");
            }
            catch (Exception e) { Debug.LogWarning("[Garage] plates: " + e.Message); }
        }
        /// The text of the plates now on the models (tests, the portrait's redraw).
        public string PlateKey { get; private set; }

        /// The garage's own post chain (ReflexRenderPipeline with UI_Compound's HDRSettings) on the garage camera; off: the raw scene values.
        public static bool OriginalPost = true;
        Rendering.ReflexRenderPipeline postChain;
        /// UI_Compound's HDRSettingsRSData (the front end's world; the executable's defaults when it has none), read once.
        static Rendering.ReflexHdrSettings frontEndHdr;
        void UsePostChain()
        {
            try
            {
                if (frontEndHdr == null)
                {
                    var l = ReflexLevelContent.Load("UI_Compound", new ReflexLevelContent.Options { scenery = false, terrain = false, tracks = false });
                    frontEndHdr = l.lighting.hdr;
                }
                Texture.Release(); Texture.antiAliasing = 1;   // the original's targets have no MSAA
                camera.allowHDR = true; camera.allowMSAA = false;
                var data = UnityEngine.Rendering.Universal.CameraExtensions.GetUniversalAdditionalCameraData(camera); data.renderPostProcessing = false;
                postChain = camera.gameObject.AddComponent<Rendering.ReflexRenderPipeline>();
                postChain.hdr = frontEndHdr;
                // the garage clears its scene target to (0, 0, 0, 0) (the pipeline's 0x808080 is the race scene's): after its OnBegin
                UnityEngine.Rendering.RenderPipelineManager.beginCameraRendering += ClearTransparent;
            }
            catch (Exception e) { Debug.LogWarning("[Garage] the garage's post chain is off: " + e.Message); if (postChain) UnityEngine.Object.DestroyImmediate(postChain); postChain = null; camera.allowHDR = false; }
        }
        void ClearTransparent(UnityEngine.Rendering.ScriptableRenderContext ctx, Camera c)
        {
            if (c != camera) return;
            c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = new Color(0, 0, 0, 0);
        }
        /// UI_Compound's key value (tests).
        public static float KeyValue => frontEndHdr?.keyValue ?? -1;

        /// MXGarage:SetHREnable (TRUE except MotocardStunts): the HR_ twins of the vehicle's and the rider's packages (ReflexBikeModel.HighRes).
        public bool highRes = true;
        /// What the last build used (tests): the chassis package and whether the HR twins were taken.
        public string BuiltChassis { get; private set; }
        ReflexBikeModel BuildBike(BikeSpec bs)
        {
            var spec = highRes ? ReflexBikeModel.HighRes(bs) : bs;
            BuiltChassis = spec.chassisDb + "/" + spec.chassisPkg + "/" + spec.chassisModel;
            return ReflexBikeModel.Build(spec);
        }
        /// The rider's HR twins ("HR_<db>/HR_<pkg>", the skin tones "HR_<db>/<pkg>"; the hierarchy stays Rider/Rider), falling back to the
        /// normal packages when the rider builder cannot use them.
        ReflexRiderModel BuildRider(RiderSpec rs)
        {
            if (highRes)
            {
                var hr = HighResRider(rs);
                if (hr != null) { try { return ReflexRiderModel.Build(hr); } catch (Exception e) { Debug.LogWarning("[Garage] HR rider: " + e.Message + " (the normal gear)"); } }
            }
            return ReflexRiderModel.Build(rs);
        }
        public static RiderSpec HighResRider(RiderSpec rs)
        {
            if (rs == null) return null;
            var install = VehicleCatalog.Shared.install;
            (string db, string pkg) Twin(string db, string pkg)
            {
                // (a pack's package keeps its namespace: "mod:kit/<hash>>RiderDLC010_graphicskit" -> "mod:kit/<hash>>HR_RiderDLC010_graphicskit", KitPacks)
                string ns = BikeMods.NamespaceOf(db), bare = BikeMods.BaseDb(db);
                if (db == null || pkg == null || bare.StartsWith("HR_", StringComparison.Ordinal)) return (db, pkg);
                if (install.Pkg(ns + "HR_" + bare, "HR_" + pkg) != null) return (ns + "HR_" + bare, "HR_" + pkg);
                if (install.Pkg(ns + "HR_" + bare, pkg) != null) return (ns + "HR_" + bare, pkg);
                return (db, pkg);
            }
            var o = new RiderSpec { hierarchyDb = rs.hierarchyDb, hierarchyPkg = rs.hierarchyPkg, hierarchyAsset = rs.hierarchyAsset, gearIds = rs.gearIds };
            (o.skinDb, o.skinPkg) = Twin(rs.skinDb, rs.skinPkg);
            bool any = false;
            foreach (var p in rs.parts)
            {
                var m = Twin(p.modelDb, p.modelPkg); var t = Twin(p.matsDb, p.matsPkg);
                any |= m.db != p.modelDb;
                string asset = p.asset != null && m.db != p.modelDb && install.Pkg(m.db, m.pkg)?.Find("HR_" + p.asset, "model") != null ? "HR_" + p.asset : m.db != p.modelDb ? null : p.asset;
                o.parts.Add(new RiderSpec.Part { key = p.key, modelDb = m.db, modelPkg = m.pkg, asset = asset, matsDb = t.db, matsPkg = t.pkg });
            }
            foreach (var e in rs.extraMaterials) o.extraMaterials.Add(Twin(e.db, e.pkg));
            return any ? o : null;
        }

        static Bounds Bounds(params GameObject[] gs)
        {
            Bounds b = default; bool any = false;
            foreach (var g in gs) if (g) foreach (var r in g.GetComponentsInChildren<Renderer>()) { if (!any) { b = r.bounds; any = true; } else b.Encapsulate(r.bounds); }
            return b;
        }

        void Adopt(GameObject g, Vector3 offset)
        {
            if (!g) return;
            g.transform.SetParent(root.transform, false);
            g.transform.localPosition += offset;
            foreach (var t in g.GetComponentsInChildren<Transform>(true)) t.gameObject.layer = layer;
        }

        // ---- the garage rider's animation. MXGarage plays RiderUIAnimations/Base SC_RU_HC_Anim_NoLoop_0n on the garage rider (a standing
        // idle on the MXRider skeleton); the clip is sampled here with the game's own keyframe rules (Content/ReflexRiderAnim) and its bone
        // matrices go to the rider like a game pose. The feet are put on the stage floor from the posed boots' lowest vertex.
        /// The RiderUIAnimations clip the garage rider plays (null: the bind pose).
        public static string RiderClip = "SC_RU_HC_Anim_NoLoop_01";
        /// Where in the clip the rider starts (seconds), and the turn about the vertical (degrees; the riders and the vehicles face +z): -35 turns him
        /// towards the viewer's side of the stage, a showroom three-quarter instead of a profile.
        public static float RiderClipStart = 0f, RiderYaw = -35f;
        /// The clip plays on (idle motion); off: the rider holds the pose of its start time (REDUCE MOTION, tests).
        public static bool RiderPlays = true;
        ReflexRiderAnim riderAnim; float riderAnimT; Vector3 riderSpot; string[] riderNodeNames; float[] riderWorld, riderRoot;
        /// The clip's name and time now (tests).
        public string RiderPose => riderAnim == null ? "bind pose" : riderAnim.name + " @" + riderAnimT.ToString("0.00") + "s of " + riderAnim.length.ToString("0.0");

        /// Where the rider stands, in feet: his posed skin stays RiderNoseGap beyond the vehicle's nose (so, seen from its left or right, he never
        /// overlaps the vehicle, and from any angle the pair cannot touch) and RiderSideGap beyond its widest point on its right.
        public static float RiderNoseGap = 0.6f, RiderSideGap = 0.8f;
        // the posed skin of the rider relative to where he stands (x / z, y as it is) and the footprint on the stage (garage space, x / z)
        readonly System.Collections.Generic.List<Vector3> riderRel = new System.Collections.Generic.List<Vector3>();
        Rect riderFoot; Vector2 plateC; float plateR;

        /// The rider stands beside the vehicle (port placement), first posed on the floor at the origin, then moved by MoveBeside.
        void StandBeside()
        {
            StandRider(Vector3.zero);
            MoveBeside();
        }

        /// The standing rider goes to the spot beside the vehicle that the vehicle's present vertices give.
        void MoveBeside()
        {
            var spot = BesideSpot();
            MoveRider(spot.x, spot.z);
        }

        /// Where the rider stands beside the vehicle: at its front right, as a showroom pose: past the vehicle's nose (the rider's rearmost skin point
        /// RiderNoseGap ahead of the vehicle's frontmost vertex) and RiderSideGap off its right-hand extreme. The sizes are the vertices' own (a
        /// wide handlebar, a mod's bigger bodywork, a truck's cab and mirrors all move him), never a table. Seen from the left or the right the
        /// rider then stands clear of the vehicle's silhouette, from the front or the back he is beside it, and the two boxes cannot meet at
        /// any angle. No vehicle: the old fixed spot.
        Vector3 BesideSpot()
        {
            if (besideLeft) return BesideLeftSpot();
            var fallback = new Vector3(3.3f, 0, 2.4f);
            if (!BikeBounds(out var bb)) return fallback;
            var bp = BikePoints();
            float xmax = bb.max.x, zmax = bb.max.z;
            if (bp.Count > 0) { xmax = float.MinValue; zmax = float.MinValue; foreach (var p in bp) { xmax = Mathf.Max(xmax, p.x); zmax = Mathf.Max(zmax, p.z); } }
            float rx = 0, rz = 0;   // the rider's skin: its smallest x and z about where he stands
            if (riderRel.Count > 0) { rx = float.MaxValue; rz = float.MaxValue; foreach (var r in riderRel) { rx = Mathf.Min(rx, r.x); rz = Mathf.Min(rz, r.z); } }
            return new Vector3(xmax + RiderSideGap - rx, 0, zmax + RiderNoseGap - rz);
        }

        /// The menu world's stance (OpenMenuWorld: where the push-bike walk ends): the rider on the vehicle's LEFT, beside its front half (his
        /// centre at BesideLeftAlong of the way from the rear wheel to the front wheel), his nearest skin point RiderSideGap clear of the vehicle's
        /// vertices within his own depth (so a wide handlebar ahead of him does not push him out). Turned by yawOverride.
        public bool besideLeft;
        /// The menu world's stance along the vehicle's axis (0 rear wheel, 1 front wheel).
        public static float BesideLeftAlong = 0.62f;
        Vector3 BesideLeftSpot()
        {
            var fallback = new Vector3(-3.0f, 0, 0.6f);
            if (!BikeBounds(out var bb)) return fallback;
            float z = bb.center.z;
            if (BikeAxis(out var f, out var r)) z = Mathf.Lerp(r.z, f.z, BesideLeftAlong);
            float rz0 = -0.6f, rz1 = 0.6f, rxMax = 0.5f;   // the rider's skin about where he stands: depth and his right-hand extreme
            if (riderRel.Count > 0) { rz0 = float.MaxValue; rz1 = float.MinValue; rxMax = float.MinValue; foreach (var p in riderRel) { rz0 = Mathf.Min(rz0, p.z); rz1 = Mathf.Max(rz1, p.z); rxMax = Mathf.Max(rxMax, p.x); } }
            float xmin = bb.min.x; bool any = false;
            foreach (var p in BikePoints())
            {
                if (p.z < z + rz0 - 0.3f || p.z > z + rz1 + 0.3f) continue;
                if (!any || p.x < xmin) xmin = p.x; any = true;
            }
            return new Vector3(xmin - RiderSideGap - rxMax, 0, z);
        }

        /// The vehicle's vertices in the garage's own space (every enabled renderer of the chassis and the wheels; a mesh the engine cannot read gives its bounds' corners).
        System.Collections.Generic.List<Vector3> BikePoints()
        {
            var pts = new System.Collections.Generic.List<Vector3>(4096);
            if (bike == null) return pts;
            var origin = root.transform.position;
            var baked = new Mesh();
            foreach (var g in new[] { bike.chassis, bike.frontWheel ? bike.frontWheel.gameObject : null, bike.rearWheel ? bike.rearWheel.gameObject : null })
            {
                if (!g) continue;
                foreach (var r in g.GetComponentsInChildren<Renderer>())
                {
                    if (!r.enabled || !r.gameObject.activeInHierarchy) continue;
                    Mesh mesh = null; Matrix4x4 m = r.transform.localToWorldMatrix;
                    try
                    {
                        if (r is SkinnedMeshRenderer smr && smr.sharedMesh) { smr.BakeMesh(baked, false); mesh = baked; }
                        else if (r.TryGetComponent<MeshFilter>(out var mf) && mf.sharedMesh && mf.sharedMesh.isReadable) mesh = mf.sharedMesh;
                    }
                    catch (Exception) { mesh = null; }
                    if (mesh == null || mesh.vertexCount == 0)
                    {
                        var bnd = r.bounds; var c = bnd.center; var e = bnd.extents;
                        for (int i = 0; i < 8; i++) pts.Add(c + new Vector3((i & 1) == 0 ? -e.x : e.x, (i & 2) == 0 ? -e.y : e.y, (i & 4) == 0 ? -e.z : e.z) - origin);
                        continue;
                    }
                    foreach (var v in mesh.vertices) pts.Add(m.MultiplyPoint3x4(v) - origin);
                }
            }
            if (Application.isPlaying) UnityEngine.Object.Destroy(baked); else UnityEngine.Object.DestroyImmediate(baked);
            return pts;
        }

        /// The built rider stands at spot (the garage's own x / z) with its feet on the floor: the clip's pose at its start time.
        void StandRider(Vector3 spot)
        {
            Adopt(rider.root, Vector3.zero);
            var o = rider.rider;
            riderSpot = spot; riderAnimT = RiderClipStart; RiderGroundLift = 0;
            riderAnim = string.IsNullOrEmpty(RiderClip) ? null : ReflexRiderAnim.Load("RiderUIAnimations", "Base", RiderClip);
            if (riderAnim != null)
            {
                if (riderNodeNames == null || riderNodeNames.Length != o.bones.Length) riderNodeNames = o.bones.Select(t => t.name).ToArray();
                if (riderWorld == null) { riderWorld = new float[OriginalRider.OriginalBoneCount * 16]; riderRoot = new float[16]; }
                if (!PoseRider()) riderAnim = null;
            }
            if (riderAnim == null)
            {   // no clip in this install: the bind pose, as before
                o.ApplyBoneMatrices(o.bindWorldMatrices, OriginalRider.OriginalBoneCount);
                var b = Bounds(rider.root);
                rider.root.transform.localPosition = new Vector3(spot.x, -b.min.y, spot.z);
                riderSpot.y = rider.root.transform.localPosition.y;
                SkinPoints(riderRel, spot);
                SetFoot(spot);
                GroundRider();
                return;
            }
            riderSpot.y += root.transform.position.y - SkinPoints(riderRel, spot);   // the soles on the floor (the vehicle's tyres are at the root's height)
            PoseRider();
            SetFoot(spot);
            GroundRider();
        }

        /// The standing rider goes to x / z (garage space) with his feet where they were on y.
        void MoveRider(float x, float z)
        {
            if (rider == null || !rider.root) return;
            riderSpot.x = x; riderSpot.z = z;
            if (riderAnim != null) PoseRider();
            else rider.root.transform.localPosition = new Vector3(x, rider.root.transform.localPosition.y, z);
            SetFoot(riderSpot);
            GroundRider();
            if (Verbose) Debug.Log("[garage-stage] rider at " + riderSpot + " foot " + riderFoot);
        }

        public static bool Verbose;

        void SetFoot(Vector3 spot)
        {
            float x0 = float.MaxValue, x1 = float.MinValue, z0 = float.MaxValue, z1 = float.MinValue;
            foreach (var r in riderRel) { x0 = Mathf.Min(x0, r.x); x1 = Mathf.Max(x1, r.x); z0 = Mathf.Min(z0, r.z); z1 = Mathf.Max(z1, r.z); }
            if (x0 > x1) { x0 = z0 = -1f; x1 = z1 = 1f; }
            riderFoot = Rect.MinMaxRect(spot.x + x0 - 0.2f, spot.z + z0 - 0.2f, spot.x + x1 + 0.2f, spot.z + z1 + 0.2f);
        }

        /// The rider's bones at the clip's current time, on riderSpot turned by RiderYaw.
        bool PoseRider()
        {
            var place = Matrix4x4.TRS(root.transform.position + riderSpot, Quaternion.Euler(0, yawOverride ?? RiderYaw, 0), Vector3.one);
            for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) riderRoot[i * 4 + k] = k == 3 ? 0 : place[k, i];
            riderRoot[12] = place[0, 3]; riderRoot[13] = place[1, 3]; riderRoot[14] = place[2, 3]; riderRoot[15] = 1;
            riderAnim.World(riderAnimT, riderNodeNames, rider.rider.boneParents, riderRoot, riderWorld);
            return rider.rider.ApplyBoneMatrices(riderWorld, OriginalRider.OriginalBoneCount);
        }

        /// The posed rider's skin vertices (the garage's own space; x / z relative to where he stands now, y absolute) go to pts; returns the lowest world y.
        float SkinPoints(System.Collections.Generic.List<Vector3> pts, Vector3 standing)
        {
            float low = float.MaxValue; var baked = new Mesh();
            pts.Clear();
            var origin = root.transform.position;
            foreach (var smr in rider.rider.skinnedMeshes)
            {
                if (!smr || !smr.sharedMesh) continue;
                smr.BakeMesh(baked, false);
                var m = smr.transform.localToWorldMatrix;
                foreach (var v in baked.vertices)
                {
                    var w = m.MultiplyPoint3x4(v);
                    low = Mathf.Min(low, w.y);
                    pts.Add(new Vector3(w.x - origin.x - standing.x, w.y - origin.y, w.z - origin.z - standing.z));
                }
            }
            if (Application.isPlaying) UnityEngine.Object.Destroy(baked); else UnityEngine.Object.DestroyImmediate(baked);
            return low == float.MaxValue ? root.transform.position.y : low;
        }

        // ---- the ground under them (world mode: the menu world's floor is the level's terrain, not a plane). The vehicle and the rider stand on
        // the root's height first; with groundAt each is then moved up or down so that its lowest points (the stand's feet / the tyres, the
        // soles) rest on the ground as drawn under them, none below it (w-fb-models: the stand and the boots sank into the compound's floor,
        // whose height and RT_Bump relief vary under them).
        /// The ground as drawn at world x / z (null: the flat floor at the root's height, the garage picture).
        public Func<float, float, float> groundAt;
        /// How far the vehicle at rest was moved to the ground (feet; + up).
        public float BikeGroundLift { get; private set; }
        /// How far the standing rider was moved to the ground (feet; + up).
        public float RiderGroundLift { get; private set; }
        /// Both placed on the ground again (the ground sampler became more exact).
        public void Reground() { GroundBike(); GroundRider(); }
        /// Points within this of a model's lowest point are its supports (the stand's feet, the tyres' contact patches, the soles).
        public static float SupportBand = 0.12f;

        /// The vehicle at rest (KeepBikeRest) moved so that its supports rest on the ground: the highest ground under them.
        void GroundBike()
        {
            if (groundAt == null || bike == null || bikeRest == null) return;
            var pts = new System.Collections.Generic.List<Vector3>(); float low = float.MaxValue;
            foreach (var g in new[] { bike.chassis, bike.frontWheel ? bike.frontWheel.gameObject : null, bike.rearWheel ? bike.rearWheel.gameObject : null })
            {
                if (!g) continue;
                foreach (var mf in g.GetComponentsInChildren<MeshFilter>())
                {
                    if (!mf.sharedMesh || !mf.sharedMesh.isReadable) continue;
                    var m = mf.transform.localToWorldMatrix;
                    foreach (var v in mf.sharedMesh.vertices) { var w = m.MultiplyPoint3x4(v); pts.Add(w); low = Mathf.Min(low, w.y); }
                }
            }
            float lift = Lift(pts, low);
            if (float.IsNaN(lift)) return;
            for (int i = 0; i < bikeRest.Length; i++) { bikeRest[i].pos.y += lift; if (bikeRest[i].t) bikeRest[i].t.localPosition += new Vector3(0, lift, 0); }
            BikeGroundLift += lift;
            UpdateFloor();
        }

        /// The standing rider moved so that his soles rest on the ground.
        void GroundRider()
        {
            if (groundAt == null || rider == null || !rider.root || riderRel.Count == 0) return;
            var o = root.transform.position; float low = float.MaxValue;
            var pts = new System.Collections.Generic.List<Vector3>(riderRel.Count);
            foreach (var r in riderRel) { var w = new Vector3(o.x + riderSpot.x + r.x, o.y + r.y, o.z + riderSpot.z + r.z); pts.Add(w); low = Mathf.Min(low, w.y); }
            float lift = Lift(pts, low);
            if (float.IsNaN(lift)) return;
            riderSpot.y += lift;
            for (int i = 0; i < riderRel.Count; i++) riderRel[i] += new Vector3(0, lift, 0);
            if (riderAnim != null) PoseRider(); else rider.root.transform.localPosition += new Vector3(0, lift, 0);
            RiderGroundLift += lift;
        }

        /// How far points must move up (down: negative) for the lowest of their supports to touch the ground and none to be below it.
        float Lift(System.Collections.Generic.List<Vector3> pts, float low)
        {
            if (pts.Count == 0 || low == float.MaxValue) return float.NaN;
            float lift = float.MinValue;
            foreach (var p in pts) if (p.y < low + SupportBand) { float g = groundAt(p.x, p.z); if (!float.IsNaN(g)) lift = Mathf.Max(lift, g - p.y); }
            return lift == float.MinValue ? float.NaN : lift;
        }

        /// Tests: the shortest distance (feet) between the rider's skin and the vehicle's vertices now (sampled; float.MaxValue with either missing), and the
        /// lowest sole's height over the stage floor.
        public float MeasureClearance(out float soleHeight) => MeasureClearance(out soleHeight, out _);
        /// plateSlack: how far inside the plate's lit rim the rider's soles are (feet; negative: off the plate; NaN without a plate).
        public float MeasureClearance(out float soleHeight, out float plateSlack)
        {
            soleHeight = float.NaN; plateSlack = float.NaN;
            if (rider == null || !rider.root) return float.MaxValue;
            var rp = new System.Collections.Generic.List<Vector3>();
            float low = SkinPoints(rp, Vector3.zero);
            soleHeight = low - root.transform.position.y;
            if (floor && floor.activeSelf)
            {
                plateSlack = float.MaxValue;
                foreach (var p in rp) if (p.y < soleHeight + 0.35f) plateSlack = Mathf.Min(plateSlack, plateR * 0.93f - (new Vector2(p.x, p.z) - plateC).magnitude);
            }
            var bp = BikePoints();
            if (bp.Count == 0) return float.MaxValue;
            int rs = Mathf.Max(1, rp.Count / 2500), bs = Mathf.Max(1, bp.Count / 6000);
            float best = float.MaxValue;
            for (int i = 0; i < rp.Count; i += rs)
                for (int j = 0; j < bp.Count; j += bs) { float d = (rp[i] - bp[j]).sqrMagnitude; if (d < best) best = d; }
            return Mathf.Sqrt(best);
        }

        /// The rider's idle goes on by dt seconds (the clip loops).
        public void Animate(float dt)
        {
            if (riderAnim == null || rider == null || !rider.root || !RiderPlays || HoldIdle) return;
            riderAnimT += dt;
            if (riderAnimT > riderAnim.length) riderAnimT -= riderAnim.length;
            PoseRider();
        }

        /// Tests: the rider at another place (garage space x / z) and turn now.
        public void SetRiderSpot(float x, float z, float yaw)
        {
            if (rider == null) return;
            RiderYaw = yaw;
            StandRider(new Vector3(x, 0, z));
            UpdateFloor();
        }

        /// Tests: the rider at another clip / time now (the feet stay where StandRider put them).
        public void SetRiderClip(string clip, float time)
        {
            if (rider == null) return;
            RiderClip = clip; RiderClipStart = time;
            var spot = riderSpot; spot.y = 0;
            StandRider(spot);
            UpdateFloor();
        }

        /// CustomizationCamera SetCameraPosition / SetPartPosition / SetPostTransformOffset (0.5 s): eye, look-at point and an offset
        /// in the camera's own axes after the look-at (it moves the vehicle to the right of the lists).
        public void SetCamera(Vector3 cam, Vector3 part, Vector3 post)
        {
            camWanted = cam; targetWanted = part; this.post = post;
            camT = 0;
        }
        Vector3 post = new Vector3(-3.21f, -0.12f, -4.80f);

        // ---- the open shell's garage (OpenFrontEnd/OpenGarageStage.cs): the shown models' bounds, a camera set directly (its own orbit
        // smoothing), and a texture of the view's shape.
        /// The vehicle's bounds in the garage's own space (root at the origin); false when no vehicle is shown.
        public bool BikeBounds(out Bounds b)
        {
            b = default;
            if (bike == null || !bike.chassis) return false;
            b = Bounds(bike.chassis, bike.frontWheel ? bike.frontWheel.gameObject : null, bike.rearWheel ? bike.rearWheel.gameObject : null);
            if (b.size == Vector3.zero) return false;
            b.center -= root.transform.position;
            return true;
        }
        /// The rider's bounds in the garage's own space; false when no rider is shown.
        public bool RiderBounds(out Bounds b)
        {
            b = default;
            if (rider == null || !rider.root) return false;
            b = Bounds(rider.root);
            if (b.size == Vector3.zero) return false;
            b.center -= root.transform.position;
            return true;
        }
        /// The vehicle's front and rear wheel centres in the garage's own space (the vehicle's axis); false when it has no wheel pair.
        public bool BikeAxis(out Vector3 front, out Vector3 rear)
        {
            front = rear = default;
            if (bike == null || !bike.frontWheel || !bike.rearWheel) return false;
            front = bike.frontWheel.position - root.transform.position; rear = bike.rearWheel.position - root.transform.position;
            return (front - rear).sqrMagnitude > 0.01f;
        }
        /// The camera at eye looking at target (garage space) now, no easing and no post offset.
        public void SetCameraNow(Vector3 eye, Vector3 target)
        {
            camPos = camWanted = eye; camTarget = targetWanted = target; post = Vector3.zero; camT = 1;
            Place();
        }
        /// The vertical field of view in degrees.
        public float Fov { get => camera.fieldOfView; set => camera.fieldOfView = value; }
        /// The texture's size (the view it fills is not 16:9). Only when it changes.
        public void Resize(int width, int height)
        {
            if (!Texture) return;   // world mode: the level's camera draws the screen
            width = Mathf.Clamp(width, 64, 4096); height = Mathf.Clamp(height, 64, 4096);
            if (Texture.width == width && Texture.height == height) return;
            camera.targetTexture = null;
            Texture.Release(); Texture.width = width; Texture.height = height;
            camera.targetTexture = Texture;
        }

        public void Update(float dt)
        {
            Animate(dt);
            if (camT < 1) { camT = Mathf.Min(1, camT + dt / 0.5f); float e = camT * camT * (3 - 2 * camT); camPos = Vector3.Lerp(camPos, camWanted, e); camTarget = Vector3.Lerp(camTarget, targetWanted, e); }
            Place();
        }
        void Place()
        {
            if (world)
            {
                var rot = Quaternion.LookRotation(camTarget - camPos, Vector3.up);
                var o = root.transform.position;
                CameraSink?.Invoke(o + camPos + rot * post, o + camTarget + rot * post);
                return;
            }
            var t = camera.transform;
            t.localPosition = camPos; t.localRotation = Quaternion.LookRotation(camTarget - camPos, Vector3.up);
            t.localPosition += t.localRotation * post;
        }

        /// Renders now (edit-mode benches; in play mode the camera renders every frame).
        public void Render() { Place(); if (!world) camera.Render(); }

        public void Unload() { KillBike(); KillRider(); bikeKey = riderKey = null; UpdateFloor(); }
        void KillBike() { bikeRest = null; if (bike != null) { Kill(bike.chassis); Kill(bike.frontWheel ? bike.frontWheel.gameObject : null); Kill(bike.rearWheel ? bike.rearWheel.gameObject : null); } bike = null; }
        void KillRider() { riderAnim = null; if (rider != null) { Kill(rider.root); Kill(rider.proxyRoot ? rider.proxyRoot.gameObject : null); } rider = null; }
        /// The models' meshes, materials and objects (Destroy in play mode, DestroyImmediate in edit-mode benches). The object leaves the picture at
        /// once: Destroy only takes it at the end of the frame, and a picture drawn by hand in the same frame (the MOTOCARD portrait's single
        /// camera.Render after a new outfit was built) drew the old rider and the new one in the same place, their gear mixed.
        static void Kill(GameObject g)
        {
            if (!g) return;
            g.SetActive(false);
            void Gone(UnityEngine.Object o) { if (!o) return; if (Application.isPlaying) UnityEngine.Object.Destroy(o); else UnityEngine.Object.DestroyImmediate(o); }
            foreach (var f in g.GetComponentsInChildren<MeshFilter>(true)) Gone(f.sharedMesh);
            foreach (var sk in g.GetComponentsInChildren<SkinnedMeshRenderer>(true)) Gone(sk.sharedMesh);
            foreach (var r in g.GetComponentsInChildren<Renderer>(true)) foreach (var m in r.sharedMaterials) Gone(m);
            Gone(g);
        }

        // ---- the menu world's push-bike walk (OpenFrontEnd/OpenMenuWorld, Content/ReflexPushAnim): the vehicle moved as one piece by the clip's
        // OBJ_Frame track and its wheels turned by the distance rolled; the rider posed by matrices the walk computes, his idle held meanwhile.
        (Transform t, Vector3 pos, Quaternion rot)[] bikeRest;
        float frontSpin, rearSpin;
        void KeepBikeRest()
        {
            var ts = new[] { bike.chassis ? bike.chassis.transform : null, bike.frontWheel, bike.rearWheel };
            bikeRest = ts.Where(t => t).Select(t => (t, t.localPosition, t.localRotation)).ToArray();
            frontSpin = rearSpin = 0; BikeGroundLift = 0;
        }
        /// The vehicle's frame node (OBJ_Frame: the model's origin) at rest, garage space (zero without a vehicle).
        public Vector3 BikeRestOrigin => bikeRest != null && bikeRest.Length > 0 && bike?.chassis && bikeRest[0].t == bike.chassis.transform ? bikeRest[0].pos : Vector3.zero;
        /// The lowest point of the wheels now (world y; the root's height without wheels): the tyres' contact with the floor.
        public float WheelsLowestY()
        {
            float low = float.MaxValue;
            foreach (var w in new[] { bike?.frontWheel, bike?.rearWheel })
                if (w) foreach (var r in w.GetComponentsInChildren<Renderer>()) low = Mathf.Min(low, r.bounds.min.y);
            return low == float.MaxValue ? root.transform.position.y : low;
        }
        /// A wheel's radius (feet; half its tyre's height), 1.05 without one.
        public float WheelRadius(bool front)
        {
            var w = front ? bike?.frontWheel : bike?.rearWheel;
            if (!w) return 1.05f;
            Bounds b = default; bool any = false;
            foreach (var r in w.GetComponentsInChildren<Renderer>()) { if (!any) { b = r.bounds; any = true; } else b.Encapsulate(r.bounds); }
            return any ? Mathf.Max(0.3f, b.extents.y) : 1.05f;
        }
        /// Counts the ShowSpecs calls that built something (the menu world's walk ends when its models are replaced).
        public int BuildSerial { get; private set; }
        /// The rider spec last shown by ShowSpecs (null: none).
        public RiderSpec ShownRider { get; private set; }
        /// The rider drawn or not (the menu world's VEHICLE preview shows the vehicle alone; his pose and place are kept).
        public void SetRiderVisible(bool on)
        {
            // (the root object, not the renderers: every applied pose turns the gear's renderers on again, OriginalRider.ApplyBoneMatrices)
            if (rider == null || !rider.root || rider.root.activeSelf == on) return;
            rider.root.SetActive(on);
        }
        /// The vehicle spec last shown by ShowSpecs (null: none).
        public BikeSpec ShownBike { get; private set; }
        /// A motocross bike is shown (two wheels: the push-bike walk's vehicle; not an ATV, a truck or a buggy).
        public bool ShowsBike => bike != null && ShownBike != null && !ShownBike.atv && !ShownBike.machine;
        /// The garage bike stand of the HR models (MX_Stand materials) shown or hidden (the menu world's walk rolls the bike on its wheels).
        public void ShowStand(bool on)
        {
            if (bike == null) return;
            foreach (var p in bike.parts)
                if (p.part && p.material != null && p.material.IndexOf("Stand", StringComparison.OrdinalIgnoreCase) >= 0 && p.part.gameObject.activeSelf != on) p.part.gameObject.SetActive(on);
        }
        /// The wheel meshes of an ATV / truck / buggy / UTV (parts on a hierarchy wheel or tyre node; their wheel groups are empty) with the
        /// wheel centre they turn about (the axle's OBJ_*_Wheel node, model space = the chassis' local space). Empty for a bike.
        public List<(Transform part, Vector3 pivot)> WheelParts()
        {
            var list = new List<(Transform, Vector3)>();
            if (bike?.hierarchyNodes == null || bike.hierarchyBind == null) return list;
            var names = bike.hierarchyNodes;
            Vector3? Center(string node)
            {
                foreach (var axle in new[] { "FRT", "BCK" }) foreach (var side in new[] { "LFT", "RHT" })
                    if (node.Contains(axle) && node.Contains(side))
                    {
                        int k = Array.IndexOf(names, "OBJ_" + axle + "_" + side + "_Wheel");
                        if (k >= 0) return bike.hierarchyBind[k].GetColumn(3);
                    }
                return null;
            }
            foreach (var p in bike.parts)
            {
                if (!p.part || p.node < 0 || p.node >= names.Length) continue;
                string n = names[p.node];
                if ((n.IndexOf("Wheel", StringComparison.Ordinal) < 0 && n.IndexOf("Tire", StringComparison.Ordinal) < 0) || n.IndexOf("Steering", StringComparison.Ordinal) >= 0) continue;
                var c = Center(n);
                if (c != null) list.Add((p.part, c.Value));
            }
            return list;
        }
        /// The hierarchy node's bind world (model space; null without it): the driver's seat (NDE_RiderLocator, NDE_SeatNode), the grips.
        public Matrix4x4? HierarchyNode(params string[] names)
        {
            if (bike?.hierarchyNodes == null || bike.hierarchyBind == null) return null;
            foreach (var n in names) { int k = Array.IndexOf(bike.hierarchyNodes, n); if (k >= 0) return bike.hierarchyBind[k]; }
            return null;
        }
        /// The vehicle's class as the menu world's intro sees it: "bike", "atv" or "machine" (null: none).
        public string VehicleKind => bike == null || ShownBike == null ? null : ShownBike.atv ? "atv" : ShownBike.machine ? "machine" : "bike";
        /// The shown vehicle's chassis and front wheel (diagnostics: the menu world's motion log).
        public Transform BikeChassis => bike != null && bike.chassis ? bike.chassis.transform : null;
        public Transform BikeFrontWheel => bike != null && bike.frontWheel ? bike.frontWheel : null;
        /// The idle clip waits (the walk poses the rider).
        public bool HoldIdle;
        /// The rider's turn about the vertical in world mode (null: RiderYaw).
        public float? yawOverride;
        /// The shown rider (null: none).
        public ReflexRiderModel RiderModel => rider;
        /// Where the idle rider stands (garage space; y = the soles' offset StandRider found).
        public Vector3 RiderSpot => riderSpot;
        /// The vehicle at its rest placement moved by m (garage space: rest -> m * rest), its wheels turned about their axles by the angles
        /// (degrees, forward roll positive). Identity / 0: the rest pose.
        public void SetBikeMotion(Matrix4x4 m, float frontDegrees, float rearDegrees)
        {
            if (bikeRest == null) return;
            var rq = m.rotation; Vector3 rp = m.GetColumn(3);
            foreach (var (t, pos, rot) in bikeRest)
            {
                if (!t) continue;
                float spin = t == bike.frontWheel ? frontDegrees : t == bike.rearWheel ? rearDegrees : 0f;
                t.localPosition = rp + rq * pos;
                t.localRotation = rq * rot * Quaternion.Euler(spin, 0, 0);
            }
        }
        /// The rider posed by world matrices (OriginalBoneCount rows, the game's row-major layout); false when it could not be applied.
        public bool ApplyRiderWorld(float[] world) => rider != null && rider.rider && rider.rider.ApplyBoneMatrices(world, OriginalRider.OriginalBoneCount);
        /// The idle pose now (the clip at its time, on the rider's spot and turn) into outWorld without applying it; false without the clip.
        readonly float[] idleRoot = new float[16];
        public bool IdleWorld(float[] outWorld)
        {
            if (riderAnim == null || rider == null || !rider.root || riderNodeNames == null) return false;
            var place = Matrix4x4.TRS(root.transform.position + riderSpot, Quaternion.Euler(0, yawOverride ?? RiderYaw, 0), Vector3.one);
            var rr = idleRoot;
            for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) rr[i * 4 + k] = k == 3 ? 0 : place[k, i];
            rr[12] = place[0, 3]; rr[13] = place[1, 3]; rr[14] = place[2, 3]; rr[15] = 1;
            riderAnim.World(riderAnimT, riderNodeNames, rider.rider.boneParents, rr, outWorld);
            return true;
        }
        /// The idle pose applied again (after a walk).
        public void ResumeIdle() { HoldIdle = false; if (riderAnim != null && rider != null && rider.root) PoseRider(); }
        /// The lowest point of the rider's posed skin (world y; the root's height without a rider).
        public float RiderSoleY() => rider == null ? root.transform.position.y : SkinPoints(new System.Collections.Generic.List<Vector3>(), Vector3.zero);
        /// The rider's stance again (StandRider + MoveBeside: the spot the vehicle's vertices give), e.g. after yawOverride or besideLeft changed.
        public void Restand() { if (rider != null && rider.root) { var keep = riderAnimT; StandBeside(); riderAnimT = keep; UpdateFloor(); } }

        public void Destroy()
        {
            UnityEngine.Rendering.RenderPipelineManager.beginCameraRendering -= ClearTransparent;
            Unload();
            if (camera && !world) camera.targetTexture = null;
            if (Texture) { Texture.Release(); if (Application.isPlaying) UnityEngine.Object.Destroy(Texture); else UnityEngine.Object.DestroyImmediate(Texture); }
            if (root) { if (Application.isPlaying) UnityEngine.Object.Destroy(root); else UnityEngine.Object.DestroyImmediate(root); }
        }
    }
}
