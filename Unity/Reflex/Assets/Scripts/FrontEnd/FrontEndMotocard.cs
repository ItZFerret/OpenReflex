// The motocard (WP-4): C# ports of the motocard setup's Lua (MotocardScreenAdjustment, MotocardWelcome, MotocardSetupVehicle,
// MotocardSetupRider) and the motocard's pages (MotocardLayer / Motocard, MotocardHighlights, MotocardRecords, MotocardStunts,
// StuntPreview); reference exports in _re/formats/out/ui/*.lua. As in FrontEndCareer.cs, what the Lua asks the game and every table
// edit it makes go to the game itself through the profile session (ReflexProfile; career.cpp "lua" runs the Lua's own table code,
// "lua_ui" brings an answer back through a UI variable). The static tables are the install's MXTables_Default (FeTables).
//
// The setup (TitleLoadGame "Create Motocard" -> Autosave Notice -> Screen Adjustment -> Welcome -> Vehicle -> Gear -> Skintone ->
// Jersey Name -> Jersey Number -> Completion; FrontEndPageGraph's "Advance" edges): every page plays its narration
// (App.PlayUILocalizedSoundEvent UIAudio UI_Game_Intro / UI_MOTO_VEHICLE / GEAR / SKINTONE / NAME / NUMBER / COMPLETE) and stops it
// in End. The garage view is FeGarage (FrontEndVehicleSelect.cs) with the pages' camera values.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    public static class FeMotocardScripts
    {
        public static FeScript Create(string script)
        {
            switch (script)
            {
                case "MotocardScreenAdjustment": return new MotocardScreenAdjustmentScript();
                case "MotocardWelcome": return new MotocardWelcomeScript();
                case "MotocardSetupVehicle": return new MotocardSetupVehicleScript();
                case "MotocardSetupRider": return new MotocardSetupRiderScript();
                case "MotocardLayer": return new MotocardLayerScript();
                case "MotocardHighlights": return new MotocardHighlightsPageScript();
                case "MotocardRecords": return new MotocardRecordsScript();
                case "MotocardStunts": return new MotocardStuntsScript();
                default: return null;
            }
        }

        /// A string of another localisation table (App.SetDynamicText( text, key, table ), App.GetLocalization( table )).
        public static string Str(FrontEndManager fe, string table, string key)
        {
            if (string.IsNullOrEmpty(key)) return "";
            if (!tables.TryGetValue(table, out var d))
            {
                d = new Dictionary<string, string>();
                foreach (var p in new[] { "FrontEnd", "Shared", "InGame", "BaseLocalized" })
                    try { if (fe.assets.data.Has(fe.assets.data.Pkg(p), "localiz", table)) { foreach (var kv in fe.assets.data.Strings(p, table)) if (!d.ContainsKey(kv.Key)) d[kv.Key] = kv.Value; } }
                    catch (Exception e) { Debug.LogWarning("[Motocard] " + table + " in " + p + ": " + e.Message); }
                tables[table] = d;
            }
            return d.TryGetValue(key, out var s) ? s : fe.assets.Str(key);
        }
        static readonly Dictionary<string, Dictionary<string, string>> tables = new Dictionary<string, Dictionary<string, string>>();

        /// GarageVehiclePresentation_Cameras.lua LookupVehiclePresentationCameraInformation( vehicleTypeId ) (as FrontEndGarage.cs: the
        /// garage stands the vehicle on its floor, the game looks at its middle).
        public static (Vector3 cam, Vector3 part, Vector3 post) PresentationCamera(int type)
        {
            (Vector3 cam, Vector3 part, Vector3 post) c;
            switch (type)
            {
                case 2: case 3: c = (new Vector3(-8.04f, -0.64f, -1.74f), new Vector3(-0.17f, 0.02f, -0.14f), new Vector3(-2.62f, 0.07f, -9.15f)); break;
                case 4: c = (new Vector3(-18.23f, -1.01f, -2.96f), new Vector3(0.13f, 0.04f, -0.24f), new Vector3(-2.65f, 0.08f, 0.79f)); break;
                case 11: c = (new Vector3(-19.30f, -0.52f, -3.48f), new Vector3(0.06f, 1.08f, -0.52f), new Vector3(-4.54f, 0.12f, -6.12f)); break;
                case 12: c = (new Vector3(-21.43f, -2.94f, -2.52f), new Vector3(10.05f, -0.29f, 3.68f), new Vector3(-5.66f, 1.89f, -6.18f)); break;
                case 14: c = (new Vector3(-18.66f, -1.35f, -3.34f), new Vector3(5.42f, 0.85f, 1.77f), new Vector3(-5.81f, 0.28f, -7.69f)); break;
                case 15: c = (new Vector3(-20.09f, -1.74f, -2.21f), new Vector3(5.42f, 0.85f, 1.77f), new Vector3(-6.27f, 0.48f, -8.31f)); break;
                default: c = (new Vector3(15, 5, 1), Vector3.zero, Vector3.zero); break;
            }
            var lift = new Vector3(0, 1.8f, 0);
            return (c.cam + lift, c.part + lift, c.post);
        }

        public static Dictionary<string, object> Row(string table, string col, int v) => FeTables.Shared.Rows(table).FirstOrDefault(r => FeTables.I(r, col) == v);
        public static int RowType(int riderGear) => FeTables.I(Row("RiderGear", "RiderGearId", riderGear), "RiderGearTypeId");
    }

    /// A UIDynamicList of several slots (ItemN / ListItemN): the slot under the cursor is the focused one, the first shown data index
    /// follows the cursor, the scroll bar beside the list (Scrollbar / Slider) shows the visible part. Each shown slot is formatted
    /// (the Lua's "FormatComponent" handler).
    public sealed class FeDynList
    {
        public readonly FeNode list; readonly List<FeNode> slots; readonly Action<FeNode, int> format; int top;
        public FeDynList(FeNode list, Action<FeNode, int> format)
        {
            this.list = list; this.format = format;
            if (list == null) { slots = new List<FeNode>(); return; }
            bool lr = list.P("NavigationType") == "LeftRight";
            slots = list.kids.Where(k => k.type != null && (k.type.Contains("Container") || k.type.Contains("Text"))).OrderBy(k => lr ? k.pos.x : k.pos.y).ToList();
        }
        public int Count => list == null ? 0 : Math.Max(0, list.numItems);
        public int Index => list?.absIndex ?? 0;
        public int Top => top;
        public void SetData(int count, int index = 0) { if (list == null) return; list.numItems = count; list.absIndex = Mathf.Clamp(index, 0, Math.Max(0, count - 1)); top = 0; Refresh(); }
        public void Select(int index) { if (list == null) return; list.absIndex = Mathf.Clamp(index, 0, Math.Max(0, Count - 1)); Refresh(); }
        public void Refresh()
        {
            if (list == null) return;
            int n = slots.Count, i = list.absIndex, count = Count;
            if (i < top) top = i; if (i >= top + n) top = i - n + 1; top = Mathf.Clamp(top, 0, Math.Max(0, count - n));
            for (int s = 0; s < n; s++)
            {
                int d = top + s; bool show = d < count;
                slots[s].Visible = show;
                if (show) format?.Invoke(slots[s], d);
            }
            if (n > 0) list.focusIndex = list.kids.IndexOf(slots[Mathf.Clamp(i - top, 0, n - 1)]);
            var bar = list.parent?.Find("Scrollbar"); var slider = bar?.Find("Slider");
            if (bar != null) bar.Visible = count > n;
            if (slider != null && count > n)
            {
                float h = bar.size.y, sh = Mathf.Max(8, h * n / count);
                slider.size.y = sh; slider.rt.sizeDelta = slider.size; slider.pos.y = (h - sh) * top / Math.Max(1, count - n);
            }
        }
    }

    // MotocardScreenAdjustment.lua (the Screen Adjustment Page): the NavList with the GammaContainer row (CRT 0 / LCD 1 =
    // STR_OPT_GAMMA_CRT / LCD; the Lua's only row), its value from App.GetUISetting( "Gamma" ), each change App.SetUISetting at once
    // (applied to the picture: ReflexScreenAdjustment, FrontEndOptions.cs), the arrows by position; Accept -> "Advance".
    public sealed class MotocardScreenAdjustmentScript : FeScript
    {
        static readonly (int value, string label)[] Gamma = { (0, "STR_OPT_GAMMA_CRT"), (1, "STR_OPT_GAMMA_LCD") };
        FeNode list, left, right;
        public int Value => Gamma[Mathf.Clamp(list?.absIndex ?? 0, 0, Gamma.Length - 1)].value;
        public override void Initialize()
        {
            var c = C("GammaContainer"); list = c?.Kid("List"); left = c?.Kid("LeftArrow"); right = c?.Kid("RightArrow");
            functions["OnAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("Advance"); };
            functions["OnBackKeypress"] = () => { };
            if (list != null) list.onListChanged = (l, old) => OnSettingsListFocusedChanged();
        }
        public override void Start()
        {
            fe.SetButtonLegend("accept", true, "STR_CONTINUE");
            if (list == null) return;
            int v = OptionsFrontEndScript.GetSetting("Gamma"), index = 0;
            for (int i = 0; i < Gamma.Length; i++) if (Gamma[i].value == v) { index = i; break; }
            list.numItems = Gamma.Length; list.absIndex = index;
            OnSettingsListFocusedChanged();
        }
        void OnSettingsListFocusedChanged()
        {
            var d = Gamma[Mathf.Clamp(list.absIndex, 0, Gamma.Length - 1)];
            if (list.kids.Count > 0) list.kids[0].SetText(Str(d.label));   // OnFormatSettingsList
            OptionsFrontEndScript.SetSetting("Gamma", d.value, fe);        // App.SetUISetting( "Gamma", value )
            ReflexProfile.Run("ui_setting Gamma " + d.value);
            int i = list.absIndex, n = list.numItems;                       // UpdateArrows
            if (left != null && right != null)
            {
                if (i == 0) { left.Visible = false; right.Visible = true; }
                else if (i == n - 1) { left.Visible = true; right.Visible = false; }
                else { left.Visible = true; right.Visible = true; }
            }
        }
    }

    // MotocardWelcome.lua, both pages: "Motocard Setup Welcome Page" (mode "": STR_MOTOCARD_WELCOME_*, the game intro narration
    // UIAudio UI_Game_Intro "UI_GAME_INTRO/VO/INTRO") and "Motocard Setup Completion" (mode "Exit": STR_MOTOCARD_TITLE_COMPLETION /
    // INTRO_COMPLETION, UI_MOTO_COMPLETE "UI_MOTO_COMPLETE/VO/COMPLETE_[PLATFORM]" (COMPLETE_PC: the data has PS3 / XENON lines only,
    // so the PC is silent here), App.RequestSaveGame in Start). End (Exit): PlayerRider MotocardSetup, PlayerInMotocardSetup removed, the
    // garage unloaded, PurchaseStoreAsset of every equipped PlayerRiderGear's RiderGear.StoreAssetId; always: MotocardRiderSetupMode /
    // MotocardWelcomeMode removed, the narration stopped. Accept -> "Advance".
    public sealed class MotocardWelcomeScript : FeScript
    {
        string mode;
        public override void Initialize() { functions["OnAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("Advance"); }; }
        public override void Start()
        {
            mode = fe.Var<string>("MotocardWelcomeMode", "") ?? "";
            if (mode == "Exit")
            {
                C("Title")?.SetText(Str("STR_MOTOCARD_TITLE_COMPLETION")); C("Message")?.SetText(Str("STR_MOTOCARD_INTRO_COMPLETION"));
                fe.SetButtonLegend("accept", true, "STR_MOTOCARD_NAV_COMPLETION");
                fe.PlayLocalizedSound("UIAudio", "UI_MOTO_COMPLETE", "UI_MOTO_COMPLETE/VO/COMPLETE_[PLATFORM]", layer.Name);
                ReflexProfile.Run("save");   // And request a save...
            }
            else
            {
                C("Title")?.SetText(Str("STR_MOTOCARD_WELCOME_TITLE")); C("Message")?.SetText(Str("STR_MOTOCARD_WELCOME_MESSAGE"));
                fe.SetButtonLegend("accept", true, "STR_MOTOCARD_NAV_WELCOME");
                fe.PlayLocalizedSound("UIAudio", "UI_Game_Intro", "UI_GAME_INTRO/VO/INTRO", layer.Name);
            }
        }
        public override void End()
        {
            if (mode == "Exit")
            {
                // Flag the motocard as being created; make sure the equipped gear has been purchased
                var cmds = new List<string> { "set_int PlayerRider 0 MotocardSetup 1" };
                var gear = FeCareerScripts.T("PlayerRiderGear");
                for (int i = 0; i < gear.rows; i++)
                {
                    int id = gear.Int("RiderGearId", i); if (id < 0) continue;
                    int asset = FeTables.I(FeMotocardScripts.Row("RiderGear", "RiderGearId", id), "StoreAssetId");
                    if (asset >= 0) cmds.Add("purchase_asset " + asset);
                }
                ReflexProfile.Run(cmds.ToArray());
                fe.vars.Remove("PlayerInMotocardSetup");
            }
            fe.vars.Remove("MotocardRiderSetupMode"); fe.vars.Remove("MotocardWelcomeMode");
            fe.StopLocalizedSound(layer.Name);
        }
    }

    // VehiclePerformanceDisplayComponent:UpdateVehicleModelStats( vehicleModelId ): the model's DefaultEngine / Suspension / Tire 1, 2
    // plus the default parts' tuning (VehicleModelParts Default with a VehiclePartType TuningAttribute: DefaultTuneAttribute1, 2) as
    // the bars of the "VehiclePerformanceDisplay.component" (width = floor(value / 100 * max / step) * step, 20 steps).
    public static class FePerformanceDisplay
    {
        static readonly string[] Attributes = { "engine", "suspension", "tires" };
        public static void UpdateVehicleModelStats(FeNode perf, int modelId)
        {
            if (perf == null) return;
            var t = FeTables.Shared; var model = FeMotocardScripts.Row("VehicleModel", "VehicleModelId", modelId);
            var bonus = Attributes.ToDictionary(a => a, a => new int[2]);
            if (model != null)
                foreach (var mp in t.Rows("VehicleModelParts").Where(r => FeTables.I(r, "VehicleModelId") == modelId && FeTables.I(r, "Default", 0) != 0))
                {
                    var part = FeMotocardScripts.Row("VehiclePart", "VehiclePartId", FeTables.I(mp, "VehiclePartId"));
                    string attr = FeTables.S(FeMotocardScripts.Row("VehiclePartType", "VehiclePartTypeId", FeTables.I(part, "VehiclePartTypeId")), "TuningAttribute");
                    if (string.IsNullOrEmpty(attr) || !bonus.ContainsKey(attr)) continue;
                    bonus[attr][0] = FeTables.I(part, "DefaultTuneAttribute1", 0); bonus[attr][1] = FeTables.I(part, "DefaultTuneAttribute2", 0);
                }
            Attribute(perf, "engine", FeTables.I(model, "DefaultEngine1", 0), FeTables.I(model, "DefaultEngine2", 0), bonus["engine"]);
            Attribute(perf, "suspension", FeTables.I(model, "DefaultSuspension1", 0), FeTables.I(model, "DefaultSuspension2", 0), bonus["suspension"]);
            Attribute(perf, "tires", FeTables.I(model, "DefaultTire1", 0), FeTables.I(model, "DefaultTire2", 0), bonus["tires"]);
        }
        static void Attribute(FeNode perf, string attribute, int base1, int base2, int[] bonus)
        {
            var c = perf.kids.FirstOrDefault(k => string.Equals(k.name, attribute + "_Attribute", StringComparison.OrdinalIgnoreCase)); if (c == null) return;
            int[,] values = { { base1, bonus[0] }, { base2, bonus[1] } };
            for (int index = 0; index < 2; index++)
            {
                var a = c.Kid("Attribute" + (index + 1)); if (a == null) continue;
                var bg = a.Kid("Background"); float max = bg != null ? bg.size.x : 160, step = max / 20;
                float baseWidth = Mathf.Floor(values[index, 0] / 100f * max / step) * step, bonusWidth = Mathf.Floor(values[index, 1] / 100f * max / step) * step;
                Place(a.Kid("Base"), 0, baseWidth); Place(a.Kid("Bonus"), baseWidth, bonusWidth);
            }
        }
        static void Place(FeNode n, float x, float w) { if (n == null) return; n.size.x = w; n.rt.sizeDelta = n.size; n.pos.x = x + n.pivot.x * w; n.Visible = w > 0; }
    }

    // MotocardSetupVehicle.lua (the Motocard Setup Vehicle page): the player's favourite MX Lite (PlayerVehicle Favorite, VehicleModelId's
    // VehicleTypeId 2), its brand (StoreProducts.BrandId -> Brand.BrandIconId) and performance bars, the garage with that vehicle and
    // no rider (LoadingStart / LoadingDone), the camera LookupVehiclePresentationCameraInformation( type ); the narration UIAudio
    // UI_MOTO_VEHICLE "UI_MOTO_VEHICLE/VO/VEHICLE". End: the vehicle removed, MotocardRiderSetupMode = "Gear", the narration stopped.
    // Accept -> "Advance".
    public sealed class MotocardSetupVehicleScript : FeScript
    {
        FeGarage garage; FeNode target, brand, perf; int model = -1, type = 2; bool shown;
        public int Model => model;
        public FeGarage Garage => garage;
        public override void Initialize()
        {
            target = C("RenderTarget"); brand = C("BrandImageIcon"); perf = C("VehiclePerformanceDisplay.component");
            functions["OnAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("Advance"); };
        }
        public override void Start()
        {
            fe.SetButtonLegend("accept", true, "STR_MOTOCARD_NAV_VEHICLE");
            fe.PlayLocalizedSound("UIAudio", "UI_MOTO_VEHICLE", "UI_MOTO_VEHICLE/VO/VEHICLE", layer.Name);
            var pv = FeCareerScripts.T("PlayerVehicle"); model = -1;
            for (int i = 0; i < pv.rows && model < 0; i++)
            {
                int m = pv.Int("VehicleModelId", i);
                if (pv.Int("Favorite", i, 0) != 0 && FeTables.I(FeMotocardScripts.Row("VehicleModel", "VehicleModelId", m), "VehicleTypeId") == 2) model = m;
            }
            var mrow = FeMotocardScripts.Row("VehicleModel", "VehicleModelId", model);
            type = FeTables.I(mrow, "VehicleTypeId", 2);
            FeGarageData.Brand(fe, brand, FeGarageData.ProductOfAsset(FeTables.I(mrow, "StoreAssetId")));
            FePerformanceDisplay.UpdateVehicleModelStats(perf, model);
            try { garage ??= new FeGarage(fe.transform); } catch (Exception e) { Debug.LogWarning("[Motocard] " + e.Message); }
            FeGarageData.AttachTarget(garage, target);
            layer.Play("LoadingStart"); shown = false;
            var c = FeMotocardScripts.PresentationCamera(type); garage?.SetCamera(c.cam, c.part, c.post);
        }
        public override void Update(float dt)
        {
            garage?.Update(dt);
            if (shown || garage == null || model < 0) return;
            shown = true;
            BikeSpec bs = null;
            // RequestVehicle( GetMXVehicleOwner( "MX" ), model ): the stock model, its default parts (VehicleModelParts Default)
            var parts = FeTables.Shared.Rows("VehicleModelParts").Where(r => FeTables.I(r, "VehicleModelId") == model && FeTables.I(r, "Default", 0) != 0).Select(r => FeTables.I(r, "VehiclePartId")).ToList();
            try { bs = ReflexVehicleSpecs.Bike(new ReflexGameTables(new FeTableSource(FeTables.Shared)), model, parts); } catch (Exception e) { Debug.LogWarning("[Motocard] vehicle: " + e.Message); }
            garage.ShowSpecs(bs, null, false);   // RequestVehicle( MX, model ), RemoveRider
            layer.Stop("LoadingStart"); layer.Play("LoadingDone");   // OnVehicleLoaded
        }
        public override void End()
        {
            garage?.Destroy(); garage = null;   // RemoveVehicle
            fe.vars["MotocardRiderSetupMode"] = "Gear";
            fe.StopLocalizedSound(layer.Name);
        }
    }

    // MotocardSetupRider.lua (one layer for the Gear, Skintone, Jersey Name and Jersey Number pages, by MotocardRiderSetupMode):
    // * Start: the skin tone = the PlayerRiderGear row whose gear is of type 10; the mode's narration (UI_MOTO_GEAR / SKINTONE / NAME /
    //   NUMBER, owner = the layer's name .. the mode), texts STR_MOTOCARD_TITLE_<mode> / STR_MOTOCARD_INTRO_<mode>, the camera.
    // * Gear: the RiderGearList ({ gear set 1, 2, 15 } with RiderGearSet.StoreProductId's titles) - each focus change
    //   LoadGearSetForPlayer: PlayerRiderGear cleared, the set's RiderGearSetGear rows (not skin tone) added, then the skin tone, the
    //   rider requested again (the Lua's table code, run in the game: ReflexProfile "lua").
    // * Skintone: the RiderSkintoneList (White 321 / Black 512 / Latino 513 / Asian 514), the current one focused; a change previews it;
    //   End writes it into the skin tone rows.
    // * Jersey name / number: Toggle (CHANGE RIDER NAME / NUMBER) -> the jersey popups (FrontEndCareer.cs).
    // Accept advances the mode (Gear -> Skintone -> JerseyName -> JerseyNumber -> MotocardWelcomeMode "Exit") with "Advance"; Back goes
    // back one (TravelBack). The narration stops in End.
    public sealed class MotocardSetupRiderScript : FeScript
    {
        static readonly int[] GearSets = { 1, 2, 15 };
        static readonly int[] Skintones = { 321, 512, 513, 514 };
        FeNode title, message, gearRoot, gearList, skinRoot, skinList, target;
        FeGarage garage; string mode = ""; int skintone = 321; string shownKey; bool loading; List<int> previewGear;
        public string Mode => mode;
        public int Skintone => skintone;
        public FeGarage Garage => garage;
        public int GearIndex => gearList?.absIndex ?? -1;

        public override void Initialize()
        {
            title = C("Title"); message = C("Message"); target = C("RenderTarget");
            gearRoot = C("RiderGearList"); gearList = gearRoot?.Kid("List");
            skinRoot = C("RiderSkintoneList"); skinList = skinRoot?.Kid("List");
            functions["OnAcceptKeypress"] = OnAcceptKeypress;
            functions["OnBackKeypress"] = OnBackKeypress;
            functions["OnChangeKeypress"] = OnChangeKeypress;
            functions["OnNameNumberUpdated"] = () => { };   // MXGarage:UpdateNameAndNumber (the port's rider has no jersey lettering)
            if (gearList != null) gearList.onListChanged = (l, old) => OnRiderGearListFocusedChanged();
            if (skinList != null) skinList.onListChanged = (l, old) => OnRiderSkintoneListFocusedChanged();
        }

        public override void Start()
        {
            fe.SetButtonLegend("back", true, "STR_BACK");
            if (gearRoot != null) { gearRoot.Visible = false; gearRoot.focusedFlag = false; }
            if (skinRoot != null) { skinRoot.Visible = false; skinRoot.focusedFlag = false; }
            loading = false; previewGear = null; shownKey = null;
            var g = FeCareerScripts.T("PlayerRiderGear");
            for (int i = 0; i < g.rows; i++) if (FeMotocardScripts.RowType(g.Int("RiderGearId", i)) == 10) { skintone = g.Int("RiderGearId", i); break; }
            try { garage ??= new FeGarage(fe.transform); } catch (Exception e) { Debug.LogWarning("[Motocard] " + e.Message); }
            FeGarageData.AttachTarget(garage, target);
            mode = fe.Var<string>("MotocardRiderSetupMode", "") ?? "";
            string vo = null;
            switch (mode)
            {
                case "Gear":
                    vo = "GEAR";
                    if (gearRoot != null) { gearRoot.Visible = true; gearRoot.focusedFlag = true; }
                    Camera(new Vector3(0.19f, 5.95f, 16.25f), new Vector3(0.03f, 4.00f, -0.01f), new Vector3(-1.90f, -1.15f, 1.18f));
                    Texts("STR_MOTOCARD_TITLE_GEAR", "STR_MOTOCARD_INTRO_GEAR"); fe.SetButtonLegend("accept", true, "STR_CONTINUE");
                    BuildGearList();
                    break;
                case "Skintone":
                    vo = "SKINTONE";
                    if (skinRoot != null) { skinRoot.Visible = true; skinRoot.focusedFlag = true; }
                    Camera(new Vector3(5.10f, 5.68f, 7.89f), new Vector3(0.03f, 4.67f, -0.01f), new Vector3(-0.80f, -0.51f, 1.18f));
                    Texts("STR_MOTOCARD_TITLE_SKINTONE", "STR_MOTOCARD_INTRO_SKINTONE"); fe.SetButtonLegend("accept", true, "STR_MOTOCARD_NAV_SKINTONE");
                    BuildSkintoneList();
                    break;
                case "JerseyName":
                    vo = "NAME";
                    Camera(new Vector3(3.16f, 5.15f, -6.86f), new Vector3(-0.08f, 4.07f, -0.37f), new Vector3(-0.80f, 0.02f, 1.18f));
                    Texts("STR_MOTOCARD_TITLE_NAME", "STR_MOTOCARD_INTRO_NAME");
                    fe.SetButtonLegend("accept", true, "STR_CONTINUE"); fe.SetButtonLegend("toggle", true, "STR_CHANGE_RIDER_NAME");
                    break;
                case "JerseyNumber":
                    vo = "NUMBER";
                    Camera(new Vector3(3.16f, 5.15f, -6.86f), new Vector3(-0.08f, 4.07f, -0.37f), new Vector3(-0.80f, 0.02f, 1.18f));
                    Texts("STR_MOTOCARD_TITLE_NUMBER", "STR_MOTOCARD_INTRO_NUMBER");
                    fe.SetButtonLegend("accept", true, "STR_CONTINUE"); fe.SetButtonLegend("toggle", true, "STR_CHANGE_RIDER_NUMBER");
                    break;
            }
            if (vo != null) fe.PlayLocalizedSound("UIAudio", "UI_MOTO_" + vo, "UI_MOTO_" + vo + "/VO/" + vo, layer.Name + mode);
        }
        void Texts(string t, string m) { title?.SetText(Str(t)); message?.SetText(Str(m)); }
        void Camera(Vector3 cam, Vector3 part, Vector3 post) => garage?.SetCamera(cam, part, post);

        public override void End()
        {
            if (mode == "Skintone")
            {   // EquipPreviewRiderGear; the skin tone rows get the chosen tone
                string lua = "local t = App.LookupPlayerTable( \"PlayerRiderGear\", 0 ); local q = App.RdbQuery( ); " +
                             "q:AddOperation_int( \"RiderGearId.RiderGearTypeId\", App.Equals, 10 ); local rows = t:query( q ); rows:EnumReset( ); " +
                             "while not ( rows:EnumDone( ) ) do local r = rows:EnumNext( ); r:setCell_int( \"RiderGearId\", " + skintone + " ); end";
                ReflexProfile.Run("lua " + lua);
            }
            fe.StopLocalizedSound(layer.Name + mode);
            garage?.Destroy(); garage = null;
        }

        public override void Update(float dt)
        {
            garage?.Update(dt);
            if (garage == null || ReflexProfile.Busy) return;
            // the rider as the profile has it now (or the skin tone previewed): MXGarage:RequestRider( Player1 ) / PreviewRiderGear
            var p = ReflexProfile.Current; if (p == null) return;
            var g = p.T("PlayerRiderGear");
            var ids = Enumerable.Range(0, g.rows).Select(i => g.Int("RiderGearId", i)).Where(i => i >= 0).ToList();
            if (previewGear != null) ids = previewGear;
            string key = string.Join(",", ids);
            if (key == shownKey) return;
            shownKey = key;
            RiderSpec rs = null;
            try { if (ids.Count > 0) rs = ReflexVehicleSpecs.Rider(new ReflexGameTables(new FeTableSource(FeTables.Shared)), ids); } catch (Exception e) { Debug.LogWarning("[Motocard] rider: " + e.Message); }
            garage.ShowSpecs(null, rs, false);
            if (loading) { layer.Stop("LoadingStart"); layer.Play("LoadingDone"); loading = false; }   // OnRiderLoaded
        }

        // ---------------------------------------------------------------------------------------- lists
        void BuildGearList()
        {
            if (gearList == null) return;
            gearList.numItems = GearSets.Length; gearList.absIndex = 0;
            OnRiderGearListFocusedChanged();
        }
        void BuildSkintoneList()
        {
            if (skinList == null) return;
            int sel = Array.IndexOf(Skintones, skintone); if (sel < 0) sel = 0;
            skinList.focusIndex = sel;
            OnRiderSkintoneListFocusedChanged();   // BuildSkintoneList's OnRiderGearListFocusedChanged does nothing for this list; the preview follows the focus
        }
        void OnRiderGearListFocusedChanged()
        {
            int i = Mathf.Clamp(gearList.absIndex, 0, GearSets.Length - 1);
            int product = FeTables.I(FeMotocardScripts.Row("RiderGearSet", "RiderGearSetId", GearSets[i]), "StoreProductId");
            if (gearList.kids.Count > 0) gearList.kids[0].SetText(Str(FeTables.Shared.TitleKey(product) ?? ""));   // OnFormatRiderGearList
            FeNodeArrows(gearRoot, gearList);
            LoadGearSetForPlayer(GearSets[i]);
        }
        static void FeNodeArrows(FeNode root, FeNode list)
        {   // UpdateArrows
            if (root == null || list == null) return;
            var l = root.Kid("LeftArrow"); var r = root.Kid("RightArrow");
            if (l != null) l.Visible = list.absIndex > 0;
            if (r != null) r.Visible = list.absIndex < list.numItems - 1;
        }
        void OnRiderSkintoneListFocusedChanged()
        {
            skintone = Skintones[Mathf.Clamp(skinList.focusIndex, 0, Skintones.Length - 1)];
            if (!loading) { layer.Play("LoadingStart"); loading = true; }
            // MXGarage:PreviewRiderGear( skintone ): the rider's current gear with that skin tone
            var g = FeCareerScripts.T("PlayerRiderGear");
            previewGear = Enumerable.Range(0, g.rows).Select(i => g.Int("RiderGearId", i)).Where(i => i >= 0 && FeMotocardScripts.RowType(i) != 10).Append(skintone).ToList();
        }
        /// LoadGearSetForPlayer( gearSetId ): the game's own table edits (RemoveRider, PlayerRiderGear:clear, the set's gear but skin tone,
        /// the skin tone, RequestRider).
        void LoadGearSetForPlayer(int gearSetId)
        {
            string lua =
                "local t = App.LookupPlayerTable( \"PlayerRiderGear\", 0 ); t:clear( ); " +
                "local s = App.Lookup( \"RiderGearSetGear\" ); local q = App.RdbQuery( ); " +
                "q:AddOperation_int( \"RiderGearSetId\", App.Equals, " + gearSetId + " ); q:AddOperation_int( \"RiderGearId.RiderGearTypeId\", App.NotEquals, 10 ); " +
                "local rows = s:query( q ); rows:EnumReset( ); " +
                "while not ( rows:EnumDone( ) ) do local r = rows:EnumNext( ); local a = App.RdbQuery( ); a:AddOperation_int( \"RiderGearId\", App.Equals, r:cell_int( \"RiderGearId\" ) ); t:addRow( a ); end " +
                "local k = App.RdbQuery( ); k:AddOperation_int( \"RiderGearId\", App.Equals, " + skintone + " ); t:addRow( k )";
            previewGear = null;
            ReflexProfile.Run("lua " + lua);
            Debug.Log("[Motocard] gear set " + gearSetId + " (skin tone " + skintone + ")");
            if (!loading) { layer.Play("LoadingStart"); loading = true; }
        }

        // ---------------------------------------------------------------------------------------- input
        void OnAcceptKeypress()
        {
            fe.PlaySound("Accept");
            switch (mode)
            {
                case "Gear": fe.vars["MotocardRiderSetupMode"] = "Skintone"; break;
                case "Skintone": fe.vars["MotocardRiderSetupMode"] = "JerseyName"; break;
                case "JerseyName": fe.vars["MotocardRiderSetupMode"] = "JerseyNumber"; break;
                case "JerseyNumber": fe.vars.Remove("MotocardRiderSetupMode"); fe.vars["MotocardWelcomeMode"] = "Exit"; break;
                default: return;
            }
            fe.SendUIEvent("Advance");
        }
        void OnBackKeypress()
        {
            fe.PlaySound("Back");
            switch (mode)
            {
                case "Gear": break;
                case "Skintone": fe.vars["MotocardRiderSetupMode"] = "Gear"; break;
                case "JerseyName": fe.vars["MotocardRiderSetupMode"] = "Skintone"; break;
                case "JerseyNumber": fe.vars["MotocardRiderSetupMode"] = "JerseyName"; break;
                default: return;
            }
            fe.SendUIEvent("TravelBack");
        }
        void OnChangeKeypress()
        {
            if (mode == "JerseyName") { fe.PlaySound("Accept"); fe.SendUIEvent("ShowJerseyNameEntryPopup"); }
            else if (mode == "JerseyNumber") { fe.PlaySound("Accept"); fe.SendUIEvent("ShowJerseyNumberEntryPopup"); }
        }
    }

    // MotocardHighlights.lua on a container (the Motocard Page's "MotocardHighlights.component"; the Motocard Highlights Page's own
    // layer, which no edge of the PC Lua sends): every Highlight row (HighlightId, Goal; HighlightId order) with the player's progress
    // (PlayerHighlight GoalProgress, UnlockedOrder), reached when GoalProgress == Goal; HighlightProgress "unlocked/total"; sorted
    // HighlightSort (unlocked first, newest unlock first; locked by progress, then id); each row: STR_HIGHLIGHT_<id>_TITLE /
    // _DESCRIPTION (MXRaven_Highlights), "progress/goal" when the goal is above 1, the Highlight%02d icon, desaturated when locked.
    public sealed class FeHighlights
    {
        readonly FrontEndManager fe; readonly FeNode root; readonly FeDynList list; readonly FeNode progress;
        readonly List<(int id, int progress, int goal, bool reached, int order)> data = new List<(int, int, int, bool, int)>();
        public int Count => data.Count; public int Unlocked { get; private set; }
        public FeNode List => list?.list;
        public FeHighlights(FrontEndManager fe, FeNode root)
        {
            this.fe = fe; this.root = root;
            var l = root?.Find("HighlightList"); progress = root?.Find("HighlightProgress");
            if (l != null) { list = new FeDynList(l, Format); l.onListChanged = (n, old) => list.Refresh(); }
        }
        public void Start()
        {
            data.Clear(); Unlocked = 0;
            var ph = FeCareerScripts.T("PlayerHighlight");
            foreach (var r in FeTables.Shared.Rows("Highlight").OrderBy(r => FeTables.I(r, "HighlightId")))
            {
                int id = FeTables.I(r, "HighlightId"), goal = FeTables.I(r, "Goal", 0), row = ph.Find("HighlightId", id);
                int gp = row >= 0 ? ph.Int("GoalProgress", row, 0) : 0, order = row >= 0 ? ph.Int("UnlockedOrder", row, -1) : -1;
                bool reached = gp == goal; if (reached) Unlocked++;
                data.Add((id, gp, goal, reached, order));
            }
            progress?.SetText(Unlocked + "/" + data.Count);
            var sorted = data.Select((d, i) => (d, i)).ToList();
            sorted.Sort((x, y) => Before(x.d, y.d) ? -1 : Before(y.d, x.d) ? 1 : x.i.CompareTo(y.i));   // table.sort( HighlightSort )
            data.Clear(); data.AddRange(sorted.Select(x => x.d));
            list?.SetData(data.Count, 0);
        }
        static bool Before((int id, int progress, int goal, bool reached, int order) a, (int id, int progress, int goal, bool reached, int order) b)
        {
            if (a.reached) return !b.reached || a.order > b.order;
            if (b.reached) return false;
            float pa = a.goal != 0 ? (float)a.progress / a.goal : 0, pb = b.goal != 0 ? (float)b.progress / b.goal : 0;
            return pa == pb ? a.id < b.id : pa > pb;
        }
        void Format(FeNode c, int d)
        {
            var h = data[d];
            c.Find("Title")?.SetText(FeMotocardScripts.Str(fe, "MXRaven_Highlights", "STR_HIGHLIGHT_" + h.id + "_TITLE"));
            c.Find("Description")?.SetText(FeMotocardScripts.Str(fe, "MXRaven_Highlights", "STR_HIGHLIGHT_" + h.id + "_DESCRIPTION"));
            c.Find("Progress")?.SetText(h.goal > 1 ? h.progress + "/" + h.goal : "");
            var icon = c.Find("Icon");
            FeCareerScripts.SetMaterial(fe, icon, (h.id < 10 ? "Highlight0" : "Highlight") + h.id);
            if (icon?.image) icon.image.desaturation = h.reached ? 0f : 1f;   // SetDesaturationLevel( 0 / 255 )
        }
    }

    // The Motocard Highlights Page (MotocardHighlights.lyr alone).
    public sealed class MotocardHighlightsPageScript : FeScript
    {
        FeHighlights h;
        public override void Initialize() { h = new FeHighlights(fe, layer.root); }
        public override void Start() => h.Start();
    }

    // MotocardLayer.lua / Motocard.lua (the Motocard Page, the local player): the list LEADERBOARDS (GotoMotocardLeaderboardPage) / STUNTS
    // (GotoMotocardStuntPage) / CHANGE RIDER NAME / CHANGE RIDER NUMBER (the jersey popups) / HIGHLIGHTS (focuses the highlights list,
    // accept legend off); the focused item's container shown (the card for the first four, MotocardHighlights.component for
    // HIGHLIGHTS). Back: from the highlights list back to the menu list, else TravelBack. The card (Motocard.lua
    // PopulateLocalInformation): the profile's display name (the persona), PlayerRider.Number, the career completion (EMB_CAREER_PROGRESS),
    // the credits (EMB_TOTAL_CREDITS), the seven most recent highlights (PlayerHighlight by UnlockedOrder). The online stats need the
    // network (N/A).
    public sealed class MotocardLayerScript : FeScript
    {
        static readonly string[] Items = { "STR_LEADERBOARDS", "STR_STUNTS", "STR_CHANGE_RIDER_NAME", "STR_CHANGE_RIDER_NUMBER", "STR_HIGHLIGHTS" };
        FeNode rootList, list, card, highlightsNode, visible; FeDynList menu; FeHighlights highlights;
        public bool HighlightsFocused => highlightsNode != null && highlightsNode.focusedFlag;
        public FeHighlights Highlights => highlights;
        public override void Initialize()
        {
            rootList = C("MotocardList"); list = rootList?.Kid("NavListContainer");
            card = C("Motocard.component"); highlightsNode = C("MotocardHighlights.component");
            highlights = new FeHighlights(fe, highlightsNode);
            functions["OnBackKeypress"] = OnBackKeypress;
            functions["OnAcceptMotocardList"] = OnAcceptMotocardList;
            menu = new FeDynList(list, (slot, d) => slot.Find("Text")?.SetText(Str(Items[d])));
            list.onListChanged = (l, old) => { menu.Refresh(); OnMotocardListFocusedChanged(); };
        }
        public override void Start()
        {
            if (rootList != null) rootList.focusedFlag = true;
            if (card != null) { card.focusedFlag = false; card.Visible = false; }
            if (highlightsNode != null) { highlightsNode.focusedFlag = false; highlightsNode.Visible = false; }
            visible = null;
            fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetHeader("STR_MOTOCARD");
            menu.SetData(Items.Length, 0);
            OnMotocardListFocusedChanged();
            highlights.Start();
            Card();
        }
        void OnMotocardListFocusedChanged()
        {
            if (visible != null) { visible.Visible = false; visible = null; }
            var c = list.absIndex == 4 ? highlightsNode : card;
            if (c != null) { c.Visible = true; visible = c; }
        }
        void OnAcceptMotocardList()
        {
            int i = Mathf.Clamp(list.absIndex, 0, Items.Length - 1);
            fe.PlaySound("Accept");
            switch (i)
            {
                case 0: fe.SendUIEvent("GotoMotocardLeaderboardPage"); break;
                case 1: fe.SendUIEvent("GotoMotocardStuntPage"); break;
                case 2: fe.SendUIEvent("ShowJerseyNameEntryPopup"); break;      // OnChangeRiderName -> OnSetMotocardName
                case 3: fe.SendUIEvent("ShowJerseyNumberEntryPopup"); break;    // OnChangeRiderNumber -> OnSetMotocardNumber
                case 4:                                                          // OnHighlightsKeypress
                    if (highlightsNode != null) { highlightsNode.focusedFlag = true; if (rootList != null) rootList.focusedFlag = false; }
                    fe.SetButtonLegend("accept", false, "STR_SELECT");
                    break;
            }
        }
        void OnBackKeypress()
        {
            if (HighlightsFocused)
            {   // OnCancelHighlights
                fe.SetButtonLegend("back", true, "STR_BACK");
                highlightsNode.focusedFlag = false; if (rootList != null) rootList.focusedFlag = true;
                fe.PlaySound("Back");
            }
            else { fe.SendUIEvent("TravelBack"); fe.PlaySound("Back"); }
        }
        int shownVersion = -1;
        public override void Update(float dt) { if (ReflexProfile.Version != shownVersion && !ReflexProfile.Busy) { Card(); highlights.Start(); } }   // a popup changed the rider
        void Card()
        {
            shownVersion = ReflexProfile.Version;
            var p = FeCareerScripts.Profile; var rider = FeCareerScripts.T("PlayerRider");
            C("NumberTextComponent")?.SetText(rider.Str("Number", 0) ?? "");
            if (C("PrimaryNameTextComponent") is FeNode name) { name.Visible = true; name.SetText(ReflexProfile.Persona); }
            if (C("SecondaryNameTextComponent") is FeNode second) second.Visible = false;
            if (C("OnlineInformationContainer") is FeNode online) online.Visible = false;
            if (C("CareerInformationContainer") is FeNode career) career.Visible = true;
            int progress = p == null ? 0 : Mathf.FloorToInt(p.careerCompletion * 100f);
            if (FeCareerScripts.WithKey(layer.root, "EMB_CAREER_PROGRESS") is FeNode cp) cp.SetText(FeCareerScripts.Embed(fe, "EMB_CAREER_PROGRESS") + progress + "%");
            if (FeCareerScripts.WithKey(layer.root, "EMB_TOTAL_CREDITS") is FeNode cr) cr.SetText(FeCareerScripts.Embed(fe, "EMB_TOTAL_CREDITS") + rider.Int("AccountBalance", 0, 0).ToString("N0", CultureInfo.InvariantCulture));
            var hl = FeCareerScripts.T("PlayerHighlight");
            var order = Enumerable.Range(0, hl.rows).OrderByDescending(i => hl.Int("UnlockedOrder", i, 0)).Take(7).Where(i => hl.Int("UnlockedOrder", i, 0) > 0).ToList();
            for (int i = 0; i < 7; i++)
            {
                if (!(card?.Find("Icon" + i) is FeNode icon)) continue;
                icon.Visible = i < order.Count;
                if (i < order.Count) { int id = hl.Int("HighlightId", order[i]); FeCareerScripts.SetMaterial(fe, icon, (id < 10 ? "Highlight0" : "Highlight") + id); }
            }
        }
    }

    // StuntPreview.lua on a container (MotocardStunts.lyr's "StuntPreview.component"; the pause page's TRICK LIBRARY uses the same
    // script): every stunt of the four tiers (App.GetStuntDescriptorTierContainer( tier ): GetStuntDescriptorName /
    // GetStuntDescriptorDisplayNameLocalization, asked of the game in the profile session; a display name already listed is skipped),
    // the list's rows: the name (MXRaven_Stunts) and its gesture image (material "Stunt_" .. the key without "STR_"); the focused
    // stunt's tier (STR_STUNT_TIER1..4). Accept: MXGarage:PreviewStunt( name ) (the port's garage plays no stunt animation).
    public sealed class FeStuntPreview
    {
        readonly FrontEndManager fe; readonly FeNode root, tier; readonly FeDynList list;
        public readonly List<(int tier, string name, string key)> data = new List<(int, string, string)>();
        System.Threading.Tasks.Task<List<(int tier, string name, string key)>> loading;
        public Action<string> previewStunt;
        static List<(int tier, string name, string key)> cache;
        public FeNode List => list?.list;
        public bool Loaded => loading == null;
        public FeStuntPreview(FrontEndManager fe, FeNode root)
        {
            this.fe = fe; this.root = root; tier = root?.Find("StuntTierComponent");
            var l = root?.Find("NavListContainer");
            if (l != null) { list = new FeDynList(l, Format); l.onListChanged = (n, old) => { list.Refresh(); OnStuntListFocusedChanged(); }; }
        }
        public void Start()
        {
            data.Clear(); list?.SetData(0);
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && cache != null) { Fill(cache); return; }
            loading = LoadAll();
        }
        /// The four tiers' stunts (tier, StuntDescriptor name, display-name key) as the game lists them, asked of the game in the profile
        /// session (opened if needed); duplicates kept (the list drops later ones with the same display name). For the pause page's
        /// TRICK LIBRARY too (the race's world runs no Lua).
        public static System.Threading.Tasks.Task<List<(int tier, string name, string key)>> LoadAll()
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            {
                try { return System.Threading.Tasks.Task.FromResult(CoreStuntCatalog.LoadAll()); }
                catch (Exception e) { return System.Threading.Tasks.Task.FromException<List<(int tier, string name, string key)>>(e); }
            }
            if (cache != null) return System.Threading.Tasks.Task.FromResult(cache);
            return System.Threading.Tasks.Task.Run(() =>
            {
                string r = FePopupScripts.AskGame(
                    "local o = \"\"; for stuntTier = 1, 4 do local c = App.GetStuntDescriptorTierContainer( stuntTier ); " +
                    "for i = 0, c:GetNumStuntDescriptors( ) - 1 do o = o .. stuntTier .. \"|\" .. c:GetStuntDescriptorName( i ) .. \"|\" .. c:GetStuntDescriptorDisplayNameLocalization( i ) .. \"^\" end end; " +
                    "ReflexAnswer = o");
                var o = new List<(int tier, string name, string key)>();
                foreach (var line in (r ?? "").Split(FePopupScripts.RowSep))
                {
                    var f = line.Split(FePopupScripts.FieldSep); if (f.Length < 3 || !int.TryParse(f[0], out int t)) continue;
                    o.Add((t, f[1], f[2]));
                }
                if (o.Count > 0) cache = o;
                return o;
            });
        }
        public void Update()
        {
            if (loading == null || !loading.IsCompleted) return;
            var t = loading; loading = null;
            if (t.IsFaulted) { Debug.LogWarning("[Motocard] stunts: " + t.Exception.GetBaseException().Message); return; }
            Fill(t.Result);
        }
        void Fill(List<(int tier, string name, string key)> all)
        {
            data.Clear();
            foreach (var s in all) if (!data.Any(d => d.key == s.Item3)) data.Add(s);   // a duplicate display name is skipped
            Debug.Log("[Motocard] Total Stunts : " + data.Count);
            list?.SetData(data.Count, 0);
            OnStuntListFocusedChanged();
        }
        void Format(FeNode c, int d)
        {
            var s = data[d];
            c.Find("Text")?.SetText(FeMotocardScripts.Str(fe, "MXRaven_Stunts", s.key));
            FeCareerScripts.SetMaterial(fe, c.Find("StuntGestureImage"), "Stunt_" + (s.key.Length > 4 ? s.key.Substring(4) : s.key));
        }
        void OnStuntListFocusedChanged()
        {
            if (data.Count == 0 || list == null) { tier?.SetText(""); return; }
            tier?.SetText(fe.assets.Str("STR_STUNT_TIER" + data[Mathf.Clamp(list.Index, 0, data.Count - 1)].tier));
        }
        public void OnAcceptStuntList()
        {
            if (data.Count == 0 || list == null) return;
            fe.PlaySound("Accept");
            previewStunt?.Invoke(data[list.Index].name);
        }
    }

    // MotocardStunts.lua (the Motocard Stunt Page): STR_STUNTS, back legend; the garage in stunt mode (SetHREnable FALSE,
    // EnableStuntMode) with the player's rider and a "moto" vehicle: the last equipped one when it is an MX / MX Lite / ATV (types 2, 3,
    // 4), else the favourite of type 3, 4, then 2; the camera (-21.52, 1.31, 9.72) / (-0.01, 0.94, -0.41) / (-5.40, 0.18, -4.90). Back
    // -> TravelBack. The StuntPreview component lists the tricks.
    public sealed class MotocardStuntsScript : FeScript
    {
        FeGarage garage; FeNode target; FeStuntPreview preview; bool shown; int vehicle = -1;
        public FeStuntPreview Preview => preview;
        public override void Initialize()
        {
            target = C("RenderTarget");
            var comp = C("StuntPreview.component"); if (comp != null) comp.focusedFlag = true;   // the layer's FocusedComponentIndex 1
            preview = new FeStuntPreview(fe, comp) { previewStunt = n => Debug.Log("[Motocard] MXGarage:PreviewStunt( " + n + " ): the stunt animation is not played by the port's garage") };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("TravelBack"); };
            functions["OnAcceptStuntList"] = () => preview.OnAcceptStuntList();
        }
        public override void Start()
        {
            fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetHeader("STR_STUNTS");
            try { garage ??= new FeGarage(fe.transform); } catch (Exception e) { Debug.LogWarning("[Motocard] " + e.Message); }
            FeGarageData.AttachTarget(garage, target);
            var pr = FeCareerScripts.T("PlayerRider"); var pv = FeCareerScripts.T("PlayerVehicle");
            int TypeOf(int row) => FeTables.I(FeMotocardScripts.Row("VehicleModel", "VehicleModelId", pv.Int("VehicleModelId", row)), "VehicleTypeId");
            vehicle = -1;
            int eq = pr.Int("LastEquippedVehicle", 0, -1), er = pv.Find("PlayerVehicleId", eq);
            if (er >= 0 && (TypeOf(er) == 2 || TypeOf(er) == 3 || TypeOf(er) == 4)) vehicle = eq;
            if (vehicle == -1)
                foreach (int pref in new[] { 3, 4, 2 })
                {
                    int row = Enumerable.Range(0, pv.rows).FirstOrDefault(i => pv.Int("Favorite", i, 0) != 0 && TypeOf(i) == pref) ;
                    if (pv.rows > 0 && pv.Int("Favorite", row, 0) != 0 && TypeOf(row) == pref) { vehicle = pv.Int("PlayerVehicleId", row); break; }
                }
            layer.Stop("LoadingDone"); layer.Play("LoadingStart"); shown = false;
            garage?.SetCamera(new Vector3(-21.52f, 1.31f, 9.72f), new Vector3(-0.01f, 0.94f, -0.41f), new Vector3(-5.40f, 0.18f, -4.90f));
            preview.Start();
        }
        public override void Update(float dt)
        {
            preview.Update();
            garage?.Update(dt);
            if (shown || garage == null || ReflexProfile.Current == null) return;
            shown = true;
            var p = ReflexProfile.Current; var pv = p.T("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", vehicle);
            BikeSpec bs = null; RiderSpec rs = null; var t = new ReflexGameTables(new FeTableSource(FeTables.Shared));
            try
            {
                if (row >= 0)
                {
                    var parts = p.T("PlayerVehicleParts");
                    var ids = Enumerable.Range(0, parts.rows).Where(i => parts.Int("PlayerVehicleId", i) == vehicle).Select(i => parts.Int("VehiclePartId", i)).ToList();
                    bs = ReflexVehicleSpecs.Bike(t, pv.Int("VehicleModelId", row), ids);
                }
                var g = p.T("PlayerRiderGear"); var gear = Enumerable.Range(0, g.rows).Select(i => g.Int("RiderGearId", i)).Where(i => i >= 0).ToList();
                if (gear.Count > 0) rs = ReflexVehicleSpecs.Rider(t, gear);
            }
            catch (Exception e) { Debug.LogWarning("[Motocard] stunt garage: " + e.Message); }
            garage.ShowSpecs(bs, rs, true);
            layer.Stop("LoadingStart"); layer.Play("LoadingDone");   // OnVehicleLoaded
        }
        public override void End() { garage?.Destroy(); garage = null; }
    }

    // MotocardRecords.lua (the Motocard Leaderboard Page) without the network (App.NetworkProfileOnlineLeaderboardsCapable FALSE:
    // STR_LEADERBOARDS_OFFLINE, the local records): the activity types of LeaderboardEventTypeVehicleTypes (sorted 9, 14, 10, 17, 7, 8,
    // 16; STR_MXACTIVITYTYPE_*), their EventTypeLocations (title, poster in "Location Image"); the duration column by the event type
    // (UI EventType -> EventHasLaps / EventHasTimer: BEST LAP TIME, HIGHEST SCORE, BEST TOTAL TIME). Accept on a location:
    // GenerateLocalRecords - the PlayerRecords of that EventTypeLocation (and vehicle type), sorted by BestLapTime / BestScore /
    // TotalTime, each the profile's network name, the vehicle type and App.FormatTime / FormatNumber of the record: the Lua runs as
    // is in the game (profile session) and its rows come back. Toggle / Misc (the online filters) are offline no-ops.
    public sealed class MotocardRecordsScript : FeScript
    {
        FeNode nav, typeRoot, typeList, locRoot, locList, image, duration, noRecords, searching, status, filter;
        FeDynList locations, records;
        readonly List<(int id, string activity)> types = new List<(int, string)>();
        readonly List<(int etl, int product, int icon, int location, int track)> locs = new List<(int, int, int, int, int)>();
        readonly List<(int index, string name, string vehicleKey, string value)> rows = new List<(int, string, string, string)>();
        List<(int id, string key)> vehicleTypes = new List<(int, string)>();
        int vehicleTypeIndex; bool queryValid; System.Threading.Tasks.Task<string> query; int queryGen;
        public int RecordCount => rows.Count; public string LastAnswer { get; private set; }
        public int TypeCount => types.Count; public int LocationCount => locs.Count;

        public override void Initialize()
        {
            nav = C("NavContainer");
            typeRoot = C("ActivityTypeList"); typeList = typeRoot?.Find("NavListContainer");
            locRoot = C("LocationList"); locList = locRoot?.Find("NavListContainer");
            image = C("Location Image");
            var rc = C("RecordsContainer");
            duration = rc?.Find("ColumnTextComponents")?.Kid("Column0");
            noRecords = C("NoRecordsFoundContainer"); searching = C("SearchingForRecordsContainer");
            status = C("StatusTextContainer"); filter = status?.Find("FilteringText");
            locations = new FeDynList(locList, (slot, d) => slot.Find("Text")?.SetText(Str(FeTables.Shared.TitleKey(locs[d].product) ?? "")));
            records = new FeDynList(rc?.Find("LeaderboardContainer"), FormatRecord);
            functions["OnAcceptActivityTypeList"] = OnAcceptActivityTypeList;
            functions["OnCancelActivityTypeList"] = () => { fe.SendUIEvent("TravelBack"); fe.PlaySound("Back"); };
            functions["OnAcceptLocationList"] = OnAcceptLocationList;
            functions["OnCancelLocationList"] = OnCancelLocationList;
            functions["OnRecordsAccept"] = () => { };      // the online motocard popup (online only)
            functions["OnRecordsCancel"] = OnRecordsCancel;
            functions["OnChangeFilter"] = UpdateLegend;     // online only
            functions["OnChangeVehicleType"] = UpdateLegend;
            if (typeList != null) typeList.onListChanged = (l, old) => OnActivityTypeListFocusedChanged();
            if (locList != null) locList.onListChanged = (l, old) => { locations.Refresh(); OnLocationListFocusedChanged(); };
            if (records.list != null) records.list.onListChanged = (l, old) => records.Refresh();
        }

        public override void Start()
        {
            fe.SetButtonLegend("back", true, "STR_BACK");
            queryValid = false; vehicleTypeIndex = 0; rows.Clear();
            PopulateActivityTypeList();
            records.SetData(0);
            if (typeRoot != null) typeRoot.focusedFlag = true;
            if (locRoot != null) { locRoot.focusedFlag = false; locRoot.Visible = true; }
            if (noRecords != null) noRecords.Visible = false;
            if (searching != null) searching.Visible = false;
            UpdateLegend();
        }

        void PopulateActivityTypeList()
        {
            types.Clear();
            int last = -1;
            foreach (var r in FeTables.Shared.Rows("LeaderboardEventTypeVehicleTypes").OrderBy(r => FeTables.I(r, "EventTypeId")))
            {
                int id = FeTables.I(r, "EventTypeId"); if (id == last) continue; last = id;
                types.Add((id, FeTables.S(FeMotocardScripts.Row("EventType", "EventTypeId", id), "ActivityName") ?? ""));
            }
            int[] order = { 9, 14, 10, 17, 7, 8, 16 };
            int Rank(int id) { int i = Array.IndexOf(order, id); return i < 0 ? 1000 : i; }
            var sorted = types.Select((t, i) => (t, i)).OrderBy(x => Rank(x.t.id)).ThenBy(x => x.i).Select(x => x.t).ToList();   // MotocardRecords_ActivityTypeSort
            types.Clear(); types.AddRange(sorted);
            typeList.numItems = types.Count; typeList.absIndex = 0;
            OnActivityTypeListFocusedChanged();
        }
        void OnActivityTypeListFocusedChanged()
        {
            records.SetData(0);
            if (types.Count == 0) return;
            var t = types[Mathf.Clamp(typeList.absIndex, 0, types.Count - 1)];
            if (typeList.kids.Count > 0) typeList.kids[0].Find("Text")?.SetText(Str("STR_MXACTIVITYTYPE_" + t.activity.ToUpperInvariant()));   // OnFormatActivityTypeList
            PopulateLocationList();
            var l = typeRoot.Kid("LeftArrow"); var r = typeRoot.Kid("RightArrow");
            if (l != null) l.Visible = typeList.absIndex > 0; if (r != null) r.Visible = typeList.absIndex < types.Count - 1;
            fe.vars["EventType"] = t.activity;
            // the duration column: the UI manager's EventType listener (005a3820) sets EventHasLaps / EventHasTimer from the activity
            if (durationKeys.TryGetValue(t.activity, out var key)) duration?.SetText(Str(key));
            else { duration?.SetText(""); string a = t.activity; durationTask = System.Threading.Tasks.Task.Run(() => (a, DurationKeyOf(a))); }
        }
        System.Threading.Tasks.Task<(string activity, string key)> durationTask;
        static readonly Dictionary<string, string> durationKeys = new Dictionary<string, string>();
        static string DurationKeyOf(string activity)
        {
            if (durationKeys.TryGetValue(activity, out var k)) return k;
            // asked of the game once per activity (the listener's own switch on the activity enum)
            string r = FePopupScripts.AskGame("App.SetUIVariableString( \"EventType\", \"" + activity + "\" ); " +
                "local l = App.GetUIVariableInt( \"EventHasLaps\" ); local t = App.GetUIVariableInt( \"EventHasTimer\" ); " +
                "if( l == TRUE and \"" + activity + "\" ~= \"WaypointRace\" ) then ReflexAnswer = \"STR_LEADERBOARD_BESTLAPTIME\" " +
                "elseif( t == TRUE ) then ReflexAnswer = \"STR_LEADERBOARD_HIGHESTSCORE\" " +
                "else ReflexAnswer = \"STR_LEADERBOARD_BESTTOTALTIME\" end");
            k = string.IsNullOrEmpty(r) ? "STR_LEADERBOARD_BESTTOTALTIME" : r;
            if (!string.IsNullOrEmpty(r)) lock (durationKeys) durationKeys[activity] = k;
            return k;
        }
        void PopulateLocationList()
        {
            locs.Clear();
            int type = types[Mathf.Clamp(typeList.absIndex, 0, types.Count - 1)].id;
            foreach (var r in FeTables.Shared.Rows("EventTypeLocations").Where(r => FeTables.I(r, "EventTypeId") == type))
                locs.Add((FeTables.I(r, "EventTypeLocationId"), FeTables.I(r, "StoreProductId"), FeTables.I(r, "PosterIconId"), FeTables.I(r, "EventLocationId"), FeTables.I(r, "TrackId", 0)));
            locations.SetData(locs.Count, 0);
            OnLocationListFocusedChanged();
        }
        void PopulateVehicleTypeList()
        {
            vehicleTypes = new List<(int, string)> { (-1, "STR_ALL") };
            int type = types[Mathf.Clamp(typeList.absIndex, 0, types.Count - 1)].id;
            foreach (var r in FeTables.Shared.Rows("LeaderboardEventTypeVehicleTypes").Where(r => FeTables.I(r, "EventTypeId") == type))
            {
                int vt = FeTables.I(r, "VehicleTypeId");
                vehicleTypes.Add((vt, FeTables.S(FeMotocardScripts.Row("VehicleType", "VehicleTypeId", vt), "Name") ?? ""));
            }
            vehicleTypeIndex = 0;
        }
        void OnLocationListFocusedChanged()
        {
            PopulateVehicleTypeList();
            if (locs.Count == 0) return;
            FeCareerScripts.SetIcon(fe, image, locs[Mathf.Clamp(locList.absIndex, 0, locs.Count - 1)].icon);
        }
        void OnAcceptActivityTypeList()
        {
            if (typeRoot != null) typeRoot.focusedFlag = false; if (locRoot != null) locRoot.focusedFlag = true;
            fe.PlaySound("Accept");
            OnLocationListFocusedChanged(); UpdateLegend();
        }
        void OnCancelLocationList()
        {
            if (typeRoot != null) typeRoot.focusedFlag = true; if (locRoot != null) locRoot.focusedFlag = false;
            fe.PlaySound("Back"); queryValid = false; UpdateLegend();
        }
        void OnAcceptLocationList()
        {
            if (locs.Count == 0) return;
            fe.PlaySound("Accept");
            queryValid = true; UpdateLegend();
            if (searching != null) searching.Visible = true; if (noRecords != null) noRecords.Visible = false;
            records.SetData(0);
            QueryLeaderboards();
        }
        void OnRecordsCancel()
        {
            if (nav != null) nav.focusedFlag = true; if (locRoot != null) locRoot.focusedFlag = true;
            queryValid = false; fe.PlaySound("Back"); UpdateLegend();
        }

        /// QueryLeaderboards (offline): GenerateLocalRecords as the game's Lua.
        void QueryLeaderboards()
        {
            var t = types[Mathf.Clamp(typeList.absIndex, 0, types.Count - 1)]; var l = locs[Mathf.Clamp(locList.absIndex, 0, locs.Count - 1)];
            int vehicleType = vehicleTypes[Mathf.Clamp(vehicleTypeIndex, 0, vehicleTypes.Count - 1)].id;
            int gen = ++queryGen;
            string lua = RecordsLua(t.activity, l.location, l.track, vehicleType);
            query = System.Threading.Tasks.Task.Run(() => FePopupScripts.AskGame(lua));
            UpdateLegend();
        }

        /// GenerateLocalRecords as Lua for the game (the activity's EventType, EventHasLaps / EventHasTimer, MapEventTypeId,
        /// GetEventTypeLocationId, the PlayerRecords query and sort, FormatTime / FormatNumber): rows "index|name|vehicle key|value^",
        /// then "#" and the EventTypeLocationId.
        public static string RecordsLua(string activity, int location, int track, int vehicleType) =>
                "App.SetUIVariableString( \"EventType\", \"" + activity + "\" ); " +
                "local hasLaps = App.GetUIVariableInt( \"EventHasLaps\" ); local hasTimer = App.GetUIVariableInt( \"EventHasTimer\" ); " +
                "if( hasLaps == FALSE and hasTimer == FALSE ) then hasLaps = TRUE; end " +
                "local mxProfile = App.GetMXProfile( 0 ); local profileName = mxProfile:GetNetworkName( ); " +
                "local mxTableHelper = App.GetMXTableHelper( ); " +
                "if( \"" + activity + "\" == \"WaypointRace\" ) then hasLaps = FALSE; end " +
                "local eventType = App.GetMXActivityType( \"" + activity + "\" ); " +
                "local eventTypeId = mxTableHelper:MapEventTypeId( eventType ); " +
                "local eventTypeLocationId = mxTableHelper:GetEventTypeLocationId( eventTypeId, " + location + ", " + track + " ); " +
                "local query = App.RdbQuery( ); query:AddOperation_int( \"EventTypeLocationId\", App.Equals, eventTypeLocationId ); " +
                (vehicleType != -1 ? "query:AddOperation_int( \"VehicleTypeId\", App.Equals, " + vehicleType + " ); " : "") +
                "local rows = App.LookupPlayerTable( \"PlayerRecords\", 0 ):query( query ); " +
                "local sort = App.RdbSortOperations( ); " +
                "if( hasLaps == TRUE ) then sort:AddSortOperation( \"BestLapTime\", App.Ascending ); elseif( hasTimer == TRUE ) then sort:AddSortOperation( \"BestScore\", App.Ascending ); else sort:AddSortOperation( \"TotalTime\", App.Ascending ); end " +
                "rows:Sort( sort ); local o = \"\"; local index = 1; rows:EnumReset( ); " +
                "while not ( rows:EnumDone( ) ) do local r = rows:EnumNext( ); local vt = r:parentValue( \"VehicleTypeId\" ); local v = \"\"; " +
                "if( hasLaps == TRUE ) then v = App.FormatTime( r:cell_float( \"BestLapTime\" ) ); elseif( hasTimer == TRUE ) then v = App.FormatNumber( r:cell_int( \"BestScore\" ) ); else v = App.FormatTime( r:cell_float( \"TotalTime\" ) ); end " +
                "o = o .. index .. \"|\" .. profileName .. \"|\" .. vt:cell_string( \"Name\" ) .. \"|\" .. v .. \"^\"; index = index + 1; end " +
                "ReflexAnswer = o .. \"#\" .. eventTypeLocationId";
        /// RecordsLua's answer as rows.
        public static List<(int index, string name, string vehicleKey, string value)> ParseRecords(string r)
        {
            var o = new List<(int, string, string, string)>();
            foreach (var line in (r ?? "").Split(FePopupScripts.RowSep))
            {
                var f = line.Split(FePopupScripts.FieldSep); if (f.Length < 4 || !int.TryParse(f[0], out int i)) continue;
                o.Add((i, f[1], f[2], f[3]));
            }
            return o;
        }

        public override void Update(float dt)
        {
            if (durationTask != null && durationTask.IsCompleted)
            {
                var d = durationTask; durationTask = null;
                if (!d.IsFaulted && types.Count > 0 && types[Mathf.Clamp(typeList.absIndex, 0, types.Count - 1)].activity == d.Result.activity) duration?.SetText(Str(d.Result.key));
            }
            if (query == null || !query.IsCompleted) return;
            var q = query; query = null;
            string r = q.IsFaulted ? null : q.Result; LastAnswer = r;
            rows.Clear(); rows.AddRange(ParseRecords(r));
            Debug.Log("[Motocard] local records: " + rows.Count + " (" + (r == null ? "no answer" : "EventTypeLocationId " + r.Substring(r.LastIndexOf('#') + 1)) + ")");
            UpdateLegend();
            if (locRoot != null) locRoot.focusedFlag = true;
            if (searching != null) searching.Visible = false;
            if (rows.Count == 0) { if (noRecords != null) noRecords.Visible = true; records.SetData(0); }
            else { records.SetData(rows.Count, 0); if (noRecords != null) noRecords.Visible = false; }
        }
        void FormatRecord(FeNode c, int d)
        {
            var r = rows[d];
            c.Find("IndexColumn")?.SetText(r.index.ToString(CultureInfo.InvariantCulture));
            c.Find("PlayerNameColumn")?.SetText(r.name);
            c.Find("Column0")?.SetText(r.value);
            c.Find("Column1")?.SetText(Str(r.vehicleKey));
        }
        void UpdateLegend()
        {   // offline: no filter / vehicle type legend, no status text
            fe.SetHeader("STR_LEADERBOARDS_OFFLINE");
            fe.SetButtonLegend("toggle", false, "STR_EMPTY");
            fe.SetButtonLegend("misc", false, "STR_EMPTY");
            if (status != null) status.Visible = false;
        }
        public override void End() { rows.Clear(); if (fe.TopPopup == "NetworkNotificationPopup") fe.SendUIEvent("HideNetworkNotificationPopup"); }
    }
}
