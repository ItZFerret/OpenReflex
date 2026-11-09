using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using UnityEngine;
using Reflex.Port.RxCore;

namespace Reflex.Port.Content
{
    // Full Player v2 is authoritative after migration; garage edits patch typed cells only.
    // The original Steam save remains an import source, never a write destination.
    public static partial class CoreGarageProfile
    {
        [Serializable] sealed class Cell { public string table, column, text; public int row; public double number; public bool isText; }
        [Serializable] sealed class Document { public int version = 1; public List<Cell> cells = new List<Cell>(); }
        static ProfileData current;
        static string loadedPath, modeDirectory;
        static bool unlocked;
        static bool unlockedProductRowsChecked;
        [Serializable] sealed class ModeDocument { public int version = 1; public bool unlockEverything; }
        public static bool IsOpen => current != null && loadedPath == FilePath;
        static string operationError;
        public static string Error { get => CheckpointError ?? operationError; private set => operationError = value; }
        public static string SaveDirectory => string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("REFLEX_SAVE_DIR")) ? ReflexProfile.SaveDir : Path.GetFullPath(Environment.GetEnvironmentVariable("REFLEX_SAVE_DIR"));
        public static string CareerFilePath => Path.Combine(SaveDirectory, "core-garage-v1.json");
        public static string UnlockedFilePath => Path.Combine(SaveDirectory, "core-garage-unlocked-v1.json");
        public static string ModeFilePath => Path.Combine(SaveDirectory, "core-garage-mode-v1.json");
        public static string FilePath
        {
            get { bool all = unlocked && modeDirectory == SaveDirectory; string snapshot = PlayerStore(SaveDirectory).FilePath(Mode(all)); return File.Exists(snapshot) ? snapshot : all ? UnlockedFilePath : CareerFilePath; }
        }
        public static bool UnlockEverythingEnabled { get { lock (gate) return Open() && unlocked; } }

        // Destination is fully validated before mode publication. Career tables and the
        // original Steam save are never write destinations of this operation.
        public static bool SetUnlockEverything(bool enabled)
        {
            lock (gate)
            {
                try { return OpenReflexSimulation.WithoutActivePlayerSession(() => SetUnlockEverythingIdle(enabled)); }
                catch (Exception e) { Error = "Garage mode was not changed: " + e.Message; return false; }
            }
        }
        static bool SetUnlockEverythingIdle(bool enabled)
        {
                if (!Open() && enabled) return false;
                bool sameMode = IsOpen && unlocked == enabled;
                if (sameMode && !enabled) return true;
                try
                {
                    PlayerSnapshotLease nextLease;
                    ProfileData next;
                    if (enabled && playerLease != null && !File.Exists(UnlockedFilePath) && !File.Exists(PlayerStore(SaveDirectory).FilePath(Mode(true))))
                    { nextLease = null; next = PlayerProfileSnapshotAdapter.CloneView(current); }
                    else next = ReadMode(enabled, out nextLease);
                    if (enabled && nextLease == null && !File.Exists(UnlockedFilePath)) next = PlayerProfileSnapshotAdapter.CloneView(current);
                    if (enabled) nextLease = RefreshUnlockedProducts(next, nextLease);
                    Validate(next); PopulateProducts(next, enabled);
                    if (enabled && nextLease == null && playerLease != null && !File.Exists(UnlockedFilePath))
                    {
                        // A new unlocked mode is an explicit clone of an existing full
                        // schema snapshot, never a write back into career.
                        var document = PlayerProfileSnapshotAdapter.ApplyDiff(playerLease.Document, current, next);
                        nextLease = PlayerStore(SaveDirectory).CreateIfAbsent(Mode(true), document);
                    }
                    else if (enabled && nextLease == null) WriteAtomic(UnlockedFilePath, JsonUtility.ToJson(Encode(next), true));
                    if (!sameMode) WriteAtomic(ModeFilePath, JsonUtility.ToJson(new ModeDocument { unlockEverything = enabled }, true));
                    unlocked = enabled; unlockedProductRowsChecked = enabled; current = next; playerLease = nextLease; modeDirectory = SaveDirectory; loadedPath = FilePath; Error = null;
                    return true;
                }
                catch (Exception e) { Error = "Garage mode was not changed: " + e.Message; return false; }
        }

        static PlayerSnapshotLease RefreshUnlockedProducts(ProfileData view, PlayerSnapshotLease lease)
        {
            // Caller holds both profile gate and actual worker/catalog lifetime guard.
            if (lease == null) { CoreGarageUnlocks.Expand(view, ReflexEvents.Tables); return null; }
            var before = PlayerProfileSnapshotAdapter.CloneView(view);
            CoreGarageUnlocks.EnsureProductUnlocks(view, ReflexEvents.Tables);
            Validate(view); PopulateProducts(view, true);
            var document = PlayerProfileSnapshotAdapter.ApplyDiff(lease.Document, before, view);
            if (PlayerSnapshotDocument.Hash(document.Encode()) == PlayerSnapshotDocument.Hash(lease.Document.Encode())) return lease;
            return PlayerStore(SaveDirectory).Commit(lease, document);
        }

        static ProfileData ReadDocument(string path)
        {
            if (new FileInfo(path).Length > 32 * 1024 * 1024) throw new InvalidDataException("Garage profile is too large.");
            var d = JsonUtility.FromJson<Document>(File.ReadAllText(path));
            if (d == null || d.version != 1 || d.cells == null || d.cells.Count > 1000000) throw new InvalidDataException("Unsupported garage profile.");
            return Decode(d);
        }

        static ProfileData ReadCareer()
        {
            var p = FromRows(ReflexEvents.Tables.Where(x => x.Key.StartsWith("Player", StringComparison.Ordinal)).ToDictionary(x => x.Key, x => x.Value));
            if (File.Exists(CareerFilePath)) return ReadDocument(CareerFilePath);
            string original = Path.Combine(SaveDirectory, "remote", "savegame.bxml");
            if (File.Exists(original))
            {
                var b = File.ReadAllBytes(original);
                if (b.Length < 40 || BitConverter.ToUInt32(b, b.Length - 4) != 0x27720006) throw new InvalidDataException("Original profile trailer is invalid.");
                Array.Resize(ref b, b.Length - 4);
                var root = BxmlData.Xml(b);
                if (root.Child("Version")?.value == null || Convert.ToInt32(root.Child("Version").value) != 0x12) throw new InvalidDataException("Unsupported original profile version.");
                foreach (var t in FromRows(Rfx.Tables(b)).tables) if (t.Key.StartsWith("Player", StringComparison.Ordinal)) p.tables[t.Key] = t.Value;
            }
            return p;
        }

        static void WriteAtomic(string path, string json)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                byte[] bytes = System.Text.Encoding.UTF8.GetBytes(json);
                using (var stream = new FileStream(temp, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { stream.Write(bytes, 0, bytes.Length); stream.Flush(true); }
                if (File.Exists(path)) File.Replace(temp, path, path + ".bak"); else File.Move(temp, path);
            }
            finally { if (File.Exists(temp)) File.Delete(temp); }
        }
        public static bool Open()
        {
            lock (gate)
            {
                if (IsOpen && (!unlocked || unlockedProductRowsChecked)) return true;
                try { return OpenReflexSimulation.WithoutActivePlayerSession(OpenIdle); }
                catch (Exception e) { Error = "Garage profile could not be read: " + e.Message; return false; }
            }
        }
        static bool OpenIdle()
        {
                try
                {
                    bool requested = false;
                    if (File.Exists(ModeFilePath))
                    {
                        if (new FileInfo(ModeFilePath).Length > 4096) throw new InvalidDataException("Garage mode file is too large.");
                        var mode = JsonUtility.FromJson<ModeDocument>(File.ReadAllText(ModeFilePath));
                        if (mode == null || mode.version != 1) throw new InvalidDataException("Unsupported garage mode.");
                        requested = mode.unlockEverything;
                    }
                    var p = ReadMode(requested, out var nextLease);
                    if (requested) nextLease = RefreshUnlockedProducts(p, nextLease);
                    Validate(p); PopulateProducts(p, requested); current = p; playerLease = nextLease; unlocked = requested; unlockedProductRowsChecked = requested; modeDirectory = SaveDirectory; loadedPath = FilePath; Error = null; return true;
                }
                catch (Exception e) { Error = "Garage profile could not be read: " + e.Message; return false; }
        }
        static readonly object gate = new object();
        public static void Close() { lock (gate) { current = null; playerLease = null; loadedPath = null; modeDirectory = null; unlocked = false; unlockedProductRowsChecked = false; } }
        public static ProfileData Capture() { lock (gate) { if (!Open()) return null; return PlayerProfileSnapshotAdapter.CloneView(current); } }
        static ProfileData FromRows(Dictionary<string, List<Dictionary<string, object>>> rows)
        {
            var p = new ProfileData { signedIn = true, storage = true, gameLoaded = true };
            foreach (var entry in rows)
            {
                var t = new ProfileData.Table { rows = entry.Value.Count };
                foreach (string key in entry.Value.SelectMany(r => r.Keys).Distinct())
                    t.columns[key] = entry.Value.Select(r => r.TryGetValue(key, out var v) ? v is string ? v : v == null ? null : (object)Convert.ToDouble(v, CultureInfo.InvariantCulture) : null).ToList();
                p.tables[entry.Key] = t;
            }
            return p;
        }
        static Document Encode(ProfileData p)
        {
            var d = new Document();
            foreach (var t in p.tables) foreach (var c in t.Value.columns) for (int r = 0; r < t.Value.rows; r++)
                if (r < c.Value.Count && c.Value[r] != null) d.cells.Add(new Cell { table = t.Key, column = c.Key, row = r, text = c.Value[r] as string, isText = c.Value[r] is string, number = c.Value[r] is double n ? n : 0 });
            return d;
        }
        static ProfileData Decode(Document d)
        {
            var p = new ProfileData { signedIn = true, storage = true, gameLoaded = true };
            foreach (var c in d.cells)
            {
                if (c == null || c.table == null || !c.table.StartsWith("Player", StringComparison.Ordinal) || string.IsNullOrEmpty(c.column) || c.row < 0 || c.row > 100000 || c.table.Length > 64 || c.column.Length > 64 || c.text != null && c.text.Length > 4096 || double.IsNaN(c.number) || double.IsInfinity(c.number)) throw new InvalidDataException("Invalid garage profile cell.");
                if (!p.tables.TryGetValue(c.table, out var t)) p.tables[c.table] = t = new ProfileData.Table();
                if (!t.columns.TryGetValue(c.column, out var values)) t.columns[c.column] = values = new List<object>();
                while (values.Count <= c.row) values.Add(null);
                values[c.row] = c.isText ? (object)c.text : c.number; t.rows = Math.Max(t.rows, c.row + 1);
            }
            return p;
        }
        static Dictionary<string, object> Row(string table, string col, int id) => ReflexEvents.Tables.TryGetValue(table, out var rows) ? RowIndex.First(rows, col, id) : null;   // (indexed: w-menufps)
        static bool Owned(ProfileData p, int asset) => p.T("PlayerAssetsPurchased").Find("StoreAssetId", asset) >= 0;
        public static bool OwnsAsset(int asset) { lock (gate) return Open() && OwnsAsset(current, asset); }
        static bool OwnsAsset(ProfileData p, int asset)
        {
            if (Owned(p, asset)) return true;
            // Imported starter equipment can be owned before its explicit acquisition row exists.
            // (the tables' rows of the asset by an index, not a scan of each table per asset: the garage asks this for every list item; w-menufps)
            if (ReflexEvents.Tables.TryGetValue("VehicleModel", out var models) && RowIndex.All(models, "StoreAssetId", asset).Any(r => p.T("PlayerVehicle").Find("VehicleModelId", Rfx.Int(r, "VehicleModelId")) >= 0)) return true;
            if (ReflexEvents.Tables.TryGetValue("RiderGear", out var gear) && RowIndex.All(gear, "StoreAssetId", asset).Any(r => p.T("PlayerRiderGear").Find("RiderGearId", Rfx.Int(r, "RiderGearId")) >= 0)) return true;
            if (ReflexEvents.Tables.TryGetValue("VehicleModelParts", out var parts))
                foreach (var part in RowIndex.All(parts, "StoreAssetId", asset))
                    for (int i = 0; i < p.T("PlayerVehicleParts").rows; ++i)
                    {
                        var equipped = p.T("PlayerVehicleParts"); var vehicles = p.T("PlayerVehicle");
                        int vr = vehicles.Find("PlayerVehicleId", equipped.Int("PlayerVehicleId", i));
                        if (equipped.Int("VehiclePartId", i) == Rfx.Int(part, "VehiclePartId") && vehicles.Int("VehicleModelId", vr) == Rfx.Int(part, "VehicleModelId")) return true;
                    }
            return false;
        }
        public static bool AvailableAsset(int asset)
        {
            lock (gate)
            {
                if (!Open()) return false;
                if (OwnsAsset(current, asset)) return true;
                var sp = Row("StoreAssets", "StoreAssetId", asset);
                var product = Row("StoreProducts", "StoreProductId", Rfx.Int(sp, "StoreProductId"));
                // a free item is owned (GarageRiderCustomize / GarageVehicleCustomize: cost 0 -> isPurchased); only a vehicle model also waits
                // for its career unlock (GarageMyRides is the one garage page that asks IsLocked)
                return product != null && Rfx.Int(product, "Price", -1) == 0 && (unlocked || !VehicleModelAsset(asset) || !ProductLocked(current, product));
            }
        }
        /// A vehicle model's store asset (VehicleModel.StoreAssetId): the garage items whose product is unlocked by the career.
        internal static bool VehicleModelAsset(int asset) => asset >= 0 && Row("VehicleModel", "StoreAssetId", asset) != null;
        static void PopulateProducts(ProfileData p, bool all = false)
        {
            if (!ReflexEvents.Tables.TryGetValue("StoreProducts", out var products)) return;
            p.products.Clear();
            foreach (var r in products)
            {
                int id = Rfx.Int(r, "StoreProductId");
                p.products[id] = (!all && ProductLocked(p, r), Rfx.Int(r, "Hidden", 0) != 0,
                    p.T("PlayerProductsNew").Find("StoreProductId", id) >= 0,
                    ReflexEvents.Tables.TryGetValue("DLCStoreProducts", out var dlc) && RowIndex.First(dlc, "StoreProductId", id) != null);
            }
        }

        static void Set(ProfileData.Table t, string col, int row, int n)
        {
            if (row < 0 || row >= t.rows) throw new InvalidDataException("Missing garage profile row.");
            if (!t.columns.TryGetValue(col, out var values)) t.columns[col] = values = new List<object>();
            while (values.Count < t.rows) values.Add(null); values[row] = (double)n;
        }
        static void Validate(ProfileData p)
        {
            ValidateTuning(p);
            if (p.T("PlayerRider").rows != 1) throw new InvalidDataException("Garage requires one local rider.");
            var v = p.T("PlayerVehicle");
            int last = p.T("PlayerRider").Int("LastEquippedVehicle", 0), rider = p.T("PlayerRider").Int("LastRiderId", 0, 0);
            if (rider > 0)
            {
                if (Row("AIRider", "AIRiderId", rider) == null || Rfx.Int(Row("AIRider", "AIRiderId", rider), "DefaultRiderGearSetId") != p.T("PlayerRider").Int("LastRiderGearSetId", 0) || !ReflexEvents.Tables["AIRiderVehicle"].Any(x => Rfx.Int(x, "AIRiderId") == rider && Rfx.Int(x, "AIVehicleId") == last)) throw new InvalidDataException("Equipped pro rider vehicle is unavailable.");
            }
            else if (v.Find("PlayerVehicleId", last) < 0) throw new InvalidDataException("Equipped vehicle is absent from the profile.");
            var vehicleIds = new HashSet<int>();
            for (int r = 0; r < v.rows; r++) if (!vehicleIds.Add(v.Int("PlayerVehicleId", r))) throw new InvalidDataException("Duplicate profile vehicle.");
            for (int r = 0; r < v.rows; r++) if (Row("VehicleModel", "VehicleModelId", v.Int("VehicleModelId", r)) == null) throw new InvalidDataException("Profile vehicle model is unavailable in this install.");
            var gearTypes = new HashSet<int>(); var g = p.T("PlayerRiderGear");
            for (int r = 0; r < g.rows; r++) if (!gearTypes.Add(Rfx.Int(Row("RiderGear", "RiderGearId", g.Int("RiderGearId", r)), "RiderGearTypeId"))) throw new InvalidDataException("Duplicate rider gear slot.");
            for (int r = 0; r < g.rows; r++) if (Row("RiderGear", "RiderGearId", g.Int("RiderGearId", r)) == null) throw new InvalidDataException("Profile rider gear is unavailable in this install.");
            var partSlots = new HashSet<string>(); var parts = p.T("PlayerVehicleParts");
            for (int r = 0; r < parts.rows; r++) if (!partSlots.Add(parts.Int("PlayerVehicleId", r) + ":" + Rfx.Int(Row("VehiclePart", "VehiclePartId", parts.Int("VehiclePartId", r)), "VehiclePartTypeId"))) throw new InvalidDataException("Duplicate vehicle part slot.");
            for (int r = 0; r < parts.rows; r++) if (v.Find("PlayerVehicleId", parts.Int("PlayerVehicleId", r)) < 0 || Row("VehiclePart", "VehiclePartId", parts.Int("VehiclePartId", r)) == null) throw new InvalidDataException("Profile vehicle part is unavailable in this install.");
            for (int r = 0; r < parts.rows; r++)
            {
                int model = v.Int("VehicleModelId", v.Find("PlayerVehicleId", parts.Int("PlayerVehicleId", r))), part = parts.Int("VehiclePartId", r);
                if (!ReflexEvents.Tables["VehicleModelParts"].Any(x => Rfx.Int(x, "VehicleModelId") == model && Rfx.Int(x, "VehiclePartId") == part)) throw new InvalidDataException("Stored vehicle part does not fit its model.");
            }
        }            
        static void ValidateTuning(ProfileData p)
        {
            var tt = p.T("PlayerVehicleTuning"); var seen = new HashSet<int>(); var pv = p.T("PlayerVehicle");
            string[] columns = { "Engine1", "Engine2", "Suspension1", "Suspension2", "Tire1", "Tire2", "Braking", "Steering" };
            for (int r = 0; r < tt.rows; r++)
            {
                int id = tt.Int("PlayerVehicleId", r), vr = pv.Find("PlayerVehicleId", id);
                if (vr < 0 || !seen.Add(id)) throw new InvalidDataException("Invalid tuning vehicle row.");
                var model = Row("VehicleModel", "VehicleModelId", pv.Int("VehicleModelId", vr));
                for (int i = 0; i < columns.Length; i++)
                {
                    int n = tt.Int(columns[i], r, 0);
                    if (n < 0 || n > 100 || i < 6 && n + Rfx.Int(model, "Default" + columns[i], 0) > 100 || i >= 6 && n % 50 != 0) throw new InvalidDataException("Stored vehicle tuning is outside its limits.");
                }
            }
        }

        static bool Change(Action<ProfileData> change)
        {
            lock (gate)
            {
                if (!Open()) return false;
                try
                {
                    var next = PlayerProfileSnapshotAdapter.CloneView(current); change(next); Validate(next); PopulateProducts(next, unlocked);
                    if (playerLease != null)
                    {
                        var document = PlayerProfileSnapshotAdapter.ApplyDiff(playerLease.Document, current, next);
                        playerLease = PlayerStore(SaveDirectory).Commit(playerLease, document);
                    }
                    else WriteAtomic(FilePath, JsonUtility.ToJson(Encode(next), true));
                    current = next; loadedPath = FilePath; Error = null; return true;
                }
                catch (Exception e) { Error = "Garage change was not saved: " + e.Message; return false; }
            }
        }
        public static bool Command(string command)
        {
            var a = (command ?? "").Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
            if (a.Length == 1 && a[0] == "save") return Change(_ => { });
            if (a.Length == 2 && a[0] == "store_not_new" && int.TryParse(a[1], out int product))
                return Change(p => StoreRemove(p.T("PlayerProductsNew"), "StoreProductId", product));
            if (a.Length == 2 && a[0] == "favorite_vehicle" && int.TryParse(a[1], out int id))
                return Change(p => { if (p.T("PlayerVehicle").Find("PlayerVehicleId", id) < 0) throw new InvalidDataException("Vehicle is not owned."); Set(p.T("PlayerRider"), "LastEquippedVehicle", 0, id); Set(p.T("PlayerRider"), "LastRiderId", 0, 0); });
            Error = "This operation requires the original career profile backend."; return false;
        }
        public static bool SelectRider(int rider, int gearSet, int vehicle) => Change(p =>
        {
            if (rider < 0) rider = 0;
            if (rider == 0 && p.T("PlayerVehicle").Find("PlayerVehicleId", vehicle) < 0) throw new InvalidDataException("Vehicle is not owned.");
            if (rider > 0)
            {
                var row = Row("AIRider", "AIRiderId", rider);
                if (row == null || Rfx.Int(row, "PlayerSelectable", 0) == 0 || Rfx.Int(row, "DefaultRiderGearSetId") != gearSet) throw new InvalidDataException("Pro rider is unavailable.");
            }
            Set(p.T("PlayerRider"), "LastRiderId", 0, rider); Set(p.T("PlayerRider"), "LastEquippedVehicle", 0, vehicle);
            if (rider > 0) Set(p.T("PlayerRider"), "LastRiderGearSetId", 0, gearSet);
        });
        public static bool EquipGear(int id) => Change(p =>
        {
            var gear = Row("RiderGear", "RiderGearId", id); if (gear == null || !AvailableAsset(Rfx.Int(gear, "StoreAssetId"))) throw new InvalidDataException("Rider gear is not owned.");
            int type = Rfx.Int(gear, "RiderGearTypeId"); var t = p.T("PlayerRiderGear");
            int r = Enumerable.Range(0, t.rows).Where(i => Rfx.Int(Row("RiderGear", "RiderGearId", t.Int("RiderGearId", i)), "RiderGearTypeId") == type).DefaultIfEmpty(-1).First();
            if (r < 0) throw new InvalidDataException("Rider gear slot is absent."); Set(t, "RiderGearId", r, id);
            if (type == 9 || type == 11) for (int i = 0; i < t.rows; i++) if (Rfx.Int(Row("RiderGear", "RiderGearId", t.Int("RiderGearId", i)), "RiderGearTypeId") == (type == 9 ? 11 : 9)) Set(t, "RiderGearId", i, type == 9 ? 322 : 13);
        });
        public static bool EquipPart(int vehicle, int part, string tune, int one, int two) => Change(p =>
        {
            var pr = Row("VehiclePart", "VehiclePartId", part); if (pr == null) throw new InvalidDataException("Vehicle part is unavailable.");
            var v = p.T("PlayerVehicle"); int vr = v.Find("PlayerVehicleId", vehicle); if (vr < 0) throw new InvalidDataException("Vehicle is not owned.");
            var membership = ReflexEvents.Tables["VehicleModelParts"].FirstOrDefault(x => Rfx.Int(x, "VehicleModelId") == v.Int("VehicleModelId", vr) && Rfx.Int(x, "VehiclePartId") == part);
            if (membership == null || !AvailableAsset(Rfx.Int(membership, "StoreAssetId"))) throw new InvalidDataException("Vehicle part is not owned or does not fit this model.");
            var t = p.T("PlayerVehicleParts"); int type = Rfx.Int(pr, "VehiclePartTypeId"); int found = -1;
            for (int r = 0; r < t.rows; r++) if (t.Int("PlayerVehicleId", r) == vehicle && Rfx.Int(Row("VehiclePart", "VehiclePartId", t.Int("VehiclePartId", r)), "VehiclePartTypeId") == type) { found = r; break; }
            if (found < 0) throw new InvalidDataException("Vehicle part slot is absent."); Set(t, "VehiclePartId", found, part);
            string prefix = tune == "engine" ? "Engine" : tune == "suspension" ? "Suspension" : tune == "tires" ? "Tire" : null;
            if (prefix != null) { if (one != Rfx.Int(pr, "DefaultTuneAttribute1", 0) || two != Rfx.Int(pr, "DefaultTuneAttribute2", 0)) throw new InvalidDataException("Vehicle part tuning differs from the install."); var tt = p.T("PlayerVehicleTuning"); int r = tt.Find("PlayerVehicleId", vehicle); Set(tt, prefix + "1", r, one); Set(tt, prefix + "2", r, two); }
        });
        public static bool Tune(int vehicle, ReflexGarage.Tuning value) => Change(p =>
        {
            var v = p.T("PlayerVehicle"); var model = Row("VehicleModel", "VehicleModelId", v.Int("VehicleModelId", v.Find("PlayerVehicleId", vehicle))); if (model == null) throw new InvalidDataException("Vehicle is not owned.");
            var t = p.T("PlayerVehicleTuning"); int r = t.Find("PlayerVehicleId", vehicle);
            string[] names = { "Engine1", "Engine2", "Suspension1", "Suspension2", "Tire1", "Tire2", "Braking", "Steering" };
            int[] vals = { value.engine1, value.engine2, value.suspension1, value.suspension2, value.tire1, value.tire2, value.braking, value.steering };
            for (int i = 0; i < vals.Length; i++)
            {
                if (vals[i] < 0 || vals[i] > 100 || (i < 6 && vals[i] + Rfx.Int(model, "Default" + names[i], 0) > 100) || (i >= 6 && vals[i] % 50 != 0)) throw new InvalidDataException("Tuning is outside the vehicle limits.");
                if (i < 6 && (i % 2) == 0 && vals[i] + vals[i + 1] != t.Int(names[i], r, 0) + t.Int(names[i + 1], r, 0)) throw new InvalidDataException("Tuning must preserve the equipped part's bonus points.");
                Set(t, names[i], r, vals[i]);
            }
        });
    }
}
