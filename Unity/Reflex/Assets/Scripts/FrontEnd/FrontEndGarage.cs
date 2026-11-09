// MOTOCAREER's garage (Race and Ride -> GARAGE), ported from the shipping scripts GarageHUB.lua, GarageRiderCustomize.lua,
// GarageRiderCustomize_Cameras.lua, GarageHUB_Cameras.lua, PurchaseProductPopup.lua and InsufficientFundsPopup.lua (FrontEnd package,
// read at run time from the install). The tables are the game's: the profile's Player* tables as the profile session reports them
// (ReflexProfile; career.cpp profile_json) and the static MXTables_Default (FeTables.Shared). Changes go through the game
// (ReflexProfile.Run: career::command set_int / purchase_asset / store_not_new / save), so the game writes them into its save and the
// next career world (built from that save) races with them (OriginalOpponents / ReflexPlates read the running game's tables).
// The 3D preview is FeGarage (the MXGarage + CustomizationCamera stand-in of the Vehicle Select page).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    static class FeGarageData
    {
        public static ProfileData P => ReflexProfile.Current;
        public static FeTables S => FeTables.Shared;
        public static Dictionary<string, object> Unique(string table, string col, int v) => RowIndex.First(S.Rows(table), col, v);   // (indexed: w-menufps)
        /// MXTableHelper:StoreProductFromStoreAsset.
        public static int ProductOfAsset(int asset) => FeTables.I(Unique("StoreAssets", "StoreAssetId", asset), "StoreProductId");
        /// MXTableHelper:Product( id ) cell: StoreProducts.
        public static int ProductInt(int product, string col, int def = -1) => FeTables.I(Unique("StoreProducts", "StoreProductId", product), col, def);
        /// MXTableHelper:Price (StoreProducts.Price).
        public static int Price(int product) => ProductInt(product, "Price", 0);
        /// MXTableHelper:IsStoreAssetPurchased (PlayerAssetsPurchased).
        public static bool Purchased(int asset) => Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex ? CoreGarageProfile.AvailableAsset(asset) : P != null && P.T("PlayerAssetsPurchased").Find("StoreAssetId", asset) >= 0;
        public static int Balance => P?.T("PlayerRider").Int("AccountBalance", 0, 0) ?? 0;
        /// MXTableHelper:CanWithdrawFromAccount (AccountBalance - price >= 0).
        public static bool CanWithdraw(int price) => Balance - price >= 0;
        public static string Title(FrontEndManager fe, int product) { string k = S.TitleKey(product); return k == null ? "" : fe.assets.Str(k); }
        /// UpdateStoreProductBrand: StoreProducts.BrandId -> Brand.BrandIconId on the MXUIStreamableImageIcon.
        public static void Brand(FrontEndManager fe, FeNode icon, int product)
        {
            int id = -1; int b = ProductInt(product, "BrandId");
            if (b != -1) id = FeTables.I(Unique("Brand", "BrandId", b), "BrandIconId");
            if (icon == null) return;
            var (tex, uv) = id != -1 ? fe.assets.Icon(id) : (null, new Rect(0, 0, 1, 1));
            if (icon.image) { icon.image.texture = tex; icon.image.uvRect = uv; icon.image.Dirty(); }
            icon.Visible = id != -1 && tex != null;
        }
        /// The player's bike (PlayerRider.LastEquippedVehicle -> PlayerVehicle + PlayerVehicleParts) and gear (PlayerRiderGear).
        public static (BikeSpec bike, RiderSpec rider, int vehicleType) PlayerSpecs(IEnumerable<int> gearOverride = null)
        {
            var p = P; if (p == null) return (null, null, -1);
            var t = new ReflexGameTables(new FeTableSource(S));
            int pro = p.T("PlayerRider").Int("LastRiderId", 0, 0);
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && pro > 0 && gearOverride == null)
            {
                int vehicle = p.T("PlayerRider").Int("LastEquippedVehicle", 0);
                int proModel = FeTables.I(Unique("AIVehicle", "AIVehicleId", vehicle), "VehicleModelId");
                var proParts = S.Rows("AIVehicleParts").Where(x => FeTables.I(x, "AIVehicleId") == vehicle).Select(x => FeTables.I(x, "VehiclePartId"));
                int set = p.T("PlayerRider").Int("LastRiderGearSetId", 0);
                var proGear = S.Rows("RiderGearSetGear").Where(x => FeTables.I(x, "RiderGearSetId") == set).Select(x => FeTables.I(x, "RiderGearId"));
                return (ReflexVehicleSpecs.Bike(t, proModel, proParts), ReflexVehicleSpecs.Rider(t, proGear), FeTables.I(Unique("VehicleModel", "VehicleModelId", proModel), "VehicleTypeId"));
            }
            int last = p.T("PlayerRider").Int("LastEquippedVehicle", 0, 0); var pv = p.T("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", last);
            int model = row >= 0 ? pv.Int("VehicleModelId", row) : -1;
            var parts = p.T("PlayerVehicleParts"); var ids = Enumerable.Range(0, parts.rows).Where(i => parts.Int("PlayerVehicleId", i) == last).Select(i => parts.Int("VehiclePartId", i)).ToList();
            var gear = (gearOverride ?? Enumerable.Range(0, p.T("PlayerRiderGear").rows).Select(i => p.T("PlayerRiderGear").Int("RiderGearId", i))).ToList();
            BikeSpec bs = null; RiderSpec rs = null;
            try { if (model >= 0) bs = ReflexVehicleSpecs.Bike(t, model, ids); } catch (Exception e) { Debug.LogWarning("[Garage] bike: " + e.Message); }
            try { if (gear.Count > 0) rs = ReflexVehicleSpecs.Rider(t, gear); } catch (Exception e) { Debug.LogWarning("[Garage] rider: " + e.Message); }
            return (bs, rs, FeTables.I(Unique("VehicleModel", "VehicleModelId", model), "VehicleTypeId"));
        }
        public static void AttachTarget(FeGarage g, FeNode target)
        {
            if (g == null || target?.image == null) return;
            target.image.texture = g.Texture; target.image.uvRect = new Rect(0, 0, 1, 1); target.image.Dirty(); target.Visible = true;
        }
        static Vector3 V(float x, float y, float z) => new Vector3(x, y, z);
        /// GarageHUB_Cameras.lua g_garageHUBCameraInformationTable[vehicleTypeId] (camera, part, post offset); the default otherwise.
        public static (Vector3 cam, Vector3 part, Vector3 post) HubCamera(int vehicleTypeId)
        {
            switch (vehicleTypeId)
            {
                case 2: case 3: case 4: return (V(-3.59f, 2.84f, 6.53f), V(0.96f, 2.40f, -0.22f), V(-0.59f, 0.36f, -9.47f));
                case 11: return (V(-4.97f, 2.82f, 9.87f), V(1.36f, 2.40f, 0.74f), V(-0.37f, 0.84f, -7.08f));
                case 12: return (V(-8.78f, 2.82f, 15.02f), V(1.70f, 2.40f, -0.46f), V(-1.73f, 0.84f, -5.67f));
                case 14: return (V(-5.82f, 2.82f, 16.67f), V(3.99f, 2.40f, -0.20f), V(0.93f, 0.84f, -2.38f));
                case 15: return (V(-10.12f, 2.82f, 18.04f), V(0.99f, 2.40f, -0.19f), V(-2.03f, 0.84f, -2.38f));
                default: return (V(15, 5, 1), V(0, 0, 0), V(0, 0, 0));
            }
        }
        /// GarageRiderCustomize_Cameras.lua LookupRiderCameraInformation( riderGearType ) (null: the whole rider).
        public static (Vector3 cam, Vector3 part, Vector3 post) RiderCamera(string gearType)
        {
            switch (gearType)
            {
                case "Helmet": return (V(1.14f, 6.13f, 1.60f), V(0.06f, 5.33f, 0.01f), V(-0.38f, -0.11f, -1.39f));
                case "Goggle": return (V(-1.43f, 6.14f, 1.65f), V(-0.07f, 5.46f, 0.24f), V(-0.36f, -0.24f, -1.26f));
                case "Jersey": return (V(-3.56f, 2.23f, 4.69f), V(0.06f, 3.22f, 0.26f), V(-0.41f, -0.35f, -5.21f));
                case "Boots": return (V(-1.12f, 1.19f, 0.72f), V(-0.00f, 0.76f, -0.14f), V(-0.40f, -0.31f, -4.14f));
                case "ChestProtector": return (V(1.02f, 4.52f, 2.44f), V(0.20f, 4.28f, -0.09f), V(-0.07f, -0.08f, -1.90f));
                case "SkinTone": return (V(-0.70f, 5.07f, 1.75f), V(-0.33f, 5.18f, -0.41f), V(-0.19f, -0.03f, 0));
                case "Neckbrace": return (V(-1.23f, 5.63f, -2.71f), V(0.02f, 4.82f, -0.22f), V(-0.39f, -0.19f, -1.07f));
                default: return (V(-1.21f, 2.66f, 8.72f), V(-0.15f, 3.56f, 0.28f), V(-0.62f, -0.81f, -7.74f));
            }
        }
        /// App.FormatNumber (thousands separators).
        public static string Number(int v) => v.ToString("N0", System.Globalization.CultureInfo.InvariantCulture);
        /// The UIEventTrigger "PurchasePopupAccept" of the garage layer that opened the purchase popup.
        public static void RaisePurchaseAccept(FrontEndManager fe)
        {
            foreach (var name in new[] { "GarageRiderCustomize.lyr", "GarageVehicleCustomize.lyr", "GarageMyRides.lyr" })
            {
                var l = fe.Layer(name); if (l != null && l.active) l.script?.Call("OnPurchasePopupAccept");
            }
        }
    }

    // GarageHUB.lua (the Garage Page, GarageMenu.lyr): the player's rider (MXGarage RequestRider Player1) beside the last equipped
    // vehicle (RequestVehicle Player1, LastEquippedVehicle) in the garage view (GarageHUB_Cameras by the vehicle's type); "Garage List":
    // VEHICLE (GotoMyRidesPage) / RIDER (GotoCustomizeRiderPage) with the Vehicle/Rider Focused / Unfocused animations; misc MOTOCARD
    // (GotoMotocardPage), start TEST DRIVE, back (TravelBack).
    public sealed class GarageHubScript : FeScript
    {
        FeGarage garage; FeNode list, target; int shownVersion = -1;
        public FeGarage Garage => garage;
        public override void Initialize()
        {
            list = C("Garage List"); target = C("RenderTarget");
            functions["OnMotocardSelect"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("GotoMotocardPage"); };
            functions["OnStartPressed"] = OnStartPressed;
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("TravelBack"); };
            functions["OnVehicleAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("GotoMyRidesPage"); };
            functions["OnRiderAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("GotoCustomizeRiderPage"); };
            if (list != null) list.onListChanged = (l, old) => Focus(l.focusIndex, old);
        }
        public override void Start()
        {
            if (ReflexGarage.TestDrive.Active) { ReflexGarage.TestDrive.End(); fe.vars["TestDriveEnabled"] = 0; }   // back from the test drive (Test Drive Exit Page -> DoneLoading)
            fe.SetButtonLegend("back", true, "STR_BACK"); fe.SetButtonLegend("misc", true, "STR_MOTOCARD"); fe.SetButtonLegend("start", !Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex, "STR_TESTDRIVE");
            fe.SetHeader("STR_GARAGE");
            layer.Play("LoadingStart");
            try { garage ??= new FeGarage(fe.transform); } catch (Exception e) { Debug.LogWarning("[Garage] " + e.Message); }
            FeGarageData.AttachTarget(garage, target);
            shownVersion = -1;
        }
        public override void Update(float dt)
        {
            garage?.Update(dt);
            if (testDrive) { LaunchTestDrive(); return; }
            if (garage == null || ReflexProfile.Current == null || ReflexProfile.Version == shownVersion || ReflexProfile.Busy) return;
            shownVersion = ReflexProfile.Version;
            var (bike, rider, type) = FeGarageData.PlayerSpecs();
            garage.ShowSpecs(bike, rider, true);
            var c = FeGarageData.HubCamera(type); garage.SetCamera(c.cam, c.part, c.post);
            layer.Stop("LoadingStart"); layer.Play("LoadingDone");   // OnRiderLoaded + OnVehicleLoaded
        }
        void Focus(int i, int old)
        {
            if (i == old) return;
            if (i == 0) { layer.Stop("VehicleUnfocused"); layer.Play("VehicleFocused"); layer.Stop("RiderFocused"); layer.Play("RiderUnfocused"); }
            else { layer.Stop("VehicleFocused"); layer.Play("VehicleUnfocused"); layer.Stop("RiderUnfocused"); layer.Play("RiderFocused"); }
        }
        public override void End() { garage?.Destroy(); garage = null; }

        /// OnStartPressed (TEST DRIVE): mxGarage:Unload( ), App.EnableTestDrive( TRUE ) (UI TestDriveEnabled), App.SyncDatabaseWithManagers( ),
        /// "GotoLoadingPage" -> the TransitionPageGraph's Test Drive Loading Page (Background01 + LoadingScreen; its page script
        /// TestDriveTransitionPage:Present calls mxGarage:TestDriveStart( )). Here: the loading overlay (the same two layers) and the race
        /// scene with the test drive's world (ReflexGarage.TestDrive: UI_Compound booted from the save), once the garage pages' saves are written.
        void OnStartPressed()
        {
            if (!ReflexGarage.PluginHasTestDrive) { Debug.LogWarning("[Garage] Test drive is not available with this simulation backend."); fe.PlaySound("Locked"); return; }
            if (testDrive) return;
            garage?.Unload();
            fe.vars["TestDriveEnabled"] = 1;
            fe.PlaySound("Accept"); testDrive = true;
        }
        bool testDrive;
        void LaunchTestDrive()
        {
            if (!testDrive || fe.Busy || ReflexProfile.Busy) return;   // the world is booted from the save file
            testDrive = false;
            var run = ReflexGarage.TestDrive.Begin();
            fe.LaunchCareerEvent(run.ev);                 // the loading overlay and the race scene, without the arcade's remembered choice
            FrontEndManager.ReturnPage = "Garage Page";   // the pause request ends the test drive there (Test Drive Exit Page -> DoneLoading)
        }
    }

    // GarageRiderCustomize.lua (the Garage Customize Rider Page): the gear types (RiderGearType but SkinTone, sorted 3,1,4,2,11,9; their
    // names STR_RIDERGEARTYPE_<FactoryName>), the type's store list (RiderGear of the type, not hidden, by product line: a parent store
    // product is a "drill in" entry, its children the secondary list; sorted by title, "No Chest Protector" / "No Neckbrace" first), new /
    // equipped / has-children / DLC marks, the price (StoreProducts.Price, 0 = owned) of what is not purchased. Accept on a purchased
    // item equips it (PlayerRiderGear: the row of that gear type gets the RiderGearId; chest protector and neck brace exclude each
    // other: the other row gets 322 "No Neckbrace" / 13 "No Chest Protector"); on a product to buy: CanWithdrawFromAccount ? the
    // purchase popup : the insufficient funds popup; the popup's accept: WithdrawFromAccount, PurchaseStoreAsset, equip. Focusing a new
    // product clears its new flag (SetStoreProductNotNew). End: RequestSaveGame when something changed. The garage shows the rider
    // with the focused gear previewed, the camera per gear type (GarageRiderCustomize_Cameras).
    public sealed class GarageRiderCustomizeScript : FeScript
    {
        sealed class Entry { public int product, parent = -1, asset = -1, gear = -1; public bool isNew, children, purchased, dlc; }
        FeNode typeRoot, typeList, primRoot, primList, secRoot, secList, productLine, brand, cost, costText, target, balance;
        readonly List<(int id, string factory, string loc)> types = new List<(int, string, string)>();
        List<Entry> prim = new List<Entry>(), sec = new List<Entry>();
        List<int> equipped = new List<int>();
        int equippedParent = -1, primTop, secTop, previewGear = -1;
        bool equippable, purchasable, suppressPreview, needSave, typeActive, primActive, secActive;
        FeGarage garage; int shownVersion = -1; string shownKey;
        public bool TypeActive => typeActive; public bool PrimaryActive => primActive; public bool SecondaryActive => secActive;
        public int TypeIndex => typeList.absIndex; public int PrimaryIndex => primList.absIndex; public int SecondaryIndex => secList.absIndex;
        public int PrimaryCount => prim.Count; public int SecondaryCount => sec.Count;
        public string FocusedTypeFactory => types.Count > 0 ? types[typeList.absIndex].factory : null;
        public FeGarage Garage => garage;

        public override void Initialize()
        {
            typeRoot = C("GearTypeList"); typeList = typeRoot.Find("NavListContainer");
            primRoot = C("PrimaryStoreList"); primList = primRoot.Find("NavListContainer");
            secRoot = C("SecondaryStoreList"); secList = secRoot.Find("NavListContainer");
            productLine = C("SelectedProductLineContainer"); brand = C("BrandImageIcon");
            cost = C("CostContainer"); costText = cost?.Find("ProductCost"); target = C("RenderTarget");
            balance = C("PlayerAccountBalance.component");
            functions["OnAcceptGearTypeList"] = OnAcceptGearTypeList; functions["OnCancelGearTypeList"] = OnCancelGearTypeList;
            functions["OnAcceptPrimaryStoreList"] = OnAcceptPrimaryStoreList; functions["OnCancelPrimaryStoreList"] = OnCancelPrimaryStoreList;
            functions["OnAcceptSecondaryStoreList"] = OnAcceptSecondaryStoreList; functions["OnCancelSecondaryStoreList"] = OnCancelSecondaryStoreList;
            functions["OnPurchasePopupAccept"] = OnPurchasePopupAccept;
            functions["OnAccountBalanceChanged"] = UpdateBalance;
            typeList.onListChanged = (l, old) => OnGearTypeListFocusedChanged();
            primList.onListChanged = (l, old) => OnStoreListFocusedChanged(true);
            secList.onListChanged = (l, old) => OnStoreListFocusedChanged(false);
        }

        public override void Start()
        {
            typeActive = true; primActive = secActive = false;
            fe.SetButtonLegend("back", true, "STR_BACK"); fe.SetHeader("STR_GARAGE_RIDER");
            suppressPreview = true; equippedParent = -1; needSave = false; previewGear = -1;
            try { garage ??= new FeGarage(fe.transform); } catch (Exception e) { Debug.LogWarning("[Garage] " + e.Message); }
            FeGarageData.AttachTarget(garage, target);
            BuildPlayerRiderGearList();
            equippable = purchasable = false;
            PopulateGearTypeList();
            UpdateCamera(false);
            typeRoot.focusedFlag = true;
            primRoot.focusedFlag = false; primRoot.Visible = false;
            secRoot.focusedFlag = false; secRoot.Visible = false;
            if (productLine != null) productLine.Visible = false;
            OnStoreListFocusedChanged(true);
            UpdateBalance(); shownVersion = -1;
        }

        public override void End()
        {
            previewGear = -1;
            if (needSave) ReflexProfile.Run("save");   // App.RequestSaveGame
            garage?.Destroy(); garage = null;
        }

        public override void Update(float dt)
        {
            garage?.Update(dt);
            if (ReflexProfile.Version != shownVersion && !ReflexProfile.Busy && ReflexProfile.Current != null)
            {   // the game answered a change: its tables again
                shownVersion = ReflexProfile.Version; BuildPlayerRiderGearList(); UpdateBalance(); FormatAll();
            }
            ShowRider();
        }

        // ------------------------------------------------------------------ lists
        void BuildPlayerRiderGearList()
        {
            var t = FeGarageData.P?.T("PlayerRiderGear");
            equipped = t == null ? new List<int>() : Enumerable.Range(0, t.rows).Select(i => t.Int("RiderGearId", i)).ToList();
        }

        static readonly int[] TypeSort = { 3, 1, 4, 2, 11, 9 };
        void PopulateGearTypeList()
        {
            types.Clear();
            foreach (var r in FeGarageData.S.Rows("RiderGearType"))
            {
                string factory = FeTables.S(r, "FactoryName"); if (factory == "SkinTone") continue;
                types.Add((FeTables.I(r, "RiderGearTypeId"), factory, ("STR_RiderGearType_" + factory).ToUpperInvariant()));
            }
            int Rank(int id) { int i = Array.IndexOf(TypeSort, id); return i < 0 ? 1000 : i; }
            var sorted = types.Select((x, i) => (x, i)).OrderBy(p => Rank(p.x.id)).ThenBy(p => p.i).Select(p => p.x).ToList();
            types.Clear(); types.AddRange(sorted);
            typeList.numItems = types.Count; typeList.absIndex = 0;
            OnGearTypeListFocusedChanged();
        }

        List<Entry> BuildStoreList(int typeId, int requestedParent)
        {
            var list = new List<Entry>(); var p = FeGarageData.P;
            var rows = FeGarageData.S.Rows("RiderGear").Where(r => FeTables.I(r, "RiderGearTypeId") == typeId).Select(r => (gear: FeTables.I(r, "RiderGearId"), asset: FeTables.I(r, "StoreAssetId")))
                       .Select(x => (x.gear, x.asset, product: FeGarageData.ProductOfAsset(x.asset)))
                       .Select(x => (x.gear, x.asset, x.product, parent: FeGarageData.ProductInt(x.product, "ParentStoreProductId")))
                       .Where(x => requestedParent == -1 || x.parent == requestedParent)
                       .Select((x, i) => (x, i)).OrderBy(q => q.x.parent).ThenBy(q => q.i).Select(q => q.x).ToList();   // the parent's ascending sort
            int lastParent = -1; bool lastNew = false;
            foreach (var x in rows)
            {
                if (p != null && p.IsHidden(x.product)) continue;
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !CoreGarageProfile.AvailableAsset(x.asset)) continue;
                bool purchased = FeGarageData.Purchased(x.asset) || FeGarageData.Price(x.product) == 0;
                bool isNew = p != null && p.IsNew(x.product), dlc = p != null && p.IsDlc(x.product);
                if (equipped.Contains(x.gear)) equippedParent = x.parent;
                if (requestedParent != -1) list.Add(new Entry { product = x.product, parent = x.parent, asset = x.asset, isNew = isNew, gear = x.gear, purchased = purchased, dlc = dlc });
                else if (x.parent == -1) list.Add(new Entry { product = x.product, parent = -1, asset = x.asset, isNew = isNew, gear = x.gear, purchased = purchased, dlc = dlc });
                else if (x.parent != lastParent)
                {
                    if (lastParent != -1 && list.Count > 0) list[list.Count - 1].isNew = lastNew;
                    bool pNew = p != null && p.IsNew(x.parent);
                    list.Add(new Entry { product = x.parent, children = true, isNew = pNew, dlc = dlc });
                    lastNew = pNew;
                }
                lastParent = x.parent;
                if (!lastNew && isNew) lastNew = true;
            }
            // RiderGearListSort: "No Chest Protector" (378) / "No Neckbrace" (1722) first, then by title
            int Hack(Entry e) => e.product == 378 || e.product == 1722 ? 0 : 1;
            return list.Select((e, i) => (e, i)).OrderBy(q => Hack(q.e)).ThenBy(q => FeGarageData.Title(fe, q.e.product), StringComparer.Ordinal).ThenBy(q => q.i).Select(q => q.e).ToList();
        }

        void PopulatePrimaryStoreList()
        {
            prim = types.Count > 0 ? BuildStoreList(types[typeList.absIndex].id, -1) : new List<Entry>();
            primList.numItems = prim.Count; primList.absIndex = 0; primTop = 0;
            OnStoreListFocusedChanged(true);
        }
        void PopulateSecondaryStoreList()
        {
            sec = types.Count > 0 && prim.Count > 0 ? BuildStoreList(types[typeList.absIndex].id, prim[primList.absIndex].product) : new List<Entry>();
            secList.numItems = sec.Count; secList.absIndex = 0; secTop = 0;
            if (sec.Count > 0) OnStoreListFocusedChanged(false);
            FormatStore(secList, sec, ref secTop);
        }

        // ------------------------------------------------------------------ formatting
        void FormatAll() { FormatTypes(); FormatStore(primList, prim, ref primTop); FormatStore(secList, sec, ref secTop); }
        void FormatTypes()
        {
            // OnFormatGearTypeList: the scrolling list shows the focused type (Item0); arrows by position
            var item0 = typeList.Kid("Item0"); var item1 = typeList.Kid("Item1");
            string s = types.Count > 0 ? Str(types[typeList.absIndex].loc) : "";
            item0?.Find("Text")?.SetText(s); item0?.Find("TextShadow")?.SetText(s);
            if (item1 != null) item1.Visible = false;
            typeList.focusIndex = 0;
            var left = typeRoot.Kid("LeftArrow"); var right = typeRoot.Kid("RightArrow");
            if (left != null) left.Visible = typeList.absIndex > 0;
            if (right != null) right.Visible = typeList.absIndex < types.Count - 1;
        }
        void FormatStore(FeNode list, List<Entry> data, ref int top)
        {
            var slots = list.kids.Where(k => k.name.StartsWith("Item", StringComparison.Ordinal)).OrderBy(k => k.pos.y).ToList();
            int n = slots.Count, i = list.absIndex, count = data.Count;
            if (i < top) top = i; if (i >= top + n) top = i - n + 1; top = Mathf.Clamp(top, 0, Math.Max(0, count - n));
            for (int s = 0; s < n; s++)
            {
                int d = top + s; bool show = d < count; slots[s].Visible = show; if (!show) continue;
                var e = data[d];
                if (e.product >= 0) slots[s].Find("Text")?.SetText(FeGarageData.Title(fe, e.product));
                if (slots[s].Find("NewImage") is FeNode ni) ni.Visible = e.isNew;
                if (slots[s].Find("HasChildrenImage") is FeNode hc) hc.Visible = e.children;
                if (slots[s].Find("EquippedImage") is FeNode eq)
                    eq.Visible = e.gear != -1 ? equipped.Contains(e.gear) : equippedParent >= 0 && (equippedParent == e.product || equippedParent == e.parent);
                if (slots[s].Find("DLCImage") is FeNode dl) dl.Visible = e.dlc;
            }
            list.focusIndex = n == 0 ? -1 : list.kids.IndexOf(slots[Mathf.Clamp(i - top, 0, n - 1)]);
            var bar = list.parent?.Find("Scrollbar"); var slider = bar?.Kid("Slider");
            if (bar != null) bar.Visible = count > n;
            if (slider != null && count > n) { float h = bar.size.y, sh = Mathf.Max(8, h * n / count); slider.size.y = sh; slider.rt.sizeDelta = slider.size; slider.pos.y = (h - sh) * top / Math.Max(1, count - n); }
        }

        // ------------------------------------------------------------------ focus changes
        void OnGearTypeListFocusedChanged()
        {
            previewGear = -1;   // PreviewGear( -1, -1 )
            FeGarageData.Brand(fe, brand, -1);
            PopulatePrimaryStoreList();
            FormatTypes();
        }
        void OnStoreListFocusedChanged(bool primary)
        {
            var data = primary ? prim : sec; var list = primary ? primList : secList;
            if (list.absIndex < 0 || list.absIndex >= data.Count) { if (primary) FormatStore(primList, prim, ref primTop); return; }
            var e = data[list.absIndex];
            if (primary)
            {
                PopulateSecondaryStoreList();
                productLine?.Find("Text")?.SetText(FeGarageData.Title(fe, e.product));
            }
            if (e.isNew) { ReflexProfile.Run("store_not_new " + e.product); e.isNew = false; }   // SetStoreProductNotNew
            if (e.gear != -1 && e.asset != -1 && !e.children)
            {
                Preview(e.gear, e.product);
                if (e.purchased) { equippable = !equipped.Contains(e.gear); purchasable = false; }
                else { equippable = false; purchasable = true; }
            }
            UpdateProductCost(e);
            if (primary) FormatStore(primList, prim, ref primTop); else FormatStore(secList, sec, ref secTop);
        }

        // ------------------------------------------------------------------ input
        void OnAcceptGearTypeList()
        {
            typeRoot.focusedFlag = false; primRoot.focusedFlag = true; primRoot.Visible = true;
            fe.PlaySound("Accept"); suppressPreview = false;
            OnStoreListFocusedChanged(true);
            UpdateCamera(true);
            typeActive = false; primActive = true;
        }
        void OnCancelGearTypeList() { fe.SendUIEvent("TravelBack"); fe.PlaySound("Back"); typeActive = false; }
        void OnAcceptPrimaryStoreList()
        {
            if (prim.Count == 0) return;
            var e = prim[primList.absIndex];
            if (e.children)
            {
                primRoot.focusedFlag = false; primRoot.Visible = false; if (productLine != null) productLine.Visible = true;
                secRoot.focusedFlag = true; secRoot.Visible = true;
                fe.PlaySound("Accept");
                OnStoreListFocusedChanged(false);
                primActive = false; secActive = true;
                return;
            }
            if (equippable) { EquipGear(e.gear); e.isNew = false; FormatStore(primList, prim, ref primTop); return; }
            if (purchasable) PurchaseGear(e);
        }
        void OnCancelPrimaryStoreList()
        {
            typeRoot.focusedFlag = true; primRoot.focusedFlag = false; primRoot.Visible = false;
            if (cost != null) cost.Visible = false;
            fe.PlaySound("Back");
            previewGear = -1; FeGarageData.Brand(fe, brand, -1); suppressPreview = true;
            UpdateCamera(false);
            typeActive = true; primActive = false;
        }
        void OnAcceptSecondaryStoreList()
        {
            if (sec.Count == 0) return;
            var e = sec[secList.absIndex];
            if (equippable) { EquipGear(e.gear); e.isNew = false; FormatStore(secList, sec, ref secTop); return; }
            if (purchasable) PurchaseGear(e);
        }
        void OnCancelSecondaryStoreList()
        {
            primRoot.focusedFlag = true; primRoot.Visible = true; if (productLine != null) productLine.Visible = false;
            secRoot.focusedFlag = false; secRoot.Visible = false;
            if (cost != null) cost.Visible = false;
            fe.PlaySound("Back");
            primActive = true; secActive = false;
        }

        /// Tests: focus the first purchased, unequipped gear of the primary list (or the cheapest affordable one to buy when `buy`)
        /// and accept it as the list's Accept would. Returns the RiderGearId, -1 when there is none.
        public int TestAcceptFirst(bool buy)
        {
            int best = -1;
            for (int i = 0; i < prim.Count; i++)
            {
                var e = prim[i]; if (e.children || e.gear < 0) continue;
                if (!buy && e.purchased && !equipped.Contains(e.gear)) { best = i; break; }
                if (buy && !e.purchased && FeGarageData.CanWithdraw(FeGarageData.Price(e.product)) && (best < 0 || FeGarageData.Price(e.product) < FeGarageData.Price(prim[best].product))) best = i;
            }
            if (best < 0)
                for (int i = 0; i < prim.Count; i++)
                {   // a piece inside a product line: drill in (the primary list's accept), then the secondary list's accept
                    if (!prim[i].children) continue;
                    var kids = BuildStoreList(types[typeList.absIndex].id, prim[i].product);
                    int k = buy ? kids.FindIndex(e => e.gear >= 0 && !e.purchased && FeGarageData.CanWithdraw(FeGarageData.Price(e.product)))
                                : kids.FindIndex(e => e.gear >= 0 && e.purchased && !equipped.Contains(e.gear));
                    if (k < 0) continue;
                    primList.absIndex = i; OnStoreListFocusedChanged(true); OnAcceptPrimaryStoreList();
                    secList.absIndex = k; OnStoreListFocusedChanged(false); OnAcceptSecondaryStoreList();
                    return sec[k].gear;
                }
            if (best < 0) return -1;
            primList.absIndex = best; OnStoreListFocusedChanged(true);
            OnAcceptPrimaryStoreList();
            return prim[best].gear;
        }
        public void TestNextType() { if (typeList.absIndex + 1 < types.Count) { typeList.absIndex++; OnGearTypeListFocusedChanged(); } }

        Entry Focused => sec.Count > 0 && secActive ? sec[secList.absIndex] : prim.Count > 0 ? prim[primList.absIndex] : null;

        /// EquipGear's table part, run as the game's Lua in the profile session (career::command "lua"): the PlayerRiderGear row whose
        /// gear has this type gets the gear (else a row is added), and the chest protector / neck brace exclusion (the other type's
        /// row, when there is one, gets 322 "No Neckbrace" / 13 "No Chest Protector").
        public void EquipGear(int gearId)
        {
            var p = FeGarageData.P; if (p == null) return;
            int TypeOf(int gear) => FeTables.I(FeGarageData.Unique("RiderGear", "RiderGearId", gear), "RiderGearTypeId");
            int type = types.Count > 0 ? types[typeList.absIndex].id : TypeOf(gearId);
            var e = Focused; if (e != null) equippedParent = e.parent;
            string lua =
                "local t = App.LookupPlayerTable( \"PlayerRiderGear\", 0 ); " +
                "local q = App.RdbQuery( ); q:AddOperation_int( \"RiderGearId.RiderGearTypeId\", App.Equals, " + type + " ); " +
                "local rows = t:query( q ); rows:EnumReset( ); local added = FALSE; " +
                "if( rows:GetSize( ) > 0 ) then local r = rows:EnumNext( ); if( r ~= nil and r:isNull( ) == FALSE ) then r:setCell_int( \"RiderGearId\", " + gearId + " ); added = TRUE; end end " +
                "if( added == FALSE ) then local a = App.RdbQuery( ); a:AddOperation_int( \"RiderGearId\", App.Equals, " + gearId + " ); t:addRow( a ); end ";
            if (type == 9 || type == 11)
            {
                int other = type == 9 ? 11 : 9, none = type == 9 ? 322 : 13;
                lua += "local q2 = App.RdbQuery( ); q2:AddOperation_int( \"RiderGearId.RiderGearTypeId\", App.Equals, " + other + " ); " +
                       "local rows2 = t:query( q2 ); rows2:EnumReset( ); if( rows2:GetSize( ) > 0 ) then local r2 = rows2:EnumNext( ); " +
                       "if( r2 ~= nil and r2:isNull( ) == FALSE ) then r2:setCell_int( \"RiderGearId\", " + none + " ); end end";
            }
            needSave = true;
            var changed = ReflexProfile.EquipGear(gearId, lua);
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !changed.Result) return;
            // the list marks until the game's tables come back
            var t = p.T("PlayerRiderGear");
            int row = Enumerable.Range(0, t.rows).Where(i => TypeOf(t.Int("RiderGearId", i)) == type).DefaultIfEmpty(-1).First();
            if (row >= 0 && row < equipped.Count) equipped[row] = gearId; else equipped.Add(gearId);
            FormatAll();
            Debug.Log("[Garage] equipped rider gear " + gearId);
        }

        void PurchaseGear(Entry e)
        {
            int price = FeGarageData.Price(e.product);
            if (!FeGarageData.CanWithdraw(price)) { fe.PlaySound("Accept"); fe.SendUIEvent("ShowInsufficientFundsPopup"); return; }
            int parent = FeGarageData.ProductInt(e.product, "ParentStoreProductId");
            PurchaseProductPopupScript.ParentName = FeGarageData.Title(fe, parent == -1 ? e.product : parent);   // DYN_STOREPRODUCT_PARENT_NAME
            PurchaseProductPopupScript.ProductName = parent == -1 ? "" : FeGarageData.Title(fe, e.product);     // DYN_STOREPRODUCT_NAME
            fe.PlaySound("Accept"); fe.SendUIEvent("ShowPurchasePopup");
        }

        /// OnPurchasePopupAccept: WithdrawFromAccount, PurchaseStoreAsset, equip.
        void OnPurchasePopupAccept()
        {
            var e = Focused; if (e == null) return;
            int price = FeGarageData.Price(e.product), bal = FeGarageData.Balance;
            if (bal - price < 0) return;
            ReflexProfile.Run($"set_int PlayerRider 0 AccountBalance {bal - price}", "purchase_asset " + e.asset);
            needSave = true; e.purchased = true; e.isNew = false;
            EquipGear(e.gear);
            fe.PlaySound("Purchase");
            equippable = false; purchasable = false;
            UpdateProductCost(e);
            Debug.Log("[Garage] bought store asset " + e.asset + " for " + price);
        }

        // ------------------------------------------------------------------ preview, camera, cost
        void Preview(int gear, int product)
        {
            if (suppressPreview) { FeGarageData.Brand(fe, brand, -1); return; }
            FeGarageData.Brand(fe, brand, product);
            previewGear = gear;
        }
        void ShowRider()
        {
            if (garage == null || FeGarageData.P == null) return;
            var gear = new List<int>(equipped);
            if (previewGear >= 0)
            {   // MXGarage:PreviewRiderGear: the gear of that type replaced
                int TypeOf(int g) => FeTables.I(FeGarageData.Unique("RiderGear", "RiderGearId", g), "RiderGearTypeId");
                int type = TypeOf(previewGear); int i = gear.FindIndex(g => TypeOf(g) == type);
                if (i >= 0) gear[i] = previewGear; else gear.Add(previewGear);
            }
            string key = string.Join(",", gear);
            if (key == shownKey) return;
            shownKey = key;
            var (_, rider, _) = FeGarageData.PlayerSpecs(gear);
            layer.Stop("LoadingDone"); layer.Play("LoadingStart");
            garage.ShowSpecs(null, rider, false);   // RemoveVehicle: the rider alone
            layer.Stop("LoadingStart"); layer.Play("LoadingDone");   // OnRiderLoaded (the models are built synchronously)
        }
        void UpdateCamera(bool useListData)
        {
            var c = FeGarageData.RiderCamera(useListData && types.Count > 0 ? types[typeList.absIndex].factory : null);
            garage?.SetCamera(c.cam, c.part, c.post);
        }
        void UpdateProductCost(Entry e)
        {
            bool show = !e.children && !e.purchased;
            if (show) costText?.SetText(FeGarageData.Number(FeGarageData.Price(e.product)));
            if (cost != null) cost.Visible = show;
        }
        void UpdateBalance()
        {   // PlayerAccountBalance: Start sets UI AccountBalance; OnAccountBalanceChanged: DYN_ACCOUNT_BALANCE = FormatNumber( it )
            if (balance?.Find("AccountBalance") is FeNode a) a.SetText(FeGarageData.Number(FeGarageData.Balance));
            if (balance?.Find("AccountBalanceShadow") is FeNode s) s.SetText(FeGarageData.Number(FeGarageData.Balance));
        }
    }

    // PurchaseProductPopup.lua (PurchasePopup.lyr): EMB_PURCHASE_POPUP_MESSAGE (the product line and product names), YES (focused) /
    // NO; YES -> TravelBack and End: "PurchasePopupAccept" (the garage layer's UIEventTrigger -> OnPurchasePopupAccept).
    public sealed class PurchaseProductPopupScript : FeScript
    {
        public static string ParentName = "", ProductName = "";
        bool accept;
        public override void Initialize()
        {
            functions["OnAcceptButtonOneKeypress"] = () => { fe.PlaySound("Accept"); accept = true; fe.SendUIEvent("PopupPageStack_Pop"); };
            functions["OnAcceptButtonTwoKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("PopupPageStack_Pop"); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("PopupPageStack_Pop"); };
        }
        static Dictionary<string, string> embeds;
        public override void Start()
        {
            fe.PlaySound("PopupShow"); accept = false;
            embeds ??= FeCareerScripts.Embeds(fe.assets.data.Read(fe.assets.data.Pkg("FrontEnd"), "localiz", "MXRaven_Default_Strings"));
            string template = embeds.TryGetValue("EMB_PURCHASE_POPUP_MESSAGE", out var t) ? t : Str("EMB_PURCHASE_POPUP_MESSAGE");
            // the embedded text: its first line, then the lines that embed DYN_STOREPRODUCT_PARENT_NAME and DYN_STOREPRODUCT_NAME
            var lines = (template ?? "").Split('\n');
            C("MessageText")?.SetText(lines[0].TrimEnd() + "\n" + ParentName + (string.IsNullOrEmpty(ProductName) ? "" : "\n" + ProductName));
            var list = C("ButtonList"); if (list != null) list.focusIndex = 0;
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End() { if (accept) { accept = false; fe.SendUIEvent("PurchasePopupAccept"); FeGarageData.RaisePurchaseAccept(fe); } }
    }

    // GarageMyRides.lua (the Garage My Rides Page): the vehicle types (VehicleModel rows whose product is not hidden, by VehicleType
    // ShortName, sorted 2,3,4,11,14,12,15; their Name), the type's models (not hidden; locked / new / purchased / DLC; sorted unlocked
    // first, new first, then by title), the equipped one marked (PlayerVehicle of LastEquippedVehicle). Focus on the equipped one allows
    // customization and tuning (m_AllowCustomize / m_AllowTuning; the start legend STR_GARAGE_VEHICLE_TUNE). Accept: the equipped one ->
    // GotoCustomizeVehiclePage; a purchased one -> EquipVehicle (MXTableHelper:EquipVehicle + SetPlayerFavoriteVehicle: career::command
    // favorite_vehicle); a locked one -> the locked popup; else the purchase (CanWithdrawFromAccount ? the purchase popup : insufficient
    // funds; accept: WithdrawFromAccount, PurchaseStoreAsset - the game adds the PlayerVehicle with the model's VehicleModelParts - and
    // equip). The garage shows the focused model: the player's vehicle with its parts when purchased (VehiclePerformanceDisplay:
    // UpdateVehicleStats( PlayerVehicleId, Player1 )), else the model with its VehicleModelParts (owner "MX"; UpdateVehicleModelStats);
    // GarageVehiclePresentation_Cameras by type. Start (OnEnableVehicleTuning) -> ShowVehicleTuningPopup; its VehicleTuningComplete -> the
    // stats again. Toggle (OnQuickViewPopup) -> ShowVehiclesQuickViewPopup; its QuickViewPlayerVehicleId -> OnSelectPlayerVehicleId (favourite
    // + EquipVehicle, the list on it). Misc (ToggleFreeLook) once the vehicle is loaded: FadeOut, the camera's free look around the vehicle
    // (distance LookupVehiclePresentationCameraDistance, longitude 0..60; the stick / arrow keys turn it), Back returns (FadeIn). End:
    // RequestSaveGame when changed.
    public sealed class GarageMyRidesScript : FeScript
    {
        sealed class Item { public int model, asset, product; public bool locked, isNew, purchased, favorite, dlc; }
        FeNode typeRoot, typeList, vehRoot, vehList, brand, cost, costText, target, balance, perfNode;
        readonly List<int> types = new List<int>(); List<Item> items = new List<Item>();
        int equippedModel = -1, vehTop; bool needSave, allowCustomize, allowTuning, allowFreeLook, freeLook, typeActive, vehActive, suppressRequest, loading, suspendAudio;
        FeGarage garage; string shownKey; int shownVersion = -1;
        FeVehiclePerformance perf;
        float lookYaw, lookPitch = 20;
        public bool VehicleListActive => vehActive; public int EquippedModel => equippedModel;
        public bool AllowTuning => allowTuning; public bool AllowFreeLook => allowFreeLook; public bool FreeLook => freeLook;
        public FeGarage Garage => garage; public FeVehiclePerformance Performance => perf;
        public int FocusedModel => items.Count > 0 ? items[Mathf.Clamp(vehList.absIndex, 0, items.Count - 1)].model : -1;

        public override void Initialize()
        {
            typeRoot = C("VehicleTypeList"); typeList = typeRoot.Find("NavListContainer");
            vehRoot = C("VehicleList"); vehList = vehRoot.Find("NavListContainer");
            brand = C("BrandImageIcon"); cost = C("CostContainer"); costText = cost?.Find("ProductCost"); target = C("RenderTarget");
            balance = C("PlayerAccountBalance.component"); perfNode = C("VehiclePerformanceDisplay.component");
            functions["OnAcceptVehicleTypeList"] = OnAcceptVehicleTypeList;
            functions["OnCancelVehicleTypeList"] = () => { if (!freeLook) { fe.SendUIEvent("TravelBack"); fe.PlaySound("Back"); } typeActive = false; };
            functions["OnAcceptVehicleList"] = OnAcceptVehicleList; functions["OnCancelVehicleList"] = OnCancelVehicleList;
            functions["OnTravelBack"] = OnTravelBack;
            functions["OnPurchasePopupAccept"] = OnPurchasePopupAccept;
            functions["OnQuickViewPopup"] = OnQuickViewPopup;
            functions["ToggleFreeLook"] = ToggleFreeLook;
            functions["OnEnableVehicleTuning"] = OnEnableVehicleTuning;
            functions["OnVehicleTuningComplete"] = OnVehicleTuningComplete; functions["OnVehicleQuickViewComplete"] = OnVehicleQuickViewComplete;
            functions["OnSelectPlayerVehicleId"] = OnSelectPlayerVehicleId;
            functions["OnAccountBalanceChanged"] = UpdateBalance;
            typeList.onListChanged = (l, old) => OnVehicleTypeListFocusedChanged();
            vehList.onListChanged = (l, old) => OnVehicleListFocusedChanged();
        }

        public override void Start()
        {
            typeActive = true; vehActive = false;
            fe.SetButtonLegend("back", true, "STR_BACK"); fe.SetButtonLegend("toggle", true, "STR_QUICKVIEW");
            fe.SetButtonLegend("start", false, "STR_GARAGE_VEHICLE_TUNE"); fe.SetButtonLegend("dev", false, "STR_EMPTY");
            fe.SetHeader("STR_GARAGE_VEHICLE_MYRIDES");
            suppressRequest = true; loading = false; allowTuning = false; needSave = false; suspendAudio = false; freeLook = false; allowFreeLook = false;
            fe.vars.Remove("QuickViewPlayerVehicleId");
            try { garage ??= new FeGarage(fe.transform); } catch (Exception e) { Debug.LogWarning("[Garage] " + e.Message); }
            FeGarageData.AttachTarget(garage, target);   // mxGarage:RemoveRider( )
            allowCustomize = false;
            perf = perfNode != null ? new FeVehiclePerformance(perfNode, FeGarageData.S, FeGarageData.P) : null;
            equippedModel = EquippedModelOf();
            typeRoot.focusedFlag = true; vehRoot.focusedFlag = false; vehRoot.Visible = false;
            PopulateVehicleTypeList();
            OnSelectPlayerVehicleId();
            UpdateBalance();
        }
        public override void End()
        {
            freeLook = false; allowFreeLook = false;
            if (needSave) ReflexProfile.Run("save");   // App.RequestSaveGame
            garage?.Destroy(); garage = null;
        }
        public override void Update(float dt)
        {
            garage?.Update(dt);
            if (ReflexProfile.Version != shownVersion && !ReflexProfile.Busy && ReflexProfile.Current != null)
            {   // the game answered a change: its tables again
                shownVersion = ReflexProfile.Version; equippedModel = EquippedModelOf(); UpdateBalance(); FormatVehicles();
                perf = perfNode != null ? new FeVehiclePerformance(perfNode, FeGarageData.S, FeGarageData.P) : null;
                if (items.Count > 0) ShowStats(items[Mathf.Clamp(vehList.absIndex, 0, items.Count - 1)]);
            }
            // the popups' UI variables (UIVariableTriggers of GarageMyRides.lyr)
            if (fe.TopPopup == null)
            {
                if (fe.vars.ContainsKey("VehicleTuningComplete")) OnVehicleTuningComplete();
                if (fe.vars.ContainsKey("VehicleQuickViewComplete")) OnVehicleQuickViewComplete();
                if (fe.vars.ContainsKey("QuickViewPlayerVehicleId")) OnSelectPlayerVehicleId();
            }
            if (freeLook) FreeLookInput(dt);
            ShowVehicle();
        }

        static int EquippedModelOf()
        {
            var p = FeGarageData.P; if (p == null) return -1;
            // A pro selection stores an AIVehicleId, whose numeric value may also name an owned player vehicle.
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && p.T("PlayerRider").Int("LastRiderId", 0, 0) > 0) return -1;
            var pv = p.T("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", p.T("PlayerRider").Int("LastEquippedVehicle", 0, 0));
            return row >= 0 ? pv.Int("VehicleModelId", row) : -1;
        }

        static readonly int[] TypeSort = { 2, 3, 4, 11, 14, 12, 15 };
        void PopulateVehicleTypeList()
        {
            var p = FeGarageData.P; var unique = new List<(string name, int id)>();
            foreach (var r in FeGarageData.S.Rows("VehicleModel"))
            {
                int product = FeGarageData.ProductOfAsset(FeTables.I(r, "StoreAssetId"));
                if (p != null && p.IsHidden(product)) continue;
                int vt = FeTables.I(r, "VehicleTypeId");
                unique.Add((FeTables.S(FeGarageData.Unique("VehicleType", "VehicleTypeId", vt), "ShortName"), vt));
            }
            int Rank(int id) { int i = Array.IndexOf(TypeSort, id); return i < 0 ? 1000 : i; }
            types.Clear(); string last = null;
            foreach (var u in unique.Select((x, i) => (x, i)).OrderBy(q => Rank(q.x.id)).ThenBy(q => q.i).Select(q => q.x)) { if (u.name != last) types.Add(u.id); last = u.name; }
            typeList.numItems = types.Count; typeList.absIndex = 0;
            OnVehicleTypeListFocusedChanged();
        }

        void PopulateVehicleList()
        {
            var p = FeGarageData.P; items = new List<Item>();
            if (types.Count > 0)
                foreach (var r in FeGarageData.S.Rows("VehicleModel").Where(r => FeTables.I(r, "VehicleTypeId") == types[typeList.absIndex]))
                {
                    int model = FeTables.I(r, "VehicleModelId"), asset = FeTables.I(r, "StoreAssetId"); if (asset == -1) continue;
                    int product = FeGarageData.ProductOfAsset(asset); if (p != null && p.IsHidden(product)) continue;
                    if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && (p == null || p.T("PlayerVehicle").Find("VehicleModelId", model) < 0)) continue;
                    var it = new Item { model = model, asset = asset, product = product, locked = p == null || p.IsLocked(product), isNew = p != null && p.IsNew(product), purchased = FeGarageData.Purchased(asset), dlc = p != null && p.IsDlc(product) };
                    if (it.purchased && p != null) { var pv = p.T("PlayerVehicle"); int row = pv.Find("VehicleModelId", model); it.favorite = row >= 0 && pv.Int("Favorite", row, 0) != 0; }
                    items.Add(it);
                }
            // GarageMyRidesVehicleListSort: unlocked first, new first, then by title
            items = items.Select((x, i) => (x, i)).OrderBy(q => q.x.locked ? 1 : 0).ThenBy(q => q.x.isNew ? 0 : 1).ThenBy(q => FeGarageData.Title(fe, q.x.product), StringComparer.Ordinal).ThenBy(q => q.i).Select(q => q.x).ToList();
            int sel = Math.Max(0, items.FindIndex(x => x.model == equippedModel || x.favorite));
            vehList.numItems = items.Count; vehList.absIndex = items.Count == 0 ? 0 : sel; vehTop = 0;
            OnVehicleListFocusedChanged();
        }

        void FormatTypes()
        {
            var item0 = typeList.Kid("Item0"); var item1 = typeList.Kid("Item1");
            string s = types.Count > 0 ? Str(FeTables.S(FeGarageData.Unique("VehicleType", "VehicleTypeId", types[typeList.absIndex]), "Name") ?? "") : "";
            item0?.Find("Text")?.SetText(s); item0?.Find("TextShadow")?.SetText(s);
            if (item1 != null) item1.Visible = false;
            typeList.focusIndex = 0;
            var left = typeRoot.Kid("LeftArrow"); var right = typeRoot.Kid("RightArrow");
            if (left != null) left.Visible = typeList.absIndex > 0;
            if (right != null) right.Visible = typeList.absIndex < types.Count - 1;
        }
        void FormatVehicles()
        {
            var slots = vehList.kids.Where(k => k.name.StartsWith("Item", StringComparison.Ordinal)).OrderBy(k => k.pos.y).ToList();
            int n = slots.Count, i = vehList.absIndex, count = items.Count;
            if (i < vehTop) vehTop = i; if (i >= vehTop + n) vehTop = i - n + 1; vehTop = Mathf.Clamp(vehTop, 0, Math.Max(0, count - n));
            for (int s = 0; s < n; s++)
            {
                int d = vehTop + s; bool show = d < count; slots[s].Visible = show; if (!show) continue;
                var it = items[d];
                slots[s].Find("Text")?.SetText(FeGarageData.Title(fe, it.product));
                if (slots[s].Find("LockedImage") is FeNode lk) lk.Visible = it.locked;
                if (slots[s].Find("NewImage") is FeNode nw) nw.Visible = it.isNew && !it.locked;
                if (slots[s].Find("EquippedImage") is FeNode eq) eq.Visible = it.model == equippedModel;
                if (slots[s].Find("DLCImage") is FeNode dl) dl.Visible = it.dlc;
            }
            vehList.focusIndex = n == 0 ? -1 : vehList.kids.IndexOf(slots[Mathf.Clamp(i - vehTop, 0, n - 1)]);
            var bar = vehList.parent?.Find("Scrollbar"); var slider = bar?.Kid("Slider");
            if (bar != null) bar.Visible = count > n;
            if (slider != null && count > n) { float h = bar.size.y, sh = Mathf.Max(8, h * n / count); slider.size.y = sh; slider.rt.sizeDelta = slider.size; slider.pos.y = (h - sh) * vehTop / Math.Max(1, count - n); }
        }

        void OnVehicleTypeListFocusedChanged() { PopulateVehicleList(); UpdateCamera(); FormatTypes(); }
        void OnVehicleListFocusedChanged()
        {
            if (items.Count == 0) { if (cost != null) cost.Visible = false; FormatVehicles(); UpdateButtonLegend(); return; }
            var it = items[vehList.absIndex];
            if (vehRoot.focusedFlag)   // m_VehicleRootList:IsFocused( )
            {
                bool updateCost = false;
                if (equippedModel == it.model) { allowCustomize = true; allowTuning = true; }      // equipped: customization and tuning
                else if (it.purchased) { allowCustomize = false; allowTuning = false; }            // purchased: equip
                else { allowCustomize = false; allowTuning = false; updateCost = true; }            // locked (the warning) or for sale
                if (it.isNew) { ReflexProfile.Run("store_not_new " + it.product); it.isNew = false; }   // SetStoreProductNotNew
                if (updateCost) costText?.SetText(FeGarageData.Number(FeGarageData.Price(it.product)));
                if (cost != null) cost.Visible = updateCost;
            }
            else if (cost != null) cost.Visible = false;
            FormatVehicles();
            RequestNewVehicle();
            UpdateButtonLegend();
        }

        /// RequestNewVehicle: the brand, the stats (the player's vehicle or the model's defaults), the garage's vehicle (LoadingStart until loaded).
        void RequestNewVehicle()
        {
            if (suppressRequest || items.Count == 0) return;
            var it = items[Mathf.Clamp(vehList.absIndex, 0, items.Count - 1)];
            FeGarageData.Brand(fe, brand, it.product);
            if (!loading) { layer.Stop("LoadingStart"); layer.Play("LoadingStart"); loading = true; }
            ShowStats(it);
            allowFreeLook = false;
            shownKey = null;   // mxGarage:RequestVehicle( owner, id ) (ShowVehicle builds it)
        }
        void ShowStats(Item it)
        {
            if (perf == null) return;
            var p = FeGarageData.P; int row = it.purchased && p != null ? p.T("PlayerVehicle").Find("VehicleModelId", it.model) : -1;
            if (row >= 0) perf.UpdateVehicleStats(p.T("PlayerVehicle").Int("PlayerVehicleId", row), ReflexWorldFiles.Garage.Player1);
            else perf.UpdateVehicleModelStats(it.model);
        }

        void OnAcceptVehicleTypeList()
        {
            typeRoot.focusedFlag = false; vehRoot.focusedFlag = true; vehRoot.Visible = true;
            if (!suspendAudio) fe.PlaySound("Accept");
            typeActive = false; vehActive = true;
            suppressRequest = true; OnVehicleListFocusedChanged(); suppressRequest = false;
            if (!loading) allowFreeLook = true;
            UpdateButtonLegend();
        }
        void OnCancelVehicleList()
        {
            typeRoot.focusedFlag = true; vehRoot.focusedFlag = false; vehRoot.Visible = false;
            if (cost != null) cost.Visible = false;
            fe.PlaySound("Back"); allowTuning = false; allowFreeLook = false; UpdateButtonLegend();
            typeActive = true; vehActive = false;
        }
        void OnTravelBack()
        {
            if (allowFreeLook && freeLook) { EnableFreeLook(false); fe.PlaySound("Back"); }   // previewing the vehicle: back to the list
            else if (!freeLook) { fe.SendUIEvent("TravelBack"); fe.PlaySound("Back"); }
        }
        void OnAcceptVehicleList()
        {
            if (items.Count == 0 || freeLook) return;
            var it = items[vehList.absIndex];
            if (allowCustomize) { fe.PlaySound("Accept"); fe.SendUIEvent("GotoCustomizeVehiclePage"); }
            else if (it.purchased) { fe.PlaySound("Accept"); EquipVehicle(it.model); it.isNew = false; FormatVehicles(); }
            else if (it.locked) { fe.PlaySound("Locked"); fe.SendUIEvent("ShowLockedPopup"); }
            else
            {
                int price = FeGarageData.Price(it.product);
                if (!FeGarageData.CanWithdraw(price)) fe.SendUIEvent("ShowInsufficientFundsPopup");
                else
                {
                    int parent = FeGarageData.ProductInt(it.product, "ParentStoreProductId");
                    PurchaseProductPopupScript.ParentName = FeGarageData.Title(fe, parent == -1 ? it.product : parent);
                    PurchaseProductPopupScript.ProductName = parent == -1 ? "" : FeGarageData.Title(fe, it.product);
                    fe.SendUIEvent("ShowPurchasePopup");
                }
            }
            UpdateButtonLegend();
        }

        /// EquipVehicle: the player's vehicle of that model equipped (SetPlayerFavoriteVehicle: PlayerRider.LastEquippedVehicle).
        public void EquipVehicle(int model)
        {
            var p = FeGarageData.P; if (p == null) return;
            var pv = p.T("PlayerVehicle"); int row = pv.Find("VehicleModelId", model);
            if (row < 0) { Debug.LogWarning("[Garage] no PlayerVehicle of model " + model); return; }
            var changed = ReflexProfile.Run("favorite_vehicle " + pv.Int("PlayerVehicleId", row));
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !changed.Result) { Debug.LogWarning("[Garage] " + ReflexProfile.Error); return; }
            equippedModel = model; allowCustomize = true; allowTuning = true; needSave = true;
            FormatVehicles(); UpdateButtonLegend();
            Debug.Log("[Garage] equipped vehicle model " + model + " (PlayerVehicleId " + pv.Int("PlayerVehicleId", row) + ")");
        }

        void OnPurchasePopupAccept()
        {
            if (items.Count == 0) return;
            var it = items[vehList.absIndex];
            int price = FeGarageData.Price(it.product), bal = FeGarageData.Balance;
            if (bal - price < 0) return;
            it.purchased = true; it.isNew = false; needSave = true;
            if (cost != null) cost.Visible = false;
            fe.PlaySound("Purchase");
            int model = it.model;
            // WithdrawFromAccount, PurchaseStoreAsset (the game adds the PlayerVehicle), then EquipVehicle on the new tables
            ReflexProfile.Run($"set_int PlayerRider 0 AccountBalance {bal - price}", "purchase_asset " + it.asset).ContinueWith(_ => pendingEquip = model);
            Debug.Log("[Garage] bought vehicle model " + model + " for " + price);
        }
        volatile int pendingEquip = -1;

        // ------------------------------------------------------------------ the popups and free look
        void OnEnableVehicleTuning()
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && EquippedModelOf() < 0) return;
            if (allowCustomize && !freeLook && allowTuning) { fe.SendUIEvent("ShowVehicleTuningPopup"); fe.PlaySound("Accept"); }
        }
        void OnVehicleTuningComplete()
        {
            fe.vars.Remove("VehicleTuningComplete");
            if (!allowCustomize || items.Count == 0 || perf == null) return;
            var p = FeGarageData.P; int row = p?.T("PlayerVehicle").Find("VehicleModelId", items[vehList.absIndex].model) ?? -1;
            int id = row >= 0 ? p.T("PlayerVehicle").Int("PlayerVehicleId", row) : -1;
            perf.UpdateVehicleStats(id, ReflexWorldFiles.Garage.Player1);
            if (GarageVehicleTuningScript.LastWritten is ReflexGarage.Tuning t && perf.Shown.ContainsKey("engine"))
            {   // the tables the game answers next (ReflexProfile) - until then the popup's values
                var s = perf.Shown;
                perf.UpdateAttribute("engine", s["engine"][0], s["engine"][1], t.engine1, t.engine2);
                perf.UpdateAttribute("suspension", s["suspension"][0], s["suspension"][1], t.suspension1, t.suspension2);
                perf.UpdateAttribute("tires", s["tires"][0], s["tires"][1], t.tire1, t.tire2);
            }
        }
        void OnQuickViewPopup() { if (!freeLook) { fe.PlaySound("Accept"); fe.SendUIEvent("ShowVehiclesQuickViewPopup"); } }
        void OnVehicleQuickViewComplete() { fe.vars.Remove("VehicleQuickViewComplete"); }

        /// OnSelectPlayerVehicleId: the quick view's choice (favourite + EquipVehicle), else the profile's last equipped vehicle; its type
        /// selected and accepted, the list on it.
        void OnSelectPlayerVehicleId()
        {
            suspendAudio = true;
            var p = FeGarageData.P; int playerVehicleId;
            if (fe.vars.TryGetValue("QuickViewPlayerVehicleId", out var q) && q is int qv)
            {
                fe.vars.Remove("QuickViewPlayerVehicleId"); playerVehicleId = qv;
                ReflexProfile.Run("favorite_vehicle " + playerVehicleId); needSave = true;   // SetPlayerFavoriteVehicle (+ EquipVehicle on the Player List row)
            }
            else playerVehicleId = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && p != null && p.T("PlayerRider").Int("LastRiderId", 0, 0) > 0
                ? -1 : p?.T("PlayerRider").Int("LastEquippedVehicle", 0, 0) ?? -1;
            var pv = p?.T("PlayerVehicle"); int row = pv?.Find("PlayerVehicleId", playerVehicleId) ?? -1;
            if (row >= 0) equippedModel = pv.Int("VehicleModelId", row);
            int type = FeTables.I(FeGarageData.Unique("VehicleModel", "VehicleModelId", equippedModel), "VehicleTypeId");
            suppressRequest = true;
            int ti = types.IndexOf(type); if (ti >= 0) { typeList.absIndex = ti; OnVehicleTypeListFocusedChanged(); }
            if (typeRoot.focusedFlag) OnAcceptVehicleTypeList(); else PopulateVehicleList();
            suppressRequest = false; suspendAudio = false;
            RequestNewVehicle();
        }

        void ToggleFreeLook() { if (allowFreeLook && !freeLook) EnableFreeLook(true); }
        void EnableFreeLook(bool on)
        {
            if (!allowFreeLook) { UpdateButtonLegend(); return; }
            if (freeLook && !on && !layer.IsPlaying("FadeOut")) { freeLook = false; layer.Play("FadeIn"); fe.PlaySound("Back"); vehRoot.focusedFlag = true; fe.SetButtonLegend("back", true, "STR_BACK"); }
            else if (!freeLook && on && !layer.IsPlaying("FadeIn")) { freeLook = true; layer.Play("FadeOut"); fe.PlaySound("Accept"); vehRoot.focusedFlag = false; fe.SetButtonLegend("back", true, "STR_BACK"); lookYaw = 0; lookPitch = 20; }
            UpdateCamera();
            UpdateButtonLegend();
        }
        /// The free-look camera's controller: the stick / arrows turn it around the vehicle (CustomizationCamera free look).
        void FreeLookInput(float dt)
        {
            var kb = UnityEngine.InputSystem.Keyboard.current; var gp = UnityEngine.InputSystem.Gamepad.current;
            Vector2 d = Vector2.zero;
            if (kb != null) { if (kb.leftArrowKey.isPressed) d.x -= 1; if (kb.rightArrowKey.isPressed) d.x += 1; if (kb.upArrowKey.isPressed) d.y += 1; if (kb.downArrowKey.isPressed) d.y -= 1; }
            if (gp != null) d += gp.rightStick.ReadValue() + gp.leftStick.ReadValue();
            if (d == Vector2.zero) return;
            lookYaw += d.x * 90 * dt; lookPitch = Mathf.Clamp(lookPitch + d.y * 60 * dt, 0, 60);   // SetMinimum / MaximumFreeLookLongitude( 0 / 60 )
            UpdateCamera();
        }
        void UpdateButtonLegend()
        {
            if (!freeLook)
            {
                fe.SetButtonLegend("misc", allowFreeLook, "STR_VEHICLE_VIEW");
                fe.SetButtonLegend("toggle", true, "STR_QUICKVIEW");
                fe.SetButtonLegend("start", allowTuning, "STR_GARAGE_VEHICLE_TUNE");
            }
            else { fe.SetButtonLegend("misc", false, "STR_VEHICLE_VIEW"); fe.SetButtonLegend("toggle", false, "STR_QUICKVIEW"); fe.SetButtonLegend("start", false, "STR_GARAGE_VEHICLE_TUNE"); }
        }

        // ------------------------------------------------------------------ preview
        void ShowVehicle()
        {
            if (pendingEquip >= 0 && !ReflexProfile.Busy) { int m = pendingEquip; pendingEquip = -1; EquipVehicle(m); }
            if (garage == null || items.Count == 0 || suppressRequest) return;
            var it = items[Mathf.Clamp(vehList.absIndex, 0, items.Count - 1)];
            var p = FeGarageData.P; List<int> parts; string key;
            int pvRow = it.purchased && p != null ? p.T("PlayerVehicle").Find("VehicleModelId", it.model) : -1;
            if (pvRow >= 0)
            {   // the player's vehicle (owner Player1)
                int id = p.T("PlayerVehicle").Int("PlayerVehicleId", pvRow); var pp = p.T("PlayerVehicleParts");
                parts = Enumerable.Range(0, pp.rows).Where(i => pp.Int("PlayerVehicleId", i) == id).Select(i => pp.Int("VehiclePartId", i)).ToList();
            }
            else parts = FeGarageData.S.Rows("VehicleModelParts").Where(r => FeTables.I(r, "VehicleModelId") == it.model).Select(r => FeTables.I(r, "VehiclePartId")).ToList();   // owner "MX": the model's parts
            key = it.model + ":" + string.Join(",", parts);
            if (key == shownKey) return;
            shownKey = key;
            if (!loading) { layer.Stop("LoadingDone"); layer.Play("LoadingStart"); }
            BikeSpec bs = null;
            try { bs = ReflexVehicleSpecs.Bike(new ReflexGameTables(new FeTableSource(FeGarageData.S)), it.model, parts); } catch (Exception e) { Debug.LogWarning("[Garage] vehicle: " + e.Message); }
            garage.ShowSpecs(bs, null, false);   // RemoveRider
            // OnVehicleLoaded (the models are built synchronously)
            layer.Stop("LoadingStart"); layer.Stop("LoadingDone"); layer.Play("LoadingDone"); loading = false;
            if (!typeRoot.focusedFlag) { allowFreeLook = true; UpdateButtonLegend(); }
        }
        static readonly Dictionary<int, (Vector3 cam, Vector3 part, Vector3 post, float distance)> Presentation = new Dictionary<int, (Vector3, Vector3, Vector3, float)>
        {   // GarageVehiclePresentation_Cameras.lua g_vehiclePresentationCameraInformationTable: camera, vehicle, post offset, free-look distance
            [2] = (new Vector3(-8.04f, -0.64f, -1.74f), new Vector3(-0.17f, 0.02f, -0.14f), new Vector3(-2.62f, 0.07f, -9.15f), 15),
            [3] = (new Vector3(-8.04f, -0.64f, -1.74f), new Vector3(-0.17f, 0.02f, -0.14f), new Vector3(-2.62f, 0.07f, -9.15f), 15),
            [4] = (new Vector3(-18.23f, -1.01f, -2.96f), new Vector3(0.13f, 0.04f, -0.24f), new Vector3(-2.65f, 0.08f, 0.79f), 15),
            [11] = (new Vector3(-19.30f, -0.52f, -3.48f), new Vector3(0.06f, 1.08f, -0.52f), new Vector3(-4.54f, 0.12f, -6.12f), 24),
            [12] = (new Vector3(-21.43f, -2.94f, -2.52f), new Vector3(10.05f, -0.29f, 3.68f), new Vector3(-5.66f, 1.89f, -6.18f), 30),
            [14] = (new Vector3(-18.66f, -1.35f, -3.34f), new Vector3(5.42f, 0.85f, 1.77f), new Vector3(-5.81f, 0.28f, -7.69f), 30),
            [15] = (new Vector3(-20.09f, -1.74f, -2.21f), new Vector3(5.42f, 0.85f, 1.77f), new Vector3(-6.27f, 0.48f, -8.31f), 35),
            [16] = (new Vector3(-8.17f, -0.30f, -1.63f), new Vector3(0.13f, 0.04f, -0.24f), new Vector3(-3.04f, -0.16f, -1.29f), 10),
            [17] = (new Vector3(-7.46f, -0.05f, -0.95f), new Vector3(-0.17f, 0.02f, -0.14f), new Vector3(-2.95f, 0, -1.19f), 10),
        };
        void UpdateCamera()
        {
            int type = types.Count > 0 ? types[typeList.absIndex] : 2;
            (Vector3 cam, Vector3 part, Vector3 post, float distance) c = Presentation.TryGetValue(type, out var x) ? x : (new Vector3(15, 5, 1), Vector3.zero, Vector3.zero, 25f);   // the Lua's fallbacks
            var lift = new Vector3(0, 1.8f, 0);   // the garage puts the vehicle on its floor; the game's presentation looks at its middle
            if (freeLook)
            {   // SetEnableFocusedMode( FALSE ), post offset and part position 0, SetViewDistance( distance ), longitude 0..60
                var dir = Quaternion.Euler(lookPitch, lookYaw + 180, 0) * Vector3.forward;
                garage?.SetCamera(lift - dir * c.distance, lift, Vector3.zero);
            }
            else garage?.SetCamera(c.cam + lift, c.part + lift, c.post);
        }
        void UpdateBalance()
        {
            if (balance?.Find("AccountBalance") is FeNode a) a.SetText(FeGarageData.Number(FeGarageData.Balance));
            if (balance?.Find("AccountBalanceShadow") is FeNode s) s.SetText(FeGarageData.Number(FeGarageData.Balance));
        }

        /// Tests: the vehicle list on the equipped vehicle, as the page starts (true when it is).
        public bool OnEquipped => items.Count > 0 && items[Mathf.Clamp(vehList.absIndex, 0, items.Count - 1)].model == equippedModel && vehActive;
    }

    // GarageVehicleCustomize.lua (the Garage Customize Vehicle Page, the equipped vehicle): the part types of the model
    // (VehicleModelParts -> VehiclePartType with AllowCustomization, not Frame; sorted 11,23,22,7,12,9,16,10,1,15,8,3; names
    // STR_VEHICLEPARTTYPE_<FactoryName>), the type's store list (VehicleModelParts of the model and type, not hidden; product lines as
    // the rider page), equipped / new / has-children / DLC marks, the price of what is not purchased. Accept on a purchased part equips
    // it (EquipPart's tables, run as the game's Lua: the PlayerVehicleParts row of that type gets it, else a row is added, and a
    // tuning part writes its DefaultTuneAttribute1/2 into PlayerVehicleTuning); on one to buy: the purchase popup or insufficient
    // funds; the popup's accept: WithdrawFromAccount, PurchaseStoreAsset, equip. The garage shows the vehicle with the focused part
    // previewed, cameras GarageVehicleCustomize_Cameras (MX types) / the presentation camera. End: RequestSaveGame when changed.
    public sealed class GarageVehicleCustomizeScript : FeScript
    {
        sealed class Entry { public int product, parent = -1, asset = -1, part = -1, tune1, tune2; public bool isNew, children, purchased, dlc; }
        FeNode typeRoot, typeList, primRoot, primList, secRoot, secList, productLine, brand, cost, costText, target, balance;
        readonly List<(int id, string factory, string tuning, string loc)> types = new List<(int, string, string, string)>();
        List<Entry> prim = new List<Entry>(), sec = new List<Entry>();
        List<int> equipped = new List<int>();
        int vehicleId = -1, modelId = -1, vehicleType = -1, equippedParent = -1, primTop, secTop, previewPart = -1;
        bool equippable, purchasable, suppressPreview, needSave, typeActive, primActive, secActive;
        FeGarage garage; int shownVersion = -1; string shownKey;
        public FeGarage Garage => garage; public int TypeCount => types.Count;

        public override void Initialize()
        {
            typeRoot = C("PartTypeList"); typeList = typeRoot.Find("NavListContainer");
            primRoot = C("PrimaryStoreList"); primList = primRoot.Find("NavListContainer");
            secRoot = C("SecondaryStoreList"); secList = secRoot.Find("NavListContainer");
            productLine = C("SelectedProductLineContainer"); brand = C("BrandImageIcon");
            cost = C("CostContainer"); costText = cost?.Find("ProductCost"); target = C("RenderTarget");
            balance = C("PlayerAccountBalance.component");
            functions["OnAcceptPartTypeList"] = OnAcceptPartTypeList; functions["OnCancelPartTypeList"] = () => { fe.SendUIEvent("TravelBack"); fe.PlaySound("Back"); typeActive = false; };
            functions["OnAcceptPrimaryStoreList"] = OnAcceptPrimaryStoreList; functions["OnCancelPrimaryStoreList"] = OnCancelPrimaryStoreList;
            functions["OnAcceptSecondaryStoreList"] = OnAcceptSecondaryStoreList; functions["OnCancelSecondaryStoreList"] = OnCancelSecondaryStoreList;
            functions["OnPurchasePopupAccept"] = OnPurchasePopupAccept;
            functions["OnAccountBalanceChanged"] = UpdateBalance;
            typeList.onListChanged = (l, old) => OnPartTypeListFocusedChanged();
            primList.onListChanged = (l, old) => OnStoreListFocusedChanged(true);
            secList.onListChanged = (l, old) => OnStoreListFocusedChanged(false);
        }

        public override void Start()
        {
            typeActive = true; primActive = secActive = false;
            fe.SetButtonLegend("back", true, "STR_BACK"); fe.SetHeader("STR_GARAGE_VEHICLE_CUSTOMIZE");
            suppressPreview = true; equippedParent = -1; needSave = false; previewPart = -1;
            var p = FeGarageData.P;
            vehicleId = p?.T("PlayerRider").Int("LastEquippedVehicle", 0, 0) ?? -1;
            var pv = p?.T("PlayerVehicle"); int row = pv?.Find("PlayerVehicleId", vehicleId) ?? -1;
            modelId = row >= 0 ? pv.Int("VehicleModelId", row) : -1;
            vehicleType = FeTables.I(FeGarageData.Unique("VehicleModel", "VehicleModelId", modelId), "VehicleTypeId");
            try { garage ??= new FeGarage(fe.transform); } catch (Exception e) { Debug.LogWarning("[Garage] " + e.Message); }
            FeGarageData.AttachTarget(garage, target);
            BuildPlayerPartList();
            equippable = purchasable = false;
            PopulatePartTypeList();
            UpdateCamera(false);
            typeRoot.focusedFlag = true;
            primRoot.focusedFlag = false; primRoot.Visible = false;
            secRoot.focusedFlag = false; secRoot.Visible = false;
            if (productLine != null) productLine.Visible = false;
            if (types.Count == 1) OnAcceptPartTypeList();
            if (cost != null) cost.Visible = false;
            layer.Stop("LoadingStart"); layer.Stop("LoadingDone"); layer.Play("LoadingHide");
            UpdateBalance(); shownVersion = -1;
        }
        public override void End() { previewPart = -1; if (needSave) ReflexProfile.Run("save"); garage?.Destroy(); garage = null; }
        public override void Update(float dt)
        {
            garage?.Update(dt);
            if (ReflexProfile.Version != shownVersion && !ReflexProfile.Busy && ReflexProfile.Current != null) { shownVersion = ReflexProfile.Version; BuildPlayerPartList(); UpdateBalance(); FormatAll(); }
            ShowVehicle();
        }

        void BuildPlayerPartList()
        {
            var t = FeGarageData.P?.T("PlayerVehicleParts");
            equipped = t == null ? new List<int>() : Enumerable.Range(0, t.rows).Where(i => t.Int("PlayerVehicleId", i) == vehicleId).Select(i => t.Int("VehiclePartId", i)).ToList();
        }
        static int PartType(int part) => FeTables.I(FeGarageData.Unique("VehiclePart", "VehiclePartId", part), "VehiclePartTypeId");

        static readonly int[] TypeSort = { 11, 23, 22, 7, 12, 9, 16, 10, 1, 15, 8, 3 };
        void PopulatePartTypeList()
        {
            types.Clear(); int last = int.MinValue;
            var rows = FeGarageData.S.Rows("VehicleModelParts").Where(r => FeTables.I(r, "VehicleModelId") == modelId).Select((r, i) => (r, i))
                       .OrderBy(q => PartType(FeTables.I(q.r, "VehiclePartId"))).ThenBy(q => q.i).Select(q => q.r);
            foreach (var r in rows)
            {
                int tid = PartType(FeTables.I(r, "VehiclePartId"));
                var tr = FeGarageData.Unique("VehiclePartType", "VehiclePartTypeId", tid);
                string factory = FeTables.S(tr, "FactoryName");
                if (FeTables.I(tr, "AllowCustomization", 0) == 0) continue;
                if (last != tid && factory != "Frame")
                {
                    string tuning = FeTables.S(tr, "TuningAttribute"); if (string.IsNullOrEmpty(tuning)) tuning = null;
                    types.Add((tid, factory, tuning, ("STR_VehiclePartType_" + factory).ToUpperInvariant()));
                    last = tid;
                }
            }
            int Rank(int id) { int i = Array.IndexOf(TypeSort, id); return i < 0 ? 1000 : i; }
            var sorted = types.Select((x, i) => (x, i)).OrderBy(q => Rank(q.x.id)).ThenBy(q => q.i).Select(q => q.x).ToList();
            types.Clear(); types.AddRange(sorted);
            typeList.numItems = types.Count; typeList.absIndex = 0;
            OnPartTypeListFocusedChanged();
        }

        List<Entry> BuildStoreList(int typeId, int requestedParent)
        {
            var list = new List<Entry>(); var p = FeGarageData.P;
            var rows = FeGarageData.S.Rows("VehicleModelParts").Where(r => FeTables.I(r, "VehicleModelId") == modelId && PartType(FeTables.I(r, "VehiclePartId")) == typeId)
                       .Select(r => (part: FeTables.I(r, "VehiclePartId"), asset: FeTables.I(r, "StoreAssetId")))
                       .Select(x => (x.part, x.asset, product: FeGarageData.ProductOfAsset(x.asset)))
                       .Select(x => (x.part, x.asset, x.product, parent: FeGarageData.ProductInt(x.product, "ParentStoreProductId")))
                       .Where(x => requestedParent == -1 || x.parent == requestedParent)
                       .Select((x, i) => (x, i)).OrderBy(q => q.x.parent).ThenBy(q => q.i).Select(q => q.x).ToList();
            int lastParent = -1; bool lastNew = false;
            foreach (var x in rows)
            {
                if (p != null && p.IsHidden(x.product)) continue;
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !CoreGarageProfile.AvailableAsset(x.asset)) continue;
                bool purchased = FeGarageData.Purchased(x.asset) || FeGarageData.Price(x.product) == 0;
                bool isNew = p != null && p.IsNew(x.product), dlc = p != null && p.IsDlc(x.product);
                if (equipped.Contains(x.part)) equippedParent = x.parent;
                var pr = FeGarageData.Unique("VehiclePart", "VehiclePartId", x.part);
                int t1 = FeTables.I(pr, "DefaultTuneAttribute1", 0), t2 = FeTables.I(pr, "DefaultTuneAttribute2", 0);
                if (requestedParent != -1) list.Add(new Entry { product = x.product, parent = x.parent, asset = x.asset, isNew = isNew, part = x.part, purchased = purchased, tune1 = t1, tune2 = t2, dlc = dlc });
                else if (x.parent == -1) list.Add(new Entry { product = x.product, parent = -1, asset = x.asset, isNew = isNew, part = x.part, purchased = purchased, tune1 = t1, tune2 = t2, dlc = dlc });
                else if (x.parent != lastParent)
                {
                    if (lastParent != -1 && list.Count > 0) list[list.Count - 1].isNew = lastNew;
                    bool pNew = p != null && p.IsNew(x.parent);
                    list.Add(new Entry { product = x.parent, children = true, isNew = pNew, tune1 = t1, tune2 = t2, dlc = dlc });
                    lastNew = pNew;
                }
                lastParent = x.parent;
                if (!lastNew && isNew) lastNew = true;
            }
            // VehiclePartListSort: by title
            return list.Select((e, i) => (e, i)).OrderBy(q => FeGarageData.Title(fe, q.e.product), StringComparer.Ordinal).ThenBy(q => q.i).Select(q => q.e).ToList();
        }

        void PopulatePrimaryStoreList()
        {
            prim = types.Count > 0 ? BuildStoreList(types[typeList.absIndex].id, -1) : new List<Entry>();
            primList.numItems = prim.Count; primList.absIndex = 0; primTop = 0;
            OnStoreListFocusedChanged(true);
        }
        void PopulateSecondaryStoreList()
        {
            sec = types.Count > 0 && prim.Count > 0 ? BuildStoreList(types[typeList.absIndex].id, prim[primList.absIndex].product) : new List<Entry>();
            secList.numItems = sec.Count; secList.absIndex = 0; secTop = 0;
            if (sec.Count > 0) OnStoreListFocusedChanged(false);
            FormatStore(secList, sec, ref secTop);
        }

        void FormatAll() { FormatTypes(); FormatStore(primList, prim, ref primTop); FormatStore(secList, sec, ref secTop); }
        void FormatTypes()
        {
            var item0 = typeList.Kid("Item0"); var item1 = typeList.Kid("Item1");
            string s = types.Count > 0 ? Str(types[typeList.absIndex].loc) : "";
            item0?.Find("Text")?.SetText(s); item0?.Find("TextShadow")?.SetText(s);
            if (item1 != null) item1.Visible = false;
            typeList.focusIndex = 0;
            var left = typeRoot.Kid("LeftArrow"); var right = typeRoot.Kid("RightArrow");
            if (left != null) left.Visible = typeList.absIndex > 0;
            if (right != null) right.Visible = typeList.absIndex < types.Count - 1;
        }
        void FormatStore(FeNode list, List<Entry> data, ref int top)
        {
            var slots = list.kids.Where(k => k.name.StartsWith("Item", StringComparison.Ordinal)).OrderBy(k => k.pos.y).ToList();
            int n = slots.Count, i = list.absIndex, count = data.Count;
            if (i < top) top = i; if (i >= top + n) top = i - n + 1; top = Mathf.Clamp(top, 0, Math.Max(0, count - n));
            for (int s = 0; s < n; s++)
            {
                int d = top + s; bool show = d < count; slots[s].Visible = show; if (!show) continue;
                var e = data[d];
                if (e.product >= 0) slots[s].Find("Text")?.SetText(FeGarageData.Title(fe, e.product));
                if (slots[s].Find("NewImage") is FeNode ni) ni.Visible = e.isNew;
                if (slots[s].Find("HasChildrenImage") is FeNode hc) hc.Visible = e.children;
                if (slots[s].Find("EquippedImage") is FeNode eq)
                    eq.Visible = e.part != -1 ? equipped.Contains(e.part) : equippedParent >= 0 && (equippedParent == e.product || equippedParent == e.parent);
                if (slots[s].Find("DLCImage") is FeNode dl) dl.Visible = e.dlc;
            }
            list.focusIndex = n == 0 ? -1 : list.kids.IndexOf(slots[Mathf.Clamp(i - top, 0, n - 1)]);
            var bar = list.parent?.Find("Scrollbar"); var slider = bar?.Kid("Slider");
            if (bar != null) bar.Visible = count > n;
            if (slider != null && count > n) { float h = bar.size.y, sh = Mathf.Max(8, h * n / count); slider.size.y = sh; slider.rt.sizeDelta = slider.size; slider.pos.y = (h - sh) * top / Math.Max(1, count - n); }
        }

        void OnPartTypeListFocusedChanged() { previewPart = -1; FeGarageData.Brand(fe, brand, -1); PopulatePrimaryStoreList(); FormatTypes(); }
        void OnStoreListFocusedChanged(bool primary)
        {
            var data = primary ? prim : sec; var list = primary ? primList : secList;
            if (list.absIndex < 0 || list.absIndex >= data.Count) { if (primary) FormatStore(primList, prim, ref primTop); return; }
            var e = data[list.absIndex];
            if (primary) { PopulateSecondaryStoreList(); productLine?.Find("Text")?.SetText(FeGarageData.Title(fe, e.product)); }
            if (e.isNew) { ReflexProfile.Run("store_not_new " + e.product); e.isNew = false; }
            if (e.part != -1 && e.asset != -1 && !e.children)
            {
                if (!suppressPreview) { FeGarageData.Brand(fe, brand, e.product); previewPart = e.part; } else FeGarageData.Brand(fe, brand, -1);
                if (e.purchased) { equippable = !equipped.Contains(e.part); purchasable = false; } else { equippable = false; purchasable = true; }
            }
            bool show = !e.children && !e.purchased;
            if (show) costText?.SetText(FeGarageData.Number(FeGarageData.Price(e.product)));
            if (cost != null) cost.Visible = show;
            if (primary) FormatStore(primList, prim, ref primTop); else FormatStore(secList, sec, ref secTop);
        }

        void OnAcceptPartTypeList()
        {
            typeRoot.focusedFlag = false; primRoot.focusedFlag = true; primRoot.Visible = true;
            fe.PlaySound("Accept"); suppressPreview = false;
            OnStoreListFocusedChanged(true); UpdateCamera(true);
            typeActive = false; primActive = true;
        }
        void OnAcceptPrimaryStoreList()
        {
            if (prim.Count == 0) return;
            var e = prim[primList.absIndex];
            if (e.children)
            {
                primRoot.focusedFlag = false; primRoot.Visible = false; if (productLine != null) productLine.Visible = true;
                secRoot.focusedFlag = true; secRoot.Visible = true; fe.PlaySound("Accept");
                OnStoreListFocusedChanged(false); primActive = false; secActive = true; return;
            }
            if (equippable) { EquipPart(e); e.isNew = false; FormatStore(primList, prim, ref primTop); return; }
            if (purchasable) Purchase(e);
        }
        void OnCancelPrimaryStoreList()
        {
            typeRoot.focusedFlag = true; primRoot.focusedFlag = false; primRoot.Visible = false;
            if (cost != null) cost.Visible = false; fe.PlaySound("Back");
            previewPart = -1; FeGarageData.Brand(fe, brand, -1); suppressPreview = true; UpdateCamera(false);
            typeActive = true; primActive = false;
        }
        void OnAcceptSecondaryStoreList()
        {
            if (sec.Count == 0) return;
            var e = sec[secList.absIndex];
            if (equippable) { EquipPart(e); e.isNew = false; FormatStore(secList, sec, ref secTop); return; }
            if (purchasable) Purchase(e);
        }
        void OnCancelSecondaryStoreList()
        {
            primRoot.focusedFlag = true; primRoot.Visible = true; if (productLine != null) productLine.Visible = false;
            secRoot.focusedFlag = false; secRoot.Visible = false; if (cost != null) cost.Visible = false;
            fe.PlaySound("Back"); primActive = true; secActive = false;
        }
        Entry Focused => sec.Count > 0 && secActive ? sec[secList.absIndex] : prim.Count > 0 ? prim[primList.absIndex] : null;

        /// EquipPart's tables, as the game's Lua in the profile session.
        void EquipPart(Entry e)
        {
            if (types.Count == 0) return;
            var ty = types[typeList.absIndex]; equippedParent = e.parent;
            string lua =
                "local t = App.LookupPlayerTable( \"PlayerVehicleParts\", 0 ); " +
                "local query = App.RdbQuery( ); query:AddOperation_int( \"PlayerVehicleId\", App.Equals, " + vehicleId + " ); " +
                "query:AddOperation_int( \"VehiclePartId.VehiclePartTypeId\", App.Equals, " + ty.id + " ); " +
                "local rows = t:query( query ); rows:EnumReset( ); local partRow = rows:EnumNext( ); " +
                "if( partRow ~= nil and partRow:isNull( ) == FALSE ) then partRow:setCell_int( \"VehiclePartId\", " + e.part + " ); " +
                "else query:AddOperation_int( \"PlayerVehicleId\", App.Equals, " + vehicleId + " ); query:AddOperation_int( \"VehiclePartId\", App.Equals, " + e.part + " ); t:addRow( query ); end ";
            if (ty.tuning != null)
            {
                string c1 = ty.tuning == "engine" ? "Engine1" : ty.tuning == "suspension" ? "Suspension1" : ty.tuning == "tires" ? "Tire1" : null;
                string c2 = ty.tuning == "engine" ? "Engine2" : ty.tuning == "suspension" ? "Suspension2" : ty.tuning == "tires" ? "Tire2" : null;
                if (c1 != null)
                    lua += "local tt = App.LookupPlayerTable( \"PlayerVehicleTuning\", 0 ); local tr = tt:searchUnique_int( \"PlayerVehicleId\", " + vehicleId + " ); " +
                           "if( tr:isNull( ) == FALSE ) then tr:setCell_int( \"" + c1 + "\", " + e.tune1 + " ); tr:setCell_int( \"" + c2 + "\", " + e.tune2 + " ); end";
            }
            needSave = true; equippable = false;
            var changed = ReflexProfile.EquipPart(vehicleId, e.part, ty.tuning, e.tune1, e.tune2, lua);
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !changed.Result) return;
            int idx = equipped.FindIndex(x => PartType(x) == ty.id); if (idx >= 0) equipped[idx] = e.part; else equipped.Add(e.part);
            FormatAll();
            Debug.Log("[Garage] equipped vehicle part " + e.part);
        }
        void Purchase(Entry e)
        {
            int price = FeGarageData.Price(e.product);
            if (!FeGarageData.CanWithdraw(price)) { fe.PlaySound("Accept"); fe.SendUIEvent("ShowInsufficientFundsPopup"); return; }
            int parent = FeGarageData.ProductInt(e.product, "ParentStoreProductId");
            PurchaseProductPopupScript.ParentName = FeGarageData.Title(fe, parent == -1 ? e.product : parent);
            PurchaseProductPopupScript.ProductName = parent == -1 ? "" : FeGarageData.Title(fe, e.product);
            fe.PlaySound("Accept"); fe.SendUIEvent("ShowPurchasePopup");
        }
        void OnPurchasePopupAccept()
        {
            var e = Focused; if (e == null) return;
            int price = FeGarageData.Price(e.product), bal = FeGarageData.Balance;
            if (bal - price < 0) return;
            ReflexProfile.Run($"set_int PlayerRider 0 AccountBalance {bal - price}", "purchase_asset " + e.asset);
            needSave = true; e.purchased = true; e.isNew = false;
            EquipPart(e); fe.PlaySound("Purchase"); purchasable = false;
            if (cost != null) cost.Visible = false;
            Debug.Log("[Garage] bought store asset " + e.asset + " for " + price);
        }
        /// Tests: equip the first purchased, unequipped part of the current type (any type when none). Returns the part, -1 none.
        public int TestEquipFirst()
        {
            for (int k = 0; k < types.Count; k++)
            {
                for (int i = 0; i < prim.Count; i++)
                {
                    var e = prim[i]; if (e.children || e.part < 0 || !e.purchased || equipped.Contains(e.part)) continue;
                    primList.absIndex = i; OnStoreListFocusedChanged(true); OnAcceptPrimaryStoreList(); return e.part;
                }
                if (typeList.absIndex + 1 >= types.Count) break;
                typeList.absIndex++; OnPartTypeListFocusedChanged();
            }
            return -1;
        }

        void ShowVehicle()
        {
            if (garage == null || modelId < 0) return;
            var parts = new List<int>(equipped);
            if (previewPart >= 0) { int ty = PartType(previewPart); int i = parts.FindIndex(x => PartType(x) == ty); if (i >= 0) parts[i] = previewPart; else parts.Add(previewPart); }   // EquipPreviewVehiclePart
            string key = string.Join(",", parts); if (key == shownKey) return; shownKey = key;
            BikeSpec bs = null;
            try { bs = ReflexVehicleSpecs.Bike(new ReflexGameTables(new FeTableSource(FeGarageData.S)), modelId, parts); } catch (Exception ex) { Debug.LogWarning("[Garage] vehicle: " + ex.Message); }
            garage.ShowSpecs(bs, null, false);
        }
        void UpdateCamera(bool useListData)
        {
            // GarageVehicleCustomize_Cameras.lua (the motocross types 2 / 3) by part type, else the presentation camera
            string f = useListData && types.Count > 0 ? types[typeList.absIndex].factory : null;
            (Vector3 cam, Vector3 part, Vector3 post)? c = null;
            if (vehicleType == 2 || vehicleType == 3)
                switch (f)
                {
                    case "GraphicKit": c = (new Vector3(-6.43f, 3.16f, 1.76f), new Vector3(-0.17f, 0.35f, -0.41f), new Vector3(-1.76f, -0.25f, -6.19f)); break;
                    case "Grips": c = (new Vector3(-2.67f, 3.46f, 2.49f), new Vector3(0.13f, 1.67f, 0.65f), new Vector3(-0.34f, 0, 0)); break;
                    case "HandGuards": case "Handlebar": c = (new Vector3(-2.67f, 3.46f, 2.49f), new Vector3(0.13f, 1.67f, 0.65f), new Vector3(-0.66f, 0, -2.06f)); break;
                    case "Exhaust": c = (new Vector3(5.62f, 2.66f, -1.56f), vehicleType == 2 ? new Vector3(0.37f, 0.44f, -1.70f) : new Vector3(0.20f, 0.44f, -0.59f), new Vector3(-1.13f, 0, -5.19f)); break;
                    case "Tires": c = (new Vector3(-2.89f, 0.59f, -5.55f), new Vector3(0.01f, -0.84f, -0.17f), new Vector3(-0.99f, 0.88f, -6.10f)); break;
                    case "Forks": c = (new Vector3(3.19f, 2.44f, 3.08f), new Vector3(-0.01f, 0.43f, 1.63f), new Vector3(-0.97f, 0.19f, -5.45f)); break;
                    case "Wheels": c = (new Vector3(3.82f, -0.37f, 9.79f), Vector3.zero, new Vector3(-0.47f, 0.03f, -2.25f)); break;
                }
            if (c == null) c = (new Vector3(-8.04f, -0.64f, -1.74f), new Vector3(-0.17f, 0.02f, -0.14f), new Vector3(-2.62f, 0.07f, -9.15f));   // LookupVehiclePresentationCameraInformation (MX)
            var lift = new Vector3(0, 1.8f, 0);   // the vehicle stands on the garage floor; the game's cameras look at its middle
            garage?.SetCamera(c.Value.cam + lift, c.Value.part + lift, c.Value.post);
        }
        void UpdateBalance()
        {
            if (balance?.Find("AccountBalance") is FeNode a) a.SetText(FeGarageData.Number(FeGarageData.Balance));
            if (balance?.Find("AccountBalanceShadow") is FeNode s) s.SetText(FeGarageData.Number(FeGarageData.Balance));
        }
    }
}
