using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Reflex.Port.Content
{
    public static partial class CoreGarageProfile
    {
        // 004e96e0: availability depends on the player's unlock row and threshold,
        // independently of price or ownership. Missing rows remain locked, even at threshold zero.
        static bool ProductLocked(ProfileData p, Dictionary<string, object> product)
        {
            var t = p.T("PlayerProductsUnlocked");
            int row = t.Find("StoreProductId", Rfx.Int(product, "StoreProductId"));
            return row < 0 || t.Int("UnlockCount", row, 0) < Rfx.Int(product, "UnlockRequirement", 0);
        }

        static ProfileData.Table StoreTable(ProfileData p, string name)
        {
            if (!p.tables.TryGetValue(name, out var t)) p.tables[name] = t = new ProfileData.Table();
            return t;
        }
        static int StoreAppend(ProfileData.Table t, params (string column, int value)[] cells)
        {
            int row = t.rows++;
            foreach (var values in t.columns.Values) while (values.Count < t.rows) values.Add(null);
            foreach (var c in cells) Set(t, c.column, row, c.value);
            return row;
        }
        static void StoreRemove(ProfileData.Table t, string column, int value)
        {
            for (int row = t.rows - 1; row >= 0; --row)
                if (t.Int(column, row) == value)
                {
                    foreach (var cells in t.columns.Values) if (row < cells.Count) cells.RemoveAt(row);
                    --t.rows;
                }
        }

        // 004e8920: purchasing a vehicle also acquires its authored default parts,
        // creates its instance and initial tuning, and records installed DLC membership.
        static void GrantStoreAsset(ProfileData p, int asset, HashSet<int> visiting)
        {
            if (Owned(p, asset)) return;
            if (!visiting.Add(asset)) throw new InvalidDataException("Cyclic store asset dependencies.");
            if (Row("StoreAssets", "StoreAssetId", asset) == null) throw new InvalidDataException("Store asset is unavailable.");
            StoreAppend(StoreTable(p, "PlayerAssetsPurchased"), ("StoreAssetId", asset));
            if (ReflexEvents.Tables.TryGetValue("DLCStoreAssets", out var dlcAssets))
                foreach (var d in dlcAssets.Where(r => Rfx.Int(r, "StoreAssetId") == asset))
                {
                    int id = Rfx.Int(d, "DLCId"); var t = StoreTable(p, "PlayerDLC");
                    if (t.Find("DLCId", id) < 0) StoreAppend(t, ("DLCId", id));
                }
            var model = Row("VehicleModel", "StoreAssetId", asset);
            if (model != null)
            {
                int modelId = Rfx.Int(model, "VehicleModelId"); var vehicles = StoreTable(p, "PlayerVehicle");
                if (vehicles.Find("VehicleModelId", modelId) < 0)
                {
                    int id = checked(Enumerable.Range(0, vehicles.rows).Select(i => vehicles.Int("PlayerVehicleId", i)).DefaultIfEmpty(-1).Max() + 1);
                    int type = Rfx.Int(model, "VehicleTypeId");
                    bool favorite = Enumerable.Range(0, vehicles.rows).Any(i => vehicles.Int("Favorite", i, 0) != 0 && Rfx.Int(Row("VehicleModel", "VehicleModelId", vehicles.Int("VehicleModelId", i)), "VehicleTypeId") == type);
                    var defaults = ReflexEvents.Tables["VehicleModelParts"].Where(r => Rfx.Int(r, "VehicleModelId") == modelId && Rfx.Int(r, "Default", 0) != 0).ToArray();
                    if (defaults.Length == 0) throw new InvalidDataException("Vehicle has no default parts.");
                    StoreAppend(vehicles, ("PlayerVehicleId", id), ("VehicleModelId", modelId), ("Favorite", favorite ? 0 : 1));
                    var parts = StoreTable(p, "PlayerVehicleParts"); var tuning = StoreTable(p, "PlayerVehicleTuning");
                    int tr = StoreAppend(tuning, ("PlayerVehicleId", id), ("Engine1", 0), ("Engine2", 0), ("Suspension1", 0), ("Suspension2", 0), ("Tire1", 0), ("Tire2", 0), ("Braking", 50), ("Steering", 50));
                    foreach (var item in defaults)
                    {
                        GrantStoreAsset(p, Rfx.Int(item, "StoreAssetId"), visiting);
                        int partId = Rfx.Int(item, "VehiclePartId"); var part = Row("VehiclePart", "VehiclePartId", partId) ?? throw new InvalidDataException("Default part is unavailable.");
                        StoreAppend(parts, ("PlayerVehicleId", id), ("VehiclePartId", partId));
                        string attr = Rfx.Str(Row("VehiclePartType", "VehiclePartTypeId", Rfx.Int(part, "VehiclePartTypeId")), "TuningAttribute");
                        string prefix = string.Equals(attr, "engine", StringComparison.OrdinalIgnoreCase) ? "Engine" : string.Equals(attr, "suspension", StringComparison.OrdinalIgnoreCase) ? "Suspension" : string.Equals(attr, "tires", StringComparison.OrdinalIgnoreCase) ? "Tire" : null;
                        if (prefix != null) { Set(tuning, prefix + "1", tr, Rfx.Int(part, "DefaultTuneAttribute1", 0)); Set(tuning, prefix + "2", tr, Rfx.Int(part, "DefaultTuneAttribute2", 0)); }
                    }
                }
            }
            visiting.Remove(asset);
        }

        public static bool PurchaseAsset(int asset, int quotedPrice) => Change(p =>
        {
            var a = Row("StoreAssets", "StoreAssetId", asset) ?? throw new InvalidDataException("Store asset is unavailable.");
            int productId = Rfx.Int(a, "StoreProductId");
            var product = Row("StoreProducts", "StoreProductId", productId) ?? throw new InvalidDataException("Store product is unavailable.");
            int price = Rfx.Int(product, "Price", -1);
            if (price < 0 || price != quotedPrice) throw new InvalidDataException("The price changed. Select this item again.");
            if (OwnsAsset(p, asset)) return; // Repeated acceptance must not charge twice.
            // MXTableHelper_PurchaseStoreAsset never asks IsLocked: the gear and parts pages sell every item that is not hidden, and only
            // GarageMyRides keeps a locked vehicle model from being bought
            if (Rfx.Int(product, "Hidden", 0) != 0 || !unlocked && VehicleModelAsset(asset) && ProductLocked(p, product)) throw new InvalidDataException("This item has not been unlocked.");
            int balance = p.T("PlayerRider").Int("AccountBalance", 0, 0);
            if (balance < price) throw new InvalidDataException("Not enough credits.");
            GrantStoreAsset(p, asset, new HashSet<int>());
            Set(p.T("PlayerRider"), "AccountBalance", 0, checked(balance - price));
            StoreRemove(p.T("PlayerProductsNew"), "StoreProductId", productId);
        });
    }
}
