using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Reflex.Port.Content
{
    // Pure catalog expansion of a detached profile. The caller owns persistence and
    // must never pass the career instance. No earned progress or selection is reset.
    internal static class CoreGarageUnlocks
    {
        internal static void Expand(ProfileData profile, Dictionary<string, List<Dictionary<string, object>>> catalog)
        {
            IEnumerable<Dictionary<string, object>> Rows(string table) => catalog.TryGetValue(table, out var rows) ? rows : Enumerable.Empty<Dictionary<string, object>>();
            Dictionary<string, object> Row(string table, string key, int id) => Rows(table).FirstOrDefault(r => Rfx.Int(r, key) == id);
            bool Retail(int asset) => asset >= 0 && Row("StoreAssets", "StoreAssetId", asset) is Dictionary<string, object> a && Row("StoreProducts", "StoreProductId", Rfx.Int(a, "StoreProductId")) != null;
            EnsureProductUnlocks(profile, catalog);
            // Grant installed store assets in this overlay only, including event/location
            // products. Do not counterfeit medals, event status or career completion.
            var assets = Rows("StoreAssets").Select(r => Rfx.Int(r, "StoreAssetId")).Where(Retail).Distinct();
            var purchased = Table(profile, "PlayerAssetsPurchased");
            foreach (int asset in assets)
                if (purchased.Find("StoreAssetId", asset) < 0) Append(purchased, ("StoreAssetId", asset));

            var vehicles = Table(profile, "PlayerVehicle");
            var parts = Table(profile, "PlayerVehicleParts");
            var tuning = Table(profile, "PlayerVehicleTuning");
            int nextId = Enumerable.Range(0, vehicles.rows).Select(i => vehicles.Int("PlayerVehicleId", i)).DefaultIfEmpty(-1).Max() + 1;
            foreach (var model in Rows("VehicleModel").Where(r => Retail(Rfx.Int(r, "StoreAssetId"))))
            {
                int modelId = Rfx.Int(model, "VehicleModelId");
                if (vehicles.Find("VehicleModelId", modelId) >= 0) continue;
                var defaults = Rows("VehicleModelParts").Where(r => Rfx.Int(r, "VehicleModelId") == modelId && Rfx.Int(r, "Default", 0) != 0).ToArray();
                if (defaults.Length == 0) throw new InvalidDataException("Vehicle model has no authored default parts: " + modelId);
                int id = nextId++;
                Append(vehicles, ("PlayerVehicleId", id), ("VehicleModelId", modelId), ("Favorite", 0));
                int tr = Append(tuning, ("PlayerVehicleId", id), ("Engine1", 0), ("Engine2", 0), ("Suspension1", 0), ("Suspension2", 0), ("Tire1", 0), ("Tire2", 0), ("Braking", 50), ("Steering", 50));
                foreach (var item in defaults)
                {
                    int partId = Rfx.Int(item, "VehiclePartId");
                    var part = Row("VehiclePart", "VehiclePartId", partId) ?? throw new InvalidDataException("Default vehicle part is unavailable: " + partId);
                    Append(parts, ("PlayerVehicleId", id), ("VehiclePartId", partId));
                    var kind = Row("VehiclePartType", "VehiclePartTypeId", Rfx.Int(part, "VehiclePartTypeId"));
                    string attr = Rfx.Str(kind, "TuningAttribute");
                    string prefix = attr == "engine" ? "Engine" : attr == "suspension" ? "Suspension" : attr == "tires" ? "Tire" : null;
                    if (prefix != null)
                    {
                        // Same default-part assignment as VehiclePerformanceDisplayComponent.
                        Put(tuning, prefix + "1", tr, Rfx.Int(part, "DefaultTuneAttribute1", 0));
                        Put(tuning, prefix + "2", tr, Rfx.Int(part, "DefaultTuneAttribute2", 0));
                    }
                }
            }
            // Normally every slot exists in the starter profile. Installed catalog additions
            // may introduce another gear type; give it an actual authored item, never a dummy ID.
            var gear = Table(profile, "PlayerRiderGear");
            var equippedTypes = new HashSet<int>(Enumerable.Range(0, gear.rows).Select(i => Rfx.Int(Row("RiderGear", "RiderGearId", gear.Int("RiderGearId", i)), "RiderGearTypeId")));
            foreach (var item in Rows("RiderGear").Where(r => Retail(Rfx.Int(r, "StoreAssetId"))))
                if (equippedTypes.Add(Rfx.Int(item, "RiderGearTypeId"))) Append(gear, ("RiderGearId", Rfx.Int(item, "RiderGearId")));
        }

        // The native original IsLocked predicate requires BOTH row presence and
        // signed UnlockCount >= authored UnlockRequirement. This is explicit
        // detached Unlock Everything policy, never earned career progression.
        internal static void EnsureProductUnlocks(ProfileData profile, Dictionary<string, List<Dictionary<string, object>>> catalog)
        {
            if (!catalog.TryGetValue("StoreProducts", out var products)) throw new InvalidDataException("Installed StoreProducts is missing.");
            var table = Table(profile, "PlayerProductsUnlocked");
            var authored = new HashSet<int>();
            var rows = new Dictionary<int, int>();
            for (int i = 0; i < table.rows; i++)
            {
                int id = table.Int("StoreProductId", i);
                if (rows.ContainsKey(id)) throw new InvalidDataException("Duplicate unlocked product.");
                rows.Add(id, i);
            }
            foreach (var product in products)
            {
                int Integer(string column)
                {
                    if (!product.TryGetValue(column, out var value) || value == null || value is string || value is bool)
                        throw new InvalidDataException("Installed product integer is missing: " + column);
                    double number = Convert.ToDouble(value, System.Globalization.CultureInfo.InvariantCulture);
                    if (double.IsNaN(number) || double.IsInfinity(number) || number < int.MinValue || number > int.MaxValue || number != Math.Truncate(number))
                        throw new InvalidDataException("Installed product integer is invalid: " + column);
                    return (int)number;
                }
                int id = Integer("StoreProductId"), required = Math.Max(1, Integer("UnlockRequirement"));
                if (!authored.Add(id)) throw new InvalidDataException("Duplicate installed store product.");
                if (!rows.TryGetValue(id, out int row)) rows.Add(id, Append(table, ("StoreProductId", id), ("UnlockCount", required)));
                else if (table.Int("UnlockCount", row) < required) Put(table, "UnlockCount", row, required);
            }
        }

        static ProfileData.Table Table(ProfileData p, string name)
        {
            if (!p.tables.TryGetValue(name, out var table)) p.tables[name] = table = new ProfileData.Table();
            return table;
        }
        static int Append(ProfileData.Table table, params (string key, int value)[] values)
        {
            int row = table.rows++;
            foreach (var cells in table.columns.Values) while (cells.Count < table.rows) cells.Add(null);
            foreach (var value in values) Put(table, value.key, row, value.value);
            return row;
        }
        static void Put(ProfileData.Table table, string column, int row, int value)
        {
            if (!table.columns.TryGetValue(column, out var cells)) table.columns[column] = cells = new List<object>();
            while (cells.Count < table.rows) cells.Add(null);
            cells[row] = (double)value;
        }
    }
}
