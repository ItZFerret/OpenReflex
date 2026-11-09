// The open garage's data (OpenShell.Garage.cs shows it): what a rider can wear, what a vehicle can carry, what the player owns and what
// is for sale, read from the game's tables (FeTables.Shared: VehicleModel, VehicleModelParts, VehiclePart, RiderGear, StoreProducts ...)
// and the profile (ReflexProfile.Current: PlayerVehicle, PlayerVehicleParts, PlayerRiderGear, PlayerVehicleTuning, PlayerRider ...).
// Changes go through ReflexProfile / CoreGarageProfile (equip, purchase, tune), the same calls the original garage's scripts make.
// An item is one row of a list: a piece of gear, a part of a vehicle or a vehicle model, with its state (equipped, owned, for sale,
// locked, new, DLC) and its price. Nothing here draws.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    /// One thing in a garage list.
    internal sealed class GarageItem
    {
        public int id;                 // RiderGearId / VehiclePartId / VehicleModelId
        public int type;               // its gear type / part type / vehicle type
        public int asset = -1, product = -1, parent = -1;
        public int price, brandIcon = -1, playerVehicle = -1;
        public string title = "", line = "";   // line: the product line (the parent product's name)
        public bool equipped, owned, locked, isNew, dlc, hidden;
        /// A bike mod's card (w-bikemods / w-stage): the base bike's item with the mod it shows ("" and null: the original bike's own card).
        public string modId = ""; public BikeMod mod; public BikeName modName;
        /// A gear / kit pack's piece (w-gearmods, KitPacks): its id is the renumbered one; null: a stock item.
        public KitItem kit;
        public GarageItem Clone() => (GarageItem)MemberwiseClone();
        /// Owned or free: can be put on now.
        public bool Available => owned;
        /// For sale: not owned, not locked, priced.
        public bool ForSale => !owned && !locked;
        public string State => equipped ? "EQUIPPED" : owned ? "OWNED" : locked ? "LOCKED" : price > 0 ? price.ToString("N0") : "FREE";
    }

    /// One entry of a gear / part section's list (the original's primary store list, GarageRiderCustomize / GarageVehicleCustomize:
    /// BuildStoreList): a product line (the parent store product, "Fox V3": a drill-in entry whose items are its colours / styles, the
    /// secondary list) or a product without a line, which is an item of its own ("No Neckbrace", "Red Bull").
    internal sealed class GarageGroup
    {
        public int line = -1;                 // the parent StoreProductId (-1: a single item)
        public string title = "";
        public GarageItem item;               // the single item (line == -1)
        public List<GarageItem> items = new List<GarageItem>();
        public bool IsLine => line >= 0;
        public bool Equipped => items.Any(i => i.equipped);
        public bool AnyNew => items.Any(i => i.isNew && !i.locked);
        public bool AnyDlc => items.Any(i => i.dlc);
        public int Owned => items.Count(i => i.Available);
    }

    /// One attribute of the vehicle's performance: the model's base and the player's tuning bonus on each of its two bars.
    internal struct GarageStat
    {
        public string name, description; public int base1, base2, bonus1, bonus2; public bool two;
    }

    internal static class OpenGarageData
    {
        static FeTables T => FeTables.Shared;
        public static ProfileData P => ReflexProfile.Current;
        static Dictionary<string, object> Row(string table, string col, int v) => FeGarageData.Unique(table, col, v);
        static int I(Dictionary<string, object> r, string c, int d = -1) => FeTables.I(r, c, d);

        /// A store product's name (STR_<id> through the install's strings).
        public static string Title(int product, string fallback = "")
        {
            if (KitPacks.IsKitId(product))
            {   // a pack's line or piece (KitPacks: the pack's own names)
                if (KitPacks.Line(product, out var line)) return line.title;
                var it = KitPacks.ProductItem(product); return it != null && it.title.Length > 0 ? it.title : fallback;
            }
            string key = T.TitleKey(product);
            string s = key == null ? null : OpenContent.Str(key, null);
            return string.IsNullOrEmpty(s) ? fallback : s;
        }

        public static string Balance => FeGarageData.Number(FeGarageData.Balance);
        public static bool Unlocked => CoreGarageProfile.UnlockEverythingEnabled;

        /// A product's brand icon id (StoreProducts.BrandId -> Brand.BrandIconId), -1 none.
        public static int BrandIcon(int product)
        {
            if (KitPacks.IsKitId(product))
            {   // a pack's piece / line: the stock brand it names (its Brand row's icon), else its own icon id
                int kb = KitPacks.Line(product, out var line) ? line.brand : KitPacks.ProductItem(product)?.brandId ?? -1;
                int icon = kb >= 0 ? I(Row("Brand", "BrandId", kb), "BrandIconId") : -1;
                return icon >= 0 ? icon : KitPacks.Line(product, out line) ? line.icon : -1;
            }
            int b = FeGarageData.ProductInt(product, "BrandId");
            return b == -1 ? -1 : I(Row("Brand", "BrandId", b), "BrandIconId");
        }

        /// The brand's own name, when the Brand table names it.
        public static string BrandName(int product)
        {
            int b = KitPacks.IsKitId(product) ? (KitPacks.Line(product, out var line) ? line.brand : KitPacks.ProductItem(product)?.brandId ?? -1) : FeGarageData.ProductInt(product, "BrandId");
            if (b == -1) return null;
            var row = Row("Brand", "BrandId", b);
            if (row == null) return null;
            foreach (var col in new[] { "Name", "BrandName", "DisplayName" })
            {
                string s = FeTables.S(row, col);
                if (!string.IsNullOrEmpty(s)) { string v = OpenContent.Str(s, null); return string.IsNullOrEmpty(v) || v.StartsWith("STR_") ? null : CleanBrand(v); }
            }
            int loc = I(row, "LocalizationId");
            if (loc != -1) { string v = OpenContent.Str(T.LocalizationKey(loc), null); if (!string.IsNullOrEmpty(v) && !v.StartsWith("STR_")) return v; }
            return null;
        }

        /// The table's brand name as a player reads it: the internal marks dropped (".NoBrandAvailable" is none; "Internal Blaze" is Blaze).
        static string CleanBrand(string v)
        {
            if (v.StartsWith(".", StringComparison.Ordinal)) return null;
            if (v.StartsWith("Internal ", StringComparison.OrdinalIgnoreCase)) v = v.Substring(9);
            return v.Length == 0 ? null : v;
        }

        /// careerLock: a vehicle model, the only garage item the career unlocks (GarageMyRides asks IsLocked; the gear and parts pages only
        /// skip hidden products and sell the rest).
        static GarageItem Make(int id, int type, int asset, int fallbackTitleKey = 0, bool careerLock = false)
        {
            var p = P;
            int product = FeGarageData.ProductOfAsset(asset);
            int parent = FeGarageData.ProductInt(product, "ParentStoreProductId");
            var it = new GarageItem
            {
                id = id, type = type, asset = asset, product = product, parent = parent, price = Math.Max(0, FeGarageData.Price(product)),
                title = Title(product, "ITEM " + id), line = parent >= 0 ? Title(parent, "") : "",
                owned = asset >= 0 && FeGarageData.Purchased(asset), brandIcon = BrandIcon(product),
                locked = careerLock && (p == null || p.IsLocked(product)), isNew = p != null && p.IsNew(product), dlc = p != null && p.IsDlc(product),
                hidden = p != null && p.IsHidden(product),
            };
            // a free item can be worn (the original's "cost 0 -> isPurchased")
            if (!it.owned && it.price == 0 && !it.locked) it.owned = true;
            return it;
        }

        /// A gear / kit pack's piece as a garage item: owned at once (a found DLC unlocks its products in the original: Profile_ApplyDLCUnlocks),
        /// tagged DLC as the original tags a DLC product, in its line (a stock line it joins, or the packs' merged line).
        static GarageItem Make(KitItem k)
        {
            var it = new GarageItem
            {
                id = k.id, type = k.type, asset = k.asset, product = k.product, parent = k.parent, price = k.price, title = k.title,
                line = k.parent >= 0 ? (KitPacks.IsKitId(k.parent) ? k.lineTitle : Title(k.parent, k.lineTitle)) : "",
                owned = true, brandIcon = BrandIcon(k.product), dlc = true, hidden = k.hidden, kit = k,
            };
            return it;
        }
        /// A pack's gear type / part type of an id (the stock tables' for a stock id).
        static int GearTypeOf(int id) => KitPacks.Item(id) is KitItem k ? k.type : I(Row("RiderGear", "RiderGearId", id), "RiderGearTypeId");
        static int PartTypeOf(int id) => KitPacks.Item(id) is KitItem k ? k.type : I(Row("VehiclePart", "VehiclePartId", id), "VehiclePartTypeId");
        static int ModelOfVehicle(int playerVehicle) { var pv = Tab("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", playerVehicle); return row < 0 ? -1 : pv.Int("VehicleModelId", row); }

        // ------------------------------------------------------------------------------------------------ the profile's choices

        public static ProfileData.Table Tab(string name) => P?.T(name) ?? new ProfileData.Table();
        public static int Equipped => Tab("PlayerRider").Int("LastEquippedVehicle", 0, 0);
        public static bool Pro => Tab("PlayerRider").Int("LastRiderId", 0, 0) > 0;
        public static int ProId => Tab("PlayerRider").Int("LastRiderId", 0, 0);
        /// The equipped PlayerVehicle's model (-1: a pro rider, or none).
        public static int EquippedModel
        {
            get
            {
                if (Pro) return I(Row("AIVehicle", "AIVehicleId", Equipped), "VehicleModelId");
                var pv = Tab("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", Equipped);
                return row < 0 ? -1 : pv.Int("VehicleModelId", row);
            }
        }
        public static int PlayerVehicleOfModel(int model)
        {
            var pv = Tab("PlayerVehicle"); int row = pv.Find("VehicleModelId", model);
            return row < 0 ? -1 : pv.Int("PlayerVehicleId", row);
        }

        // ------------------------------------------------------------------------------------------------ gear

        static readonly int[] GearSort = { 3, 1, 4, 2, 11, 9 };
        /// The gear types of the rider's profile rows (helmet, goggles, jersey ... as GarageRiderCustomize sorts them), SkinTone last.
        public static List<(int id, string factory, string name)> GearTypes()
        {
            var list = new List<(int id, string factory, string name)>();
            var t = Tab("PlayerRiderGear");
            var seen = new HashSet<int>();
            for (int i = 0; i < t.rows; i++)
            {
                int type = I(Row("RiderGear", "RiderGearId", t.Int("RiderGearId", i)), "RiderGearTypeId");
                if (type < 0 || !seen.Add(type)) continue;
                string factory = FeTables.S(Row("RiderGearType", "RiderGearTypeId", type), "FactoryName") ?? "GEAR";
                list.Add((type, factory, OpenContent.Str("STR_RIDERGEARTYPE_" + factory.ToUpperInvariant(), factory).ToUpperInvariant()));
            }
            int Rank(int id) { int k = Array.IndexOf(GearSort, id); return k < 0 ? 1000 : k; }
            return list.Select((x, i) => (x, i)).OrderBy(q => q.x.factory == "SkinTone" ? 1 : 0).ThenBy(q => Rank(q.x.id)).ThenBy(q => q.i).Select(q => q.x).ToList();
        }

        public static int EquippedGear(int type)
        {
            var kit = KitPacks.ChosenGear(type); if (kit != null) return kit.id;   // a pack's piece the player chose (the profile keeps its stock one)
            return StockGear(type);
        }
        static int StockGear(int type)
        {
            var t = Tab("PlayerRiderGear");
            for (int i = 0; i < t.rows; i++)
            {
                int g = t.Int("RiderGearId", i);
                if (I(Row("RiderGear", "RiderGearId", g), "RiderGearTypeId") == type) return g;
            }
            return -1;
        }

        public static List<int> EquippedGearIds()
        {
            var t = Tab("PlayerRiderGear");
            var stock = Enumerable.Range(0, t.rows).Select(i => t.Int("RiderGearId", i)).ToList();
            var kits = KitPacks.ChosenGear();
            return kits.Count == 0 ? stock : KitPacks.WithGear(stock, kits, GearTypeOf).ToList();
        }

        /// Every piece of a gear type that is not hidden, "No ..." first, then by product line and name.
        public static List<GarageItem> GearItems(int type)
        {
            int worn = EquippedGear(type);
            var list = new List<GarageItem>();
            foreach (var r in T.Rows("RiderGear"))
            {
                if (I(r, "RiderGearTypeId") != type) continue;
                var it = Make(I(r, "RiderGearId"), type, I(r, "StoreAssetId"));
                if (it.hidden || it.product < 0) continue;
                it.equipped = it.id == worn;
                list.Add(it);
            }
            foreach (var k in KitPacks.Current.byId.Values)   // the gear / kit packs' pieces of this type (w-gearmods)
            {
                if (k.bike || k.type != type || k.hidden) continue;
                var it = Make(k); it.equipped = it.id == worn; list.Add(it);
            }
            // RiderGearListSort: "No Chest Protector" (product 378) / "No Neckbrace" (1722) first, then by title
            int Hack(GarageItem x) => x.product == 378 || x.product == 1722 ? 0 : 1;
            return list.Select((x, i) => (x, i)).OrderBy(q => Hack(q.x)).ThenBy(q => q.x.line, StringComparer.Ordinal).ThenBy(q => q.x.title, StringComparer.Ordinal).ThenBy(q => q.i).Select(q => q.x).ToList();
        }

        /// The pieces that can be worn now (owned or free), the worn one among them (a worn piece the catalog omits stays as CURRENT).
        public static List<GarageItem> WearableGear(int type)
        {
            var items = GearItems(type).Where(i => i.Available || i.equipped).ToList();
            int worn = EquippedGear(type);
            if (worn >= 0 && !items.Any(i => i.id == worn)) items.Insert(0, new GarageItem { id = worn, type = type, title = "CURRENT EQUIPMENT", owned = true, equipped = true });
            return items;
        }

        /// A section's items as the original's primary store list groups them (BuildStoreList): one entry per product line (its items in the
        /// order given), one per product without a line; the entries by title, "No Chest Protector" (378) / "No Neckbrace" (1722) first
        /// (RiderGearListSort; VehiclePartListSort is by title alone).
        public static List<GarageGroup> Groups(List<GarageItem> items)
        {
            var list = new List<GarageGroup>();
            var lines = new Dictionary<int, GarageGroup>();
            foreach (var it in items)
            {
                if (it.parent < 0) { list.Add(new GarageGroup { title = it.title, item = it, items = { it } }); continue; }
                if (!lines.TryGetValue(it.parent, out var g))
                {
                    g = lines[it.parent] = new GarageGroup { line = it.parent, title = string.IsNullOrEmpty(it.line) ? Title(it.parent, "LINE " + it.parent) : it.line };
                    list.Add(g);
                }
                g.items.Add(it);
            }
            int Hack(GarageGroup g) => g.item != null && (g.item.product == 378 || g.item.product == 1722) ? 0 : 1;
            return list.Select((g, i) => (g, i)).OrderBy(q => Hack(q.g)).ThenBy(q => q.g.title, StringComparer.OrdinalIgnoreCase).ThenBy(q => q.i).Select(q => q.g).ToList();
        }

        public static bool EquipGear(GarageItem item)
        {
            if (item == null || !item.Available) return false;
            if (item.kit != null) { KitPacks.SetGear(item.type, item.kit); return true; }   // a pack's piece: the client save (the profile keeps its stock piece)
            bool ok = StockGear(item.type) == item.id || ReflexProfile.EquipGear(item.id, null).Result;
            if (ok) KitPacks.ClearGear(item.type);
            return ok;
        }

        // ------------------------------------------------------------------------------------------------ vehicle parts

        static readonly int[] PartSort = { 11, 23, 22, 7, 12, 9, 16, 10, 1, 15, 8, 3 };
        /// The customizable part types of the equipped vehicle (GarageVehicleCustomize: AllowCustomization, sorted for the motocross types).
        public static List<(int id, string factory, string tuning, string name)> PartTypes(int playerVehicle)
        {
            var list = new List<(int id, string factory, string tuning, string name)>();
            var parts = Tab("PlayerVehicleParts"); var seen = new HashSet<int>();
            for (int i = 0; i < parts.rows; i++)
            {
                if (parts.Int("PlayerVehicleId", i) != playerVehicle) continue;
                int type = I(Row("VehiclePart", "VehiclePartId", parts.Int("VehiclePartId", i)), "VehiclePartTypeId");
                var tr = Row("VehiclePartType", "VehiclePartTypeId", type);
                if (tr == null || I(tr, "AllowCustomization", 0) == 0 || !seen.Add(type)) continue;
                string factory = FeTables.S(tr, "FactoryName") ?? "PART";
                list.Add((type, factory, FeTables.S(tr, "TuningAttribute"), OpenContent.Str("STR_VEHICLEPARTTYPE_" + factory.ToUpperInvariant(), factory).ToUpperInvariant()));
            }
            int Rank(int id) { int k = Array.IndexOf(PartSort, id); return k < 0 ? 1000 + id : k; }
            return list.Select((x, i) => (x, i)).OrderBy(q => Rank(q.x.id)).ThenBy(q => q.i).Select(q => q.x).ToList();
        }

        public static int EquippedPart(int playerVehicle, int type)
        {
            var kit = KitPacks.ChosenPart(ModelOfVehicle(playerVehicle), type); if (kit != null) return kit.id;   // a pack's kit / part the player chose
            return StockPart(playerVehicle, type);
        }
        static int StockPart(int playerVehicle, int type)
        {
            var parts = Tab("PlayerVehicleParts");
            for (int i = 0; i < parts.rows; i++)
            {
                if (parts.Int("PlayerVehicleId", i) != playerVehicle) continue;
                int id = parts.Int("VehiclePartId", i);
                if (I(Row("VehiclePart", "VehiclePartId", id), "VehiclePartTypeId") == type) return id;
            }
            return -1;
        }

        public static List<int> EquippedPartIds(int playerVehicle)
        {
            var parts = Tab("PlayerVehicleParts");
            var stock = Enumerable.Range(0, parts.rows).Where(i => parts.Int("PlayerVehicleId", i) == playerVehicle).Select(i => parts.Int("VehiclePartId", i)).ToList();
            var kits = KitPacks.ChosenParts(ModelOfVehicle(playerVehicle));
            foreach (var k in kits) stock = KitPacks.WithPart(stock, k, PartTypeOf).ToList();
            return stock;
        }

        public static List<GarageItem> PartItems(int model, int playerVehicle, int type)
        {
            int worn = EquippedPart(playerVehicle, type);
            var list = new List<GarageItem>();
            foreach (var r in T.Rows("VehicleModelParts"))
            {
                if (I(r, "VehicleModelId") != model) continue;
                int part = I(r, "VehiclePartId");
                if (I(Row("VehiclePart", "VehiclePartId", part), "VehiclePartTypeId") != type) continue;
                var it = Make(part, type, I(r, "StoreAssetId"));
                if (it.hidden || it.product < 0) continue;
                it.equipped = part == worn;
                list.Add(it);
            }
            foreach (var k in KitPacks.Current.byId.Values)   // the kit packs' kits / parts made for this model (w-gearmods)
            {
                if (!k.bike || k.type != type || k.hidden || !k.models.Contains(model)) continue;
                var it = Make(k); it.equipped = it.id == worn; list.Add(it);
            }
            return list.Select((x, i) => (x, i)).OrderBy(q => q.x.line, StringComparer.Ordinal).ThenBy(q => q.x.title, StringComparer.Ordinal).ThenBy(q => q.i).Select(q => q.x).ToList();
        }

        public static List<GarageItem> WearableParts(int model, int playerVehicle, int type)
        {
            var items = PartItems(model, playerVehicle, type).Where(i => i.Available || i.equipped).ToList();
            int worn = EquippedPart(playerVehicle, type);
            if (worn >= 0 && !items.Any(i => i.id == worn)) items.Insert(0, new GarageItem { id = worn, type = type, title = "CURRENT EQUIPMENT", owned = true, equipped = true });
            return items;
        }

        public static bool EquipPart(int playerVehicle, GarageItem item, string tuning)
        {
            if (item == null || !item.Available) return false;
            int model = ModelOfVehicle(playerVehicle);
            if (item.kit != null) { if (model < 0) return false; KitPacks.SetPart(model, item.type, item.kit); return true; }   // a pack's kit: the client save
            var part = Row("VehiclePart", "VehiclePartId", item.id);
            bool ok = StockPart(playerVehicle, item.type) == item.id || ReflexProfile.EquipPart(playerVehicle, item.id, tuning, I(part, "DefaultTuneAttribute1", 0), I(part, "DefaultTuneAttribute2", 0), null).Result;
            if (ok && model >= 0) KitPacks.ClearPart(model, item.type);
            return ok;
        }

        // ------------------------------------------------------------------------------------------------ vehicles

        static readonly int[] TypeSort = { 2, 3, 4, 11, 14, 12, 15 };
        /// One list row of the vehicle page: a model with the player's vehicle of it (-1 when not owned).
        public static List<GarageItem> Vehicles()
        {
            var list = new List<GarageItem>();
            int equipped = Pro ? -1 : Equipped;
            foreach (var r in T.Rows("VehicleModel"))
            {
                int model = I(r, "VehicleModelId"), asset = I(r, "StoreAssetId"); if (asset < 0) continue;
                var it = Make(model, I(r, "VehicleTypeId"), asset, careerLock: true);
                if (it.hidden || it.product < 0) continue;
                it.playerVehicle = PlayerVehicleOfModel(model);
                it.owned = it.playerVehicle >= 0;   // owning a model is having its PlayerVehicle (a purchase makes it)
                it.equipped = it.playerVehicle >= 0 && it.playerVehicle == equipped;
                list.Add(it);
            }
            int Rank(int id) { int k = Array.IndexOf(TypeSort, id); return k < 0 ? 1000 + id : k; }
            // a class at a time (the original's order); owned first, then unlocked, then by name
            return list.Select((x, i) => (x, i)).OrderBy(q => Rank(q.x.type)).ThenBy(q => q.x.owned ? 0 : 1).ThenBy(q => q.x.locked ? 1 : 0).ThenBy(q => q.x.title, StringComparer.Ordinal).ThenBy(q => q.i).Select(q => q.x).ToList();
        }

        /// The vehicle the player rides in a vehicle class (or several): the equipped one when its type is one of the class's (VehicleClassType;
        /// what RaceAndRideEvents:OnAcceptSeries tests), else the favourite - failing that the first - owned vehicle of the class's types.
        /// equipped = the one the garage has equipped. No profile, a pro rider on the stage with none of the class owned, or nothing of the class owned: null.
        public static (string title, int type, bool equipped)? VehicleOfClasses(IEnumerable<int> classes)
        {
            var p = P; if (p == null || classes == null) return null;
            var types = new HashSet<int>();
            foreach (int c in classes) foreach (var r in T.Rows("VehicleClassType")) if (I(r, "VehicleClassId") == c) types.Add(I(r, "VehicleTypeId"));
            if (types.Count == 0) return null;
            var pv = p.T("PlayerVehicle"); if (pv.rows == 0) return null;
            int TypeOfRow(int row) => I(Row("VehicleModel", "VehicleModelId", pv.Int("VehicleModelId", row)), "VehicleTypeId");
            int pick = -1; bool equipped = false;
            int eq = Pro ? -1 : pv.Find("PlayerVehicleId", Equipped);
            if (eq >= 0 && types.Contains(TypeOfRow(eq))) { pick = eq; equipped = true; }
            if (pick < 0)
                for (int i = 0; i < pv.rows && pick < 0; i++) if (pv.Int("Favorite", i, 0) != 0 && types.Contains(TypeOfRow(i))) pick = i;
            if (pick < 0)
                for (int i = 0; i < pv.rows && pick < 0; i++) if (types.Contains(TypeOfRow(i))) pick = i;
            if (pick < 0) return null;
            var model = Row("VehicleModel", "VehicleModelId", pv.Int("VehicleModelId", pick));
            string title = Title(FeGarageData.ProductOfAsset(I(model, "StoreAssetId")), "");
            return string.IsNullOrEmpty(title) ? ((string, int, bool)?)null : (title.ToUpperInvariant(), I(model, "VehicleTypeId"), equipped);
        }

        public static string TypeName(int type) => OpenContent.VehicleTypeName(type);

        public static bool SelectVehicle(int playerVehicle) => ReflexProfile.SelectRider(0, -1, playerVehicle).Result;

        // ------------------------------------------------------------------------------------------------ riders

        public sealed class ProRide { public int rider, vehicle, gearSet, vehicleType, model; public string name; }
        public static List<ProRide> ProRides()
        {
            var list = new List<ProRide>();
            foreach (var rider in T.Rows("AIRider").Where(r => I(r, "PlayerSelectable", 0) != 0))
            {
                int id = I(rider, "AIRiderId"), gear = I(rider, "DefaultRiderGearSetId");
                string name = FeTables.S(rider, "DisplayName") ?? FeTables.S(rider, "Name") ?? "RIDER " + id;
                string shown = OpenContent.Str(name, null); if (string.IsNullOrEmpty(shown) || shown.StartsWith("STR_")) shown = name;
                foreach (var ride in T.Rows("AIRiderVehicle").Where(r => I(r, "AIRiderId") == id))
                {
                    int vehicle = I(ride, "AIVehicleId");
                    list.Add(new ProRide { rider = id, vehicle = vehicle, gearSet = gear, vehicleType = I(ride, "VehicleTypeId"), model = I(Row("AIVehicle", "AIVehicleId", vehicle), "VehicleModelId"), name = shown.ToUpperInvariant() });
                }
            }
            return list;
        }

        public static bool SelectPro(ProRide ride) => ReflexProfile.SelectRider(ride.rider, ride.gearSet, ride.vehicle).Result;
        public static bool SelectOwn() { var pv = Tab("PlayerVehicle"); return pv.rows > 0 && SelectVehicle(FirstOwnVehicle()); }
        /// The vehicle YOUR RIDER rides: the last one chosen as a player vehicle, else the first owned.
        public static int FirstOwnVehicle()
        {
            var pv = Tab("PlayerVehicle");
            if (pv.rows == 0) return -1;
            for (int i = 0; i < pv.rows; i++) if (pv.Int("Favorite", i, 0) != 0) return pv.Int("PlayerVehicleId", i);
            return pv.Int("PlayerVehicleId", 0);
        }

        // ------------------------------------------------------------------------------------------------ previews

        static ReflexGameTables Tables() => KitPacks.Tables(new FeTableSource(T));   // (the gear / kit packs' rows after the game's)

        /// What the stage shows for a model: the player's vehicle with its parts when owned, else the model with its default parts.
        /// asChosen: the bike model the player chose for it (GARAGE > VEHICLE > MODEL: Original or a bike mod, BikeMods.ForPlayer).
        public static BikeSpec BikeOfModel(int model, int partOverride = -1, bool asChosen = true)
        {
            int pv = PlayerVehicleOfModel(model);
            var parts = pv >= 0 ? EquippedPartIds(pv) : T.Rows("VehicleModelParts").Where(r => I(r, "VehicleModelId") == model && I(r, "Default", 0) != 0).Select(r => I(r, "VehiclePartId")).ToList();
            return BikeWithPart(model, parts, partOverride, asChosen);
        }

        static readonly Dictionary<int, List<BikeMod>> s_modsOfModel = new Dictionary<int, List<BikeMod>>();
        static List<BikeMod> s_modsOfModelFor, s_coverageLoggedFor;   // (per scan: a choice bumps BikeMods.Version but changes no coverage)
        /// The installed bike mods that have a chassis for this vehicle model (empty without mods), cached per BikeMods.Version.
        public static List<BikeMod> ModsOfModel(int model)
        {
            if (BikeMods.All.Count == 0 || model < 0) return new List<BikeMod>();
            if (!ReferenceEquals(s_modsOfModelFor, BikeMods.All)) { s_modsOfModel.Clear(); s_modsOfModelFor = BikeMods.All; }
            if (s_modsOfModel.TryGetValue(model, out var l)) return l;
            try { l = BikeMods.ModsFor(BikeOfModel(model, -1, false)); } catch (Exception e) { Debug.LogWarning("[BikeMods] coverage of model " + model + ": " + e.Message); l = new List<BikeMod>(); }
            return s_modsOfModel[model] = l;
        }

        /// The bike of a model as the mod shows it (modId "" or a mod that lacks the chassis: the original), whatever the player's choice is.
        public static BikeSpec BikeOfModelAs(int model, string modId)
        {
            var spec = BikeOfModel(model, -1, false);
            if (spec == null || string.IsNullOrEmpty(modId)) return spec;
            try { return BikeMods.Apply(spec, BikeMods.Find(modId)) ?? spec; }
            catch (Exception e) { Debug.LogWarning("[BikeMods] " + modId + ": " + e.Message); return spec; }
        }

        /// Logs once per scan which vehicle models each installed bike mod covers (the garage's list of bikes).
        public static void LogModCoverage(IEnumerable<GarageItem> vehicles)
        {
            if (BikeMods.All.Count == 0 || ReferenceEquals(s_coverageLoggedFor, BikeMods.All)) return;
            s_coverageLoggedFor = BikeMods.All;
            var list = vehicles.ToList();
            foreach (var m in BikeMods.All)
            {
                var covered = list.Where(v => ModsOfModel(v.id).Contains(m)).ToList();
                Debug.Log("[BikeMods] covers " + covered.Count + " bikes (of " + list.Count + " in the garage): " + m.name + ": "
                    + string.Join(", ", covered.Select(v => "#" + v.id + " " + v.title)));
            }
        }

        public static BikeSpec BikeWithPart(int model, List<int> parts, int partOverride, bool asChosen = true)
        {
            var spec = OriginalBikeWithPart(model, parts, partOverride);
            return asChosen ? BikeMods.ForPlayer(spec) : spec;
        }

        /// The tables a run of part questions reads (one per strip).
        public static ReflexGameTables PartTables() => Tables();

        /// A part's model and material packages (VehiclePart ModelPackageDatabaseId / MaterialPackageDatabaseId), null for none.
        public static ((string db, string pkg)? model, (string db, string pkg)? material) PartPackages(ReflexGameTables t, int partId)
        {
            int r = t.Find("VehiclePart", "VehiclePartId", partId);
            if (r < 0) return (null, null);
            return (t.Package(t.Int("VehiclePart", "ModelPackageDatabaseId", r)), t.Package(t.Int("VehiclePart", "MaterialPackageDatabaseId", r)));
        }

        /// Whether a bike mod (the one chosen for the bike: BikeMods.ChosenFor) changes this part (the garage strips' MOD tag).
        public static bool PartIsMod(BikeMod m, ReflexGameTables t, int partId)
        {
            if (m == null) return false;
            var (pm, mm) = PartPackages(t, partId);
            return BikeMods.ChangesPart(m, pm, mm);
        }

        static BikeSpec OriginalBikeWithPart(int model, List<int> parts, int partOverride)
        {
            parts = new List<int>(parts);
            if (partOverride >= 0)
            {
                int ty = PartTypeOf(partOverride);
                int k = parts.FindIndex(x => PartTypeOf(x) == ty);
                if (k >= 0) parts[k] = partOverride; else parts.Add(partOverride);
            }
            try { return ReflexVehicleSpecs.Bike(Tables(), model, parts); }
            catch (Exception e) { Debug.LogWarning("[Garage] vehicle: " + e.Message); return null; }
        }

        public static RiderSpec RiderWithGear(IEnumerable<int> gear, int gearOverride = -1)
        {
            var ids = gear.ToList();
            if (gearOverride >= 0)
            {
                int ty = GearTypeOf(gearOverride);
                int k = ids.FindIndex(x => GearTypeOf(x) == ty);
                if (k >= 0) ids[k] = gearOverride; else ids.Add(gearOverride);
            }
            try { return ids.Count == 0 ? null : ReflexVehicleSpecs.Rider(Tables(), ids); }
            catch (Exception e) { Debug.LogWarning("[Garage] rider: " + e.Message); return null; }
        }

        public static (BikeSpec bike, RiderSpec rider) ProSpecs(ProRide r)
        {
            var parts = T.Rows("AIVehicleParts").Where(x => I(x, "AIVehicleId") == r.vehicle).Select(x => I(x, "VehiclePartId")).ToList();
            var gear = T.Rows("RiderGearSetGear").Where(x => I(x, "RiderGearSetId") == r.gearSet).Select(x => I(x, "RiderGearId")).ToList();
            var rider = RiderWithGear(gear);
            if (rider != null) rider.plateRider = r.rider;   // his own jersey name / number
            return (BikeWithPart(r.model, parts, -1), rider);
        }

        // ------------------------------------------------------------------------------------------------ performance

        public static readonly string[] StatNames = { "engine", "suspension", "tires", "braking", "steering" };
        static readonly string[][] StatColumns =
        {
            new[] { "Engine1", "Engine2" }, new[] { "Suspension1", "Suspension2" }, new[] { "Tire1", "Tire2" }, new[] { "Braking" }, new[] { "Steering" },
        };

        /// The bars of a model (a player's vehicle: with its tuning; else the model's default parts' bonus).
        public static List<GarageStat> Stats(int model, int playerVehicle)
        {
            var m = Row("VehicleModel", "VehicleModelId", model);
            var list = new List<GarageStat>();
            if (m == null) return list;
            var tt = Tab("PlayerVehicleTuning"); int trow = playerVehicle >= 0 ? tt.Find("PlayerVehicleId", playerVehicle) : -1;
            var bonus = new Dictionary<string, int[]> { ["engine"] = new int[2], ["suspension"] = new int[2], ["tires"] = new int[2] };
            if (trow >= 0)
            {
                bonus["engine"][0] = tt.Int("Engine1", trow, 0); bonus["engine"][1] = tt.Int("Engine2", trow, 0);
                bonus["suspension"][0] = tt.Int("Suspension1", trow, 0); bonus["suspension"][1] = tt.Int("Suspension2", trow, 0);
                bonus["tires"][0] = tt.Int("Tire1", trow, 0); bonus["tires"][1] = tt.Int("Tire2", trow, 0);
            }
            else
                foreach (var r in T.Rows("VehicleModelParts").Where(r => I(r, "VehicleModelId") == model && I(r, "Default", 0) != 0))
                {
                    var part = Row("VehiclePart", "VehiclePartId", I(r, "VehiclePartId"));
                    string attr = FeTables.S(Row("VehiclePartType", "VehiclePartTypeId", I(part, "VehiclePartTypeId")), "TuningAttribute")?.ToLowerInvariant();
                    if (attr != null && bonus.TryGetValue(attr, out var b)) { b[0] = I(part, "DefaultTuneAttribute1", 0); b[1] = I(part, "DefaultTuneAttribute2", 0); }
                }
            for (int i = 0; i < 3; i++)
                list.Add(new GarageStat
                {
                    name = StatLabel(i), two = true, description = OpenContent.Str("STR_TUNE_" + StatNames[i].ToUpperInvariant() + "_DESCRIPTION", ""),
                    base1 = I(m, "Default" + StatColumns[i][0], 0), base2 = I(m, "Default" + StatColumns[i][1], 0), bonus1 = bonus[StatNames[i]][0], bonus2 = bonus[StatNames[i]][1],
                });
            for (int i = 3; i < 5; i++)
            {
                int v = trow >= 0 ? tt.Int(StatColumns[i][0], trow, 50) : 50;
                list.Add(new GarageStat { name = StatLabel(i), two = false, description = OpenContent.Str("STR_TUNE_" + StatNames[i].ToUpperInvariant() + "_DESCRIPTION", ""), base1 = v, base2 = 0, bonus1 = 0, bonus2 = 0 });
            }
            return list;
        }

        public static string StatLabel(int i)
        {
            string[] fallback = { "ENGINE", "SUSPENSION", "TIRES", "BRAKING", "STEERING" };
            string s = OpenContent.Str("STR_TUNE_" + StatNames[i].ToUpperInvariant(), null);
            return string.IsNullOrEmpty(s) || s.StartsWith("STR_") ? fallback[i] : s.ToUpperInvariant();
        }
    }
}
