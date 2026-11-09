// MOTOCAREER and the motocard: C# ports of the career pages' Lua (_re/formats/out/ui: TitleLoadGame, MotocardAutosaveNotice,
// MotocardScreenAdjustment, MotocardWelcome, CareerWelcome, RaceAndRide, RaceAndRideEvents, CareerPlayerInformation, StartRacePopup,
// LockedPopup, InvalidRaceRequirements, MotocardLayer / Motocard). Everything the Lua asks the game (App.LookupPlayerTable,
// MXTableHelper IsLocked / IsStoreProductNew / GetPlayerEventStatus, App.CalculateCareerCompletion, App.GetUISetting) and every change
// it makes (SetPlayerEventStatus, setCell_int, App.SetUISetting, PurchaseStoreAsset, SetStoreProductNotNew, App.RequestSaveGame) goes
// to the game itself through ReflexProfile (a profile session: the game booted to its front end with the profile signed in from
// its own save, native/drafts/world_runtime/career.h). The static tables (Series, StoreProducts, EventLocation, ...) are the install's
// MXTables_Default as for the arcade pages (FeTables).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    public static class FeCareerScripts
    {
        public static FeScript Create(string script)
        {
            switch (script)
            {
                case "TitleLoadGame": return new TitleLoadGameScript();
                case "MotocardAutosaveNotice": return new MotocardAutosaveNoticeScript();
                case "CareerWelcome": return new CareerWelcomeScript();
                case "RaceAndRide": return new RaceAndRideScript();
                case "RaceAndRideEvents": return new RaceAndRideEventsScript();
                case "CareerPlayerInformation": return new CareerPlayerInformationScript();
                case "StartRacePopup": return new StartRacePopupScript();
                case "CareerPracticePopup": return new CareerPracticePopupScript();
                case "JerseyNameEntryPopup": return new JerseyEntryPopupScript(false);
                case "JerseyNumberEntryPopup": return new JerseyEntryPopupScript(true);
                case "InvalidMotocardNamePopup": return new InvalidMotocardNamePopupScript();
                case "MotocardNameEntry": return new MotocardNameEntryScript();
                case "RewardVehiclePresentation": return new RewardVehiclePresentationScript();
                case "CareerCompletion": return new CareerCompletionScript();
                case "GarageHUB": return new GarageHubScript();                       // FrontEndGarage.cs
                case "GarageRiderCustomize": return new GarageRiderCustomizeScript();
                case "GarageMyRides": return new GarageMyRidesScript();
                case "GarageVehicleCustomize": return new GarageVehicleCustomizeScript();
                case "PurchaseProductPopup": return new PurchaseProductPopupScript();
                case "GarageVehicleTuning": case "VehiclesQuickViewPopup": return FeGarageScripts.Create(script);   // FrontEndGarageTuning.cs (WP-G)
                case "InsufficientFundsPopup": return new OneButtonPopupScript("STR_INSUFFICIENTFUNDS_TITLE", "STR_INSUFFICIENTFUNDS_MESSAGE", "STR_CONTINUE", null);
                case "LockedPopup": return new OneButtonPopupScript("STR_LOCKEDPOPUP_TITLE", "STR_LOCKEDPOPUP_MESSAGE", "STR_CONTINUE", null);
                case "InvalidRaceRequirements": return new OneButtonPopupScript("STR_INVALID_VEHICLEREQ_TITLE", "STR_INVALID_VEHICLEREQ_MESSAGE", "STR_CONTINUE", null);
                default: return FeMotocardScripts.Create(script);   // the motocard setup and pages (FrontEndMotocard.cs)
            }
        }

        /// The profile the pages read (null until the first session answered).
        public static ProfileData Profile => ReflexProfile.Current;
        public static ProfileData.Table T(string table) => Profile?.T(table) ?? new ProfileData.Table();

        /// The node whose text uses a localization key (DYN_ / EMB_ strings the Lua sets with App.SetLocalizationString).
        public static FeNode WithKey(FeNode n, string key)
        {
            if (n.P("LocalizationKey") == key) return n;
            foreach (var k in n.kids) { var f = WithKey(k, key); if (f != null) return f; }
            return null;
        }
        public static void SetIcon(FrontEndManager fe, FeNode n, int icon)
        {
            if (n?.image == null) return;
            var (tex, uv) = fe.assets.Icon(icon); n.image.texture = tex; n.image.uvRect = uv; n.image.Dirty();
        }
        public static void SetMaterial(FrontEndManager fe, FeNode n, string material)
        {
            if (n?.image == null) return;
            var (tex, uv) = fe.assets.Material(material); n.image.texture = tex; n.image.uvRect = uv; n.image.Dirty();
        }
        /// The EMB_ entries' own texts of a string table (the localiz asset Rfx.Strings reads: version 2, count, key pool, offsets, then
        /// UTF-16 strings; an EMB_ key's string is a '?' template followed by its text, to which the game appends the DYN_ value).
        public static Dictionary<string, string> Embeds(byte[] b)
        {
            var d = new Dictionary<string, string>();
            try
            {
                int n = BitConverter.ToInt32(b, 4), pool = BitConverter.ToInt32(b, 8), p = 12;
                var keys = new string[n];
                for (int i = 0; i < n; i++) { int e = Array.IndexOf(b, (byte)0, p); keys[i] = System.Text.Encoding.ASCII.GetString(b, p, e - p); p = e + 1; }
                p = 12 + pool + 4 * n + 4;
                string Next() { int s0 = p; while (p + 1 < b.Length && (b[p] != 0 || b[p + 1] != 0)) p += 2; string v = System.Text.Encoding.Unicode.GetString(b, s0, p - s0); p += 2; return v; }
                for (int i = 0; i < n; i++) { string a = Next(); if (keys[i].StartsWith("EMB_", StringComparison.Ordinal) && a.Trim('?').Length == 0) d[keys[i]] = Next(); }
            }
            catch (Exception) { }
            return d;
        }
        /// UISpinnerComponent (TargetComponent, RotationSpeed degrees per second).
        public static void Spin(FeNode spinner, FeNode target, float dt)
        {
            if (spinner == null || target == null) return;
            target.rotZ = (target.rotZ + OriginalWorld.OriginalUi.F(spinner.props, "RotationSpeed", 180f) * dt) % 360f;
        }
        /// The EMB_ text of a key in MXRaven_Default_Strings (Embeds; "" when the key has none).
        public static string Embed(FrontEndManager fe, string key)
        {
            embeds ??= Embeds(fe.assets.data.Read(fe.assets.data.Pkg("FrontEnd"), "localiz", "MXRaven_Default_Strings"));
            return embeds.TryGetValue(key, out var v) ? v : "";
        }
        static Dictionary<string, string> embeds;
        static Dictionary<string, object> Row(string table, string col, int v) => FeTables.Shared.Rows(table).FirstOrDefault(r => FeTables.I(r, col) == v);
        public static int Lookup(string table, int seriesId, string col) => FeTables.I(table == "SeriesEventLocations"
            ? FeTables.Shared.Rows(table).Where(r => FeTables.I(r, "SeriesId") == seriesId).OrderBy(r => FeTables.I(r, "EventNumber", 1)).FirstOrDefault()
            : Row(table, "SeriesId", seriesId), col);
        /// The VehicleTypeId of the profile's equipped vehicle (PlayerRider.LastEquippedVehicle -> PlayerVehicle -> VehicleModel), -1 none.
        public static int PlayerVehicleTypeId()
        {
            var pv = T("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", T("PlayerRider").Int("LastEquippedVehicle", 0, 0));
            return row < 0 ? -1 : FeTables.I(Row("VehicleModel", "VehicleModelId", pv.Int("VehicleModelId", row)), "VehicleTypeId");
        }
    }

    // TitleLoadGame.lua (Load Game Page): the game's load of the signed-in profile (here: the profile session coming up, which
    // signs in and loads savegame.bxml as the PC game does), then, as OnGameLoaded / OnGameLoadFailed:
    // * loaded: MXProfile:ValidateDLC (asked of the game) FALSE -> the DLC bind error popup; else MotocardSetup ? GotoMainMenu :
    //   GotoCreateMotoCard;
    // * the load failed with FileCorrupt (status 4: a save the Steam Cloud stand-in hands over, its 0x27720006 trailer intact, whose
    //   BXML document does not read) -> StorageQuestion "DeviceCorrupted" (ButtonOne GotoCreateMotoCard, ButtonTwo SetSaveEnabled( FALSE )
    //   and GotoMainMenu);
    // * no save yet (the first run): App.DoesSystemHaveSaveGameSpace ? GotoCreateMotoCard : StorageNotification "DeviceOutOfSpace",
    //   SetSaveEnabled( FALSE ), GotoCreateMotoCard.
    // A profile that cannot be read at all falls back to the guest path (the profile reminder, as without Steam).
    public sealed class TitleLoadGameScript : FeScript
    {
        System.Threading.Tasks.Task<bool> open; System.Threading.Tasks.Task<(bool dlcValid, bool space)> checks;
        bool routed, hadSave, corrupt; FeNode spinner, spinImage;
        /// Tests: the verdicts the page routed on.
        public static string LastRoute { get; private set; }
        public override void Initialize()
        {
            spinner = C("LoadingSpinner1"); spinImage = C("LoadingImage1");
            functions["OnAcceptStorageQuestionButtonOne"] = () =>
            {
                if (!ReflexProfile.IsOpen)
                {   // the game's own session could not even come up with that file (the interpreter's allocator gave out where the PC
                    // game reports FileCorrupt): set it aside, as the PC game's next save would overwrite it, and sign in again
                    try { System.IO.File.Copy(ReflexProfile.SaveFile, ReflexProfile.SaveFile + ".corrupt", true); System.IO.File.Delete(ReflexProfile.SaveFile); } catch (Exception e) { Debug.LogWarning("[Career] " + e.Message); }
                    ReflexProfile.Open();
                }
                GotoCreateMotoCard();
            };
            functions["OnAcceptStorageQuestionButtonTwo"] = () => { ReflexProfile.Run("lua App.GetMXProfile( 0 ):SetSaveEnabled( FALSE )"); GotoMainMenu(); };
        }
        public override void Start()
        {
            routed = false; checks = null;
            hadSave = ReflexProfile.HasSave;
            corrupt = !Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && hadSave && SaveLooksCorrupt();
            try { open = ReflexProfile.Open(); }
            catch (Exception e) { Debug.LogWarning("[Career] profile not opened: " + e.Message); open = System.Threading.Tasks.Task.FromResult(false); }
        }
        /// PCStorageDevice::vf5 (006d4710) hands the file over only with the trailer 0x27720006; the saved-game worker's load then fails
        /// with FileCorrupt when the document does not read.
        public static bool SaveLooksCorrupt()
        {
            try
            {
                var b = System.IO.File.ReadAllBytes(ReflexProfile.SaveFile);
                if (b.Length < 8 || BitConverter.ToUInt32(b, b.Length - 4) != 0x27720006u) return false;   // no "SaveGame" directory: a new one
                var doc = new byte[b.Length - 4]; Buffer.BlockCopy(b, 0, doc, 0, doc.Length);
                var root = Rfx.Bxml(doc);
                return root == null;
            }
            catch (Exception) { return true; }
        }
        public override void Update(float dt)
        {
            FeCareerScripts.Spin(spinner, spinImage, dt);
            if (routed || open == null || !open.IsCompleted || fe.Busy) return;
            var p = ReflexProfile.Current;
            if (!open.Result || p == null || !p.signedIn)
            {
                routed = true;
                if (corrupt) { LastRoute = "corrupt"; fe.vars["StorageQuestion"] = "DeviceCorrupted"; fe.SendUIEvent("ShowStorageDeviceQuestionPopup"); return; }
                LastRoute = "guest"; fe.SendUIEvent("ShowProfileReminderPopup"); return;   // no profile: the guest path
            }
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            {
                // Core garage profiles are ready without the legacy DLC/Steam queries or career setup flow.
                routed = true; LastRoute = "main"; GotoMainMenu(); return;
            }
            if (checks == null)
            {
                checks = System.Threading.Tasks.Task.Run(() =>
                {
                    string dlc = FePopupScripts.AskGame("if( App.GetMXProfile( 0 ):ValidateDLC( ) == TRUE ) then ReflexAnswer = \"1\" else ReflexAnswer = \"0\" end");
                    string space = FePopupScripts.AskGame("if( App.DoesSystemHaveSaveGameSpace( 0 ) == TRUE ) then ReflexAnswer = \"1\" else ReflexAnswer = \"0\" end");
                    return (dlc != "0", space != "0");   // no answer (an older libreflexworld): valid, space
                });
                return;
            }
            if (!checks.IsCompleted) return;
            routed = true;
            var (dlcValid, space) = checks.IsFaulted ? (true, true) : checks.Result;
            Debug.Log("[Career] profile loaded: motocard " + (p.MotocardSetup ? "set up" : "not set up") + ", save " + (hadSave ? ReflexProfile.SaveFile : "none") + (corrupt ? " (corrupt)" : "") + ", DLC " + (dlcValid ? "valid" : "invalid") + ", space " + space);
            if (corrupt)
            {   // OnGameLoadFailed, saveLoadStatus 4 (FileCorrupt)
                LastRoute = "corrupt";
                fe.vars["StorageQuestion"] = "DeviceCorrupted"; fe.SendUIEvent("ShowStorageDeviceQuestionPopup");
                return;
            }
            if (!hadSave)
            {   // OnGameLoadFailed (no save): DoesSystemHaveSaveGameSpace
                LastRoute = space ? "new" : "nospace";
                if (!space)
                {
                    fe.vars["StorageNotification"] = "DeviceOutOfSpace"; fe.SendUIEvent("ShowStorageNotificationPopup");
                    ReflexProfile.Run("lua App.GetMXProfile( 0 ):SetSaveEnabled( FALSE )");
                }
                GotoCreateMotoCard();
                return;
            }
            if (!dlcValid) { LastRoute = "dlc"; fe.SendUIEvent("ShowDLCBindErrorPopup"); return; }   // OnGameLoaded: Bad DLC!
            LastRoute = p.MotocardSetup ? "main" : "create";
            if (p.MotocardSetup) GotoMainMenu(); else GotoCreateMotoCard();
        }
        void GotoMainMenu() => fe.SendUIEvent("Main Menu Page");
        void GotoCreateMotoCard() => fe.SendUIEvent("Create Motocard");
    }

    // MotocardAutosaveNotice.lua: PlayerInMotocardSetup, legend CONTINUE; Accept -> "Advance" (the PC path; the PS3 asks for a
    // name first). The autosave icon spins.
    public sealed class MotocardAutosaveNoticeScript : FeScript
    {
        FeNode spinner, image;
        public override void Initialize()
        {
            spinner = C("Spinner"); image = C("SavingImage");
            // (PS3: a profile that is not signed in names its motocard first -> AdvanceNameEntry.) The port's stand-in for the Steam
            // persona the PC build signs in with: the Motocard Setup Name Page while no persona has been entered (ReflexProfile.HasPersona).
            functions["OnAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent(ReflexProfile.HasPersona ? "Advance" : "AdvanceNameEntry"); };
        }
        public override void Start() { fe.vars["PlayerInMotocardSetup"] = true; fe.SetButtonLegend("accept", true, "STR_CONTINUE"); }
        public override void Update(float dt) => FeCareerScripts.Spin(spinner, image, dt);
    }

    // CareerWelcome.lua: the career's welcome text and the career difficulty list (80 / 95 / 105 / 120 = STR_AI_LEVEL1..4, from
    // App.GetUISetting "CareerDifficulty"); Accept: App.SetUISetting, SetPlayerEventStatus "CareerWelcome", App.SaveUISettings,
    // App.RequestSaveGame, then CareerModeMenuPage.
    public sealed class CareerWelcomeScript : FeScript
    {
        static readonly (int skill, string key)[] Data = { (80, "STR_AI_LEVEL1"), (95, "STR_AI_LEVEL2"), (105, "STR_AI_LEVEL3"), (120, "STR_AI_LEVEL4") };
        FeNode list, left, right;
        public override void Initialize()
        {
            list = C("List"); left = C("LeftArrow"); right = C("RightArrow");
            functions["OnAcceptKeypress"] = OnAcceptKeypress;
            list.onListChanged = (l, old) => Changed();
        }
        public override void Start()
        {
            fe.PlayLocalizedSound("UIAudio", "UI_MOTO_INTRO", "UI_MOTO_INTRO_ENG/VO/INTRO", layer.Name);
            int setting = FeCareerScripts.Profile != null && FeCareerScripts.Profile.uiSettings.TryGetValue("CareerDifficulty", out var s) ? s : 80;
            int sel = 0; for (int i = 0; i < Data.Length; i++) if (Data[i].skill == setting) sel = i;
            list.numItems = Data.Length; list.absIndex = sel;
            Changed();
        }
        void Changed()
        {
            int i = Mathf.Clamp(list.absIndex, 0, Data.Length - 1);
            var t = list.kids.Count > 0 ? list.kids[0] : null; t?.SetText(Str(Data[i].key));
            left.Visible = i > 0; right.Visible = i < Data.Length - 1;
        }
        public override void End() => fe.StopLocalizedSound(layer.Name);
        void OnAcceptKeypress()
        {
            int skill = Data[Mathf.Clamp(list.absIndex, 0, Data.Length - 1)].skill;
            ReflexProfile.Run("ui_setting CareerDifficulty " + skill, "event_status CareerWelcome 1", "save_ui_settings", "save");
            fe.PlaySound("Accept");
            fe.SendUIEvent("CareerModeMenuPage");
        }
    }

    // RaceAndRide.lua (Race and Ride Page): the career's series groups: Series rows grouped by their store product's parent
    // (StoreProducts.ParentStoreProductId), hidden-and-locked groups skipped, sorted locked last then by SortValue (SeriesSort);
    // poster IconId, lock / new / DLC marks, a gold trophy when every series of the group has BestPlaceFinish 0; the info box
    // (title, STR_SERIES_DESCRIPTION_<product>, brand) and the eight progress slots (each series' lock / new / medal). Present:
    // the career completion (PlayerRider.PercentComplete; CareerCompleted -> the completion page). Accept: locked -> the locked
    // popup; Free Ride (2904) -> the challenges page (FrontEndChallenges.cs); else the events page.
    public sealed class RaceAndRideScript : FeScript
    {
        sealed class Item { public int product, sortValue, placing; public bool locked, isNew, dlc; }
        List<Item> data = new List<Item>(); FePosterList posters; FeNode focusedItem, title, description, brand, progress, leftArrow, rightArrow;
        int version = -1;
        public override void Initialize()
        {
            posters = new FePosterList(C("Select List"));
            focusedItem = C("FocusedItem"); title = C("Title"); description = C("Description"); brand = C("BrandImageIcon"); progress = C("ProgressContainer");
            leftArrow = C("Left Arrow"); rightArrow = C("Right Arrow");
            functions["OnAccept"] = OnAccept;
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("GotoMainMenuPage"); };
            functions["OnGarageKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("GotoGaragePage"); };
            posters.list.onListChanged = (l, old) => { posters.Scrolled(l.absIndex - old); OnSeriesListFocusedChanged(); };
        }
        public override void Start()
        {
            fe.SetButtonLegend("back", true, "STR_BACK"); fe.SetButtonLegend("toggle", true, "STR_GARAGE");
            fe.SetButtonLegend("start", false, "STR_EMPTY"); fe.SetButtonLegend("misc", false, "STR_EMPTY");
            fe.SetHeader("STR_RACEANDRIDE");
            Build();
        }
        public override void Update(float dt)
        {
            posters.Update(dt);
            if (ReflexProfile.Version != version && !ReflexProfile.Busy) Build();   // the game answered again (a save, a session after a race)
        }
        void Build()
        {
            version = ReflexProfile.Version;
            var p = FeCareerScripts.Profile; data.Clear();
            if (p == null) { posters.list.numItems = 0; return; }
            var t = FeTables.Shared; var seriesStatus = p.T("PlayerSeriesStatus");
            var series = t.Rows("Series").Where(r => FeTables.I(r, "SeriesId") > -1).ToList();
            int Parent(Dictionary<string, object> r) => FeTables.I(t.Rows("StoreProducts").FirstOrDefault(sp => FeTables.I(sp, "StoreProductId") == FeTables.I(r, "StoreProductId")), "ParentStoreProductId");
            foreach (var group in series.Select(r => (r, parent: Parent(r))).OrderBy(x => x.parent).GroupBy(x => x.parent))
            {
                int parent = group.Key;
                bool locked = p.IsLocked(parent), hidden = p.IsHidden(parent);
                if (hidden && locked) continue;
                bool isNew = p.IsNew(parent), dlc = p.IsDlc(parent);
                int sortValue = 1000, firsts = 0, count = 0;
                foreach (var (r, _) in group)
                {
                    count++; sortValue = FeTables.I(r, "SortValue");
                    if (p.IsNew(FeTables.I(r, "StoreProductId"))) isNew = true;
                    int row = seriesStatus.Find("SeriesId", FeTables.I(r, "SeriesId"));
                    if (row >= 0 && seriesStatus.Int("BestPlaceFinish", row) == 0) firsts++;
                }
                data.Add(new Item { product = parent, locked = locked, isNew = isNew, placing = firsts == count ? 0 : 1000, sortValue = sortValue, dlc = dlc });
            }
            data = data.Select((d, i) => (d, i)).OrderBy(x => x.d.locked ? 1 : 0).ThenBy(x => x.d.sortValue).ThenBy(x => x.i).Select(x => x.d).ToList();   // SeriesSort
            posters.list.numItems = data.Count;
            posters.list.absIndex = Mathf.Clamp(posters.list.absIndex, 0, Math.Max(0, data.Count - 1));
            OnSeriesListFocusedChanged();
            Present();
        }
        public override void Present()
        {
            // RaceAndRide:Present: the career completion
            var p = FeCareerScripts.Profile; if (p == null) return;
            int percent = (int)(p.careerCompletion * 100);
            var cmds = new List<string>();
            if (FeCareerScripts.T("PlayerRider").Int("PercentComplete", 0, 0) != percent) cmds.Add("set_int PlayerRider 0 PercentComplete " + percent);
            if (p.careerCompletion >= 1f && p.EventStatus("CareerCompleted") == 0) { cmds.Add("event_status CareerCompleted 1"); fe.SendUIEvent("CareerCompletionPage"); }
            if (cmds.Count > 0) ReflexProfile.Run(cmds.ToArray());
        }
        void Format(FeNode c, int d)
        {
            var it = data[d];
            FeCareerScripts.SetIcon(fe, c.Find("EventImage"), FeTables.I(FeTables.Shared.Rows("StoreProducts").FirstOrDefault(r => FeTables.I(r, "StoreProductId") == it.product), "IconId"));
            if (c.Find("LockImage") is FeNode l) l.Visible = it.locked;
            if (c.Find("NewImage") is FeNode n) n.Visible = it.isNew;
            if (c.Find("DLCImage") is FeNode dl) dl.Visible = it.dlc;
            if (c.Find("Gold_ImageComponent") is FeNode g) g.Visible = it.placing == 0;
            if (c.Find("Silver_ImageComponent") is FeNode s) s.Visible = it.placing == 1;
            if (c.Find("Bronze_ImageComponent") is FeNode b) b.Visible = it.placing == 2;
        }
        void OnSeriesListFocusedChanged()
        {
            layer.Stop("FocusedItem_Changed"); layer.Play("FocusedItem_Changed");
            posters.Format(Format);
            if (data.Count == 0) { focusedItem.Visible = false; return; }
            focusedItem.Visible = true; Format(focusedItem, posters.list.absIndex);
            int i = posters.list.absIndex, n = posters.list.numItems; leftArrow.Visible = i > 0; rightArrow.Visible = i < n - 1;   // UpdateArrows
            var it = data[i]; var t = FeTables.Shared; var p = FeCareerScripts.Profile;
            title.SetText(Str(t.TitleKey(it.product) ?? ""));
            description.SetText(Str("STR_SERIES_DESCRIPTION_" + it.product));
            // UpdateStoreProductBrand: StoreProducts.BrandId -> Brand.BrandIconId
            int brandId = FeTables.I(t.Rows("StoreProducts").FirstOrDefault(r => FeTables.I(r, "StoreProductId") == it.product), "BrandId");
            int icon = brandId == -1 ? -1 : FeTables.I(t.Rows("Brand").FirstOrDefault(r => FeTables.I(r, "BrandId") == brandId), "BrandIconId");
            brand.Visible = icon != -1; if (icon != -1) FeCareerScripts.SetIcon(fe, brand, icon);
            // series progress: the group's series by SeriesId in the eight slots
            var status = p.T("PlayerSeriesStatus");
            var rows = t.Rows("Series").Where(r => FeTables.I(t.Rows("StoreProducts").FirstOrDefault(sp => FeTables.I(sp, "StoreProductId") == FeTables.I(r, "StoreProductId")), "ParentStoreProductId") == it.product)
                        .OrderBy(r => FeTables.I(r, "SeriesId")).ToList();
            for (int s = 0; s < 8; s++)
            {
                var slot = progress.Find("Slot" + s); if (slot == null) continue;
                void Set(string name, bool v) { if (slot.Find(name) is FeNode nn) nn.Visible = v; }
                if (s >= rows.Count) { Set("Invalid", true); foreach (var k in new[] { "Locked", "New", "Bronze", "Silver", "Gold" }) Set(k, false); continue; }
                Set("Invalid", false);
                int sp = FeTables.I(rows[s], "StoreProductId"), row = status.Find("SeriesId", FeTables.I(rows[s], "SeriesId")), best = row < 0 ? -1 : status.Int("BestPlaceFinish", row);
                Set("Locked", p.IsLocked(sp)); Set("New", p.IsNew(sp));
                Set("Bronze", best == 2); Set("Silver", best == 1); Set("Gold", best == 0);
            }
        }
        void OnAccept()
        {
            if (data.Count == 0) return;
            var it = data[posters.list.absIndex];
            if (it.locked) { fe.PlaySound("Locked"); fe.SendUIEvent("ShowLockedPopup"); return; }
            fe.PlaySound("Accept");
            fe.vars["SeriesStoreProductId"] = it.product; ReflexCareer.SeriesStoreProductId = it.product;
            if (it.product == 2904) { fe.SendUIEvent("GotoRaceAndRideChallengesPage"); return; }   // the challenges page (FrontEndChallenges.cs)
            fe.SendUIEvent("GotoRaceAndRideEventsPage");
        }
    }

    // RaceAndRideEvents.lua (Race and Ride Events): the Tour series of the chosen group (SeriesOwner "Tour"), each with its poster
    // (IconId), lock / new marks and medal (PlayerSeriesStatus.BestPlaceFinish); the info box: series title, location name and
    // preview (EventTypeLocations PreviewIconId), allowed vehicle class (VehicleClassType of Series.VehicleClassId), format
    // (STR_MXSERIESFORMAT_* / STR_MXSERIESAIFORMAT_RIVAL). Focus on a new series: SetStoreProductNotNew; End: the group's too.
    // Accept: locked -> the locked popup; the equipped vehicle's type not in the class -> the invalid requirements popup; else
    // progress:Reset / LoadSeries / SetVehicleTypeId (done by the race's world build) and the start popup (Heat / Moto: the practice
    // popup, not ported: the start popup). Back -> CareerModeMenuPage. QuickView (Toggle) is not ported.
    public sealed class RaceAndRideEventsScript : FeScript
    {
        sealed class Item { public int seriesId, product, placing; public bool locked, isNew; }
        List<Item> data = new List<Item>(); FePosterList posters; FeNode focusedItem, info, leftArrow, rightArrow;
        List<int> validTypes = new List<int>(); int playerType = -1, version = -1, parent = -1;
        public override void Initialize()
        {
            posters = new FePosterList(C("Event Select List"));
            focusedItem = C("FocusedItem"); info = C("Event Information Container"); leftArrow = C("Left Arrow"); rightArrow = C("Right Arrow");
            functions["OnAcceptSeries"] = OnAcceptSeries;
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("CareerModeMenuPage"); };
            functions["OnQuickViewKeyPress"] = () => { };
            posters.list.onListChanged = (l, old) => { posters.Scrolled(l.absIndex - old); OnEventListFocusedChanged(); };
        }
        public override void Start()
        {
            fe.SetButtonLegend("back", true, "STR_BACK"); fe.SetButtonLegend("toggle", false, "STR_QUICKVIEW"); fe.SetButtonLegend("misc", false, "STR_OPTIONS");
            parent = fe.Var("SeriesStoreProductId", ReflexCareer.SeriesStoreProductId);
            fe.SetHeader(FeTables.Shared.TitleKey(parent) ?? "STR_RACEANDRIDE");   // DYN_HEADER = the group's title
            Build(true);
        }
        public override void Update(float dt)
        {
            posters.Update(dt);
            if (ReflexProfile.Version != version && !ReflexProfile.Busy) Build(false);
        }
        public override void End() { if (parent >= 0) ReflexProfile.Run("store_not_new " + parent); }
        void Build(bool reset)
        {
            version = ReflexProfile.Version;
            var p = FeCareerScripts.Profile; data.Clear();
            if (p == null) { posters.list.numItems = 0; return; }
            playerType = FeCareerScripts.PlayerVehicleTypeId();
            var t = FeTables.Shared; var status = p.T("PlayerSeriesStatus");
            foreach (var r in t.Rows("Series"))
            {
                int sp = FeTables.I(r, "StoreProductId");
                if (FeTables.S(r, "SeriesOwner") != "Tour" || FeTables.I(t.Rows("StoreProducts").FirstOrDefault(s => FeTables.I(s, "StoreProductId") == sp), "ParentStoreProductId") != parent) continue;
                int id = FeTables.I(r, "SeriesId"), row = status.Find("SeriesId", id);
                data.Add(new Item { seriesId = id, product = sp, locked = p.IsLocked(sp), isNew = p.IsNew(sp), placing = row < 0 ? 1000 : status.Int("BestPlaceFinish", row) });
            }
            posters.list.numItems = data.Count;
            if (reset) posters.list.absIndex = 0;
            posters.list.absIndex = Mathf.Clamp(posters.list.absIndex, 0, Math.Max(0, data.Count - 1));
            OnEventListFocusedChanged();
        }
        void Format(FeNode c, int d)
        {
            var it = data[d];
            FeCareerScripts.SetIcon(fe, c.Find("EventImage"), FeTables.I(FeTables.Shared.Rows("StoreProducts").FirstOrDefault(r => FeTables.I(r, "StoreProductId") == it.product), "IconId"));
            if (c.Find("LockImage") is FeNode l) l.Visible = it.locked;
            if (c.Find("NewImage") is FeNode n) n.Visible = it.isNew;
            if (c.Find("Gold_ImageComponent") is FeNode g) g.Visible = it.placing == 0;
            if (c.Find("Silver_ImageComponent") is FeNode s) s.Visible = it.placing == 1;
            if (c.Find("Bronze_ImageComponent") is FeNode b) b.Visible = it.placing == 2;
        }
        void OnEventListFocusedChanged()
        {
            posters.Format(Format);
            if (data.Count == 0) { focusedItem.Visible = false; return; }
            var it = data[posters.list.absIndex];
            if (it.isNew) { ReflexProfile.Run("store_not_new " + it.product); }
            layer.Stop("FocusedItem_Changed"); layer.Play("FocusedItem_Changed");
            focusedItem.Visible = true; Format(focusedItem, posters.list.absIndex);
            int i = posters.list.absIndex, n = posters.list.numItems; leftArrow.Visible = i > 0; rightArrow.Visible = i < n - 1;
            UpdateEventDisplay(it);
        }
        void UpdateEventDisplay(Item it)
        {
            var t = FeTables.Shared; var series = t.Rows("Series").First(r => FeTables.I(r, "SeriesId") == it.seriesId);
            info.Find("SeriesNameTextComponent")?.SetText(Str(t.TitleKey(it.product) ?? ""));
            int location = FeCareerScripts.Lookup("SeriesEventLocations", it.seriesId, "EventLocationId"), track = Math.Max(0, FeCareerScripts.Lookup("SeriesEventLocations", it.seriesId, "TrackId"));
            int eventType = FeTables.I(series, "EventTypeId");
            int locKey = FeTables.I(t.Rows("EventLocation").FirstOrDefault(r => FeTables.I(r, "EventLocationId") == location), "LocalizationId");
            info.Find("LocationNameTextComponent")?.SetText(locKey != -1 ? Str(t.LocalizationKey(locKey) ?? "") : "");
            var etl = t.Rows("EventTypeLocations").FirstOrDefault(r => FeTables.I(r, "EventLocationId") == location && FeTables.I(r, "TrackId", 0) == track && FeTables.I(r, "EventTypeId") == eventType);
            FeCareerScripts.SetIcon(fe, info.Find("LocationImageComponent"), FeTables.I(etl, "PreviewIconId"));
            int vehicleClass = FeTables.I(series, "VehicleClassId");
            validTypes = t.Rows("VehicleClassType").Where(r => FeTables.I(r, "VehicleClassId") == vehicleClass).Select(r => FeTables.I(r, "VehicleTypeId")).ToList();
            info.Find("AllowedVTTextComponent")?.SetText(Str(FeTables.S(t.Rows("VehicleClass").FirstOrDefault(r => FeTables.I(r, "VehicleClassId") == vehicleClass), "Name") ?? ""));   // App.BuildVehicleClassComponent
            string ai = FeTables.S(series, "SeriesAIFormat"), format = FeTables.S(series, "SeriesFormat") ?? "";
            info.Find("SeriesFormatTextComponent")?.SetText(Str(ai == "Rival" ? "STR_MXSERIESAIFORMAT_RIVAL" : "STR_MXSERIESFORMAT_" + format.ToUpperInvariant()));
        }
        void OnAcceptSeries()
        {
            if (data.Count == 0) return;
            var it = data[posters.list.absIndex];
            if (it.locked) { fe.PlaySound("Locked"); fe.SendUIEvent("ShowLockedPopup"); return; }
            if (!validTypes.Contains(playerType)) { fe.SendUIEvent("ShowInvalidRaceRequirementsPopup"); return; }
            var ev = ReflexCareer.EventOf(it.seriesId, FeCareerScripts.Lookup);
            if (ev == null) { Debug.Log("[Career] series " + it.seriesId + ": its event is not one the race scene runs"); fe.SendUIEvent("ShowLockedPopup"); return; }
            fe.vars["CareerSeries"] = new ReflexCareer.Race { seriesId = it.seriesId, vehicleTypeId = playerType, parentStoreProductId = parent, eventKey = ev.key };
            string format = FeTables.S(FeTables.Shared.Rows("Series").First(r => FeTables.I(r, "SeriesId") == it.seriesId), "SeriesFormat");
            fe.SendUIEvent(format == "Heat" || format == "Moto" ? "ShowPracticePopup" : "ShowStartRacePopup");
        }
    }

    // CareerPlayerInformation.lua: the equipped vehicle's name (StoreAssets -> StoreProducts title) and its type icon.
    public sealed class CareerPlayerInformationScript : FeScript
    {
        public override void Start()
        {
            var pv = FeCareerScripts.T("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", FeCareerScripts.T("PlayerRider").Int("LastEquippedVehicle", 0, 0));
            var text = C("CurrentVehicleText"); var image = C("CurrentVehicleTypeImage");
            if (row < 0) { text?.SetText(""); if (image != null) image.Visible = false; return; }
            var t = FeTables.Shared; var model = t.Rows("VehicleModel").FirstOrDefault(r => FeTables.I(r, "VehicleModelId") == pv.Int("VehicleModelId", row));
            int asset = FeTables.I(model, "StoreAssetId"), type = FeTables.I(model, "VehicleTypeId");
            int product = FeTables.I(t.Rows("StoreAssets").FirstOrDefault(r => FeTables.I(r, "StoreAssetId") == asset), "StoreProductId");   // StoreProductFromStoreAsset
            text?.SetText(Str(t.TitleKey(product) ?? ""));
            string mat = type == 2 || type == 3 || type == 17 ? "IconVehicleMX" : type == 4 || type == 16 ? "IconVehicleATV" : type == 11 ? "IconVehicleUTV" : type == 12 ? "IconVehicleBuggy" : type == 14 ? "IconVehicleSportTruck" : type == 15 ? "IconVehicleSport2Truck" : null;
            if (image != null) { image.Visible = mat != null; if (mat != null) FeCareerScripts.SetMaterial(fe, image, mat); }
        }
    }

    // StartRacePopup.lua (GenericTwoButtonPopup): START EVENT / "Are you sure ..." / YES (focused) / NO. YES: End (tour) ->
    // CommitAndInitializeSeries, StartEvent, GotoLoadingPage: the race scene with the series' career world (ReflexCareer).
    public sealed class StartRacePopupScript : FeScript
    {
        bool accept;
        public override void Initialize()
        {
            functions["OnAcceptButtonOneKeypress"] = () => { fe.PlaySound("Accept"); accept = true; fe.SendUIEvent("PopupPageStack_Pop"); };   // TravelBack: the page stack
            functions["OnAcceptButtonTwoKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("PopupPageStack_Pop"); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("PopupPageStack_Pop"); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow"); accept = false;
            C("TitleText")?.SetText(Str("STR_START_EVENT_TITLE")); C("MessageText")?.SetText(Str("STR_START_EVENT_MESSAGE"));
            C("ButtonOne")?.Find("ButtonText")?.SetText(Str("STR_PURCHASE_POPUP_BUTTONONE"));
            C("ButtonTwo")?.Find("ButtonText")?.SetText(Str("STR_PURCHASE_POPUP_BUTTONTWO"));
            var list = C("ButtonList"); if (list != null) list.focusIndex = 0;
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End()
        {
            if (!accept) return;
            accept = false;
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            { Debug.LogWarning("[Career] Career events are not available in OPEN REFLEX yet. Choose an Arcade event to ride."); return; }
            var race = fe.Var<ReflexCareer.Race>("CareerSeries");
            if (race == null) return;
            race.stamp = ReflexProfile.SaveStamp();
            ReflexCareer.Current = race; ReflexCareer.SeriesStoreProductId = race.parentStoreProductId;
            ReflexProfile.Close();   // the race's world carries the profile from here (built from the same save)
            Debug.Log("[Career] starting " + race);
            fe.LaunchCareerEvent(ReflexEvents.Find(race.eventKey));
        }
    }

    // CareerPracticePopup.lua (Heat / Moto instead of StartRacePopup): PRACTICE / message / PRACTICE (ButtonOne, focused:
    // progress:SetSkipPractice( FALSE )) / SKIP PRACTICE (ButtonTwo: SetSkipPractice( TRUE )); either accepts and End starts the series
    // as StartRacePopup does (CommitAndInitializeSeries, StartEvent, GotoLoadingPage); Back closes. The answer goes to the world build
    // (ReflexCareer.Race.skipPractice -> career::start_series_event).
    public sealed class CareerPracticePopupScript : FeScript
    {
        int answer = -1;
        public override void Initialize()
        {
            functions["OnAcceptButtonOneKeypress"] = () => { fe.PlaySound("PopupShow"); answer = 0; fe.SendUIEvent("PopupPageStack_Pop"); };
            functions["OnAcceptButtonTwoKeypress"] = () => { fe.PlaySound("Accept"); answer = 1; fe.SendUIEvent("PopupPageStack_Pop"); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); answer = -1; fe.SendUIEvent("PopupPageStack_Pop"); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow"); answer = -1;
            C("TitleText")?.SetText(Str("STR_PRACTICE_POPUP_TITLE")); C("MessageText")?.SetText(Str("STR_PRACTICE_POPUP_MESSAGE"));
            C("ButtonOne")?.Find("ButtonText")?.SetText(Str("STR_PRACTICE_POPUP_BUTTONONE"));
            C("ButtonTwo")?.Find("ButtonText")?.SetText(Str("STR_PRACTICE_POPUP_BUTTONTWO"));
            var list = C("ButtonList"); if (list != null) list.focusIndex = 0;
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End()
        {
            if (answer < 0) return;
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            { answer = -1; Debug.LogWarning("[Career] Career events are not available in OPEN REFLEX yet. Choose an Arcade event to ride."); return; }
            var race = fe.Var<ReflexCareer.Race>("CareerSeries");
            int skip = answer; answer = -1;
            if (race == null) return;
            race.skipPractice = skip; race.outcomes.Clear(); race.gates.Clear();   // gates: the Gate Selection page (WP-5)
            race.stamp = ReflexProfile.SaveStamp();
            ReflexCareer.Current = race; ReflexCareer.SeriesStoreProductId = race.parentStoreProductId;
            ReflexProfile.Close();
            Debug.Log("[Career] starting " + race);
            fe.LaunchCareerEvent(ReflexEvents.Find(race.eventKey));
        }
    }

    // The PC UI text entry the jersey popups use (App.InitializeUIDisplayText: an empty buffer; UseAlpha / UseNumeric /
    // UseAlphaNumeric: the characters it takes; SetInputSize: its length; every change is a "UIKeyboardInput" event with the text).
    public sealed class FeTextInput
    {
        public string Text { get; private set; } = "";
        readonly int size; readonly bool letters, digits; readonly Action<string> changed;
        public FeTextInput(int size, bool letters, bool digits, Action<string> changed)
        {
            this.size = size; this.letters = letters; this.digits = digits; this.changed = changed;
            if (UnityEngine.InputSystem.Keyboard.current != null) UnityEngine.InputSystem.Keyboard.current.onTextInput += OnChar;
        }
        public void End() { if (UnityEngine.InputSystem.Keyboard.current != null) UnityEngine.InputSystem.Keyboard.current.onTextInput -= OnChar; }
        public void Type(string s) { foreach (char c in s) OnChar(c); }   // tests
        void OnChar(char c)
        {
            bool ok = letters && (char.IsLetter(c) || c == ' ') || digits && char.IsDigit(c);
            if (!ok || Text.Length >= size) return;
            Text += c; changed?.Invoke(Text);
        }
        public void Update()
        {
            var kb = UnityEngine.InputSystem.Keyboard.current;
            if (kb != null && kb.backspaceKey.wasPressedThisFrame && Text.Length > 0) { Text = Text.Substring(0, Text.Length - 1); changed?.Invoke(Text); }
        }
    }

    // JerseyNameEntryPopup.lua / JerseyNumberEntryPopup.lua (the motocard's CHANGE RIDER NAME / NUMBER): the current PlayerRider Name /
    // Number, typed text replaces it (name: alphanumeric, 8, upper case; number: numeric, 3). Accept: the name through
    // App.ValidateJerseyName + App.IsStringSafe ("" or unsafe -> "RIDER" and the invalid name popup, MotocardNameError "jersey"),
    // NameStatus 0; the number "" -> "998"; then App.SaveUISettings and App.RequestSaveGame; HideJersey{Name,Number}EntryPopup.
    public sealed class JerseyEntryPopupScript : FeScript
    {
        readonly bool number; string value; FeTextInput input; FeNode message;
        System.Threading.Tasks.Task<string> applying;
        public string Value => value;
        public FeTextInput Input => input;
        public JerseyEntryPopupScript(bool number) { this.number = number; }
        string Hide => number ? "HideJerseyNumberEntryPopup" : "HideJerseyNameEntryPopup";
        public override void Initialize()
        {
            functions["OnAcceptKeypress"] = Accept;
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent(Hide); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            C("TitleText")?.SetText(Str(number ? "STR_MOTOCARD_TITLE_NUMBER" : "STR_MOTOCARD_TITLE_NAME"));
            C("ButtonText")?.SetText(Str("STR_CONTINUE"));
            message = C("MessageText");
            value = FeCareerScripts.T("PlayerRider").Str(number ? "Number" : "Name", 0) ?? "";
            message?.SetText(value);
            input = number ? new FeTextInput(3, false, true, t => { message?.SetText(t); value = t; })
                           : new FeTextInput(8, true, true, t => { message?.SetText(t.ToUpperInvariant()); value = t; });
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End() { input?.End(); input = null; fe.vars.Remove("MotocardNameError"); }
        public override void Update(float dt)
        {
            input?.Update();
            if (applying != null && applying.IsCompleted)
            {
                string r = applying.Result; applying = null;
                if (r == "invalid") { fe.vars["MotocardNameError"] = "jersey"; fe.SendUIEvent("ShowInvalidMotocardNamePopup"); }
            }
        }
        void Accept()
        {
            fe.PlaySound("Accept");
            string v = value ?? "";
            applying = System.Threading.Tasks.Task.Run(() =>
            {
                if (!ReflexProfile.Open().Result) return "no profile";
                string result = "ok", entry;
                if (number) entry = v == "" ? "998" : v;
                else
                {
                    string r = ReflexProfile.Command("validate_jersey_name " + v.ToUpperInvariant()) ?? "0 ";
                    int sp = r.IndexOf(' '); bool safe = r.StartsWith("1"); entry = sp >= 0 ? r.Substring(sp + 1) : "";
                    if (entry == "" || !safe) { entry = "RIDER"; result = "invalid"; }
                }
                ReflexProfile.Run("rider_string " + (number ? "Number " : "Name ") + entry, number ? "json" : "set_int PlayerRider 0 NameStatus 0", "save_ui_settings", "save").Wait();
                return result;
            });
            fe.SendUIEvent(Hide);
        }
    }

    // InvalidMotocardNamePopup.lua: INVALID NAME, the message for MotocardNameError "jersey" (STR_MOTOCARD_INVALIDJERSEY_MESSAGE) or a
    // motocard name (STR_MOTOCARD_INVALIDNAME_MESSAGE), CONTINUE; accept / back: HideInvalidMotocardNamePopup.
    public sealed class InvalidMotocardNamePopupScript : FeScript
    {
        public override void Initialize()
        {
            functions["OnAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("HideInvalidMotocardNamePopup"); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("HideInvalidMotocardNamePopup"); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            C("TitleText")?.SetText(Str("STR_MOTOCARD_INVALIDNAME_TITLE")); C("ButtonText")?.SetText(Str("STR_CONTINUE"));
            C("MessageText")?.SetText(Str(fe.Var<string>("MotocardNameError") == "jersey" ? "STR_MOTOCARD_INVALIDJERSEY_MESSAGE" : "STR_MOTOCARD_INVALIDNAME_MESSAGE"));
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End() => fe.vars.Remove("MotocardNameError");
    }

    // MotocardNameEntry.lua (the Motocard Setup Name Page) as the port's Steam persona entry: the PC build signs in with the Steam
    // user's persona name (the motocard's name); without Steam the player names it here once (<save>/steam_persona.txt, read by the
    // profile's boot: career::persona). The console's system keyboard is the typed text (alpha, 16 characters, App.IsProfileNameValid
    // -> else the invalid name popup); accept with a valid name -> the persona, the profile session again with it, "Advance";
    // back -> GotoTitlePage.
    public sealed class MotocardNameEntryScript : FeScript
    {
        FeTextInput input; string name = ""; FeNode message; string messageText;
        System.Threading.Tasks.Task<bool> checking;
        public FeTextInput Input => input;
        public override void Initialize()
        {
            functions["OnAcceptKeypress"] = Accept;
            functions["OnBackKeypress"] = () => { fe.PlaySound("Cancel"); fe.SendUIEvent("GotoTitlePage"); };
            functions["OnToggleKeypress"] = () => { };
        }
        public override void Start()
        {
            name = ""; message = C("Message"); messageText = Str("STR_MOTOCARD_NAMEENTRY_MESSAGE");
            fe.SetButtonLegend("accept", true, "STR_MOTOCARD_NAV_ENTERNAME"); fe.SetButtonLegend("back", true, "STR_MOTOCARD_NAV_CANCEL");
            input = new FeTextInput(16, true, true, t => { name = t; message?.SetText(t.Length > 0 ? t : messageText); });
        }
        public override void End() { input?.End(); input = null; }
        public override void Update(float dt)
        {
            input?.Update();
            if (checking != null && checking.IsCompleted)
            {
                bool ok = checking.Result; checking = null;
                if (!ok) { fe.SendUIEvent("ShowInvalidMotocardNamePopup"); return; }
                ReflexProfile.Persona = name;   // the Steam persona stand-in
                ReflexProfile.Close(); ReflexProfile.Open();   // the profile signs in again as that user
                Debug.Log("[Career] motocard name (persona): " + name);
                fe.SendUIEvent("Advance");
            }
        }
        void Accept()
        {
            fe.PlaySound("Accept");
            if (checking != null || name.Trim() == "") return;   // RequestName: the keyboard (here: type the name)
            string n = name.Trim();
            checking = System.Threading.Tasks.Task.Run(() => ReflexProfile.Open().Result && (ReflexProfile.Command("profile_name_valid " + n) ?? "0").StartsWith("1"));
        }
    }

    // RewardVehiclePresentation.lua (the Vehicle Reward Page, from CareerFrontEndLoadingPage:DoneLoading's GotoVehicleReward): the
    // garage's free-look view of the rewarded vehicle (MXGarage:RequestVehicle; FeGarage, the bike from the profile's PlayerVehicle of
    // that model and its parts, else nothing) and its title (StoreProductFromStoreAsset -> TitleLocalizationString). Accept (OnAccept):
    // CompleteSeries / EnableTourMode (already run in the race's world: ReflexCareer.Finish) and GotoCareer -> the series' events.
    public sealed class RewardVehiclePresentationScript : FeScript
    {
        FeGarage garage; FeNode target; int asset = -1, model = -1; bool shown;
        public override void Initialize() { functions["OnAccept"] = () => { fe.PlaySound("Accept"); fe.JumpTo("Race and Ride Events"); }; }
        public override void Start()
        {
            asset = ReflexCareer.RewardVehicleAsset; model = ReflexCareer.RewardVehicleModel; shown = false;
            ReflexCareer.RewardVehicleAsset = ReflexCareer.RewardVehicleModel = -1;   // RemoveUIVariable RewardVehicleStoreAssetId / SeriesRewardVehicleModelId
            var t = FeTables.Shared;
            int product = FeTables.I(t.Rows("StoreAssets").FirstOrDefault(r => FeTables.I(r, "StoreAssetId") == asset), "StoreProductId");
            C("VehicleTitle")?.SetText(Str(t.TitleKey(product) ?? ""));
            target = C("RenderTarget");
            try { garage = new FeGarage(fe.transform); garage.SetCamera(new Vector3(-10.33f, 3, 9.82f), new Vector3(0.96f, 1.2f, -0.22f), Vector3.zero); }
            catch (Exception e) { Debug.LogWarning("[Career] reward vehicle garage: " + e.Message); }
            if (garage != null && target?.image) { target.image.texture = garage.Texture; target.image.uvRect = new Rect(0, 0, 1, 1); target.image.Dirty(); target.Visible = true; }
            Debug.Log("[Career] reward vehicle: store asset " + asset + ", model " + model);
        }
        public override void Update(float dt)
        {
            garage?.Update(dt);
            if (shown || garage == null || ReflexProfile.Current == null || ReflexProfile.Busy) return;
            shown = true;   // the profile now owns it (CompleteSeries purchased the reward): its PlayerVehicle and parts
            try
            {
                var p = ReflexProfile.Current; var pv = p.T("PlayerVehicle"); int row = -1;
                for (int i = 0; i < pv.rows; i++) if (pv.Int("VehicleModelId", i) == model) row = i;
                if (row < 0) return;
                int id = pv.Int("PlayerVehicleId", row); var parts = p.T("PlayerVehicleParts");
                var ids = Enumerable.Range(0, parts.rows).Where(i => parts.Int("PlayerVehicleId", i) == id).Select(i => parts.Int("VehiclePartId", i)).ToList();
                garage.ShowBike(ReflexVehicleSpecs.Bike(new ReflexGameTables(new FeTableSource(FeTables.Shared)), model, ids));
            }
            catch (Exception e) { Debug.LogWarning("[Career] reward vehicle model: " + e.Message); }
        }
        public override void End() { garage?.Destroy(); garage = null; }
    }

    // CareerCompletion.lua (the Career Completion Page, from RaceAndRide:Present when CalculateCareerCompletion reaches 1). Initialize /
    // Start / End: both videos stopped and hidden. Present: Videos\CareerCompletion.bik played (FrontEndVideo.cs), UI music muted; its
    // DoneEvent CompletionVideoDone -> OnCompletionVideoDone: it is stopped and hidden, the music back, Videos\Credits.bik played;
    // CreditsVideoDone -> OnCreditsVideoDone: CareerModeMenuPage and App.RequestSaveGame.
    public sealed class CareerCompletionScript : FeScript
    {
        FeVideo completion, credits;
        bool muted;
        public override void Initialize()
        {
            completion = FeVideo.Of(C("CareerCompletionVideo")); credits = FeVideo.Of(C("CreditsVideo"));
            functions["OnCompletionVideoDone"] = OnCompletionVideoDone;
            functions["OnCreditsVideoDone"] = OnCreditsVideoDone;
            Hide();
        }
        void Hide()
        {
            foreach (var v in new[] { completion, credits }) if (v != null) { v.Stop(fe); v.node.Visible = false; }
        }
        void Mute(bool m) { if (m != muted) { muted = m; fe.MuteUiMusic(m); } }
        public override void Start() => Hide();
        public override void End() { Hide(); Mute(false); }
        public override void Present()
        {
            if (completion == null) { OnCompletionVideoDone(); return; }
            completion.Play(fe); completion.node.Visible = true;
            Mute(true);   // App.MuteUIMusic( TRUE )
        }
        void OnCompletionVideoDone()
        {
            if (completion != null) { completion.Stop(fe); completion.node.Visible = false; }
            Mute(false);
            if (credits == null) { OnCreditsVideoDone(); return; }
            credits.Play(fe); credits.node.Visible = true;
        }
        void OnCreditsVideoDone()
        {
            fe.SendUIEvent("CareerModeMenuPage");
            ReflexProfile.Run("save");   // App.RequestSaveGame( App.GetExclusiveDeviceIndex( ) )
        }
    }
}
